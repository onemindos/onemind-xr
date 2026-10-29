---
status: proposed — pending spike S3
date: 2026-10-29
---

# 0003 — Reaching the Meta Quest Pro (DK94EC)

The Quest Pro is standalone and Android-based. Quest Link needs a Windows PC, so the
owner's Mac cannot drive it over Link.

Options, in the order to try:

1. **Standalone build:** package the client for Android with OpenXR and install it on
   the headset. Best for offline field use. Risk: the headset's mobile GPU limits
   tile detail and entity counts; Cesium for Unreal Android builds have open
   community issues.
2. **Streamed:** run the client on a PC or GPU host and stream to the headset
   (Pixel Streaming in the headset browser, or a PC VR streaming path).
3. **WebXR:** the L0 web view in the headset's browser with WebXR.

Spike S3 picks the first option that holds the headset's native frame rate with the
globe and one live layer.
