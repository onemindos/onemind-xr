# OneMind source catalogs — evidence-backed scope audit

**Result: 240 retained catalog records: 161 Fabric Feeds + 79 Map Layers. No runtime selected. No endpoint, account, license or deployment tested.**

## 1. Count and provenance reconciliation

- Read the actual UTF-8 contents and strict YAML frontmatter, not just filenames: **265 retrieved files**, of which **244** are in the two catalog directories. Exclude four host/index/view files; retain **240 `kind: record` notes**. The other 21 files are supporting context, not extra records in these totals.
- **Fabric Feeds: 163 files = 161 records + one host page + one `.base` view.** Its host says 160 nested rows. Exactly 160 records declare `notion_database: Fabric Feeds`; an additional LiveUAMap explicitly declares `Fabric Feeds (top-level copy)`, with an older August 12, 2026 edit date and substantive duplicate-preservation note [F161]. Thus the host count describes the main database, not the entire retrieved folder. Preserve the extra row. The migration manifest was not retrieved, so this is content-level, not manifest-level reconciliation.
- **Map Layers: 81 files = 79 records + one host page + one `.base` view.** Its 79-row host count matches.
- **240 distinct record IDs and 240 distinct Notion IDs; no duplicate IDs.** Three casefold-title duplicate groups cover seven rows: LiveUAMap ×3 [F079/F080/F161], OpenWeatherMap Tiles ×2 [F114/M063], WRI Global Power Plant DB ×2 [F157/M079]. These remain separate rows. There are **236 distinct casefold titles**, not 240 unique providers.
- **226 bodies explicitly say “Empty in Notion: title only”; 14 have substantive notes.** Frontmatter remains evidence, but it is not executable pipeline configuration.
- Historical status: feeds **34 Live / 126 Planned / 1 Testing**; maps **41 Live / 22 Planned / 16 Reference**. The 75 total Live labels and universal generic `status: active` are **not live operational evidence**. Feed edits are August 12/16, 2026; imported metadata is October 16, 2026. No present availability is inferred.

All source paths below are exact relative paths under:

`/Users/zeus/OneMind - 10-23-26/notion-transfer/`

The companion JSON retains every exact full path, ID, raw Markdown, frontmatter property line number, byte length and SHA-256. Classification is analyst interpretation; historical fields are retained unchanged.

## 2. What the catalog actually contains

Primary record roles are mutually exclusive **catalog classifications**, not deployable-source counts:

| Record role | Fabric Feeds | Map Layers | Total |
|---|---:|---:|---:|
| source or dataset candidate | 105 | 24 | 129 |
| website or dashboard only | 26 | 24 | 50 |
| hosted service or platform | 2 | 13 | 15 |
| protocol or standard | 11 | 4 | 15 |
| tool or adapter | 4 | 7 | 11 |
| catalog or reference | 7 | 1 | 8 |
| commercial platform reference | 6 | 6 | 12 |

Only **129** rows are classified as source/dataset candidates. Even these are **not certified accessible**. The other rows capture necessary platforms, references, protocols, software and UIs; do not silently count them as independent upstreams.

Primary intended delivery is also mutually exclusive for counting; mixed capabilities remain as secondary classes in JSON. **These counts include protocol/tool/reference rows and never imply this many live datasets.** “Events/observations” includes sensor samples, changing polygons and historical event batches.

| Intended primary delivery | Fabric Feeds | Map Layers | Total |
|---|---:|---:|---:|
| live entities | 25 | 7 | 32 |
| event alert features | 42 | 11 | 53 |
| raster vector imagery | 8 | 24 | 32 |
| static overlays | 16 | 24 | 40 |
| media | 12 | 1 | 13 |
| non geospatial reference | 58 | 12 | 70 |

### Important workload examples

- **Live entities:** ADS-B aircraft, AIS vessels, APRS contacts, MTA/Amtrak transit, mesh/robot/drone telemetry [F007–F018/F086/F087/F112/F083/F123]. CelesTrak supplies **orbital elements**, requiring time-aware propagation rather than copying TLEs into marker coordinates [F029]. GTFS-RT is a specification until an agency producer is selected [F064].
- **Event/alert features and observations:** ACLED/UCDP conflict, USGS earthquakes, NWS CAP/weather, FIRMS detections, NIFC incident/perimeter geometry, air/water/radiation readings [F005/F139/F148/F105/F090/F093/F108/F042/F125]. Events need issuance/revision/cancellation/expiry; fixed monitoring-station locations do not themselves provide fresh samples [F042].
- **Imagery and terrain/maps:** Copernicus/Earthdata/STAC assets, ArcGIS/Bing/Google/OSM imagery, GIBS, radar, PMTiles/XYZ/WMS/WMTS and hosted tile services [F034/F039/F088/M005/M007/M026/M062/M056/M067/M077/M078]. STAC is discovery before selected asset delivery. A WMS/WMTS spec is not a provider. Terrain/offline DTED/GeoTIFF/HGT adds an asset-delivery path beyond feed messages [S07].
- **Static overlays:** HIFLD stations/hospitals, WRI power plants, Amtrak/subway lines, boundaries, ALPR camera inventories and OSM/KMZ POIs [M028–M031/F157/M003/M057/F053/M012/S06]. DeFlock maps fixed crowdsourced surveillance infrastructure, **not Flock plate reads** [M012 vs F051/M023].
- **Media:** DOT camera metadata, Broadcastify/OpenMHZ/KiwiSDR/WebSDR, local radio recording and ASR [F092/F158/F026/F109/F076/F151/F137/F048]. Camera coordinates/URLs, video/audio transport, recordings, transcripts and derived alerts are different artifacts. Trunk Recorder and faster-whisper are tools, not public providers.
- **Non-geospatial/reference:** finance/economics, cyber/reputation, sanctions, social/news and identity/crypto enrichment [F049/F052/F070/F003/F110/F135/F099/F102]. Do not fabricate coordinates or force these into CoT because they travel over the same bus.

## 3. Access and executable-configuration gaps

Every row has some URL, but URL presence is not a usable contract. Syntax/context-only endpoint classification gives:

| Evidence kind | Rows | Meaning |
|---|---:|---|
| E1 | 39 | Machine-looking path/tile template; **untested**, often still lacks query, layer, scope or time parameters |
| E2 | 35 | API/service root; executable request unresolved |
| E3 | 51 | Documentation, discovery or download catalog |
| E4 | 69 | Website/dashboard/provider landing |
| E5 | 26 | Software or specification reference |
| E6 | 7 | Catalogued self-hosted base service without selected layer/asset |
| E7 | 12 | North Star UI/marketing fragment, not proven API |
| E8 | 1 | Provider identity and source link disagree |

- Historical access labels: **182 Free, 26 Freemium, 15 Paid, 9 Gov/Open, 8 Self-hosted**. Historical auth: **164 None, 67 API Key, 5 Certificate, 3 OAuth2, 1 Basic**. Neither Free nor None establishes permission or current anonymous access. No row configures a secret reference or its scope/renewal.
- **149/161 feeds have a NATS subject label**; **145 contain `*`**, five have angle-bracket placeholders (one overlaps), and **zero specify concrete publish subjects**. Twelve omit subjects entirely. Shared subjects and mixed prefixes require a reviewed namespace/source envelope, not blind copy into publisher configuration.
- **77 feeds have a `rate_limit` label; 84 omit it.** Existing values mix request quotas, refresh cadence, Real-time/Unlimited and Classified. Scheduler cadence, throughput quota, retry/backoff and freshness must be separate fields.
- **No record defines any of the three independent toggle states, a typed normalized schema, identity/expiry contract or measured health.** Map clients/CRS/format labels are compatibility intentions, not tested support. Five map rows omit attribution.

### Errors and ambiguities requiring review

1. **USPTO Patents points to `https://api.usaspending.gov/api/v2/search/`** [F150], while USAspending is also its own row [F144]. Original URL preserved; no replacement guessed.
2. **LiveUAMap copies conflict:** older F161 is Free/None/Live; F079/F080 are Freemium/API Key/Planned. MarineTraffic duplicate feed URLs differ in subject/quota [F011/F082]. MTA copies share URL/subject but differ in protocol/status [F086/F087].
3. **Cross-catalog overlaps are collection/display interfaces, not automatically new providers:** World Imagery [F043/M005/M016], FIRMS [F090/M044], NIFC [F093/M045], WRI [F157/M079], weather tiles [F113/F114/M063]. M005/M016 share an exact service URL. Review source and interface identities before sharing collectors or budgets.
4. **Auth is interface/version-specific:** ACLED historical map/feed status differs and supplemental source note gives account credentials rather than the API Key label [M001/F005/S03]; UCDP token note conflicts with None [F139/S03]; NYC 511 copies disagree on auth [F001/F106]. AIS map origin conflates MarineTraffic and AISStream [M002]. FIRMS API and overlay should not inherit each other's auth labels.
5. **North Star comprises 12 catalog records** across feed/map sectors, all Paid. Its 250k-camera, 45+-feed, AI-detection and cross-source fusion claims are **unverified descriptive scope**, not additional providers, working APIs or permission to export [F098–F103/M049–M054]. Feature URLs/fragment routes and API Key metadata prove neither API availability nor entitlement.
6. **Non-public/hardware integration is not a free hosted feed:** DCGS-AF fact sheet, Link 16/STANAG, Remote ID, MAVLink, ROS 2/DDS, Modbus, GPIO, serial and mesh all need real endpoints/devices/decoders/authorization [F036/F077/F134/F122/F083/F123/F085/F060/F128/M040/M041]. Keep telemetry collection distinct from control-command authority.
7. **Rendering/tool/service references are not sources:** Maputnik, QGIS Desktop, PyTAK, GeoServer/MapServer references and OGC/PMTiles specs [M038/M069/F118/M024/M037/M075/M077/M078/M067]. Self-hosted tailnet/cluster addresses need selected assets and an actual client access route [M039/M048/M064/M066/M070/M073]. Internal cluster DNS is not a Mac/ATAK endpoint [M066]. Unofficial Google and HTTP Bing tile templates require rights/security/client validation [M026/M007].
8. **Frontend intent is inconsistent:** architecture says TAK-only/no custom dashboard [S04:L474]; map/globe note recommends WWV plus local layer controls [S05:L44/L53]. Do not turn historical prose into a current frontend or runtime choice.

