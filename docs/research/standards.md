# Research lane: standards (Phase 1 / M0)

Date: 2026-10-29. Lane owner: standards research agent. Scope: symbology
(MIL-STD-2525 / APP-6), CoT types, drone video metadata (MISB KLV, STANAG 4609),
UAV control (STANAG 4586), MAVLink, ROS 2 and DDS, OGC (WMS/WMTS, 3D Tiles,
SensorThings, GeoPackage), OpenXR, and civilian emergency standards (CAP, EDXL).
For each: does OneMind adopt it, where, and how available are the spec and open
libraries. Feeds features B7, E1, F4, H3, K5 and L3 in `docs/FEATURES.md`.

Repos read: omos at `62e3d22b`, onemind-xr at `0b3f20c`.

## Sources

Local:

| Ref | Source |
|---|---|
| L1 | `omos/CLAUDE.md`, "NATS Subject Grammar (LOCKED)" block |
| L2 | `omos/platform/omos/tak-projector.yaml` lines 57-90 (`COT_TYPE_MAP`, commented "MIL-STD-2525C / TAK") |
| L3 | `omos/docs/infra/TAK-INTEGRATION-REFERENCE.md` lines 66, 90, 176, 763-766 (milsymbol, CoT type to 2525 mapping) |
| L4 | `omos/services/nex/overwatch/om-overwatch-state/ontology.go` line 22 (affiliation "simplified" from 2525) |
| L5 | `omos/docs/data-plane.md` lines 55, 95, 121 (`intel.weather.noaa.alert.*` from NWS CAP API; weather alert CoT type `a-f-G-U-W`) |
| L6 | `omos/platform/onetak/tileserver-gl.yaml` lines 1-3 (WMS/WMTS from MBTiles, scaled to 0); `omos/platform/cloudtak/tiles.yaml` lines 1-2, 42 (PMTiles server); `omos/docs/onetak-offline-native.md` |
| L7 | `onemind-xr/docs/sources/map-sources/Fabric Feeds/STANAG 4586 (UAS Control).md` and `.../NWS CAP Alerts.md` (catalog records, subjects `fabric.drone.control.stanag.*`, `fabric.intel.weather.nws.alerts.*`, source URL `alerts.weather.gov/cap/us.php`) |
| L8 | `onemind-xr/docs/FEATURES.md` rows B7, E1, F4, H3, K5, L3; `docs/ARCHITECTURE.md` lines 8, 51, 61-62; `docs/decisions/0003-meta-quest-pro.md` |
| L9 | `onemind-xr/docs/research/drones-robots-agents.md` (MAVLink bridge test, ATAK-CIV video/sensor CoT detail handlers, `mil-std-2525c` assets in ATAK-CIV) |
| L10 | ffmpeg 8.1.1 (`/opt/homebrew/bin/ffmpeg`). *Verifier correction:* `-demuxers` has **no** `klv` entry. KLV appears in `-codecs` as a data codec: `..D... klv  SMPTE 336M Key-Length-Value (KLV) metadata`. `-demuxers` lists `D mpegts` and `D data`, and `-muxers` lists `E mpegts` and `E data` |

GitHub licence checks (`gh api repos/<r> --jq '[.license.spdx_id,.pushed_at,.archived]'`, 2026-10-29):

