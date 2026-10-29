# Design

Written in milestone M0 from the research: ONTOLOGY, REFERENCE-ARCHITECTURE, BUS-DESIGN, JADC2-MAP, GAP-ANALYSIS.

All of it is **proposed design, 2026-10-29**. Nothing here is built (`docs/STATUS.md`). omos was
read at commit `62e3d22b` (2026-10-28). The one-page summary of the research behind it is
[`../research/SUMMARY.md`](../research/SUMMARY.md).

## Design documents

| File | What it covers |
|---|---|
| [ONTOLOGY.md](ONTOLOGY.md) | Every object type, trait, link, action and permission, mapped to NATS subjects and buckets, CoT, and the Palantir and Lattice equivalents; owner proposals ONT-P1–ONT-P14 |
| [REFERENCE-ARCHITECTURE.md](REFERENCE-ARCHITECTURE.md) | The whole system across hub, home server, Mac and field kit: sites, bus topology, services, Sense / Make Sense / Act flows, video, plugins, client tiers, deployment, security zones, DDIL, and current vs target |
| [BUS-DESIGN.md](BUS-DESIGN.md) | Target NATS design: every subject used, streams with retention and caps, KV, Object Store, accounts and permissions, leaf and JetStream-domain topology, conflict rules, owner proposals BUS-P1–BUS-P11 and the combined register of every proposal (§10.1) |
| [JADC2-MAP.md](JADC2-MAP.md) | The JADC2 pattern mapped to civilian OneMind (homestead, neighbourhood, response team; NIMS/ICS), watch jobs with envelopes, and the stop lines with how each is enforced |
| [GAP-ANALYSIS.md](GAP-ANALYSIS.md) | OneMind against Palantir, Lattice, JADC2, the TAK clients and the intel dashboards, capability by capability, with the feature and milestone that closes each gap |

## Contracts

Wire formats the client reads and sends. Each names the omos file that defines its subjects and
buckets. Index with omos sources: [`../contracts/README.md`](../contracts/README.md).

| File | What it covers |
|---|---|
| [entity.md](../contracts/entity.md) | The fused entity record on `ent.{entity_type}.{entity_id}.state`: identity, envelope, provenance, confidence, expiry, overrides |
| [layer-snapshot.md](../contracts/layer-snapshot.md) | Loading the current state of one map layer in one read (layer manifest in KV `world-state`, `svc.layer.*`) |
| [layer-delta.md](../contracts/layer-delta.md) | Following a layer's changes after the snapshot (watch path or `ENT` ordered consumer), rates and freshness |
| [mission-task.md](../contracts/mission-task.md) | Missions and tasks: records in KV `mission-state`, task types, state machine, status reports |
| [asset-command.md](../contracts/asset-command.md) | Actions and asset commands: propose, policy, human confirm, signed `cmd.fleet.*` dispatch, audit, bridge safety |
| [cot-projection.md](../contracts/cot-projection.md) | Entities to TAK CoT (gates, types, detail, rate limits, `MESH_TO_TAK_SERVER`) and TAK CoT back to entities |
| [robotics-cot-profile.md](../contracts/robotics-cot-profile.md) | CoT for OneMind's own drones, robots and vehicles: types, detail, sensor footprints, video links, command echo |

## Open items across the set

- Policy subject names are settled across the set on BUS-DESIGN §3.4's four-token names (`svc.policy.command.*`, `svc.policy.task.*`, `svc.policy.action.get`, `svc.policy.check`; RA2, ONT-P14), and filed for the owner to confirm.
- Grammar gaps go to the owner as omos `docs/` proposals (decision 0004 #8). The IDs are distinct (BUS-P*, ONT-P*, RA*, CP-*, F1), and every one is listed in the combined register, BUS-DESIGN §10.1, which FEATURES A24 files.
