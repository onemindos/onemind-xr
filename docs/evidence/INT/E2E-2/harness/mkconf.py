#!/usr/bin/env python3
"""E2E integration: render the disposable nats-server config and JetStream configs.

Inputs (all from the branch worktrees, nothing invented):
  - A19 identities render (scripts/nats_identities.py conf --stage B) -> accounts.json
  - NACK Stream/KeyValue CRs from feat/stream-caps (base), feat/entity-service (ENT,
    kv-entity-correlation) and feat/ingest-framework (kv-ingest-*), replicas forced to 1.

E2E-2: no deviations from the A19 render. accounts.json comes from
`nats_identities.py conf --stage B --test-users` (adds the branch's test_only second human,
alice). hermes stays LEAFNODE only and connects through its stand-in sidecar
(`leaf-conf --remote --plain`), so the hub gets a leafnode listener on 127.0.0.1.
Usage: mkconf.py RUNDIR NATS_PORT WS_PORT LEAF_PORT
"""
import copy, json, re, secrets, sys
from pathlib import Path
import yaml

run = Path(sys.argv[1]); port = int(sys.argv[2]); ws = int(sys.argv[3])
src = Path("/Users/danielcarku/Desktop/work/.run/tmp-e2e/src")
acc = json.loads((run / "accounts.json").read_text())
pw = json.loads((run / "passwords.json").read_text())

leaf = int(sys.argv[4])

conf = {
    "listen": f"127.0.0.1:{port}",
    "http": "127.0.0.1:0",
    "jetstream": {"store_dir": str(run / "js"), "max_mem": 268435456, "max_file": 68719476736},
    "websocket": {"listen": f"127.0.0.1:{ws}", "no_tls": True},
    "leafnodes": {"host": "127.0.0.1", "port": leaf},
    **acc,
}
(run / "nats.conf").write_text(json.dumps(conf, indent=1))

def dur(s):
    if s is None: return 0
    m = re.fullmatch(r"(\d+)(ms|s|m|h|d)", str(s))
    n, u = int(m.group(1)), m.group(2)
    return n * {"ms": 10**6, "s": 10**9, "m": 60 * 10**9, "h": 3600 * 10**9, "d": 86400 * 10**9}[u]

def stream_cfg(spec):
    c = {"name": spec["name"], "subjects": spec.get("subjects", []), "retention": spec.get("retention", "limits"),
         "storage": spec.get("storage", "file"), "discard": spec.get("discard", "old"), "num_replicas": 1,
         "max_age": dur(spec.get("maxAge")), "max_bytes": spec.get("maxBytes", -1), "max_msgs": spec.get("maxMsgs", -1),
         "max_msgs_per_subject": spec.get("maxMsgsPerSubject", -1), "max_msg_size": spec.get("maxMsgSize", -1),
         "duplicate_window": dur(spec.get("duplicateWindow", "2m")), "allow_direct": spec.get("allowDirect", False),
         "deny_delete": spec.get("denyDelete", False), "deny_purge": spec.get("denyPurge", False),
         "allow_rollup_hdrs": spec.get("allowRollup", False)}
    if spec.get("discardPerSubject"): c["discard_new_per_subject"] = True
    if spec.get("allowMsgTtl"): c["allow_msg_ttl"] = True
    if spec.get("subjectDeleteMarkerTtl"): c["subject_delete_marker_ttl"] = dur(spec["subjectDeleteMarkerTtl"])
    return c

def kv_cfg(spec):
    b = spec["bucket"]
    c = {"name": "KV_" + b, "subjects": [f"$KV.{b}.>"], "retention": "limits", "storage": spec.get("storage", "file"),
         "discard": "new", "num_replicas": 1, "max_msgs_per_subject": spec.get("history", 1),
         "max_age": dur(spec.get("ttl")), "max_bytes": spec.get("maxBytes", -1), "max_msg_size": spec.get("maxValueSize", -1),
         "duplicate_window": dur("2m"), "allow_direct": True, "allow_rollup_hdrs": True, "deny_delete": True, "deny_purge": False}
    if spec.get("ttl") and c["duplicate_window"] > c["max_age"]: c["duplicate_window"] = c["max_age"]
    return c

wanted_streams = {"CMD", "EVT", "MISSION", "TEL", "COT", "INTEL", "WORLD", "SYS", "DET", "ENT"}
wanted_kv = {"geofences", "mission-state", "world-state", "fleet-state", "entity-correlation", "ingest-locks", "ingest-state", "feed-config"}
files = sorted((src / "stream-caps/platform/nats/streams").glob("*.yaml"))
files += [src / "entity-service/platform/nats/streams/ent.yaml", src / "entity-service/platform/nats/streams/kv-entity-correlation.yaml",
          src / "ingest-framework/platform/nats/streams/kv-ingest-locks.yaml", src / "ingest-framework/platform/nats/streams/kv-ingest-state.yaml"]
out, origin = [], {}
for f in files:
    for doc in yaml.safe_load_all(f.read_text()):
        if not doc: continue
        sp = doc["spec"]
        if doc["kind"] == "Stream" and sp["name"] in wanted_streams:
            out.append({"kind": "stream", "config": stream_cfg(sp)}); origin[sp["name"]] = str(f.relative_to(src))
        elif doc["kind"] == "KeyValue" and sp["bucket"] in wanted_kv:
            out.append({"kind": "kv", "config": kv_cfg(sp)}); origin["KV_" + sp["bucket"]] = str(f.relative_to(src))
(run / "streams.json").write_text(json.dumps(out, indent=1))
(run / "streams-origin.json").write_text(json.dumps(origin, indent=1))
print("streams:", sorted(origin))
missing = (wanted_streams | {"KV_" + k for k in wanted_kv}) - set(origin)
print("missing:", sorted(missing))
