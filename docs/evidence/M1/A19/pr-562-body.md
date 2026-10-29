## What

FEATURES A19 (per-service NATS identities, GAP-N5), following onemind-xr `docs/design/BUS-DESIGN.md` §7 and `docs/research/security-and-trust.md` Rec. 1. It reuses the permission proposal from #554. **Nothing is live.** This PR contains only config templates, a renderer, tests and the owner runbook. The NATS release is still Helm-CLI with a gitignored `values.secret.yaml`, so there are no credentials here, only `__<USER>_PW__` placeholders and SealedSecret instructions.

- `platform/nats/identities/identities.yaml` is the single source of truth: accounts `SYS`, `ONEMIND`, `CLIENT` and `AGENTS`, and one user per service:
  - `policy`, `entity`, `ingest`, `layer`, `projector`, `tak-client`, `bridge-mavlink-home`, `mesh-gateway`, `agent-fabric`
  - leaf-only users `leaf-tak`, `leaf-site-mac` and `hermes` (AGENTS)
  - `zeus` (CLIENT)
  - the shared users `onemind` and `leaf_tak` stay, and are restricted in stage B. Stage B closes policy's command domains (`cmd.{fleet,drone,robot,vehicle,mission,entity}.>`), `tak.cot.inject`, `tak.cot.mesh.tx`, `ent.*.*.state`, `evt.so.action.>` and `svc.policy.>`. Other command domains (`cmd.projector`, `cmd.ha`, `cmd.alert`) and forgeable `tak.cot.out`, `tel.fleet.*`, `evt.drone.*` stay open until stage C. They are listed in IDENTITIES.md "Limits" and proposal A19-P10.
  - from A2 the legacy users get connection types (`onemind` STANDARD and WEBSOCKET, `sys` and `monitor` STANDARD), so dropping `leafnodes.authorization` does not let them open leaf links.

  Subjects match #550, #551, #552, #553 and #554, plus the live projector and agent-fabric.
- `scripts/nats_identities.py` renders three things:
  - the Helm values templates (`platform/nats/values.identities.stage-{a1,a2,b}.template.yaml`, generated)
  - the new live `nats-config` ConfigMap: it keeps the live bcrypt hashes and the bare `45Gi` / `$SERVER_NAME` tokens
  - the test config
  - Hermes' sidecar `leaf.conf` (`leaf-conf`), whose local user carries the hub `hermes` allow-lists.

  `check` enforces the invariants: only `policy` publishes `cmd.*` (except `cmd.map.>`), only the projector publishes `tak.cot.inject` / `tak.cot.mesh.tx`, only `entity` publishes `ent.*.*.state`, agents never reach `svc.policy.*.confirm`, nobody publishes `_INBOX.>` (responders use `allow_responses`), and nobody gets stream admin.
- `platform/nats/IDENTITIES.md` is the owner runbook. Activation patches the ConfigMap and never runs `helm upgrade`. The stages:
  - **A1**: add the accounts and users. Config reload only.
  - **A2**: move the leaf users into accounts. Needs a one-pod-at-a-time restart, because nats-server 2.14.2 cannot reload `leafnodes.authorization.users`; proved in the tests.
  - **B**: restrict the shared users. Config reload.
  - **C**: retire the shared users.

  Each stage has a rollback, and the runbook includes the SealedSecret commands.
- Owner proposals A19-P1 to A19-P10: `docs/proposals/2026-10-29-nats-identities-a19.md`.

## Tests (run locally)

- `platform/nats/identities/permtest` runs the rendered config on **nats-server 2.14.2**, the live version, with random test-only passwords outside Git. It has two modes: compiled in-process (default), or the checksum-verified release binary as an external process (`-tags natsbin`). `go test -race` passes in both modes: 9 tests, **302 allow/deny checks**. The first commit's 293 checks also passed with bcrypt hashes, as live. Coverage:
  - the stage-B matrix for every user, with an observer that proves allowed messages are delivered
  - JetStream grants per stream and bucket
  - cross-account requests with the server-set `Nats-Request-Info`
  - streamed `svc.layer.watch` replies
  - Hermes, TAK and site leafs filtered in both directions
  - `allowed_connection_types`, including that `onemind`, `sys` and `monitor` cannot log in as leafs
  - the live → A1 → A2 (restart) → B activation and its rollback.
- FEATURES A19 acceptance:
  - a CLIENT user cannot publish `cmd.fleet.>` or `ent.*` (server `Permissions Violation`; CMD and ENT stay empty)
  - Hermes cannot publish `tak.cot.inject`, `cmd.fleet.>` or `cmd.ha.*`, and cannot reach `svc.policy.*.confirm`. **Each refusal is a server permission violation**: Hermes' client gets `Permissions Violation for Publish to "cmd.fleet.drone.sim-x1.goto"` from its sidecar, and the sidecar logs `Publish Violation - Subject "cmd.fleet.drone.sim-x1.goto"`. The sidecar does this because its rendered `leaf.conf` gives the local user the same allow-lists as the hub user. With hub-side leaf filtering alone, the first run showed these publishes dropped with no error and no log line.
- `python3 -m unittest scripts/tests/test_nats_identities.py`: 20 pass. `scripts/nats_identities.py check` passes.
- `helm template` with chart nats-2.14.2 renders A1, A2 and B to exactly the accounts the tests run.
- Smoke run on the nats-server 2.15.2 binary with the real services built from their PR branches, each logged in as its own user (entity #552, policy #554, omos-ingest #553, MAVLink bridge #550 on the site leaf):
  - The policy service sees CLIENT `zeus` as the verified actor.
  - Hermes, through its leaf, gets `awaiting_confirmation` on propose and "no responders" on confirm.
  - A human confirm dispatches a signed `cmd.fleet.drone.sim-x1.goto` across the site leaf to the bridge and the sim drone, and the outcome events come back.
  - The smoke run found two missing grants, both fixed here: the entity service's micro `$SRV` discovery subjects, and Hermes' service-import replies `_R_.>` over a leaf.

  Evidence: onemind-xr `docs/evidence/M1/A19/`.
- CI-equivalent checks pass locally: yamllint, `validate_changed_yaml.py`, `validate_manifests.py`, gitleaks (CI mode), `check_onemind_mcp_coverage.py`.
- The new workflow `.github/workflows/test-nats-identities.yml` runs `check`, the Python unit tests (PyYAML pinned to 6.0.2) and the Go permission tests. The Go tests use `-tags natsbin -race` against the nats-server v2.14.2 linux-amd64 release binary, checked against its published SHA256. They no longer compile the server: compiling it got the zeus-v2 runner container OOMKilled twice (1 GiB limit).

## Follow-ups (not in this PR)

- After stage A1 is live, #552 and #553 (today `omos-nats-creds`) and the projector and agent-fabric manifests switch to their own Secrets. #554 already reads `policy-service-runtime`.
- `values.template.yaml` says `20Gi` for the JetStream file store, but the live value is `45Gi` (drift found, not changed).

🤖 Generated with [Claude Code](https://claude.com/claude-code)

