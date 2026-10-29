# A6 evidence: AOI-gated TAK projector

PR https://github.com/onemindos/omos/pull/563 (draft), branch `feat/projector-aoi`, base
`feat/layer-service` (#561). Machine: macOS arm64, Go 1.27.3, nats-server from `.tools/bin`.

| File | What |
|---|---|
| `go-test-race.txt` | `go vet` + `go test -race -count=1 -v ./...` in `services/projector`: 22/22 PASS (15 unit, 7 acceptance), CoTtypes.xml membership checked (`COTTYPES_XML` set) |
| `acceptance-results.log` | one line per acceptance check with the measured numbers |
| `test8-gateway-half.txt` | CP-2 test on #551's branch (read-only run); end-to-end half in `../A7/EVIDENCE.md` |
| `ci-equivalent.txt` | validate_changed_yaml, MCP coverage, kubeconform (4/4 valid), gitleaks dir + PR commits (no leaks), gofmt |
| `github-ci-run-40875135483.txt` | GitHub CI on PR #563: Build TAK projector / test (vet, race tests, 66 s, comparable to the 68 s local run that includes the entity-service e2e): pass. quality.yml does not trigger on a PR whose base is not main; it runs when the stack is retargeted |
| `OWNER-STEPS.md` | cluster sync order, live read-only checks, ATAK display checks |

| Acceptance test | Result |
|---|---|
| cot-projection §12 test 1 (types) | PASS: 59 rows, all in CoTtypes.xml or listed TAK types; 728 civilian-check combinations, none forbidden |
| §12 test 2 (AOI, 2,000 aircraft / 20 inside) | PASS: exactly 20 uids; restart → 20; AOI deleted → 20 removals at 3.03 s (limit 8 s), then 0 in 5 s |
| §12 test 3 (marking) | PASS |
| §12 test 4 + JADC2 stop-line 10 (people) | PASS: 0 CoT on both targets |
| §12 test 5 (echo, with the A4 entity service) | PASS |
| §12 test 6 (rate, ceiling) | PASS: 1/s inject, 1/5 s mesh; 5,000 in AOI → max 50 per second, 500 live |
| §12 test 8 | projector half PASS here; gateway half PASS in A7 (#551) |
| JADC2 stop-line 8 (`t-k-*` from TAK) | PASS: no entity, no task, no cmd/policy message, refusal logged |
| ATAK display | not passable here: OWNER-STEPS.md §3 |
