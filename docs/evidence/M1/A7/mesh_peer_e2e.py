#!/usr/bin/env python3
"""A7 end-to-end: the real tak-mesh gateway binary, disposable nats-servers and a
scripted ATAK-compatible mesh peer built on the `takproto` package.

Run (from any directory):
  /Users/danielcarku/Desktop/work/.tools/py/bin/python -I mesh_peer_e2e.py <gateway-binary>

Everything runs on loopback (lo0) multicast groups on non-standard ports, so it
does not touch a real TAK LAN. Test keys are random per run and never stored.
"""
import asyncio, hashlib, http.server, json, os, queue, shutil, socket, struct
import subprocess, sys, tempfile, threading, time, uuid

import nats
import takproto
import takproto.functions
from takproto import TAKProtoVer

GW = sys.argv[1]
NATS_SERVER = "/Users/danielcarku/Desktop/work/.tools/bin/nats-server"
RUN = "/Users/danielcarku/Desktop/work/.run/tmp-a7"
LO = "127.0.0.1"
SA_GROUP = ("239.2.3.1", 26969)
CHAT_GROUP = ("224.10.10.1", 27012)
ENC_GROUP = ("239.2.3.1", 26970)

results = []


def check(name, ok, detail=""):
    results.append((name, bool(ok)))
    print(f"[{'PASS' if ok else 'FAIL'}] {name}" + (f" -- {detail}" if detail else ""), flush=True)


def free_port(kind=socket.SOCK_STREAM):
    s = socket.socket(socket.AF_INET, kind)
    s.bind((LO, 0))
    p = s.getsockname()[1]
    s.close()
    return p


def now_iso(offset=0.0):
    t = time.time() + offset
    return time.strftime("%Y-%m-%dT%H:%M:%S", time.gmtime(t)) + f".{int(t * 1000) % 1000:03d}Z"


def cot(uid, typ, lat=30.2672, lon=-97.7431, detail="", how="m-g"):
    return (f'<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
            f'<event version="2.0" uid="{uid}" type="{typ}" how="{how}" time="{now_iso()}" '
            f'start="{now_iso()}" stale="{now_iso(300)}"><point lat="{lat}" lon="{lon}" hae="150" '
            f'ce="9.9" le="9999999"/><detail>{detail}</detail></event>')


def eud_sa(uid, endpoint):
    return cot(uid, "a-f-G-U-C", how="h-e", detail=(
        f'<takv os="34" version="5.6.0.12" device="PIXEL" platform="ATAK-CIV"/>'
        f'<contact endpoint="{endpoint}" callsign="E2E-EUD"/><uid Droid="E2E-EUD"/>'
        f'<__group role="Team Member" name="Cyan"/><status battery="88"/>'
        f'<track course="90" speed="1.5"/>'))


def mesh_bytes(xml):
    return bytes(takproto.xml2proto(xml, TAKProtoVer.MESH))


def takcontrol(uid):
    # TakMessage{takControl{minProtoVersion=1,maxProtoVersion=1,contactUid=uid}}
    inner = b"\x08\x01\x10\x01\x1a" + bytes([len(uid)]) + uid.encode()
    return b"\xbf\x01\xbf\x0a" + bytes([len(inner)]) + inner


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


def tcp_listener():
    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind((LO, 0))
    srv.listen(8)
    q = queue.Queue()

    def loop():
        while True:
            try:
                c, _ = srv.accept()
            except OSError:
                return
            buf = b""
            c.settimeout(5)
            try:
                while True:
                    d = c.recv(65536)
                    if not d:
                        break
                    buf += d
            except OSError:
                pass
            c.close()
            q.put(buf)
    threading.Thread(target=loop, daemon=True).start()
    return srv, q


def send_tcp(port, data):
    with socket.create_connection((LO, port), timeout=5) as c:
        c.sendall(data)


def drain_for(q, pred, timeout=4.0):
    end = time.time() + timeout
    while time.time() < end:
        try:
            item = q.get(timeout=max(0.05, end - time.time()))
        except queue.Empty:
            break
        if pred(item):
            return item
    return None


