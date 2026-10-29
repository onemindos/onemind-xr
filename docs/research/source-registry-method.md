# Source registry: method and findings (M0, ingestion lane parts a and b)

Lane: sources-registry. Date: 2026-10-29. Outputs:
[`docs/sources/SOURCE-REGISTRY.md`](../sources/SOURCE-REGISTRY.md) (human view) and
[`docs/sources/sources.yaml`](../sources/sources.yaml) (machine-readable, 240 records).
Part c (choosing the ingestion platform) belongs to another lane. This document gives it the
delivery-mode mix it has to cover.

## Sources

Local files (read in full or in the cited ranges):

- `docs/sources/map-sources/Audit Reports/source-catalog-240-records.csv`: 240 rows, 161 Fabric
  Feeds and 79 Map Layers. Columns used: `record_role`, `primary_delivery`, `endpoint_evidence`,
  `source_url_exact`, `historical_auth`, `historical_access`, `declared_nats_pattern`.
- `docs/sources/map-sources/Audit Reports/README.md`: the 2026-10-28 audit. It says no endpoint
  was tested and that historical "Live" labels are not health checks.
- `omos/CLAUDE.md` § NATS Subject Grammar (locked), and `omos/docs/NATS-DOCTRINE.md` §9 (lines 170–367).
- `omos/platform/nats/streams/*.yaml`: the `INTEL` stream is `intel.>` with maxAge 2160h. KV
  `world-state` has history 1 and TTL 5m. KV `fleet-state` has history 3 and TTL 30s.
- `omos/platform/omos/intel-scripts.yaml` lines 284–1782: the subjects and polls of the 7 live intel
  adapters. `omos/platform/data/redpanda-connect/deployment.yaml` lines 16, 38, 129, 153, 223 and 284:
  the live Connect pipelines.
- `omos/services/schemas/registry/source.schema.json`: the existing omos registry record. Its
  `kind` enum is reused in `sources.yaml`.
- `docs/research/omos-live-state.md` (other lane): live subjects and the INTEL message counts.
- `docs/decisions/0004-owner-defaults-for-the-run.md` #4, #8 and #19, and `ULTRACODE-PROMPT.md` lines 223–281 and 318.

Primary web sources fetched on 2026-10-29:

| Source | URL |
|---|---|
| OSMF Tile Usage Policy | https://operations.osmfoundation.org/policies/tiles/ |
| OSMF Nominatim Usage Policy | https://operations.osmfoundation.org/policies/nominatim/ |
| NWS API docs | https://www.weather.gov/documentation/services-web-api |
| OpenSky REST API | https://openskynetwork.github.io/opensky-api/rest.html |
| NASA FIRMS API and MAP_KEY | https://firms.modaps.eosdis.nasa.gov/api/ , https://firms.modaps.eosdis.nasa.gov/api/map_key/ |
| CelesTrak GP data formats | https://celestrak.org/NORAD/documentation/gp-data-formats.php |
| Overpass API wiki | https://wiki.openstreetmap.org/wiki/Overpass_API |
| adsb.lol API README | https://github.com/adsblol/api |
| USGS GeoJSON feeds | https://earthquake.usgs.gov/earthquakes/feed/v1.0/geojson.php |
| CARTO basemap styles | https://github.com/CartoDB/basemap-styles |
| NASA GIBS access basics | https://nasa-gibs.github.io/gibs-api-docs/access-basics/ |
| Esri World Imagery service | https://services.arcgisonline.com/arcgis/rest/services/World_Imagery/MapServer |
| GDELT 2.0 announcement | https://blog.gdeltproject.org/gdelt-2-0-our-global-world-in-realtime/ |
| ACLED API getting started | https://acleddata.com/api-documentation/getting-started |
| airplanes.live API guide | https://airplanes.live/api-guide (403 to fetch; facts from the search-engine excerpt of that page) |
| aisstream.io docs | https://aisstream.io/documentation (403 to fetch; facts from the search-engine excerpt) |

Endpoint probes: `docs/sources/sources.yaml` → `test` per record. The command, run once per unique
URL at 2026-10-29T08:20:29Z without following redirects, was:
`curl -sS -m 15 -o /dev/null -A 'OneMind-source-registry/0.1 (+https://github.com/onemindos/onemind-xr; one-time reachability check)' -w '%{http_code} %{time_total} %{redirect_url}' <url>`.

## Verified facts

