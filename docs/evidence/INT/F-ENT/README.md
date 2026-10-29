# F-ENT: fleet health.overall and connection restore (omos #552, feat/entity-service)

Commits: 6e4cb407 (engine, tests, golden), 4f4f65d8 (README).

| Item | Change | Tests |
|---|---|---|
| `health.overall` (entity.md 4.1) for fleet entities | Set in `derive` for any entity with a `fleet-*` source. The worst condition wins. failed: connection offline, battery_pct <= 15, lifecycle lost/expired. warning: degraded or missing link, battery_pct <= 30, stale. Otherwise healthy. Thresholds: `internal/engine/policy.go` constants plus the README table | `TestFleetHealthOverall` (10 cases), `TestFleetHealthOverallStaleness`, `TestFleetOverallMapping` (7 cases), `TestNoOverallWithoutFleetSource` |
| E-ENT minor: pre-stale connection restore | Only a `fleet-*` observation restores the connection. A tak-mesh CoT report brings the lifecycle back to active, but the connection stays offline (overall failed) | `TestConnectionNotRestoredByCotSource`; the existing `TestConnectionOfflineWhenStale` still passes |

Files:
- `go-test-race.txt`: gofmt -l (empty), go vet, and `go test -race -count=1 ./...`, all passing
- `health-tests-verbose.txt`: the new tests, verbose
- `gitleaks.txt`: gitleaks on the new commits, no leaks
- `gh-pr-checks.txt`: GitHub CI on the PR, all passing

The golden ENT state (`testdata/golden/entities.json`) changed in one place: the fleet entity now has `"overall": "healthy"`.

Owner steps: none.
