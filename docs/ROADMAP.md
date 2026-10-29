# Roadmap

Each milestone has exit criteria. A milestone is done only when its evidence is
stored in `docs/evidence/<milestone>/`. Milestones in different repos can run in
parallel once M0 is done.

| M | Name | Repo | Features | Exit criteria |
|---|---|---|---|---|
| **M0** | Research and contracts | both | — | Research reports in `docs/research/`; contracts in `docs/contracts/` for entities, layer snapshots, layer deltas, missions/tasks, asset commands, CoT projection; spikes S1–S4 run and decisions 0002–0003 updated |
| **M1** | Spine | omos | A1–A7, H1, H2, H6, H9, I4 | Feeds on grammar subjects; entities in KV; projector AOI filter live; mesh gateway on the Mac; SITL drone as entity and on ATAK. All as omos PRs. |
| **M2** | Client on the Mac | xr | B1–B6, C1, C2, F1 | Packaged macOS app: globe, five live layers, TAK users, markers sent to ATAK, 10k entities at 60 fps |
| **M3** | Web | xr (+ omos if a GPU node) | F2 or F3, J1 | A browser on another device controls the client; a phone shows the L0 view |
| **M4** | Act | both | D1–D6, H3, H4, I1–I3, J2 | Draw an AOI, create a mission, task a SITL drone, Hermes proposes and a human confirms; ATAK sees all of it |
| **M5** | Twin and headset | both | E1, E2, F4, H7, J3 | Homestead 3D tiles from a real drone flight; drone video panel; the client in the Quest Pro; rehearsal in sim |
| **M6** | The leap | both | C3–C6, E3–E5, F5, F6, H5, H8, I5, B7–B10 | Commo in the client, swarm search, detection to entities, multi-user, Windows and Linux builds |
| **M7** | Hosted | omos | F7, G1–G3 | Owner-approved GPU streaming deployment through Argo, tailnet-only |

## Spikes (run in M0, before committing code)

| ID | Question | Pass | If it fails (decision 0004) |
|---|---|---|---|
| S1 | Does UE 5.8 + Cesium for Unreal (current release) build and run on the owner's Mac? | Packaged app shows the globe | Pin the newest working pair and record why |
| S2 | Does Pixel Streaming 2 stream from the Mac to a browser on the LAN? | Live picture and input < 150 ms | Decision 0002 falls back to a Linux GPU host, plus the L0 web client |
| S3 | Can the client reach the Quest Pro? (a) standalone Android/OpenXR build, (b) streamed from a PC, (c) WebXR in the headset browser | Globe plus one layer at the headset's frame rate | Pick the best working option in decision 0003 |
| S4 | Does Commo (atak-civ `commoncommo`) build for macOS arm64? | Library links and joins the mesh | Keep the omos mesh gateway as the mesh path; C4 waits |
| S5 | Which NATS C client works inside Unreal on macOS (nats.c, or a thin wrapper)? | KV read and subscribe from a plugin | Use WebSocket NATS from C++ |
