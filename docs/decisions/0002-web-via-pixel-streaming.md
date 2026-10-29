---
status: proposed — pending spike S2
date: 2026-10-29
---

# 0002 — The web client is the same app, via Pixel Streaming 2

## Decision

Browsers get the full client through Pixel Streaming 2, so the web never drifts from
desktop. A light L0 web view (CesiumJS or MapLibre on the same contracts) serves
phones and weak connections.

## Open

Pixel Streaming 2 on macOS is unconfirmed for UE 5.8 (see findings §5). Spike S2:

1. Stream from the owner's Mac on the LAN. If it works, this is the M3 path.
2. If not, stream from a Linux host with an NVIDIA GPU (home box, or a GPU node
   added through an omos PR with owner approval). The Mac stays the native client.

Either way the browser is never offline-capable; disconnected use stays with native
clients and TAK on the mesh.