## 4. Separate three controls — required contract, not existing implementation

| Control | Scope | Off means | Must not implicitly change |
|---|---|---|---|
| `source_enabled` | Canonical collector/dataset; authorized shared/global operation | Pause new source fetch/stream work under explicit drain/restart policy; retain history | TAK enablement or another client's visibility |
| `tak_publish_enabled` | Dataset publication to a TAK sink/mission/audience | Stop eligible CoT/layer/data-package publication; apply explicit stale/withdraw policy to existing objects | Upstream ingestion, durable history, AI consumers or local view preferences |
| `local_visibility` | User + device/view + rendered layer | Hide/unsubscribe **that client**; it may avoid fetching its own tiles | Source polling, shared processing, TAK distribution or any other device |

The historical feed `tak_integration` values are **None 60 / CoT Inject 55 / Map Layer 38 / Both 5 / Planned 3**, not toggles. Map-layer serving/data packages/KML links are different publication modes from per-contact CoT [S03:L296–L305]. Mark publication **not applicable** where no valid geospatial/TAK deliverable exists.

Required behavior: source-on/TAK-off retains ingestion; source-on/TAK-on/local-hidden still updates other authorized clients; source-off/local-visible shows only clearly aged cached data; enabling TAK never silently enables a source or re-emits expired history as fresh. Define in-flight drain, catch-up, backfill, existing-track expiry and persistence across restart. If a local UI consumes only TAK, hiding publication can affect its available data; distinguish that architecture from a native normalized-bus consumer rather than conflating controls.

S01 merely says `/ops/feeds` can toggle subscriptions/ETL; it **does not resolve scope**. S05/S06 clearly describe rendering/group toggles. The new control contract should expose desired state, effective worker/publisher state and local visibility separately, with different authorization and audit.

## 5. Reliable-platform implications and acceptance criteria

The required system is **not just an HTTP-to-CoT job runner**. It needs independent acquisition/normalization, durable fan-out/history, TAK publication, asset/tile/media delivery and local visualization. Hardware/RF capture, ASR, orbit propagation and geospatial asset processing may require companion workers. Choose runtimes only after validating these boundaries against representative workloads; the retrieved Redpanda advocacy is historical design material, not a product benchmark.

- **AC01 — Inventory and canonical identity:** All 240 retrieved records remain traceable by immutable Notion ID/path. Alias/overlap groups resolve to reviewed canonical provider/dataset/interface IDs without deleting history or counting software as data. Quarantine the USPTO/USAspending mismatch; retain unresolved older LiveUAMap until approved.
- **AC02 — Access and executable source contract:** Before activation, each real dataset has approved license/coverage/AOI, exact request or subscription (method/URL/query/headers/topics/filter), documented format/schema and current expected response, provider quota distinct from poll cadence, secret reference/scope/renewal, owning operator and network/hardware dependency. Paid, restricted and sensitive sources default blocked pending authorization.
- **AC03 — Representative adapter coverage:** Demonstrate with authorized fixtures and then scoped pilot data: polled JSON/GeoJSON hazards, WebSocket AIS, GTFS-RT protobuf plus static GTFS enrichment, APRS TCP or MQTT telemetry, STAC/raster/vector tile delivery, static feature/KMZ delivery and media metadata. RF capture/ASR/orbit propagation may be separate jobs; prove integration rather than assuming one engine natively handles them.
- **AC04 — Schema and lifecycle:** Versioned envelope preserves canonical source/provider IDs, upstream entity/event IDs, event/observed/ingested times, geometry with CRS/altitude reference/accuracy, units, quality and provenance. Distinguish moving entity state, alert revisions/cancellation, historical batches, fixed POIs, media and imagery. Define original-event-time freshness/TTL per class; reject invalid coordinates and avoid invented geolocation.
- **AC05 — Reliability and recovery:** Bound per-provider retries/backoff/rate budgets and queues; isolate slow or malformed feeds; quarantine poison messages with replay provenance. Prove reconnect/restart, checkpointed resumption, provider outages, duplicate deliveries, out-of-order updates and sink outage recovery. Idempotent source+event/revision handling must preserve changed positions under the same entity UID and never replay expired alerts as fresh.
- **AC06 — Three independent controls:** Persist source_enabled globally per canonical collector/dataset; persist tak_publish_enabled per publication/mission/audience; store local_visibility per user/device/view. No checkbox implicitly changes another scope. Test source on/TAK off, source on/TAK on/local hidden, source off/visible, and re-enable after restart. Define drain, in-flight cutoff, catch-up, expiration/withdrawal and existing-track behavior. TAK not applicable is explicit for non-geospatial/reference rows.
- **AC07 — TAK and local delivery:** For tracks/events, validate stable UIDs, correct source-derived affiliations/types, times, stale policy and geometry against target clients. For large/static/raster data, test WMS/WMTS/XYZ, feature/KML network-link or data-package paths as applicable instead of forcing all items into CoT points. Validate selected layers/styles/assets, EPSG:4326/3857 transforms, tile matrices, bounds, attribution and authorized device access. If local UI only mirrors TAK, explain why TAK-disabled data cannot appear there without a separate local data path.
- **AC08 — Operator health, audit and policy:** Show desired vs effective source/publisher state separately from local visibility; show last successful fetch, upstream event age, errors/rate limiting, collection/publication counters and backlog per source and sink. Apply scoped authorization, change audit, rollback and sensitive-data retention. Local hide must not stop collection/history/AI or hide data on another device.
- **AC09 — Platform evaluation boundary:** Only after dataset contracts and workload volumes are known, score candidate platforms against these acceptance tests, operating model and measured resource/cost limits. Historical notes advocating Redpanda or a frontend do not count as current product verification. This audit makes no n8n/Node-RED/Redpanda runtime selection.

## 6. Verification and limits

- Inventory coverage: every one of the 244 target-directory inventory paths is present in the retrieved input; all target records classified exactly once.
- Zero strict frontmatter parse errors, duplicate YAML keys, duplicate IDs, or UTF-8 byte-count mismatches; both role and primary-delivery totals reconcile to 240.
- Every record includes exact path/ID/URL, historical values, per-field source lines, raw body, classification and missing-configuration flags in JSON.
- No endpoint tests, live credentials, purchases, external service calls, or Mac/cluster writes. Only the two local analysis artifacts were created. Current deployment/accessibility, actual API/auth behavior and migration-manifest reconciliation remain unverified.

## Appendix A. All catalog records and exact source paths

Legend: **SRC** source/dataset candidate; **WEB** website/dashboard only; **SVC** hosted service/platform; **SPEC** protocol/format standard; **TOOL** software/adapter/client; **CAT** discovery/reference; **PAID-REF** North Star commercial product reference. E1–E8 are the endpoint-evidence kinds above. These are classifications, not readiness ratings. The JSON holds all URLs and full field-level evidence; the path column resolves under the exact root in section 1.

### Fabric Feeds

