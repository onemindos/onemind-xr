# Contract: entity state

Status: M0, 2026-10-29. Version `v: 1`.
Ontology: [ONTOLOGY.md](../design/ONTOLOGY.md) §3.

This contract defines the fused entity record that the omos entity service (FEATURES A4)
publishes and that every client, Hermes tool and the TAK projector reads. Per onemind-xr
`CLAUDE.md`, omos is the source of truth for the subjects and buckets; the omos files are cited
in §1. Where an omos file does not exist yet it is marked **proposed**.

## 1. Subjects, stream and buckets

| Item | Value | Defined in (omos) | State |
|---|---|---|---|
| Latest state | `ent.{entity_type}.{entity_id}.state` | `CLAUDE.md` grammar block ("fused entity state") | pattern exists |
| Stream | `ENT`, subjects `ent.*.*.state`, `maxMsgsPerSubject: 1`, `allowDirect`, `allowMsgTtl`, `subjectDeleteMarkerTtl: 15m`, `maxBytes: 1 GiB`, R3 file | `platform/nats/streams/ent.yaml` | **proposed** ([nats-platform.md](../research/nats-platform.md) Recommendation §2) |
| Interim (until `ENT` merges) | core `ent.*.*.state` plus a snapshot from `svc.entity.query`. KV `world-state` and `fleet-state` are **not** used for per-entity copies: [BUS-DESIGN.md](../design/BUS-DESIGN.md) D3 gives them layer manifests and site status | `platform/nats/streams/kv-world-state.yaml`, `kv-fleet-state.yaml` | buckets exist, empty today ([nats-platform.md](../research/nats-platform.md) §1) |
| Lifecycle facts | `evt.entity.{entity_id}.{event}`, event ∈ `created, deleted, expired, linked, unlinked, correlated, decorrelated, overridden, classified, alert` | `platform/nats/streams/evt.yaml` must add `evt.entity.>` | **proposed** (no stream binds it today; [lattice-patterns.md](../research/lattice-patterns.md) R3) |
| Edit commands | `cmd.entity.{entity_id}.{override\|correlate\|decorrelate\|delete\|create\|star\|acknowledge\|resolve}` | grammar `cmd.{domain}.{entity}.{action}`; stored by `platform/nats/streams/cmd.yaml` (`cmd.>`) | pattern and stream exist; domain `entity` to be ratified (ONT-P9) |
| Reads | `svc.entity.get`, `svc.entity.search`, `svc.entity.query`, `svc.entity.history`, `svc.fusion.explain` | grammar `svc.{service}.{endpoint}` | pattern exists; service **proposed** |
| Schema | `services/schemas/entity/entity.v1.schema.json`, mirrored to KV `om-schema` key `entity.v1` | `services/schemas/` (new file); `platform/nats/streams/kv-om-schema.yaml` | schema **proposed**; bucket exists |
| Inputs (for reference) | `tel.fleet.{class}.{id}.position\|status`, `det.{domain}.{type}.{id}`, `intel.{domain}.{source}.{type}`, `evt.world.*`, `tak.cot.out`, `tak.cot.mesh.rx` | `docs/NATS-DOCTRINE.md` §9.1, §9.8; streams `tel.yaml`, `intel.yaml`, `world.yaml`, `cot.yaml`; `tak.cot.mesh.rx` from the mesh gateway `services/bridge/bridges/tak/main.go` line 36 (branch `feat/tak-mesh-gateway`), stored by `COT` (`tak.cot.>`) through the site stream `SITE_COT` ([BUS-DESIGN.md](../design/BUS-DESIGN.md) §3.3; [cot-projection.md](cot-projection.md) §8.1, proposal CP-4) | exist (except `DET`, proposed; `tak.cot.mesh.rx` exists on the branch only, no PR yet) |

