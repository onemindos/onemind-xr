# M4 / I1, I3, D10 evidence (omos PR #567, branch feat/agents-watch-briefs)

| File | What it shows |
|---|---|
| `go-test-race.txt` | `go vet` + `go test -race -count=1 -v ./...` of services/agentops: 31 tests pass, 0 fail, 0 skip (7 packages), including e2e against the real #552 and #554 binaries |
| `transcript.log` | One line per checked fact from the integration and e2e tests (alerts, proposals, briefs, chat replies) |
| `binary-smoke.txt` | The agentops binary with the shipped ConfigMap watch file: 4 watches load, offline brief via stub, in-process agents on `tel.fleet.agent.*`, /metrics |
| `ci-equivalent.txt` | yamllint, validate_changed_yaml, MCP coverage, kubeconform, gitleaks on the new paths |
| `proposal-subject-check.txt` | #556's `check_proposal_subjects.py` on the I1-P1..P7 file: 28 subjects, 0 errors |
| `local-model-licences.txt` | Hugging Face / GitHub API licence facts for the local-model candidates |
| `OWNER-STEPS.md` | Steps needing ATAK, the Mac, live Hermes or a real model |

Acceptance on this machine:
- **I3**: a watch job raised an alert and proposed `task.assign` (rtl, sim-x1) that waits for a human: stub policy and the real #554 policy both answered `awaiting_confirmation`; 0 confirm requests, 0 `cmd.>` messages.
- **D10**: brief with the network off (Hermes and local model unreachable, stub wrote it), 6 entities cited (>= 3) plus stale/silent feeds named; also from the real #552 `svc.entity.query`.
- **I1**: Hermes heartbeat became `ent.agent.agent-hermes-zeus-legacy.state` in the real #552 entity service with the current task in `meta`; GeoChat from a scripted ATAK message reached a stub Hermes and the reply came back over the TAK Server and mesh subjects. ATAK display: **built, awaiting owner check** (OWNER-STEPS 3).
