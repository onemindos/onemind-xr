# D2 / H4 task service: owner steps

omos PR #568 (`feat/mission-service`). What the build machine proved is in `unit-tests.txt`,
`e2e-unpatched.txt`, `e2e-patched.txt` (and the `*-logs/` transcripts) and `ci-equivalent.txt`.
These steps need the owner's Mac, an ATAK device, PX4 SITL, a real model, or live access.

## 1. Decide D2-P1 first (blocks every asset task)

The MAVLink bridge (#550) refuses every task-dispatched command, because #554 stamps it with
the `task.assign` `params_hash`. Rule on D2-P1 in
`omos/docs/proposals/2026-10-29-mission-service-owner-proposals.md`. If you approve, the #554
lane applies `services/mission/test/interop/policy-task-fleet-params-hash.patch` (or an
equivalent change). Check it on the Mac:

```bash
cd omos/services/mission
SIM_DRONE=../../../onemind-xr/scripts/sim/mavlink_sim_drone.py PYTHON=python3 \
POLICY_REF=origin/feat/policy-service ./test/e2e.sh
```

Expected after the fix is in #554: the run **fails** at "unpatched run: want done_not_ok
rejected" because the drone now flies, which proves the fix landed. Then run with
`POLICY_PATCH` unset and `E2E_POLICY_PATCHED=1` exported. Expected: `--- PASS: TestE2E`,
with the transcript showing `task_facts: created, sent, received, executing, done_ok`.
(Requires Go 1.26+, nats-server 2.12+ on PATH, pymavlink.)

## 2. Same flow against PX4 SITL (H2/H4 owner check)

Start PX4 SITL (`make px4_sitl gz_x500`, MAVLink on udp 14550). Then run the e2e with the
bridge pointed at it instead of the scripted drone. Today `e2e_test.go` always starts the
scripted drone, so this is a manual variant: start the stack as `test/e2e.sh` does, but
drop the `sim-drone` process and set `MAVLINK_ENDPOINT=udpserver:127.0.0.1:14550` for the
bridge. Expected: the same task states. Arrival (D2-P2) completes the goto within 5 m of the
goal. PX4 reports altitude AMSL, so check `hae` against the goal `hae_m`.

## 3. Contract test 7: a mission created in the client appears on ATAK as a route

Needs the tak-client bridge (#560, A21), which mirrors `mission-state` to a TAK Server Data
Sync mission, a TAK Server, and an ATAK device on the LAN or tailnet:

1. With #554, #568 and #560 running on the local stack, create a mission with two
   waypoints through `svc.policy.task.propose` (`mission.create`, `mission.waypoint` ×2), confirmed by you.
2. On ATAK: Data Sync → Missions. Expected: a mission with the record's `name`, holding a
   route of 2 points at the waypoint positions. In KV `mission-state`, `tak_mission.synced_t` is set.

## 4. Hermes with a real model (no paid API was called here)

Hermes proposals were driven from the test as the AGENTS `hermes` identity. With the real
model and the A19 identities live, ask Hermes: "Send sim-x1 to the north gate as part of
the North field mission." Expected:
- `evt.so.action.proposed` with `requested_by.kind: agent`;
- nothing on `cmd.fleet.>` until you confirm;
- after your confirm, `evt.mission.{tsk}.created` → `done_ok`.

Then ask Hermes to confirm its own proposal. Expected: refused (server permission, and
`agent_cannot_confirm` in policy).

## 5. Live activation (cluster stays read-only for the agents)

In order, from `omos/platform/mission/README.md`:

1. Merge order: #554 (policy), then #568 (this). Merge #562 (A19 identities) with the
   `mission` user of D2-P5 added.
2. The first `build-mission.yml` run on main pushes `ghcr.io/onemindos/mission:sha-<commit>`.
   Pin the digest in `platform/mission/deployment.yaml`.
3. Seal `mission-service-runtime` with the kubeseal command in the README. Commit only the
   sealed file.
4. Sync `platform-policy`, then `platform-mission`, by hand in Argo CD.

   Expected: `kubectl -n onemind-core logs deploy/mission-service` shows "task service ready";
   `nats consumer ls CMD` lists `mission-commands`; `nats consumer ls EVT` lists `mission-events`.
