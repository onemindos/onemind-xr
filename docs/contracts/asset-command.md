# Contract: actions and asset commands

Status: M0, 2026-10-29. Version `v: 1`.
Ontology: [ONTOLOGY.md](../design/ONTOLOGY.md) §6 (actions) and §7 (permissions).

This contract defines how anyone (an operator in the client, a TAK user through the ATAK plugin,
Hermes, a watch job) changes the world. **Actions are the only write path.** Every action goes
request → policy check → human confirmation → signed command → executor → outcome event. This
holds for every asset command **even in simulation** (ULTRACODE-PROMPT Rule 6; decision 0004 #13).
The flow in §4 applies to all actions; §3 lists the asset verbs. Tasks use the same flow
([mission-task.md](mission-task.md) §6).

## 1. Subjects, streams and files

| Purpose | Subject | Defined in (omos) | State |
|---|---|---|---|
| Propose an action (request/reply) | `svc.policy.command.propose` (tasks: `svc.policy.task.propose`) | grammar `svc.{service}.{endpoint}` (`CLAUDE.md`) | pattern exists; policy service **proposed** (`services/policy/`, new) |
| Confirm or reject (request/reply) | `svc.policy.command.confirm` (tasks: `svc.policy.task.confirm`) | same; [BUS-DESIGN.md](../design/BUS-DESIGN.md) §3.4 | **proposed** |
| Look up an action | `svc.policy.action.get` | same | **proposed** |
| Asset command (dispatch) | `cmd.fleet.{class}.{asset_id}.{verb}` | `docs/NATS-DOCTRINE.md` §9.1 (`cmd.fleet.{class}.{id}.{verb}`, verb ∈ goto\|estop\|rtl\|…); stored by `platform/nats/streams/cmd.yaml` (`cmd.>`, 720 h) | exists |
| Other action commands | `cmd.entity.{id}.{verb}`, `cmd.mission.{id}.{verb}`, `cmd.map.…`, `cmd.team.{id}.{verb}`, `cmd.watch.{id}.{verb}`, `cmd.feed.{id}.{verb}` | grammar `cmd.{domain}.{entity}.{action}` and doctrine §9.1/§9.7; `cmd.yaml` | exist; new domains to ratify (ONT-P9) |
| Audit and outcome facts (single writer: policy service) | `evt.so.action.{event}`, event ∈ `proposed, denied, approved, rejected, expired, dispatched, acknowledged, executed, failed, refused` | grammar `evt.{domain}.{entity}.{event}`; stored by `platform/nats/streams/evt.yaml` (`evt.so.>`, 2160 h, dedup 2 m) | pattern and stream exist |
| Bridge outcome report | `evt.{class}.{asset_id}.command` (outcome in `data.outcome`) | same grammar; `evt.yaml` binds `evt.drone.>` and `evt.robot.>`; **`evt.vehicle.>` is not bound** (ONT-P13) | exists for drone and robot |
| Command envelope schema | v1 | `services/schemas/command/envelope.schema.json` | exists; OneMind fields in §6.3 are allowed by `additionalProperties: true` |
| Event envelope schema | v1 | `services/schemas/envelope/envelope.schema.json` | exists |
| Action specs (parameters per action) | `services/schemas/actions/{action_type}.v{n}.schema.json`, mirrored to KV `om-schema` | `platform/nats/streams/kv-om-schema.yaml` | **proposed** files; bucket exists |
| Telemetry the bridge publishes | `tel.fleet.{class}.{asset_id}.position\|status` | doctrine §9.1; `platform/nats/streams/tel.yaml` (`tel.fleet.>`) | exists |

Choices recorded as owner proposals (decision 0004 #8): the five-token `cmd.fleet` form is used
instead of `CLAUDE.md`'s four-token example (ONT-P4); audit uses `evt.so.action.*`, the only
proposed audit subject that a stream already stores (ONT-P10). The omos command-envelope
description names `evt.command.accepted|rejected|executed|failed`; those are three tokens, outside
the grammar and unstored, so they are not used.

**Policy subject names (settled 2026-10-29, proposal RA2):** the set uses the four-token names in
[BUS-DESIGN.md](../design/BUS-DESIGN.md) §3.4. Asset commands and every other non-task action
(entity edits, team, watch, feed, projector) use `svc.policy.command.propose` and
`svc.policy.command.confirm`; tasks and missions use `svc.policy.task.propose` and
`svc.policy.task.confirm` ([mission-task.md](mission-task.md) §6); any action is looked up with
`svc.policy.action.get`. All fit `svc.{service}.{endpoint}` (omos `CLAUDE.md`), because the endpoint
may contain dots, as in doctrine §2's example `svc.meta.task.list` (omos `docs/NATS-DOCTRINE.md`).
The message bodies in §6 are the same for both families. BUS-DESIGN's verb list for
`cmd.fleet` (`goto rtl land arm disarm estop orbit`) is a subset of §3.

`{class}` ∈ `drone | robot | vehicle` for commandable assets (doctrine §9.1 class vocabulary).
`{asset_id}` follows [entity.md](entity.md) §3 and must start `sim-` in this run.

## 2. Ownership

| Role | Publishes | Subscribes | Never |
|---|---|---|---|
| Client (Unreal, L0 web, Quest, ATAK plugin) | requests to `svc.policy.*`; `cmd.map.>` (A1 view actions) | `evt.so.action.>` (to show status), entities | publish any other `cmd.*` |
| Hermes and other agents | requests to `svc.policy.command.propose` and `svc.policy.task.propose` only; `cmd.map.>` | `evt.so.action.>` for its own `action_id`s | call `svc.policy.*.confirm`; publish `cmd.fleet.>` |
| Policy service | `cmd.>` (the only publisher besides `cmd.map.>`); every `evt.so.action.*` event, including the bridge outcomes it republishes | `svc.policy.*`, `evt.drone.>`, `evt.robot.>` | execute anything itself |
| Asset bridge (MAVLink `services/bridge/bridges/mavlink`, ROS adapter) | `evt.{class}.{own id}.command` (outcomes), `evt.{class}.{own id}.task` (task status, [mission-task.md](mission-task.md) §7); `tel.fleet.{class}.{own ids}.*` | `cmd.fleet.{class}.{own ids}.>` only | accept a command it cannot verify |
| Entity and task services | the result of each `cmd.entity.*` / `cmd.mission.*` they apply, as the reply-free fact `evt.entity.{id}.*` / `evt.mission.{id}.*`; the policy service turns it into `evt.so.action.executed` or `.failed` | their `cmd` domains | — |

Sources: [security-and-trust.md](../research/security-and-trust.md) §1, §3 and Recommendation 1–2;
[agents-that-act.md](../research/agents-that-act.md) §6;
[drones-robots-agents.md](../research/drones-robots-agents.md) "How it applies" 4–5;
[nats-platform.md](../research/nats-platform.md) §4 (`CLIENT` account cannot publish `cmd.>`);
[BUS-DESIGN.md](../design/BUS-DESIGN.md) (bridges publish only `tel.fleet.{class}.{own ids}` and
`evt.{class}.{own ids}`; `evt.so.action.*` has one writer).
**Not enforced on the live bus today**: one account and shared passwords; Hermes can publish any
subject through a generic `nats_publish` tool ([agents-that-act.md](../research/agents-that-act.md)
Risks). The local test stack must run with these permissions so the tests in §9 prove them.

The existing omos MAVLink bridge publishes off-grammar `omos.air.{id}.telemetry` and subscribes
`omos.air.{id}.command`; FEATURES H2 moves it to the subjects above
([drones-robots-agents.md](../research/drones-robots-agents.md) "Existing omos MAVLink bridge").

## 3. Asset verbs

Each verb is an action type `asset.{verb}` with its own parameter schema. MAVLink mapping is from
[drones-robots-agents.md](../research/drones-robots-agents.md) Recommendation 1 and
[PREFLIGHT.md](../PREFLIGHT.md) (scripted drone ACKs arm 400 and takeoff 22). Mappings not in
those sources are marked *unverified* and are the bridge's to confirm.

| Verb | `data` parameters | Level | MAVLink (drone) | Preconditions checked by policy and again by the bridge |
|---|---|---|---|---|
| `goto` | `{lat, lon, hae_m, speed_mps?, yaw_deg?}` | A3 | `MAV_CMD_DO_REPOSITION` | point inside the asset's keep-in zone and outside every keep-out zone; `health.connection = online` |
| `hold` | `{duration_s?}` | A3 | *unverified* (bridge-defined) | online |
| `rtl` | `{}` | A3 | `MAV_CMD_NAV_RETURN_TO_LAUNCH` (20) | online |
| `land` | `{lat?, lon?}` | A3 | `MAV_CMD_NAV_LAND` (21) | online |
| `orbit` | `{center:{lat, lon} \| entity_id, radius_m, hae_m, duration_s}` | A3 | *unverified* | center inside keep-in; target is not a person or a person-class track |
| `observe` | `{target: {lat, lon, hae_m} \| entity_id, sensor_id}` | A3 | gimbal or PTZ, *unverified* | target is not a person or a person-class track |
| `estop` | `{}` | A3 | `MAV_CMD_DO_FLIGHTTERMINATION` (185) | none beyond identity: a stop must not be blocked |
| `arm` / `disarm` | `{}` | A4 / A3 | `MAV_CMD_COMPONENT_ARM_DISARM` (400) | inside keep-in; two distinct humans for `arm` |
| `takeoff` | `{hae_m}` | A4 | `MAV_CMD_NAV_TAKEOFF` (22) | inside keep-in; two humans |
| `estop_release` | `{}` | A4 | bridge-defined | two humans |
| `geofence_override` | `{zone_id, duration_s ≤ 600}` | A4 | none (bridge config) | two humans; time-boxed |

Levels follow [agents-that-act.md](../research/agents-that-act.md) §8 (A3 simulated asset command:
policy + audit + one human confirm; A4 risky: two humans). **Note on `estop`:** the drones lane
placed `estop` behind a two-person confirm
([drones-robots-agents.md](../research/drones-robots-agents.md) Recommendation 1), while the agents
lane lists only "estop release" as A4. This contract sets `estop` to A3 (one confirm) because it is
the H6 kill switch and a second person would delay a stop; it is a design choice for the H6
reviewer to confirm. Agents may propose `estop` but, as always, never confirm it.

Verbs not in this table are refused. In particular there is no strike, engage, designate, payload
release of anything but supplies, or follow-a-person verb
([lattice-patterns.md](../research/lattice-patterns.md) R2; ONTOLOGY §6.2).

## 4. The policy and approval flow

```
requester                policy service                 human (UI/ATAK)        bridge (sim-x1)
   | svc.policy.command.propose ---->|                                |                      |
   |                         | validate, stop line, sim-*,    |                      |
   |                         | capability, preconditions,     |                      |
   |                         | OpenFGA can_propose            |                      |
   |                         |-- evt.so.action.proposed ----->| (UI shows exact      |
   |<-- reply awaiting_conf. |                                |  params + hash)      |
   |                         |<------- svc.policy.command.confirm ----|                      |
   |                         | approver is human, has role,   |                      |
   |                         | hash matches, not expired      |                      |
   |                         |-- evt.so.action.approved       |                      |
   |                         | re-check preconditions, sign   |                      |
   |                         |-- cmd.fleet.drone.sim-x1.goto (JetStream publish, CMD) ->|
   |                         |-- evt.so.action.dispatched     |   verify sig, expiry,|
   |                         |                                |   nonce, own id, zone|
   |                         |<------------ evt.drone.sim-x1.command (ack / executed / failed / refused)
   |<-- evt.so.action.acknowledged / executed / failed / refused (republished by policy)
```

| Step | Who | What | On failure |
|---|---|---|---|
| 1 Propose | requester | Request `svc.policy.command.propose` with an ActionRequest (§6.1). `request_id` is an idempotency key: the same id with the same `params_hash` returns the same `action_id`; with a different hash it is refused (`conflict`), as onemind-core does for missions ([agents-that-act.md](../research/agents-that-act.md) fact 2) | reply `denied` |
| 2 Check | policy | JSON Schema of the action spec; action type is in the catalogue and not on the stop line; target id starts `sim-` (decision 0004 #13); for asset verbs the matching task type is in the target's `capabilities`; preconditions (§3); OpenFGA `can_propose` for the requester, and for an agent the **intersection** of the agent's and the on-behalf-of human's grants ([palantir-patterns.md](../research/palantir-patterns.md) §7). OpenFGA unreachable → deny (fail closed, [security-and-trust.md](../research/security-and-trust.md) §4) | `evt.so.action.denied` with `code` (§6.4); reply `denied`; nothing else happens |
| 3 Hold | policy | Store the action as `awaiting_confirmation` with `params_hash` and `confirm_by` (default now + 120 s). Publish `evt.so.action.proposed`. A0/A1 actions skip steps 3–4 | — |
| 4 Confirm | human | The UI (or ATAK plugin) shows the exact parameters and the hash; the human requests `svc.policy.command.confirm` (§6.2). Policy checks: the confirmer is a **human user** (never an agent principal) with `approver` on the action type and `operator` on the asset; `params_hash` matches; not expired; for A4 a **second, different** human confirms within the window | `evt.so.action.rejected` (human said no) or `.expired` (timeout denies, [agents-that-act.md](../research/agents-that-act.md) §8); a repeat confirm by the same human is refused |
| 5 Dispatch | policy | Publish `evt.so.action.approved`. Re-check preconditions (the world may have changed). Build the command (§6.3), sign it, publish with JetStream so `CMD` records it (the PubAck is the proof of record), then publish `evt.so.action.dispatched` | precondition now false → `evt.so.action.denied` code `precondition_failed` |
| 6 Execute | bridge | Verify signature, `expires_at`, unseen `id` (nonce), target is its own id, `simulated` matches, and its own geofence check before moving the vehicle. Publish `evt.{class}.{id}.command` with `outcome: acknowledged` on receipt, then `executed` or `failed` | `outcome: refused` with `code`; nothing moves |
| 7 Close | policy | Republish each bridge outcome as `evt.so.action.{outcome}` with the full audit fields. No `acknowledged` within 10 s → `evt.so.action.failed` code `no_ack` | — |

**No queued replay.** The bridge holds a core subscription on `cmd.fleet.{class}.{own id}.>`, not a
durable consumer, and every command carries `expires_at` (default `ts` + 30 s). An asset that is
offline does not receive a command later; the UI shows it as offline instead
([nats-platform.md](../research/nats-platform.md) Recommendation §3: "A site that is offline gets no
command"). The Lattice lane proposed durable per-asset consumers that queue tasks until
`delivery.before_t` ([lattice-patterns.md](../research/lattice-patterns.md) §E); this contract does
not follow it, because a stale movement command replayed after reconnection is a safety risk.

**Approvals are bound** to one `action_id` and its `params_hash`, so a human approves exactly what
was shown. Standing approvals ("session", "always") do not exist for A2 and above
([agents-that-act.md](../research/agents-that-act.md) §8). Approval replies over an agent prompt
are not approvals; only `svc.policy.command.confirm` from an authenticated human counts (same source).

## 5. Audit

Every step above publishes one event in the omos event envelope v1
(`services/schemas/envelope/envelope.schema.json`: required `v, id, ts, type, source, subject,
data`) on `evt.so.action.{event}`. `EVT` stores `evt.so.>` for 90 days with `Nats-Msg-Id` = the
envelope `id`. The policy service also writes the decision row to its database in the same
transaction as an outbox entry, fixing the split that onemind-core has today
([agents-that-act.md](../research/agents-that-act.md) §7 and fact 8). Raw prompts are not put on the
bus; only hashes.

`data` fields of every `evt.so.action.*` event:

| Field | Type | Req | Meaning |
|---|---|---|---|
| `action_id` | string `act-{ulid}` | yes | |
| `action_type` | string | yes | e.g. `asset.goto` |
| `action_version` | integer | yes | spec version |
| `level` | enum `A0`–`A4` | yes | |
| `target` | `{entity_type, entity_id}` | yes | |
| `params_hash` | string `sha256:…` | yes | SHA-256 of the JCS-canonical JSON (RFC 8785, https://www.rfc-editor.org/rfc/rfc8785) of `{action_type, action_version, target, params}` |
| `requested_by` | `{kind: user\|agent\|system, id, on_behalf_of?}` | yes | |
| `actor_verified` | boolean | yes | `false` while identities are shared passwords |
| `approvals` | `[{by, at, step: 1\|2}]` | on approved | |
| `policy` | `{store, model_id, checks: [{relation, object, allowed}], conditions}` | yes | |
| `command_id` | uuid | on dispatched | the `id` of the `cmd.*` envelope |
| `task_id` | string | no | when the action dispatches a task |
| `code`, `message` | string | on denied, failed, refused | §6.4 |
| `agent_session`, `model_alias`, `prompt_sha256`, `tool_input_sha256` | string | if agent | ([agents-that-act.md](../research/agents-that-act.md) §7) |
| `simulated` | boolean | yes | |

## 6. Message envelopes

Timestamps in these envelopes are RFC 3339 strings with milliseconds, as the omos command and event
envelopes require; positions inside `data` follow [entity.md](entity.md) units.

### 6.1 ActionRequest (`svc.policy.command.propose` request body)

| Field | Type | Req | Meaning |
|---|---|---|---|
| `v` | integer | yes | `1` |
| `request_id` | uuid | yes | idempotency key |
| `action_type` | string | yes | catalogue name (ONTOLOGY §6.2) |
| `action_version` | integer | yes | spec version the requester built against |
| `target` | `{entity_type, entity_id}` | yes | |
| `params` | object | yes | validated against `services/schemas/actions/{action_type}.v{n}.schema.json` |
| `requested_by` | `{kind, id, on_behalf_of?}` | yes | the policy service replaces it with the verified identity of the NATS user when available |
| `reason` | string ≤ 500 | no | shown to the confirmer |
| `task_id` | string | no | set by the task service |
| `corr`, `causation` | uuid | no | propagated |
| `dry_run` | boolean | no | validate and decide only |

Reply:

| Field | Type | Meaning |
|---|---|---|
| `action_id` | string | |
| `status` | enum | `awaiting_confirmation`, `denied`, `executed` (A0/A1 only) |
| `level` | enum | |
| `params_hash` | string | |
| `confirm_by` | RFC 3339 | |
| `approvals_needed` | integer | 1 (A2/A3) or 2 (A4) |
| `denial` | `{code, message}` \| null | |

```json
{
  "v": 1,
  "request_id": "7b0e7d0c-3a4f-4c7e-9a51-2f0f4f1c9a10",
  "action_type": "asset.goto",
  "action_version": 1,
  "target": { "entity_type": "drone", "entity_id": "sim-x1" },
  "params": { "lat": 30.2690, "lon": -97.7420, "hae_m": 160.0, "speed_mps": 8.0 },
  "requested_by": { "kind": "agent", "id": "agent:hermes/zeus/legacy", "on_behalf_of": "user:zeus" },
  "reason": "Look at the unknown vehicle near the north gate",
  "corr": "1f9a6a52-8d2b-4b8e-b7e4-5f0c2a9d3e11"
}
```

### 6.2 Confirm (`svc.policy.command.confirm` request body)

| Field | Type | Req |
|---|---|---|
| `v` | integer | yes |
| `action_id` | string | yes |
| `params_hash` | string | yes, must equal the stored hash |
| `decision` | enum `approve` \| `reject` | yes |
| `reason` | string | required for `reject` |

The confirmer's identity comes from the authenticated connection (Authentik through NATS auth
callout for browsers, per [security-and-trust.md](../research/security-and-trust.md) §2), never from
the body.

### 6.3 Asset command (`cmd.fleet.{class}.{asset_id}.{verb}`)

The omos command envelope v1 (`services/schemas/command/envelope.schema.json`: required `v, id, ts,
cmd, source, subject, data`) plus OneMind fields:

| Field | Type | Req | Meaning |
|---|---|---|---|
| `v`, `id`, `ts`, `cmd`, `source`, `subject`, `data` | as omos schema | yes | `cmd` = the subject (lowercase); `source` = `service:policy`; `id` doubles as the nonce |
| `action_id` | string | yes | links to the audit trail |
| `expires_at` | RFC 3339 | yes | bridge refuses after this |
| `simulated` | boolean | yes | must be `true` in this run |
| `approval` | object | yes | omos fields `required, status, approver, reason` plus `level`, `params_hash`, `approvals[]` |
| `policy` | `{decision_id, allowed: true}` | yes | |
| `task_id` | string | no | when dispatching a task ([mission-task.md](mission-task.md)) |
| `corr`, `causation`, `trace` | as omos | no | `causation` = the `evt.so.action.approved` event id |
| `sig` | `{alg: "ed25519", kid, value}` | yes (target) | signature over the JCS-canonical envelope without `sig`; bridges verify with the policy service public key from their config ([security-and-trust.md](../research/security-and-trust.md) Recommendation 2, R9). **Planned**; until keys exist, bridges run in `sig_required: false` locally and log a warning |
| `dry_run` | boolean | no | omos field |

```json
{
  "v": 1,
  "id": "c3a1f1e2-5b7d-4e2a-9c1d-0e8f7a6b5c4d",
  "ts": "2026-10-29T12:00:05.120Z",
  "cmd": "cmd.fleet.drone.sim-x1.goto",
  "source": "service:policy",
  "subject": "cmd.fleet.drone.sim-x1.goto",
  "data": { "lat": 30.2690, "lon": -97.7420, "hae_m": 160.0, "speed_mps": 8.0 },
  "action_id": "act-01jbx7m3c4d5e6f7g8h9j0k1m2",
  "expires_at": "2026-10-29T12:00:35.120Z",
  "simulated": true,
  "approval": {
    "required": true, "status": "approved", "approver": "user:zeus", "reason": "",
    "level": "A3",
    "params_hash": "sha256:9f2c4e1b7a0d3c6e8f1a2b4c6d8e0f1a3b5c7d9e1f2a4b6c8d0e2f4a6b8c0d2e",
    "approvals": [ { "by": "user:zeus", "at": "2026-10-29T12:00:04.900Z", "step": 1 } ]
  },
  "policy": { "decision_id": "dec-01jbx7m3c4d5e6f7g8h9j0k1m3", "allowed": true },
  "corr": "1f9a6a52-8d2b-4b8e-b7e4-5f0c2a9d3e11",
  "causation": "5d6e7f80-1a2b-4c3d-8e9f-0a1b2c3d4e5f",
  "sig": { "alg": "ed25519", "kid": "policy-2026-10", "value": "base64…" }
}
```

### 6.4 Outcome events

The bridge reports on `evt.{class}.{asset_id}.command` in the omos event envelope v1 with `data`
`{action_id, command_id, outcome: acknowledged|executed|failed|refused, code?, message?, mavlink?: {command, result}, simulated}`:

```json
{
  "v": 1,
  "id": "2b3c4d5e-6f70-4a81-9b2c-3d4e5f607182",
  "ts": "2026-10-29T12:00:05.980Z",
  "type": "evt.drone.command",
  "source": "bridge-mavlink",
  "subject": "evt.drone.sim-x1.command",
  "causation": "c3a1f1e2-5b7d-4e2a-9c1d-0e8f7a6b5c4d",
  "data": { "action_id": "act-01jbx7m3c4d5e6f7g8h9j0k1m2", "command_id": "c3a1f1e2-5b7d-4e2a-9c1d-0e8f7a6b5c4d",
            "outcome": "executed", "mavlink": { "command": "MAV_CMD_DO_REPOSITION", "result": 0 }, "simulated": true }
}
```

The policy service republishes it with the full audit fields of §5 on `evt.so.action.executed`:

```json
{
  "v": 1,
  "id": "0a1b2c3d-4e5f-4a6b-8c7d-9e0f1a2b3c4d",
  "ts": "2026-10-29T12:00:06.004Z",
  "type": "evt.so.action.executed",
  "source": "service:policy",
  "subject": "evt.so.action.executed",
  "corr": "1f9a6a52-8d2b-4b8e-b7e4-5f0c2a9d3e11",
  "causation": "2b3c4d5e-6f70-4a81-9b2c-3d4e5f607182",
  "data": {
    "action_id": "act-01jbx7m3c4d5e6f7g8h9j0k1m2", "action_type": "asset.goto", "action_version": 1,
    "level": "A3", "target": { "entity_type": "drone", "entity_id": "sim-x1" },
    "params_hash": "sha256:9f2c4e1b7a0d3c6e8f1a2b4c6d8e0f1a3b5c7d9e1f2a4b6c8d0e2f4a6b8c0d2e",
    "requested_by": { "kind": "agent", "id": "agent:hermes/zeus/legacy", "on_behalf_of": "user:zeus" },
    "actor_verified": false,
    "command_id": "c3a1f1e2-5b7d-4e2a-9c1d-0e8f7a6b5c4d",
    "mavlink": { "command": "MAV_CMD_DO_REPOSITION", "result": 0 },
    "simulated": true
  }
}
```

Denial, failure and refusal codes: `schema_invalid`, `unknown_action`, `forbidden_action` (stop
line), `not_simulated`, `not_capable`, `not_authorized`, `precondition_failed` (with
`detail: keep_out|keep_in|battery|offline`), `policy_unavailable`, `conflict`, `expired`,
`rejected_by_human`, `duplicate_confirmation`, `agent_cannot_confirm`, `bad_signature`,
`replayed`, `no_ack`, `vehicle_rejected` (MAVLink result ≠ 0).

## 7. Bridge safety rules (H6)

1. Refuse any command whose target id does not start `sim-` or whose `simulated` is not `true`
   (decision 0004 #13).
2. Refuse expired, replayed (seen `id`) or unsigned commands once signing is on.
3. Run its own keep-in/keep-out check before `goto`, `orbit`, `takeoff`, independent of the policy
   service, because the hub may be unreachable or compromised
   ([security-and-trust.md](../research/security-and-trust.md) §1).
4. Return-to-launch on link loss is autopilot configuration on the vehicle, not a bus command
   (FEATURES H6); the bridge reports the event as `evt.so.action.executed` only for commands it
   received, and reports link loss in the entity `health`.
5. Use MAVLink 2 so signing can be enabled on real links later
   ([drones-robots-agents.md](../research/drones-robots-agents.md) "How it applies" 4).

## 8. Versioning

- Envelope `v` follows the omos schemas: consumers pin a minimum and tolerate unknown fields.
- Each action type has its own spec version (`action_version`). Handlers declare the versions they
  support; a request for an unsupported version is denied (`schema_invalid`), never silently
  upgraded ([palantir-patterns.md](../research/palantir-patterns.md) §3, "Function-backed action
  pinned to a version").
- Adding an optional parameter keeps the version; removing, renaming or retyping one creates
  `{action_type}.v{n+1}.schema.json`.
- New verbs need a catalogue entry, a spec, a level and a review; nothing outside the catalogue runs.

## 9. Acceptance tests (D3, D5, H6, I4)

Run on the local stack (local `nats-server` with per-role users, scripted `sim-x1`):

1. A goto proposed in the client waits for confirmation; after confirm, `sim-x1` reports
   `executed` and its entity moves (D3).
2. A Hermes-proposed goto waits; Hermes calling `svc.policy.command.confirm` is refused with
   `agent_cannot_confirm` (D5).
3. A goto into a keep-out zone is denied by policy, and a hand-crafted command that bypasses policy
   is refused by the bridge (H6).
4. With OpenFGA stopped, every proposal is denied with `policy_unavailable` and logged (I4).
5. No `cmd.fleet.>` message appears without a matching `evt.so.action.approved` by a human
   ([agents-that-act.md](../research/agents-that-act.md) §9 invariants).
6. Replaying a recorded command after its `expires_at` is refused with `expired`.
7. A command to a non-`sim-` id is denied with `not_simulated`.
8. A client user cannot publish `cmd.fleet.>` (permission violation from the server).

## 10. Unverified and open

- The policy service, the OpenFGA model extension and command signing do not exist yet (omos PRs,
  [security-and-trust.md](../research/security-and-trust.md) Recommendation 2).
- MAVLink mappings for `hold`, `orbit`, `observe`, `estop_release`.
- The `estop` level (A3 here, two-person in the drones lane).
- `CMD` retention is 720 h today; 365 d proposed for audit (ONT-P7).
- Identity is shared-password today, so `actor_verified` stays `false` until per-user NATS
  identities and auth callout exist ([security-and-trust.md](../research/security-and-trust.md) R1).
