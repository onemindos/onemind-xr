# Video and plugins

Lane: video-plugins (Phase 1 / M0). Date: 2026-10-29. Features: K1–K6, E2, E4, A9, H9.
Evidence: `docs/evidence/M0/video/` (proof log, the lane's MediaMTX config with placeholder
credentials, KLV generator and decoder, a frame read back from HLS).

**Summary.** The cluster MediaMTX (`onetak-media`) runs, but nothing can reach it:

- it has no ingress of any kind;
- CloudTAK's network policy does not allow traffic to it;
- CloudTAK needs the dfpc-coe media-infra wrapper API, not plain MediaMTX;
- no stream has been published for as long as the logs go back.

On this machine, all four ingest protocols (RTSP, RTMP, SRT, WHIP) went through
MediaMTX v1.21.3 and were read back over HLS and RTSP, with per-path authentication,
fMP4 recording and the playback API. A synthetic MISB ST 0601 KLV stream survived
MediaMTX into MPEG-TS recording with every checksum valid. Two limits showed up:
Unreal's Electra player does not support LL-HLS partial segments, and ffmpeg cannot
depacketize KLV over RTSP. Both shape the design below.

## Sources

Local runs and files (this machine and the omos clone at `main`):

| Id | Source |
|---|---|
| L1 | `omos/platform/onetak/media-mtx.yaml` (ConfigMap, Deployment, Service) |
| L2 | `omos/platform/cloudtak/README.md` lines 45–58 (service table, network summary) and 156–181 ("Media replacement gate") |
| L3 | `omos/platform/cloudtak/network.yaml` lines 72–78 (only egress into `onetak` is `app: tile38` port 9851) |
| L4 | `omos/platform/takml/deployment.yaml` lines 229–282 (vision worker `FEEDS`, publishes `fabric.vision.{feed_id}.detect`), `platform/takml/network.yaml` 132–157 |
| L5 | `omos/infra/edge/mac-leaf/ffmpeg-cam-push.plist` lines 4–8, 57 (pushes to `rtsp://mediamtx-tak-1.tail717ba0.ts.net:8554/maccam`) |
| L6 | `omos/platform/hermes/agent/RUNBOOK.md` line 93 (says MediaMTX is in the `takserver` namespace) |
| L7 | `omos/platform/nats/streams/kv-overwatch-video.yaml` (KV `OVERWATCH_VIDEO`, "per-entity video stream availability", R3) |
| L8 | `omos/platform/hermes/agent/plugins/onemind-vision/plugin.yaml` and `plugins/README.md` (Hermes plugin manifest format, install state) |
| L9 | `omos/docs/STACK.md` line 135 (`platform-onetak` row) |
| K1 | `kubectl --context do-nyc3-omos-cloud` read-only, 2026-10-29 08:14Z: `-n onetak get deploy,svc,pod,ingress`, `logs deploy/onetak-media --tail=25`, `-n cloudtak get pods`, `-n argocd get application platform-onetak platform-cloudtak`, `get svc -A \| grep media`, `get keyvalue -A \| grep video`. No Secrets were read |
| R1 | `docs/evidence/M0/video/video-lane-proofs.txt`: loop 1 (shared session MediaMTX) and loop 2 (lane MediaMTX on offset ports), 2026-10-29 08:16–08:24Z |
| R2 | Reference `mediamtx.yml` shipped with v1.21.3 (`work/.run/mediamtx.yml`): auth (lines 50–146), playback (216–237), path sources (488–508), forward (542–560), record (576–600), hooks (800–871) |

Primary sources online:

| Id | Source |
|---|---|
| W1 | MediaMTX docs in the repo (`gh api repos/bluenviron/mediamtx/contents/docs/...`): `3-publish/02-srt.md`, `3-publish/04-rtsp.md`, `5-read/02-srt.md`, `5-read/03-webrtc.md`, `5-read/04-rtsp.md`, `5-read/06-hls.md` (codec tables incl. KLV), `2-features/05-authentication.md` (lines 292–339). Latest release v1.21.3, 2026-10-25 |
| W2 | paretech/klvdata (MIT) `klvdata/misb0601.py` (UAS LS key, tag maps) and `klvdata/common.py` (`packet_checksum`, `float_to_bytes`) |
| W3 | ATAK-CIV mirror `deptofdefense/AndroidTacticalAssaultKit-CIV` (main, last push 2024-10-18): `atak/ATAK/app/src/main/java/com/atakmap/android/video/ConnectionEntry.java` (enum `Protocol` lines 42–54, `ignoreEmbeddedKLV` 119), `video/cot/VideoDetailHandler.java` (lines 61–105) |
| W4 | Epic, "Electra Media Player in Unreal Engine" (page titled Unreal Engine 5.8): https://dev.epicgames.com/documentation/unreal-engine/electra-media-player-in-unreal-engine |
| W5 | Apple WWDC20 session 10228, "What's new in low-latency HLS": https://developer.apple.com/videos/play/wwdc2020/10228/ |
| W9 | (added by verifier) bluenviron/gortsplib (MIT) at `main` 943f5ac38a8e (2026-10-28); MediaMTX v1.21.3 `go.mod` pins gortsplib v5.6.7. `pkg/format/klv.go` (type `KLV`, "Specification: RFC6597", `CreateDecoder() (*rtpklv.Decoder, error)`) and `pkg/format/rtpklv/decoder.go` (`func (d *Decoder) Decode(pkt *rtp.Packet) ([]byte, error)`) |
| W6 | worldwideview (Elastic-2.0, **read for design only, no code copied**): `packages/wwv-plugin-sdk/src/manifest.ts` (`PluginManifest`, `PluginCapability`, `TrustTier`, `McpToolDeclaration`), `src/index.ts` (`WorldPlugin` lines 299–350), file list of `src/core/plugins/`. https://github.com/silvertakana/worldwideview |
| W7 | gods-eye-view (MIT): `src/app/catalog.js`, `src/app/layers/cctv.js`, `src/layers/cctv/index.js` (lines 20–124), `projection.js`, `videoPlayback.js` (lines 1–52). https://github.com/bilawalsidhu/gods-eye-view |
| W8 | Sibling lanes, cited and not re-researched: `docs/research/tak-protocols.md` (S6 `__video.xsd`, S7 ATAK plugin template, S10 TAK Server `VideoConnectionManagerV2.java`/`FeedV2.java`), `docs/research/drones-robots-agents.md` (W12 `VideoDetailHandler`, `SensorDetailHandler`; MAVLink `VIDEO_STREAM_INFORMATION`), `docs/research/standards.md` (MISB/STANAG 4609 status, jMISB), `docs/research/vrtak-porting-map.md` (VH_Plugins, line 170–212), `docs/research/unreal-platform.md` (Pixel Streaming on Mac) |

