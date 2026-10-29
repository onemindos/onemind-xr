# M0 research summary

Date: 2026-10-29. One page. Every row points to the file that proves it. **Nothing is built yet**: `docs/STATUS.md` shows M1–M7 not started, and every "target" below is a design. The full design is indexed in [`../design/README.md`](../design/README.md); contracts in [`../contracts/README.md`](../contracts/README.md).

## Key findings

| # | Finding | Source |
|---|---|---|
| 1 | omos has no entity layer. Nothing publishes `ent.*.*.state`, and the KV buckets `world-state` and `fleet-state` exist but have never been written | [omos-live-state.md](omos-live-state.md) §4–5; omos `platform/nats/streams/kv-world-state.yaml` |
| 2 | Feeds bypass fusion. `feed-mta` and `feed-satellites` push CoT straight to `tak.cot.inject`; aircraft and ships have been off since 2026-08-27 | [omos-live-state.md](omos-live-state.md) §3–4 |
| 3 | The bus → TAK Server leg is broken: no subscriber behind `tak-nats-leaf`, nothing on `tak.cot.out` in 30 days. The NatsBridgePlugin may not be in the running image (*unverified*) | [omos-live-state.md](omos-live-state.md) §6; [tak-server-and-bridges.md](tak-server-and-bridges.md) §4 |
| 4 | Streams are uncapped (only `WORLD` has `maxBytes`); `COT` holds 4.95 GB unread. Per-node JetStream limit is 45 GiB | [nats-platform.md](nats-platform.md) §1 and verifier note |
| 5 | One shared NATS account; Hermes and the TAK leaf share a user; the Mac leaf has no JetStream domain and `tls insecure` | [security-and-trust.md](security-and-trust.md) T1–T2; [edge-and-ddil.md](edge-and-ddil.md) §2 |
| 6 | The omos mesh gateway (branch `feat/tak-mesh-gateway`, no PR) passed mesh-to-bus tests here and matched `takproto` byte for byte | [tak-server-and-bridges.md](tak-server-and-bridges.md) §6; [tak-protocols.md](tak-protocols.md) Rec. 5 |
| 7 | Go collectors on `nats.go` passed the chaos test (NATS and collector `kill -9`, 0 duplicates, 0 missing) | [ingestion-platform.md](ingestion-platform.md) Recommendation; `docs/evidence/M0/ingestion/` |
| 8 | NATS TTL + delete markers, service import/export across accounts and WebSocket KV/subscribe were proved on the local server | [nats-platform.md](nats-platform.md) P6, P10; [unreal-platform.md](unreal-platform.md) [C-WS]; `docs/evidence/M0/nats-lane-proofs.txt` |
| 9 | 240 catalog sources classified: 123 candidates, 61 to JetStream, 28 to the tile proxy, 5 to MediaMTX; tiles and video never go on NATS | [../sources/SOURCE-REGISTRY.md](../sources/SOURCE-REGISTRY.md) Counts, Routing rule |
| 10 | Unreal, the Quest and Commo could not be tested here: no UE 5.8 (admin rights, ~150 GB vs 25 GiB free), no headset, and the Commo and ArduPilot builds were denied by the machine's agent policy | [../PREFLIGHT.md](../PREFLIGHT.md); [tak-server-and-bridges.md](tak-server-and-bridges.md) §S4 |

## Decisions

