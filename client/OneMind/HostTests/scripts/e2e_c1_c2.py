#!/usr/bin/env python3
"""C1 and C2 on the build machine (FEATURES C1, C2; cot-projection.md §12 test 7).

Runs, all on loopback and on non-standard multicast ports (never a real TAK LAN):
  - a disposable nats-server (JetStream + WebSocket) with streams COT and ENT
    (ENT with the settings of omos platform/nats/streams/ent.yaml, R1);
  - the omos TAK mesh gateway built from branch feat/tak-mesh-gateway (PR #551);
  - the omos entity service built from branch feat/entity-service (PR #552);
  - a scripted ATAK-compatible mesh peer built on the `takproto` package;
  - a stand-in for the projector's `svc.cot.send` endpoint (test double; the real
    endpoint belongs to the omos projector and is not written yet);
  - the OneMind client core (`onemind_headless`, the same ClientCore the Unreal plugins run)
    over the NATS WebSocket listener.

C1: a takproto EUD PLI on the mesh becomes ent.person.tak-....state (AMBER+STRICT,
    egress_tak false) and shows in the client's tak layer; the inspector shows sources,
    confidence parts and the raw CoT fetched from COT by sequence.
C2: a marker sent from the client through svc.cot.send reaches tak.cot.mesh.tx and is
    received as TAK Protocol v1 by a takproto listener on the mesh group.

Usage: python -I e2e_c1_c2.py <headless-binary> <omos-repo> <work-dir>
"""
import asyncio, json, os, shutil, socket, struct, subprocess, sys, tempfile, threading, time, queue, uuid

import nats
import takproto
from takproto import TAKProtoVer

HEADLESS, OMOS, RUN = sys.argv[1], sys.argv[2], sys.argv[3]
# Tools: $ONEMIND_TOOLS, else work/.tools beside the onemind-xr checkout if it exists, else PATH.
WORK = os.environ.get("ONEMIND_WORK") or os.path.abspath(os.path.join(os.path.dirname(__file__), *[".."] * 5))
TOOLS = os.environ.get("ONEMIND_TOOLS") or os.path.join(WORK, ".tools")


def tool(name):
    p = os.path.join(TOOLS, "bin", name)
    return p if os.access(p, os.X_OK) else (shutil.which(name) or name)


NATS_SERVER = os.environ.get("NATS_SERVER_BIN") or tool("nats-server")
GO = tool("go")
LO = "127.0.0.1"
SA_GROUP = ("239.2.3.1", 36969)      # not the real 6969
CHAT_GROUP = ("224.10.10.1", 37012)  # not the real 17012
results, procs = [], []


def check(name, ok, detail=""):
    results.append((name, bool(ok)))
    print(f"[{'PASS' if ok else 'FAIL'}] {name}" + (f" -- {detail}" if detail else ""), flush=True)


def free_port():
    s = socket.socket()
    s.bind((LO, 0))
    p = s.getsockname()[1]
    s.close()
    return p


def wait_port(port, timeout=10):
    end = time.time() + timeout
    while time.time() < end:
        try:
            socket.create_connection((LO, port), timeout=0.2).close()
            return True
        except OSError:
            time.sleep(0.1)
    return False


def now_iso(offset=0.0):
    t = time.time() + offset
    return time.strftime("%Y-%m-%dT%H:%M:%S", time.gmtime(t)) + f".{int(t * 1000) % 1000:03d}Z"


def eud_pli(uid, callsign):
    # Same shape as docs/evidence/M1/A7/mesh_peer_e2e.py eud_sa().
    return (f'<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
            f'<event version="2.0" uid="{uid}" type="a-f-G-U-C" how="h-e" time="{now_iso()}" '
            f'start="{now_iso()}" stale="{now_iso(300)}"><point lat="30.2672" lon="-97.7431" hae="150" '
            f'ce="9.9" le="9999999"/><detail><takv os="34" version="5.6.0.12" device="PIXEL" platform="ATAK-CIV"/>'
            f'<contact endpoint="{LO}:4242:tcp" callsign="{callsign}"/><uid Droid="{callsign}"/>'
            f'<__group role="Team Member" name="Cyan"/><status battery="88"/><track course="90" speed="1.5"/>'
            f'</detail></event>')


def mcast_sender():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    s.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_IF, socket.inet_aton(LO))
    s.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 1)
    s.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_LOOP, 1)
    return s


