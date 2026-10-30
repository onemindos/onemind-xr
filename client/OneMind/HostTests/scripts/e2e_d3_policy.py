#!/usr/bin/env python3
# D3 end to end on the build machine: the client's click-to-task (onemind_tak task, the same
# om::AssetTasking the Unreal plugin uses) against the REAL omos policy service built from PR
# #554 (branch feat/policy-service), on a disposable nats-server with the per-identity account
# layout of that PR's test harness (ONEMIND, CLIENT with a share:true service import, AGENTS).
# The entity service and the MAVLink bridge are stubs that follow asset-command.md §6.4.
#
# Checks (asset-command.md §9 and FEATURES D3):
#   1  a goto proposed by the client waits; no cmd.fleet.> is published before the human confirms
#   2  after confirm, exactly one cmd.fleet.drone.sim-x1.goto with the clicked point, signed approval
#      bound to the same params_hash the client computed; audit proposed..executed on evt.so.action.*
#   3  a goto into the keep-out zone is denied (precondition_failed), nothing dispatched
#   4  a human rejection ends the action (rejected), nothing dispatched
#   5  a non-sim id proposed by hand is denied not_simulated by policy
#   6  the client's NATS user cannot publish cmd.fleet.> (permissions violation from the server)
#
# Usage: e2e_d3_policy.py <onemind_tak binary> <omos repo> <out dir>
# Passwords are random per run, live only in the temp dir, and are never printed.
# SPDX-License-Identifier: GPL-3.0-or-later
import asyncio
import json
import os
import re
import secrets
import shutil
import socket
import subprocess
import sys
import tempfile
import time

import nats

# Tools: $ONEMIND_TOOLS, else work/.tools beside the onemind-xr checkout (missing tools come from PATH).
WORK = os.environ.get("ONEMIND_WORK") or os.path.abspath(os.path.join(os.path.dirname(__file__), *[".."] * 5))
TOOLS = os.environ.get("ONEMIND_TOOLS") or os.path.join(WORK, ".tools")
NATS_SERVER = os.environ.get("NATS_SERVER_BIN") or (TOOLS + "/bin/nats-server" if os.access(TOOLS + "/bin/nats-server", os.X_OK) else shutil.which("nats-server") or "nats-server")
GOENV = dict(os.environ, PATH=TOOLS + "/bin:" + os.environ.get("PATH", ""), GOFLAGS="-mod=mod")
if os.path.isdir(TOOLS + "/gopath"):  # the build machine's offline module cache
    GOENV.update(GOPATH=TOOLS + "/gopath", GOMODCACHE=TOOLS + "/gopath/pkg/mod", GOPROXY="off")
VERBS = ["goto", "hold", "rtl", "land", "orbit", "observe", "estop", "arm", "disarm", "takeoff", "estop_release",
         "geofence_override"]
results = []


def check(name, ok, detail=""):
    results.append((name, bool(ok), detail))
    print(("PASS " if ok else "FAIL ") + name + ("" if ok else "  " + str(detail)), flush=True)


def free_port():
    s = socket.socket()
    s.bind(("127.0.0.1", 0))
    p = s.getsockname()[1]
    s.close()
    return p


def nats_conf(store, port, ws, pw):
    return f"""
listen: "127.0.0.1:{port}"
jetstream {{ store_dir: "{store}", max_mem: 64MB, max_file: 256MB }}
websocket {{ listen: "127.0.0.1:{ws}", no_tls: true }}
accounts {{
  ONEMIND {{
    jetstream: enabled
    users: [
      {{ user: policy, password: "{pw['policy']}", permissions: {{
          publish: {{ allow: ["cmd.>", "evt.so.action.>", "svc.entity.get", "$JS.API.>", "_INBOX.>"] }}
          subscribe: {{ allow: ["svc.policy.>", "evt.drone.>", "evt.robot.>", "evt.vehicle.>", "evt.mission.>", "evt.entity.>", "_INBOX.>"] }}
          allow_responses: true }} }}
      {{ user: entity, password: "{pw['entity']}", permissions: {{
          publish: {{ allow: ["_INBOX.>"] }}, subscribe: {{ allow: ["svc.entity.>"] }}, allow_responses: true }} }}
      {{ user: bridge, password: "{pw['bridge']}", permissions: {{
          publish: {{ allow: ["evt.drone.sim-x1.>", "tel.fleet.drone.sim-x1.>"] }}
          subscribe: {{ allow: ["cmd.fleet.drone.sim-x1.>"] }} }} }}
      {{ user: admin, password: "{pw['admin']}" }}
    ]
    exports: [ {{ service: "svc.policy.>" }}, {{ stream: "evt.so.action.>" }} ]
  }}
  CLIENT {{
    users: [
      {{ user: zeus, password: "{pw['zeus']}", permissions: {{
          publish: {{ allow: ["svc.policy.>", "cmd.map.>"] }}, subscribe: {{ allow: ["_INBOX.>", "cmd.map.>", "evt.so.action.>"] }} }} }}
    ]
    imports: [
      {{ service: {{ account: ONEMIND, subject: "svc.policy.>" }}, share: true }}
      {{ stream: {{ account: ONEMIND, subject: "evt.so.action.>" }} }}
    ]
  }}
}}
"""


