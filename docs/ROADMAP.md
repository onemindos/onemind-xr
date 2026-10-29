# Roadmap

Updated 2026-10-29 (M0 synthesis) from [`design/`](design/), [`contracts/`](contracts/) and the
research in [`research/`](research/). Feature IDs are from [`FEATURES.md`](FEATURES.md).

Each milestone has exit criteria. A milestone is done only when its evidence is stored in
`docs/evidence/<milestone>/` (run Rule 7). Features that need the owner's Mac, the Quest Pro, the
LAN with ATAK or a real SITL end as **built, awaiting owner check**, with their step in
[`OWNER-CHECKLIST.md`](OWNER-CHECKLIST.md) (decision 0004 #17). Milestones in different repos run
in parallel once M0 is done. Client lanes never wait on an omos merge: they run unmerged omos
branches against the local nats-server (decision 0004 #11, run Rule 9).

## Milestones

| M | Name | Repo | Features | Exit criteria | State (2026-10-29) |
|---|---|---|---|---|---|
| **M0** | Research, design and contracts | both | — | Research in `docs/research/`; design in `docs/design/`; contracts in `docs/contracts/` (entity, layer snapshot, layer delta, mission and task, asset command, CoT projection, robotics CoT profile); spikes run or recorded as not runnable; decisions 0002–0003 updated | in progress ([STATUS.md](STATUS.md)); research, design and contracts written; S1, S2, S3, S5 could not run, S4 blocked |
| **M1** | Spine | omos | A4, A22, A1, A0, A2, A3, A24, A19, I4, I6, H6, H2, H1, A8, H9, A7, A21, A5, A6 | Every M1 PR open in omos in the lane A order below, each with tests passing on the local stack. Entities in `ENT`; feeds on grammar subjects; nothing from feeds on `tak.cot.inject`; policy and audit gate every command; the scripted `sim-x1` drone is an entity and is projected as CoT; the mesh gateway passes its tests with a `takproto` peer. Owner checks written: LAN mesh test, ATAK display checks, real SITL | not started |
| **M2** | Client on the Mac | xr | B1–B6, C1, C2, F1 | Unreal project and `OneMind*` plugins committed, macOS-ready; C1 and C2 pass on the build machine against the local stack. Owner checks S1, S5 and the 10,000-entity check written; features marked built, awaiting owner check | not started (no Unreal on the build machine, [PREFLIGHT.md](PREFLIGHT.md)) |
| **M3** | Web and video | both | F2 or F3, J1, K1, K2, K3, K4, K6, K9 | L0 web client on the same contracts works on the build machine (phone-sized viewport); MediaMTX PR with auth, recording and registry; a stream plays tied to `sim-x1`; one example plugin per kind. Owner checks S2 and video written | not started |
| **M4** | Act | both | D1–D6, D10, D11, H3, H4, I1–I3, J2, B11, B20, C3, C7, C8 | Draw an AOI, create a mission, task `sim-x1`; Hermes proposes and a human confirms; every step on `evt.so.action.*` and `evt.mission.*`; ATAK sees the mission (owner check) | not started |
| **M5** | Twin, headset and depth | both | E1, E2, F4, H7, J3, F9, F11, A23, A9, A10, A11, A13, A14, A18, B7–B10, B13–B17, J6, G4 | Homestead 3D Tiles; drone video panel; the client in the Quest Pro (owner check S3); rehearsal in sim; disconnected mode on a site leaf; G4 benchmark report | not started |
| **M6** | The leap | both | C4 (blocked), C5, C6, C9, E3–E6, F5, F6, F8, F10, H5, H8, I5, B12, B18, B19, B22, A12, A15, A16, A17, A20, D7, D8, D9, J4, J5, K5, K7 | Swarm search with `sim-*` drones, detection to entities (no identity), multi-user, Windows and Linux builds, plugin registry, ATAK plugin | not started |
| **M7** | Hosted and operations | omos (+ xr CI) | F7, G1, G2, G3, G5 | Owner-approved GPU streaming deployment through Argo, tailnet-only; macOS CI on every PR; signed images. G1 and G2 may start as soon as M2 has code | not started |
| Later | P3 items | both | B21, D12, E7, E8, H10, J7, K8 | Each passes its own acceptance test | — |

What moved in this update, and why:

| Change | Reason (source) |
|---|---|
| M1 is now an ordered PR sequence that starts with the entity service and the feed fixes | Nothing publishes `ent.*.state` and feeds push CoT straight to `tak.cot.inject` ([omos-live-state.md](research/omos-live-state.md) Rec. 1; run prompt "Facts already verified") |
| I4, A19 and I6 come before H2 and before any agent write tool | Agent safety before agent tools ([agents-that-act.md](research/agents-that-act.md) Rec. 1, 6; [drones-robots-agents.md](research/drones-robots-agents.md) Rec. 4) |
| A21 (TAK Server leg) added to M1 | The bus-to-TAK path is broken live, so A6, C1 and C2 cannot pass over the server path without it ([GAP-ANALYSIS.md](design/GAP-ANALYSIS.md) §5 prerequisite row) |
| The 38 competitor rows and the new design rows are placed by priority | GAP-ANALYSIS §8 said they were unplaced; P1 goes to M4–M5, P2 to M6, P3 to Later (FEATURES header) |
| B7–B10 moved from M6 to M5; C3 moved from M6 to M4 | They are P1 in FEATURES |
| K9 added to M3; video moved into M3's name | Edge MediaMTX is a script plus an owner step ([video-and-plugins.md](research/video-and-plugins.md) Rec. 2) |

## Lane A order (M1, omos)

One focused omos PR per row, each with tests, opened in this order. The owner merges (decision
0004 #10). Each PR follows the omos checklist: pinned GHCR image, Argo Application, requests and
limits on the ecosystem pool, SealedSecrets, a `docs/STACK.md` row (omos `CLAUDE.md`, "Adding a
New Service"). Every subject below is from [BUS-DESIGN.md](design/BUS-DESIGN.md) §3.

| # | PR | Features | Defines (omos file) | Depends on | Proof on the build machine | Source |
|---|---|---|---|---|---|---|
| 1 | **Entity service** (Go, one active replica) and the `ENT` stream CR, with `services/schemas/entity/entity.v1.schema.json`. First slice: stages 0–2 for CoT and MAVLink telemetry, echo filter, replay test | A4, A22 (ENT part) | `platform/nats/streams/ent.yaml` (proposed); `services/schemas/entity/` (proposed) | — | [entity.md](contracts/entity.md) §10 tests on the local nats-server with recorded `COT` and `INTEL` samples and `sim-x1` | [fusion-and-tracking.md](research/fusion-and-tracking.md) Recommendation; [nats-platform.md](research/nats-platform.md) Rec. §6 item 2 |
| 2 | **Feed fixes**: `feed-mta` and `feed-satellites` stop publishing CoT to `tak.cot.inject` and publish raw observations on grammar subjects; remove duplicate `feed-hazards`; aircraft and ships stay off | A1 | existing `platform/data/redpanda-connect` pipelines; `platform/nats/streams/intel.yaml`, `world.yaml` (existing) | 1 (so TAK gets entities back through the projector) | A1 test: replay shows only grammar subjects, nothing on `tak.cot.inject` | [omos-live-state.md](research/omos-live-state.md) §4, Rec. 4; decision 0004 #18 (old pipeline stays until the new one is proven) |
| 3 | **Stream caps** on every existing CR, `COT` 72 h, `CMD` `discard: new`, `TEL` adds `tel.feed.>`, new `DET`, `kv-ingest-locks`, `kv-entity-correlation` | A22 | `platform/nats/streams/*.yaml` | — (can go in parallel with 1) | `make validate`; capped streams behave as configured locally | [BUS-DESIGN.md](design/BUS-DESIGN.md) §4.1, §11 |
| 4 | **Ingestion framework** `omos-ingest` (Go on `nats.go`): ack, dedup, lease, schema and dead letters, politeness, freshness, golden tests, metrics, logs, traces; `sources.yaml` loader; Grafana staleness alert. Then one PR per feed migration, in the plan's order (NWS alerts first, MTA last) | A0, A2, A3 | `services/bridge` (framework); `platform/nats/streams/kv-ingest-locks.yaml`; `tel.feed.{source}.status` in `TEL` | 3 | Chaos test and leader test saved in `docs/evidence/M1/` | [ingestion-platform.md](research/ingestion-platform.md) Recommendation and migration plan |
| 5 | **Grammar proposals** for the owner: every entry in the combined register (BUS-P1–BUS-P11, ONT-P1–ONT-P14, RA1–RA4, CP-1–CP-4, the `intercept` example F1) | A24 | omos `docs/` proposal files (proposed) | — | Review only; no live subject changes | [BUS-DESIGN.md](design/BUS-DESIGN.md) §10.1 (register), §10; [ONTOLOGY.md](design/ONTOLOGY.md) §9; [REFERENCE-ARCHITECTURE.md](design/REFERENCE-ARCHITECTURE.md) §13; [cot-projection.md](contracts/cot-projection.md) §11; [JADC2-MAP.md](design/JADC2-MAP.md) §6 F1 |
| 6 | **Identities**: per-service users, `CLIENT` account, separate Hermes and TAK leaf users (SealedSecrets) | A19 | NATS server config and SealedSecrets under `platform/nats/` | 3 | Permission-violation tests on the local stack with the same config | [BUS-DESIGN.md](design/BUS-DESIGN.md) §7; [security-and-trust.md](research/security-and-trust.md) Rec. 1 |
| 7 | **Policy and audit** before any agent action: `svc.policy.*` with OpenFGA (model extended so agents are proposers only), fail closed, closed verb list, human confirm, Ed25519-signed commands, audit on `evt.so.action.*`; plus the **agent evaluation and red-team suite** and **safety** rules | I4, I6, H6 | `services/` (policy service, proposed); `platform/security/` (OpenFGA model); `platform/nats/streams/cmd.yaml`, `evt.yaml` (existing) | 6 | Stop-line tests 4–7 ([JADC2-MAP.md](design/JADC2-MAP.md) §5.3) and [asset-command.md](contracts/asset-command.md) §9 tests with `sim-x1` | [agents-that-act.md](research/agents-that-act.md) Rec. 1; [security-and-trust.md](research/security-and-trust.md) Rec. 2 |
| 8 | **MAVLink bridge** on `tel.fleet.drone.{id}.*` and `cmd.fleet.drone.{id}.{verb}`, MSL to HAE, verifies signed commands, refuses non-`sim-*`; robotics CoT profile in the projector | H2, H1, A8, H9 | `services/bridge/bridges/mavlink` (existing, off-grammar today); `platform/nats/streams/tel.yaml`, `cmd.yaml` | 1, 7 | Contract test with the scripted drone (`MAVLINK20=1`); [robotics-cot-profile.md](contracts/robotics-cot-profile.md) §9 tests 1–3, 6 | [drones-robots-agents.md](research/drones-robots-agents.md) Rec. 1, 3 |
| 9 | **Mesh gateway PR** from `feat/tak-mesh-gateway`: transmit-interface fix, GeoChat, mesh crypto, data-package getfile; Mac run script with a local leaf | A7 | `services/bridge/bridges/tak` (branch); `platform/nats/streams/cot.yaml` (existing) | — (code exists; PR after 8 so its tests can use projected `sim-x1`) | `go test ./...`; `takproto` round trip ([cot-projection.md](contracts/cot-projection.md) §12 test 7) | [tak-server-and-bridges.md](research/tak-server-and-bridges.md) Rec. 1; [tak-protocols.md](research/tak-protocols.md) Rec. 2 |
| 10 | **TAK Server leg**: NatsBridgePlugin loads visibly; `tak-client` bridge publishes `tak.cot.out` and retires cot-tap | A21 | `services/takserver/`, `services/bridge/bridges/tak-client` (proposed) | 6 | `tak-client` against a scripted TAK-stream peer | [tak-server-and-bridges.md](research/tak-server-and-bridges.md) Rec. 2; [tak-protocols.md](research/tak-protocols.md) Rec. 1 |
| 11 | **Layer service** (`svc.layer.snapshot`, `svc.layer.watch`, KV `world-state` manifests and cells, KV `fleet-state` site status) | A5 | `platform/nats/streams/kv-world-state.yaml`, `kv-fleet-state.yaml` (existing, never written) | 1 | [layer-snapshot.md](contracts/layer-snapshot.md) and [layer-delta.md](contracts/layer-delta.md) acceptance tests | [BUS-DESIGN.md](design/BUS-DESIGN.md) D3, D7 |
| 12 | **Projector AOI filter** with marking gate and person-track exclusion | A6 | `platform/omos/tak-projector.yaml` (existing); KV `geofences` (`kv-geofences.yaml`, existing) | 1, 11 | [cot-projection.md](contracts/cot-projection.md) §12 tests 1–6, 8; stop-line tests 8, 10 | [cot-projection.md](contracts/cot-projection.md) §3 |

Order rule: rows 1 and 2 first; row 4 before further feed migrations; row 7 before any agent
action (I2 proposals, D5) and before row 8 accepts commands; row 8 before row 9. Rows 3, 5 and
6 can run in parallel with rows 1–2 because they block later rows, not earlier ones.

## Other lanes

| Lane | Repo | Scope | Starts | Runs against |
|---|---|---|---|---|
| B — Client | onemind-xr | M2: Unreal project, `OneMindBus`, `OneMindGlobe`, `OneMindLayers`, `OneMindEntities`, `OneMindCoT` (port VH_COT first, [vrtak-porting-map.md](research/vrtak-porting-map.md) Rec. 5) | After M0 | Local nats-server and lane A branches; Unreal code is source-only here (no UE install, [PREFLIGHT.md](PREFLIGHT.md)) |
| C — Surfaces and video | both | M3: L0 web client (J1, the 0004 #1 fallback, built either way), K1–K4, K6, K9, F2 setup | L0 can start at once (no Unreal needed); video after lane A row 1 | Local nats-server WebSocket 9222, local MediaMTX |
| D — Act | both | M4 | After lane A rows 7, 11, 12 and lane B C1/C2 | Local stack with `sim-x1` |
| E — Twin and XR | both | M5 | After D | Owner checklist for headset and Mac |
| F — Leap | both | M6 | After D | — |

## Spikes

| ID | Question | Pass | Result (M0) | If it fails (decision 0004) |
|---|---|---|---|---|
| S1 | Does UE 5.8 + Cesium for Unreal (current release) build and run on the owner's Mac? | Packaged app shows the globe | **Not run**: Unreal is not installed on the build machine (admin rights, about 150 GB, 25 GiB free). Owner checklist section 3 | Pin the newest working pair and record why (#14) |
| S2 | Does Pixel Streaming 2 stream from the Mac to a browser on the LAN? | Live picture and input < 150 ms | **Not run** (needs S1). PS2 on macOS is **unverified** (decision 0002). Owner checklist section 6 | The L0 web client becomes the web path; a Linux GPU host is documented, not provisioned (#1) |
| S3 | Can the client reach the Quest Pro? (a) standalone Android/OpenXR build, (b) WebXR in the headset browser, (c) streamed | Globe plus one layer at the headset's frame rate | **Not run** (needs S1, an Android toolchain and the headset). Order corrected to standalone, WebXR, streamed (decision 0003). Owner checklist section 7 | Pick the best working option in decision 0003 (#2) |
| S4 | Does Commo (atak-civ `commoncommo`) build for macOS arm64? | Library links and joins the mesh | **Blocked**: the build step was denied by the build machine's permission policy ([tak-server-and-bridges.md](research/tak-server-and-bridges.md) §S4) | **Applied**: the omos mesh gateway is the only mesh path; C4 blocked (#3) |
| S5 | Which NATS C client works inside Unreal on macOS (nats.c, or a thin wrapper)? | KV read and subscribe from a plugin | **Partly**: the server side (NATS WebSocket KV get and subscribe) was proven locally; the nats.c static build was blocked by the permission policy ([STATUS.md](STATUS.md)). Owner checklist section 4 | Use WebSocket NATS from C++ |