| Ref | Exact title | Role | Intended primary output | Historical status; access/auth | Endpoint evidence | Exact source-relative path |
|---|---|---|---|---|---|---|
| <a id="f001"></a>F001 | 511 NY | SRC | Events/obs | 🟢 Live; Free/None | E4 | `so/00 Core/Platform/Fabric Feeds/511 Ny.md` |
| <a id="f002"></a>F002 | 511 NY Rideshare | SRC | Static | 🔵 Planned; Free/None | E4 | `so/00 Core/Platform/Fabric Feeds/511 NY Rideshare.md` |
| <a id="f003"></a>F003 | abuse.ch (Feodo/URLhaus) | SRC | Nongeo/ref | 🟢 Live; Free/None | E4 | `so/00 Core/Platform/Fabric Feeds/abuse.ch (Feodo - URLhaus).md` |
| <a id="f004"></a>F004 | AbuseIPDB | SRC | Nongeo/ref | 🔵 Planned; Freemium/API Key | E3 | `so/00 Core/Platform/Fabric Feeds/AbuseIPDB.md` |
| <a id="f005"></a>F005 | ACLED | SRC | Events/obs | 🔵 Planned; Free/API Key | E3 | `so/00 Core/Platform/Fabric Feeds/Acled.md` |
| <a id="f006"></a>F006 | ADIS | TOOL | Nongeo/ref | 🔵 Planned; Gov/Open/Certificate | E5 | `so/00 Core/Platform/Fabric Feeds/Adis.md` |
| <a id="f007"></a>F007 | ADS-B Exchange | SRC | Entities | 🔵 Planned; Freemium/API Key | E3 | `so/00 Core/Platform/Fabric Feeds/ADS-B Exchange.md` |
| <a id="f008"></a>F008 | adsb.lol | SRC | Entities | 🟢 Live; Free/None | E4 | `so/00 Core/Platform/Fabric Feeds/adsb.lol.md` |
| <a id="f009"></a>F009 | Airframes.io (ACARS/VDL) | SRC | Events/obs | 🔵 Planned; Free/API Key | E3 | `so/00 Core/Platform/Fabric Feeds/Airframes.io (ACARS - VDL).md` |
| <a id="f010"></a>F010 | airplanes.live | SRC | Entities | 🟢 Live; Free/None | E4 | `so/00 Core/Platform/Fabric Feeds/airplanes.live.md` |
| <a id="f011"></a>F011 | AIS (MarineTraffic) | SRC | Entities | 🔵 Planned; Freemium/API Key | E3 | `so/00 Core/Platform/Fabric Feeds/AIS (MarineTraffic).md` |
| <a id="f012"></a>F012 | AISStream.io | SRC | Entities | 🔵 Planned; Free/API Key | E4 | `so/00 Core/Platform/Fabric Feeds/AISStream.io.md` |
| <a id="f013"></a>F013 | AIStream | SRC | Nongeo/ref | 🔵 Planned; Freemium/API Key | E4 | `so/00 Core/Platform/Fabric Feeds/AIStream.md` |
| <a id="f014"></a>F014 | Al Jazeera RSS | SRC | Nongeo/ref | 🟢 Live; Free/None | E1 | `so/00 Core/Platform/Fabric Feeds/Al Jazeera RSS.md` |
| <a id="f015"></a>F015 | Alternative.me Fear/Greed | SRC | Nongeo/ref | 🔵 Planned; Free/None | E4 | `so/00 Core/Platform/Fabric Feeds/Alternative.me Fear - Greed.md` |
| <a id="f016"></a>F016 | Amtrak Train Tracking | SRC | Entities | 🔵 Planned; Free/None | E1 | `so/00 Core/Platform/Fabric Feeds/Amtrak Train Tracking.md` |
| <a id="f017"></a>F017 | APRS (aprs2.net) | SRC | Entities | 🟢 Live; Free/None | E3 | `so/00 Core/Platform/Fabric Feeds/APRS (aprs2.net).md` |
| <a id="f018"></a>F018 | APRS-IS Network | SRC | Entities | 🔵 Planned; Free/None | E3 | `so/00 Core/Platform/Fabric Feeds/APRS-IS Network.md` |
| <a id="f019"></a>F019 | ArcGIS Living Atlas | CAT | Static | 🔵 Planned; Free/None | E3 | `so/00 Core/Platform/Fabric Feeds/ArcGIS Living Atlas.md` |
| <a id="f020"></a>F020 | Asfinag (Austria CCTV) | WEB | Media | 🔵 Planned; Free/None | E4 | `so/00 Core/Platform/Fabric Feeds/Asfinag (Austria CCTV).md` |
| <a id="f021"></a>F021 | AviationStack | SRC | Entities | 🔵 Planned; Freemium/API Key | E3 | `so/00 Core/Platform/Fabric Feeds/AviationStack.md` |
| <a id="f022"></a>F022 | Baltic Live Cam | WEB | Media | 🔵 Planned; Free/None | E4 | `so/00 Core/Platform/Fabric Feeds/Baltic Live Cam.md` |
| <a id="f023"></a>F023 | BBC World RSS | SRC | Nongeo/ref | 🟢 Live; Free/None | E1 | `so/00 Core/Platform/Fabric Feeds/BBC World RSS.md` |
| <a id="f024"></a>F024 | BLS (Bureau of Labor) | SRC | Nongeo/ref | 🔵 Planned; Free/API Key | E3 | `so/00 Core/Platform/Fabric Feeds/BLS (Bureau of Labor).md` |
| <a id="f025"></a>F025 | Bluesky (Social OSINT) | SRC | Nongeo/ref | 🔵 Planned; Free/OAuth2 | E3 | `so/00 Core/Platform/Fabric Feeds/Bluesky (Social OSINT).md` |
| <a id="f026"></a>F026 | Broadcastify (Scanner Streams) | WEB | Media | 🔵 Planned; Freemium/API Key | E4 | `so/00 Core/Platform/Fabric Feeds/Broadcastify (Scanner Streams).md` |
| <a id="f027"></a>F027 | Caltrans / 511 | SRC | Nongeo/ref | 🔵 Planned; Free/None | E3 | `so/00 Core/Platform/Fabric Feeds/Caltrans - 511.md` |
| <a id="f028"></a>F028 | CARTO Basemaps | SRC | Imagery/tiles | 🟢 Live; Free/None | E3 | `so/00 Core/Platform/Fabric Feeds/CARTO Basemaps.md` |
| <a id="f029"></a>F029 | CelesTrak TLE | SRC | Entities | 🟢 Live; Free/None | E4 | `so/00 Core/Platform/Fabric Feeds/CelesTrak TLE.md` |
| <a id="f030"></a>F030 | CISA KEV | SRC | Nongeo/ref | 🟢 Live; Free/None | E1 | `so/00 Core/Platform/Fabric Feeds/Cisa Kev.md` |
| <a id="f031"></a>F031 | CitiBike GBFS API | SRC | Entities | 🔵 Planned; Free/None | E1 | `so/00 Core/Platform/Fabric Feeds/CitiBike GBFS API.md` |
| <a id="f032"></a>F032 | Cloudflare Radar | SRC | Nongeo/ref | 🔵 Planned; Free/API Key | E2 | `so/00 Core/Platform/Fabric Feeds/Cloudflare Radar.md` |
| <a id="f033"></a>F033 | CoinGecko | SRC | Nongeo/ref | 🔵 Planned; Freemium/None | E3 | `so/00 Core/Platform/Fabric Feeds/CoinGecko.md` |
| <a id="f034"></a>F034 | Copernicus/Sentinel | CAT | Imagery/tiles | 🔵 Planned; Free/OAuth2 | E3 | `so/00 Core/Platform/Fabric Feeds/Copernicus - Sentinel.md` |
| <a id="f035"></a>F035 | Datacenter Map | WEB | Static | 🔵 Planned; Free/None | E4 | `so/00 Core/Platform/Fabric Feeds/Datacenter Map.md` |
| <a id="f036"></a>F036 | DCGS-AF (Air Force) | CAT | Nongeo/ref | 🔵 Planned; Gov/Open/Certificate | E3 | `so/00 Core/Platform/Fabric Feeds/DCGS-AF (Air Force).md` |
| <a id="f037"></a>F037 | DeepState Map | SRC | Events/obs | 🟢 Live; Free/None | E4 | `so/00 Core/Platform/Fabric Feeds/DeepState Map.md` |
| <a id="f038"></a>F038 | EIA (Energy Info Admin) | SRC | Nongeo/ref | 🔵 Planned; Free/API Key | E1 | `so/00 Core/Platform/Fabric Feeds/EIA (Energy Info Admin).md` |
| <a id="f039"></a>F039 | Element84 Earth Search | SRC | Imagery/tiles | 🟢 Live; Free/None | E1 | `so/00 Core/Platform/Fabric Feeds/Element84 Earth Search.md` |
| <a id="f040"></a>F040 | EMSC Seismic Portal | SRC | Events/obs | 🟢 Live; Free/None | E2 | `so/00 Core/Platform/Fabric Feeds/EMSC Seismic Portal.md` |
| <a id="f041"></a>F041 | EPA RadNet | SRC | Events/obs | 🔵 Planned; Free/None | E4 | `so/00 Core/Platform/Fabric Feeds/EPA RadNet.md` |
| <a id="f042"></a>F042 | EPA Water Quality Portal | SRC | Static | 🔵 Planned; Free/None | E1 | `so/00 Core/Platform/Fabric Feeds/EPA Water Quality Portal.md` |
| <a id="f043"></a>F043 | Esri World Imagery | SRC | Imagery/tiles | 🟢 Live; Free/None | E1 | `so/00 Core/Platform/Fabric Feeds/Esri World Imagery.md` |
| <a id="f044"></a>F044 | Eurostat | SRC | Nongeo/ref | 🔵 Planned; Free/None | E3 | `so/00 Core/Platform/Fabric Feeds/Eurostat.md` |
| <a id="f045"></a>F045 | FAA DroneZone (LAANC) | SRC | Events/obs | 🔵 Planned; Free/API Key | E3 | `so/00 Core/Platform/Fabric Feeds/FAA DroneZone (LAANC).md` |
| <a id="f046"></a>F046 | FAA NOTAM | SRC | Events/obs | 🔵 Planned; Gov/Open/None | E1 | `so/00 Core/Platform/Fabric Feeds/Faa Notam.md` |
| <a id="f047"></a>F047 | FAA SWIM | SRC | Entities | 🔵 Planned; Free/API Key | E3 | `so/00 Core/Platform/Fabric Feeds/Faa Swim.md` |
| <a id="f048"></a>F048 | faster-whisper (Radio Transcription) | TOOL | Nongeo/ref | 🔵 Planned; Free/None | E5 | `so/00 Core/Platform/Fabric Feeds/faster-whisper (Radio Transcription).md` |
| <a id="f049"></a>F049 | Finnhub | SRC | Nongeo/ref | 🔵 Planned; Freemium/API Key | E2 | `so/00 Core/Platform/Fabric Feeds/Finnhub.md` |
| <a id="f050"></a>F050 | FlightAware AeroAPI | SRC | Entities | 🔵 Planned; Freemium/API Key | E3 | `so/00 Core/Platform/Fabric Feeds/FlightAware AeroAPI.md` |
| <a id="f051"></a>F051 | Flock Safety API | SRC | Events/obs | 🔵 Planned; Paid/API Key | E4 | `so/00 Core/Platform/Fabric Feeds/Flock Safety API.md` |
| <a id="f052"></a>F052 | FRED (St. Louis Fed) | SRC | Nongeo/ref | 🔵 Planned; Free/API Key | E2 | `so/00 Core/Platform/Fabric Feeds/FRED (St. Louis Fed).md` |
| <a id="f053"></a>F053 | GADM Boundaries | CAT | Static | 🔵 Planned; Free/None | E3 | `so/00 Core/Platform/Fabric Feeds/GADM Boundaries.md` |
| <a id="f054"></a>F054 | GDACS | SRC | Events/obs | 🔵 Planned; Free/None | E3 | `so/00 Core/Platform/Fabric Feeds/Gdacs.md` |
| <a id="f055"></a>F055 | GDELT Project | SRC | Events/obs | 🟢 Live; Free/None | E3 | `so/00 Core/Platform/Fabric Feeds/GDELT Project.md` |
| <a id="f056"></a>F056 | GeoFabrik OSM | SRC | Static | 🟢 Live; Free/None | E3 | `so/00 Core/Platform/Fabric Feeds/GeoFabrik OSM.md` |
| <a id="f057"></a>F057 | Global Fishing Watch | SRC | Static | 🔵 Planned; Free/API Key | E3 | `so/00 Core/Platform/Fabric Feeds/Global Fishing Watch.md` |
| <a id="f058"></a>F058 | GlobalTuners | WEB | Media | 🔵 Planned; Freemium/API Key | E4 | `so/00 Core/Platform/Fabric Feeds/GlobalTuners.md` |
| <a id="f059"></a>F059 | GloFAS Flood Forecast | SRC | Events/obs | 🔵 Planned; Free/None | E2 | `so/00 Core/Platform/Fabric Feeds/GloFAS Flood Forecast.md` |
| <a id="f060"></a>F060 | GPIO | SPEC | Nongeo/ref | 🔵 Planned; Free/None | E5 | `so/00 Core/Platform/Fabric Feeds/Gpio.md` |
| <a id="f061"></a>F061 | GPSJam | WEB | Events/obs | 🔵 Planned; Free/None | E4 | `so/00 Core/Platform/Fabric Feeds/GPSJam.md` |
| <a id="f062"></a>F062 | GreyNoise | SRC | Nongeo/ref | 🔵 Planned; Freemium/API Key | E1 | `so/00 Core/Platform/Fabric Feeds/GreyNoise.md` |
| <a id="f063"></a>F063 | GSCPI (NY Fed) | WEB | Nongeo/ref | 🔵 Planned; Free/None | E3 | `so/00 Core/Platform/Fabric Feeds/GSCPI (NY Fed).md` |
| <a id="f064"></a>F064 | GTFS-RT Standard | SPEC | Nongeo/ref | 🔵 Planned; Free/None | E5 | `so/00 Core/Platform/Fabric Feeds/GTFS-RT Standard.md` |
| <a id="f065"></a>F065 | HF Radio / SafeComLink | WEB | Static | 🔵 Planned; Free/None | E4 | `so/00 Core/Platform/Fabric Feeds/HF Radio - SafeComLink.md` |
| <a id="f066"></a>F066 | HIFLD | CAT | Static | 🟢 Live; Gov/Open/None | E3 | `so/00 Core/Platform/Fabric Feeds/Hifld.md` |
| <a id="f067"></a>F067 | Hyperliquid (DeFi) | WEB | Nongeo/ref | 🔵 Planned; Free/None | E4 | `so/00 Core/Platform/Fabric Feeds/Hyperliquid (DeFi).md` |
| <a id="f068"></a>F068 | IAEA Nuclear Events | SRC | Events/obs | 🔵 Planned; Free/None | E3 | `so/00 Core/Platform/Fabric Feeds/IAEA Nuclear Events.md` |
| <a id="f069"></a>F069 | IAEA Radiation Data | SRC | Events/obs | 🔵 Planned; Free/None | E3 | `so/00 Core/Platform/Fabric Feeds/IAEA Radiation Data.md` |
| <a id="f070"></a>F070 | IEA Oil & Energy Stats | SRC | Nongeo/ref | 🔵 Planned; Free/API Key | E2 | `so/00 Core/Platform/Fabric Feeds/IEA Oil & Energy Stats.md` |
| <a id="f071"></a>F071 | IMF SDMX / WEO | SRC | Nongeo/ref | 🔵 Planned; Free/None | E2 | `so/00 Core/Platform/Fabric Feeds/Imf Sdmx - Weo.md` |
| <a id="f072"></a>F072 | INFORM Risk Index | WEB | Nongeo/ref | 🔵 Planned; Free/None | E4 | `so/00 Core/Platform/Fabric Feeds/INFORM Risk Index.md` |
| <a id="f073"></a>F073 | IODA (Georgia Tech) | SRC | Events/obs | 🟢 Live; Free/None | E4 | `so/00 Core/Platform/Fabric Feeds/IODA (Georgia Tech).md` |
| <a id="f074"></a>F074 | ipapi.co | SRC | Static | 🟢 Live; Free/None | E2 | `so/00 Core/Platform/Fabric Feeds/ipapi.co.md` |
| <a id="f075"></a>F075 | Kalshi (Prediction Markets) | SRC | Nongeo/ref | 🔵 Planned; Free/API Key | E3 | `so/00 Core/Platform/Fabric Feeds/Kalshi (Prediction Markets).md` |
| <a id="f076"></a>F076 | KiwiSDR | WEB | Media | 🔵 Planned; Free/None | E4 | `so/00 Core/Platform/Fabric Feeds/KiwiSDR.md` |
| <a id="f077"></a>F077 | Link 16 / J-Series (JREAP) | SPEC | Entities | 🔵 Planned; Gov/Open/Certificate | E5 | `so/00 Core/Platform/Fabric Feeds/Link 16 - J-Series (JREAP).md` |
| <a id="f078"></a>F078 | Live News Streams (YouTube) | WEB | Media | 🟢 Live; Free/None | E4 | `so/00 Core/Platform/Fabric Feeds/Live News Streams (YouTube).md` |
| <a id="f079"></a>F079 | LiveUAMap | WEB | Events/obs | 🔵 Planned; Freemium/API Key | E4 | `so/00 Core/Platform/Fabric Feeds/LiveUAMap.md` |
| <a id="f080"></a>F080 | LiveUAMap | WEB | Events/obs | 🔵 Planned; Freemium/API Key | E4 | `so/00 Core/Platform/Fabric Feeds/LiveUAMap (d98d18).md` |
| <a id="f081"></a>F081 | MappingSupport | CAT | Static | 🟢 Live; Free/None | E3 | `so/00 Core/Platform/Fabric Feeds/MappingSupport.md` |
| <a id="f082"></a>F082 | MarineTraffic AIS | SRC | Nongeo/ref | 🔵 Planned; Freemium/API Key | E3 | `so/00 Core/Platform/Fabric Feeds/MarineTraffic AIS.md` |
| <a id="f083"></a>F083 | MAVLink | SPEC | Entities | 🔵 Planned; Free/None | E5 | `so/00 Core/Platform/Fabric Feeds/MAVLink.md` |
| <a id="f084"></a>F084 | Meshtastic Map | WEB | Entities | 🔵 Planned; Free/None | E4 | `so/00 Core/Platform/Fabric Feeds/Meshtastic Map.md` |
| <a id="f085"></a>F085 | Modbus | SPEC | Nongeo/ref | 🔵 Planned; Free/None | E5 | `so/00 Core/Platform/Fabric Feeds/Modbus.md` |
| <a id="f086"></a>F086 | MTA GTFS-RT | SRC | Entities | 🟡 Testing; Free/API Key | E2 | `so/00 Core/Platform/Fabric Feeds/Mta Gtfs-rt.md` |
| <a id="f087"></a>F087 | MTA NYC (Transit) | SRC | Entities | 🔵 Planned; Free/API Key | E2 | `so/00 Core/Platform/Fabric Feeds/MTA NYC (Transit).md` |
| <a id="f088"></a>F088 | NASA Earthdata | CAT | Imagery/tiles | 🔵 Planned; Free/None | E3 | `so/00 Core/Platform/Fabric Feeds/NASA Earthdata.md` |
| <a id="f089"></a>F089 | NASA EONET | SRC | Events/obs | 🟢 Live; Free/None | E2 | `so/00 Core/Platform/Fabric Feeds/Nasa Eonet.md` |
| <a id="f090"></a>F090 | NASA FIRMS | SRC | Events/obs | 🔵 Planned; Free/API Key | E2 | `so/00 Core/Platform/Fabric Feeds/Nasa Firms.md` |
| <a id="f091"></a>F091 | NASA GIBS / MODIS Imagery | SRC | Imagery/tiles | 🔵 Planned; Free/None | E3 | `so/00 Core/Platform/Fabric Feeds/NASA GIBS - MODIS Imagery.md` |
| <a id="f092"></a>F092 | NC DOT Traffic Cameras | SRC | Media | 🔵 Planned; Free/None | E1 | `so/00 Core/Platform/Fabric Feeds/NC DOT Traffic Cameras.md` |
| <a id="f093"></a>F093 | NIFC Fire Perimeters | SRC | Events/obs | 🔵 Planned; Free/None | E1 | `so/00 Core/Platform/Fabric Feeds/NIFC Fire Perimeters.md` |
| <a id="f094"></a>F094 | NOAA NWS Alerts | SRC | Events/obs | 🟢 Live; Free/None | E3 | `so/00 Core/Platform/Fabric Feeds/NOAA NWS Alerts.md` |
| <a id="f095"></a>F095 | NOAA SWPC Space Weather | SRC | Nongeo/ref | 🟢 Live; Free/None | E2 | `so/00 Core/Platform/Fabric Feeds/NOAA SWPC Space Weather.md` |
| <a id="f096"></a>F096 | NOAA Weather | SRC | Events/obs | 🟢 Live; Gov/Open/None | E2 | `so/00 Core/Platform/Fabric Feeds/NOAA Weather.md` |
| <a id="f097"></a>F097 | Nominatim (OSM Geocoding) | SVC | Static | 🟢 Live; Free/None | E2 | `so/00 Core/Platform/Fabric Feeds/Nominatim (OSM Geocoding).md` |
| <a id="f098"></a>F098 | North Star — Camera Globe | PAID-REF | Media | 🔵 Planned; Paid/API Key | E7 | `so/00 Core/Platform/Fabric Feeds/North Star — Camera Globe.md` |
| <a id="f099"></a>F099 | North Star — Crypto Tracing | PAID-REF | Nongeo/ref | 🔵 Planned; Paid/API Key | E7 | `so/00 Core/Platform/Fabric Feeds/North Star — Crypto Tracing.md` |
| <a id="f100"></a>F100 | North Star — NetScan | PAID-REF | Nongeo/ref | 🔵 Planned; Paid/API Key | E7 | `so/00 Core/Platform/Fabric Feeds/North Star — NetScan.md` |
| <a id="f101"></a>F101 | North Star — Platform (nstarlive.com) | PAID-REF | Nongeo/ref | 🔵 Planned; Paid/API Key | E7 | `so/00 Core/Platform/Fabric Feeds/North Star — Platform (nstarlive.com).md` |
| <a id="f102"></a>F102 | North Star — Username / Identity OSINT | PAID-REF | Nongeo/ref | 🔵 Planned; Paid/API Key | E7 | `so/00 Core/Platform/Fabric Feeds/North Star — Username - Identity OSINT.md` |
| <a id="f103"></a>F103 | North Star — Watchtower (Global SA Globe) | PAID-REF | Entities | 🔵 Planned; Paid/API Key | E7 | `so/00 Core/Platform/Fabric Feeds/North Star — Watchtower (Global SA Globe).md` |
| <a id="f104"></a>F104 | NRC Event Notifications | SRC | Events/obs | 🔵 Planned; Free/None | E4 | `so/00 Core/Platform/Fabric Feeds/NRC Event Notifications.md` |
| <a id="f105"></a>F105 | NWS CAP Alerts | SRC | Events/obs | 🔵 Planned; Free/None | E1 | `so/00 Core/Platform/Fabric Feeds/NWS CAP Alerts.md` |
| <a id="f106"></a>F106 | NYC 511 (Traffic) | SRC | Events/obs | 🔵 Planned; Free/API Key | E3 | `so/00 Core/Platform/Fabric Feeds/NYC 511 (Traffic).md` |
| <a id="f107"></a>F107 | ONVIF | SPEC | Nongeo/ref | 🔵 Planned; Free/Basic | E5 | `so/00 Core/Platform/Fabric Feeds/Onvif.md` |
| <a id="f108"></a>F108 | OpenAQ | SRC | Events/obs | 🔵 Planned; Free/API Key | E1 | `so/00 Core/Platform/Fabric Feeds/OpenAQ.md` |
| <a id="f109"></a>F109 | OpenMHZ (Scanner Feeds) | WEB | Media | 🔵 Planned; Free/None | E4 | `so/00 Core/Platform/Fabric Feeds/OpenMHZ (Scanner Feeds).md` |
| <a id="f110"></a>F110 | OpenSanctions | SRC | Nongeo/ref | 🔵 Planned; Free/None | E3 | `so/00 Core/Platform/Fabric Feeds/OpenSanctions.md` |
| <a id="f111"></a>F111 | OpenSeaMap | SRC | Static | 🔵 Planned; Free/None | E4 | `so/00 Core/Platform/Fabric Feeds/OpenSeaMap.md` |
| <a id="f112"></a>F112 | OpenSky Network | SRC | Entities | 🔵 Planned; Free/None | E2 | `so/00 Core/Platform/Fabric Feeds/OpenSky Network.md` |
| <a id="f113"></a>F113 | OpenWeatherMap | SRC | Events/obs | 🔵 Planned; Freemium/API Key | E2 | `so/00 Core/Platform/Fabric Feeds/OpenWeatherMap.md` |
| <a id="f114"></a>F114 | OpenWeatherMap Tiles | SRC | Imagery/tiles | 🔵 Planned; Freemium/API Key | E4 | `so/00 Core/Platform/Fabric Feeds/OpenWeatherMap Tiles.md` |
| <a id="f115"></a>F115 | Overpass API (OSM) | SVC | Static | 🟢 Live; Free/None | E2 | `so/00 Core/Platform/Fabric Feeds/Overpass API (OSM).md` |
| <a id="f116"></a>F116 | PeeringDB | SRC | Static | 🔵 Planned; Free/None | E2 | `so/00 Core/Platform/Fabric Feeds/PeeringDB.md` |
| <a id="f117"></a>F117 | PulsePoint (Fire/EMS CAD Dispatch) | WEB | Events/obs | 🔵 Planned; Free/API Key | E4 | `so/00 Core/Platform/Fabric Feeds/PulsePoint (Fire - EMS CAD Dispatch).md` |
| <a id="f118"></a>F118 | PyTAK / ADSBCOT / AISCOT | TOOL | Nongeo/ref | 🟢 Live; Free/Certificate | E5 | `so/00 Core/Platform/Fabric Feeds/Pytak - Adsbcot - Aiscot.md` |
| <a id="f119"></a>F119 | RadiationNetwork.com | WEB | Events/obs | 🔵 Planned; Free/None | E4 | `so/00 Core/Platform/Fabric Feeds/RadiationNetwork.com.md` |
| <a id="f120"></a>F120 | Reddit (Social OSINT) | SRC | Nongeo/ref | 🔵 Planned; Free/OAuth2 | E3 | `so/00 Core/Platform/Fabric Feeds/Reddit (Social OSINT).md` |
| <a id="f121"></a>F121 | ReliefWeb (UN OCHA) | SRC | Events/obs | 🔵 Planned; Free/None | E2 | `so/00 Core/Platform/Fabric Feeds/ReliefWeb (UN OCHA).md` |
| <a id="f122"></a>F122 | Remote ID (ASTM F3411) | SPEC | Entities | 🔵 Planned; Free/None | E5 | `so/00 Core/Platform/Fabric Feeds/Remote ID (ASTM F3411).md` |
| <a id="f123"></a>F123 | ROS 2 / DDS | SPEC | Entities | 🔵 Planned; Free/None | E5 | `so/00 Core/Platform/Fabric Feeds/Ros 2 - Dds.md` |
| <a id="f124"></a>F124 | SAE J2735 / V2X | SPEC | Entities | 🔵 Planned; Free/None | E5 | `so/00 Core/Platform/Fabric Feeds/Sae J2735 - V2X.md` |
| <a id="f125"></a>F125 | Safecast (Radiation) | SRC | Events/obs | 🔵 Planned; Free/None | E2 | `so/00 Core/Platform/Fabric Feeds/Safecast (Radiation).md` |
| <a id="f126"></a>F126 | SAM.gov (Federal Contracts) | SRC | Nongeo/ref | 🔵 Planned; Free/API Key | E3 | `so/00 Core/Platform/Fabric Feeds/SAM.gov (Federal Contracts).md` |
| <a id="f127"></a>F127 | SATnogs | SRC | Nongeo/ref | 🔵 Planned; Free/API Key | E1 | `so/00 Core/Platform/Fabric Feeds/SATnogs.md` |
| <a id="f128"></a>F128 | Serial Port | SPEC | Nongeo/ref | 🔵 Planned; Free/None | E5 | `so/00 Core/Platform/Fabric Feeds/Serial Port.md` |
| <a id="f129"></a>F129 | Shadowserver Foundation | WEB | Nongeo/ref | 🔵 Planned; Free/API Key | E4 | `so/00 Core/Platform/Fabric Feeds/Shadowserver Foundation.md` |
| <a id="f130"></a>F130 | Shodan InternetDB | SRC | Nongeo/ref | 🔵 Planned; Free/None | E2 | `so/00 Core/Platform/Fabric Feeds/Shodan InternetDB.md` |
| <a id="f131"></a>F131 | SkyWatch EarthCache | SRC | Imagery/tiles | 🔵 Planned; Freemium/API Key | E3 | `so/00 Core/Platform/Fabric Feeds/SkyWatch EarthCache.md` |
| <a id="f132"></a>F132 | Smithsonian GVP Volcanoes | WEB | Events/obs | 🔵 Planned; Free/None | E4 | `so/00 Core/Platform/Fabric Feeds/Smithsonian GVP Volcanoes.md` |
| <a id="f133"></a>F133 | SpaceWeatherLive | SRC | Nongeo/ref | 🔵 Planned; Free/None | E2 | `so/00 Core/Platform/Fabric Feeds/SpaceWeatherLive.md` |
| <a id="f134"></a>F134 | STANAG 4586 (UAS Control) | SPEC | Entities | 🔵 Planned; Gov/Open/Certificate | E5 | `so/00 Core/Platform/Fabric Feeds/STANAG 4586 (UAS Control).md` |
| <a id="f135"></a>F135 | Telegram OSINT Channels | SRC | Nongeo/ref | 🟢 Live; Free/API Key | E3 | `so/00 Core/Platform/Fabric Feeds/Telegram OSINT Channels.md` |
| <a id="f136"></a>F136 | TfL (Transport for London) | SRC | Nongeo/ref | 🔵 Planned; Free/API Key | E2 | `so/00 Core/Platform/Fabric Feeds/TfL (Transport for London).md` |
| <a id="f137"></a>F137 | Trunk Recorder (P25/SmartNet Radio) | TOOL | Media | 🔵 Planned; Free/None | E5 | `so/00 Core/Platform/Fabric Feeds/Trunk Recorder (P25 - SmartNet Radio).md` |
| <a id="f138"></a>F138 | Tzeva Adom (Red Alert IL) | SRC | Events/obs | 🔵 Planned; Free/None | E1 | `so/00 Core/Platform/Fabric Feeds/Tzeva Adom (Red Alert IL).md` |
| <a id="f139"></a>F139 | UCDP (Uppsala Conflict Data) | SRC | Events/obs | 🔵 Planned; Free/None | E3 | `so/00 Core/Platform/Fabric Feeds/UCDP (Uppsala Conflict Data).md` |
| <a id="f140"></a>F140 | UKMTO (UK Maritime Trade Ops) | WEB | Events/obs | 🔵 Planned; Free/None | E4 | `so/00 Core/Platform/Fabric Feeds/UKMTO (UK Maritime Trade Ops).md` |
| <a id="f141"></a>F141 | UN Comtrade | WEB | Nongeo/ref | 🔵 Planned; Freemium/API Key | E4 | `so/00 Core/Platform/Fabric Feeds/UN Comtrade.md` |
| <a id="f142"></a>F142 | UNHCR Population Data | SRC | Nongeo/ref | 🔵 Planned; Free/None | E2 | `so/00 Core/Platform/Fabric Feeds/UNHCR Population Data.md` |
| <a id="f143"></a>F143 | US Treasury FiscalData | SRC | Nongeo/ref | 🔵 Planned; Free/None | E3 | `so/00 Core/Platform/Fabric Feeds/US Treasury FiscalData.md` |
| <a id="f144"></a>F144 | USAspending.gov | SRC | Nongeo/ref | 🔵 Planned; Free/None | E2 | `so/00 Core/Platform/Fabric Feeds/USAspending.gov.md` |
| <a id="f145"></a>F145 | USDA NASS Yields | SRC | Nongeo/ref | 🔵 Planned; Gov/Open/API Key | E2 | `so/00 Core/Platform/Fabric Feeds/USDA NASS Yields.md` |
| <a id="f146"></a>F146 | USDA PLANTS DB | SRC | Nongeo/ref | 🔵 Planned; Gov/Open/None | E3 | `so/00 Core/Platform/Fabric Feeds/Usda Plants DB.md` |
| <a id="f147"></a>F147 | USFS Fire Detection | SRC | Events/obs | 🔵 Planned; Free/None | E3 | `so/00 Core/Platform/Fabric Feeds/USFS Fire Detection.md` |
| <a id="f148"></a>F148 | USGS Earthquakes | SRC | Events/obs | 🟢 Live; Free/None | E3 | `so/00 Core/Platform/Fabric Feeds/USGS Earthquakes.md` |
| <a id="f149"></a>F149 | USGS NWIS (Water) | SRC | Events/obs | 🔵 Planned; Free/None | E2 | `so/00 Core/Platform/Fabric Feeds/USGS NWIS (Water).md` |
| <a id="f150"></a>F150 | USPTO Patents | SRC | Nongeo/ref | 🔵 Planned; Free/None | E8 | `so/00 Core/Platform/Fabric Feeds/USPTO Patents.md` |
| <a id="f151"></a>F151 | WebSDR Network | WEB | Media | 🔵 Planned; Free/None | E4 | `so/00 Core/Platform/Fabric Feeds/WebSDR Network.md` |
| <a id="f152"></a>F152 | Where the ISS at | SRC | Entities | 🟢 Live; Free/None | E2 | `so/00 Core/Platform/Fabric Feeds/Where the ISS at.md` |
| <a id="f153"></a>F153 | WHO Disease Outbreaks | SRC | Events/obs | 🔵 Planned; Free/None | E1 | `so/00 Core/Platform/Fabric Feeds/WHO Disease Outbreaks.md` |
| <a id="f154"></a>F154 | Wikidata | SRC | Nongeo/ref | 🔵 Planned; Free/None | E3 | `so/00 Core/Platform/Fabric Feeds/Wikidata.md` |
| <a id="f155"></a>F155 | Wikipedia REST API | SRC | Nongeo/ref | 🟢 Live; Free/None | E2 | `so/00 Core/Platform/Fabric Feeds/Wikipedia REST API.md` |
| <a id="f156"></a>F156 | Windy API | SRC | Events/obs | 🔵 Planned; Freemium/API Key | E2 | `so/00 Core/Platform/Fabric Feeds/Windy API.md` |
| <a id="f157"></a>F157 | WRI Global Power Plant DB | SRC | Static | 🔵 Planned; Free/None | E3 | `so/00 Core/Platform/Fabric Feeds/WRI Global Power Plant DB.md` |
| <a id="f158"></a>F158 | WSDOT Traffic Cameras | SRC | Media | 🔵 Planned; Free/None | E3 | `so/00 Core/Platform/Fabric Feeds/WSDOT Traffic Cameras.md` |
| <a id="f159"></a>F159 | WWV (NIST Time) | WEB | Nongeo/ref | 🟢 Live; Free/None | E4 | `so/00 Core/Platform/Fabric Feeds/WWV (NIST Time).md` |
| <a id="f160"></a>F160 | Yahoo Finance | WEB | Nongeo/ref | 🔵 Planned; Free/None | E4 | `so/00 Core/Platform/Fabric Feeds/Yahoo Finance.md` |
| <a id="f161"></a>F161 | LiveUAMap | WEB | Events/obs | 🟢 Live; Free/None | E4 | `so/00 Core/Platform/Fabric Feeds/LiveUAMap (older version).md` |

