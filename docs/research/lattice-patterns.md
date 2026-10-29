# Research lane: Anduril Lattice patterns (Phase 1 / M0, lane 13)

What Lattice's public developer material shows about entities, tasks, the autonomy loop,
fusion and mesh, and the OneMind entity and task model that comes out of it. That model
maps onto `ent.*`, `cmd.*`, `evt.*` and CoT.

**Scope and licence.** This document copies designs only. The Lattice SDK licence (L13)
lets you use the SDK "solely to develop applications for compatible implementations of
Lattice". It forbids copying, modifying or creating derivative works of the SDK, and
using it "to develop another SDK". So OneMind takes no Lattice protobufs, generated code,
field numbers or sample code, and does not use the names "Lattice" or "Anduril" in its
product. OneMind's names below are its own: snake_case JSON on NATS and its own
vocabularies. Where a OneMind enum resembles a Lattice one, it is because both follow
MIL-STD-2525 or CoT conventions, which predate both.

Researched 2026-10-29. All `developer.anduril.com` pages were fetched as Markdown
(`<page>.md`, as `https://developer.anduril.com/llms.txt` advertises) into the scratchpad
folder `/private/tmp/claude-501/-Users-danielcarku-Desktop-ONEMIND/28794bd7-008f-413a-92c6-5afd0f9c2e20/scratchpad/lattice/`.

## Sources

| # | Source | What was read |
|---|---|---|
| L1 | https://developer.anduril.com/llms.txt | Index of the developer docs (Entities, Tasks, Objects, Video, best practices, API reference) |
| L2 | https://developer.anduril.com/guides/concepts/principles.md | Local-first and open data exchange principles; the four data models |
| L3 | https://developer.anduril.com/guides/entities/overview.md | Entity definition, required components, lifecycle, `noExpiry`, templates and shapes |
| L4 | https://developer.anduril.com/reference/rest/entities/publish-entity.md | The full Entity component list and every sub-type (Location, Correlation, Provenance, Overrides, Relationships, Health and so on) |
| L5 | https://developer.anduril.com/reference/rest/entities/override-entity.md | Override semantics |
| L6 | https://developer.anduril.com/reference/rest/entities/stream-entities.md, https://developer.anduril.com/guides/entities/watch.md | Entity event types, heartbeat, `preExistingOnly`, `componentsToInclude` |
| L7 | https://developer.anduril.com/guides/tasks/overview.md | Task model, task catalog, task status fields, lifecycle |
| L8 | https://developer.anduril.com/guides/tasks/define-a-task.md | Task definitions as typed schemas, packages, schema registry, breaking-change rules |
| L9 | https://developer.anduril.com/guides/tasks/integrate-an-agent.md | Agent workflow, status sequence, heartbeats, rejection and cancellation |
| L10 | https://developer.anduril.com/reference/rest/tasks/create-task.md, https://developer.anduril.com/reference/rest/tasks/listen-as-agent.md, https://developer.anduril.com/guides/tasks/command-and-operate.md | Task fields, `TaskStatus` enum, `TaskError`, Relations, Principal, delivery and execution constraints, retry, replication, Execute/Cancel/Complete requests |
| L11 | https://developer.anduril.com/guides/objects/overview.md | Objects model, tiered CDN across the mesh, 1 GiB limit, expiry, SHA-256 |
| L12 | https://developer.anduril.com/guides/best-practices/configure-certificates.md, https://developer.anduril.com/reference/overview/versioning/migrating-to-v2.md | Offline and air-gapped environments (private CA); REST and gRPC parity |
| L13 | https://developer.anduril.com/license.md | Lattice SDK licence terms |
| L14 | `gh api repos/anduril/sample-app-auto-reconnaissance/readme` (and `gh search repos --owner anduril`) | A worked autonomy loop: watch entities, then a rule, an override and a task |
| L15 | https://patents.google.com/patent/US10506436B1/en, "Lattice mesh", assignee Anduril Industries Inc, priority 2018-06-11 | Mesh design: pub/sub topics, live-first prioritisation, caching, gossip, key lifetimes |
| L16 | Web search results (not primary): defensescoop.com 2024-12-03 (CDAO $100M Edge Data Mesh), anduril.com/news "Scaling Next-Generation Command and Control…", maginative.com on Lattice for Mission Autonomy | Vendor and press claims about the mesh and Mission Autonomy. Primary anduril.com pages render with JavaScript only (curl and WebFetch got a title only), and blog.anduril.com returned HTTP 403 |
| C1 | `atak-civ/takcot/mitre/types.txt` (`gh api repos/TAK-Product-Center/atak-civ/contents/takcot/mitre/types.txt`) | The CoT type, how and link-relation trees, the tasking `t-*` tree and the reply `y-*` tree |
| C2 | `atak-civ/takcot/mitre/CoT Base-Event Schema (PUBLIC RELEASE).xsd` | `stale`, `how`, `qos`, `opex`, `ce`, `le` semantics |
| C3 | `atak-civ/takcot/examples/*.cot` | TAK shape and route types: `u-d-c-c`, `u-d-f`, `u-d-r`, `b-m-r`, `u-r-b-bullseye`, `b-m-p-s-m`; route links `b-m-p-w`, `b-m-p-c` with `relation='c'` |
| N1 | `omos/CLAUDE.md`, `omos/docs/NATS-DOCTRINE.md` §2, §9.1–§9.8 | Locked grammar, geo-envelope, fleet and mission subjects, TAK subject decision |
| N2 | `omos/platform/nats/streams/*.yaml` | Stream and KV bindings: `CMD` binds `cmd.>`; `EVT` binds `evt.{so,hp,le,ge,task,cloud,bio,robot,drone,sat}.>` (*verifier correction*: the original listed only so/hp/le/ge; see `omos/platform/nats/streams/evt.yaml`); `MISSION` binds `evt.mission.>`, `evt.map.>`; `GEO` binds `evt.tak.cot.>`, `evt.tak.status.>`; `WORLD` binds `evt.world.>`; KV `mission-state` (history 5), `task-index` (TTL 720h), `geofences` (history 10), `world-state` (TTL 5m), `fleet-state` (TTL 30s). **No stream binds `ent.>`** (`grep -rn "ent\.>"` returned nothing) |
| N3 | `omos/services/schemas/command/envelope.schema.json`, `envelope/envelope.schema.json`, `missions/mission-state.schema.json`, `registry/entity.schema.json` | The existing command envelope (`approval`, `corr`, `causation`, `dry_run`), event envelope, mission states (`requested, running, completed, failed, cancelled`) |
| N4 | `omos/services/nex/overwatch/om-task/main.go`, `task_write.go` | The existing om-task engine: subjects `task.*`, events `evt.task.created`, `evt.task.updated`, `evt.task.{id}.approved/rejected`, `evt.task.deleted`, KV `task-index` |
| N5 | https://docs.nats.io/nats-concepts/subjects | Token character rules |
| X1 | `onemind-xr/docs/research/nats-platform.md`, `drones-robots-agents.md`, `tak-protocols.md`, `onemind-xr/docs/FEATURES.md`, `ARCHITECTURE.md` | Sibling lanes: the proposed `ENT` stream (per-message TTL), the `CLIENT` account that cannot publish `cmd.>`, the `cmd.fleet.*` asset command form, the CoT detail elements |

