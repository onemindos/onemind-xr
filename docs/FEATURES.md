# Feature Catalog

Updated 2026-10-29 (M0 synthesis) from the design in [`design/`](design/) and the contracts in
[`contracts/`](contracts/). Feature IDs never change. New rows have new IDs.

**Priority.** **P0** is needed for the first usable system (M1–M3), **P1** makes it the owner's
main client (M4–M5), **P2** is the leap past competitors (M6), and **P3** comes later. Where each
feature lands is in [`ROADMAP.md`](ROADMAP.md).

**Done means proven.** A feature is done only when its acceptance test passes and the proof is in
`docs/evidence/<milestone>/` (run Rule 7). A feature that needs the owner's Mac, the Quest Pro, the
LAN with ATAK or a real SITL is marked **built, awaiting owner check** once it passes on the
build machine. Its owner step is in [`OWNER-CHECKLIST.md`](OWNER-CHECKLIST.md) (decision 0004
#17). **State on 2026-10-29: nothing is built** ([`STATUS.md`](STATUS.md)), so no feature is
"built, awaiting owner check" yet. The features that will need an owner check are listed in
[the last section](#features-that-need-an-owner-check).

**Rules every row follows.** Subjects and buckets come from the locked omos grammar (omos
`CLAUDE.md`) and [`design/BUS-DESIGN.md`](design/BUS-DESIGN.md). A gap in the grammar becomes an
owner proposal (decision 0004 #8; feature A24), never a new pattern. Asset commands go to
simulated assets only (`sim-*`, decision 0004 #13), and every one passes policy, audit and a
human confirm, even in simulation (run Rule 6). OneMind stays civilian: no weapons and no
targeting of people (stop lines SL1–SL13 in [`design/JADC2-MAP.md`](design/JADC2-MAP.md) §5).

**What changed in this update** (sources in brackets):

| Change | Rows | Source |
|---|---|---|
| New rows from the gap analysis | A18 federation, A19 NATS identities, A20 radio link, B22 link graph, C9 ATAK plugin, F11 site deployment, G5 supply chain, I6 agent evaluation | [GAP-ANALYSIS.md](design/GAP-ANALYSIS.md) §7 (GAP-N1 to GAP-N7, GAP-N9) |
| New rows from the reference architecture and bus design | A21 TAK Server leg, A22 stream caps and new streams, A23 site leaf with its own JetStream domain, A24 grammar proposals, K9 edge MediaMTX | [REFERENCE-ARCHITECTURE.md](design/REFERENCE-ARCHITECTURE.md) §4.1, §12; [BUS-DESIGN.md](design/BUS-DESIGN.md) §4, §10, §11; [video-and-plugins.md](research/video-and-plugins.md) Rec. 2 |
| Stop-line tests added to acceptance tests | I4, H6, D5, A4, A6, H4, D12, F9 | [JADC2-MAP.md](design/JADC2-MAP.md) §5.3 (GAP-N8) |
| D5 reworded: "every asset command", not "destructive or kinetic" | D5 | [JADC2-MAP.md](design/JADC2-MAP.md) §6 F2 |
| A1 corrected to the live feed list | A1 | [omos-live-state.md](research/omos-live-state.md) §4; [ingestion-platform.md](research/ingestion-platform.md) migration plan |
| Re-ranked: A8 P1 → P0 (H1's test needs fleet telemetry fused as entities) | A8 | [drones-robots-agents.md](research/drones-robots-agents.md) Rec. 1 |
| C4 marked blocked (S4 build denied; decision 0004 #3) | C4 | [tak-server-and-bridges.md](research/tak-server-and-bridges.md) §S4 |
| Acceptance tests made concrete and tied to contract tests | A0–A7, B3, C1, C2, D2, D3, H1, H2, H4, H9, I2, K1–K3, K6 | the contracts in [`contracts/`](contracts/) |

## A. Data spine (omos)

Design sources: [BUS-DESIGN.md](design/BUS-DESIGN.md), [entity.md](contracts/entity.md),
[layer-snapshot.md](contracts/layer-snapshot.md), [layer-delta.md](contracts/layer-delta.md),
[cot-projection.md](contracts/cot-projection.md),
[ingestion-platform.md](research/ingestion-platform.md) Recommendation,
[fusion-and-tracking.md](research/fusion-and-tracking.md) Recommendation.

| ID | Feature | P | Where | Acceptance test |
|---|---|---|---|---|
| A0 | **Pipeline standard.** Go collectors on `nats.go` as one framework (`omos-ingest`), chosen by the ingestion lane ([ingestion-platform.md](research/ingestion-platform.md) Recommendation; decision 0004 #18). Every pipeline has: acked JetStream publish with `Nats-Msg-Id` dedup, a single owner per source (KV `ingest-locks` lease), schema validation with dead letters on `evt.sys.{source}.rejected` (BUS-P9), rate limits and terms of use, freshness on `tel.feed.{source}.status`, stream limits, fixture-based golden tests, metrics, logs and traces | P0 | omos `services/bridge`, `platform/data` | Chaos test (kill NATS, an upstream and a collector under load) loses no data, duplicates nothing, and fires the stale-feed alert; two collector instances for one source never both publish (leader test); a malformed upstream record lands on `evt.sys.{source}.rejected` with the reason |
| A1 | **Fix the feeds.** The feeds that publish CoT straight to `tak.cot.inject` today (`feed-mta` in Redpanda Connect, `feed-satellites`) publish raw observations on grammar subjects instead; the duplicate `feed-hazards` is removed; aircraft and ships stay off until they return through A0 with an AOI box and quota ([omos-live-state.md](research/omos-live-state.md) §4) | P0 | omos `platform/data/redpanda-connect`, `services/` | `nats sub '>'` on the local stack replaying each feed shows only grammar subjects (`intel.*`, `evt.world.*`, `tel.*`); nothing from these feeds lands on `tak.cot.inject` |
| A2 | Feed registry: one record per feed (source, licence, cadence, layer, freshness limit, terms) in Git `sources.yaml`, loaded into KV `feed-config` ([BUS-DESIGN.md](design/BUS-DESIGN.md) §5) | P0 | omos | Every running collector has a record; CI fails when a collector has no record; the registry lists the freshness limit used by A3 |
| A3 | Per-feed health: `fetchedAt`, record count, last error and stale flag on `tel.feed.{source}.status` every 10 s, stored in `TEL` (BUS-DESIGN §4.1, P1) | P0 | omos | A stopped collector shows `stale: true` within its freshness limit, and the Grafana alert fires |
| A4 | **Entity service.** Fuses observations into `ent.{entity_type}.{entity_id}.state` in the new `ENT` stream (one message per subject, per-message TTL) with identity, aliases, last position, `sources[]` with stream pointers, decomposed confidence and lifecycle ([entity.md](contracts/entity.md); [fusion-and-tracking.md](research/fusion-and-tracking.md) Recommendation; omos `platform/nats/streams/ent.yaml`, proposed) | P0 | omos `services/` | [entity.md](contracts/entity.md) §10 tests: the same aircraft from two feeds gives one entity with two sources; a ship's position updates in place; a replay of `COT` and `INTEL` from scratch rebuilds the same entities; a record with `entity_type: person` and `disposition: hostile` is rejected to `evt.sys.{source}.rejected` (stop-line test 9) |
| A5 | **Layer snapshots.** A layer service writes per-layer manifests and density cells into the existing KV `world-state` (`{layer}.meta`, `{layer}.cells.z{z}`) and serves `svc.layer.snapshot` and `svc.layer.watch` with an AOI filter ([layer-snapshot.md](contracts/layer-snapshot.md), [layer-delta.md](contracts/layer-delta.md); BUS-DESIGN D3, D7) | P0 | omos | A new client loads a full layer in one `svc.layer.snapshot` request, then follows deltas from `svc.layer.watch`; the acceptance tests in both contracts pass |
| A6 | Projector AOI filter: only mission-tier entities (inside an AOI or geofence in KV `geofences`, or tasked) with `egress_tak` become CoT on `tak.cot.inject` ([cot-projection.md](contracts/cot-projection.md) §3) | P0 | omos `platform/omos/tak-projector.yaml` | [cot-projection.md](contracts/cot-projection.md) §12 test 2: 2,000 aircraft with an AOI around 20 gives exactly 20 on `tak.cot.inject`, and removing the AOI removes them; a person-class track in an AOI never appears (stop-line test 10); an incoming `t-k-*` CoT creates no task (stop-line test 8). ATAK display: owner check |
| A7 | Mesh gateway PR from branch `feat/tak-mesh-gateway`: fix transmit on `MESH_INTERFACE` (`mesh.go:84`), add GeoChat, mesh AES-256-CBC + HMAC-SHA256 (keys from the environment) and data-package getfile, then run on the Mac with a local leaf ([tak-server-and-bridges.md](research/tak-server-and-bridges.md) Rec. 1; [tak-protocols.md](research/tak-protocols.md) Rec. 2) | P0 | omos `services/bridge/bridges/tak` | Build machine: `go test ./...` passes, including a transmit-on-named-interface test, and a `takproto` peer round-trips through `tak.cot.mesh.rx` and `tak.cot.mesh.tx` ([cot-projection.md](contracts/cot-projection.md) §12 test 7). Owner check: ATAK on the LAN appears on `tak.cot.mesh.rx`, and a marker sent to `tak.cot.mesh.tx` appears on ATAK |
| A8 | Fleet positions from MAVLink and other bridges on `tel.fleet.{class}.{id}.position` and `.status` (BUS-P1), fused as entities. Raised to P0: H1's test depends on it | P0 | omos `services/bridge` | The scripted drone `sim-x1` appears as `ent.drone.sim-x1.state` with battery and mode; owner check with real SITL |
| A9 | Camera registry: public and owned cameras as entities with stream URLs (MediaMTX for owned), registry in KV `OVERWATCH_VIDEO` | P1 | omos | Camera layer loads and a feed plays |
| A10 | History and replay: entity tracks queryable by time range (ClickHouse sink and stream replay) | P1 | omos | Replay of the last hour of a track |
| A11 | News and OSINT text: RSS/Atom and public-channel posts geoparsed to `intel.news.{source}.item`, each source tagged with tier, state affiliation and licence in the feed registry, duplicates merged | P1 | omos `services/` | 20 recorded feed fixtures produce geolocated items on the map; the same story from two outlets shows once with both sources |
| A12 | Satellites: TLE catalog (CelesTrak) on `intel.space.celestrak.tle`, propagated with SGP4 by the entity service, orbit rings, next pass over a point | P2 | omos + client | ISS position matches an independent SGP4 reference to within 5 km; pass times for the homestead match within 60 s |
| A13 | Weather and hazards: animated wind (GFS), radar, satellite clouds, lightning, cyclone tracks and cones, CAP/NWS alerts as `intel.weather.*` and tile layers | P1 | omos + client | "Storm watch" preset shows radar, wind and an active NWS alert polygon for the homestead county from fixtures |
| A14 | World-tier aggregation: per-layer density cells (`{layer}.cells.z{z}` in KV `world-state`) and caps for zoomed-out views | P1 | omos | A 50,000-vessel fixture renders as ≤ 5,000 cells at world view, and as individual ships inside a 50 km view |
| A15 | Layer-domain parity: every layer domain in [competitive-matrix.md](research/competitive-matrix.md) (conflict, disasters, aviation, maritime, cyber, infrastructure, space, cameras, transit, energy) ships as a source plus layer plugin (K6) | P2 | omos + client | The matrix's layer-coverage list is all present, each from a running plugin with a feed-registry record |
| A16 | Camera catalog at scale: ≥ 200,000 public camera records as entities; latest frame fetched through a caching snapshot proxy (never through NATS), low resolution, no history for public cameras, no recognition (SL3) | P2 | omos | Catalog of 200,000 fixture records loads clustered in under 2 s; one camera shows its latest frame |
| A17 | Push ingest: webhook endpoint, CoT webhook (XML or GeoJSON) and vendor drone/CAD/AVL adapters deliver into JetStream with the A0 reliability list | P2 | omos `services/` | A posted CoT event appears as an entity once, even when the webhook is retried |
| A18 | **Federation and release service** (GAP-N4): `PARTNER-<org>` accounts, marking filter (TLP and releasability), incident exports that expire, TAK group mapping for FedHub; configuration only, no own `FederatedChannel` ([security-and-trust.md](research/security-and-trust.md) Rec. 3; [tak-protocols.md](research/tak-protocols.md) Rec. 3) | P1 | omos | A partner account sees only records marked for release to it; a `RED` record never leaves; access ends when the incident closes, and who received what is audited (SL11) |
| A19 | **Per-service NATS identities** (GAP-N5): one user per service with publish allow-lists, a `CLIENT` account for viewers, separate Hermes and TAK leaf users, per-site leaf users; only the policy service publishes `cmd.>` (except `cmd.map.>`) and `tak.cot.inject` ([BUS-DESIGN.md](design/BUS-DESIGN.md) §7; [security-and-trust.md](research/security-and-trust.md) Rec. 1) | P0 | omos (SealedSecrets) | On the local stack with the same config: a client user cannot publish `cmd.fleet.>` ([asset-command.md](contracts/asset-command.md) §9 test 8); Hermes cannot publish `tak.cot.inject` or `cmd.fleet.>`; each refusal is a server permission violation |
| A20 | Low-bandwidth radio link (GAP-N7): Meshtastic bridged to the bus through the mesh gateway ([edge-and-ddil.md](research/edge-and-ddil.md) Rec. 5) | P2 | omos `services/bridge` | A position from a Meshtastic node (recorded fixture here; a real node in the owner checklist) appears as an entity |
| A21 | **TAK Server leg proven.** The bus-to-TAK path is broken live: nothing consumes `tak.cot.inject` into TAK and nothing publishes `tak.cot.out` ([omos-live-state.md](research/omos-live-state.md) §6). Fix: make NatsBridgePlugin load visibly, and add the Go `tak-client` bridge (mTLS 8089, v1 negotiation, mission sync) that publishes `tak.cot.out` and retires cot-tap ([tak-server-and-bridges.md](research/tak-server-and-bridges.md) Rec. 2; [tak-protocols.md](research/tak-protocols.md) Rec. 1) | P0 | omos `services/takserver`, `services/bridge/bridges/tak-client` | Build machine: `tak-client` passes against a scripted TAK-stream peer. After the owner merges: hub `leafz` shows a `tak.cot.inject` subscription from the TAK leaf, and stream `COT` gains `tak.cot.out` ([omos-live-state.md](research/omos-live-state.md) Rec. 2) |
| A22 | **Stream caps and new streams**: `maxBytes` on every stream, `COT` 72 h, `CMD` 365 d with `discard: new`; new CRs `ENT`, `DET`, `kv-ingest-locks`, `kv-entity-correlation` in omos `platform/nats/streams/` ([BUS-DESIGN.md](design/BUS-DESIGN.md) §4.1, §11) | P0 | omos `platform/nats/streams/` | `make validate` and kubeconform pass on the CRs; on the local stack the same settings give a capped stream that refuses or drops as configured; total caps fit 45 GiB per node (BUS-DESIGN §4.1 arithmetic) |
| A23 | **Site leaf with its own JetStream domain** (Mac first): domain `site-mac`, a 72 h edge stream for `tel`, `evt` and `det`, CA-verified TLS, its own credential, local-only WebSocket; the hub sources site streams ([edge-and-ddil.md](research/edge-and-ddil.md) Rec. 1–2; [BUS-DESIGN.md](design/BUS-DESIGN.md) §8–§9) | P1 | omos `infra/edge/` | Local two-server test: telemetry published while the leaf is cut off reaches the hub after reconnect with no loss and no duplicates; a command is never replayed after reconnect (SL12). Owner check: repeat on the Mac |
| A24 | **Grammar proposals filed**: one omos PR with proposal files in omos `docs/` for every entry in the combined register, [BUS-DESIGN.md](design/BUS-DESIGN.md) §10.1: BUS-P1–BUS-P11 (BUS-DESIGN §10), ONT-P1–ONT-P14 ([ONTOLOGY.md](design/ONTOLOGY.md) §9), RA1–RA4 ([REFERENCE-ARCHITECTURE.md](design/REFERENCE-ARCHITECTURE.md) §13), CP-1–CP-4 ([cot-projection.md](contracts/cot-projection.md) §11) and the `intercept` example in omos `CLAUDE.md` ([JADC2-MAP.md](design/JADC2-MAP.md) §6 F1); entries marked "information only" are recorded, duplicates filed once | P0 | omos `docs/` | Each gap has evidence, a proposed ruling and the reading used until ruled; the PR changes no live subject or stream |

## B. Client core (this repo)

Design sources: [REFERENCE-ARCHITECTURE.md](design/REFERENCE-ARCHITECTURE.md) §4.2, §8;
[unreal-platform.md](research/unreal-platform.md); [vrtak-porting-map.md](research/vrtak-porting-map.md).

| ID | Feature | P | Acceptance test |
|---|---|---|---|
| B1 | UE 5.8 project runs natively on the owner's Apple Silicon Mac (Cesium for Unreal v2.30.0 or newer) | P0 | Packaged `.app` launches at ≥ 60 fps on an idle globe (owner check, OWNER-CHECKLIST S1) |
| B2 | Cesium for Unreal globe: open terrain and imagery (decision 0004 #4), offline cache | P0 | Globe renders; still renders from cache with the network off (owner check, S1) |
| B3 | `OneMindBus` plugin: `IOneMindTransport` with nats.c v3.14.0 (TLS, user and password, reconnect) and a WebSocket transport for the headset; KV snapshot read, subject subscribe, request/reply | P0 | Connects to the local leaf; reconnects after the leaf restarts; a `kv put` on `world-state` shows a marker within 1 s (owner check, S5) |
| B4 | Layer system: a layer is a snapshot (`svc.layer.snapshot`) plus deltas (`svc.layer.watch`), a renderer, a style and a filter. Toggling a layer subscribes and unsubscribes | P0 | 10,000 point entities at ≥ 60 fps on the Mac; a layer off means no subscription (owner check, B4 section) |
| B5 | Batched rendering: Niagara icons far, instanced meshes mid, actors near, clustering, horizon culling | P0 | Aircraft are icons far away and models near; no frame drop when a layer loads |
| B6 | Entity inspector: select anything, see sources, history, confidence and raw CoT | P0 | Click an entity, inspector opens with its `sources[]` and confidence breakdown |
| B7 | MIL-STD-2525 / CoT-type symbology for TAK entities (CoT-type to SIDC table from `CoTtypes.xml`; milsymbol in L0) | P1 | ATAK team members show correct symbols |
| B8 | Timeline: scrub back through entity history (A10) | P1 | Scrubbing replays tracks |
| B9 | Measurement tools: range, bearing, area, line of sight (port VTAK_Mensuration) | P1 | Matches ATAK to within 1% |
| B10 | Search and fly-to: coordinates, MGRS, place names, entities | P1 | `geo:` URI and MGRS strings fly correctly |
| B11 | Drawing and tactical graphics: point, line, polygon, rectangle, circle, sector, range rings, range and bearing, lasso select, buffer, labels; shareable as CoT | P1 | Each shape drawn in the client appears on ATAK with the same geometry |
| B12 | Point query: elevation, address, weather, sun phase and magnetic declination at a clicked point | P2 | Values match a reference (NOAA declination calculator, terrain DEM) within stated tolerance |
| B13 | Track and follow: click-to-track lock-on with a trail, chase and cockpit camera on any moving entity | P1 | Camera stays locked on a moving `sim-*` drone for 5 minutes without drift |
| B14 | Smooth motion: interpolation between fixes, dead reckoning in gaps, staleness fade, icons on true heading at every camera angle | P1 | A 15 s-cadence fixture moves without jumps; an entity silent past its limit fades and shows "stale" |
| B15 | Import and export: GeoJSON, KML/KMZ, GPX, CSV, Shapefile, GeoPackage, MBTiles/PMTiles in; GeoJSON, CSV and KML out for a selection or AOI | P1 | Round trip of each format keeps geometry and names; an AOI export lists every camera inside it |
| B16 | Basemap and overlay sources: OGC WMS/WMTS, XYZ, TileJSON, ArcGIS MapServer/ImageServer/FeatureServer, self-hosted PMTiles; all through a caching tile proxy, never NATS | P1 | One source of each kind renders; repeated views are served from the proxy cache |
| B17 | Shareable views and time windows: a link restores camera, layers, time window (1 h to 7 d) and selected entity | P1 | A copied link opens the same view on another device |
| B18 | Command palette (Cmd+K) for layers, places, entities and actions | P2 | Every layer and action reachable from the palette with the keyboard only |
| B19 | Elevation analysis: viewshed, terrain profile, contours; DTED, SRTM and GeoTIFF elevation import | P2 | Viewshed from a point matches a GDAL `gdal_viewshed` reference to within 5% of cells |
| B20 | Intelligence-gaps panel: shows stale, failed or missing feeds (A3) and which layers and alerts they weaken | P1 | Stopping a feed shows it in the panel within its freshness limit, naming the affected layer |
| B21 | Sensor visual modes: night vision, thermal, high contrast, and graphics visible through terrain and buildings | P3 | Each mode toggles live without dropping below the B4 frame budget |
| B22 | Link graph panel in the inspector (GAP-N1): an entity's links over `svc.entity.get` ([ONTOLOGY.md](design/ONTOLOGY.md) §5) | P2 | Open an entity and walk two hops of links |

## C. TAK-native

Design sources: [cot-projection.md](contracts/cot-projection.md),
[robotics-cot-profile.md](contracts/robotics-cot-profile.md),
[tak-protocols.md](research/tak-protocols.md), [tak-server-and-bridges.md](research/tak-server-and-bridges.md).
The server path depends on A21; the mesh path on A7.

| ID | Feature | P | Acceptance test |
|---|---|---|---|
| C1 | TAK entities on the globe from `tak.cot.out` and `tak.cot.mesh.rx` (stream `COT`), derived per UID by the entity service (decision 0004 #7) | P0 | Build machine: a `takproto` EUD PLI on the mesh becomes `ent.person.tak-….state` and shows in the client ([cot-projection.md](contracts/cot-projection.md) §12 test 7). Owner check: ATAK on the LAN appears, and over TAK Server after A21 |
| C2 | Send CoT: markers, routes, geofences, chat to TAK users through `svc.cot.send` (projector to `tak.cot.inject` and `tak.cot.mesh.tx`) | P0 | Build machine: a marker sent from the client is received as v1 by a `takproto` listener on the mesh group. Owner check: the marker appears on ATAK |
| C3 | TAK Server streaming connection with client certificates (8089 mTLS, enrollment through `/Marti/api/tls/signClient/v2`), so the client is a TAK contact itself | P1 | Build machine: passes against the gateway's directed TCP. Owner check (needs a TAK certificate): the client shows in ATAK's contact list |
| C4 | Commo inside the client (port VR-TAK `VH_COT`; build Commo for macOS) for direct mesh without the gateway. **Blocked**: the S4 build was denied by the build machine's permission policy; decision 0004 #3 fallback, the mesh gateway is the only mesh path ([tak-server-and-bridges.md](research/tak-server-and-bridges.md) §S4) | P2 | Mesh works with the gateway stopped |
| C5 | Mission packages and data packages: send and receive (Object Store `datapkg`) | P2 | A data package from ATAK opens in the client |
| C6 | Mumble voice (TAK's voice) and push-to-talk | P2 | Talk to an ATAK user |
| C7 | TAK Data Sync (Marti missions): list, subscribe, view contents, logs and changes, publish features into a mission (through `tak-client`, A21) | P1 | A feature added to a mission in ATAK appears in the client's mission view, and the reverse |
| C8 | Contacts, chat and channels: TAK contact list, direct and group chat, channel (group) selection | P1 | Chat between the client and an ATAK user works both ways; switching channel changes what is visible |
| C9 | `OneMindATAK` plugin (GAP-N9): ATAK 5.6.0 plugin API, GPL-3.0, ordinary CoT plus OneMind XML detail; tasking from ATAK goes through policy and a human confirm ([tak-protocols.md](research/tak-protocols.md) Level 4, Rec. 4) | P2 | ATAK shows OneMind provenance and the task queue for an entity (owner check: needs the ATAK build toolchain and plugin signing, both **unverified**) |

## D. Act: missions and AI

Design sources: [mission-task.md](contracts/mission-task.md), [asset-command.md](contracts/asset-command.md),
[ONTOLOGY.md](design/ONTOLOGY.md) §6, [agents-that-act.md](research/agents-that-act.md) Recommendation.
D3 and D5 need I4 and A19 first.

| ID | Feature | P | Acceptance test |
|---|---|---|---|
| D1 | Draw areas of interest and geofences (KV `geofences` through `cmd.map.geofence.*`); they drive the A6 filter and Tile38 geofence alerts on `evt.map.geofence.{id}.{enter\|exit}` | P0 | Drawing an AOI changes what TAK receives (A6 test on the local stack) |
| D2 | Missions: create, assign people and assets, waypoints, status (`cmd.mission.{id}.{verb}`, KV `mission-state`, `evt.mission.{id}.{state}`) | P1 | [mission-task.md](contracts/mission-task.md) §10 tests 1–3 and 6 pass; owner check: a mission created in the client shows on ATAK as a route (test 7) |
| D3 | Asset command: `cmd.fleet.{class}.{id}.{verb}` from the closed verb list (SL5), proposed through `svc.policy.command.propose`, confirmed by a human, signed, audited on `evt.so.action.*` | P1 | `sim-x1` flies to a clicked point only after a human confirm; proposal, approval and outcome are on `evt.so.action.*`; a non-`sim-` id is denied `not_simulated` ([asset-command.md](contracts/asset-command.md) §9 tests; stop-line test 3) |
| D4 | Hermes panel: chat and voice with the map as context (selection, viewport, layers); Hermes can highlight, annotate and propose missions | P1 | "What's moving near the north field?" answers from entities and highlights them |
| D5 | Hermes actions go through the same proposal path with policy and audit; **every asset command needs a human confirm**, and an agent can propose but never confirm | P1 | A Hermes-proposed command waits for confirmation; Hermes calling `svc.policy.*.confirm` is refused `agent_cannot_confirm` (stop-line test 2); asked to "follow that person", Hermes refuses and any proposal is denied `forbidden_action` and recorded (stop-line test 6) |
| D6 | Alerts: geofence crossings, stale assets, feed outages as an alert stream | P1 | A `sim-*` drone leaving the geofence raises an alert |
| D7 | Routing on roads and trails: A-to-B route with distance and ETA, usable as a mission route and as a camera fly-along | P2 | A route between two homestead points follows OSM roads and sends to ATAK as a route |
| D8 | Incident board: incidents with assignments, forms, logs and attachments on `evt.*` | P2 | An incident created in the client gets an assignee and a filled form, all visible in the audit log |
| D9 | Anomaly and convergence detection: per-region and per-type baselines, focal points where several feeds spike together, each with sources and confidence | P2 | A replayed fixture with a seeded spike raises one explained anomaly; a quiet replay raises none |
| D10 | AI briefs: Hermes writes a periodic brief of an AOI or the world, citing entities and naming stale feeds; falls back to a local model when offline | P1 | Brief generated with the network off, citing at least three entities and the B20 gaps |
| D11 | Hermes UI tools over `cmd.map.>` (reversible only): fly to, frame, toggle layers, select, annotate real boundaries, measure, camera tours; Hermes reports only actions that succeeded | P1 | Ten scripted voice or text requests each produce the matching view change, verified by screenshot |
| D12 | What-if forecast: multi-agent scenario run over current entities, labelled as simulation, never driving commands (SL7) | P3 | A scenario run produces a report marked "simulation"; its attempt to publish `cmd.fleet.sim-x1.goto` is a server permission violation (stop-line test 11) |

## E. Digital twin and imagery

| ID | Feature | P | Acceptance test |
|---|---|---|---|
| E1 | Homestead 3D Tiles: drone photos to WebODM (omos `platform/onetak`; deployment **unverified**, [GAP-ANALYSIS.md](design/GAP-ANALYSIS.md) §11) to 3D Tiles in Object Store `tiles`, loaded in the client | P1 | The homestead model aligns with terrain to within 1 m |
| E2 | Drone live video on the map: MediaMTX stream projected as a video panel tied to the drone | P1 | Video panel follows the drone entity |
| E3 | Rapid re-map: a new flight replaces the tileset, keeping versions | P2 | Before and after toggle |
| E4 | Sensor and camera view cones, and video draped on terrain (decals on Cesium tiles are **unverified**; the S1 run tests them, [video-and-plugins.md](research/video-and-plugins.md) item 11) | P2 | Camera footprint drawn on the twin |
| E5 | Photorealistic 3D basemap (Cesium ion or Google), within licence; Google stays off by default (decision 0004 #4) | P2 | Licence tier recorded in a decision record |
| E6 | Offline field mapping: drone photos to 3D Tiles on the home server or field kit with no internet | P2 | A 200-photo set processes to a tileset with the uplink disconnected |
| E7 | Street-level imagery panel (Panoramax, KartaView; Mapillary with a key) | P3 | Clicking a street opens the nearest image with its view cone |
| E8 | Interior and subterranean models (building floors, basements) as 3D Tiles or glTF | P3 | One building interior loads and aligns with its exterior |

## F. Surfaces and deployment

Design sources: decisions [0002](decisions/0002-web-via-pixel-streaming.md) and
[0003](decisions/0003-meta-quest-pro.md); [REFERENCE-ARCHITECTURE.md](design/REFERENCE-ARCHITECTURE.md) §8–§9.

| ID | Feature | P | Acceptance test |
|---|---|---|---|
| F1 | Packaged macOS app, signed for local use, bundle id `org.onemindos.onemind` (decision 0004 #16) | P0 | Installs and runs on the owner's Mac (owner check, S1) |
| F2 | **Web via Pixel Streaming 2** from the Mac. S2 has not run (no Unreal on the build machine); PS2 on macOS is **unverified** (decision 0002) | P0 spike, P1 feature | Browser on another device controls the client with < 150 ms input latency on the LAN (owner check, S2) |
| F3 | Fallback web client if F2 fails on macOS: the L0 CesiumJS client (J1) on the same contracts; a Linux GPU host is documented, not provisioned (decision 0004 #1, #5) | P1 | Same layers visible in a browser |
| F4 | Meta Quest Pro: standalone Android/OpenXR build first, then WebXR in the headset browser, then streaming (decision 0004 #2; decision 0003). Epic OpenXR only; Meta XR plugin off | P1 spike | Globe and one layer in the headset at the headset's native frame rate (owner check, S3) |
| F5 | Windows and Linux builds | P2 | CI builds both |
| F6 | Multi-user sessions: shared view, pointers, follow-me (VH_Replication ideas on NATS KV) | P2 | Two users see each other's pointers |
| F7 | Hosted streaming on a GPU node, deployed through omos Argo | P3 | Owner-approved omos PR; tailnet-only ingress |
| F8 | Synchronised briefing: a presenter drives camera, time and layers for every desktop and headset in the session | P2 | Two clients show the same view within 200 ms of the presenter's change |
| F9 | Disconnected mode: the whole stack (site leaf A23, entity service, tiles, terrain, video, Hermes with a local model) runs on the Mac or home server with no internet; the site policy service fails closed (SL12) | P1 | With the uplink off, the client shows entities, tiles and terrain, and Hermes answers a question; a goto proposed with OpenFGA unreachable is denied `policy_unavailable` (stop-line test 12) |
| F10 | Public API: REST with OpenAPI, an MCP server and a CLI over `svc.*`, with scoped tokens and the I4 policy | P2 | An MCP client lists tools and queries entities in an AOI; a token without scope is refused and logged |
| F11 | Site deployment (GAP-N3): a site agent reporting on `tel.fleet.node.{site}.status`, release channels as Git refs or digests, change windows, signed offline bundles, Zarf for the home server ([edge-and-ddil.md](research/edge-and-ddil.md) Rec. 4) | P1 | A site applies a stable release from an offline bundle, and refuses an upgrade during an active mission |

## H. Drones, robotics and autonomy

Design sources: [drones-robots-agents.md](research/drones-robots-agents.md) Recommendation,
[robotics-cot-profile.md](contracts/robotics-cot-profile.md), [asset-command.md](contracts/asset-command.md).
On the build machine the asset is the scripted MAVLink drone `sim-x1` (`scripts/sim/mavlink_sim_drone.py`);
a real SITL runs in the owner checklist ([PREFLIGHT.md](PREFLIGHT.md), SITL).

| ID | Feature | P | Where | Acceptance test |
|---|---|---|---|---|
| H1 | Every robot, drone and vehicle is an entity with a CoT type (`a-f-A-M-H-Q` for the multirotor `sim-x1`, `a-f-G-E-V` UGV, per [robotics-cot-profile.md](contracts/robotics-cot-profile.md) §2), live position, battery, mode and link health | P0 | omos bridges + entity service | `sim-x1` shows as an entity with battery and mode; owner check: a PX4 SITL drone does the same |
| H2 | MAVLink bridge (omos `services/bridge/bridges/mavlink`) moved from off-grammar `omos.air.*` to `tel.fleet.drone.{id}.position\|status` and `cmd.fleet.drone.{id}.{verb}`; MSL converted to HAE; verbs mapped to MAVLink commands | P0 | omos | A contract test runs the scripted drone with `MAVLINK20=1` and asserts that a local `TEL`-equivalent stream stores every fleet message; a scripted arm, takeoff, goto and RTL runs end to end. Owner check: the same with PX4 SITL |
| H3 | ROS 2 bridge (zenoh-bridge-ros2dds plus a small zenoh-to-NATS adapter; rosbridge fallback) for ground robots, robot dogs and arms | P1 | omos `services/bridge` | A ROS 2 turtlebot sim moves on the map and takes a goto command after a human confirm |
| H4 | Tasking model: task, assignee (human, robot, agent), constraints, state machine (KV `mission-state` with CAS on `status_version`, facts on `evt.mission.*`) | P1 | omos + client | [mission-task.md](contracts/mission-task.md) §10 tests 1–4 pass: one task assigned to `sim-x1` completes and reports; an `observe` task on a person-class track is denied `forbidden_action` (stop-line test 1) |
| H5 | Swarm and multi-asset: group selection, formation, area search split across assets | P2 | client + omos | Three `sim-*` drones search a drawn area; each child task has its own approval ([mission-task.md](contracts/mission-task.md) §10 test 5) |
| H6 | Safety: geofence enforcement in policy and in the bridge, return-to-launch on link loss, kill switch in the UI, two-person confirm for A4 verbs, signed commands with nonce and expiry, no replay after reconnect | P0 | omos + client | A command to `sim-x1` outside the geofence is refused and alerts; replaying an expired command is refused `expired` (stop-line test 5); a non-`sim-` id is refused by policy and again by the bridge (SL8) |
| H7 | Simulation mode: the same client drives `sim-*` drones and simulated robots in the digital twin, for rehearsal | P1 | client | A mission rehearsed in sim replays on other `sim-*` assets unchanged |
| H8 | Detection to `det.{domain}.{type}.{id}` (stream `DET`, A22) with Apache-2.0 detectors (RT-DETR, D-FINE, YOLOX; never Ultralytics), fused into entities; vehicles and animals only, no identity (SL3) | P2 | omos | A detected vehicle appears as an entity with a snapshot |
| H9 | CoT-for-robotics profile ([robotics-cot-profile.md](contracts/robotics-cot-profile.md)): fleet telemetry to CoT detail (`track`, `status`, `sensor`, `__video`) so ATAK shows robots properly | P0 | omos docs + projector | [robotics-cot-profile.md](contracts/robotics-cot-profile.md) §9 tests 1–3 and 6 pass on the local stack; owner check: ATAK shows `SIM-X1` with heading, speed and the video link |
| H10 | RF and spectrum layer: survey records and emitter detections from owned sensors as `det.rf.*`; receive only (SL4) | P3 | omos bridge + client | A recorded survey file shows as a heat layer with per-reading detail |

## I. Agents (Hermes and the agent fabric)

Design sources: [agents-that-act.md](research/agents-that-act.md) Recommendation 1–6;
[JADC2-MAP.md](design/JADC2-MAP.md) §4–§5. Order: A19, I4 and I6 before any agent write tool (I2's
proposals, D5).

| ID | Feature | P | Where | Acceptance test |
|---|---|---|---|---|
| I1 | Agents are entities and contacts: Hermes and other agents appear on the map and in TAK with status and current task | P1 | omos | Hermes shows as a contact; messaging it from ATAK reaches it |
| I2 | Agent tools over NATS: `svc.entity.*`, `svc.layer.*` reads, `svc.policy.*.propose` proposals and `cmd.map.>` UI actions; `nats_publish` retired | P1 | omos `platform/hermes`, `services/onemind-core` | Hermes answers a spatial question using only bus tools; Hermes has no tool that publishes `cmd.fleet.>` |
| I3 | Watch jobs: deterministic watchers on JetStream and KV emit events within an explicit envelope; the LLM explains and proposes only on trigger | P1 | omos | A watch job raises an alert and proposes a task that waits for a human |
| I4 | Policy and audit: a policy service (`svc.policy.*`) with OpenFGA, fail closed, closed verb list, human confirm, Ed25519-signed commands; every proposal, approval, denial and failure audited on `evt.so.action.*` (stream `EVT`) and every command in `CMD` | P0 | omos `platform/security`, `services/` | A denied action is logged and not executed; no `cmd.fleet.>` message exists without a human `evt.so.action.approved` over the whole test run (stop-line test 4); `asset.engage`, `asset.strike` and `asset.intercept` are each denied and nothing reaches `cmd.fleet.>` (stop-line test 7); "follow that person" is denied and recorded (stop-line test 6) |
| I5 | Multi-agent: specialised agents (drone ops, intel analyst, homestead) coordinated by Hermes, with signed agent identity; new agents start read-only | P2 | omos | Two agents split one task and report |
| I6 | **Agent evaluation and red-team suite** (GAP-N2): tool evals, safety invariants, a prompt-injection corpus, impersonation tests and the stop-line tests, plus a CI check that fails on forbidden names (`strike`, `engage`, `intercept`, `designate`, `hostile`, `t-k-`) in omos schemas and the action catalogue | P0 | omos + `docs/evidence/M1/` | All [JADC2-MAP.md](design/JADC2-MAP.md) §5.3 tests that apply to M1 pass on the local stack, with results in `docs/evidence/M1/`; the forbidden-name check fails on a seeded bad schema |

## J. Levels: every device and every user

The same system serves every level. Lower levels get less rendering, never less truth.

| Level | Device | How it runs | What it gets |
|---|---|---|---|
| L0 | Any phone or old laptop browser | Light web view (CesiumJS, nats.js over WebSocket) on the same contracts | Mission tier, alerts, chat, tasks; 2D/3D lite |
| L1 | Any modern browser | Pixel Streaming of the full client | Everything, rendered on a GPU host |
| L2 | Mac, Windows, Linux desktop | Native client | Everything, offline-capable |
| L3 | Meta Quest Pro and other OpenXR headsets | Standalone or streamed (decision 0003) | Immersive twin, planning, rehearsal |
| L4 | Field: ATAK, iTAK, WinTAK, TAKX, VR-TAK | Existing TAK clients over TAK Server and the mesh | Mission tier as CoT; the `OneMindATAK` plugin later (C9) |

| User level | Mode | Behaviour |
|---|---|---|
| Newcomer | Guided | Plain-language labels, presets ("Home", "Drone patrol", "Storm watch"), Hermes explains what is on screen, risky actions hidden |
| Operator | Standard | Layers, missions, tasking, asset commands with confirm |
| Analyst | Analyst | Raw sources, confidence, history, queries, exports |
| Commander | Command | Multi-mission view, approvals, two-person confirm queue |

| ID | Feature | P | Acceptance test |
|---|---|---|---|
| J1 | L0 light web client on the same contracts (also the decision 0004 #1 fallback for F2, so it is built in this run either way, decision 0002) | P1 | Phone browser shows mission tier and alerts from the local nats-server WebSocket |
| J2 | Role-based modes with permissions from omos auth (Authentik) | P1 | Newcomer cannot see asset commands |
| J3 | Adaptive quality: the client lowers detail to hold frame rate on weaker hardware | P1 | Holds 60 fps on the Mac and 72 fps in the headset (owner check, S1 and S3) |
| J4 | Accessibility: keyboard-only, screen-reader labels on panels, colour-blind palettes | P2 | Audit checklist passes |
| J5 | Localization of UI strings | P2 | One second language and one right-to-left language |
| J6 | L0 modes: flat 2D map and 3D globe on one layer catalog, installable PWA, touch gestures | P1 | Phone browser switches 2D/3D with the same layers; app installs to the home screen |
| J7 | Mounted and touch mode: large targets, hardware-button mapping for vehicle displays | P3 | Every primary action reachable with touch or mapped buttons only |

## K. Video and plugins

Design sources: [video-and-plugins.md](research/video-and-plugins.md) Recommendation;
[REFERENCE-ARCHITECTURE.md](design/REFERENCE-ARCHITECTURE.md) §6–§7. Video never travels on NATS;
only its registry and metadata do.

| ID | Feature | P | Where | Acceptance test |
|---|---|---|---|---|
| K1 | Video ingest through MediaMTX (omos `platform/onetak`): pinned image, internal users from a SealedSecret, path allowlists, recording with retention on `do-block-storage-retain`, tailnet-only listeners | P0 | omos | An ffmpeg test stream on `drone/sim-x1` plays in a browser over LL-HLS and is recorded; an unauthenticated publish is refused |
| K2 | Stream registry: every stream is linked to an entity (drone, camera) with its URL, codec, latency class and permissions, in KV `OVERWATCH_VIDEO` and the entity `media[]` field ([entity.md](contracts/entity.md)) | P0 | omos | Selecting the `sim-x1` entity shows its live stream |
| K3 | Video in TAK: `__video` with `ConnectionEntry` and `sensor` in projected CoT; feeds registered through `/Marti/api/video` | P1 | omos projector | [robotics-cot-profile.md](contracts/robotics-cot-profile.md) §9 test 3 passes; owner check: ATAK opens the drone stream from its marker |
| K4 | Video in the client: panels on macOS and in the headset, tied to their entity; Electra HLS now, native RTSP/SRT/WHEP decoder spike for ≤ 1 s | P1 | xr | Panel follows the drone, under 1 s glass-to-glass on the LAN (owner check) |
| K5 | KLV metadata (MISB ST 0601 in STANAG 4609 MPEG-TS): frame footprint and sensor point on `tel.fleet.drone.{id}.sensor\|footprint` (BUS-P7), drawn on the twin; video draped on terrain | P2 | both | A synthetic ST 0601 stream produces a footprint at most once per second ([robotics-cot-profile.md](contracts/robotics-cot-profile.md) §9 test 4); the footprint matches the drone's camera view |
| K6 | Plugin model: `onemind-plugin.yaml` manifest plus source, layer, action and Hermes-skill plugin types, with a CI check that validates `bus.*` subjects against the locked grammar; one example per kind | P1 | both | A new layer (USGS quakes) ships as a plugin with no core change; a manifest with an off-grammar subject fails CI |
| K7 | Plugin registry: browse, install, update and disable plugins from the UI, with signature and permission checks | P2 | both | A signed plugin installs from the UI; an unsigned one is refused |
| K8 | Live broadcast and radio panel: public news TV and radio streams placed on the map | P3 | xr | A station plays from its map marker; panels stop when hidden |
| K9 | Edge MediaMTX on the Mac or home server, forwarding to the cloud over SRT, with the lane config (`docs/evidence/M0/video/mediamtx-lane.example.yml`) and credentials from the keychain ([video-and-plugins.md](research/video-and-plugins.md) Rec. 2) | P1 | omos script + owner | Build machine: the SRT forward loop in `docs/evidence/M0/video/video-lane-proofs.txt` passes again from the script. Owner check: a stream published on the Mac plays on another LAN device |

## G. Operations

| ID | Feature | P | Acceptance test |
|---|---|---|---|
| G1 | CI: build and test on macOS for every PR | P1 | PR shows a macOS build check |
| G2 | Performance budget in CI or a script: entity count vs fps on a fixed scene | P1 | Report checked in per milestone |
| G3 | Telemetry of the client itself on `tel.fleet.agent.{id}.status` | P2 | Client health on the bus |
| G4 | Competitor benchmark: fixed scene of ≥ 11,575 objects with live movement (gods-eye-view's published 57.6 fps figure); the gate for any "beats" claim ([competitive-matrix.md](research/competitive-matrix.md) Rec. 4) | P1 | Native ≥ 60 fps on the Mac; L0 ≥ 57.6 fps on comparable hardware; L0 cold start ≤ 1.86 s; report in `docs/evidence/` |
| G5 | Supply chain (GAP-N6): SBOM, provenance, cosign keyless signing and `attest` in every `build-*.yml`; Kyverno image verification in Audit for `ghcr.io/onemindos/*` ([security-and-trust.md](research/security-and-trust.md) Rec. 4) | P1 | Every built image has an attestation; an unsigned image is flagged by Kyverno |

Competitor sources for every row: [competitive-matrix.md](research/competitive-matrix.md).
Reference-system gaps and the reason for each new row: [design/GAP-ANALYSIS.md](design/GAP-ANALYSIS.md).

## Features that need an owner check

These features can pass on the build machine only up to a point. Their last step runs on the
owner's hardware ([OWNER-CHECKLIST.md](OWNER-CHECKLIST.md)). None is built yet, so none is marked
"built, awaiting owner check".

| Feature | Owner hardware | Checklist section | State |
|---|---|---|---|
| B1, B2, F1, J3 (Mac) | Mac | 3. S1 | not built |
| B3 | Mac | 4. S5 | not built |
| B4, B5, G2 | Mac | 5. 10,000 entities | not built |
| F2 | Mac + a second LAN device | 6. S2 | not built |
| F4, J3 (headset) | Mac + Quest Pro | 7. S3 | not built |
| A7, C1, C2 | Mac + ATAK on the LAN | 8. LAN mesh test | not built (the gateway exists on an omos branch and can be tried now) |
| A6, H9, K3, D2 | Mac + ATAK on the LAN | 9. ATAK display checks | not built |
| H1, H2, A8 | Mac with PX4 SITL | 10. Real SITL | not built |
| K4, K9 | Mac + a second LAN device | 11. Video | not built |
| B15 (ATAK import), B2 (offline) | Mac + ATAK | 12. Offline map pack | not built |
| C3, A21 | TAK certificate, tailnet | 13. TAK Server | not built |
| A23, F9 | Mac | 14. Site leaf and offline | not built |
| A20, C9, E4 | Meshtastic node; ATAK toolchain; Mac | 15. Later checks | not built |
