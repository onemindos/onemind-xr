# Architecture

The full design is in [`design/REFERENCE-ARCHITECTURE.md`](design/REFERENCE-ARCHITECTURE.md)
(sites, services, flows, video, plugins, deployment, security zones). Subjects, streams and
buckets are in [`design/BUS-DESIGN.md`](design/BUS-DESIGN.md); object types and actions in
[`design/ONTOLOGY.md`](design/ONTOLOGY.md). This page is the short version. Status: M0 design,
2026-10-29; most of it is not built yet (REFERENCE-ARCHITECTURE §12).

```
 SOURCES                        OMOS PLATFORM (cloud hub + site leaves)                     ONEMIND XR CLIENTS
 ───────                        ───────────────────────────────────────                     ──────────────────
 public feeds ──► Go collectors (omos-ingest) ──► intel.* evt.world.air|sea.* ─┐
 drones (MAVLink) ─► MAVLink bridge ────────────► tel.fleet.drone.{id}.* ──────┤  (site streams sourced
 robots (ROS 2) ──► zenoh adapter ──────────────► tel.fleet.robot.{id}.* ──────┤   into hub TEL/EVT/DET/COT)
 home sensors ────► HA / MQTT bridges ──────────► tel.{class}.{node}.* (BUS-P1)┤
 detectors ───────────────────────────────────────► det.{domain}.{type}.{id} ──┤
 TAK mesh ◄──────► mesh gateway (site) ◄──────────► tak.cot.mesh.rx / .tx ──────┤
 TAK Server ◄─────► NatsBridgePlugin + tak-client ◄► tak.cot.out / .inject ─────┤
 cameras, drones ─► MediaMTX (edge → cloud) ─► video registry, KLV ─► entity media[], tel.fleet.drone.{id}.sensor
                                                                                ▼
                                        ENTITY SERVICE (Make Sense) ──► ENT  ent.{type}.{id}.state
                                               │                          │
                                               │                          ├──► layer service ─► KV world-state ─► svc.layer.* ─┬► Unreal (L2, L3)
                          projector (AOI, mission tier, marking check)    │                                                    ├► L0 web (CesiumJS)
                                               │                          ├──► Hermes (svc.entity.*, agents.*)                  └► ATAK plugin
                                               ▼                          │
                                 tak.cot.inject + tak.cot.mesh.tx         └──► policy service ◄── svc.policy.*.propose / confirm
                                   (mission tier only)                              │ OpenFGA, human confirm, signed
                                                                                    ▼
                                                                         cmd.fleet.{class}.{id}.{verb} ─► bridges ─► evt.drone|robot.*
```

`tel.{class}.{node}.*` for home sensors is owner proposal BUS-P1, used until ruled
([design/BUS-DESIGN.md](design/BUS-DESIGN.md) §10). The locked grammar order is
`tel.{node}.{class}.{metric}` (omos `CLAUDE.md`); omos `platform/nats/streams/env.yaml` binds
class-first subjects such as `tel.soil.>`. Policy requests use the four-token names in
BUS-DESIGN §3.4 (`svc.policy.command.*`, `svc.policy.task.*`).

## Layers

1. **Sources and bridges (omos).** Go collectors on `nats.go` and device bridges publish raw
   observations on grammar subjects with a JetStream ack, `Nats-Msg-Id` dedup, one lease per
   source and freshness on `tel.feed.{source}.status`. No feed publishes CoT for TAK directly
   (feature A1). Map tiles go through a caching tile proxy and video through MediaMTX, never
   over NATS. ([ingestion-platform.md](research/ingestion-platform.md),
   [SOURCE-REGISTRY.md](sources/SOURCE-REGISTRY.md) "Routing rule")
2. **Bus (omos).** In-cluster NATS JetStream is the authority. Each site (the Mac, the home
   server, field kits) is a leaf with its own JetStream domain; the hub sources the site
   streams, and sites mirror the entity state they need. Commands are never queued for an
   offline site. Streams and KV buckets are CRs in `platform/nats/streams/`.
   ([BUS-DESIGN.md](design/BUS-DESIGN.md) §8–9)
3. **Make Sense (omos).** The entity service fuses observations into
   `ent.{entity_type}.{entity_id}.state` (stream `ENT`, proposed) with provenance and expiry.
   The layer service keeps per-layer manifests and density cells in KV `world-state` and
   serves AOI-filtered snapshots and deltas over `svc.layer.*`. This is what clients and Hermes
   read. ([fusion-and-tracking.md](research/fusion-and-tracking.md),
   [layer-snapshot.md](contracts/layer-snapshot.md))