| Decision | Where |
|---|---|
| New UE 5.8 + Cesium for Unreal v2.30.0 client; VR-TAK is a donor: port VH_COT, Orbit, parts of Mensuration; rewrite VR, replication and playback; drop VH_Plugins and `EnginePatch/` | [decision 0001](../decisions/0001-new-ue58-client-with-vrtak-as-donor.md); [vrtak-porting-map.md](vrtak-porting-map.md) Rec. 1–3; [unreal-platform.md](unreal-platform.md) Rec. 1 |
| NATS JetStream stays the one authority bus; latest entity state in a new KV-shaped stream `ENT` on the locked subject `ent.{entity_type}.{entity_id}.state` (omos `platform/nats/streams/ent.yaml`, proposed) | [BUS-DESIGN.md](../design/BUS-DESIGN.md) D1–D2; [nats-platform.md](nats-platform.md) Rec. §1 |
| Entity service in Go: key identity + alias map, Kalman per entity, GNN gating for keyless `det.*`, Lattice-style provenance and expiry. Stone Soup offline as the CI scorer | [fusion-and-tracking.md](fusion-and-tracking.md) Recommendation |
| Ingestion standard: Go `omos-ingest` collectors with the full reliability list; Redpanda Connect retired after `feed-mta` moves (decision 0004 #18) | [ingestion-platform.md](ingestion-platform.md) Recommendation |
| Actions are the only write path: propose → OpenFGA policy (fail closed) → human confirm → signed command → bridge verifies → audit on `evt.so.action.*`. Agents only propose. Only the policy service publishes `cmd.*` | [ONTOLOGY.md](../design/ONTOLOGY.md) §6; [asset-command.md](../contracts/asset-command.md) §4; [agents-that-act.md](agents-that-act.md) Rec. 1–2 |
| Clients read layers as snapshot + delta through `svc.layer.*`; a `CLIENT` account that cannot publish `cmd.>` | [BUS-DESIGN.md](../design/BUS-DESIGN.md) D6–D7; [layer-snapshot.md](../contracts/layer-snapshot.md) |
| Each site is a leaf with its own JetStream domain; the hub sources site streams; commands are never queued for replay | [BUS-DESIGN.md](../design/BUS-DESIGN.md) D5; [edge-and-ddil.md](edge-and-ddil.md) Rec. 1 |
| Civilian scope: JADC2 as pattern, NIMS/ICS as doctrine; no `target` type, closed `cmd.*` verb allowlist, `ent.person.*` only for consenting team members | [JADC2-MAP.md](../design/JADC2-MAP.md) §5; [jadc2.md](jadc2.md) Rec. 1–2; BUS-DESIGN D9 |

## Fallbacks in use (decision 0004)

| # | Default not met | Fallback | Source |
|---|---|---|---|
| 3 | Commo for macOS arm64 (S4): build denied | omos mesh gateway is the only mesh path; C4 blocked | [0004 run log](../decisions/0004-owner-defaults-for-the-run.md) |
| 12 | NATS user, TAK certificate, cluster sandbox | Local nats-server 2.15.2, mesh gateway + `takproto` peer, scripted MAVLink drone `sim-x1` | [../PREFLIGHT.md](../PREFLIGHT.md) Access |
| 4 | Cesium ion token | Open imagery and terrain; Google 3D Tiles off | [../PREFLIGHT.md](../PREFLIGHT.md) Access |
| — | S1, S2, S3, S5 not run (no Unreal) | Not failed, so #1 and #2 are **not** triggered; #17 applies (owner checklist U1–U5). The L0 CesiumJS client is built anyway | [decision 0002](../decisions/0002-web-via-pixel-streaming.md); [decision 0003](../decisions/0003-meta-quest-pro.md) |

## Top risks

| Risk | Mitigation | Source |
|---|---|---|
| Pixel Streaming 2 on macOS (*unverified*; Epic's page contradicts itself; one UE 5.7 report of a flat-colour stream, no fix) | L0 web client built now as the 0004 #1 fallback | [unreal-platform.md](unreal-platform.md) facts 8–10, Risks #2 |
| Meta XR plugin fails to load on UE 5.8; no Quest Pro fps data for Cesium | OpenXR-only build; WebXR next | [decision 0003](../decisions/0003-meta-quest-pro.md); [unreal-platform.md](unreal-platform.md) Risks #7 |
| Grammar ambiguities: telemetry shape (P1), five-token `cmd.fleet` (P2), policy subject names (ONTOLOGY P14, unreconciled) | Owner proposals in omos `docs/` before the policy service is built | [BUS-DESIGN.md](../design/BUS-DESIGN.md) §10; [ONTOLOGY.md](../design/ONTOLOGY.md) §9 |
| A compromised agent or node issues commands | Per-service NATS users, policy fail-closed, signed commands, human confirm, I4 before any agent write tool | [security-and-trust.md](security-and-trust.md) Rec. 1–2; [agents-that-act.md](agents-that-act.md) Rec. 1, 6 |
| hub-core node pool memory-saturated | New workloads target the ecosystem pool | omos `CLAUDE.md` "Deployment Pattern" item 5 |
| Licence contamination (worldmonitor AGPL, worldwideview Elastic, CloudTAK AGPL) | Design-only; CloudTAK added to #15 as an owner proposal | [competitive-matrix.md](competitive-matrix.md) Rec. 2; 0004 run log |

## What M1 builds first

M1 (Spine) is omos PRs only; the owner merges. Order from [REFERENCE-ARCHITECTURE.md](../design/REFERENCE-ARCHITECTURE.md) §12 and [nats-platform.md](nats-platform.md) Rec. §6:

1. Stream byte caps and `COT` retention 72 h (CR edits in omos `platform/nats/streams/`).
2. `ENT` and `DET` stream CRs, `evt.entity.>` in `EVT`, and the **A4 entity service** (first slice: CoT and `sim-x1` MAVLink → one entity per thing, replay test).
3. A0 collector framework, then feed migrations one per PR, life-safety first (NWS → seismic → FIRMS → …), old pipeline kept running 7 days.
4. I4 per-service NATS users and the policy service (OpenFGA) **before** any agent or asset write tool; H2 MAVLink bridge on `tel.fleet.*` / `cmd.fleet.*`; H6 safety.
5. A5 layer service (`svc.layer.*`), A6 projector AOI filter, A7 mesh gateway PR, H9 robotics CoT profile.

Client lanes meanwhile run against the local nats-server and unmerged omos branches (decision 0004 #11).
