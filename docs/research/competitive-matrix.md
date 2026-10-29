# Competitive matrix (M0 research lane: competitors)

Date: 2026-10-29. Scope: worldmonitor, osiris, worldwideview, gods-eye-view, Argos
Atlas, ARTAK (EolianVR), CloudTAK, TAKX, WinTAK, compared against every row of
`docs/FEATURES.md`. Target: OneMind matches or beats every competitor on every row.

Method: primary sources only (repo files through `gh api`, official docs, product
pages, raw page HTML with one polite request per page). Repo files were read at
their default branch HEAD on 2026-10-29. Licences were read from each repo's LICENSE
or README, not inferred. No competitor code was copied. worldmonitor (AGPL-3.0),
worldwideview (Elastic 2.0) and CloudTAK (AGPL-3.0) were read for design only.

Legend for the matrix: **Y** has it, **P** partial, **—** not present in any source
read, **?** unverified (closed product, no public evidence). OneMind's column is the
*target* feature ID: nothing in OneMind is built yet (`docs/STATUS.md`: M1–M7 not
started).

## Sources

Repository metadata (`gh api repos/<repo>`, 2026-10-29):

| Repo | Licence | Stars | Last push | Language |
|---|---|---|---|---|
| `koala73/worldmonitor` | AGPL-3.0 | 89,736 | 2026-10-29 | TypeScript |
| `silvertakana/worldwideview` | Elastic License 2.0 (LICENSE file; API reports NOASSERTION) | 1,928 | 2026-10-29 | TypeScript |
| `bilawalsidhu/gods-eye-view` | MIT (LICENSE file, "Copyright (c) 2026 Bilawal Sidhu"; API reports NOASSERTION) | 60,998 | 2026-10-29 | JavaScript |
| `simplifaisoul/osiris` | MIT | 11,393 | 2026-10-28 | TypeScript |
| `dfpc-coe/CloudTAK` | **AGPL-3.0** | 136 | 2026-10-28 (release v13.116.0, 2026-10-27) | TypeScript |

Files and pages read:

- worldmonitor: `README.md`, `docs/map-engine.mdx`, `docs/data-sources.mdx`,
  `docs/ai-intelligence.mdx`, `docs/features.mdx`, `docs/architecture.mdx`,
  `src/config/map-layer-definitions.ts`, repo tree (230 paths under `scripts/seed-*`).
  https://github.com/koala73/worldmonitor
- worldwideview: `README.md`, `docs/ARCHITECTURE.md`, repo tree (`src/plugins/`,
  `src/core/plugins/`); `silvertakana/worldwideview-plugins` `packages/` listing;
  `silvertakana/wwv-data-engine` root listing. https://github.com/silvertakana/worldwideview
- gods-eye-view: `README.md`, `LICENSE`, `docs/PERFORMANCE.md`, repo tree.
  https://github.com/bilawalsidhu/gods-eye-view
- osiris: `README.md`, repo tree. https://github.com/simplifaisoul/osiris
- CloudTAK: `README.md`, `docs/TASK.md`, `CHANGELOG.md`, `app/package.json`,
  `app/src/stores/map.ts`, repo tree (`api/stateless`, `api/stateful`,
  `app/src/components/CloudTAK/Menu`); code search for `terrain`, `globe`,
  `mediamtx`, `openai`, `bedrock`; https://docs.cloudtak.io/ ,
  https://docs.cloudtak.io/user/ , https://docs.cloudtak.io/integrations/
- ARTAK: https://www.eolianvr.com/artak
- Argos Atlas: raw HTML of https://argosatlas.com/map/ and
  https://argosatlas.com/fuentes-y-metodologia/ (one `curl` each)
- TAKX: `TAK-Product-Center/atak-civ` `takkernel/README.md` (build target
  `-PkernelBuildTarget=java` "to build TAK Kernel for TAKX"); code search hit
  `takkernel/shared/src/jre/java/gov/takx/api/persist/IPersistable.java`; CTI
  vendor sheet https://ctistorage.blob.core.usgovcloudapi.net/ctifileshare/CTI%20Slick%20Sheets/TAKX%20-%202025.pdf
  (text extracted locally); `docs/research/2026-10-29-findings.md` §1 (TAK Kernel
  renders 3D Tiles through vendored cesium-native).
- WinTAK: https://github.com/Hellikandra/WinTAK-plugintemplate (README quoting the
  WinTAK SDK doc `md_doxygen_extending_wintak__extending_win_tak.html`,
  `TemplateButton.cs`); https://www.civtak.org/2020/09/23/wintak-is-publicly-available/ ;
  COTAK forum post on WinTAK 5.5 (https://forum.cotak.gov/post/122, seen only as a
  search-result excerpt; the host did not resolve from this machine).
- Blocked: https://tak.gov/products/wintak-civ and `/products/takx` return a login
  wall (USG consent banner), so official TAKX/WinTAK feature lists were not readable.

