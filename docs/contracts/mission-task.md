# Contract: missions and tasks

Status: M0, 2026-10-29. Version `v: 1`.
Ontology: [ONTOLOGY.md](../design/ONTOLOGY.md) §3.3 (mission, task), §5 (links), §6 (actions).

A **mission** is a goal with an area, assigned teams and assets, a route and child tasks. A
**task** is one unit of work for one assignee (an asset, a person, a team or an agent), with a
typed specification, a state machine and an approval. The design follows the Lattice task model
(typed tasks, capability gate, fine-grained acknowledgement states, terminal states that never
change) ([lattice-patterns.md](../research/lattice-patterns.md) §E), with OneMind field names,
the omos `mission` domain and the approval flow of [asset-command.md](asset-command.md) §4.

Not to be confused with two existing omos concepts:
- **om-task** (personal and project tasks) uses `task.*` and `evt.task.*`. Asset tasks never use
  those subjects ([lattice-patterns.md](../research/lattice-patterns.md) R4, N4).
- **onemind-core missions** are runs of named operations capabilities with UUID ids on
  `evt.so.mission.{requested|started|completed|failed|cancelled}`
  (`omos/services/schemas/missions/mission-state.schema.json`;
  [agents-that-act.md](../research/agents-that-act.md) fact 3). Field missions here use `msn-` ids on
  `evt.mission.*`. The coarse `rollup` field below reuses that schema's state enum so dashboards can
  treat both alike.

## 1. Subjects, stream and buckets

| Purpose | Subject or key | Defined in (omos) | State |
|---|---|---|---|
| Mission and task records (latest) | KV `mission-state`, keys `msn-{ulid}` and `tsk-{ulid}`, history 5; the KV revision is the compare-and-set guard | `platform/nats/streams/kv-mission-state.yaml` | bucket exists |
| Facts (history) | `evt.mission.{id}.{event}` (`{id}` = `msn-…` or `tsk-…`) | grammar `evt.{domain}.{entity}.{event}` (`CLAUDE.md`, example `evt.mission.patrol1.completed`); stored by `platform/nats/streams/mission.yaml` (`evt.mission.>`, 2160 h) | exists |
| Lifecycle commands | `cmd.mission.{id}.{create\|waypoint\|abort\|complete}` | `docs/NATS-DOCTRINE.md` §9.1 (verbs exactly as listed there); stored by `platform/nats/streams/cmd.yaml` | exists |
| Dispatch to an asset | `cmd.fleet.{class}.{asset_id}.{verb}` with `task_id` | doctrine §9.1; [asset-command.md](asset-command.md) | exists |
| Propose and confirm | `svc.policy.task.propose`, `svc.policy.task.confirm` | grammar `svc.{service}.{endpoint}`; names from [BUS-DESIGN.md](../design/BUS-DESIGN.md) §3.4 (proposal RA2) | **proposed** service |
| Reads and reports | `svc.mission.get`, `svc.mission.list`, `svc.mission.report` (people and agents), `svc.mission.apply` (policy service only) | same | **proposed** (task service) |
| Asset status reports | `evt.{class}.{asset_id}.task` | grammar; `evt.yaml` binds `evt.drone.>`, `evt.robot.>` (not `evt.vehicle.>`, ONT-P13) | exists for drone and robot |
| Task type schemas | `services/schemas/tasks/{name}.v{n}.schema.json`, mirrored to KV `om-schema` | `services/schemas/` (new directory); `platform/nats/streams/kv-om-schema.yaml` | **proposed** files; bucket exists |
| Geofences and AOIs referenced by missions | KV `geofences` | `platform/nats/streams/kv-geofences.yaml` | exists |
| Map route drawing | `cmd.map.mission.draw` | doctrine §9.1 / §9.7 | exists |

No new subject pattern is used. Doctrine §9.1 has no `cancel` verb, so cancelling is `abort`
([lattice-patterns.md](../research/lattice-patterns.md) §E verifier correction).

## 2. Ownership

