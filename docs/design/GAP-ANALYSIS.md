# Gap analysis: OneMind against the reference systems

Status: M0 design, 2026-10-29. This page compares OneMind, capability by capability, with
Palantir, Anduril Lattice, JADC2, the TAK clients and the intel dashboards. For each gap it says
how OneMind closes it and when (feature ID and milestone).

**Read this first: OneMind has not built anything yet.** `docs/STATUS.md` shows M1–M7 not
started. Every "OneMind" cell below is a *design or a target*, not a shipped capability. The
claim "OneMind matches or beats" a competitor is allowed only after that feature's acceptance
test passes with evidence in `docs/evidence/` (run Rule 7), and any performance claim only after
the G4 benchmark ([competitive-matrix.md](../research/competitive-matrix.md) Recommendation 4).

Inputs: [competitive-matrix.md](../research/competitive-matrix.md) (nine competitors against
every FEATURES row, verified 2026-10-29), [palantir-patterns.md](../research/palantir-patterns.md),
[lattice-patterns.md](../research/lattice-patterns.md), [jadc2.md](../research/jadc2.md),
[tak-protocols.md](../research/tak-protocols.md),
[omos-live-state.md](../research/omos-live-state.md), [agents-that-act.md](../research/agents-that-act.md),
[security-and-trust.md](../research/security-and-trust.md),
[edge-and-ddil.md](../research/edge-and-ddil.md), [ONTOLOGY.md](ONTOLOGY.md),
[BUS-DESIGN.md](BUS-DESIGN.md), [JADC2-MAP.md](JADC2-MAP.md) and the contracts in
[`../contracts/`](../contracts/). Feature IDs are from `docs/FEATURES.md`.

## How to read the tables

| Column | Meaning |
|---|---|
| Capability | One thing a reference system does |
| Reference has (evidence) | What the reference system has, with its source. Palantir and Anduril facts come from their public docs as read by the research lanes; closed products (ARTAK, TAKX, WinTAK) come from marketing pages and are weaker evidence |
| OneMind today | One of: **none**; **designed** (in a design document or contract, not built); **partly live** (something runs in omos, with the source) |
| How OneMind closes it | The mechanism |
| Feature → milestone | Feature IDs and the milestone from `docs/ROADMAP.md`. If ROADMAP does not place the feature yet, the milestone comes from its priority (FEATURES header: P0 is M1–M3, P1 is M4–M5, P2 is M6, P3 later), written "M4–M5 (P1)" and so on |
| Bar to beat | What OneMind's acceptance test must meet or exceed |

**GAP-N#** marks a gap with no feature in `docs/FEATURES.md` yet. These are listed in §7 as
proposals for the FEATURES owner; this page does not edit FEATURES.

---

## 1. The baseline: what runs in omos today

The gaps below start from this verified live state, not from zero.

| Item | Live state | Source |
|---|---|---|
| Authority bus | NATS JetStream, 3 nodes; streams in `omos/platform/nats/streams/` | omos `CLAUDE.md`; [omos-live-state.md](../research/omos-live-state.md) §1 |
| Fused entities | **None.** No service publishes `ent.*.*.state`; no stream stores `ent.>`; KV `world-state` and `fleet-state` were never written (`last_seq 0`) | [omos-live-state.md](../research/omos-live-state.md) §4, §5 |
| Feeds | Only transit (`feed-mta`) runs through Redpanda Connect, publishing CoT straight to `tak.cot.inject`; aircraft and ships are off since 2026-08-27 | [omos-live-state.md](../research/omos-live-state.md) §4 |
| TAK | TAK Server 5.8 and FedHub pods run, but **the CoT path from the bus into TAK is broken**: nothing consumes `tak.cot.inject` into TAK and nothing publishes `tak.cot.out`; the NatsBridgePlugin is probably not in the running image | [omos-live-state.md](../research/omos-live-state.md) §6 |
| Hermes | Running, with monitoring and read-only tools over an MCP server; no entity or command tools | [omos-live-state.md](../research/omos-live-state.md) §7 |
| Policy | OpenFGA runs, but its model has only `user`, `group`, `capability#runner` and is used by Backstage only | [palantir-patterns.md](../research/palantir-patterns.md) O-d; [ONTOLOGY.md](ONTOLOGY.md) §7.2 |
| Video | MediaMTX is deployed in `omos/platform/onetak/`; CloudTAK shows it "Not connected"; the Hermes adapter is disabled (HTTP 401) | [video-and-plugins.md](../research/video-and-plugins.md) §A; [omos-live-state.md](../research/omos-live-state.md) §7 |
| NATS identities | All application traffic runs in one account on shared passwords | [ONTOLOGY.md](ONTOLOGY.md) §7.1, citing [nats-platform.md](../research/nats-platform.md) §1 |

---

## 2. Palantir (Foundry ontology, Gotham, AIP, Apollo, Maven Smart System)

Learn the patterns, copy no code, names or branding ([palantir-patterns.md](../research/palantir-patterns.md) Risk 7).

