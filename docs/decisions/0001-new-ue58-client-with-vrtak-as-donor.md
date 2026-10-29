---
status: accepted
date: 2026-10-29
---

# 0001 — Build a new UE 5.8 client; use VR-TAK as donor code, not as the base

## Context

VR-TAK is the most capable open TAK 3D client, but it is an engine fork of UE 5.3.2
(10,081 patched engine files), ships Commo only for Windows, and cannot use current
Cesium for Unreal, which supports UE 5.6 and newer. The owner wants the client to
run on a Mac first, stream to the web, and run on a Meta Quest Pro.

## Decision

Create a new Unreal Engine 5.8 project with Cesium for Unreal as the globe. Port
selected VR-TAK plugins (CoT, mensuration, orbit, replication, VR interaction) as
GPL-3.0 code with attribution. Do not rebase VR-TAK's engine patch.

## Consequences

- Runs on current Unreal on macOS, Windows, Linux and Android.
- This repo is GPL-3.0.
- Porting effort per plugin. Anything that depends on VR-TAK's engine patch must be
  rewritten.
- If spike S1 fails, revisit with the newest engine and Cesium pair that works on
  the Mac.
