# Status

Resume rule: read this file and `docs/PREFLIGHT.md` first, then continue at the first
row that is not done.

| Milestone | State | Evidence |
|---|---|---|
| Phase 0 Pre-flight | done 2026-10-29 | [PREFLIGHT.md](PREFLIGHT.md) |
| M0 Research | done 2026-10-29: 19 lanes, each independently verified | [research/](research/), [evidence/M0](evidence/M0/) |
| M0 Design and contracts | done 2026-10-29: reviewed by three independent review agents (final: approve) | [design/](design/), [contracts/](contracts/), [research/SUMMARY.md](research/SUMMARY.md) |
| M1 lane A (spine, omos draft PRs) | built 2026-10-29; each independently reviewed (no first-pass approvals) and every finding fixed and re-tested; awaiting owner merge: [#552 A4 entity](https://github.com/onemindos/omos/pull/552), [#553 A0 ingest](https://github.com/onemindos/omos/pull/553), [#554 I4 policy](https://github.com/onemindos/omos/pull/554), [#550 H2 MAVLink](https://github.com/onemindos/omos/pull/550), [#551 A7 mesh gateway](https://github.com/onemindos/omos/pull/551). Suggested merge order: A0, A4, I4, H2, A7 | [evidence/M1](evidence/M1/) |
| M1 lane A wave 2 (omos draft PRs) | built 2026-10-29, each independently reviewed, majors fixed; awaiting owner merge: [#558 A1 feeds](https://github.com/onemindos/omos/pull/558), [#555 A22 stream caps](https://github.com/onemindos/omos/pull/555) (merging activates; COT 72 h cap drops history), [#556 A24 grammar proposals](https://github.com/onemindos/omos/pull/556), [#562 A19 identities](https://github.com/onemindos/omos/pull/562), [#557 I6/H6 red-team suite](https://github.com/onemindos/omos/pull/557) (stacked on #554), [#560 A21 TAK Server leg](https://github.com/onemindos/omos/pull/560), [#561 A5 layer service](https://github.com/onemindos/omos/pull/561) (stacked on #552), [#563 A6 projector AOI](https://github.com/onemindos/omos/pull/563) (stacked on #561) | [evidence/M1](evidence/M1/) |
| M2 lane B client (source only) | written, host-testable core passes; UE not installed: [xr #3](https://github.com/onemindos/onemind-xr/pull/3) | [evidence/M2](evidence/M2/) |
| M3 lane C | L0 web client [xr #2](https://github.com/onemindos/onemind-xr/pull/2) (headless e2e passes); video [omos #559](https://github.com/onemindos/omos/pull/559) | [evidence/M3](evidence/M3/) |
| M4–M7 | next: M4 Act | — |

## Blockers (exact cause)

- onemind-xr PR #1 (M0) is approved by the review agent, but the merge was refused by this machine's permission policy ("Merge Without Review"). The owner merges it. Work continues on branch `run/m0`.
- Lane A activation needs owner approval of the ENT and kv-entity-correlation stream CRs, the first GHCR image builds with pinned digests, and NATS 2.14.9 before per-message TTL is enabled (see the proposals in omos #552).

- Unreal Engine 5.8 not installed: needs admin rights and about 150 GB; the build machine has 25 GiB free. Unreal lanes are source-only; build steps go in `OWNER-CHECKLIST.md`.
- Docker not installed (admin rights).
- ArduPilot SITL source build blocked by the machine's agent permission policy; scripted MAVLink drone used.
- No TAK certificate, no NATS run user, no tailnet route to `tail717ba0`: local fallbacks (decision 0004 #12).
- Commo (S4) build for macOS blocked by the permission policy: decision 0004 #3 fallback, the mesh gateway is the only mesh path; C4 blocked.
- nats.c static build (S5) blocked by the permission policy; S5 is an owner-checklist item.
