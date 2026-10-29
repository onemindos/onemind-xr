---
id: "notion-3bad0cf70a7e815d9124cfb8cbd5812d"
title: "Esri GIS — Enterprise GIS & ArcGIS Platform"
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
notion_tags: ["Architecture"]
notion_category: "Geo"
notion_id: "3bad0cf70a7e815d9124cfb8cbd5812d"
notion_url: "https://app.notion.com/p/Esri-GIS-Enterprise-GIS-ArcGIS-Platform-3bad0cf70a7e815d9124cfb8cbd5812d"
notion_created: "2026-08-12"
notion_last_edited: "2026-08-15T22:32:00.000Z"
imported: "2026-10-16"
---

# Esri GIS — Enterprise GIS & ArcGIS Platform

> Esri is THE enterprise GIS company. Founded 1969 — the original geospatial software. ArcGIS platform covers desktop (Pro), server (Enterprise), cloud (ArcGIS Online), developer SDKs. Used by DoD, DHS, FEMA, Fortune 500. OneMind does NOT compete with Esri full-suite — wins via open, real-time, tactical intelligence.
---
## What GIS Is (Esri's definition)
- GIS = technology that connects data to a map — location data (where) + descriptive data (what)
- Purpose: map, analyze, communicate patterns, relationships, conditions
- Four use cases: choosing location, finding best route, keeping track, planning for future
---
## ArcGIS Full Stack
```plain text
Desktop: ArcGIS Pro (primary), ArcMap (legacy, deprecating)
Server: ArcGIS Enterprise (self-hosted web GIS), Portal for ArcGIS (content mgmt)
Cloud: ArcGIS Online (SaaS), ArcGIS Hub (community), StoryMaps, Dashboards
Developer SDKs: JavaScript, Native (iOS/Android), .NET, Qt, Unreal, Unity
Field Apps: Collector, Survey123, QuickCapture — mobile data collection

```
## ArcGIS Products (Key)
| Product | What It Is | OneMind Relevance |
| ArcGIS Pro | Desktop GIS — analysis, cartography | 🔄 Competitive |
| ArcGIS Enterprise | Self-hosted web GIS backend | 🔄 Competitive |
| ArcGIS Online | SaaS web GIS platform (below) | 🔄 Competitive |
| ArcGIS Hub | Community engagement portal | ⚪ External |
| ArcGIS StoryMaps | Narrative spatial storytelling | ⚪ Public communication |
| ArcGIS Dashboards | Real-time ops dashboards | 🔄 Competitive |
| ArcGIS Field Ops | Mobile collection (Collector, Survey123) | ⚪ TAK integration possible |
| 3D Tiles in Esri | 3D Tiles support since Pro 2.8 | ⚪ We use CesiumJS with 3D Tiles |
## OneMind vs Esri Comparison
| Capability | Esri ArcGIS | OneMind OS |
| Deployment | Windows Server | Kubernetes — any infra |
| Architecture | Windows-only, proprietary | Linux-native, open-source, K8s-first |
| Real-time | ArcGIS Dashboards — polling | NATS pub/sub — event-driven |
| 3D Globe | ArcGIS Pro 3D | CesiumJS — full web 3D |
| Mobile | ArcGIS Field Apps (iOS/Android) | Flutter — any platform |
| Data Format | File Geodatabase (proprietary) | Open: GeoJSON, GeoParquet, MVT, COG, 3D Tiles |
| ML/AI | ArcGIS Pro + partner models | NATS + any ML (Bedrock, vLLM) |
| Pricing | Enterprise licenses per user/product | FOSS base — no license fees |
| Suitability | Government standards, large orgs | Emerging tech, tactical, startups |
---
## Esri Integration into OneMind
- ArcGIS JavaScript SDK — web mapping library — Connects to Cesium 3D Tiles
- ArcGIS Hub — community engagement — could link Notion SO Wiki
- field Operations (Survey123/Collector) — TAK field data collection
- 3D Tiles import — ArcGIS Pro 2.8+ imports 3D Tiles — WebODM outputs to ArcGIS
## Relevant Links
- ArcGIS Pro: esri.com/arcgis/products/arcgis-pro
- ArcGIS Enterprise: esri.com/arcgis/products/arcgis-enterprise
- ArcGIS Online: esri.com/arcgis/products/arcgis-online
- Developer: developers.arcgis.com
- 3D Tiles: github.com/CesiumGS/3d-tiles/tree/main/specification
---
## Where OneMind Fits
> We don't replace Esri. We solve its problems: speed, ownership, cost.

Esri: $100K+ per seat for full enterprise stack, Windows lock-in, slow polling.

OneMind: $0 license, Linux/K8s, real-time NATS, any platform, modern stack. Win by being the open, fast, flexible alternative for geospatial teams that reject enterprise GIS lock-in.
