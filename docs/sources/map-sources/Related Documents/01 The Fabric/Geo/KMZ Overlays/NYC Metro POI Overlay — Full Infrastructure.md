---
id: "notion-3bed0cf70a7e8125afa0e934436c6293"
title: "NYC Metro POI Overlay — Full Infrastructure"
type: "reference"
status: "active"
created: "2026-10-16"
updated: "2026-10-16"
owner: "zeus"
tags: ["type/reference", "domain/so", "collection/so", "kind/record"]
collection: "so"
kind: "record"
notion_database: "KMZ Overlays"
notion_file_size: "6.0 MB (KMZ) / 63.8 MB (KML uncompressed)"
notion_coverage_area: "New York City Metro (40.45 to 40.95 lat, -74.30 to -73.65 lon)"
notion_build_date: "2026-08-13"
notion_status: "Built"
notion_script: "/opt/data/scripts/osm_to_kmz.py"
notion_layer_groups: 13
notion_poi_count: 127350
notion_data_source: "OSM/Overpass"
notion_id: "3bed0cf70a7e8125afa0e934436c6293"
notion_url: "https://app.notion.com/p/NYC-Metro-POI-Overlay-Full-Infrastructure-3bed0cf70a7e8125afa0e934436c6293"
notion_created: "2026-08-16"
notion_last_edited: "2026-08-16T02:17:00.000Z"
imported: "2026-10-16"
---

# NYC Metro POI Overlay — Full Infrastructure

