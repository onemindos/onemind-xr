# OneMind bus design (target NATS design)

Status: **M0 design, proposed.** Date: 2026-10-29. Nothing here is live. Every platform change
below is an omos PR (NACK CRs in `omos/platform/nats/streams/`, server config, SealedSecrets),
never `nats stream add` or `kubectl apply` (omos `CLAUDE.md`, "NATS Streams"). The owner merges
omos PRs (decision 0004 #10).

Inputs: [`nats-platform.md`](../research/nats-platform.md) (NATS lane, proofs P1–P10),
[`edge-and-ddil.md`](../research/edge-and-ddil.md), [`ingestion-platform.md`](../research/ingestion-platform.md),
[`security-and-trust.md`](../research/security-and-trust.md), [`omos-live-state.md`](../research/omos-live-state.md),
plus subject usage in [`lattice-patterns.md`](../research/lattice-patterns.md),
[`fusion-and-tracking.md`](../research/fusion-and-tracking.md), [`agents-that-act.md`](../research/agents-that-act.md),
[`drones-robots-agents.md`](../research/drones-robots-agents.md), [`tak-server-and-bridges.md`](../research/tak-server-and-bridges.md),
[`video-and-plugins.md`](../research/video-and-plugins.md) and [`../sources/SOURCE-REGISTRY.md`](../sources/SOURCE-REGISTRY.md).
Platform rules: omos `CLAUDE.md` (locked grammar) and omos `docs/NATS-DOCTRINE.md` §§2–9.
omos is read at commit `62e3d22b` (2026-10-28).

Client contracts that depend on this file: [`layer-snapshot.md`](../contracts/layer-snapshot.md),
[`layer-delta.md`](../contracts/layer-delta.md). The entity payload itself is defined in
`docs/contracts/entity.md` (another M0 lane) and the object types in `docs/design/ONTOLOGY.md`.

---

## 1. Summary of decisions

| # | Decision | Why (source) |
|---|---|---|
| D1 | NATS JetStream stays the single authority bus | Streams, KV, objects, request/reply, WebSocket and MQTT in one server, proved (nats-platform.md P1–P8); omos `CLAUDE.md` "Bus Authority" |
| D2 | Latest entity state lives in a new **KV-shaped stream `ENT`** on the locked subject `ent.{entity_type}.{entity_id}.state` (1 message per subject, Direct Get, per-message TTL). The entity service publishes **once** with a JetStream ack; core subscribers (`omos-tak-projector`, `sink-clickhouse`) still receive the same message | A KV bucket would move the subject to `$KV.*` and need a second write (nats-platform.md Rec. §1); `ent.>` is stored by no stream today (fusion-and-tracking.md F1); TTL + markers proved on `ent.*` (P10) |
| D3 | The empty buckets get one clear job each: `world-state` holds **per-layer manifests and density cells**, `fleet-state` holds **per-site link and sync status**. No per-entity KV copies | Both were never written (`last_seq 0`, omos-live-state.md §5); A5 says reuse them first (`docs/FEATURES.md` A5); `history 1` avoids the TTL resurfacing gotcha (P2c) |
| D4 | Every stream gets `maxBytes`; total caps fit the 45 GiB per-node JetStream limit with headroom | Only `WORLD` has a cap; 50Gi PVCs, `max_storage` 45 GiB (nats-platform.md §1, B4 verifier) |
| D5 | Each edge site is a **leaf with its own JetStream domain**; the hub **sources** site streams; sites **mirror** what they need from the hub. Commands are never queued for replay | Store-and-forward needs an edge stream (P7, P7d); overlap trap (P7b); edge-and-ddil.md Rec. 1 |
| D6 | Accounts: `SYS`, `ONEMIND` (data and services, one user per service), `CLIENT` (viewers, request/reply only), `AGENTS` (stage 2), `PARTNER-<org>` (filtered release). Only the policy service publishes `cmd.>` (except `cmd.map.>`) | security-and-trust.md Rec. 1–3; nats-platform.md Rec. §4; isolation and service imports proved (P6) |
| D7 | Clients read layers through **`svc.layer.*`** (AOI-filtered snapshot and watch) so the `CLIENT` account never needs the JetStream API; trusted site clients may also read `ENT` directly | Service import/export proved (P6); cross-account JetStream not run (nats-platform.md "Unverified") |
| D8 | All subjects stay inside the locked grammar. Where the grammar and doctrine disagree, this file picks one reading and lists it as an owner proposal (section 10) | Decision 0004 #8 |
| D9 | Civilian scope: no OneMind service publishes or consumes the doctrine's military-surge subjects (`evt.world.air.mil.*`, `evt.world.military.*`); `ent.person.*` is only for consenting team members (TAK EUDs); people detected by sensors are never turned into persistent identity tracks | Run prompt lane 14 and rule 6; omos `docs/NATS-DOCTRINE.md` §9.1 lists those subjects |

---

## 2. What goes where

| Data | Primitive | Subject or bucket | Source |
|---|---|---|---|
| Observations, detections, intel, world raw positions, events, commands, raw TAK wire | Stream | `tel.*`, `det.*`, `intel.*`, `evt.*`, `cmd.*`, `tak.cot.*` | nats-platform.md Rec. §1 |
| Latest state of every entity | KV-shaped stream `ENT` | `ent.{entity_type}.{entity_id}.state` | D2 |
| Per-layer manifest, freshness, density cells | KV `world-state` | keys `{layer}.meta`, `{layer}.cells.z{z}` | D3; [`layer-snapshot.md`](../contracts/layer-snapshot.md) |
| Per-site link and sync status | KV `fleet-state` | key `{site}` | D3 |
| Config, registries, schemas, mission state, geofences, presence | KV (existing buckets) | section 5 | omos `platform/nats/streams/kv-*.yaml` |
| Collector leases | KV `ingest-locks` (new) | key `{source}` | ingestion-platform.md Rec. table "One owner" |
| Offline map packs, 3D Tiles, models, data packages, imagery stills, static layers | Object Store | buckets `tiles`, `models`, `datapkg`, `imagery`, `overlays` | nats-platform.md Rec. §1; edge-and-ddil.md Rec. 3 |
| Basemap tiles (XYZ/WMS/WMTS/3D Tiles) | **not NATS**: caching tile proxy | — | run prompt §10b; SOURCE-REGISTRY.md "Routing rule" |
| Video | **not NATS**: MediaMTX; URL in the entity `media[]` and KV `OVERWATCH_VIDEO` (omos `platform/nats/streams/kv-overwatch-video.yaml` line 9) | — | run prompt §10b; ingestion-platform.md "How it applies" 1 |
| Reference documents | Hermes knowledge store | — | SOURCE-REGISTRY.md "Routing rule" |

Payload limit: the live `max_payload` is 1 MiB (nats-platform.md §1). Anything larger goes to
the Object Store (chunked, digest-checked, P3) and travels on the bus as a reference.

---

## 3. Every subject OneMind uses

All patterns below appear in omos `CLAUDE.md` (grammar block) or omos `docs/NATS-DOCTRINE.md`
§2, §9.1 or §9.7. "Stream" is where the subject is stored in the target. Rows marked **P#**
depend on an owner proposal in section 10; until the owner rules, the subject is the
closest-fit placement (decision 0004 #8).

### 3.1 Sense (inputs)

| Subject | Publisher | Consumers | Stream | Notes and source |
|---|---|---|---|---|
| `tel.fleet.{class}.{id}.position` | MAVLink bridge, ROS bridge, site agents | entity service, ClickHouse | `TEL` | Doctrine §9.1; FEATURES A8; class ∈ `drone robot vehicle node edge agent aircraft vessel`. **BUS-P1** |
| `tel.fleet.{class}.{id}.status` | same; mesh gateway as `tel.fleet.edge.{id}.status`; site agent as `tel.fleet.node.{site}.status` | entity service, layer service | `TEL` | Doctrine §9.1; tak-server-and-bridges.md §6; edge lane's `tel.{node}.sys.{metric}` is remapped here (**BUS-P1**) |
| `tel.fleet.drone.{id}.sensor`, `tel.fleet.drone.{id}.footprint` | KLV extractor | entity service, client video drape | `TEL` | standards.md (MISB KLV row), video-and-plugins.md. Metric names beyond `position`/`status` need **BUS-P7** |
| `tel.{class}.{node}.{metric}` for environment classes (`soil weather water nature bio`) | MQTT gateway, Home Assistant bridge, collectors | entity service (sensor entities), ClickHouse | `ENV` | Class-first so `ENV` captures it (omos `env.yaml`). The MQTT proof's `tel.sim-s1.soil.moisture` and the collector proof's `tel.ws1.weather.reading` are **not** captured and become `tel.soil.sim-s1.moisture` / `tel.weather.ws1.reading`. **BUS-P1** |
| `tel.host.{host}.{metric}` | node exporters (existing) | ops | `SYS` | omos `sys.yaml` |
| `tel.feed.{source}.status` | every collector, every 10 s (`fetchedAt`, `count`, `lastError`, `stale`) | layer service, Grafana | `TEL` (add `tel.feed.>`) | ingestion-platform.md Rec. "Freshness"; today core-only from `feed-intel` (omos-live-state.md §3) |
| `det.{domain}.{type}.{id}` | onboard/edge detectors, Remote ID receiver (`det.air.remoteid.{id}`) | entity service (Stage 3 GNN) | `DET` (new) | Grammar; no stream binds `det.>` today (fusion-and-tracking.md F1). Off-grammar `det.cot.{id}` is not used |
| `intel.{domain}.{source}.{type}` | collectors (`omos-ingest`) | entity service (hazards, events), ClickHouse, timeline | `INTEL` | Grammar; new collectors use exactly four tokens with the id in the payload and `Nats-Msg-Id`. Live `feed-intel` subjects carry 5–6 tokens and stay unchanged until **BUS-P3** |
| `intel.space.celestrak.tle` | satellite collector | entity service (SGP4) | `INTEL` | ingestion-platform.md migration step 7 |
| `evt.world.air.{id}` | aircraft collector (AOI-bounded) | entity service | `WORLD` | Doctrine §9.1; ingestion-platform.md migration step 4. SOURCE-REGISTRY rows that say `intel.air.{source}.position` should follow this row (see open issue) |
| `evt.world.sea.{mmsi}` | AIS collector (AOI-bounded) | entity service | `WORLD` | Doctrine §9.1; ingestion-platform.md step 5 |
| `evt.world.{class}.{id}` for other moving world classes (rail, transit) | transit collector | entity service | `WORLD` | No doctrine class yet: **BUS-P6** (ingestion-platform.md step 8) |
| `tak.cot.out` | NatsBridgePlugin (TAK Server) | entity service | `COT` | Decision 0004 #7 option 1; omos `docs/NATS-DOCTRINE.md` §9.8 |
| `tak.cot.mesh.rx` | mesh gateway (site) | entity service | `SITE_COT` → hub `COT` | tak-server-and-bridges.md §6; FEATURES A7, C1 |

### 3.2 Make Sense (derived state)

| Subject or key | Publisher | Consumers | Store | Notes and source |
|---|---|---|---|---|
| `ent.{entity_type}.{entity_id}.state` | **only** the entity service (one writer per key; a site's entity service owns its own assets) | layer service, projector, ClickHouse, trusted clients, Hermes via `svc.entity.*` | `ENT` (new) | Grammar; lattice-patterns.md §A; `Nats-Msg-Id` = `{entity_id}:{seq}`; `Nats-TTL` per type. Type vocabulary is owned by `entity.md`/`ONTOLOGY.md`; note the projector's `COT_TYPE_MAP` uses `ship` where the Lattice lane uses `vessel` (fusion-and-tracking.md "Payload") |
| `evt.entity.{entity_id}.{event}` (`created deleted expired correlated decorrelated overridden conflict alert`) | entity service, watch jobs | timeline, Hermes, alerts | `EVT` (add `evt.entity.>`) | lattice-patterns.md §D; edge-and-ddil.md conflict rule 2. **BUS-P5** |
| `evt.map.geofence.{id}.{enter\|exit}` | geofence watcher (Tile38) | alerts, Hermes | `MISSION` | Doctrine §9.1, §9.7 |
| KV `world-state` `{layer}.meta`, `{layer}.cells.z{z}` | layer service | clients (through `svc.layer.*`), Hermes | KV | D3; [`layer-snapshot.md`](../contracts/layer-snapshot.md) |
| KV `fleet-state` `{site}` | hub site monitor (part of the layer service) | clients, UI site badge | KV (TTL 30 s, so a silent site expires to "disconnected") | D3; edge-and-ddil.md R4 "show per-site source lag" |

Not used: `ent.manual.upsert|delete` and `ent.air.{id}.deleted` from the undeployed
`om-overwatch` services (lattice-patterns.md §A verifier note) are outside `ent.*.*.state`;
deletes are a final `live:false` record with a short `Nats-TTL` (section 4.2). Off-grammar
`omos.air.*`/`omos.bridge.*` in the existing MAVLink bridge (drones-robots-agents.md
"Existing omos MAVLink bridge") move to the §3.1 subjects when that bridge adopts the
collector core. `evt.tak.cot.*` is not published by any OneMind service: `ENT` replaces it
(decision 0004 #7).

### 3.3 Act (commands, tasks, audit)

| Subject | Publisher | Consumers | Stream | Notes and source |
|---|---|---|---|---|
| `cmd.fleet.{class}.{id}.{verb}` (`goto rtl land arm disarm estop orbit`) | **only** the policy service, after policy, audit and human confirm | the owning bridge (core subscription, no replay) | `CMD` | Doctrine §9.1; FEATURES D3; run rule 6. **BUS-P2**. Published with a JetStream ack (the `CMD` PubAck is the audit receipt). Never sent as core request/reply, because `CMD` would answer the request with its own PubAck |
| `evt.drone.{id}.{accepted\|rejected\|completed\|failed}`, `evt.robot.{id}.…` | bridges, after verifying the command signature and expiry | policy service | `EVT` | Grammar `evt.{domain}.{entity}.{event}`; `EVT` binds `evt.drone.>`, `evt.robot.>` (omos `evt.yaml`) |
| `cmd.mission.{id}.{verb}` (`create waypoint abort complete`; `cancel` needs **BUS-P2**) | policy service | task service | `CMD` | Doctrine §9.1; lattice-patterns.md task table |
| `evt.mission.{id}.{state}` | task service, bridges (with `status_version`) | UI, Hermes | `MISSION` | lattice-patterns.md task table |
| `cmd.entity.{entity_id}.{override\|correlate\|decorrelate\|delete\|star}` | policy service | entity service | `CMD` | lattice-patterns.md §D |
| `cmd.map.{filter.set\|highlight.{uid}\|viewport.set\|geofence.{id}.{verb}\|annotation.{id}.{verb}\|mission.draw}` | clients, Hermes (reversible UI, level A1) | clients | `CMD` | Doctrine §9.7; agents-that-act.md approval table |
| `evt.so.action.{proposed\|approved\|denied\|executed\|failed}` | policy service (single audit record per decision) | UI, Hermes, audit | `EVT` | agents-that-act.md Rec. 2. The security lane's `evt.policy.{decision}.{result}` is folded into this subject, because `EVT` already stores `evt.so.>` and `evt.policy.>` is stored by no stream |
| `evt.sys.{source}.rejected` | collectors (dead-letter, with reason) | ops | `SYS` | Closest fit for the ingestion lane's `evt.feed.{source}.rejected`, which no stream binds (ingestion-platform.md Risks "Subjects"); `SYS` binds `evt.sys.>` |
| `tak.cot.inject` | **only** the projector (AOI/mission tier, marking check) | NatsBridgePlugin | `COT` | Grammar; FEATURES A6; security-and-trust.md T4 |
| `tak.cot.mesh.tx` | **only** the site projector | mesh gateway | `SITE_COT` | tak-server-and-bridges.md §6. Senders use a **core publish with no reply subject**, and delivery is confirmed by the gateway's `tx` counter on `tel.fleet.edge.{id}.status`, not a reply. The gateway answers any message with a reply subject with `ok`, which would race a JetStream PubAck on the same inbox (reasoned, not run; test in A7). `SITE_COT` still stores the message because it captures the subject ([cot-projection.md](../contracts/cot-projection.md) §6.3, proposal CP-3). Direction and capture under `tak.cot.>` go to the owner as CP-4 |

### 3.4 RPC services (`svc.{service}.{endpoint}`)

The endpoint may contain dots, as in the doctrine example `svc.meta.task.list`
(omos `docs/NATS-DOCTRINE.md` §2).

| Subject | Server | Callers | Source |
|---|---|---|---|
| `svc.entity.get`, `svc.entity.search`, `svc.entity.query`, `svc.entity.history` | entity service | clients, Hermes | agents-that-act.md tool table; lattice-patterns.md §D |
| `svc.layer.list`, `svc.layer.meta`, `svc.layer.snapshot`, `svc.layer.watch`, `svc.layer.unwatch` | layer service | clients, Hermes (`list`, `meta` only) | [`layer-snapshot.md`](../contracts/layer-snapshot.md), [`layer-delta.md`](../contracts/layer-delta.md) |
| `svc.policy.command.propose`, `svc.policy.task.propose` | policy service | clients, Hermes, watch jobs | The one naming used across the set (proposal RA2): `command` for asset commands and every other non-task action, `task` for tasks and missions ([asset-command.md](../contracts/asset-command.md) §1; [mission-task.md](../contracts/mission-task.md) §6). security-and-trust.md Rec. 2; replaces agents-that-act.md's `svc.command.propose`/`svc.task.propose` and lattice's `svc.policy.task.create` with one service |
| `svc.policy.command.confirm`, `svc.policy.task.confirm` | policy service | **human** clients only (agents denied) | run rule 6; security-and-trust.md §4 |
| `svc.policy.action.get` | policy service | clients, Hermes (own actions only) | look up one action by `action_id` ([asset-command.md](../contracts/asset-command.md) §1) |
| `svc.policy.check` | policy service | other services | security-and-trust.md §4 |
| `svc.cot.send` | projector (hub) and site projector | clients (C2 markers, routes, chat) | FEATURES C2; only the projector may publish `tak.cot.inject`/`tak.cot.mesh.tx` (security-and-trust.md T4) |
| `svc.registry.*`, `svc.plugins.list` | feed and plugin registries | clients, Hermes | source-registry-method.md; video-and-plugins.md |
| `svc.bridge.>` | Hermes (existing) | — | omos-live-state.md §7 |

### 3.5 Agents

| Subject | Notes and source |
|---|---|
| `agents.{verb}.{agent}.{owner}.{session}`, verbs `prompt hb status attachments` | Synadia Agent Protocol; omos `docs/NATS-DOCTRINE.md` §2; `AGENTS` stores `agents.hb.>`, `agents.status.>` only. Prompts are not stored; decisions are audited on `evt.so.action.*` |
| `$SRV.PING.agents`, `$SRV.INFO.agents` | Discovery (doctrine §2, §8); NATS system subjects |

### 3.6 Legacy subjects kept, not extended

`saas.>`, `voice.>`, `zeus.>`, `op.zeus.>`, `sys.>`, `agents.discover`, `agents.heartbeat`
(out-of-grammar stream subjects, nats-platform.md "Mapping onto omos" 1); `task.*` and
`evt.task.created` from `om-task` (lattice-patterns.md N4). OneMind adds nothing to them (**BUS-P4**).

---

## 4. Streams

### 4.1 Retention per stream (target CR values)

All streams: `storage: file`, `retention: limits`, `preventDelete: true`, R3 unless noted,
applied by `platform-nats-streams` (omos `CLAUDE.md`). Byte caps are per replica, so the sum
must fit one node's 45 GiB.

| Stream | Subjects | maxAge | maxBytes | discard | Other settings | Why |
|---|---|---|---|---|---|---|
| `ENT` (new) | `ent.*.*.state` | 7d backstop | 1 GiB | old | `maxMsgsPerSubject: 1`, `allowDirect: true`, `allowMsgTtl: true`, `subjectDeleteMarkerTtl: 15m`, `duplicateWindow: 2m`. Per-message `Nats-TTL`: aircraft 120 s, vessel 30 min, fleet asset 60 s, TAK EUD = CoT stale, geo up to 30 d; long-lived entities heartbeat at least every `min(stale/2, 2 min)` or 3.5 d for `no_expiry` | P10, P2c (history 1 avoids resurfacing); lattice-patterns.md §A expiry row; fusion-and-tracking.md "Rate control" |
| `COT` | `tak.cot.>` | **72h** | **3 GiB** | old | `Nats-Msg-Id` = CoT uid + time | 4.95 GB unread today (omos-live-state.md §2); 3.6 msg/s × ~1 KB ≈ 0.95 GB per 72 h (estimate) |
| `TEL` | today's 7 filters + **`tel.feed.>`** | **72h** | 4 GiB | old | `maxMsgsPerSubject: 2000`, `allowDirect: true` (latest feed status by Direct Get) | nats-platform.md Rec. §2; ingestion freshness |
| `DET` (new) | `det.>` | 7d | 2 GiB | old | `duplicateWindow: 2m` | fusion-and-tracking.md owner proposal 1 |
| `INTEL` | `intel.>` | 90d (keep) | **1 GiB** | old | `duplicateWindow: 1h`; `Nats-Msg-Id` = source event id | 104 MB in 86 days today (nats-platform.md §1); R10 dedup horizon |
| `WORLD` | `evt.world.>` | 24h (keep) | 2 GB (keep) | old | — | Already capped (omos `world.yaml`); becomes live for raw air/sea positions |
| `EVT` | today's 10 filters + **`evt.entity.>`** (BUS-P5) | 90d | 2 GiB | old | — | nats-platform.md Rec. §2 |
| `MISSION` | `evt.mission.>`, `evt.map.>` | 90d | 1 GiB | old | — | add cap |
| `CMD` | `cmd.>` | **365d** | 512 MiB | **new** | alert at 80% | Audit trail of every command (run rule 6). `discard: new` makes a full stream refuse the publish, so the policy service fails closed and no un-audited command goes out. This deviates from the NATS lane's `discard: old` on purpose |
| `GEO` | as today | **7d** | 512 MiB | old | — | No OneMind publisher (D2); retirement candidate (**BUS-P10**) |
| `AGENTS`, `SYS` | as today (`SYS` keeps `evt.sys.>`, so it stores dead letters) | 7d | 1 GiB each | old | — | add cap |
| `ENV` | as today | 30d (keep) | 1 GiB | old | — | add cap |
| `VOICE`, `SAAS`, `ZEUS` | as today | as today | 512 MiB each | old | — | add cap; legacy (**BUS-P4**) |

Per-node budget (all R3 streams count fully on every node): streams ≈ 21.4 GiB, object
buckets ≤ 9 GiB (section 6), KV buckets ≤ 2 GiB (section 5): **≈ 32.4 GiB of 45 GiB**,
leaving about 12 GiB for `reserved_storage` and growth. This is arithmetic from the caps, not
a measurement.

Prerequisites, in order:
1. Server 2.14.2 → 2.14.9 before relying on per-message TTL (stale-TTL fix in 2.14.8;
   nats-platform.md R9).
2. Confirm that the NACK CRD (chart `0.34.0`, omos `platform/argocd/edge-nack.yaml`) accepts
   `allowMsgTtl`, `subjectDeleteMarkerTtl` and `allowDirect`. **Unverified.** If it does not,
   `ENT` ships without per-message TTL: the entity service then ends an entity with the
   `live:false` record and relies on the 7d `maxAge` backstop, and clients use `expiry_t`.
3. Consumer budget: durables only for services (`entity-svc`, `layer-svc`, `projector`,
   `sink-clickhouse`, `policy-audit`) with `max_ack_pending` 1000 or lower; clients use ordered
   ephemeral consumers or Direct Get, never durables (nats-platform.md R15; doctrine §3).

### 4.2 Entity lifecycle on `ENT`

| Event | What the entity service does | What consumers see |
|---|---|---|
| Create or change | JetStream publish `ent.{type}.{id}.state`, `Nats-Msg-Id: {id}:{seq}`, `Nats-TTL: {type default}` | new message; projector and sinks get it on core too |
| Heartbeat | republish unchanged state at least every `min(stale/2, 2 min)` | refresh; `seq` unchanged allowed |
| Expire | nothing (TTL) | JetStream consumers get a delete marker (`Nats-Marker-Reason: MaxAge`); core subscribers do not (P10), so they must apply `expiry_t` themselves |
| Delete | publish a final record with `live:false` and `Nats-TTL: 60`; emit `evt.entity.{id}.deleted` | removal op |

---

## 5. KV buckets

| Bucket | Today (omos CR) | Target | Writer (single) | Readers |
|---|---|---|---|---|
| `world-state` | history 1, TTL 5m, never written | **layer manifests and cells** (`{layer}.meta`, `{layer}.cells.z{z}`), refreshed every 30 s; add `maxBytes: 64 MiB` | layer service | clients via `svc.layer.*`; trusted clients by KV watch |
| `fleet-state` | history 3, TTL 30s, never written | **site status** (`{site}`: connected, source lag per stream, last contact), refreshed every 10 s; add `maxBytes: 8 MiB` | hub site monitor | clients, UI |
| `ingest-locks` (new) | — | history 1, TTL 10s, 8 MiB | each collector, by CAS lease | collectors |
| `entity-correlation` (new) | — | history 5, 64 MiB; manual correlation and `decorrelated_from` | entity service | entity service |
| `feed-config` | R1, created by an app, excluded from Argo | keep; adopt only after matching `nats kv info feed-config` (omos `CLAUDE.md` "NATS Streams"). Feed registry source of truth stays Git `sources.yaml` | registry loader | collectors |
| `geofences` | history 10 | AOIs and geofences that drive `ent.zone.*` and the A6 filter | policy service (via `cmd.map.geofence.*`) | projector, layer service |
| `mission-state` | history 5 | task state, CAS on `status_version` | task service | UI |
| `OVERWATCH_VIDEO` (CR `kv-overwatch-video.yaml` line 9) | history 1 | stream registry (URL, codec, latency class) | video registry | clients |
| `task-index`, `task-notify`, `agent-caps`, `agent-config`, `agent-registry`, `device-registry`, `node-registry`, `om-config`, `om-meta-dedup`, `om-schema`, `om_ui_projections`, `projects`, `sys-health`, `webhook-registry`, `zeus-context`, `zeus-focus`, `zeus-presence` | as in omos | unchanged; add a `maxBytes` (8–64 MiB) to each in the caps PR | owners as today | — |
| `OVERWATCH_STATE`, `OVERWATCH_INTERCEPT` | om-overwatch (not deployed) | **not used by OneMind**; `OVERWATCH_INTERCEPT` is out of civilian scope | — | — |

Rules: `history: 1` wherever per-key TTL marks staleness (P2c). Human edits use
compare-and-set (`Update` with the expected revision), so an edit from a stale view fails
visibly with error 10071 (P2; edge-and-ddil.md conflict rule 3).

---

## 6. Object Store

| Bucket | Content | Replicas | maxBytes | Notes |
|---|---|---|---|---|
| `tiles` | offline PMTiles map packs, terrain, WebODM 3D Tiles | R1 (regenerable) | 4 GiB | edge-and-ddil.md Rec. 3; homestead pack about 4 MB, state at zoom 12 about 135 MB (E2) |
| `models` | glTF models, ML models | R3 | 2 GiB | nats-platform.md Rec. §1 |
| `datapkg` | TAK data packages | R3 | 1 GiB | tak-protocols.md |
| `imagery` | stills referenced by entities (`media[].object`) | R1 | 1 GiB | lattice-patterns.md §A `media` |
| `overlays` (new) | static layers (public camera catalog, boundaries) as PMTiles/GeoJSON, versioned | R3 | 1 GiB | SOURCE-REGISTRY.md "Routing rule"; FEATURES A16 |

Objects are chunked with a SHA-256 digest (P3). Sites mirror the `OBJ_*` streams they need.

---

## 7. Accounts, users and permissions

Stage 1 is config mode (doctrine §5) through an omos PR with SealedSecrets. Stage 2 is
operator/JWT with the full resolver, started at the doctrine trigger (more than about 20
nodes or the first non-family device). JWT issuance and scoped login were **not** proved
(nats-platform.md P9). Publisher identity is not a core NATS concept, so audited payloads
carry `source_id` (doctrine §5).

### 7.1 Accounts

| Account | Members | Exports / imports |
|---|---|---|
| `SYS` | ops, monitor | — |
| `ONEMIND` | all services, TAK leaf, site leafs (keeps every stream, no migration) | exports services `svc.layer.>`, `svc.entity.>`, `svc.policy.>`, `svc.cot.send`, `svc.registry.>`, `svc.plugins.list`; exports stream `cmd.map.>`; the watch export uses a streamed response type (**unverified**: P6 proved only single replies) |
| `CLIENT` | Unreal (remote), L0 web, Quest WebXR, ATAK plugin; browsers through Authentik and auth callout, never a shared password | imports the above; **no publish to any data subject**; no JetStream |
| `AGENTS` (stage 2) | Hermes, other harnesses | `agents.>`; imports `svc.entity.>`, `svc.layer.list`, `svc.layer.meta`, `svc.policy.*.propose`; stream `cmd.map.>` is publish-only for reversible UI |
| `PARTNER-<org>` | a sharing group | imports only the release service's filtered export (security-and-trust.md §5) |

### 7.2 Per-identity allow-lists (stage 1, inside `ONEMIND` unless noted)

`_INBOX.>` replies and the JetStream API calls each identity needs on its own streams and
buckets are granted per identity in the omos PR; they are omitted below for readability.

| Identity | Publish | Subscribe / serve |
|---|---|---|
| Collector group (`omos-ingest`, one user per group) | `intel.>`, `evt.world.>`, `tel.feed.>`, `evt.sys.*.rejected` | KV `ingest-locks`, `feed-config` |
| Push receivers (2+ replicas) | same as collectors | — |
| Entity service | `ent.*.*.state`, `evt.entity.>` | `tak.cot.out`, `tak.cot.mesh.rx`, `tel.fleet.>`, `tel.soil.>`… (ENV), `evt.world.>`, `intel.>`, `det.>`, `cmd.entity.>`; serves `svc.entity.>` |
| Layer service and site monitor | KV `world-state`, `fleet-state` | `ent.*.*.state` (ordered consumer), `tel.feed.>`; serves `svc.layer.>` |
| Policy service | `cmd.>`, `evt.so.action.>`; KV `geofences`, `mission-state` | `evt.drone.>`, `evt.robot.>`, `evt.mission.>`; serves `svc.policy.>`; OpenFGA over HTTP, **fail closed** |
| Projector (hub) | `tak.cot.inject` | `ent.*.*.state`, KV `geofences`, `mission-state`; serves `svc.cot.send` |
| TAK leaf (NatsBridgePlugin), own credential | `tak.cot.out` | `tak.cot.inject` |
| Release service | `PARTNER-*` exports | `ent.*.*.state`, `evt.>`, `intel.>` |
| `sink-clickhouse` | — | `intel.>`, `ent.*.*.state`, `tel.>`, `evt.>` |
| Hermes (own user and own leaf credential, then `AGENTS`) | `agents.>` (its own), `cmd.map.>` | `agents.prompt.hermes.>`; requests `svc.entity.>`, `svc.layer.list`, `svc.layer.meta`, `svc.policy.command.propose`, `svc.policy.task.propose`, `svc.policy.action.get` (own actions). **Denied**: every other `cmd.*`, `tak.cot.>`, `svc.policy.*.confirm`, raw `intel.>`/`tel.>` firehoses (prompt-injection surface, security-and-trust.md T1) |
| `CLIENT` users | requests only: `svc.layer.>`, `svc.entity.>`, `svc.policy.>` (confirm only for authenticated humans), `svc.cot.send`; `cmd.map.>` | `cmd.map.>` |
| Legacy (`feed-intel`, `feed-satellites`, Redpanda Connect, `onemind-core`) | today's subjects only, until each migrates (ingestion-platform.md migration plan) | today's subjects |

### 7.3 Site identities

| Identity | Where | Publish | Subscribe |
|---|---|---|---|
| Site leaf user `leaf-<site>` (one per site; no shared `leaf_tak`) | hub, enforced by the hub (security-and-trust.md fact 16) | that site's own subjects only (`tel.fleet.{class}.{site asset id}.>`, `tak.cot.mesh.rx`), plus JetStream API traffic for sourcing | `cmd.fleet.{class}.{site asset id}.>` enumerated per asset (NATS wildcards cannot match `sim-*` partially), `ent.*.*.state` for the site mirror |
| MAVLink bridge | site | `tel.fleet.drone.{its ids}.>`, `evt.drone.{its ids}.*` | `cmd.fleet.drone.{its ids}.>`; verifies the Ed25519 signature, nonce and `expires` locally |
| ROS 2 bridge | site | `tel.fleet.robot.{its ids}.>`, `evt.robot.{its ids}.*` | `cmd.fleet.robot.{its ids}.>` |
| Mesh gateway | site | `tak.cot.mesh.rx`, `tel.fleet.edge.{id}.status`; `tak.cot.inject` only when `MESH_TO_TAK_SERVER=true` (default `false`, decision 0004 #6) | `tak.cot.mesh.tx` |
| Site entity, layer and projector services | site | as their hub twins, limited to site-owned keys | as their hub twins |

Safety in this run: every asset id is `sim-*` (decision 0004 #13); the policy service rejects
any other id before OpenFGA is asked.

Transport hardening (security-and-trust.md §2; nats-platform.md R4): `verify: true` on hub
`leafnodes.tls`; sites verify the hub CA (no `tls insecure`); site WebSocket and MQTT
listeners bind to `127.0.0.1` (the live Mac config listens on all interfaces, omos
`infra/edge/mac-leaf/nats-server.conf`); JetStream limits per account (today all `-1`).

---

## 8. Leaf and JetStream-domain topology

```
                      HUB  (omos-cloud, 3-node cluster, JetStream domain "hub" — proposed)
   ENT  COT  TEL  DET  INTEL  WORLD  EVT  MISSION  CMD  SYS ...    KV / Object Store
     ▲ sources SITE_* via $JS.site-<name>.API          │ mirror (filtered) ENT, OBJ_*
     │                                                 ▼
 ┌───┴──────────── leaf (TLS, leaf-<site> credential) ─┴──────────────┐
 │ SITE  (domain site-<name>: site-mac, site-home, site-kit-1 …)      │
 │ SITE_TEL  SITE_EVT  SITE_DET  SITE_COT  SITE_ENT   HUB_ENT (mirror) │
 │ site entity svc · layer svc · projector · mesh gateway · bridges  │
 │ MediaMTX · pmtiles serve · local clients (Unreal on the Mac)      │
 └────────────────────────────────────────────────────────────────────┘
```

| Site stream | Subjects | Limits | Hub side |
|---|---|---|---|
| `SITE_TEL` | `tel.fleet.>` and ENV classes for site assets | 72h, 1 GiB, R1 | sourced into hub `TEL` |
| `SITE_EVT` | `evt.drone.>`, `evt.robot.>`, `evt.entity.>` for site keys | 72h, 1 GiB, R1 | sourced into hub `EVT` |
| `SITE_DET` | `det.>` | 72h, 1 GiB, R1 | sourced into hub `DET` |
| `SITE_COT` | `tak.cot.mesh.rx`, `tak.cot.mesh.tx` | 72h, 256 MiB, R1 | `tak.cot.mesh.rx` sourced into hub `COT` |
| `SITE_ENT` | `ent.*.*.state` for site-owned keys | as `ENT`, 256 MiB, R1 | sourced into hub `ENT` |
| `HUB_ENT` | mirror of hub `ENT`, filtered by entity types the site needs | 512 MiB, R1 | — |

Notes:
- A JetStream leaf without a domain extends the hub's JetStream (S4 in nats-platform.md).
  The live Mac config enables JetStream with no domain (omos `infra/edge/mac-leaf/nats-server.conf`;
  live effect **unverified**). Fix it first: domain `site-mac` (edge-and-ddil.md Rec. 2).
- Giving the hub the domain `hub` is a server config change in an omos PR; its effect on
  existing hub clients is **unverified** and must be tested on a local pair first.
- Mirrors and sources cannot use `FilterSubject` and `SubjectTransforms` together
  (nats-platform.md §3 verifier). `HUB_ENT` filters by entity type subjects only; there is no
  geographic filter in a subject, so the site layer service does AOI filtering.
- The live Hermes pod has its own `nats-leaf` sidecar (omos `CLAUDE.md`). Target: give it its
  own leaf credential and no JetStream (it is not a site).

---

## 9. Edge-to-cloud replication and conflict rules

Replication paths:
1. **Site → hub:** the hub sources `SITE_*` streams (merge pattern, doctrine §4). The site
   leaf remote sets `deny_exports` on the subjects it streams, so the same message does not
   also reach hub streams live over the leaf and land twice (overlap trap, P7b). Leaf
   `deny_exports`/`deny_imports` exist in server source but were **not exercised**
   (nats-platform.md §6).
2. **Hub → site:** the site mirrors hub `ENT` (filtered) into `HUB_ENT` and the object buckets
   it needs. With the cloud gone, the site keeps a working picture.
3. **Commands:** hub → site over the leaf only while connected; recorded in hub `CMD`. An
   offline site gets no command, and the UI shows the site as disconnected. Commands carry an
   `expires` (motion verbs default 30 s, an assumption to tune) and bridges drop expired
   ones. Durable tasks live in KV `mission-state`; after a reconnect a task is offered again
   only through a new human confirm (edge-and-ddil.md ER5).
4. **Lag:** catch-up took 7 s, about 16 s and 41 s in three local runs (edge-and-ddil.md ER4;
   cause unverified). The site monitor writes source lag to `fleet-state`; clients show
   "site syncing" when it is above 10 s.

Conflict rules (edge-and-ddil.md "Conflict rules"):

| Rule | Mechanism |
|---|---|
| One writer per key | Site-owned entities (for example `ent.drone.sim-x1.state`) are written only by the owning site's entity service. The hub never writes a site-owned key, also not while the site is partitioned. Shared objects (zones, missions) have one owning site; others propose changes as `svc.policy.*` requests, not direct writes |
| Origin time and version in every value | Each value carries `origin`, `ts` (origin clock) and a per-origin `seq`. A receiver applies a value only if `(ts, origin, seq)` is newer; otherwise it keeps the newer one and emits `evt.entity.{entity_id}.conflict` (**BUS-P5**). `Nats-Stream-Source` gives a second, clock-independent order per origin |
| Human edits | KV compare-and-set; stale edits fail visibly (P2) |
| Clock skew | Use GPS time where present; record receive-minus-origin skew in the entity; hybrid logical clocks are **unverified** for this run |
| No command replay | Section 9 item 3 |

---

## 10. Owner proposals (grammar gaps; decision 0004 #8)

None of these changes a live subject. Each becomes a short proposal file in omos `docs/`
(omos PR) for the owner. IDs here carry the prefix `BUS-`; a bare `P1`…`P10`, `P2c`, `P7b` or
`P7d` elsewhere in this file is a proof from nats-platform.md, not a proposal. Section 10.1 is
the one register of every owner proposal in the set.

| # | Gap | Evidence | Proposed ruling | Used until ruled |
|---|---|---|---|---|
| BUS-P1 | `tel.{node}.{class}.{metric}` (omos `CLAUDE.md`, example `tel.drone.x47.position`) vs `TEL`/`ENV`/`SYS` filters that treat the second token as a class, and doctrine §9.1 `tel.fleet.{class}.{id}.{metric}` | nats-platform.md "Mapping onto omos" 2; drones-robots-agents.md (`tel.sim-x1.drone.position` captured by no stream) | Second token is always a class from the stream filter list. Fleet assets use §9.1 `tel.fleet.{class}.{id}.{metric}`; environment sensors use `tel.{class}.{node}.{metric}`; infrastructure `tel.host.{host}.{metric}`; feed health `tel.feed.{source}.status`. No stream subject change except adding `tel.feed.>` to `TEL` | This ruling |
| BUS-P2 | Asset command shape: §9.1 `cmd.fleet.{class}.{id}.{verb}` (5 tokens) vs grammar `cmd.{domain}.{entity}.{action}` (security lane R10 prefers `cmd.drone.{id}.{verb}`); `cancel` is not in the §9.1 mission verbs | agents-that-act.md, security-and-trust.md R10, lattice-patterns.md task table | Ratify §9.1 for fleet assets (symmetric with `tel.fleet.*`, already in FEATURES D3, captured by `CMD`); add `cancel` to `cmd.mission.{id}.{verb}` | §9.1 form |
| BUS-P3 | Live `intel.*` subjects with 5–6 tokens (`intel.weather.noaa.alert.{state}.{event}` and others) | omos-live-state.md §3 | Ratify `intel.{domain}.{source}.{type}` plus optional trailing qualifiers for existing feeds; new collectors use four tokens | Keep live subjects |
| BUS-P4 | Out-of-grammar legacy subjects (`saas`, `voice`, `zeus`, `op.zeus`, `sys`, `agents.discover`, `agents.heartbeat`, `task.*`, `evt.task.created`, `omos.air.*`, `omos.bridge.*`, `ent.manual.*`, `ent.*.*.deleted`) | nats-platform.md; lattice-patterns.md; drones-robots-agents.md | Record as legacy; migrate the bridge and overwatch subjects when those services are next touched | Not extended |
| BUS-P5 | Domain `entity` for lifecycle and conflict events (`evt.entity.{entity_id}.{event}`) and its `EVT` binding | lattice-patterns.md §D; fusion-and-tracking.md proposal 4; edge-and-ddil.md rule 2 | Accept domain `entity`; add `evt.entity.>` to `EVT` in the same PR as the `ENT` CR | Not published until ruled; entity removal still works through `is_live:false` and markers |
| BUS-P6 | Moving world classes without a §9.1 subject (rail, transit) | ingestion-platform.md migration step 8 | Add `rail` and `transit` to the §9.1 world classes (`evt.world.rail.{id}`) | MTA stays on its current path (migrates last) |
| BUS-P7 | Fleet telemetry metrics beyond `position` and `status` (`sensor`, `footprint` for KLV) | standards.md; video-and-plugins.md | Allow any metric name in `tel.fleet.{class}.{id}.{metric}`, as the grammar line `{metric}` already does | Use `sensor`, `footprint` |
| BUS-P8 | AOI filtering cannot be done with subjects (no geographic token in the grammar) | this design | **Not proposed.** AOI filtering is done by `svc.layer.watch` (D7). Recorded so nobody adds a geohash token later without a ruling | — |
| BUS-P9 | Dead-letter subject | ingestion-platform.md Risks "Subjects" | Ratify `evt.sys.{source}.rejected` (stored by `SYS`) instead of `evt.feed.{source}.rejected` | `evt.sys.{source}.rejected` |
| BUS-P10 | Retire `GEO` and stop `evt.tak.cot.*` | Decision 0004 #7; D2 | Shrink `GEO` now; delete the CR after `onemind-core` stops subscribing (`preventDelete` keeps data either way) | Shrink only |
| BUS-P11 | Doctrine §4 says "the leaf node buffers during disconnection"; it does not without an edge stream | P7d; edge-and-ddil.md ER2 | Amend §4: buffering needs a site stream | — |


### 10.1 Combined proposal register (the list FEATURES A24 files)

Every grammar or stream gap found in M0, from all four design sources, with distinct prefixes.
One omos PR (FEATURES A24; ROADMAP row 5; OWNER-CHECKLIST §16 item 2) files a proposal for each
row that is not marked "information only". Duplicates are filed once, under the first ID.

| ID | Gap (short) | Defined in | Same as |
|---|---|---|---|
| BUS-P1 | Telemetry token order (class second); `tel.feed.>` in `TEL` | §10 above | ONT-P5, ONT-P6, RA4 |
| BUS-P2 | Asset command shape `cmd.fleet.{class}.{id}.{verb}`; `cancel` mission verb | §10 above | ONT-P4 |
| BUS-P3 | Live 5–6-token `intel.*` subjects | §10 above | — |
| BUS-P4 | Out-of-grammar legacy subjects | §10 above | CP-1 (recorded with it) |
| BUS-P5 | Domain `entity`, `evt.entity.>` in `EVT` | §10 above | ONT-P2 |
| BUS-P6 | `rail`, `transit` world classes | §10 above | — |
| BUS-P7 | Fleet metrics beyond `position`/`status` | §10 above | RA4 |
| BUS-P8 | No geographic subject token | §10 above | information only (not proposed) |
| BUS-P9 | Dead-letter subject `evt.sys.{source}.rejected` | §10 above | — |
| BUS-P10 | Retire `GEO`, stop `evt.tak.cot.*` | §10 above | decision 0004 #7 |
| BUS-P11 | Doctrine §4 leaf buffering wording | §10 above | — |
| ONT-P1 | `ENT` stream CR | [ONTOLOGY.md](ONTOLOGY.md) §9 | §4.1, §11 target |
| ONT-P3 | `DET` stream CR | ONTOLOGY §9 | §11 target |
| ONT-P7 | `CMD` 365 d retention for audit | ONTOLOGY §9 | §11 target |
| ONT-P8 | New `entity_type` vocabulary | ONTOLOGY §9 | — |
| ONT-P9 | New `cmd` domains `entity`, `team`, `watch`, `feed` | ONTOLOGY §9 | — |
| ONT-P10 | One audit subject `evt.so.action.{event}` | ONTOLOGY §9 | RA2 (audit half) |
| ONT-P11 | Asset tasks in the `mission` domain, not `evt.task.*` | ONTOLOGY §9 | — |
| ONT-P12 | Robot and vehicle CoT types | ONTOLOGY §9 | information only (settled in [robotics-cot-profile.md](../contracts/robotics-cot-profile.md)) |
| ONT-P13 | `evt.vehicle.>` not bound in `EVT` | ONTOLOGY §9 | — |
| ONT-P14 | Policy subject naming (four-token, settled in the set) | ONTOLOGY §9 | RA2 |
| RA1 | `evt.video.>` not bound in `EVT` | [REFERENCE-ARCHITECTURE.md](REFERENCE-ARCHITECTURE.md) §13 | — |
| RA2 | Policy names and audit subject used everywhere | REFERENCE-ARCHITECTURE §13 | ONT-P10, ONT-P14 |
| RA3 | Site upgrade as a command | REFERENCE-ARCHITECTURE §13 | information only (not proposed) |
| RA4 | KLV subject form | REFERENCE-ARCHITECTURE §13 | BUS-P1, BUS-P7 |
| CP-1 | `cmd.projector.disable\|enable` has three tokens | [cot-projection.md](../contracts/cot-projection.md) §11 | recorded with BUS-P4 |
| CP-2 | Gateway forwards OneMind's own CoT when `MESH_TO_TAK_SERVER=true` | cot-projection §11 | — |
| CP-3 | `tak.cot.mesh.tx` sender semantics (core publish, no reply) | cot-projection §11 | §3.3 aligned |
| CP-4 | `tak.cot.mesh.rx` / `tak.cot.mesh.tx`: inject direction under `tak.cot.>`, captured by `COT`/`SITE_COT` | cot-projection §11 | — |
| F1 | Grammar example `cmd.drone.x47.intercept` | [JADC2-MAP.md](JADC2-MAP.md) §6 F1 | — |

---

## 11. Current vs target, against `omos/platform/nats/streams/`

| CR file | Today (Git) | Target | Change |
|---|---|---|---|
| `agents.yaml` | `AGENTS`, 4 subjects, 168h, no cap | 7d, 1 GiB | cap |
| `cmd.yaml` | `CMD`, `cmd.>`, 720h, no cap | 365d, 512 MiB, `discard: new` | retention, cap, discard |
| `cot.yaml` | `COT`, `tak.cot.>`, 720h, no cap | 72h, 3 GiB | retention, cap |
| `env.yaml` | `ENV`, 5 `tel.*` classes, 720h | 1 GiB | cap |
| `evt.yaml` | `EVT`, 10 domains, 2160h | + `evt.entity.>` (BUS-P5), 2 GiB | subject, cap |
| `geo.yaml` | `GEO`, `evt.tak.cot.>`, `evt.tak.status.>`, 720h | 7d, 512 MiB; retire later (BUS-P10) | shrink |
| `intel.yaml` | `INTEL`, `intel.>`, 2160h, dedup 2m | 1 GiB, dedup 1h | cap, dedup |
| `mission.yaml` | `MISSION`, 2160h | 1 GiB | cap |
| `saas.yaml`, `voice.yaml`, `zeus.yaml` | legacy, 2160h / 720h / 8760h | 512 MiB each | cap |
| `sys.yaml` | `SYS`, 168h | 1 GiB | cap |
| `tel.yaml` | `TEL`, 7 filters, 168h | + `tel.feed.>`, 72h, 4 GiB, `maxMsgsPerSubject: 2000`, `allowDirect` | subject, retention, cap |
| `world.yaml` | `WORLD`, 24h, 2 GB | unchanged | none |
| `ent.yaml` | **does not exist** | new `ENT` (section 4.1) | new |
| `det.yaml` | **does not exist** | new `DET`, `det.>`, 7d, 2 GiB | new |
| `kv-world-state.yaml` | history 1, TTL 5m | layer manifests and cells; `maxBytes` 64 MiB | purpose, cap |
| `kv-fleet-state.yaml` | history 3, TTL 30s | site status; `maxBytes` 8 MiB | purpose, cap |
| `kv-ingest-locks.yaml` | **does not exist** | new, history 1, TTL 10s | new |
| `kv-entity-correlation.yaml` | **does not exist** | new, history 5 | new |
| `kv-feed-config.yaml` | R1, excluded from the Application | unchanged until matched to live (omos `CLAUDE.md`) | none |
| other 21 `kv-*.yaml` | as listed in section 5 | add `maxBytes` | cap |
| Object Store buckets | **none** in Git | `tiles`, `models`, `datapkg`, `imagery`, `overlays` (section 6); NACK `ObjectStore` CRD support **unverified** | new |

Other platform items (not stream CRs): per-service users and the `CLIENT` account
(SealedSecrets), separate Hermes and TAK leaf credentials, server 2.14.9, site leaf template
in omos `infra/edge/` (domain, site streams, `deny_exports`, verified TLS, `127.0.0.1`
listeners). Order follows nats-platform.md Rec. §6: caps first, then `ENT`/`DET` with the
entity service, then identities, then the server upgrade and site template.

---

## 12. Unverified items and risks carried into Phase 2

- NACK support for `allowMsgTtl`, `subjectDeleteMarkerTtl`, `allowDirect` and `ObjectStore`
  CRs (section 4.1 has the fallback).
- Cross-account JetStream and streamed service responses (`svc.layer.watch` to a `CLIENT`
  inbox); only single-reply service imports were proved (P6).
- Leaf `deny_exports` behaviour and the effect of adding a hub domain.
- The PubAck race on stream-captured request subjects (`cmd.*`, `tak.cot.mesh.tx`) is reasoned
  from how JetStream acknowledges any stored message with a reply subject; it was not run.
  The design avoids request/reply on those subjects either way.
- Batch Direct Get (doctrine §3 item 5 names it) was not proved locally; the contracts use
  ordered consumers or `svc.layer.snapshot` instead.
- Re-sync lag figures are single local runs.
- The `ENT` byte cap and per-type TTLs are estimates until the entity service runs against
  the `COT` and `INTEL` replays.