Independent verification (2026-10-29): 22 load-bearing claims were re-fetched from the
cited primary sources (`gh api` file reads, `gh api search/code`, one `curl` per web
page). 19 confirmed as written; 2 corrected in place (ARTAK macOS listing, G1 macOS
CI); 1 marked **unverified** (worldmonitor signed builds). No licence-rule or
cluster-safety violation was found in this file or `docs/FEATURES.md`.

## Verified facts

### worldmonitor (AGPL-3.0, design only)

- **Surfaces:** web app plus five variants (tech, finance, commodity, happy, energy),
  Tauri 2 desktop for Windows/macOS/Linux, terminal dashboard (`npx worldmonitor`),
  CLI, Python/Ruby/Go SDKs, MCP server at `https://worldmonitor.app/mcp`, REST API
  with OpenAPI (README "Ways to Use It", "Programmatic Access").
- **Data sources and layers:** domains Geopolitics & Conflict (ACLED, UCDP, GDELT,
  UNHCR/OCHA, Polymarket, OREF rocket alerts), Demographics, Military & Strategic
  (bases, military flights via adsb.lol and Wingbits, CelesTrak satellites,
  gpsjam.org), News & OSINT (tiered feeds, Telegram channels, X accounts, live TV and
  YouTube webcams), Finance, Energy, Infrastructure & Cyber (cables, pipelines,
  ports, Cloudflare Radar outages, abuse.ch IOCs), Environment & Disasters (USGS,
  GDACS, NASA FIRMS/EONET, Open-Meteo), Aviation (FAA delays, NOTAMs), China, Tech
  (`docs/data-sources.mdx` "Coverage at a Glance"). Map layer keys in
  `map-layer-definitions.ts`: conflicts, ucdpEvents, ciiChoropleth, natural, weather,
  canadaRoads, canadaAlerts, flights, aircraftEmergencies, ais, waterways,
  tradeRoutes, cyberThreats, hotspots, no2Activity; the shared catalog adds bases,
  nuclear, cables, pipelines, fires, protests, datacenters, displacement, climate
  anomalies, GNSS jamming, day/night terminator, exchanges, central banks, trade
  routes, airport delays, sanctions (`docs/map-engine.mdx`).
- **Ingestion:** Railway cron "seed" scripts write a canonical Redis key plus a
  bootstrap key per dataset; failed seeds never overwrite the last good value
  (`docs/architecture.mdx` l.271, l.312). An AIS relay holds a WebSocket to
  AISStream with three-watermark backpressure, caps tracked vessels at 20,000 and
  aggregates into 2°×2° density cells (max 5,000) for overview (l.370–386).
- **Caching:** in-memory → Redis (Upstash) → upstream, stale-on-error, negative
  caching, in-flight promise coalescing, per-feed circuit breakers, CDN `s-maxage`;
  `/api/bootstrap` hydrates 38 datasets in two requests (l.16–27, l.226–253).
- **Freshness:** a tracker classifies ~40 sources fresh (<15 min) / stale (2 h) /
  very_stale (6 h) / no_data / error and surfaces "intelligence gaps" to the user
  (`docs/data-sources.mdx` "Data Freshness & Intelligence Gaps").
- **Rendering:** globe.gl + Three.js globe with HTML markers ("28+ data categories"),
  and deck.gl + MapLibre flat map with Supercluster clustering, progressive
  disclosure, label deconfliction; pixel-ratio presets; render loop pauses when
  hidden; time windows 1h–7d; URL state sharing; 8 regional presets; Cmd+K
  (`docs/map-engine.mdx`). Tiles: OpenFreeMap, CARTO, self-hosted PMTiles with
  fallback; service worker caches tiles offline.
- **AI:** summarization chain Ollama/LM Studio → OpenRouter → browser T5
  (Transformers.js); DistilBERT sentiment in-browser; World Brief, country briefs,
  focal-point detection, Country Instability Index, Welford baselines for anomaly
  detection, client-side headline RAG, breaking-news alerts
  (`docs/ai-intelligence.mdx`, `docs/architecture.mdx` l.79, l.95).
- **Cannot do (no evidence in any file read):** no TAK/CoT, no mesh, no tasking or
  asset commands, no drones/robots, no 3D Tiles or terrain twin (globe is a textured
  sphere), no XR, no multi-user session, no on-prem bus; data is snapshot/poll
  based, not entity-fused.

### worldwideview (Elastic 2.0, design only)

- **Architecture:** Next.js 16 + React 19, CesiumJS/Resium, Zustand, a custom typed
  `DataBus`, PostgreSQL/Prisma, Docker (README "Core Technologies").
- **Data sources and layers:** built-in plugins earthquakes, geojson, iss, weather
  (`src/plugins/`); first-party plugins in `worldwideview-plugins/packages`:
  air-defense, airports, aviation, borders, camera, civil-unrest, conflict-events,
  conflict-zones, cyber-attacks, daynight, earthquakes, embassies, gps-jamming,
  international-sanctions, iranwarlive, lighthouses, maritime, military-aviation,
  military-bases, mineral-mines, nuclear, osm-search, satellite, seaports,
  spaceports, surveillance-satellites, undersea-cables, volcanoes, wildfire.