| Repo | SPDX | Last push | Notes |
|---|---|---|---|
| spatialillusions/milsymbol | MIT | 2026-10-27 | |
| missioncommand/mil-sym-ts | Apache-2.0 | 2026-10-21 | US Army mission command symbol renderer (TypeScript) |
| missioncommand/mil-sym-java | Apache-2.0 | 2026-10-21 | |
| deptofdefense/AndroidTacticalAssaultKit-CIV | NOASSERTION | 2024-10-18 | **archived = true** |
| WestRidgeSystems/jmisb | MIT | 2026-09-24 | Java MISB/KLV |
| paretech/klvdata | MIT | 2025-01-07 | Python KLV, low activity |
| bluenviron/mediamtx, bluenviron/gomavlib | MIT | 2026-10-27 | |
| snstac/pytak | Apache-2.0 | 2026-10-23 | |
| NERVsystems/cotlib | MIT | 2026-10-05 | Go CoT library, ships `cottypes/CoTtypes.xml` |
| dB-SPL/cot-types | Apache-2.0 | 2021-12-30 | CoTtypes.xml copy, MITRE 2003 header |
| kdudkov/goatak | **AGPL-3.0** | 2026-06-30 | do not copy into GPL client |
| FreeTAKTeam/FreeTakServer | EPL-2.0 | 2024-10-29 | |
| ros2/rmw_zenoh | Apache-2.0 | 2026-10-28 | |
| eclipse-zenoh/zenoh-plugin-ros2dds | NOASSERTION (EPL-2.0 OR Apache-2.0 per drones lane W9, not re-checked) | 2026-10-29 | |
| RobotWebTools/rosbridge_suite | BSD-3-Clause | 2026-10-05 | |
| eProsima/Fast-DDS | Apache-2.0 | 2026-10-29 | |
| eclipse-cyclonedds/cyclonedds | NOASSERTION; `LICENSE` file: EPL-2.0 or EDL (BSD-3) | 2026-10-22 | |
| CesiumGS/cesium | Apache-2.0 | 2026-10-29 | |
| CesiumGS/3d-tiles | none detected | 2026-10-23 | spec repo |
| KhronosGroup/OpenXR-SDK | Apache-2.0 | 2026-10-16 | loader + headers |
| FraunhoferIOSB/FROST-Server | LGPL-3.0 | 2026-10-28 | SensorThings server |
| protomaps/PMTiles | NOASSERTION; `LICENSE`: spec public domain/CC0, reference code BSD-3 | 2026-09-16 | |
| mavlink/mavlink | NOASSERTION (see W2) | 2026-10-28 | |

Web (fetched 2026-10-29):

