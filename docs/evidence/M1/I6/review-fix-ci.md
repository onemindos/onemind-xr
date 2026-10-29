# I6 review fixes: CI record

- Commit: 1a1fbdf7 on feat/agent-eval-redteam (PR https://github.com/onemindos/omos/pull/557, base feat/policy-service)
- PR #557 targets feat/policy-service, so the pull_request triggers (build-policy.yml, quality.yml) do not fire; `gh pr checks 557` shows no checks.
- Ran build-policy.yml by workflow_dispatch on feat/agent-eval-redteam:
  https://github.com/onemindos/omos/actions/runs/40869950371 (result: success)
  - test job (go vet, go test -race -count=1 ./..., incl. RT01-RT18 and TestForbiddenNames): success
  - image job also ran (it skips only pull_request events) and pushed
    ghcr.io/onemindos/policy:sha-1a1fbdf7... from the feature branch. That tag is not referenced by any manifest.
- Local: review-fix-test-output.txt (24 top-level PASS, 0 FAIL); redteam-report-review-fix.md (18/18 PASS).
- Negative checks: RT18 fails with the old quota key (req.RequestedBy.ID); RT17 fails with the old unconditional text refusal.
- gitleaks: no leaks (diff commits and services/policy).
- Re-check pull_request CI once #554 merges and #557 is rebased onto main.
