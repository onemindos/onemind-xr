# E-ENT: entity-service whole-loop fixes (omos #552, branch feat/entity-service)

Commit: 996acfc5 (pushed to feat/entity-service; PR body updated).

| Item | Fix | Test |
|---|---|---|
| E2E bug 2: `releasable_to` always `[]` | A `sim-*` entity whose sources are all `tel.fleet.*` gets `OWN_RELEASABLE_TO` (default `homestead`; an empty value means no group). Feeds, non-sim fleet ids and records with any CoT source keep `[]` | `TestOwnSimFleetAssetReleasableTo`, `TestOwnReleasableToConfigurable`, `TestReleasableToNeverWidenedForFeeds` |
| E2E bug 6: no `cot_type` on fleet entities | Set from class, kind and environment per robotics-cot-profile.md §2. A robot with kind usv or uuv gets environment surface or subsurface | `TestFleetCotTypeFromClass` (10 cases) |
| CAP: `health.connection` stays online after telemetry stops | The sweep sets `offline` on `stale` or `lost`, and the next report restores the earlier value | `TestConnectionOfflineWhenStale` |
| CAP: `svc.entity.get` unknown id | Returns `{"v":1,"error":"not_found","detail":...}` plus the micro 404 headers. entity.md defines no error body, so this uses the layer-snapshot.md shape | `TestRPC` (body assertion) |

Files:
- `go-test-race.txt`: full `go test -race -count=1 -v ./...` (all packages pass, golden replay unchanged).
- `live_check.sh` / `live_check.txt`: a disposable nats-server and the entity-service binary. Shows the marking, cot_type, the ENT headers, the stale flip to offline, and the 404 body.
- `ci-local.txt`: yamllint (warnings only), validate_changed_yaml, validate_manifests (kubeconform), gitleaks.

Owner steps: none new. E2E/OWNER-STEPS.md §3 (ATAK sees `OM.drone.sim-x1`) is now blocked only by the projector Deployment setting `TAK_RELEASE_GROUPS=homestead`.