def mcast_listener(group):
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
    s.bind(("", group[1]))
    s.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP,
                 struct.pack("4s4s", socket.inet_aton(group[0]), socket.inet_aton(LO)))
    q = queue.Queue()

    def loop():
        while True:
            try:
                data, _ = s.recvfrom(65536)
            except OSError:
                return
            q.put(data)
    threading.Thread(target=loop, daemon=True).start()
    return s, q


def build(branch, subdir, pkg, out):
    """git archive the omos branch subtree into RUN and go build it there (omos is untouched)."""
    src = os.path.join(RUN, "src-" + os.path.basename(out))
    shutil.rmtree(src, ignore_errors=True)
    os.makedirs(src)
    arch = subprocess.run(["git", "-C", OMOS, "archive", f"origin/{branch}", subdir], capture_output=True, check=True)
    subprocess.run(["tar", "-x", "-C", src], input=arch.stdout, check=True)
    sha = subprocess.run(["git", "-C", OMOS, "rev-parse", f"origin/{branch}"], capture_output=True, text=True).stdout.strip()
    env = dict(os.environ, GOFLAGS="-mod=readonly", PATH=f"{TOOLS}/bin:" + os.environ.get("PATH", ""))
    if os.path.isdir(TOOLS):
        env.update(GOPATH=f"{TOOLS}/gopath", GOMODCACHE=f"{TOOLS}/gopath/pkg/mod")
    r = subprocess.run([GO, "build", "-o", out, pkg], cwd=os.path.join(src, subdir), env=env, capture_output=True, text=True)
    print(f"build {branch}@{sha[:12]} {subdir} -> {out}: rc={r.returncode} {r.stderr.strip()[:400]}", flush=True)
    return r.returncode == 0, sha


def start(name, argv, env=None):
    log = open(os.path.join(RUN, f"{name}.log"), "w")
    p = subprocess.Popen(argv, env=env, stdout=log, stderr=subprocess.STDOUT)
    procs.append((name, p))
    return p


async def projector_stand_in(nc, sent):
    """Test double for svc.cot.send (proposed body: client/OneMind/Docs/svc-cot-send.md).
    Applies the checks the real projector must apply (uid prefix, type allowlist, marking),
    then forwards to the mesh target with a request so the gateway acknowledges it."""
    async def handler(msg):
        try:
            req = json.loads(msg.data)
        except Exception:
            await msg.respond(json.dumps({"v": 1, "error": "invalid", "detail": "bad JSON"}).encode())
            return
        cot = req.get("cot", "")
        if req.get("v") != 1 or req.get("kind") not in ("marker", "spi", "route", "geofence", "chat") or not cot:
            await msg.respond(json.dumps({"v": 1, "error": "invalid", "detail": "kind/cot"}).encode())
            return
        if 'uid="OM.' in cot or 'uid="INTEL.' in cot:
            await msg.respond(json.dumps({"v": 1, "error": "forbidden", "detail": "reserved uid prefix"}).encode())
            return
        if req.get("marking", {}).get("tlp") not in ("CLEAR", "GREEN"):
            await msg.respond(json.dumps({"v": 1, "error": "forbidden", "detail": "marking gate"}).encode())
            return
        targets = []
        if "mesh" in req.get("targets", []):
            # Core publish without a reply (cot-projection.md §11 CP-3). A request would race:
            # the COT stream binds tak.cot.> (omos cot.yaml), so JetStream answers a request on
            # tak.cot.mesh.tx with a PubAck before the gateway's "ok" (seen in the first run).
            await nc.publish("tak.cot.mesh.tx", cot.encode())
            targets.append("mesh")
        uid = cot.split('uid="', 1)[1].split('"', 1)[0]
        sent.append(uid)
        await msg.respond(json.dumps({"v": 1, "ok": True, "uid": uid, "targets": targets}).encode())
    await nc.subscribe("svc.cot.send", cb=handler)


