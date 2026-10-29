---
id: "notion-3bad0cf70a7e81a2bcabd51cb66a22d0"
title: "Cesium — 3D Geospatial Globe & Visualization Platform"
type: "reference"
status: "active"
created: "2026-10-16"
updated: "2026-10-16"
owner: "zeus"
tags: ["type/reference", "domain/so", "collection/so", "kind/record"]
collection: "so"
kind: "record"
notion_database: "Geo DB"
notion_status: "Not started"
notion_tags: ["TAK", "Drones"]
notion_category: "Geo"
notion_id: "3bad0cf70a7e81a2bcabd51cb66a22d0"
notion_url: "https://app.notion.com/p/Cesium-3D-Geospatial-Globe-Visualization-Platform-3bad0cf70a7e81a2bcabd51cb66a22d0"
notion_created: "2026-08-12"
notion_last_edited: "2026-08-15T22:32:00.000Z"
imported: "2026-10-16"
---

# Cesium — 3D Geospatial Globe & Visualization Platform

> Cesium is THE open platform for 3D geospatial — the world's most accurate web-based globe engine. OneMind already uses Cesium for TAK 3D view (tak3d.tail717ba0.ts.net). Google, Komatsu, NVIDIA, Unity, Unreal Engine are official partners. 15.6k stars, 46k commits, basis of CesiumJS (open source), Cesium ion (SaaS data), 3D Tiles (OGC standard), Cesium for Unity and Cesium for Unreal
## What It Is
- Open platform for 3D geospatial software development
- The globe engine: renders WGS84 Earth with terrain/imagery in any browser
- 3D Tiles: OGC community standard for streaming massive 3D datasets (buildings, point clouds, terrain)
- enant: 3D Tiles Next → glTF extensions for city-scale AEC workflows
- Community: 15.6k GitHub stars, 46k commits, Fedora Forums active, contributor program
---
## Cesium Products
```plain text
Product            | What It Is | Status in OneMind
───────────────────|────────────|───────────────────────────────────────────
CesiumJS           | Open-source JavaScript 3D globe library | ✅ USED — tak3d.tail717ba0.ts.net
Cesium ion         | SaaS platform — 3D tiling, hosting, streaming | ⚪ Optional — could use for terrain/imagery
3D Tiles           | OGC standard for streaming massive 3D | ✅ Via CesiumJS
Cesium for Unity   | Unity engine integration (XR/VTOL) | 🚧 PLANNED — Unity 7 + Unreal page
Cesium for Unreal  | Unreal Engine plugin | 🚧 PLANNED — Unity 7 + Unreal page
Cesium for Omniverse| NVIDIA Omniverse connector | ⚪ Available if needed
Sandcastle         | Live code playground + examples | ✅ For testing

```
---
## CesiumJS — The OneMind Usage
> TAK 3D View: tak-webview-cesium pod in takserver ns serves CesiumJS 3D map from TAK Server CoT data — shows drones, aircraft, ground units on 3D globe. URL: tak3d.tail717ba0.ts.net
```plain text
Architecture:
  TAK Server CoT (UDP multicast)
    └──► takserver-nats (TAK↔NATS bridge) → fabric.tak.cot
          └──► takserver Postgres (mission DB)
                └──► tak-webview-cesium pod serves the CesiumJS UI
                      └──► Browser renders 3D globe with live CoT tracks

Data flow:
  NATS cots → JSON → CesiumJS Entity API → 3D model/position on globe

```
- Currently: self-hosted CesiumJS on a K8s pod — full control, no external deps
- Imagery: local or provider (Bing/Aerial) via Cesium ion token (optional)
- Value: 3D perspective = see drone altitude, LOS obstructions, terrain effects
---
## 3D Tiles — The Streaming Standard
- OGC standard for streaming massive 3D datasets — geospatial equivalent of HLS/DASH for video
- OGC 3D Tiles 2.0 = glTF extensions for AEC (CAD-like workflows)
- Data types: city models (3D buildings), point clouds, terrain, photogrammetry
- Sources: ContextCapture, Pix4D, LIDAR surveys — any 3D Tiles source renders in Cesium
## Cesium for Unity + Cesium for Unreal
> 🚧 PLANNED — Unity 7 + Unreal Engine 5 page covers the XR/VR cross-engine combination. Cesium for Unity + Unreal are the 3D globe layers in those engines. This is how you build VR TAK/AR overlay tools.
- Cesium for Unity: full Cesium globe inside Unity projects (XR/VR/AR/mobile/desktop)
- Cesium for Unreal: full Cesium globe inside Unreal Engine 5 (high-fidelity 3D)
- Both are Apache 2.0, official Cesium products — not third-party hacks
## OneMind geospatial rendering stack
```plain text
Data layers (bottom to top):
  Base      → Cesium World Terrain / ion imagery (optional)
  Street    → OpenStreetMap via Martin tile server
  Geo-vec   → Martin (tiles: pg_featureserv, STAC API)
  Imagery   → CloudTAK tiles / WebODM orthos
  3D Mesh   → 3D Tiles from photogrammetry / LiDAR

🌐 CesiumJS (web)
   └── Renders all layers above in 3D globe view

🖥️ omos-ui (Flutter)
   └── MapLibre → 2D tactical map (primary)
   └── Cesium via WebView → secondary 3D view

🎮 Unity 7 + Unreal 5 (planned)
   └── Overlays Cesium globe in Unity/Unreal projects
   └── XR/VR/AR applications

```
## TAK Integration — Why Cesium Matters
- TAK Server 5.7: CoT data flows to TAK WebTAK UI (2D) + tak-webview-cesium (3D)
- 3D view: See droppoint altitude, terrain blocking LOS, building heights in urban operations
- CloudTAK: data packages stream to CesiumJS for 3D view of shared mission data
- VRTAK (planned): Cesium for Unity in VR headset — full 3D mission in VR
## Cesium ion — Terrain & Imagery Service
```plain text
Cesium ion provides:
  • Cesium World Terrain — quantized-mesh terrain tiles
  • Bing Maps Aerial imagery
  • Global 3D building models
  • Point cloud data sources

OneMind: ⚪ Not using ion SaaS yet — self-hosted CesiumJS works without it
  → Future: paid ion tier for global 3D buildings + terrain
  → Alternative: AWS 3D Tiles (Amazon provides NYC, SF, etc. buildings)

```
- Optional: ion s free tier covers basic usage — signup, generate token, use in CesiumJS
- Alternative: self-host terrain tiles from SRTM/ETOPO1 global elevation
## Relevant Links
- CesiumJS: github.com/CesiumGS/cesium — open source, 15.6k stars
- Cesium ion: cesium.com/ion — SaaS 3D data platform
- 3D Tiles Spec: ogc.org/standards/3DTiles
- Cesium for Unity: github.com/CesiumGS/cesium-unity
- Cesium for Unreal: github.com/CesiumGS/cesium-unreal
- Sandcastle (live playground): sandcastle.cesium.com — try examples without install
---
## Next Steps
```plain text
IMMEDIATE:
  □ tak3d view — verify CesiumJS pod healthy in takserver ns
  □ Sandcastle Copilot — try AI-assisted globe building
  □ Add Cesium ion free token → Bing imagery in tak3d

SOON:
  □ Cesium for Unity → VR/AR TAK overlay (VRTAK)
  □ Cesium for Unreal → high-fidelity training sim
  □ 3D Tiles from WebODM photogrammetry (buildings, terrain)

FUTURE:
  □ 3D Tiles 2.0 — vector data in 3D (roads, parcels in volumetric view)
  □ Custom terrain from LiDAR surveys
  □ Omniverse Cesium → digital twin of facilities

```
---
> Cesium in OneMind: already used for TAK 3D view. Cesium for Unity/Unreal connects to the planned VRTAK/XR efforts. 3D Tiles standard is the data backbone — investigate generating custom 3D Tiles from WebODM photogrammetry for future training/sim.
