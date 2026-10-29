"""Upstream stubs for the OneMind ingestion proof (M0, ingestion lane part c).

One process imitates every delivery mode with deterministic, synthetic data:
  HTTP 127.0.0.1:38080
    /usgs.geojson            REST JSON poll, ETag/If-None-Match, 429 every 7th request
    /nws/longpoll?cursor=N   long poll, blocks up to 10 s for events after N
    /eonet/sse               Server-Sent Events, one event every 2 s
    /celestrak/stations.txt  bulk file (TLE text), Last-Modified/If-Modified-Since
    /gdacs/rss.xml           RSS 2.0
    /outages.html            HTML page to scrape (table rows)
    /tiles/{z}/{x}/{y}.png   XYZ tile (probe only; tiles never go on the bus)
    /control?mode=ok|down|bad  ok, all endpoints 503, or malformed records
  WebSocket 127.0.0.1:38081/ais   AIS-like positions at --ais-rate msg/s (load source)
  Webhook pusher: POSTs CAP-like JSON to --webhook-url every 3 s
  MQTT publisher: tcp://127.0.0.1:31883 topic sensors/ws1/reading, QoS 1, every 1 s

Every emitted record id is appended to <emit-dir>/<source>.log so the verifier
can compare emitted vs stored (gaps and duplicates).
Synthetic data only. No real upstream is contacted.
"""
import argparse
import asyncio
import email.utils
import hashlib
import json
import os
import threading
import time
import urllib.request
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlparse

STATE = {"mode": "ok"}
LOCK = threading.Condition()
T0 = time.time()
EMIT_DIR = "."
_emitted = {}


def emit(source, rid):
    key = (source, rid)
    if key in _emitted:
        return
    _emitted[key] = True
    with open(os.path.join(EMIT_DIR, source + ".log"), "a") as f:
        f.write(f"{rid} {time.time():.3f}\n")


