# I1 / I3 / D10: owner steps (omos PR #567, branch `feat/agents-watch-briefs`)

These need the owner's Mac, an ATAK device, live Hermes or a real model, which the build
machine does not have. Everything else is proven by the tests in `go-test-race.txt` and
`transcript.log` in this directory. Run the steps top to bottom. Status of each feature until
these pass: **built, awaiting owner check**.

Prerequisites on the Mac: Go 1.26+, `nats-server` and `nats` CLI (or the run-mac script of the
mesh gateway, PR #551), ATAK device on the same Wi-Fi. Use a checkout of omos with the branches
of PRs #567, #552, #551 and (for step 3 over TAK Server) #560.

## 1. Agent entity from the real Hermes heartbeat (I1, read-only on the cluster)

Checks that live Hermes heartbeats have the shape agentops expects. Read-only.

```bash
nats --context <your hub context> sub 'agents.hb.hermes.zeus.legacy' --count 1
```

Expected: one JSON heartbeat with `agent: "hermes"`, `owner: "zeus"`, `interval_s`. If it
carries no `onemind` block, Hermes shows as `mode: idle` without a task until its heartbeat
adds one (agent side, optional, Synadia §8.3 allows unknown fields).

## 2. Local stack on the Mac: agent on the map (I1)

```bash
nats-server -js -p 4222 &
nats stream add TEL --subjects 'tel.fleet.>' --defaults
nats stream add AGENTS --subjects 'agents.hb.>' --defaults
nats stream add ENT --subjects 'ent.*.*.state' --max-msgs-per-subject 1 --defaults
nats stream add COT --subjects 'tak.cot.>' --defaults
# entity service (PR #552 branch)
(cd services/entity && go run ./cmd/entity-service) &   # env below
#   ENT_MSG_TTL=false LEASE_BUCKET=none START_LOOKBACK=0 INPUTS='TEL=tel.fleet.>;COT=tak.cot.>' \
#   ENTITY_LINKS='{"fleet_uid:agent-hermes-zeus-legacy":"agent-hermes-zeus-legacy"}'
# agentops (PR #567 branch); SITE_* = where agents are drawn
(cd services/agentops && SITE_LAT=<lat> SITE_LON=<lon> COMPONENTS=presence,chat \
  REPLY_TARGETS=mesh CONTACT_TARGETS=mesh go run ./cmd/agentops) &
nats sub 'ent.agent.>'
```

Expected within 30 s of a Hermes (or Hermes Edge on the Mac) heartbeat reaching this server
(through the Mac leaf): `ent.agent.agent-hermes-zeus-legacy.state` with `entity_type: agent`,
`callsign`, `meta.mode`, `meta.current_task`. In the OneMind client the agent appears at the
site anchor.

## 3. ATAK sees the agent as a contact and can message it (I1)

Run the mesh gateway (PR #551) on the Mac: `services/bridge/bridges/tak/run-mac.sh` (its
README). With step 2 running:

1. On the ATAK device, open **Contacts**. Expected: a contact named after the agent's callsign
   (`LEGACY` by default) within 60 s. **Unverified until this step:** that ATAK lists the
   `OM.agent.*` PLI (type `a-f-G-U-C`, `<contact endpoint="*:-1:stcp">`) as a messageable
   contact, and which icon it shows (proposal I1-P1).
2. Tap it, **Chat**, send `what is sim-x1 doing?`.
   Expected on the Mac: `nats sub 'agents.prompt.hermes.>'` shows the prompt with
   `"trust":"claimed"` and your callsign; within Hermes' answer time the ATAK chat shows the
   reply from the agent.
3. Send 7 messages within a minute. Expected: a "Rate limit" reply after the 6th.
4. Stop Hermes (or its leaf) and send a message. Expected: "… could not answer right now (agent
   offline). Your message was not acted on."

Over TAK Server instead of the mesh: needs tak-client (PR #560) and the owner's approval of
I1-P2; set `REPLY_TARGETS=server CONTACT_TARGETS=server` and repeat 1–2 from an ATAK device
connected to `tak.tail717ba0.ts.net`.

## 4. Real model explains an alert and proposes; a human confirms (I3)

Spend note: this calls whatever model Hermes is configured with (the cluster's existing
provider). Skip it if you do not want that; the stub path is proven.

```bash
# policy service (PR #554 branch) with POLICY_AUTHZ=local and a tuples file that makes
# user:zeus proposer+approver of task.assign and operator of asset:sim-x1, and
# agent:onemind-watch/zeus/hub proposer of task.assign (I1-P6).
(cd services/agentops && SITE_LAT=<lat> SITE_LON=<lon> COMPONENTS=watch \
  MODEL_PRIMARY=agent:agents.prompt.hermes.zeus.legacy WATCH_FILE=<copy of the ConfigMap watches.yaml> \
  go run ./cmd/agentops) &
nats kv add geofences
nats kv put geofences zone-home '<zone record with a keep_in polygon around the sim area>'
# fly the scripted drone sim-x1 out of the zone (MAVLink bridge PR #550), or publish an
# ent.drone.sim-x1.state outside the polygon
nats sub 'evt.entity.sim-x1.alert'
```

Expected: revision 1 (`alert_kind: geofence_exit`), then revision 2 with
`explanation.model: agent:agents.prompt.hermes.zeus.legacy`, a sensible explanation, and
`proposal.status: awaiting_confirmation`. Nothing on `nats sub 'cmd.>'`. Confirm in the OneMind
app (D5 confirm dialog); only then does `cmd.fleet.drone.sim-x1.rtl` appear.

## 5. Offline brief with a real local model (D10)

Pick a model from `services/agentops/LOCAL-MODELS.md` (Apache-2.0 or MIT ones first), check
free disk first (a 7B quantised model is several GB).

```bash
brew install ollama          # or the ollama.com installer
ollama serve &
ollama pull qwen2.5:7b-instruct
# Turn Wi-Fi off now (network off).
(cd services/agentops && COMPONENTS=brief MODEL_PRIMARY=agent:agents.prompt.hermes.zeus.legacy \
  MODEL_LOCAL='openai:http://127.0.0.1:11434/v1|qwen2.5:7b-instruct' \
  EXPECTED_FEEDS='{"nws-alerts":300}' go run ./cmd/agentops) &
nats req svc.brief.generate '{"aoi":{"id":"home","name":"Home","bbox":[<lon0>,<lat0>,<lon1>,<lat1>]}}' --timeout 120s
```

Expected: `model.used` is the local model, `model.offline: true`, `validation.cited >= 3`,
`validation.unknown_removed` empty or listing what was removed, and the text names
`nws-alerts` as a gap. Note the time the brief took and whether `validation.repaired` was
needed: that tells whether the model is good enough or the stub is doing the work.

## 6. Cluster rollout (owner, after merge)

1. Approve or amend I1-P1 … I1-P7 (`docs/proposals/2026-10-29-grammar-register/i1-agents-watch-briefs.md`).
2. After the first `build-agentops` run on main, pin the digest in
   `platform/omos/agentops/deployment.yaml`.
3. Set `SITE_LAT`/`SITE_LON` in `platform/omos/agentops/configmap.yaml`.
4. Sync `platform-omos-agentops` by hand in Argo CD. Expected: one pod Ready on the ecosystem
   pool; `tel.fleet.agent.agent-onemind-watch-zeus-hub.position` and
   `…onemind-brief-zeus-hub.position` appear in `TEL`.
