# Status

Resume rule: read this file and `docs/PREFLIGHT.md` first, then continue at the first
row that is not done.

| Milestone | State | Evidence |
|---|---|---|
| Phase 0 Pre-flight | done 2026-10-29 | [PREFLIGHT.md](PREFLIGHT.md) |
| M0 Research | done 2026-10-29: 19 lanes, each independently verified | [research/](research/), [evidence/M0](evidence/M0/) |
| M0 Design and contracts | in progress (synthesis workflow wf_be184489-a92) | — |
| M1–M7 | not started | — |

## Blockers (exact cause)

- Unreal Engine 5.8 not installed: needs admin rights and about 150 GB; the build machine has 25 GiB free. Unreal lanes are source-only; build steps go in `OWNER-CHECKLIST.md`.
- Docker not installed (admin rights).
- ArduPilot SITL source build blocked by the machine's agent permission policy; scripted MAVLink drone used.
- No TAK certificate, no NATS run user, no tailnet route to `tail717ba0`: local fallbacks (decision 0004 #12).
- Commo (S4) build for macOS blocked by the permission policy: decision 0004 #3 fallback, the mesh gateway is the only mesh path; C4 blocked.
- nats.c static build (S5) blocked by the permission policy; S5 is an owner-checklist item.
