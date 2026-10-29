# INT/E-MIS evidence (omos #568, feat/mission-service @ 4242cf7c)

- Removed `services/mission/test/interop/policy-task-fleet-params-hash.patch` and the
  `POLICY_PATCH` / `E2E_POLICY_PATCHED` switch; the default e2e expects done_ok.
- D2-P1 in `docs/proposals/2026-10-29-mission-service-owner-proposals.md` is now marked resolved by POL-16 (#554/#550/#566).
- `go-test-race.txt`: go vet (plain and -tags e2e) + go test -race -v: all pass.
- `e2e.txt`, `e2e-transcript.jsonl`: test/e2e.sh, no patch, policy f4519b17 + bridge 2b724093 +
  scripted drone sim-x1: PASS. Task goto `sig_verified: true`, `outcome: executed`, task facts
  created,sent,received,executing,done_ok; mission completed; tests 3, 6 and invariant pass.
- `ci-equivalent.txt`: validate_changed_yaml, shellcheck, gitleaks on changed paths (no leaks).
- `github-ci-checks.txt`: GitHub CI on #568 after the push: all pass (image/acceptance skipped as on PRs).