- **Ingestion:** `wwv-data-engine` is a content-agnostic runner of seeder scripts;
  each seeder exposes both WebSocket `/stream` and REST `/api/[plugin-id]`; each
  plugin declares its own `streamUrl`; complex compute lives in the plugin's own
  backend ("99/1 rule") (`docs/ARCHITECTURE.md` "Data Engine & Seeders").
- **Rendering at scale:** Cesium primitive collections (not the Entity API), "10k+
  live entities still hold 60 fps on a moderate laptop", 500-entity chunks across
  microtasks, manual horizon culling by dot product, billboard→glTF LOD, stack /
  spiderifier for co-located entities (`docs/ARCHITECTURE.md` "Rendering Pipeline").
- **Plugins:** plugin SDK on npm, CLI scaffolding, marketplace install from the UI.
- **AI:** opt-in Agent Bus (HTTP+SSE) lets an external MCP server fly the globe,
  toggle layers and select entities; built-in MCP endpoint `/api/mcp` with API-key
  auth; plugin tools appear in `tools/list` only after the browser tab loads them
  (README "MCP").
- **Cannot do:** no TAK/CoT, no tasking/commands, no entity fusion across plugins,
  no offline-first story, no XR, no video pipeline (camera plugin only).

### gods-eye-view (MIT; reuse allowed with notice)

- **Layers (README "What's on the Globe", 20 layers, 18 keyless):** map stack (Esri,
  OSM, Google Photorealistic 3D via ion or direct key), live flights (OpenSky +
  adsb.lol, "11,000+"), military flights, GNSS integrity, fire perimeters (WFIGS/NIFC),
  vessels (AISStream), satellites (838-object catalog, Starlink shell), earthquakes,
  air quality (ECCC, Canada), traffic (simulated on OSM roads, TomTom flow), CCTV
  mesh (~4,000 public cameras projected into 3D with viewsheds), mapped ALPR camera
  locations (OSM tags), street level (Mapillary, Google, KartaView, Panoramax),
  radio (750 stations), transit (GTFS-RT style, many cities), bikeshare, directions
  (A→B routing, fly along), active fires (FIRMS), space missions (30-day launches),
  mapped installations, wind (GFS/ECMWF), observed weather (radar, GOES clouds,
  lightning), cyclones (NHC/CPHC cones). Bundled: datacenters (4,351), dams (704),
  submarine cables (712).
- **Ingestion and caching:** `server/providers` per source (findings §3); server-side
  key brokering with SSRF protection; OpenSky credit governor, TomTom daily tile
  budget, disk-cached TLEs (README "Under the Hood").
- **Rendering:** CesiumJS + Vite, vanilla JS; screen-projected true headings;
  render one interval behind real time with interpolation and dead reckoning; SGP4
  satellites; entity heights aligned to Google 3D tiles; glyph→3D model swap per
  aircraft class; GLSL looks (CRT, NVG, FLIR, Noir, Snow); screen-space detection
  overlay; cockpit and click-to-track camera; scene director; share links carrying
  camera, layers, style and a tracked target.
