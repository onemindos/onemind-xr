# E2E-2 (second whole-loop run): steps that need the owner

Run on 2026-10-29 against these omos branch tips: entity-service 996acfc5, policy-service
39570a54, mavlink-bridge-fleet 118978a0, layer-service 0bcd36a5, projector-aoi 345eb7f5,
geofence-alerts 1677a7ca, mission-service 4242cf7c, ingest-framework 61d8bc29, stream-caps
2655e00a, nats-identities 957b8da5. The web client is onemind-xr run/m4-web d2480cf. Build log:
`build.txt`.

Contents:
- `run1/`: the stack plus the browser step. Steps 1-8 pass.
- `run2-arrival-off/`: the same stack with the mission deployment's `MISSION_ARRIVAL=false`.
  Step 5 fails because the task stays `executing` (see below).
- `web/`: the Playwright output, web-notes.json and the screenshots.
- `harness/`: the harness used here. `changes-from-E2E.diff` is its diff against the first
  run's harness.

Hermes was a scripted client that connected through the stand-in sidecar from
feat/nats-identities. No model API was called.

## 1. Rerun locally (Mac, no cluster)

```bash
export PATH=$HOME/Desktop/work/.tools/bin:$PATH
T=$HOME/Desktop/work/.run/tmp-e2e
# worktrees: git -C ~/Desktop/work/omos worktree add --detach $T/src/<branch-name> origin/feat/<branch-name>
#   (the ten branches above) and onemind-xr origin/run/m4-web at $T/src/web; build the binaries into $T/bin
#   exactly as listed in build.txt; copy harness/ to $T/harness.
P=$HOME/Desktop/work/.tools/py/bin/python
$P $T/src/nats-identities/scripts/nats_identities.py gen-passwords --out $T/run/passwords.json
$P $T/src/nats-identities/scripts/nats_identities.py conf --passwords $T/run/passwords.json --stage B --test-users --out $T/run/accounts.json
(cd $T/harness && go build -o ../bin/harness .) && cd $T && E2E_EV=<evidence dir> ./bin/harness   # steps 1-7, then holds the stack
# browser step: copy int.config.ts to $T/src/web/web/ and int.spec.ts to $T/src/web/web/e2e-int/, npm ci, then
cd $T/src/web/web && PLAYWRIGHT_BROWSERS_PATH=$HOME/Desktop/work/.tools/ms-playwright \
  E2E_STACK=$T/run/stack.json E2E_SHOTS=<evidence dir>/web/screens npx playwright test -c int.config.ts
touch $T/run/stop                                               # stops every process
```

Expected with the branch tips above: every step passes. With `E2E_MISSION_ARRIVAL=false` (the
deployment value), step 5 ends with the task still `executing` even though the drone is 0 m from
the goal.

## 2. Owner decision D2-P2 (mission completion), needed before the deployed loop completes a task

The mavlink bridge reports `received` and `executing` for a goto and never `done_ok`
(`services/bridge/bridges/mavlink/bridge.go` reportTask). The mission service closes the task only
when `MISSION_ARRIVAL=true` (D2-P2). The deployment sets it to `false`, and in that case
`run2-arrival-off` shows `mission-state` staying `executing`. Decide one of the following:
ratify D2-P2 and set `MISSION_ARRIVAL=true` in `platform/mission/deployment.yaml` (this run used a
25 m radius; the proposal says 5 m), or have the bridge report `done_ok` on arrival (H2 follow-up).

## 3. Hermes with a real model (needs the model key; not run here)

1. Deploy Hermes with its `nats-leaf` sidecar using the A19 `hermes` leaf credential (`leaf-conf`
   without `--plain`, sealed as in IDENTITIES.md step 2).
2. In the L0 web client, ask: "Send sim-x1 to the north edge of the range".
3. Expected: one `svc.policy.task.propose` or `svc.policy.command.propose` from
   `agent:hermes/zeus/legacy` with `on_behalf_of: user:zeus`, then `evt.so.action.proposed`.
   Nothing appears on `cmd.fleet.>` until zeus confirms. If the model tries
   `svc.policy.*.confirm`, the sidecar log shows `Publish Violation - Subject
   "svc.policy.task.confirm"`, as in `run1/logs/hermes-sidecar.log`.

## 4. ATAK device or WinTAK (not run here)

The projector now emits `OM.drone.sim-x1` (`a-f-A-M-H-Q`) at about 1 Hz while the drone moves, and
every 5 s while it holds (run1 transcript, 181 `tak.cot.inject`). To check on a device, connect ATAK
to `tak.tail717ba0.ts.net:8089` and run the projector Deployment (`TAK_RELEASE_GROUPS=homestead`)
against the TAK leaf. Expected: SIM-X1 appears and moves, and no NWS item reaches ATAK through
`tak.cot.inject`.

## 5. Headset / Unreal (not run here)

The Unreal client (onemind-xr #3) uses the same `svc.layer.*` and `svc.policy.*` endpoints. Repeat
step 8 with it: select sim-x1, choose Go to point, then confirm. Expected audit sequence:
`proposed, approved, dispatched, acknowledged, executed`.
