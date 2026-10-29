# VR-TAK porting map (lane: vrtak)

Date: 2026-10-29. Phase 1 / M0 research lane. Read-only; nothing was cloned, built or
committed. Inputs read first: `ULTRACODE-PROMPT.md` (Mission, Rules, Facts already
verified), `docs/decisions/0001-…`, `docs/decisions/0004-owner-defaults-for-the-run.md`,
`docs/PREFLIGHT.md`, `docs/ARCHITECTURE.md`, `docs/FEATURES.md`.

**Main finding.** Of the seven plugins in scope, only three ship C++ implementation
source: `VH_COT`, `VTAK_Mensuration` and `VTAK_Orbit`. `VH_Replication`, `VH_VR`,
`VH_SessionPlayback` and `VH_Plugins` ship public headers plus prebuilt Win64 DLLs
built against the patched 5.3.2 engine (`bUsePrecompiled = true`). So do the support
plugins all three source plugins need (`VH_Globe`, `VH_GlobeMath`, `VH_Developer`,
`VH_UI`, `VH_FileIO*`, `VH_Database_*`, `VH_Keyboard`). Those four can't be ported as
code. They have to be rewritten against UE 5.8 (or replaced by NATS), using their headers
as an interface reference. This corrects the "Facts already verified" line, which lists
`VH_Replication`, `VH_VR` and `VH_SessionPlayback` as plugins to port, and the
2026-10-29 findings, which describe `VH_Globe` as public geodesy source (it is headers
only).

## Sources

All VR-TAK paths are relative to `https://github.com/TAK-Product-Center/VR/blob/<sha>/`.