| Role | Writes | Reads |
|---|---|---|
| omos task service (new; single active replica; may run inside the policy service process, as [BUS-DESIGN.md](../design/BUS-DESIGN.md) assigns KV `mission-state` to the policy service) | KV `mission-state` (only writer, CAS); **every** `evt.mission.>` fact, including assignee reports it republishes; applies `cmd.mission.>`; serves `svc.mission.*` | `evt.so.action.>`, `evt.drone.>`, `evt.robot.>` |
| Policy service | `cmd.mission.*`, `cmd.fleet.*` after approval | `svc.policy.*` |
| Asset bridge | `evt.{class}.{own id}.task` status reports (§7) | its own commands |
| Person (client, ATAK plugin) or agent assignee | status reports through `svc.mission.report` (same body as §7 `data`) | its tasks |
| Clients and Hermes | propose through `svc.policy.task.propose`; read through `svc.mission.*`, KV watch or `evt.mission.>` | — |

Hermes and watch jobs can only **propose**; a human confirms
([lattice-patterns.md](../research/lattice-patterns.md) §E "Approval levels";
[agents-that-act.md](../research/agents-that-act.md) §3). Not enforced on the live bus today (one shared
account, [nats-platform.md](../research/nats-platform.md) §1).

## 3. Records

Times are epoch milliseconds (`_t` suffix), matching [entity.md](entity.md). Ids are lowercase
(`msn-`, `tsk-` plus a lowercase ULID) so they fit the omos `cmd` pattern
(`omos/services/schemas/command/envelope.schema.json`).

### 3.1 Mission record (KV `mission-state` key `msn-…`)

| Field | Type | Req | Meaning |
|---|---|---|---|
| `v` | integer | yes | `1` |
| `mission_id` | string | yes | `msn-{ulid}` |
| `name` | string | yes | human name; also the TAK Server mission name when synced |
| `intent` | string ≤ 2000 | no | what the mission is for |
| `state` | enum | yes | `planned`, `active`, `completed`, `aborted` |
| `rollup` | enum | yes | omos mission-state enum: `requested` (planned), `running` (active), `completed`, `failed`, `cancelled` (aborted) |
| `aoi` | string \| null | no | zone id in KV `geofences` |
| `route` | array | no | `[{seq, lat, lon, hae_m, name?}]`, appended by `waypoint` |
| `assignments` | array | no | `[{to: "{entity_type}.{entity_id}", role, since_t, by}]` (link `assigned_to`) |
| `task_ids` | string[] | yes | child tasks |
| `window` | `{start_t?, end_t?}` | no | |
| `marking` | `{tlp, releasable_to[]}` | yes | as entity.md |
| `simulated` | boolean | yes | `true` if any assigned asset is simulated; rehearsals are always simulated (H7) |
| `tak_mission` | `{name, guid?, synced_t?}` \| null | no | TAK Server Data Sync mission, written by the planned tak-client bridge ([tak-protocols.md](../research/tak-protocols.md) Recommendation 1) |
| `created_t`, `created_by`, `updated_t`, `updated_by` | | yes | actors as `user:…`, `agent:…`, `service:…` |
| `corr` | uuid | no | |

States: `planned` → `active` (when the first child task reaches `executing`) → `completed`
(`cmd.mission.{msn}.complete`, or all child tasks terminal with `done_ok`) or `aborted`
(`cmd.mission.{msn}.abort`, which also aborts every non-terminal child task).

### 3.2 Task record (KV `mission-state` key `tsk-…`)

| Field | Type | Req | Meaning |
|---|---|---|---|
| `v` | integer | yes | `1` |
| `task_id` | string | yes | `tsk-{ulid}` |
| `parent_id` | string \| null | yes | `msn-…` or a parent `tsk-…` (decomposition) |
| `type` | string | yes | task type with version, e.g. `onemind.task.orbit.v1` (§4) |
| `spec` | object | yes | validated against the type's schema |
| `description` | string ≤ 500 | no | shown to operators and TAK users |
| `author` | `{kind: user\|agent\|system, id, on_behalf_of?}` | yes | |
| `assignee` | `{entity_type, entity_id}` | yes | an asset (`drone`, `robot`, `vehicle`, `camera`), `person`, `team` or `agent` |
| `target_snapshots` | array | no | `[{entity_id, snapshot}]`: frozen copies of target entities at creation, never updated |
| `delivery` | `{before_t?, require_ack: true}` | yes | dispatch must be acknowledged by `before_t` (default creation + 60 s) |
| `execution` | `{start_after_t?, complete_before_t?}` | no | |
| `stale_t` | integer | yes | after this a non-terminal task times out (CoT `stale`) |
| `action_id` | string | yes | the approved `task.assign` action ([asset-command.md](asset-command.md) §5) |
| `approval` | `{level, approvals: [{by, at_t, step}]}` | yes | copy of the approval |
| `version` | `{definition: int, status: int}` | yes | `status` increases by one at every state change; stale updates are rejected |
| `status` | object | yes | `{state, error: {code, message}\|null, progress: {pct?, note?}, estimate: {eta_t?}, result: object\|null, start_t?, end_t?, updated_t}` |
| `allocation` | `{entity_type, entity_id}[]` | no | for team tasks: which members or assets took part |
| `rollup` | enum | yes | omos mission-state enum, mapping in §5 |
| `simulated` | boolean | yes | |
| `created_t`, `created_by`, `updated_t`, `updated_by` | | yes | |
| `corr`, `trace` | | no | |

