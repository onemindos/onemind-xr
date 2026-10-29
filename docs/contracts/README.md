# Contracts

Written in milestone M0. Each contract describes what the client reads or sends, and names the omos file that is the source of truth for every subject, stream and KV bucket.

Planned: `entity.md`, `layer-snapshot.md`, `layer-delta.md`, `mission-task.md`, `asset-command.md`, `cot-projection.md`.

## Index (M0, 2026-10-29)

All seven are written: the six planned plus `robotics-cot-profile.md` (FEATURES H9). Status is
**proposed**: nothing is deployed. omos paths are relative to the omos repo, read at commit
`62e3d22b`. **Proposed** means the omos file does not exist yet and arrives by an omos PR that
the owner merges (decision 0004 #10). Design context: [`../design/README.md`](../design/README.md).

| Contract | What it defines | Main omos sources (state) |
|---|---|---|
| [entity.md](entity.md) | Fused entity record on `ent.{entity_type}.{entity_id}.state`: identity, envelope, provenance, confidence, overrides | `CLAUDE.md` grammar (exists); `platform/nats/streams/ent.yaml` (**proposed**); `evt.entity.>` in `platform/nats/streams/evt.yaml` (**proposed**) |
| [layer-snapshot.md](layer-snapshot.md) | One-read snapshot of a map layer; layer manifest `{layer}.meta` | `platform/nats/streams/kv-world-state.yaml`, `kv-fleet-state.yaml` (exist; new purpose **proposed**); `ent.yaml`, `obj-overlays.yaml`, `services/layer/` (**proposed**) |
| [layer-delta.md](layer-delta.md) | Following layer changes after the snapshot | `CLAUDE.md` grammar; `ent.yaml` (**proposed**); `kv-world-state.yaml`; `evt.yaml` (`evt.entity.>` **proposed**) |
| [mission-task.md](mission-task.md) | Mission and task records, task types, state machine, reports | `kv-mission-state.yaml`, `mission.yaml`, `cmd.yaml`, `kv-geofences.yaml` (exist); `docs/NATS-DOCTRINE.md` §9.1; `services/schemas/tasks/` (**proposed**) |
| [asset-command.md](asset-command.md) | Actions and asset commands: propose → policy → confirm → `cmd.fleet.{class}.{asset_id}.{verb}` → audit on `evt.so.action.*` | `docs/NATS-DOCTRINE.md` §9.1; `cmd.yaml`, `evt.yaml`, `services/schemas/command/envelope.schema.json` (exist); `services/policy/`, `services/schemas/actions/` (**proposed**) |
| [cot-projection.md](cot-projection.md) | Entities to CoT on `tak.cot.inject` / `tak.cot.mesh.tx`, and CoT back from `tak.cot.out` / `tak.cot.mesh.rx` | `platform/nats/streams/cot.yaml`, `platform/omos/tak-projector.yaml`, NatsBridgePlugin YAML in `services/takserver/plugin/` (exist); mesh gateway `services/bridge/bridges/tak/` (branch `feat/tak-mesh-gateway`, no PR) |
| [robotics-cot-profile.md](robotics-cot-profile.md) | CoT for own drones, robots and vehicles: types, detail, footprints, video, command echo | `docs/NATS-DOCTRINE.md` §9.1; `tel.yaml`, `evt.yaml`, `kv-overwatch-video.yaml` (exist); `ent.yaml` (**proposed**) |

## Rules every contract follows

- Subjects use only the locked omos grammar (omos `CLAUDE.md`). Gaps are owner proposals (decision 0004 #8), listed in each contract's last sections and collected in [BUS-DESIGN.md](../design/BUS-DESIGN.md) §10.
- The client is a view and an input device. It reads `ent.*` and layers, and changes the world only through `svc.policy.*` requests (onemind-xr `CLAUDE.md` rule 3).
- Asset ids start `sim-` in this run (decision 0004 #13).

Not yet written: `plugin-manifest.schema.json` (named in [video-and-plugins.md](../research/video-and-plugins.md) Recommendation 6).