| # | Source | What it gave |
|---|---|---|
| S1 | `gh api repos/TAK-Product-Center/VR` | licence `GPL-3.0` (detected on `main`), size 8,620,868 KB, last push 2026-04-02 |
| S2 | `gh api repos/TAK-Product-Center/VR/branches`, `/releases`, `/tags` | branches `main` (4bd38497 = 5.4-RELEASE-7), `upstream/5.4-RELEASE-7` (d2560422), `upstream/5.5-RELEASE-0` (41ad0bba, newest); release `5.5-RELEASE-0` 2025-09-07 targets `upstream/5.5-RELEASE-0`; no release assets |
| S3 | `gh api …/git/trees/41ad0bba?recursive=1` (32,989 entries, not truncated) and `…/4bd38497…` (28,952) | file inventory per plugin; `EnginePatch/` = 12,481 entries (5.5) / 12,443 (5.4) |
| S4 | `README.md` @41ad0bba | build: "Download Unreal Engine 5.3.2", "Copy the contents of [sdkRoot]/EnginePatch into [sdkRoot]/Engine and replace all files", Visual Studio 2022 |
| S5 | `Game/Game.uproject` @4bd38497 | enabled plugins incl. `VH_VR`, `VH_SessionPlayback`, `VH_Replication`, `VH_Plugins`, `OpenXR` (Win64/Linux/Android), `WebBrowserWidget`, `LidarPointCloud` |
| S6 | Each plugin's `.uplugin` and `*.Build.cs` @41ad0bba (listed per plugin below) | dependencies, `bUsePrecompiled`, Win64-only third-party blocks |
| S7 | Plugin `.cpp`/`.h` @41ad0bba (VH_COT 11,446 lines; Mensuration 2,710; Orbit 2,148; headers of the four binary plugins 2,690) | function, engine coupling |
| S8 | `Game/Config/DefaultEngine.ini` @41ad0bba lines 69–75 | `+ChannelDefinitions=(ChannelName=VH, ClassName=/Script/VH_Replication.Replication_DataChannel …)` on `NetDriver` and `DemoNetDriver` |
| S9 | GitHub code search `WITH_VH repo:TAK-Product-Center/VR` (indexes default branch `main` only): `total_count=74`, all under `EnginePatch/` | VR-TAK's own engine modifications are marked `// WITH_VH`; all 74 files fetched and read |
| S10 | `Game/Source/Game/…` @41ad0bba (`Game_MotionControllerComponent.cpp`, `SessionPlayback/*`) | where the engine hooks are consumed |
| S11 | `LICENSE` @main (GPL-3.0 text); `LICENSE` @41ad0bba → HTTP 404 (not on the 5.5 branch; top-level listing in S3 confirms) | licence state |
| S12 | UE 5.8 API: [FXmlNode](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/XmlParser/FXmlNode), [FXmlFile](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/XmlParser/FXmlFile), [UMotionControllerComponent](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/HeadMountedDisplay/UMotionControllerComponent), [UReplaySubsystem](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/UReplaySubsystem), [IPluginManager](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Projects/IPluginManager), [Introduction to Iris](https://dev.epicgames.com/documentation/en-us/unreal-engine/introduction-to-iris-in-unreal-engine) (all pages titled "Unreal Engine 5.8 Documentation") | stock 5.8 replacements |
| S13 | [UCesiumGlobeAnchorComponent](https://cesium.com/learn/cesium-unreal/ref-doc/classUCesiumGlobeAnchorComponent.html) | geodetic placement replacing VH_Globe |
| S14 | [Meta XR Interaction SDK for Unreal](https://developers.meta.com/vr/downloads/package/meta-xr-interaction-sdk-unreal/), [Meta UE5 integration](https://developers.meta.com/vr/downloads/package/unreal-engine-5-integration/) (via web search) | Quest on 5.8 |
| S15 | UE forum threads [verify host](https://forums.unrealengine.com/t/http-request-ssl-fails-due-to-verify-host/1838932), [TLS client cert](https://forums.unrealengine.com/t/how-do-i-set-the-tls-field-in-unreal-engine/511462) | stock HTTP lacks a client-cert API (community evidence only) |

## Verified facts

### Repository-wide

- The newest code is branch `upstream/5.5-RELEASE-0` @41ad0bba. The default branch `main` is
  5.4-RELEASE-7 (S2). Both READMEs still say UE **5.3.2** plus EnginePatch (S4).
  `ProjectVersion` in the uproject is `5.4.0`, which is the VR-TAK version, not the engine
  version (S5).
- Plugin layout is the same on both branches: the binary-only plugins have 0 hand-written `.cpp` under `Source/` on
  both (S3; verifier: the tree does contain UHT-generated `Intermediate/Build/Win64/**/*.gen.cpp` files and `VH_Plugins/Templates/**/PLUGIN_NAME.cpp` stubs, which are not implementation source), and no commit ever touched `Game/Plugins/{VH_VR,VH_Replication,
  VH_SessionPlayback,VH_Plugins,VH_Globe}/Source/*/Private` (`gh api commits?path=…`
  returned 0 for each).
- Per-plugin inventory @41ad0bba (S3, own source only, ThirdParty excluded):

  | Plugin | .cpp | .h | DLLs | Build.cs flag | Author (`CreatedBy`) |
  |---|---|---|---|---|---|
  | VH_COT | 17 | 18 | 29 (Commo + Gv2F, Win64) | source | Virtual Heroes, a division of ARA |
  | VTAK_Mensuration | 15 | 16 | 0 | source (33 .uasset under `Content/`; verifier-corrected from 31, tree @41ad0bba) | Virtual Heroes, a division of ARA |
  | VTAK_Orbit | 10 | 11 | 0 | source (10 .uasset) | Virtual Heroes, a division of ARA |
  | VH_Replication | 0 | 10 | 1 | `bUsePrecompiled = true` | Applied Research Associates / Virtual Heroes Division |
  | VH_VR | 0 | 10 | 1 | `bUsePrecompiled = true` | same |
  | VH_SessionPlayback | 0 | 5 | 1 | `bUsePrecompiled = true` | same |
  | VH_Plugins | 0 | 9 | 2 | `bUsePrecompiled = true` | (none) |
  | VH_Globe / VH_GlobeMath, VH_Developer, VH_UI, VH_FileIO*, VH_Database_*, VH_Keyboard | 0 | — | yes | `bUsePrecompiled = true` | — |

- The engine-modification marker is `WITH_VH`. Code search finds 74 marked files, all in
  `EnginePatch/` (S9). The other ~12,400 EnginePatch files can't be told apart from stock
  without Epic's 5.3.2 source (see Unverified).

### Engine patch hunks relevant to the seven plugins (S9, files read)

| Engine area (EnginePatch path) | `WITH_VH` change | Used by |
|---|---|---|
| `Runtime/XmlParser` `XmlFile.h/.cpp`, `XmlNode.h/.cpp` | adds `FXmlFile::SaveToString`, makes `WriteNodeHierarchy` public, adds `FXmlNode::SetAttribute`, `RemoveChildNode(s)`, `SetNextNode`, `FXmlAttribute::SetValue`, public `~FXmlNode`, strips a `<?xml …?>` prolog that shares a line with content | **VH_COT**: `VH_COTEvent.cpp` (`SaveToString`, `WriteNodeHierarchy`, `RemoveChildNodes`), `->SetAttribute(` in COTManager (14), COTComponent (5), DataPackage (2); Mensuration/Orbit use it indirectly via `UVH_COTEvent::SetAttribute` |
| `Runtime/Online/HTTP` `CurlHttp.cpp/.h`, `IHttpRequest.h` | `IHttpRequest::CustomizeForVH(keyValues)` sets `CURLOPT_SSLCERT/SSLKEY/CAINFO` (client-cert TLS) **and `CURLOPT_SSL_VERIFYHOST=false`** | **VH_COT** `VH_COTManager.cpp:472` (TAK Server HTTPS: data packages, uploads) |
| `Plugins/Runtime/OpenCV` (661 files in EnginePatch, no `WITH_VH` hits) | patched OpenCV plugin copy | **VH_COT** `VH_COTManager.cpp:18-24` includes OpenCV headers; no `cv::`/`Mat` usage found in VH_COT sources |
| `Runtime/HeadMountedDisplay` `MotionControllerComponent.h/.cpp` | adds `virtual bool GetNewPositionAndRotation(...)` called before `SetRelativeLocationAndRotation` | **VH_VR** stack: `UVR_MotionControllerComponent : UMotionControllerComponent`; overridden in `Game_MotionControllerComponent.cpp:102` to re-express tracking in a globe-tangent "camera transform override" |
| `Runtime/Engine` `StereoRendering.cpp` | adds a `-nohmd` command-line override: stock `-vr \|\| bStartInVR` becomes `!nohmd && (-vr \|\| bStartInVR)` (`StereoRendering.cpp:37-38` @41ad0bba; verifier-corrected: `-vr` itself is stock) | VH_VR (startup) |
| `Runtime/Slate` `SlateUser.h/.cpp`, `SlateApplication.cpp`, `SEditableText.cpp`; `Runtime/Engine` `SceneViewport.cpp` | focus lock and capture tweaks | VR widget interaction and keyboard (VH_VR / VH_Keyboard / VH_UI); exact consumer is in binaries, **unverified** |
| `Runtime/Engine` `GameFramework/Actor.h` (`SetActorScale3D` made virtual), `Components/SceneComponent.h` (private→protected), `GameFramework/PlayerController.h` (`GetHitResultAtScreenPosition` overloads made virtual) | "VTAK-only" | Game module base classes. Mensuration/Orbit call `SetActorScale3D` (`VTAK_COTComponent_DrawMensuration.cpp:156`, `VTAK_DatabaseComponent_Orbit.cpp:350`) and derive from `AGame_Arrow`/`AGame_EditPoint` |
| `Runtime/Engine` `VoiceChannel.h/.cpp`, `VoiceConfig.cpp`; `OnlineSubsystemUtils/VoiceInterfaceImpl.cpp` ("allow playback outside of session; for replay capability") | `UVoiceChannel::OnPacketReceived/OnPacketSent` delegates | **Session playback** VOIP handler `Game_SessionPlaybackHandler_VOIP.cpp:72-119` (verifier-corrected from 70-102) |
| `Runtime/Projects` `PluginManager.*`, `PluginDescriptor.*`, `ProjectDescriptor.*`, `IPluginManager.h`; `Runtime/Core` `ModuleManager.*`, `ModuleManifest.cpp`; `Launch/LaunchEngineLoop.cpp`; `TargetPlatformManagerModule.cpp` | external plugin directories, external binaries directories, `FInvalidPlugin`, `"SlatedForDeletion"` descriptor field, `SKIPCOMPATIBLE` | **VH_Plugins**: `VH_PluginSubsystem.h:82,303` uses `bSlatedForDeletion` and `FInvalidPlugin`; Mensuration/Orbit `.uplugin` carry `"SlatedForDeletion": false` |
| `Runtime/WebBrowser`, `WebBrowserWidget` | `GoBack/GoForward/IsBrowserValid`, keyboard focus, **`ignore_certificate_errors = true`** | not used by the seven plugins (Game/UI) |
| Others (LidarPointCloud, ProceduralMesh, Metal, D3D11, Windows title "VR-TAK", ChaosCloth, Datasmith) | various | not used by the seven plugins |

### Per-plugin facts

**VH_COT** (`Game/Plugins/VH_COT`, S6/S7)
- Function: the TAK CoT client. `UVH_COTManager` (3,183 lines) owns a Commo instance
  (`atakmap::commoncommo::Commo`). A helper class `CommoListener` in the same file (not `UVH_COTManager` itself; verifier-corrected) implements `CoTMessageListener`,
  `ContactPresenceListener`, `MissionPackageIO`, `InterfaceStatusListener` and
  `CoTSendFailureListener` (`VH_COTManager.cpp:52-56`). It covers streaming connections over
  TCP, SSL and QUIC (`VH_COTConnectionInfo.cpp:871-893`), broadcast/inbound interfaces,
  certificate enrollment (`generateKeyCryptoString`, `generateCSRCryptoString`,
  `generateKeystoreCryptoString`), `SendCOT`, `BroadcastCOT`, GeoChat, heartbeat,
  contacts, data packages and DataSync (missions) from TAK Server over HTTPS, the TAK video
  list, and per-entity attachments. `UVH_COTEvent` (635 lines, .cpp+.h @41ad0bba: 386+249) is the CoT XML model, built on
  `FXmlFile`. `VH_MilStd2525` (525 lines, 418+107) maps symbols. `VH_TAKVideoPlayer` (1,284 lines, 1,047+237)
  plays video through **Gv2F**.
- Build deps (`VH_COT.Build.cs`): Core, CoreUObject, Engine, HTTP, ImageWrapper, Json,
  JsonUtilities, Networking, **OpenCV**, RenderCore, RHI, Sockets, UMG, XmlParser, plus
  precompiled `VH_Database_Spatialite`, `VH_Developer`, `VH_FileIO`, `VH_FileIO_Kml`,
  `VH_FileIO_Network`, `VH_Globe`, `VH_GlobeMath`, and externals `commo`, `gv2F`.
- `commo.Build.cs` and `Gv2F.Build.cs` set libraries only `if (Target.Platform ==
  UnrealTargetPlatform.Win64)`. Gv2F also defines `WITH_COT_GV2F=0` elsewhere.
- Gv2F headers: "Copyright 2011 PAR Government Systems … Restricted Rights: Use,
  reproduction, or disclosure … subject to restrictions set forth in the contract and/or
  license agreement" (`ThirdParty/Gv2F/include/mediaconsumers.h:1-14`). It needs a
  `Data/Gv2F/gv2f.lic` licence file (`VH_TAKVideoPlayer.cpp:38`).
- Geodesy used: `FGeodeticCoord2D/3D`, `UVH_GlobeMathStatics::CalculateSurfaceDestination/
  Distance/Bearing` (precompiled VH_GlobeMath).

**VTAK_Mensuration** (S6/S7)
- Function (`.uplugin`: "a measurement tool used for accurately measuring things in a
  particular plane"): three actors, Range, Angle and Height
  (`VTAK_Actor_Mensuration_{Range,Angle,Height}`). Each is a two-point line with editable
  end points, a detail widget with unit selection and locks, and CoT persistence as
  `u-d-m-r`, `u-d-m-a`, `u-d-m-h` with `strokeColor` and `link` points
  (`VTAK_COTComponent_DrawMensuration.cpp:94-127`).
- Math (`VTAK_Actor_Mensuration.cpp:112-185`): length = `FVector::Dist` in Unreal units
  / 100. That is the straight-line chord in world space, not a geodesic. Bearing =
  `UKismetMathLibrary::GetAzimuthAndElevation` relative to the base point transform.
  Angle = `AVH_GlobeReferenceActor::WorldRotationToPitch`. Units go through
  `UDeveloper_Subsystem_UnitConverter`.
- Deps: private dep on the **`Game` module** (`Game_Arrow`, `Game_EditPoint`,
  `Game_TransformGizmo`, `Game_EntityDB`, `Game_VirtualObjective`, `Game_PlayerController`,
  and more), plus precompiled VH_UI, VH_VR, VH_Plugins, VH_Globe, VH_Developer and
  Spatialite persistent settings.

**VTAK_Orbit** (S6/S7)
- Function: an orbit-camera marker. `AVTAK_Actor_Orbit` (an `AGame_EditPoint`) has a
  `UCameraComponent`, `RadiusMeters=300` and `SpeedMetersPerSecond=20`
  (`VTAK_Actor_Orbit.cpp:65-66`). On Tick it circles the point in the local tangent frame
  (`:69-115`). `SetOrbit` swaps the view target with `SetViewTargetWithBlend` and disables
  pawn input (`:162-200`). It persists as CoT `u-d-o` (`VTAK_COTComponent_Orbit.cpp:56`)
  and as a Spatialite blob. The `.uplugin` description ("allows users to play-back a
  particular path. Not available in VR mode") does not match the code, which is an orbit
  around a point.
- Deps: same pattern as Mensuration (private `Game` module plus precompiled VH_*).

**VH_Replication** (binary only)
- Headers show `UReplication_DataChannel : UChannel` with chunked send/receive buffers,
  owner ID and priority (`Core/Public/Replication_DataChannel.h`). `UReplication_Subsystem_Core
  : UWorldSubsystem` keeps prioritised per-ID channels per `APlayerController`, with
  `SendData(id, bytes, onlySendTo, exclude)`
  (`Subsystems/Public/Replication_Subsystem_Core.h`). It is a prioritised bulk binary
  channel over UE's net connection, registered through stock `ChannelDefinitions`
  config (S8). Deprecated `VH_DataChannel*` classes are also registered.

**VH_VR** (binary only)
- Headers: `UVR_MotionControllerComponent : UMotionControllerComponent` (replicated
  relative transform, a replicated "input tool stack" of actors implementing
  `IInterface_VR_Tool`, Enhanced Input actions, VR emulation offset for desktop mode).
  Also `UVR_MovementComponent : UFloatingPawnMovement` with `WorldTransformOverride`
  and gravity override (globe-tangent locomotion), `UVR_TraceComponent`,
  `UVR_WidgetInteractionComponent : UWidgetInteractionComponent`,
  `UVR_Subsystem_Core` (hardware interface mode switching) and `UVR_Statics::IsUsingHMD`.
  Build deps include HeadMountedDisplay, EnhancedInput, UMG and precompiled
  VH_Developer/VH_UI. `Game/Config/SteamVRBindings/*` exists for legacy SteamVR bindings.

**VH_SessionPlayback** (binary only, but its main implementation is in the Game module)
- Headers: `USessionPlayback_Subsystem_Core` (load, record, rate, seek, status, exec test
  commands), abstract `USessionPlayback_Impl` and `USessionPlayback_UnrealImpl` on
  `UReplaySubsystem`. `Game/Source/Game/SessionPlayback` (1,848 lines, source available)
  has `UGame_SessionPlayback_Impl`, which records entity DB changes and CoT connection state
  to a Spatialite DB (`SessionPlaybackEntityDB.sqlite`) and replays avatars and VOIP. The VOIP
  part needs the patched `UVoiceChannel::OnPacketReceived`.

**VH_Plugins** (binary only)
- `UVH_PluginSubsystem` registers, enables, deletes, imports and versions VR-TAK "VTAK_*"
  plugins at runtime and finds plugin classes and maps via the asset registry.
  `VH_PluginsEditor` (wizard, packager, templates) is for third-party plugin authors.
  Loading phase is `PreLoadingScreen`. It needs the patched Projects/Core plugin manager
  (`FInvalidPlugin`, `SlatedForDeletion`, external binaries directories).

### UE 5.8 stock APIs (S12–S14)

- `FXmlNode` (5.8) has `AppendChildNode`, `FindChildNode`, `GetAttribute(s)`,
  `GetChildrenNodes`, `GetContent`, `SetAttributes(TArray)` and `SetContent`. It has **no**
  `SetAttribute` (singular) and no child removal. `FXmlFile` (5.8) has `LoadFile`,
  `Save(Path)`, `GetRootNode`, static `WriteNodeHierarchy` (access level not shown) and
  **no `SaveToString`**.
- `UMotionControllerComponent` (5.8) is still in HeadMountedDisplay. `TickComponent`,
  `PollControllerState_*` are listed non-virtual. `OnMotionControllerUpdated` is a
  BlueprintImplementableEvent. There is no stock equivalent of `GetNewPositionAndRotation`.
- `UReplaySubsystem` (5.8): `RecordReplay`, `PlayReplay`, `StopReplay`, `AddEvent`,
  `RequestCheckpoint`, `GetReplayCurrentTime/TotalTime`. `GotoTimeInSeconds` is not on
  the page.
- `IPluginManager` (5.8): `AddPluginSearchPath`, `MountExplicitlyLoadedPlugin`,
  `UnmountExplicitlyLoadedPlugin` ("does not work on plugins with compiled modules"; the page also lists an undocumented second overload taking `bAllowUnloadCode`, so whether code plugins can be unloaded is **unverified**),
  `AddToPluginsList`, `MountNewlyCreatedPlugin`.
- Iris (5.8 docs): opt-in; "the existing replication system is still used as the default".
- Cesium `UCesiumGlobeAnchorComponent`: `MoveToLongitudeLatitudeHeight`,
  `GetLongitudeLatitudeHeight`, ECEF position and rotation, `Get/SetEastSouthUpRotation`,
  `SnapToEastSouthUp`, `AdjustOrientationForGlobeWhenMoving`.
- Meta's Interaction SDK lists UE 5.8 support and "defaults to the Epic Native OpenXR
  XRAPI" with the MetaXR plugin (S14, from search snippet).

## How it applies to OneMind

Port map (OneMind plugin names follow `docs/ARCHITECTURE.md` and decision 0004 #16):

| VR-TAK plugin | Target | Port or rewrite | Engine-patch deps to remove | UE 5.8 / OneMind replacement | Cost (engineer-days, estimate) |
|---|---|---|---|---|---|
| **VH_COT** | `OneMindCoT` | **Port a subset** (GPL source): `UVH_COTEvent` (CoT XML model), `VH_MilStd2525`, GeoChat (`VH_COTChat`), CoT type and `link` handling. **Do not port** Commo session management, enrollment, DataSync, data-package HTTPS, Gv2F video or the Spatialite entity DB | patched XmlParser (`SetAttribute`, `RemoveChildNodes`, `SaveToString`); `CustomizeForVH` HTTP; OpenCV include; Win64 Commo/Gv2F; precompiled VH_Globe/GlobeMath/Database/FileIO/Developer | Stock 5.8 `FXmlFile`/`FXmlNode`, with a ~100-line OneMind XML writer and attribute helper in place of the patched methods (or a vendored MIT/BSD XML library). Transport = NATS through `OneMindBus`: read `tak.cot.>` (raw wire, decision 0004 #7) and send to `tak.cot.inject` / `tak.cot.mesh.tx` per the architecture. Geodesy = Cesium (`UCesiumGlobeAnchorComponent`, georeference transforms). Drop the OpenCV include. Video = MediaMTX via UE Media Framework (other lane) | Core (event model, 2525, chat, NATS adapter, tests): **5–6**. Direct Commo (C4) is separate: +**5–10** after spike S4 builds `commoncommo` for macOS arm64 (**unverified** feasibility). DataSync/data packages: +**5** (defer; omos side) |
| **VTAK_Mensuration** | `OneMindMission` (measure tools) | **Rewrite, using the source as reference** (GPL; small logic can be ported with attribution). It derives from `Game` module classes, and its `.uasset` widgets/Blueprints reference those classes | Game module (`AGame_Arrow`, `AGame_EditPoint`, gizmo, `SetActorScale3D` virtual, `SceneComponent` protected); precompiled VH_UI/VR/Plugins/Globe/Developer | Two anchored end points (`UCesiumGlobeAnchorComponent`), a line component, a UMG detail panel. Range should use the **geodesic** on the WGS84 ellipsoid (VR-TAK uses world-space chord), with slant range shown separately. Bearing from the ENU frame at the base point. Height = ellipsoidal/terrain delta. CoT emit `u-d-m-r/a/h` | **4–6** |
| **VTAK_Orbit** | `OneMindMission` (camera tool) or `OneMindGlobe` camera | **Port logic** (Tick math is ~50 lines) onto OneMind base classes | Game module (`AGame_EditPoint`, `AGame_VirtualObjective::GetGlobeReference`, `UGame_Statics::GetWorldTransformOverride`) | Orbit pivot from `UCesiumGlobeAnchorComponent` ESU frame; `APlayerController::SetViewTargetWithBlend` (stock); CoT `u-d-o` | **2–3** |
| **VH_Replication** | `OneMindXR`/F6 multi-user | **Rewrite, no code to port** (binary only) | the plugin itself is a 5.3.2-patched binary; uses a stock-config custom `UChannel` | For F6 (shared view, pointers, follow-me): NATS. Use existing grammar subjects for presence/pointers (subject choice is the bus lane's, under decision 0004 #8), KV for session state, JetStream object store for bulk blobs. This avoids a UE listen server and works across L0/L1/L2 clients. A custom `UChannel` with `ChannelDefinitions` is still possible on 5.8's default (non-Iris) replication if UE-to-UE sessions are ever needed | F6 over NATS: **3–5**. UE `UChannel` equivalent: **3–4** (not recommended) |
| **VH_VR** | `OneMindXR` | **Rewrite, no code to port** (binary only); keep the header design (tool stack, desktop emulation, globe-tangent movement) | patched `UMotionControllerComponent::GetNewPositionAndRotation`; `-vr` StereoRendering patch; Slate focus-lock patches; D3D11/Windows voice HMD patches | Stock OpenXR plugin + Enhanced Input + `UWidgetInteractionComponent`. Put the motion controllers and camera under a "VR origin" scene component that the pawn rotates to the local ESU frame from Cesium. That makes the re-oriented tracking the hook gave without an engine patch. Locomotion: Cesium globe-aware pawn and `UFloatingPawnMovement` with the ESU up vector. Quest Pro: Android/OpenXR build (decision 0004 #2), optionally MetaXR plugin (S14) | **6–10** plus headset checks on the owner checklist |
| **VH_SessionPlayback** | B8 timeline / A10 replay | **Rewrite** (core binary; Game-module impl is source but bound to Spatialite and Game classes) | patched `UVoiceChannel` delegates (VOIP replay); precompiled core | JetStream time-based replay of the per-layer streams and `ent.*.state` (consumer start-time), driving the same layer renderers: the data-level replay A10/B8 already call for. Stock `UReplaySubsystem` (5.8) only if a local "session video" capture is wanted. Drop VOIP replay | **3–5** (client side) |
| **VH_Plugins** | none | **Do not port** | patched Projects/Core (`FInvalidPlugin`, `SlatedForDeletion`, external binaries directories) | Ship `OneMind*` as normal built-in project plugins. If runtime-loadable content add-ons are needed, stock `IPluginManager::MountExplicitlyLoadedPlugin` (content-only plugins; the 5.8 docs say the one-argument unmount does not work on code plugins; a `bAllowUnloadCode` overload exists but is undocumented) | **0** (2–3 if runtime content add-ons are ever required) |

Recommended-scope total: about **23–35 engineer-days** (COT core 5–6, Mensuration 4–6,
Orbit 2–3, VR 6–10, playback 3–5, F6 over NATS 3–5). Estimates assume an experienced UE
C++ engineer and an existing OneMind 5.8 + Cesium base project (spike S1). They are
judgement, not measured.

Related plugins outside this lane's list that the architecture names as donors:
`VH_Globe` (for `OneMindGlobe`) is **headers plus binaries only**. Its geodesy (UTM,
MGRS, EGM96) must come from Cesium plus a vetted library, not VR-TAK.

### Licence and attribution (GPL-3.0)

- What may be copied: only VR-TAK's own `.cpp`/`.h`/`.cs`/`.uplugin` (VH_COT minus
  ThirdParty, Mensuration, Orbit, Game-module sources, and the headers of the binary
  plugins), under GPL-3.0. onemind-xr is GPL-3.0 (decision 0001), so it is compatible.
- Required when porting: keep a `NOTICE`/`THIRD_PARTY.md` entry naming the origin
  (TAK Product Center, `github.com/TAK-Product-Center/VR`, branch and commit, e.g.
  `upstream/5.5-RELEASE-0` @41ad0bba), the authors as stated in the `.uplugin` files
  ("Virtual Heroes, a division of ARA" / "Applied Research Associates"), and the licence.
  Each ported file needs a header stating it is derived from VR-TAK, modified by OneMind,
  with the date (GPL-3.0 §5(a) "prominent notices stating that you modified it, and
  giving a relevant date"). VR-TAK source files carry no per-file copyright headers
  (grep of plugin sources found none), so OneMind's header is the only marker.
- Never copy:
  - `EnginePatch/**`. These are Epic Unreal Engine source files, under the Unreal
    Engine EULA, not GPL. Re-implement the few needed behaviours in OneMind code.
  - `ThirdParty/Gv2F/**` (PAR Government restricted rights, S7).
  - Commo, TAKKernel and other prebuilt DLLs (Windows binaries; their own licences).
  - VR-TAK `.uasset` content. Unreal-format binaries that also reference `Game` classes;
    copyright status of art assets is **unverified**.
- Branch licence gap: `LICENSE` exists on `main` (5.4-RELEASE-7) but **not** on
  `upstream/5.5-RELEASE-0` (S11). Port from either branch, but cite the GPL-3.0 grant on
  `main`. Where code differs between branches, record which branch each file came from.

## Risks

1. **Scope assumption is wrong for 4 of 7 plugins.** VH_Replication, VH_VR,
   VH_SessionPlayback and VH_Plugins have no source. The roadmap must budget rewrites,
   not ports (decision 0001's "port selected plugins" wording should be amended).
2. **Hidden Game-module coupling.** Mensuration and Orbit are thin layers over
   `Game` (≈2,275 files incl. ThirdParty). Porting them "as is" would drag in the Game
   module and with it the engine-patched base classes.
3. **Engine patch is larger than the marked hunks.** 74 files are `WITH_VH`-marked, but
   12,481 files are replaced. Unmarked changes (for example the bundled OpenCV, Net/Replay
   files) can't be ruled out without Epic's 5.3.2 source (**unverified**).
4. **Security defects in donor code:** `CURLOPT_SSL_VERIFYHOST=false` in `CustomizeForVH`
   and CEF `ignore_certificate_errors = true`. Do not carry either into OneMind.
5. **Mac:** Commo and Gv2F are Win64-only (`*.Build.cs`). UE's HTTP on Apple platforms is
   believed not to use libcurl (note: VR-TAK's own `Game/Config/DefaultEngine.ini` sets `[Networking] UseLibCurl=true`, verifier-checked @41ad0bba), so any client-cert approach based on curl options would not
   apply on macOS (**unverified for 5.8**). Stock UE HTTP has no documented client-cert
   API (S15, community evidence). This supports keeping TAK Server access in omos (NATS).
6. **Measurement accuracy:** VR-TAK measures the chord in Unreal space. Ported naively onto
   a Cesium globe, long ranges will under-read against geodesic distance.
7. **Quest Pro:** VR-TAK's OpenXR targets list Android, but there are no Quest-specific
   build files in the plugins. Quest Pro on 5.8 is tested only via the owner checklist.
   Meta notes possible `r.Mobile.ShadingPath=0` needs on 5.8 APKs (S14).
8. **GPL plus Unreal EULA:** distributing GPL code linked into an Unreal build is a known
   licensing tension. VR-TAK does the same; legal review is **unverified / owner item**.

## Recommendation

1. Treat VR-TAK as a **design and partial-source donor**. Port source only from VH_COT
   (CoT event model, MIL-STD-2525 mapping, GeoChat), VTAK_Orbit (orbit math) and small
   parts of VTAK_Mensuration. Use the binary-only plugins' headers as interface
   specifications for clean OneMind rewrites.
2. Build `OneMindCoT` on NATS first (`tak.cot.>` in, `tak.cot.inject`/`tak.cot.mesh.tx`
   out, per architecture and decision 0004 #7), using stock 5.8 `FXmlFile` plus a small
   OneMind XML writer. Leave direct Commo (C4) behind spike S4.
3. Do not port VH_Plugins, Gv2F video, Spatialite DBs, DataSync or anything in
   `EnginePatch/`. Re-implement the motion-controller hook with a rotated VR origin, and
   replace VH_Replication and SessionPlayback with NATS KV/streams/object store and
   JetStream time replay.
4. Before porting, add `THIRD_PARTY.md` with the VR-TAK attribution block and a per-file
   header template. Amend the "Facts already verified" bullet and decision 0001 to say
   "port VH_COT/Mensuration/Orbit; rewrite VR/Replication/SessionPlayback; drop
   VH_Plugins".
5. Order inside M-phases: OneMindCoT core (5–6 d) → Orbit (2–3 d) → Mensuration (4–6 d)
   → OneMindXR (6–10 d, after S1) → replay (3–5 d, after A10) → F6 (3–5 d).

## Unverified items

- Whether any of the ~12,400 unmarked EnginePatch files differ from stock 5.3.2 (e.g.
  `DataChannel.cpp`, `DemoNetDriver.cpp`, `ReplaySubsystem.cpp`, OpenCV). **Blocker:**
  `gh api repos/EpicGames/UnrealEngine` returned 404 (this account has no Epic-linked
  GitHub access), so a blob-SHA diff against `5.3.2-release` was not possible.
- The `WITH_VH` count (74) comes from GitHub code search, which indexes only the default
  branch (`main` = 5.4-RELEASE-7) and skips large files. The 5.5 branch may differ.
  `EnginePatch/Source/Runtime/Engine/Private/RepLayout.cpp` could not be fetched raw
  (download failed).
- Which binary plugin consumes the Slate focus-lock, `SceneViewport`, PlayerController
  hit-test and `SceneComponent` access patches (consumers are inside DLLs).
- Whether `WriteNodeHierarchy` is public in stock 5.8 (doc page lists it under "Static"
  without access level).
- Whether custom `UChannel` classes work under Iris (5.8 docs silent). They are relevant
  only if UE-native replication is chosen.
- UE 5.8 Apple HTTP backend (NSURLSession vs libcurl).
- Licence of the bundled `commoncommo` binaries and TAKKernel DLLs (e.g. `kdu_v64R.dll`),
  and of VR-TAK `.uasset` art. Not needed if none are copied.
- macOS arm64 build of `commoncommo` (spike S4).
- All day estimates are judgement, not measured.
- Blockers met during this lane: GitHub code-search rate limit (10/min) hit once and
  retried after reset; Epic UnrealEngine repo inaccessible (404). No permission-system
  denials.

## Independent verification (2026-10-29)

A separate verifier re-fetched the primary sources (`gh api` against `TAK-Product-Center/VR` @41ad0bba/`main`; UE 5.8 API pages; GitHub code search). 18 claims were checked.

- **Confirmed:** S1 metadata (GPL-3.0, 8,620,868 KB, push 2026-04-02); branches, SHAs and the 5.5-RELEASE-0 release (2025-09-07, 0 assets); `LICENSE` 404 on 5.5 and present on `main`; README 5.3.2/EnginePatch/VS2022; `bUsePrecompiled = true` (VH_VR, VH_Replication); 0 hand-written `.cpp` under `Source/` for VH_Replication/VR/SessionPlayback/Plugins/Globe, and 0 commits to their `Private` dirs; `ChannelDefinitions` in DefaultEngine.ini lines 69-75; `CustomizeForVH` sets SSLCERT/SSLKEY/CAINFO and `SSL_VERIFYHOST=false` (`CurlHttp.cpp:205-227`), called at `VH_COTManager.cpp:472`; the XmlParser `WITH_VH` additions; OpenCV includes at `VH_COTManager.cpp:18-24` with no `cv::` usage; TCP/SSL/QUIC at `VH_COTConnectionInfo.cpp:871-893`; Gv2F restricted-rights header, `gv2f.lic`, Win64-only Commo/Gv2F; the `GetNewPositionAndRotation` hook (`MotionControllerComponent.cpp:131`, `Game_MotionControllerComponent.cpp:102`); Orbit 300 m/20 m/s, `u-d-o`, `SetViewTargetWithBlend`; Mensuration chord `/100.f` (line 180), `u-d-m-r/a/h`; CEF `ignore_certificate_errors = true` (`WebBrowserSingleton.cpp:278`); `UVoiceChannel` delegates; `bSlatedForDeletion`/`FInvalidPlugin`; `WITH_VH` code search = 74, all under `EnginePatch/`; the UE 5.8 FXmlNode, FXmlFile, UMotionControllerComponent, UReplaySubsystem and Iris statements.
- **Corrected in place:** the StereoRendering patch (it adds `-nohmd`; `-vr` is stock); the Mensuration `.uasset` count (33); the VOIP line range (72-119); the Commo listener class (`CommoListener`, not `UVH_COTManager`); the "0 `.cpp`" wording (generated `.gen.cpp` files exist).
- **Marked unverified:** whether code plugins can be unmounted in 5.8 (`bAllowUnloadCode` overload).
- **Scratch caveat:** the files under the lane scratch `vrtak/src` differ from the 41ad0bba blobs (for example `VH_TAKVideoPlayer.cpp` is 1,043 lines there and 1,047 at 41ad0bba), so they look like `main` copies. The per-file line counts in this document match 41ad0bba.
- **Rules:** no licence-rule violation was found (no worldmonitor or worldwideview material, and no VR-TAK code is copied here beyond identifiers and a short legal-notice quote). No cluster access was used.
