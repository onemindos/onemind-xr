# OneMind client (Unreal Engine 5.8 + Cesium for Unreal)

Lane B of the build run (milestone M2): features B1 to B6, C1, C2 and F1 in
[`docs/FEATURES.md`](../../docs/FEATURES.md). Status on 2026-10-29: **source written, not yet
compiled in Unreal** (Unreal is not installed on the build machine, [PREFLIGHT.md](../../docs/PREFLIGHT.md)).
Everything that does not need the engine is built and tested on the host; the engine side is
**built as source, awaiting owner check** on the Mac (`docs/evidence/M2/B/OWNER-STEPS.md`).

## Layout

```
client/OneMind/
  OneMind.uproject              UE 5.8, Cesium for Unreal, six OneMind plugins; Mac first
  Config/                       DefaultEngine.ini (map, game mode, Cesium cache, bundle id), DefaultGame.ini (bus, globe, layers)
  Source/OneMind/               game module: game mode, player controller (click select, 1-9 layers, M marker)
  Plugins/
    OneMindCore/                engine-independent C++17 core, also built by CMake (HostTests/)
      NatsProtocol, NatsClient  NATS client protocol, subscribe, publish+headers, request/reply, request-many, reconnect
      WebSocketFraming          RFC 6455 framing (host transport; the engine does framing in Unreal)
      JetStream                 KV get (Direct Get + fallback), message by sequence, ordered consumer (KV watch, ENT)
      Layers                    layer sessions: watch (svc.layer.*), direct (ENT), core (interim); LayerManager
      Entity, RenderPlan        SoA entity store (seq rule, live:false, expiry, picking), inspector rows, LOD/cluster/horizon plan
      CoT, MilStd2525           VR-TAK VH_COT port (event model, 2525 ids), CoT parse/build, svc.cot.send body
      ClientCore                everything above in one object, driven by Tick(now)
      TakProto, TakStream       M4 C3: TAK Protocol v1 codec; 8089 streaming client (XML, v1 negotiation, ping), directed sends
      TakEnroll                 M4 C3: /Marti/api/tls/signClient/v2 enrollment, HTTPS over a transport (HttpExchange)
      TakContacts               M4 C8: contacts from SA, GeoChat conversations (direct, group), channel filter
      TakMissions               M4 C7: Data Sync missions through tak-client (Docs/svc-tak-mission.md, proposed)
      Drawing                   M4 B11: point, line, polygon, rectangle, circle, sector, range rings, R&B, buffer, lasso
      FeedGaps                  M4 B20: stale, failed and missing feeds from tel.feed.*.status and manifests
      AssetTasking              M4 D3: click-to-task through svc.policy.command.*, JCS params_hash, audit trail
    OneMindBus/                 UOneMindBusSubsystem + FOneMindUeWebSocketTransport (engine WebSockets module)
    OneMindGlobe/               AOneMindGlobe: georeference, ellipsoid or URL terrain, open raster overlay, sun/sky
    OneMindLayers/              UOneMindLayerSubsystem: layer catalogue, toggles, AOI from the camera, gaps panel (B20)
    OneMindEntities/            AOneMindEntityRenderer (ISM per class and band, near actors, clusters), overlay + inspector
    OneMindCoT/                 UOneMindCoTSubsystem: marker, SPI, route, geofence, chat, B11 shapes through svc.cot.send;
                                UOneMindTakSubsystem (C3/C8): own TAK stream, enrollment, contacts, chat, channels;
                                FOneMindUeTlsTransport (FSocket + Tls/OmTlsSession, OpenSSL), OneMindKeychain (Mac keychain)
    OneMindMission/             UOneMindMissionSubsystem: TAK missions (C7), click-to-task with the confirm dialog (D3)
  HostTests/                    CMake build of OneMindCore, unit + integration tests, headless client, e2e scripts
  Tools/entity_load.py          10,000 simulated entities into ENT for the render check
  Tools/mac/build-and-run.sh    owner's one command: checks, build, package, run
  Docs/svc-cot-send.md          proposed svc.cot.send body (C2)
  THIRD_PARTY.md                VR-TAK attribution and other notices
```

Every plugin lists `Mac`, `Win64`, `Linux` and `Android` in `PlatformAllowList`.

## How the pieces fit

The client is a view and an input device; the bus is the authority (repo rule 3). Each layer loads
a snapshot and then applies deltas (layer contracts); the Mac, as a trusted site client next to its
own leaf, reads small layers straight from `ENT` with an ordered consumer, and large layers through
the layer service with an AOI. The camera AOI applies only to watch-path and large layers
(`LayerSpec::UsesCameraAoi`): small and medium direct/core layers load whole and the renderer
culls them (layer-snapshot.md §1 tier rules). The client never publishes `ent.*`, `cmd.*` or `tak.cot.*`: CoT goes
out only through `svc.cot.send` to the projector, asset commands only through policy.

Transport: NATS over WebSocket from C++ (ROADMAP S5 fallback). The nats.c TCP transport named in
FEATURES B3 is not included: its build was blocked on the build machine (PREFLIGHT) and the
WebSocket path covers the Mac (local leaf's WebSocket listener) and the headset. A `nats.c`
transport can be added behind the same `om::ITransport` later.

## M4 (Act): what the client adds

Lane UE, branch `run/m4-client`: B11, B20, C3, C7, C8, D3. Keys: T task the selected `sim-*` asset
to the cursor, Y/N confirm or reject in the dialog, B drawing tools (click points, Enter sends),
G gaps panel. Console: `OneMindTakEnroll <user>` (password from `ONEMIND_TAK_ENROLL_PASSWORD`),
`OneMindTakConnect`, `OneMindTakChat <uid|All> <text>`, `OneMindChannel <team>`,
`OneMindMissionList`, `OneMindMissionOpen <name>`, `OneMindMissionAdd <name> <callsign>`,
`OneMindTask <asset> <class> <lat> <lon> <hae>`. The client speaks TAK itself only on its own
stream (C3); everything it receives there is shown, never republished to the bus. Evidence:
`docs/evidence/M4/UE/`; engine build steps: `docs/evidence/M4/UE/OWNER-STEPS.md`.

## Build and test on a host without Unreal

```bash
client/OneMind/HostTests/scripts/run-host-tests.sh     # C++17 and C++20 builds, unit, integration, ASan/UBSan, 10k load, C1/C2, C3 and D3 e2e
```

Needs CMake 3.20+, a C++17 compiler, OpenSSL 1.1.1+ (Homebrew `openssl@3` on the Mac), `nats-server`
2.11+ (per-message TTL and delete markers), Go (to build the omos services for the e2e) and a Python
venv with `nats-py` and `takproto`.

## Build and run on the Mac

`Tools/mac/build-and-run.sh` and `docs/evidence/M2/B/OWNER-STEPS.md` (S1, S5, 10,000 entities, C1, C2, F1).
The script packages **Development** by default (console, logs and trace for the owner checks);
`package-shipping` builds Shipping for the F1 install check only. The overlay's first line shows
fps, average and worst frame time, so the frame-rate checks do not need `stat`.

## Verified vs not verified

- Verified on the build machine: everything under "Evidence" in `docs/evidence/M2/B/EVIDENCE.md`.
- Not verified (needs Unreal): that the engine-side code compiles and links in UE 5.8; the
  WebSockets module behaviour on Mac (binary frames through `OnRawMessage`); the
  `BasicShapeMaterial` `Color` parameter; `/Engine/Maps/Entry` as the default map; the
  `XcodeProjectSettings.BundleIdentifier` key; frame rate. Each has an owner step.
