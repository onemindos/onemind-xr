# Feature Catalog

Priority: **P0** is needed for the first usable client (M1–M3), **P1** makes it the
owner's main client (M4–M5), **P2** is the leap past competitors (M6), and **P3**
comes later. "Where" says which repo does the work. Every feature needs its
acceptance test passed, with evidence, before it counts as done.

## A. Data spine (omos)

| ID | Feature | P | Where | Acceptance test |
|---|---|---|---|---|
| A0 | **Pipeline standard.** One collector model for every source (Redpanda Connect, a fork, NATS-native or Go, chosen by the ingestion-pipelines research lane). Every pipeline has: acked JetStream publish with `Nats-Msg-Id` dedup, a single owner per source, schema validation with a dead-letter subject, rate limits and terms of use, freshness alerts, stream limits, fixtures-based golden tests, metrics, logs and traces | P0 | omos `platform/data`, `services/` | Chaos test (kill NATS, an upstream and a collector under load) loses no data, duplicates nothing, and alerts on the stale feed |
| A1 | **Fix the feeds.** Redpanda Connect pipelines (feed-mta, feed-hazards, feed-ships, feed-aircraft) publish normalized observations to `intel.*` / `tel.*` instead of straight CoT on `tak.cot.inject`. | P0 | omos `platform/data/redpanda-connect` | `nats sub` shows each feed on grammar subjects; nothing from these feeds lands on `tak.cot.inject` |
| A2 | Feed registry: one record per feed (source, licence, cadence, layer, freshness limit) in a KV bucket or a Git file | P0 | omos | Every running feed has a record; health shows stale feeds |
| A3 | Per-feed health: `fetchedAt`, record count, last error, on a status subject | P0 | omos | A stopped feed shows as stale within its limit |
| A4 | **Entity service.** Fuses observations into `ent.{type}.{id}.state` with identity, last position, history pointer, sources and confidence | P0 | omos `services/` | Same aircraft from two feeds gives one entity; a ship's position updates in place |
| A5 | **Layer snapshots.** Latest state per layer in NATS KV (check the existing `kv-world-state` and `kv-fleet-state` buckets first), plus a delta subject per layer | P0 | omos | A new client loads a full layer in one KV read, then follows deltas |
| A6 | Projector AOI filter: only mission-tier entities (inside an area of interest or geofence, or tasked) become CoT on `tak.cot.inject` | P0 | omos `platform/omos/tak-projector.yaml` | ATAK shows mission tracks only; a world feed with thousands of entities does not flood TAK |
| A7 | Mesh gateway merged and run on the Mac with a local leaf node (branch `feat/tak-mesh-gateway`) | P0 | omos `services/bridge/bridges/tak` | ATAK on the LAN appears on `tak.cot.mesh.rx`; a marker sent to `tak.cot.mesh.tx` appears on ATAK |
| A8 | Fleet positions from MAVLink and other bridges on `tel.fleet.{class}.{id}.position`, fused as entities | P1 | omos `services/bridge` | A SITL drone appears as an entity and on TAK |
| A9 | Camera registry: public and owned cameras as entities with stream URLs (MediaMTX for owned) | P1 | omos | Camera layer loads and a feed plays |
| A10 | History and replay: entity tracks queryable by time range | P1 | omos | Replay of the last hour of a track |
| A11 | News and OSINT text: RSS/Atom and public-channel posts geoparsed to `intel.news.{source}.item`, each source tagged with tier, state affiliation and licence in the feed registry, duplicates merged | P1 | omos `services/` | 20 recorded feed fixtures produce geolocated items on the map; the same story from two outlets shows once with both sources |
| A12 | Satellites: TLE catalog (CelesTrak) propagated with SGP4, orbit rings, next pass over a point | P2 | omos + client | ISS position matches an independent SGP4 reference to within 5 km; pass times for the homestead match within 60 s |
| A13 | Weather and hazards: animated wind (GFS), radar, satellite clouds, lightning, cyclone tracks and cones, CAP/NWS alerts as `intel.weather.*` and tile layers | P1 | omos + client | "Storm watch" preset shows radar, wind and an active NWS alert polygon for the homestead county from fixtures |
| A14 | World-tier aggregation: per-layer density cells and caps for zoomed-out views (no layer sends more than its cap; detail loads on zoom) | P1 | omos | A 50,000-vessel fixture renders as ≤ 5,000 cells at world view, and as individual ships inside a 50 km view |
| A15 | Layer-domain parity: every layer domain in `docs/research/competitive-matrix.md` (conflict, disasters, aviation, maritime, cyber, infrastructure, space, cameras, transit, energy) ships as a source plus layer plugin (K6) | P2 | omos + client | The matrix's layer-coverage list is all present, each from a running plugin with a feed-registry record |
| A16 | Camera catalog at scale: ≥ 200,000 public camera records as entities; latest frame fetched through a caching snapshot proxy (never through NATS), low resolution, no history for public cameras | P2 | omos | Catalog of 200,000 fixture records loads clustered in under 2 s; one camera shows its latest frame |
| A17 | Push ingest: webhook endpoint, CoT webhook (XML or GeoJSON) and vendor drone/CAD/AVL adapters deliver into JetStream with the A0 reliability list | P2 | omos `services/` | A posted CoT event appears as an entity once, even when the webhook is retried |