Example (orbit by `sim-x1`, executing):

```json
{
  "v": 1,
  "task_id": "tsk-01jbx7k2m9q4r8s6t0v2w4y6z8",
  "parent_id": "msn-01jbx7a0000000000000000000",
  "type": "onemind.task.orbit.v1",
  "spec": { "center": { "lat": 30.2690, "lon": -97.7420 }, "radius_m": 60, "hae_m": 180, "duration_s": 600 },
  "description": "Overwatch the north gate",
  "author": { "kind": "agent", "id": "agent:hermes/zeus/legacy", "on_behalf_of": "user:zeus" },
  "assignee": { "entity_type": "drone", "entity_id": "sim-x1" },
  "target_snapshots": [],
  "delivery": { "before_t": 1793275265000, "require_ack": true },
  "execution": { "start_after_t": null, "complete_before_t": 1793276400000 },
  "stale_t": 1793276400000,
  "action_id": "act-01jbx7m3c4d5e6f7g8h9j0k1m2",
  "approval": { "level": "A3", "approvals": [ { "by": "user:zeus", "at_t": 1793275204900, "step": 1 } ] },
  "version": { "definition": 1, "status": 5 },
  "status": { "state": "executing", "error": null, "progress": { "pct": 10 },
              "estimate": { "eta_t": 1793275810000 }, "result": null,
              "start_t": 1793275210000, "end_t": null, "updated_t": 1793275210000 },
  "allocation": [],
  "rollup": "running",
  "simulated": true,
  "created_t": 1793275205000, "created_by": "service:policy",
  "updated_t": 1793275210000, "updated_by": "bridge-mavlink",
  "corr": "1f9a6a52-8d2b-4b8e-b7e4-5f0c2a9d3e11"
}
```

## 4. Task types

Task types are versioned JSON Schemas (`services/schemas/tasks/{name}.v{n}.schema.json`). The
rule is Lattice's: add optional fields freely; removing, renaming or retyping a field makes
`v{n+1}` ([lattice-patterns.md](../research/lattice-patterns.md) L8). **Capability gate:** a task can
be assigned to an asset only if the asset entity's `capabilities` lists the exact type
([entity.md](entity.md) §4; Lattice "taskCatalog", L8).

| Type | Spec | Assignee | Dispatch | Level |
|---|---|---|---|---|
| `onemind.task.goto.v1` | `{lat, lon, hae_m, speed_mps?}` | asset | `cmd.fleet.{class}.{id}.goto` | A3 |
| `onemind.task.hold.v1` | `{duration_s?}` | asset | `…hold` | A3 |
| `onemind.task.rtl.v1` | `{}` | asset | `…rtl` | A3 |
| `onemind.task.land.v1` | `{lat?, lon?}` | asset | `…land` | A3 |
| `onemind.task.orbit.v1` | `{center \| target_entity_id, radius_m, hae_m, duration_s}` | asset | `…orbit` | A3 |
| `onemind.task.observe.v1` | `{target: {lat, lon, hae_m} \| entity_id, sensor_id, duration_s?}` | asset with camera | `…observe` | A3 |
| `onemind.task.survey_area.v1` | `{zone_id, hae_m, overlap_pct}` | asset or team | decomposed into child `goto` tasks | A3 per child |
| `onemind.task.follow_route.v1` | `{waypoints[] \| mission_id}` | asset | decomposed into child `goto` tasks | A3 per child |
| `onemind.task.patrol.v1` | `{zone_id \| waypoints[], loops}` | asset | decomposed | A3 per child |
| `onemind.task.inspect_structure.v1` | `{structure_id, standoff_m}` | asset | decomposed into `goto` + `observe` | A3 per child |
| `onemind.task.capture_media.v1` | `{sensor_id, kind: image\|video, duration_s?}` | asset with camera | `…observe` with capture flag | A3 |
| `onemind.task.relay.v1` | `{lat, lon, hae_m, duration_s}` | asset | `…goto` then `…hold` | A3 per child |
| `onemind.task.deliver.v1` | `{to: place_id, item: "supplies"}`; payload is supplies only | asset | decomposed; release step is A4 | A3 / A4 |
| `onemind.task.manual.v1` | `{instructions, location?}` | person or team | none: shown in the client and ATAK plugin | A2 |
| `onemind.task.agent_request.v1` | `{question, context_entity_ids[]}` | agent | onemind-core `delegate-agent` | A2 |

