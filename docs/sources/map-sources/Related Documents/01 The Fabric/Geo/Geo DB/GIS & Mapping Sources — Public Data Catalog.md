---
id: "notion-3bad0cf70a7e814b8805cd74ac21d389"
title: "GIS & Mapping Sources — Public Data Catalog"
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
notion_tags: ["OSINT", "TAK"]
notion_category: "Geo"
notion_id: "3bad0cf70a7e814b8805cd74ac21d389"
notion_url: "https://app.notion.com/p/GIS-Mapping-Sources-Public-Data-Catalog-3bad0cf70a7e814b8805cd74ac21d389"
notion_created: "2026-08-12"
notion_last_edited: "2026-08-15T22:32:00.000Z"
imported: "2026-10-16"
---

# GIS & Mapping Sources — Public Data Catalog

> Public GIS data sources for PostGIS, QGIS, Martin tiles, TAK overlays, and CesiumJS. All free, no API key required.
https://download.geofabrik.de
---
## OSM & Base Maps
- GeoFabrik — OSM extracts any region, daily updates → PostGIS, Martin, Valhalla | https://download.geofabrik.de
- OpenStreetMap — full planet PBF → Overpass + Valhalla (already running) | https://openstreetmap.org
- Natural Earth — clean vector boundaries 1:10m/1:50m/1:110m → QGIS projects | https://naturalearthdata.com
- Geo-Maps — high quality programmatic GeoJSON boundaries | https://github.com/simonepri/geo-maps
---
## Administrative Boundaries
- GADM — global admin boundaries country/state/county/city polygons → QGIS + TAK WMS | https://gadm.org
- GeoNames — place name geocoding, 25M+ entries → Nominatim (already running) | http://www.geonames.org
- Database of Continents, Countries, States, Cities | https://www.back4app.com/database/back4app/list-of-all-continents-countries-cities
---
## Government ArcGIS Servers
MappingSupport.com — curated list of 7,500+ public government ArcGIS REST endpoints (federal → city). Updated every Wednesday by Joseph Elfelt.
https://mappingsupport.com
- How to use in ATAK: Settings → Maps → Add Map Layer → ArcGIS → paste URL
- How to use in QGIS: Layer → Add Layer → ArcGIS REST Server → paste URL
- How to use in CesiumJS: ArcGisMapServerImageryProvider({ url: '...' })
- ArcGIS Open Data Portal — 300k+ .gov datasets | http://opendata.arcgis.com
---
## 3D & Specialized
- 3D Semantic City Models (awesome-citygml) — open CityGML → pg2b3dm → TAK 3D Tiles | https://github.com/OloOcki/awesome-citygml
- IEEE GRSS DASE — remote sensing & Earth observation data | http://dase.grss-ieee.org
