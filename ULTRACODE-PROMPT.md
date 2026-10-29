# Ultracode run prompt

Before starting:

1. On the build machine (any machine with Claude Code; the owner's Mac is not
   required), put both repos side by side and start Claude Code in `work/`:
   ```
   work/
     onemind-xr/   git clone git@github.com:onemindos/onemind-xr.git
     omos/         git clone git@github.com:onemindos/omos.git
   ```
2. Grant what you can from [`docs/ACCESS.md`](docs/ACCESS.md). Anything missing is
   handled by the fallbacks in decision 0004; the run will not stop to ask.
3. Paste everything below the line.

---

ultracode

# Mission

Build **OneMind**: the best open TAK platform on the market. It is one 3D operating
picture that combines intel-dashboard breadth (worldmonitor, osiris, worldwideview,
gods-eye-view, Argos Atlas), TAK-native action and mesh, drones, robots and AI
agents, and a live 3D digital twin of the owner's homestead. It runs on every level,
from a phone browser to a native Mac app and a Meta Quest Pro headset. The owner is
Zeus (OneMind, an NGO). Use the JADC2 loop throughout: Sense, Make Sense, Act.

**Work autonomously from start to finish. Do not ask the owner questions.** Every
owner decision is already made in
`onemind-xr/docs/decisions/0004-owner-defaults-for-the-run.md`. When something is
blocked, apply its fallback, record what happened, and keep going on everything
else. Report blockers only in `docs/STATUS.md` and the final report.

# The two repositories

| Repo | Owns | Merge rule |
|---|---|---|
| `onemind-xr/` | The client: an Unreal Engine 5.8 project with Cesium for Unreal, its `OneMind*` plugins, the web front ends, client contracts and docs | Merge to `main` after an independent review agent approves |
| `omos/` | The platform: NATS JetStream (the authority bus), TAK Server 5.8, feeds (Redpanda Connect, Python), Hermes, bridges, the TAK mesh gateway, Argo CD manifests | PRs only. Never merge `main`. Its `CLAUDE.md` governs all work inside it |

Read before acting:
- `onemind-xr/`: `CLAUDE.md`, `docs/VISION.md`, `docs/FEATURES.md`,
  `docs/ARCHITECTURE.md`, `docs/ROADMAP.md`, `docs/ACCESS.md`, all of
  `docs/decisions/`, and `docs/research/2026-10-29-findings.md`.
- `omos/`: `CLAUDE.md`, `docs/MISSION.md`, `docs/NATS-DOCTRINE.md`, `docs/STACK.md`,
  `PLATFORM-ROADMAP.md`, `docs/architecture/decisions/0003-tak-native-clients-and-mesh.md`.
- omos branch `feat/tak-mesh-gateway`: the TAK mesh gateway in
  `services/bridge/bridges/tak` (multicast SA, directed TCP, TAK Protocol v1). It has
  no PR yet; opening and finishing that PR is yours (feature A7).

# Where this runs

You are on a **build machine that is not the owner's Mac**. The owner's Apple
Silicon Mac, the Meta Quest Pro and the ATAK devices on the owner's Wi-Fi are
the **acceptance platform**, and you cannot reach them. So:

- Build and test everything you can here: omos services, contracts, the Unreal
  project and plugins, web clients, simulators. If this machine is a Mac, package
  the macOS app here. If not, build for this platform and keep the project
  macOS-ready: no platform-only APIs without a macOS path, and
  `Mac` in every plugin's `PlatformAllowList`.
- Replace LAN multicast with what works here: the mesh gateway's own multicast
  tests, a second gateway or a scripted mesh peer on this machine, and
  ATAK-compatible test messages from the `takproto` Python package.
- Everything that needs the owner's hardware goes into
  `onemind-xr/docs/OWNER-CHECKLIST.md` as exact, copy-paste steps with the
  expected result: S1 (packaged app on the Mac), S2 (Pixel Streaming from the Mac),
  S3 (Quest Pro), the LAN mesh test with ATAK, and each milestone's on-Mac
  acceptance. Mark those features **built, awaiting owner check**, not done.