## Verified facts

### A. Why CloudTAK shows MediaMTX "Not connected"

The README says it outright (L2 line 47: "Media server | `onetak-media` … | **Not
connected.**"). The manifests and the cluster give four independent causes:

1. **Wrong API.** CloudTAK's video features need the dfpc-coe
   [media-infra wrapper](https://github.com/dfpc-coe/media-infra/tree/v9.12.1). It exposes
   port 9997 (`/path`, `/path/:name`, `/v3/config/global/get`, `/stream/<name>/index.m3u8`)
   and authenticates with CloudTAK's `SigningSecret`. `onetak-media` is plain upstream MediaMTX
   with its API on `:4000`, so `media::url` is deliberately left unset and video leases are off
   (L2 lines 158–161; L1 `apiAddress: :4000`).
2. **Network policy.** The only egress from the `cloudtak` namespace into `onetak` goes to
   pods labelled `app: tile38` on TCP 9851 (L3). Even with `media::url` set, the API could not
   reach `onetak-media`. The README confirms: "No media egress is opened until a compatible media
   replacement is reviewed" (L2 line 58).
3. **No publisher can reach it.** `service/onetak-media` is `ClusterIP` only. No Ingress,
   LoadBalancer or Tailscale Service fronts it, and no other media Service exists in the cluster
   (K1). The only edge pusher in Git targets `mediamtx-tak-1.tail717ba0.ts.net`, a host that
   no Service in the cluster provides (L5, K1).
4. **Nothing is publishing.** (*Verifier:* the pod's log only reaches back to 2026-10-28
   20:36Z, about 12 h, with 0 "is publishing" lines in 34,424 lines. So "no stream has been
   published" is proven for that window only, not for the pod's 10 days.) The pod has run for 10 days (Deployment 49 days, image
   `bluenviron/mediamtx:1-ffmpeg`, on node `onemind-hub-core-v2-3rtf4m`). Every 10 s its log shows
   the takml vision worker trying `drone1`, `takcam`, `helmcam` and `maccam` and getting "no
   stream is available on path" (K1, L4). Both `platform-onetak` and `platform-cloudtak` are
   `Synced`/`Healthy` (K1), so this is a design gap, not drift.

Other problems in the same manifest (L1):

- It has no authentication ("default authMethod 'internal' with zero users = open").
- It sets `apiAllowOrigins: ['*']` and `playback: yes` with no `record` setting, and the
  shipped default is `record: false` (R2 line 576). So there is nothing to play back.
- The image tag `1-ffmpeg` floats (it is not a digest), and the pod runs on the hub-core
  pool, which the omos rules call memory-saturated.

The takml worker publishes `fabric.vision.*`, which is outside the locked grammar (L4). The
Hermes runbook still places MediaMTX in `takserver` (L6).

### B. Local loop results (R1)

The loop used MediaMTX v1.21.3 and ffmpeg 8.1.1 on localhost, with a 1280×720 30 fps
`testsrc2` H.264 test pattern (`-tune zerolatency`, GOP 1 s), plus AAC or Opus audio.

| Test | Result |
|---|---|
| Loop 1: RTSP publish → shared MediaMTX `:8554/m0-video-test` → HLS read | `ffprobe` over HLS: h264 1280×720 30/1 + aac 48 kHz. 5 s HLS capture: 5.03 s, 4.86 MB. RTSP readback: h264 + aac. One PNG frame from HLS (`hls-readback-frame.png`) |
| (a) Anonymous RTSP publish to the lane instance | `ANNOUNCE failed: 401 (Unauthorized)` |
| (f) Authenticated publisher to `other/x` | `400 Bad Request`. That path matches no configured path, so this proves the path allowlist, not the permission regex (see Unverified) |
| RTSP publish (`drone/sim-x1`) with `pub-drone` | ready, source `rtspSession`, tracks H264 + MPEG-4 Audio. Read over HLS with `viewer` |
| RTMP publish (`cam/rtmp-test?user=…&pass=…`) | ready, source `rtmpConn`. Read over HLS |
| WHIP publish (ffmpeg 8.1.1 `whip` muxer, URL userinfo → Basic) | ready, source `webRTCSession`, tracks Opus + H264. Read over HLS (h264 + opus) and RTSP. The first attempt failed with 401 because I sent `Bearer base64(user:pass)`; the docs say Bearer takes plain `user:pass` (W1 auth lines 316–320) |
| SRT publish | Homebrew ffmpeg has no `srt` protocol (`ffmpeg -protocols` lists only `srtp`). MediaMTX's own SRT client was used instead: path `drone/srt-src` with `forward: srt://127.0.0.1:18890?streamid=publish:drone/srt-test:pub-drone:…` gave `drone/srt-test` ready with source `srtConn`, readable over HLS and RTSP |
| HLS anonymous / with viewer | The first response is always `302` to `?cookieCheck=1` (a cookie check, not auth). After following it, anonymous gets **401**. Viewer reads succeeded through ffprobe (above) |
| Recording | fMP4 segments under `recordings/<path>/`, 10 s each (`recordSegmentDuration: 10s`), probe h264 + aac 10.0 s. MediaMTX warns that `recordPath` without `%z` misplaces segments at daylight-saving changes |
| Playback server | `/list?path=drone/sim-x1` with viewer → JSON `[{"start":…,"duration":34.58,…}]`. Anonymous → **401**. `/get … duration=5&format=fmp4` → h264 + aac 5.02 s |
| Control API | `POST /v3/config/paths/add/<name>` and `PATCH /v3/config/paths/patch/<name>` → HTTP 200, applied live |
| Metrics | `paths{name=…,state="ready"} 1`, `paths_bytes_received{…}` |
| LL-HLS edge lag | `hlsVariant: lowLatency`, 1 s segments, 200 ms parts. Wall clock minus the end of the newest listed part (from `EXT-X-PROGRAM-DATE-TIME`): **0.06, 0.10, 0.16, 0.20, 0.07 s**. The playlist advertises `PART-HOLD-BACK=0.50000`. This is server packaging lag only; glass-to-glass was not measured |
| Bitrate | `testsrc2` at x264 ultrafast is about 7.9 Mb/s (`BANDWIDTH=7930320` in the master playlist), and the recordings totalled 136 MB across 5 paths. *Verifier correction:* the segment timestamps and the `/list` duration in R1 add up to about 2.5 min of recorded media across those paths, not 3.5 min. That works out to roughly 7 Mb/s, which matches `BANDWIDTH`. Real cameras will differ |
| KLV (ST 0601) + H.264 in MPEG-TS → path source `udp+mpegts://` | Path tracks `['H264', 'KLV']`. MPEG-TS recording holds `h264 klv`. ffmpeg extracted the data stream, and `klv_decode.py` found **101 packets, 101 checksums OK**, with decoded sensor lat/lon/alt, heading, HFOV/VFOV and frame centre matching the generator |
| KLV over RTSP read | MediaMTX offers the track. ffmpeg reports `unknown,data` and copies it as `bin_data` (no RFC 6597 depacketizer), so a KLV reader needs a library that speaks KLV-over-RTP |
| KLV over LL-HLS | HLS readback shows `h264` only. The docs say KLV is supported in "MPEG-TS-based HLS only" (W1 `06-hls.md`), so the fMP4 low-latency variant drops it |

### C. MediaMTX capabilities used by the design (W1, R2)

- Codecs: SRT publish and read carry "other: KLV". RTSP carries "KLV, MPEG-TS, any
  RTP-compatible codec". WebRTC read lists KLV. HLS carries KLV only in MPEG-TS HLS (W1).
- Auth methods are `internal` (users with `ips` and per-action, per-path permissions, path regex
  with `~`), `http` (POST to an external URL per action), and `jwt` (JWKS URL, claim
  `mediamtx_permissions`, optional `iss`/`aud` checks). Credentials travel per protocol: RTSP
  userinfo, RTMP `?user=&pass=`, SRT `streamid=publish:path:user:pass`, HLS/WebRTC
  `Authorization: Basic` or `Bearer user:pass`, and a JWT as `?token=` (R2 lines 50–146; W1 auth
  lines 292–339).
- Path sources (R2 lines 470–508): `publisher` and pulled `rtsp(s)://`, `rtmp(s)://`,
  `http(s)://` HLS, `udp+mpegts://`, `srt://`, `whep(s)://` and `redirect`, plus
  `sourceOnDemand` (pull only while readers exist).
- `forward` pushes a path to another RTSP, RTMP, SRT, MoQ or WHIP server (R2 542–560).
- Recording is fMP4 or MPEG-TS with `recordDeleteAfter`; the playback server's `/list` and
  `/get` are gated by the `playback` permission (R2, R1).
- Hooks `runOnReady`/`runOnNotReady`/`runOnRecordSegmentComplete` get `MTX_PATH`, `MTX_QUERY`
  and `MTX_SOURCE_TYPE` (R2 800–871).

### D. TAK and ATAK video

- CoT `<__video>` has a `url` attribute in the TAK XSD (W8 tak-protocols S6). ATAK's
  `VideoDetailHandler` also reads a nested `<ConnectionEntry>` with `alias, address, port,
  roverPort, path, protocol, networkTimeout (default 5000), bufferTime, rtspReliable,
  ignoreEmbeddedKLV` (W3 lines 82–103; W8 drones W12).
- ATAK `ConnectionEntry.Protocol` covers `raw, udp, rtsp(554), rtmp(1935), rtmps(443),
  http(80), https(443), file, tcp, rtp, dir, srt` (W3 lines 42–54). The `ignoreEmbeddedKLV` field
  shows ATAK reads KLV embedded in the stream (W3 line 119). This is the 2024 CIV mirror; the
  ATAK 5.6 list is **unverified**.
- `<sensor>` carries `azimuth, fov, vfov, range, elevation, roll …` and draws an FOV cone
  (W8 drones W12).
- TAK Server's video connection manager is `GET/POST /Marti/api/video` and
  `GET/PUT/DELETE /Marti/api/video/{uid}`. A FeedV2 has `alias, url, latitude, longitude, fov,
  heading, range, ignoreEmbeddedKLV, rtspReliable, bufferTime, classification, …` (W8
  tak-protocols S10, 5.7-RELEASE-14 source).
- Over TAK Protocol v1, `__video` and `sensor` travel in `xmlDetail` (W8 drones).

### E. Unreal and headset playback

- Electra (UE 5.8 page) runs on Windows, **macOS**, iOS 13+ and **Android 5.1+**. It plays live
  and VOD HLS, DASH and MPEG streams, with H.264/H.265 through the platform OS (W4).
- The page lists `#EXT-X-PART`, `#EXT-X-PART-INF` and `#EXT-X-PRELOAD-HINT` as **unsupported**
  tags and `#EXT-X-SERVER-CONTROL` as only partly used (W4). So Electra plays an LL-HLS playlist
  as ordinary segmented HLS. The page does not mention RTSP or WebRTC (W4).
- Apple's AVPlayer has supported LL-HLS with no entitlement since iOS 14 and tvOS 14 (W5).
  *Verifier correction:* the session says "this year's macOS" and never names macOS 11. That
  macOS 11 (Big Sur, 2020) is meant is an inference, so it stays **unverified** from W5 alone.
- Pixel Streaming 2 on macOS is unproven (W8 unreal-platform facts 8–10).

### F. Plugin references

- **worldwideview** (design only, W6). The manifest has `id, name, version, type`
  (`data-layer | extension`), `format` (`declarative | static | bundle`), `trust`
  (`built-in | verified | unverified`) and `capabilities` (`data:own`, `data:read:<id>`,
  `ui:*`, `globe:overlay`, `globe:camera`, `storage:*`, `network:fetch`). It also has
  `compatibility`, `requires.envVars`, a declarative `dataSource` (url, poll interval, format,
  auth reference to an env var), `fieldMapping`, `rendering` (entity type, clustering,
  `maxEntities`), `mcpTools` (name, description, input schema; executed only in the browser) and
  `localData`. The runtime interface `WorldPlugin` has `initialize/destroy`, `fetch(timeRange)`,
  `getPollingInterval`, `getLayerConfig`, `renderEntity`, optional filters and `executeMcpTool`.
- **gods-eye-view** (MIT, W7). Each layer is a factory `create<Layer>({ services, source })`
  that gets its scene services injected. The CCTV layer requires a source with `getCatalog`,
  `getHealth`, `getFrameUrl` and `getMediaUrl`. `catalog.js` validates unique layer ids against
  serialization metadata. Video plays through `hls.js` with a per-client `lease` query parameter,
  and is shown on a "monitor plane" placed at the camera frustum's cap, sized from frustum
  geometry. That is a plane, not a texture projected on terrain.
- **ATAK** (W8 tak-protocols S7). A plugin is an APK with `assets/plugin.xml` declaring
  `gov.tak.api.plugin.IPlugin`, built with `atak-gradle-takdev` against ATAK 5.6.0. The
  examples include `videooverlay`, `videocollections` and `videomosaic`.
- **VR-TAK VH_Plugins** (W8 vrtak-porting-map lines 170–212). It is binary only. A runtime
  subsystem registers, enables, deletes and imports `VTAK_*` plugins, and it depends on
  engine-patched Projects/Core (`FInvalidPlugin`, `SlatedForDeletion`). The verdict there:
  **do not port**. Use stock project plugins, plus `IPluginManager::MountExplicitlyLoadedPlugin`
  for content-only add-ons.
- **Hermes** (L8). A plugin is a directory with `plugin.yaml` (`name, version, description,
  requires_env`) and Python code. `onemind-vision` was installed by hand (a GitOps gap).
- **NATS.** omos already has the KV bucket `OVERWATCH_VIDEO` ("per-entity video stream
  availability", R3, `Ready` live) (L7, K1). No writer for it exists in the omos repo (grep
  found only docs).

## How it applies to OneMind

### Video path (K1–K5, E2, E4, A9, H9)

```
 drones (RTSP/companion, SRT over lossy links, MAVLink VIDEO_STREAM_INFORMATION.uri)
 owned cameras (RTSP pull, sourceOnDemand)        phones/browsers/OBS (WHIP, RTMP)
 public cameras (HLS pull; MJPEG/snapshots via a snapshot proxy, not MediaMTX)
            │
            ▼
   EDGE MediaMTX (home server / Mac, local-first)  ── forward (SRT) over Tailscale ──►  CLOUD onetak-media
     auth, record (local retention), hooks                                            auth (JWT), record, playback
            │  readers on the LAN                                                         │ remote readers
            ├── WebRTC/WHEP  ≤300 ms class (FPV, control)                                 ├── LL-HLS (web, Apple)
            ├── RTSP / SRT   ≤1 s class (Unreal native player, ATAK, KLV extractor)       ├── HLS (Electra, ATAK http)
            └── hooks → video-registry ──► NATS: ent.*.state video block, evt.video.*, KV OVERWATCH_VIDEO
                         KLV extractor ──► NATS: tel.{node}.drone.sensor, tel.{node}.drone.footprint
                         TAK projector ──► CoT __video + ConnectionEntry + sensor; POST /Marti/api/video
```

1. **Topology.** This mirrors the bus rule: the edge MediaMTX is a forwarding node and the
   cloud keeps working when the Mac is off. LAN viewers read the edge directly for low latency.
   Edge paths `forward` (fact C) to the cloud over SRT, which carries KLV (fact C).
2. **Ingest by source kind.**
   - Drones: RTSP from the companion computer or camera, or SRT push from the edge. The stream
     URI comes from MAVLink `VIDEO_STREAM_INFORMATION` (W8 drones).
   - Owned cameras: `source: rtsp://…` with `sourceOnDemand: yes`, so idle cameras cost
     nothing.
   - Public cameras: `source: https://…m3u8` where the terms allow restreaming. Otherwise the
     client plays the public URL directly. MJPEG and snapshot-only cameras go through a small
     snapshot proxy (MediaMTX has no MJPEG source; R2 source list).
   - Browsers and phones: WHIP (proven, R1).
3. **Path naming.** Use `{class}/{entity_id}`, for example `drone/sim-x1` or
   `cam/gate-north`, so the path maps 1:1 to an entity. Permissions are regexes over these
   prefixes (proven, R1).
4. **Auth.**
   - Phase A (K1, now): `internal` users per role, as in R1: `pub-<class>` publish on its
     prefix, `viewer` read and playback, `takml` read, and the API and metrics only from
     localhost or the cluster CIDR. Passwords go in a SealedSecret. Getting them into
     MediaMTX by environment override is **unverified**, so test it before relying on it.
   - Phase B: `authMethod: jwt` against Authentik's JWKS, with `mediamtx_permissions` claims
     per group. Clients pass `?token=` or Bearer (fact C). This fits the zero-trust layer
     (OpenFGA) better than static users.
   - Do not deploy the media-infra wrapper until CloudTAK media is actually wanted, because it
     moves all auth to CloudTAK leases and prunes paths without a lease (L2 lines 163–167).
5. **Recording.**
   - fMP4 for video-only paths, which suits the playback API.
   - MPEG-TS for paths that carry KLV: proven to keep KLV (R1). Whether fMP4 keeps it is
     unverified.
   - Retention per class: drones 7 days, owned cameras 3 days, public cameras none. Use
     `recordDeleteAfter`, with `%z` in `recordPath` (R1 warning).
   - Storage is a PVC on `do-block-storage-retain` (omos rule 6). Size it from measured
     bitrate. At 4 Mb/s, one stream is about 1.8 GB/h, so 7 days of one drone flying 2 h/day
     is about 25 GB. Exclude recordings from VolSync unless the owner wants them backed up,
     because off-site copies of video cost money (decision 0004 #5).
6. **Registry (K2).** Use MediaMTX hooks (`runOnReady`/`runOnNotReady`, fact C) or the API.
   - A small omos `video-registry` service writes each stream into the owning entity's state
     on `ent.{entity_type}.{entity_id}.state`. The `video` block holds URLs per protocol,
     codec, latency class, `klv: bool` and permission scope.
   - It also mirrors availability into the existing KV `OVERWATCH_VIDEO` (L7), so no new
     bucket is needed.
   - It publishes `evt.video.{entity_id}.online|offline`. All subjects use existing grammar
     patterns: `ent.*.state` and `evt.{domain}.{entity}.{event}`.
7. **KLV (K5).** A Go `klv-extractor` reads SRT (MPEG-TS) or RTSP with a KLV-capable library,
   because ffmpeg cannot depacketize RTSP KLV (R1). It parses ST 0601 using the tag maps and
   checksum proven in R1 (W2), then publishes two subjects:
   - `tel.{node}.drone.sensor`: sensor lat/lon/alt, platform heading/pitch/roll, sensor
     relative az/el, HFOV/VFOV, slant range;
   - `tel.{node}.drone.footprint`: frame centre (tags 23/24) and the four corners (tags
     82–89, or 26–33 offsets). When the platform sends no corners, compute them by
     ray-casting the FOV frustum against terrain.

   These follow `tel.{node}.{class}.{metric}`, as `standards.md` also proposes. *Verifier:*
   gortsplib, which MediaMTX itself uses, has a public RFC 6597 KLV format and RTP decoder:
   `format.KLV.CreateDecoder()` returns an `rtpklv.Decoder`, and its `Decode` method turns RTP
   packets into KLV bytes (W9, read from `main`). So a Go RTSP reader can get KLV, but nobody
   has tested that end to end, and that the API exists in the tagged v5.6.7 is **unverified**.
   jMISB (MIT, Java) is the fallback (W8 standards).
8. **TAK (K3, H9).** The entity projector adds `<__video url=…>` with a nested
   `<ConnectionEntry protocol="rtsp|srt|https" …/>` and a `<sensor azimuth fov vfov range/>`
   to the drone or camera CoT (fact D). It registers each stream with TAK Server
   `POST /Marti/api/video` (FeedV2 with lat/lon/fov/heading/range) through the TAK client
   lane (W8 tak-protocols recommendation 3).
   - Give ATAK an MPEG-TS-carrying URL (SRT or RTSP) when the stream has KLV, so ATAK's own
     KLV footprint works.
   - CloudTAK keeps media off (fact A) until the README's replacement gate is done.
9. **Unreal on macOS (K4, E2).** Two players behind one `OneMindVideo` plugin, with one
   `UMediaTexture` per stream on a world-space panel attached to the entity actor:
   - **Situational class (2–5 s):** stock Media Framework with Electra on HLS. It works on
     macOS and Android (fact E), but ignores LL-HLS parts, so latency is several segment
     durations (estimate, **unverified**).
   - **Tactical class (≤1 s, K4 target):** a native decoder path, either RTSP/SRT through
     libavformat with VideoToolbox (macOS) and MediaCodec (Android), or WHEP through a
     WebRTC library, feeding a dynamic texture. It needs a spike: this machine has no Unreal
     (PREFLIGHT), so it is an owner-checklist item.
   - On macOS, AVFoundation (AvfMedia) could play LL-HLS natively (fact E, Apple side), but
     whether UE 5.8 still ships and supports the AvfMedia player is **unverified**.
10. **Headset (Quest Pro, standalone Android/OpenXR per decision 0004 #2).** Electra on
    Android plays HLS (fact E). The ≤1 s path is the same native decoder plugin with
    MediaCodec. Show panels as world-locked quads or OpenXR quad layers (**unverified** for
    UE 5.8 on Quest).
11. **Video draped on the 3D twin (E4, K5).**
    - Step 1: the frustum and footprint polygon from `tel.*.drone.footprint`, plus a "monitor
      plane" at the frustum cap (the gods-eye-view pattern, MIT, fact F).
    - Step 2: true projective texturing. A material computes each pixel's UV by transforming
      its world position with the camera's view-projection matrix (built from KLV pose and
      FOV) and samples the video texture there, applied through a deferred decal volume over
      the footprint.
    - Whether Cesium for Unreal tile meshes receive deferred decals is **unverified**, so the
      first S1 run must test it. The fallback is a Cesium material-layer hook.
12. **Latency targets.**

    | Class | Target (glass-to-glass) | Transport |
    |---|---|---|
    | Control/FPV | ≤ 300 ms LAN | WebRTC (WHEP) |
    | Tactical | ≤ 1 s LAN (K4), ≤ 2 s WAN | RTSP/SRT native, or WebRTC |
    | Situational | 2–5 s | LL-HLS (web, Apple), HLS (Electra) |
    | Archive | n/a | playback API (fMP4) |

    Measured so far: LL-HLS server packaging adds 0.06–0.20 s, and players must hold back at
    least 0.5 s (R1). All glass-to-glass numbers are owner-checklist measurements.

### Plugin model (K6)

One manifest, four plugin kinds, one registry on the bus. Every host reads the same manifest
and ignores the parts it cannot run.

```yaml
# onemind-plugin.yaml (schema: onemind-xr/docs/contracts/plugin-manifest.schema.json, to be written)
id: org.onemindos.layer.usgs-quakes      # reverse-DNS, unique
version: 1.2.0
kinds: [source, layer]                   # source | layer | action | skill (one or more)
trust: verified                          # built-in | verified (signed, reviewed) | community (declarative only)
licence: MIT                             # checked against the GPL-3.0 client
compat: { onemind: ">=0.1", omos: ">=2026.10" }
requires: { secrets: [] }                # names only; values come from SealedSecrets or the keychain
bus:                                     # validated against the locked grammar in CI
  publishes:  ["intel.hazard.usgs.quake", "ent.quake.*.state"]
  subscribes: []
  kv: { snapshot: "kv-world-state" }
source:  { image: "ghcr.io/onemindos/collector-usgs@sha256:…", schedule: "60s", delivery: rest-poll }
layer:   { from: "ent.quake.*.state", render: { entity: point, sizeField: magnitude, maxEntities: 5000, lod: on-demand } }
action:  null                            # e.g. { subject: "cmd.drone.{id}.orbit", schema: …, policy: openfga:drone#task, confirm: human }
skill:   null                            # e.g. { hermes: "plugins/onemind-quakes", rpc: "svc.quakes.query" }
video:   null                            # e.g. { source: "rtsp://…", path: "cam/gate-north", latencyClass: tactical }
```

- **source:** an omos collector. It runs as a pinned GHCR container under an Argo
  Application (omos checklist), or on whatever ingestion platform the ingestion lane picks.
  It publishes only to the declared subjects. NATS account permissions are generated from
  `bus.publishes`, so a plugin cannot publish outside its manifest.
- **layer:** declarative rendering that maps subjects or KV snapshots to entity type,
  symbology, LOD and caps. The same layer renders in Unreal (`OneMindLayers`), the L0 CesiumJS
  client and ATAK (as CoT through the projector, or natively inside `OneMindATAK`). Code
  layers are only allowed at trust `built-in`/`verified`:
  - in Unreal, as a compiled `OneMind*` plugin;
  - on the web, as an ES module;
  - in ATAK, inside `OneMindATAK`.

  The pattern of injected services plus a source interface (`getCatalog/getHealth/getMediaUrl`)
  comes from gods-eye-view (MIT).
- **action:** a `cmd.{domain}.{entity}.{action}` handler with a JSON schema. It always passes
  policy (OpenFGA), audit and a human-confirm step in the UI (run rule 6). The UI binding is
  generated from the schema.
- **skill:** a Hermes plugin in the existing `plugin.yaml` format (L8), exposed over NATS RPC
  `svc.{service}.{endpoint}`. The manifest declares tools (name, description, input schema)
  so Hermes and MCP clients can list them, an idea worldwideview also uses (`mcpTools`, design
  only). Skills that act go through the action path.
- **Registry and discovery:** manifests are published to a KV bucket (one new `KeyValue` CR in
  `platform/nats/streams/`, through an omos PR), and clients list them with
  `svc.plugins.list`. Both are existing grammar patterns.
- **Hosts:**
  - Unreal does not load plugin code at runtime (VH_Plugins needs engine patches, fact F).
    Data-driven layers load at runtime, and content-only packs use
    `MountExplicitlyLoadedPlugin`.
  - ATAK gets a single `OneMindATAK` APK that reads the same manifests, not one APK per
    plugin.
  - Hermes installs skills through GitOps, closing the hand-install gap in L8.

## Risks

1. **Exposure.** Publishing the cluster MediaMTX on the tailnet or the internet with today's
   config (no auth, `apiAllowOrigins: *`) would let anyone publish or overwrite streams
   (`overridePublisher` defaults to true, R2). Auth must land before any ingress.
2. **WebRTC beyond HTTPS.** WebRTC needs a reachable ICE transport (UDP/TCP 8189 or TURN).
   HTTPS ingress alone does not carry it (L2 line 177). A public UDP port or a TURN server is
   new attack surface, and possibly cost.
3. **Memory and storage.** The pod sits on the memory-saturated hub-core pool (K1). Remuxing
   several streams, with transcoding in the `-ffmpeg` image, can exceed the 512 Mi limit.
   Recording storage grows with bitrate (fact B) and is billed.
4. **Unreal latency.** Electra cannot meet the ≤1 s K4 target with LL-HLS (fact E). The
   native decoder plugin is real engineering and has not been built or measured.
5. **KLV.** The KLV chain is untested on real drone video. Different encoders send
   async/sync KLV, other ST 0601 versions and tag subsets. Footprint accuracy also depends on
   the terrain model.
6. **CloudTAK media.** Cutting over to the media-infra wrapper breaks the takml worker's named
   paths and turns on lease-based path pruning (L2 lines 163–167).
7. **Licences.** ffmpeg builds that include GPL parts are fine for the GPL-3.0 client, but
   not for any future non-GPL component. worldwideview stays design-only (Elastic-2.0).
8. **Floating image tag.** `bluenviron/mediamtx:1-ffmpeg` can change underneath a restart.

## Recommendation

1. **omos PR "onetak-media: auth, recording, registry" (K1 and K2; owner merges):**
   - pin the MediaMTX image to a v1.21.3 digest;
   - add `internal` users from a SealedSecret (phase A), path allowlist regexes
     (`drone/…`, `cam/…`), `apiAllowOrigins` restricted, API and metrics limited to the cluster;
   - turn on `record` per path class (fMP4; MPEG-TS for KLV paths), with `%z` and
     `recordDeleteAfter`, on a `do-block-storage-retain` PVC;
   - expose RTSP, SRT, LL-HLS and WHIP **only on the tailnet** (Tailscale Service, as for
     TAK);
   - target the ecosystem pool;
   - move takml `FEEDS` to the new paths and credentials, and its detections to
     `det.{domain}.{type}.{id}`;
   - fix the Hermes runbook and the Mac push plist.

   Keep CloudTAK media off and record that decision in the CloudTAK README.
2. **Edge MediaMTX** on the owner's Mac or home server, using the lane's config
   (`docs/evidence/M0/video/mediamtx-lane.example.yml`) with `forward` to the cloud over SRT.
   This is a script plus an owner-checklist step.
3. **`video-registry` and `klv-extractor`** (Go, omos):
   - `video-registry` writes the entity `video` block, `OVERWATCH_VIDEO` and
     `evt.video.*`;
   - `klv-extractor` publishes `tel.*.drone.sensor` and `tel.*.drone.footprint`;
   - test both locally with `klv_ts_gen.py` and `klv_decode.py`.
4. **TAK projector:** add `__video` + `ConnectionEntry` + `sensor` to the CoT, and register
   feeds through `/Marti/api/video`.
5. **Client:**
   - `OneMindVideo` with Electra for HLS now, plus a spike on a native RTSP/SRT/WHEP decoder
     for ≤1 s (macOS VideoToolbox, Quest MediaCodec);
   - footprint and monitor plane first (E2, E4), then a projective decal over Cesium tiles
     once S1 shows decals work on them;
   - the L0 web client uses WHEP or LL-HLS (hls.js).
6. **Plugins:**
   - write `plugin-manifest.schema.json` with the four kinds above, a CI check that validates
     `bus.*` against the locked grammar, and generation of NATS permissions from it;
   - ship one example per kind (K6): a USGS quake source+layer, a `cmd.drone.*.orbit` action
     on `sim-*`, and a Hermes `svc.*` skill;
   - do not port VH_Plugins.

## Unverified items

- Whether MediaMTX accepts nested `authInternalUsers` through environment variables (needed to
  keep passwords out of the ConfigMap). The alternative is mounting the whole config from a
  SealedSecret.
- Whether an authenticated publisher is refused on a *configured* path outside its permission
  regex (the R1 test hit an unconfigured path and got 400).
- Whether fMP4 recording keeps KLV (only MPEG-TS recording was tested).
- Whether WebRTC reading of KLV works in a browser (documented, W1, not tested).
- ~~Whether gortsplib exposes KLV to applications~~ (verifier: it does on `main`, W9). Still
  unverified: that the tagged v5.6.7 has the same API, and an end-to-end Go KLV read.
- Whether ATAK 5.6 matches the CIV-mirror protocol list and reads KLV from RTSP or SRT streams;
  whether ATAK renders a KLV footprint from a OneMind stream.
- Unreal Electra latency on HLS, whether AvfMedia is available in UE 5.8, OpenXR quad layers on
  Quest Pro in UE 5.8, and deferred decals on Cesium for Unreal tiles. None can be tested here
  (no Unreal on this machine, PREFLIGHT).
- Glass-to-glass latency for every class. Only server-side LL-HLS packaging lag was measured.
- Who, if anyone, writes the live `OVERWATCH_VIDEO` bucket (no writer in the omos repo).
- SRT ingest from an external SRT encoder: proven only with MediaMTX's own SRT client, because
  Homebrew ffmpeg has no libsrt.
- Blockers this lane hit: none from the permission system. Tool limits: no `timeout` on macOS,
  and Homebrew ffmpeg has no SRT.

## Independent verification (2026-10-29)

A separate verifier re-checked 18 load-bearing claims against the primary sources:

- MediaMTX v1.21.3 release date; the `docs/` codec tables for KLV (SRT, RTSP, WebRTC, "MPEG-TS-based HLS only"); the auth docs (Bearer takes `user:pass`; `?token=`).
- The reference `mediamtx.yml`: `record: false` on line 576, `overridePublisher: true`, the forward targets (RTSP, RTMP, SRT, MoQ, WHIP) and the path sources.
- The raw lane logs: the v1.21.3 banner, the `%z` warning, `BANDWIDTH=7930320`, `PART-HOLD-BACK=0.50000`, the 401 on the first WHIP attempt and the missing `srt` protocol in ffmpeg 8.1.1. The readback PNG is identical to the run's frame (1280×720).
- The omos files L1–L7 at the cited lines.
- The cluster, read only: Deployment 49 days, pod 10 days on `onemind-hub-core-v2-3rtf4m`, the `1-ffmpeg` image, ClusterIP only, no media Ingress or other media Service, the takml poll every 10 s, both Applications Synced/Healthy, and KV `OVERWATCH_VIDEO` Ready.
- The dfpc-coe media-infra v9.12.1 routes: `/path`, `/path/:path`, `/v3/config/global/get`, `/stream/:stream/:type.m3u8`, port 9997 and `SigningSecret`.
- The Electra UE 5.8 page: platforms, the unsupported `EXT-X-PART`, `PART-INF` and `PRELOAD-HINT` tags, and `SERVER-CONTROL` as partly used.
- The ATAK-CIV `ConnectionEntry.Protocol` enum and the `VideoDetailHandler` attributes, including the `networkTimeout` default of 5000.
- The klvdata (MIT) UAS key and `packet_checksum`, which the scripts match.
- The licences: worldwideview is Elastic-2.0 and gods-eye-view is MIT.

Corrections are marked inline: the recorded duration, the macOS version cited from W5, the log window behind "nothing is publishing", and the gortsplib KLV API (W9).

No violations of the licence rule (worldwideview is described, not copied) or of cluster safety (get and logs only, no Secrets) were found. The committed evidence redacts the test credentials. The scratch file `work/.run/video-lane/p_srt.err` (mode 644, not committed) still holds a random local test credential in an SRT URL. Delete it or redact it before anyone shares `.run/`.
