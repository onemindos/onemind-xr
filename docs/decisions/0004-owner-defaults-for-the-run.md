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
| 18 | Pipeline technology | Whatever the ingestion-pipelines research lane scores best, as long as it is open source, NATS/JetStream-native or close to it, and meets the full reliability list. Migrate feeds one at a time, each with its own PR, keeping the old pipeline running until the new one is proven | Keep Redpanda Connect and harden it to the reliability list |
| 16 | Naming | App: **OneMind**. Plugins: `OneMind*`. Bundle id: `org.onemindos.onemind` | — |
| 17 | The run is on a machine other than the owner's Mac | Build and test on the build machine; write every Mac, headset and LAN step into `docs/OWNER-CHECKLIST.md` with expected results; mark those features "built, awaiting owner check" | — |
