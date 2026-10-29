#!/usr/bin/env python3
# C3 / C8 / B11 end to end on the build machine, with the client core driven by onemind_tak:
#
# A. Mesh gateway (omos PR #551, branch feat/tak-mesh-gateway) built from source and run
#    against a disposable nats-server: the client sends its own SA (contact card), a chat and
#    every B11 shape as TAK directed TCP messages (v1 and XML); they must appear on
#    tak.cot.mesh.rx with Tak-Delivery: direct. The gateway then sends a chat from the bus
#    (tak.cot.mesh.tx + Tak-Dest-Uid) back to the endpoint the client advertised.
# B. Enrollment against a scripted TAK Server stub of /Marti/api/tls/config and
#    /Marti/api/tls/signClient/v2 (HTTPS, Basic auth), signing the client's CSR with a
#    per-run test CA.
# C. A scripted TLS TAK-stream peer (mutual TLS, written here with the takproto package for
#    v1, MIT): with the enrolled certificate the client connects, negotiates TAK Protocol v1,
#    shows the peer's user in its contact list, chats both ways, pings, and sends every
#    shape; the geometry the peer decodes equals what the client drew. Variants: an XML-only
#    server, channel filtering, a client without a valid certificate, a server signed by an
#    unknown CA.
# Usage: e2e_c3_tak.py <onemind_tak> <omos repo> <out dir>
# SPDX-License-Identifier: GPL-3.0-or-later
import asyncio
import base64
import http.server
import json
import os
import re
import secrets
import shutil
import socket
import ssl
import subprocess
import sys
import threading
import time

import nats
import takproto
from takproto import TAKProtoVer

TOOLS = "/Users/danielcarku/Desktop/work/.tools"
OPENSSL = "/opt/homebrew/opt/openssl@3/bin/openssl"
NATS_SERVER = os.environ.get("NATS_SERVER_BIN", TOOLS + "/bin/nats-server")
GOENV = dict(os.environ, PATH=TOOLS + "/bin:" + os.environ.get("PATH", ""), GOPATH=TOOLS + "/gopath",
             GOMODCACHE=TOOLS + "/gopath/pkg/mod", GOPROXY="off", GOFLAGS="-mod=mod")
UID, CS = "ONEMIND-e2e", "OM-E2E"
results, transcript = [], []
SHAPE_SUFFIXES = ["point", "line", "polygon", "rect", "circle", "sector", "rings.r1", "rings.r2", "rings.r3", "rb", "buffer"]


def check(name, ok, detail=""):
    results.append((name, bool(ok), detail))
    print(("PASS " if ok else "FAIL ") + name + ("" if ok else "  " + str(detail)[:600]), flush=True)


def free_port():
    s = socket.socket()
    s.bind(("127.0.0.1", 0))
    p = s.getsockname()[1]
    s.close()
    return p


def now_iso(off=0):
    return time.strftime("%Y-%m-%dT%H:%M:%S.000Z", time.gmtime(time.time() + off))


def sa_xml(uid, callsign, group="Cyan"):
    return (f'<event version="2.0" uid="{uid}" type="a-f-G-U-C" how="m-g" time="{now_iso()}" start="{now_iso()}" '
            f'stale="{now_iso(120)}"><point lat="30.268" lon="-97.744" hae="150" ce="10" le="10"/><detail>'
            f'<contact endpoint="*:-1:stcp" callsign="{callsign}"/><__group name="{group}" role="Team Member"/>'
            f'<takv device="Pixel" platform="ATAK-CIV" os="34" version="5.6.0"/></detail></event>')


def chat_xml(from_uid, from_cs, to_uid, to_cs, text):
    mid = secrets.token_hex(6)
    return (f'<event version="2.0" uid="GeoChat.{from_uid}.{to_uid}.{mid}" type="b-t-f" how="h-g-i-g-o" time="{now_iso()}" '
            f'start="{now_iso()}" stale="{now_iso(86400)}"><point lat="0" lon="0" hae="9999999" ce="9999999" le="9999999"/>'
            f'<detail><__chat parent="RootContactGroup" groupOwner="false" messageId="{mid}" chatroom="{to_cs}" id="{to_uid}" '
            f'senderCallsign="{from_cs}"><chatgrp uid0="{from_uid}" uid1="{to_uid}" id="{to_uid}"/></__chat>'
            f'<link uid="{from_uid}" type="a-f-G-U-C" relation="p-p"/><remarks source="BAO.F.ATAK.{from_uid}" to="{to_uid}" '
            f'time="{now_iso()}">{text}</remarks></detail></event>')


