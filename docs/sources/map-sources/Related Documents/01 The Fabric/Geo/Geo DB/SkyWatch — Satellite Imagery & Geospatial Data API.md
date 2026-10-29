---
id: "notion-3bad0cf70a7e8101b3b8f04e26b412df"
title: "SkyWatch — Satellite Imagery & Geospatial Data API"
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
notion_tags: ["Infrastructure"]
notion_category: "Geo"
notion_id: "3bad0cf70a7e8101b3b8f04e26b412df"
notion_url: "https://app.notion.com/p/SkyWatch-Satellite-Imagery-Geospatial-Data-API-3bad0cf70a7e8101b3b8f04e26b412df"
notion_created: "2026-08-12"
notion_last_edited: "2026-08-15T22:32:00.000Z"
imported: "2026-10-16"
---

# SkyWatch — Satellite Imagery & Geospatial Data API

> SkyWatch is the world's leading geospatial data marketplace — 700+ sensors, 95% of commercial imaging satellites, one unified API. This is the procurement layer. Vexcel = data provider (one of hundreds). SkyWatch = marketplace connecting buyers to ALL providers. Already serves 150K+ users, 12M+ km² delivered in 12 months, 8M+ API calls.
---
## What SkyWatch Is
- Geospatial data marketplace — one catalog, one API to 700+ data providers
- One POST to order, one GET to receive — satellite/aerial/drone imagery
- Services: Identity, Archive & Tasking, Order Management, Fulfillment, Payments, Policy, Locations
- AI Data Advisor — describe your problem in plain language, get imagery recommendations
- Archive or task fresh captures — same API for both
---
## Scale
| Metric | Value |
| Sensors & data sources | 700+ |
| Commercial imaging satellites covered | 95% |
| Users on platform | 150,000+ |
| Data delivered last 12 months | 12 million km² |
| API calls handled | 8M+ in last 12 months |
| Active organizations | 7,500+ |
---
## SkyWatch Products (The 5 Pillars)
| Product | Purpose | OneMind Use |
| Explore | Search catalog, compare providers, transparent pricing, purchase | Imagery procurement |
| Hub | Procure, store, manage, share across org | Centralized procurement + licensing |
| Map | Direct integration into ArcGIS/QGIS desktop | Pull data into existing GIS |
| Build | Developer API — one POST/one GET — embed imagery | OneMind programmatic imagery |
| Connect | Sell your data to 150K+ buyers | If OneMind captures proprietary data |
---
## SkyWatch API — One POST, One GET
> Their core promise: 'ship features, not infrastructure'. Single API to 400+ (now 700+) providers.
```bash
# STEP 1: Place an order with one POST
curl -X POST https://api.skywatch.com/v1/orders \
  -H "Authorization: Bearer $SKYWATCH_API_KEY" \
  -H "Content-Type: application/json" \
  -d '{
    "aoi": "-122.42,37.77,-122.39,37.80",
    "resolution": "0.5m",
    "source": "archive"
  }'
# → Returns order_id, status, estimated_delivery

# STEP 2: Retrieve delivered data with one GET
curl https://api.skywatch.com/v1/orders/ord_8fa2 \
  -H "Authorization: Bearer $SKYWATCH_API_KEY"
# → status: "delivered"
# → format: "COG + STAC"
# → download_url: presigned URL

```
- AOI = Area of Interest — bbox or GeoJSON polygon
- Resolution filter: 0.3m, 0.5m, 1m, 10m, etc.
- Source filter: archive (existing) or tasking (fresh capture)
- Output: Cloud Optimized GeoTIFF (COG) + STAC metadata
- Delivery: presigned S3 URLs — direct download or stream
---
## Platform Services
| Service | What It Handles |
| Identity | User creation, KYC validation, role/permission management |
| Archive & Tasking | Search 700+ sensors, order existing or task fresh captures |
| Order | Schedule delivery, maintain status, notify on completion |
| Fulfillment | Ingest from providers, quality check (cloud cover %), stitch |
| Payments | E-commerce for small-parcel imagery, anti-fraud, sales tax |
| Policy | Pricing rules, permissions, territory/AOI restrictions |
| Locations | Geocoding, AOI processing, stored search areas |
## Data Types Available
- Optical: True-color, NIR, SWIR bands — from Pleiades, Planet, SkySat, WorldView, SPOT
- SAR: Radar imagery — day/night, all-weather — from Capella, ICEYE, Umbra
- Hyperspectral: 100+ bands — material identification — from Pixxel, Orbital Sidekick
- Aerial: Fixed-wing and helicopter surveys — like Vexcel data (Vexcel IS a SkyWatch provider)
- Drone: Local drone captures from partners
- Open Data: Free sources — Sentinel, Landsat — also via same API
---
## OneMind Integration
```plain text
Data flow into OneMind geo stack:

  SkyWatch API (order)
    └──► Presigned S3 URL
          └──► COG + STAC item
                ├──► STAC API pod (geo ns) → catalog discovery
                ├──► TimescaleDB → metadata → STAC items
                ├──► TiTiler → COG → tile service
                └──► Martin → vector overlay

Use cases:
  • Basemap imagery for TAK/omos-ui (via TiTiler)
  • Tasking fresh captures for AOIs (mission prep)
  • SAR over cloudy regions (day/night, all-weather)
  • Change detection (archive timestamps compare)
  • ML training datasets (ordered previews → bulk)

```
---
## Vexcel vs SkyWatch
| Aspect | Vexcel | SkyWatch |
| Role | Data provider — owns aircraft + cameras | Marketplace — aggregates 700+ providers |
| Coverage | 45+ countries (their flights) | Global (all providers) |
| Resolution | 3-6in (aerial) | 0.3m+ (satellite), 3-6in (aerial via providers) |
| Pricing | Subscript或/per-area licensing | Pay-per-use + subscription |
| OneMind role | Imagery supplier | Imagery gateway — access Vexcel + others |
| API | WMTS/MVT/Salesforce/ImageServer | REST + STAC + COG |
> SkyWatch is an aggregator — Vexcel is a provider you access THROUGH SkyWatch (or direct). OneMind gets both: SkyWatch for breadth + flexibility, Vexcel direct for their high-res aerial areas.
---
## Integration Path for OneMind
```plain text
IMMEDIATE:
  □ Sign up at skywatch.com → get API key
  □ Query NYC/Stamford AOI → see available imagery
  □ Run curl order → receive COG in minutes
  □ Add COG to TiTiler/STAC catalog

BUILD:
  □ SkyWatch → Redpanda Connect feed → on-demand imagery ingest
  □ Policy engine: auto-order imagery when AOIs marked
  □ SkyWatch MCP tool — Legacy can 'order imagery' naturally

SCALE:
  □ Budget controls + approval workflow in Hub
  □ Auto-tasking for high-priority AOIs
  □ ML pipeline: order preview → train model → bulk order for training

```
---
## Industries Served
- AEC — construction site monitoring, planning
- Energy/Oil/Gas — pipeline inspection, site monitoring
- Forestry — canopy health, land-use change
- Agriculture — crop health, field conditions
- Insurance — property risk, claims verification
- Government — public safety, urban planning
---
## Relevant Links
- Main site: https://skywatch.com
- Developer docs: https://docs.skywatch.com
- API reference: https://docs.skywatch.com/api-reference
- Explore UI: https://app.skywatch.com — search + buy
---
> SkyWatch = procurement gateway for geospatial data. Vexcel is one of 700+ providers. OneMind uses SkyWatch as the marketplace for ONE API to satellite/aerial imagery — then stores it in our own geo stack (STAC catalog, TiTiler, Martin) and delivers it via TAK mission packs to TAK clients.