def seq(period):
    return int((time.time() - T0) // period)


def quakes():
    n = seq(5)
    out = []
    for i in range(max(0, n - 19), n + 1):
        out.append({"type": "Feature", "id": f"q{i}",
                    "properties": {"mag": round(2.5 + (i % 30) / 10, 1), "place": f"stub place {i}",
                                   "time": int((T0 + i * 5) * 1000)},
                    "geometry": {"type": "Point", "coordinates": [-120 + i % 40, 35 + (i % 10) / 10, 10.0]}})
    return out


def alerts_after(cursor):
    n = seq(4)
    if cursor < 0:  # no cursor: recent items only, like real feeds
        cursor = max(-1, n - 10)
    return n, [{"id": f"a{i}", "event": "Severe Thunderstorm Warning", "area": f"zone {i % 9}",
                "sent": int((T0 + i * 4) * 1000)} for i in range(cursor + 1, n + 1)]


def gdacs_items():
    n = seq(10)
    return [(f"g{i}", f"Stub disaster {i}", 10 + i % 50, 20 + i % 40) for i in range(max(0, n - 9), n + 1)]


def outages():
    n = seq(15)
    return [(f"o{i}", f"Substation {i % 12}", 100 + i * 3) for i in range(max(0, n - 4), n + 1)]


def tle_version():
    return seq(60)


def tle_text():
    v = tle_version()
    lines = []
    for sat in range(25544, 25549):
        lines.append(f"STUB-SAT-{sat}")
        lines.append(f"1 {sat}U 98067A   26302.{v:08d}  .00016717  00000-0  10270-3 0  9001")
        lines.append(f"2 {sat}  51.6416 247.4627 0006703 130.5360 325.0288 15.72125391 {v:05d}")
    return "\n".join(lines) + "\n"


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"
    counter = {"usgs": 0}

    def log_message(self, fmt, *args):
        pass

    def send(self, code, body=b"", ctype="application/json", headers=None):
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        for k, v in (headers or {}).items():
            self.send_header(k, v)
        self.end_headers()
        if body:
            self.wfile.write(body)

    def do_HEAD(self):
        self.do_GET(head=True)

    def do_GET(self, head=False):
        u = urlparse(self.path)
        q = parse_qs(u.query)
        if u.path == "/control":
            STATE["mode"] = q.get("mode", ["ok"])[0]
            return self.send(200, json.dumps(STATE).encode())
        if STATE["mode"] == "down":
            return self.send(503, b'{"error":"stub down"}')
        bad = STATE["mode"] == "bad"
        if u.path == "/usgs.geojson":
            Handler.counter["usgs"] += 1
            if Handler.counter["usgs"] % 7 == 0:
                return self.send(429, b'{"error":"rate limited"}', headers={"Retry-After": "2"})
            feats = quakes()
            if bad:
                feats[-1] = {"type": "Feature", "id": feats[-1]["id"] + "-bad", "properties": {"mag": "not-a-number"}}
            body = json.dumps({"type": "FeatureCollection", "features": feats}).encode()
            etag = '"' + hashlib.sha1(body).hexdigest()[:16] + '"'
            if self.headers.get("If-None-Match") == etag:
                return self.send(304, headers={"ETag": etag})
            for f in feats:
                if "geometry" in f:
                    emit("usgs", f["id"])
            return self.send(200, b"" if head else body, "application/geo+json", {"ETag": etag})
        if u.path == "/nws/longpoll":
            cursor = int(q.get("cursor", ["-1"])[0])
            deadline = time.time() + 10
            n, items = alerts_after(cursor)
            while not items and time.time() < deadline:
                time.sleep(0.25)
                n, items = alerts_after(cursor)
            for a in items:
                emit("nws", a["id"])
            return self.send(200, json.dumps({"cursor": n, "alerts": items}).encode())
        if u.path == "/eonet/sse":
            self.send_response(200)
            self.send_header("Content-Type", "text/event-stream")
            self.send_header("Cache-Control", "no-cache")
            self.send_header("Connection", "close")
            self.end_headers()
            self.close_connection = True
            last = int(self.headers.get("Last-Event-ID", seq(2) - 1))
            try:
                while STATE["mode"] != "down":
                    n = seq(2)
                    while last < n:
                        last += 1
                        ev = {"id": f"e{last}", "title": f"Stub wildfire {last}", "lon": -100 + last % 30, "lat": 40}
                        emit("eonet", ev["id"])
                        self.wfile.write(f"id: {last}\ndata: {json.dumps(ev)}\n\n".encode())
                        self.wfile.flush()
                    time.sleep(0.2)
            except (BrokenPipeError, ConnectionResetError):
                pass
            return
        if u.path == "/celestrak/stations.txt":
            v = tle_version()
            lm = email.utils.formatdate(T0 + v * 60, usegmt=True)
            ims = self.headers.get("If-Modified-Since")
            if ims and ims == lm:
                return self.send(304, headers={"Last-Modified": lm})
            for sat in range(25544, 25549):
                emit("celestrak", f"{sat}-{v}")
            return self.send(200, b"" if head else tle_text().encode(), "text/plain", {"Last-Modified": lm})
        if u.path == "/gdacs/rss.xml":
            items = "".join(
                f"<item><guid>{g}</guid><title>{t}</title><geo:lat>{la}</geo:lat><geo:long>{lo}</geo:long></item>"
                for g, t, la, lo in gdacs_items())
            for g, *_ in gdacs_items():
                emit("gdacs", g)
            body = ('<?xml version="1.0"?><rss version="2.0" xmlns:geo="http://www.w3.org/2003/01/geo/wgs84_pos#">'
                    f"<channel><title>stub</title>{items}</channel></rss>").encode()
            return self.send(200, body, "application/rss+xml")
        if u.path == "/outages.html":
            rows = "".join(f'<tr data-id="{i}"><td class="name">{n}</td><td class="cust">{c}</td></tr>'
                           for i, n, c in outages())
            for i, *_ in outages():
                emit("outages", i)
            body = f"<html><body><h1>Outages</h1><table id='outages'>{rows}</table></body></html>".encode()
            return self.send(200, body, "text/html")
        if u.path.startswith("/tiles/"):
            png = bytes.fromhex("89504e470d0a1a0a0000000d4948445200000001000000010806000000"
                                "1f15c4890000000d49444154789c6360000002000154a24f5d0000000049454e44ae426082")
            return self.send(200, b"" if head else png, "image/png", {"Cache-Control": "max-age=86400"})
        return self.send(404, b'{"error":"not found"}')


def webhook_pusher(url):
    n = 0
    log = open(os.path.join(EMIT_DIR, "webhook-responses.log"), "a")
    while True:
        time.sleep(3)
        if STATE["mode"] == "down":
            continue
        n += 1
        body = json.dumps({"identifier": f"c{n}", "event": "Flood Warning", "severity": "Severe",
                           "sent": int(time.time() * 1000), "area": {"lat": 41.0, "lon": -74.0}}).encode()
        code = "ERR"
        for attempt in range(5):  # a polite pusher retries on non-2xx, like real webhook senders
            try:
                req = urllib.request.Request(url, data=body, headers={"Content-Type": "application/json"})
                with urllib.request.urlopen(req, timeout=5) as r:
                    code = str(r.status)
                    break
            except Exception as e:  # noqa: BLE001
                code = getattr(e, "code", type(e).__name__)
                time.sleep(1 + attempt)
        emit("cap", f"c{n}")
        log.write(f"{time.time():.3f} c{n} {code}\n")
        log.flush()


def mqtt_publisher(host, port):
    import paho.mqtt.client as mqtt
    c = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="stub-ws1", clean_session=True)
    while True:
        try:
            c.connect(host, port, keepalive=10)
            break
        except OSError:
            time.sleep(1)
    c.loop_start()
    n = 0
    while True:
        time.sleep(1)
        if STATE["mode"] == "down":
            continue
        n += 1
        info = c.publish("sensors/ws1/reading", json.dumps({"station": "ws1", "seq": n, "tempC": 12 + n % 7}), qos=1)
        try:
            info.wait_for_publish(timeout=2)
            if info.is_published():
                emit("mqtt", f"ws1-{n}")
        except (RuntimeError, ValueError):
            pass