def links(xml):
    return [tuple(round(float(v), 6) for v in m.split(",")[:2]) for m in re.findall(r'<link[^>]*point="([^"]+)"', xml)]


def run(cmd, env=None, timeout=60):
    p = subprocess.run(cmd, env=env, capture_output=True, text=True, timeout=timeout)
    lines = []
    for l in p.stdout.splitlines():
        try:
            lines.append(json.loads(l))
        except ValueError:
            pass
    return p.returncode, lines, p.stderr


# --------------------------------------------------------------------------------------------- A
async def part_a(tak, omos, work):
    src = work + "/gw"
    os.makedirs(src)
    subprocess.run(f"git -C {omos} archive origin/feat/tak-mesh-gateway services/bridge/bridges/tak | tar -x -C {src}",
                   shell=True, check=True)
    commit = subprocess.run(["git", "-C", omos, "rev-parse", "origin/feat/tak-mesh-gateway"], capture_output=True,
                            text=True).stdout.strip()
    b = subprocess.run(["go", "build", "-o", work + "/takgw", "."], cwd=src + "/services/bridge/bridges/tak",
                       env=GOENV, capture_output=True, text=True)
    check("A mesh gateway builds from feat/tak-mesh-gateway " + commit[:8], b.returncode == 0, b.stderr[-300:])
    if b.returncode:
        return
    port, dport, gport = free_port(), free_port(), free_port()
    conf = work + "/a-nats.conf"
    open(conf, "w").write(f'listen: "127.0.0.1:{port}"\njetstream {{ store_dir: "{work}/a-js" }}\n')
    procs = [subprocess.Popen([NATS_SERVER, "-c", conf, "-l", work + "/a-nats.log"])]
    try:
        for _ in range(100):
            try:
                socket.create_connection(("127.0.0.1", port), 0.1).close()
                break
            except OSError:
                time.sleep(0.05)
        nc = await nats.connect(f"nats://127.0.0.1:{port}")
        js = nc.jetstream()
        # COT as omos platform/nats/streams/cot.yaml (tak.cot.>, 2 min dedup), memory for the test.
        await js.add_stream(name="COT", subjects=["tak.cot.>"], storage="memory", duplicate_window=120)
        rx = []

        async def on_rx(m):
            rx.append((dict(m.headers or {}), m.data.decode()))
        await nc.subscribe("tak.cot.mesh.rx", cb=on_rx)
        await nc.flush()
        env = dict(os.environ, NATS_URL=f"nats://127.0.0.1:{port}", GATEWAY_ID="m4ue", MESH_GROUP=f"239.2.3.1:{gport}",
                   MESH_INTERFACE="lo0", MESH_CHAT_GROUP="off", DIRECT_TCP_PORT=str(dport), UDP_LISTEN_PORT="0",
                   MESH_RX_JETSTREAM="true", MESH_TO_TAK_SERVER="false")
        gw = subprocess.Popen([work + "/takgw"], env=env, stdout=open(work + "/gw.log", "w"), stderr=subprocess.STDOUT)
        procs.append(gw)
        for _ in range(100):
            try:
                socket.create_connection(("127.0.0.1", dport), 0.1).close()
                break
            except OSError:
                await asyncio.sleep(0.05)
        for proto in ("v1", "xml"):
            rx.clear()
            lport = free_port()
            uid = UID + "-" + proto
            args = [tak, "direct", "--to", f"127.0.0.1:{dport}:tcp", "--uid", uid, "--callsign", CS, "--listen",
                    str(lport), "--chat-to", "All Chat Rooms", "--text", "hello mesh " + proto, "--shapes", "--seconds", "4"]
            if proto == "v1":
                args.append("--v1")
            p = await asyncio.create_subprocess_exec(*args, stdout=asyncio.subprocess.PIPE)
            await asyncio.sleep(1.5)
            # A chat from the bus to the client: the gateway sends it directed to the endpoint
            # the client advertised in its SA.
            await nc.publish("tak.cot.mesh.tx", chat_xml("ANDROID-mesh", "MESHUSER", uid, CS, "reply over mesh").encode(),
                             headers={"Tak-Dest-Uid": uid})
            out, _ = await p.communicate()
            lines = [json.loads(l) for l in out.decode().splitlines() if l.strip()]
            transcript.append({"part": "A-" + proto, "client": lines, "rx": rx[:]})
            sent = [l for l in lines if l["ev"] == "direct_sent"]
            check(f"A[{proto}] all {len(sent)} directed sends succeeded", sent and all(not l["error"] for l in sent),
                  sent)
            mine = [(h, x) for h, x in rx if uid in x]
            sa = [x for h, x in mine if re.match(r'<event[^>]* type="a-f-G-U-C"', x)]
            check(f"A[{proto}] client SA (contact card) on tak.cot.mesh.rx",
                  sa and f'callsign="{CS}"' in sa[0] and f'endpoint="127.0.0.1:{lport}:tcp"' in sa[0], mine[:1])
            hdrs = [h for h, x in mine]
            want_proto = "1" if proto == "v1" else "0"
            check(f"A[{proto}] delivered as directed TCP, Tak-Proto {want_proto}",
                  hdrs and all(h.get("Tak-Delivery") == "direct" and h.get("Tak-Proto") == want_proto for h in hdrs),
                  hdrs[:2])
            check(f"A[{proto}] chat reached the bus", any("hello mesh " + proto in x for h, x in mine))
            got_shapes = [s for s in SHAPE_SUFFIXES if any(f'uid="{uid}-{s}"' in x for h, x in mine)]
            check(f"A[{proto}] all {len(SHAPE_SUFFIXES)} shapes reached the bus", len(got_shapes) == len(SHAPE_SUFFIXES),
                  got_shapes)
            back = [l for l in lines if l["ev"] == "direct_in"]
            check(f"A[{proto}] chat from the bus delivered to the client's own endpoint",
                  any(l.get("kind") == "chat" and l.get("text") == "reply over mesh" for l in back), back)
        await nc.close()
    finally:
        for p in reversed(procs):
            p.terminate()
            try:
                p.wait(5)
            except Exception:
                p.kill()


