# Unreal platform research (M0 lane: unreal)

Date: 2026-10-29. Lane scope: spikes S1 (UE 5.8 + Cesium for Unreal), S2 (Pixel
Streaming 2 on macOS), S3 (Meta Quest Pro DK94EC standalone OpenXR/Android), S5 (NATS
client inside Unreal), and options for rendering 10,000 entities at 60 fps.

**This machine cannot run S1, S2, S3 or S5 inside Unreal.** Unreal Engine is not
installed: it needs admin rights and about 150 GB, and 25 GiB was free at preflight
(`docs/PREFLIGHT.md`; verifier `df -g` on 2026-10-29 showed 22 GiB). Each spike below is therefore a design, source notes and exact
owner steps. The only things run here are a test of the local NATS WebSocket listener
(S5 fallback) and API checks against upstream source.

## Sources

Official docs and release notes (fetched 2026-10-29):

- [S-UE58] UE 5.8 release notes: https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-engine-5-8-release-notes
- [S-MACREQ] macOS development requirements: https://dev.epicgames.com/documentation/en-us/unreal-engine/macos-development-requirements-for-unreal-engine
- [S-ANDREQ] Android development requirements: https://dev.epicgames.com/documentation/en-us/unreal-engine/android-development-requirements-for-unreal-engine
- [S-PSREF] Pixel Streaming reference (UE 5.8 docs): https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-engine-pixel-streaming-reference
- [S-PS2] Pixel Streaming 2 overview: https://dev.epicgames.com/documentation/en-us/unreal-engine/pixel-streaming-2-overview-in-unreal-engine
- [S-ISM] Instanced Static Mesh component (UE 5.8 docs): https://dev.epicgames.com/documentation/en-us/unreal-engine/instanced-static-mesh-component-in-unreal-engine
- [S-NDC] Niagara Data Channels overview: https://dev.epicgames.com/documentation/en-us/unreal-engine/niagara-data-channels-overview
- [S-NSAAS] Niagara systems as a service: https://dev.epicgames.com/documentation/en-us/unreal-engine/niagara-systems-as-a-service
- [S-MASS] Mass Entity overview: https://dev.epicgames.com/documentation/en-us/unreal-engine/overview-of-mass-entity-in-unreal-engine
- [S-MASSGP] Mass Gameplay overview: https://dev.epicgames.com/documentation/en-us/unreal-engine/overview-of-mass-gameplay-in-unreal-engine
- [S-CQ] Cesium "Build for Quest": https://cesium.com/learn/unreal/build-for-quest/
- [S-MPREQ] Meta Unreal prerequisites: https://developers.meta.com/horizon/documentation/unreal/unreal-quick-start-install-prerequisites/
- [S-MCOMPAT] Meta OS compatibility mode (Unreal), updated 2026-10-07: https://developers.meta.com/horizon/documentation/unreal/os-compatibility-mode/
- [S-MDEV] Meta device setup (developer mode): https://developers.meta.com/horizon/documentation/native/android/mobile-device-setup/
- [S-MMATRIX] Meta Unreal compatibility reference: https://developers.meta.com/horizon/documentation/unreal/unreal-compatibility-matrix/
- [S-MWEBXR] Meta WebXR overview: https://developers.meta.com/vr/documentation/web/webxr-overview/

Source code and trackers (via `gh`, 2026-10-29):

