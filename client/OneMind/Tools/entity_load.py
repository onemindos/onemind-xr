#!/usr/bin/env python3
"""Synthetic entity load for the 10,000-entity check (FEATURES B4/B5, OWNER-STEPS.md).

Publishes N simulated drones (`sim-load-{n}`, indicators.simulated true, decision 0004 #13)
as entity.v1 records on `ent.drone.{id}.state` into the ENT stream, then moves every one
of them `--rate` times per second. It stands in for the omos entity service output so the
client's fleet layer (direct ENT path) can be loaded on the Mac without real feeds.

  python entity_load.py --url nats://127.0.0.1:4222 --count 10000 --rate 1 --duration 120

Creates ENT with the settings of omos platform/nats/streams/ent.yaml (R1) if it is
missing. Credentials, if any, come from NATS_USER / NATS_PASSWORD in the environment.
Needs: pip install nats-py
"""
import argparse, asyncio, json, math, os, time

import nats

ENT_CONFIG = {"name": "ENT", "subjects": ["ent.*.*.state"], "retention": "limits", "max_msgs_per_subject": 1,
              "max_bytes": 1073741824, "max_age": 604800000000000, "discard": "old", "storage": "file",
              "num_replicas": 1, "duplicate_window": 120000000000, "allow_direct": True, "allow_msg_ttl": True,
              "subject_delete_marker_ttl": 900000000000}


def record(i, seq, now_ms, lat0, lon0, t):
    # Entities on a grid around (lat0, lon0), each circling slowly.
    gx, gy = i % 100, i // 100
    lat = lat0 + (gy - 50) * 0.01 + 0.002 * math.sin(t / 30.0 + i)
    lon = lon0 + (gx - 50) * 0.01 + 0.002 * math.cos(t / 30.0 + i)
    eid = f"sim-load-{i}"
    return eid, {
        "uid": eid, "type": "fleet.drone", "lat": lat, "lon": lon, "hae": 150.0, "course": (t * 6 + i) % 360,
        "speed": 12.0, "callsign": eid.upper(), "stale": 60, "t": now_ms,
        "entity_id": eid, "entity_type": "drone", "confidence": 0.9,
        "properties": {"callsign": eid.upper(), "altitude": 150.0, "speed": 12.0, "course": 0, "ce": 5.0, "le": 3.0},
        "v": 1, "seq": seq, "live": True, "lifecycle": "active", "created_t": now_ms, "expiry_t": now_ms + 120000,
        "pinned": False, "template": "asset", "traits": ["locatable", "trackable"], "disposition": "friendly",
        "environment": "air", "cot_type": "a-f-A-M-H-Q", "aliases": {"name": eid.upper(), "ids": []},
        "indicators": {"simulated": True, "exercise": True, "emergency": False, "starred": False, "egress_tak": False},
        "marking": {"tlp": "CLEAR", "releasable_to": []},
        "provenance": {"primary": "entity_load.py", "how": "m-s", "source_t": now_ms},
        "sources": [], "confidence_parts": {"source_prior": 0.9, "track_quality": 1.0, "corroboration": 0.9},
        "relationships": [],
    }


async def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--url", default="nats://127.0.0.1:4222")
    ap.add_argument("--count", type=int, default=10000)
    ap.add_argument("--rate", type=float, default=1.0, help="updates per entity per second")
    ap.add_argument("--duration", type=float, default=60.0)
    ap.add_argument("--lat", type=float, default=30.2672)
    ap.add_argument("--lon", type=float, default=-97.7431)
    ap.add_argument("--remove", action="store_true", help="publish live:false for every entity and exit")
    a = ap.parse_args()
    opts = {}
    if os.environ.get("NATS_USER"):
        opts = {"user": os.environ["NATS_USER"], "password": os.environ.get("NATS_PASSWORD", "")}
    nc = await nats.connect(a.url, name="onemind-entity-load", **opts)
    r = await nc.request("$JS.API.STREAM.INFO.ENT", b"", timeout=5)
    if b'"error"' in r.data:
        r = await nc.request("$JS.API.STREAM.CREATE.ENT", json.dumps(ENT_CONFIG).encode(), timeout=5)
        print("created ENT:", "ok" if b'"config"' in r.data else r.data[:200])
    js = nc.jetstream()
    seq = int(time.time() * 1000)  # strictly increasing across runs
    t0 = time.time()
    rounds = 0
    while True:
        now = time.time()
        now_ms = int(now * 1000)
        seq += 1
        futs = []
        for i in range(a.count):
            eid, rec = record(i, seq, now_ms, a.lat, a.lon, now - t0)
            if a.remove:
                rec["live"] = False
            subj = f"ent.drone.{eid}.state"
            futs.append(js.publish(subj, json.dumps(rec).encode(), headers={"Nats-Msg-Id": f"{eid}:{seq}"}))
            if len(futs) >= 1000:
                await asyncio.gather(*futs)
                futs = []
        if futs:
            await asyncio.gather(*futs)
        rounds += 1
        took = time.time() - now
        print(f"round {rounds}: {a.count} entities published in {took:.2f} s", flush=True)
        if a.remove or now - t0 >= a.duration:
            break
        await asyncio.sleep(max(0.0, 1.0 / a.rate - took))
    await nc.close()


asyncio.run(main())