## Verified facts

### Entities (L2–L6)

- An entity "models anything in the world that is of significance to an operator" (L2).
  It is a **bag of components**, not a type hierarchy. Partial entities are expected,
  and the same data must not be modelled on two components (L3).
- Required: `entity_id` (GUID), `is_live`, `expiry_time`, `provenance.integration_name`.
  `aliases.name` is optional (L3).
- **Lifecycle.** Lattice emits `CREATE` on first publish and `UPDATE` only when
  `provenance.source_update_time` changes; it does not diff the whole message (L3). An
  update is accepted only if its `sourceUpdateTime` is **greater** than the stored one
  (L4). A delete happens when `expiry_time` passes or when a publish sets `is_live=false`
  (L3). Stream event types: `EVENT_TYPE_CREATED, _UPDATE, _DELETED, _PREEXISTING,
  _POST_EXPIRY_OVERRIDE` (L6). Streams carry heartbeats (default 30 s), and
  `preExistingOnly` sends a snapshot and then closes. `componentsToInclude` filters which
  components are sent (L6).
- **Expiry.** `expiry_time` must be in the future and less than 30 days away. Constantly
  updating entities move it forward with each update (L3, L4). The publish example in L9
  uses `latest + 1 minute`. `noExpiry` is for long-lived information such as geographic
  entities, and it overrides `expiryTime`. After a service restart, a `noExpiry` entity
  that was not republished within the previous 5 minutes is **not restored** (L3).
- **Ownership.** Entities published through the API are "owned" by the originator: "other
  sources, such as the UI, may not edit or delete these entities" (L4). Operators change
  them through **overrides**. Overrides are allowed only on fields marked overridable,
  are eventually consistent, and "last writer wins" (L5). Each override records
  `requestId, fieldPath, maskedFieldValue, status (APPLIED|PENDING|TIMEOUT|REJECTED|
  DELETION_PENDING), provenance, type (LIVE|POST_EXPIRY), requestTimestamp` (L4).
- **Templates** set the minimum required components: `TEMPLATE_TRACK`,
  `TEMPLATE_SENSOR_POINT_OF_INTEREST`, `TEMPLATE_ASSET`, `TEMPLATE_GEO`,
  `TEMPLATE_SIGNAL_OF_INTEREST` (L4). An asset or a track requires `ontology`, `location`
  and `milView`. A geo-entity requires `geo_details` and `geo_shape` (L3).
- **Component catalogue** (L4, top-level Entity fields): `description, is_live,
  created_time, expiry_time, no_expiry, status{platform_activity, role},
  location{position, velocity_enu, speed_mps, acceleration, attitude_enu},
  location_uncertainty{position_enu_cov, velocity_enu_cov, position_error_ellipse},
  kinematics (geodetic or ECEF; preferred for tracks), geo_shape{point, line, polygon,
  ellipse, ellipsoid}, geo_details{type: GENERAL|HAZARD|EMERGENCY|ENGAGEMENT_ZONE|
  CONTROL_AREA|BULLSEYE|ACM, …}, aliases{alternate_ids[{id,type}], name},
  tracked{track_quality 0–15, sensor_hits, number_of_objects, radar_cross_section,
  last_measurement_time, line_of_bearing}, correlation{primary, secondary, membership,
  decorrelation}, mil_view{disposition, environment, nationality},
  ontology{platform_type, specific_type, template}, sensors[{sensor_id,
  operational_state, sensor_type, fields_of_view, last_detection_timestamp…}], payloads,
  power_state, provenance{integration_name, data_type, source_id, source_update_time,
  source_description}, overrides, indicators{simulated, exercise, emergency, c2,
  egressable, starred}, target_priority, signal, transponder_codes,
  data_classification{default{level, caveats}, fields[]}, task_catalog{task_definitions
  [{task_specification_url}]}, media[{id, type IMAGE|VIDEO, relative_path}],
  relationships[{related_entity_id, relationship_id, relationship_type: tracked_by |
  group_child | group_parent | merged_from | active_target}], visual_details{range_rings},
  dimensions{length_m}, route_details{destination_name, estimated_arrival_time},
  schedules, health{connection_status, health_status, components[], update_time,
  active_alerts[{alert_code, description, level ADVISORY|CAUTION|WARNING,
  activated_time, active_conditions}]}, group_details{team, platform_subcomponents,
  echelon}, supplies{munitions, fuel}, orbit, symbology{mil_std_2525c}`.
- **Position** is WGS84 with four altitude references: HAE, AGL, above sea floor and
  pressure depth. MSL (EGM-96/08) is **not** supported in `Position` (L4). *Verifier
  nuance:* the separate `Altitude` message used by kinematics does carry `egm96Amsl` and
  `pressureAmsl` (L4 `### Altitude`), so "MSL unsupported" applies to `location.position`
  only.
- **Correlation** (the visible part of fusion): an N-to-1 set. The primary represents the
  set and secondaries point to it. Membership has a `correlation_set_id`. Metadata records
  `provenance`, `type MANUAL|AUTOMATED` and `replication_mode LOCAL|GLOBAL`. An explicit
  **decorrelation** "prevents lower precedence correlations from overriding it in the
  future", so an operator's "these are not the same" beats the automatic correlator (L4).

### Tasks and the autonomy loop (L7–L10, L14)

- A task models "deliberate, sequential actions that an operator can execute on an asset
  or team of assets". A **taskable agent** is "a service, asset, group, or human that can
  accept and perform tasks" (L7).
- Fields (L10): `task_id` (matches `[A-Za-z0-9_-.]`), `description`, `specification`
  (a typed `Any`: type URL plus data), `author` / `created_by` / `last_updated_by`
  (a **Principal**: `system | user | team`, with optional `on_behalf_of`),
  `relations{assignee: Principal, parent_task_id}`, `initial_entities[{entity,
  snapshot}]` (frozen copies, "not updated during execution"), `retry_strategy`
  (fixed, default 5 s, only when `deliver_before` is set),
  `delivery_constraints{deliver_after, deliver_before, require_acknowledgement}`,
  `execution_constraints{start_after, complete_before}`, `is_executed_elsewhere`,
  `replication{stale_time}`, `scheduled_time`, and
  `version{task_id, definition_version, status_version}` for optimistic concurrency.