- [G-CES-REL] `gh release list -R CesiumGS/cesium-unreal`: latest release **v2.30.0, published 2026-09-29**. Its assets include `CesiumForUnreal-58-v2.30.0.zip` (1,262,445,524 bytes) and `-56-`/`-57-` builds.
- [G-CES-CHG] `CesiumGS/cesium-unreal` `CHANGES.md` on `main`: v2.28.0 "Added support for Unreal Engine 5.8"; v2.29.0 "Removed support for Unreal Engine 5.5. Unreal Engine 5.6 or later is now required." It also has a **v2.31.0 entry dated 2026-11-02 with no release yet**, so it is unreleased.
- [G-CES-UPL] `CesiumForUnreal.uplugin` on `main`: `"SupportedTargetPlatforms": [ "Win64", "Mac", "Linux", "Android", "IOS" ]`.
- [G-CES-GEO] `Source/CesiumRuntime/Public/CesiumGeoreference.h` on `main`: `TransformLongitudeLatitudeHeightPositionToUnreal`, `TransformEarthCenteredEarthFixedPositionToUnreal`, `ComputeEarthCenteredEarthFixedToUnrealTransformation`, `ComputeEastSouthUpToUnrealTransformation`, `GetDefaultGeoreference`.
- [G-CES-ISS] Open cesium-unreal issues: #1484 "Our macOS release binaries are not signed" (the issue says the Launcher binaries *are* signed); #1850 "CesiumRuntime crash launch in DebugGame configuration on mac" (2026-06-02; Development config works); #1712 "Quick Launch on Android device crashes on startup" (the thread says a full package works); #913 "Large seams appearing on globe when packaged for Android"; #1196 "Visual artifacts with Cartographic Polygons on Mac".
- [G-NATSC] `nats-io/nats.c`: latest release **v3.14.0 (2026-09-17)**, Apache-2.0. README: "There is support for Mac OS/X, Linux and Windows" (Android and iOS not mentioned). JetStream since v3.0.0; KeyValue supported. `CMakeLists.txt` at v3.14.0: `NATS_BUILD_STREAMING` default **OFF** (the README text saying "on by default" is stale), `NATS_BUILD_WITH_TLS` ON, `NATS_BUILD_LIB_STATIC` ON, `NATS_BUILD_USE_SODIUM` OFF. Its only Android handling skips `-lrt` (`if(NOT ANDROID)`). OpenSSL ≥ 1.1.1 is required for TLS.
- [G-NATSC-H] `src/nats.h` at v3.14.0: `natsConnection_Connect`, `natsOptions_SetURL`, `natsOptions_SetUserInfo`, `natsOptions_SetSecure`, `natsOptions_SetMaxReconnect`, `natsOptions_SetReconnectWait`, `natsOptions_SetRetryOnFailedConnect`, `natsOptions_SetDisconnectedCB`, `natsOptions_SetReconnectedCB`, `natsConnection_Subscribe`, `natsConnection_Request`, `natsConnection_JetStream`, `js_KeyValue`, `kvStore_Get`, `kvStore_Watch`, `kvStore_WatchAll`, `kvWatcher_Next`, `kvEntry_Key/Value/ValueLen/Revision/Operation`. The header contains **0** occurrences of "websocket".
- [G-NATSC-371] nats.c issue #371 "How to use WebSocket as transport in the NATS C client library" (closed): the maintainer says "There is no plan at the moment". #540 "WebTransport support" is open.
- [G-PSI] `EpicGamesExt/PixelStreamingInfrastructure` has a **`UE5.8` branch**. On it: `NODE_VERSION` = `v22.14.0`; `SignallingWebServer/platform_scripts/bash/` has `setup.sh`, `start.sh`, `start_with_stun.sh`, `start_with_turn.sh`; `common.sh` downloads `node-$NODE_VERSION-darwin-arm64.tar.gz` on Apple Silicon and a Mac arm64 coturn build. `config.json` defaults: `streamer_port` 8888, `player_port` 80, `sfu_port` 8889. `src/index.ts` options include `--player_port`, `--streamer_port`, `--serve`. `start.sh` calls `set_public_ip`, which runs `curl https://api.ipify.org` unless you pass `--publicip`. The frontend has `Frontend/library/src/WebXR/WebXRController.ts` and an "immersive-vr" auto-start setting.
- [F-PS-MAC] UE forum, 2026-06-21: "Pixel Streaming (UE 5.7, Mac): stream connects fine but browser shows flat colour instead of the scene". Apple Silicon M4, PS2, UE5.7 infrastructure branch, Chrome and Safari; **no replies**. https://forums.unrealengine.com/t/pixel-streaming-ue-5-7-mac-stream-connects-fine-but-browser-shows-flat-colour-instead-of-the-scene/2730308
- [F-META-58] Meta developer feedback "Meta XR plugins version mismatch with Unreal Engine 5.8": status **Investigating**, no workaround. OculusXR, OculusInteraction, MetaXRHaptics and OculusPlatform are "built for UE 5.7.0" and fail to load modules in 5.8. https://developers.meta.com/horizon/feedback/vr/investigations/1053459897316880/
- [F-META-MAC] Meta forum, 2025-08-13: Meta XR plugin on macOS (Meta fork and stock UE) crashes at the splash screen on a Quest 3, "typically a shader crash"; stock UE **without** the plugin launches; 0 replies. https://communityforums.atmeta.com/discussions/dev-unreal/meta-xr-on-mac-os---meta-fork--ue-standard-install-both-failing/1340433
- [F-MASS-BENCH] UE forum, 2026-10-25: "Profiling Mass Representation: Native ISM vs Bulk ISM Updates vs NDC". UE 5.8.4, packaged Development build, Windows 11 DX12, RTX 3080, 20,000 entities. CPU frame time added over a movement-only baseline: native Mass ISM +6.719 ms, custom bulk ISM +2.994 ms, NDC +1.337 ms. Preliminary, no GPU timings. https://forums.unrealengine.com/t/profiling-mass-representation-native-ism-vs-bulk-ism-updates-vs-ndc/2844794
- [T-THESIS] Master's thesis (FH OÖ), "Rendering and AI Optimization in RTS games": ISM renderer fine to about 10,000 units, with GPU time the limit above that; Niagara better to 50,000+. Known only from the search-result abstract. https://pure.fh-ooe.at/en/studentTheses/rendering-and-ai-optimization-in-rts-games-enhancing-performance-/

Command output on this machine:

- [C-WS] Local NATS WebSocket probe, 2026-10-29. `.run/nats.conf` has `websocket { listen: "127.0.0.1:9222", no_tls: true }`. Using `nats` CLI v0.5.1 with `-s ws://127.0.0.1:9222`, a throwaway bucket `unreal_lane_probe` was created, then `kv put`/`kv get` of key `ent.sim-x1.state` returned `{"lat":40.7,"lon":-74.0}`. A `sub 'tel.sim-x1.>'` over WebSocket received `{"probe":1}` published on `nats://127.0.0.1:4222` as `tel.sim-x1.drone.position`. The bucket was deleted afterwards (`kv ls` count 0).
- [C-DENIED] Building nats.c v3.14.0 from the release tarball with CMake (static, TLS against Homebrew OpenSSL 3, streaming off) was **denied by this machine's agent permission policy** ("Code from External"). Recorded as a blocker; the downloaded tarball was deleted. SHA-256 of `v3.14.0.tar.gz`: `1f8b450bc295d0c94be201e34713ca0b515aae2c0d1b279273c3e6e0e72fe005`.
- [C-OMOS] `omos/platform/nats/streams/kv-world-state.yaml`: bucket `world-state`, replicas 3, history 1, ttl 5m. `kv-fleet-state.yaml`: bucket `fleet-state`, history 3, ttl 30s.

## Verified facts

### Engine and host (S1)

1. UE 5.8 is released. A UE 5.8.4 build is in use by 2026-10-25 [F-MASS-BENCH].
2. UE 5.8 on macOS needs at least **macOS Sonoma 14.5** (recommended macOS is inconsistent on the page: the hardware table says "Latest macOS Sequoia 14", the 5.8 version-history row says "Latest macOS Sequoia 15"; verifier re-fetch 2026-10-29) and at least **Xcode 26.0**; **26.1.1 is recommended**, and the page says "**Xcode 26.4 is not compatible with Unreal Engine**". It recommends an M3 and 32 GB, with a minimum of "M1 or M2 depending on rendering features" and 16 GB [S-MACREQ]. The release notes say supported Xcode versions "were downgraded from UE 5.6" [S-UE58]; the stated reason (compile and link issues with the latest versions) is **unverified** (verifier fetch confirmed only the quoted phrase).
3. UE 5.8 Android: NDK **r27c**; SDK 35 recommended, SDK 34 the minimum for compiling (and the default shipping target); build-tools 35.0.1; OpenJDK 21.0.3 [S-ANDREQ][S-UE58]. Gradle 8.7 and "target android-26" for the NDK level come from the release notes only; the requirements page does not state them [S-UE58]. 5.8 adds Turnkey automated Android SDK setup [S-UE58].
4. UE 5.8 supports OpenXR 1.0 and 1.1 runtimes, and either can be disabled in project settings [S-UE58].
5. Cesium for Unreal v2.30.0 is the latest release and ships a UE 5.8 binary build. It needs UE 5.6 or later, and its plugin lists Mac and Android as target platforms [G-CES-REL][G-CES-CHG][G-CES-UPL].
6. Cesium's macOS GitHub release binaries are unsigned, while the Epic Launcher (Fab) binaries are signed [G-CES-ISS #1484]. A DebugGame-Editor configuration crashes on Mac; Development works [#1850].
7. Cesium's georeference API converts lon/lat/height and ECEF to Unreal coordinates relative to the georeference's parent frame [G-CES-GEO]. This is the entry point for placing instanced entities without a per-actor globe anchor.

