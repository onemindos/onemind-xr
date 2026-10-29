# Architecture

```
 SOURCES                      OMOS PLATFORM (cloud hub + edge leaves)                 ONEMIND XR CLIENT
 ───────                      ──────────────────────────────────────                 ─────────────────
 public feeds ──► Redpanda Connect / Python feeds ──► intel.* tel.* det.* ─┐
 drones (MAVLink) ─► bridges ──────────────────────► tel.fleet.* ──────────┤
 robots (ROS 2) ──► bridges ───────────────────────► tel.fleet.* ──────────┤
 home sensors ────► bridges ───────────────────────► tel.* ────────────────┤
 TAK mesh ◄──────► TAK mesh gateway (edge) ◄───────► tak.cot.mesh.rx/tx ───┤
 TAK Server ◄─────► NatsBridgePlugin + cot-tap ◄───► tak.cot.out / inject ─┤
                                                                            ▼
                                       ENTITY SERVICE (Make Sense) ──► ent.{type}.{id}.state
                                              │                         KV layer snapshots + deltas
                                              │                                │
                          projector (AOI, geofence, tasking filter)            ├──► OneMindBus (NATS) ──► layers ──► Cesium globe
                                              │                                │
                                              ▼                                ├──► Hermes / agents (svc.*, agents.*)
                                 tak.cot.inject + tak.cot.mesh.tx              │
                                   (mission tier only)                         └──► cmd.* ◄── missions, tasks, asset commands
```

## Layers

1. **Sources and bridges (omos).** Feeds and device bridges publish observations on
   grammar subjects. No feed publishes CoT for TAK directly (feature A1).
2. **Bus (omos).** In-cluster NATS JetStream is the authority. Edge sites (the Mac,
   the home server, field kits) run leaf nodes. Streams and KV buckets are CRs in
   `platform/nats/streams/`.
3. **Make Sense (omos).** The entity service fuses observations into entities and
   keeps per-layer snapshots in KV. This is what clients and Hermes read.
4. **Projection to TAK (omos).** The projector sends only the mission tier as CoT,
   to TAK Server (`tak.cot.inject`) and to the mesh (`tak.cot.mesh.tx`).
5. **Client (this repo).** Unreal 5.8 with Cesium for Unreal. It reads snapshots and
   deltas, renders layers in batches, and sends commands. It is TAK-aware from day
   one through the bus, and gains its own Commo link later (C4).
6. **Act.** Commands (`cmd.*`) from people and Hermes pass policy and audit (I4),
   go to bridges and TAK, and come back as events (`evt.*`).

## Unreal plugins

| Plugin | Responsibility | Donor code |
|---|---|---|
| `OneMindBus` | NATS connection, KV, subscribe, request/reply, reconnect | nats.c (C client) |
| `OneMindGlobe` | Cesium for Unreal setup, tilesets, offline cache, geodesy helpers | VR-TAK `VH_Globe` (UTM, MGRS, EGM96) |
| `OneMindLayers` | Layer registry, snapshot plus delta, batched renderers, LOD, clustering | patterns from worldwideview (design only) and gods-eye-view (MIT) |
| `OneMindEntities` | Entity cache, inspector, history, symbology | — |
| `OneMindCoT` | CoT parse and emit; later Commo (C4) | VR-TAK `VH_COT`, atak-civ `commoncommo` |
| `OneMindMission` | AOIs, geofences, missions, tasks, asset commands | VR-TAK `VTAK_Mensuration`, `VTAK_Orbit` |
| `OneMindHermes` | Chat, voice, map context, action proposals | — |
| `OneMindXR` | OpenXR interaction, headset UI | VR-TAK `VH_VR` |
| `OneMindStreaming` | Pixel Streaming 2 setup, web front end, input mapping | Epic Pixel Streaming 2 |

## Deployment targets (by level)

| Level | Target | Notes |
|---|---|---|
| L2 | macOS native (owner's Apple Silicon Mac) | First target. UE runs natively on Apple Silicon since 5.2. Cesium for Unreal lists macOS. |
| L1 | Pixel Streaming 2 from the Mac | Unconfirmed on macOS; community reports issues. Spike first (decision 0002). |
| L1 | Pixel Streaming from a Linux GPU node | The supported path if macOS streaming fails. Needs a GPU host; omos has CPU-only pools today. |
| L0 | Light web client | CesiumJS or MapLibre on the same contracts; also a WebXR option for the headset. |
| L3 | Meta Quest Pro | Standalone Android/OpenXR build, or PC streaming; Quest Link needs Windows (decision 0003). |
| L4 | TAK clients | Unchanged; they get the mission tier over TAK Server and the mesh. |

## Data contracts

The client depends on the contracts in [`contracts/`](contracts/). Each one names
the omos file that is the source of truth. The first job of the build is to write
them (milestone M0) before any client code.