**Catalog shape** (CSV, counted by script): 240 records. Roles: 129 source/dataset candidates, 50
websites or dashboards, 15 standards, 15 hosted services, 12 commercial references, 11 tools and
8 catalogs. Historical auth: 164 none, 67 API key, 5 certificate, 3 OAuth2 and 1 basic. Access:
182 free, 26 freemium, 15 paid, 9 Gov/Open and 8 self-hosted. 149 records declare a historical
pattern (127 `fabric.intel.*`). None of these patterns is in the locked grammar.

**Registry result** (240 records; `sources.yaml`, counts recomputed by the generator):

| Status | n | | Route | n |
|---|---:|---|---|---:|
| candidate | 123 | | JetStream | 61 |
| website-only | 48 | | JetStream + KV state | 15 |
| excluded (policy, military or terms) | 11 | | KV only | 3 |
| edge-device | 9 | | Object Store / PMTiles | 34 |
| hosted-service | 8 | | Tile proxy | 28 |
| commercial-reference | 8 | | MediaMTX | 5 |
| self-hosted | 8 | | Hermes knowledge | 21 |
| catalog | 7 | | Edge leaf → JetStream | 9 |
| standard | 7 | | Not ingested | 64 |
| tool | 6 | | | |
| reference-only | 4 | | | |
| identity-conflict | 1 | | | |

Delivery modes: REST polling 100; none (not a source) 60; bulk download 16; XYZ tiles 16;
device protocol 9; audio 6; scraping 6; RSS 5; WMS 5; WebSocket 4; video 3; raw TCP stream
(APRS-IS) 2; PMTiles 2; MVT 2; message queue (FAA SWIM) 1; WMTS 1; 3D Tiles 1; MQTT 1. **No
catalog source needs webhook push or SSE.**

Priority (value to OneMind): P0 13, P1 32, P2 51, P3 101, not used 43.

**Probe outcomes** (each record lands in exactly one row):

| Outcome | Records |
|---|---:|
| Probed: HTTP 200 | 87 |
| Probed: 3xx moved (redirect target recorded) | 18 |
| Probed: 403 (auth or bot protection) | 10 |
| Probed: network error (DNS or connect) | 6 |
| Probed: 401, 404, 429, 400 (one each) | 4 |
| Shares another record's endpoint (duplicate) | 12 |
| Not probed: key, OAuth2 or account needed | 48 |
| Not probed: paid | 17 |
| Not probed: not an endpoint (standard, tool or reference) | 27 |
| Not probed: tailnet or cluster-internal host | 7 |
| Not probed: terms forbid direct access (Google and Bing XYZ, Yahoo Finance) | 3 |
| Not probed: catalog link is wrong (F150 USPTO points at USAspending) | 1 |

125 unique URLs were requested, one request each. No paid or keyed API was called.

**Corrections to the catalog, proven by primary sources or probes:**

- **ACLED (F005)** now uses OAuth2: myACLED credentials go to `https://acleddata.com/oauth/token`, which issues a 24 h token (ACLED docs). The catalog says "API Key".
- **OpenSky (F112)** accepts authentication only by OAuth2 client credentials. Basic auth is no longer accepted. Anonymous use gets 400 credits/day at 10 s resolution (rest.html). omos `feed-aircraft` injects `OPENSKY_USERNAME/PASSWORD` as env vars (`deployment.yaml:546–556`, comment at line 16), but its `http` processor sends no credentials (`deployment.yaml:283–288`: URL, verb, timeout and User-Agent only), so it already polls anonymously (400 credits/day, 10 s). The vars are unused; OAuth2 is needed only for the 4,000/day standard tier (rest.html). *(Verifier correction 2026-10-29: the original said the authenticated mode "cannot work as written", implying the pipeline uses basic auth; it does not.)*
- **CARTO basemaps (F028)** now require an API key. Non-commercial use is free up to 5M tiles/month and commercial use up to 1M (CartoDB/basemap-styles README). The catalog says keyless.
- **NASA FIRMS (F090, M044)**: a MAP_KEY is needed for both the API and the map services, with a limit of 5,000 transactions per 10 minutes (map_key page). The catalog marks M044 keyless.
- **UCDP (F139)** answered HTTP 401 to an unauthenticated request. The catalog says no auth.
- **ReliefWeb (F121)** answered 403 with an `appname`. The approval requirement is unverified.
- **Dead or moved links:** `scihub.copernicus.eu` (F034) refused the connection. `alerts.weather.gov` (F105) and `data.mta.info` (M042) do not resolve. `hifld-geoplatform.hub.arcgis.com` (F066) returned 404. The NYC subway GeoJSON export (M057) returned 400. TFR moved to `tfr.faa.gov/tfr3/` (M020), and INFORM moved to `drmkc.jrc.ec.europa.eu` (F072).
- **`tiles.atoc.onemindos.dev` (M010)**, OneMind's own PMTiles host, does not resolve in public DNS from this machine.