| Capability | Palantir has (evidence) | OneMind today | How OneMind closes it | Feature → milestone | Bar to beat |
|---|---|---|---|---|---|
| Object types and interfaces | Object types with backing datasources; interfaces implemented by many types (F2, F4) | designed: 19 object types and 4 traits ([ONTOLOGY.md](ONTOLOGY.md) §3–§4); schemas planned in omos `services/schemas/entity/` | Entity service publishes typed records on `ent.{entity_type}.{entity_id}.state` into the `ENT` stream (`omos/platform/nats/streams/ent.yaml`, proposed) | A4, A5 → M1 | Same aircraft from two feeds gives one entity (A4 test) |
| Link types and traversal | Bidirectional links with a named side per type; many-to-many backed by their own datasource (F3) | designed: 17 link types with inverse sides kept by the owner ([ONTOLOGY.md](ONTOLOGY.md) §5) | `relationships[]` on the record; link facts on `evt.entity.{id}.linked` (`evt.yaml` binding proposed, BUS-P5) | A4 → M1 | One read per hop |
| Link graph view | Gotham-style graph of an object's links (description **unverified**, [palantir-patterns.md](../research/palantir-patterns.md) §6) | none | Graph panel in the inspector over `svc.entity.get` with links | **GAP-N1** → M6 (P2) | Open an entity and walk two hops of links |
| Actions as the only write path, with an action log | Action types with parameters, criteria, side effects; action log omits failed submissions (F5–F8) | designed: action catalogue and approval levels A0–A5 ([ONTOLOGY.md](ONTOLOGY.md) §6; [asset-command.md](../contracts/asset-command.md)) | Every edit is `svc.policy.*.propose` → policy → human confirm → `cmd.*` (`cmd.yaml`); audit on `evt.so.action.*` (`evt.yaml`) **including denials and failures**, which Palantir's log omits (F8) | I4 → M1; D3, D5 → M4 | Beat: failures and denials are audited (F8 does not log them) |
| Writeback with per-property strategy | "User edits win" or "most recent wins" (F10) | designed: `override_wins` / `newest_wins` per property ([ONTOLOGY.md](ONTOLOGY.md) §3.2) | `cmd.entity.{id}.override` applied by the entity service | A4 → M1 | A telemetry update beats an old manual position edit; a manual name survives |
| Functions (reads) | Server-side TypeScript or Python functions (F9) | designed: `svc.entity.*`, `svc.fusion.explain`, `svc.layer.*` ([ONTOLOGY.md](ONTOLOGY.md) §6.3) | NATS request/reply services | I2 → M4 | Hermes answers a spatial question using only bus tools (I2 test) |
| Scenarios (stage changes before commit) | Proposed changes packaged into a sandboxed subset (F11) | partial design: what-if forecast (D12) and rehearsal in sim (H7) | What-if runs are labelled and never publish `cmd.*` (JADC2-MAP SL7); mission rehearsal in the twin | H7 → M5; D12 → later (P3) | A rehearsed mission replays on assets unchanged (H7 test) |
| Operational picture: plan, execute, report | Gotham hands off from analysts to operators (G-1) | designed: 3D COP from snapshots plus deltas | Layers, missions, incident board, Hermes briefs | B4–B6 → M2; D2 → M4; D8 → M6 (P2); D10 → M4–M5 (P1) | A mission created in the client shows on ATAK (D2 test) |
| AI agents with tools, staged for review | AIP tools (action, object query, function, …); edits "applied automatically or staged for human review" (A2, A3) | designed: Hermes gets query, function and proposal tools only; agents never confirm ([agents-that-act.md](../research/agents-that-act.md) Rec. 1–3) | Hermes requests `svc.entity.*`, `svc.layer.*`, `svc.policy.*.propose`; denied every other `cmd.*` ([BUS-DESIGN.md](BUS-DESIGN.md) §7.2) | I2 → M4; D4, D5 → M4; I4 → M1 | Beat: every asset command needs a human confirm even in simulation (Rule 6); AIP allows auto-run |
| AI evaluations | AIP cites evaluations and audit trails (A1) | none | Tool evals, safety-invariant tests, injection corpus, impersonation tests on the local stack ([agents-that-act.md](../research/agents-that-act.md) Rec. 6) | **GAP-N2** → M1 (with I4) | Every stop-line red-team test in [JADC2-MAP.md](JADC2-MAP.md) §5.3 passes |
| Agents earn trust | Agents start with limited access and earn more (A4) | designed: `trust_level` on agent entities ([ONTOLOGY.md](ONTOLOGY.md) §3.3 `agent`) | Promotion recorded in KV `agent-registry` (`kv-agent-registry.yaml`) as an owner decision | I5 → M6 | New agent starts read-only |
| Apollo: hub and spoke, channels, constraints, air-gapped | Pull model with release channels; product and environment constraints; disconnected installs (AP1–AP6) | partly live: omos Argo CD for the cloud only | Site agent on each site reporting on `tel.fleet.node.{site}.status` (`tel.yaml`); channels as Git refs or digests; change windows; signed offline bundles; Zarf for the home server ([edge-and-ddil.md](../research/edge-and-ddil.md) Rec. 4) | **GAP-N3** → M4–M5 (P1); F9 → M4–M5 (P1) | A site refuses an upgrade during an active mission |
| Maven-style detection and workbench | CV that detects, tags and tracks objects; data fusion "for AI-enabled battle management" (MSS-2, MSS-3) | none | Civilian version only: `det.*` from owned detectors (`det.yaml`, proposed) fused into tracks; alert → mission workbench | H8 → M6; D6, D2 → M4 | Detected vehicle appears as an entity (H8 test). **Stop:** no targeting workflow (JADC2-MAP SL1–SL3) |
| SDK with scoped tokens | Ontology SDK scoped to chosen entities plus user permissions (F12) | none | REST with OpenAPI, MCP server and CLI over `svc.*` with scoped tokens | F10 → M6 (P2) | A token without scope is refused and logged (F10 test) |
| Scale of users | MSS: 20,000+ users across 35+ tools (MSS-1) | — | Not a goal. OneMind targets a household, a neighbourhood and a team | — | — |

---

## 3. Anduril Lattice (entities, tasks, autonomy, mesh)

