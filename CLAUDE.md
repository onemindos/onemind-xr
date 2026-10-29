# OneMind XR — Agent Instructions

This repository is the OneMind client: a 3D, TAK-native operating picture on
Unreal Engine 5.8 with Cesium for Unreal. It runs natively on macOS (Apple
Silicon) first, streams to browsers, and later runs on Windows, Linux and the
Meta Quest Pro. Read `docs/VISION.md`, `docs/ARCHITECTURE.md` and `docs/ROADMAP.md`
before any design or code work.

## Two repositories, one system

- **This repo** owns the client: the Unreal project, its plugins, the web
  streaming front end, client-side contracts and client docs.
- **`onemindos/omos`** owns the platform: NATS streams and subjects, TAK Server,
  data feeds (Redpanda Connect, Python feeds), the entity layer, Hermes and the
  TAK mesh gateway (`services/bridge/bridges/tak`). Platform changes go to omos
  as PRs that follow **omos's own `CLAUDE.md`**, which wins for anything inside omos.
- Never copy omos contracts here. Link to them. Client contracts in
  `docs/contracts/` describe only what the client reads and sends, and cite the
  omos file that defines each subject.

## Rules that do not bend

1. **No guessing.** Every claim about an external project, API, engine version or
   live system cites a file, URL or command output. Mark anything unverified as
   *unverified*. Distinguish verified live state from intent.
2. **NATS subject grammar is locked** (omos `CLAUDE.md` and `docs/NATS-DOCTRINE.md`).
   Do not invent subject patterns. If one is missing, write a proposal for the
   owner instead.
3. **The bus is the authority.** The client is a view and an input device. It
   never becomes the source of truth for entities, missions or tracks.
4. **No secrets in Git**, not even test values. Tokens (Cesium ion, NATS, Google)
   come from the local keychain or environment at run time.
5. **The cluster is read-only** unless the owner approves a specific change.
   Never `kubectl apply`, `helm upgrade`, `terraform apply`, or run
   `platform/install.sh` in omos. New workloads reach the cluster through Argo
   Applications in omos PRs.
6. **Licences.** This repo is GPL-3.0. VR-TAK and ATAK-CIV code may be ported with
   attribution. MIT code (gods-eye-view, osiris) may be reused with its notice.
   **Do not copy code** from worldmonitor (AGPL-3.0) or worldwideview (Elastic
   License 2.0); learn from their designs only. Cesium ion and Google
   Photorealistic 3D Tiles have their own terms; record which is used and under
   what tier.
7. **Every milestone ends with proof**: a test, a recording, a screenshot or a
   command transcript stored under `docs/evidence/<milestone>/`. "Done" without
   evidence is not done.
8. **Do not merge to `main` in omos.** Open PRs. The owner (or the harness the
   owner names) merges. Do not touch omos branches that another harness is
   assigned to (see omos `CLAUDE.md`).

## Engineering conventions

- Unreal: C++ for systems (`Plugins/OneMind*`), Blueprints only for UI and
  layout. One plugin per concern: `OneMindBus` (NATS), `OneMindCoT` (CoT and
  later Commo), `OneMindGlobe` (Cesium integration, layers), `OneMindEntities`,
  `OneMindMission`, `OneMindHermes`, `OneMindStreaming`.
- The client renders snapshots, not firehoses. Each layer loads a snapshot from
  NATS KV, then applies deltas from its subject. Big layers render with batched
  instancing (HISM or Niagara), never one actor per entity.
- Pin every dependency: engine version, Cesium for Unreal release, NATS client
  library and commit SHAs.
- macOS first. Every feature must run on the owner's Apple Silicon Mac before it
  counts as done. Other platforms come after.