- `TaskStatus` (L10): `status, task_error{code CANCELLED|REJECTED|TIMEOUT|FAILED,
  message, error_details}, progress, result, estimate` (each a typed schema),
  `start_time, end_time, last_update_time` ("used to guard against out of order
  updates") and `allocation{active_agents[]}`.
- Status enum (L10): `CREATED, SCHEDULED_IN_MANAGER, SENT, MACHINE_RECEIPT, ACK, WILCO,
  EXECUTING, WAITING_FOR_UPDATE, DONE_OK, DONE_NOT_OK, REPLACED, CANCEL_REQUESTED,
  COMPLETE_REQUESTED, VERSION_REJECTED`. The agent's normal sequence is SENT →
  MACHINE_RECEIPT → ACK → WILCO → EXECUTING → DONE_OK | DONE_NOT_OK, incrementing
  `status_version` at each step. **DONE_OK and DONE_NOT_OK are terminal and immutable**
  (L7, L9).
- **The agent decides.** It may reject with DONE_NOT_OK + `ERROR_CODE_REJECTED`. On a
  cancel it either accepts (DONE_NOT_OK + `CANCELLED`) or refuses (keeps its status and
  attaches `REJECTED`). A task not yet sent cancels at once (L9, L10).
- **Capability gate.** An asset advertises tasks in its entity's `task_catalog`. "An
  operator can only assign a task to an asset if the asset's `taskCatalog` advertises
  that task's type URL" (L8).
- **Task definitions** are typed, versioned schemas (`org.repo.package.version`, with
  stability markers `alpha|beta|test`). A registry enforces breaking-change detection:
  adding fields is allowed; removing or renaming fields or changing types is not (L8).
  Common shapes are a parameterless toggle, an entity-targeted task (`entity_id`) and a
  `oneof {entity_id, lla}` objective (L8).
- **Delivery.** Agents hold a long-lived stream (REST long-poll up to 5 min, or gRPC),
  selected by their entity IDs, that delivers `ExecuteRequest`, `CancelRequest` and
  `CompleteRequest`. Stream delivery "does not wait for the agent to acknowledge
  receipt", so heartbeats (default 30 s, minimum 1 s) detect drops (L9, L10).
- **Autonomy loop, worked example** (L14): (1) a track publisher; (2) a simulated asset
  that advertises an `Orbit` task; (3) an "auto-reconnaissance" service that streams all
  entities. When a non-friendly track comes within 5 miles of an asset, it **overrides
  the track's disposition to suspicious and creates an `Orbit` task** for the asset.
  The asset receives the ExecuteRequest. So the loop is entity stream → rule → override
  plus task → agent → status, all through the same two APIs.
- **Mission Autonomy** (L16, press and vendor, not primary): the operator gives intent,
  software "breaks down operator intent into discrete tasks, distributing them across
  unmanned systems … under human supervision".

### Fusion (L4, L16)

- What is public is the **result schema**, not the algorithm: correlation sets with
  manual-over-automated precedence, `merged_from` relationships, `tracked` (quality 0–15,
  sensor hits, last measurement, line of bearing for tracks without a 3D fix), full ENU
  covariance or an error ellipse with probability, and `tracked_by` / `active_target`
  relationships between sensor and track (L4). Press says each product runs fusion and
  "multi-track reconciliation at the edge" (L16, unverified).

### Mesh and objects (L2, L11, L15)

- Principles: "local-first … build with the assumption that the network … can be
  intermittently unavailable", plus standard data models for "inter-operability without
  requiring high levels of prior coordination" (L2).
- Patent US10506436B1 (L15, Anduril): topics are `(topicID, assetID)` tuples with
  point-to-point and multi-destination publication. "The network prioritizes real time
  data despite variable performance of network links", and "Live data is the priority".
  The network "strategically caches data … so that when channels are available data can
  be sent" (backfill DB). Hosts discover mesh nodes and join through a registration hub.
  A **gossip** service distributes a signed, monotonically versioned asset DB.
  Reachability assertions carry an **expiration time**. Group keys live "on the order of
  10s of minutes". JWTs are refreshed from the hub.
- Objects (L11): files under 1 GiB, with `expiry_time` and a SHA-256 checksum. "Every node
  in the mesh runs a single instance of the CDN". A lookup tries the local cache, then
  peers at the same layer, then the next layer up.
- Offline environments use a private CA, with full TLS verification as the recommended
  approach (L12).

### CoT counterparts (C1–C3)

- `how` tree: `h-e` estimated, `h-c` calculated, `h-t` transcribed, `m-g` GPS, `m-i`
  mensurated, `m-s` **simulated**, `m-p` passed through a gateway "w/o pedigree", `m-f`
  **fused**, "corroborated from multiple sources". The attribute exists "to weight the
  data in systems that fuse multiple inputs" (C1).
- Tasking `t-*` (requests and orders): `t-s-*` surveillance (`t-s-i` imagery, `t-s-v`
  video, `t-s-r` radar), `t-m` mensurate, `t-r` relocate, `t-x-v-m` medical evacuation,
  `t-o-{p,o,f,w}` OPLAN/OPORD/FRAGO/WARNO, `t-z` abort, `t-u-z` cancel a request,
  `t-u-q` query status, `t-q` ask whether a tasking can be honoured, and `t-k-*` strike
  (C1). Most entries are marked P (proposed) or Exp (experimental).
- Reply `y-*`: `y-a-r` machine ack, `y-a-w` wilco (`y-a-w-d` best effort), `y-s-e`
  executing, `y-s-p` planning, `y-s-r` in review, `y-s-c` cancelled (acknowledges
  `t-u-z`), `y-c-s` completed successfully, `y-c-f` failed: `-a` no asset, `-i`
  insufficient info, `-b` rejected by machine (`-b-v` violates exclusion-zone rules,
  `-b-a` authorization failed, `-b-p` physically impossible), `-s` went stale, `-r`
  rejected by a human (C1).
- Link relations: `p-p` producer, `p-o` owner, `p-l` leader, `p-s` source ("derived from
  parent"), `p-t` "references the tasking that elicited this event", `c-c` correlated
  element, `c-f` fused element, `c-a` alternate, `c-p` composite, `t-o` object of
  tasking, `t-s` subject of tasking (C1).
- `stale` ends the event's validity interval. `opex` marks events `o` (operations), `e`
  (exercise) or `s` (simulation). `qos` carries priority, overtaking and assurance. `ce`
  and `le` (metres) define a cylinder around the point (C2).
- TAK shapes: circle and geofence `u-d-c-c`, free form `u-d-f`, rectangle `u-d-r`, route
  `b-m-r` with `b-m-p-w` / `b-m-p-c` child links, bullseye `u-r-b-bullseye`, spot marker
  `b-m-p-s-m` (C3).

### OneMind today (N1–N5)

- The locked grammar includes `ent.{entity_type}.{entity_id}.state`,
  `cmd.{domain}.{entity}.{action}` and `evt.{domain}.{entity}.{event}` (N1). Doctrine
  §9.1 also locks `cmd.fleet.{class}.{id}.{verb}` and `cmd.mission.{id}.{verb}` (five
  and four tokens). The drones lane and FEATURES D3 use `cmd.fleet.*` (X1).
- **No stream binds `ent.>`** (N2). *Verifier correction:* the original also said "nothing
  publishes `ent.*.*.state`", which is wrong for the omos source. `om-overwatch-state`
  publishes `ent.air.{id}.state` after each fusion and `ent.air.{id}.deleted`
  (`omos/services/nex/overwatch/om-overwatch-state/main.go:177,491`, ontology in
  `ontology.go`, KV `OVERWATCH_STATE`). `om-overwatch-tak` and `om-overwatch-interceptor`
  subscribe to `ent.air.*.state`. `om-overwatch-api` publishes the off-grammar
  `ent.manual.upsert|delete` (`om-overwatch-api/main.go:153,185`). Note that the existing
  type token is `air`, not `aircraft` as section A proposes. Whether these services run
  live is **unverified**: `grep -rl overwatch omos/platform` finds only KV CRs, a Backstage
  catalog entry and Hermes doctrine, with no Argo Application. The cluster was not queried. The NATS
  lane proposes an `ENT` stream with `maxMsgsPerSubject: 1`, per-message TTL and delete
  markers (X1 `nats-platform.md` §2).
- `CMD` (`cmd.>`) and `MISSION` (`evt.mission.>`) exist. `EVT` binds
  `evt.{so,hp,le,ge,task,cloud,bio,robot,drone,sat}.>`, so `evt.entity.>` is **not stored**
  by any stream (N2). *Verifier correction:* the original said `EVT` binds only so/hp/le/ge
  and that `evt.task.>` is not stored. `evt.task.>` **is** stored by `EVT`
  (`omos/platform/nats/streams/evt.yaml`, maxAge 2160h).
- om-task (N4) is a personal and project task engine (calendar, inbox, projects). It
  uses off-grammar `task.*` request subjects and publishes `evt.task.created` (three
  tokens, off-grammar) and `evt.task.{id}.approved|rejected`. "Task" is therefore
  already taken on the bus for a different concept.
- The command envelope already carries `approval{required, approver, status, reason}`,
  `corr`, `causation`, `dry_run` and `trace` (N3).
- Subject tokens cannot contain `.`, `*`, `>` or whitespace. Use `[A-Za-z0-9_-]` (N5).

## How it applies to OneMind

The Lattice design reduces to five ideas that OneMind adopts:

1. One composable entity record that every producer and consumer shares.
2. Explicit provenance and monotonic source time.
3. Expiry as a first-class field.
4. Typed, capability-gated tasks with a fine-grained acknowledgement state machine.
5. A local-first mesh that gives live data priority and caches the rest.

OneMind goes further in three places where Lattice is thin or closed: **per-source
provenance** (Lattice has one primary provenance), **persistent state across restarts**
(Lattice drops un-republished `noExpiry` entities after 5 minutes; JetStream R3 keeps
them), and **policy plus human confirm on every asset command** (run rule 6).

### A. The OneMind entity (`ent.{entity_type}.{entity_id}.state`)

**Subject and identity**

- `entity_type` is the subject's second token. Values: `aircraft | vessel | drone |
  robot | vehicle | person | sensor | camera | node | agent | track | signal | place |
  structure | zone | hazard`. The first eight match the fleet class vocabulary in
  doctrine §9.1. `track` holds detections of unknown class; once classified, the entity
  stays on `track` with `ontology.platform_type` set, so its subject never changes. The
  **template** (`asset | track | geo | spi | soi`) is a field, not a token, as in
  Lattice, where templates are minimum-component presets, not types (L3).
- `entity_id` must be stable, globally unique and subject-safe (`[A-Za-z0-9_-]{1,64}`),
  in the form `{source-prefix}-{native-id}`: `icao-a1b2c3`, `mmsi-367000001`, `sim-x1`,
  `tak-…`, `trk-{ulid}` for fused tracks. If a native id contains other characters
  (CoT uids may contain `.`), use `{prefix}-{base32(sha256(native))[0:26]}` and keep the
  original in `aliases.alternate_ids`. No dot in an id may ever reach a subject (N5).

**Payload.** JSON. The top level stays compatible with the doctrine §9.2 geo-envelope, so
TACMAP and existing consumers keep working. Components sit beside it. Hot-path times are
epoch ms (doctrine §9.2).

```jsonc
{
  // §9.2 envelope (required by the doctrine: uid, type, lat, lon, t)
  "uid": "sim-x1", "type": "fleet.drone", "lat": 30.2672, "lon": -97.7431, "hae": 152.4,
  "course": 90, "speed": 12.5, "callsign": "SIM-X1", "stale": 60, "t": 1793275200000,
  "meta": {},

  // lifecycle
  "v": 1, "entity_id": "sim-x1", "is_live": true,
  "created_t": 1793275100000, "expiry_t": 1793275260000, "no_expiry": false,
  "seq": 42,                                  // entity-service revision (monotonic)

  "ontology":   { "template": "asset", "platform_type": "drone", "specific_type": "quad-sim",
                  "cot_type": "a-f-A-M-H-Q", "symbol_2525": null },
  "disposition": "friendly",                  // friendly|assumed_friendly|neutral|unknown|suspicious|pending  (no 'hostile' target lists; see Risks)
  "environment": "air",                       // air|surface|subsurface|land|space
  "aliases":    { "name": "SIM-X1", "alternate_ids": [ { "type": "mavlink_sysid", "id": "1" },
                                                       { "type": "cot_uid", "id": "sim-x1" } ] },
  "location":   { "lat": 30.2672, "lon": -97.7431, "hae_m": 152.4, "agl_m": 40.0,
                  "vel_enu_mps": [12.5, 0, 0], "speed_mps": 12.5, "course_deg": 90,
                  "attitude_q": [1, 0, 0, 0], "measured_t": 1793275200000 },
  "uncertainty": { "pos_enu_cov": [4,0,0,4,0,9], "vel_enu_cov": null,
                   "error_ellipse": { "p": 0.95, "semi_major_m": 5, "semi_minor_m": 3, "orient_deg": 10 },
                   "ce_m": 5.0, "le_m": 3.0 },  // CoT point ce/le, derived from covariance; per C2 an error ce/le is 1-sigma, not the p=0.95 ellipse axis
  "geo":        null,                         // geo template: { "shape": point|line|polygon|ellipse|ellipsoid|corridor, "coords": GeoJSON or params, "kind": general|hazard|emergency|control_area|keep_in|keep_out|aoi|route }
  "status":     { "activity": "orbit", "role": "overwatch", "mode": "AUTO" },
  "health":     { "connection": "online", "overall": "healthy", "battery_pct": 82, "link_rssi": -67,
                  "components": [ { "id": "gps", "health": "healthy" } ],
                  "alerts": [ { "code": "LOW_BATT", "level": "caution", "since_t": 1793275150000 } ] },
  "sensors":    [ { "id": "cam0", "type": "camera", "state": "operational",
                    "fov": { "h_deg": 82, "v_deg": 52, "az_deg": 90, "el_deg": -30, "range_m": 300 } } ],
  "task_catalog": [ "onemind.task.orbit.v1", "onemind.task.goto.v1", "onemind.task.rtl.v1",
                    "onemind.task.survey_area.v1" ],
  "current_task": { "task_id": "tsk-01JB…", "status": "executing" },
  "tracked":    null,                         // track template: { "quality": 0-15, "hits": n, "last_meas_t": t, "lob": { "from": entity_id, "bearing_deg": b, "elev_deg": e } }
  "correlation": null,                        // { "set_id", "role": primary|secondary, "primary_id", "secondary_ids", "method": manual|automated, "scope": local|global, "decorrelated_from": [...] }
  "relationships": [ { "id": "rel-1", "type": "member_of", "to": "team-cyan" },
                     { "id": "rel-2", "type": "tasked_by", "to": "tsk-01JB…" } ],
  "media":      [ { "kind": "video", "url": "rtsp://127.0.0.1:8554/sim-x1", "role": "primary" },
                  { "kind": "image", "object": "imagery/sim-x1/latest.jpg", "sha256": "…" } ],
  "route":      { "destination": "Barn", "eta_t": 1793275500000, "waypoints_ref": "msn-…" },
  "dimensions": { "length_m": 0.5 },
  "indicators": { "simulated": true, "exercise": false, "emergency": false, "starred": false,
                  "egress_tak": true },        // egress_tak = projector may send to TAK (Lattice 'egressable')
  "classification": { "default": "team", "fields": { "location": "team" },
                      "releasable_to": ["team-cyan"] }, // maps to CoT access/caveat/releasableTo

  // provenance: Lattice has one primary; OneMind keeps every contributor
  "provenance": { "primary": "bridge-mavlink", "data_type": "mavlink",
                  "source_id": "sysid-1", "source_t": 1793275200000, "how": "m-g" },
  "sources": [ { "integration": "bridge-mavlink", "data_type": "mavlink", "source_id": "sysid-1",
                 "source_t": 1793275200000, "subject": "tel.fleet.drone.sim-x1.position",
                 "components": ["location", "health", "status"] },
               { "integration": "tak-mesh-gateway", "data_type": "cot", "source_id": "sim-x1",
                 "source_t": 1793275198000, "subject": "tak.cot.out", "components": ["aliases"] } ],
  "overrides": [ { "id": "ovr-…", "field": "disposition", "value": "friendly", "by": "user:zeus",
                   "status": "applied", "kind": "live", "at_t": 1793275000000 } ]
}
```

**Rules the entity service enforces.** These are Lattice semantics, applied to NATS.

| Rule | Lattice source | OneMind mechanism |
|---|---|---|
| A component update is accepted only if its source time is newer than the stored time **for that source** | L4 (per entity) | Per-source `source_t` in `sources[]`; older contributions are dropped and counted (`tel.{node}.entity.dropped_stale`) |
| An event fires only on a real change | L3 | Publish to `ent.*` only when a component changes or the heartbeat interval passes. `Nats-Msg-Id` = `{entity_id}:{seq}` for dedup |
| Expiry | L3, 30 days maximum | `expiry_t` is required unless `no_expiry`. The service sets the `Nats-TTL` header on `ENT` (X1). Per-type defaults: aircraft 120 s, vessel 30 min, fleet asset 60 s, TAK EUD = CoT `stale`, geo up to 30 d, `no_expiry` for places and structures. Expiry produces a JetStream delete marker, which consumers treat as a delete event |
| Delete | `is_live=false` (L3) | Publish a final record with `is_live:false`, then purge the subject. Also emit `evt.entity.{id}.deleted` (see D) |
| Ownership | Publisher owns; UI uses overrides (L4, L5) | Bridges own their contributions. People and agents change entities only with `cmd.entity.{id}.override` (policy-gated), recorded in `overrides[]`. **Overrides survive source updates** until removed, unlike a raw write |
| Override after expiry | `OVERRIDE_TYPE_POST_EXPIRY` (L4) | `kind: post_expiry` revives an expired entity as `no_expiry` (for example, "keep this vehicle sighting") |
| Correlation precedence | manual > automated; decorrelation blocks lower precedence (L4) | The same rule. `decorrelated_from` is stored in KV `entity-correlation` (proposal) so it survives restarts |
| Restart | `noExpiry` entities lost after 5 min without a republish (L3) | **Better:** `ENT` is R3 file storage; the service rebuilds from the stream on start |
| Snapshot + live | `preExistingOnly`, heartbeats (L6) | An ordered consumer with `DeliverLastPerSubject` on `ent.{type}.>` gives a snapshot and then deltas. Direct Get gives a single entity. `componentsToInclude` becomes the projection service `svc.entity.query` (fields, bbox, types) |

### B. Observations versus entities (Sense → Make Sense)

Lattice integrations publish entities directly. OneMind keeps raw observations on their
own grammar subjects and lets one service own `ent.*`. That one service is FEATURES A4,
and having a single owner is what lets per-source provenance work:

| Input | Subject (existing grammar) | Becomes |
|---|---|---|
| Fleet telemetry | `tel.fleet.{class}.{id}.position|status` | `ent.{class}.{id}.state` (asset) |
| Detections from cameras, ML or radar | `det.{domain}.{type}.{id}` (note: no stream binds `det.>` yet, X1) | `ent.track.{trk-id}.state` (track) or SPI |
| World feeds | `intel.{domain}.{source}.{type}`, `evt.world.*` | `ent.aircraft.icao-…`, `ent.vessel.mmsi-…`, `ent.hazard.…` |
| TAK EUDs, markers, shapes | `tak.cot.out` (decision 0004 #7) | `ent.person.tak-….state`, `ent.place.…`, `ent.zone.…` |
| Geofences and AOIs | KV `geofences` | `ent.zone.{id}.state` (geo template) |

### C. Fusion (the correlation component; algorithm deferred to lane 15)

- Each source track stays an entity (secondary). The fused entity is the primary, which
  matches Lattice's model (L4). Clients show primaries by default and can reveal
  secondaries.
- The fused primary's `provenance.how` = `m-f`, matching the CoT "fused" `how` (C1). Its
  `sources[]` lists every contributor.
- Operator correlate and decorrelate go through `cmd.entity.{id}.correlate|decorrelate`
  with `method: manual`. Manual decisions always win over automated ones (L4).
- `tracked.quality` (0–15), `hits`, covariance and line of bearing are carried exactly
  so the lane-15 tracker (Kalman, IMM, JPDA or MHT) has somewhere to write. The algorithm
  is lane 15's choice; Lattice publishes none.

### D. Entity events and commands

| Purpose | Subject | Stream today |
|---|---|---|
| Latest state | `ent.{type}.{id}.state` | none: needs the `ENT` CR (X1) |
| Lifecycle facts (rare, not every update): `created`, `deleted`, `expired`, `correlated`, `decorrelated`, `overridden`, `classified` | `evt.entity.{entity_id}.{event}` | **none: `EVT` does not bind `evt.entity.>`** (it binds so/hp/le/ge/task/cloud/bio/robot/drone/sat; verifier correction). Owner proposal: add `evt.entity.>` to `EVT` (or to `ENT` with its own maxAge) |
| Alerts raised from entity health or watch jobs | `evt.entity.{entity_id}.alert` | same proposal |
| Override, correlate, decorrelate, delete, star | `cmd.entity.{entity_id}.{override|correlate|decorrelate|delete|star}` | `CMD` (`cmd.>`) |
| Query (fields, bbox, types, as-of time) | `svc.entity.query`, `svc.entity.get`, `svc.entity.history` | request/reply |

All `cmd.*` messages use the existing command envelope (N3). Clients cannot publish
`cmd.>` (X1, CLIENT account): they call `svc.policy.*`, which checks authorization,
writes the audit record, gets human confirmation where required, and then publishes the
`cmd.*`.

### E. The OneMind task model

**Naming.** "Task" on the bus already means om-task's personal tasks (N4). Asset tasks
therefore live under the existing `mission` domain, which `MISSION` already stores, with
id prefixes `msn-` (mission) and `tsk-` (task). A mission is a task with children
(`parent_task_id`). This mirrors Lattice's parent and sub-task relation (L10) and CoT
OPORD/FRAGO (`t-o-o`, `t-o-f`, C1).

**Task record** (JSON, KV `mission-state` key `tsk-…`, history 5; the KV revision is the
CAS guard):

```jsonc
{
  "v": 1, "task_id": "tsk-01JB…", "parent_task_id": "msn-01JA…",
  "type": "onemind.task.orbit.v1",                     // must be in the assignee's task_catalog
  "spec": { "target": { "entity_id": "trk-01J9…" },     // or { "lla": [lat, lon, hae] }
            "radius_m": 60, "alt_hae_m": 180, "duration_s": 600 },
  "description": "Overwatch unknown vehicle at north gate",
  "author":   { "user": "zeus", "on_behalf_of": null }, // or { "agent": "hermes" } / { "system": "watch-gate" }
  "assignee": { "entity_id": "sim-x1" },                // or { "team": "team-cyan" } or { "user": "…" }
  "initial_entities": [ { "entity_id": "trk-01J9…", "snapshot": { /* frozen ent payload */ } } ],
  "delivery":  { "after_t": null, "before_t": 1793275800000, "require_ack": true, "retry_s": 5 },
  "execution": { "start_after_t": null, "complete_before_t": 1793276400000 },
  "stale_t": 1793276400000,                           // Lattice replication.stale_time; CoT stale
  "approval":  { "level": "confirm", "status": "approved", "approver": "user:zeus", "at_t": 1793275205000 },
  "version":   { "definition": 1, "status": 3 },
  "status":    { "state": "wilco", "error": null, "progress": { "pct": 0 },
                 "estimate": { "eta_t": 1793275320000 }, "result": null,
                 "start_t": null, "end_t": null, "updated_t": 1793275210000,
                 "allocation": ["sim-x1"] },
  "created_t": 1793275200000, "created_by": "user:zeus", "updated_by": "sim-x1",
  "simulated": true, "corr": "uuid", "trace": "…"
}
```

**Task types** are JSON Schemas in omos `services/schemas/tasks/{name}.v{n}.schema.json`,
mirrored to the existing KV `om-schema`. They follow the same breaking-change rule as the
Lattice registry (L8): add fields freely; a removal or type change means a new `vN`. The
initial civilian catalogue: `goto, rtl, land, hold, orbit, survey_area, follow_route,
observe (point a camera or PTZ), capture_media, patrol, relay (comms relay), deliver
(payload drop of supplies, not munitions), inspect_structure, return_home`. **No strike,
engage or target-designation type exists or will be accepted** (no `t-k-*`, C1). Policy
rejects any type not in the catalogue.

**Subjects** (all inside the locked grammar):

| Step | Subject | Producer → consumer |
|---|---|---|
| Propose or create (client, Hermes) | `svc.policy.task.create` (request) | client or agent → policy service |
| Accepted task fact | `evt.mission.{task_id}.created` | task service → MISSION |
| Dispatch to the asset | `cmd.fleet.{class}.{asset_id}.{verb}` with `data.task_id`, and the full task in `data` (D3 form, X1) | task service → bridge or agent (durable consumer on `CMD` filtered to its own id, so tasks queue while the asset is offline until `delivery.before_t`) |
| Status reports | `evt.mission.{task_id}.{state}`, carrying `status_version` | agent or bridge → task service, which CAS-writes KV `mission-state` and **rejects a non-increasing `status_version`** (Lattice `VERSION_REJECTED`) |
| Cancel or complete request | `cmd.mission.{task_id}.cancel|complete` (*verifier correction*: doctrine §9.1 lists the verbs `create|waypoint|abort|complete` only, `omos/docs/NATS-DOCTRINE.md:191`. `cancel` is a OneMind addition that needs owner sign-off; otherwise use `abort`) | operator → task service → forwarded as `cmd.fleet.{class}.{asset}.cancel` with `task_id` |
| Mission lifecycle | `cmd.mission.{msn_id}.create|waypoint|abort|complete`, `evt.mission.{msn_id}.*` | as doctrine §9.1 |

**State machine** (Lattice states, OneMind names, CoT on the TAK side):

| OneMind `state` | Lattice (L10) | Set by | CoT projection (C1) | Terminal |
|---|---|---|---|---|
| `proposed` | — (OneMind adds this: agent or rule proposals awaiting a human) | policy | `t-q` query only, if sent at all | no |
| `rejected_by_human` | — | policy | `y-c-f-r` | yes |
| `created` | CREATED | task service | — | no |
| `scheduled` | SCHEDULED_IN_MANAGER | task service | — | no |
| `sent` | SENT | task service | task CoT `t-…` sent to the assignee | no |
| `received` | MACHINE_RECEIPT | agent | `y-a-r` | no |
| `acknowledged` | ACK | agent | `y-a` | no |
| `wilco` | WILCO | agent | `y-a-w` (`y-a-w-d` best effort) | no |
| `executing` | EXECUTING | agent | `y-s-e` | no |
| `waiting` | WAITING_FOR_UPDATE | agent | `y-s-p` | no |
| `cancel_requested` / `complete_requested` | CANCEL_/COMPLETE_REQUESTED | task service | `t-u-z` | no |
| `done_ok` | DONE_OK | agent | `y-c-s` | yes |
| `done_not_ok` + `error.code=rejected` | DONE_NOT_OK + REJECTED | agent | `y-c-f-b` (`-b-v` keep-out zone, `-b-p` impossible) | yes |
| `done_not_ok` + `cancelled` | DONE_NOT_OK + CANCELLED | agent | `y-s-c` | yes |
| `done_not_ok` + `timeout` (delivery or `stale_t`) | DONE_NOT_OK + TIMEOUT | task service | `y-c-f-s` | yes |
| `done_not_ok` + `failed` / `no_asset` | DONE_NOT_OK + FAILED | agent or service | `y-c-f`, `y-c-f-a` | yes |
| `replaced` | REPLACED | task service | `y-c-f` with a `p-t` link to the new task | yes |

These match the omos mission-state enum (N3): `requested`=proposed…sent,
`running`=received…waiting, `completed`=done_ok, `failed`, `cancelled`. Keep that enum as
the coarse roll-up and add `state` as the fine field.

**Approval levels** (OneMind addition; Lattice leaves this to the UI): `none` (map and
annotation tasks), `confirm` (any `cmd.fleet.*`; run rule 6 requires it even in
simulation), and `two_person` (later: real hardware). Hermes and watch jobs can only
**propose**; a human confirms.

### F. The autonomy loop (Sense → Make Sense → Decide → Act → Assess)

Following L14, with OneMind's safety rules:

1. **Sense:** bridges publish `tel.*`, `det.*`, `intel.*`, `tak.cot.out`.
2. **Make Sense:** the entity service fuses inputs into `ent.*` (sections A–C).
3. **Decide:** a watch job (FEATURES I3) or Hermes subscribes to `ent.{type}.>` with a
   rule (for example, "unknown vehicle inside zone `north-gate`"). It may raise
   `evt.entity.{id}.alert`, request an override (disposition `unknown → suspicious`)
   and **propose** an `observe` or `orbit` task through `svc.policy.task.create`.
4. **Act:** policy checks the catalogue, authorization (OpenFGA, lane 17), approval
   level and keep-out zones. A human confirms in the UI or ATAK. The task service
   dispatches `cmd.fleet.*`.
5. **Assess:** the asset reports `evt.mission.{task_id}.*`. Its entity carries
   `current_task` and a `tasked_by` relationship, and the track gains `tracked_by`
   (Lattice `tracked_by` / `active_target`, L4). The loop closes when the track's
   correlation or classification changes or the task reaches a terminal state.

Teams and Mission Autonomy style decomposition: assign to `{team: …}`. A planner (Hermes
or a rule) splits the mission into child `tsk-` tasks with `parent_task_id` and
`allocation[]`, which is the Lattice parent and allocation model (L10). Each child still
passes policy.

### G. Mesh (Lattice Mesh to NATS)

| Lattice Mesh pattern (L2, L11, L15) | OneMind equivalent |
|---|---|
| Every system is a node that publishes and subscribes | Every site (hub, home server, Mac, field kit) runs a NATS leaf or server. The TAK mesh gateway covers LAN ATAK peers (omos `feat/tak-mesh-gateway`) |
| Live data first, backfill later | Separate consumers: the live push consumer for `ent.>` and `cmd.fleet.>` gets priority. Backfill uses JetStream mirrors or sources that catch up after reconnect. CoT `qos` priority is set on projection (C2) |
| Caching for store-and-forward | Site streams mirror the hub `ENT` filtered to the site's area (X1). `CMD` durable consumers per asset hold tasks while the asset is offline |
| Gossip of a signed, versioned asset DB | KV `node-registry` and `device-registry` (exist, N2) replicated to sites. Entries are versioned by KV revision. Signing belongs to lane 17 |
| Reachability assertions with expiry; short-lived group keys | Leaf health as `tel.{node}.link.*`, with entities of type `node` carrying `health.connection`. Short-lived NATS user JWTs (lane 17) |
| Tiered object CDN (local, then peer, then next layer up), 1 GiB limit, SHA-256 | NATS Object Store (`OBJ_imagery`, `OBJ_datapkg`, X1) mirrored per site, with a digest per object. Entity `media[].object` references it |
| Offline private CA | Site CA plus NATS TLS (lane 17) |

### H. CoT projection of entities and tasks

| OneMind | CoT |
|---|---|
| `entity_id` | `uid` (original CoT uid kept in `aliases` when hashed) |
| `ontology.cot_type`, built from `disposition` + `environment` + `platform_type` | `type` `a-{f|a|n|u|s|p}-{A|S|U|G|P}-…` (affiliation and dimension trees, C1) |
| `provenance.how` | `how` (`m-g`, `m-f` fused, `m-s` simulated, `h-e` human, `m-p` relayed) |
| `t`, `expiry_t` | `time`/`start`, `stale` |
| `uncertainty.ce_m`, `le_m` | `point ce`, `le` (C2) |
| `location.course_deg`, `speed_mps` | `<track course speed>` |
| `aliases.name` | `<contact callsign>` |
| `relationships member_of team` | `<__group name role>`; leader as `<link relation="p-l">` |
| `correlation` primary → secondaries | `<link relation="c-c">` (correlated) or `c-f` (fused) per secondary |
| `sources[]` | `<link relation="p-s">` (derived from) for each non-TAK source; `remarks` for a summary |
| `health.battery_pct` | `<status battery>` |
| `sensors[].fov` | `<sensor azimuth fov vfov range …>` |
| `media` video | `<__video url>` |
| `indicators.simulated` / `exercise` | `opex="s-…"` / `"e-…"` (C2); also `how="m-s"` for simulated sources |
| `classification` | `access`, `caveat`, `releasableTo` attributes (tak-protocols lane) |
| `indicators.egress_tak` | The projector sends the entity only if true and it is in an AOI or tasked (FEATURES A6) |
| geo `zone` circle / polygon / rectangle | `u-d-c-c` / `u-d-f` / `u-d-r` (C3) |
| route | `b-m-r` with `b-m-p-w` / `b-m-p-c` links (C3) |
| Task (to TAK) | `t-s-v` / `t-s-i` (observe, capture), `t-r` (goto), `t-x-v-m` (medevac), `t-o-o` (mission), with `<link relation="t-o">` to the target entity and `t-s` to the assignee |
| Task status (to TAK) | `y-*` reply with `<link relation="p-t">` to the task uid (table in E) |

## Risks

| # | Risk | Mitigation |
|---|---|---|
| R1 | **Licence and IP.** The Lattice SDK licence bars derivative works and using the SDK to build another SDK (L13). Copying protos or field names wholesale could be argued to be derivation | Design only: OneMind's own names, JSON, enums and schemas; no Lattice code or protos in either repo; no "Lattice" branding. This document records the provenance of each idea. *Verifier note:* the scope paragraph says OneMind takes no Lattice field names, but section A/E reuses several (`is_live`, `no_expiry`, `task_catalog`, `initial_entities`, `parent_task_id`, `alternate_ids`). The licence (L13) restricts use of the **SDK**, and §3.2 says you are "free to develop applications for other platforms" if the SDK is not used for them. Whether reusing field names from the public API docs is acceptable is a legal judgement and **unverified**. Rename them, or have the owner accept them |
| R2 | **Military-only fields** (`target_priority`, `supplies.munitions`, `engagement_zone`, CoT `t-k-*` strike) conflict with the mission (civilian, no weapons, no targeting of people) | Excluded from the schema. Policy rejects strike or engage task types and any `hostile` target list. `disposition` is kept for TAK symbology only. A person entity can never be the object of a `cmd.fleet.*` task (policy rule) |
| R3 | `evt.entity.>` and `ent.>` have **no stream** today (N2); a publisher would lose data silently | omos PR: `ENT` CR (from the NATS lane) plus `evt.entity.>` added to `EVT`. Until then, run locally per decision 0004 #11 |
| R4 | "Task" name collision with om-task's `task.*` and `evt.task.*` (N4) | Asset tasks use the `mission` domain (`evt.mission.tsk-…`). Never publish `evt.task.*` for asset tasks |
| R5 | Grammar conflict: `cmd.{domain}.{entity}.{action}` (four tokens, CLAUDE.md) versus `cmd.fleet.{class}.{id}.{verb}` (five, doctrine §9.1) | Follow §9.1 and FEATURES D3, as the drones lane does. Record the conflict for the owner (BUS-DESIGN) |
| R6 | Ids with `.` or other unsafe characters (CoT uids, AIS names) break subjects (N5) | The hash rule in A, with the original kept in `aliases` |
| R7 | Update storms: publishing `ent.*` on every telemetry tick | Change-only publishing with a minimum interval per type. Lifecycle `evt.entity.*` events only for meaningful changes (L3's "update only when source time changes") |
| R8 | ATAK support for `t-*` tasking and `y-*` replies is **unverified**; most of the tree is P or Exp (C1) | Treat CoT tasking as best effort; the authoritative path is `cmd.*`/`evt.mission.*`. Spike with the ATAK plugin lane |
| R9 | Fusion quality: Lattice's algorithms are not public | Lane 15 chooses the tracker. This model only fixes the output schema |
| R10 | Privacy: `person` entities (TAK EUDs, people seen by cameras) | Classification `private` by default for camera-derived persons, no face or identity fields, short TTL; projection to TAK only for team members |

## Recommendation

1. **Adopt sections A–H as the basis for `docs/contracts/entity.md`,
   `mission-task.md`, `asset-command.md` and `cot-projection.md`, and for the entity and
   task rows of `docs/design/ONTOLOGY.md`.** The model covers every public Lattice entity
   component except the weapons-oriented ones (R2). It keeps the §9.2 geo-envelope for
   backward compatibility and adds per-source provenance, persistent expiry through
   `Nats-TTL`, and policy-gated tasks, so it is at least as complete as Lattice's.
2. **Entity service (FEATURES A4)** owns `ent.*`. It enforces per-source monotonic
   `source_t`, expiry through `Nats-TTL` on `ENT`, overrides through
   `cmd.entity.{id}.override`, and manual-over-automated correlation precedence.
3. **Task service** stores the task record in KV `mission-state` with CAS on
   `status_version`, uses `evt.mission.{tsk-…}.{state}` for facts and `cmd.fleet.*` for
   dispatch, and gates every task on the assignee's `task_catalog`, policy and human
   confirmation. Hermes and watch jobs only propose.
4. **omos PR (owner merges):** the `ENT` stream CR; `evt.entity.>` bound to a stream; a
   `det.>` stream (from the drones lane); `services/schemas/entity/entity.v1.schema.json`
   and `services/schemas/tasks/*.v1.schema.json`. Write an owner note on the R5 grammar
   conflict per decision 0004 #8.
5. Build the first loop locally with `sim-x1`: a `det.*` track near a zone, then a watch
   job proposes an `orbit`, a human confirms, MAVLink executes, and `evt.mission.*`
   reports reach `done_ok`, with the CoT `y-*` projection checked by `takproto` decode.
   This is the acceptance test for FEATURES H4.

## Unverified items

- How Lattice correlates and fuses tracks (algorithms, gating, track quality
  computation): not public. Only the result schema is verified (L4).
- The list of **overridable** entity fields: L5 says "refer to our documentation" but no
  list was found in the fetched pages.
- The full `ALT_ID_TYPE_*` and `NATIONALITY_*` enums were truncated in extraction (L4);
  not needed for OneMind.
- Lattice Mesh internals beyond patent US10506436B1 (routing, current protocol): the
  patent dates from 2018, and whether today's product follows it is **unverified**.
  anduril.com product pages could not be read (JavaScript-rendered), and
  blog.anduril.com returned HTTP 403. These were fetch limits, not permission denials.
- Mission Autonomy's intent-to-task decomposition: press and vendor claims only (L16).
- Whether ATAK or iTAK render or act on CoT `t-*` tasking and `y-*` replies (C1 marks most
  of them proposed or experimental).
- The covariance-to-`ce`/`le` conversion: the formula is left to lane 15. *Verifier
  correction:* the target is not CEP. When `ce` and `le` represent error, the CoT base
  schema defines them as "the one sigma point for a zero mean normal (Gaussian)
  distribution" (C2, `ce`/`le` attribute documentation).
- The per-type TTL defaults (aircraft 120 s and so on) come from the NATS lane proposal
  (X1) and have not been measured against feed cadences.

**Blockers:** none from the permission system. No cluster access was needed for this
lane (the omos facts come from the fresh clone at `/Users/danielcarku/Desktop/work/omos`).

## Independent verification (2026-10-29)

A second agent re-checked 30 claims against the primary sources: the scratch Lattice
`.md` pages, a fresh fetch of `reference/rest/entities/stream-entities.md` (the same
content as scratch `se.md`), the GitHub API README of `anduril/sample-app-auto-reconnaissance`,
a fresh fetch of the patent page, atak-civ `types.txt` and the Base-Event XSD, and the omos clone.

- **Confirmed (25):** the required entity fields; CREATE/UPDATE driven by
  `source_update_time`; the "greater than" update rule; `is_live=false` and expiry deletes;
  the 30-day expiry limit; the 5-minute `noExpiry` restart rule; the "owned" and "may not
  edit or delete" quote; override eventual consistency and last-writer-wins; the override
  status and type enums; the template enum and the asset/track/geo required components;
  the stream event types, the 30 s heartbeat default and `preExistingOnly`; decorrelation
  precedence, correlation type and replication mode; the relationship types; track quality
  0–15; the task status and error-code enums; the `taskId` pattern; the 5 s fixed retry,
  which applies only with `deliverBefore`; "guard against out of order updates"; terminal
  DONE_OK/DONE_NOT_OK; the capability-gate quote; agent reject and cancel accept/refuse,
  and immediate cancel when unsent; the 30 s default and 1 s minimum agent heartbeat, the
  5-minute long-poll and "does not wait for the agent to acknowledge"; breaking-change
  rules; the sample-app loop (5 miles, suspicious, Orbit); the licence quotes; objects
  (1 GiB, SHA-256, tiered CDN); the principles quotes; the patent quotes (live-first,
  caching, signed monotonic asset DB, expiring assertions, 10s-of-minutes group keys, JWTs
  from the hub); the CoT how, `t-*`, `y-*` and link trees; `opex`, `qos`, `ce`/`le` as a
  cylinder; KV settings; the om-task subjects; the envelope fields; the mission-state enum;
  and doctrine §9.1 `cmd.fleet` and `cmd.mission`.
- **Corrected (5):** the `EVT` bindings (two places, plus the N2 source row); "nothing
  publishes `ent.*`"; the `cmd.mission.*.cancel` verb attributed to §9.1; `ce`/`le` as
  1-sigma rather than CEP. Partly corrected: MSL is unsupported only in `Position`.
- **Marked unverified (2):** whether the om-overwatch services run live; whether reusing
  Lattice API field names is licence-safe.
- **Licence and cluster safety:** no worldmonitor or worldwideview code is copied, no
  Lattice code or protos are copied, and the cluster was not touched. The field-name reuse
  is flagged in R1.
- **Follow-up outside this lane:** `om-overwatch-state/ontology.go` says it is "derived
  from Constellation Overwatch", and `om-overwatch-interceptor` describes itself as "our own
  kill vehicle". Both bear on the mission and licence rules. Neither was checked here.