Design only; Lattice field names are renamed ([lattice-patterns.md](../research/lattice-patterns.md) R1; [ONTOLOGY.md](ONTOLOGY.md) §10).

| Capability | Lattice has (evidence) | OneMind today | How OneMind closes it | Feature → milestone | Bar to beat |
|---|---|---|---|---|---|
| Entity as a bag of components with provenance, aliases, expiry | Required `entity_id`, `is_live`, `expiry_time`, provenance; updates accepted only with newer source time (L3, L4) | designed ([entity.md](../contracts/entity.md); [ONTOLOGY.md](ONTOLOGY.md) §3.2) | `ENT` stream, one message per subject, per-message `Nats-TTL` ([BUS-DESIGN.md](BUS-DESIGN.md) D2, §4.2) | A4 → M1 | Beat: per-source `sources[]` with stream pointers and decomposed confidence, not only one provenance block |
| Overrides on overridable fields | Last writer wins; override status recorded (L4, L5) | designed | `cmd.entity.{id}.override` (`cmd.yaml`); recorded in `overrides[]` | A4 → M1 | Override survives the next source update when `override_wins` |
| Correlation with manual over automated precedence | Correlation sets; explicit decorrelation beats the automatic correlator (L4) | designed | `cmd.entity.{id}.correlate`/`.decorrelate`; KV `entity-correlation` (`kv-entity-correlation.yaml`, proposed) | A4 → M1 | An operator's "not the same" holds against the fuser |
| Tracks for keyless detections | `tracked{quality, hits, …}`, covariance (L4); algorithm not public (R9) | designed: GNN with chi-square gating, Kalman/IMM ([fusion-and-tracking.md](../research/fusion-and-tracking.md) Recommendation) | Entity service stage 3 over `det.*` (`det.yaml`, proposed) | A4 → M1 (stages 0–2); H8 → M6 (detections) | OSPA/GOSPA scored against Stone Soup in CI |
| Tasks with typed specs and a state machine | Typed task specs, status enum, `status_version`, terminal states immutable (L7–L10) | designed ([mission-task.md](../contracts/mission-task.md)) | Task records in KV `mission-state` (`kv-mission-state.yaml`), facts on `evt.mission.{id}.{state}` (`mission.yaml`) | H4 → M4 | One task assigned to a drone completes and reports (H4 test) |
| Capability gate | Assign only if the asset's `task_catalog` advertises the type (L8) | designed: `capabilities[]` | Policy checks `capabilities` before propose | H4 → M4 | Denied with `not_capable` ([mission-task.md](../contracts/mission-task.md) test 3) |
| Autonomy loop | Rule on an entity stream overrides a track and creates an `Orbit` task automatically (L14) | designed ([lattice-patterns.md](../research/lattice-patterns.md) §F) | Watch job proposes; a human confirms; task service dispatches `cmd.fleet.*` | I3, H4, D5 → M4 | Civilian change: no automatic task to an asset, no "suspicious" disposition for person tracks (JADC2-MAP SL2, SL6, SL9) |
| Mission autonomy (intent to tasks across assets) | Press only (L16) | designed: composite tasks split into child tasks, each approved | Task service decomposition; Hermes as planner | H5, I5 → M6 | Three simulated drones search a drawn area (H5 test) |
| Mesh: local-first, live first, backfill | Live data priority, store-and-forward cache, gossip of a signed asset DB (L2, L15) | designed: per-site JetStream domains, hub sources site streams ([BUS-DESIGN.md](BUS-DESIGN.md) §8–§9) | Site streams `SITE_*`, `HUB_ENT` mirror; site leaf template in omos `infra/edge/` (proposed) | A7 → M1; F9 → M4–M5 (P1) | Site keeps a working picture with the hub gone (F9 test) |
| Object distribution | Every node runs a CDN, files under 1 GiB with SHA-256 (L11) | designed: Object Store buckets `tiles`, `models`, `datapkg`, `imagery`, `overlays` ([BUS-DESIGN.md](BUS-DESIGN.md) §6) | Sites mirror the object streams they need | E1 → M5; C5 → M6 | Homestead tileset loads offline on a site |
| Short-lived identities | JWTs refreshed from the hub; group keys live tens of minutes (L15) | none (shared passwords today) | Stage 1 per-service users; stage 2 operator/JWT ([BUS-DESIGN.md](BUS-DESIGN.md) §7) | **GAP-N5** → M1 | A client user cannot publish `cmd.fleet.>` ([asset-command.md](../contracts/asset-command.md) test 8) |
| Developer SDK and plugins | Public SDK and docs | designed: plugin model ([video-and-plugins.md](../research/video-and-plugins.md) "Plugin model") | Manifest plus source, layer, action and skill plugins | K6 → M3; K7 → M6 (P2) | A new layer ships as a plugin with no core change (K6 test) |
| Weapons components | `target_priority`, `supplies.munitions`, `ENGAGEMENT_ZONE`, `active_target` (L4) | — | **Deliberately not built** | — | JADC2-MAP SL1, SL2 |

---

## 4. JADC2 (the loop, fabric, federation, zero trust, DDIL)

The full mapping, with tiers and stop lines, is in [JADC2-MAP.md](JADC2-MAP.md) §2. This table
lists only the gap and the closing feature.