### Map Layers

| Ref | Exact title | Role | Intended primary output | Historical status; access/auth | Endpoint evidence | Exact source-relative path |
|---|---|---|---|---|---|---|
| <a id="m001"></a>M001 | ACLED Crisis Map | WEB | Events/obs | 🟢 Live; Free/API Key | E4 | `so/01 The Fabric/Geo/Map Layers/ACLED Crisis Map.md` |
| <a id="m002"></a>M002 | AIS Vessel Overlay | WEB | Entities | 🔵 Planned; Free/None | E4 | `so/01 The Fabric/Geo/Map Layers/AIS Vessel Overlay.md` |
| <a id="m003"></a>M003 | Amtrak Rail Network | SRC | Static | 🟢 Live; Free/None | E1 | `so/01 The Fabric/Geo/Map Layers/Amtrak Rail Network.md` |
| <a id="m004"></a>M004 | ArcGIS Online | SVC | Nongeo/ref | 🟢 Live; Free/None | E2 | `so/01 The Fabric/Geo/Map Layers/ArcGIS Online.md` |
| <a id="m005"></a>M005 | ArcGIS World Imagery | SRC | Imagery/tiles | 🟢 Live; Free/None | E1 | `so/01 The Fabric/Geo/Map Layers/ArcGIS World Imagery.md` |
| <a id="m006"></a>M006 | Bing Maps | SVC | Nongeo/ref | ⚫ Reference; Freemium/API Key | E4 | `so/01 The Fabric/Geo/Map Layers/Bing Maps.md` |
| <a id="m007"></a>M007 | Bing Satellite (XYZ) | SRC | Imagery/tiles | 🟢 Live; Free/None | E1 | `so/01 The Fabric/Geo/Map Layers/Bing Satellite (XYZ).md` |
| <a id="m008"></a>M008 | Cesium ion | SVC | Imagery/tiles | ⚫ Reference; Paid/API Key | E4 | `so/01 The Fabric/Geo/Map Layers/Cesium ion.md` |
| <a id="m009"></a>M009 | Cloudflare Radar Map | WEB | Events/obs | 🟢 Live; Free/API Key | E4 | `so/01 The Fabric/Geo/Map Layers/Cloudflare Radar Map.md` |
| <a id="m010"></a>M010 | CloudTAK Tiles (PMTiles) | SVC | Imagery/tiles | 🟢 Live; Self-hosted/None | E6 | `so/01 The Fabric/Geo/Map Layers/CloudTAK Tiles (PMTiles).md` |
| <a id="m011"></a>M011 | DCD Data Center Map | WEB | Static | ⚫ Reference; Free/None | E4 | `so/01 The Fabric/Geo/Map Layers/DCD Data Center Map.md` |
| <a id="m012"></a>M012 | DeFlock — Crowdsourced ALPR / License Plate Reader Map | WEB | Static | 🟢 Live; Free/None | E4 | `so/01 The Fabric/Geo/Map Layers/DeFlock — Crowdsourced ALPR - License Plate Reader Map.md` |
| <a id="m013"></a>M013 | EIA Power Plant Locations | WEB | Static | 🟢 Live; Free/None | E4 | `so/01 The Fabric/Geo/Map Layers/EIA Power Plant Locations.md` |
| <a id="m014"></a>M014 | EPA RadNet Map | WEB | Events/obs | 🟢 Live; Free/None | E4 | `so/01 The Fabric/Geo/Map Layers/EPA RadNet Map.md` |
| <a id="m015"></a>M015 | EPA Water Quality Map | SRC | Events/obs | 🔵 Planned; Free/None | E2 | `so/01 The Fabric/Geo/Map Layers/EPA Water Quality Map.md` |
| <a id="m016"></a>M016 | Esri Imagery (World) | SRC | Imagery/tiles | 🟢 Live; Free/None | E1 | `so/01 The Fabric/Geo/Map Layers/Esri Imagery (World).md` |
| <a id="m017"></a>M017 | Esri World Street Map | SRC | Imagery/tiles | 🟢 Live; Free/None | E1 | `so/01 The Fabric/Geo/Map Layers/Esri World Street Map.md` |
| <a id="m018"></a>M018 | Esri World Topo | SRC | Imagery/tiles | 🟢 Live; Free/None | E1 | `so/01 The Fabric/Geo/Map Layers/Esri World Topo.md` |
| <a id="m019"></a>M019 | FAA Sectional Charts | WEB | Imagery/tiles | 🟢 Live; Free/None | E4 | `so/01 The Fabric/Geo/Map Layers/FAA Sectional Charts.md` |
| <a id="m020"></a>M020 | FAA TFR Overlay | WEB | Events/obs | 🔵 Planned; Free/None | E4 | `so/01 The Fabric/Geo/Map Layers/FAA TFR Overlay.md` |
| <a id="m021"></a>M021 | FEMA Flood Zones | SRC | Static | 🟢 Live; Free/None | E1 | `so/01 The Fabric/Geo/Map Layers/FEMA Flood Zones.md` |
| <a id="m022"></a>M022 | FlightAware Map | WEB | Entities | 🔵 Planned; Freemium/API Key | E4 | `so/01 The Fabric/Geo/Map Layers/FlightAware Map.md` |
| <a id="m023"></a>M023 | Flock Safety Camera Map | WEB | Static | 🔵 Planned; Paid/API Key | E4 | `so/01 The Fabric/Geo/Map Layers/Flock Safety Camera Map.md` |
| <a id="m024"></a>M024 | GeoServer (OGC) | TOOL | Imagery/tiles | ⚫ Reference; Free/None | E5 | `so/01 The Fabric/Geo/Map Layers/GeoServer (OGC).md` |
| <a id="m025"></a>M025 | Google Maps | SVC | Nongeo/ref | ⚫ Reference; Freemium/API Key | E2 | `so/01 The Fabric/Geo/Map Layers/Google Maps.md` |
| <a id="m026"></a>M026 | Google Satellite (XYZ) | SRC | Imagery/tiles | 🟢 Live; Free/None | E1 | `so/01 The Fabric/Geo/Map Layers/Google Satellite (XYZ).md` |
| <a id="m027"></a>M027 | Heavens Above (Sat Tracks) | WEB | Entities | 🔵 Planned; Free/None | E4 | `so/01 The Fabric/Geo/Map Layers/Heavens Above (Sat Tracks).md` |
| <a id="m028"></a>M028 | HIFLD EMS Stations | SRC | Static | 🟢 Live; Free/None | E1 | `so/01 The Fabric/Geo/Map Layers/HIFLD EMS Stations.md` |
| <a id="m029"></a>M029 | HIFLD Fire Stations | SRC | Static | 🟢 Live; Free/None | E1 | `so/01 The Fabric/Geo/Map Layers/HIFLD Fire Stations.md` |
| <a id="m030"></a>M030 | HIFLD Hospitals | SRC | Static | 🟢 Live; Free/None | E1 | `so/01 The Fabric/Geo/Map Layers/HIFLD Hospitals.md` |
| <a id="m031"></a>M031 | HIFLD Law Enforcement | SRC | Static | 🟢 Live; Free/None | E1 | `so/01 The Fabric/Geo/Map Layers/HIFLD Law Enforcement.md` |
| <a id="m032"></a>M032 | IAEA PRIS (Global Reactors) | WEB | Static | 🟢 Live; Free/None | E4 | `so/01 The Fabric/Geo/Map Layers/IAEA PRIS (Global Reactors).md` |
| <a id="m033"></a>M033 | IODA Internet Outage Map | WEB | Events/obs | 🟢 Live; Free/None | E4 | `so/01 The Fabric/Geo/Map Layers/IODA Internet Outage Map.md` |
| <a id="m034"></a>M034 | KiwiSDR Receiver Map | WEB | Static | 🟢 Live; Free/None | E4 | `so/01 The Fabric/Geo/Map Layers/KiwiSDR Receiver Map.md` |
| <a id="m035"></a>M035 | LiveUAMap Overlay | WEB | Events/obs | 🔵 Planned; Freemium/API Key | E4 | `so/01 The Fabric/Geo/Map Layers/LiveUAMap Overlay.md` |
| <a id="m036"></a>M036 | Mapbox | SVC | Nongeo/ref | ⚫ Reference; Freemium/API Key | E4 | `so/01 The Fabric/Geo/Map Layers/Mapbox.md` |
| <a id="m037"></a>M037 | MapServer (OGC) | TOOL | Imagery/tiles | ⚫ Reference; Free/None | E5 | `so/01 The Fabric/Geo/Map Layers/MapServer (OGC).md` |
| <a id="m038"></a>M038 | Maputnik (Style Editor) | TOOL | Nongeo/ref | 🟢 Live; Self-hosted/None | E5 | `so/01 The Fabric/Geo/Map Layers/Maputnik (Style Editor).md` |
| <a id="m039"></a>M039 | Martin (Vector Tiles) | SVC | Imagery/tiles | 🟢 Live; Self-hosted/None | E6 | `so/01 The Fabric/Geo/Map Layers/Martin (Vector Tiles).md` |
| <a id="m040"></a>M040 | MeshCore | TOOL | Entities | 🔵 Planned; Free/None | E5 | `so/01 The Fabric/Geo/Map Layers/MeshCore.md` |
| <a id="m041"></a>M041 | Meshtastic | TOOL | Entities | 🟢 Live; Free/None | E5 | `so/01 The Fabric/Geo/Map Layers/Meshtastic.md` |
| <a id="m042"></a>M042 | MTA Map (NYC Transit) | WEB | Static | 🔵 Planned; Free/None | E4 | `so/01 The Fabric/Geo/Map Layers/MTA Map (NYC Transit).md` |
| <a id="m043"></a>M043 | NASA Eyes (3D Space) | WEB | Nongeo/ref | ⚫ Reference; Free/None | E4 | `so/01 The Fabric/Geo/Map Layers/NASA Eyes (3D Space).md` |
| <a id="m044"></a>M044 | NASA FIRMS Fire Overlay | SRC | Events/obs | 🟢 Live; Free/None | E1 | `so/01 The Fabric/Geo/Map Layers/NASA FIRMS Fire Overlay.md` |
| <a id="m045"></a>M045 | NIFC Fire Perimeters (Map) | SRC | Events/obs | 🔵 Planned; Free/None | E1 | `so/01 The Fabric/Geo/Map Layers/NIFC Fire Perimeters (Map).md` |
| <a id="m046"></a>M046 | NJ Transit Rail | CAT | Static | 🔵 Planned; Free/None | E3 | `so/01 The Fabric/Geo/Map Layers/NJ Transit Rail.md` |
| <a id="m047"></a>M047 | NOAA ENC (Nautical Charts) | SRC | Imagery/tiles | 🟢 Live; Free/None | E1 | `so/01 The Fabric/Geo/Map Layers/NOAA ENC (Nautical Charts).md` |
| <a id="m048"></a>M048 | Nominatim (Geocoding) | SVC | Static | 🟢 Live; Self-hosted/None | E6 | `so/01 The Fabric/Geo/Map Layers/Nominatim (Geocoding).md` |
| <a id="m049"></a>M049 | North Star — Camera Globe (250k Cameras) | PAID-REF | Media | 🔵 Planned; Paid/API Key | E7 | `so/01 The Fabric/Geo/Map Layers/North Star — Camera Globe (250k Cameras).md` |
| <a id="m050"></a>M050 | North Star — Crypto Tracing Map | PAID-REF | Nongeo/ref | 🔵 Planned; Paid/API Key | E7 | `so/01 The Fabric/Geo/Map Layers/North Star — Crypto Tracing Map.md` |
| <a id="m051"></a>M051 | North Star — Identity OSINT Layer | PAID-REF | Nongeo/ref | 🔵 Planned; Paid/API Key | E7 | `so/01 The Fabric/Geo/Map Layers/North Star — Identity OSINT Layer.md` |
| <a id="m052"></a>M052 | North Star — NetScan (Network Recon) | PAID-REF | Nongeo/ref | 🔵 Planned; Paid/API Key | E7 | `so/01 The Fabric/Geo/Map Layers/North Star — NetScan (Network Recon).md` |
| <a id="m053"></a>M053 | North Star — Platform Overview | PAID-REF | Nongeo/ref | ⚫ Reference; Paid/API Key | E7 | `so/01 The Fabric/Geo/Map Layers/North Star — Platform Overview.md` |
| <a id="m054"></a>M054 | North Star — Watchtower Globe | PAID-REF | Entities | 🔵 Planned; Paid/API Key | E7 | `so/01 The Fabric/Geo/Map Layers/North Star — Watchtower Globe.md` |
| <a id="m055"></a>M055 | NRC Nuclear Reactor Map | WEB | Static | 🟢 Live; Free/None | E4 | `so/01 The Fabric/Geo/Map Layers/NRC Nuclear Reactor Map.md` |
| <a id="m056"></a>M056 | NWS Radar (MRMS) | SRC | Imagery/tiles | 🟢 Live; Free/None | E1 | `so/01 The Fabric/Geo/Map Layers/NWS Radar (MRMS).md` |
| <a id="m057"></a>M057 | NYC Subway Lines | SRC | Static | 🟢 Live; Free/None | E1 | `so/01 The Fabric/Geo/Map Layers/NYC Subway Lines.md` |
| <a id="m058"></a>M058 | OpenAIP (Aviation) | SRC | Static | 🟢 Live; Free/API Key | E2 | `so/01 The Fabric/Geo/Map Layers/OpenAIP (Aviation).md` |
| <a id="m059"></a>M059 | OpenCPN | TOOL | Nongeo/ref | ⚫ Reference; Free/None | E5 | `so/01 The Fabric/Geo/Map Layers/OpenCPN.md` |
| <a id="m060"></a>M060 | OpenSeaMap (Nautical Charts) | SRC | Imagery/tiles | 🟢 Live; Free/None | E1 | `so/01 The Fabric/Geo/Map Layers/OpenSeaMap (Nautical Charts).md` |
| <a id="m061"></a>M061 | OpenSky Network (Map) | WEB | Entities | 🔵 Planned; Free/None | E4 | `so/01 The Fabric/Geo/Map Layers/OpenSky Network (Map).md` |
| <a id="m062"></a>M062 | OpenStreetMap (Tile CDN) | SRC | Imagery/tiles | 🟢 Live; Free/None | E1 | `so/01 The Fabric/Geo/Map Layers/OpenStreetMap (Tile CDN).md` |
| <a id="m063"></a>M063 | OpenWeatherMap Tiles | SRC | Imagery/tiles | 🔵 Planned; Freemium/API Key | E1 | `so/01 The Fabric/Geo/Map Layers/OpenWeatherMap Tiles.md` |
| <a id="m064"></a>M064 | Overpass API | SVC | Static | 🟢 Live; Self-hosted/None | E6 | `so/01 The Fabric/Geo/Map Layers/Overpass API.md` |
| <a id="m065"></a>M065 | PeeringDB Data Center Map | SRC | Static | 🔵 Planned; Free/None | E1 | `so/01 The Fabric/Geo/Map Layers/PeeringDB Data Center Map.md` |
| <a id="m066"></a>M066 | pg_featureserv | SVC | Static | 🟢 Live; Self-hosted/None | E6 | `so/01 The Fabric/Geo/Map Layers/pg_featureserv.md` |
| <a id="m067"></a>M067 | PMTiles (Format/Spec) | SPEC | Imagery/tiles | ⚫ Reference; Free/None | E5 | `so/01 The Fabric/Geo/Map Layers/PMTiles (Format - Spec).md` |
| <a id="m068"></a>M068 | Protomaps | SVC | Imagery/tiles | ⚫ Reference; Free/None | E4 | `so/01 The Fabric/Geo/Map Layers/Protomaps.md` |
| <a id="m069"></a>M069 | QGIS Desktop | TOOL | Nongeo/ref | ⚫ Reference; Free/None | E5 | `so/01 The Fabric/Geo/Map Layers/QGIS Desktop.md` |
| <a id="m070"></a>M070 | QGIS Server (WMS/WFS) | SVC | Imagery/tiles | 🟢 Live; Self-hosted/None | E6 | `so/01 The Fabric/Geo/Map Layers/QGIS Server (WMS - WFS).md` |
| <a id="m071"></a>M071 | Safecast Radiation Map | WEB | Events/obs | 🟢 Live; Free/None | E4 | `so/01 The Fabric/Geo/Map Layers/Safecast Radiation Map.md` |
| <a id="m072"></a>M072 | SafeComLink HF Map | WEB | Static | 🔵 Planned; Free/None | E4 | `so/01 The Fabric/Geo/Map Layers/SafeComLink HF Map.md` |
| <a id="m073"></a>M073 | TileServer GL | SVC | Imagery/tiles | 🟢 Live; Self-hosted/None | E6 | `so/01 The Fabric/Geo/Map Layers/TileServer GL.md` |
| <a id="m074"></a>M074 | USGS Water Resources Map | WEB | Events/obs | 🔵 Planned; Free/None | E4 | `so/01 The Fabric/Geo/Map Layers/USGS Water Resources Map.md` |
| <a id="m075"></a>M075 | WFS (OGC Standard) | SPEC | Static | ⚫ Reference; Free/None | E5 | `so/01 The Fabric/Geo/Map Layers/WFS (OGC Standard).md` |
| <a id="m076"></a>M076 | Windy Embed | WEB | Imagery/tiles | 🔵 Planned; Freemium/API Key | E4 | `so/01 The Fabric/Geo/Map Layers/Windy Embed.md` |
| <a id="m077"></a>M077 | WMS (OGC Standard) | SPEC | Imagery/tiles | ⚫ Reference; Free/None | E5 | `so/01 The Fabric/Geo/Map Layers/WMS (OGC Standard).md` |
| <a id="m078"></a>M078 | WMTS (OGC Standard) | SPEC | Imagery/tiles | ⚫ Reference; Free/None | E5 | `so/01 The Fabric/Geo/Map Layers/WMTS (OGC Standard).md` |
| <a id="m079"></a>M079 | WRI Global Power Plant DB | SRC | Static | 🟢 Live; Free/None | E3 | `so/01 The Fabric/Geo/Map Layers/WRI Global Power Plant DB.md` |