## B. Client core (this repo)

| ID | Feature | P | Acceptance test |
|---|---|---|---|
| B1 | UE 5.8 project runs natively on the owner's Apple Silicon Mac | P0 | Packaged `.app` launches at 60 fps on an idle globe |
| B2 | Cesium for Unreal globe: world terrain and imagery, offline cache | P0 | Globe renders; still renders from cache with the network off |
| B3 | `OneMindBus` plugin: NATS client (TLS, user and password, reconnect), KV snapshot read, subject subscribe, request/reply | P0 | Connects to the local leaf; reconnects after the leaf restarts |
| B4 | Layer system: a layer is a KV snapshot plus a delta subject, a renderer, a style and a filter. Toggling a layer subscribes and unsubscribes. | P0 | 10,000 point entities at ≥ 60 fps on the Mac; a layer off means no subscription |
| B5 | Batched rendering: instanced meshes or Niagara for points, billboards for icons, LOD to glTF models up close, clustering, horizon culling | P0 | Aircraft are icons far away and models near; no frame drop when a layer loads |
| B6 | Entity inspector: select anything, see sources, history, confidence and raw CoT | P0 | Click an entity, inspector opens with its sources |
| B7 | MIL-STD-2525 / CoT-type symbology for TAK entities | P1 | ATAK team members show correct symbols |
| B8 | Timeline: scrub back through entity history (A10) | P1 | Scrubbing replays tracks |
| B9 | Measurement tools: range, bearing, area, line of sight (port VTAK_Mensuration) | P1 | Matches ATAK to within 1% |
| B10 | Search and fly-to: coordinates, MGRS, place names, entities | P1 | `geo:` URI and MGRS strings fly correctly |
| B11 | Drawing and tactical graphics: point, line, polygon, rectangle, circle, sector, range rings, range and bearing, lasso select, buffer, labels; shareable as CoT | P1 | Each shape drawn in the client appears on ATAK with the same geometry |
| B12 | Point query: elevation, address, weather, sun phase and magnetic declination at a clicked point | P2 | Values match a reference (NOAA declination calculator, terrain DEM) within stated tolerance |
| B13 | Track and follow: click-to-track lock-on with a trail, chase and cockpit camera on any moving entity | P1 | Camera stays locked on a moving SITL drone for 5 minutes without drift |
| B14 | Smooth motion: interpolation between fixes, dead reckoning in gaps, staleness fade, icons on true heading at every camera angle | P1 | A 15 s-cadence fixture moves without jumps; an entity silent past its limit fades and shows "stale" |
| B15 | Import and export: GeoJSON, KML/KMZ, GPX, CSV, Shapefile, GeoPackage, MBTiles/PMTiles in; GeoJSON, CSV and KML out for a selection or AOI | P1 | Round trip of each format keeps geometry and names; an AOI export lists every camera inside it |
| B16 | Basemap and overlay sources: OGC WMS/WMTS, XYZ, TileJSON, ArcGIS MapServer/ImageServer/FeatureServer, self-hosted PMTiles; all through a caching tile proxy | P1 | One source of each kind renders; repeated views are served from the proxy cache |
| B17 | Shareable views and time windows: a link restores camera, layers, time window (1 h to 7 d) and selected entity | P1 | A copied link opens the same view on another device |
| B18 | Command palette (Cmd+K) for layers, places, entities and actions | P2 | Every layer and action reachable from the palette with the keyboard only |
| B19 | Elevation analysis: viewshed, terrain profile, contours; DTED, SRTM and GeoTIFF elevation import | P2 | Viewshed from a point matches a GDAL `gdal_viewshed` reference to within 5% of cells |
| B20 | Intelligence-gaps panel: shows stale, failed or missing feeds (A3) and which layers and alerts they weaken | P1 | Stopping a feed shows it in the panel within its freshness limit, naming the affected layer |
| B21 | Sensor visual modes: night vision, thermal, high contrast, and graphics visible through terrain and buildings | P3 | Each mode toggles live without dropping below the B4 frame budget |