**Upstream politeness facts that bind the collectors:**

- **OSM tiles:** a distinct User-Agent; honour cache headers, or keep tiles at least 7 days; no bulk download, pre-seeding or offline use (OSMF tile policy).
- **Nominatim:** at most 1 req/s; 4 req/min for scheduled scripts; results must be cached (OSMF).
- **CelesTrak:** one download per 2-hour update; more than 50 HTTP errors in 2 hours gets the IP firewalled.
- **Overpass:** under 10,000 queries/day and 1 GB/day, divided by 100 for regular use; wait 30 s after a 429. The main instance timed out on the probe.
- **NWS:** a User-Agent is required. The rate limit is not public; retry after about 5 s. Data is "open data, free to use for any purpose".
- **airplanes.live:** 1 req/s; non-commercial; no SLA.
- **adsb.lol:** limits are dynamic; an API key is planned.
- **aisstream.io:** BETA, no SLA, throttled per key and per user (search-engine excerpt of aisstream.io/documentation; the page returns 403 to direct fetches). The same excerpt adds 3 subscribed connections per account and per IP, and at most 1 subscription update per second per connection. "Backend use only" is **unverified** (verifier 2026-10-29: not in the excerpt obtained).
- **CelesTrak (verifier addendum):** since 2026-03-26 a second download of the same data before the next 2-hour update returns HTTP 403 (gp-data-formats.php). A collector must not retry on that 403, because 403s count toward the 50-errors-in-2-hours firewall rule.
- **USGS feeds:** updated every minute. **GDELT 2.0:** every 15 minutes.

## How it applies to OneMind

**Grammar mapping.** Every record that produces bus traffic gets a subject from the locked block in `omos/CLAUDE.md`:

- `intel.{domain}.{source}.{type}` for observations and events from the outside world. This is the same form the seven live adapters already use, for example `intel.seismic.usgs…` and `intel.conflict.acled…` (`intel-scripts.yaml`). Domains used: air, sea, space, transit, weather, seismic, fire, disaster, natural, conflict, cyber, finance, energy, news, social, health, radiation, nuclear, water, environment, humanitarian, rf, imagery, gov. The `{source}` token is always one token. For example, live `intel.fire.nasa.firms` becomes `intel.fire.firms.detection`.
- `ent.{type}.{id}.state` for derived state of moving things (aircraft, vessel, satellite, train, station, drone and robot). The entity service (A4) writes it, not the collector, and the latest value lives in KV `world-state` or `fleet-state`.
- `tel.{node}.{class}.{metric}` for our own devices through the edge leaf (MAVLink, ROS 2, GPIO, Modbus, serial, Meshtastic and MeshCore). `det.air.remoteid.{id}` is for drones seen by a Remote ID receiver.
- `svc.{service}.{endpoint}` for on-demand lookups (geocoding, IP reputation, Wikidata and Wikipedia), wrapped as RPC for Hermes tools.
- No subject at all for tiles, video, audio and reference documents (routing rule b).

**Routing (part b).** The route counts above put 88 records on the bus (61 JetStream, 15 JetStream + KV, 3 KV and 9 through the edge leaf). The other 152 never touch it: 34 as files in Object Store or PMTiles, 28 behind the tile proxy, 5 in MediaMTX, 21 in the Hermes knowledge store, and 64 not ingested at all.

**What part c must cover.** It must handle REST polling with per-source quotas, RSS, bulk files with ETag and If-Modified-Since, scraping (6), a WebSocket stream (aisstream), a raw TCP line stream (APRS-IS), MQTT (Meshtastic at the edge) and one JMS-style queue (FAA SWIM, keyed, P3). Webhook push and SSE are not needed by any catalog record.

**Build order (P0, 13 records):**

- Aircraft: adsb.lol, airplanes.live and OpenSky, feeding `ent.aircraft`.
- Ships: aisstream, feeding `ent.vessel`.
- Hazards: USGS, EMSC, NWS, GDACS, EONET and FIRMS. These are already live in omos and need only the subject and reliability fixes.
- Own assets: MAVLink, through `tel.drone`.
- Basemaps: the self-hosted PMTiles host M010 and the PMTiles standard M067.

## Risks