4. **Projection to TAK (omos).** Only the projector publishes CoT: the mission tier to TAK
   Server (`tak.cot.inject`) and to the mesh (`tak.cot.mesh.tx`), with stream URLs as
   `__video`. TAK input arrives as `tak.cot.out` and `tak.cot.mesh.rx` (decision 0004 #7).
   Today the bus-to-TAK leg is broken ([omos-live-state.md](research/omos-live-state.md) §6).
5. **Clients (this repo).** Unreal 5.8 with Cesium for Unreal, a light CesiumJS web client and
   the headset build all read snapshots and deltas, render layers in batches, and send changes
   only as policy requests. They are TAK-aware through the bus and never hold TAK certificates.
6. **Act.** People, watch jobs and Hermes **propose**; the policy service checks OpenFGA (fail
   closed), gets a human confirm in the UI, and only then publishes a signed
   `cmd.fleet.{class}.{id}.{verb}`; bridges verify it locally and report `evt.drone|robot.*`
   (I4, D3, run rule 6). Every decision is audited on `evt.so.action.*`. Assets are `sim-*`
   only in this run.

## Sites

| Site | Runs | Updated by |
|---|---|---|
| Cloud hub (`omos-cloud`) | NATS 3-node, TAK Server, platform services, cloud MediaMTX, Hermes | Argo CD from omos `main` |
| Home server (later; the Mac stands in) | site leaf, site entity/layer/projector, bridges, mesh gateway, MediaMTX, offline tiles, local Hermes model | K3s + Zarf packages, pulled by a site agent within change windows |
| Owner's Mac | site leaf, mesh gateway, MediaMTX, the Unreal client | `scripts/` setup, launchd, signed bundles |
| Field kit (Pi 5, Jetson, Starlink, mesh radios) | site leaf, mesh gateway, detector, MediaMTX | Zarf package or pinned-binary tarball |

Deployment follows the Apollo pattern: one hub, pull-based spokes, release channels
(`canary`, `stable`) and signed offline bundles for disconnected sites
([palantir-patterns.md](research/palantir-patterns.md) §8,
[edge-and-ddil.md](research/edge-and-ddil.md) Rec. 4).

## Unreal plugins

| Plugin | Responsibility | Donor code |
|---|---|---|
| `OneMindBus` | `IOneMindTransport`: nats.c v3.14.0 to the local leaf, WebSocket transport for the headset; KV, subscribe, request/reply, reconnect | nats.c (Apache-2.0) |
| `OneMindGlobe` | Cesium for Unreal v2.30.0 setup, open tilesets, offline cache, geodesy helpers | VR-TAK `VH_Globe` (UTM, MGRS, EGM96) |
| `OneMindLayers` | Layer registry from plugin manifests, snapshot plus delta, batched renderers (Niagara far, ISM mid, actors near), LOD, clustering | patterns from worldwideview (design only) and gods-eye-view (MIT) |
| `OneMindEntities` | SoA entity cache, inspector, history, symbology | — |
| `OneMindCoT` | CoT parse and emit over NATS; later Commo (C4, blocked by spike S4) | VR-TAK `VH_COT`, atak-civ `commoncommo` |
| `OneMindMission` | AOIs, geofences, missions, tasks, command proposals with confirm | VR-TAK `VTAK_Mensuration`, `VTAK_Orbit` |
| `OneMindHermes` | Chat, voice, map context, action proposals | — |
| `OneMindVideo` | Stream panels tied to entities; HLS now, ≤ 1 s native decoder later; footprint and drape | gods-eye-view monitor-plane pattern (MIT) |
| `OneMindXR` | OpenXR interaction, headset UI | VR-TAK `VH_VR` (rewritten) |
| `OneMindStreaming` | Pixel Streaming 2 setup, web front end, input mapping | Epic Pixel Streaming 2 |

Sources: [unreal-platform.md](research/unreal-platform.md),
[vrtak-porting-map.md](research/vrtak-porting-map.md),
[video-and-plugins.md](research/video-and-plugins.md).

## Deployment targets (by level)

| Level | Target | Notes |
|---|---|---|
| L2 | macOS native (owner's Apple Silicon Mac) | First target. UE 5.8 + Cesium for Unreal v2.30.0 (Fab, signed), Xcode 26.0–26.3 (not 26.4). Not buildable on the build machine (no UE, 25 GiB free, [PREFLIGHT.md](PREFLIGHT.md)); spike S1 is an owner-checklist step |
| L1 | Pixel Streaming 2 from the Mac | Unproven on macOS (contradictory docs, one flat-colour report). If S2 fails, decision 0004 #1: L0 becomes the web path and a Linux GPU host is documented, not provisioned |
| L0 | Light web client | CesiumJS globe and flat 2D on one layer catalog, PWA; nats.js over NATS WebSocket into the `CLIENT` account with Authentik login; WHEP or LL-HLS video. Not a disconnected client (decision 0002) |
| L3 | Meta Quest Pro | Standalone Android/OpenXR build with Epic's OpenXR plugin (Meta XR plugin off: fails to load in 5.8), WebSocket bus transport, 72 fps budget. Fallbacks: WebXR with L0, then streaming (decisions 0003, 0004 #2) |
| L4 | TAK clients | Unchanged; they get the mission tier and video URLs over TAK Server (tailnet) and the mesh. Later an `OneMindATAK` plugin |

## Plugins

New sources, layers, actions and Hermes skills ship as plugins with one manifest
(`onemind-plugin.yaml`): NATS permissions are generated from the subjects it declares, CI checks
them against the locked grammar, and actions always pass policy and human confirm
([video-and-plugins.md](research/video-and-plugins.md) "Plugin model").

## Data contracts

The client depends on the contracts in [`contracts/`](contracts/): `entity.md`,
`layer-snapshot.md`, `layer-delta.md`, `mission-task.md`, `asset-command.md` (written in M0),
plus `cot-projection.md` and `robotics-cot-profile.md` (planned). Each one names the omos file
that is the source of truth for its subjects and buckets, or marks it proposed.
