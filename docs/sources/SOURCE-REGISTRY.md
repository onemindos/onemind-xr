# Source registry

Every one of the 240 catalog records in `map-sources/` classified for ingestion: delivery mode,
cadence, auth, rate limit, licence, cost, geometry, OneMind layer, route and the subject in the
**locked omos grammar** (`omos/CLAUDE.md`). The full record per source (licence, rate limit,
notes, probe detail, historical `fabric.*` pattern) is in [`sources.yaml`](sources.yaml); the
method, evidence and caveats are in
[`../research/source-registry-method.md`](../research/source-registry-method.md).

- Generated 2026-10-29. Endpoint probes: one request each, 2026-10-29T08:20:29Z, from the build machine.
- **unverified** means the value comes from the historical catalog or could not be proven from a
  primary source. A probe result shows reachability on that date only, not a working integration.
- Subjects: collector output is `intel.{domain}.{source}.{type}` (observations and events),
  `tel.*` (own devices through an edge leaf), `det.*` (detections) or `svc.*` (on-demand lookups).
  Moving things also get a derived `ent.{type}.{id}.state` written by the entity service (A4),
  with the latest value in KV. Tiles, video and documents get no bus subject.

## Routing rule

| Kind of data | Goes to | Never |
|---|---|---|
| Observations and events | JetStream (`intel.*`, `tel.*`, `det.*`) | — |
| Latest state of a moving thing or a gridded value | KV (derived `ent.*.state`, per-AOI forecasts) | — |
| Static overlays (stations, boundaries, perimeters archive) | Object Store / PMTiles / GeoPackage | per-feature messages on the bus |
| Map tiles (XYZ, WMS, WMTS, 3D Tiles) | caching tile proxy straight to the client | NATS |
| Video and audio | MediaMTX; stream URL in a camera entity | NATS |
| Reference documents and lookup data | Hermes knowledge store (or `svc.*` RPC) | the map |

## Counts

| Status | Records |
|---|---:|
| candidate | 123 |
| website-only | 48 |
| excluded | 11 |
| edge-device | 9 |
| hosted-service | 8 |
| commercial-reference | 8 |
| self-hosted | 8 |
| catalog | 7 |
| standard | 7 |
| tool | 6 |
| reference-only | 4 |
| identity-conflict | 1 |

| Route | Records |
|---|---:|
| none | 64 |
| jetstream | 61 |
| object_store | 34 |
| tile_proxy | 28 |
| hermes_knowledge | 21 |
| jetstream+kv | 15 |
| edge_leaf | 9 |
| mediamtx | 5 |
| kv | 3 |

| Delivery mode | Records |
|---|---:|
| rest_poll | 100 |
| none | 60 |
| bulk_download | 16 |
| tiles_xyz | 16 |
| device | 9 |
| audio | 6 |
| scrape | 6 |
| rss | 5 |
| tiles_wms | 5 |
| websocket | 4 |
| video | 3 |
| tcp_stream | 2 |
| tiles_pmtiles | 2 |
| tiles_mvt | 2 |
| message_queue | 1 |
| tiles_wmts | 1 |
| tiles_3d | 1 |
| mqtt | 1 |

| Probe outcome | Records |
|---|---:|
| tested reachable | 87 |
| not tested: needs API key / account | 41 |
| not tested: not an endpoint | 27 |
| tested moved | 18 |
| not tested: paid | 17 |
| shared result | 12 |
| tested forbidden (auth or bot protection) | 10 |
| not tested: tailnet/cluster-internal host, unreachable from build machine | 7 |
| tested network error | 6 |
| not tested: OAuth2 account required | 1 |
| not tested: provider now requires a key | 1 |
| tested rate limited | 1 |
| not tested: FAA partner program | 1 |
| not tested: FAA API portal issues client_id/client_secret | 1 |
| tested not found | 1 |
| not tested: OAuth2 required | 1 |
| not tested: MTProto api_id/api_hash account required | 1 |
| tested auth required | 1 |
| not tested: catalog link points to USAspending, not USPTO | 1 |
| not tested: no official API | 1 |
| not tested: direct tile URL bypasses Bing Maps API keys/terms | 1 |
| not tested: Google allows imagery only through Maps Platform APIs | 1 |
| not tested: FIRMS map services need a MAP_KEY | 1 |
| tested bad request | 1 |

## Registry


### Fabric Feeds

