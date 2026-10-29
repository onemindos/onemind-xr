# OneMind L0 light web client (FEATURES J1)

A CesiumJS globe in any browser, phone included, on the **same NATS contracts as the Unreal
client** ([`docs/contracts/`](../docs/contracts/)). It is the decision 0004 #1 fallback for the
web path and is built either way (decision 0002; ROADMAP M3). It is a view and an input device
only: it never publishes `ent.*` or `cmd.*` (entity.md §2; onemind-xr `CLAUDE.md` rule 3).

## What it does

| Capability | How | Contract |
|---|---|---|
| Globe, no ion token, no paid API | CesiumJS 1.146.0; imagery: Natural Earth II bundled with CesiumJS (offline default) or OpenStreetMap tiles (`?imagery=osm`); terrain: none (default) or AWS Terrain Tiles, terrarium encoding, decoded in the browser (`?terrain=terrarium`, `src/ui/terrain.ts`). The geocoder and base-layer picker are off because they use Cesium ion | — |
| Bus over WebSocket | `@nats-io/nats-core` 3.4.0 `wsconnect` (REFERENCE-ARCHITECTURE.md §8 L0 row) | — |
| Layer snapshot + delta | `svc.layer.snapshot` then `svc.layer.watch` from `ent_seq + 1`, lease renewal at half the lease, gap and 30 s dead-watch detection, re-snapshot (`src/bus/layerclient.ts`) | layer-snapshot.md §3.1, layer-delta.md §2 |
| Interim path | When no layer service answers: core subscription to `ent.{type}.*.state` first, then a `svc.entity.query` snapshot paged by `cursor` (the reply shape of omos PR #552, `services/entity`) | entity.md §8 |
| Core-only path | When no service answers at all: `ent.{type}.*.state` only | entity.md §8, layer-delta.md §1 |
| Entities, removal and expiry | `seq` rule (older or equal ignored), `live: false` removes, `expiry_t` expires (core subscribers never see delete markers), `stale` dims, tombstones stop a late copy resurrecting a removed entity (`src/bus/store.ts`) | entity.md §5, §8; layer-delta.md §2.2, §3 |
| Marking check | defence in depth: records whose `marking` the viewer may not see are not drawn (`?groups=`, default `homestead`) | entity.md §8 |
| Mission tier | view filter: tasked, or inside an AOI or geofence zone (FEATURES A6 rule) | FEATURES A6, J1 |
| Alerts | `ent.alert.*` entities, entity `health.alerts[]`, and `evt.map.geofence.{id}.{enter\|exit}` | ONTOLOGY.md §3; omos NATS-DOCTRINE |
| Entity detail with provenance | full record from the feed or `svc.entity.get`; shows `provenance`, `sources[]`, `confidence_parts`, `conflicts` | entity.md §4.1, §8 |
| Video panel | from the entity's `media[]` (MediaMTX URL, never NATS): WHEP first, LL-HLS second (hls.js 1.7.3, or native HLS on iOS Safari). A loopback host is replaced by the page's host so a phone on the LAN reaches the Mac | entity.md §4.1 `media`; video-and-plugins.md |
| Phone layout | globe on top, tabbed bottom sheet, 44 px touch targets, no horizontal scroll | J1 acceptance |

Subjects it reads: `ent.{drone,robot,vehicle,node,track,zone,alert,person,place,hazard,event,aircraft,vessel}.*.state`
(only on the interim and core paths), `evt.map.geofence.*.*`. Requests it sends: `svc.layer.snapshot`,
`svc.layer.watch`, `svc.layer.unwatch`, `svc.entity.query`, `svc.entity.get`. All are existing
grammar patterns (omos `CLAUDE.md`); no new subject is introduced.

## Run it

```bash
cd web
npm ci                     # exact pins from package-lock.json (.npmrc: legacy-peer-deps, see below)
npm run build              # typecheck + production build into dist/
npx vite preview --host 127.0.0.1 --port 4173
# then open http://127.0.0.1:4173/?nats=ws://127.0.0.1:9222
```

Something must publish entities. Without the omos services, use the fixture publisher:

```bash
node tools/fixture-publisher.mjs --nats ws://127.0.0.1:9222 --mode entity \
  --video rtsp://127.0.0.1:8554/drone/sim-x1
```

Modes: `core` (only `ent.*`), `entity` (+ `svc.entity.query/get`, PR #552 shapes), `layer`
(+ a stand-in `svc.layer.snapshot/watch`). Every record is simulated (`sim-*`).

Page parameters: `nats` (WebSocket URL; default `ws://{page host}:9222`), `groups`, `mission=1`,
`imagery=osm`, `terrain=terrarium`, `layers=fleet,tracks,…`, `whep` and `hls` (MediaMTX base
URLs), `video=whep|hls`, `select={entity_id}`, `lon`/`lat`/`height`. Credentials are never URL
parameters: type them in the Connection tab (kept in `sessionStorage` for the tab only).

Running on the owner's Mac and phone: [`docs/evidence/M3/J1/OWNER-STEPS.md`](../docs/evidence/M3/J1/OWNER-STEPS.md).

## Tests

```bash
npm run typecheck
npm test                   # vitest: unit tests (merge rules, layer client, mission tier, alerts, video URLs)
                           # + integration against a disposable nats-server over WebSocket (3 fixture modes)
PLAYWRIGHT_BROWSERS_PATH=/path/to/ms-playwright npx playwright install --only-shell chromium
npm run test:e2e           # Playwright, chromium only: desktop and phone viewports, WHEP and LL-HLS video
L0_ONLINE=1 npm run test:e2e   # adds the OSM + terrarium terrain check (internet)
```

The integration and e2e tests start their own `nats-server` (JetStream, WebSocket) and MediaMTX
on free 127.0.0.1 ports with temporary stores under `$ONEMIND_WORK/.run/tmp-j1` and an ffmpeg
test pattern on `drone/sim-x1`; they never touch the shared session services. Binaries are found
through `NATS_SERVER_BIN`, `MEDIAMTX_BIN`, `FFMPEG_BIN`, `work/.tools/bin` or `PATH`.

The e2e browser runs with `--in-process-gpu` because the separate GPU process hangs on the build
machine ([webgl-probe.txt](../docs/evidence/M3/J1/webgl-probe.txt)).

## Act features (M4: D1, D3, B11, B20, D4, D6, J2, C8)

The Act panels live in `src/act/` (logic, unit tested) and `src/ui/act.ts`, `src/ui/overlay.ts`
(DOM and Cesium). The client still never publishes `cmd.fleet.*` or `ent.*`.

| Feature | What the operator does | Bus (all existing grammar) |
|---|---|---|
| D1 AOIs and geofences | Draw an area, pick a kind (`aoi`, `keep_in`, `keep_out`, `hazard`, `control_area`), Share as zone. Keep-in and keep-out ask for a confirm first | publishes `cmd.map.geofence.{id}.create\|update\|remove` (doctrine §9.7); the body carries the doctrine fields plus the zone record KV `geofences` stores, with `geo.shape` `polygon` or `circle` only (the policy service treats any other keep-in/keep-out shape as unevaluable) |
| D3 click to task | Select a `sim-*` asset, Go to point, click the map. The confirm dialog shows the exact parameters, the `params_hash`, the level and the deadline; Confirm or Reject (with a reason). Status updates live in Tasks and Detail | `svc.policy.command.propose`, `svc.policy.command.confirm`, `svc.policy.action.get` (polled), `evt.so.action.>` when the connection may subscribe |
| B11 drawing | Point, label, line, polygon, rectangle, circle, sector, range rings, range and bearing, lasso select, buffer; measurements on the map; CoT export (copy or download) | none until shared; CoT types per cot-projection.md §5.5 (`src/act/cot.ts` header) |
| B20 intelligence gaps | Gaps tab: stale, failing, silent or never-fetched feeds and down layers, each naming the layers and alerts it weakens | `tel.feed.*.status` (omos #553 shape) when permitted; layer manifests' `freshness` otherwise |
| D4 Hermes | Ask about the map; the question goes with the selection, viewport, layers, visible entities (≤ 50), zones and gaps. Hermes' highlights and annotations show on the map; its proposals appear as pending confirms | request (single reply or chunks) on `agents.prompt.hermes.zeus.legacy` (`?hermes=` to change); renders `cmd.map.highlight\|annotation\|viewport\|filter` |
| D6 alerts | Alerts tab adds stale own assets and feed outages to J1's geofence crossings and alert entities; Dismiss hides one for this viewer | as above |
| J2 role modes | Mode: Newcomer (view, chat, Hermes), Observer (+ draw, share zones), Operator (+ asset commands, confirm). Operator only takes effect when a policy dry run says the user operates the asset | `svc.policy.check` with `{v, action}` (dry run of `asset.estop`, which skips asset preconditions, so the answer is authorization only) |
| C8 contacts and chat | Chat tab: TAK contacts, channels (TAK groups), All Chat Rooms, group and direct rooms; send GeoChat | reads CoT XML on `tak.cot.out` and `tak.cot.mesh.rx` when permitted; sends `svc.cot.send` (structured body, the projector builds the CoT) |

A `CLIENT` account user in the omos #562 layout may subscribe only `cmd.map.>` and `_INBOX.>`:
the client then falls back to polling (`svc.policy.action.get`, layer manifests) and the Tasks
tab shows which subscriptions were refused. Chat and direct feed status need the owner proposals
in [`docs/evidence/M4/WEB/OWNER-PROPOSALS.md`](../docs/evidence/M4/WEB/OWNER-PROPOSALS.md).

### Act tests

```bash
npx vitest run test/unit/act.test.ts      # geodesy, drawings, CoT, policy tracker, roles, gaps, Hermes, chat
npx playwright test e2e/act.spec.ts       # the Act stack below, in headless Chromium
```

`e2e/act.spec.ts` runs against `test/standins/stack.ts`: a disposable nats-server with `ONEMIND`,
`CLIENT` and `AGENTS` accounts and random per-run passwords; the **real policy service** (omos
#554, built from `$OMOS_POLICY_DIR`, default `work/wt/i4-policy`) with its OpenFGA model and a
per-run tuples file; the **real MAVLink bridge** (omos #550, `$OMOS_BRIDGE_DIR`, default
`work/wt/h2-mavlink`) with an Ed25519 key per run and signatures required; and the scripted drone
`sim-x1`. Setup arms and takes off through policy with two humans (A4), never around it.
Stand-ins, in test fixtures only: entity (omos #552 sets no `capabilities`, so the real policy
would deny every goto `not_capable`), the KV `geofences` writer and geofence watcher (not built
yet), feed heartbeats, a scripted Hermes (no model API may be called here), `svc.cot.send` (not
built anywhere) and a scripted ATAK peer. The spec is skipped with the reason when Go, the omos
worktrees or pymavlink are missing (`POLICY_BIN`, `BRIDGE_BIN`, `PYTHON`, `SIM_DRONE` override).

## Pins and licences

| Package | Version | Licence |
|---|---|---|
| cesium | 1.146.0 | Apache-2.0 |
| @nats-io/nats-core | 3.4.0 | Apache-2.0 |
| hls.js | 1.7.3 | Apache-2.0 |
| vite 8.3.4, vitest 4.1.12, typescript 6.0.3, @playwright/test 1.64.0, @types/node 22.18.0 | dev only | MIT / Apache-2.0 |

`.npmrc` sets `legacy-peer-deps=true`: npm 10.9.4 crashes (`Cannot read properties of null
(reading 'edgesOut')`) resolving Vite 8's optional devtools peer dependencies otherwise.

Data: Natural Earth II imagery ships inside the CesiumJS package. OpenStreetMap tiles need the
"© OpenStreetMap contributors" credit (shown) and light use only
(https://operations.osmfoundation.org/policies/tiles/); a production deployment should use its
own tile cache (REFERENCE-ARCHITECTURE: tiles go through a caching tile proxy, never NATS). AWS
Terrain Tiles: https://registry.opendata.aws/terrain-tiles/ (attribution in the credit line).
The Cesium logo in the corner is CesiumJS's default credit; Cesium ion is not called (the e2e
online test asserts no request reaches `cesium.com`).

No code was copied from worldmonitor (AGPL-3.0), CloudTAK (AGPL-3.0) or worldwideview (Elastic
2.0). Design ideas only: primitive collections instead of one Cesium Entity per object, render
loop that sleeps when nothing changes (competitive-matrix.md).

## Known gaps

- Density cells (`mode: "cells"`) for large layers at wide zoom are not drawn. The client always
  asks a large layer for entities inside the view AOI; when the layer service answers with cells
  it shows "zoom in for entities" and keeps the watch, which adds entities once the zoom allows.
  On any other layer-service error (`aoi_required`, `too_many`, `layer_down`, timeout) a large
  layer shows the error and retries the layer service; it never falls back to raw `ent.*`
  while a layer service answers. With no layer responders at all, the raw `ent.*` fallback
  opens for a large layer only when the view is under 10° on each side, and the store caps a
  large layer at 5,000 entities.
- Authentik login through auth callout into the `CLIENT` account (REFERENCE-ARCHITECTURE §8) is
  not wired: the client offers NATS user and password only.
- PWA install, flat 2D map mode on one catalog and touch gestures beyond Cesium's are J6.
- Benchmark gate G4 (≥ 57.6 fps, cold start ≤ 1.86 s) is not measured here; the headless
  SwiftShader browser is not representative.
- Contract gaps found while building are listed as owner proposals in
  [`docs/evidence/M3/J1/OWNER-PROPOSALS.md`](../docs/evidence/M3/J1/OWNER-PROPOSALS.md) and
  [`docs/evidence/M4/WEB/OWNER-PROPOSALS.md`](../docs/evidence/M4/WEB/OWNER-PROPOSALS.md).
- Act: alert acknowledgement through policy (`alert.acknowledge`) is not offered (Dismiss is
  local); missions (D2), voice for Hermes, and drawings persisted on the bus (entity.create is
  not in the policy catalogue) are not built. Drawings live in the page until shared.