# --------------------------------------------------------------------------------------------- B
def make_ca(work):
    os.makedirs(work + "/pki", exist_ok=True)
    k = work + "/pki"
    sh = lambda c: subprocess.run(c, shell=True, check=True, capture_output=True)
    for name in ("ca", "badca"):
        sh(f"{OPENSSL} req -x509 -newkey rsa:2048 -nodes -keyout {k}/{name}.key -out {k}/{name}.pem -days 1 "
           f"-subj /CN=onemind-test-{name}")
    open(k + "/srv.ext", "w").write("subjectAltName=DNS:localhost,IP:127.0.0.1\nextendedKeyUsage=serverAuth\n")
    for name, ca in (("server", "ca"), ("badserver", "badca")):
        sh(f"{OPENSSL} req -newkey rsa:2048 -nodes -keyout {k}/{name}.key -out {k}/{name}.csr -subj /CN=takserver-test")
        sh(f"{OPENSSL} x509 -req -in {k}/{name}.csr -CA {k}/{ca}.pem -CAkey {k}/{ca}.key -CAcreateserial -days 1 "
           f"-extfile {k}/srv.ext -out {k}/{name}.pem")
    # A client certificate from the wrong CA (the "no valid certificate" case).
    sh(f"{OPENSSL} req -newkey rsa:2048 -nodes -keyout {k}/rogue.key -out {k}/rogue.csr -subj /CN=rogue")
    sh(f"{OPENSSL} x509 -req -in {k}/rogue.csr -CA {k}/badca.pem -CAkey {k}/badca.key -CAcreateserial -days 1 -out {k}/rogue.pem")
    return k


