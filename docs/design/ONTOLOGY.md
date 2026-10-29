# OneMind ontology

Status: M0 design, 2026-10-29. This drives Phase 2 (entity service A4, tasking H4, asset
commands D3/D5, policy I4, agent tools I2).

The ontology is the shared model of the world that every OneMind part reads and writes:
the entity service, the task and policy services, Hermes, the Unreal and web clients, and
the TAK projection. It follows the *design* of the Palantir Foundry ontology (object types,
link types, actions, functions, interfaces) and the Anduril Lattice entity and task model,
built on NATS and TAK. No Palantir or Lattice code, protos, names or branding are used
([palantir-patterns.md](../research/palantir-patterns.md) Risk 7,
[lattice-patterns.md](../research/lattice-patterns.md) R1).

Exact wire formats are in the contracts:
[entity.md](../contracts/entity.md),
[mission-task.md](../contracts/mission-task.md),
[asset-command.md](../contracts/asset-command.md). CoT detail is owned by
`cot-projection.md` and `robotics-cot-profile.md` (other lanes).

## 1. Principles

| # | Principle | Source |
|---|---|---|
| 1 | **The bus is the authority.** Clients are views and input devices, never the source of truth | onemind-xr `CLAUDE.md` rule 3 |
| 2 | **Actions are the only write path.** Every change made by a person or an agent is a named action: request → policy and approval → executor → outcome event. Nobody writes KV or `ent.*` directly except the service that owns it | [palantir-patterns.md](../research/palantir-patterns.md) Recommendation 2; [agents-that-act.md](../research/agents-that-act.md) Recommendation 2 |
| 3 | **One owner per subject family.** Bridges own observations; the entity service owns `ent.*`; the task service owns task records; only the policy service publishes `cmd.*` | [lattice-patterns.md](../research/lattice-patterns.md) §B; [security-and-trust.md](../research/security-and-trust.md) §3 |
| 4 | **Observations are not entities.** Raw data stays on `tel.*`, `det.*`, `intel.*`, `tak.cot.>`; fused state is on `ent.*` | [lattice-patterns.md](../research/lattice-patterns.md) §B; [fusion-and-tracking.md](../research/fusion-and-tracking.md) Stage 0 |
| 5 | **Locked grammar.** Only the patterns in omos `CLAUDE.md` and `docs/NATS-DOCTRINE.md` §9.1 are used. Gaps are owner proposals (decision 0004 #8), listed in §9 | omos `CLAUDE.md` "NATS Subject Grammar"; decision 0004 #8 |
| 6 | **Civilian stop line.** No weapons, no targeting, no tracking or identifying people as targets. The forbidden actions do not exist in the catalogue and policy rejects them | [palantir-patterns.md](../research/palantir-patterns.md) §9 and Recommendation 6; [lattice-patterns.md](../research/lattice-patterns.md) R2 |
| 7 | **Simulated assets only in this run.** Asset ids are `sim-*`; every asset command passes policy, audit and a human confirm, even in simulation | decision 0004 #13; ULTRACODE-PROMPT Rule 6 |

## 2. Palantir and Lattice concepts mapped to OneMind

| Concept | Palantir (Foundry/AIP) | Lattice | OneMind | Where it lives |
|---|---|---|---|---|
| Kind of thing | Object type | Template plus `ontology` component | **Object type** with a versioned JSON Schema | omos `services/schemas/entity/entity.v1.schema.json` (proposed), published to KV `om-schema` (omos `platform/nats/streams/kv-om-schema.yaml`) |
| One real thing | Object | Entity | **Entity**: latest fused state | `ent.{entity_type}.{entity_id}.state` |
| Shared behaviour | Interface | Template (minimum components) | **Trait** (§4) | `traits[]` in the schema |
| Relation | Link type | `relationships[]`, `correlation` | **Link** stored on the record, inverse kept by the owner | `relationships[]` (entity.md §5) |
| Field | Property | Component field | **Property** with source, time and confidence | entity.md |
| Edit | Action type | Override, Task | **Action** (§6) | asset-command.md |
| Read logic | Function | — | **Function** = NATS request/reply | `svc.{service}.{endpoint}` |
| Human edits vs source data | Writeback, per-property strategy | Overrides (last writer wins) | Overrides layer, per-property `override_wins` or `newest_wins` | entity.md §6 |
| Work for an asset | Action with side effects | Task + TaskStatus | **Task** inside a **mission** | mission-task.md |
| AI that acts | AIP agents with tools | Autonomy services using the same APIs | Hermes with query, function and proposal tools only | [agents-that-act.md](../research/agents-that-act.md) §1 |

Sources: [palantir-patterns.md](../research/palantir-patterns.md) "How it applies" §1–§7;
[lattice-patterns.md](../research/lattice-patterns.md) §A–§H.

## 3. Object types

### 3.1 Summary

"Store" is where the latest state lives. "Facts" is where change history lives. Stream and
bucket names are the omos CR names in `omos/platform/nats/streams/`. **(P)** marks an item
that does not exist yet and needs an omos PR or owner proposal (§9).

| Object type | Subject token / key | Store (latest) | Facts (history) | Writes come from | Traits |
|---|---|---|---|---|---|
| person | `ent.person.{id}.state` | ENT (P) | `evt.entity.{id}.*` (P) | entity service from `tak.cot.out` (TAK EUDs) and team actions | locatable |
| team | `ent.team.{id}.state` | ENT (P) | `evt.entity.{id}.*` (P) | `cmd.team.{id}.{verb}` via policy | — |
| asset | abstract (trait `taskable`), not a token | — | — | — | taskable |
| drone | `ent.drone.{id}.state` | ENT (P) | `tel.fleet.drone.{id}.*` in TEL; `evt.drone.{id}.*` in EVT; `evt.mission.*` | entity service from `tel.fleet.drone.*` | locatable, trackable, taskable, observable |
| robot | `ent.robot.{id}.state` | ENT (P) | `tel.fleet.robot.{id}.*` in TEL; `evt.robot.{id}.*` in EVT | entity service from `tel.fleet.robot.*` | locatable, trackable, taskable |
| vehicle | `ent.vehicle.{id}.state` | ENT (P) | `tel.fleet.vehicle.{id}.*` in TEL; `intel.*` | entity service | locatable, trackable, (taskable if own) |
| sensor | `ent.sensor.{id}.state` | ENT (P) | `tel.*` | entity service from sensor bridges | locatable, observable |
| camera | `ent.camera.{id}.state` | ENT (P); video availability in KV `OVERWATCH_VIDEO` | `det.*` it produces | entity service; video-registry writes the `media` block | locatable, observable |
| place | `ent.place.{id}.state` | ENT (P), `pinned` | `evt.entity.{id}.*` (P) | `cmd.entity.{id}.*` actions; TAK markers | locatable |
| structure | `ent.structure.{id}.state` | ENT (P), `pinned` | `evt.entity.{id}.*` (P) | actions; twin import | locatable |
| zone | `ent.zone.{id}.state` (mirror) | **KV `geofences`** is authoritative | `evt.map.geofence.{id}.{enter\|exit}` in MISSION | `cmd.map.geofence.{id}.{verb}` via policy | locatable (geo) |
| mission | KV `mission-state` key `msn-{ulid}` | KV `mission-state` | `evt.mission.{id}.{state}` in MISSION | task service via `cmd.mission.{id}.{verb}` | — |
| task | KV `mission-state` key `tsk-{ulid}` | KV `mission-state` | `evt.mission.{id}.{state}` in MISSION | task service; asset bridges report status | — |
| event | `ent.event.{id}.state` | ENT (P) | `intel.*` in INTEL; `evt.world.*` in WORLD | entity service from feeds | locatable (geo) |
| alert | `ent.alert.{id}.state` | ENT (P) | `evt.map.geofence.*` (exists); `evt.entity.{id}.alert` (P) | watch jobs, entity service health rules | — |
| observation | none (raw message) | — | `tel.*` TEL, `det.*` DET (P), `intel.*` INTEL, `tak.cot.>` COT | bridges and collectors | — |
| track | `ent.track.{id}.state` | ENT (P) | `det.*` DET (P) | entity service (fusion) | locatable, trackable |
| agent | `ent.agent.{id}.state` | ENT (P); KV `agent-registry` | `agents.hb.>`, `agents.status.>` in AGENTS | entity service from agent registry and heartbeats | — |
| feed | KV `feed-config` key `{source}` | KV `feed-config` | `tel.feed.{source}.status` (not captured by TEL today) | ingestion pipeline | — |

Support types used by the entity vocabulary but not in the required list: `aircraft` and
`vessel` (world feeds, `ent.aircraft.icao-…`, `ent.vessel.mmsi-…`) and `node` (edge
computers, `tel.fleet.node.*`). The full `entity_type` token list is in
[entity.md](../contracts/entity.md) §3.

Evidence for the stores: KV and stream CRs in `omos/platform/nats/streams/`
(`kv-geofences.yaml` history 10, `kv-mission-state.yaml` history 5, `kv-fleet-state.yaml`
TTL 30 s, `kv-world-state.yaml` TTL 5 m, `kv-agent-registry.yaml`, `kv-overwatch-video.yaml`,
`kv-feed-config.yaml`; `mission.yaml` binds `evt.mission.>` and `evt.map.>`; `tel.yaml` binds
`tel.fleet.>`; `cot.yaml` binds `tak.cot.>`). No stream binds `ent.>` or `det.>` and no stream
binds `evt.entity.>` ([lattice-patterns.md](../research/lattice-patterns.md) N2;
`omos/platform/nats/streams/evt.yaml`). `tel.feed.*` is used by `feed-intel`
([ingestion-platform.md](../research/ingestion-platform.md) line 138) but `tel.yaml` does not
bind it.

### 3.2 Properties common to every entity

Every entity record carries the doctrine §9.2 geo-envelope fields (`uid`, `type`, `lat`,
`lon`, `t`, …) at the top level for existing consumers, plus the fields the TAK projector
already reads (`entity_id`, `entity_type`, `confidence`, `properties`), plus components. Full
definition: [entity.md](../contracts/entity.md) §4.

| Group | Properties | Writeback strategy |
|---|---|---|
| Identity | `entity_id`, `entity_type`, `kind`, `aliases{name, ids[]}`, `template` (`asset\|track\|geo\|spi`) | `override_wins` for `name` |
| Lifecycle | `v`, `seq`, `live`, `lifecycle` (`tentative\|active\|stale\|lost\|expired`), `created_t`, `expiry_t`, `pinned` | service-owned |
| Location | `location{lat, lon, hae_m, agl_m, course_deg, speed_mps, vel_enu_mps, measured_t}`, `uncertainty{ce_m, le_m, pos_enu_cov}` | `newest_wins` |
| Geometry | `geo{shape, coords, kind}` for places, structures, zones, events | `override_wins` |
| Classification | `disposition` (no `hostile`), `environment`, `cot_type` | `override_wins` |
| Health | `health{connection, overall, battery_pct, link_rssi, alerts[]}` | `newest_wins` |
| Capability | `capabilities[]` (task types it accepts), `current_task` | service-owned |
| Media | `media[]` (video URL, Object Store reference) | video-registry |
| Provenance | `provenance{primary, how, source_t}`, `sources[]` with `{stream, seq}` pointers, `confidence`, `confidence_parts`, `conflicts[]` | service-owned |
| Edits | `overrides[]` (who, what, when, status) | from actions only |
| Markings | `marking{tlp, releasable_to[]}`, `indicators{simulated, exercise, emergency, egress_tak}` | `override_wins` |

Sources: [lattice-patterns.md](../research/lattice-patterns.md) §A;
[fusion-and-tracking.md](../research/fusion-and-tracking.md) "Confidence", "Provenance",
"Conflicting data"; [palantir-patterns.md](../research/palantir-patterns.md) §5;
[security-and-trust.md](../research/security-and-trust.md) §5 (TLP 2.0 marking).

### 3.3 Each object type in detail

Each row: what it is, type-specific properties, the CoT mapping, and the Palantir and Lattice
equivalents. CoT types marked *(projector)* come from `COT_TYPE_MAP` in
`omos/platform/omos/tak-projector.yaml` lines 61–100; others cite their source. Final CoT detail
is owned by `cot-projection.md` and `robotics-cot-profile.md`.

| Type | Meaning and specific properties | CoT | Palantir | Lattice |
|---|---|---|---|---|
| **person** | A team member or a safety subject (missing or injured person reported by the team). Properties: `callsign`, `role`, `team`, `safety_status` (`ok\|needs_help\|injured\|missing`). **Never created from camera or ML detection.** People seen by sensors stay anonymous `track` entities (below) | `a-f-G-U-C` *(projector `human`)*; `<contact callsign>`, `<__group name role>` ([tak-protocols.md](../research/tak-protocols.md) §8 spike) | Object type (custom) | Entity with `group_details.team`; no person template |
| **team** | A named group of people and assets. Properties: `name`, `color` (TAK team colour), `members[]` via links, `leader` | No own event; members carry `<__group name role>`; leader `<link relation="p-l">` ([lattice-patterns.md](../research/lattice-patterns.md) §H) | Object type; many-to-many with attributes becomes its own object (`assignment`) | `group_details{team, echelon}`; Principal type `team` for task assignees (L10) |
| **asset** | Abstract: anything OneMind can task. Implemented by drone, robot, vehicle, sensor, camera when the trait `taskable` is present | — | Interface | `TEMPLATE_ASSET` |
| **drone** | Own UAV. `airframe` (`multirotor\|fixed_wing\|vtol`), `mode`, `battery_pct`, `home`, `geofence_id`, `capabilities[]` | `a-f-A-M-H-Q` multirotor, `a-f-A-M-F-Q` fixed wing; `<track>`, `<status battery>`, `<__video>`, `<sensor>` ([drones-robots-agents.md](../research/drones-robots-agents.md) "How it applies" 3) | Object type implementing `taskable` | Asset entity, `task_catalog`, `health`, `sensors` |
| **robot** | Own ground robot or robot dog. As drone, plus `locomotion` | `a-f-G-E-V` until a UGV type is proven ([drones-robots-agents.md](../research/drones-robots-agents.md) Risks); the projector uses `a-f-G-U-C-R` today. **Conflict for the robotics profile to settle** | Object type | Asset entity |
| **vehicle** | A car, truck, ATV or boat. Own (taskable only if it has a bridge) or seen in a feed (GTFS-RT transit). `plate` is never stored | `a-f-G-E-V` ([drones-robots-agents.md](../research/drones-robots-agents.md)) or `a-f-G-U-C-V` *(projector `vehicle`)*; same conflict | Object type | Asset or track entity |
| **sensor** | A fixed or mounted non-imaging sensor (weather, soil, RF, acoustic). `sensor_type`, `fov`, `operational_state` | `a-f-G-I-S` *(projector `sensor`)*; `<sensor>` for coverage | Object type implementing `observable` | `sensors[]` component on the host entity, or own entity |
| **camera** | Fixed, PTZ or drone-mounted camera. `fov{h_deg, v_deg, az_deg, el_deg, range_m}`, `ptz`, `stream_id` | `a-f-G-I-S-C` *(projector `camera`)*; `<__video url>` and `<sensor azimuth fov vfov range>` ([drones-robots-agents.md](../research/drones-robots-agents.md) "CoT for UAVs and UGVs") | Object type | `sensors[]` with `fields_of_view`; `media[]` |
| **place** | A named point: gate, well, rally point, landing pad. `geo.shape=point`, `pinned=true` | `b-m-p-s-m` spot marker ([lattice-patterns.md](../research/lattice-patterns.md) C3) | Object type | `TEMPLATE_GEO` point, `noExpiry` |
| **structure** | A building or fixed installation in the digital twin: barn, house, tower. `footprint` polygon, `height_m`, `tileset_ref` (3D Tiles in Object Store) | No verified CoT type; send as `u-d-f` footprint polygon (*unverified* choice) | Object type | `TEMPLATE_GEO` polygon with `dimensions` |
| **zone** | Geofence, AOI, keep-in, keep-out, hazard area. `geo.kind` (`aoi\|keep_in\|keep_out\|hazard\|control_area`), `watch` flag, `rules` (alt limits). Authoritative copy in KV `geofences` | `u-d-c-c` circle, `u-d-f` free form, `u-d-r` rectangle (C3); `__geofence` detail element exists ([tak-protocols.md](../research/tak-protocols.md) §1) | Object type | `TEMPLATE_GEO`, `geo_details.type` (CONTROL_AREA, HAZARD …; `ENGAGEMENT_ZONE` excluded) |
| **mission** | A goal with an area, assigned teams and assets, and child tasks. `name`, `intent`, `aoi` (zone id), `route`, `state`, `window` | TAK Server mission (Data Sync, `/missions/{name}`) through the tak-client bridge; route `b-m-r` with `b-m-p-w` links ([tak-protocols.md](../research/tak-protocols.md) Recommendation 1; C3); order `t-o-o` (C1) | Object type plus actions (plan, execute, report; Gotham description *unverified*, [palantir-patterns.md](../research/palantir-patterns.md) §6) | Task with children (`parent_task_id`); Mission Autonomy decomposition (press only) |
| **task** | One unit of work for one assignee (asset, team, person, agent). Typed (`onemind.task.orbit.v1` …), state machine, approval | `t-s-v`, `t-s-i`, `t-r`, `t-x-v-m`; replies `y-*` with `<link relation="p-t">`. ATAK support is *unverified* ([lattice-patterns.md](../research/lattice-patterns.md) R8) | Action-backed object; staged proposal | Task, TaskStatus, task definition registry (L7–L10) |
| **event** | A real-world occurrence from a feed or a report: earthquake, wildfire, storm, incident. `event_kind`, `severity`, `area` (CAP polygon), `source_ref` | Projector types (`fire` `b-r-.-H-.`, `earthquake`, `flood` …); CAP-area shapes planned ([standards.md](../research/standards.md) Recommendation 2) | Object type | `TEMPLATE_GEO` with `geo_details.type` HAZARD/EMERGENCY |
| **alert** | Something needs attention: geofence crossing, stale asset, feed outage, low battery. `alert_kind`, `level` (`advisory\|caution\|warning`), `about` (entity id), `raised_by`, `state` (`open\|acknowledged\|resolved`) | No verified CoT type; `emergency` detail element exists ([tak-protocols.md](../research/tak-protocols.md) §1). Mapping *unverified*, owned by cot-projection | Object type; Maven-style workbench turns alert into mission ([palantir-patterns.md](../research/palantir-patterns.md) §9) | `health.active_alerts[]` (alert_code, level, activated_time) |
| **observation** | One raw report from one source at one time: a telemetry sample, a detection, an intel record, a CoT event. Never edited. Referenced from entities by `{stream, seq}` | Incoming CoT is itself an observation on `tak.cot.out` | Backing datasource row | A publish into the entity API |
| **track** | A fused estimate of something not self-identifying (camera or RF detections). `tracked{quality 0–15, hits, last_meas_t, lob}`, `class` (`vehicle\|person\|animal\|unknown`), `correlation`. **Class `person` tracks are marking AMBER+STRICT, short TTL, never projected to TAK, never a task target, never linked to a `person` entity** | `a-u-…` (unknown affiliation tree, C1); `how="m-f"` when fused | Object type | `TEMPLATE_TRACK`, `tracked`, `correlation` |
| **agent** | Hermes or another AI agent, shown as a contact (I1). `agent_ref` (`agent/owner/name` from the Synadia registry), `status`, `current_task`, `trust_level` | Contact with `<contact callsign>`; CoT type to be set by cot-projection (*unverified*) | AIP agent | Taskable agent / Principal `system` (L7, L10) |
| **feed** | A data source in the catalogue. `source_id`, `delivery_mode`, `cadence_s`, `licence`, `fetched_at`, `count`, `last_error`, `stale` | Not projected | Datasource / pipeline | `provenance.integration_name` |

Sources for the type list and properties: ULTRACODE-PROMPT Phase 1 "Then synthesize";
[lattice-patterns.md](../research/lattice-patterns.md) §A (entity_type values, templates);
[video-and-plugins.md](../research/video-and-plugins.md) lines 214–258 (video-registry, KV
`OVERWATCH_VIDEO`); [ingestion-platform.md](../research/ingestion-platform.md) lines 182, 392
(feed status fields); [docs/sources/SOURCE-REGISTRY.md](../sources/SOURCE-REGISTRY.md) (feed
catalogue).

## 4. Traits (interfaces)

| Trait | Means | Required properties | Who uses it |
|---|---|---|---|
| `locatable` | Has a position or geometry; can be projected to CoT | `lat`, `lon` (or `geo`), `t`, `uncertainty.ce_m` | Client layers, CoT projector |
| `trackable` | Has kinematics and a filter state | `location.speed_mps`, `course_deg`, `uncertainty` | Fusion, prediction at render time |
| `taskable` | Accepts tasks and commands | `capabilities[]`, `health.connection`, `current_task` | Task and policy services |
| `observable` | Has a sensor footprint | `sensors[].fov` | Coverage layers, KLV footprint (K5) |

Client layers and the projector select by trait, not by type
([palantir-patterns.md](../research/palantir-patterns.md) §1).

## 5. Link types

Links are stored in the `relationships[]` array of the entity record (or in the mission/task
record for mission links). The owning service maintains the inverse side, so a traversal is one
read per hop. A link change is also a fact on `evt.entity.{id}.linked` / `.unlinked` (ONT-P2).
Sources: [palantir-patterns.md](../research/palantir-patterns.md) §2;
[lattice-patterns.md](../research/lattice-patterns.md) §A (relationship types `tracked_by`,
`group_child`, `merged_from`).

| Link (forward / inverse) | From → to | Cardinality | Set by | CoT | Lattice |
|---|---|---|---|---|---|
| `member_of` / `has_member` | person, asset, agent → team | many-to-many | `team.add_member` action | `<__group>` | `group_child`/`group_parent` |
| `leads` / `led_by` | person → team | one-to-one | `team.set_leader` | `<link relation="p-l">` | — |
| `operates` / `operated_by` | person → asset | many-to-many | asset registry, action | `<link relation="p-o">` | — |
| `assigned_to` / `has_assignment` | team, person, asset → mission | many-to-many (carries role, since) | `mission.assign` | TAK mission membership | relations.assignee |
| `part_of` / `has_task` | task → mission; task → task | many-to-one | task service | `t-o-o` order | `parent_task_id` |
| `assignee` / `tasked_with` | task → asset, team, person, agent | one-to-one per task | `task.assign` | `<link relation="t-s">` | relations.assignee |
| `targets` / `targeted_by` | task → place, zone, structure, track, event (**never person**, never a person-class track) | many-to-one | `task.assign` | `<link relation="t-o">` | objective `entity_id` |
| `inside` / `contains` | locatable → zone, place, structure | computed | entity service (geofence check) | — | — |
| `mounted_on` / `carries` | sensor, camera → drone, robot, vehicle, structure | many-to-one | registry, action | — | `sensors[]` on host |
| `tracked_by` / `tracks` | track → sensor, camera, drone | many-to-many | fusion | — | `tracked_by` |
| `correlated_with` | secondary → primary entity | many-to-one | fusion (`automated`) or `entity.correlate` (`manual`) | `<link relation="c-c">` or `c-f` | `correlation` |
| `derived_from` | entity → observation `{stream, seq}` | many | entity service | `<link relation="p-s">` | `provenance` |
| `about` / `has_alert` | alert → any entity | many-to-one | watch job, health rule | — | `active_alerts` |
| `raised_by` | alert → agent, feed, zone (watch) | many-to-one | watch job | — | — |
| `produced_by` | observation, event → feed, sensor, camera | many-to-one | collector | — | `integration_name` |
| `streams` / `stream_of` | camera, drone → video stream id (KV `OVERWATCH_VIDEO`) | one-to-many | video-registry | `<__video url>` | `media[]` |
| `proposed_by` | task, action request → agent or person | many-to-one | policy service | — | `author` Principal |

## 6. Actions

An action has a spec: name, target type, parameter schema, preconditions, approval level,
policy relation, side effects and handler version. Specs are JSON Schemas in omos
`services/schemas/actions/{name}.v{n}.schema.json` (P, new directory) published to KV `om-schema`.
The request and approval flow is in [asset-command.md](../contracts/asset-command.md) §4; it
applies to every action, not only asset commands.

### 6.1 Approval levels

| Level | Meaning | Gate |
|---|---|---|
| A0 | Read | none; logged |
| A1 | Reversible UI or low-risk shared marker | none; logged; undoable |
| A2 | Non-kinetic write (entity edit, zone, mission, task to a person) | policy + one human confirm |
| A3 | Asset command, simulated | policy + audit + one human confirm, always (Rule 6) |
| A4 | Risky asset command | policy + two distinct humans |
| A5 | Never through OneMind actions (credentials, cluster, foundation) | not exposed |

Source: [agents-that-act.md](../research/agents-that-act.md) §8. An agent can propose at any
level but can never confirm or approve ([agents-that-act.md](../research/agents-that-act.md) §6).

### 6.2 Catalogue

"Executes as" is the subject the policy service (or the task service, for tasks) publishes
after approval. All are inside the locked grammar; `cmd.>` is stored by `CMD`
(`omos/platform/nats/streams/cmd.yaml`).

| Action | Target | Executes as | Level | Notes |
|---|---|---|---|---|
| `entity.override` | any entity | `cmd.entity.{id}.override` | A2 | Field must be overridable; recorded in `overrides[]` |
| `entity.correlate` / `entity.decorrelate` | entity pair | `cmd.entity.{id}.correlate` / `.decorrelate` | A2 | Manual beats automated ([lattice-patterns.md](../research/lattice-patterns.md) §C) |
| `entity.delete` | place, structure, team, manual entity | `cmd.entity.{id}.delete` | A2 | Tombstone with actor and reason |
| `entity.star` | any | `cmd.entity.{id}.star` | A1 | |
| `map.highlight`, `map.viewport`, `map.filter`, `map.annotate` | view | `cmd.map.highlight.{uid}`, `cmd.map.viewport.set`, `cmd.map.filter.set`, `cmd.map.annotation.{id}.{verb}` | A1 | Doctrine §9.7 (omos `docs/NATS-DOCTRINE.md`) |
| `zone.create` / `zone.update` / `zone.remove` | zone | `cmd.map.geofence.{id}.{create\|update\|remove}` | A2 | Doctrine §9.1 lists `cmd.map.geofence.{id}.{verb}`; writes KV `geofences` |
| `place.create` / `structure.create` | place, structure | `cmd.entity.{id}.create` | A2 | |
| `team.create`, `team.add_member`, `team.remove_member`, `team.set_leader` | team | `cmd.team.{id}.{verb}` | A2 | |
| `person.set_safety_status` | person | `cmd.entity.{id}.override` (field `safety_status`) | A2 | Team members and safety subjects only |
| `mission.create`, `mission.waypoint`, `mission.abort`, `mission.complete` | mission | `cmd.mission.{id}.{create\|waypoint\|abort\|complete}` | A2 | Verbs exactly as doctrine §9.1 |
| `mission.assign` | mission + team/person/asset | no `cmd`: the task service edits the mission record and publishes `evt.mission.{id}.assigned` | A2 | Doctrine §9.1 has no `assign` verb; assignment is a record edit, not an asset command |
| `task.assign` | task → asset | `cmd.mission.{tsk}.create`, then `cmd.fleet.{class}.{id}.{verb}` with `task_id` | A3 (A4 for risky verbs) | mission-task.md §6.1; composite types are split into child tasks, each approved |
| `task.assign` | task → person, team, agent | `cmd.mission.{tsk}.create` | A2 | |
| `task.abort` | task | `cmd.mission.{tsk}.abort` (+ `cmd.fleet.{class}.{id}.hold` for asset tasks) | A2; A3 for asset tasks | Doctrine verb `abort`; no new `cancel` verb (mission-task.md §6.2) |
| `asset.goto`, `asset.hold`, `asset.rtl`, `asset.land`, `asset.orbit`, `asset.observe` | own asset (`sim-*`) | `cmd.fleet.{class}.{id}.{verb}` | A3 | asset-command.md §3 |
| `asset.estop` | own asset | `cmd.fleet.{class}.{id}.estop` | A3 | Stopping action: one confirm (see asset-command.md §3 note) |
| `asset.arm`, `asset.takeoff`, `asset.estop_release`, `asset.geofence_override` | own asset | `cmd.fleet.{class}.{id}.{verb}` | A4 | Two people (H6) |
| `alert.acknowledge` / `alert.resolve` | alert | `cmd.entity.{id}.acknowledge` / `.resolve` | A1 | |
| `watch.create` / `watch.delete` | watch rule (KV) | `cmd.watch.{id}.{create\|delete}` | A2 | Deterministic watcher ([agents-that-act.md](../research/agents-that-act.md) §4) |
| `feed.enable` / `feed.disable` | feed | `cmd.feed.{source}.{enable\|disable}` | A2 | |
| `agent.delegate` | agent | onemind-core `delegate-agent` capability | A2 | Existing at-most-once dispatch ([agents-that-act.md](../research/agents-that-act.md) fact 2) |

**Actions that do not exist and that policy rejects by name:** strike, engage, designate
target, any CoT `t-k-*` tasking, any task whose target is a person or a person-class track,
face or identity recognition, payload release of anything but supplies, any `hostile`
target list ([lattice-patterns.md](../research/lattice-patterns.md) R2 and §E catalogue;
[palantir-patterns.md](../research/palantir-patterns.md) Recommendation 6).

### 6.3 Functions (reads)

All reads are NATS request/reply on `svc.{service}.{endpoint}` (three tokens, as in omos
`CLAUDE.md`). Hermes tools map one-to-one onto these
([agents-that-act.md](../research/agents-that-act.md) §1).

| Function | Subject | Returns |
|---|---|---|
| Get one entity | `svc.entity.get` | entity record plus links |
| Search | `svc.entity.search` | ids, names, positions, `t`, sources |
| Query (fields, bbox, types, as-of) | `svc.entity.query` | page of entities |
| History | `svc.entity.history` | downsampled track and changes from source-stream pointers |
| Explain fusion | `svc.fusion.explain` | sources, confidence parts, conflicts |
| Layers | `svc.layer.list`, `svc.layer.summary` | counts and freshness (layer contracts) |
| Missions and tasks | `svc.mission.get`, `svc.mission.list` | records from KV `mission-state` |
| Apply an approved record edit (internal, policy service only) | `svc.mission.apply` | updated record (mission-task.md §6.3) |
| Propose, confirm, look up actions | `svc.policy.command.propose`, `svc.policy.task.propose`, `svc.policy.command.confirm`, `svc.policy.task.confirm`, `svc.policy.action.get` ([BUS-DESIGN.md](BUS-DESIGN.md) §3.4) | asset-command.md §1, §4; mission-task.md §6 |

## 7. Permissions

### 7.1 Who may do what

| Principal | NATS publish (target) | Actions | Source |
|---|---|---|---|
| Viewer (person) | none on `cmd.>`; `svc.*` reads | A0 | [nats-platform.md](../research/nats-platform.md) §4 `CLIENT` account |
| Operator (person) | `svc.policy.*` requests | propose and confirm A1–A3 on assets they `operate` | [security-and-trust.md](../research/security-and-trust.md) §4 |
| Second approver (person) | `svc.policy.*.confirm` | second confirm on A4 | [agents-that-act.md](../research/agents-that-act.md) §8 |
| Owner (Zeus) | as operator | grants roles; A4 | decision 0004 |
| Agent (Hermes and others) | `svc.*` imports and `cmd.map.>` only; denied all other `cmd.*` | propose any level; **never confirm**; A1 map actions direct | [agents-that-act.md](../research/agents-that-act.md) §6; [security-and-trust.md](../research/security-and-trust.md) §3 |
| Policy service | the only publisher of `cmd.>` (except `cmd.map.>` from agents and clients) | executes approved actions | [security-and-trust.md](../research/security-and-trust.md) Recommendation 1 |
| Entity service | `ent.>`, `evt.entity.>` | applies `cmd.entity.*` | [lattice-patterns.md](../research/lattice-patterns.md) §A |
| Task service | `evt.mission.>`, KV `mission-state` | applies `cmd.mission.*`, dispatches tasks through policy | [lattice-patterns.md](../research/lattice-patterns.md) §E |
| Asset bridge (MAVLink, ROS) | `tel.fleet.{class}.{own ids}.*` and `evt.{class}.{own id}.command` / `.task` (outcomes and task status; the policy and task services republish them as `evt.so.action.*` and `evt.mission.*`); subscribes only `cmd.fleet.{class}.{own ids}.>` | executes commands after verifying them | [security-and-trust.md](../research/security-and-trust.md) §1; [BUS-DESIGN.md](BUS-DESIGN.md) bridge rows |
| Collectors | `intel.>`, `tel.>`, `det.>` only | none | [nats-platform.md](../research/nats-platform.md) §4 |

**Today this is not enforced**: all application traffic runs in one account `ONEMIND` on shared
passwords ([nats-platform.md](../research/nats-platform.md) §1 "Live omos bus today"). The table is
the target for the omos per-service-user PR. Local tests use a local `nats-server` configured with
these users (decision 0004 #11).

### 7.2 Policy model (OpenFGA, proposed extension)

OpenFGA v1.22.0 runs in omos (`omos/platform/security/ecosystem-openfga.yaml`) with a model
that has only `user`, `group` and `capability#runner`
([agents-that-act.md](../research/agents-that-act.md) facts 10–11). Proposed types and relations:

| Type | Relations | Conditions (CEL, evaluated with caller-supplied context) |
|---|---|---|
| `asset` | `operator`, `viewer`, parent `site` | `is_simulated` (id starts `sim-`), `within_window` |
| `action_type` | `proposer`, `approver` (users only), `second_approver`, `executor` (services only) | — |
| `mission` | `owner`, `member`, `viewer` | — |
| `layer` | `viewer` | `marking_allows` (requester groups vs `releasable_to`) |
| `team` | `member`, `leader` | — |

Rule shape: `can_execute = operator and confirmed_by_human`; `can_propose = proposer`; agents
are never `operator` or `approver`. Geofence checks run in the policy service before Check,
because CEL has no geometry ([security-and-trust.md](../research/security-and-trust.md) §4). The
policy service fails closed if OpenFGA is unreachable (same source).

### 7.3 Markings

Every record carries `marking.tlp` (TLP 2.0: `CLEAR|GREEN|AMBER|AMBER+STRICT|RED`) and
`marking.releasable_to[]` (OneMind groups), in the payload and in headers
`OneMind-Marking` / `OneMind-Releasable-To`, never in the subject. Defaults: homestead data
AMBER+STRICT, open feeds CLEAR. Outgoing CoT omits `access` unless a federation config names a
marking ([security-and-trust.md](../research/security-and-trust.md) §5).

## 8. The loop in ontology terms

| JADC2 step | Ontology activity | Subjects |
|---|---|---|
| Sense | Bridges and collectors publish observations | `tel.fleet.*`, `det.*`, `intel.*`, `tak.cot.out` |
| Make Sense | Entity service fuses observations into entities, tracks, events; watch jobs raise alerts | `ent.*.*.state`, `evt.map.geofence.*`, `evt.entity.*` (P) |
| Decide | A person, a watch job or Hermes proposes an action | `svc.policy.command.propose` or `svc.policy.task.propose` |
| Act | Policy checks, a human confirms, the executor runs it | `evt.so.action.*`, `cmd.*`, `evt.mission.*` |
| Assess | Task status and entity changes close the loop | `evt.mission.{tsk}.*`, `ent.*` |

Source: [lattice-patterns.md](../research/lattice-patterns.md) §F;
[agents-that-act.md](../research/agents-that-act.md) §4.

## 9. Owner proposals and conflicts (decision 0004 #8)

None of these changes a subject today. IDs carry the prefix `ONT-` so they never collide with
BUS-DESIGN's `BUS-P*`, REFERENCE-ARCHITECTURE's `RA*` or cot-projection's `CP-*` items. Every entry
is filed through the combined register in [BUS-DESIGN.md](BUS-DESIGN.md) §10.1 (FEATURES A24).

| # | Item | Proposal | Evidence |
|---|---|---|---|
| ONT-P1 | No stream binds `ent.>` | `ENT` stream CR `omos/platform/nats/streams/ent.yaml` on `ent.*.*.state`, `maxMsgsPerSubject: 1`, `allowMsgTtl`, delete markers | [nats-platform.md](../research/nats-platform.md) Recommendation §2 |
| ONT-P2 | Entity lifecycle, link and alert facts have no stored domain | Add `evt.entity.>` to `omos/platform/nats/streams/evt.yaml` | [lattice-patterns.md](../research/lattice-patterns.md) §D, R3 |
| ONT-P3 | No stream binds `det.>` | `DET` stream CR | [drones-robots-agents.md](../research/drones-robots-agents.md) Recommendation 6 |
| ONT-P4 | Two "locked" command shapes: `cmd.{domain}.{entity}.{action}` (omos `CLAUDE.md`) and `cmd.fleet.{class}.{id}.{verb}` (doctrine §9.1) | OneMind uses doctrine §9.1 for asset commands (as FEATURES D3 and the drones and Lattice lanes do); amend the `CLAUDE.md` example. The security lane preferred `cmd.drone.{id}.{verb}`; this design does not follow it because §9.1 is the documented fleet form and `CMD` stores both | [drones-robots-agents.md](../research/drones-robots-agents.md) Risks; [security-and-trust.md](../research/security-and-trust.md) R10; [lattice-patterns.md](../research/lattice-patterns.md) R5 |
| ONT-P5 | Two telemetry shapes: `tel.{node}.{class}.{metric}` vs `tel.fleet.{class}.{id}.*` | Use §9.1 for fleet assets (captured by `TEL` today) | [nats-platform.md](../research/nats-platform.md) Recommendation §5 item 1 |
| ONT-P6 | `tel.feed.{source}.status` is published but not stored | Add to a stream or change the subject, ingestion lane's choice | `omos/platform/nats/streams/tel.yaml`; [ingestion-platform.md](../research/ingestion-platform.md) line 138 |
| ONT-P7 | `CMD` keeps 720 h; audit needs longer | 365 d on `CMD` | [nats-platform.md](../research/nats-platform.md) Recommendation §2 |
| ONT-P8 | New `entity_type` tokens (`person`, `team`, `track`, `place`, `structure`, `zone`, `event`, `alert`, `agent`) | No new pattern (the token is a variable in `ent.{entity_type}.…`); ratify the vocabulary | omos `CLAUDE.md` grammar |
| ONT-P9 | New `cmd` domains `entity`, `team`, `watch`, `feed` | Inside `cmd.{domain}.{entity}.{action}`; ratify | omos `CLAUDE.md` grammar |
| ONT-P10 | The research lanes proposed three different audit subjects (`evt.command.*`, `evt.policy.*`, `evt.so.action.*`) | This design uses `evt.so.action.{event}`, the only one already stored (`EVT` binds `evt.so.>`). `evt.command.*` named in the omos command-envelope description is three tokens and unstored | [agents-that-act.md](../research/agents-that-act.md) §7; `omos/services/schemas/command/envelope.schema.json` description |
| ONT-P11 | "Task" collides with om-task (`task.*`, `evt.task.*`) | Asset tasks live in the `mission` domain (`evt.mission.tsk-…`); never publish `evt.task.*` for them | [lattice-patterns.md](../research/lattice-patterns.md) R4 |
| ONT-P12 | Robot and vehicle CoT types differ between the projector and the drones lane | Settled in `robotics-cot-profile.md` | `omos/platform/omos/tak-projector.yaml` lines 74–78; [drones-robots-agents.md](../research/drones-robots-agents.md) |
| ONT-P13 | Bridge outcome and task-status facts `evt.{class}.{id}.command\|task` are stored for `drone` and `robot` (`EVT` binds `evt.drone.>`, `evt.robot.>`) but not for `vehicle` | Add `evt.vehicle.>` to `EVT` before a commandable vehicle exists | `omos/platform/nats/streams/evt.yaml` |
| ONT-P14 | Policy request subjects: an earlier draft used three-token `svc.policy.propose\|confirm\|get`; [BUS-DESIGN.md](BUS-DESIGN.md) §3.4 lists four-token names | **Settled in the set (RA2)**: `svc.policy.command.propose\|confirm` for asset commands and other non-task actions, `svc.policy.task.propose\|confirm` for tasks, `svc.policy.action.get`, `svc.policy.check` (§8). Both forms fit `svc.{service}.{endpoint}` (doctrine §2 example `svc.meta.task.list`); filed so the owner can confirm the naming | omos `CLAUDE.md`; `omos/docs/NATS-DOCTRINE.md` §2; [REFERENCE-ARCHITECTURE.md](REFERENCE-ARCHITECTURE.md) §13 RA2 |

## 10. Unverified items

- Gotham's plan/execute/report picture (Gaia, Graph, Dossier) descriptions
  ([palantir-patterns.md](../research/palantir-patterns.md) "Unverified items").
- ATAK and iTAK behaviour for CoT `t-*` tasking and `y-*` replies
  ([lattice-patterns.md](../research/lattice-patterns.md) R8).
- CoT types for structure, alert and agent (choices above are placeholders for cot-projection).
- A dedicated UGV CoT type ([drones-robots-agents.md](../research/drones-robots-agents.md)).
- Whether reusing any Lattice API field name is licence-safe. This design renames them
  (`is_live` → `live`, `no_expiry` → `pinned`, `task_catalog` → `capabilities`,
  `initial_entities` → `target_snapshots`, `parent_task_id` → `parent_id`,
  `alternate_ids` → `aliases.ids`) to avoid the question
  ([lattice-patterns.md](../research/lattice-patterns.md) R1).
- Whether the `om-overwatch-*` services run live; their `ent.air.*` use and KV
  `OVERWATCH_STATE` are not adopted here ([lattice-patterns.md](../research/lattice-patterns.md) N1–N5).
- NACK support for `allowMsgTtl` and delete-marker fields on the `ENT` CR
  ([fusion-and-tracking.md](../research/fusion-and-tracking.md) owner proposal 3).