- Make the owner's part one command where you can: a `scripts/` setup script for
  the Mac (install checks, build, package, run), and a script that runs the mesh
  gateway and a local NATS server on the Mac.

# Facts already verified (don't re-research these; build on them)

- VR-TAK is GPL-3.0, an engine fork of UE 5.3.2 (10,081 patched engine files), with
  Commo only as Windows x64 binaries. **Do not fork it.** Port its plugins
  (`VH_COT`, `VTAK_Mensuration`, `VTAK_Orbit`, `VH_Replication`, `VH_VR`,
  `VH_SessionPlayback`) into the new project with attribution (decision 0001).
- Cesium for Unreal supports the three newest Unreal versions (UE 5.8 since v2.28.0,
  July 2026) and lists macOS and Android.
- Pixel Streaming 2 on macOS is unproven: community reports show bugs. The
  headset is a Meta Quest Pro (DK94EC). Quest Link needs Windows.
- Common Commo (atak-civ `commoncommo`) is C++ with JNI, Objective-C and Win32
  bindings. The TAK Protocol spec is atak-civ `takproto/README.txt`.
- Intel dashboards scale by: collectors separate from the viewer, a latest-snapshot
  cache per layer, loading layers on demand, and GPU batch rendering. In NATS terms:
  streams per feed, KV for snapshots, subjects for deltas.
- In omos today, Redpanda Connect pushes every aircraft, ship and transit item as CoT
  straight to `tak.cot.inject`, and nothing publishes `ent.*.state`. Fixing both is
  the first platform job.

# Rules

1. **No guessing.** Cite a file, URL or command output for every claim. Mark anything
   unproven as unverified.
2. **NATS grammar is locked** (omos `CLAUDE.md`). Use existing patterns only;
   decision 0004 #8 covers the rare gap.
3. **Cluster:** read-only, except a sandbox namespace if one was granted. Never
   `kubectl apply` outside it, `helm upgrade`, `terraform apply`, or
   `platform/install.sh`. Platform changes are omos PRs with the full omos
   checklist: pinned GHCR image, Argo Application, requests and limits,
   SealedSecrets, a `docs/STACK.md` row. The hub-core node pool is
   memory-saturated, so new workloads target the ecosystem pool. Leave omos PRs
   assigned to other harnesses (omos `CLAUDE.md`) alone.
4. **No secrets in Git**, not even test values. Tokens come from the macOS keychain
   or the environment.
5. **Licences:** onemind-xr is GPL-3.0. Port VR-TAK and ATAK-CIV code with
   attribution; reuse MIT code (gods-eye-view, osiris) with its notice; never copy
   worldmonitor (AGPL-3.0) or worldwideview (Elastic 2.0) code.
6. **Safety:** simulated assets only (`sim-*`). Asset commands always pass policy,
   audit and a human-confirm step in the UI, even in simulation.
7. **Evidence:** a feature is done only when its acceptance test in
   `docs/FEATURES.md` passes and the proof (test output, screenshot, screen
   recording, transcript) is in `onemind-xr/docs/evidence/<milestone>/`.
8. **Mac is the acceptance platform:** a feature is done when it passes here
   and its on-Mac check is written in `docs/OWNER-CHECKLIST.md`. Never mark a
   Mac-only check as passed.
9. **Never wait on a merge:** run unmerged omos services locally on this machine
   from their branches against a local NATS server (or a leaf to the cluster if
   credentials exist), so client lanes keep moving.
10. **No spending:** create, buy or resize nothing that costs money.

# Phase 1 — Research and contracts (M0)

Run in parallel. Each lane writes `onemind-xr/docs/research/<topic>.md` with
sources, verified facts, risks and a recommendation.

1. **Competitors:** worldmonitor, osiris, worldwideview, gods-eye-view, Argos Atlas,
   ARTAK/EolianVR, CloudTAK, TAKX, WinTAK. For each: every data source and layer,
   ingestion, caching, rendering at scale, AI features, and what it cannot do.
   Output `competitive-matrix.md` against `docs/FEATURES.md`, then add every
   missing capability to `docs/FEATURES.md` with a priority and acceptance test.
   **Target: match or beat every competitor on every row.**
