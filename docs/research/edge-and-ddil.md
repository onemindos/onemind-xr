# Edge and disconnected operations (DDIL)

Lane 18, Phase 1 / M0, 2026-10-29. Scope: Apollo-style deployment to the Mac, home
server and field kits; offline tiles and maps; store-and-forward; conflict resolution on
reconnect; low-bandwidth links; power and hardware tiers.

This lane builds on the NATS lane (`docs/research/nats-platform.md` §6 and "Edge-to-cloud
replication"), which already proved leaf store-and-forward locally. This document does not
repeat those tests. It adds two local proofs: **E1**, how KV behaves when both sides write
during a partition, and **E2**, an offline map pack extracted from the public planet build.

## Sources

Primary documentation and specifications
- S1 PMTiles v3 spec: https://raw.githubusercontent.com/protomaps/PMTiles/main/spec/v3/spec.md
- S2 MBTiles 1.3 spec: https://raw.githubusercontent.com/mapbox/mbtiles-spec/master/1.3/spec.md
- S3 pmtiles CLI docs: https://docs.protomaps.com/pmtiles/cli
- S4 Protomaps basemap downloads: https://docs.protomaps.com/basemaps/downloads ; build list https://build-metadata.protomaps.dev/builds.json (latest `20261028.pmtiles`, 139,207,195,021 bytes, version 4.15.2, read 2026-10-29)
- S5 go-pmtiles releases and licence: `gh api repos/protomaps/go-pmtiles/releases/latest` → v1.31.2, 2026-07-22; `gh api …/license` → BSD-3-Clause
- S6 NATS KV design, ADR-8 (status Implemented, revision 13, 2026-02-02): https://raw.githubusercontent.com/nats-io/nats-architecture-and-design/main/adr/ADR-8.md
- S7 NATS mirrors and sources: https://docs.nats.io/nats-concepts/jetstream/source_and_mirror
- S8 NATS leaf node configuration: https://docs.nats.io/running-a-nats-service/configuration/leafnodes/leafnode_conf
- S9 nats-server v2.15.2 source: `server/opts.go` (leaf remote keys `deny_imports` l.3200, `deny_exports` l.3207, `ws_no_masking` l.3216, `compression` l.3243, `first_info_timeout` l.3248; S2 auto-compression RTT thresholds l.100-108); `server/leafnode.go` (ws/wss remotes l.301-304, l.1947); `server/const.go` l.161-162 (`DEFAULT_LEAF_NODE_RECONNECT = time.Second`)
- S10 nats-server v2.15.2 release assets (darwin-arm64, linux-arm64, linux-arm7, linux-arm6): `gh api repos/nats-io/nats-server/releases/tags/v2.15.2`
- S11 MediaMTX v1.21.3 release assets (darwin-arm64, linux-arm64, linux-armv7): `gh api repos/bluenviron/mediamtx/releases/tags/v1.21.3`
- S12 Palantir Apollo introduction: https://www.palantir.com/docs/apollo/core/introduction/ (Hub/Spoke details are in the Palantir lane, `palantir-patterns.md` AP1-AP6 and §8)
- S13 Zarf: https://docs.zarf.dev/ ; licence Apache-2.0 per https://github.com/zarf-dev/zarf
- S14 K3s air-gap install: https://docs.k3s.io/installation/airgap ; requirements: https://docs.k3s.io/installation/requirements
- S15 Cesium for Unreal `CHANGES.md` (fetched with `gh api repos/CesiumGS/cesium-unreal/contents/CHANGES.md`): v2.0.0 (2023-11-01) "Added support for `file:///` URLs across all platforms"; v2.13.0 (2025-02-03) `CesiumUrlTemplateRasterOverlay`; v2.18.0 (2025-08-01) `CesiumGeoJsonDocumentRasterOverlay`; v2.27.0 (2026-06-01) `CesiumVectorTilesRasterOverlay` "supporting vector data loaded from 3D Tiles tilesets"; `MaximumCachedBytes`, SQLite request cache and "Clear Request Cache". Latest release: v2.30.0, 2026-09-29.
- S16 Meshtastic radio presets: https://meshtastic.org/docs/overview/radio-settings/
- S17 Meshtastic ATAK plugin (GPL-3.0): https://github.com/meshtastic/ATAK-Plugin
- S18 goTenna Pro X2 spec sheet (updated 2023-05-18): https://cdn.shopify.com/s/files/1/0149/5551/1856/files/051823_ProX2_V3.pdf
- S19 NVIDIA Jetson Orin Nano Super developer kit: https://www.nvidia.com/en-us/autonomous-machines/embedded-systems/jetson-orin/nano-super-developer-kit/
- S20 Raspberry Pi 5: https://www.raspberrypi.com/products/raspberry-pi-5/
- S21 Apple Mac mini power consumption: https://support.apple.com/en-us/103253
- S22 Starlink Mini, **secondary source only** (starlink.com/specifications renders client-side and returned no text): Network Innovations data sheet https://www.networkinnovations.com/hubfs/docs/product/starlink/NI-Starlink-Mini.pdf (via web search)

Repository files
- O1 omos `CLAUDE.md` ("Bus Authority"; the leaf is "a forwarding pipe, not a peer bus"; "When the home server arrives, it becomes another leaf (and eventually the authority)")
- O2 omos `docs/NATS-DOCTRINE.md` §4 (consume / mirror / merge; "the leaf node buffers during disconnection")
- O3 omos `infra/edge/mac-leaf/nats-server.conf` l.22-66 and `README.md`
- O4 omos `infra/edge/takserver/values-edge.yaml` l.1-25 (edge TAK Server; federation to HQ needs the FedHub on the tailnet)
- O5 omos `docs/self-hosted-capability-plan.md` (K3s / KubeEdge / RDFM / openBalena / NetBird / Nebula assessment)
- O6 omos `docs/onetak-offline-native.md` (CloudTAK service worker, `dfpc-coe/mbtiles-offline`, CloudTAK tiles as PMTiles)
- O7 onemind-xr `docs/research/nats-platform.md` §6 (P7, P7b, P7c, P7d) and "Edge-to-cloud replication"
- O8 onemind-xr `docs/research/tak-protocols.md` l.130-151 (an SA event is 281 bytes in TAK Protocol v1 mesh, 621 bytes as XML)
- O9 onemind-xr `docs/research/palantir-patterns.md` §8 (Apollo mapped to Argo CD and NATS leaves)
- O10 onemind-xr `docs/FEATURES.md` (A7, B2, B15, B16, D10, E6, F9, H8)

Local proofs (this lane; config under `work/.run/edge-lane/`, data deleted afterwards)
- E1 KV partition test: nats-server 2.15.2 hub (domain `hub`, 127.0.0.1:46222, leaf listen 46722) and leaf `site-a` (domain `site-a`, 46223). Results in Verified facts §3.
- E2 offline map pack: go-pmtiles v1.31.2 in `work/.tools/bin/pmtiles` (archive in `work/.tools/dl/go-pmtiles/`). Results in Verified facts §4.

## Verified facts

### 1. Deployment (Apollo pattern, edge tooling)

- Apollo uses a pull model. Environments subscribe to release channels (for example
  RELEASE, CANARY, STABLE), products and environments both declare constraints, and
  Apollo "automatically orchestrate[s] software upgrades … across connected and
  disconnected, air-gapped environments" (S12). Hub and Spoke Control Plane: see
  `palantir-patterns.md` AP4.
- **Zarf** (Apache-2.0) is a "statically compiled binary" with zero dependencies. It
  packages what an app needs "into a single compressed file". Its motto is "Develop
  Connected, Deploy Disconnected". It includes a registry, a Gitea Git server and a mutating
  webhook that rewrites image paths. It deploys a new K3s cluster while fully disconnected,
  or into any existing cluster (S13).
- **K3s** installs air-gapped from an image tarball copied to each node (no registry), from a
  private registry, or through its embedded registry mirror. The K3s binary and the image
  archive must come from the same release (S14). Minimums: server 2 cores and 2 GB; agent
  1 core and 512 MB. Architectures: x86_64, armhf, arm64. On a Pi, enable cgroups and use an
  SSD, not an SD card. The SSD advice is stated for etcd ("SD cards and eMMC cannot handle
  the IO load") (S14).
- omos already assessed K3s against KubeEdge (KubeEdge 1.23 supports Kubernetes only up to
  1.32, while the hub runs 1.36), RDFM (Apache-2.0 OTA with A/B rollback) and openBalena. It
  notes that "Argo at the hub cannot reconcile while disconnected; a local reconciler needs
  local desired state access" (O5).
- The edge binaries OneMind needs ship for every tier: nats-server 2.15.2 for darwin-arm64,
  linux-arm64, arm7 and arm6 (S10), and MediaMTX 1.21.3 for darwin-arm64, linux-arm64 and
  armv7 (S11). The nats-server binary is 17.7 MB (`ls -la work/.tools/bin/nats-server`).

### 2. Store-and-forward (from the NATS lane, not re-run)

- Store-and-forward needs an **edge JetStream stream** in the site's own domain, with the hub
  **sourcing** it. Without one, core publishes made during an outage are lost (O7 P7, P7d).
  This contradicts doctrine §4's "the leaf node buffers during disconnection" (O2).
- When a hub stream's subjects overlap the edge subjects, the hub captures each message twice
  (O7 P7b). Re-sync took 41 s after the leaf reconnected (O7 P7c).
- Leaf remotes support `deny_imports`, `deny_exports`, `compression`, `first_info_timeout`
  and WebSocket transport (`ws://`/`wss://` URLs, `ws_no_masking`) (S9). Leaf reconnect
  defaults to 1 s (S9 `const.go`). With S2 auto-compression the level rises with RTT, from
  `S2Fast` to `S2Better` to `S2Best` above the thresholds (S9 `opts.go` l.100-108).
  `dial_timeout` (default 1 s) should be raised on high-latency links (S8).
- The committed Mac leaf config enables JetStream but sets **no JetStream `domain`**. It
  uses `tls { insecure: true }`, `no_auth_user`, WebSocket 8080 `no_tls` and MQTT 1883 with
  no bind host, and has static placeholder user passwords in its local accounts (O3
  l.22-66). The live Mac was not inspected (no access, `PREFLIGHT.md`).

### 3. Conflict resolution on reconnect (proof E1)

Setup: KV `ZONES_SITEA` on the leaf (domain `site-a`), and KV `ZONES` on the hub whose
stream `KV_ZONES` sources `KV_ZONES_SITEA` across domains (`external.api =
$JS.site-a.API`, transform `$KV.ZONES_SITEA.>` → `$KV.ZONES.>`). `nats kv info ZONES`
showed `Source Bucket: ZONES_SITEA, Lag: 0`.

| Step | Time | Action | Hub `gate.north` |
|---|---|---|---|
| 1 | 04:26:04 | site writes `open` (T1) while connected | `open` by site-a (rev 1), arrived within 2 s |
| 2 | 04:26:24 | site restarted **without leaf remote**; site writes `closed` (T2) | unchanged |
| 3 | 04:26:27 | hub operator writes `open` (T3, **later** than T2) | `open` by hub-operator (rev 2) |
| 4 | 04:26:29 | site restarted with leaf remote | — |
| 5 | 04:26:35 | source catches up (about 7 s) | **`closed` by site-a, T2 (rev 3)**: the older offline write overwrote the newer hub write |

Further outputs:
- `nats stream get KV_ZONES 3` reported `received 2026-10-29 08:26:35 UTC` (the arrival time,
  not the T2 origin time 08:26:24) with header `Nats-Stream-Source: KV_ZONES_SITEA:… 2 …
  $KV.ZONES_SITEA.gate.north`. The source sequence and origin subject survive. The origin
  timestamp does not.
- Sourcing is one-way: the site bucket never received the hub's T3 write (`nats kv get
  ZONES_SITEA gate.north` → T2).
- Compare-and-set protects the operator. `nats kv update ZONES gate.north … 2` failed with
  `err_code=10071 wrong last sequence: 3: key revision mismatch`, and the same update with
  revision 3 succeeded.
- These results match S6 and S7. KV `Update` "only succeeds when the most recent operation on
  a key has the expected revision". There is no read-after-write consistency. Sources give
  "no ordering guarantee" across upstreams, and mirrors are read-only. Neither document
  defines any conflict resolution beyond this.

*Independent re-run (verifier, 2026-10-29 08:40 UTC, same configs and `src.json` on ports
47222/47722/47223 in a scratch directory, data deleted afterwards).* It reproduced every
outcome above. The hub showed `open-T3` during the partition and `closed-T2` after reconnect.
`nats stream get KV_ZONES 3` showed the arrival time (08:40:29) and `Nats-Stream-Source:
KV_ZONES_SITEA:… 2 … $KV.ZONES_SITEA.gate.north`. `kv update … 2` failed with
`err_code=10071`, and `kv update … 3` succeeded. Catch-up took about **16 s** in this run,
not 7 s, so treat the delay as variable.

**Conclusion:** a JetStream source merge is **last arrival wins**, not last writer wins.
NATS does not resolve conflicts. OneMind has to resolve them through how keys are owned and
what goes in each value.

### 4. Offline tiles and maps (proof E2)

- **PMTiles v3:** one file with a 127-byte header, a root directory in the first 16 KiB,
  Hilbert-ordered tile IDs, and tile type codes for Unknown, MVT, PNG, JPEG, WebP, AVIF and
  MapLibre Vector Tile (0x06) (S1; verifier added the omitted Unknown and 0x06 MLT types).
  It is a static layout. The spec does not address in-place updates (S1). Spec CC0/public domain; go-pmtiles BSD-3 (S5).
- **MBTiles 1.3:** a SQLite database with `metadata` and `tiles` tables, TMS y-flip
  (`tile_row = 2^z − 1 − y`), Spherical Mercator, and pbf, jpg, png or webp tiles (S2).
  `pmtiles convert` turns MBTiles into PMTiles (S3).
- `pmtiles extract` cuts a bbox or a GeoJSON region out of a remote, clustered archive
  using range requests. `pmtiles serve` serves ZXY plus TileJSON with an LRU cache (S3).
- The Protomaps planet basemap (zoom 0-15, ODbL, OpenStreetMap attribution required) is
  139.2 GB, decimal (S4 `builds.json`: 139,207,195,021 bytes, or 129.6 GiB). The downloads page
  still says "roughly 120 GB", so use `builds.json` for sizing.
- **E2 results** (build `20261028.pmtiles`):
  - A 0.1° × 0.1° box (about 8 × 11 km, `-74.05,40.95,-73.95,41.05`) at zoom 0-15 gave
    217 tiles and 4.4 MB. It downloaded in 7.6 s over 54 range requests, and `pmtiles
    verify` passed.
  - Dry run for the New York State box (`-79.8,40.5,-71.8,45.0`): zoom 0-12 is 8,530 tiles
    and 135 MB; zoom 0-15 is 499,066 tiles and 1.4 GB.
  - `pmtiles serve` returned tile 15/9648/12285 as HTTP 200, 2,453 B,
    `application/x-protobuf`. TileJSON listed `{z}/{x}/{y}.mvt`, zoom 0-15 and the
    OpenStreetMap attribution. *(Verifier correction: the server did not bind 127.0.0.1.
    `work/.run/edge-lane/serve.log` reads "Serving . on port 46080 and interface 0.0.0.0",
    which is go-pmtiles' default, so the pack was reachable from the LAN during the test.
    The process is now stopped. Pass `--interface=127.0.0.1` in any future run or site
    unit. The 2,453 B size and content type were not re-checked because the server is
    stopped.)*
- **Cesium for Unreal:** `file:///` URLs work on all platforms since v2.0.0. Tile responses
  go into a SQLite request cache (`MaximumCachedBytes`, "Clear Request Cache"). Raster
  overlays can be WMS, WMTS, TMS or URL templates. GeoJSON is rasterized onto terrain
  (v2.18.0). The vector raster overlay reads vector data **from 3D Tiles tilesets**
  (v2.27.0) (S15). The changelog lists no overlay that reads MVT/XYZ vector tiles, so
  Protomaps MVT tiles need a rendering step before the Unreal globe can show them (see
  Recommendation).
- omos already runs a TileServer GL (WMS/WMTS from MBTiles, scaled to 0) and a PMTiles tile
  server for CloudTAK. The CloudTAK service worker caches the app shell for offline use
  (O6; `standards.md` L6).

### 5. Low-bandwidth links

| Link | Verified figures | Source |
|---|---|---|
| Meshtastic LoRa | Theoretical maximum: SHORT_TURBO 21.88 kbps, LONG_FAST 1.07 kbps, LONG_MODERATE 0.34 kbps. Real throughput is lower (headers, hops, retransmits) | S16 |
| Meshtastic ATAK plugin | PLI and GeoChat use compact protobuf; other CoT is EXI-compressed. Messages over 231 bytes go out as fountain (LT) codes. Files only on SHORT_TURBO, max 56 KB. Default PLI every 5 min (30 s-30 min). Hop limit 1-7, default 3. GPL-3.0 | S17 |
| goTenna Pro X2 | VHF 142-175 MHz / UHF 445-480 MHz, 12.5/25 kHz channels, 4GFSK, 0.5-5 W. 15 mi line of sight body-worn, 55 mi elevated relay, over 100 mi aerial relay. "up to 9 hours" nominal battery (3.51 Wh, 474 mAh), AES-256 for broadcast, private and group messaging (a general feature, not tied to the goTenna-ATAK plugin, which is listed separately), recommended hop count 6, more than 60 nodes demonstrated, IP68. **No data rate published** | S18 |
| Starlink Mini | 25-40 W average, 12-48 V DC input rated 60 W (**secondary source**) | S22 |
| TAK SA event | 281 bytes in TAK Protocol v1 mesh, 621 bytes as XML | O8 |

Derived from these figures (arithmetic, not measured): one 281-byte SA event needs about
2.1 s of airtime at the LONG_FAST theoretical rate (281 × 8 / 1070) and about 0.1 s at
SHORT_TURBO. The 4.4 MB E2 pack would take about 9 hours at LONG_FAST. Tiles, models and
video can never go over mesh radio. They must be pre-staged or sent over Starlink or Wi-Fi.

### 6. Power and hardware tiers

| Tier | Example hardware | Verified figures | Source |
|---|---|---|---|
| T0 radio node | goTenna Pro X2 / Meshtastic node | 3.51 Wh battery, 9 h nominal (goTenna) | S18 |
| T1 relay / gateway | Raspberry Pi 5 | 4-core Cortex-A76 at 2.4 GHz; 1-16 GB; 5 V / 5 A USB-C (27 W PSU recommended); PCIe 2.0 x1 for M.2 SSD; 16 GB model $305 | S20 |
| T2 edge AI | Jetson Orin Nano Super | 67 INT8 TOPS, 7-25 W, 1,024-core Ampere GPU, 6-core A78AE, 8 GB LPDDR5 | S19 |
| T3 home server | Mac mini (2024, M4) | 4 W idle, 65 W max at the wall; M4 Pro 5 W / 140 W | S21 |
| T3 laptop | Owner's Apple Silicon Mac | Runs the client and a local leaf (A7, F9) | O3, O10 |
| Uplink | Starlink Mini | 25-40 W average (secondary) | S22 |

## How it applies to OneMind

**Site model.** Each site is a NATS leaf with **its own JetStream domain** (`site-<name>`),
following the NATS lane design (O7). Sites: the owner's Mac, the homestead server and each
field kit. The cloud hub sources the site streams. Each site mirrors the parts of hub `ENT`
and the object buckets it needs. This keeps the omos rule that the leaf forwards and the
cloud is the authority (O1), and it lets the home server later become the authority without
redesign.

**What crosses a DDIL boundary.** Each class below gets its own transport.

| Data | At the site | To the hub | Over mesh radio |
|---|---|---|---|
| Telemetry, detections, events (`tel.*`, `det.*`, `evt.*`) | edge streams, 72 h | sourced after reconnect (catch-up lag shown in the UI) | no (TAK SA only) |
| Entity state (`ent.*.state`, KV) | site-owned keys written locally | sourced; hub KV merges per key | no |
| Commands (`cmd.*`) | local policy + human confirm (run rule 6) | recorded in hub `CMD` | **never queued for replay**: an offline site gets no command (O7) |
| TAK SA/chat | mesh gateway `tak.cot.mesh.*` (A7) | through the leaf when up | yes: ATAK with goTenna or Meshtastic plugins at TAK v1 sizes |
| Tiles, terrain, 3D Tiles, models | local PMTiles/MBTiles + `pmtiles serve`, Object Store | Object Store mirror when bandwidth allows | never |
| Video | local MediaMTX recording | pulled on demand | never |
| Software | signed offline bundle (OCI digests + Git bundle + tile packs) | site agent reports versions on `tel.{node}.sys.{metric}` (O9) | never |

**Conflict rules (from E1).** Because the merge is last arrival wins:
1. **One writer per key.** Site-originated state is keyed by origin, for example
   `ent.drone.sim-x1.state` written only by the site that owns `sim-x1`. The hub never writes
   a site-owned key while that site is partitioned. Shared objects (a zone, a mission) have
   one owning site, and others propose changes as `evt.*`, not as direct KV writes.
2. **Carry origin time and version in the value.** Every value carries `origin`, `ts`
   (origin clock) and a per-origin `seq`. The entity service applies a sourced value only if
   `(ts, origin, seq)` is newer than what it holds. If it is older, it records an
   `evt.{domain}.{entity}.conflict` and keeps the newer value. The `Nats-Stream-Source`
   header gives a second, clock-independent order per origin.
3. **Operators use compare-and-set** (`Update` with expected revision) so a human edit made
   from a stale view fails visibly (E1, err 10071) and is not overwritten silently.
4. **Clock skew.** Field kits may have no NTP. Use GPS time where it exists, and record the
   skew between the receive time and the origin `ts`. Hybrid logical clocks are an option
   for the entity service, but are **unverified** for this run.

**Offline maps.** Per site, an operator selects an AOI and the system builds a map pack:
- Protomaps vector basemap extract (about 4 MB for a homestead at zoom 15, 135 MB for a state
  at zoom 12; E2).
- Open raster imagery and terrain for the same area.
- WebODM 3D Tiles (E6).

Packs are stored in the Object Store `tiles` bucket (NATS lane design) and served locally by
`pmtiles serve` or TileServer GL. The same pack feeds:
- the L0 web client (MapLibre reads PMTiles directly; **unverified in this lane**);
- the Unreal globe, through raster tiles from TileServer GL or a WMTS endpoint, because
  Cesium for Unreal has no MVT overlay (S15);
- ATAK, through WMTS/XYZ from TileServer GL. Direct MBTiles/GeoPackage import into ATAK is
  **unverified**.

**Deployment.** The Apollo pattern on OneMind tooling (O9):
- Hub: Git `main` plus Argo CD.
- Channels: Git refs or OCI digests.
- Each site runs a local reconciler with local desired state, because Argo at the hub cannot
  reach a disconnected site (O5).

| Tier | Runtime | How it updates |
|---|---|---|
| Mac | launchd + Homebrew-free binaries (`scripts/` setup, as in O3) | pulls a signed bundle when online |
| Home server | K3s, air-gapped install (S14) | Zarf packages (S13) built in CI and carried or pulled; local registry and Gitea mirror |
| Field kit (Pi 5 / Jetson) | systemd units or K3s agent | the same Zarf package or a tarball of pinned binaries |

## Risks

| # | Risk | Evidence | Mitigation |
|---|---|---|---|
| ER1 | Older offline writes silently overwrite newer hub state on reconnect | E1 step 5 | Single writer per key; origin `ts`/`seq` check in the entity service; CAS for operators |
| ER2 | Doctrine says the leaf buffers; it does not without an edge stream | O2 vs O7 P7d | Edge streams per site; correct the doctrine through an omos docs PR |
| ER3 | Mac leaf config has no JetStream domain, plus insecure TLS, `no_auth_user` and unauthenticated WS/MQTT on all interfaces | O3 | Domain `site-mac`, CA-verified TLS, per-site leaf credential, bind WS/MQTT to 127.0.0.1 (same as NATS lane R4) |
| ER4 | Re-sync lag (7 s in E1, about 16 s in the verifier's E1 re-run, 41 s in P7c) makes the hub picture stale right after reconnect | E1, O7 P7c | Show per-site source lag and a "syncing" badge |
| ER5 | Commands replayed after a partition could move an asset on stale intent | O7 commands rule | Never queue `cmd.*`; commands carry an expiry; offline site shows "disconnected" |
| ER6 | Mesh links are about 1 kbps; anything beyond SA/chat saturates them | S16, S17, derived airtime | Only TAK SA and chat on mesh; rate-limit PLI; pre-stage tiles |
| ER7 | Cesium for Unreal cannot render MVT; offline packs need a raster or 3D Tiles path | S15 | Raster tiles from TileServer GL for Unreal; MVT for MapLibre clients |
| ER8 | Map data licences (ODbL needs attribution; some imagery is non-commercial) | S4 | Attribution in every client; record each imagery or terrain licence in the source registry |
| ER9 | Large packs vs disk: a state at zoom 15 is 1.4 GB, the planet 139 GB | E2, S4 | AOI packs with a maxzoom cap; byte caps on Object Store buckets |
| ER10 | Self-hosted identity or mesh control can become a single point of failure for a disconnected site | O5 | Pre-provision local credentials and recovery paths (O5 mesh evaluation) |
| ER11 | goTenna data rate is unpublished and its plugin is proprietary | S18 | Treat goTenna as an owner-supplied option; Meshtastic (GPL-3.0 plugin) is the open default |

## Recommendation

1. **Adopt per-site JetStream domains with hub sourcing as the only edge-to-cloud path**
   (with the NATS lane). Add the E1 conflict rules to `docs/design/BUS-DESIGN.md` and
   `docs/contracts/entity.md`:
   - single writer per key;
   - `origin`, `ts` and `seq` in every value;
   - an `evt.{domain}.{entity}.conflict` event;
   - CAS for human edits;
   - no queued commands.
2. **Fix the Mac leaf first (A7/F9):** add domain `site-mac`, a 72 h edge stream for
   `tel/evt/det`, CA-verified TLS, its own credential, and local-only WS/MQTT. Then rerun the
   P7 and E1 tests on the owner's Mac as an owner-checklist item. In omos, this is a PR to
   `infra/edge/mac-leaf/`, plus a doctrine §4 correction.
3. **Offline map packs (B2, B15, B16, F9):**
   - Standardize on PMTiles AOI extracts of the Protomaps basemap, served by `pmtiles serve`
     (BSD-3 binary, single file, no Docker).
   - Use TileServer GL (already in omos) to render raster/WMTS for Unreal and ATAK.
   - Use open terrain and 3D Tiles from local `file:///` or localhost URLs, with the Cesium
     SQLite cache sized through `MaximumCachedBytes`.
   - Store packs in the Object Store `tiles` bucket and mirror them to sites.
4. **Deployment:**
   - Mac and field kits get pinned binaries under launchd or systemd from one setup script.
   - The home server runs K3s installed air-gapped, with **Zarf** packages (Apache-2.0) built
     in CI from the same pinned images and charts Argo uses.
   - A small site agent reports versions and health on `tel.{node}.sys.{metric}` and accepts
     upgrades only inside its change window.
   - Defer KubeEdge (version gap) and RDFM (needs A/B partitions) until hardware is chosen
     (O5).
5. **Links:**
   - Meshtastic with its GPL-3.0 ATAK plugin is the open low-bandwidth default, and the omos
     mesh gateway (A7) bridges it to the bus.
   - goTenna is an owner option.
   - Starlink is the field kit's bulk uplink, budgeted at 25-40 W.
   - Raise leaf `dial_timeout` and leave S2 auto-compression on for satellite or cellular
     leaves.
6. **Hardware tiers for the owner checklist:**
   - T1 Pi 5 (16 GB, SSD) as a field relay running nats-server, the mesh gateway and
     MediaMTX.
   - T2 Jetson Orin Nano Super for H8 detection.
   - T3 Mac mini or the owner's Mac as home server and client.
   - A field kit of Pi 5 + Starlink Mini + LoRa nodes is roughly 27 W + 25-40 W on paper
     (arithmetic from the PSU rating and the secondary Starlink figure, not measured).
   - Buy nothing in this run (decision 0004 #5).

## Unverified items

- Apollo Hub and Spoke mechanics come from the Palantir lane (P17). Palantir's own blog on
  air-gapped delivery returned HTTP 403. Apollo's offline transfer mechanics are
  **unverified**.
- Starlink Mini power and input figures come from a reseller data sheet, not starlink.com.
- The goTenna Pro X2 data rate is not published anywhere I found.
- ATAK-CIV direct import of MBTiles, GeoPackage or PMTiles is unconfirmed. The
  `deptofdefense/AndroidTacticalAssaultKit-CIV` repo is archived (pushed 2024-10-18) and
  GitHub code search returned 0 hits. Test on a device (owner checklist).
- Whether MapLibre in the L0 client reads local PMTiles offline was not tested in this lane.
- Cesium for Unreal offline terrain from local quantized-mesh `layer.json`: community posts
  only. Test with S1 on the Mac.
- The 7 s (E1), about 16 s (verifier re-run of E1) and 41 s (P7c) catch-up delays are single
  local runs. The cause (source retry
  backoff) is not confirmed.
- Hybrid logical clocks for entity ordering are not evaluated.
- Real Meshtastic throughput with headers, hops and retransmits was not measured. No radios
  are available.
- The live state of the Mac leaf (vs the Git config) was not inspected.
- `CHANGES.md` on Cesium for Unreal `main` already lists v2.31.0 dated 2026-11-02, which is
  not yet released (latest release is v2.30.0). Only released-version facts are used above.

## Independent verification (2026-10-29)

The verifier re-checked 24 claims against primary sources or by re-running them:
- S1 (PMTiles spec), S2 not re-fetched, S3/E2 (re-ran both NYS dry runs: 8,530 tiles /
  135 MB and 499,066 tiles / 1.4 GB, exact), S4 (`builds.json`), S5 (`gh api`; local binary
  `pmtiles version` → 1.31.2, sha256 matches the extracted archive copy), S6, S7, S8, S9
  (opts.go l.100-108, l.3200-3250; const.go l.161-162; leafnode.go ws URL handling), S10,
  S11, S12, S13, S14, S15 (`CHANGES.md` version headings), S16, S17 (README), S18 (both PDF
  pages), S19, S20, S21, O2, O3, O5, O1, O8, E1 (re-run), and the homestead pack (`pmtiles
  show` → 217 tiles, bounds and zoom as stated; `pmtiles verify` passed).
- Corrected: the `pmtiles serve` bind address (0.0.0.0, not 127.0.0.1), the PMTiles tile-type
  list, the goTenna AES-256 attribution, the K3s SSD context, the planet-size note, and the
  E1 catch-up time.
- Not re-checked: S2, S22 (secondary source), the 54 range requests and 7.6 s for the
  homestead extract, and the `pmtiles serve` response size (the server is stopped).
- Licence and cluster safety: no worldmonitor or worldwideview code found, and the
  GPL-3.0 Meshtastic plugin is only referenced. The lane used no cluster access. All lane
  servers are stopped and the JetStream directories are deleted. One safety finding: the
  0.0.0.0 bind exposed the test pack to the LAN while it ran (see §4).

## Blockers

None from the permission system in this lane. No live cluster reads were needed. Hardware
and links (radios, Starlink, Jetson, Pi, the owner's Mac) are not reachable from the build
machine, so their checks belong in `docs/OWNER-CHECKLIST.md`.