### Pixel Streaming 2 on macOS (S2)

8. The UE 5.8 Pixel Streaming reference contradicts itself: "The Pixel Streaming Plugin supports Windows, Linux, and Mac platforms. OSX is not supported." It was tested only on Windows 10/11, Ubuntu and "MacOS Ventura 13.5.1". Macs need "hardware that supports the use of the VideoToolbox framework". Codecs are H.264 (default), VP8, VP9 and AV1, with AV1 requiring an NVIDIA Ada GPU. Flags: `-PixelStreamingURL=ws://…`, `-RenderOffscreen`, `-PixelStreamingEncoderCodec` [S-PSREF].
9. PS2 keeps "all the same steps and launch arguments" as PS1, and Epic advises using the infrastructure branch that matches the engine version [S-PS2]. A `UE5.8` branch exists, and its bash scripts support macOS arm64 (Node v22.14.0, a Mac arm64 coturn) [G-PSI].
10. The only recent Mac PS2 report found (UE 5.7, M4) shows a flat colour in the browser while WebRTC stats look healthy. It has no fix [F-PS-MAC].
11. The PS2 frontend on `UE5.8` ships a WebXR controller ("immersive-vr"), so the headset browser can view a Pixel Stream in VR [G-PSI].

### Meta Quest Pro (S3)

12. Meta lists macOS as a supported OS for developing Horizon OS apps in Unreal. The Meta XR Simulator is Windows-only [S-MPREQ].
13. Quest Pro's manifest identifier is `questpro`. Meta's example manifest value is `quest2|questpro|quest3|quest3s|vrglasses`. In the Meta XR plugin, the "Supported Meta Quest devices" setting generates this list [S-MCOMPAT], and the page was updated 2026-10-07, so Quest Pro is still a listed target.
14. The Meta XR plugins currently fail to load in UE 5.8 (version mismatch, "Investigating") [F-META-58]. A 2025 report says the Meta XR plugin crashes at the splash screen when built on macOS, while stock UE without it launches [F-META-MAC].
15. Cesium's Quest guide uses **Epic's OpenXR plugin with the Oculus VR plugin unchecked**. Its settings: arm64, Vulkan on, OpenGL ES 3.2 off, Forward Shading, MSAA 4x, Mobile HDR off, Mobile Multi-View on, experimental occlusion culling on, and Maximum Screen Space Error 256 on terrain. The guide is old: it uses SDK 29, "Package for Oculus Mobile devices" and a `.bat` installer [S-CQ].
16. Enabling developer mode is done in the Meta Horizon phone app (Headset Settings, Developer Mode); it requires membership of a developer team on the Meta Horizon Developer Dashboard and a verified account, and the headset must allow USB debugging [S-MDEV].
17. Meta recommends feature detection for WebXR on Quest rather than user-agent sniffing [S-MWEBXR].

### NATS in Unreal (S5)