def decode_any(b):
    if b[:3] == b"\xbf\x01\xbf":
        m = takproto.parse_proto(bytearray(b))
        return m.cotEvent.uid if m and m.cotEvent.uid else None, 1, m
    return None, 0, b


# ── OpenSSL-based Commo crypto (independent of the Go implementation) ────────
def ossl_encrypt(pt, ak, ck):
    iv = os.urandom(16)
    ct = subprocess.run(["openssl", "enc", "-aes-256-cbc", "-K", ck, "-iv", iv.hex()],
                        input=pt, capture_output=True, check=True).stdout
    mac = subprocess.run(["openssl", "dgst", "-sha256", "-mac", "HMAC", "-macopt", "hexkey:" + ak, "-binary"],
                         input=iv + ct, capture_output=True, check=True).stdout
    return iv + ct + mac


def ossl_decrypt(msg, ak, ck):
    body, tag = msg[:-32], msg[-32:]
    mac = subprocess.run(["openssl", "dgst", "-sha256", "-mac", "HMAC", "-macopt", "hexkey:" + ak, "-binary"],
                         input=body, capture_output=True, check=True).stdout
    if mac != tag:
        return None
    r = subprocess.run(["openssl", "enc", "-d", "-aes-256-cbc", "-K", ck, "-iv", body[:16].hex()],
                       input=body[16:], capture_output=True)
    return r.stdout if r.returncode == 0 else None


# ── Processes ─────────────────────────────────────────────────────────────────
procs = []