## Appendix B. Supporting evidence (not additional catalog records)

### S01

Source: `so/01 The Fabric/TAK/OneTAK UI/omos-ops - Feed Systems.md`

- L28: Manage NATS subscriptions and ETL data feeds from one UI. /ops/feeds shows active subjects, message rates, toggle on/off. Connects to omos-intel overlay feeds.

### S02

Source: `so/01 The Fabric/TAK/OneTAK UI/omos-intel — Intel Feeds Plugin.md`

- L27: Full-page route + map layers. ShadowBroker (aircraft), WorldMonitor (ships), ACLED/FIRMS/NOAA.

### S03

Source: `so/01 The Fabric/TAK/Knowledge/TAK CoT Feed Sources — OneMind OS.md`

- L28: ## What is CoT?
- L296: ### Option A — NATS → CoT Bridge (recommended for OMOS)
- L301: ### Option B — TAK Server Data Package
- L303: ### Option C — Network Link KML
- L105: **Auth:** `ACLED_EMAIL` + `ACLED_PASSWORD` (free academic/non-profit registration)
- L131: **Auth:** `UCDP_ACCESS_TOKEN` (free registration)
- L65: **Auth:** Anonymous (limited rate) or OAuth2 (`OPENSKY_CLIENT_ID` / `OPENSKY_CLIENT_SECRET`)

