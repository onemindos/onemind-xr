#!/usr/bin/env python3
"""Stand-in for the omos projector's `svc.cot.send` endpoint, for local tests only.

The real endpoint belongs to the omos projector (cot-projection.md §1, §9) and is not written
yet. This stand-in applies the checks the projector must apply (reserved uid prefixes, kind,
marking gate) and core-publishes the CoT to `tak.cot.mesh.tx` (proposal CP-3), where the omos
mesh gateway sends it to the TAK mesh. It never publishes `tak.cot.inject`.

  python svc_cot_send_standin.py --url nats://127.0.0.1:4222
Needs: pip install nats-py. NATS_USER / NATS_PASSWORD from the environment if required.
"""
import argparse, asyncio, json, os

import nats

KINDS = ("marker", "spi", "route", "geofence", "chat", "shape")  # shape: B11 (Docs/svc-cot-send.md)


async def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--url", default="nats://127.0.0.1:4222")
    a = ap.parse_args()
    opts = {}
    if os.environ.get("NATS_USER"):
        opts = {"user": os.environ["NATS_USER"], "password": os.environ.get("NATS_PASSWORD", "")}
    nc = await nats.connect(a.url, name="svc-cot-send-standin", **opts)

    async def handler(msg):
        def reply(obj):
            return msg.respond(json.dumps(obj).encode())
        try:
            req = json.loads(msg.data)
        except ValueError:
            await reply({"v": 1, "error": "invalid", "detail": "bad JSON"})
            return
        cot = req.get("cot", "")
        if req.get("v") != 1 or req.get("kind") not in KINDS or not cot.startswith("<event"):
            await reply({"v": 1, "error": "invalid", "detail": "v, kind or cot"})
            return
        if 'uid="OM.' in cot or 'uid="INTEL.' in cot:
            await reply({"v": 1, "error": "forbidden", "detail": "reserved uid prefix"})
            return
        if req.get("marking", {}).get("tlp") not in ("CLEAR", "GREEN"):
            await reply({"v": 1, "error": "forbidden", "detail": "marking gate"})
            return
        targets = []
        if "mesh" in req.get("targets", []):
            await nc.publish("tak.cot.mesh.tx", cot.encode())  # core publish, no reply (CP-3)
            targets.append("mesh")
        uid = cot.split('uid="', 1)[1].split('"', 1)[0] if 'uid="' in cot else ""
        print(f"svc.cot.send {req.get('kind')} uid={uid} -> {targets}", flush=True)
        await reply({"v": 1, "ok": True, "uid": uid, "targets": targets})

    await nc.subscribe("svc.cot.send", cb=handler)
    print(f"svc.cot.send stand-in on {a.url} (mesh target only; Ctrl-C to stop)", flush=True)
    while True:
        await asyncio.sleep(3600)


asyncio.run(main())
