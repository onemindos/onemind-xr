# M2 lane B (client): evidence, 2026-10-29

Branch `run/m2-client` (worktree `work/wt/b-client`), code under `client/OneMind/`. Build machine:
M1 Max, macOS 26.5, Apple clang 21.0.0, CMake 4.4.3, nats-server 2.15.2, Go 1.27.3. **No Unreal
Engine** (PREFLIGHT.md), so nothing below was compiled or run inside Unreal.

## What ran here

| File | What it shows | Result |
|---|---|---|
| `host-test-suite.txt` | `HostTests/scripts/run-host-tests.sh`: C++17 and C++20 builds with `-Wall -Wextra -Wshadow -Werror -fno-exceptions -fno-rtti`, unit, integration, ASan+UBSan unit and integration, 10k load, C1/C2 e2e | all passed, exit 0 |
| `unit-tests.txt` | 31 unit tests (JSON, codecs, RFC 6455 framing, NATS parser and client on a scripted transport, header CR/LF and `:` rejection, slow-consumer queue cap with reconnect, JetStream ack metadata, XML, CoT and the VH_COT port, MIL-STD-2525 port, entity store, inspector rows, render plan, layer watch path on a scripted service) | 31/31, 728 checks |
| `integration-tests.txt` | 8 tests against disposable nats-servers over the WebSocket listener: pub/sub + request/reply + headers + no-responders + 512 KiB payload; KV get (Direct Get, 404, STREAM.MSG.GET fallback) and watch with delete; reconnect after a server kill/restart; ordered consumer on an `ENT` stream (ent.yaml settings) with snapshot, live update, `live:false` and a `Nats-TTL` delete marker; layer off leaves no subscription or consumer on the server (`/connz`, `/jsz`); layer watch path against a contract fixture (snapshot chunks, deltas, AOI renewal with enter/exit, batch gap → re-snapshot and unwatch of the broken watch, toggle off → unwatch); dead watch → re-snapshot after 30 s; 10,000 entities over `svc.layer.snapshot`; camera AOI on direct layers (a small layer keeps all 400 entities whatever the view; a large direct layer holds only the view and reloads entities on a pan and on a pan back) | 9/9, 99 checks |
| `load-10k/run.txt` | `Tools/entity_load.py` publishing 10,000 `sim-load-*` drones at 1 Hz into `ENT`, headless client on the fleet layer (direct path) | 10,000 entities, snapshot 209 ms, 150,000 updates applied in 15 s, 0 dropped or reordered |
| `load-10k/run-camera-aoi.txt` | Same load with `--camera-aoi -97.7931,30.2240,-97.6931,30.3104`, the box `UpdateAoiFromCamera` computes at the default start (Austin, 8 km), applied through `LayerManager::SetAoi` as the Unreal subsystem does | 10,000 entities (before the review fix: 93) |
| `e2e-c1-c2/run.txt` (+ `headless-c1.txt`, `headless-c2.txt`, gateway and entity-service logs) | omos mesh gateway (PR #551, `bef11eb1`) and entity service (PR #552, `886596b1`) built from their branches; `takproto` mesh peer; svc.cot.send stand-in; OneMind client core | 12/12 checks |
| `e2e-c1-c2/cp3-puback-probe.{py,txt}` | A request on `tak.cot.mesh.tx` is answered by JetStream's PubAck when `COT` binds `tak.cot.>` | confirmed (5 of 5 first replies were PubAcks) |

Key numbers (from the files above): KV put → client watch callback 2–3 ms (B3 target 1 s);
reconnect 1.57 s after a server that was down 1.5 s; Nats-TTL 1 s → removed by the delete marker
after 1.00 s; 10,000-entity snapshot over `svc.layer.snapshot` 83 ms; 30,000 delta upserts in 0.31 s;
render planning (bands, horizon, clusters) for 10,000 entities 0.6 ms per frame on the CPU.

## Features

| Feature | State | Build-machine proof | Owner check |
|---|---|---|---|
| B1 UE 5.8 project on the Mac | built as source, awaiting owner check | — (no Unreal) | OWNER-STEPS §1 |
| B2 globe, open terrain/imagery, offline cache | built as source, awaiting owner check. Ellipsoid by default (no keyless open terrain endpoint verified); OSM raster; Cesium SQLite cache raised to 200,000 items | — | §1 steps 3–4 |
| B3 OneMindBus | core done and tested (WebSocket transport, subscribe, KV get/watch, request/reply, reconnect); UE transport written, not compiled. nats.c TCP transport not included (blocker 3) | unit + integration tests | §2 |
| B4 layers: snapshot + delta, toggle subscribes/unsubscribes | core done and tested (watch, direct, core paths); UE subsystem written | integration tests incl. server-side subscription counts; 10k load | §3 |
| B5 batched rendering | LOD/cluster/horizon plan tested; UE ISM renderer written | `render_plan_*` unit tests | §3 steps 2–3 |
| B6 inspector | rows and raw-CoT lookup tested; Slate overlay written | `inspector_rows_*`, C1d, C1e | §3–4 |
| C1 TAK entities in the client | passes on the build machine | e2e C1a–C1e | §4 |
| C2 send CoT | passes on the build machine with a `svc.cot.send` stand-in (blocker 1) | e2e C2a–C2d | §5 |
| F1 packaged, signed for local use, `org.onemindos.onemind` | config written (bundle id key unverified) | — | §1 step 5 |

## Review fixes (independent review of PR #3), 2026-10-29

| Finding | Fix | Proof |
|---|---|---|
| major: the camera AOI emptied direct small layers (fleet held 93 of 10,000 at the default start; static markers and zones never came back) | `LayerSpec::UsesCameraAoi()`: only watch-path and `large` layers follow the camera. Small/medium direct and core layers load whole (layer-snapshot.md §1). A client-filtered layer (direct/core `large`) drops what left the view and re-runs its snapshot on every AOI change, so entities that come back reload | `it_direct_layers_camera_aoi_small_keeps_all_large_reloads_on_pan`; `load-10k/run-camera-aoi.txt` |
| major: owner steps used a Shipping package (no console, no `stat`, no UE_LOG, no trace) | `build-and-run.sh package` builds **Development** by default (`CONFIG=`), into `$OUT/<Config>`; `package-shipping` only for F1; `DefaultGame.ini` `BuildConfiguration=PPBC_Development`. The overlay's first line shows `fps`, average and worst frame time (5 s window), so B1/B4 do not need `stat`. OWNER-STEPS names the config per step | not compiled (no Unreal) |
| minor: unbounded `events_`/`ops_` queues | `NatsOptions::max_pending_bytes` (64 MiB) over the transport queue and parsed-but-undispatched messages; past it the client closes the connection as a slow consumer and reconnects (layers re-snapshot on reconnect); counted in `Stats::slow_consumer_closes` | `nats_client_slow_consumer_cap_closes_and_reconnects` |
| minor: per-entity `FString` in the renderer, Niagara deviation unrecorded | the selection is resolved once per layer per rebuild to an index; no strings in the per-entity loop. ISM icons recorded as B-P8 | not compiled (no Unreal) |
| minor: header injection | `NatsHeaders::Add` rejects CR/LF/NUL in values and `:`/space/control characters in keys; `Serialize`/`HPub` return empty for invalid headers and `Publish`/`Request` fail (`bad_request`) | `nats_headers_reject_crlf_and_colon` |
| minor: layer catalogue differs from layer-snapshot.md §1 | recorded as B-P9; note in `DefaultGame.ini` | — |

## VH_COT port and licence (vrtak-porting-map.md Rec. 5)

`gh api repos/TAK-Product-Center/VR/contents/LICENSE?ref=4bd384978b48b0a46e7f17b272933a152a61c013`
returns the GNU GPL v3 text (`vh-cot-licence.txt`; the repo API reports `GPL-3.0`); the plugin's `CreatedBy` is "Virtual
Heroes, a division of ARA". GPL-3.0 allows porting into this GPL-3.0 repo with attribution, so the
event model API (`VH_COTEvent`) and the 2525 id helpers (`VH_MilStd2525`) were ported into
engine-independent C++ (`om::CotEvent`, `om::MilStd2525`), with file headers and
`client/OneMind/THIRD_PARTY.md`. Commo, Gv2F, EnginePatch and assets were not copied.

## Blockers

1. **`svc.cot.send` does not exist in omos.** The projector endpoint (cot-projection.md §9) is
   unwritten; C2 was proven with a stand-in that applies the same gates. Lane A needs to add it to
   the projector (body proposed in `client/OneMind/Docs/svc-cot-send.md`).
2. **No Unreal Engine on the build machine.** All engine-side code (5 plugin modules plus the game
   module, about 1,900 lines) is uncompiled; the 5,600-line core it uses is compiled and tested. First compile is OWNER-STEPS §1 step 2.
3. **nats.c transport not built.** FEATURES B3 names nats.c v3.14.0; its build was denied by the
   permission policy in M0 (PREFLIGHT). The client uses the ROADMAP S5 fallback (WebSocket from C++)
   for both Mac and headset; a nats.c transport can sit behind `om::ITransport` later.
4. **Layer service (omos A5) not written.** The watch path was tested against a contract fixture in
   `HostTests/tests/it_layers.cpp`; the default Mac config uses the direct `ENT` path for small layers.
5. **Open terrain source not chosen.** No keyless public quantized-mesh endpoint was verified; the
   globe uses the ellipsoid unless `TerrainUrl` is set (decision 0004 #4 says open terrain).

## Owner proposals (decision 0004 #8 style; none changes a subject pattern)

- **B-P1** Markers, routes and geofences: FEATURES C2 sends them through `svc.cot.send`;
  cot-projection.md §9 sends persistent items as entities via `entity.create` in policy and keeps
  `svc.cot.send` for ephemeral items. Proposed: persistent items via `entity.create` (bus holds them,
  repo rule 3), `svc.cot.send` for chat, SPI and quick markers with a short stale. The client supports both.
- **B-P2** FEATURES B3's test does a `kv put` on `world-state`, which BUS-DESIGN D3 reserves for
  layer manifests. Proposed: reword the B3 test to a `probe.*` key (as implemented) or to an `ENT` publish.
- **B-P3** CP-3 evidence: a request on `tak.cot.mesh.tx` gets JetStream's PubAck first because
  `COT` binds `tak.cot.>`. Proposed: the projector core-publishes (no reply) and confirms through the
  gateway's `tx` counter; the gateway README's "request/reply gets ok" should note this.
- **B-P4** Adopt the `svc.cot.send` v1 body in `client/OneMind/Docs/svc-cot-send.md` for the projector.
- **B-P5** layer-delta.md §2.1 does not say where batches go after a renewal. The client and its
  fixture assume batches keep flowing to the first request's inbox and the renewal gets one ack;
  A5 should state and implement the same.
- **B-P6** Accept the WebSocket transport as the B3 transport for M2 (S5 fallback), with nats.c as a
  later option, or re-allow the nats.c build.
- **B-P7** Default imagery is the OpenStreetMap tile server (policy-limited, single-user testing).
  Proposed: point `ImageryUrlTemplate` at the omos caching tile proxy once it exists, and pick a
  terrain source (self-hosted quantized mesh or WebODM 3D Tiles) for `TerrainUrl`.

- **B-P8** FEATURES B5 names Niagara icons for the far band. The renderer draws far entities as
  instanced static-mesh spheres scaled with camera distance (constant screen size), coloured by
  symbol class, and clusters as larger spheres. Reason: ISMs need no assets (the project has no
  content yet) and batch 10,000 instances in one component per class. Proposed: accept ISM icons
  for M2 and replace them with Niagara sprite icons (2525-style billboards) as a follow-up once the
  project has a content folder; the renderer's band plan (`om::PlanRender`) does not change.
- **B-P9** The layer catalogue in `DefaultGame.ini` differs from layer-snapshot.md §1, whose table is
  marked provisional until ONTOLOGY.md fixes the vocabulary. ONTOLOGY.md (§3, `event`, `alert`,
  `track`) now does, so the contract is the stale side. Proposed contract rows: `hazards` =
  `event alert` (ONTOLOGY has no `hazard` type; hazard areas are `zone` with `geo.kind=hazard`);
  `tak` = `person place track`: the entity service (#552, `services/entity` CoT parser) turns
  non-EUD ground CoT, such as markers drawn on ATAK (`a-h-G`, `a-u-G`), into `track`, so without it
  ATAK markers would show in no layer; TAK shapes stay in `zones` (`zone`) so they are not drawn
  twice; `fleet` = `drone robot vehicle node`, adding `edge` back when an `edge` entity producer
  exists (ONTOLOGY lists edge boxes as `node`).

## Not verified

Everything engine-side (see README "Verified vs not verified"), the GeoChat element layout on real
ATAK, ATAK rendering of the `a-u-G` marker, and frame rate on the Mac.
