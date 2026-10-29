---
id: "notion-3bad0cf70a7e81feab51d047623e98d6"
title: "Vexcel — Aerial Imagery & Geospatial Data Platform"
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
notion_tags: ["Infrastructure", "Drones", "TAK"]
notion_category: "Geo"
notion_id: "3bad0cf70a7e81feab51d047623e98d6"
notion_url: "https://app.notion.com/p/Vexcel-Aerial-Imagery-Geospatial-Data-Platform-3bad0cf70a7e81feab51d047623e98d6"
notion_created: "2026-08-12"
notion_last_edited: "2026-08-15T22:32:00.000Z"
imported: "2026-10-16"
---

# Vexcel — Aerial Imagery & Geospatial Data Platform

> Vexcel is the world's largest aerial imagery and geospatial data program — 45+ countries, 99.6% U.S. population coverage. Ready-to-use, oblique, ortho, 3D, AI-enhanced, multispectral. Enterprise-grade (not free). OneMind integrates into our geo stack for training data + tactical basemaps.
---
## What Vexcel Is
- World's largest unmanned aerial imagery & geospatial data program — 45+ countries
- 99.6% U.S. population coverage (urban, suburban, rural)
- Aerial photography at 3-15cm resolution — satellites x4 clearer than drones in scale
- Ground control points matched — mapping-grade accuracy, pixel-to-data consistent
- Enterprise SaaS — not free — licensing per data type/region
- Hey HQ: Vienna, Austria (EU), US ops Houston TX
---
## Products & Services
| Product | Resolution | Type | Use Case |
| Oblique | 3in / 7.5cm | 45° angle views from 4 directions | Property inspection, urban analysis, machine vision |
| Orthomosaic (Ortho) | 3-6in / 7.5-15cm | Top-down nadir+orthorectified | Property assessment, cadastral, construction |
| TrueOrtho | 3in / 7.5cm | No building lean, highest accuracy | Detailed urban analysis, 3D modeling |
| Wide Area Ortho | 6-8in / 15-20cm | Large-area ortho coverage | Regional planning, infrastructure mapping |
| Multispectral | CIR (Color Infrared) | NIR (near-infrared) | Vegetation health, impervious surfaces, land cover |
| .Disaster Imagery | Post-disaster event response | Rapid capture events | Hurricanes, wildfires, tornadoes, flooding |
| 3D Cities | 3D model data | City digital twins | 3D visualization, simulation, planning |
| Elevate | DSM + DTM (surface models) | Elevation data | DSM for 3D, DTM for terrain analysis |
| Elements | AI-detected attributes | Semantics information | Building damage, solarPotential, vegetation health |
| On Demand | Custom requests | Specific areas/events | Custom missions, specific AOI |
## Integration Capabilities
- Vexcel Viewer — web-based world viewer for their data — WMTS/WMS compatible
- Vexcel MCP — Vector Tile Feature Server (MVT) endpoint for their data
- VectorTile API — vector tile endpoints for dynamic web maps
- Arc GIS Image Services — Esri-compatible integration
- MapControl SDK — developer SDK for embedding
- WMTS — web map tile service — OGC standard
## OneMind Integration
```plain text
Data flows:
  Vexcel API → GeoStack (Martin/TileServer GL)
    ├── MVT endpoints  → tiles on-demand
    ├── WMS endpoint   → direct rendering
    ├── WFS endpoint   → feature queries
    └── Vexcel Viewer   → embed directly

TAK Integration:
  Vexcel Data + TAK Server + Packaged Mission Data
    └── Imagery packages with embedded metadata
    └── TAK clients render mission data on their base maps

ML/AI:
  Vexcel Intelligence + OneMind → custom AI model training
  Building detection, damage assessment, vegetation health
  Property attribution — automated building attributes

```
- ✅ Ready-to-use — georeferenced, processed, QA'd — no drone/hiring pilot overhead
- 📊 Enterprise SaaS — pay per data type/region — recurring imagery cost
- 📘 Not free — pricing per coverage area/data product — but saves over drone ops
- ⚙️ Integration: vector tile API, WMTS/WMS, image services, SDK, MCP server
---
## Coverage Statistics
| Metric | Value |
| Countries | 45+ (U.S., Canada, UK, Germany, France, Italy, Spain, Australia, Japan, etc.) |
| U.S. Population Coverage | 99.6% (urban, suburban, rural) |
| Resolution Range | 3in (7.5cm) to 6in (15cm) |
| Flight Fleet | Dedicated fixed-wing aircraft, calibrated cameras |
| Product Types | Oblique, ortho, TrueOrtho, multispectral, 3D Cities, elevElevate, Elements, disasters, On Demand |
---
## OneMind Fit
> Vexcel = enterprise imagery layer for OneMind.

✅ Use for: Basemaps, training data (ML/AI), 3D city models, disaster response, property analysis
❌ Don't use for: real-time tactical ops (too expensive), DoD source (classified/intel), classified areas

Deployment path:
  1. Get Vexcel API key — access to WMTS/MVT endpoints
  2. Add to CloudTAK — imagery tile server
  3. Consume in Martin/TileServer GL — vector tiles
  4. Use in Sentinel/QGIS — direct rendering
  5. Train AI models from Vexcel Intelligence datasets
---
## Links
- Main site: https://vexceldata.com
- Data products: https://vexceldata.com/vdp-aerial-imagery
- Coverage: viewable on vexceldata.com/coverage
- Platform docs: vexceldata.com/platform
- Public viewer: viewer.vexceldata.com
- Demo request: vexceldata.com/request-demo