| Ref | Source |
|---|---|
| W1 | Khronos OpenXR page: https://www.khronos.org/openxr/ |
| W2 | MAVLink licence: https://mavlink.io/en/ (License section) |
| W3 | OASIS CAP 1.2: https://docs.oasis-open.org/emergency/cap/v1.2/CAP-v1.2-os.html |
| W4 | OASIS EDXL-DE 2.0: https://docs.oasis-open.org/emergency/edxl-de/v2.0/edxl-de-v2.0.html |
| W5 | NWS API docs: https://www.weather.gov/documentation/services-web-api |
| W6 | NWS notices: https://www.weather.gov/media/notification/pdf_2025/pns25-36_alerts_webpage.pdf (via search result summary) and the 2024 ATOM-to-API notice (search summary; not opened) |
| W7 | OGC 3D Tiles: https://www.ogc.org/standards/3dtiles/ ; spec README: https://github.com/CesiumGS/3d-tiles |
| W8 | OGC GeoPackage: https://www.ogc.org/standards/geopackage/ ; licence text in https://docs.ogc.org/is/12-128r19/12-128r19.html |
| W9 | OGC SensorThings: https://www.ogc.org/standards/sensorthings/ |
| W10 | OGC WMTS: https://www.ogc.org/standards/wmts/ ; OGC standards list: https://www.ogc.org/standards/ |
| W11 | ROS REP-2000 (distributions, RMW tiers): https://raw.githubusercontent.com/ros-infrastructure/rep/master/rep-2000.rst |
| W12 | milsymbol README: https://github.com/spatialillusions/milsymbol |
| W13 | jMISB README: https://github.com/WestRidgeSystems/jmisb |
| W14 | cot-types README (MITRE CoTtypes.xml origin): https://github.com/dB-SPL/cot-types |
| W15 | Search results for MIL-STD-2525E / 2525D status (globalspec listing; Joint Staff 2525D PDF https://www.jcs.mil/Portals/36/Documents/Doctrine/Other_Pubs/ms_2525d.pdf, which returned HTTP 403 to WebFetch) |
| W16 | Search results for STANAG 4586 Ed. 4 / AEP-84 (https://en.wikipedia.org/wiki/STANAG_4586, shop.standards.ie, globalspec) |
| W17 | Search results for STANAG 4609 / AEDP-8 (https://impleotv.com/2025/03/11/stanag-4609-isr-video/, globalspec, shop.standards.ie) |

Blocked fetches (recorded, not worked around): `nsgreg.nga.mil/misb.jsp` returned a
captcha page; `www.gwg.nga.mil` did not resolve; `docs.ros.org` returned an
anti-bot "Access Denied"; the Joint Staff 2525D PDF returned 403.

## Verified facts

### Symbology: MIL-STD-2525 and APP-6

- milsymbol (MIT) renders MIL-STD-2525C, D and E, FM 1-02.2 and APP-6 B, D and E;
  since v3.0 it draws "as closely as possible" to 2525E/APP-6 E (W12). It is JS, so it
  fits the L0 web client directly, and can pre-render SVG/PNG atlases for Unreal.
- The US Army `missioncommand/mil-sym-ts` and `mil-sym-java` renderers are
  Apache-2.0 and active (licence table).
- MIL-STD-2525E exists (Rev E, 2022-12-31, per a standards listing, W15). The only
  copy confirmed as "Distribution A: approved for public release" in search results is
  2525D (2014) hosted by the Joint Staff (W15); I could not open it (403).
- ATAK-CIV ships `mil-std-2525c` icon assets (L9), and the ATAK-CIV repository is
  archived as of 2024-10-18 (licence table).
- omos already maps entity kinds to CoT atom types in `tak-projector.yaml`, e.g.
  drone `a-f-A-M-F-Q`, ship `a-f-S-X-W-U`, robot `a-f-G-U-C-R`, camera `a-f-G-I-S-C`
  (L2). The overwatch ontology uses a "simplified" 2525 affiliation axis (L4).

### CoT types

- The canonical CoT type table is MITRE's `CoTtypes.xml` (copyright MITRE 2003,
  author Mike Butler). Upper-case segments come from the MIL-STD-2525 hierarchy,
  lower-case are CoT extensions, matching is case-sensitive, and the table covers the
  atom part after `a-x-`, so it applies to `a-f-`, `a-h-`, `a-u-` and so on (W14).
- Permissively licensed copies are in `dB-SPL/cot-types` (Apache-2.0) and
  `NERVsystems/cotlib` (MIT); `goatak` also ships one but is AGPL-3.0 (licence table).
- omos maps NOAA weather alerts to `a-f-G-U-W...` (L5), i.e. a friendly ground-unit
  atom, and fires/explosions to `b-r-.-H-.` (L2).
- TAK Protocol framing and takproto are already verified by the run (prompt "Facts
  already verified"); not re-researched here.

### Drone video metadata: MISB KLV and STANAG 4609

- ffmpeg 8.1.1 on this machine recognises KLV (SMPTE 336M) as a data codec, `klv`,
  and has an MPEG-TS demuxer and muxer and a raw `data` muxer (L10). *Verifier
  correction:* it has no KLV demuxer, as the earlier text said. KLV is carried as
  a data stream inside MPEG-TS. Extracting it with
  `-map 0:d -c copy -f data` is plausible but was not run, so it is **unverified**.
- jMISB (MIT, Java) implements ST 0601 (mostly, as of 0601.17), ST 0102.12, ST 0903.5
  (VMTI/VTrack), ST 1201, ST 1204 and ST 1402 (MPEG-2 TS carriage, sync and async)
  (W13, licence table). `paretech/klvdata` (Python, MIT) is a smaller option but last
  pushed 2025-01-07.
- STANAG 4609 is NATO's digital motion imagery standard for FMV; AEDP-8 is its
  implementation guide (W17). Only commercial copies were found; free NATO release
  could not be confirmed (W17).
- FEATURES K5 already asks for the frame footprint and sensor point from KLV (L8).

### UAV control: STANAG 4586

- STANAG 4586 (2017), with AEP-84 volumes dated 2017, is listed as current by
  retailers (https://shop.standards.ie/en-ie/standards/stanag-4586-2017-736356_saig_nato_nato_1788547,
  https://shop.standards.ie/en-ie/standards/aep-84-4-a2017-737615_saig_nato_nato_1792019).
  *Verifier correction:* the Wikipedia article (W16) gives no edition number or date
  and does not mention AEP-84. The edition number "4" comes only from search
  summaries and is **unverified**. Copies were found only from commercial sellers; no
  free NATO download was found (W16).
- The source catalog lists STANAG 4586 as "Planned" with auth "Certificate" and
  source URL Wikipedia (L7); there is no implementation in omos (repo grep found only
  catalog files).

### MAVLink

- The XML message definitions and the generated C library are MIT; the generator
  toolchain is LGPL-3.0 (W2). gomavlib (Go, MIT) is used by MediaMTX's author and is the
  library in omos' MAVLink bridge per the drones lane (L9).
- A local MAVLink-to-NATS bridge test publishing `tel.sim-x1.drone.position` already
  passed (L9).

### ROS 2 and DDS

- REP-2000: Jazzy Jalisco (LTS) runs May 2024 to May 2029; Kilted Kaiju runs May 2025
  to November 2026 (W11). Kilted's default RMW is `rmw_fastrtps_cpp` (Fast DDS), and
  `rmw_zenoh_cpp` is Tier 1 alongside Cyclone DDS and Connext (W11). macOS is Tier 3
  for Kilted (W11).
- Fast DDS is Apache-2.0; Cyclone DDS is EPL-2.0 or EDL/BSD-3; rmw_zenoh is
  Apache-2.0; rosbridge_suite is BSD-3-Clause (licence table).

### OGC

- 3D Tiles 1.1 (OGC 22-025r4) and 1.0 (18-053r2) are OGC Community Standards;
  3D Tiles 2.0 is "Currently in development" according to the CesiumGS/3d-tiles
  README. The OGC page does not mention 2.0 (W7, verifier note). The spec licence is not stated on
  the pages read (W7).
- GeoPackage 1.4.0 (OGC 12-128r19), published 2024-02-06. Its OGC licence grants
  permission "free of charge" to implement, use and modify, is "a copyright licence
  only" and conveys no patent rights; the status section says royalty-free,
  non-discriminatory (W8).
- SensorThings: Sensing 1.1 (18-088), Tasking Core 1.0, STAplus 1.0 and WebSub
  messaging 1.0; no 2.0 listed (W9). FROST-Server is LGPL-3.0 (licence table).
- WMTS 1.0.0 (07-057r7) plus Simple Profile 1.0 (W10). OGC says implementers "are
  encouraged to use these newer OGC API Standards". The sentence is on the
  https://www.ogc.org/standards/ list page, not on the WMTS page (W10, verifier note).
- omos already has a WMS/WMTS TileServer GL deployment (scaled to 0) and a PMTiles
  server for CloudTAK (L6). PMTiles' spec is public domain/CC0 (licence table).

### OpenXR

- OpenXR 1.1 is current, royalty-free, and the spec is "free for anyone to download";
  Meta lists a conformant runtime for Quest Pro, Quest 2/3 and Meta XR Simulator (W1).
  The OpenXR SDK loader is Apache-2.0 (licence table).

### Civilian emergency: CAP and EDXL

- CAP 1.2 is an OASIS Standard (2010-07-01), namespace
  `urn:oasis:names:tc:emergency:cap:1.2`, structure `alert` > `info` > `area`
  (polygon, circle, geocode). OASIS permits copying and redistribution with the notice;
  patent disclosures are on the TC IPR page (W3).
- EDXL-DE 2.0 (Committee Specification 02, 2013-09-19) is the routing envelope, with
  namespace `urn:oasis:names:tc:emergency:EDXL:DE:2.0`; the family includes HAVE, RM
  and SitRep (W4).
- NWS serves alerts from `api.weather.gov/alerts`, format chosen with `Accept`
  (e.g. `application/cap+xml`), User-Agent required, rate limits not published, retry
  after about 5 s (W5). It is **unverified** that NWS moved the ATOM feed to
  API-sourced CAP 1.2 around 2024-01-31: that notice was never opened. PNS 25-36
  (2025-05-27) solicited feedback until 2025-06-26 on the "potential retirement" of
  alerts.weather.gov and named `api.weather.gov/alerts` (GeoJSON, JSON-LD, Atom, CAP
  XML) as the alternative. The verifier opened the PDF to confirm this (W6).

## How it applies to OneMind

| Standard | Adopt? | Where in OneMind | Spec availability | Open libraries |
|---|---|---|---|---|
| **CoT types** (MITRE CoTtypes.xml) | **Yes, core** | omos entity service and `tak-projector` emit CoT; client `OneMindCoT` (from VR-TAK `VH_COT`) parses it; `ent.{type}.{id}.state` keeps the CoT type as a field | Public file (MITRE 2003) in Apache/MIT repos | takproto, pytak (Apache-2.0), cotlib (MIT) |
| **MIL-STD-2525 / APP-6** | **Yes, render only** (feature B7) | Symbol from CoT type (2525C letter codes) in the client; web L0 with milsymbol; Unreal via pre-rendered milsymbol atlases or mil-sym-ts output | 2525D public (Dist. A); 2525E and APP-6 E availability **unverified** | milsymbol (MIT), mil-sym-ts/java (Apache-2.0) |
| **MISB KLV / STANAG 4609** | **Yes, read-only, P2** (feature K5) | MediaMTX carries the MPEG-TS; a small omos KLV extractor (ffmpeg demux + ST 0601 parse) publishes sensor pose to `tel.{node}.drone.sensor` and footprint to the entity; client drapes video | MISB docs from NGA (not reachable here); STANAG 4609 copies only commercial | jMISB (MIT, Java), klvdata (MIT, Python), ffmpeg (mpegts demuxer + `klv` data codec) |
| **STANAG 4586** | **No** (document only) | Out of scope: no free spec found, military UAS control, no sim assets speak it. Keep the catalog record as "not planned"; MAVLink covers control | Commercial only (found) | none found |
| **MAVLink 2** | **Yes, core** for drones | omos `services/bridge` MAVLink bridge to `tel.*` / `cmd.*`; local sim `sim-x1` | Free (MIT XML) | gomavlib (MIT), pymavlink, MAVSDK (BSD-3) |
| **ROS 2 / DDS** | **Yes at the edge, never on the bus** (feature H3) | Robots stay on DDS or rmw_zenoh locally; bridge to NATS (Zenoh bridge or rosbridge) in omos `services/bridge`; target Jazzy LTS | ROS free; OMG DDS specs free (**unverified** here) | Fast DDS (Apache-2.0), Cyclone (EPL/BSD), rmw_zenoh (Apache-2.0), rosbridge (BSD-3) |
| **3D Tiles** | **Yes, core** (feature E1) | Homestead twin from WebODM; Cesium for Unreal and CesiumJS consume it; served by the caching tile proxy, never NATS | Open spec, OGC Community Standard; licence text **unverified** | CesiumJS (Apache-2.0), Cesium for Unreal |
| **WMS/WMTS, OGC API Tiles** | **Consume yes; serve WMTS/XYZ for TAK** | Client imports WMTS/XYZ basemaps; omos TileServer GL/PMTiles serve offline tiles to ATAK and the client | Free, royalty-free (OGC licence) | TileServer GL, PMTiles (BSD-3), martin |
| **GeoPackage** | **Yes, import/export** | Static overlays and offline packages (with PMTiles/MBTiles) in Object Store; data-package exchange with TAK | Free, royalty-free (W8) | GDAL/OGR (not re-checked) |
| **SensorThings API** | **Optional ingest only** | A source plugin may poll public STA endpoints into `tel.*`; do not run FROST as a core service (overlaps NATS + KV) | Free OGC | FROST-Server (LGPL-3.0) |
| **OpenXR** | **Yes** (features F4, L3) | `OneMindXR` plugin (from `VH_VR`) on Quest Pro standalone; WebXR fallback in L0 | Free, royalty-free (W1) | OpenXR SDK (Apache-2.0); Unreal OpenXR plugin |
| **CAP 1.2** | **Yes, ingest** | NWS (and other CAP feeds) to `intel.weather.nws.alert` events, latest state in KV, CoT drawing to TAK | Free (OASIS) | standard XML parsers |
| **EDXL-DE / SitRep / RM** | **Later, exchange format only** | Optional export for response-team partners; no ingest today | Free (OASIS) | none needed |

## Risks

1. **Weather alerts typed as friendly ground units.** omos maps weather alerts to
   `a-f-G-U-W...` (L5); in CoT/2525 that is an atom with friendly affiliation, so ATAK
   shows a unit symbol, not an area or hazard. Whether intended is **unverified**.
   Alerts are areas (CAP `area` polygons) and should render as shapes.
2. **Catalog subjects break the locked grammar.** The imported source catalog uses
   `fabric.drone.control.stanag.*` and `fabric.intel.weather.nws.alerts.*` (L7), and
   data-plane uses `intel.weather.noaa.alert.*` (L5). `fabric.*` is not in the grammar
   (L1); anything built from the catalog must use `intel.weather.nws.alert` style
   subjects.
3. **Catalog uses a legacy NWS URL** (`alerts.weather.gov/cap/us.php`, L7); NWS has
   moved feeds to `api.weather.gov` and proposed retiring the alerts webpage (W6).
4. **Symbol revision drift.** ATAK-CIV uses 2525C assets (L9), milsymbol targets 2525E
   (W12). If OneMind renders 2525E while ATAK shows 2525C, users see different icons for
   the same entity. Keep CoT letter-code (2525C-style) as the interchange and choose the
   rendering style per client.
5. **ATAK-CIV repo is archived** (licence table), so CoT type tables and assets there
   will not be updated.
6. **AGPL CoT code.** goatak is AGPL-3.0; copying it into GPL-3.0 onemind-xr is
   incompatible in spirit with the run's licence rule. Use Apache/MIT sources.
7. **STANAG/MISB spec access.** NATO STANAGs found only commercially; NGA MISB site
   unreachable from here. Implementations must rely on jMISB and public summaries,
   which risks gaps in newer 0601 tags.
8. **ROS 2 on macOS is Tier 3** (W11). The Mac cannot be the robot host; the ROS side
   stays on Linux robots or sims, bridged over NATS.
9. **SensorThings server would duplicate NATS/KV** and FROST is LGPL-3.0 Java; keep it
   as a source, not a layer of the platform.

## Recommendation

Adopt as core: **CoT types (interchange), MAVLink 2, 3D Tiles, OpenXR, CAP 1.2.**
Adopt for rendering and exchange: **MIL-STD-2525/APP-6 via milsymbol (MIT) and
mil-sym-ts (Apache-2.0)**, WMTS/XYZ and OGC API Tiles, GeoPackage/PMTiles. Adopt
read-only at P2: **MISB ST 0601/0903 KLV in STANAG 4609 MPEG-TS** via ffmpeg demux
plus a small parser modelled on jMISB (MIT). Bridge **ROS 2/DDS** at the edge only
(Jazzy LTS, rmw_zenoh or rosbridge to NATS). Treat **SensorThings** as an optional
source and **EDXL** as a later partner export. **Do not adopt STANAG 4586.**

Concrete next steps for the build lanes:
1. Client `OneMindCoT`: load a CoT-type to SIDC/icon table from the Apache-2.0
   `CoTtypes.xml` copy; L0 uses milsymbol directly. Acceptance B7.
2. omos PR: change weather-alert CoT from `a-f-G-U-W` atoms to CAP-area shapes, and
   point NWS ingestion at `api.weather.gov/alerts` with `Accept: application/geo+json`
   or `application/cap+xml` and a User-Agent, publishing `intel.weather.nws.alert`.
3. Spike for K5: mux a synthetic ST 0601 packet into MPEG-TS, push through local
   MediaMTX, demux with ffmpeg, parse sensor lat/lon/alt and footprint corners.
4. Mark the STANAG 4586 catalog record "not planned" and fix catalog subjects to the
   locked grammar.

## Unverified items

- Free public availability of MIL-STD-2525E and APP-6(E) (only 2525D confirmed as
  Distribution A, and that PDF could not be opened here).
- Free availability of STANAG 4609, AEDP-8, STANAG 4586 and AEP-84 from NATO NSO.
- Current MISB ST 0601 revision number and NGA distribution terms (site blocked).
- Licence text of the 3D Tiles specification itself.
- OMG DDS spec free availability; zenoh-plugin-ros2dds exact licence (GitHub reports
  NOASSERTION).
- Whether a newer ROS 2 distribution (May 2026) exists; REP-2000 as fetched listed
  Jazzy and Kilted only.
- Whether ATAK (current, non-CIV-repo builds) renders 2525D/E; whether omos'
  `a-f-G-U-W` weather type was intentional.
- That ffmpeg can mux a KLV data stream into MPEG-TS, or extract one from it, for
  the spike. The verifier confirmed only that the `klv` data codec and the
  mpegts/data muxers and demuxers exist; neither direction was run.
- The NWS ATOM-to-API CAP move around 2024-01-31 (notice not opened).
- The STANAG 4586 edition number ("Edition 4").

## Verifier pass (2026-10-29)

The verifier re-checked 16 claims against primary sources: milsymbol README, jMISB
README, cot-types README, mavlink.io licence, REP-2000, OGC pages for 3D Tiles,
GeoPackage (and the 12-128r19 spec text), SensorThings, WMTS and the standards list,
the Khronos OpenXR page, OASIS CAP 1.2 and EDXL-DE 2.0, NWS API docs, the PNS 25-36
PDF, and 12 `gh api` licence rows. It also checked local L2, L5 and L10 and the omos
`gomavlib` usage in `services/bridge/bridges/mavlink/go.mod`.
Corrected: the ffmpeg KLV "demuxer" (L10, MISB section, table); the attribution of
STANAG 4586 Edition 4/AEP-84 to Wikipedia; the source for 3D Tiles 2.0; the source
page for the WMTS/OGC API quote. Marked **unverified**: the NWS ATOM move of
2024-01-31, the STANAG 4586 edition number, and ffmpeg KLV extraction/muxing.
No licence-rule violation: no worldmonitor/worldwideview content, and goatak (AGPL)
is flagged, not copied. No cluster-safety violation: no cluster commands were used.
- Final retirement date of the alerts.weather.gov webpage.