18. nats.c v3.14.0 (Apache-2.0 per `gh api repos/nats-io/nats.c/license`; the FSF lists Apache-2.0 as "compatible with version 3 of the GNU GPL", https://www.gnu.org/licenses/license-list.html) supports macOS, with JetStream and KV (`js_KeyValue`, `kvStore_Get`, `kvStore_Watch`, `kvWatcher_Next`), reconnect options, user and password, and TLS through OpenSSL ≥ 1.1.1 [G-NATSC][G-NATSC-H].
19. nats.c has **no WebSocket transport**, and the maintainers have no plan to add one [G-NATSC-H][G-NATSC-371].
20. The local nats-server WebSocket listener (127.0.0.1:9222) serves KV get/put and subject subscriptions to a WebSocket client, and messages cross between the TCP and WebSocket listeners [C-WS].
21. omos KV buckets the client will read include `world-state` (history 1, ttl 5m) and `fleet-state` (history 3, ttl 30s) [C-OMOS].

### Rendering 10,000 entities

22. ISM vs HISM per the UE 5.8 docs: HISM suits "1000s of instances that don't move", but "if you have many instances that are not completely static, errors can arise … using HISMs". ISM culls and LODs each instance on the GPU. In Nanite-only projects "ISM is always the choice". An instance costs about 64 B of GPU memory against 672 B for a primitive [S-ISM].
23. Niagara Data Channels move data from game code into Niagara systems through a defined payload, and Epic recommends them for combining many bursts into one shared simulation [S-NDC][S-NSAAS].
24. Mass: MassEntity is a data-oriented framework (fragments, stateless processors, archetypes, chunks) [S-MASS]. Mass Representation gives each LOD a choice of high-res actor, low-res actor, ISM or nothing, and ISM animation is experimental [S-MASSGP]. UE 5.8 moved Mass Signals into the core engine, allows creating entities off the game thread and adds a MassCore module [S-UE58].
25. In one UE 5.8.4 measurement at 20,000 entities (Windows, RTX 3080), NDC added the least CPU frame time (+1.3 ms), then custom bulk ISM (+3.0 ms), then native Mass ISM (+6.7 ms). It is preliminary and has no GPU numbers [F-MASS-BENCH].

## How it applies to OneMind

### S1: UE 5.8 + Cesium for Unreal (design)

- **Engine/plugin pair:** UE 5.8 with Cesium for Unreal v2.30.0, installed from Fab through the Epic Launcher so the binaries are signed (fact 6). This meets decision 0004 #14. The next-older fallback pair is UE 5.7 with v2.30.0 (`-57-` asset exists).
- **Xcode pin:** the owner's Mac must have Xcode 26.0 to 26.3, ideally 26.1.1, and **not 26.4** (fact 2). This is the most likely S1 failure on an up-to-date Mac, so the checklist checks it first.
- **Basemap:** decision 0004 #4 means no Cesium ion by default. The globe is a `Cesium3DTileset` for terrain or WebODM 3D Tiles plus a raster overlay from an open tile source, with an ion token only if one is in the keychain. Picking open terrain and imagery endpoints belongs to the ingestion and tiles lanes.
- **Configuration:** Development and Shipping only, never DebugGame on Mac (#1850). Every `OneMind*` plugin lists `Mac`, `Win64`, `Linux` and `Android` in `PlatformAllowList` (run rule). Cesium covers all of these: its `.uplugin` declares `SupportedTargetPlatforms` Win64, Mac, Linux, Android and IOS [G-CES-UPL], so it is a superset, not the same list or the same field.
- **Placement:** convert positions with `ACesiumGeoreference::TransformLongitudeLatitudeHeightPositionToUnreal` (or the ECEF variant, batched through `ComputeEarthCenteredEarthFixedToUnrealTransformation`). Instances live in the georeference's frame: the ISM/Niagara component is attached under the georeference actor, so origin rebasing (sub-levels, origin shifting) moves all instances in one transform (fact 7).

### S5: NATS client inside Unreal (design)

Plugin `OneMindBus` (feature B3), runtime module, `PlatformAllowList: Mac, Win64, Linux, Android`:

```
Plugins/OneMindBus/
  OneMindBus.uplugin
  Source/ThirdParty/NatsC/            # External module: prebuilt libnats_static.a per platform + include/
    NatsC.Build.cs                    # Type = ModuleType.External; PublicIncludePaths; PublicAdditionalLibraries per Target.Platform
    Mac/lib/libnats_static.a          # built by scripts/unreal/build-natsc-mac.sh (owner step), arm64
    Android/arm64-v8a/lib/libnats_static.a   # unverified path; built with NDK r27c toolchain (owner step)
    include/nats/*.h
    LICENSE (Apache-2.0) + NOTICE
  Source/OneMindBus/
    Public/IOneMindTransport.h        # Connect, Subscribe(subject), KvGet(bucket,key), KvWatch(bucket,keyFilter), Request
    Private/NatsCTransport.cpp        # nats.c implementation (Mac/Win/Linux; Android if the NDK build works)
    Private/WsNatsTransport.cpp       # fallback: NATS text protocol over UE WebSockets module (core subs + KV via $JS.API)
    Private/OneMindBusSubsystem.cpp   # UGameInstanceSubsystem; owns transport, reconnect state, metrics
    Private/MessageQueue.h            # TQueue<FBusMsg, EQueueMode::Mpsc>, drained on game thread in Tick
```

Rules for the design:

- nats.c callbacks run on nats.c's own threads. They only enqueue into a lock-free MPSC queue. The game thread drains a bounded number of messages per frame (a frame-time budget) and applies them to entity storage. No UObject is touched off the game thread.
- **Connection:** on the Mac the client connects to the **local leaf** at `nats://127.0.0.1:4222` (omos edge pattern: bridges and clients use a local leaf that forwards to the cluster). TLS therefore stays out of the in-process path on the Mac, which avoids linking a second OpenSSL next to the engine's own (see Risks). The credentials come from the keychain or environment, never Git (run rule 4).
- **Subjects:** within the locked grammar only. Read latest state with `kvStore_WatchAll`/`kvStore_Watch` on `world-state`/`fleet-state` (fact 21), for example key filters by entity type. Read deltas from `ent.{entity_type}.{entity_id}.state`, `tel.{node}.{class}.{metric}`, `det.*` and `evt.*` subjects as the layer contract (`docs/contracts/layer-*.md`, written by the contracts step) defines. Commands go out as `cmd.{domain}.{entity}.{action}` only after the UI human-confirm step (run rule 6). Layer off means the watcher or subscription is destroyed (B4 acceptance).
- **Build options for nats.c:** `-DNATS_BUILD_STREAMING=OFF -DNATS_BUILD_EXAMPLES=OFF -DBUILD_TESTING=OFF -DNATS_BUILD_LIB_SHARED=OFF -DNATS_BUILD_WITH_TLS=OFF` for the Mac local-leaf build. A TLS build is a second variant only if a remote connection is needed.
- **Fallback (ROADMAP S5 fallback):** `WsNatsTransport` speaks the NATS client protocol over `ws://`/`wss://` to the server's WebSocket listener. This is the route for the Quest standalone build if nats.c does not build for Android, or if the headset must reach the bus without a leaf. Fact 20 proves the server side works for KV and subscriptions. The client side (NATS protocol framing, JetStream KV get through `$JS.API.DIRECT.GET.<bucket>` and watch through an ordered push consumer) must be written and tested in Unreal. Treat that cost as **unverified**.
- **Test without Unreal:** the transport's protocol logic can be unit-tested outside the engine, and `scripts/sim/mavlink_sim_drone.py` plus the local nats-server provide live data. The Phase-2 client lane can add a plain C++ test harness against `libnats` once a build of nats.c is allowed on the build machine (blocked here, C-DENIED).

### S2: Pixel Streaming 2 from the Mac (design)

- Run the PS2 plugin in the packaged `.app` with the `UE5.8` infrastructure branch's signalling server on the same Mac (facts 8–9). H.264 is the codec (VideoToolbox), and the browser goes to `http://<mac-lan-ip>:8080`.
- Run with `--publicip <LAN IP>` so the script does not call `api.ipify.org`, and use `--player_port=8080` to avoid port 80.
- The known risk is the flat-colour picture [F-PS-MAC]. The checklist runs both windowed and `-RenderOffscreen`, then a second resolution, and records which one shows the scene. If none does, decision 0004 #1 applies: the L0 CesiumJS client becomes the web path, and a Linux NVIDIA streaming host is documented, not provisioned.

### S3: Quest Pro standalone (design)

- **Path:** Epic's built-in **OpenXR** plugin only. **Do not enable the Meta XR plugin** for now: it fails to load in 5.8 [F-META-58] and has a macOS splash-screen crash report [F-META-MAC]. Cesium's own Quest guide also uses OpenXR without the Oculus plugin [S-CQ]. Eye-tracked foveation and other Meta-only features are out of scope until Meta fixes 5.8.
- **Build host:** the owner's Mac packages for Android (fact 12). Quest Link does not exist on macOS, so there is no VR Preview. Every test is a packaged APK installed with `adb`.
- **Budget:** 72 fps on the headset (FEATURES J3). The headset target is a reduced layer budget, not 10,000 entities, until measured: Cesium SSE 256 and occlusion culling (fact 15), Forward Shading, Multi-View, MSAA 4x, no Lumen, Nanite or virtual shadow maps.
- **Fallbacks (decision 0004 #2):** first WebXR in the headset browser through the L0 CesiumJS client, then Pixel Streaming into the headset browser through the PS2 frontend's WebXR controller (fact 11), which works only if S2 works on the Mac.

### 10,000 entities at 60 fps (B4/B5)

| Option | Fits | Against | Verdict |
|---|---|---|---|
| One actor per entity with `CesiumGlobeAnchorComponent` | Simple, full interactivity | ~10,000 primitives at 672 B GPU each plus per-actor tick and transform cost (fact 22) | Only for the ≤ 50–200 nearest or selected entities |
| HISM | Static instances, cluster culling | Entities move every update, and the docs warn of errors with non-static instances (fact 22) | Only for static overlays (structures, POIs) |
| ISM, one component per symbol class, bulk transform updates | Moving instances, GPU per-instance culling/LOD, Nanite-compatible, per-instance custom data for colour/affiliation | GPU cost limits it above ~10k (T-THESIS); per-instance update path costs CPU (F-MASS-BENCH +3.0 ms bulk at 20k) | **Primary for 3D models at mid range** |
| Niagara with Data Channels or a Niagara Data Interface array (sprite/mesh renderer) | Lowest CPU cost in the only 5.8 measurement (+1.3 ms at 20k); scales to 50k+ (T-THESIS); billboards for icons at range | Picking/selection needs a CPU-side spatial index (Niagara particles are not hit-testable); NDC data is per-frame transient (forum reports, unverified) | **Primary for far-range icons/points** |
| Mass Entity + Mass Representation (LOD → actor / ISM / none) | Built-in LOD switching between actor near and ISM far; 5.8 adds off-game-thread entity creation | Native Mass ISM was the slowest of the three in the 5.8 measurement (+6.7 ms); ISM animation experimental (facts 24–25) | Not needed for 10k; revisit for H5 swarms or simulation |

A hybrid falls out of this. Entity state lives in a plain C++ SoA store (not UObjects) owned by `OneMindEntities`. Each frame, entities split by camera distance and screen size into Niagara icons (far), ISM models (mid) and actors (near or selected), with clustering above a density threshold. A CPU spatial grid on the SoA store handles picking for the inspector (B6). This meets B5 ("icons far away and models near; no frame drop when a layer loads"): layer loads go through the off-thread queue and are applied in budgeted batches.

## Risks

1. **Xcode version drift** (high): an owner Mac on Xcode 26.4 or later cannot build UE 5.8 (fact 2). Mitigation: checklist step 1 checks `xcodebuild -version` and installs 26.1.1 beside it from Apple's developer downloads.
2. **PS2 on Mac shows the wrong picture** (high): the only recent report has no fix (fact 10), and Epic's page contradicts itself (fact 8). Mitigation: decision 0004 #1 fallback is ready.
3. **Meta XR plugin unusable in 5.8** (high, known): OpenXR-only path. Meta-specific features (passthrough APIs, eye-tracked foveation, MQSR) are unavailable on it. Whether Epic's OpenXR plugin alone sets `com.oculus.supportedDevices` to include `questpro` is **unverified**. Without it, Horizon OS may run the app in compatibility mode (S-MCOMPAT). Check: `aapt dump xmltree` on the APK (checklist).
4. **Cesium on Android** (medium): open issues include #1712 (Quick Launch crash, full package works) and #913 (seams when packaged). Mitigation: always do full packages, never Quick Launch.
5. **nats.c on Android** (medium): upstream claims only macOS, Linux and Windows. An NDK build is untested (**unverified**). Mitigation: the WebSocket transport fallback.
6. **OpenSSL symbol clash** (medium, unverified): UE ships its own OpenSSL third-party module (version not verified for 5.8). Statically linking another OpenSSL through nats.c could clash. Mitigation: Mac build with TLS off against the local leaf; a TLS variant must link against the engine's OpenSSL.
7. **Quest Pro performance** (medium): no Quest Pro measurement of Cesium for Unreal was found. Cesium community reports cite slow tile loading on Quest 3 (search summary, unverified). The headset budget must be measured in S3.
8. **Quest Pro lifecycle** (low/unknown): Meta docs (2026-10-07) still list `questpro`. No end-of-support date was found (**unverified**).
9. **Disk:** this machine cannot hold UE. All Unreal proof is owner-side. The owner Mac needs about 150 GB free (run prompt) plus Android SDK/NDK (size **unverified**).

## Recommendation

1. **S1:** UE 5.8 (latest 5.8.x hotfix) with Cesium for Unreal v2.30.0 from Fab, Xcode 26.1.1, Development/Shipping only, open basemap. Fall back to the UE 5.7 + v2.30.0 pair only if S1 fails on 5.8.
2. **S5:** nats.c v3.14.0 as a static ThirdParty module, TLS off, connected to the local leaf on `127.0.0.1:4222`, with callbacks marshalled through an MPSC queue to the game thread. Hide the transport behind `IOneMindTransport` from day one, and write the WebSocket transport as the Android/Quest route (it also covers the ROADMAP S5 fallback). The server side is already proven (C-WS).
3. **S2:** run the PS2 checklist once S1 passes. Accept it only on the stated criteria (live picture, < 150 ms input on LAN). On failure, apply 0004 #1 and make the L0 CesiumJS client the web path.
4. **S3:** Epic OpenXR-only Android build, packaged on the Mac and sideloaded with `adb`. Keep the Meta XR plugin disabled until F-META-58 closes. Measure fps with the globe and one layer. On failure, fall back to WebXR in the headset browser with the L0 client.
5. **Rendering:** a hybrid Niagara (far icons), ISM (mid models) and actors (near/selected) over a C++ SoA entity store with a CPU spatial grid for picking. Use HISM only for static overlays; defer Mass until swarms (H5). Lane B proves 10k at 60 fps on the Mac with a synthetic load (10,000 `sim-*` entities from a local publisher) and `stat unit` / Unreal Insights captures saved as evidence.

## Owner checklist steps

Run on the owner's Apple Silicon Mac unless a step says "headset". Set these once per
Terminal window. The UE project file is created in Phase 2 by lane B, so
`OneMind.uproject` below is the planned name.

```bash
export WORK="$HOME/work"                       # folder holding onemind-xr/ and omos/
export OM="$WORK/onemind-xr"
export UE="/Users/Shared/Epic Games/UE_5.8"    # Epic Launcher default install path; change if installed elsewhere
export UPROJECT="$OM/OneMind.uproject"         # planned; use the path lane B commits
```

### U0: Host checks (before installing anything)

1. `sw_vers -productVersion`. Expected: `14.5` or higher.
2. `uname -m`. Expected: `arm64`.
3. `xcodebuild -version`. Expected: `Xcode 26.0` to `26.3` (26.1.1 recommended). If it shows **26.4 or later**, download Xcode 26.1.1 from https://developer.apple.com/download/all/ (Apple ID sign-in), put it in `/Applications/Xcode-26.1.1.app`, then run `sudo xcode-select -s /Applications/Xcode-26.1.1.app` and repeat. (Unreal 5.8 does not support Xcode 26.4, per Epic's macOS requirements page.)
4. `df -h "$HOME" | tail -1`. Expected: at least 200 GiB available (Unreal about 150 GB, plus Android SDK/NDK and build output).
5. `sysctl -n hw.memsize | awk '{print $1/1073741824 " GiB"}'`. Expected: 16 or more (32 recommended).

### U1 (S1): Install Unreal 5.8 and Cesium for Unreal

1. Install the Epic Games Launcher from https://www.unrealengine.com/download and sign in. Open **Unreal Engine > Library > Engine Versions > +**, choose **5.8.x** (latest), then **Install**. Under **Options**, tick **Android** if you will do U4. Expected: `ls "$UE/Engine/Binaries/Mac/UnrealEditor.app"` prints the bundle.
2. In the Launcher, open **Fab**, search **Cesium for Unreal**, then **Install to Engine 5.8**. Expected: `ls "$UE/Engine/Plugins/Marketplace" | grep -i cesium` prints a Cesium folder, and `grep -h '"VersionName"' "$UE"/Engine/Plugins/Marketplace/*/CesiumForUnreal.uplugin` shows `2.30.0` or newer.
3. `"$UE/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" -version 2>/dev/null | head -1 || open "$UE/Engine/Binaries/Mac/UnrealEditor.app"`. Expected: the Project Browser opens without a crash.

### U2 (S1): Build, package and run the OneMind app

1. `cd "$OM" && git pull`. Expected: up to date with `main`.
2. Generate and build (Development, editor):
   `"$UE/Engine/Build/BatchFiles/Mac/Build.sh" OneMindEditor Mac Development -Project="$UPROJECT" -WaitMutex`
   Expected: ends with `Result: Succeeded` (or `Total execution time`, no `error:` lines).
3. Package (Shipping, Mac):
   `"$UE/Engine/Build/BatchFiles/RunUAT.sh" BuildCookRun -project="$UPROJECT" -platform=Mac -clientconfig=Shipping -build -cook -stage -pak -archive -archivedirectory="$WORK/build/mac" -utf8output`
   Expected: last lines include `BUILD SUCCESSFUL` and `AutomationTool exiting with ExitCode=0 (Success)`. Then `ls "$WORK/build/mac"/Mac*/` lists `OneMind.app`.
4. Run it: `open "$WORK/build/mac"/Mac*/OneMind.app`. Expected: the globe renders with open terrain and imagery. Press the backtick key and type `stat fps`, then `stat unit`. Expected: **≥ 60 fps on the idle globe** (B1). Take a screenshot (Cmd-Shift-4) and save it to `$OM/docs/evidence/M2/S1-globe-mac.png`.
5. Offline cache (B2): quit, turn Wi-Fi off, reopen the app, and fly to the same view. Expected: the globe still renders from cache. Screenshot to `$OM/docs/evidence/M2/S1-offline-cache.png`.
6. If step 2 or 3 fails, save the log (`~/Library/Logs/Unreal Engine/LocalBuildLogs/` and the UAT log path printed on failure) to `$OM/docs/evidence/M2/S1-failure.log`. Do not use the DebugGame configuration (Cesium issue #1850).

### U3 (S5): NATS in the app (local leaf)

1. Start the local stack: run the Mac script lane A provides (planned name `scripts/mac/run-local-bus.sh`). Until it exists: `nats-server -js -a 127.0.0.1 -p 4222 &` (nats-server from https://github.com/nats-io/nats-server/releases). Expected: `nats server check jetstream` prints `OK JetStream`.
2. Build nats.c for the plugin (lane B commits this script; planned name `scripts/unreal/build-natsc-mac.sh`). It runs:
   `cmake -S nats.c-3.14.0 -B build-natsc -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 -DNATS_BUILD_STREAMING=OFF -DNATS_BUILD_EXAMPLES=OFF -DBUILD_TESTING=OFF -DNATS_BUILD_LIB_SHARED=OFF -DNATS_BUILD_WITH_TLS=OFF && cmake --build build-natsc -j`
   Expected: `build-natsc/src/libnats_static.a` exists and `lipo -info` on it says `arm64`.
3. Launch the packaged app (U2.4) and publish a test entity:
   `nats kv add world-state --history=1 --ttl=5m 2>/dev/null; nats kv put world-state ent.drone.sim-x1.state '{"lat":40.7128,"lon":-74.0060,"alt":120}'`
   Expected: a `sim-x1` marker appears over New York within one second.
4. Reconnect (B3): `pkill nats-server; sleep 3; nats-server -js -a 127.0.0.1 -p 4222 &`, then repeat the `kv put` with a changed `lat`. Expected: the marker moves; the app log (`~/Library/Logs/OneMind/OneMind.log`, path **unverified**) shows a reconnect.

### U4 (S2): Pixel Streaming 2 from the Mac

1. `cd "$WORK" && git clone --branch UE5.8 --depth 1 https://github.com/EpicGamesExt/PixelStreamingInfrastructure.git`. Expected: `cat PixelStreamingInfrastructure/NODE_VERSION` prints `v22.14.0`.
2. `LANIP=$(ipconfig getifaddr en0); echo $LANIP`. Expected: your Mac's LAN address, for example `192.168.1.20`.
3. `cd "$WORK/PixelStreamingInfrastructure/SignallingWebServer/platform_scripts/bash" && chmod +x *.sh && ./start.sh --publicip "$LANIP" -- --player_port=8080`
   Expected: on first run it downloads Node v22.14.0 (darwin-arm64) into the repo and builds the frontend, then logs that it is listening for streamers on 8888 and players on 8080. Leave it running. If macOS asks to allow incoming connections for `node`, click **Allow**.
4. In a second Terminal (the app must be packaged with the **Pixel Streaming 2** plugin enabled, which lane C does), run windowed first:
   `"$WORK/build/mac"/Mac*/OneMind.app/Contents/MacOS/OneMind -PixelStreamingURL=ws://127.0.0.1:8888 -ResX=1920 -ResY=1080 -Windowed`
   Expected: the app window opens and the signalling log shows a streamer connected.
5. On another device on the same Wi-Fi, open `http://<LANIP>:8080` and click to start. Expected: the **live globe** (not a solid colour) and mouse/touch control. Open the stats panel (the frontend's settings/stats overlay) and note latency. **Pass: picture correct and input latency < 150 ms** (F2).
6. If the picture is a flat colour, quit the app and retry with `-RenderOffscreen` added, then with `-ResX=1280 -ResY=720`. Record which variant works in `$OM/docs/evidence/M3/S2-result.md`, with screenshots from the viewing device.
7. If no variant shows the scene, S2 has failed. Record it; the run has already set the L0 web client as the fallback (decision 0004 #1). Stop the server with Ctrl-C.

### U5 (S3): Quest Pro standalone build (Mac + headset)

1. **Android toolchain on the Mac.** Install Android Studio from https://developer.android.com/studio. In **SDK Manager**, install **Android SDK Platform 35** and 34, **Build-Tools 35.0.1**, **NDK 27.2.12479018 (r27c)** and **Platform-Tools**. Then:
   ```bash
   export ANDROID_HOME="$HOME/Library/Android/sdk"
   export NDKROOT="$ANDROID_HOME/ndk/27.2.12479018"
   export JAVA_HOME="/Applications/Android Studio.app/Contents/jbr/Contents/Home"
   export PATH="$ANDROID_HOME/platform-tools:$PATH"
   adb version && "$JAVA_HOME/bin/java" -version
   ```
   Expected: adb prints a version, and Java prints `21.x`. (r27c's package version 27.2.12479018 and Android Studio's bundled JBR 21 are **unverified**; if they differ, use the r27c folder name SDK Manager shows.) Alternative: `"$UE/Engine/Build/BatchFiles/RunUAT.sh" Turnkey -command=VerifySdk -platform=Android -UpdateIfNeeded` (UE 5.8 Turnkey Android setup; command form **unverified**). Expected: Android reported as valid.
2. **Headset developer mode.** On your phone, open the Meta Horizon app, tap the headset icon, tap your Quest Pro, tap **Headset Settings > Developer Mode** and switch it on (this needs a Meta developer organization). Connect the Quest Pro to the Mac with a USB-C data cable. In the headset, accept **Allow USB debugging > Always allow from this computer**.
3. `adb devices`. Expected: one line ending in `device` (not `unauthorized`). Then `adb shell getprop ro.product.model`, which prints the model name (expected to identify Quest Pro; exact string **unverified**).
4. **Project settings** (lane E commits them in `Config/DefaultEngine.ini`; check them in the editor under Edit > Project Settings): OpenXR plugin **on**; Meta XR / OculusXR plugin **off** (UE 5.8 load failure, Meta investigation 1053459897316880); Start in VR on; Android: Support arm64 on, Vulkan on, OpenGL ES 3.2 off, Minimum SDK 32 or higher, Target SDK 34 or higher; Forward Shading on; MSAA 4x; Mobile HDR off; Mobile Multi-View on; Cesium experimental occlusion culling on; terrain Maximum Screen Space Error 256 (Cesium's Quest guide).
5. **Package (full package, never Quick Launch: Cesium issue #1712):**
   `"$UE/Engine/Build/BatchFiles/RunUAT.sh" BuildCookRun -project="$UPROJECT" -platform=Android -cookflavor=ASTC -clientconfig=Development -build -cook -stage -pak -package -archive -archivedirectory="$WORK/build/quest" -utf8output`
   Expected: `BUILD SUCCESSFUL` and `ExitCode=0`; `ls "$WORK/build/quest"/Android*/*.apk` lists one APK.
6. **Device list in the manifest:** `"$ANDROID_HOME/build-tools/35.0.1/aapt" dump xmltree "$WORK/build/quest"/Android*/*.apk AndroidManifest.xml | grep -A1 supportedDevices`. Expected: a value containing `questpro`. If it is missing, record it: the app may run in compatibility mode (Meta OS compatibility docs). Lane E then adds the entry through the Android manifest extra-settings config.
7. **Install and run:** `adb install -r "$WORK/build/quest"/Android*/*.apk`. Expected: `Success`. In the headset, open **Library > Unknown Sources > OneMind**. Expected: the globe in VR with one live layer (`sim-x1` from U3 if the headset can reach the bus; see step 9).
8. **Frame rate:** `adb logcat -s VrApi | grep -m 20 FPS`. Expected: lines with `FPS=72` (or 90). **Pass: holds 72 fps with the globe and one layer** (F4, J3). The VrApi log tag is **unverified** for OpenXR apps; if nothing prints, use the in-app `stat fps` overlay and record the number. Save the log: `adb logcat -d > "$OM/docs/evidence/M5/S3-logcat.txt"`.
9. **Bus reachability from the headset:** the Quest has no local leaf. Point the app's bus setting at the Mac's WebSocket listener, `ws://<LANIP>:9222`, after starting nats-server with a WebSocket block bound to the LAN (the lane A script provides the flag; no TLS on the LAN means the test network only). Expected: the `sim-x1` marker updates in the headset.
10. **If S3 fails** (crash, black screen, or < 72 fps after lowering the budget): record the cause in `$OM/docs/evidence/M5/S3-result.md`. The fallback (decision 0004 #2) is the WebXR L0 client: in the headset browser open `https://<host>/l0/` (the HTTPS host from lane C; WebXR needs a secure context) and tap **Enter VR**. Expected: the globe and one layer in VR.

### U6 (B4): 10,000 entities at 60 fps on the Mac

1. Start the synthetic load publisher (lane B commits it; planned `scripts/sim/entity_load.py --count 10000 --rate 1`). Expected: it reports 10,000 `sim-*` entities publishing.
2. Open the packaged app (U2.4), turn on the synthetic layer, and run `stat fps` and `stat unit` in the console. Expected: **≥ 60 fps** with all 10,000 in view at global zoom, and no hitch when toggling the layer (B4, B5).
3. Capture 30 s with Unreal Insights: launch the app with `-trace=cpu,gpu,frame -tracefile="$OM/docs/evidence/M2/B4-10k.utrace"`. Expected: the file exists, and the frame track shows ≤ 16.7 ms frames.

## Unverified items

- Whether Epic's OpenXR plugin alone (without the Meta XR plugin) writes `questpro` into `com.oculus.supportedDevices` in UE 5.8, and the name of the UE 5.8 "Package for Meta Quest devices" setting (UE source not accessible: `gh api repos/EpicGames/UnrealEngine` → 404).
- That a UE 5.8 OpenXR-only APK runs on a Quest Pro at 72 fps with Cesium tiles. No measurement exists.
- That nats.c v3.14.0 builds with the Android NDK r27c and runs on Horizon OS. Upstream claims only macOS, Linux and Windows.
- That nats.c builds and passes KV/subscribe on macOS arm64. A local build was denied (C-DENIED), but upstream lists macOS support.
- UE's bundled OpenSSL version in 5.8, and whether a separately linked OpenSSL clashes.
- UE `WebSockets` module API (`IWebSocketsModule::CreateWebSocket`) availability on Mac and Android in 5.8. The API page did not render through WebFetch.
- UE ISM batch-update API names (`BatchUpdateInstancesTransforms`, `UpdateInstances`) and their per-frame cost on Apple Silicon.
- That NDC data is transient per frame (forum reports only).
- Pixel Streaming 2 working at all on macOS 14/15 with UE 5.8; the official test OS is Ventura 13.5.1.
- `-RenderOffscreen` behaviour on macOS.
- Exact paths: `/Users/Shared/Epic Games/UE_5.8` (Launcher default), the app log location, the Fab plugin folder name, the NDK r27c package version string, the Android Studio JBR path, and the Turnkey command form.
- Quest Pro end-of-support status; no Meta date found.
- Quest Pro refresh rates (72/90 Hz per Road to VR / UploadVR launch coverage, not a Meta spec page).
- The "Ventura" and "Sequoia 14" inconsistencies on Epic pages are recorded as found.
- Thesis result (ISM to ~10k, Niagara to 50k+) known only from the abstract shown in search results.

## Independent verification (2026-10-29)

A second agent re-checked 30 load-bearing claims against their primary sources (`gh` against CesiumGS/cesium-unreal, nats-io/nats.c and EpicGamesExt/PixelStreamingInfrastructure at `UE5.8`; WebFetch of every Epic, Meta, Cesium and forum page cited; local `omos/platform/nats/streams/kv-*.yaml`; and `nats` CLI 0.5.1 against the local server: `nats server check jetstream` printed `OK JetStream`). Confirmed with no change: G-CES-REL (including the asset size), G-CES-CHG (v2.31.0 entry dated 2026-11-02, unreleased), G-CES-UPL, G-CES-GEO, G-CES-ISS #1484 and #1850, G-NATSC (CMake defaults, stale README streaming text, `if(NOT ANDROID)`, OpenSSL 1.1.1), G-NATSC-H (0 "websocket"), G-NATSC-371, G-PSI (branch, Node, ports, `--publicip`/ipify, `--` passthrough to the server in `common.sh`), F-PS-MAC (Discourse JSON: 1 post, 0 replies), F-META-58, F-META-MAC, F-MASS-BENCH, S-MACREQ Xcode figures, S-PSREF, S-PS2, S-ISM, S-MCOMPAT, S-MPREQ, S-CQ, S-MASSGP, and the S-UE58 OpenXR, Turnkey, Gradle, NDK and Mass statements, C-OMOS.

Corrected in place: the free-disk figure (PREFLIGHT says 25 GiB), the recommended macOS (the page is inconsistent), the sources for Gradle 8.7 and android-26 (release notes only), the Cesium platform list (a superset that includes IOS), the GPL-compatibility claim (now cited) and the developer-mode prerequisites. Marked **unverified**: the stated reason for the Xcode downgrade.

Licence and cluster safety: no worldmonitor or worldwideview code is copied; no cluster command was run; the denied nats.c build was recorded as a blocker rather than worked around. Note for the owner: `start.sh` also defaults its STUN server to `stun.l.google.com:19302` (`common.sh`), which is an outbound call even when `--publicip` is passed.
