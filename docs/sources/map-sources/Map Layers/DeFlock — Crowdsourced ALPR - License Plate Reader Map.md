---
id: "notion-3bed0cf70a7e8197a3e2d577b4e392f2"
title: "DeFlock — Crowdsourced ALPR / License Plate Reader Map"
aliases: ["DeFlock — Crowdsourced ALPR / License Plate Reader Map"]
type: "reference"
status: "active"
created: "2026-10-16"
updated: "2026-10-16"
owner: "zeus"
tags: ["type/reference", "domain/so", "collection/so", "kind/record"]
collection: "so"
kind: "record"
notion_database: "Map Layers"
notion_format: ["MVT"]
notion_attribution: "DeFlock / OpenStreetMap contributors"
notion_type: "Overlay"
notion_zoom_range: "0–19"
notion_clients: ["ATAK", "CloudTAK", "QGIS", "CesiumJS", "Browser"]
notion_status: "🟢 Live"
notion_projection: "EPSG:3857"
notion_access: "Free"
notion_endpoint_url: "https://deflock.org"
notion_auth: "None"
notion_origin: "deflock.org — Open Source (FoggedLens/deflock on GitHub, 830★). Crowdsourced ALPR locations via OSM/Overpass API. iOS + Android app for field reporting. Privacy surveillance awareness tool."
notion_id: "3bed0cf70a7e8197a3e2d577b4e392f2"
notion_url: "https://app.notion.com/p/DeFlock-Crowdsourced-ALPR-License-Plate-Reader-Map-3bed0cf70a7e8197a3e2d577b4e392f2"
notion_created: "2026-08-16"
notion_last_edited: "2026-08-16T02:28:00.000Z"
imported: "2026-10-16"
---

# DeFlock — Crowdsourced ALPR / License Plate Reader Map

# ⚡ Zeus Intelligence Notes — Surveillance Evolution
> The trajectory of surveillance is away from cameras toward invisible, distributed sensor fusion. DeFlock maps today's visible infrastructure — what's coming has no lens.
## The Sensor Evolution Chain
```plain text
LICENSE PLATES
      ↓
    VIDEO (CCTV, Flock, ANPR)
      ↓
    BLUETOOTH (device MAC scanning)
      ↓
    RF (WiFi probes, IMSI catchers, passive SDR)
      ↓
    DRONES (aerial surveillance + sensor fusion)
      ↓
    FUSION ← NEW SENSORS → NEW DATABASE
         (no single system — everything talks to everything)
```
## The Shift: No Camera — Pure RF Sensor
> The next generation of surveillance has no lens. RF sensors are smaller, cheaper, hidden, and passive. They leave no visible footprint and require no line-of-sight.
- RF sensors detect device signatures without a camera — no image, no privacy trigger, no obvious hardware
- Smaller than a smoke detector — can be embedded in street furniture, utility boxes, signage
- Completely passive — emits nothing, only listens — nearly undetectable
- No maintenance, no lens to clean, no light requirement — works day/night/fog
- Reads: WiFi probe requests, Bluetooth advertisements, IMSI (cell), tire pressure sensors (TPMS), key fobs, AirTags
## What Gets Tracked — PLATE · TIME · PLACE · ROUTE
```plain text
PLATE     → vehicle identity (ALPR)
TIME      → when you were there (timestamp)
PLACE     → where you were (GPS/cell sector)
ROUTE     → where you came from + where you went (correlated across nodes)

Together = pattern of life. Not just where you are — where you always are.
```
## Fusion: The Person Behind the Vehicle
> The vehicle is just the first node. Fusion correlates the vehicle to the person inside it using every sensor they carry on their body.
- PHONE — WiFi probe + Bluetooth MAC + IMSI + app location pings = device fingerprint tied to vehicle plate
- WATCH — Bluetooth LE advertisement (Apple Watch, Garmin, Fitbit) — unique device ID broadcast continuously
- EARBUDS — Bluetooth broadcast (AirPods, Galaxy Buds) — additional corroborating device signature
- TIRE SENSORS — TPMS (Tire Pressure Monitoring System) broadcasts a unique ID on 315/433MHz RF — passive readers already deployed at some intersections and border crossings
- Fusion: plate + phone MAC + watch BLE + TPMS ID = individual person, not just vehicle
- Cross-reference across nodes = full route reconstruction of person, not just car
## The Future: No Cameras — Just Sensors
> The end state is a world where surveillance infrastructure is invisible. No cameras. No lenses. Just a silent RF mesh that knows where everyone is, was, and will be.
- Passive RF mesh — no active emission, no visible hardware, no legal 'camera' to regulate
- Every device a person carries becomes a beacon — opt-out is physically removing the device
- New database paradigm — not 'we have footage of you' but 'we have your signature across 400 nodes'
- TPMS unique IDs mean even a rental car with a burner phone is traceable if the tires are stock
- The countermeasure is hardware removal, not covering a lens — fundamentally different threat model
## OneMind Relevance
- DeFlock today maps the visible layer — the ALPR cameras Zeus can see and document
- The OneMind fabric should model the invisible layer too — TPMS readers, BLE scanners, IMSI catchers on the map
- Fabric Feeds: fabric.intel.surveillance.alpr.* → extend to fabric.intel.surveillance.rf.*
- TAK overlay: not just cameras as CoT points — RF sensor nodes, TPMS readers, BLE mesh as separate intel layer
- Fusion engine on NATS: correlate plate + device + TPMS hits across time → pattern of life stream
---
---
---
