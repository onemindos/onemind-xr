# INT/HASH evidence

- `policy-go-test-race.txt`: services/policy `go vet` + `go test -race -v` (feat/policy-service f4519b17).
- `mavlink-go-test-race.txt`: services/bridge/bridges/mavlink `go vet` + `go test -race -v` (feat/mavlink-bridge-fleet 2b724093).
- `ros2-pytest.txt`: services/bridge/bridges/ros2 compileall + pytest (feat/ros2-bridge 44de8068): 174 passed, 1 skipped.
- `ci-equivalent.txt`: yamllint, validate_changed_yaml.py, vector JSON parse, gitleaks (the 9 findings are the pre-existing baseline files, none in changed paths).
- `cross-e2e-task.txt`, `cross-e2e-task-transcript.jsonl`: #568's e2e harness with policy f4519b17 (no D2-P1 patch) + MAVLink bridge 2b724093 + scripted drone sim-x1: direct arm/takeoff and the task-bearing goto executed, task done_ok.
- Shared vector file (identical sha256 8c7ae4d7… in all three branches): `omos/services/schemas/command/testdata/params-hash-vectors.json`.
- `github-ci-checks.txt`: GitHub CI on #554, #550, #566 after the push: all pass.
