"""Verify an ingestion proof run: emitted vs stored, duplicates, rejects, feed status, KV.

usage: python -I verify.py --nats nats://127.0.0.1:34222 --emit-dir DIR [--since-seq-file F] [--label L]
Reads every message in INTEL, WORLD, TEL, FEEDEVT from the start (ordered consumer).
"""
import argparse
import asyncio
import collections
import json
import os
import time

import nats

SOURCE_OF_SUBJECT = [
    ("intel.seismic.usgs.", "usgs"), ("intel.weather.nws.", "nws"), ("intel.natural.eonet.", "eonet"),
    ("evt.world.sea.", "ais"), ("intel.alert.cap.", "cap"), ("tel.ws1.weather.", "mqtt"),
    ("intel.space.celestrak.", "celestrak"), ("intel.disaster.gdacs.", "gdacs"), ("intel.infra.outages.", "outages"),
]


def emitted_id(source, msgid):
    """Map a Nats-Msg-Id back to the stub's emitted id."""
    p = msgid.split(":")
    if source == "ais":
        return f"{p[1]}-{p[2]}"
    if source == "mqtt":
        return f"{p[1]}-{p[2]}"
    if source == "celestrak":
        return None  # compared by count of distinct (norad, epoch)
    if source == "outages":
        return p[1]
    return p[1]


async def read_stream(js, name):
    out = []
    info = await js.stream_info(name)
    last = info.state.last_seq
    if info.state.messages == 0:
        return out
    sub = await js.subscribe(">", stream=name, ordered_consumer=True)
    try:
        while True:
            m = await sub.next_msg(timeout=5)
            out.append(m)
            if m.metadata.sequence.stream >= last:
                break
    except asyncio.TimeoutError:
        pass
    await sub.unsubscribe()
    return out


async def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--nats", default="nats://127.0.0.1:34222")
    ap.add_argument("--emit-dir", required=True)
    ap.add_argument("--label", default="run")
    ap.add_argument("--grace", type=float, default=40.0, help="ignore ids emitted in the last N seconds")
    a = ap.parse_args()
    nc = await nats.connect(a.nats)
    js = nc.jetstream()
    stored = collections.defaultdict(list)
    rejects = collections.Counter()
    status = {}
    for s in ("INTEL", "WORLD", "TEL", "FEEDEVT"):
        for m in await read_stream(js, s):
            subj = m.subject
            mid = (m.headers or {}).get("Nats-Msg-Id", "")
            if subj.startswith("evt.feed."):
                d = json.loads(m.data)
                rejects[(d.get("source"), d.get("reason", "")[:90])] += 1
                continue
            if subj.startswith("tel.feed."):
                d = json.loads(m.data)
                status[d.get("source")] = d
                continue
            for prefix, src in SOURCE_OF_SUBJECT:
                if subj.startswith(prefix):
                    stored[src].append(mid)
                    break
    kv = await js.key_value("overwatch-video")
    video = {}
    try:
        for k in await kv.keys():
            video[k] = json.loads((await kv.get(k)).value)
    except Exception as e:  # noqa: BLE001
        video = {"error": str(e)}
    report = {"label": a.label, "sources": {}}
    for prefix, src in SOURCE_OF_SUBJECT:
        mids = stored.get(src, [])
        c = collections.Counter(mids)
        dups = {k: v for k, v in c.items() if v > 1}
        em_path = os.path.join(a.emit_dir, src + ".log")
        emitted = set()
        cutoff = time.time() - a.grace
        if os.path.exists(em_path):
            for line in open(em_path):
                parts = line.split()
                if len(parts) == 2 and float(parts[1]) <= cutoff:
                    emitted.add(parts[0])
        got = {emitted_id(src, x) for x in c} - {None}
        row = {"stored": len(mids), "distinctMsgIds": len(c), "duplicates": sum(v - 1 for v in dups.values()),
               "emitted": len(emitted)}
        if src != "celestrak" and emitted:
            missing = sorted(emitted - got)
            row["missingVsEmitted"] = len(missing)
            row["missingSample"] = missing[:5]
        nums = sorted({int("".join(ch for ch in (emitted_id(src, x) or "").split("-")[-1] if ch.isdigit()) or -1)
                       for x in c if src in ("usgs", "nws", "eonet", "cap", "mqtt", "gdacs")} - {-1})
        gaps = [(a2, b2) for a2, b2 in zip(nums, nums[1:]) if b2 != a2 + 1]
        if nums:
            row["storedSeqRange"] = [nums[0], nums[-1]]
            row["storedSeqGaps"] = [f"{x + 1}..{y - 1}" for x, y in gaps][:10]
        report["sources"][src] = row
    report["rejects"] = [{"source": k[0], "reason": k[1], "count": v} for k, v in rejects.most_common()]
    report["latestStatus"] = {k: {kk: v.get(kk) for kk in ("engine", "fetchedAt", "ageSec", "count", "lastError", "stale")}
                              for k, v in sorted(status.items())}
    report["videoRegistryKV"] = video
    print(json.dumps(report, indent=1, default=str))
    await nc.close()


if __name__ == "__main__":
    asyncio.run(main())