Existing consumers that must keep working: the TAK projector subscribes `ent.*.*.state` on core
NATS and reads `entity_id`, `entity_type`, `lat`, `lon`, `confidence`, `properties`
(`omos/platform/omos/tak-projector.yaml` lines 13, 165–220); `sink-clickhouse` also subscribes
`ent.*.*.state` ([fusion-and-tracking.md](../research/fusion-and-tracking.md) "Payload").

## 2. Ownership

| Who | May | May not |
|---|---|---|
| omos entity service (single active replica, KV lease) | publish `ent.>` (JetStream publish with ack once `ENT` exists), `evt.entity.>`; consume `cmd.entity.>`; serve `svc.entity.*` | — |
| Bridges and collectors | publish observations on their own subjects | publish `ent.*` or `cmd.entity.*` |
| Policy service | publish `cmd.entity.*` after an approved action | write entities |
| Clients (Unreal, L0 web, Quest, ATAK plugin) and Hermes | read through `svc.entity.*`, the `ENT` stream or KV; propose edits with `svc.policy.command.propose` | publish `ent.*` or `cmd.*` (except `cmd.map.>`) |

Sources: [lattice-patterns.md](../research/lattice-patterns.md) §A rules and §B;
[fusion-and-tracking.md](../research/fusion-and-tracking.md) "JetStream sketch";
[security-and-trust.md](../research/security-and-trust.md) §3. **Not enforced today** (one shared
account, [nats-platform.md](../research/nats-platform.md) §1); enforced by the planned per-service
users.

## 3. Identity

**`entity_type`** (subject token two, and the payload field of the same name):

| Token | Object type | Notes |
|---|---|---|
| `drone`, `robot`, `vehicle`, `node` | own fleet | match `tel.fleet.{class}` vocabulary in doctrine §9.1 |
| `aircraft`, `vessel` | world feeds | `vessel`, not `ship`; both are keys in the projector's `COT_TYPE_MAP` |
| `person`, `team`, `agent` | people, groups, AI agents | `person` only for team members and safety subjects |
| `sensor`, `camera` | sensing | |
| `track` | fused keyless detections | stays `track` after classification; class goes in `kind` |
| `place`, `structure`, `zone` | geography and twin | `zone` mirrors KV `geofences` |
| `event`, `alert` | occurrences and attention items | |

**`entity_id`** (subject token three): `[a-z0-9_-]{1,64}`, lowercase, globally unique and
stable, `{prefix}-{native}`: `sim-x1`, `icao-a1b2c3`, `mmsi-367000001`, `norad-25544`,
`tak-{uid}`, `gtfs-{agency}-{vehicle}`, `trk-{ulid}`, `plc-{ulid}`, `zone-{id}`, `alr-{ulid}`.
ULIDs are written in lowercase. If a native id has any other character (CoT uids often contain
`.`), use `{prefix}-{base32(sha256(native))[0:26]}` in lowercase and keep the original in
`aliases.ids`. Lowercase is required because the omos command envelope's `cmd` field pattern is
`^cmd\.[a-z0-9_][a-z0-9_.\-$]*$` (`omos/services/schemas/command/envelope.schema.json`), and
`cmd.entity.{entity_id}.*` must match it. NATS tokens cannot contain `.`, `*`, `>` or
whitespace ([lattice-patterns.md](../research/lattice-patterns.md) N5, R6).

