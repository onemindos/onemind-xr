#!/usr/bin/env python3
"""Publish entity fixtures (JetStream, acked), each as the user that owns its subject under A19.
Usage: pubfx.py URL PASSWORDS_JSON FIXTURES_JSONL. Passwords are passed via the environment only."""
import json, os, subprocess, sys

url, pwfile, fx = sys.argv[1], sys.argv[2], sys.argv[3]
pw = json.load(open(pwfile))
OWNER = [("tak.cot.inject", "projector"), ("tak.cot.out", "tak-client"), ("tak.cot.mesh.rx", "mesh-gateway"),
         ("tel.fleet.drone.sim-x1.", "bridge-mavlink-home"), ("evt.world.", "ingest"), ("intel.", "ingest")]
n = {}
for line in open(fx):
    f = json.loads(line)
    user = next((u for p, u in OWNER if f["subject"].startswith(p)), None)
    if user is None:
        print("no owner for", f["subject"]); continue
    env = dict(os.environ, NATS_USER=user, NATS_PASSWORD=pw[user])
    r = subprocess.run(["nats", "-s", url, "pub", "--jetstream", f["subject"], f["data"]], env=env, capture_output=True, text=True)
    key = (user, r.returncode == 0)
    n[key] = n.get(key, 0) + 1
    if r.returncode != 0:
        print(f"publish {f['subject']} as {user} failed: {r.stderr.strip()[-200:]}")
for (user, ok), c in sorted(n.items()):
    print(f"  {c:3d} fixture(s) published as {user}: {'acked' if ok else 'FAILED'}")