2. **TAK:** TAK Server 5.8 APIs (Marti, missions, data packages, federation), the omos
   NatsBridgePlugin and cot-tap, the mesh gateway, the ATAK plugin SDK. Run spike S4.
3. **VR-TAK porting map:** each plugin's function, its engine-patch dependencies, and
   port cost.
4. **Unreal platform:** run S1 (UE + Cesium) and S5 (NATS client in Unreal) on
   this machine; S2 (Pixel Streaming 2) here if it has a GPU. Research S2 on macOS
   and S3 (Quest Pro DK94EC) from docs and issue trackers, then write their
   on-Mac and on-headset steps into `docs/OWNER-CHECKLIST.md`. Large-entity
   rendering options (HISM, Niagara, Mass).
5. **Drones, robots, agents:** PX4/ArduPilot SITL, MAVLink, ROS 2 to NATS (Zenoh or
   rosbridge), CoT types and detail for UAVs and UGVs, swarm tasking, onboard
   detection, Hermes agent tools over NATS (omos `services/onemind-core`,
   `platform/hermes`).
6. **omos live state (read-only):** what each feed publishes, what writes
   `kv-world-state` and `kv-fleet-state`, TAK Server and plugin health, Hermes
   tools, and node capacity.

Then:
- Write `docs/contracts/`: `entity.md`, `layer-snapshot.md`, `layer-delta.md`,
  `mission-task.md`, `asset-command.md`, `cot-projection.md`,
  `robotics-cot-profile.md`. Each one cites the omos file that defines its
  subjects and buckets.
- Update decisions 0002 and 0003 from the spike results, applying decision 0004
  fallbacks where a spike failed.
- Write `docs/research/SUMMARY.md` (one page).

Do not stop here. Continue to Phase 2.

# Phase 2 — Build (M1–M6 in `docs/ROADMAP.md`)

Parallel lanes in separate worktrees, one writer per set of files:

| Lane | Repo | Scope (feature IDs) | Starts |
|---|---|---|---|
| A — Spine | omos | A1 fix the feeds, A2–A3 feed registry and health, A4 entity service, A5 layer snapshots, A6 projector AOI filter, A7 mesh gateway PR (LAN test with ATAK goes to the owner checklist), H1–H2 MAVLink + SITL, H6 safety, H9 robotics CoT profile, I4 policy and audit. One focused PR per feature, each with tests | After Phase 1 |
| B — Client | onemind-xr | B1–B6, C1–C2, F1: the UE project (macOS-ready), `OneMindBus`, `OneMindGlobe`, `OneMindLayers`, `OneMindEntities`, `OneMindCoT`; 10,000 entities at 60 fps | After Phase 1 |
| C — Surfaces | onemind-xr | F2/F3 web streaming or fallback, J1 L0 light web client, J3 adaptive quality | After B shows a globe |
| D — Act | both | D1–D6 AOIs, missions, tasking, asset commands, Hermes panel and tools, alerts; H3–H4 ROS 2 and tasking; I1–I3 agents; J2 role modes | After A6 and B |
| E — Twin and XR | both | E1–E2 WebODM to 3D Tiles and drone video, F4 Quest Pro, H7 simulation and rehearsal | After D |
| F — Leap | both | C3–C6 TAK contact, Commo in the client, data packages, voice; E3–E5; F5–F6 Windows, Linux, multi-user; H5 swarm; H8 detection; I5 multi-agent; B7–B10; G1–G3 | After D |

For every feature: implement, test, save the evidence, then have an independent
reviewer agent check it against its acceptance test and the rules above. Fix
findings before marking it done.

# Reporting

- Keep `onemind-xr/docs/STATUS.md` current: each milestone and feature, its state
  (done with an evidence link, in progress, or blocked with the exact cause), and
  the omos PR links.
- Keep `onemind-xr/docs/OWNER-CHECKLIST.md` current and ordered, so the owner can
  run it top to bottom on the Mac, the headset and the LAN.
- **Final report:** what is proven on the build machine; what awaits the owner's
  checklist; which omos PRs are open and in what
  order to merge them; which decision 0004 fallbacks were used and why; what access
  was missing; and what is left, in priority order.
