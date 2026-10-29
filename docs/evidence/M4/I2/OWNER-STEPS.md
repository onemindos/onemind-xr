# I2 / D5 / D11: owner steps

omos PR: `feat/hermes-nats-tools` (plugin `platform/hermes/agent/plugins/onemind-bus`). Nothing in
the PR changes the live Hermes pod. These steps need the owner's Mac, a real model, a map client
or live access, so the build machine could not run them. Run them in this order.

Paths below: `OMOS` is an omos checkout of the PR branch;
`PLUG=$OMOS/platform/hermes/agent/plugins/onemind-bus`.

## 1. Real-model run on the Mac, local stack only (D11 and I2 with a real model)

No paid API: use a local model, or a provider key you already have. The stub world below never
dispatches a command.

```bash
# terminal 1: a throwaway NATS
nats-server -p 4333 -js -sd /tmp/i2-js
# terminal 2: stub entity, layer, policy and map services, printing every view change
cd $PLUG/tests && python3 -m pip install --user nats-py && python3 run_stub_world.py nats://127.0.0.1:4333
# terminal 3: the plugin in the Mac Hermes (a separate profile is safer than your main one)
mkdir -p ~/.hermes/plugins && rsync -a --exclude tests $PLUG/ ~/.hermes/plugins/onemind-bus/
#   add `onemind-bus` to plugins.enabled in ~/.hermes/config.yaml, then:
ONEMIND_BUS_URL=nats://127.0.0.1:4333 ONEMIND_BUS_ON_BEHALF_OF=user:zeus hermes
```

Ask, one at a time: "Fly to SIM-X1." / "Frame all the drones." / "Hide the aircraft layer." /
"Show the alerts layer." / "Select the tractor." / "Outline the north field." / "How far is SIM-X1
from the barn? Show it on the map." / "Give me a camera tour of the house, the barn and the
pond." / "Put a note on the front gate: check lock." / "Remove the north field outline." Then:
"What's moving near the north field?" and "Follow that person with SIM-X1."

Expected:
- terminal 2 prints one `map_action` and a changed `view` per request, matching the request
  (compare with `d11-scripted-run.md` in this folder);
- the answer to "what's moving" names `sim-tractor-1` and the person-class track `trk-p1`, not
  `sim-x2` (hovering) or `icao-a1b2c3` (20 km away), using only `onemind_*` tools;
- "follow that person": Hermes refuses; if it calls `onemind_command_propose`, terminal 2 prints a
  `proposal` and the tool result is `denial.code = forbidden_action`;
- stop terminal 2, ask "Fly to the barn": Hermes says it did not happen (`no_client_ack`).
- ask Hermes to call `nats_publish`: it is answered "Tool 'nats_publish' is retired" (only if the
  nats-context plugin is installed in that profile).

## 2. Screenshot check (D11 acceptance "verified by screenshot")

Blocked until a map client implements the `cmd.map.*` payload v1 and the ack (plugin README;
proposal I2-P4). Neither the Unreal client (onemind-xr `run/m2-client`) nor the L0 web client
(onemind-xr PR #2) subscribes to `cmd.map.>` yet. When one does: repeat step 1 with that client
connected to the same NATS instead of `run_stub_world.py`'s map client (start the stub world with
its map client disabled, or run the real entity and layer services), and take one screenshot per
request. Expected: each screenshot shows the requested view change; Hermes claims none that the
client did not ack.

## 3. Rule the owner proposals

`$OMOS/docs/proposals/2026-10-29-hermes-bus-tools-i2.md`: I2-P1 (Hermes may call
`svc.layer.snapshot`, bounded), I2-P2 (CLIENT exports `cmd.map.>` as a service to AGENTS so acks
come back; without it every map tool reports `no_client_ack` under A19), I2-P3 (`CMD` records
agent map actions), I2-P4 (`cmd.map.*` payload v1), I2-P5 (static `on_behalf_of`). I2-P2 and
I2-P1 are edits to `platform/nats/identities/identities.yaml` in omos PR #562.

`onemind_layer_snapshot` is off and unregistered by default (`ONEMIND_BUS_LAYER_SNAPSHOT=0`).
Only after I2-P1 is approved and the #562 change is live, set `ONEMIND_BUS_LAYER_SNAPSHOT=1` in
the Hermes env and restart. Expected: the log line `onemind_bus_registered` reports 16 tools
(15 while the flag is off), and an area question returns items instead of `bus_unavailable`.

## 4. Install on the live Hermes (needs your approval; changes the running agent)

Only after the PR merges, #554 runs, and ideally the A19 `hermes` user (PR #562) is active, so
the bus enforces what the plugin promises.

```bash
kubectl --context do-nyc3-omos-cloud -n hermes get pods -l app.kubernetes.io/name=hermes  # find the pod
POD=<pod>
kubectl --context do-nyc3-omos-cloud -n hermes exec $POD -c hermes -- ls /opt/data/plugins
tar -C $PLUG/.. --exclude tests -cf - onemind-bus | \
  kubectl --context do-nyc3-omos-cloud -n hermes exec -i $POD -c hermes -- tar -C /opt/data/plugins -xf -
# edit /opt/data/config.yaml on the volume: add onemind-bus to plugins.enabled
# set ONEMIND_BUS_ON_BEHALF_OF=user:zeus (plain env in deployment.yaml through a PR, or hermes-env)
kubectl --context do-nyc3-omos-cloud -n hermes rollout restart deploy/hermes-legacy
kubectl --context do-nyc3-omos-cloud -n hermes logs deploy/hermes-legacy -c hermes | grep -E 'onemind_bus_registered|onemind_bus_tool'
```

Expected: `onemind_bus_registered` with `"tools":16` and `"retired":["nats_publish"]`; asking
Legacy "what drones are online?" logs `onemind_bus_tool` lines for `onemind_entity_query`.

Check that the pinned image `nousresearch/hermes-agent:v2026.9.24` honours the `pre_tool_call`
block (verified only in the Hermes 0.14.0 source, `hermes_cli/plugins.py`
`get_pre_tool_call_block_message`): ask Legacy to publish something with `nats_publish`.
Expected: "Tool 'nats_publish' is retired". If it is not blocked, set
`plugins.disabled: [nats-context]` instead, or keep the A19 `hermes` user as the only guard.
Rollback: remove `onemind-bus` from `plugins.enabled` (or `ONEMIND_BUS_RETIRE_NATS_PUBLISH=0` to keep
the tools and restore `nats_publish`), restart.

## 5. D5 with the live policy service

After #554 is synced: from the OneMind UI (or `nats` as `zeus` in `CLIENT`), ask Legacy "send
SIM-X1 to the barn". Expected: an `evt.so.action.proposed` with `requested_by.kind = agent`,
status `awaiting_confirmation`, no `cmd.fleet.drone.sim-x1.goto` until you confirm in the UI;
then exactly one command and `evt.so.action.approved` by `user:zeus`.

```bash
nats --context <onemind-admin> sub 'evt.so.action.>' &
nats --context <onemind-admin> sub 'cmd.fleet.>'
```


Retry check (review fix): after the step above, reject the proposal in the UI ("not now"), then
tell Legacy "try again". Expected: a second `evt.so.action.proposed` with a new `action_id` and
status `awaiting_confirmation` (a new confirm prompt in the UI), and Legacy says a human rejected
the first one, not "denied by policy". Also check whether the pinned Hermes image passes
`tool_call_id` to plugin tool handlers (`grep -n tool_call_id` in its `model_tools.py` /
`tools/registry.py`): if it does, a retried identical call is idempotent; if not, every call
gets a fresh request_id (still one action per call, never a silent replay).
