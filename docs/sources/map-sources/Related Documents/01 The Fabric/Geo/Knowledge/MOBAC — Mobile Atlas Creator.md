---
id: "notion-3c2d0cf70a7e81bdaf9ee8b2a15c4ffd"
title: "MOBAC — Mobile Atlas Creator"
type: "reference"
status: "active"
created: "2026-10-16"
updated: "2026-10-16"
owner: "zeus"
tags: ["type/reference", "domain/so", "collection/so", "kind/knowledge"]
collection: "so"
kind: "knowledge"
notion_database: "Sovereign Operations "
notion_status: "Not started"
notion_area_2: "🌐 Fabric"
notion_category: "Geo"
notion_id: "3c2d0cf70a7e81bdaf9ee8b2a15c4ffd"
notion_url: "https://app.notion.com/p/MOBAC-Mobile-Atlas-Creator-3c2d0cf70a7e81bdaf9ee8b2a15c4ffd"
notion_created: "2026-08-20"
notion_last_edited: "2026-09-14T03:38:00.000Z"
imported: "2026-10-16"
---

# MOBAC — Mobile Atlas Creator

> MOBAC is the tool for building offline ATAK map packages. Download tiles from OSM, satellite imagery, USGS topo — export as map packages for ATAK. Essential for DDIL deployments where live map tiles aren't available.
https://mobac.sourceforge.io/
# What is MOBAC?
Mobile Atlas Creator (MOBAC) is an open-source tool that downloads map tiles from online sources and packages them into offline atlases for mobile and GPS applications. For ATAK, MOBAC is the primary tool for creating offline map packages — download your AO (Area of Operations) imagery while connected, deploy to ATAK for disconnected field use.
# Why It Matters for OneMind / DDIL
- DDIL operations — no internet in the field means no live map tiles; MOBAC pre-loads the AO
- ATAK map packages — operators see satellite and topo imagery on ATAK without connectivity
- Multiple zoom levels — download at street, neighborhood, and regional zoom for full coverage
- Pre-mission planning — prep the map package before deploying, distribute to all ATAK devices
# Supported Map Sources
- OpenStreetMap — standard OSM, OSM cycle, OSM transport overlays
- USGS National Maps — US topo, imagery, and elevation data
- Satellite imagery — Bing, Google (via plugins), and other tile servers
- Custom tile servers — point MOBAC at any XYZ/TMS tile server
- NZ Topo, regional maps — 30+ supported sources out of the box
# ATAK Workflow
```plain text
1. Open MOBAC → select map source (satellite, OSM topo, USGS)
2. Draw bounding box over AO (Area of Operations)
3. Select zoom levels (e.g., z10–z18 for regional to street)
4. Select output format: ATAK / TAK Map Package
5. Build atlas → exports .zip map package
6. Transfer to ATAK device (sideload via USB or TAK Server)
7. Enable in ATAK: Maps → Offline Maps → import package
8. Operators now have offline imagery at all zoom levels
```
# Output Formats
- ATAK map package (.zip) — primary format for ATAK offline maps
- OsmAnd — for OsmAnd navigation apps
- Garmin KMZ — custom maps for Garmin GPS devices
- OruxMaps, TrekBuddy, OziExplorer — 30+ supported app formats
# Reference
- ATAKwizard tutorial: MOBAC Tutorial: Easily Download ATAK Imagery (YouTube)
https://www.youtube.com/watch?v=JXXLLgsgh9w
# Action Items
- [ ] Download MOBAC and build a test map package for a known AO
- [ ] Watch ATAKwizard MOBAC tutorial before first use
- [ ] Create pre-mission AO map package SOP — zoom levels, sources, distribution method
- [ ] Test distribution via TAK Server — push map package to all connected ATAK clients