### S04

Source: `so/00 Core/Architecture/Knowledge/OneMind Feed Architecture - Before & After (Redpanda Connect).md`

- L419: ### 3. Separation of Concerns
- L474: | Frontend | NOT custom dashboards | TAK IS the UI — don't duplicate maps |
- L413: ### 2. The Universal Bus (NATS)

### S05

Source: `so/01 The Fabric/Geo/Knowledge/OneMind OS — Recon 02 - Map - Globe Frontend Overlap.md`

- L53: Deploy **WorldWideView** to DOKS as the OneMind global map. Build a **NATS → WebSocket bridge** that maps subjects to DataBus events. Each data domain = one WWV plugin. Wire **Agent Bus → Legacy** so Legacy can fly the globe and toggle layers. Keep om-unity COP as debug view. Extract layer catalog + CII from WorldMonitor before deleting it.
- L44: 1. **Agent Bus (docs/agent-bus.md).** HTTP+SSE write path. MCP server `szski/wwv-mcp` exposes 15 tools (globe_fly_to, layer_toggle, engine_query…). Legacy can drive the globe. Nothing else has this.

### S06

Source: `so/01 The Fabric/Geo/KMZ Overlays/NYC Metro POI Overlay — Full Infrastructure.md`

- L30: > ✅ BUILT — 127,350 placemarks across 13 toggleable layer groups. Live on Zeus's Mac at ~/Desktop/OneMind-Overlays/nyc_metro_poi_overlay.kmz
- L135: > Total: 127,350 placemarks across 13 groups / 51 sub-categories
- L37: 1. Toggle any group on/off — each expands to sub-categories