| JADC2 capability (source) | OneMind today | How OneMind closes it | Feature → milestone |
|---|---|---|---|
| Sense from all sources (S1 p. 4) | partly live: one transit feed, wrong subject ([omos-live-state.md](../research/omos-live-state.md) §4) | Pipeline standard and feeds on grammar subjects; fleet telemetry; source plugins | A0–A3 → M1; A8, A11, A13 → M4–M5 (P1); A15 → M6 (P2) |
| Make Sense: fuse and render (S1 p. 4) | none (no entity publisher) | Entity service, layer snapshots, client | A4, A5 → M1; B4–B6 → M2 |
| Make Sense: AI (S1 p. 5; S11) | partly live: Hermes with read-only monitoring tools | Ontology tools, watch jobs, audit | I2, I3 → M4; I4 → M1 |
| Act: decide and disseminate (S1 p. 5) | none (TAK path broken) | Policy service, missions, tasks, CoT projection | A6 → M1; C2 → M2; D1–D3 → M4 |
| Mission command and bundled authorities (S1 p. 5, LOE 2) | none | Watch jobs with envelopes ([JADC2-MAP.md](JADC2-MAP.md) §4) | I3 → M4 |
| Data fabric and VAULTIS (S1, S8) | partly live: bus and streams; no catalogue in KV | [BUS-DESIGN.md](BUS-DESIGN.md); feed registry | A2, A5 → M1 |
| Mission partner environment (DoDI 8110.01) | partly live: TAK Server and FedHub run ([tak-protocols.md](../research/tak-protocols.md) Level 3) | Release service, `PARTNER-<org>` accounts, expiring incident exports, TAK group mapping | **GAP-N4** → M4–M5 (P1); C7 → M4–M5 (P1) |
| Zero trust, seven pillars (S4) | partly live: Tailscale, OpenFGA, Kyverno, Falco in omos `platform/security/` | Per-service identities, OpenFGA in the command path, markings, signed supply chain | GAP-N5 → M1; I4 → M1; J2 → M4; **GAP-N6** → M7 |
| DDIL (S1 principle 4; S9; S10) | none (Mac leaf has no JetStream domain, [BUS-DESIGN.md](BUS-DESIGN.md) §8) | Site domains and streams; mesh gateway; offline tiles; local model; low-bandwidth radio | A7 → M1; B2 → M2; F9 → M4–M5 (P1); **GAP-N7** → M6 (P2) |
| Faster delivery (S1; S3) | partly live: Argo CD for the cloud | Site agent and channels | GAP-N3 → M4–M5 (P1) |
| Training and exercises (S1 LOE 2) | none | Simulation, rehearsal, synchronised briefing | H7 → M5; F8 → M6 (P2) |
| Fires, targeting, NC3 (S1 LOE 3–4; S2) | — | **Deliberately not built** | [JADC2-MAP.md](JADC2-MAP.md) §2.5 |

---

## 5. TAK clients (ATAK family, WinTAK, TAKX, CloudTAK, ARTAK)

Evidence: [competitive-matrix.md](../research/competitive-matrix.md) matrix and per-competitor
facts. CloudTAK is AGPL-3.0 and design-only for OneMind. ARTAK, TAKX and WinTAK are closed; rows
marked "?" there may hide capabilities.