Own simulated assets use `sim-*` ids and `indicators.simulated: true` (decision 0004 #13).

## 4. Message envelope

The payload is a plain JSON object, **not** wrapped in the omos event envelope v1. Reason: the
projector reads top-level fields, and wrapping them in `data` would break TAK output
([fusion-and-tracking.md](../research/fusion-and-tracking.md) Risk 4). Event identity travels in
headers (§4.3).

### 4.1 Fields

Times on this hot path are epoch milliseconds UTC, as doctrine §9.2 requires. Distances are
metres, speeds m/s, angles degrees true.

**Group A: doctrine §9.2 geo-envelope** (`omos/docs/NATS-DOCTRINE.md` §9.2). Kept so existing
consumers and TACMAP render the entity unchanged.

| Field | Type | Req | Meaning |
|---|---|---|---|
| `uid` | string | yes | = `entity_id` |
| `type` | string | yes | doctrine §9.3 dot-path, e.g. `fleet.drone`, `air.commercial`, `sea.cargo`, `tak.atak`; for types §9.3 lacks, `onemind.{entity_type}` |
| `lat`, `lon` | number | yes | WGS-84 degrees. For non-located types (`team`, `agent` without position, `alert` without `about` position) use the reserved values `lat: 0, lon: 0` and set `traits` without `locatable`; clients must not draw them |
| `hae` | number | no | metres above ellipsoid, -1 unknown |
| `course`, `speed` | number | no | -1 unknown |
| `callsign` | string | no | display name |
| `stale` | integer | no | seconds until the reading is stale |
| `t` | integer | yes | source time of the newest contribution, epoch ms |
| `meta` | object | no | doctrine §9.4 domain fields (`battery_pct`, `mode`, `link_rssi`, `mission_id`, `node_id` for fleet) |

**Group B: fields the TAK projector reads today** (`tak-projector.yaml` lines 165–220).

| Field | Type | Req | Meaning |
|---|---|---|---|
| `entity_id` | string | yes | §3 |
| `entity_type` | string | yes | §3; equals subject token two |
| `confidence` | number 0–1 | yes | fused confidence (§7) |
| `properties` | object | yes | `{callsign, altitude, speed, course, ce, le}` copied from the components below for the projector |

**Group C: OneMind components**

| Field | Type | Req | Meaning |
|---|---|---|---|
| `v` | integer | yes | contract version, `1` |
| `seq` | integer | yes | entity-service revision for this entity, strictly increasing |
| `live` | boolean | yes | `false` on the final record before deletion |
| `lifecycle` | enum | yes | `tentative`, `active`, `stale`, `lost`, `expired` |
| `created_t` | integer | yes | first publish, epoch ms |
| `expiry_t` | integer\|null | yes | when the entity expires; `null` only if `pinned` |
| `pinned` | boolean | yes | never expires (places, structures, zones, teams, pinned operator entities) |
| `kind` | string | no | finer type: `multirotor`, `earthquake`, `geofence`, `person` (track class) … |
| `template` | enum | yes | `asset`, `track`, `geo`, `spi` |
| `traits` | string[] | yes | subset of `locatable`, `trackable`, `taskable`, `observable` |
| `disposition` | enum | no | `friendly`, `assumed_friendly`, `neutral`, `unknown`, `suspicious`, `pending`. **No `hostile`** |
| `environment` | enum | no | `air`, `surface`, `subsurface`, `land`, `space` |
| `cot_type` | string | no | precomputed CoT type for the projector (cot-projection contract) |
| `aliases` | object | yes | `{name: string\|null, ids: [{type, id}]}`; `type` ∈ `cot_uid, icao, mmsi, norad, mavlink_sysid, gtfs, tak_callsign, …` |
| `location` | object | if `locatable` | `{lat, lon, hae_m, agl_m, course_deg, speed_mps, vel_enu_mps[3], attitude_q[4], measured_t}` |
| `uncertainty` | object | if `locatable` | `{ce_m, le_m, pos_enu_cov[6]\|null, vel_enu_cov[6]\|null, error_ellipse{p, semi_major_m, semi_minor_m, orient_deg}\|null}`. `ce_m`/`le_m` are 1-sigma, as the CoT base schema defines them ([lattice-patterns.md](../research/lattice-patterns.md) verifier correction) |
| `geo` | object\|null | if `template=geo` | `{shape: point\|line\|polygon\|circle\|corridor, coords: GeoJSON geometry, radius_m?, kind: aoi\|keep_in\|keep_out\|hazard\|control_area\|footprint\|route, alt_min_m?, alt_max_m?}` |
| `status` | object | no | `{activity, role, mode}` |
| `health` | object | no | `{connection: online\|degraded\|offline, overall: healthy\|warning\|failed, battery_pct, link_rssi, components:[{id, health}], alerts:[{code, level: advisory\|caution\|warning, since_t}]}` |
| `sensors` | array | if `observable` | `[{id, type, state, fov:{h_deg, v_deg, az_deg, el_deg, range_m}}]` |
| `capabilities` | string[] | if `taskable` | task types accepted, e.g. `onemind.task.goto.v1` (mission-task.md §4) |
| `current_task` | object\|null | no | `{task_id, state}` |
| `tracked` | object\|null | if `template=track` | `{quality: 0–15, hits, last_meas_t, lob:{from, bearing_deg, elev_deg}\|null}` |
| `correlation` | object\|null | no | `{set_id, role: primary\|secondary, primary_id, secondary_ids[], method: manual\|automated, decorrelated_from[]}` |
| `relationships` | array | yes (may be empty) | `[{id, type, to, since_t, source}]`, link types in ONTOLOGY §5 |
| `media` | array | no | `[{kind: video\|image\|tileset, url?, object?, sha256?, role}]`; video URLs point at MediaMTX, never at NATS |
| `route` | object\|null | no | `{destination, eta_t, mission_id}` |
| `indicators` | object | yes | `{simulated, exercise, emergency, starred, egress_tak}` |
| `marking` | object | yes | `{tlp: CLEAR\|GREEN\|AMBER\|AMBER+STRICT\|RED, releasable_to: [group]}` |
| `cot_security` | object\|null | no | incoming CoT `{access, caveat, releasableTo}` preserved verbatim ([security-and-trust.md](../research/security-and-trust.md) §5) |
| `provenance` | object | yes | `{primary, how, source_t}`; `how` uses CoT codes `m-g`, `m-f` (fused), `m-s` (simulated), `h-e`, `m-p` |
| `sources` | array | yes | `[{integration, source_id, data_type, subject, stream, seq, first_seen_t, last_seen_t, source_t, components[]}]` |
| `confidence_parts` | object | yes | `{source_prior, track_quality, corroboration}` each 0–1 |
| `conflicts` | array | no | `[{attribute, kept, rejected, sources[]}]` |
| `overrides` | array | no | `[{id, field, value, by, action_id, status: applied\|pending\|rejected, kind: live\|post_expiry, at_t}]` |

Field names are OneMind's own; Lattice names are deliberately not reused
([lattice-patterns.md](../research/lattice-patterns.md) R1; ONTOLOGY §10). The model drops
Lattice's weapons-oriented components (`target_priority`, `supplies.munitions`,
`ENGAGEMENT_ZONE`) ([lattice-patterns.md](../research/lattice-patterns.md) R2).