### S07

Source: `so/01 The Fabric/Geo/Geo DB/Elevation Data — DTED, SRTM, USGS & TAK Integration.md`

- L25: > Elevation data gives every asset on the fabric a Z-axis — terrain awareness for drones, robots, vehicles, and operators. ATAK/CivTAK uses DTED format natively. This page unifies the format spec, all public data sources, and the OneMind pipeline for getting terrain into TAK.
- L31: - Formats used in OneMind: DTED (.dt0/.dt1/.dt2), GeoTIFF (.tif), HGT (SRTM)

### S08

Source: `so/01 The Fabric/Integrations/Knowledge/Redpanda Connect — OneMind Universal Feed Engine (Technical Reference).md`

- L90: ### RATE LIMITS (Throttling)

## Appendix C. Excluded host/view files

- `so/01 The Fabric/Geo/Map Layers/Map Layers.md` — host/index Markdown or .base view, not kind:record catalog row.
- `so/01 The Fabric/Geo/Map Layers/Map Layers.base` — host/index Markdown or .base view, not kind:record catalog row.
- `so/00 Core/Platform/Fabric Feeds/Fabric Feeds.md` — host/index Markdown or .base view, not kind:record catalog row.
- `so/00 Core/Platform/Fabric Feeds/Fabric Feeds.base` — host/index Markdown or .base view, not kind:record catalog row.