def start_nats(name):
    port = free_port()
    store = os.path.join(RUN, name)
    os.makedirs(store, exist_ok=True)
    p = subprocess.Popen([NATS_SERVER, "-a", LO, "-p", str(port), "-js", "-sd", store],
                         stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    procs.append(p)
    for _ in range(50):
        try:
            socket.create_connection((LO, port), timeout=0.2).close()
            break
        except OSError:
            time.sleep(0.1)
    return f"nats://{LO}:{port}"


def start_gw(name, env):
    log = open(os.path.join(RUN, f"{name}.log"), "a")
    e = {"PATH": os.environ.get("PATH", ""), "GATEWAY_ID": name, "MESH_INTERFACE": "lo0"}
    e.update(env)
    p = subprocess.Popen([GW], env=e, stdout=log, stderr=subprocess.STDOUT)
    procs.append(p)
    time.sleep(1.0)
    return p


def stop(p):
    p.terminate()
    try:
        p.wait(5)
    except subprocess.TimeoutExpired:
        p.kill()


async def collect(nc, subject):
    q = asyncio.Queue()

    async def cb(msg):
        await q.put(msg)
    await nc.subscribe(subject, cb=cb)
    await nc.flush()
    return q


async def wait_msg(q, pred, timeout=4.0):
    end = time.time() + timeout
    while time.time() < end:
        try:
            m = await asyncio.wait_for(q.get(), timeout=max(0.05, end - time.time()))
        except asyncio.TimeoutError:
            break
        if pred(m):
            return m
    return None


def count_q(q):
    n = []
    while not q.empty():
        n.append(q.get_nowait())
    return n


async def main():
    os.makedirs(RUN, exist_ok=True)
    natsA = start_nats("natsA")
    natsB = start_nats("natsB")
    nc = await nats.connect(natsA)
    ncB = await nats.connect(natsB)
    # The gateway publishes tak.cot.mesh.rx with acked JetStream publishes, as
    # to stream COT in the cluster (2 min duplicate window).
    for c in (nc, ncB):
        await c.jetstream().add_stream(name="COT", subjects=["tak.cot.mesh.rx"], duplicate_window=120.0)

    direct_port = free_port()
    dp_port = free_port()
    dp_dir = os.path.join(RUN, "datapkg")
    shutil.rmtree(dp_dir, ignore_errors=True)
    base_env = {"NATS_URL": natsA, "MESH_GROUP": f"{SA_GROUP[0]}:{SA_GROUP[1]}",
                "MESH_CHAT_GROUP": f"{CHAT_GROUP[0]}:{CHAT_GROUP[1]}",
                "DIRECT_TCP_PORT": str(direct_port), "DATAPKG_DIR": dp_dir,
                "DATAPKG_HTTP_PORT": str(dp_port)}

    rx = await collect(nc, "tak.cot.mesh.rx")
    inject = await collect(nc, "tak.cot.inject")
    status = await collect(nc, "tel.fleet.edge.>")

    _, sa_q = mcast_listener(SA_GROUP)
    _, chat_q = mcast_listener(CHAT_GROUP)
    eud_tcp, eud_q = tcp_listener()
    eud_port = eud_tcp.getsockname()[1]
    tx = mcast_sender()
    eud_uid = "ANDROID-e2e" + uuid.uuid4().hex[:8]

    gw = start_gw("a7e2e", base_env)
    print(f"gateway a7e2e: NATS {natsA}, SA {SA_GROUP}, chat {CHAT_GROUP}, direct :{direct_port}, getfile :{dp_port}")

    # 1. takproto peer -> mesh -> tak.cot.mesh.rx
    tx.sendto(takcontrol(eud_uid), SA_GROUP)
    sa_xml = eud_sa(eud_uid, f"{LO}:{eud_port}:tcp")
    tx.sendto(mesh_bytes(sa_xml), SA_GROUP)
    m = await wait_msg(rx, lambda m: eud_uid.encode() in m.data)
    check("1 takproto v1 SA on the mesh appears on tak.cot.mesh.rx", m is not None and m.headers.get("Tak-Proto") == "1"
          and m.headers.get("Tak-Delivery") == "mesh" and m.headers.get("Nats-Msg-Id", "").startswith("cot-"),
          m and json.dumps(dict(m.headers)))
    check("1b XML on the bus keeps typed and untyped detail (__group, uid Droid, contact)",
          m is not None and b'<__group name="Cyan"' in m.data and b'<uid Droid="E2E-EUD"' in m.data
          and f'endpoint="{LO}:{eud_port}:tcp"'.encode() in m.data, m and m.data[:400])

    # 2. MESH_TO_TAK_SERVER default false
    await asyncio.sleep(0.5)
    check("2 MESH_TO_TAK_SERVER unset (default false): nothing on tak.cot.inject", len(count_q(inject)) == 0)

    # 3. projected sim-x1 on tak.cot.mesh.tx (core publish, no reply) -> v1 on the mesh (contract §12 test 7)
    simx1 = cot("OM.drone.sim-x1", "a-f-A-M-H-Q", lat=30.30, lon=-97.70, how="m-p", detail=(
        '<contact callsign="sim-x1"/><track course="45" speed="12"/><remarks>OneMind sim-x1</remarks>'))
    await nc.publish("tak.cot.mesh.tx", simx1.encode())
    await nc.flush()
    got = drain_for(sa_q, lambda b: decode_any(b)[0] == "OM.drone.sim-x1")
    check("3 projected sim-x1 on tak.cot.mesh.tx is received as TAK Protocol v1 by a takproto listener on lo0",
          got is not None and got[:3] == b"\xbf\x01\xbf", got and f"{len(got)} bytes, header {got[:3].hex()}")
    if got:
        msg = takproto.parse_proto(bytearray(got))
        check("3b takproto decodes type, point and callsign",
              msg.cotEvent.type == "a-f-A-M-H-Q" and abs(msg.cotEvent.lat - 30.30) < 1e-9
              and msg.cotEvent.detail.contact.callsign == "sim-x1")
    reply = await nc.request("tak.cot.mesh.tx", simx1.encode(), timeout=3)
    check("3c request/reply on tak.cot.mesh.tx answers ok", reply.data == b"ok", reply.data)
    bad = await nc.request("tak.cot.mesh.tx", b"<event", timeout=3)
    check("3d malformed CoT is refused with an error reply", bad.data.startswith(b"error:"), bad.data[:80])
    hostile = cot("OM.track.e2e-hostile", "a-h-A", how="m-p")
    ref = await nc.request("tak.cot.mesh.tx", hostile.encode(), timeout=3)
    leaked = drain_for(sa_q, lambda b: decode_any(b)[0] == "OM.track.e2e-hostile", timeout=1.5)
    check("3e a-h-* CoT on tak.cot.mesh.tx is refused with an error reply and not sent on the mesh",
          ref.data.startswith(b"error:") and leaked is None, ref.data[:100])

    # 4. GeoChat both ways
    chat_xml = cot(f"GeoChat.{eud_uid}.All Chat Rooms.{uuid.uuid4()}", "b-t-f", how="h-g-i-g-o", detail=(
        f'<__chat parent="RootContactGroup" groupOwner="false" chatroom="All Chat Rooms" id="All Chat Rooms" '
        f'senderCallsign="E2E-EUD"><chatgrp uid0="{eud_uid}" uid1="All Chat Rooms" id="All Chat Rooms"/></__chat>'
        f'<link uid="{eud_uid}" type="a-f-G-U-C" relation="p-p"/>'
        f'<remarks source="BAO.F.ATAK.{eud_uid}" to="All Chat Rooms" time="{now_iso()}">hello hermes</remarks>'))
    tx.sendto(mesh_bytes(chat_xml), CHAT_GROUP)
    m = await wait_msg(rx, lambda m: b"hello hermes" in m.data)
    check("4 GeoChat (v1) on 224.10.10.1 group appears on tak.cot.mesh.rx with Tak-Delivery chat",
          m is not None and m.headers.get("Tak-Delivery") == "chat", m and dict(m.headers))
    out_chat = chat_xml.replace("hello hermes", "reply from onemind").replace(eud_uid, "omos-gw-a7e2e")
    await nc.publish("tak.cot.mesh.tx", out_chat.encode())
    await nc.flush()
    got = drain_for(chat_q, lambda b: b"reply from onemind" in (takproto.parse_proto(bytearray(b)).cotEvent.detail.xmlDetail.encode()
                                                               if b[:3] == b"\xbf\x01\xbf" else b))
    check("4b b-t-f on tak.cot.mesh.tx goes out on the chat group", got is not None)

    # 5. directed TCP both ways
    send_tcp(direct_port, cot("ANDROID-direct-1", "b-m-p-s-m", how="h-g-i-g-o", detail='<contact callsign="DIRECT"/>').encode())
    m = await wait_msg(rx, lambda m: b"ANDROID-direct-1" in m.data)
    check("5 directed TCP CoT to the gateway appears with Tak-Delivery direct",
          m is not None and m.headers.get("Tak-Delivery") == "direct")
    await nc.publish("tak.cot.mesh.tx", cot("OM.marker.direct-1", "b-m-p-s-m").encode(),
                     headers={"Tak-Dest-Uid": eud_uid})
    await nc.flush()
    got = drain_for(eud_q, lambda b: b"OM.marker.direct-1" in b or decode_any(b)[0] == "OM.marker.direct-1")
    check("5b Tak-Dest-Uid header sends a directed message to the contact's advertised endpoint", got is not None)

    # 6. data package: the peer hosts /getfile, sends b-f-t-r directed, gateway fetches and acks
    payload = os.urandom(20000)
    sha = hashlib.sha256(payload).hexdigest()

    class H(http.server.BaseHTTPRequestHandler):
        def do_GET(self):
            if self.path == "/getfile?file=pkg1":
                self.send_response(200)
                self.send_header("Content-Length", str(len(payload)))
                self.end_headers()
                self.wfile.write(payload)
            else:
                self.send_error(404)

        def log_message(self, *a):
            pass
    hs = http.server.HTTPServer((LO, 0), H)
    threading.Thread(target=hs.serve_forever, daemon=True).start()
    ack_uid = str(uuid.uuid4())
    req = cot(str(uuid.uuid4()), "b-f-t-r", how="h-e", detail=(
        f'<fileshare filename="mission1.zip" senderUrl="http://192.168.77.7:{hs.server_port}/getfile?file=pkg1" '
        f'sizeInBytes="{len(payload)}" sha256="{sha}" senderUid="{eud_uid}" senderCallsign="E2E-EUD" name="mission1"/>'
        f'<ackrequest uid="{ack_uid}" ackrequested="true" tag="mission1"/>'))
    send_tcp(direct_port, mesh_bytes(req))
    ack = drain_for(eud_q, lambda b: ack_uid.encode() in (b if b[:1] == b"<" else
                    takproto.parse_proto(bytearray(b)).cotEvent.detail.xmlDetail.encode()), timeout=8)
    ack_text = ack if ack is None or ack[:1] == b"<" else takproto.parse_proto(bytearray(ack)).cotEvent.detail.xmlDetail.encode()
    stored = os.path.join(dp_dir, "inbox", f"{sha}-mission1.zip")
    check("6 directed b-f-t-r: gateway fetches from the sender's address, verifies SHA-256, stores it",
          os.path.exists(stored) and open(stored, "rb").read() == payload, stored)
    check("6b gateway sends b-f-t-a ackresponse success=true to the sender's endpoint",
          ack_text is not None and b'success="true"' in ack_text, ack_text and ack_text[:300])
    with open(os.path.join(dp_dir, "outbox", "onemind-pkg.zip"), "wb") as f:
        f.write(b"PK-onemind")
    import urllib.request
    body = urllib.request.urlopen(f"http://{LO}:{dp_port}/getfile?file=onemind-pkg.zip", timeout=5).read()
    check("6c gateway serves its outbox at /getfile?file=<name>", body == b"PK-onemind")
    try:
        urllib.request.urlopen(f"http://{LO}:{dp_port}/getfile?file=..%2Finbox%2F{sha}-mission1.zip", timeout=5)
        check("6d path traversal on /getfile refused", False)
    except urllib.error.HTTPError as e:
        check("6d path traversal on /getfile refused", e.code == 404)
    hs.shutdown()

    # status heartbeat (30 s) confirms delivery via the tx counter (contract §6.3, CP-3)
    st = await wait_msg(status, lambda m: True, timeout=35)
    meta = json.loads(st.data)["meta"] if st else {}
    check("7 tel.fleet.edge.a7e2e.status reports tx_interface lo0, tx count, chat group, datapkg_rx",
          meta.get("tx_interface") == "lo0" and meta.get("tx", 0) >= 4 and meta.get("datapkg_rx") == 1
          and meta.get("to_tak_server") is False, json.dumps(meta))
    js_info = await nc.jetstream().stream_info("COT")
    check("7c tak.cot.mesh.rx is stored in stream COT by acked publishes and counted in meta.rx",
          meta.get("rx", 0) >= 1 and js_info.state.messages >= 1 and "not acked" not in meta.get("last_error", ""),
          f"meta.rx={meta.get('rx')} COT msgs={js_info.state.messages}")
    check("7b broadcast protocol stays v1 after GeoChat from a takproto peer (TakControl with unset versions)",
          meta.get("proto_version") == 1, f"proto_version={meta.get('proto_version')}")
    stop(gw)

    # 8. MESH_TO_TAK_SERVER=true with CP-2
    gw = start_gw("a7e2e", dict(base_env, MESH_TO_TAK_SERVER="true", DATAPKG_DIR="", DATAPKG_HTTP_PORT="0"))
    count_q(inject)
    pli = eud_sa(eud_uid, f"{LO}:{eud_port}:tcp")
    tx.sendto(mesh_bytes(pli), SA_GROUP)
    tx.sendto(mesh_bytes(cot("OM.drone.sim-x1", "a-f-A-M-H-Q", how="m-p")), SA_GROUP)
    tx.sendto(mesh_bytes(cot("INTEL.acled.e2e", "a-h-G", how="m-p")), SA_GROUP)
    tx.sendto(mesh_bytes(cot("omos-gw-othersite", "a-f-G-E-S", how="h-g-i-g-o")), SA_GROUP)
    # A v1 peer whose xmlDetail closes the event and appends a forged OM.* hostile track.
    smug = takproto.functions.xml2message(cot("ANDROID-smuggle", "a-f-G-U-C", how="h-e"))
    smug.cotEvent.detail.xmlDetail = (
        '<contact callsign="x"/></detail></event><event version="2.0" uid="OM.aircraft.fake" type="a-h-A" '
        f'how="m-g" time="{now_iso()}" start="{now_iso()}" stale="{now_iso(300)}"><point lat="1" lon="1" hae="0" '
        'ce="1" le="1"/><detail>')
    count_q(rx)
    tx.sendto(bytes(takproto.functions.msg2proto(smug, TAKProtoVer.MESH)), SA_GROUP)
    await asyncio.sleep(1.5)
    inj = count_q(inject)
    rx8 = count_q(rx)
    fake_n = sum(1 for m in inj + rx8 if b"OM.aircraft.fake" in m.data or b"ANDROID-smuggle" in m.data)
    check("8c xmlDetail that closes </detail></event> is dropped: nothing on tak.cot.inject or tak.cot.mesh.rx",
          fake_n == 0, f"{fake_n} messages")
    eud_n = sum(1 for m in inj if eud_uid.encode() in m.data)
    om_n = sum(1 for m in inj if b'uid="OM.' in m.data or b'uid="INTEL.' in m.data or b'uid="omos-gw-' in m.data)
    check("8 MESH_TO_TAK_SERVER=true: the EUD PLI appears once on tak.cot.inject", eud_n == 1, f"{eud_n} copies")
    check("8b CP-2: OM.*, INTEL.* and omos-gw-* CoT heard on the mesh are not copied to tak.cot.inject",
          om_n == 0, f"{om_n} copies, {len(inj)} inject msgs total")
    stop(gw)

    # 9. two gateways, mesh encryption, plus an OpenSSL-encrypting takproto peer
    ak, ck = os.urandom(32).hex(), os.urandom(32).hex()
    enc = {"MESH_GROUP": f"{ENC_GROUP[0]}:{ENC_GROUP[1]}", "MESH_CHAT_GROUP": "off", "DIRECT_TCP_PORT": "0",
           "MESH_AUTH_KEY": ak, "MESH_CIPHER_KEY": ck}
    gwC = start_gw("a7encC", dict(enc, NATS_URL=natsA))
    gwD = start_gw("a7encD", dict(enc, NATS_URL=natsB))
    rxB = await collect(ncB, "tak.cot.mesh.rx")
    _, enc_q = mcast_listener(ENC_GROUP)
    await nc.publish("tak.cot.mesh.tx", simx1.encode())
    await nc.flush()
    m = await wait_msg(rxB, lambda m: b"OM.drone.sim-x1" in m.data)
    check("9 gateway C (bus A) -> encrypted mesh -> gateway D (bus B)", m is not None, m and dict(m.headers))
    raw = drain_for(enc_q, lambda b: True)
    plain = ossl_decrypt(raw, ak, ck) if raw else None
    check("9b the datagram on the wire is not plaintext and OpenSSL decrypts it to a takproto frame",
          raw is not None and b"sim-x1" not in raw and plain is not None and plain[:1] in (b"\xbf", b"<"),
          raw and f"{len(raw)} bytes on the wire; decrypted header {plain[:3].hex() if plain else None}")
    uid2 = "ANDROID-enc" + uuid.uuid4().hex[:6]
    tx.sendto(ossl_encrypt(mesh_bytes(eud_sa(uid2, "127.0.0.1:4242:tcp")), ak, ck), ENC_GROUP)
    m = await wait_msg(rxB, lambda m: uid2.encode() in m.data)
    check("9c OpenSSL-encrypted takproto SA from the peer is accepted", m is not None)
    uid3 = "ANDROID-plain" + uuid.uuid4().hex[:6]
    tx.sendto(mesh_bytes(eud_sa(uid3, "127.0.0.1:4242:tcp")), ENC_GROUP)
    m = await wait_msg(rxB, lambda m: uid3.encode() in m.data, timeout=2)
    check("9d plaintext on an encrypted mesh is dropped", m is None)
    stop(gwC)
    stop(gwD)

    await nc.close()
    await ncB.close()


try:
    asyncio.run(main())
except Exception as e:
    import traceback
    traceback.print_exc()
    check("script ran to completion", False, repr(e))
finally:
    for p in procs:
        if p.poll() is None:
            stop(p)
    passed = sum(1 for _, ok in results if ok)
    print(f"\n{passed}/{len(results)} checks passed")
    sys.exit(0 if results and passed == len(results) else 1)