async def ais_server(rate):
    import websockets

    async def handler(ws):
        ships = [366000000 + i for i in range(200)]
        i = 0
        period = 1.0 / rate
        while True:
            if STATE["mode"] == "down":
                await ws.close()
                return
            m = ships[i % len(ships)]
            ts = int(time.time() * 1000)
            rid = f"{m}-{ts}"
            if STATE["mode"] == "bad" and i % 50 == 0:
                await ws.send("{not json")
            else:
                await ws.send(json.dumps({"mmsi": m, "ts": ts, "lat": 40 + (i % 100) / 100, "lon": -73 - (i % 70) / 100,
                                          "sog": 12.5}))
                emit("ais", rid)
            i += 1
            await asyncio.sleep(period)

    async with websockets.serve(handler, "127.0.0.1", 38081):
        await asyncio.Future()


def main():
    global EMIT_DIR
    p = argparse.ArgumentParser()
    p.add_argument("--emit-dir", required=True)
    p.add_argument("--ais-rate", type=float, default=50)
    p.add_argument("--webhook-url", default="")
    p.add_argument("--mqtt", default="127.0.0.1:31883")
    a = p.parse_args()
    EMIT_DIR = a.emit_dir
    os.makedirs(EMIT_DIR, exist_ok=True)
    srv = ThreadingHTTPServer(("127.0.0.1", 38080), Handler)
    srv.daemon_threads = True
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    if a.webhook_url:
        threading.Thread(target=webhook_pusher, args=(a.webhook_url,), daemon=True).start()
    if a.mqtt:
        h, prt = a.mqtt.split(":")
        threading.Thread(target=mqtt_publisher, args=(h, int(prt)), daemon=True).start()
    print("stub up", flush=True)
    asyncio.run(ais_server(a.ais_rate))


if __name__ == "__main__":
    main()
