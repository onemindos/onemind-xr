# Ultracode run prompt

Paste everything below the line into Claude Code, started in a folder that contains
both checkouts side by side:

```
work/
  onemind-xr/   git clone https://github.com/onemindos/onemind-xr
  omos/         git clone https://github.com/onemindos/omos
```

Grant the access in [`docs/ACCESS.md`](docs/ACCESS.md) first.

---

ultracode

You are building **OneMind XR**, the best open TAK platform: intel-dashboard breadth,
TAK-native action and mesh, drones, robots and AI agents, and a live 3D digital twin,
on every device level. The owner is Zeus (OneMind, an NGO). Work in two repos:

- `onemind-xr/`: the client (Unreal Engine 5.8 + Cesium for Unreal). Read its
  `CLAUDE.md`, then `docs/VISION.md`, `docs/FEATURES.md`, `docs/ARCHITECTURE.md`,
  `docs/ROADMAP.md`, `docs/ACCESS.md`, `docs/decisions/` and
  `docs/research/2026-10-29-findings.md`.
- `omos/`: the platform (NATS bus, TAK Server, feeds, Hermes, bridges). Read its
  `CLAUDE.md`, `docs/MISSION.md`, `docs/NATS-DOCTRINE.md`, `docs/STACK.md` and
  `PLATFORM-ROADMAP.md`. **Its `CLAUDE.md` governs all work inside omos.** Branch
  `feat/tak-mesh-gateway` holds the TAK mesh gateway (omos ADR 0003).

## Hard rules

1. No guessing. Cite a file, URL or command output for every claim. Mark anything
   unproven as unverified.
2. NATS subject grammar is locked. Missing pattern: write a proposal, don't invent.
3. Cluster access is read-only except the sandbox namespace in `docs/ACCESS.md`.
   Platform changes are omos PRs; never merge omos `main`; never `helm upgrade`,
   `kubectl apply` outside the sandbox, `terraform apply` or `platform/install.sh`.
   Do not touch omos PRs assigned to another harness in omos `CLAUDE.md`.
4. No secrets in Git. Tokens come from the keychain or environment.
5. Licences: this repo is GPL-3.0. Port VR-TAK and ATAK-CIV code with attribution;
   reuse MIT code with its notice; never copy worldmonitor (AGPL) or worldwideview
   (Elastic 2.0) code.
6. Real assets: publish only to simulated assets (`sim-*`) and `tak.cot.mesh.tx`
   until the owner signs off M4. Any command that moves real hardware needs a human
   confirm.
7. Done means evidence in `onemind-xr/docs/evidence/<milestone>/` (test output,
   recording, screenshot, transcript).
8. macOS (the owner's Apple Silicon Mac) is the first platform. A feature isn't done
   until it runs there.

## Phase 1: Research (fan out, then synthesize)

Run these lanes in parallel. Each writes `onemind-xr/docs/research/<topic>.md` with
sources, verified facts, open risks and a recommendation.

1. **Competitors in depth:** worldmonitor, osiris, worldwideview, gods-eye-view,
   Argos Atlas, ARTAK/EolianVR, CloudTAK, TAKX, WinTAK. For each: every data source
   and layer, ingestion design, caching, rendering at scale, AI features, what they
   cannot do. Output a feature-parity matrix against `docs/FEATURES.md` and list
   features we are missing.
2. **TAK depth:** atak-civ (Commo, takkernel, takproto, plugin SDK), TAK Server 5.8
   APIs (Marti, mission API, data packages, federation), the omos NatsBridgePlugin
   and cot-tap, and the omos mesh gateway. Answer spike S4 (Commo on macOS arm64).
3. **VR-TAK porting map:** for each VR-TAK plugin, what it does, what depends on its
   engine patch, and the port cost to UE 5.8.
4. **Unreal 5.8 platform:** Cesium for Unreal (current release, macOS, Android),
   Pixel Streaming 2 on macOS (spike S2), OpenXR on the Meta Quest Pro DK94EC
   (spike S3), large-entity rendering (HISM, Niagara, Mass), a NATS client in Unreal
   (spike S5).
5. **Drones, robotics and agents:** PX4/ArduPilot SITL, MAVLink, ROS 2 to NATS
   (Zenoh, rosbridge), CoT types and details for UAV/UGV, swarm tasking, onboard
   detection, and agent tool design for Hermes over NATS (omos
   `services/onemind-core`, `platform/hermes`).
6. **omos live state (read-only):** what each feed publishes today, what writes
   `kv-world-state` and `kv-fleet-state`, TAK Server and plugin health, Hermes
   tools, and capacity for new services (the hub-core pool is memory-saturated).

Then synthesize: update `docs/FEATURES.md` (add what competitors have that we
lack, re-rank priorities), write `docs/contracts/` (entity, layer snapshot, layer
delta, mission/task, asset command, CoT projection, each citing its omos source),
and update decisions 0002 and 0003 from the spike results. **Stop and report to
the owner** with a one-page summary and any decision only the owner can make.

## Phase 2: Build (milestones M1–M6 in `docs/ROADMAP.md`)

Run independent lanes in parallel worktrees, one writer per set of files:

- **Lane A, omos spine (M1):** fix the feeds (A1), feed registry and health (A2,
  A3), entity service (A4), layer snapshots (A5), projector AOI filter (A6), mesh
  gateway on the Mac (A7), MAVLink and SITL (H1, H2, H6, H9), policy and audit (I4).
  One focused omos PR per feature, each with tests and the omos checklist (GHCR
  pinned image, Argo Application, requests and limits, SealedSecrets, STACK.md row).
- **Lane B, client on the Mac (M2):** UE 5.8 project, plugins from
  `docs/ARCHITECTURE.md`, globe, layers on the contracts, TAK entities and sending
  CoT, 10k entities at 60 fps.
- **Lane C, surfaces (M3):** Pixel Streaming or its fallback, and the L0 light web
  view on the same contracts.
- **Lane D, act (M4):** AOIs, missions, tasking, asset commands with confirm,
  Hermes panel and tools, alerts, role modes.
- **Lane E, twin and headset (M5):** WebODM to 3D Tiles for the homestead, drone
  video panels, Quest Pro build, simulation and rehearsal.
- **Lane F, the leap (M6):** Commo in the client, swarm search, detections to
  entities, multi-user, Windows and Linux builds.

Order: A and B start together after Phase 1; C after B shows a globe; D after A6
and B; E and F after D. Every lane verifies its own work, then an independent
reviewer agent checks it against the acceptance test in `docs/FEATURES.md` before
it counts.

## Reporting

Keep `onemind-xr/docs/STATUS.md` current: milestone, lane, done (with evidence
link), blocked (with the exact owner action needed), next. At the end, report what
is proven on the Mac, what is merged or awaiting the owner in omos, and what is
left.