def enroll_server(pki, user, password, log):
    class H(http.server.BaseHTTPRequestHandler):
        def log_message(self, *a):
            pass

        def authed(self):
            want = "Basic " + base64.b64encode(f"{user}:{password}".encode()).decode()
            if self.headers.get("Authorization") != want:
                self.send_response(401)
                self.end_headers()
                log.append({"path": self.path, "status": 401})
                return False
            return True

        def do_GET(self):
            if not self.authed():
                return
            if self.path != "/Marti/api/tls/config":
                self.send_response(404)
                self.end_headers()
                return
            body = ('<?xml version="1.0" encoding="UTF-8" standalone="yes"?><ns2:certificateConfig '
                    'xmlns:ns2="com.bbn.marti.config"><nameEntries><nameEntry name="O" value="OneMind-Test"/>'
                    '<nameEntry name="OU" value="TAK"/></nameEntries></ns2:certificateConfig>').encode()
            log.append({"path": self.path, "status": 200})
            self.send_response(200)
            self.send_header("Content-Type", "application/xml")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def do_POST(self):
            if not self.authed():
                return
            n = int(self.headers.get("Content-Length", "0"))
            csr_b64 = self.rfile.read(n).decode()
            m = re.match(r"^/Marti/api/tls/signClient/v2\?clientUid=([^&]+)&version=(.+)$", self.path)
            if not m:
                self.send_response(400)
                self.end_headers()
                return
            d = pki + "/enroll-" + secrets.token_hex(4)
            os.makedirs(d)
            pem = "-----BEGIN CERTIFICATE REQUEST-----\n" + "\n".join(csr_b64[i:i + 64] for i in range(0, len(csr_b64), 64)) + \
                  "\n-----END CERTIFICATE REQUEST-----\n"
            open(d + "/c.csr", "w").write(pem)
            subj = subprocess.run([OPENSSL, "req", "-in", d + "/c.csr", "-noout", "-subject", "-verify"],
                                  capture_output=True, text=True)
            subprocess.run([OPENSSL, "x509", "-req", "-in", d + "/c.csr", "-CA", pki + "/ca.pem", "-CAkey", pki + "/ca.key",
                            "-CAcreateserial", "-days", "1", "-out", d + "/c.pem"], check=True, capture_output=True)
            bare = lambda f: "".join(l for l in open(f).read().splitlines() if "-----" not in l)
            body = json.dumps({"signedCert": bare(d + "/c.pem"), "ca0": bare(pki + "/ca.pem")}, indent=2).encode()
            log.append({"path": self.path, "status": 200, "client_uid": m.group(1), "csr_subject": subj.stdout.strip(),
                        "csr_verify": subj.stderr.strip()})
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

    srv = http.server.ThreadingHTTPServer(("127.0.0.1", 0), H)
    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    ctx.load_cert_chain(pki + "/server.pem", pki + "/server.key")
    srv.socket = ctx.wrap_socket(srv.socket, server_side=True)
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    return srv


