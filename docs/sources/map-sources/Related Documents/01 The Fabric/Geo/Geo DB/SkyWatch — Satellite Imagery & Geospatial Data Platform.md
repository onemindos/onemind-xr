---
id: "notion-3bad0cf70a7e81158ba2d554f4fa137a"
title: "SkyWatch — Satellite Imagery & Geospatial Data Platform"
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
notion_tags: ["Drones", "TAK", "Architecture"]
notion_category: "Geo"
notion_id: "3bad0cf70a7e81158ba2d554f4fa137a"
notion_url: "https://app.notion.com/p/SkyWatch-Satellite-Imagery-Geospatial-Data-Platform-3bad0cf70a7e81158ba2d554f4fa137a"
notion_created: "2026-08-12"
notion_last_edited: "2026-08-15T22:32:00.000Z"
imported: "2026-10-16"
---

# SkyWatch — Satellite Imagery & Geospatial Data Platform

> SkyWatch is a unified geospatial data marketplace — 700+ sensors, 400+ providers, satellite imagery, aerial, drone, radar. Single account, transparent pricing, no contracts. Key to OneMind's intelligence fusion layer.
---
# 🌍 What Is SkyWatch
SkyWatch is the world's leading geospatial data platform — a marketplace and toolchain that unifies satellite imagery, aerial photography, drone data, and radar from 400+ providers into a single API, GIS plugin, and procurement workflow.
Founded in 2014, headquartered in Waterloo, Ontario. Esri Gold Partner since 2022. Used by 150,000+ users, delivered 10M+ km² of imagery in the last year.
---
# 🧩 The SkyWatch Product Family
## EXPLORE — Discover & Purchase
Web-based catalog search. Find and buy imagery from a unified archive of optical, radar, and aerial data. Compare providers side-by-side with transparent pricing before purchase.
- Filter by resolution, date, cloud cover, sensor type
- Preview footprints overlaid on the map
- Archive and tasking (order fresh captures)
- URL: https://skywatch.com/explore/
## HUB — Enterprise Procurement
Multi-team imagery procurement, storage, and spend management. Centralize purchasing, licensing, and data sharing across an org.
- Controlled team access
- Payment and licensing in one place
- URL: https://skywatch.com/hub/
## MAP — Native GIS Integration
Bring SkyWatch directly into ArcGIS Online, Enterprise, and Pro. Search, task, and ingest imagery without leaving your GIS environment.
- ArcGIS Pro Add-In (free)
- Content Store for ArcGIS Online & Enterprise
- No switching between portals or vendor contracts
- URL: https://skywatch.com/map/
## BUILD — Developer API
Pay-as-you-use REST API with access to 400+ providers. No bulk commitments, no procurement cycles. Build imagery into any application.
- 400+ providers in one API
- Transparent per-km² pricing
- Tasking, archive, delivery via API
- Docs: https://api.skywatch.com/docs
## CONNECT — Data Distribution
For satellite operators and data producers. Distribute your archive and tasking capability to 150K+ buyers. SkyWatch handles commerce, licensing, and delivery.
---
# 📡 Data Coverage
## Sensor Types
- Optical — high-res multispectral, panchromatic, RGB
- SAR (Synthetic Aperture Radar) — all-weather, day/night, cloud-penetrating
- Aerial — sub-meter resolution, urban detail
- Drone — ultra-high-res, custom AOI captures
- Hyperspectral — vegetation, minerals, environmental
- Video — moving imagery from satellites
## Scale
- 700+ sensors and data sources
- 95% of all commercial imaging satellites covered
- 150,000+ users on the platform
- 10M+ km² delivered in the last year
---
# 🗺️ OneMind Integration
## Why SkyWatch Matters for OneMind
OneMind's geo stack (PostGIS, Tile38, QGIS, TAK) is built to ingest, store, and display geospatial intelligence. SkyWatch is the acquisition layer — the source of fresh satellite imagery that feeds into that stack for real-time and historical analysis.
## Integration Points
### 1. QGIS Server (in geo namespace)
SkyWatch MAP ArcGIS plugin covers ArcGIS workflows. For QGIS, use the BUILD API to pull imagery tiles directly into QGIS via WMS/WMTS or direct download.
```bash
# Pull imagery via SkyWatch API into QGIS
# 1. Get API key from skywatch.com/build
# 2. Use QGIS XYZ Tiles with SkyWatch tile endpoint
# 3. Or download GeoTIFF via API and load as raster layer
```
### 2. TAK Server — Map Layers
Ingest SkyWatch imagery as KML/GeoPackage overlays into TAK. Push fresh satellite captures to all TAK clients (ATAK, WebTAK, CloudTAK) as map layers.
```bash
# Export from SkyWatch → GeoTIFF → convert to MBTiles → push to TAK
gdal_translate input.tif output.mbtiles -of MBTILES
# Upload to TAK Server data package, distribute to all clients
```
### 3. NATS Intelligence Fusion
Trigger SkyWatch API calls via Legacy when a geofence event fires on the fabric. Automatically task fresh imagery over an AOI when an alert is raised.
```json
# NATS-triggered SkyWatch tasking
# Subject: fabric.intel.skywatch.task
# Payload:
{
  "aoi": {"lat": 40.7128, "lon": -74.0060, "radius_km": 10},
  "resolution": "0.5m",
  "sensor_type": "optical",
  "triggered_by": "geofence.breach"
}
```
### 4. PostGIS — Store & Query Imagery Metadata
Store SkyWatch scene footprints, metadata, and acquisition history in PostGIS. Query available coverage by AOI and date range.
```sql
-- Query available SkyWatch scenes over NYC
SELECT scene_id, provider, resolution, acquired_at, cloud_cover
FROM skywatch_scenes
WHERE ST_Intersects(footprint, ST_MakePoint(-74.006, 40.7128)::geography::geometry)
AND acquired_at > NOW() - INTERVAL '30 days'
ORDER BY acquired_at DESC;
```
---
# 🏗️ Architecture Fit
> SkyWatch sits at the top of the OneMind intelligence pyramid: Physical sensors → NATS bus → ClickHouse → SkyWatch API (tasking & archive) → PostGIS (metadata) → QGIS/TAK (display)
```bash
┌─────────────────────────────────────────────┐
│            ONEMIND GEO STACK                │
├─────────────────────────────────────────────┤
│  TAK / QGIS / CloudTAK  ← Display Layer     │
│  PostGIS + Tile38        ← Storage Layer     │
│  SkyWatch API            ← Acquisition Layer │
│  NATS fabric.intel.*     ← Trigger Layer     │
│  Legacy (AI)             ← Decision Layer    │
└─────────────────────────────────────────────┘
```
---
# 💲 Pricing Model
- Pay-as-you-use — no subscriptions, no minimums, no annual contracts
- Per km² pricing shown upfront before purchase
- Credit card or prepayment
- Archive imagery: cheaper than tasking
- Tasking (fresh capture): priced per collection
- Free account to explore catalog and pricing before committing
---
# 🔗 Key Links
- Main: https://skywatch.com
- MAP (GIS plugin): https://skywatch.com/map/
- BUILD (API): https://skywatch.com/build/
- API Docs: https://api.skywatch.com/docs
- EXPLORE (catalog): https://skywatch.com/explore/
- AI Data Advisor: https://skywatch.com (homepage)
---
# 📋 Status & Next Steps
> Status: 📋 RESEARCH — evaluate BUILD API for NATS-triggered tasking over OneMind AOIs
- [ ] Create free SkyWatch account
- [ ] Test BUILD API over NYC/Stamford corridor AOI
- [ ] Evaluate pricing for 0.5m optical over 10km² AOI
- [ ] Prototype NATS geofence → SkyWatch task trigger via Legacy
- [ ] Load sample GeoTIFF into QGIS Server for TAK overlay test
- [ ] Compare vs USGS EarthExplorer for historical archive depth
---
# 🔄 Related Pages
- Elevation Data — DTED, SRTM, USGS & TAK Integration
- PostGIS — Geospatial Extension for PostgreSQL
- TAK Server & Client Ecosystem — Reference Guide
- FLIR Black Recon — Vehicle-Organic UAS Profile
- omos-cloud — DOKS Cluster Live Reference
