# E2E integration: steps that need the owner

Run on 2026-10-29 against these branch tips (omos): entity-service 73823045, policy-service
f4519b17, mavlink-bridge-fleet 2b724093, layer-service d5d1a8bc, projector-aoi 3d385a10,
geofence-alerts 1677a7ca, mission-service 21453fe8, ingest-framework 61d8bc29, stream-caps
2655e00a, nats-identities 19aa0b87; onemind-xr run/m4-web d2480cf.

What ran here: `run1/` is the stack exactly as the branches define it. `run2/` adds two
test-only workarounds: the `policy` user gets `evt.map.geofence.>` subscribe, and the projector
gets `TAK_RELEASE_GROUPS=homestead` plus a correctly marked probe record `sim-x9`. `web/` is the
browser step, run against the run-1 stack. Hermes was played by a scripted NATS client (user
`hermes`, AGENTS account). No model API was called.

## 1. Rerun the integration locally (Mac, no cluster)

```bash
export PATH=$HOME/Desktop/work/.tools/bin:$PATH
# build each service from its branch tip (see harness/main.go for paths), then:
python3 scripts/nats_identities.py gen-passwords --out ~/Desktop/work/.run/tmp-e2e/run/passwords.json   # in an omos checkout of feat/nats-identities
python3 scripts/nats_identities.py conf --passwords ~/Desktop/work/.run/tmp-e2e/run/passwords.json --stage B --out ~/Desktop/work/.run/tmp-e2e/run/accounts.json
cd ~/Desktop/work/.run/tmp-e2e && ./bin/harness            # steps 1-7, then holds the stack
# in run/m4-web web/: E2E_STACK=.../run/stack.json E2E_SHOTS=... npx playwright test -c int.config.ts
touch ~/Desktop/work/.run/tmp-e2e/run/stop                 # stops every process
```

Expected (branch tips as above): steps 1, 2, 5, 7, 8 pass. Step 3 fails (no CoT for sim-x1).
Step 4 fails at zone.create (`no_ack`). Step 6 reaches the vehicle, and the scripted sim
answers estop with `MAV_RESULT_UNSUPPORTED`. With `E2E_DEV=1`, step 4 and probe 3b pass.

## 2. Hermes with a real model (needs the owner's model key; not run here)

1. Deploy Hermes with its `nats-leaf` sidecar using the A19 `hermes` leaf credential (LEAFNODE
   only; this run used a STANDARD connection for the scripted stand-in).
2. Ask in the L0 web client, Hermes tab: "Send sim-x1 to the north edge of the range".
3. Expected: one `svc.policy.task.propose` or `svc.policy.command.propose` from
   `agent:hermes/zeus/legacy` with `on_behalf_of: user:zeus`, followed by `evt.so.action.proposed`.
   The web client shows a pending confirm. Nothing is sent on `cmd.fleet.>` until zeus confirms.
   If the model tries `svc.policy.*.confirm`, nats-server logs
   `Publish Violation - Subject "svc.policy.task.confirm"` for `AGENTS/user:hermes`, as in
   `run1/logs/nats-server.log` and `run2/logs/nats-server.log`.

## 3. ATAK device or WinTAK (not run here)

Possible only after the entity-service marking bug is fixed (`releasable_to` comes out empty for
fleet assets) and the projector Deployment sets `TAK_RELEASE_GROUPS=homestead`:
connect ATAK to `tak.tail717ba0.ts.net:8089`, start the projector against the TAK leaf, and
check that `OM.drone.sim-x1` appears with type `a-f-A-M-H-Q` and moves. Before either fix, ATAK
gets nothing for own assets (run 1, step 3: projector `blocked_g7_marking` 462).

## 4. Headset / Unreal

Not covered by this run. The Unreal client (onemind-xr #3) reads the same `svc.layer.*` and
`svc.policy.*` endpoints. Repeat step 8 with it: select sim-x1, Go to point, confirm. Expected:
the same audit sequence (`proposed, approved, dispatched, acknowledged, executed`) for the
action.