# --------------------------------------------------------------------------------------------- C
class Peer:
    """Scripted TAK Server streaming input: mutual TLS, XML first, optional v1 offer."""

    def __init__(self, pki, offer_v1, server_cert="server"):
        self.offer_v1 = offer_v1
        self.ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        self.ctx.load_cert_chain(f"{pki}/{server_cert}.pem", f"{pki}/{server_cert}.key")
        self.ctx.verify_mode = ssl.CERT_REQUIRED
        self.ctx.load_verify_locations(pki + "/ca.pem")
        self.events, self.log, self.client_cn, self.pings, self.handshake_errors = [], [], [], 0, 0

    async def start(self):
        self.server = await asyncio.start_server(self.handle, "127.0.0.1", 0, ssl=self.ctx,
                                                 ssl_handshake_timeout=5)
        self.port = self.server.sockets[0].getsockname()[1]

    async def handle(self, reader, writer):
        cert = writer.get_extra_info("peercert") or {}
        cn = dict(x[0] for x in cert.get("subject", ())).get("commonName")
        self.client_cn.append(cn)
        mode = "xml"
        buf = b""

        def send(xml):
            if mode == "v1":
                writer.write(bytes(takproto.xml2proto(xml, TAKProtoVer.STREAM)))
            else:
                writer.write(b'<?xml version="1.0" encoding="UTF-8"?>\n' + xml.encode())

        if self.offer_v1:
            send(f'<event version="2.0" uid="protouid" type="t-x-takp-v" time="{now_iso()}" start="{now_iso()}" '
                 f'stale="{now_iso(60)}" how="m-g"><point lat="0.0" lon="0.0" hae="0.0" ce="999999" le="999999"/>'
                 f'<detail><TakControl><TakProtocolSupport version="1"/><TakServerVersionInfo serverVersion="5.8-test"/>'
                 f'</TakControl></detail></event>')
        sent_contact = False
        try:
            while True:
                data = await asyncio.wait_for(reader.read(65536), 30)
                if not data:
                    break
                buf += data
                while buf:
                    if mode == "v1":
                        if buf[0] != 0xbf:
                            raise ValueError("bad magic")
                        n, shift, i = 0, 0, 1
                        while i < len(buf):
                            b = buf[i]
                            n |= (b & 0x7f) << shift
                            i += 1
                            if not b & 0x80:
                                break
                            shift += 7
                        else:
                            break
                        if len(buf) < i + n:
                            break
                        tm = takproto.parse_proto(bytearray(buf[:i + n]))
                        buf = buf[i + n:]
                        ev = tm.cotEvent
                        rec = {"mode": "v1", "uid": ev.uid, "type": ev.type, "xmlDetail": ev.detail.xmlDetail,
                               "callsign": ev.detail.contact.callsign, "endpoint": ev.detail.contact.endpoint,
                               "lat": ev.lat, "lon": ev.lon}
                    else:
                        e = buf.find(b"</event>")
                        if e < 0:
                            break
                        chunk, buf = buf[:e + 8], buf[e + 8:]
                        s = chunk.find(b"<event")
                        xml = chunk[s:].decode()
                        t = re.search(r'type="([^"]+)"', xml).group(1)
                        u = re.search(r'uid="([^"]+)"', xml).group(1)
                        cs = re.search(r'callsign="([^"]*)"', xml)
                        rec = {"mode": "xml", "uid": u, "type": t, "xmlDetail": xml, "callsign": cs.group(1) if cs else ""}
                        if t == "t-x-takp-q":
                            self.log.append("takp-q")
                            send(f'<event version="2.0" uid="protouid" type="t-x-takp-r" time="{now_iso()}" '
                                 f'start="{now_iso()}" stale="{now_iso(60)}" how="m-g"><point lat="0.0" lon="0.0" '
                                 f'hae="0.0" ce="999999" le="999999"/><detail><TakControl><TakResponse status="true"/>'
                                 f'</TakControl></detail></event>')
                            mode = "v1"
                            continue
                    self.events.append(rec)
                    if rec["type"] == "t-x-c-t":
                        self.pings += 1
                        send(f'<event version="2.0" uid="takPong" type="t-x-c-t-r" time="{now_iso()}" start="{now_iso()}" '
                             f'stale="{now_iso(20)}" how="h-g-i-g-o"><point lat="0" lon="0" hae="0" ce="9999999" '
                             f'le="9999999"/><detail/></event>')
                    if rec["type"] == "a-f-G-U-C" and rec["uid"] == UID and not sent_contact:
                        # The client is on the server: an ATAK user's SA and a direct chat to it.
                        sent_contact = True
                        send(sa_xml("ANDROID-peer", "BRAVO"))
                        await writer.drain()
                        await asyncio.sleep(0.3)
                        send(chat_xml("ANDROID-peer", "BRAVO", UID, CS, "status?"))
                await writer.drain()
        except (asyncio.TimeoutError, ConnectionError, ssl.SSLError, ValueError) as e:
            self.log.append("closed: " + str(e)[:120])
        finally:
            writer.close()