## C. TAK-native

| ID | Feature | P | Acceptance test |
|---|---|---|---|
| C1 | TAK entities on the globe from `tak.cot.out` and `tak.cot.mesh.rx` | P0 | ATAK on the LAN and over TAK Server both appear |
| C2 | Send CoT: markers, routes, geofences, chat to TAK users (via `tak.cot.inject` and `tak.cot.mesh.tx`) | P0 | A marker from the client appears on ATAK |
| C3 | TAK Server streaming connection with client certificates, so the client is a TAK contact itself | P1 | The client shows in ATAK's contact list |
| C4 | Commo inside the client (port VR-TAK `VH_COT`; build Commo for macOS) for direct mesh without the gateway | P2 | Mesh works with the gateway stopped |
| C5 | Mission packages and data packages: send and receive | P2 | A data package from ATAK opens in the client |
| C6 | Mumble voice (TAK's voice) and push-to-talk | P2 | Talk to an ATAK user |
| C7 | TAK Data Sync (Marti missions): list, subscribe, view contents, logs and changes, publish features into a mission | P1 | A feature added to a mission in ATAK appears in the client's mission view, and the reverse |
| C8 | Contacts, chat and channels: TAK contact list, direct and group chat, channel (group) selection | P1 | Chat between the client and an ATAK user works both ways; switching channel changes what is visible |

## D. Act: missions and AI

| ID | Feature | P | Acceptance test |
|---|---|---|---|
| D1 | Draw areas of interest and geofences; they drive the A6 filter and Tile38 geofence alerts | P0 | Drawing an AOI changes what TAK receives |
| D2 | Missions: create, assign people and assets, waypoints, status (`cmd.mission.*`, `evt.mission.*`) | P1 | A mission created in the client shows on ATAK as a route and tasks |
| D3 | Asset command: send `cmd.fleet.{class}.{id}.{verb}` (goto, rtl, estop) with confirm and audit | P1 | SITL drone flies to a clicked point |
| D4 | Hermes panel: chat and voice with the map as context (selection, viewport, layers); Hermes can highlight, annotate and propose missions | P1 | "What's moving near the north field?" answers from entities and highlights them |
| D5 | Hermes actions go through the same command subjects with policy and audit; destructive or kinetic commands need a human confirm | P1 | A Hermes-proposed command waits for confirmation |
| D6 | Alerts: geofence crossings, stale assets, feed outages as an alert stream | P1 | A drone leaving the geofence raises an alert |
| D7 | Routing on roads and trails: A-to-B route with distance and ETA, usable as a mission route and as a camera fly-along | P2 | A route between two homestead points follows OSM roads and sends to ATAK as a route |
| D8 | Incident board: incidents with assignments, forms, logs and attachments on `evt.*` | P2 | An incident created in the client gets an assignee and a filled form, all visible in the audit log |
| D9 | Anomaly and convergence detection: per-region and per-type baselines, focal points where several feeds spike together, each with sources and confidence | P2 | A replayed fixture with a seeded spike raises one explained anomaly; a quiet replay raises none |
| D10 | AI briefs: Hermes writes a periodic brief of an AOI or the world, citing entities and naming stale feeds; falls back to a local model when offline | P1 | Brief generated with the network off, citing at least three entities and the B20 gaps |
| D11 | Hermes UI tools: fly to, frame, toggle layers, select, annotate real boundaries, measure, camera tours; Hermes reports only actions that succeeded | P1 | Ten scripted voice or text requests each produce the matching view change, verified by screenshot |
| D12 | What-if forecast: multi-agent scenario run over current entities, labelled as simulation, never driving commands | P3 | A scenario run produces a report marked "simulation" and no `cmd.*` message |

## E. Digital twin and imagery

| ID | Feature | P | Acceptance test |
|---|---|---|---|
| E1 | Homestead 3D Tiles: drone photos to WebODM (omos `platform/onetak`) to 3D Tiles, loaded in the client | P1 | The homestead model aligns with terrain to within 1 m |
| E2 | Drone live video on the map: MediaMTX stream projected as a video panel tied to the drone | P1 | Video panel follows the drone entity |
| E3 | Rapid re-map: a new flight replaces the tileset, keeping versions | P2 | Before and after toggle |
| E4 | Sensor and camera view cones, and video draped on terrain | P2 | Camera footprint drawn on the twin |
| E5 | Photorealistic 3D basemap (Cesium ion or Google), within licence | P2 | Licence tier recorded in a decision record |
| E6 | Offline field mapping: drone photos to 3D Tiles on the home server or field kit with no internet | P2 | A 200-photo set processes to a tileset with the uplink disconnected |
| E7 | Street-level imagery panel (Panoramax, KartaView; Mapillary with a key) | P3 | Clicking a street opens the nearest image with its view cone |
| E8 | Interior and subterranean models (building floors, basements) as 3D Tiles or glTF | P3 | One building interior loads and aligns with its exterior |

## F. Surfaces and deployment

| ID | Feature | P | Acceptance test |
|---|---|---|---|
| F1 | Packaged macOS app, signed for local use | P0 | Installs and runs on the owner's Mac |
| F2 | **Web via Pixel Streaming 2** from the Mac (spike first, see decision 0002) | P0 spike, P1 feature | Browser on another device controls the client with < 150 ms input latency on the LAN |
| F3 | Fallback web client if F2 fails on macOS: Pixel Streaming from a Linux GPU node, or a CesiumJS view on the same contracts | P1 | Same layers visible in a browser |
| F4 | Meta Quest Pro: standalone Android build (OpenXR) **or** streamed from a PC; spike decides (decision 0003) | P1 spike | Globe and one layer in the headset at the headset's native frame rate |
| F5 | Windows and Linux builds | P2 | CI builds both |
| F6 | Multi-user sessions: shared view, pointers, follow-me (port VH_Replication ideas) | P2 | Two users see each other's pointers |
| F7 | Hosted streaming on a GPU node, deployed through omos Argo | P3 | Owner-approved omos PR; tailnet-only ingress |
| F8 | Synchronised briefing: a presenter drives camera, time and layers for every desktop and headset in the session | P2 | Two clients show the same view within 200 ms of the presenter's change |
| F9 | Disconnected mode: the whole stack (NATS leaf, entity service, tiles, terrain, video, Hermes with a local model) runs on the Mac or home server with no internet | P1 | With the uplink off, the client shows entities, tiles and terrain, and Hermes answers a question |
| F10 | Public API: REST with OpenAPI, an MCP server and a CLI over `svc.*`, with scoped tokens and the I4 policy | P2 | An MCP client lists tools and queries entities in an AOI; a token without scope is refused and logged |

## H. Drones, robotics and autonomy

| ID | Feature | P | Where | Acceptance test |
|---|---|---|---|---|
| H1 | Every robot, drone and vehicle is an entity with a CoT type (`a-f-A-M-F-Q` UAV, `a-f-G-E-V` UGV and so on), live position, battery, mode and link health | P0 | omos bridges + entity service | SITL PX4 or ArduPilot drone shows with battery and mode |
| H2 | MAVLink bridge (omos `services/bridge/bridges/mavlink`) on grammar subjects; PX4 and ArduPilot SITL in a local test rig | P0 | omos | Scripted SITL mission runs end to end in CI or a script |
| H3 | ROS 2 bridge (Zenoh or rosbridge to NATS) for ground robots, robot dogs and arms | P1 | omos `services/bridge` | A ROS 2 turtlebot sim moves on the map and takes a goto command |
| H4 | Tasking model: task, assignee (human, robot, agent), constraints, state machine, all on `cmd.*` and `evt.*` | P1 | omos + client | One task assigned to a drone completes and reports |
| H5 | Swarm and multi-asset: group selection, formation, area search split across assets | P2 | client + omos | Three SITL drones search a drawn area |
| H6 | Safety: geofence enforcement, return-to-launch on link loss, kill switch in the UI, two-person confirm for risky commands | P0 | omos + client | Commanded drone outside the geofence is refused and alerts |
| H7 | Simulation mode: the same client drives SITL drones and simulated robots in the digital twin, for rehearsal | P1 | client | A mission rehearsed in sim replays on real assets unchanged |
| H8 | Onboard detection to `det.*` (YOLO on Jetson or in the drone video pipeline), fused into entities | P2 | omos | A detected vehicle appears as a `det.` entity with a snapshot |
| H9 | CoT-for-robotics profile: documented mapping from fleet telemetry to CoT detail (track, status, sensor, video URL) so ATAK shows robots properly | P0 | omos docs + entity projector | ATAK shows a SITL drone with heading, speed and video link |
| H10 | RF and spectrum layer: survey records and emitter detections from owned sensors (SDR, Wi-Fi/cell survey) as `det.rf.*` | P3 | omos bridge + client | A recorded survey file shows as a heat layer with per-reading detail |

## I. Agents (Hermes and the agent fabric)

| ID | Feature | P | Where | Acceptance test |
|---|---|---|---|---|
| I1 | Agents are entities and contacts: Hermes and other agents appear on the map and in TAK with status and current task | P1 | omos | Hermes shows as a contact; messaging it from ATAK reaches it |
| I2 | Agent tools over NATS: query entities, layers and history; create annotations and tasks; propose commands (`svc.*`, `agents.*` subjects) | P1 | omos `platform/hermes`, `services/onemind-core` | Hermes answers a spatial question using only bus tools |
| I3 | Watch jobs: an agent watches a condition (geofence, stale asset, feed anomaly) and acts or alerts | P1 | omos | A watch job raises an alert and proposes a task |
| I4 | Policy and audit for agent actions: OpenFGA or equivalent check before execution; full audit log on the bus | P0 | omos `platform/security` | A denied action is logged and not executed |
| I5 | Multi-agent: specialised agents (drone ops, intel analyst, homestead) coordinated by Hermes | P2 | omos | Two agents split one task and report |

## J. Levels: every device and every user

The same system serves every level. Lower levels get less rendering, never less truth.

| Level | Device | How it runs | What it gets |
|---|---|---|---|
| L0 | Any phone or old laptop browser | Light web view (CesiumJS or MapLibre) on the same contracts | Mission tier, alerts, chat, tasks; 2D/3D lite |
| L1 | Any modern browser | Pixel Streaming of the full client | Everything, rendered on a GPU host |
| L2 | Mac, Windows, Linux desktop | Native client | Everything, offline-capable |
| L3 | Meta Quest Pro and other OpenXR headsets | Standalone or streamed (decision 0003) | Immersive twin, planning, rehearsal |
| L4 | Field: ATAK, iTAK, WinTAK, TAKX, VR-TAK | Existing TAK clients over TAK Server and the mesh | Mission tier as CoT; OneMind plugins later |

| User level | Mode | Behaviour |
|---|---|---|
| Newcomer | Guided | Plain-language labels, presets ("Home", "Drone patrol", "Storm watch"), Hermes explains what is on screen, risky actions hidden |
| Operator | Standard | Layers, missions, tasking, asset commands with confirm |
| Analyst | Analyst | Raw sources, confidence, history, queries, exports |
| Commander | Command | Multi-mission view, approvals, two-person confirm queue |

| ID | Feature | P | Acceptance test |
|---|---|---|---|
| J1 | L0 light web client on the same contracts | P1 | Phone browser shows mission tier and alerts |
| J2 | Role-based modes with permissions from omos auth (Authentik) | P1 | Newcomer cannot see asset commands |
| J3 | Adaptive quality: the client lowers detail to hold frame rate on weaker hardware | P1 | Holds 60 fps on the Mac and 72 fps in the headset |
| J4 | Accessibility: keyboard-only, screen-reader labels on panels, colour-blind palettes | P2 | Audit checklist passes |
| J5 | Localization of UI strings (raised from P3: worldmonitor ships multilingual UI with RTL, Argos Atlas ES/EN) | P2 | One second language and one right-to-left language |
| J6 | L0 modes: flat 2D map and 3D globe on one layer catalog, installable PWA, touch gestures | P1 | Phone browser switches 2D/3D with the same layers; app installs to the home screen |
| J7 | Mounted and touch mode: large targets, hardware-button mapping for vehicle displays | P3 | Every primary action reachable with touch or mapped buttons only |

## K. Video and plugins

| ID | Feature | P | Where | Acceptance test |
|---|---|---|---|---|
| K1 | Video ingest through MediaMTX (`platform/onetak`): RTSP, RTMP, SRT, WebRTC/WHIP, HLS out; auth per stream; recording with retention | P0 | omos | A SITL or ffmpeg test stream plays in a browser and is recorded |
| K2 | Stream registry: every stream is linked to an entity (drone, camera) with its URL, codec, latency class and permissions, on the bus | P0 | omos | Selecting a drone entity shows its live stream |
| K3 | Video in TAK: stream URLs in CoT so ATAK and VR-TAK users open the same feeds | P1 | omos projector | ATAK opens the drone stream from its marker |
| K4 | Video in the client: panels on macOS and in the headset, tied to their entity | P1 | xr | Panel follows the drone, under 1 s latency on the LAN |
| K5 | KLV metadata: frame footprint and sensor point drawn on the twin; video draped on terrain | P2 | both | Footprint matches the drone's camera view |
| K6 | Plugin model: manifest plus source, layer, action and Hermes-skill plugin types, documented with one example each | P1 | both | A new layer ships as a plugin with no core change |
| K7 | Plugin registry: browse, install, update and disable plugins from the UI, with signature and permission checks | P2 | both | A signed plugin installs from the UI; an unsigned one is refused |
| K8 | Live broadcast and radio panel: public news TV and radio streams placed on the map | P3 | xr | A station plays from its map marker; panels stop when hidden |

## G. Operations

| ID | Feature | P | Acceptance test |
|---|---|---|---|
| G1 | CI: build and test on macOS for every PR | P1 | PR shows a macOS build check |
| G2 | Performance budget in CI or a script: entity count vs fps on a fixed scene | P1 | Report checked in per milestone |
| G3 | Telemetry of the client itself on `tel.fleet.agent.{id}.status` | P2 | Client health on the bus |
| G4 | Competitor benchmark: fixed scene of ≥ 11,575 objects with live movement (gods-eye-view's published 57.6 fps figure) | P1 | Native ≥ 60 fps on the Mac; L0 ≥ 57.6 fps on comparable hardware; L0 cold start ≤ 1.86 s; report in `docs/evidence/` |

Competitor sources for every row: `docs/research/competitive-matrix.md`.
