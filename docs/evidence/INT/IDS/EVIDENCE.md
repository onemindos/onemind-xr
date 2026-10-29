# INT/IDS evidence: omos #562 (feat/nats-identities), second pass

| File | What |
|---|---|
| python-checks.txt | `nats_identities.py check` (24 users, 5 accounts), 29 unit tests, `check-roles` on #565's role file and on the A19-P11 fixture |
| go-vet.txt | `go vet` (both build tags) |
| permtest-inproc-2.14.2.txt | `go test -race -v`, nats-server 2.14.2 compiled in: 11 tests, 583 permission checks pass |
| permtest-natsbin-2.15.2.txt | same against the local nats-server 2.15.2 binary (`-tags natsbin`): 583 pass |
| ci-equivalent.txt | gofmt, yamllint, validate_changed_yaml, validate_manifests, gitleaks |
| OWNER-STEPS.md | what needs the cluster, the keychain or other PR owners |

Each test started its own disposable nats-servers on free ports (temp store dirs, work dir /Users/danielcarku/Desktop/work/.run/tmp-ids), with random test-only passwords outside Git.

Finding: with `allow_responses: true` (max 1), a CLIENT that holds two subscriptions matching `cmd.map.>` gets `Publish Violation` on `_R_.*` when it acks a Hermes map action through the service import. Fixed with `{max: -1, expires: 5s}` and covered by TestHermesLeaf.

CI on omos #562 at 19aa0b87: all 5 checks pass (test, Lint & Validate, Helm render, kubeconform, gitleaks). The first push (eeb2352d) failed `test` on a release-binary timing race in TestHermesLeaf (log line read before the external sidecar flushed it). Fixed by polling (19aa0b87), then 5/5 local reruns passed.