| Capability | Reference has (evidence) | OneMind today | How OneMind closes it | Feature → milestone | Bar to beat |
|---|---|---|---|---|---|
| **Prerequisite: bus to TAK path** | — | **broken** in omos ([omos-live-state.md](../research/omos-live-state.md) §6) | omos issue or PR: run the plugin overlay image or fix `tak-nats-leaf` routing (owner merges) | prerequisite of A6, C1 → M1 | Hub `leafz` shows a `tak.cot.inject` subscription; `COT` gains `tak.cot.out` |
| Mesh peer (SA multicast, directed TCP, TAK Protocol v1) | TAKX, WinTAK native (matrix A7) | designed; gateway on omos branch `feat/tak-mesh-gateway`, no PR yet | Finish the gateway PR: GeoChat, mesh encryption, data packages ([tak-protocols.md](../research/tak-protocols.md) Rec. 2) | A7 → M1; C4 → M6 | ATAK on the LAN appears on `tak.cot.mesh.rx` (A7 test) |
| TAK entities and sending CoT | All TAK clients (C1, C2) | none | Entities from `tak.cot.out` and `tak.cot.mesh.rx` (`cot.yaml`); send through `svc.cot.send` | C1, C2 → M2 | A marker from the client appears on ATAK (C2 test) |
| Being a TAK contact | CloudTAK, TAKX, WinTAK (C3) | none | TAK streaming connection with client certificates | C3 → M6 | The client shows in ATAK's contact list |
| Data Sync missions | CloudTAK (subscribe, layers, files, logs) | none | `tak-client` bridge syncs missions to KV and `evt.mission.{name}.changed` ([tak-protocols.md](../research/tak-protocols.md) Rec. 1) | C7 → M4–M5 (P1) | A feature added in ATAK appears in the client, and the reverse |
| Contacts, chat, channels | CloudTAK, TAK clients | none | Chat as CoT; channel = TAK group | C8 → M4–M5 (P1) | Chat works both ways |
| Data packages | CloudTAK, TAKX, WinTAK | none | Object Store `datapkg` ([BUS-DESIGN.md](BUS-DESIGN.md) §6) | C5 → M6 | A data package from ATAK opens in the client |
| Voice | ARTAK integrated voice; Mumble runs in omos TAK overlay | partly live: Mumble/murmur pods ([omos-live-state.md](../research/omos-live-state.md) §6) | Mumble push-to-talk in the client | C6 → M6 | Talk to an ATAK user |
| Symbology | ARTAK 2525E; TAK clients | none | CoT-type and 2525 symbology | B7 → M6 | Team members show correct symbols |
| Drawing and tactical graphics | CloudTAK full set (point to lasso, buffer, rings) | none | Shapes shareable as CoT (`u-d-*`, `b-m-r`) | B11 → M4–M5 (P1) | Each shape appears on ATAK with the same geometry |
| Measurement, search, MGRS | CloudTAK; TAK clients | none | Port VTAK_Mensuration; `geo:` and MGRS | B9, B10 → M6 | Within 1% of ATAK (B9 test) |
| Point query | CloudTAK Query Mode | none | Elevation, address, NWS weather, sun, declination | B12 → M6 (P2) | Matches NOAA and DEM references |
| Elevation analysis | TAK clients (ATAK guide; parity **unverified** for TAKX and WinTAK) | none | Viewshed, profile, contours | B19 → M6 (P2) | Within 5% of `gdal_viewshed` |
| Import and export | CloudTAK; TAK clients | none | GeoJSON, KML/KMZ, GPX, CSV, Shapefile, GeoPackage, PMTiles | B15 → M4–M5 (P1) | Round trip keeps geometry and names |
| Basemaps and overlays | CloudTAK ZXY, TileJSON, ArcGIS, PMTiles | none | Caching tile proxy, never through NATS | B16 → M4–M5 (P1) | Each source kind renders; repeat views come from cache |
| Routing | CloudTAK roads and trails with ETA | none | OSM routing usable as a mission route | D7 → M6 (P2) | Route follows OSM roads and sends to ATAK |
| Incident boards and forms | CloudTAK `core-event*`, `core-form*` | none | Incidents on `evt.*` with assignments and forms | D8 → M6 (P2) | Assignee and form visible in the audit log |
| Geofences driving what TAK sees | CloudTAK geofence connections (P) | designed: KV `geofences` (`kv-geofences.yaml`) drives the projector | AOI filter in the projector | D1 → M4; A6 → M1 | Drawing an AOI changes what TAK receives |
| Video in TAK | CloudTAK leases and WHEP; ARTAK 8 feeds | partly live: MediaMTX deployed, not connected | Ingest, stream registry in KV `OVERWATCH_VIDEO`, URLs in CoT | K1–K4 → M3 | ATAK opens the drone stream from its marker (K3 test) |
| KLV footprint | ATAK family "Viewing KLV" (**unverified** for TAKX, WinTAK) | none | `tel.fleet.drone.{id}.footprint` (`tel.yaml`; metric names need BUS-P7) | K5 → M6 (P2) | Footprint matches the camera view |
| Robots and drones as tracks | ARTAK air, ground, underwater; CloudTAK DJI, Skydio, BRINC ETL | designed ([ONTOLOGY.md](ONTOLOGY.md) drone, robot) | MAVLink and ROS 2 bridges; robotics CoT profile | H1, H2, H9 → M1; H3 → M4 | SITL drone shows with battery and mode on ATAK |
| Drone command with audit | None publicly (matrix D3) | designed ([asset-command.md](../contracts/asset-command.md)) | Policy service, human confirm, signed commands | D3 → M4; H6, I4 → M1 | **Beat:** no competitor shows commands with policy and audit |
| 3D maps from drone photos, offline | ARTAK Map Maker about 5.5 km², works without a network | partly live: WebODM in omos `platform/onetak` (per FEATURES E1) | WebODM to 3D Tiles; offline on the home server | E1 → M5; E3 → M6; E6 → M6 (P2) | Model aligns with terrain within 1 m (E1 test) |
| Interiors and subterranean | ARTAK | none | 3D Tiles or glTF interiors | E8 → later (P3) | One interior aligns with its exterior |
| XR headset, synced multi-user | ARTAK on Quest 3, "every headset in the room holds the same picture" | none | Quest Pro (decision 0003); multi-user; synchronised briefing | F4 → M5; F6 → M6; F8 → M6 (P2) | Two clients within 200 ms of the presenter (F8 test) |
| Disconnected operation | ARTAK "full operational capability in disconnected, austere environments"; TAKX "No Internet Connection Required" | none | Whole stack on the Mac or home server | F9 → M4–M5 (P1) | Uplink off: entities, tiles, terrain and Hermes all work |
| Roles and identity | CloudTAK LDAP, SCIM, passkeys | partly live: Authentik manifests in omos `platform/auth/` | Role modes from Authentik | J2 → M4 | Newcomer cannot see asset commands |
| Mounted and touch mode | TAKX Mounted Support | none | Large targets, hardware buttons | J7 → later (P3) | Every primary action by touch or buttons |
| RF survey | TAKX-RF, Network Survey | none | Receive-only survey layer `det.rf.*` | H10 → later (P3) | Survey file shows as a heat layer. **Stop:** receive only (JADC2-MAP SL4) |
| Plugin inside ATAK | ATAK plugin SDK; WinTAK MEF plugins | none | `OneMindATAK` plugin on the 5.6.0 API; ordinary CoT plus OneMind XML detail ([tak-protocols.md](../research/tak-protocols.md) Level 4) | **GAP-N9** → M6 (P2) | ATAK shows OneMind provenance for an entity |
| Federation | TAK Server federation (gRPC `FederatedChannel`) | partly live: FedHub 5.8 runs, ports not exposed | Configuration only: group mapping and partner CAs; no own `FederatedChannel` ([tak-protocols.md](../research/tak-protocols.md) Rec. 3) | GAP-N4 → M4–M5 (P1) | A partner federate receives only the `OneMind-COP` group |
| Weapon inventory | ARTAK 200+ U.S. systems with range and engagement data | — | **Deliberately not built** | — | JADC2-MAP SL1 |

---

## 6. Intel dashboards (worldmonitor, osiris, worldwideview, gods-eye-view, Argos Atlas)

