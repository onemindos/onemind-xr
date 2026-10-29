---
id: "notion-3c0d0cf70a7e81a4853afa9073d867bb"
title: "TAK CoT Feed Sources — OneMind OS"
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
notion_category: "TAK"
notion_id: "3c0d0cf70a7e81a4853afa9073d867bb"
notion_url: "https://app.notion.com/p/TAK-CoT-Feed-Sources-OneMind-OS-3c0d0cf70a7e81a4853afa9073d867bb"
notion_created: "2026-08-18"
notion_last_edited: "2026-09-14T03:38:00.000Z"
imported: "2026-10-16"
---

# TAK CoT Feed Sources — OneMind OS

# TAK CoT Feed Sources — OneMind OS
> Unified reference for all data sources identified across osiris, worldmonitor, worldwideview, and Crucix that can be ingested and published as Cursor on Target (CoT) XML to a TAK Server.
---
## What is CoT?
**Cursor on Target (CoT)** is a lightweight XML schema used by TAK (ATAK, WinTAK, TAK Server) to share situational awareness data across a network. Every CoT message has:
- A **type** (what kind of entity/event)
- A **UID** (unique identifier)
- A **time/start/stale** window
- A **point** with lat/lon/hae (height above ellipsoid) and CE/LE error circles
- Optional **detail** block (callsign, remarks, links, sensor data)
CoT types follow a dot-notation hierarchy: `a-f-A-M-F` = air / friendly / air / military / fixed-wing. Custom types use `b-` prefix for non-track events.
---
## Tier 1 — Real-Time Track Sources (Highest TAK Value)
These produce **moving or positional tracks** — the core of SA in TAK.
---
### 1. airplanes.live
**What it is:** Community-run ADS-B aggregation network fed by thousands of volunteer receivers worldwide. Entirely unfiltered — includes aircraft that Mode-S/FAA LADD rules hide from FlightAware/FlightRadar24.
**What you get:**
- `/v2/mil` — military aircraft only (ICAO hex ranges mapped to military operators)
- `/v2/ladd` — FAA LADD-blocked aircraft (private/sensitive operators who opted out of public tracking)
- `/v2/pia` — Privacy ICAO Address aircraft (rotating hex codes used by government/law enforcement)
- `/v2/squawk/7700` — all aircraft declaring emergency right now
- `/v2/point/{lat}/{lon}/{dist}` — all aircraft within radius of a coordinate
**CoT Type:** `a-u-A` (unknown air) or `a-f-A-M-F` (air military fixed-wing) based on ICAO hex lookup
**Auth:** None — completely free, no key
**Update Rate:** ~5–10 seconds
**Source:** osiris `src/app/api/flights/route.ts`
---
### 2. ADSB.lol
**What it is:** Independent ADS-B feeder network, runs parallel to airplanes.live. Used as a fallback/cross-reference. Also provides unfiltered military/LADD/PIA/emergency feeds.
**What you get:** Same feed categories as airplanes.live — mil, ladd, pia, squawk 7700, regional
**CoT Type:** `a-u-A` / `a-f-A-M-F`
**Auth:** None — free
**Update Rate:** ~5–10 seconds
**Source:** osiris `src/app/api/flights/route.ts`
---
### 3. OpenSky Network
**What it is:** Academic/research ADS-B network run out of Switzerland. Full global state vector snapshot — every tracked aircraft with position, velocity, callsign, altitude. The most complete single snapshot source.
**What you get:** Full state vectors — lat, lon, altitude, velocity, heading, callsign, ICAO hex, squawk
**CoT Type:** `a-u-A` default; enrich to friendly/hostile with ICAO hex database
**Auth:** Anonymous (limited rate) or OAuth2 (`OPENSKY_CLIENT_ID` / `OPENSKY_CLIENT_SECRET`)
**Update Rate:** ~10 seconds (anonymous), ~5 seconds (authenticated)
**Source:** osiris `src/app/api/flights/route.ts`, worldmonitor `server/worldmonitor/aviation/v1/`
---
### 4. aisstream.io
**What it is:** Real-time global AIS (Automatic Identification System) vessel tracking via WebSocket. AIS is the maritime equivalent of ADS-B — every commercial vessel over 300 tons is required to broadcast. aisstream.io aggregates terrestrial and satellite AIS receivers into one stream.
**What you get:**
- Position reports (lat, lon, speed, heading, COG)
- Static data (vessel name, MMSI, IMO, call sign, vessel type, destination, ETA)
- Configurable geographic bounding boxes — subscribe to chokepoints: Strait of Hormuz, Suez Canal, Strait of Malacca, Bosphorus, etc.
**CoT Type:** `a-u-S-X-M` (unknown surface maritime) or `a-f-S-X-M` (friendly surface maritime)
**Auth:** `AISSTREAM_API_KEY` (free tier available)
**Update Rate:** Real-time WebSocket stream
**Source:** osiris `src/app/api/maritime/route.ts`, Crucix `apis/sources/ships.mjs`, worldmonitor `scripts/ais-relay.cjs`
---
### 5. Wingbits
**What it is:** Commercial aircraft intelligence platform. Goes beyond raw ADS-B — enriches aircraft with owner, operator, aircraft type, and maintains a curated military/government flight database. Used by worldmonitor for enriched military flight tracking.
**What you get:**
- Live military and government aircraft positions
- Owner/operator attribution per ICAO hex
- Aircraft type, registration, photo
**CoT Type:** `a-f-A-M-F` / `a-f-A-M-H` (military helicopter) with enriched callsign/remarks
**Auth:** `WINGBITS_API_KEY`
**Update Rate:** ~10–30 seconds
**Source:** worldmonitor `server/worldmonitor/military/v1/`
---
## Tier 2 — Geospatial Event Sources (Point/Area Events)
These produce **stationary events** — alerts, incidents, detections with coordinates that map to CoT event types.
---
### 6. ACLED — Armed Conflict Location & Event Data
**What it is:** The gold standard academic conflict database. Covers battles, explosions, remote violence, riots, protests, and civilian targeting events globally. Updated within 48 hours. Maintained by a dedicated research team who code every event from open sources.
**What you get:**
- Event type (battle, explosion, protest, mob violence, etc.)
- Sub-event type (armed clash, shelling, IED, etc.)
- Exact coordinates
- Actor 1 / Actor 2 (who vs. who)
- Fatality count
- Event notes (what happened in plain text)
- Country, region, locality
**CoT Type:** `b-m-p-w` (battle/warning) for kinetic; `b-m-p-i` for IED; custom hostile event types
**Auth:** `ACLED_EMAIL` + `ACLED_PASSWORD` (free academic/non-profit registration)
**Update Rate:** ~daily batch, last 7 days queryable in real-time
**Source:** Crucix `apis/sources/acled.mjs`, worldmonitor `server/_shared/acled.ts`, worldwideview `.env.example`
---
### 7. GDELT — Global Database of Events, Language and Tone
**What it is:** Monitors print, broadcast, and online news in 100+ languages in real-time, extracts events, actors, locations, and tone. Updated every 15 minutes. One of the largest open datasets in the world — effectively a real-time global event stream derived from media monitoring.
**What you get:**
- Event type (CAMEO codes — military action, protest, arrest, attack, etc.)
- Geographic coordinates (country, city, lat/lon)
- Actors involved
- Tone/sentiment score
- Source URLs
**CoT Type:** `b-m-p` (military point) for kinetic events; `b-t-f` for protest/unrest
**Auth:** None — completely free
**Update Rate:** Every 15 minutes
**Source:** Crucix `apis/sources/gdelt.mjs`, worldmonitor `server/worldmonitor/unrest/v1/`
---
### 8. UCDP — Uppsala Conflict Data Program
**What it is:** Academic conflict dataset from Uppsala University (Sweden). Georeferenced event data on organized violence — state-based conflict, non-state conflict, one-sided violence. More curated than ACLED, slower update cadence but very precise geo data.
**What you get:**
- Event coordinates
- Conflict type
- Deaths (best/low/high estimate)
- Actors
- Country/region
**CoT Type:** `b-m-p-w` (battle warning/event)
**Auth:** `UCDP_ACCESS_TOKEN` (free registration)
**Update Rate:** Weekly batch
**Source:** worldmonitor `scripts/seed-ucdp-events.mjs`
---
### 9. NASA FIRMS — Fire Information for Resource Management System
**What it is:** NASA satellite fire detection. VIIRS (Suomi NPP) and MODIS sensors detect thermal anomalies from orbit. Coverage is global, latency is ~3 hours from satellite pass. This data is used militarily to detect strikes, vehicle fires, artillery impacts, and industrial explosions as well as wildfires.
**What you get:**
- Lat/lon of thermal anomaly
- Fire Radiative Power (FRP) — intensity in MW
- Brightness temperature
- Satellite pass time
- Confidence level (low/nominal/high)
**CoT Type:** `b-m-p-s-p-i` (point of interest — fire/thermal) or custom `t-x-c-f`
**Auth:** Free NASA earthdata account for CSV feeds; `FIRMS_MAP_KEY` for API
**Update Rate:** Every ~3 hours (per satellite pass)
**Source:** osiris `src/app/api/fires/route.ts`, Crucix `apis/sources/firms.mjs`, worldmonitor `scripts/seed-fire-detections.mjs`
---
### 10. USGS Earthquake Feed
**What it is:** US Geological Survey real-time earthquake catalog. Every earthquake above a threshold magnitude is published within minutes of detection. GeoJSON format with full metadata.
**What you get:**
- Lat/lon/depth of hypocenter
- Magnitude and type (Mw, ML, etc.)
- Time of event
- Place name
- USGS event page URL
**CoT Type:** `b-n-g` (natural event/ground) or `b-m-p-s-p-i` point of interest
**Auth:** None — completely free
**Update Rate:** Near real-time (minutes after detection)
**Source:** osiris `src/app/api/earthquakes/route.ts`, worldwideview `src/app/api/earthquake/route.ts`, worldmonitor `scripts/seed-earthquakes.mjs`
---
### 11. Oref / Tzeva Adom — Israel Home Front Command
**What it is:** Israeli Home Front Command's official rocket/missile alert system. When a rocket is fired toward Israel, this API publishes the alert within seconds including the target zone. Also aggregates historical alert data.
**What you get:**
- Alert zone name (city/region in Hebrew and English)
- Alert type (rocket, UAV, earthquake, tsunami)
- Timestamp
- Historical alert log
**CoT Type:** `b-m-p-w-CBRN-W` (warning) or `b-f-t-r` (rocket attack)
**Auth:** None — public endpoint
**Update Rate:** Real-time (sub-second publication)
**Source:** worldmonitor `scripts/ais-relay.cjs`
---
### 12. GDACS — Global Disaster Alert and Coordination System
**What it is:** UN-run disaster alert system that aggregates data from USGS, NOAA NHC, ECHO, and other sources to produce unified global disaster alerts. Covers earthquakes, tropical cyclones, floods, volcanoes, droughts.
**What you get:**
- Event type, severity score (green/orange/red)
- Coordinates and affected area polygon
- Affected population estimates
- Event timeline
**CoT Type:** `b-n-g` (natural ground event) with severity in remarks
**Auth:** None — free RSS/API
**Update Rate:** Continuous as events occur
**Source:** osiris `src/app/api/weather/route.ts`, worldmonitor `scripts/seed-natural-events.mjs`
---
### 13. NGA Maritime Safety — Navigational Warnings
**What it is:** US National Geospatial-Intelligence Agency publishes active NAVTEX and broadcast navigational warnings. These include military exercise zones, mined areas, restricted operations areas, hazardous debris, and diplomatic restrictions on navigation.
**What you get:**
- Warning type and text
- Geographic coordinates / area description
- Issue authority (NAVAREA)
- Active/cancelled status
**CoT Type:** `b-m-p-w` (warning point) or area polygon overlay
**Auth:** None — public US government API
**Update Rate:** As warnings are issued/cancelled
**Source:** worldmonitor `server/worldmonitor/maritime/v1/list-navigational-warnings.ts`
---
### 14. GPSJam
**What it is:** Community-aggregated GPS interference map. Collects reports from ADS-B receivers that detect GPS jamming or spoofing (abnormal position/altitude reports). Displays heat map of interference zones updated daily. Heavily used for monitoring conflict zones — the Levant, Ukraine, and Baltic regions consistently show active jamming.
**What you get:**
- Grid cells with interference probability score
- Date of detection
- Geographic coverage
**CoT Type:** `b-m-p-j` (jamming zone) — area point/polygon
**Auth:** None — free
**Update Rate:** Daily
**Source:** worldmonitor `scripts/fetch-gpsjam.mjs`
---
### 15. DeepState Map — Ukraine Frontline
**What it is:** Ukrainian open-source intelligence project that maintains the most detailed and up-to-date Ukraine frontline map. GeoJSON polygons of Russian-controlled, Ukrainian-controlled, and contested territory updated daily by analysts.
**What you get:**
- GeoJSON polygons for territorial control
- Timestamps per update
- Named features (cities, towns, strategic locations)
**CoT Type:** Shape/polygon CoT or imported as KML overlay (already KMZ-compatible)
**Auth:** None — public API
**Update Rate:** Daily
**Source:** osiris `src/app/api/frontlines/route.ts`, worldwideview (referenced)
---
### 16. AlienVault OTX — Open Threat Exchange
**What it is:** Crowd-sourced threat intelligence platform with millions of indicators of compromise (IOCs). Tor exit nodes, malicious IPs, C2 servers, botnet infrastructure. While not traditionally a TAK source, IP geolocating these indicators creates a cyber-physical threat overlay.
**What you get:**
- Malicious IP addresses with country, city, lat/lon (via ip-api)
- Threat tags and malware family
- Confidence score
**CoT Type:** `b-m-p-s-p-i` (point of interest — threat) in cyber/infrastructure layer
**Auth:** Free OTX account for full feed
**Update Rate:** Continuous
**Source:** osiris `src/app/api/osint/threats/route.ts`, worldmonitor `server/worldmonitor/cyber/v1/`
---
## Tier 3 — Contextual/Enrichment Sources
These don't produce CoT directly but enrich other feeds or provide context layers.
---
### 17. CelesTrak — Satellite TLEs
**What it is:** Gold standard source for Two-Line Element orbital data for every tracked satellite. Used to compute real-time satellite positions and ground tracks. Military satellites, Starlink, GPS constellation, debris.
**CoT Use:** Satellite position tracks (`a-u-S-S` — unknown space) or ground track overlays
**Auth:** None
**Source:** osiris `src/app/api/satellites/route.ts`, Crucix `apis/sources/space.mjs`
---
### 18. Safecast + EPA RadNet — Radiation Monitoring
**What it is:** Safecast is a citizen science network with 150M+ radiation readings worldwide, heavily concentrated near nuclear sites (Zaporizhzhia, Chernobyl, Fukushima, Dimona, Yongbyon, Bushehr). EPA RadNet is the US government fixed-station gamma radiation network.
**CoT Use:** Sensor point (`b-m-p-s-p-loc`) with radiation reading in detail/remarks
**Auth:** None
**Source:** Crucix `apis/sources/safecast.mjs`, worldmonitor `scripts/seed-radiation-watch.mjs`
---
### 19. ReliefWeb (UN OCHA) + UNHCR
**What it is:** UN Office for the Coordination of Humanitarian Affairs publishes active disaster and crisis reports with country/region coordinates. UNHCR publishes refugee population flows and displacement statistics by location.
**CoT Use:** Humanitarian crisis points with population/severity context
**Auth:** ReliefWeb requires free appname registration; UNHCR is open
**Source:** Crucix `apis/sources/reliefweb.mjs`, worldmonitor `server/worldmonitor/displacement/v1/`
---
### 20. NOAA/NWS Severe Weather Alerts
**What it is:** US National Weather Service active alerts — tornadoes, hurricanes, flash floods, blizzards, extreme heat. Polygon-based zones with severity and instructions.
**CoT Use:** `b-w-A-N-a` (weather alert) or KML polygon import
**Auth:** None — completely free US government API
**Source:** osiris `src/app/api/weather/route.ts`, Crucix `apis/sources/noaa.mjs`, worldmonitor `scripts/seed-weather-alerts.mjs`
---
## CoT Type Quick Reference
| CoT Type | Meaning | Use Case |
| `a-u-A` | Unknown Air | Unidentified aircraft |
| `a-f-A-M-F` | Friendly Air Military Fixed-wing | Known military fixed-wing |
| `a-f-A-M-H` | Friendly Air Military Helicopter | Known military rotary |
| `a-h-A` | Hostile Air | Adversary aircraft |
| `a-u-S-X-M` | Unknown Surface Maritime | Unidentified vessel |
| `a-f-S-X-M` | Friendly Surface Maritime | Known friendly vessel |
| `b-m-p-w` | Battle/Warning Point | Conflict event, incoming fire |
| `b-m-p-i` | IED/Explosive Point | IED or explosive device |
| `b-n-g` | Natural Ground Event | Earthquake, volcano |
| `b-t-f` | Threat Fire | Fire/thermal anomaly |
| `b-w-A-N-a` | Weather Alert | Severe weather polygon |
| `b-m-p-s-p-i` | Sensor Point of Interest | Radiation, cyber threat, sensor |
---
## Recommended Implementation Order for OMOS
### Phase 1 — Wire These Now (free, no auth)
1. **airplanes.live** `/v2/mil` + `/v2/ladd` — military/sensitive air tracks
2. **ADSB.lol** `/v2/mil` — redundant military air feed
3. **USGS Earthquakes** — seismic events globally
4. **NASA FIRMS** — thermal/fire anomalies
5. **GDACS** — disaster alerts
6. **Oref/Tzeva Adom** — Israel rocket alerts
7. **DeepState Map** — Ukraine frontline polygons
8. **GPSJam** — daily GPS interference zones
9. **NGA NAVTEX** — maritime navigational warnings
10. **NOAA/NWS** — US severe weather alerts
### Phase 2 — Register and Wire (free, need account)
11. **aisstream.io** — live vessel tracking WebSocket
12. **OpenSky Network** — full aircraft state vectors
13. **ACLED** — conflict event database
14. **NASA FIRMS API key** — higher rate limit fire data
15. **AlienVault OTX** — threat intelligence
### Phase 3 — Paid/Priority Keys
16. **Wingbits** — enriched military flight attribution
17. **UCDP** — academic conflict events
18. **Cloudflare Radar** — internet outages / gov shutdowns
---
## Architecture Notes for TAK Integration
### Option A — NATS → CoT Bridge (recommended for OMOS)
Each source becomes a NATS publisher. A bridge service subscribes, converts to CoT XML, and publishes to TAK Server via TCP (port 8087) or UDP multicast.
```plain text
Source Fetcher → NATS subject (e.g. omos.feeds.aircraft.mil) → CoT Bridge → TAK Server
```
### Option B — TAK Server Data Package
For static/slow-changing data (ACLED historical, frontlines, radiation sites): wrap as KMZ/KML in a `.dp` Data Package and push to all connected clients via TAK Server's data sync.
### Option C — Network Link KML
For large datasets (Flock cameras, ACLED, FIRMS): serve a Network Link KML endpoint that TAK polls by viewport. Only fetches what's visible at current zoom.
### CoT XML Skeleton
```xml
<event version="2.0"
       uid="omos-firms-{lat}-{lon}-{time}"
       type="b-t-f"
       time="2026-07-24T00:00:00Z"
       start="2026-07-24T00:00:00Z"
       stale="2026-07-24T06:00:00Z"
       how="m-g">
  <point lat="34.123" lon="-118.456" hae="0" ce="100" le="100"/>
  <detail>
    <remarks>NASA FIRMS — VIIRS detection. FRP: 45.2 MW. Confidence: nominal.</remarks>
    <contact callsign="FIRMS-THERMAL"/>
    <color argb="-65536"/>
  </detail>
</event>
```
---
*Generated: 2026-07-24 | Sources: osiris, worldmonitor, worldwideview, Crucix*