async def main(tak_bin, omos, out):
    work = os.path.join(tempfile.gettempdir(), "onemind-m4-ue", "d3-" + str(os.getpid()))
    os.makedirs(work, mode=0o700)
    src = work + "/src"
    os.makedirs(src)
    subprocess.run(f"git -C {omos} archive origin/feat/policy-service services/policy services/schemas "
                   f"platform/security/openfga | tar -x -C {src}", shell=True, check=True)
    commit = subprocess.run(["git", "-C", omos, "rev-parse", "origin/feat/policy-service"], capture_output=True,
                            text=True).stdout.strip()
    b = subprocess.run(["go", "build", "-o", work + "/policy", "./cmd/policy"], cwd=src + "/services/policy",
                       env=GOENV, capture_output=True, text=True)
    check("policy service builds from feat/policy-service " + commit[:8], b.returncode == 0, b.stderr[-400:])
    if b.returncode:
        return
    pw = {u: secrets.token_hex(16) for u in ["policy", "entity", "bridge", "admin", "zeus"]}
    port, ws, health = free_port(), free_port(), free_port()
    conf = work + "/nats.conf"
    with open(conf, "w") as f:
        f.write(nats_conf(work + "/js", port, ws, pw))
    os.chmod(conf, 0o600)
    srv = subprocess.Popen([NATS_SERVER, "-c", conf, "-l", work + "/nats.log"])
    url = f"nats://127.0.0.1:{port}"
    for _ in range(100):
        try:
            socket.create_connection(("127.0.0.1", port), 0.1).close()
            socket.create_connection(("127.0.0.1", ws), 0.1).close()
            break
        except OSError:
            time.sleep(0.05)
    procs = [srv]
    transcript = []
    try:
        admin = await nats.connect(url, user="admin", password=pw["admin"])
        js = admin.jetstream()
        await js.add_stream(name="CMD", subjects=["cmd.>"], discard="new", duplicate_window=120)
        await js.add_stream(name="EVT", subjects=["evt.so.>", "evt.drone.>", "evt.robot.>"], duplicate_window=120)
        await js.add_stream(name="MISSION", subjects=["evt.mission.>"])
        kv = await js.create_key_value(bucket="geofences", history=10)
        await kv.put("zone-pond", json.dumps({"entity_id": "zone-pond", "entity_type": "zone", "live": True, "geo": {
            "shape": "polygon", "kind": "keep_out", "coords": {"type": "Polygon", "coordinates": [[
                [-97.705, 30.295], [-97.695, 30.295], [-97.695, 30.305], [-97.705, 30.305], [-97.705, 30.295]]]}}}).encode())
        await kv.put("zone-home", json.dumps({"entity_id": "zone-home", "entity_type": "zone", "live": True, "geo": {
            "shape": "circle", "kind": "keep_in", "radius_m": 20000,
            "coords": {"type": "Point", "coordinates": [-97.7431, 30.2672]}}}).encode())
        cmds, audit = [], []
        async def rec_cmd(m):
            _rec(cmds, m)

        async def rec_audit(m):
            _rec(audit, m)
        await admin.subscribe("cmd.>", cb=rec_cmd)
        await admin.subscribe("evt.so.action.>", cb=rec_audit)

        ent = await nats.connect(url, user="entity", password=pw["entity"])

        async def entity_get(m):
            q = json.loads(m.data or b"{}")
            if q.get("entity_id") == "sim-x1":
                rec = {"entity_id": "sim-x1", "entity_type": "drone", "kind": "multirotor",
                       "location": {"lat": 30.2672, "lon": -97.7431, "hae_m": 152.4},
                       "health": {"connection": "online"}, "indicators": {"simulated": True},
                       "capabilities": ["onemind.task.goto.v1", "onemind.task.rtl.v1", "onemind.task.hold.v1"]}
                await m.respond(json.dumps({"entity": rec}).encode())
            else:
                await m.respond(b'{"error":{"code":"not_found"}}')
        await ent.subscribe("svc.entity.get", cb=entity_get)

        br = await nats.connect(url, user="bridge", password=pw["bridge"])

        async def bridge(m):
            env = json.loads(m.data)
            asset = m.subject.split(".")[3]
            for outcome in ("acknowledged", "executed"):
                data = {"action_id": env.get("action_id"), "command_id": env.get("id"), "outcome": outcome,
                        "simulated": True, "mavlink": {"command": "MAV_CMD_DO_REPOSITION", "result": 0}}
                evt = {"v": 1, "id": secrets.token_hex(16), "ts": time.strftime("%Y-%m-%dT%H:%M:%S.000Z", time.gmtime()),
                       "type": "evt.drone.command", "source": "bridge-stub", "subject": f"evt.drone.{asset}.command",
                       "causation": env.get("id"), "data": data}
                await br.publish(f"evt.drone.{asset}.command", json.dumps(evt).encode())
        await br.subscribe("cmd.fleet.drone.sim-x1.>", cb=bridge)
        await admin.flush()
        await ent.flush()
        await br.flush()

        actions = ["asset." + v for v in VERBS] + ["alert.acknowledge", "alert.resolve", "entity.override",
                                                  "entity.star", "task.abort", "task.assign"]
        tuples = [{"user": "user:zeus", "relation": "operator", "object": "asset:sim-x1"}]
        for a in actions:
            for rel in ("proposer", "approver"):
                tuples.append({"user": "user:zeus", "relation": rel, "object": "action_type:" + a})
        with open(work + "/tuples.json", "w") as f:
            json.dump(tuples, f)
        with open(work + "/principals.json", "w") as f:
            json.dump({"own_account": "ONEMIND", "agent_accounts": ["AGENTS"], "principals": [
                {"account": "CLIENT", "user": "zeus", "kind": "user", "id": "user:zeus"}]}, f)
        penv = dict(os.environ, NATS_URL=url, NATS_USER="policy", NATS_PASSWORD=pw["policy"], POLICY_AUTHZ="local",
                    POLICY_MODEL_FILE=src + "/platform/security/openfga/onemind-actions.json",
                    POLICY_TUPLES_FILE=work + "/tuples.json", POLICY_PRINCIPALS_FILE=work + "/principals.json",
                    POLICY_SCHEMA_DIR=src + "/services/schemas", HEALTH_ADDR=f"127.0.0.1:{health}")
        plog = open(work + "/policy.log", "w")
        pol = subprocess.Popen([work + "/policy"], env=penv, stdout=plog, stderr=subprocess.STDOUT)
        procs.append(pol)
        ready = False
        for _ in range(100):
            await asyncio.sleep(0.1)
            if "policy service ready" in open(work + "/policy.log").read():
                ready = True
                break
        check("policy service ready (local authz, fail-closed checks on)", ready,
              open(work + "/policy.log").read()[-500:])
        if not ready:
            return

        cenv = dict(os.environ, OM_NATS_USER="zeus", OM_NATS_PASSWORD=pw["zeus"])

        async def run_client(*args):
            p = await asyncio.create_subprocess_exec(tak_bin, "task", "--ws", f"ws://127.0.0.1:{ws}", *args,
                                                     env=cenv, stdout=asyncio.subprocess.PIPE)
            lines = []
            snapshots = {}
            while True:
                line = await p.stdout.readline()
                if not line:
                    break
                j = json.loads(line)
                lines.append(j)
                transcript.append({"client": j})
                if j["ev"] == "dialog":
                    snapshots["cmds_at_dialog"] = len([c for c in cmds if c[0].startswith("cmd.fleet.")])
                if j["ev"] == "confirmed":
                    snapshots["cmds_at_confirm_reply"] = len([c for c in cmds if c[0].startswith("cmd.fleet.")])
            await p.wait()
            return lines, snapshots

        # 1 + 2: click-to-goto, human confirms after 1.5 s.
        n0 = len(cmds)
        t_dialog = time.time()
        lines, snap = await run_client("--asset", "sim-x1", "--lat", "30.27512345", "--lon", "-97.73987654",
                                       "--hae", "120", "--confirm", "--confirm-after-ms", "1500", "--seconds", "15")
        prop = next((l for l in lines if l["ev"] == "proposed"), {})
        check("1 proposal waits for a human (awaiting_confirmation, A3)",
              prop.get("status") == "awaiting_confirmation" and prop.get("level") == "A3", prop)
        check("1 client hash equals the policy service's params_hash",
              prop.get("params_hash") and prop.get("params_hash") == prop.get("local_hash"), prop)
        check("1 no cmd.fleet.> before the human confirmed", snap.get("cmds_at_dialog") == 0 and
              all(not c[0].startswith("cmd.fleet.") or c[2] > t_dialog + 1.4 for c in cmds[n0:]), snap)
        await asyncio.sleep(0.3)
        fleet = [c for c in cmds[n0:] if c[0].startswith("cmd.fleet.")]
        check("2 exactly one cmd.fleet.drone.sim-x1.goto after confirm",
              len(fleet) == 1 and fleet[0][0] == "cmd.fleet.drone.sim-x1.goto", [c[0] for c in fleet])
        if fleet:
            env = json.loads(fleet[0][1])
            check("2 command carries the clicked point and the approved hash",
                  env["data"].get("lat") == 30.2751235 and env["data"].get("lon") == -97.7398765 and
                  env.get("approval", {}).get("params_hash") == prop.get("params_hash") and
                  env.get("approval", {}).get("approver") == "user:zeus" and env.get("simulated") is True,
                  {"data": env.get("data"), "approval": env.get("approval")})
        events = [l["event"] for l in lines if l["ev"] == "audit"]
        final = next((l for l in lines if l["ev"] == "final"), {})
        check("2 client followed the audit trail to executed",
              final.get("status") == "executed" and
              all(e in events for e in ["proposed", "approved", "dispatched", "acknowledged", "executed"]), events)
        aid = prop.get("action_id")
        mine = [a[0].rsplit(".", 1)[1] for a in audit if json.loads(a[1])["data"].get("action_id") == aid]
        check("2 audit on evt.so.action.* in order", mine[:3] == ["proposed", "approved", "dispatched"] and
              "executed" in mine, mine)

        # 3: keep-out.
        n0 = len(cmds)
        lines, _ = await run_client("--asset", "sim-x1", "--lat", "30.300", "--lon", "-97.700", "--hae", "120",
                                    "--confirm", "--seconds", "5")
        prop = next((l for l in lines if l["ev"] == "proposed"), {})
        check("3 goto into the keep-out zone is denied (precondition_failed)",
              prop.get("status") == "denied" and prop.get("denial_code") == "precondition_failed", prop)
        check("3 nothing dispatched", not [c for c in cmds[n0:] if c[0].startswith("cmd.fleet.")])

        # 4: human rejects.
        n0 = len(cmds)
        lines, _ = await run_client("--asset", "sim-x1", "--lat", "30.271", "--lon", "-97.741", "--hae", "100",
                                    "--reject", "wrong drone", "--confirm-after-ms", "300", "--seconds", "6")
        final = next((l for l in lines if l["ev"] == "final"), {})
        check("4 a human rejection ends the action", final.get("status") == "rejected", lines[-3:])
        check("4 nothing dispatched", not [c for c in cmds[n0:] if c[0].startswith("cmd.fleet.")])

        # 5 + 6: raw requests as the client's user.
        errs = []

        async def on_err(e):
            errs.append(str(e))
        zeus = await nats.connect(url, user="zeus", password=pw["zeus"], error_cb=on_err)
        req = {"v": 1, "request_id": "5f0c2a9d-3e11-4b8e-b7e4-1f9a6a528d2b", "action_type": "asset.goto",
               "action_version": 1, "target": {"entity_type": "drone", "entity_id": "x1"},
               "params": {"lat": 30.27, "lon": -97.74, "hae_m": 100}, "requested_by": {"kind": "user", "id": "user:zeus"}}
        r = json.loads((await zeus.request("svc.policy.command.propose", json.dumps(req).encode(), timeout=5)).data)
        transcript.append({"raw_propose_non_sim": r})
        check("5 policy denies a non-sim id with not_simulated",
              r.get("status") == "denied" and (r.get("denial") or {}).get("code") == "not_simulated", r)
        n0 = len(cmds)
        await zeus.publish("cmd.fleet.drone.sim-x1.goto", b'{"v":1}')
        await zeus.flush()
        await asyncio.sleep(0.5)
        check("6 the client's user cannot publish cmd.fleet.> (server permissions violation)",
              any("ermissions" in e for e in errs) and not cmds[n0:], errs)
        await zeus.close()
        for c in (admin, ent, br):
            await c.close()
    finally:
        for p in reversed(procs):
            p.terminate()
            try:
                p.wait(5)
            except Exception:
                p.kill()
        os.makedirs(out, exist_ok=True)
        with open(out + "/d3-transcript.json", "w") as f:
            json.dump(transcript, f, indent=1)
        if os.path.exists(work + "/policy.log"):
            log = open(work + "/policy.log").read()
            for v in pw.values():
                log = log.replace(v, "***")
            with open(out + "/d3-policy.log", "w") as f:
                f.write(log)
        shutil.rmtree(work, ignore_errors=True)


def _rec(lst, m):
    lst.append((m.subject, m.data.decode(), time.time()))


if __name__ == "__main__":
    asyncio.run(main(sys.argv[1], sys.argv[2], sys.argv[3]))
    passed = sum(1 for r in results if r[1])
    print(f"{passed}/{len(results)} checks passed")
    sys.exit(0 if passed == len(results) else 1)