Evidence: [competitive-matrix.md](../research/competitive-matrix.md). worldmonitor (AGPL-3.0)
and worldwideview (Elastic 2.0) are design-only; gods-eye-view and osiris (MIT) may be reused
with their notice, code only, not bundled datasets.

| Capability | Reference has (evidence) | OneMind today | How OneMind closes it | Feature → milestone | Bar to beat |
|---|---|---|---|---|---|
| Collectors separate from the viewer, stale-on-error | worldmonitor seed scripts, last good value kept, circuit breakers | partly live: Redpanda Connect, one feed | Pipeline standard with the full reliability list and chaos test | A0 → M1 | Chaos test loses and duplicates nothing (A0 test) |
| Feed freshness and "intelligence gaps" | worldmonitor fresh/stale/very_stale for about 40 sources | none (`tel.feed.*` published but not stored, [ONTOLOGY.md](ONTOLOGY.md) ONT-P6) | Feed health on `tel.feed.{source}.status` (`tel.yaml`, binding proposed); gaps panel | A3 → M1; B20 → M4–M5 (P1) | A stopped feed shows within its limit, naming the weakened layer |
| One-call bootstrap | worldmonitor 38 datasets in two requests | none | `svc.layer.snapshot` from KV `world-state` (`kv-world-state.yaml`) | A5 → M1 | A full layer in one read |
| World-tier aggregation | worldmonitor 2°×2° cells, max 5,000, 20,000 vessel cap | none | Density cells in KV `world-state` keys `{layer}.cells.z{z}` | A14 → M4–M5 (P1) | 50,000-vessel fixture as ≤ 5,000 cells (A14 test) |
| Rendering at scale | gods-eye-view 11,575 objects at 57.6 fps (one static scene); worldwideview "10k+ at 60 fps" (unbenchmarked) | none | Batched instancing in Unreal; L0 client on the same contracts | B4, B5 → M2; G4 → M4–M5 (P1) | Native ≥ 60 fps; L0 ≥ 57.6 fps; L0 cold start ≤ 1.86 s (G4) |
| Smooth motion and follow | gods-eye-view interpolation, dead reckoning, true headings, cockpit camera | none | Render one interval behind with interpolation (design only) | B13, B14 → M4–M5 (P1) | 15 s cadence moves without jumps |
| Layer breadth | worldmonitor about 30 layer keys; worldwideview 29 plugins; gods-eye-view 20 layers | partly live: feeds in omos `INTEL` | Source plus layer plugins for every civilian domain | A15 → M6 (P2); K6 → M3 | The matrix layer-coverage list is all present |
| Cameras at scale | Argos Atlas 212,612 cameras; osiris 17,000+; gods-eye-view 3D-projected CCTV | none | Catalog as entities; latest low-resolution frame through a caching proxy, never NATS, no history | A9 → M4–M5 (P1); A16 → M6 (P2); E4 → M6 | 200,000 records load clustered in under 2 s. **Stop:** no recognition, no ALPR (JADC2-MAP SL3) |
| News and OSINT text | worldmonitor tiered feeds; osiris Telegram geoparsing | none | `intel.news.{source}.item` (`intel.yaml`) with tier and licence in the registry | A11 → M4–M5 (P1) | Same story from two outlets shows once |
| Satellites | worldmonitor, worldwideview, gods-eye-view SGP4 | partly live: `feed-satellites` publishes CoT ([omos-live-state.md](../research/omos-live-state.md) §4) | `intel.space.celestrak.tle` (`intel.yaml`) propagated by the entity service | A12 → M6 (P2) | ISS within 5 km of a reference |
| Weather and hazards | gods-eye-view wind, radar, lightning, cyclone cones | partly live: `intel.weather.noaa.alert.*` ([standards.md](../research/standards.md) L5) | Weather layers and CAP polygons | A13 → M4–M5 (P1) | "Storm watch" preset shows radar, wind and an NWS polygon |
| Voice agent that drives the map | gods-eye-view 30 tools; osiris OI Assist | partly live: Hermes, no UI tools | Hermes UI tools via `cmd.map.>` (`cmd.yaml`; doctrine §9.7) | D4, D11 → M4 / M4–M5 (P1) | Ten scripted requests each produce the right view change |
| AI briefs and anomaly detection | worldmonitor World Brief, Welford baselines, focal points | none | Briefs that name stale feeds; local model offline; convergence detection | D10 → M4–M5 (P1); D9 → M6 (P2) | Brief generated with the network off (D10 test) |
| What-if forecast | osiris OI Forecast multi-agent | none | Labelled simulation, never commands | D12 → later (P3) | Report marked "simulation", no `cmd.*` |
| Public API and MCP | worldmonitor REST, MCP, SDKs; worldwideview, osiris, gods-eye-view MCP | partly live: onemind-mcp read tools | REST, MCP and CLI over `svc.*` | F10 → M6 (P2) | Scoped token test (F10) |
| Share links, time windows, command palette | worldmonitor 1 h–7 d windows, URL state, Cmd+K | none | Shareable views; palette | B17 → M4–M5 (P1); B18 → M6 (P2) | A copied link opens the same view |
| Plugin marketplace | worldwideview SDK on npm and install from the UI | none | Signed plugin registry with permission checks | K7 → M6 (P2) | Unsigned plugin refused |
| Localisation and mobile | worldmonitor multilingual and RTL, PWA; Argos ES/EN | none | L0 web client, PWA, 2D/3D; two languages including RTL | J1 → M3; J6 → M4–M5 (P1); J5 → M6 (P2) | Phone switches 2D/3D on the same layers |
| Photoreal 3D basemap | gods-eye-view and worldwideview Google 3D | none | Cesium ion optional behind config; Google tiles **off** (decision 0004 #4) | E5 → M6 | Licence tier recorded in a decision. Deliberate: open data by default |
| Visual modes | gods-eye-view CRT, NVG, FLIR looks | none | Night vision, thermal, high contrast | B21 → later (P3) | Holds the B4 frame budget |
| Live TV and radio | worldmonitor, osiris, gods-eye-view | none | Broadcast panel | K8 → later (P3) | A station plays from its marker |
| Native desktop app | worldmonitor Tauri macOS (signing **unverified**) | none | Packaged UE app on the Mac | B1, F1 → M2 | Installs and runs on the owner's Mac |
| TAK, tasking, drones, XR, offline | **None of the dashboards** (matrix "Cannot do") | — | OneMind's core design | §5 rows | — |

---

## 7. Gaps with no feature yet (proposals for the FEATURES owner)

This lane does not edit `docs/FEATURES.md`. Each row is a proposed new feature with a priority
and an acceptance test.

| ID | Proposed feature | Why (gap) | Suggested section and priority | Acceptance test | Milestone |
|---|---|---|---|---|---|
| GAP-N1 | Link graph panel in the inspector | Palantir link view ([palantir-patterns.md](../research/palantir-patterns.md) §6) | B, P2 | Open an entity and walk two hops of links | M6 |
| GAP-N2 | Agent evaluation and red-team suite: tool evals, safety invariants, injection corpus, impersonation, and the stop-line tests in [JADC2-MAP.md](JADC2-MAP.md) §5.3 | AIP evaluations (A1); [agents-that-act.md](../research/agents-that-act.md) Rec. 6 | I, P0 (gate before any agent write tool) | All §5.3 tests pass; results in `docs/evidence/M1/` | M1 |
| GAP-N3 | Site deployment: site agent, release channels, change windows, signed offline bundles, Zarf for the home server | Apollo (AP1–AP6); [edge-and-ddil.md](../research/edge-and-ddil.md) Rec. 4 | F, P1 | A site applies a stable release from an offline bundle and refuses one during an active mission | M4–M5 |
| GAP-N4 | Federation and release service: `PARTNER-<org>` accounts, marking filter, incident exports that expire, TAK group mapping for FedHub | DoDI 8110.01 MPE; [security-and-trust.md](../research/security-and-trust.md) Rec. 3; [tak-protocols.md](../research/tak-protocols.md) Rec. 3 | A, P1 | A partner account sees only GREEN and CLEAR records released to it; access ends at incident close | M4–M5 |
| GAP-N5 | Per-service NATS identities, `CLIENT` account and per-site leaf users | Lattice short-lived identities; [security-and-trust.md](../research/security-and-trust.md) Rec. 1; [BUS-DESIGN.md](BUS-DESIGN.md) §7 | A, P0 (I4 depends on it) | A client user cannot publish `cmd.fleet.>`; Hermes cannot publish `tak.cot.inject` | M1 |
| GAP-N6 | Supply chain: SBOM, provenance, cosign keyless signing, Kyverno image verification for `ghcr.io/onemindos/*` | Zero-trust Applications pillar (S4); [security-and-trust.md](../research/security-and-trust.md) Rec. 4 | G, P1 | Every built image has an attestation; an unsigned image is flagged by Kyverno | M7 |
| GAP-N7 | Low-bandwidth radio link: Meshtastic bridged to the bus through the mesh gateway | DDIL; [edge-and-ddil.md](../research/edge-and-ddil.md) Rec. 5 | A, P2 | A position from a Meshtastic node appears as an entity | M6 |
| GAP-N8 | Stop-line tests in feature acceptance (not a new feature: amend I4, H6, D5) | [JADC2-MAP.md](JADC2-MAP.md) §5.3 tests 6–12 | amend I4, H6, D5 | As listed there | M1 / M4 |
| GAP-N9 | `OneMindATAK` plugin | TAK Level 4 ([tak-protocols.md](../research/tak-protocols.md) Rec. 4); FEATURES J lists L4 plugins "later" with no row | C, P2 | ATAK shows OneMind provenance and the task queue for an entity | M6 |

---

## 8. Closing order by milestone

| Milestone | Gaps closed (with the reference they close against) |
|---|---|
| **M1 Spine** | Entity model, overrides, correlation (Palantir objects and writeback; Lattice entities); pipeline standard and freshness (worldmonitor); AOI filter and the TAK path repair (CloudTAK geofences); mesh gateway (TAKX, WinTAK); robots as entities and the robotics CoT profile (ARTAK, CloudTAK); safety and policy with audit of failures (beats Palantir's action log); GAP-N2, GAP-N5, GAP-N8 |
| **M2 Client on the Mac** | Native app (worldmonitor desktop); globe, layers, batched rendering, inspector (all dashboards); TAK entities and send CoT (TAK clients) |
| **M3 Web** | Video ingest, registry, video in TAK and the client (CloudTAK, ARTAK); plugin model (worldwideview, CloudTAK); L0 web (every web dashboard) |
| **M4 Act** | Missions, tasking, asset commands, Hermes panel and policy, alerts, watch jobs (Lattice tasks and autonomy loop; AIP); ROS 2; role modes (CloudTAK) |
| **M4–M5 by priority (not yet in ROADMAP)** | Fleet, cameras, history, news, weather, world-tier cells (dashboards); drawing, import/export, basemaps, follow, motion, share links, gaps panel (CloudTAK, gods-eye-view, worldmonitor); Data Sync and chat (CloudTAK); briefs and UI tools (worldmonitor, gods-eye-view); disconnected mode (ARTAK, TAKX); G4 benchmark; PWA; GAP-N3, GAP-N4 |
| **M5 Twin and headset** | Homestead 3D Tiles, drone video panel, Quest Pro, rehearsal (ARTAK) |
| **M6 The leap** | Commo in the client, data packages, voice (TAK clients); re-map, view cones, photoreal option (ARTAK, gods-eye-view); multi-user, Windows and Linux; swarm, detection, multi-agent (Lattice mission autonomy, Maven-style detection under the stop lines); symbology, timeline, measurement, search; plus P2 items: satellites, layer parity, cameras at scale, push ingest, routing, incident board, anomalies, API and MCP, plugin registry, KLV, elevation; GAP-N1, GAP-N7, GAP-N9 |
| **M7 Hosted** | Hosted streaming (ARTAK Team Server); CI, performance budget, client telemetry; GAP-N6 |
| **Later (P3)** | What-if forecast, street level, interiors, visual modes, RF survey, mounted mode, broadcast panel |

ROADMAP lists milestones only for M1–M7 feature sets; the 38 features added by the competitor
lane are not yet placed. Placing them is the ROADMAP owner's job.

---

## 9. Where OneMind leads, and where it deliberately does not follow

### 9.1 The combination no reference system has (by design, once built)

| Combination | Who has part of it | Evidence |
|---|---|---|
| TAK-native action and mesh **plus** intel-dashboard breadth | TAK clients have the first; dashboards the second; none both | [competitive-matrix.md](../research/competitive-matrix.md) "Where OneMind already beats the field" |
| Open bus with entity fusion | None of the nine competitors | same |
| Drone and robot commands with policy, audit and a human confirm | None publicly | matrix rows D3, H6, I4 |
| Agents as TAK contacts | None | matrix row I1 |
| 3D Tiles homestead twin and XR | Only ARTAK, closed and paid ($3,400 a year solo) | [competitive-matrix.md](../research/competitive-matrix.md) ARTAK |
| Open, local-first and civilian | Palantir and Anduril are closed defence platforms; JADC2 is a DoD concept | [palantir-patterns.md](../research/palantir-patterns.md); [lattice-patterns.md](../research/lattice-patterns.md); [jadc2.md](../research/jadc2.md) |

### 9.2 Deliberate non-goals

| Capability | Who has it | Why OneMind stops | Stop line |
|---|---|---|---|
| Fires, kinetic and non-kinetic | JADC2 LOE 3 | Civilian mission | SL1, SL4 |
| Target identification and weapon recommendation | JADC2 as described in CRS IF11493; Maven "targeting" (MSS-2) | No targeting of people | SL1, SL2 |
| Weapon inventory with engagement data | ARTAK | No weapons | SL1 |
| Weapon-oriented entity fields | Lattice `target_priority`, `munitions`, engagement zones | No weapons | SL1 |
| ALPR layer, face recognition, cross-camera re-identification | gods-eye-view maps ALPR locations | No identity surveillance | SL3 |
| Active scanning of third-party hosts | osiris RECON | Legal risk; not situational awareness | SL4 |
| Crypto wallet tracing | osiris | Off-mission | — |
| Prediction markets and stock panels as core | worldmonitor | Optional source plugins only (A15, P3) | — |
| Google Photorealistic 3D Tiles by default | gods-eye-view, worldwideview | Metered, separate terms (decision 0004 #4) | — |
| Automatic tasking of assets by a rule or AI | Lattice autonomy loop (L14); AIP auto-run actions (A3) | Every asset command needs a human confirm (Rule 6) | SL6 |

---

## 10. Risks to this analysis

| Risk | Effect | Mitigation |
|---|---|---|
| Closed products (ARTAK, TAKX, WinTAK, Palantir, Anduril) are known only from public pages | Gaps may be larger than shown | Rows say what the evidence is; "?" rows in the matrix stay open |
| Competitor scale claims are self-reported | "Beats" claims may be wrong | G4 measures OneMind's own fixed scene before any claim |
| Everything on the OneMind side is design | The whole page is a plan | A gap closes only with a passing acceptance test and evidence |
| The TAK path in omos is broken today | M1 TAK rows cannot pass until it is fixed | Listed as a prerequisite row in §5; it is an omos PR the owner merges |
| Scope pressure from matching every dashboard domain | Dilutes the TAK and homestead mission | Breadth enters as plugins (A15, K6) |
| Licence contamination (worldmonitor AGPL, worldwideview Elastic, CloudTAK AGPL) | Legal risk | Design only for those three (decision 0004 #15; [competitive-matrix.md](../research/competitive-matrix.md) Recommendation 2) |

## 11. Unverified items

- TAKX operating systems, layers and AI; WinTAK 5.5 changes; TAKX and WinTAK parity with ATAK
  elevation, KLV and viewshed tools ([competitive-matrix.md](../research/competitive-matrix.md) "Unverified items").
- ARTAK engine and whether "PC / Mac" means a native macOS build.
- Gotham's Gaia, Graph and Dossier descriptions; Lattice fusion algorithms and Mission Autonomy
  (press only).
- Whether the omos `om-overwatch-*` services run anywhere ([lattice-patterns.md](../research/lattice-patterns.md) N2).
- That WebODM is deployed in omos `platform/onetak` is taken from FEATURES E1 and was not checked
  against the cluster by this lane.
- NACK support for the `ENT` stream settings ([BUS-DESIGN.md](BUS-DESIGN.md) §12), which the
  M1 entity rows depend on.