async def part_bc(tak, work):
    pki = make_ca(work)
    password = secrets.token_hex(12)
    elog = []
    es = enroll_server(pki, "omclient", password, elog)
    out = work + "/enrolled"
    os.makedirs(out)
    env = dict(os.environ, TAK_ENROLL_PASSWORD=password)
    rc, lines, err = run([tak, "enroll", "--host", "127.0.0.1", "--port", str(es.server_address[1]), "--user", "omclient",
                          "--uid", UID, "--ca", pki + "/ca.pem", "--out", out], env=env)
    transcript.append({"part": "B-enroll", "client": lines, "server": elog})
    check("B enrollment succeeded (tls/config + signClient/v2)", rc == 0 and lines and lines[0].get("ok"), (lines, err))
    sign = [l for l in elog if "signClient" in l["path"]]
    check("B CSR subject is CN=<user> plus the server's nameEntries, signature valid",
          sign and "CN=omclient" in sign[0]["csr_subject"] and "O=OneMind-Test" in sign[0]["csr_subject"] and
          "OU=TAK" in sign[0]["csr_subject"] and "verify OK" in sign[0]["csr_subject"] + sign[0]["csr_verify"], sign)
    check("B clientUid passed to signClient/v2", sign and sign[0]["client_uid"] == UID, sign)
    check("B private key written owner-only (0600)", os.path.exists(out + "/client.key") and
          (os.stat(out + "/client.key").st_mode & 0o777) == 0o600)
    rc, lines, _ = run([tak, "enroll", "--host", "127.0.0.1", "--port", str(es.server_address[1]), "--user", "omclient",
                        "--uid", UID, "--ca", pki + "/ca.pem", "--out", out], env=dict(os.environ, TAK_ENROLL_PASSWORD="wrong"))
    check("B wrong TAK password is refused", rc != 0 and lines and "rejected" in lines[0].get("error", ""), lines)
    rc, lines, _ = run([tak, "enroll", "--host", "127.0.0.1", "--port", str(es.server_address[1]), "--user", "omclient",
                        "--uid", UID, "--ca", pki + "/badca.pem", "--out", out], env=env)
    check("B enrollment refuses a server its CA does not trust", rc != 0 and lines and "tls" in lines[0].get("error", ""),
          lines)
    es.shutdown()

    cert = ["--cert", out + "/client.pem", "--key", out + "/client.key", "--ca", out + "/server-ca.pem"]

    async def stream(peer, *extra, seconds=7):
        p = await asyncio.create_subprocess_exec(tak, "stream", "--url", f"tls://127.0.0.1:{peer.port}", "--uid", UID,
                                                 "--callsign", CS, "--seconds", str(seconds), *extra,
                                                 stdout=asyncio.subprocess.PIPE)
        o, _ = await p.communicate()
        return [json.loads(l) for l in o.decode().splitlines() if l.strip()]

    # C1: v1 server, enrolled certificate, chat both ways, pings, every shape.
    peer = Peer(pki, offer_v1=True)
    await peer.start()
    lines = await stream(peer, *cert, "--chat-to", "ANDROID-peer", "--text", "all good", "--shapes", "--ping-idle-ms",
                         "1500", "--settle-ms", "1200")
    transcript.append({"part": "C1-v1", "client": lines, "peer_events": peer.events, "peer_log": peer.log})
    fin = next((l for l in lines if l["ev"] == "final"), {})
    check("C1 mutual TLS with the enrolled certificate (server sees CN=omclient)", peer.client_cn[:1] == ["omclient"],
          peer.client_cn)
    check("C1 TAK Protocol v1 negotiated (takp-q answered, client in v1)",
          "takp-q" in peer.log and fin.get("negotiated_v1") == 1 and fin.get("mode") == 2, (peer.log, fin))
    check("C1 server version read from the offer", fin.get("server_version") == "5.8-test", fin)
    sa = [e for e in peer.events if e["uid"] == UID and e["type"] == "a-f-G-U-C"]
    check("C1 client is a TAK contact: its SA reached the server with callsign and stcp endpoint",
          sa and sa[-1]["callsign"] == CS and sa[-1].get("endpoint", "*:-1:stcp") == "*:-1:stcp", sa[-1:])
    check("C1 server's user shows in the client's contact list", fin.get("visible_contacts") == ["BRAVO"], fin)
    chats = fin.get("chats", [])
    check("C1 chat server -> client", any(c["text"] == "status?" and not c["outgoing"] for c in chats), chats)
    out_chat = [e for e in peer.events if e["type"] == "b-t-f" and e["uid"].startswith("GeoChat." + UID)]
    check("C1 chat client -> server (direct, marti dest BRAVO)",
          out_chat and "all good" in out_chat[0]["xmlDetail"] and 'callsign="BRAVO"' in out_chat[0]["xmlDetail"] and
          'id="ANDROID-peer"' in out_chat[0]["xmlDetail"], out_chat)
    check("C1 keep-alive pings sent and answered", peer.pings >= 1 and fin.get("pings_out", 0) >= 1, (peer.pings, fin))
    drawn = {l["shape"]["uid"]: l["shape"] for l in lines if l["ev"] == "shape_sent" and not l["error"]}
    got = {e["uid"]: e for e in peer.events if e["uid"].startswith(UID + "-")}
    same = []
    for uid, s in drawn.items():
        e = got.get(uid)
        if not e:
            continue
        pts = links(e["xmlDetail"])
        want = [(round(p[0], 6), round(p[1], 6)) for p in s["points"]]
        radius = re.search(r'major="([^"]+)"', e["xmlDetail"])
        ok = pts == want and (not s["radius_m"] or abs(float(radius.group(1)) - s["radius_m"]) < 1e-6) and \
            abs(e["lat"] - s["center"][0]) < 1e-6 and abs(e["lon"] - s["center"][1]) < 1e-6
        rng = re.search(r'<range value="([^"]+)"', e["xmlDetail"])
        if s["range_m"]:
            ok = ok and rng and abs(float(rng.group(1)) - s["range_m"]) < 1e-3
        if ok:
            same.append(uid)
    check(f"C1 every shape decoded by the peer (v1) with the drawn geometry ({len(same)}/{len(drawn)})",
          len(drawn) == len(SHAPE_SUFFIXES) and len(same) == len(drawn), sorted(set(drawn) - set(same)))
    peer.server.close()

    # C2: XML-only server, channel filter Red hides BRAVO (team Cyan).
    peer = Peer(pki, offer_v1=False)
    await peer.start()
    lines = await stream(peer, *cert, "--channel", "Red", seconds=4)
    transcript.append({"part": "C2-xml-channel", "client": lines, "peer_log": peer.log})
    fin = next((l for l in lines if l["ev"] == "final"), {})
    check("C2 XML-only server: no negotiation, client stays on XML", fin.get("negotiated_v1") == 0 and fin.get("mode") == 0
          and "takp-q" not in peer.log, fin)
    check("C2 switching channel changes what is visible (BRAVO hidden under Red, still known)",
          fin.get("visible_contacts") == [] and fin.get("all_contacts") == 1, fin)
    peer.server.close()

    # C3: no valid client certificate -> the server refuses the TLS handshake.
    peer = Peer(pki, offer_v1=True)
    await peer.start()
    lines = await stream(peer, "--cert", pki + "/rogue.pem", "--key", pki + "/rogue.key", "--ca", pki + "/ca.pem", seconds=3)
    transcript.append({"part": "C3-rogue-cert", "client": lines})
    fin = next((l for l in lines if l["ev"] == "final"), {})
    # Under TLS 1.3 the client finishes its side of the handshake before the server checks
    # its certificate, so the refusal shows on the server: no session, no event, and the
    # client never receives anything.
    check("C3 a certificate from another CA is refused (server never accepts a session or an event)",
          peer.client_cn == [] and not peer.events and fin.get("events_in") == 0, (peer.client_cn, fin))
    peer.server.close()

    # C4: the server's certificate is from an unknown CA -> the client refuses.
    peer = Peer(pki, offer_v1=True, server_cert="badserver")
    await peer.start()
    lines = await stream(peer, *cert, seconds=3)
    transcript.append({"part": "C4-bad-server", "client": lines})
    fin = next((l for l in lines if l["ev"] == "final"), {})
    check("C4 the client refuses a server signed by an unknown CA",
          fin.get("connects") == 0 and "verify" in fin.get("last_error", "").lower(), fin)
    peer.server.close()


async def main(tak, omos, out):
    work = "/Users/danielcarku/Desktop/work/.run/tmp-m4-ue/c3-" + str(os.getpid())
    os.makedirs(work, mode=0o700)
    try:
        await part_a(tak, omos, work)
        await part_bc(tak, work)
    finally:
        os.makedirs(out, exist_ok=True)
        with open(out + "/c3-transcript.json", "w") as f:
            json.dump(transcript, f, indent=1)
        if os.path.exists(work + "/gw.log"):
            shutil.copy(work + "/gw.log", out + "/c3-gateway.log")
        shutil.rmtree(work, ignore_errors=True)


if __name__ == "__main__":
    asyncio.run(main(sys.argv[1], sys.argv[2], sys.argv[3]))
    passed = sum(1 for r in results if r[1])
    print(f"{passed}/{len(results)} checks passed")
    sys.exit(0 if passed == len(results) else 1)
