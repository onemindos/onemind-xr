# E2E-3 (third whole-loop run): steps that need the owner

Run on 2026-10-29 against these omos branch tips: entity-service 4f4f65d8, policy-service 39570a54,
mavlink-bridge-fleet 6e4f29ea, layer-service 14ef80a5, projector-aoi 127375a5, geofence-alerts 9952d3c3,
mission-service e69ea4c9, ingest-framework 61d8bc29, stream-caps 2655e00a, nats-identities 1f2e33ff.
Web client: onemind-xr run/m4-web 55618af. Build log: `build.txt`.

Contents: `run1/` (harness-stdout.txt, results.json, transcript.jsonl, logs/), `web/` (Playwright
output, web-notes.json, screenshots; `attempt1-alert-before-goto/` is the first browser attempt, see
workarounds.txt), `harness/` (changes-from-E2E-2.diff is the diff to the second run's harness).
All 13 harness checks and the browser step pass. No model API was called; Hermes was a scripted client.

## 1. Rerun locally (Mac, no cluster)

```bash
export PATH=$HOME/Desktop/work/.tools/bin:$PATH
T=$HOME/Desktop/work/.run/tmp-e2e; P=$HOME/Desktop/work/.tools/py/bin/python
# worktrees: git -C ~/Desktop/work/omos worktree add --detach $T/src/<name> origin/feat/<name> for the ten
#   branches above, and git -C ~/Desktop/work/onemind-xr worktree add --detach $T/src/web origin/run/m4-web.
# build the eight binaries into $T/bin with the commands in build.txt (main packages under cmd/).
cp -R harness $T/harness && mkdir -p $T/run
$P -I $T/src/nats-identities/scripts/nats_identities.py gen-passwords --out $T/run/passwords.json
$P -I $T/src/nats-identities/scripts/nats_identities.py conf --passwords $T/run/passwords.json --stage B --test-users --out $T/run/accounts.json
(cd $T/harness && go build -o ../bin/harness .) && cd $T && E2E_EV=<evidence dir>/run1 ./bin/harness
# when it prints STACK READY:
cp $T/harness/int.config.ts $T/src/web/web/ && mkdir -p $T/src/web/web/e2e-int && cp $T/harness/int.spec.ts $T/src/web/web/e2e-int/
cd $T/src/web/web && npm ci && PLAYWRIGHT_BROWSERS_PATH=$HOME/Desktop/work/.tools/ms-playwright \
  E2E_STACK=$T/run/stack.json E2E_SHOTS=<evidence dir>/web/screens npx playwright test -c int.config.ts
touch $T/run/stop     # stops every process the harness started
```

Expected: every step `[PASS]`; step 5 and 5b tasks end `done_ok` with `result.source = "bridge"` and
facts `created, sent, received, executing, done_ok`, with the mission deployment's `MISSION_ARRIVAL=false`.

## 2. Arrival tolerance on real hardware (SITL or vehicle)

The bridge reports `done_ok` when within `ARRIVAL_RADIUS_M=5` / `ARRIVAL_VERTICAL_M=5` (MSL). The scripted
drone stops 3-4 m from the goal. Against ArduPilot SITL, send a goto task and check that
`evt.drone.<id>.task` reaches `done_ok`; if the autopilot holds farther than 5 m away, raise
`ARRIVAL_RADIUS_M` in the bridge env. Expected: `mission-state` task state `done_ok` within about
10 s of the vehicle holding at the point.

## 3. Hermes with a real model (needs the model key; not run here)

Same as E2E-2 step 3: deploy Hermes with its `nats-leaf` sidecar (A19 `hermes` leaf credential, sealed
per IDENTITIES.md), ask in L0 "Send sim-x1 to the north edge of the range". Expected: a
`svc.policy.task.propose` from `agent:hermes/zeus/legacy` with `on_behalf_of: user:zeus`, nothing on
`cmd.fleet.>` until zeus confirms, and any `svc.policy.*.confirm` from the model fails at the sidecar
with `Publish Violation` (as in `run1/logs/hermes-sidecar.log`).

## 4. ATAK device or WinTAK (not run here)

Connect ATAK to `tak.tail717ba0.ts.net:8089`, run the projector Deployment (`TAK_RELEASE_GROUPS=homestead`)
against the TAK leaf. Expected: `SIM-X1` (`a-f-A-M-H-Q`, uid `OM.drone.sim-x1`) appears and moves; no NWS
item arrives through `tak.cot.inject`.

## 5. Headset / Unreal (not run here)

With onemind-xr #3, select sim-x1, Go to point, confirm. Expected audit sequence:
`proposed, approved, dispatched, acknowledged, executed`, and the geofence alert listed when it leaves the pad.
