---
status: accepted
date: 2026-10-29
decided-by: Zeus (owner), so the build run never has to stop and ask
---

# 0004 — Owner defaults for the build run

The run applies these instead of asking. Each one may be revisited later by the
owner. A run that finds a default impossible picks the listed fallback, records
why in this file, and continues.

| # | Question | Default | Fallback |
|---|---|---|---|
| 1 | Web client path | Pixel Streaming 2 from the owner's Mac (spike S2) | If S2 fails: the L0 light web client (CesiumJS on the same contracts) is the web path, and a Linux GPU streaming host is **documented, not provisioned** |
| 2 | Quest Pro path | Standalone Android/OpenXR build | Then WebXR in the headset browser (L0 client), then streaming. Record the result in decision 0003 |
| 3 | Commo in the client | Build atak-civ `commoncommo` for macOS arm64 (spike S4) | Keep the omos mesh gateway as the only mesh path; feature C4 moves to "blocked" with the build error recorded |
| 4 | Basemap and terrain | Open data by default (OpenStreetMap-derived imagery, open terrain tiles, self-hosted 3D Tiles from WebODM). Cesium ion is optional, behind config, using a token from the keychain if present | Google Photorealistic 3D Tiles are **off** (metered, separate terms) |
| 5 | GPU host or new paid infrastructure | **None.** Do not create, buy or resize anything that costs money | Document what would be needed and its cost |
| 6 | `MESH_TO_TAK_SERVER` | `false` by default; `true` only in the owner's local Mac config for testing | — |
| 7 | Raw TAK wire subject (omos NATS-DOCTRINE §9.8) | **Option 1:** `tak.cot.*` is the raw wire; the entity service derives per-UID state. No TAK Server change | — |
| 8 | New NATS subjects | Use the existing grammar only. If something truly has no pattern, use the closest existing pattern and write a proposal in omos `docs/` for the owner | — |
| 9 | Repo visibility | `onemind-xr` stays private | — |
| 10 | Merges | Merge freely in `onemind-xr` after review. In omos, open PRs only; the owner merges | Keep building on unmerged omos branches locally (below) |
| 11 | Unmerged omos work | Run omos services locally on the build machine (local NATS server, or a leaf to the cluster when credentials exist) from their branches, so client work never waits for a merge | — |
| 12 | Missing access or credentials (Cesium token, TAK certificate, NATS user, cluster) | Use the local fallback: local NATS, the mesh gateway with a scripted mesh peer and `takproto` test messages in place of ATAK, open tiles. Never ask; list what is missing in the final report | — |
| 13 | Real hardware | Simulated assets only (`sim-*`). No command to a real drone, robot or vehicle in this run | — |
| 14 | Engine version | Newest Unreal Engine 5.x that the current Cesium for Unreal supports, builds on the build machine and keeps a macOS target (target 5.8) | Next older supported pair |
| 15 | Licence conflicts | Never copy AGPL (worldmonitor) or Elastic (worldwideview) code; write it from scratch | — |
| 18 | Pipeline technology | Whatever the ingestion research lane scores best. It need not be NATS-native, but it must be open source, handle every delivery mode in the source catalog with per-source rate limits, and deliver into JetStream with the full reliability list. Migrate feeds one at a time, each with its own PR, keeping the old pipeline running until the new one is proven | Keep Redpanda Connect and harden it to the reliability list |
| 19 | Paid or keyed sources in the catalog | Free and open sources first. Keyed free sources only if a key is in the environment; paid sources are documented, never called | — |
| 16 | Naming | App: **OneMind**. Plugins: `OneMind*`. Bundle id: `org.onemindos.onemind` | — |
| 17 | The run is on a machine other than the owner's Mac | Build and test on the build machine; write every Mac, headset and LAN step into `docs/OWNER-CHECKLIST.md` with expected results; mark those features "built, awaiting owner check" | — |

## Run log

### 2026-10-29 — M0 (research and spikes)

**Fallbacks used so far**

| # | Default that could not be met | Fallback in use | Evidence |
|---|---|---|---|
| 3 | Build atak-civ `commoncommo` for macOS arm64 (spike S4) | The omos mesh gateway (`services/bridge/bridges/tak/`) is the **only mesh path**. Feature **C4 is blocked**. The build step `make TARGET=macos-64 GDAL_USE_KDU=no build_commoncommo` was denied: `Permission for this action was denied by the Claude Code auto mode classifier. Reason: [Code from External].` No workaround was attempted. The gateway passed mesh-to-bus tests on the build machine | `docs/research/tak-server-and-bridges.md` §S4, §6 |
| 12 | NATS run user or leaf to the cluster; TAK user certificate and `tak.tail717ba0.ts.net`; cluster sandbox namespace | Local nats-server 2.15.2 with JetStream (127.0.0.1:4222, WebSocket 9222, leaf 7422). The mesh gateway with a scripted mesh peer and `takproto` test messages in place of ATAK. The scripted MAVLink drone `sim-x1` in place of SITL, because the ArduPilot SITL build was denied by the same policy. No in-cluster spikes | `docs/PREFLIGHT.md` (Local stack, Access, SITL) |
| 4 | Cesium ion token from the keychain (`cesium-ion`) | Open imagery and open terrain tiles. Google Photorealistic 3D Tiles stay off | `docs/PREFLIGHT.md` (Access) |

**Spikes that did not run (no fallback triggered)**

| Spike | Status | Effect |
|---|---|---|
| S1, S2, S3, S5 (in-engine) | Not run. Unreal Engine is not installed (it needs admin rights and about 150 GB; 25 GiB was free) | Default #17 applies: Unreal work is source code plus owner-checklist steps U1–U5. Defaults #1 and #2 stand until the owner runs them. Fallback #1 (the L0 web client) is built anyway as J1. Decisions 0002 and 0003 record the details. The S5 server side (NATS WebSocket KV and subscriptions) was proven locally |

Sources: `docs/research/unreal-platform.md` (lane header, [C-WS], Owner checklist steps); `docs/decisions/0002-web-via-pixel-streaming.md`; `docs/decisions/0003-meta-quest-pro.md`.

**Proposal for the owner (not a change to the table above)**

- Add **CloudTAK** (`dfpc-coe/CloudTAK`, **AGPL-3.0**) to #15 as **design-only**, alongside
  worldmonitor and worldwideview. The reason: GPL-3.0 §13 allows combining it with AGPL-3.0
  code, but the combined network service would then carry AGPL source-offer duties. The
  competitive lane already read CloudTAK for design only and copied no code. Until the owner
  decides, the run treats CloudTAK as design-only. Source: `docs/research/competitive-matrix.md`
  (Risks: licence contamination; Recommendation #2).