Consumers must ignore unknown fields (the forward-compatibility rule of
`omos/services/schemas/envelope/envelope.schema.json`, `additionalProperties: true`).

### 4.2 Example: simulated drone

```json
{
  "uid": "sim-x1", "type": "fleet.drone", "lat": 30.2672, "lon": -97.7431, "hae": 152.4,
  "course": 90, "speed": 12.5, "callsign": "SIM-X1", "stale": 60, "t": 1793275200000,
  "meta": { "battery_pct": 82, "mode": "AUTO", "link_rssi": -67, "mission_id": "msn-01jbx7a0000000000000000000", "node_id": "build-mac" },

  "entity_id": "sim-x1", "entity_type": "drone", "confidence": 0.97,
  "properties": { "callsign": "SIM-X1", "altitude": 152.4, "speed": 12.5, "course": 90, "ce": 5.0, "le": 3.0 },

  "v": 1, "seq": 42, "live": true, "lifecycle": "active",
  "created_t": 1793275100000, "expiry_t": 1793275260000, "pinned": false,
  "kind": "multirotor", "template": "asset",
  "traits": ["locatable", "trackable", "taskable", "observable"],
  "disposition": "friendly", "environment": "air", "cot_type": "a-f-A-M-H-Q",
  "aliases": { "name": "SIM-X1", "ids": [ { "type": "mavlink_sysid", "id": "1" }, { "type": "cot_uid", "id": "sim-x1" } ] },
  "location": { "lat": 30.2672, "lon": -97.7431, "hae_m": 152.4, "agl_m": 40.0, "course_deg": 90,
                "speed_mps": 12.5, "vel_enu_mps": [12.5, 0, 0], "attitude_q": [1, 0, 0, 0], "measured_t": 1793275200000 },
  "uncertainty": { "ce_m": 5.0, "le_m": 3.0, "pos_enu_cov": [25, 0, 0, 25, 0, 9], "vel_enu_cov": null, "error_ellipse": null },
  "geo": null,
  "status": { "activity": "orbit", "role": "overwatch", "mode": "AUTO" },
  "health": { "connection": "online", "overall": "healthy", "battery_pct": 82, "link_rssi": -67,
              "components": [ { "id": "gps", "health": "healthy" } ], "alerts": [] },
  "sensors": [ { "id": "cam0", "type": "camera", "state": "operational",
                 "fov": { "h_deg": 82, "v_deg": 52, "az_deg": 90, "el_deg": -30, "range_m": 300 } } ],
  "capabilities": [ "onemind.task.goto.v1", "onemind.task.orbit.v1", "onemind.task.rtl.v1", "onemind.task.land.v1", "onemind.task.observe.v1" ],
  "current_task": { "task_id": "tsk-01jbx7k2m9q4r8s6t0v2w4y6z8", "state": "executing" },
  "tracked": null, "correlation": null,
  "relationships": [ { "id": "rel-1", "type": "member_of", "to": "team.team-cyan", "since_t": 1793275000000, "source": "registry" } ],
  "media": [ { "kind": "video", "url": "rtsp://127.0.0.1:8554/sim-x1", "role": "primary" } ],
  "route": null,
  "indicators": { "simulated": true, "exercise": false, "emergency": false, "starred": false, "egress_tak": true },
  "marking": { "tlp": "AMBER+STRICT", "releasable_to": ["homestead"] },
  "cot_security": null,
  "provenance": { "primary": "bridge-mavlink", "how": "m-s", "source_t": 1793275200000 },
  "sources": [ { "integration": "bridge-mavlink", "source_id": "sysid-1", "data_type": "mavlink",
                 "subject": "tel.fleet.drone.sim-x1.position", "stream": "TEL", "seq": 118220,
                 "first_seen_t": 1793275100000, "last_seen_t": 1793275200000, "source_t": 1793275200000,
                 "components": ["location", "health", "status"] } ],
  "confidence_parts": { "source_prior": 0.95, "track_quality": 1.0, "corroboration": 0.95 },
  "conflicts": [], "overrides": []
}
```