Catalogue source: [lattice-patterns.md](../research/lattice-patterns.md) §E ("initial civilian
catalogue"). Composite types are decomposed by the task service into child tasks with
`parent_id`; **each child passes policy and confirmation on its own** (same source, §F). In v1 the
task service may present all children of one composite for a single confirmation screen, but each
child keeps its own `action_id` and `params_hash`. Bridges only ever receive single verbs from
[asset-command.md](asset-command.md) §3.

**Rejected by policy, always:** any strike, engage or target-designation type, any CoT `t-k-*`
equivalent, any task whose target is a `person` entity or a person-class `track`, any type not in
this catalogue ([lattice-patterns.md](../research/lattice-patterns.md) §E, R2).

## 5. State machine

| `status.state` | Set by | Terminal | `rollup` | Lattice equivalent (L10) | CoT reply to TAK (C1; ATAK support *unverified*) |
|---|---|---|---|---|---|
| `created` | task service | no | requested | CREATED | — |
| `scheduled` | task service (`start_after_t` in future) | no | requested | SCHEDULED_IN_MANAGER | — |
| `sent` | task service on `evt.so.action.dispatched` | no | requested | SENT | task CoT `t-…` |
| `received` | assignee (via §7) | no | running | MACHINE_RECEIPT | `y-a-r` |
| `acknowledged` | assignee | no | running | ACK | `y-a` |
| `wilco` | assignee | no | running | WILCO | `y-a-w` |
| `executing` | assignee | no | running | EXECUTING | `y-s-e` |
| `waiting` | assignee | no | running | WAITING_FOR_UPDATE | `y-s-p` |
| `cancel_requested` | task service on approved `task.abort` | no | running | CANCEL_REQUESTED | `t-u-z` |
| `complete_requested` | task service on `cmd.mission.{tsk}.complete` | no | running | COMPLETE_REQUESTED | — |
| `done_ok` | assignee | **yes** | completed | DONE_OK | `y-c-s` |
| `done_not_ok` + `error.code = rejected` | assignee | **yes** | failed | DONE_NOT_OK + REJECTED | `y-c-f-b` (`-b-v` keep-out, `-b-p` impossible) |
| `done_not_ok` + `cancelled` | assignee | **yes** | cancelled | DONE_NOT_OK + CANCELLED | `y-s-c` |
| `done_not_ok` + `timeout` | task service (`delivery.before_t` or `stale_t` passed) | **yes** | failed | DONE_NOT_OK + TIMEOUT | `y-c-f-s` |
| `done_not_ok` + `failed` / `no_asset` | assignee or task service | **yes** | failed | DONE_NOT_OK + FAILED | `y-c-f`, `y-c-f-a` |
| `replaced` | task service | **yes** | cancelled | REPLACED | `y-c-f` with `<link relation="p-t">` to the new task |

Source: [lattice-patterns.md](../research/lattice-patterns.md) §E state table, renamed to OneMind
terms. Changes from that table: `proposed` and `rejected_by_human` are **action** states held by the
policy service (`evt.so.action.proposed` / `.rejected`), not task states, so a task record exists
only after a human approved it. The CoT projection itself is owned by `cot-projection.md`.

Rules:
1. `version.status` increases by exactly one per change. The task service writes KV with
   compare-and-set on the KV revision and drops an assignee report whose `status_version` is not
   greater than the stored one (Lattice "guard against out of order updates", L10).
2. Terminal states never change. A later report for a terminal task is dropped and counted.
3. An assignee may refuse (`done_not_ok` + `rejected`). On abort it either accepts
   (`done_not_ok` + `cancelled`) or keeps its state and reports `error.code = rejected`; a task not
   yet `sent` is cancelled at once ([lattice-patterns.md](../research/lattice-patterns.md) L9, L10).
4. If no `received` arrives by `delivery.before_t`, the task times out. Dispatch is not retried
   automatically and is never replayed later ([asset-command.md](asset-command.md) §4 "No queued
   replay"); re-tasking is a new proposal.

## 6. Flows

### 6.1 Assign a task to an asset

| # | Subject | From → to | Body |
|---|---|---|---|
| 1 | `svc.policy.task.propose` | client or Hermes → policy | ActionRequest `action_type: "task.assign"`, `target` = the assignee, `params` = `{task_id?, parent_id, type, spec, description, delivery, execution, stale_t}` |
| 2 | `evt.so.action.proposed` | policy → all | audit; the client draws the pending task |
| 3 | `svc.policy.task.confirm` | human → policy | approve with `params_hash` |
| 4 | `cmd.mission.{tsk}.create` | policy → task service | command envelope, `data` = the task spec plus approval |
| 5 | `evt.mission.{tsk}.created` | task service → MISSION | event envelope, `data` = task record; KV `mission-state` written |
| 6 | `cmd.fleet.{class}.{id}.{verb}` | policy → bridge | [asset-command.md](asset-command.md) §6.3 with `task_id` |
| 7 | `evt.so.action.dispatched` | policy | task service sets `sent`, publishes `evt.mission.{tsk}.sent` |
| 8 | `evt.drone.sim-x1.task` (state `received` … `executing` … `done_ok`) | bridge → task service | status report (§7); the task service writes KV and republishes `evt.mission.{tsk}.{state}` |
| 9 | KV update + `evt.mission.{msn}.updated` | task service | mission rollup and entity `current_task` follow |

Steps 4 and 6 are both published by the policy service for the same approved action, in that order;
if step 5 fails, step 6 is not sent and the action ends `failed`.

### 6.2 Abort

`task.abort` is proposed and confirmed like any action (A2 for person, team or agent tasks; A3 for
asset tasks, because it also commands the asset). After approval the policy service publishes
`cmd.mission.{tsk}.abort` (task service → `cancel_requested`) and, for an asset task,
`cmd.fleet.{class}.{id}.hold` with `task_id` and `data.reason: "abort"`. The bridge reports
`done_not_ok` + `cancelled`. `cmd.mission.{msn}.abort` aborts every non-terminal child the same way.

### 6.3 Missions

`mission.create`, `mission.waypoint`, `mission.complete`, `mission.abort` are A2 actions that
execute as `cmd.mission.{msn}.{verb}`. Assignment (`mission.assign`) is a record edit: after
approval the policy service requests the task service's internal `svc.mission.apply` endpoint
(callable only by the policy service user), which updates the record and publishes
`evt.mission.{msn}.assigned`. The client draws the route with the existing `cmd.map.mission.draw`
(doctrine §9.7). Planned: the tak-client bridge mirrors each mission to a TAK Server Data Sync
mission (`/missions/{name}`) so TAK users see the route and tasks
([tak-protocols.md](../research/tak-protocols.md) §4 and Recommendation 1; FEATURES D2).

### 6.4 Watch job or Hermes proposes

A watch job sees, for example, an unknown vehicle track inside zone `zone-north-gate`, raises an
alert entity, and proposes `onemind.task.observe.v1` for `sim-x1` through `svc.policy.task.propose`
with `requested_by.kind: "system"` (watch) or `"agent"`. Nothing happens until a human confirms
([lattice-patterns.md](../research/lattice-patterns.md) §F; [agents-that-act.md](../research/agents-that-act.md) §4).

## 7. Status reports

An asset bridge reports on `evt.{class}.{asset_id}.task` (stored by `EVT` for `drone` and `robot`;
`evt.yaml` binds `evt.drone.>` and `evt.robot.>`). This keeps each bridge's publish permission
limited to its own ids ([BUS-DESIGN.md](../design/BUS-DESIGN.md) bridge rows). People and agents
report through `svc.mission.report`. The task service validates the report (§5 rules), writes KV
and republishes it as `evt.mission.{tsk}.{state}`, the fact clients read.

Both use the omos event envelope v1 (`services/schemas/envelope/envelope.schema.json`). On
`evt.mission.{tsk}.{state}`, `type` is the subject with the id replaced by `task`
(`evt.mission.task.executing`), so the type stays a fixed string while the subject carries the id
(the envelope schema allows `type` and `subject` to differ). The example below is the republished
fact; the bridge's own report has `subject: "evt.drone.sim-x1.task"`, `type: "evt.drone.task"` and
the same `data`.

`data`:

| Field | Type | Req |
|---|---|---|
| `task_id` | string | yes |
| `state` | enum (§5) | yes |
| `status_version` | integer | yes |
| `error` | `{code: rejected\|cancelled\|timeout\|failed\|no_asset, message}` \| null | for `done_not_ok` |
| `progress` | `{pct?, note?}` | no |
| `estimate` | `{eta_t?}` | no |
| `result` | object | no (type-specific; e.g. `capture_media` gives an Object Store key) |
| `at_t` | integer | yes |
| `simulated` | boolean | yes |

```json
{
  "v": 1,
  "id": "6c7d8e9f-0a1b-4c2d-8e3f-4a5b6c7d8e9f",
  "ts": "2026-10-29T12:00:10.000Z",
  "type": "evt.mission.task.executing",
  "source": "service:task",
  "subject": "evt.mission.tsk-01jbx7k2m9q4r8s6t0v2w4y6z8.executing",
  "corr": "1f9a6a52-8d2b-4b8e-b7e4-5f0c2a9d3e11",
  "data": {
    "task_id": "tsk-01jbx7k2m9q4r8s6t0v2w4y6z8",
    "state": "executing",
    "status_version": 5,
    "error": null,
    "progress": { "pct": 10 },
    "estimate": { "eta_t": 1793275810000 },
    "at_t": 1793275210000,
    "simulated": true
  }
}
```

`Nats-Msg-Id` = `{task_id}:{status_version}` on both subjects, so a re-sent report is
deduplicated by `EVT` and `MISSION` (2 m window, `evt.yaml`, `mission.yaml`).

## 8. How a client reads missions and tasks

- Snapshot: KV watch on `mission-state` (keys `msn-*`, `tsk-*`), or `svc.mission.list`
  `{state?, assignee?, bbox?}`.
- Live: the same KV watch, or a JetStream ordered consumer on `evt.mission.>`.
- Pending (not yet approved) tasks: `evt.so.action.proposed` with `action_type: "task.assign"`.
- Clients never write `mission-state`.

## 9. Versioning

- Record and envelope `v` as in [entity.md](entity.md) §9 (add optional fields freely; breaking
  change bumps `v`; consumers tolerate unknown fields).
- Task types carry their version in the name (`….v1`). Handlers list the versions they accept;
  assigning an unsupported version is denied. The asset advertises exact versions in
  `capabilities`.
- `version.definition` increases when the task's own spec is edited before dispatch (only
  allowed in `created` or `scheduled`, through a new approved action).

## 10. Acceptance tests (D2, H4, H5)

1. A goto task for `sim-x1` proposed by Hermes waits, is confirmed by a human, reaches `done_ok`,
   and every step is on `evt.mission.{tsk}.*` and `evt.so.action.*` (H4).
2. An out-of-order report (`status_version` 3 after 5) is dropped; a report after `done_ok` is
   dropped.
3. Assigning `onemind.task.orbit.v1` to an asset whose `capabilities` lacks it is denied
   (`not_capable`).
4. An `observe` task targeting a person-class track is denied (`forbidden_action`).
5. A survey of a drawn zone by three simulated drones produces child tasks with `parent_id`, each
   with its own `action_id` (H5).
6. A task not acknowledged by `delivery.before_t` ends `done_not_ok` + `timeout` and is not sent
   again.
7. A mission created in the client appears on ATAK as a route (D2; on-Mac and LAN check in
   `docs/OWNER-CHECKLIST.md`, awaiting owner).

## 11. Unverified and open

- The task service, policy service and `services/schemas/tasks/` do not exist yet (omos PRs).
- ATAK and iTAK handling of `t-*` tasking and `y-*` replies
  ([lattice-patterns.md](../research/lattice-patterns.md) R8); the authoritative path is the bus.
- TAK Server 5.8 mission API details come from 5.7 public source
  ([tak-protocols.md](../research/tak-protocols.md) §4).
- Whether one confirmation screen for all children of a composite task satisfies Rule 6, or each
  child needs its own click, is for the owner; this contract keeps one approval record per child.
- KV `mission-state` keeps history 5 only; full history is the `MISSION` stream (90 days).
