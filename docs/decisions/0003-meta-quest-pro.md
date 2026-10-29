---
status: proposed. S3 has not run, because Unreal is not installed on the build machine. The owner must run S3 with the headset
date: 2026-10-29
updated: 2026-10-29 (M0 synthesis, from spike results)
---

# 0003 — Reaching the Meta Quest Pro (DK94EC)

The Quest Pro is a standalone, Android-based headset. Quest Link needs a Windows PC, so
the owner's Mac cannot drive it over Link, and there is no VR Preview on macOS
(`docs/research/unreal-platform.md` §S3 design).

## Options, in the order to try (decision 0004 #2)

1. **Standalone build.** Package the client for Android with Epic's OpenXR plugin and install it
   on the headset with `adb`. This is best for offline field use.
2. **WebXR.** Run the L0 web client (CesiumJS) in the headset browser with WebXR.
3. **Streamed.** Pixel Streaming 2 into the headset browser through the PS2 frontend's WebXR
   controller. This works only if S2 passes on the Mac (decision 0002).

Decision 0004 #2 puts WebXR before streaming. The original draft of this file put
streaming second, and this update corrects that order.

## Spike S3 result: not run

| Item | Result | Source |
|---|---|---|
| Could S3 run on the build machine? | **No.** Unreal Engine is not installed (it needs admin rights, about 150 GB, and 25 GiB was free). There is no Android SDK or NDK, and no headset is attached | `docs/PREFLIGHT.md`; `docs/research/unreal-platform.md` (lane header) |
| Is Quest Pro still a target? | Yes. Meta's compatibility page (updated 2026-10-07) still lists the `questpro` manifest identifier. No end-of-support date was found (*unverified*) | fact 13 [S-MCOMPAT]; Risks #8 |
| Meta XR plugin on UE 5.8 | **Fails to load** (version mismatch; Meta status "Investigating"). There is also a 2025 report of a splash-screen crash when building on macOS | fact 14 [F-META-58], [F-META-MAC] |
| Cesium for Unreal on Android/Quest | v2.30.0 lists Android as a target. Cesium's Quest guide uses Epic OpenXR with the Oculus plugin off. Known issues: #1712 (a Quick Launch crash, but a full package works) and #913 (seams when packaged) | facts 5, 15 [G-CES-UPL], [S-CQ], [G-CES-ISS] |
| UE 5.8 Android toolchain | NDK r27c, SDK 35 (34 minimum), build-tools 35.0.1, OpenJDK 21.0.3 | fact 3 [S-ANDREQ] |
| NATS on the headset | nats.c supports macOS, Linux and Windows only. An Android build is *unverified*. The WebSocket route is proven on the server side | facts 18–20 [G-NATSC], [C-WS] |
| Headset frame rate with the globe and one layer | **Not measured.** No Quest Pro measurement of Cesium for Unreal exists (*unverified*) | Risks #7 |
| Overall | **Unproven.** No option has been tested on the headset | — |

S3 did not fail. It could not run. So the 0004 #2 fallbacks (WebXR, then streaming) are
**not triggered**, and the standalone build is still the default.

## What the run does now

| Topic | What applies | Why |
|---|---|---|
| Plugin choice for the standalone build | Epic **OpenXR only**. The Meta XR plugin stays disabled until F-META-58 is closed, so Meta-only features (eye-tracked foveation, passthrough APIs) are out of scope for now | `docs/research/unreal-platform.md` §S3 design, Recommendation #4 |
| Build settings | Cesium's Quest settings: arm64, Vulkan, Forward Shading, MSAA 4x, Mobile Multi-View, occlusion culling, terrain SSE 256. No Lumen, Nanite or virtual shadow maps. Always use a full package, never Quick Launch. Target 72 fps (FEATURES J3) with a reduced layer budget | fact 15 [S-CQ]; §S3 design; Risks #4 |
| Bus transport for the headset | `IOneMindTransport` with a WebSocket transport (`WsNatsTransport`) as the Android route, in case nats.c does not build for Android. The client-side cost is *unverified* | `docs/research/unreal-platform.md` §S5 design, Recommendation #2 |
| Local bus | **0004 #12 fallback in use:** local nats-server (TCP 4222, WebSocket 9222), because the run has no NATS user or leaf to the cluster | `docs/PREFLIGHT.md` (Access) |
| Basemap | **0004 #4 fallback in use:** open tiles and terrain. No Cesium ion token is present | `docs/PREFLIGHT.md` (Access) |
| Mesh | **0004 #3 fallback in use:** the omos mesh gateway is the only mesh path, because the Commo build (S4) was blocked. The headset never joins the mesh directly | `docs/research/tak-server-and-bridges.md` §S4; omos `services/bridge/bridges/tak/` |
| WebXR fallback readiness | The L0 client (J1) is built in this run (decision 0002), so option 2 can be tried without Unreal. Meta advises feature detection, not user-agent sniffing | fact 17 [S-MWEBXR] |
| Where the build and tests happen | Decision 0004 **#17**: F4 is "built, awaiting owner check" | `docs/PREFLIGHT.md` |

## How the owner closes S3

Run step **U5 (S3)** in `docs/research/unreal-platform.md` (Owner checklist steps) after S1
passes:

1. Install Android Studio with SDK 35 and 34, build-tools 35.0.1, NDK r27c and platform-tools.
   Turn on developer mode on the headset (this needs a Meta developer team and a verified
   account; [S-MDEV]).
2. Package the APK on the Mac with OpenXR only. Run `aapt dump xmltree` and check that
   `com.oculus.supportedDevices` includes `questpro`. Whether Epic's OpenXR plugin sets this by
   itself is *unverified*. Without it, Horizon OS may run the app in compatibility mode (Risks #3).
3. Install it with `adb` and measure fps with the globe and one live layer. **Pass:** the
   headset's native frame rate (FEATURES F4; `docs/ROADMAP.md` S3). Save the evidence under
   `docs/evidence/M5/`.
4. **Fail:** apply **0004 #2**. Try WebXR with the L0 client in the headset browser next.
   Use PS2 streaming only if S2 passed. Record the result here and in the 0004 run log.

All headset tests use simulated assets only (`sim-*`; decision 0004 #13).