1. **Grammar arity.** The locked form has 4 tokens, but live adapters append qualifiers, for example `intel.seismic.usgs.{bucket}.{id}` and `intel.weather.noaa.alert.{state}.{event}`. The registry uses the 4-token base as the stream filter and leaves identity in the payload and `Nats-Msg-Id`. Whether trailing tokens stay allowed is a BUS-DESIGN decision (decision 0004 #8).
2. **Two grammars disagree.** NATS-DOCTRINE §9.1 uses `evt.world.air.{id}` and similar for world intel, and `source.schema.json` uses `evt.world.air.>` as its example and `registry.source.{id}` as its subject. `omos/CLAUDE.md` locks `intel.*` and `ent.*`. This registry follows `CLAUDE.md`, which is what the live adapters and the INTEL stream use. The owner needs to reconcile the doctrine and the schema. `registry.source.*` is outside the grammar; the closest pattern would be `svc.registry.*` or KV only.
3. **INTEL stream volume.** `intel.>` keeps 90 days (`intel.yaml`). Aircraft and AIS positions at 1–10 s would flood it. The positions need their own stream, with a short `max_age` and a `max_bytes` limit, or a narrower INTEL filter. That is BUS-DESIGN's decision.
4. **Probe meaning is narrow.**
   - An HTTP 200 on a landing page proves only that the site is up.
   - A 403 on a website (asfinag, openmhz, ukmto, volcano.si.edu and others) may be bot protection, not an auth wall.
   - The 403 from the airplanes.live API (F010) and from CitiBike GBFS (F031) may be User-Agent filtering. This is unverified, and both need a retry with a fixture-capturing collector before anything is built on them.
5. **Terms and licences remain mostly unverified.** 120+ records carry `licence: unverified`. Commercial or redistribution limits are confirmed only for airplanes.live (non-commercial), CARTO (tile quotas), OSM (attribution, no bulk) and FIRMS (quota). Before a source is enabled, its terms must be read and recorded in `sources.yaml`.
6. **Policy exclusions.** Flock, North Star identity and crypto tracing, and PulsePoint are marked `excluded` under the "no targeting of people" boundary (`ULTRACODE-PROMPT.md:318`). The owner may disagree on DeFlock (camera sites only) and on social sources (topic monitoring only).
7. **Upstream fragility.** Several useful sources are unofficial or undocumented: the Amtrak map endpoint, Tzeva Adom, DeepState and vfrmap. They need fixtures and fail-soft behaviour.

## Recommendation

- **Make `docs/sources/sources.yaml` the seed of feature A2, the feed registry.** It uses the `source.schema.json` `kind` enum, so it can be loaded into a KV registry bucket. Add `fetchedAt`, record count and last error as runtime fields.
- **Fix the 13 P0 sources first.** Move the CoT-direct Connect pipelines (aircraft, ships, hazards, MTA) to `intel.*` plus entity state. OpenSky already polls anonymously: add OAuth2 client credentials for the higher quota, or remove the unused `OPENSKY_USERNAME/PASSWORD` env vars. Keep FIRMS on its MAP_KEY from a SealedSecret.
- **Never put tiles on NATS.** Stand up one caching tile proxy that honours the OSM, Esri and CARTO terms (User-Agent, cache at least 7 days, no pre-seeding). Do offline work only from self-hosted PMTiles built from Geofabrik or Protomaps extracts.
- **Re-probe from inside the cluster's egress** before building on any record marked 403 or network error. Machine endpoints get a recorded fixture for CI golden tests, which is reliability-list item "Replayable".

## Unverified items

- Licence or terms for every record whose `licence` field says unverified, including GDELT, USGS (public-domain status), OpenSky data, adsb.lol data, GADM, OpenSanctions, Wikidata and Wikipedia, WRI and Safecast.
- Cadence for records marked unverified or "(proposed)". The proposed values are design choices, not upstream facts.
- The auth values that carry "(catalog, unverified)", especially FAA NOTAM (client_id and secret suspected), WSDOT (access code suspected), BLS v1 keyless versus v2 keyed, CoinGecko's demo key, and UCDP's token scheme.
- Whether `alerts.weather.gov` CAP (F105) is permanently retired in favour of `api.weather.gov/alerts`; whether HIFLD Open moved after 2025; and whether SciHub has been replaced by the Copernicus Data Space Ecosystem.
- Whether the Amtrak `getTrainsData` response is encrypted.
- airplanes.live and aisstream.io facts come from search-engine excerpts of their official pages, because direct fetches returned 403.
- The cause of the 403s from the airplanes.live API (F010), CitiBike GBFS (F031) and the website-only records: User-Agent filtering, geography or auth.
- Whether `tiles.atoc.onemindos.dev` (M010) is meant to be tailnet-only.

Blockers: none from the permission system. The tailnet hosts (M038, M039, M048, M064, M070, M073)
and the cluster-internal `pg-featureserv` (M066) could not be probed from this machine
(`PREFLIGHT.md`: no route to `tail717ba0`).