> ✅ BUILT — 127,350 placemarks across 13 toggleable layer groups. Live on Zeus's Mac at ~/Desktop/OneMind-Overlays/nyc_metro_poi_overlay.kmz
---
# How to Load in ATAK
1. Transfer the KMZ file to your device (AirDrop, Google Drive, USB, or SCP to S24 via ADB)
1. Open ATAK → Menu (hamburger) → Import → Data Package
1. Select nyc_metro_poi_overlay.kmz
1. Open Overlay Manager (layers icon) — you'll see all 13 layer groups
1. Toggle any group on/off — each expands to sub-categories
1. Tap any placemark for full details (address, phone, operator, hours, all OSM tags)
Each placemark is color-coded by group: red for emergency, orange for surveillance, purple for military, blue for transportation, etc. Labels are off by default (scale 0) so the map stays clean — tap to see details.
---
# How to Regenerate / Customize
The build script is at /opt/data/scripts/osm_to_kmz.py on the Legacy pod. It takes about 12 minutes to run (Overpass rate limiting slows it down).
### Change the coverage area
```python
# In osm_to_kmz.py, edit the BBOX variable:
# Format: (south, west, north, east)
BBOX = (40.45, -74.30, 40.95, -73.65)  # NYC metro
# BBOX = (34.0, -118.5, 34.1, -118.1)  # LA
# BBOX = (42.3, -71.2, 42.4, -71.0)    # Boston
```
### Add new OSM categories
Edit the BATCHES list in the script to add any OSM tag. The Overpass API supports any tag the community has mapped. See https://wiki.openstreetmap.org/wiki/Map_Features for the full tag list.
```python
# Example: add wind turbines and solar farms
BATCHES.append([
    ("power", "generator", "generator:source", "wind"),
    ("power", "generator", "generator:source", "solar"),
])
```
### Rebuild from the command line
```bash
# On the Legacy pod:
cd /opt/data && python3 scripts/osm_to_kmz.py

# Output: nyc_metro_poi_overlay.kmz
# Raw JSON cache: nyc_metro_osm_raw.json
```
---
# File Locations
- KMZ file (delivery): ~/Desktop/OneMind-Overlays/nyc_metro_poi_overlay.kmz (Zeus's Mac)
- Build script: /opt/data/scripts/osm_to_kmz.py (Legacy pod)
- Raw JSON cache: /opt/data/nyc_metro_osm_raw.json (Legacy pod, 127K elements)
- Build log: /opt/data/osm_kmz_build.log (Legacy pod)
---
# Related Skills
- notion-workspace — Notion API operations for documenting overlays
- maps — 46 built-in OSM category searches via maps_client.py
- overpass-poi-search — Direct Overpass API queries for any custom OSM tag
- tailscale-pod-access — SCP delivery from Legacy pod to Mac/S24
---
> Built by Legacy (Mother) on August 13, 2026 · Powered by OpenStreetMap · Overpass API · OneMind OS
# Overview
Custom KMZ map overlay built from OpenStreetMap data via the Overpass API. Covers the New York City metropolitan area (5 boroughs + surrounding NJ suburbs) with 127,350 individual points of interest organized into 13 toggleable layer groups. Designed for import into ATAK as a data package — each layer group and sub-category can be toggled on/off independently in the Overlay Manager.
This is not a live data feed. It is a static snapshot of OSM's community-mapped infrastructure as of August 13, 2026. The overlay persists on the map until removed — no NATS connection required.
---
# Why OSM/Overpass Instead of Google Maps?
Google Maps is for looking at. OSM is for extracting and building with. For sovereign map overlays, OSM is the only play.
---
# How It Was Built — Step by Step
## Step 1: Query Overpass API
The Overpass API is the read-only query endpoint for OpenStreetMap. No API key required. We queried 79 OSM tag categories across the NYC metro bounding box (40.45°S, -74.30°W, 40.95°N, -73.65°E).
Queries were batched into 11 groups (8 tags per batch) to avoid Overpass rate limiting (504 Gateway Timeout / 429 Too Many Requests). Each batch saved incrementally to a JSON cache file so progress survived timeouts.
```bash
# Example Overpass query (one batch)
curl -s "https://overpass-api.de/api/interpreter" \
  --data-urlencode 'data=[out:json][timeout:120];
(
  node["amenity"="police"](40.45,-74.30,40.95,-73.65);
  node["amenity"="fire_station"](40.45,-74.30,40.95,-73.65);
  node["amenity"="hospital"](40.45,-74.30,40.95,-73.65);
  node["surveillance:type"="ALPR"](40.45,-74.30,40.95,-73.65);
);
out body;'
```
## Step 2: Categorize & Normalize
Each OSM element was classified into one of 13 layer groups based on its tags. Within each group, sub-categories provide finer granularity (e.g., Surveillance → ALPR Cameras, CCTV, Gunshot Detectors).
Each placemark carries: name (from OSM name tag), coordinates (lat/lon), address (from addr:street/housenumber), phone (from phone/contact:phone), operator (from operator tag), and all original OSM tags as description metadata.
## Step 3: Generate KML
KML (Keyhole Markup Language) is the XML format that TAK/ATAK natively imports. Each POI becomes a <Placemark> with name, description, Point coordinates, Style (icon + color), and Folder hierarchy (Group → Sub-category → Placemarks).
```xml
<?xml version="1.0" encoding="UTF-8"?>
<kml xmlns="http://www.opengis.net/kml/2.2">
  <Document>
    <Folder>
      <name>Surveillance</name>
      <Folder>
        <name>ALPR Cameras</name>
        <Placemark>
          <name>FLOCK</name>
          <description>operator=Flock Safety</description>
          <Point><coordinates>-74.0,40.7,0</coordinates></Point>
        </Placemark>
      </Folder>
    </Folder>
  </Document>
</kml>
```
## Step 4: Zip to KMZ
KMZ is simply a zipped KML file. The 63.8 MB KML compressed to 6.0 MB — small enough to AirDrop, email, or push through TAK Server. Command: zip -j output.kmz doc.kml
## Step 5: Deliver to Mac
SCP'd from the Legacy pod (DOKS) to Zeus's MacBook Pro via Tailscale egress: ~/Desktop/OneMind-Overlays/nyc_metro_poi_overlay.kmz
---
# Layer Groups & Counts
13 top-level folders, each with sub-category folders. All toggleable independently in ATAK Overlay Manager.
> Total: 127,350 placemarks across 13 groups / 51 sub-categories
---
# Key Findings
## Surveillance Layer (Benn Jordan's Point)
1,799 ALPR cameras mapped in NYC metro alone — Flock Safety, NYPD, NYC MTA, PANYNJ, Westchester County Police all operating license plate readers. 551 additional CCTV cameras, 81 ShotSpotter gunshot detectors, 79 speed cameras. This is the crowdsourced counter-surveillance layer: OSM community members map these so anyone can see where the cameras are.
Benn Jordan's content highlighted that Flock cameras — marketed as proprietary police infrastructure — are being mapped on OSM as a transparency effort. The surveillance:type=ALPR tag was created by the community specifically for this purpose.
## Military Layer
98 military installations mapped in the NYC metro: 23 checkpoints, 27 bunkers, 45 military offices, 2 naval bases, 1 border control point. These are mostly federal installations — Fort Hamilton area, naval yards, federal building security.
## Emergency Infrastructure
254 police stations (NYPD precincts), 298 fire stations (FDNY houses), 134 hospitals, 22 ambulance stations, 1,018 emergency shelters, 103 fire hydrants. Complete first-responder overlay for the metro area.
---
# CoT & NATS Pipeline Context
This KMZ is the static snapshot version of what could also be a live data pipeline. The original conversation explored two approaches:
- Static (this KMZ): Query once → cache → generate KML → zip to KMZ → import into ATAK → toggle on/off. Persists on map until removed. No NATS required.
- Live (future): Publish each POI as a CoT (Cursor on Target) event atom to NATS JetStream → TAK subscribes and renders overlays in real-time. Good for dynamic data (buses, trains, traffic) but ephemeral — disappears when NATS connection drops.
For static infrastructure (police stations, hospitals, cameras), KMZ is the right choice — permanent, toggleable, no dependency on the bus.
## CoT Atom Structure (for reference)
Each POI becomes one CoT event XML atom with: UID (unique), Point (lat/lon), Type (a-u-G for ground unit), IconsetPath (icon), and Details (metadata).
```xml
<event version="2.0" uid="OSM-123456" type="a-f-G-U-CV" time="2026-08-13T12:00:00Z" start="2026-08-13T12:00:00Z" stale="2027-08-13T12:00:00Z" how="h-g-i">
  <point lat="40.7128" lon="-74.0060"/>
  <detail>
    <track speed="0" course="0"/>
    <contact name="FLOCK" phone=""/>
    <remarks>operator=Flock Safety, surveillance:type=ALPR</remarks>
  </detail>
</event>
```
## NATS Subject Taxonomy (Planned)
If this becomes a live feed, POIs would flow through the OMOS subject taxonomy:
```plain text
omos.raw.geographic.poi          ← raw OSM elements
omos.cot.ground.emergency        ← police, fire, hospital, ambulance
omos.cot.ground.infrastructure    ← fuel, power, telecom, water
omos.cot.ground.transportation    ← bus, rail, ferry, parking
omos.cot.ground.commercial        ← shops, pharmacies, food
omos.cot.ground.hazard            ← industrial, pipelines, dams
→ sinks: TAK map overlay, ClickHouse Time Machine (10yr TTL)
```
---
# Full OSM Category Catalog
OSM has far more than the 13 groups we used. Here is the complete catalog of what we found in the NYC metro from a single exploratory query — 64,492 nodes across 51 categories. Any of these can be added to future KMZ builds.
## Surveillance & Security
## Emergency & Infrastructure
## Transportation & Daily Life
OSM also has (not queried but available): prisons, courthouses, embassies, pipelines, quarries, landfills, water sources, wetlands, rivers, canals, runways, helipads, airports, stadiums, hotels, hostels, campsites, every shop type (hundreds), government offices, clinics, dentists, mechanics, electricians, plumbers — any tag the community has mapped.
---
# Surveillance Operators Found in NYC
The ALPR camera layer revealed specific operators running license plate reader networks across the NYC metro. These were extracted from the OSM operator= tag on each camera node.
One camera was explicitly named FLOCK at 40.8180N, 74.2799W — the only one with a name= tag set to the product name.
> Benn Jordan's content highlighted that Flock cameras — marketed as proprietary police infrastructure — are being crowdsourced onto OSM as a transparency effort. The surveillance:type=ALPR tag was created by the community specifically for this purpose.
---
# Build Statistics
---
# Overpass API Technical Details
## Endpoints and Mirrors
- Primary: https://overpass-api.de/api/interpreter
- Fallback: https://overpass.kumi.systems/api/interpreter
- No API key required — fully free, open
## Rate Limiting and Batching
Overpass is a community-run service with shared capacity. During peak hours (US daytime), large queries return 504 Gateway Timeout or 429 Too Many Requests. The build script handles this by:
1. Batching 79 tag categories into 11 groups (~8 tags per batch)
1. Retrying each batch 3x on primary mirror, then falling back to kumi.systems mirror
1. 10-second delay between batches to be a good citizen
1. Incremental JSON save after each successful batch — progress survives timeouts
Even with batching, some batches still hit 504 on the first attempt and needed retries. The transportation batch alone returned 35K+ elements (bus stops and parking dominate).
## Query Structure
Each batch is a single Overpass QL query that unions multiple tag selectors over the bounding box:
```plain text
[out:json][timeout:120];
(
  node["amenity"="police"](40.45,-74.30,40.95,-73.65);
  node["amenity"="fire_station"](40.45,-74.30,40.95,-73.65);
  node["amenity"="hospital"](40.45,-74.30,40.95,-73.65);
  node["surveillance:type"="ALPR"](40.45,-74.30,40.95,-73.65);
);
out body;
```
The [timeout:120] gives the server 2 minutes to respond per batch. [out:json] returns structured JSON instead of raw OSM XML.
---
# OSM Tag to CoT Domain Mapping
This table maps the OSM tags we queried to their CoT (Cursor on Target) domain categories. This is the bridge between raw OSM data and TAK map overlay structure. Each domain becomes a top-level folder in the KMZ, and each OSM tag becomes a sub-folder.
---
# Google Maps vs OSM — Why We Chose OSM
Zeus asked: should we use Google Maps or OpenStreetMap for extracting POIs? The answer was clear:
> Google Maps is for looking at. OSM is for extracting and building with. For sovereign map overlays on the OneMind fabric, OSM is the only play.
---
# Next Steps & Expansion
## Immediate
- Upload nyc_metro_poi_overlay.kmz to ATAK on S24 and verify layers toggle correctly
- Test placemark detail view — confirm address, phone, operator, OSM tags display
- Verify color coding renders correctly per layer group
## Near-Term
- Build additional city overlays (LA, Boston, DC) by changing the BBOX variable
- Add more OSM categories (prisons, courthouses, embassies, pipelines, quarries, airports, stadiums)
- Push KMZ through TAK Server for automatic distribution to all connected devices
## Future / Live Pipeline
- Convert static KMZ to live CoT atoms published to NATS JetStream (omos.cot.ground.*)
- Set up weekly cron job to re-query Overpass for delta changes and regenerate KMZ
- Integrate real-time data sources: MTA GTFS-RT for live bus/train positions, 511NY for traffic
- Build CoT event types that match TAK icon standards (a-f-A for friend, a-h-A for hostile, etc.)
- Store all POIs in ClickHouse Time Machine for temporal queries (10-year TTL)
## Other Data Sources to Explore
- FCC — antenna registration database (cell towers, broadcast)
- EPA — toxic release inventory (TRI) facilities
- FEMA — flood zones, hazard mitigation
- USGS — stream gauges, earthquake faults
- NOAA — weather stations, marine buoys
- NTSB — transportation incident data
- HIFLD — Homeland Infrastructure Foundation-Level Data (hospitals, schools, utilities)
| Factor | Google Maps | OSM/Overpass |
| Cost | $17 per 1,000 API calls | Free, no key, no limits |
| Bulk POI extraction | ToS prohibits scraping/export | Designed for bulk download |
| Owning derived data | Can't pipe into your own map | ODbL license — you own derived CoT |
| Custom categories | Their categories only | Any tag the community has mapped |
| Data freshness | Google decides | Community edits, visible changesets |
| Layer Group | Count | Sub-categories |
| Surveillance | 2,674 | ALPR (1,799), CCTV (551), Gunshot (81), Speed (79), Sensors (22), Guards (3) |
| Emergency | 1,830 | Police (254), Fire (298), Hospitals (134), Shelters (1,018), Ambulance (22), Hydrants (103) |
| Military | 98 | Checkpoints (23), Bunkers (27), Offices (45), Naval (2), Border (1) |
| Transportation | 36,887 | Bus Stops (15,885), Parking (18,619), Gas (1,393), Rail (714), Ferry (71), EV (162) |
| Infrastructure | 3,566 | Power Towers (2,628), Substations (488), Towers (301), Water Towers (108), Dams (41) |
| Hazards | 1,082 | Industrial Zones (1,080), Environmental (2) |
| Food & Supply | 24,891 | Restaurants (10,503), Fast Food (6,018), Cafes (3,045), Bars (1,637), Supermarkets (1,516) |
| Medical | 4,768 | Pharmacies (1,373), Doctors (1,053), Dentists (929), Clinics (871), Childcare (300), Vet (242) |
| Finance | 2,695 | Banks (1,767), Post Offices (523), ATMs (405) |
| Public Services | 9,458 | Schools (3,415), Worship (4,976), Libraries (452), Courts (78), Prisons (27), Town Halls (147) |
| Water & Sanitation | 10,049 | Waste Baskets (5,844), Drinking Water (1,685), Toilets (1,037), Recycling (626), Fountains (529) |
| Recreation | 19,204 | Sports Pitches (12,539), Parks (3,809), Hotels (853), Theatres (334), Museums (242), Cinemas (116) |
| Aviation | 106 | Helipads (91), Airfields (15) |
| Category | Count | OSM Tag |
| ALPR cameras | 1,795 | surveillance:type=ALPR |
| CCTV cameras | 550 | surveillance:type=camera |
| Other surveillance | 136 | man_made=surveillance |
| Gunshot detectors | 81 | surveillance:type=gunshot_detector |
| Sensors | 22 | surveillance:type=sensor |
| Speed cameras | 79 | highway=speed_camera |
| Guard posts | 2 | surveillance:type=guard |
| Military checkpoints | 14 | military=checkpoint |
| Military bunkers | 9 | military=bunker |
| Military offices | 20 | military=office |
| Naval bases | 2 | military=naval_base |
| Border control | 1 | barrier=border_control |
| Category | Count | OSM Tag |
| Fire hydrants | 9,707 | emergency=fire_hydrant |
| Fire stations | 298 | amenity=fire_station |
| Police stations | 123 | amenity=police |
| Hospitals | 22 | amenity=hospital |
| Ambulance stations | 22 | emergency=ambulance_station |
| Shelters | 171 | amenity=shelter |
| Power towers | 2,628 | power=tower |
| Fuel/gas stations | 580 | amenity=fuel |
| EV charging | 152 | amenity=charging_station |
| Dams | 2 | waterway=dam |
| Industrial zones | 15 | landuse=industrial |
| Category | Count | OSM Tag |
| Bus stops | 15,884 | highway=bus_stop |
| Railway stations | 706 | railway=station |
| Parking | 459 | amenity=parking |
| Restaurants | 9,500 | amenity=restaurant |
| Cafes | 2,905 | amenity=cafe |
| Bars | 1,515 | amenity=bar |
| Supermarkets | 1,169 | shop=supermarket |
| Pharmacies | 1,266 | amenity=pharmacy |
| Banks | 1,302 | amenity=bank |
| Schools | 1,756 | amenity=school |
| Places of worship | 2,026 | amenity=place_of_worship |
| Parks | 312 | leisure=park |
| Operator | Description | Example Location |
| Flock Safety | Commercial ALPR vendor — directly operated | 40.4155N, 74.1668W |
| NYPD | New York Police Department | 40.7050N, 74.0166W |
| NYC MTA | Metropolitan Transportation Authority | 40.7456N, 73.9734W |
| PANYNJ | Port Authority of NY and NJ (bridges/tunnels) | 40.7299N, 74.0382W |
| Westchester County Police | Regional data sharing program | 40.9788N, 73.8682W |
| Riverside Court | Court-operated ALPR | 40.8002N, 74.0638W |
| Metric | Value |
| Total POIs extracted | 127,350 |
| Layer groups | 13 |
| Sub-categories | 51 |
| Overpass batches | 11 |
| OSM tag categories queried | 79 |
| Build time | 761 seconds (~12.7 min) |
| Raw KML size | 63.8 MB |
| Compressed KMZ size | 6.0 MB (10.6x compression) |
| Raw JSON cache | /opt/data/nyc_metro_osm_raw.json |
| Bounding box | 40.45S, -74.30W, 40.95N, -73.65E |
| Coverage area | NYC 5 boroughs + surrounding NJ suburbs |
| CoT Domain | OSM Tags | Layer Group in KMZ |
| Emergency | amenity=police, fire_station, hospital, emergency=ambulance_station, fire_hydrant, shelter | Emergency |
| Surveillance | surveillance:type=ALPR, camera, gunshot_detector, sensor, guard, speed_camera, man_made=surveillance | Surveillance |
| Military | military=checkpoint, bunker, office, naval_base, barrier=border_control | Military |
| Transportation | highway=bus_stop, railway=station, amenity=parking, bus_station, ferry_terminal, fuel, charging_station | Transportation |
| Infrastructure | power=tower, substation, man_made=water_tower, communications_tower, tower, waterway=dam | Infrastructure |
| Hazards | landuse=industrial, man_made=environmental_hazard | Hazards |
| Food & Supply | amenity=restaurant, fast_food, cafe, bar, shop=supermarket, bakery, convenience_store, hardware, electronics, mobile_phone | Food & Supply |
| Medical | amenity=pharmacy, clinic, dentist, doctors, veterinary, childcare | Medical |
| Finance | amenity=bank, atm, post_office | Finance |
| Public Services | amenity=school, university, library, courthouse, townhall, prison, embassy, community_centre, place_of_worship | Public Services |
| Water & Sanitation | amenity=drinking_water, toilets, waste_basket, waste_disposal, recycling, fountain | Water & Sanitation |
| Recreation | leisure=park, stadium, sports_centre, pitch, amenity=cinema, theatre, tourism=museum, hotel, hostel, motel, viewpoint, attraction, camp_site | Recreation |
| Aviation | aeroway=helipad, airfield, aerodrome | Aviation |
| Factor | Google Maps | OSM / Overpass | Winner |
| Cost | $17 per 1,000 API calls | Free, no key, no limits | OSM |
| ToS on bulk POI extraction | Prohibited — can't scrape or export | Designed for bulk download | OSM |
| Owning derived data | Can't pipe into your own map | ODbL license — you own derived CoT | OSM |
| Custom categories | Their fixed categories only | Any tag the community has mapped | OSM |
| Data freshness | Google decides when to update | Community edits, visible changesets | OSM |
| Surveillance data | Not available | surveillance:type=ALPR (1,795 cameras) | OSM |
| Military data | Not available | military=* tags (98 installations) | OSM |
| Privacy/sovereignty | Google owns the data | You own your copy of the data | OSM |