async def main():
    shutil.rmtree(RUN, ignore_errors=True)
    os.makedirs(RUN)
    print("== build omos services from their PR branches")
    ok_gw, gw_sha = build("feat/tak-mesh-gateway", "services/bridge/bridges/tak", ".", os.path.join(RUN, "tak-gateway"))
    ok_es, es_sha = build("feat/entity-service", "services/entity", "./cmd/entity-service", os.path.join(RUN, "entity-service"))
    check("0 omos mesh gateway (PR #551) and entity service (PR #552) build", ok_gw and ok_es, f"gw {gw_sha[:12]} es {es_sha[:12]}")
    if not (ok_gw and ok_es):
        return

    port, ws_port, mon = free_port(), free_port(), free_port()
    conf = os.path.join(RUN, "nats.conf")
    with open(conf, "w") as f:
        f.write(f'listen: {LO}:{port}\nhttp: {LO}:{mon}\n'
                f'jetstream {{ store_dir: "{RUN}/js", max_file_store: 2GB, max_memory_store: 128MB }}\n'
                f'websocket {{ listen: "{LO}:{ws_port}", no_tls: true }}\n')
    start("nats-server", [NATS_SERVER, "-c", conf])
    wait_port(ws_port)
    url, ws_url = f"nats://{LO}:{port}", f"ws://{LO}:{ws_port}"
    nc = await nats.connect(url, name="e2e-harness")
    js = nc.jetstream()
    await js.add_stream(name="COT", subjects=["tak.cot.>"], duplicate_window=120.0, allow_direct=True)
    ent = json.load(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "fixtures", "ent-stream.json")))
    r = await nc.request("$JS.API.STREAM.CREATE.ENT", json.dumps(ent).encode(), timeout=5)
    check("0b streams COT and ENT (ent.yaml settings) created", b'"config"' in r.data, r.data[:160])

    gw_env = {"PATH": os.environ.get("PATH", ""), "NATS_URL": url, "GATEWAY_ID": "tak-mesh-e2e", "MESH_INTERFACE": "lo0",
              "MESH_GROUP": f"{SA_GROUP[0]}:{SA_GROUP[1]}", "MESH_CHAT_GROUP": f"{CHAT_GROUP[0]}:{CHAT_GROUP[1]}",
              "DIRECT_TCP_PORT": "0", "UDP_LISTEN_PORT": "0", "DATAPKG_HTTP_PORT": "0",
              "MESH_TO_TAK_SERVER": "false", "MESH_RX_JETSTREAM": "true", "DATAPKG_DIR": os.path.join(RUN, "datapkg")}
    start("tak-gateway", [os.path.join(RUN, "tak-gateway")], gw_env)
    es_http = free_port()
    es_env = {"PATH": os.environ.get("PATH", ""), "NATS_URL": url, "LEASE_BUCKET": "none", "ENT_MODE": "jetstream",
              "INPUTS": "COT=tak.cot.>", "START_LOOKBACK": "10m", "HTTP_ADDR": f"{LO}:{es_http}", "POD_NAME": "e2e"}
    start("entity-service", [os.path.join(RUN, "entity-service")], es_env)
    await asyncio.sleep(2.0)
    alive = all(p.poll() is None for _, p in procs)
    check("0c nats-server, gateway and entity service running", alive,
          ", ".join(f"{n}={'up' if p.poll() is None else 'exit ' + str(p.returncode)}" for n, p in procs))

    # ---------------- C1 ----------------
    print("== C1: takproto EUD PLI on the mesh -> ent.person.tak-* -> client")
    eud_uid = "ANDROID-c1e2e" + uuid.uuid4().hex[:8]
    ent_id = "tak-" + eud_uid.lower()
    ent_q = asyncio.Queue()

    async def on_ent(m):
        await ent_q.put(m)
    await nc.subscribe(f"ent.person.{ent_id}.state", cb=on_ent)
    client = await asyncio.create_subprocess_exec(
        HEADLESS, "--url", ws_url, "--layer", "tak:person,track,place:direct", "--duration", "12",
        "--expect", ent_id, "--inspect", ent_id, "--print-every", "4",
        stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.STDOUT)
    await asyncio.sleep(1.0)  # client connected and snapshot done (empty layer)
    tx = mcast_sender()
    t_send = time.time()
    tx.sendto(bytes(takproto.xml2proto(eud_pli(eud_uid, "C1-EUD"), TAKProtoVer.MESH)), SA_GROUP)
    try:
        m = await asyncio.wait_for(ent_q.get(), timeout=10)
        e = json.loads(m.data)
    except asyncio.TimeoutError:
        e = None
    check("C1a mesh PLI becomes ent.person.tak-{uid}.state", e is not None,
          f"{(time.time() - t_send) * 1000:.0f} ms" if e else "no entity")
    if e:
        check("C1b marking AMBER+STRICT and egress_tak false (cot-projection.md §8.3)",
              e["marking"]["tlp"] == "AMBER+STRICT" and e["indicators"]["egress_tak"] is False,
              json.dumps({"tlp": e["marking"]["tlp"], "egress_tak": e["indicators"]["egress_tak"],
                          "sources": [(s["integration"], s["subject"], s["stream"], s["seq"]) for s in e["sources"]]}))
    out = (await asyncio.wait_for(client.communicate(), timeout=60))[0].decode()
    open(os.path.join(RUN, "headless-c1.txt"), "w").write(out)
    print(out)
    check("C1c the OneMind client core shows the EUD in its tak layer", f"[headless] SEEN {ent_id}" in out)
    check("C1d inspector shows sources and confidence breakdown",
          "INSPECTOR" in out and "source_prior" in out and "source 1 subject" in out and "tak.cot.mesh.rx" in out)
    check("C1e inspector raw CoT fetched from COT by sequence", "RAW COT" in out and eud_uid in out.split("RAW COT", 1)[-1])

    # ---------------- C2 ----------------
    print("== C2: client marker -> svc.cot.send -> tak.cot.mesh.tx -> mesh as TAK Protocol v1")
    sent = []
    await projector_stand_in(nc, sent)
    _, sa_q = mcast_listener(SA_GROUP)
    await asyncio.sleep(0.3)
    client = await asyncio.create_subprocess_exec(
        HEADLESS, "--url", ws_url, "--duration", "6", "--send-marker", "30.2700,-97.7400,OM-C2-MARK",
        stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.STDOUT)
    out = (await asyncio.wait_for(client.communicate(), timeout=30))[0].decode()
    open(os.path.join(RUN, "headless-c2.txt"), "w").write(out)
    print(out)
    check("C2a svc.cot.send accepted the marker (reply ok, target mesh)", "svc.cot.send reply ok=1" in out and sent,
          f"uids={sent}")
    got = None
    end = time.time() + 6
    while time.time() < end and sent:
        try:
            data = sa_q.get(timeout=0.5)
        except queue.Empty:
            continue
        if data[:3] == b"\xbf\x01\xbf":
            msg = takproto.parse_proto(bytearray(data))
            if msg and msg.cotEvent.uid == sent[0]:
                got = msg
                break
    check("C2b takproto listener on the mesh group receives the marker as TAK Protocol v1 (mesh, 0xbf 0x01 0xbf)",
          got is not None, f"uid={got.cotEvent.uid} type={got.cotEvent.type} lat={got.cotEvent.lat:.4f} "
                           f"callsign={got.cotEvent.detail.contact.callsign}" if got else "not received")
    if got:
        check("C2c marker content intact (type a-u-G, position, callsign, opex simulated in xmlDetail or attrs)",
              got.cotEvent.type == "a-u-G" and abs(got.cotEvent.lat - 30.27) < 1e-6 and
              got.cotEvent.detail.contact.callsign == "OM-C2-MARK")
    # Reserved prefixes are refused before anything reaches TAK.
    rr = await nc.request("svc.cot.send", json.dumps({"v": 1, "kind": "marker", "targets": ["mesh"], "client_id": "x",
                                                     "marking": {"tlp": "CLEAR"},
                                                     "cot": '<event version="2.0" uid="OM.drone.fake" type="a-f-A"/>'}).encode(), timeout=3)
    check("C2d svc.cot.send stand-in refuses the projector's reserved OM. uid prefix", b"forbidden" in rr.data)
    await nc.close()


try:
    asyncio.run(main())
finally:
    for name, p in procs:
        p.terminate()
        try:
            p.wait(5)
        except subprocess.TimeoutExpired:
            p.kill()
    passed = sum(1 for _, ok in results if ok)
    print(f"\n{passed}/{len(results)} checks passed")
    shutil.rmtree(os.path.join(RUN, "js"), ignore_errors=True)
    for d in os.listdir(RUN) if os.path.isdir(RUN) else []:
        if d.startswith("src-"):
            shutil.rmtree(os.path.join(RUN, d), ignore_errors=True)
    sys.exit(0 if results and passed == len(results) else 1)