- **Scale evidence:** one Apple M5 / Chrome 150 capture: median cold start 1.86 s
  settle; "Combined static" scene rendered **11,575 objects at 57.6/60 FPS** using
  872.2 MiB JS heap, with live aircraft and traffic rows empty ("a limited stress
  case") (`docs/PERFORMANCE.md` l.80–97).
- **AI:** voice agent on OpenAI Realtime or Gemini Live with "Thirty tools": camera
  direction, annotation of real boundaries, measurement, analyst queries that report
  stale feeds, layer control; viewport screenshot grounding; AI HUD five-word scene
  summary; MCP server for Claude/Codex (README "Talk to It").
- **Cannot do:** no TAK/CoT, no tasking, no drones, no multi-user, no offline
  stack (keys and providers are online; Docker caches only map/satellite data), no
  XR; traffic is simulated, camera poses are "coarse estimates" (README).

### osiris (MIT; reuse allowed with notice)

- **Layers (README "Key Capabilities"):** aviation (OpenSky), maritime (39 ports, 10
  chokepoints, static), CCTV 17,000+ (TfL, WSDOT, Caltrans, ODOT, MDOT, HK, Taiwan,
  NZTA, Rijkswaterstaat, bekijkhet.nu catalogue, TxDOT), seismic (USGS M2.5+), fires
  (FIRMS), 23 live news streams, weather (EONET), space (NOAA SWPC, N2YO), cyber (NVD
  CVEs), 15 hand-kept conflict zones with headlines, GDELT events, crypto wallet
  tracing (blockstream, Blockscout), OFAC SDN search (OpenSanctions), geoparsed
  Telegram public-channel posts. "16 toggleable data layers".
- **Ingestion and caching:** one Next.js API route per source; server caches with
  stale data kept during outages; polling relaxed to 15–30 min for stable data;
  viewport-aware, on-demand layer loading (README "Features", "Texas CCTV").
- **Rendering:** MapLibre GL (WebGL), "60fps ... with thousands of concurrent
  entities"; 3D terrain shown in screenshots.
- **AI:** "OI Assist" talks to the map (fly, toggle layers, find live entities,
  mark search area); "OI Forecast" multi-agent prediction engine (method after
  MiroFish, no MiroFish code); BYO key for nine LLM providers; REST `/api/oi` with
  SSE and an MCP server `/api/mcp`.
- **Other:** "Save an Area" finds every camera inside a drawn region and exports
  GeoJSON; RECON toolkit (TCP port scan, DNS, WHOIS, TLS, IP intel, CVE lookup).
- **Cannot do:** no TAK/CoT, no tasking, no entity fusion, no 3D Tiles twin, no
  offline mode, no multi-user, no XR.

### Argos Atlas (closed, web)

- Spanish-first web map: "+200.000 cámaras en directo"; an HTML comment records
  "212,612 distinct live cameras" counted 2026-09-27 (raw `/map/` HTML).
- Also ADS-B flights, AIS ships, airports (OurAirports), ports, power plants,
  critical infrastructure, OSM (ODbL) base, Natural Earth context; footer lists
  "ADS-B · AIS · CCTV · OSINT · GEOINT · MERCADOS" (`/fuentes-y-metodologia/`).
- Engines: the bundle references MapLibre (`maplibre-*.js`) and comments mention
  CesiumJS and "Preparing the 3D globe" (raw HTML).
- Imagery policy: shows only the latest frame in low resolution, no face or plate
  recognition, no history except a rolling ≤24 h window for paid plans; ARGOS PRO
  subscription, sign-up wall, affiliate programme.
- **Cannot do:** no TAK, no tasking, no AI features visible, no offline, no API
  visible.

### ARTAK (EolianVR, commercial)

- "device agnostic, running across AR, VR, Tablet, Phone, Laptop, and PC"; Meta
  Quest 3 bundled with starters; iOS and Android.
- Map Maker builds 3D maps from UAS images ("~5.5 sq KM"), also without a network
  (Block 2); "World in a Box" 3D tiled base layer of the planet; satellite, road,
  topo, night and Cesium basemaps; interiors, subterranean and underwater maps.
- ATAK integration for unmanned asset position/status and force tracking (goTenna,
  Starlink case study); live video and position from air, ground and underwater
  unmanned systems (eight feeds and six assets in one session); synced headsets
  ("every headset in the room holds the same picture at the same moment"); voice;
  MIL-STD-2525E symbology; graphics through terrain; Team Server on your PC or edge
  server; "Full operational capability in disconnected, austere environments".
- AI: only "predictive analytics" and "data analysis" are claimed; no detail.
- Includes a weapon inventory of 200+ U.S. systems with range/engagement data.
- Pricing: Solo $3,400/yr, Duo $6,800/yr, Team $14,500 year 1 then $13,600/yr.
- **Cannot do / not stated:** no open data plane or plugin SDK stated, no intel
  dashboard breadth, closed source. (Verifier correction 2026-10-29: the product page
  *does* list "DESKTOP PC / Mac" among its form factors, next to "MOBILE iOS /
  Android"; whether that is a native macOS build is not stated. Source:
  https://www.eolianvr.com/artak raw HTML.)

### CloudTAK (AGPL-3.0, design only)

- "Full Featured in-browser TAK Client" and ETL into TAK Server (README). Vue +
  `@tak-ps/maplibre-gl` fork, `@tak-ps/node-cot`, `@tak-ps/node-p12`
  (`app/package.json`); globe projection is the default view and 3D terrain can be
  toggled (`CHANGELOG.md` l.4209, l.4632); offline TileJSON cache (l.575,
  `app/src/utils/offline-tiles.ts`); a map worker ("atlas") off the main thread
  (`app/src/stores/map.ts`).
- **TAK features (docs.cloudtak.io/user):** contacts, chat, channels, Data Sync
  (Marti missions: subscribe, layers, files, chats, users, change log, CSV logs),
  data packages (cleared after two months unless `#permanent`), video streams and
  leases (MediaMTX integration, WHEP reader: `app/src/components/util/video/WhepReader.ts`),
  routes snapped to roads and trails with ETA, breadcrumbs up to 24 h, lock-on,
  drawing (coordinate input, range & bearing, range rings, point, line, polygon,
  rectangle, circle, sector, lasso, GeoJSON import, buffer), DD/DM/DMS/MGRS/UTM,
  Query Mode (address, elevation, NWS weather, sun phase, magnetic declination).
- **Basemaps/overlays:** ZXY, TileJSON, uploads, ArcGIS FeatureServer/MapServer/
  ImageServer (`app/src/components/CloudTAK/Menu/Basemaps/TypeSelector*.vue`);
  PMTiles task (`tasks/pmtiles`).
- **Ingestion:** ETL "layers" run as AWS Lambda containers (128 MB default) posting
  GeoJSON/CoT to the ETL API (`docs/TASK.md`); ~41 integrations including ADS-B
  Exchange, DJI FlightHub 2, Skydio, BRINC, Dronetag Remote ID, CAD systems
  (CentralSquare, JeffCom 911, WildWeb), AVL (Geotab), satellite messengers
  (SPOT, inReach), Esri ArcGIS (with push back), Waze, USGS gauges, Verkada and
  Shotover video, and a CoT webhook (docs.cloudtak.io/integrations).
- **Other:** incident boards, forms and assignments (`core-event*`, `core-form*`
  routes), mission templates, geofences, passkeys, LDAP, SCIM, plugin API (menus,
  routes, overlays, status-bar widgets), retention tasks; Postgres + PostGIS.
- **AI:** none found (no hits for `openai` or `bedrock`; docs do not mention AI).
- **Cannot do:** no 3D Tiles twin or models (MapLibre terrain only), no XR, no
  drones command, no entity fusion, full ETL needs AWS ("a full AWS deployment is
  still required for the complete optional ETL infrastructure", README).

### TAKX (TAK Product Center / CTI)

- Java desktop TAK client built on TAK Kernel: `takkernel/README.md` "to build TAK
  Kernel for TAKX ... `-PkernelBuildTarget=java`"; Java package `gov.takx` exists in
  atak-civ. TAK Kernel renders Cesium 3D Tiles via vendored cesium-native (findings §1).
- CTI vendor sheet (marketing): "cross-platform", "No Internet Connection Required",
  "Zero licensing fees", "DOZENS OF PLUGINS" naming TAKX-RF (wireless protocol
  analytics and geolocation with an open API), Network Survey (GSM/CDMA/UMTS/LTE/
  802.11 survey records), Mounted Support (touch UI with physical buttons). CTI
  "supports TAK.gov as the core developer for TAKX".
- AI, layers and OS list: not public (tak.gov login wall).

### WinTAK (TAK Product Center)

- Windows-only .NET client; plugins are WPF user-control libraries on **.NET
  Framework 4.8.1** loaded through MEF, packaged as `.wpk`, using
  `WinTak.Framework.*` (template README quoting the SDK doc; `TemplateButton.cs`).
- Public civil release since 4.1.0.231 (civtak.org, 2020-09-23).
- WinTAK 5.5 (CIV build 5.5.0.155) released around 2025-08-27; post says WinTAK is
  in "long-term sustainment" with clamp-to-ground shapes, vehicle block models,
  software-render fallback (COTAK forum excerpt; post marked deleted; weak source).
- AI: none known.

## How it applies to OneMind

### Feature matrix (every `docs/FEATURES.md` row)

Columns: WM worldmonitor, OS osiris, WWV worldwideview, GEV gods-eye-view, AA Argos
Atlas, AR ARTAK, CT CloudTAK, TX TAKX, WT WinTAK. "Bar to beat" is the strongest
competitor evidence OneMind's acceptance test must meet or exceed.

| ID | WM | OS | WWV | GEV | AA | AR | CT | TX | WT | Bar to beat |
|---|---|---|---|---|---|---|---|---|---|---|
| A0 pipeline standard | P | P | P | P | ? | ? | P | — | — | WM: stale-on-error, circuit breakers, coalescing; CT: per-layer ETL tasks |
| A1 feeds on grammar | — | — | — | — | — | — | — | — | — | none (internal) |
| A2 feed registry | Y | — | P | P | ? | ? | Y | — | — | WM catalog with tier, licence posture, method |
| A3 feed health | Y | P | — | Y | ? | ? | P | — | — | WM fresh/stale/very_stale tracker |
| A4 entity fusion | P | — | — | — | — | ? | — | P | P | WM multi-source corroboration (protests, disasters) |
| A5 layer snapshots | Y | P | Y | P | ? | ? | P | — | — | WM 38-dataset bootstrap in 2 calls |
| A6 AOI filter to TAK | — | — | — | — | — | ? | P | — | — | CT geofence connections |
| A7 mesh gateway | — | — | — | — | — | ? | — | Y | Y | TAK clients' native mesh |
| A8 fleet from MAVLink | — | — | — | — | — | P | P | P | P | CT DJI/Skydio/BRINC ETL |
| A9 camera registry | Y | Y | P | Y | Y | P | P | — | — | AA 212,612 cameras; GEV 3D projection |
| A10 history/replay | P | — | — | P | P | ? | P | ? | ? | CT 24 h breadcrumbs; GEV 24 h trace backfill |
| B1 native Mac app | Y | — | — | — | — | P | — | ? | — | WM Tauri macOS arm64; AR lists "PC / Mac" (no build detail) |
| B2 globe + offline cache | P | P | P | P | P | Y | P | Y | Y | AR offline 3D; CT offline tiles |
| B3 bus client | — | — | P | — | — | ? | P | Y | Y | TAK clients (Commo) |
| B4 layer system | Y | Y | Y | Y | P | ? | Y | Y | Y | WWV plugin layers; WM single catalog |
| B5 batched rendering, LOD | Y | Y | Y | Y | ? | ? | P | Y | Y | WWV 10k+ at 60 fps; GEV 11,575 objects at 57.6 fps |
| B6 entity inspector | P | P | P | Y | P | ? | Y | Y | Y | GEV telemetry card; CT CoT view |
| B7 2525 symbology | — | — | — | — | — | Y | Y | Y | Y | AR 2525E |
| B8 timeline | P | — | — | P | — | P | P | ? | ? | GEV weather timeline, launch scrub |
| B9 measurement | — | — | — | P | — | ? | Y | Y | Y | CT range & bearing, rings |
| B10 search, MGRS, geo: | P | — | P | Y | — | Y | Y | Y | Y | GEV `geo:` URI; CT MGRS/UTM |
| C1 TAK entities | — | — | — | — | — | Y | Y | Y | Y | TAK clients |
| C2 send CoT, chat | — | — | — | — | — | Y | Y | Y | Y | CT chat, features, routes |
| C3 TAK contact | — | — | — | — | — | ? | Y | Y | Y | CT contacts |
| C4 Commo in client | — | — | — | — | — | ? | — | Y | Y | TX/WT native |
| C5 data packages | — | — | — | P | — | ? | Y | Y | Y | CT packages |
| C6 voice | — | — | — | P | — | Y | — | ? | ? | AR integrated voice |
| D1 AOIs drive feeds | — | P | — | P | — | ? | P | P | P | OS Save an Area; CT geofence |
| D2 missions | — | — | — | — | — | Y | Y | Y | Y | CT Data Sync |
| D3 asset command | — | — | — | — | — | P | — | ? | ? | none commands a drone with audit |
| D4 Hermes panel | P | Y | P | Y | — | — | — | — | — | GEV 30-tool voice agent; OS OI Assist |
| D5 AI under policy | — | — | — | P | — | — | — | — | — | GEV "only confirms actions that succeeded" |
| D6 alerts | Y | — | — | — | — | ? | P | ? | ? | WM breaking-news pipeline |
| E1 homestead 3D Tiles | — | — | — | — | — | Y | — | P | — | AR Map Maker ~5.5 km² |
| E2 drone video on map | — | — | — | — | — | Y | P | ? | ? | AR 8 feeds + 6 assets |
| E3 rapid re-map | — | — | — | — | — | Y | — | — | — | AR Map Maker offline |
| E4 view cones, drape | — | — | — | Y | — | ? | — | ? | ? | GEV CCTV projected + viewshed |
| E5 photoreal basemap | — | — | Y | Y | — | Y | — | P | — | GEV/WWV Google 3D |
| F1 packaged Mac app | Y | — | — | P | — | ? | — | ? | — | WM Tauri macOS downloads (README). **unverified** that shipped builds are signed: `.github/workflows/build-desktop.yml` signs only when Apple secrets are configured, otherwise "building unsigned" |
| F2 web streaming | — | — | — | — | — | Y | — | — | — | AR server streams to headsets |
| F3 web client | Y | Y | Y | Y | Y | — | Y | — | — | every web competitor |
| F4 Quest Pro | — | — | — | — | — | Y | — | — | — | AR Quest 3 |
| F5 Windows/Linux | Y | P | P | P | P | Y | P | Y | Y | WM Tauri 3 OS |
| F6 multi-user | — | — | — | — | — | Y | P | ? | ? | AR synced headsets |
| F7 hosted streaming | — | — | — | — | — | Y | — | — | — | AR Team Server |
| H1 robots as entities | — | — | — | — | — | Y | P | P | P | AR air/ground/underwater |
| H2 MAVLink + SITL | — | — | — | — | — | ? | — | ? | ? | none public |
| H3 ROS 2 | — | — | — | — | — | ? | — | — | — | none |
| H4 tasking | — | — | — | — | — | ? | P | ? | ? | CT assignments |
| H5 swarm | — | — | — | — | — | ? | — | — | — | none |
| H6 safety | — | — | — | — | — | ? | — | — | — | none |
| H7 sim/rehearsal | — | — | — | P | — | Y | — | — | — | AR planning/rehearsal |
| H8 onboard detection | — | — | — | P | — | ? | — | — | — | GEV overlay is cosmetic |
| H9 robotics CoT profile | — | — | — | — | — | ? | P | ? | ? | CT drone ETLs emit CoT |
| I1 agents as contacts | — | — | — | — | — | — | — | — | — | none |
| I2 agent tools over bus | P | P | P | Y | — | — | — | — | — | WM/OS/WWV/GEV MCP servers |
| I3 watch jobs | P | — | — | — | — | — | — | — | — | WM custom monitors |
| I4 policy and audit | — | — | — | — | — | — | — | — | — | none |
| I5 multi-agent | — | P | — | — | — | — | — | — | — | OS OI Forecast actors |
| J1 L0 web | Y | Y | Y | Y | Y | — | Y | — | — | WM mobile gestures, PWA |
| J2 role modes | P | — | P | — | P | ? | Y | ? | ? | CT LDAP/SCIM, permissions |
| J3 adaptive quality | Y | P | P | P | ? | ? | — | ? | P | WM pixel-ratio presets; WT software fallback |
| J4 accessibility | P | — | — | — | — | ? | — | ? | ? | WM RTL; none audited |
| J5 localization | Y | — | — | — | Y | ? | — | ? | ? | WM multilingual + RTL; AA ES/EN |
| K1 video ingest | — | — | — | P | — | Y | Y | ? | ? | CT MediaMTX + WHEP |
| K2 stream registry | P | Y | P | Y | Y | Y | Y | ? | ? | OS/AA camera catalogs |
| K3 video in TAK | — | — | — | — | — | Y | Y | Y | Y | CT leases |
| K4 video in client | Y | Y | — | Y | Y | Y | Y | Y | Y | AR 8 feeds in session |
| K5 KLV footprint | — | — | — | — | — | ? | — | ? | ? | ATAK family "Viewing KLV" (unverified for TX/WT) |
| K6 plugin model | P | — | Y | P | — | — | Y | Y | Y | WWV SDK + marketplace; WT MEF |
| G1 macOS CI | P | ? | — | ? | — | — | ? | — | — | none verified per PR. Verifier correction: WWV `ci.yml` runs on PRs but every job is `runs-on: ubuntu-latest` (no macOS); WM `build-desktop.yml` builds on `macos-15`/`macos-latest` only on tag push or manual dispatch |
| G2 perf budget | Y | — | P | Y | — | — | — | — | — | GEV PERFORMANCE.md baseline |
| G3 client telemetry | — | — | — | — | — | — | — | — | — | none |

### Rows competitors have that FEATURES.md lacked (now added)

| New ID | Capability | Who has it (evidence above) |
|---|---|---|
| A11 | News and OSINT text with geoparsing, source tiering | WM, OS, AA |
| A12 | Satellites with SGP4 and passes | WM, WWV, GEV, OS |
| A13 | Weather and hazard overlays (wind, radar, clouds, lightning, cyclone cones, alerts) | GEV, WM, CT |
| A14 | World-tier aggregation (density cells, caps) | WM AIS relay |
| A15 | Layer-domain coverage parity via source plugins | WM, WWV, GEV, OS, AA |
| A16 | Camera catalog at 200k+ scale with snapshot proxy | AA, OS, GEV |
| A17 | Push ingest (webhooks, CoT webhook, vendor drones, CAD) | CT |
| B11 | Drawing and tactical graphics | CT, TAK clients, GEV |
| B12 | Point query (elevation, address, weather, sun, declination) | CT |
| B13 | Click-to-track, follow and cockpit camera | GEV, CT lock-on |
| B14 | Smooth motion: interpolation, dead reckoning, true headings | GEV |
| B15 | Import/export of GIS formats | CT, OS, WWV, TAK clients |
| B16 | OGC/ArcGIS/PMTiles basemap and overlay sources, tile proxy | CT, WM |
| B17 | Shareable views and time windows | WM, GEV |
| B18 | Command palette | WM |
| B19 | Elevation analysis (viewshed, profile, contours) | TAK clients (ATAK guide), GEV viewsheds |
| B20 | Intelligence-gaps panel | WM, GEV |
| B21 | Sensor visual modes (NVG, thermal, x-ray through terrain) | GEV, AR |
| C7 | TAK Data Sync (Marti missions) | CT, TAK clients |
| C8 | Contacts, chat and channels panel | CT, TAK clients |
| D7 | Road and trail routing | CT, GEV |
| D8 | Incident board, forms, assignments | CT |
| D9 | Anomaly and convergence detection | WM |
| D10 | AI briefs with local-model fallback | WM, GEV HUD |
| D11 | Hermes UI-control tools | GEV, OS, WWV |
| D12 | What-if forecast | OS |
| E6 | Offline field mapping on the edge | AR |
| E7 | Street-level imagery | GEV |
| E8 | Interiors and subterranean models | AR |
| F8 | Synchronised briefing session | AR |
| F9 | Disconnected mode, whole stack | AR, TX |
| F10 | Public API, MCP and CLI | WM, WWV, OS, GEV |
| G4 | Competitor performance benchmark | GEV, WWV |
| H10 | RF and spectrum layer | TX (TAKX-RF) |
| J6 | 2D/3D modes, PWA and touch | WM, CT |
| J7 | Mounted / touch mode | TX |
| K7 | Plugin registry with install from UI | WWV marketplace |
| K8 | Live broadcast and radio panel | WM, OS, GEV |

### Where OneMind already beats the field (by design, once built)

No competitor combines: TAK-native action plus mesh (only CT/TX/WT/AR), an open
bus with entity fusion (none), drone/robot command with policy, audit and human
confirm (none publicly), agents as TAK contacts (none), a 3D Tiles homestead twin
(only AR, closed), XR (only AR, closed) and intel-dashboard breadth (only the web
dashboards). That combination is OneMind's position; the matrix shows each row
must still be built and proven.

### Deliberate non-goals (competitor features OneMind will not match)

| Capability | Competitor | Reason |
|---|---|---|
| Weapon inventory with range/engagement data | AR | Mission: no weapons, no targeting (`docs/VISION.md`; prompt §14) |
| Active network scanning of third-party hosts (port scan, vuln scan) | OS RECON | Legal risk; not a civilian situational-awareness function |
| Crypto wallet tracing | OS | Off-mission (people-targeting finance intel) |
| ALPR camera location layer | GEV | Surveillance of people; out of civilian mission scope. Generic public cameras are covered by A9/A16 |
| Prediction-market and stock panels as core | WM | Allowed only as optional source plugins under A15 (P3) |

## Risks

- **Licence contamination.** CloudTAK is **AGPL-3.0** (GitHub licence field). The
  run prompt names worldmonitor and worldwideview but not CloudTAK. GPL-3.0 §13
  allows combining with AGPL-3.0 code, but the combined network service then
  carries AGPL source-offer duties. Treat CloudTAK as design-only like worldmonitor.
- **Data licences differ from code licences.** gods-eye-view's bundled Nepal flood
  data is non-commercial (README); its `local_data/` has per-folder provenance.
  MIT code reuse does not cover bundled datasets. Argos Atlas reuses public-sector
  imagery under each portal's terms.
- **Scale claims are self-reported.** WWV "10k+ at 60 fps on a moderate laptop" has
  no published benchmark; GEV's 11,575 objects was one static scene with live rows
  empty. G4 must measure on OneMind's own fixed scene before claiming "beats".
- **Closed products.** ARTAK, TAKX and WinTAK capabilities come from marketing pages
  or second-hand posts; tak.gov is login-walled. Rows marked **?** may hide
  capabilities.
- **Scope pressure.** Matching every dashboard domain (finance, China macro,
  prediction markets) dilutes the TAK/homestead mission; A15 limits this to plugins.
- **Camera catalogs** at 200k+ invite privacy and terms-of-use problems; Argos'
  policy (latest frame only, low resolution, no recognition) is the safer model.

## Recommendation

1. Adopt the 38 added rows (A11–A17, B11–B21, C7–C8, D7–D12, E6–E8, F8–F10, G4,
   H10, J6–J7, K7–K8) and the J5 re-rank in `docs/FEATURES.md` (done in this lane).
   Priorities follow the mission: TAK-parity and offline rows (B11, B15, B16, C7,
   C8, F9) are P1 because CloudTAK and the TAK clients already ship them; dashboard
   breadth rows are P1–P2 and enter as source/layer plugins (K6); cosmetic or
   off-mission rows are P3.
2. Treat CloudTAK as design-only (AGPL-3.0) and record that in decision 0004 #15's
   list when the next owner-facing update is made.
3. Reuse MIT code from gods-eye-view and osiris only with their notices, and only
   code, not bundled datasets, unless each dataset's licence is checked.
4. Make G4 the gate for any "beats" claim: one fixed scene, ≥ 11,575 objects
   (GEV's figure) with live movement, ≥ 60 fps on the Mac (native) and ≥ 57.6 fps in
   the L0 web client on comparable hardware, plus cold start ≤ 1.86 s to settled
   globe for L0.
5. Design patterns worth copying (design, not code): WM's freshness classes and
   "intelligence gaps" (A3/B20), WM's stale-on-error and request coalescing (A0),
   WM's density cells for world tier (A14), WWV's primitive collections, chunked
   loads and horizon culling (B5, maps to UE instancing), GEV's render-one-interval-
   behind interpolation (B14), CloudTAK's per-layer ETL task model (A17), and the
   competitor MCP servers (F10) mapped onto `svc.*` and `agents.*` subjects.

## Unverified items

- **unverified:** TAKX operating systems, layers and AI (tak.gov login wall; CTI
  sheet is marketing and contains the typo "ATAK (Windows)").
- **unverified:** WinTAK 5.5 change list and "long-term sustainment" status (only a
  deleted COTAK forum post, seen as a search excerpt; host unreachable here).
- **unverified:** WinTAK/TAKX parity with ATAK features (elevation tools, KLV video,
  viewsheds) listed in the public ATAK user guide; marked **?** in the matrix.
- **unverified:** ARTAK engine, whether its "PC / Mac" listing means a native macOS
  build, data formats, AI method, and how its ATAK integration works (not stated on
  the product page).
- **unverified:** whether worldmonitor's released macOS binaries are code-signed
  (signing is conditional on repo secrets in `build-desktop.yml`).
- **unverified:** Argos Atlas data refresh cadence, API, and whether the 3D globe is
  live in production (comments mention it; no runtime check was done).
- **unverified:** WWV "10k+ live entities ... 60 fps" (claim without benchmark).
- **unverified:** what `app/android` in CloudTAK ships (directory exists in the tree;
  not read).
- **unverified:** osiris 3D terrain (seen only in README screenshot captions).
- Not run: no competitor was installed or executed on this machine (no Docker; disk
  budget). All findings are from source files and documents.