| Ref | Name | Status | Pri | Mode | Cadence | Auth | Cost | Geometry | Layer | Route | Subject / state | Probe |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| F001 | 511 NY | candidate | P2 | rest_poll | unverified | none (catalog, unverified) | Free (catalog) | point / polygon | road-traffic | JetStream | `intel.transit.511ny.event` | 200 reachable |
| F002 | 511 NY Rideshare | website-only | P3 | none | n/a | none (catalog, unverified) | Free (catalog) | none | road-traffic | not ingested | — | 200 reachable |
| F003 | abuse.ch (Feodo/URLhaus) | candidate | P3 | bulk_download | unverified (Feodo blocklist regenerated periodically) | none (catalog, unverified) | Free (catalog) | none | cyber | Hermes knowledge store | `intel.cyber.abusech.ioc` | 200 reachable |
| F004 | AbuseIPDB | candidate | P3 | rest_poll | unverified | api_key (catalog, unverified) | Freemium (catalog) | none | cyber | Hermes knowledge store | `svc.cyber.abuseipdb` | not tested: needs API key / account (rule: never call keyed APIs) |
| F005 | ACLED | candidate | P1 | rest_poll | omos polls 3600 s (omos/platform/omos/intel-scripts.yaml); ACLED data updated weekly (unverified) | OAuth2 (myACLED email/password -> https://acleddata.com/oauth/token, 24 h access token) | Free (catalog) | point / polygon | conflict | JetStream | `intel.conflict.acled.event` | not tested: OAuth2 account required |
| F006 | ADIS | reference-only | P3 | none | n/a | certificate (catalog, unverified) | Gov/Open (catalog) | none | reference | not ingested | — | not tested: not an endpoint (standard, tool or reference) |
| F007 | ADS-B Exchange | candidate | P3 | rest_poll | unverified | api_key (catalog, unverified) | Paid (RapidAPI/enterprise) - unverified | point (moving) | air | JetStream + KV latest state | `intel.air.adsbx.position ; ent.aircraft.{icao}.state -> KV world-state` | not tested: needs API key / account (rule: never call keyed APIs); paid tiers |
| F008 | adsb.lol | candidate | P0 | rest_poll | 5-10 s per AOI (proposed) | none (catalog, unverified) | Free (catalog) | point (moving) | air | JetStream + KV latest state | `intel.air.adsblol.position ; ent.aircraft.{icao}.state -> KV world-state` | 200 reachable |
| F009 | Airframes.io (ACARS/VDL) | candidate | P3 | websocket | unverified | api_key (catalog, unverified) | Free (catalog) | point / polygon | air | JetStream | `intel.air.airframes.acars` | not tested: needs API key / account (rule: never call keyed APIs) |
| F010 | airplanes.live | candidate | P0 | rest_poll | 1 req/s max | none (catalog, unverified) | Free (catalog) | point (moving) | air | JetStream + KV latest state | `intel.air.airplaneslive.position ; ent.aircraft.{icao}.state -> KV world-state` | 403 forbidden (auth or bot protection) |
| F011 | AIS (MarineTraffic) | candidate | P3 | rest_poll | unverified | api_key (catalog, unverified) | Paid credits - unverified | point (moving) | sea | JetStream + KV latest state | `intel.sea.marinetraffic.position ; ent.vessel.{mmsi}.state` | not tested: paid (decision 0004 #19) |
| F012 | AISStream.io | candidate | P0 | websocket | continuous stream | api_key (catalog, unverified) | Free (catalog) | point (moving) | sea | JetStream + KV latest state | `intel.sea.aisstream.position ; ent.vessel.{mmsi}.state -> KV world-state` | not tested: needs API key / account (rule: never call keyed APIs) (key sent in subscription message) |
| F013 | AIStream | candidate | P3 | rest_poll | unverified | api_key (catalog, unverified) | Freemium (catalog) | none | news | Hermes knowledge store | `intel.news.aistream.item` | not tested: needs API key / account (rule: never call keyed APIs) |
| F014 | Al Jazeera RSS | candidate | P2 | rss | 15 min (proposed) | none (catalog, unverified) | Free (catalog) | none | news | JetStream | `intel.news.aljazeera.item` | 200 reachable |
| F015 | Alternative.me Fear/Greed | candidate | P3 | rest_poll | daily | none (catalog, unverified) | Free (catalog) | none | finance | JetStream | `intel.finance.alternativeme.index` | 200 reachable |
| F016 | Amtrak Train Tracking | candidate | P2 | rest_poll | 60 s (proposed) | none (catalog, unverified) | Free (catalog) | point (moving) | rail-transit | JetStream + KV latest state | `intel.transit.amtrak.position ; ent.train.{id}.state` | 200 reachable |
| F017 | APRS (aprs2.net) | candidate | P1 | tcp_stream | continuous | APRS-IS login with callsign; receive-only passcode -1 (unverified) | Free (catalog) | point (moving) | rf | JetStream + KV latest state | `intel.rf.aprs.position ; ent.station.{callsign}.state` | 200 reachable |
| F018 | APRS-IS Network | candidate | P1 | tcp_stream | continuous | none (catalog, unverified) | Free (catalog) | point (moving) | rf | JetStream + KV latest state | `intel.rf.aprs.position ; ent.station.{callsign}.state` | 200 reachable |
| F019 | ArcGIS Living Atlas | catalog | P2 | rest_poll | on demand (catalog search) | none (catalog, unverified) | Free (catalog) | point / line / polygon | overlays | Object Store / PMTiles | `svc.geo.arcgis` | 200 reachable |
| F020 | Asfinag (Austria CCTV) | website-only | P3 | video | unverified | none (catalog, unverified) | Free (catalog) | point (camera/receiver) + stream | cameras | MediaMTX | `ent.camera.{id}.state` | 403 forbidden (auth or bot protection) |
| F021 | AviationStack | candidate | P3 | rest_poll | unverified | api_key (catalog, unverified) | Freemium (catalog) | point (moving) | air | JetStream | `intel.air.aviationstack.flight` | not tested: needs API key / account (rule: never call keyed APIs) |
| F022 | Baltic Live Cam | website-only | P3 | video | unverified | none (catalog, unverified) | Free (catalog) | point (camera/receiver) + stream | cameras | not ingested | `ent.camera.{id}.state` | 200 reachable |
| F023 | BBC World RSS | candidate | P2 | rss | 15 min (proposed) | none (catalog, unverified) | Free (catalog) | none | news | JetStream | `intel.news.bbc.item` | 200 reachable |
| F024 | BLS (Bureau of Labor) | candidate | P3 | rest_poll | monthly releases | API key optional: v1 keyless, v2 keyed (unverified) | Free (catalog) | none | finance | JetStream | `intel.finance.bls.series` | 200 reachable |
| F025 | Bluesky (Social OSINT) | candidate | P3 | websocket | continuous (Jetstream firehose) or search poll | Public AppView reads keyless; posting needs OAuth (unverified) | Free (catalog) | none | news | JetStream | `intel.social.bluesky.post` | 200 reachable |
| F026 | Broadcastify (Scanner Streams) | website-only | P3 | audio | unverified | api_key (catalog, unverified) | Freemium (catalog) | point (camera/receiver) + stream | rf | not ingested | — | not tested: needs API key / account (rule: never call keyed APIs); terms restrict rebroadcast (unverified) |
| F027 | Caltrans / 511 | candidate | P3 | rest_poll | unverified | none (catalog, unverified) | Free (catalog) | none | road-traffic | JetStream | `intel.transit.caltrans.status` | 200 reachable |
| F028 | CARTO Basemaps | candidate | P2 | tiles_xyz | unverified | API key now required (github.com/CartoDB/basemap-styles README) | Free (catalog) | raster or vector tiles | basemap | caching tile proxy (never NATS) | — | not tested: provider now requires a key |
| F029 | CelesTrak TLE | candidate | P1 | rest_poll | once per 2 h (CelesTrak updates every 2 h; repeat downloads get 403) | none (catalog, unverified) | Free (catalog) | point (moving) | space | JetStream + KV latest state | `intel.space.celestrak.tle ; ent.satellite.{norad}.state (propagated)` | 200 reachable |
| F030 | CISA KEV | candidate | P3 | bulk_download | daily check | none (catalog, unverified) | Free (catalog) | none | cyber | Hermes knowledge store | `intel.cyber.cisa.kev` | 200 reachable |
| F031 | CitiBike GBFS API | candidate | P2 | rest_poll | per GBFS ttl (proposed 60 s) | none (catalog, unverified) | Free (catalog) | point (moving) | rail-transit | JetStream + KV latest state | `intel.transit.citibike.status ; ent.station.{id}.state` | 403 forbidden (auth or bot protection) |
| F032 | Cloudflare Radar | candidate | P3 | rest_poll | unverified | api_key (catalog, unverified) | Free (catalog) | none | cyber | JetStream | `intel.cyber.cloudflare.outage` | not tested: needs API key / account (rule: never call keyed APIs) |
| F033 | CoinGecko | candidate | P3 | rest_poll | unverified | public tier now uses a free demo key (unverified) | Freemium (catalog) | none | finance | JetStream | `intel.finance.coingecko.price` | 200 reachable |
| F034 | Copernicus/Sentinel | catalog | P2 | rest_poll | daily (STAC search) | OAuth2 for downloads (catalog) | Free (catalog) | raster or vector tiles | imagery | Object Store / PMTiles | — | 000 network error |
| F035 | Datacenter Map | website-only | P3 | none | n/a | none (catalog, unverified) | Free (catalog) | none | infra | not ingested | — | 429 rate limited |
| F036 | DCGS-AF (Air Force) | reference-only | P3 | none | n/a | certificate (catalog, unverified) | Gov/Open (catalog) | none | reference | not ingested | — | not tested: not an endpoint (standard, tool or reference) |
| F037 | DeepState Map | candidate | P2 | scrape | daily | none (catalog, unverified) | Free (catalog) | point / polygon | conflict | Object Store / PMTiles | `intel.conflict.deepstate.frontline` | 200 reachable |
| F038 | EIA (Energy Info Admin) | candidate | P3 | rest_poll | unverified | api_key (catalog, unverified) | Free (catalog) | none | finance | JetStream | `intel.energy.eia.series` | not tested: needs API key / account (rule: never call keyed APIs) |
| F039 | Element84 Earth Search | candidate | P1 | rest_poll | on demand / daily STAC search | none (catalog, unverified) | Free (catalog) | raster or vector tiles | imagery | Object Store / PMTiles | `intel.imagery.earthsearch.scene` | 200 reachable |
| F040 | EMSC Seismic Portal | candidate | P0 | rest_poll | omos polls 120 s (omos/platform/omos/intel-scripts.yaml); WebSocket push also offered (unverified) | none (catalog, unverified) | Free (catalog) | point / polygon | hazards | JetStream | `intel.seismic.emsc.event` | 200 reachable |
| F041 | EPA RadNet | candidate | P2 | scrape | hourly (unverified) | none (catalog, unverified) | Free (catalog) | point / polygon | hazards | JetStream | `intel.radiation.radnet.reading` | 200 reachable |
| F042 | EPA Water Quality Portal | candidate | P3 | rest_poll | daily | none (catalog, unverified) | Free (catalog) | point / line / polygon | environment | Object Store / PMTiles | `intel.water.wqp.station` | 200 reachable |
| F043 | Esri World Imagery | candidate | P1 | tiles_xyz | unverified | none (catalog, unverified) | Free (catalog) | raster or vector tiles | basemap | caching tile proxy (never NATS) | — | 200 reachable |
| F044 | Eurostat | candidate | P3 | rest_poll | monthly | none (catalog, unverified) | Free (catalog) | none | finance | JetStream | `intel.finance.eurostat.series` | 200 reachable |
| F045 | FAA DroneZone (LAANC) | candidate | P3 | rest_poll | unverified | api_key (catalog, unverified) | Free (catalog) | point / polygon | aviation-airspace | JetStream | `intel.air.laanc.authorization` | not tested: FAA partner program (approved UAS service suppliers only) - unverified |
| F046 | FAA NOTAM | candidate | P1 | rest_poll | unverified | none (catalog, unverified) | Gov/Open (catalog) | point / polygon | aviation-airspace | JetStream | `intel.air.faa.notam` | not tested: FAA API portal issues client_id/client_secret (unverified; catalog says none) |
| F047 | FAA SWIM | candidate | P3 | message_queue | continuous (JMS/Solace, unverified) | api_key (catalog, unverified) | Free (catalog) | point (moving) | air | JetStream | `intel.air.faaswim.flight` | not tested: needs API key / account (rule: never call keyed APIs) (SWIM subscription) |
| F048 | faster-whisper (Radio Transcription) | tool | P2 | none | n/a | none (catalog, unverified) | Free (catalog) | none | rf | Hermes knowledge store | `intel.rf.whisper.transcript` | not tested: not an endpoint (standard, tool or reference) |
| F049 | Finnhub | candidate | P3 | rest_poll | unverified | api_key (catalog, unverified) | Freemium (catalog) | none | finance | JetStream | `intel.finance.finnhub.quote` | not tested: needs API key / account (rule: never call keyed APIs) |
| F050 | FlightAware AeroAPI | candidate | P3 | rest_poll | unverified | api_key (catalog, unverified) | Freemium/metered | point (moving) | air | JetStream | `intel.air.flightaware.flight` | not tested: needs API key / account (rule: never call keyed APIs); metered |
| F051 | Flock Safety API | excluded | - | rest_poll | unverified | api_key (catalog, unverified) | Paid (catalog) | point / polygon | none | not ingested | — | not tested: paid (decision 0004 #19) |
| F052 | FRED (St. Louis Fed) | candidate | P3 | rest_poll | unverified | api_key (catalog, unverified) | Free (catalog) | none | finance | JetStream | `intel.finance.fred.series` | not tested: needs API key / account (rule: never call keyed APIs) |
| F053 | GADM Boundaries | catalog | P2 | bulk_download | per release (years) | none (catalog, unverified) | Free (catalog) | point / line / polygon | boundaries | Object Store / PMTiles | — | 200 reachable |
| F054 | GDACS | candidate | P0 | rss | omos polls 600 s (omos/platform/omos/intel-scripts.yaml) | none (catalog, unverified) | Free (catalog) | point / polygon | hazards | JetStream | `intel.disaster.gdacs.event` | 200 reachable |
| F055 | GDELT Project | candidate | P1 | bulk_download | every 15 min (blog.gdeltproject.org GDELT 2.0) | none (catalog, unverified) | Free (catalog) | point / polygon | news | JetStream | `intel.news.gdelt.event` | 301 moved → https://data.gdeltproject.org:443/gdeltv2/lastupdate.txt |
| F056 | GeoFabrik OSM | candidate | P1 | bulk_download | daily extracts | none (catalog, unverified) | Free (catalog) | point / line / polygon | overlays | Object Store / PMTiles | — | 200 reachable |
| F057 | Global Fishing Watch | candidate | P3 | rest_poll | unverified | api_key (catalog, unverified) | Free (catalog) | point / line / polygon | sea | JetStream | `intel.sea.gfw.event` | not tested: needs API key / account (rule: never call keyed APIs) |
| F058 | GlobalTuners | website-only | P3 | audio | unverified | api_key (catalog, unverified) | Freemium (catalog) | point (camera/receiver) + stream | rf | not ingested | — | not tested: needs API key / account (rule: never call keyed APIs) |
| F059 | GloFAS Flood Forecast | candidate | P3 | rest_poll | daily | none (catalog, unverified) | Free (catalog) | point / polygon | hazards | Object Store / PMTiles | `intel.water.glofas.forecast` | 301 moved → https://global-flood.emergency.copernicus.eu/api/ |
| F060 | GPIO | edge-device | P2 | device | event-driven | none (catalog, unverified) | Free (catalog) | none | fleet | edge NATS leaf -> JetStream | `tel.{node}.gpio.{metric}` | not tested: not an endpoint (standard, tool or reference) |
| F061 | GPSJam | website-only | P3 | bulk_download | daily | none (catalog, unverified) | Free (catalog) | point / polygon | rf | Object Store / PMTiles | `intel.rf.gpsjam.daily` | 200 reachable |
| F062 | GreyNoise | candidate | P3 | rest_poll | unverified | api_key (catalog, unverified) | Freemium (catalog) | none | cyber | Hermes knowledge store | `svc.cyber.greynoise` | not tested: needs API key / account (rule: never call keyed APIs) |
| F063 | GSCPI (NY Fed) | website-only | P3 | bulk_download | monthly | none (catalog, unverified) | Free (catalog) | none | finance | JetStream | `intel.finance.nyfed.gscpi` | 200 reachable |
| F064 | GTFS-RT Standard | standard | P1 | none | n/a | none (catalog, unverified) | Free (catalog) | none | rail-transit | not ingested | — | not tested: not an endpoint (standard, tool or reference) |
| F065 | HF Radio / SafeComLink | website-only | P3 | none | n/a | none (catalog, unverified) | Free (catalog) | none | rf | not ingested | — | 301 moved → https://www.safecomlink.com/ |
| F066 | HIFLD | catalog | P1 | rest_poll | dataset updates (months) | none (catalog, unverified) | Gov/Open (catalog) | point / line / polygon | infra | Object Store / PMTiles | — | 404 not found |
| F067 | Hyperliquid (DeFi) | website-only | P3 | none | n/a | none (catalog, unverified) | Free (catalog) | none | finance | not ingested | — | 200 reachable |
| F068 | IAEA Nuclear Events | candidate | P3 | scrape | unverified | none (catalog, unverified) | Free (catalog) | point / polygon | hazards | JetStream | `intel.nuclear.iaea.event` | 403 forbidden (auth or bot protection) |
| F069 | IAEA Radiation Data | reference-only | P3 | none | n/a | none (catalog, unverified) | Free (catalog) | point / polygon | hazards | Hermes knowledge store | — | 403 forbidden (auth or bot protection) |
| F070 | IEA Oil & Energy Stats | candidate | P3 | rest_poll | unverified | api_key (catalog, unverified) | Free (catalog) | none | finance | JetStream | `intel.energy.iea.series` | not tested: needs API key / account (rule: never call keyed APIs) |
| F071 | IMF SDMX / WEO | candidate | P3 | rest_poll | monthly/quarterly | none (catalog, unverified) | Free (catalog) | none | finance | JetStream | `intel.finance.imf.series` | 308 moved → https://data.imf.org/api |
| F072 | INFORM Risk Index | website-only | P3 | bulk_download | yearly | none (catalog, unverified) | Free (catalog) | none | humanitarian | Hermes knowledge store | — | 301 moved → http://drmkc.jrc.ec.europa.eu/inform-index |
| F073 | IODA (Georgia Tech) | candidate | P2 | rest_poll | 5-10 min (proposed) | none (catalog, unverified) | Free (catalog) | point / polygon | cyber | JetStream | `intel.cyber.ioda.outage` | 200 reachable |
| F074 | ipapi.co | candidate | P3 | rest_poll | on demand | none (catalog, unverified) | Free (catalog) | point / line / polygon | cyber | not ingested | `svc.geo.ipapi` | 200 reachable |
| F075 | Kalshi (Prediction Markets) | candidate | P3 | rest_poll | unverified | api_key (catalog, unverified) | Free (catalog) | none | finance | JetStream | `intel.finance.kalshi.market` | not tested: needs API key / account (rule: never call keyed APIs) |
| F076 | KiwiSDR | website-only | P3 | audio | unverified | none (catalog, unverified) | Free (catalog) | point (camera/receiver) + stream | rf | not ingested | — | 000 network error |
| F077 | Link 16 / J-Series (JREAP) | excluded | - | none | n/a | certificate (catalog, unverified) | Gov/Open (catalog) | none | none | not ingested | — | not tested: not an endpoint (standard, tool or reference); military datalink, certificate-gated |
| F078 | Live News Streams (YouTube) | website-only | P3 | video | unverified | none (catalog, unverified) | Free (catalog) | point (camera/receiver) + stream | news | not ingested | — | 200 reachable |
| F079 | LiveUAMap | website-only | P3 | rest_poll | unverified | api_key (catalog, unverified) | Freemium (catalog) | point / polygon | conflict | JetStream | `intel.conflict.liveuamap.event` | 200 reachable |
| F080 | LiveUAMap | website-only | P3 | rest_poll | unverified | api_key (catalog, unverified) | Freemium (catalog) | point / polygon | conflict | JetStream | `intel.conflict.liveuamap.event` | see F079 |
| F081 | MappingSupport | catalog | P3 | none | n/a | none (catalog, unverified) | Free (catalog) | none | overlays | not ingested | — | 200 reachable |
| F082 | MarineTraffic AIS | candidate | P3 | rest_poll | unverified | api_key (catalog, unverified) | Freemium (catalog) | none | sea | JetStream + KV latest state | `intel.sea.marinetraffic.position` | not tested: paid (decision 0004 #19) |
| F083 | MAVLink | edge-device | P0 | device | 1-10 Hz | none (catalog, unverified) | Free (catalog) | point (moving) | fleet | edge NATS leaf -> JetStream | `tel.drone.{id}.{metric} ; ent.drone.{id}.state; KV fleet-state` | not tested: not an endpoint (standard, tool or reference) |
| F084 | Meshtastic Map | website-only | P3 | rest_poll | unverified | none (catalog, unverified) | Free (catalog) | point (moving) | rf | JetStream | `intel.rf.meshtastic.node` | 200 reachable |
| F085 | Modbus | edge-device | P3 | device | poll per device | none (catalog, unverified) | Free (catalog) | none | fleet | edge NATS leaf -> JetStream | `tel.{node}.modbus.{metric}` | not tested: not an endpoint (standard, tool or reference) |
| F086 | MTA GTFS-RT | candidate | P1 | rest_poll | 30 s (GTFS-RT protobuf) | none: omos fetches api-endpoint.mta.info without a key (omos/platform/data/redpanda-connect/deployment.yaml:38) | Free (catalog) | point (moving) | rail-transit | JetStream + KV latest state | `intel.transit.mta.position ; ent.train.{id}.state` | 200 reachable |
| F087 | MTA NYC (Transit) | candidate | P1 | rest_poll | 30 s | api_key (catalog, unverified) | Free (catalog) | point (moving) | rail-transit | JetStream + KV latest state | `intel.transit.mta.position ; ent.train.{id}.state` | see F086 |
| F088 | NASA Earthdata | catalog | P2 | rest_poll | on demand (CMR search) | Earthdata Login for downloads (unverified) | Free (catalog) | raster or vector tiles | imagery | Object Store / PMTiles | — | 200 reachable |
| F089 | NASA EONET | candidate | P0 | rest_poll | omos polls 300 s (omos/platform/omos/intel-scripts.yaml) | none (catalog, unverified) | Free (catalog) | point / polygon | hazards | JetStream | `intel.natural.eonet.event` | 200 reachable |
| F090 | NASA FIRMS | candidate | P0 | rest_poll | omos polls 600 s (omos/platform/omos/intel-scripts.yaml) | MAP_KEY required (firms.modaps.eosdis.nasa.gov/api/map_key) | Free (catalog) | point / polygon | hazards | JetStream | `intel.fire.firms.detection` | not tested: needs API key / account (rule: never call keyed APIs) |
| F091 | NASA GIBS / MODIS Imagery | candidate | P1 | tiles_wmts | unverified | none (GIBS access-basics page mentions no auth) | Free (catalog) | raster or vector tiles | imagery | caching tile proxy (never NATS) | — | 200 reachable |
| F092 | NC DOT Traffic Cameras | candidate | P3 | rest_poll | 5 min (camera list) | none (catalog, unverified) | Free (catalog) | point (camera/receiver) + stream | cameras | MediaMTX | `ent.camera.{id}.state` | 200 reachable |
| F093 | NIFC Fire Perimeters | candidate | P1 | rest_poll | 15 min (proposed) | none (catalog, unverified) | Free (catalog) | point / polygon | hazards | JetStream | `intel.fire.nifc.perimeter` | 200 reachable |
| F094 | NOAA NWS Alerts | candidate | P0 | rest_poll | omos polls 300 s (omos/platform/omos/intel-scripts.yaml) | User-Agent required (weather.gov services-web-api) | Free (catalog) | point / polygon | hazards | JetStream | `intel.weather.noaa.alert` | 200 reachable |
| F095 | NOAA SWPC Space Weather | candidate | P2 | rest_poll | 1-5 min | none (catalog, unverified) | Free (catalog) | none | space | JetStream | `intel.space.swpc.index` | 200 reachable |
| F096 | NOAA Weather | candidate | P1 | rest_poll | hourly forecasts | User-Agent required | Gov/Open (catalog) | point / polygon | weather | KV latest state | `intel.weather.noaa.forecast` | 200 reachable |
| F097 | Nominatim (OSM Geocoding) | hosted-service | P2 | rest_poll | on demand | none (catalog, unverified) | Free (catalog) | point / line / polygon | reference | not ingested | `svc.geo.geocode` | 200 reachable |
| F098 | North Star — Camera Globe | commercial-reference | - | none | n/a | api_key (catalog, unverified) | Paid (catalog) | none | none | not ingested | — | not tested: paid (decision 0004 #19) |
| F099 | North Star — Crypto Tracing | excluded | - | none | n/a | api_key (catalog, unverified) | Paid (catalog) | none | none | not ingested | — | not tested: paid (decision 0004 #19) |
| F100 | North Star — NetScan | commercial-reference | - | none | n/a | api_key (catalog, unverified) | Paid (catalog) | none | none | not ingested | — | not tested: paid (decision 0004 #19) |
| F101 | North Star — Platform (nstarlive.com) | commercial-reference | - | none | n/a | api_key (catalog, unverified) | Paid (catalog) | none | none | not ingested | — | not tested: paid (decision 0004 #19) |
| F102 | North Star — Username / Identity OSINT | excluded | - | none | n/a | api_key (catalog, unverified) | Paid (catalog) | none | none | not ingested | — | not tested: paid (decision 0004 #19) |
| F103 | North Star — Watchtower (Global SA Globe) | commercial-reference | - | none | n/a | api_key (catalog, unverified) | Paid (catalog) | none | none | not ingested | — | not tested: paid (decision 0004 #19) |
| F104 | NRC Event Notifications | candidate | P3 | scrape | daily (business days) | none (catalog, unverified) | Free (catalog) | point / polygon | hazards | JetStream | `intel.nuclear.nrc.event` | 301 moved → https://www.nrc.gov/reading-rm/doc-collections/event-status/event/index.html |
| F105 | NWS CAP Alerts | candidate | P2 | rss | 2-5 min | none (catalog, unverified) | Free (catalog) | point / polygon | hazards | JetStream | `intel.weather.noaa.alert` | 000 network error |
| F106 | NYC 511 (Traffic) | candidate | P2 | rest_poll | unverified | api_key (catalog, unverified) | Free (catalog) | point / polygon | road-traffic | JetStream | `intel.transit.511ny.event` | not tested: needs API key / account (rule: never call keyed APIs) |
| F107 | ONVIF | standard | P1 | device | stream | basic (catalog, unverified) | Free (catalog) | none | cameras | MediaMTX | `ent.camera.{id}.state` | not tested: not an endpoint (standard, tool or reference) |
| F108 | OpenAQ | candidate | P2 | rest_poll | unverified | api_key (catalog, unverified) | Free (catalog) | point / polygon | environment | JetStream | `intel.environment.openaq.reading` | not tested: needs API key / account (rule: never call keyed APIs) |
| F109 | OpenMHZ (Scanner Feeds) | website-only | P3 | audio | unverified | none (catalog, unverified) | Free (catalog) | point (camera/receiver) + stream | rf | not ingested | — | 403 forbidden (auth or bot protection) |
| F110 | OpenSanctions | candidate | P3 | bulk_download | daily | none (catalog, unverified) | Free (catalog) | none | reference | Hermes knowledge store | — | 200 reachable |
| F111 | OpenSeaMap | candidate | P2 | tiles_xyz | unverified | none (catalog, unverified) | Free (catalog) | point / line / polygon | charts-nautical | caching tile proxy (never NATS) | — | 200 reachable |
| F112 | OpenSky Network | candidate | P0 | rest_poll | 10 s anonymous / 5 s authenticated | anonymous allowed; authenticated = OAuth2 client credentials only (opensky-api rest.html) | Free (catalog) | point (moving) | air | JetStream + KV latest state | `intel.air.opensky.position ; ent.aircraft.{icao}.state -> KV world-state` | 200 reachable |
| F113 | OpenWeatherMap | candidate | P3 | rest_poll | unverified | api_key (catalog, unverified) | Freemium (catalog) | point / polygon | weather | KV latest state | `intel.weather.owm.observation` | not tested: needs API key / account (rule: never call keyed APIs) |
| F114 | OpenWeatherMap Tiles | candidate | P3 | tiles_xyz | unverified | api_key (catalog, unverified) | Freemium (catalog) | raster or vector tiles | weather | caching tile proxy (never NATS) | — | not tested: needs API key / account (rule: never call keyed APIs) |
| F115 | Overpass API (OSM) | hosted-service | P2 | rest_poll | on demand | none (catalog, unverified) | Free (catalog) | point / line / polygon | overlays | Object Store / PMTiles | `svc.geo.overpass` | 000 network error |
| F116 | PeeringDB | candidate | P3 | rest_poll | daily | none (catalog, unverified) | Free (catalog) | point / line / polygon | infra | Object Store / PMTiles | — | 200 reachable |
| F117 | PulsePoint (Fire/EMS CAD Dispatch) | excluded | - | none | n/a | api_key (catalog, unverified) | Free (catalog) | none | hazards | not ingested | — | 200 reachable |
| F118 | PyTAK / ADSBCOT / AISCOT | tool | P2 | none | n/a | certificate (catalog, unverified) | Free (catalog) | none | reference | not ingested | — | not tested: not an endpoint (standard, tool or reference) |
| F119 | RadiationNetwork.com | website-only | P3 | none | n/a | none (catalog, unverified) | Free (catalog) | none | hazards | not ingested | — | 200 reachable |
| F120 | Reddit (Social OSINT) | candidate | P3 | rest_poll | unverified | oauth2 (catalog, unverified) | Free (catalog) | none | news | JetStream | `intel.social.reddit.post` | not tested: OAuth2 required |
| F121 | ReliefWeb (UN OCHA) | candidate | P2 | rest_poll | hourly | appname parameter (approval may be required - unverified) | Free (catalog) | point / polygon | humanitarian | Hermes knowledge store | `intel.humanitarian.reliefweb.report` | 403 forbidden (auth or bot protection) |
| F122 | Remote ID (ASTM F3411) | edge-device | P2 | device | 1 Hz broadcast | none (catalog, unverified) | Free (catalog) | point (moving) | air | edge NATS leaf -> JetStream | `det.air.remoteid.{id} ; ent.drone.{id}.state` | not tested: not an endpoint (standard, tool or reference) |
| F123 | ROS 2 / DDS | edge-device | P1 | device | topic rate | none (catalog, unverified) | Free (catalog) | point (moving) | fleet | edge NATS leaf -> JetStream | `tel.robot.{id}.{metric} ; ent.robot.{id}.state; KV fleet-state` | not tested: not an endpoint (standard, tool or reference) |
| F124 | SAE J2735 / V2X | standard | P3 | device | unverified | none (catalog, unverified) | Free (catalog) | point (moving) | road-traffic | edge NATS leaf -> JetStream | `tel.{node}.v2x.{metric}` | not tested: not an endpoint (standard, tool or reference); SAE standard is paywalled |
| F125 | Safecast (Radiation) | candidate | P2 | rest_poll | hourly | none (catalog, unverified) | Free (catalog) | point / polygon | hazards | JetStream | `intel.radiation.safecast.reading` | 200 reachable |
| F126 | SAM.gov (Federal Contracts) | candidate | P3 | rest_poll | unverified | api_key (catalog, unverified) | Free (catalog) | none | reference | Hermes knowledge store | `intel.gov.sam.notice` | not tested: needs API key / account (rule: never call keyed APIs) |
| F127 | SATnogs | candidate | P3 | rest_poll | unverified | api_key (catalog, unverified) | Free (catalog) | none | space | JetStream | `intel.space.satnogs.observation` | not tested: needs API key / account (rule: never call keyed APIs) |
| F128 | Serial Port | edge-device | P3 | device | unverified | none (catalog, unverified) | Free (catalog) | none | fleet | edge NATS leaf -> JetStream | `tel.{node}.serial.{metric}` | not tested: not an endpoint (standard, tool or reference) |
| F129 | Shadowserver Foundation | website-only | P3 | rest_poll | unverified | api_key (catalog, unverified) | Free (catalog) | none | cyber | Hermes knowledge store | — | not tested: needs API key / account (rule: never call keyed APIs) |
| F130 | Shodan InternetDB | candidate | P3 | rest_poll | on demand | none (catalog, unverified) | Free (catalog) | none | cyber | Hermes knowledge store | `svc.cyber.internetdb` | 200 reachable |
| F131 | SkyWatch EarthCache | candidate | P3 | rest_poll | unverified | api_key (catalog, unverified) | Freemium (catalog) | raster or vector tiles | imagery | Object Store / PMTiles | — | not tested: needs API key / account (rule: never call keyed APIs); paid tasking |
| F132 | Smithsonian GVP Volcanoes | website-only | P2 | rss | weekly report | none (catalog, unverified) | Free (catalog) | point / polygon | hazards | JetStream | `intel.natural.gvp.report` | 403 forbidden (auth or bot protection) |
| F133 | SpaceWeatherLive | candidate | P3 | rest_poll | unverified | none (catalog, unverified) | Free (catalog) | none | space | JetStream | `intel.space.spaceweatherlive.index` | 301 moved → https://www.spaceweatherlive.com/en/api.html |
| F134 | STANAG 4586 (UAS Control) | excluded | - | none | n/a | certificate (catalog, unverified) | Gov/Open (catalog) | none | none | not ingested | — | not tested: not an endpoint (standard, tool or reference); NATO standard, not adopted (MAVLink used) |
| F135 | Telegram OSINT Channels | candidate | P3 | websocket | unverified | api_key (catalog, unverified) | Free (catalog) | none | news | JetStream | `intel.social.telegram.post` | not tested: MTProto api_id/api_hash account required |
| F136 | TfL (Transport for London) | candidate | P3 | rest_poll | unverified | api_key (catalog, unverified) | Free (catalog) | none | rail-transit | JetStream | `intel.transit.tfl.status` | not tested: needs API key / account (rule: never call keyed APIs) |
| F137 | Trunk Recorder (P25/SmartNet Radio) | edge-device | P3 | audio | continuous (local SDR) | none (catalog, unverified) | Free (catalog) | point (camera/receiver) + stream | rf | MediaMTX | `intel.rf.trunk.call` | not tested: not an endpoint (standard, tool or reference) |
| F138 | Tzeva Adom (Red Alert IL) | candidate | P2 | rest_poll | 5 s during alerts (unverified) | none (catalog, unverified) | Free (catalog) | point / polygon | conflict | JetStream | `intel.conflict.tzevaadom.alert` | 200 reachable |
| F139 | UCDP (Uppsala Conflict Data) | candidate | P2 | rest_poll | monthly (candidate events) | access token may now be required (unverified) | Free (catalog) | point / polygon | conflict | JetStream | `intel.conflict.ucdp.event` | 401 auth required |
| F140 | UKMTO (UK Maritime Trade Ops) | website-only | P2 | scrape | hourly | none (catalog, unverified) | Free (catalog) | point / polygon | sea | JetStream | `intel.sea.ukmto.incident` | 403 forbidden (auth or bot protection) |
| F141 | UN Comtrade | website-only | P3 | rest_poll | unverified | api_key (catalog, unverified) | Freemium (catalog) | none | finance | Hermes knowledge store | — | not tested: needs API key / account (rule: never call keyed APIs) |
| F142 | UNHCR Population Data | candidate | P3 | rest_poll | yearly | none (catalog, unverified) | Free (catalog) | none | humanitarian | Hermes knowledge store | — | 200 reachable |
| F143 | US Treasury FiscalData | candidate | P3 | rest_poll | daily | none (catalog, unverified) | Free (catalog) | none | finance | JetStream | `intel.finance.treasury.series` | 200 reachable |
| F144 | USAspending.gov | candidate | P3 | rest_poll | daily | none (catalog, unverified) | Free (catalog) | none | reference | Hermes knowledge store | — | 200 reachable |
| F145 | USDA NASS Yields | candidate | P3 | rest_poll | unverified | api_key (catalog, unverified) | Gov/Open (catalog) | none | reference | Hermes knowledge store | — | not tested: needs API key / account (rule: never call keyed APIs) |
| F146 | USDA PLANTS DB | candidate | P3 | bulk_download | rare | none (catalog, unverified) | Gov/Open (catalog) | none | reference | Hermes knowledge store | — | 301 moved → https://plants.sc.egov.usda.gov:443/downloads |
| F147 | USFS Fire Detection | candidate | P3 | rest_poll | unverified | none (catalog, unverified) | Free (catalog) | point / polygon | hazards | JetStream | `intel.fire.usfs.detection` | 200 reachable |
| F148 | USGS Earthquakes | candidate | P0 | rest_poll | feeds update every minute (USGS geojson.php); omos polls 60 s | none (catalog, unverified) | Free (catalog) | point / polygon | hazards | JetStream | `intel.seismic.usgs.event` | 200 reachable |
| F149 | USGS NWIS (Water) | candidate | P2 | rest_poll | 15 min (instantaneous values) | none (catalog, unverified) | Free (catalog) | point / polygon | environment | JetStream | `intel.water.nwis.reading` | 200 reachable |
| F150 | USPTO Patents | identity-conflict | P3 | rest_poll | unverified | none (catalog, unverified) | Free (catalog) | none | reference | Hermes knowledge store | — | not tested: catalog link points to USAspending, not USPTO; endpoint unknown |
| F151 | WebSDR Network | website-only | P3 | audio | unverified | none (catalog, unverified) | Free (catalog) | point (camera/receiver) + stream | rf | not ingested | — | 200 reachable |
| F152 | Where the ISS at | candidate | P3 | rest_poll | 10 s | none (catalog, unverified) | Free (catalog) | point (moving) | space | JetStream + KV latest state | `intel.space.wheretheiss.position ; ent.satellite.25544.state` | 200 reachable |
| F153 | WHO Disease Outbreaks | candidate | P2 | rest_poll | daily | none (catalog, unverified) | Free (catalog) | point / polygon | health | JetStream | `intel.health.who.outbreak` | 200 reachable |
| F154 | Wikidata | candidate | P2 | rest_poll | on demand | none (catalog, unverified) | Free (catalog) | none | reference | Hermes knowledge store | `svc.knowledge.wikidata` | 200 reachable |
| F155 | Wikipedia REST API | candidate | P2 | rest_poll | on demand | none (catalog, unverified) | Free (catalog) | none | reference | Hermes knowledge store | `svc.knowledge.wikipedia` | 200 reachable |
| F156 | Windy API | candidate | P3 | rest_poll | unverified | api_key (catalog, unverified) | Freemium (catalog) | point / polygon | weather | KV latest state | — | not tested: needs API key / account (rule: never call keyed APIs) |
| F157 | WRI Global Power Plant DB | candidate | P2 | bulk_download | static (no recent release, unverified) | none (catalog, unverified) | Free (catalog) | point / line / polygon | infra | Object Store / PMTiles | — | 301 moved → https://datasets.wri.org/datasets/global-power-plant-database |
| F158 | WSDOT Traffic Cameras | candidate | P3 | rest_poll | unverified | Traveler API access code required (unverified) | Free (catalog) | point (camera/receiver) + stream | cameras | MediaMTX | `ent.camera.{id}.state` | 200 reachable |
| F159 | WWV (NIST Time) | reference-only | - | none | n/a | none (catalog, unverified) | Free (catalog) | none | reference | not ingested | — | not tested: not an endpoint (standard, tool or reference) (radio time service) |
| F160 | Yahoo Finance | website-only | - | none | n/a | none (catalog, unverified) | Free (catalog) | none | finance | not ingested | — | not tested: no official API; scraping against terms (unverified) |
| F161 | LiveUAMap | website-only | P3 | rest_poll | unverified | none (catalog, unverified) | Free (catalog) | point / polygon | conflict | JetStream | `intel.conflict.liveuamap.event` | see F079 |

### Map Layers

| Ref | Name | Status | Pri | Mode | Cadence | Auth | Cost | Geometry | Layer | Route | Subject / state | Probe |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| M001 | ACLED Crisis Map | website-only | - | none | n/a | api_key (catalog, unverified) | Free (catalog) | none | conflict | not ingested | — | not tested: needs API key / account (rule: never call keyed APIs) |
| M002 | AIS Vessel Overlay | website-only | - | none | n/a | none (catalog, unverified) | Free (catalog) | none | sea | not ingested | — | not tested: paid (decision 0004 #19) (MarineTraffic UI) |
| M003 | Amtrak Rail Network | candidate | P3 | bulk_download | rare | none (catalog, unverified) | Free (catalog) | point / line / polygon | rail-transit | Object Store / PMTiles | — | 301 moved → https://hub.arcgis.com/api/download/v1/items/baa5a6c4d4ae4034850e99aaca38cfbb/geojson?redirect=true&layers=0 |
| M004 | ArcGIS Online | hosted-service | P3 | rest_poll | unverified | none (catalog, unverified) | Free (catalog) | none | overlays | not ingested | — | 200 reachable |
| M005 | ArcGIS World Imagery | candidate | P1 | tiles_xyz | unverified | none (catalog, unverified) | Free (catalog) | raster or vector tiles | basemap | caching tile proxy (never NATS) | — | 200 reachable |
| M006 | Bing Maps | hosted-service | - | tiles_xyz | unverified | api_key (catalog, unverified) | Freemium (catalog) | none | basemap | caching tile proxy (never NATS) | — | not tested: needs API key / account (rule: never call keyed APIs) |
| M007 | Bing Satellite (XYZ) | excluded | - | tiles_xyz | unverified | none (catalog, unverified) | Free (catalog) | raster or vector tiles | basemap | not ingested | — | not tested: direct tile URL bypasses Bing Maps API keys/terms (unverified) |
| M008 | Cesium ion | hosted-service | P2 | tiles_3d | unverified | api_key (catalog, unverified) | Freemium/paid | raster or vector tiles | basemap | caching tile proxy (never NATS) | — | not tested: needs API key / account (rule: never call keyed APIs); optional behind keychain token (decision 0004 #4) |
| M009 | Cloudflare Radar Map | website-only | - | none | n/a | api_key (catalog, unverified) | Free (catalog) | none | cyber | not ingested | — | not tested: needs API key / account (rule: never call keyed APIs) |
| M010 | CloudTAK Tiles (PMTiles) | self-hosted | P0 | tiles_pmtiles | unverified | none (catalog, unverified) | Self-hosted (catalog) | raster or vector tiles | basemap | caching tile proxy (never NATS) | — | 000 network error |
| M011 | DCD Data Center Map | website-only | - | none | n/a | none (catalog, unverified) | Free (catalog) | none | infra | not ingested | — | 403 forbidden (auth or bot protection) |
| M012 | DeFlock — Crowdsourced ALPR / License Plate Reader Map | website-only | P3 | bulk_download | weekly | none (catalog, unverified) | Free (catalog) | point / line / polygon | infra | Object Store / PMTiles | — | 200 reachable |
| M013 | EIA Power Plant Locations | website-only | P3 | none | n/a | none (catalog, unverified) | Free (catalog) | point / line / polygon | infra | Object Store / PMTiles | — | 301 moved → https://atlas.eia.gov/search |
| M014 | EPA RadNet Map | website-only | - | none | n/a | none (catalog, unverified) | Free (catalog) | none | hazards | not ingested | — | see F041 |
| M015 | EPA Water Quality Map | candidate | P3 | rest_poll | rare | none (catalog, unverified) | Free (catalog) | point / polygon | environment | Object Store / PMTiles | — | 200 reachable |
| M016 | Esri Imagery (World) | candidate | P1 | tiles_xyz | unverified | none (catalog, unverified) | Free (catalog) | raster or vector tiles | basemap | caching tile proxy (never NATS) | — | see M005 |
| M017 | Esri World Street Map | candidate | P2 | tiles_xyz | unverified | none (catalog, unverified) | Free (catalog) | raster or vector tiles | basemap | caching tile proxy (never NATS) | — | 200 reachable |
| M018 | Esri World Topo | candidate | P2 | tiles_xyz | unverified | none (catalog, unverified) | Free (catalog) | raster or vector tiles | basemap | caching tile proxy (never NATS) | — | 200 reachable |
| M019 | FAA Sectional Charts | website-only | P2 | tiles_xyz | unverified | none (catalog, unverified) | Free (catalog) | raster or vector tiles | aviation-airspace | caching tile proxy (never NATS) | — | 200 reachable |
| M020 | FAA TFR Overlay | website-only | P1 | scrape | 15 min | none (catalog, unverified) | Free (catalog) | point / polygon | aviation-airspace | JetStream | `intel.air.faa.tfr` | 302 moved → https://tfr.faa.gov/tfr3/ |
| M021 | FEMA Flood Zones | candidate | P2 | tiles_wms | rare | none (catalog, unverified) | Free (catalog) | point / line / polygon | hazards | caching tile proxy (never NATS) | — | 200 reachable |
| M022 | FlightAware Map | website-only | - | none | n/a | api_key (catalog, unverified) | Freemium (catalog) | none | air | not ingested | — | not tested: needs API key / account (rule: never call keyed APIs) |
| M023 | Flock Safety Camera Map | excluded | - | none | n/a | api_key (catalog, unverified) | Paid (catalog) | none | none | not ingested | — | not tested: paid (decision 0004 #19) |
| M024 | GeoServer (OGC) | tool | P3 | none | n/a | none (catalog, unverified) | Free (catalog) | raster or vector tiles | overlays | caching tile proxy (never NATS) | — | not tested: not an endpoint (standard, tool or reference) |
| M025 | Google Maps | hosted-service | - | none | n/a | api_key (catalog, unverified) | Freemium (catalog) | none | basemap | not ingested | — | not tested: needs API key / account (rule: never call keyed APIs) |
| M026 | Google Satellite (XYZ) | excluded | - | tiles_xyz | unverified | none (catalog, unverified) | Free (catalog) | raster or vector tiles | basemap | not ingested | — | not tested: Google allows imagery only through Maps Platform APIs (archived FAQ; current terms unverified); Photorealistic tiles off (decision 0004 #4) |
| M027 | Heavens Above (Sat Tracks) | website-only | - | none | n/a | none (catalog, unverified) | Free (catalog) | none | space | not ingested | — | 200 reachable |
| M028 | HIFLD EMS Stations | candidate | P1 | rest_poll | rare | none (catalog, unverified) | Free (catalog) | point / line / polygon | infra | Object Store / PMTiles | — | 200 reachable |
| M029 | HIFLD Fire Stations | candidate | P1 | rest_poll | rare | none (catalog, unverified) | Free (catalog) | point / line / polygon | infra | Object Store / PMTiles | — | 200 reachable |
| M030 | HIFLD Hospitals | candidate | P1 | rest_poll | rare | none (catalog, unverified) | Free (catalog) | point / line / polygon | infra | Object Store / PMTiles | — | 200 reachable |
| M031 | HIFLD Law Enforcement | candidate | P2 | rest_poll | rare | none (catalog, unverified) | Free (catalog) | point / line / polygon | infra | Object Store / PMTiles | — | 200 reachable |
| M032 | IAEA PRIS (Global Reactors) | website-only | P3 | none | n/a | none (catalog, unverified) | Free (catalog) | point / line / polygon | infra | Object Store / PMTiles | — | 302 moved → https://pris-stats.iaea.org/ |
| M033 | IODA Internet Outage Map | website-only | - | none | n/a | none (catalog, unverified) | Free (catalog) | none | cyber | not ingested | — | see F073 |
| M034 | KiwiSDR Receiver Map | website-only | - | none | n/a | none (catalog, unverified) | Free (catalog) | none | rf | not ingested | — | 200 reachable |
| M035 | LiveUAMap Overlay | website-only | - | none | n/a | api_key (catalog, unverified) | Freemium (catalog) | none | conflict | not ingested | — | see F079 |
| M036 | Mapbox | hosted-service | - | tiles_xyz | unverified | api_key (catalog, unverified) | Freemium (catalog) | none | basemap | caching tile proxy (never NATS) | — | not tested: needs API key / account (rule: never call keyed APIs) |
| M037 | MapServer (OGC) | tool | P3 | none | n/a | none (catalog, unverified) | Free (catalog) | raster or vector tiles | overlays | caching tile proxy (never NATS) | — | not tested: not an endpoint (standard, tool or reference) |
| M038 | Maputnik (Style Editor) | self-hosted | P3 | none | n/a | none (catalog, unverified) | Self-hosted (catalog) | none | basemap | not ingested | — | not tested: tailnet/cluster-internal host, unreachable from build machine (PREFLIGHT.md Access) |
| M039 | Martin (Vector Tiles) | self-hosted | P1 | tiles_mvt | unverified | none (catalog, unverified) | Self-hosted (catalog) | raster or vector tiles | overlays | caching tile proxy (never NATS) | — | not tested: tailnet/cluster-internal host, unreachable from build machine (PREFLIGHT.md Access) |
| M040 | MeshCore | edge-device | P3 | device | unverified | none (catalog, unverified) | Free (catalog) | point (moving) | fleet | edge NATS leaf -> JetStream | `tel.{node}.mesh.{metric}` | not tested: not an endpoint (standard, tool or reference) |
| M041 | Meshtastic | edge-device | P2 | mqtt | event-driven | none (catalog, unverified) | Free (catalog) | point (moving) | fleet | edge NATS leaf -> JetStream | `tel.{node}.mesh.{metric} ; ent.station.{nodeid}.state` | not tested: not an endpoint (standard, tool or reference) |
| M042 | MTA Map (NYC Transit) | website-only | - | none | n/a | none (catalog, unverified) | Free (catalog) | none | rail-transit | not ingested | — | 000 network error |
| M043 | NASA Eyes (3D Space) | website-only | - | none | n/a | none (catalog, unverified) | Free (catalog) | none | space | not ingested | — | 301 moved → https://science.nasa.gov/eyes/ |
| M044 | NASA FIRMS Fire Overlay | candidate | P2 | tiles_wms | unverified | none (catalog, unverified) | Free (catalog) | point / polygon | hazards | caching tile proxy (never NATS) | — | not tested: FIRMS map services need a MAP_KEY (FIRMS map_key page) |
| M045 | NIFC Fire Perimeters (Map) | candidate | P1 | rest_poll | unverified | none (catalog, unverified) | Free (catalog) | point / polygon | hazards | JetStream | `intel.fire.nifc.perimeter` | see F093 |
| M046 | NJ Transit Rail | catalog | P3 | bulk_download | unverified | developer registration for GTFS (unverified) | Free (catalog) | point / line / polygon | rail-transit | Object Store / PMTiles | — | 302 moved → https://developer.njtransit.com/registration/login |
| M047 | NOAA ENC (Nautical Charts) | candidate | P2 | tiles_wms | unverified | none (catalog, unverified) | Free (catalog) | raster or vector tiles | charts-nautical | caching tile proxy (never NATS) | — | 200 reachable |
| M048 | Nominatim (Geocoding) | self-hosted | P2 | rest_poll | unverified | none (catalog, unverified) | Self-hosted (catalog) | point / line / polygon | reference | not ingested | `svc.geo.geocode` | not tested: tailnet/cluster-internal host, unreachable from build machine (PREFLIGHT.md Access) |
| M049 | North Star — Camera Globe (250k Cameras) | commercial-reference | - | none | n/a | api_key (catalog, unverified) | Paid (catalog) | none | none | not ingested | — | not tested: paid (decision 0004 #19) |
| M050 | North Star — Crypto Tracing Map | excluded | - | none | n/a | api_key (catalog, unverified) | Paid (catalog) | none | none | not ingested | — | not tested: paid (decision 0004 #19) |
| M051 | North Star — Identity OSINT Layer | excluded | - | none | n/a | api_key (catalog, unverified) | Paid (catalog) | none | none | not ingested | — | not tested: paid (decision 0004 #19) |
| M052 | North Star — NetScan (Network Recon) | commercial-reference | - | none | n/a | api_key (catalog, unverified) | Paid (catalog) | none | none | not ingested | — | not tested: paid (decision 0004 #19) |
| M053 | North Star — Platform Overview | commercial-reference | - | none | n/a | api_key (catalog, unverified) | Paid (catalog) | none | none | not ingested | — | not tested: paid (decision 0004 #19) |
| M054 | North Star — Watchtower Globe | commercial-reference | - | none | n/a | api_key (catalog, unverified) | Paid (catalog) | none | none | not ingested | — | not tested: paid (decision 0004 #19) |
| M055 | NRC Nuclear Reactor Map | website-only | P3 | none | n/a | none (catalog, unverified) | Free (catalog) | point / line / polygon | infra | Object Store / PMTiles | — | 301 moved → https://www.nrc.gov/facilities-safety/operating-reactors/map-power-reactors |
| M056 | NWS Radar (MRMS) | candidate | P1 | tiles_wms | 2 min (MRMS) - unverified | none (catalog, unverified) | Free (catalog) | raster or vector tiles | weather | caching tile proxy (never NATS) | — | 200 reachable |
| M057 | NYC Subway Lines | candidate | P3 | bulk_download | rare | none (catalog, unverified) | Free (catalog) | point / line / polygon | rail-transit | Object Store / PMTiles | — | 400 bad request |
| M058 | OpenAIP (Aviation) | candidate | P2 | rest_poll | unverified | api_key (catalog, unverified) | Free (catalog) | point / line / polygon | aviation-airspace | Object Store / PMTiles | — | not tested: needs API key / account (rule: never call keyed APIs) |
| M059 | OpenCPN | tool | - | none | n/a | none (catalog, unverified) | Free (catalog) | none | charts-nautical | not ingested | — | not tested: not an endpoint (standard, tool or reference) |
| M060 | OpenSeaMap (Nautical Charts) | candidate | P2 | tiles_xyz | unverified | none (catalog, unverified) | Free (catalog) | raster or vector tiles | charts-nautical | caching tile proxy (never NATS) | — | 200 reachable |
| M061 | OpenSky Network (Map) | website-only | - | none | n/a | none (catalog, unverified) | Free (catalog) | none | air | not ingested | — | see F112 |
| M062 | OpenStreetMap (Tile CDN) | candidate | P1 | tiles_xyz | unverified | none (catalog, unverified) | Free (catalog) | raster or vector tiles | basemap | caching tile proxy (never NATS) | — | 200 reachable |
| M063 | OpenWeatherMap Tiles | candidate | P3 | tiles_xyz | unverified | api_key (catalog, unverified) | Freemium (catalog) | raster or vector tiles | weather | caching tile proxy (never NATS) | — | not tested: needs API key / account (rule: never call keyed APIs) |
| M064 | Overpass API | self-hosted | P2 | rest_poll | unverified | none (catalog, unverified) | Self-hosted (catalog) | point / line / polygon | overlays | Object Store / PMTiles | `svc.geo.overpass` | not tested: tailnet/cluster-internal host, unreachable from build machine (PREFLIGHT.md Access) |
| M065 | PeeringDB Data Center Map | candidate | P3 | rest_poll | unverified | none (catalog, unverified) | Free (catalog) | point / line / polygon | infra | Object Store / PMTiles | — | see F116 |
| M066 | pg_featureserv | self-hosted | P2 | rest_poll | unverified | none (catalog, unverified) | Self-hosted (catalog) | point / line / polygon | overlays | Object Store / PMTiles | — | not tested: tailnet/cluster-internal host, unreachable from build machine (PREFLIGHT.md Access) |
| M067 | PMTiles (Format/Spec) | standard | P0 | none | n/a | none (catalog, unverified) | Free (catalog) | raster or vector tiles | basemap | Object Store / PMTiles | — | not tested: not an endpoint (standard, tool or reference) |
| M068 | Protomaps | hosted-service | P1 | tiles_pmtiles | unverified | none (catalog, unverified) | Free (catalog) | raster or vector tiles | basemap | Object Store / PMTiles | — | 200 reachable |
| M069 | QGIS Desktop | tool | - | none | n/a | none (catalog, unverified) | Free (catalog) | none | overlays | not ingested | — | not tested: not an endpoint (standard, tool or reference) |
| M070 | QGIS Server (WMS/WFS) | self-hosted | P3 | tiles_wms | unverified | none (catalog, unverified) | Self-hosted (catalog) | raster or vector tiles | overlays | caching tile proxy (never NATS) | — | not tested: tailnet/cluster-internal host, unreachable from build machine (PREFLIGHT.md Access) |
| M071 | Safecast Radiation Map | website-only | - | none | n/a | none (catalog, unverified) | Free (catalog) | none | hazards | not ingested | — | 302 moved → https://map.safecast.org/ |
| M072 | SafeComLink HF Map | website-only | - | none | n/a | none (catalog, unverified) | Free (catalog) | none | rf | not ingested | — | see F065 |
| M073 | TileServer GL | self-hosted | P1 | tiles_mvt | unverified | none (catalog, unverified) | Self-hosted (catalog) | raster or vector tiles | basemap | caching tile proxy (never NATS) | — | not tested: tailnet/cluster-internal host, unreachable from build machine (PREFLIGHT.md Access) |
| M074 | USGS Water Resources Map | website-only | - | none | n/a | none (catalog, unverified) | Free (catalog) | none | environment | not ingested | — | 301 moved → http://waterdata.usgs.gov/ |
| M075 | WFS (OGC Standard) | standard | P2 | none | n/a | none (catalog, unverified) | Free (catalog) | none | overlays | not ingested | — | not tested: not an endpoint (standard, tool or reference) |
| M076 | Windy Embed | website-only | - | none | n/a | api_key (catalog, unverified) | Freemium (catalog) | none | weather | not ingested | — | not tested: needs API key / account (rule: never call keyed APIs) (embed) |
| M077 | WMS (OGC Standard) | standard | P1 | none | n/a | none (catalog, unverified) | Free (catalog) | raster or vector tiles | overlays | caching tile proxy (never NATS) | — | not tested: not an endpoint (standard, tool or reference) |
| M078 | WMTS (OGC Standard) | standard | P1 | none | n/a | none (catalog, unverified) | Free (catalog) | raster or vector tiles | overlays | caching tile proxy (never NATS) | — | not tested: not an endpoint (standard, tool or reference) |
| M079 | WRI Global Power Plant DB | candidate | P2 | bulk_download | unverified | none (catalog, unverified) | Free (catalog) | point / line / polygon | infra | Object Store / PMTiles | — | see F157 |
