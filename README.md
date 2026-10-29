# OneMind XR

The OneMind client: one 3D operating picture for the homestead, the team and the
world. You can see it, understand it and act on it, with the TAK ecosystem and the
OneMind bus underneath.

- **Sense:** live intel layers, fleet telemetry, drones, cameras and TAK users on
  one globe.
- **Make Sense:** fused entities, a 3D digital twin of the homestead built from
  drone imagery, and Hermes (the OneMind AI) reasoning over all of it.
- **Act:** missions, tasking, and commands to people, robots and drones, sent to
  TAK clients and the mesh.

Status: **planning and setup.** No client code yet. Start with
[`ULTRACODE-PROMPT.md`](ULTRACODE-PROMPT.md) and [`CLAUDE.md`](CLAUDE.md).

| Read | For |
|---|---|
| [docs/VISION.md](docs/VISION.md) | What we are building and why it is different |
| [docs/FEATURES.md](docs/FEATURES.md) | Every feature, with priority and acceptance test |
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | Layers, data flow, deployment targets |
| [docs/ROADMAP.md](docs/ROADMAP.md) | Milestones M0–M7 with exit criteria |
| [docs/decisions/](docs/decisions/) | Decisions taken, including the owner defaults the run uses instead of asking (0004) |
| [docs/research/](docs/research/) | Verified findings this plan rests on |
| [docs/OPEN-QUESTIONS.md](docs/OPEN-QUESTIONS.md) | What is not known yet |

## Related repositories

| Repository | Role |
|---|---|
| [`onemindos/omos`](https://github.com/onemindos/omos) | Platform and bus: NATS, TAK Server, feeds, Hermes, the TAK mesh gateway. The source of truth for subjects and streams. |
| [`TAK-Product-Center/VR`](https://github.com/TAK-Product-Center/VR) | VR-TAK (GPL-3.0, UE 5.3.2, Windows). Reference and donor code. |
| [`TAK-Product-Center/atak-civ`](https://github.com/TAK-Product-Center/atak-civ) | Common Commo, TAK Kernel, takproto, takcot. |
| [`CesiumGS/cesium-unreal`](https://github.com/CesiumGS/cesium-unreal) | Globe, terrain and 3D Tiles in Unreal. |

## License

GPL-3.0, because the client ports code from VR-TAK (GPL-3.0). See [LICENSE](LICENSE).