The `to` field of a relationship is `{entity_type}.{entity_id}` so a client can build the
subject or KV key directly.

### 4.3 Headers

| Header | Value | Why |
|---|---|---|
| `Nats-Msg-Id` | `{entity_id}:{seq}` | JetStream dedup on replay ([lattice-patterns.md](../research/lattice-patterns.md) §A rules) |
| `Nats-TTL` | per type: aircraft `120s`, vessel `30m`, fleet asset `60s`, TAK EUD = CoT stale, pinned `never` | per-message expiry on `ENT` ([nats-platform.md](../research/nats-platform.md) §2; defaults not yet measured against feed cadence, [lattice-patterns.md](../research/lattice-patterns.md) "Unverified") |
| `OneMind-Marking`, `OneMind-Releasable-To` | copy of `marking` | release service filters without parsing the body ([security-and-trust.md](../research/security-and-trust.md) §5) |
| `OneMind-Schema` | `entity.v1` | schema lookup in KV `om-schema` |

## 5. Semantics the entity service guarantees

| Rule | Behaviour | Source |
|---|---|---|
| Per-source monotonic time | A contribution is accepted only if its `source_t` is newer than the last accepted `source_t` **for that source**; older ones are dropped and counted | [lattice-patterns.md](../research/lattice-patterns.md) §A; [fusion-and-tracking.md](../research/fusion-and-tracking.md) Stage 2 |
| Clock skew | `source_t` more than 5 s in the future is replaced by receive time and counted | [fusion-and-tracking.md](../research/fusion-and-tracking.md) "Conflicting data" |
| Change-only publish | Publish when a component changes beyond threshold (position moved more than `ce_m`, class or status changed) or at a heartbeat of at most `min(stale/2, 120 s)` | [fusion-and-tracking.md](../research/fusion-and-tracking.md) "Rate control" |
| `seq` | Strictly increasing per entity; a client that sees a lower `seq` than it holds ignores the message | this contract |
| Lifecycle | `tentative` (keyless tracks before M-of-N confirmation; not published to `ent` unless a client asks for tentative tracks through `svc.entity.query`), `active`, `stale` (no update within `stale`), `lost` (3 × stale), `expired` (deleted) | [fusion-and-tracking.md](../research/fusion-and-tracking.md) "Stale and expiry" |
| Delete | Publish a last record with `live: false`, then let `Nats-TTL` expire it (delete marker on `ENT`) or purge; emit `evt.entity.{id}.deleted` (or `.expired`) | [lattice-patterns.md](../research/lattice-patterns.md) §A |
| Conflicts | Per attribute: human override, then cooperative self-report, then sensor estimate; newest `source_t` within a tier; classification never downgrades to unknown; every resolution recorded in `conflicts[]` | [fusion-and-tracking.md](../research/fusion-and-tracking.md) "Conflicting data" |
| Correlation | Manual beats automated; a manual decorrelation blocks later automatic merges. Auto-merge only on a shared hard key or a declared link; otherwise a human-confirmed proposal | [lattice-patterns.md](../research/lattice-patterns.md) §C; [fusion-and-tracking.md](../research/fusion-and-tracking.md) Stage 4 |
| Echo filter | Ignore CoT on `tak.cot.>` whose uid starts `OM.` (the projector's own output) | [fusion-and-tracking.md](../research/fusion-and-tracking.md) Risk 1; `tak-projector.yaml` line 191 |
| Person safety | No `person` entity is created from camera, ML or RF detection; such detections are `track` with `kind: person`, marking AMBER+STRICT, `egress_tak: false`, TTL ≤ 60 s | ONTOLOGY §3.3; [lattice-patterns.md](../research/lattice-patterns.md) R10 |
| Restart | Rebuild from `ENT` and resume consumers; a full rebuild replays `COT`/`TEL`/`WORLD`/`INTEL` from a start time | [fusion-and-tracking.md](../research/fusion-and-tracking.md) "Rebuild from scratch" |

## 6. Edits (overrides)

Clients and agents never write the record. They propose an action (`entity.override`,
`entity.correlate`, `entity.decorrelate`, `entity.delete`, `entity.star`) through
`svc.policy.command.propose` ([asset-command.md](asset-command.md) §4). After approval the policy service
publishes `cmd.entity.{id}.{verb}` in the omos command envelope with `data`:

| Verb | `data` |
|---|---|
| `override` | `{field, value, kind: live\|post_expiry, reason}` |
| `correlate` | `{with: entity_id, reason}` |
| `decorrelate` | `{from: entity_id, reason}` |
| `delete` | `{reason}` |
| `create` | a partial entity (places, structures, manual entities) |
| `star`, `acknowledge`, `resolve` | `{}` |

Per-property writeback strategy: `override_wins` (name, disposition, notes, geometry of
manual entities, `safety_status`) or `newest_wins` (location, health). Overrides persist across
source updates until removed. A `post_expiry` override revives an expired entity as pinned
([palantir-patterns.md](../research/palantir-patterns.md) §5;
[lattice-patterns.md](../research/lattice-patterns.md) §A rules). The entity service applies the
edit, appends to `overrides[]`, publishes the new state and `evt.entity.{id}.overridden`, and
reports the action outcome on `evt.so.action.executed` (asset-command.md §5).

## 7. Confidence

`confidence` is one number in [0, 1], below 1, computed from `source_prior` (per-feed reliability
from the feed registry), `track_quality` (confirmation, hit ratio, covariance size) and
`corroboration` (probabilistic OR over independent sources), and it decays with age relative to
`stale`. The three parts are always published in `confidence_parts` so the UI and
Hermes can explain it. The exact formula is the entity service's to fix and test
([fusion-and-tracking.md](../research/fusion-and-tracking.md) "Confidence").

## 8. How a client reads entities

| Need | Target (after `ENT` merges) | Interim |
|---|---|---|
| Snapshot plus live | Trusted site clients: ordered ephemeral consumer on `ENT`, filter `ent.{type}.>`, `DeliverLastPerSubject`; never a durable per client ([nats-platform.md](../research/nats-platform.md) R15). `CLIENT`-account viewers: the layer service (`svc.layer.*`, layer contracts), because that account has no JetStream API ([BUS-DESIGN.md](../design/BUS-DESIGN.md) D7) | `svc.entity.query` snapshot, then core subscription to `ent.{type}.*.state` |
| One entity | `svc.entity.get`, or Direct Get last-by-subject on `ENT` for trusted clients | `svc.entity.get` |
| Filtered by area or fields | `svc.entity.query` `{types[], bbox[lon0,lat0,lon1,lat1], fields[], as_of_t?, limit, cursor}` | same service |
| Expiry | Delete marker from `ENT` (ordered consumer sees it; core subscribers do not, [nats-platform.md](../research/nats-platform.md) R8) | the final `live: false` record |

Clients drop records whose `marking` the viewer may not see (defence in depth; the release
service and account boundary are the real control, [security-and-trust.md](../research/security-and-trust.md) §5).
Clients dim `stale` and hide `live: false`. Large layers render with batched instancing
(onemind-xr `CLAUDE.md` "Engineering conventions").

## 9. Versioning

- `v` is the contract version. Adding optional fields keeps `v: 1`. Removing or renaming a field,
  changing a type or changing a meaning is breaking: publish `entity.v2.schema.json`, set `v: 2`,
  and run both for one release; consumers pin a minimum `v` and tolerate unknown fields (rule in
  `omos/services/schemas/envelope/envelope.schema.json`).
- The schema is published to KV `om-schema` as `entity.v{n}` so clients can discover it.
- This mirrors the Lattice task-definition rule (add freely, never remove or retype)
  ([lattice-patterns.md](../research/lattice-patterns.md) §Tasks, L8).

## 10. Acceptance tests (for A4 and client B lanes)

1. The same aircraft from two feeds yields one `ent.aircraft.icao-….state` with two `sources[]`
   (FEATURES A4).
2. Replaying `TEL` twice produces no duplicate `seq` and no `ENT` growth (dedup by `Nats-Msg-Id`).
3. An observation with an older `source_t` than the stored one is dropped and counted.
4. A `sim-x1` record validates against `entity.v1.schema.json` and the projector produces CoT
   from it unchanged.
5. A camera detection of a person never produces `ent.person.*` and never reaches `tak.cot.inject`.

## 11. Unverified and open

- `ENT`, `DET` and `evt.entity.>` storage need owner-merged omos PRs (ONT-P1–ONT-P3).
- NACK support for `allowMsgTtl` and delete-marker settings; live server 2.14.2 predates a TTL
  fix in 2.14.8 ([nats-platform.md](../research/nats-platform.md) R9).
- The reserved `lat: 0, lon: 0` for non-located entities is a design choice that keeps doctrine
  §9.2's required fields; the owner may prefer to keep non-located types off `ent.*`.
- The per-type TTL defaults are not measured against feed cadences.
