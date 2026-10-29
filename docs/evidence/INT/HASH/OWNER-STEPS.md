# INT/HASH: owner steps

## 1. Rule on POL-16 / D2-P1 (params_hash on task-bearing commands)

Decision taken from the contract (asset-command.md §4 "approvals are bound to one action_id
and its params_hash", §5 hash = JCS of the action's `{action_type, action_version, target,
params}`, §6.3 the command carries that approval; mission-task.md §6 step 6 "§6.3 with
task_id"): a command's `approval.params_hash` is the approved action's own hash.

- No `task_id`: `asset.{verb}` over the command `data`; bridges recompute and compare.
- `task_id` set: `task.assign` / `task.abort` over the task parameters; bridges cannot
  recompute it, check the `tsk-` id and `sha256:{64 hex}` form, and rely on the Ed25519
  signature (which covers `data` and `approval`).

This is what #554 already did; #550 and #566 were changed. The alternative (D2-P1 as
written in #568: #554 re-stamps task commands with `asset.{verb}`) is not applied.
Record the ruling in `omos/docs/proposals/2026-10-29-policy-service-owner-proposals.md`
POL-16 and in #568's D2-P1 row.

Consequence: with `SIG_REQUIRED=false` (local only) nothing binds a task-bearing command's
data to its approval; the bridges log a warning. Keep `SIG_REQUIRED=true` everywhere but
local development.

## 2. Follow-ups for other PR owners

- #568 (mission): drop `services/mission/test/interop/policy-task-fleet-params-hash.patch`
  and make the default e2e expectation the "patched" one (drone flies, `done_ok`). Today
  its default run fails at "unpatched run: want done_not_ok rejected" because the bridge
  now accepts the task, which is the expected sign that the fix landed.
- #562 (identities): policy user subscribe `evt.map.geofence.*` (zone results, D1-P4).
- #550: the MAVLink bridge has no `hold` verb, so `task.abort` (which sends
  `cmd.fleet.{class}.{id}.hold`) is refused `unknown_action` on drones.

## 3. Re-run the cross test on the Mac

```bash
cd omos && git fetch -q origin
git worktree add /tmp/d2 origin/feat/mission-service && cd /tmp/d2/services/mission
POLICY_REF=origin/feat/policy-service BRIDGE_REF=origin/feat/mavlink-bridge-fleet \
E2E_POLICY_PATCHED=1 SIM_DRONE=<onemind-xr>/scripts/sim/mavlink_sim_drone.py \
PYTHON=<python with pymavlink> ./test/e2e.sh
```

Expected: `--- PASS: TestE2E`; transcript has `evt.drone.sim-x1.command` with `task_id`,
`outcome: executed`, `mavlink.command: MAV_CMD_DO_REPOSITION`, `sig_verified: true`, and
`contract_test 1` `task_facts` ending `done_ok`. (`E2E_POLICY_PATCHED=1` only selects the
expectation; no patch is applied.)
