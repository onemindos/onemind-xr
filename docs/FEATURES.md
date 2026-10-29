# Feature Catalog

Priority: **P0** is needed for the first usable client (M1–M3), **P1** makes it the
owner's main client (M4–M5), **P2** is the leap past competitors (M6), and **P3**
comes later. "Where" says which repo does the work. Every feature needs its
acceptance test passed, with evidence, before it counts as done.

## A. Data spine (omos)

| ID | Feature | P | Where | Acceptance test |
|---|---|---|---|---|
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

## C. TAK-native

| ID | Feature | P | Acceptance test |
|---|---|---|---|
| C1 | TAK entities on the globe from `tak.cot.out` and `tak.cot.mesh.rx` | P0 | ATAK on the LAN and over TAK Server both appear |
| C2 | Send CoT: markers, routes, geofences, chat to TAK users (via `tak.cot.inject` and `tak.cot.mesh.tx`) | P0 | A marker from the client appears on ATAK |
| C3 | TAK Server streaming connection with client certificates, so the client is a TAK contact itself | P1 | The client shows in ATAK's contact list |
| C4 | Commo inside the client (port VR-TAK `VH_COT`; build Commo for macOS) for direct mesh without the gateway | P2 | Mesh works with the gateway stopped |
| C5 | Mission packages and data packages: send and receive | P2 | A data package from ATAK opens in the client |
| C6 | Mumble voice (TAK's voice) and push-to-talk | P2 | Talk to an ATAK user |

## D. Act: missions and AI

| ID | Feature | P | Acceptance test |
|---|---|---|---|
| D1 | Draw areas of interest and geofences; they drive the A6 filter and Tile38 geofence alerts | P0 | Drawing an AOI changes what TAK receives |
| D2 | Missions: create, assign people and assets, waypoints, status (`cmd.mission.*`, `evt.mission.*`) | P1 | A mission created in the client shows on ATAK as a route and tasks |
| D3 | Asset command: send `cmd.fleet.{class}.{id}.{verb}` (goto, rtl, estop) with confirm and audit | P1 | SITL drone flies to a clicked point |
| D4 | Hermes panel: chat and voice with the map as context (selection, viewport, layers); Hermes can highlight, annotate and propose missions | P1 | "What's moving near the north field?" answers from entities and highlights them |
| D5 | Hermes actions go through the same command subjects with policy and audit; destructive or kinetic commands need a human confirm | P1 | A Hermes-proposed command waits for confirmation |
| D6 | Alerts: geofence crossings, stale assets, feed outages as an alert stream | P1 | A drone leaving the geofence raises an alert |

## E. Digital twin and imagery

| ID | Feature | P | Acceptance test |
|---|---|---|---|
| E1 | Homestead 3D Tiles: drone photos to WebODM (omos `platform/onetak`) to 3D Tiles, loaded in the client | P1 | The homestead model aligns with terrain to within 1 m |
| E2 | Drone live video on the map: MediaMTX stream projected as a video panel tied to the drone | P1 | Video panel follows the drone entity |
| E3 | Rapid re-map: a new flight replaces the tileset, keeping versions | P2 | Before and after toggle |
| E4 | Sensor and camera view cones, and video draped on terrain | P2 | Camera footprint drawn on the twin |
| E5 | Photorealistic 3D basemap (Cesium ion or Google), within licence | P2 | Licence tier recorded in a decision record |

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
| J5 | Localization of UI strings | P3 | One second language |

## G. Operations

| ID | Feature | P | Acceptance test |
|---|---|---|---|
| G1 | CI: build and test on macOS for every PR | P1 | PR shows a macOS build check |
| G2 | Performance budget in CI or a script: entity count vs fps on a fixed scene | P1 | Report checked in per milestone |
| G3 | Telemetry of the client itself on `tel.fleet.agent.{id}.status` | P2 | Client health on the bus |
