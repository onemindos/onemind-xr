---
id: "notion-3bad0cf70a7e81e48259fb993d46767b"
title: "Elevation Data — DTED, SRTM, USGS & TAK Integration"
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
notion_tags: ["TAK", "Infrastructure", "Drones"]
notion_category: "Geo"
notion_id: "3bad0cf70a7e81e48259fb993d46767b"
notion_url: "https://app.notion.com/p/Elevation-Data-DTED-SRTM-USGS-TAK-Integration-3bad0cf70a7e81e48259fb993d46767b"
notion_created: "2026-08-12"
notion_last_edited: "2026-08-15T22:32:00.000Z"
imported: "2026-10-16"
---

# Elevation Data — DTED, SRTM, USGS & TAK Integration

> Elevation data gives every asset on the fabric a Z-axis — terrain awareness for drones, robots, vehicles, and operators. ATAK/CivTAK uses DTED format natively. This page unifies the format spec, all public data sources, and the OneMind pipeline for getting terrain into TAK.
---
## What Is Elevation Data?
Elevation data (Digital Elevation Models / DEMs) defines the height of the Earth's surface at every point on a grid. In TAK and OneMind it enables: terrain-following drone flight paths, line-of-sight calculations, 3D map rendering, accurate CoT altitude, and realistic simulation environments.
- Two types: DTM (Digital Terrain Model) — bare ground, no buildings/trees. DSM (Digital Surface Model) — includes all objects above ground.
- Units: meters above mean sea level (MSL) or WGS84 ellipsoid (HAE). TAK CoT uses HAE.
- Formats used in OneMind: DTED (.dt0/.dt1/.dt2), GeoTIFF (.tif), HGT (SRTM)
---
## DTED — Digital Terrain Elevation Data
> DTED is the native elevation format for ATAK, CivTAK, WinTAK, and all TAK clients. Must be in DTED format to load into TAK devices.
### Format Specification
- Developed by the U.S. Department of Defense (DoD) — National Geospatial-Intelligence Agency (NGA)
- Grid-based raster format — elevation value at each lat/lon cell
- File extensions: .dt0 (Level 0), .dt1 (Level 1), .dt2 (Level 2)
- Coordinate system: WGS84 geographic (EPSG:4326)
```plain text
DTED Level | Resolution  | Cell Size    | Use Case
───────────|─────────────|──────────────|──────────────────────────────
Level 0    | 900m (~30") | ~1km grid    | Global overview, coarse terrain
Level 1    | 90m  (~3") | ~100m grid   | Regional ops, general flight
Level 2    | 30m  (~1") | ~30m grid    | Tactical ops, drone navigation
Level 3    | 10m         | ~10m grid    | High-detail (restricted, DoD only)
Level 4    | 3m          | ~3m grid     | Very high-detail (classified)

```
### File Structure
- Each file covers 1° × 1° of latitude/longitude (e.g. N40W074 = 40°N, 74°W)
- Filename convention: N40W074.dt2 → lat N40, lon W074, level 2
- Directory structure: DTED/W074/N40.dt2 (longitude folder → latitude file)
- Public DoD DTED is available only at Level 0 — Levels 1+ require DoD access or SRTM substitution
### dted.org — Public DTED API
- Site: dted.org — public DTED endpoint server for CONUS (continental US)
- Provides free DTED Level 1 (90m) and Level 2 (30m) tiles via REST API
- Coverage: Continental United States — not global
- Use for: pulling CONUS tiles for ATAK field ops, preloading NYC/Stamford area for Zeus
```bash
# dted.org API — fetch a DTED tile
# Tile for NYC area (N40W074)
curl https://dted.org/api/tile/N40W074/2 -o N40W074.dt2

# Place in ATAK DTED directory on S24
adb push N40W074.dt2 /sdcard/atak/dted/2/w074/n40.dt2

```
---
## Data Sources
### 1. USGS EarthExplorer — earthexplorer.usgs.gov
> Primary U.S. government source for elevation data. Free. Requires USGS account (free registration).
- Access: earthexplorer.usgs.gov — USGS EROS (Earth Resources Observation and Science Center)
- Datasets available: SRTM 1-Arc (30m), SRTM 3-Arc (90m), NED (National Elevation Dataset), 3DEP (1m LiDAR)
- 3DEP (3D Elevation Program): USGS national LiDAR program — 1m resolution for most of CONUS as of 2026
- Download formats: GeoTIFF, ArcGrid, IMG — convert to DTED using GDAL
- Best for: Highest-resolution CONUS data (1m 3DEP LiDAR), authoritative U.S. government source
- Note: Large area downloads require bulk order via EarthExplorer or M2M API (machine-to-machine)
```bash
# Convert USGS GeoTIFF to DTED Level 2 using GDAL
gdal_translate -of DTED -co PROFILE=DTED_2 input.tif N40W074.dt2

# Batch convert a directory
for f in *.tif; do
  gdal_translate -of DTED -co PROFILE=DTED_2 $f ${f%.tif}.dt2
done

```
### 2. SRTM — Shuttle Radar Topography Mission
- Source: NASA / NGA — Space Shuttle Endeavour radar mission (February 2000)
- Coverage: 56°S to 60°N latitude — ~80% of Earth's land surface
- Resolution: SRTM1 = 1 arc-second (~30m), SRTM3 = 3 arc-seconds (~90m)
- Format: HGT files (.hgt) — convert to DTED with GDAL
- Download: earthexplorer.usgs.gov or NASA Earthdata (search 'SRTM')
- Best for: Global coverage, offline ops outside CONUS, free with no registration for basic tiles
```bash
# Convert SRTM HGT to DTED
gdal_translate -of DTED N40W074.hgt N40W074.dt2

```
### 3. Copernicus DEM (EU)
- Source: European Space Agency (ESA) — Copernicus program
- Resolution: GLO-30 (30m global) and GLO-90 (90m global)
- Coverage: Global — better than SRTM in some regions (better void filling, updated 2021)
- Download: spacedata.copernicus.eu — free, requires ESA account
- Best for: Operations outside CONUS/NATO where SRTM has voids or low quality
### 4. CivTAK Elevation Guide — civtak.org/elevation-data
- Community resource — step-by-step guide for loading elevation data into ATAK/CivTAK
- Covers: DTED directory structure on Android, file placement, verifying terrain is active in ATAK
- Key path on Android: /sdcard/atak/dted/<level>/<lon_folder>/<lat_file>.dt<level>
- Verifying in ATAK: Settings → Elevation → check 'Use Elevation Data' is enabled
---
## Source Comparison
```plain text
Source          | Resolution | Coverage    | Free | Format  | Best For
────────────────|────────────|─────────────|──────|---------|--------------------------
dted.org        | 30–90m     | CONUS only  | ✅   | DTED    | Direct ATAK load, CONUS ops
USGS EarthExplorer | 1m–30m  | CONUS best  | ✅   | GeoTIFF | Highest res US data (3DEP)
SRTM (NASA)     | 30–90m     | 80% global  | ✅   | HGT     | Global offline ops
Copernicus DEM  | 30–90m     | Global      | ✅   | GeoTIFF | EU/global, better than SRTM
DoD DTED L2+    | 30m–10m    | Global      | ❌   | DTED    | Military ops (restricted)

```
---
## OneMind DTED Pipeline
### Step 1 — Download Tiles
```bash
# Option A: dted.org (CONUS, fastest, direct DTED)
# NYC/Stamford operational area tiles
curl https://dted.org/api/tile/N40W074/2 -o N40W074.dt2  # NYC
curl https://dted.org/api/tile/N41W074/2 -o N41W074.dt2  # Stamford CT
curl https://dted.org/api/tile/N40W073/2 -o N40W073.dt2  # Long Island

# Option B: USGS EarthExplorer (manual web UI)
# 1. Go to earthexplorer.usgs.gov
# 2. Draw AoI on map
# 3. Select dataset: SRTM 1 Arc-Second Global or 3DEP 1m
# 4. Download GeoTIFF → convert with GDAL

```
### Step 2 — Convert to DTED (if needed)
```bash
# Install GDAL
apt-get install gdal-bin  # or brew install gdal

# GeoTIFF → DTED Level 2
gdal_translate -of DTED -co PROFILE=DTED_2 input.tif output.dt2

# HGT (SRTM) → DTED Level 2
gdal_translate -of DTED N40W074.hgt N40W074.dt2

# Verify output
gdalinfo N40W074.dt2

```
### Step 3 — Load onto S24 Ultra (ATAK)
```bash
# ATAK DTED directory structure on Android
# /sdcard/atak/dted/<level>/<longitude_folder>/<latitude_file>.dt<level>

# Example: DTED Level 2 for NYC (N40, W074)
adb push N40W074.dt2 /sdcard/atak/dted/2/w074/n40.dt2

# Multiple tiles via script
for f in *.dt2; do
  # Extract lat/lon from filename (e.g. N40W074.dt2)
  lat=$(echo $f | grep -oP '[NS]\d+' | tr '[:upper:]' '[:lower:]')
  lon=$(echo $f | grep -oP '[EW]\d+' | tr '[:upper:]' '[:lower:]')
  adb push $f /sdcard/atak/dted/2/${lon}/${lat}.dt2
done

# Verify in ATAK:
# Settings → Device → Elevation → 'Use Elevation Data' = ON
# 3D map mode should now show terrain relief

```
### Step 4 — Load into PostGIS/TimescaleDB (for server-side queries)
```bash
# Convert DTED to GeoTIFF first
gdal_translate -of GTiff N40W074.dt2 N40W074.tif

# Load raster into PostGIS
raster2pgsql -s 4326 -I -C -M N40W074.tif public.elevation | \
  psql -h timescaledb-ha.geo.svc.cluster.local -U postgres -d onemind

# Query elevation at a point
SELECT ST_Value(rast, ST_SetSRID(ST_MakePoint(-74.006, 40.7128), 4326)) AS elevation_m
FROM elevation
WHERE ST_Intersects(rast, ST_SetSRID(ST_MakePoint(-74.006, 40.7128), 4326));

```
---
## TAK Integration — What Elevation Data Enables
- 3D terrain map in ATAK/CloudTAK/WebTAK — elevation contours, hillshade, terrain relief
- Line-of-sight (LoS) calculations — can operator at point A see asset at point B?
- Drone flight path planning — terrain-following, obstacle avoidance using DTED grid
- Accurate CoT altitude — HAE (height above ellipsoid) vs. AGL (above ground level) conversion
- FLIR Black Recon integration — autonomous path generation requires terrain mesh
- Simulation fidelity — OSSIM and Unreal Engine terrain use same DTED source
- Go2 robot terrain awareness — slope analysis, no-go zones from elevation gradient
---
## Zeus Operational Area — Pre-load Tiles
> Pre-load these tiles for NYC ↔ Stamford CT operational corridor. All available free from dted.org.
```plain text
# Zeus AO — NYC/Stamford corridor DTED Level 2 tiles
Tile        | Coverage
────────────|────────────────────────────
N40W074     | NYC, Jersey City, Newark
N40W073     | Long Island, eastern NYC
N41W074     | Stamford CT, Yonkers, lower Hudson
N41W073     | Greenwich CT, New Haven area
N40W075     | western NJ (optional)

```
---
## Implementation Status
> 📋 NOT YET LOADED — Elevation data pipeline not yet set up. ATAK devices do not currently have DTED loaded. Priority: download NYC/Stamford tiles from dted.org and push to S24 Ultra via ADB.
- 📋 Download CONUS tiles from dted.org for NYC/Stamford AO
- 📋 Push to S24 Ultra via ADB pipeline
- 📋 Load into TimescaleDB-HA PostGIS raster table for server-side LoS queries
- 📋 Integrate with FLIR Black Recon flight planning (terrain mesh)
- 📋 Feed into OSSIM simulation terrain layer
---
## Resources
- dted.org — Public DTED API (CONUS) — dted.org
- USGS EarthExplorer — earthexplorer.usgs.gov
- CivTAK Elevation Guide — civtak.org/elevation-data
- Copernicus DEM — spacedata.copernicus.eu
- NASA Earthdata (SRTM) — search.earthdata.nasa.gov
- GDAL DTED driver docs — gdal.org/drivers/raster/dted.html
---
## Related Pages
- PostGIS — Geospatial Extension for PostgreSQL — raster2pgsql, ST_Value
- TimescaleDB — Time-Series PostgreSQL Extension — elevation + position queries
- FLIR Black Recon — Vehicle-Organic UAS Profile — terrain-following flight
- TAK Server & Client Ecosystem — ATAK terrain layer
- awareness.md — Location dimension, altitude in CoT
- OSSIM — OneMind Sovereign Simulation Foundation — sim terrain
