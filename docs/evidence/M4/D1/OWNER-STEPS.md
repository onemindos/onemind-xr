# D1 / D6 geofence and alert service: owner steps

PR: https://github.com/onemindos/omos/pull/569 (draft, stacked on #552). Proven on the build
machine: see `go-test-race.txt` (30 tests pass), `e2e-transcript.txt` (local stack, E2E PASS),
`compat-projector-policy.txt`, `omos-ci-local.txt`, `grammar-check-a24.txt`. These steps are
what needs the owner, live access or another PR. Keep them in this order.

## 1. Rulings (no live change)

Tick the boxes in omos `docs/proposals/2026-10-29-geofence-alert-service.md`: D1-P1 (KV value),
D1-P2 (result subject), D1-P3 (`evt.so.alert.*`), D1-P4 (policy catalogue), D1-P5 (open
question), D1-P6 (NATS user), D1-P7 (verb), D1-P8 (no people).
Expected: nothing changes live; the rulings unblock steps 2 and 3.

## 2. Changes in other PRs (their owners)

- #554 (policy): apply the D1-P4 patch, which adds `zone.create|update|remove` (A2) executing
  as `cmd.map.geofence.{id}.{verb}` and subscribes the result handler to `evt.map.geofence.*`.
  Test there: a `zone.create` proposal confirmed by one human yields
  `evt.so.action.dispatched`, then `evt.so.action.executed` once the geofence service answers.
- #562 (identities): add the D1-P6 `geofence` user block to `platform/nats/identities/identities.yaml`,
  then `python3 scripts/nats_identities.py values` and commit. Change `policy` to
  `kv_read: [geofences]` plus `kv_write: [mission-state]`.

## 3. Before the first sync of `platform-omos-geofence`

1. #552 is merged, the owner has approved `ENT` (ONT-P1) and the entity service is running.
2. Put the policy public key in `platform/omos/geofence/configmap.yaml`. Run this where the
   policy signing key was created (platform/policy/README.md in #554):
   ```bash
   openssl pkey -in policy-signing.pem -pubout
   ```
   Paste the output as `data: {policy-2026-10.pem: | ...}`. Expected: one `-----BEGIN PUBLIC KEY-----` block.
   This is a public key, so no SealedSecret is needed.
3. After merge, the first `Build geofence service` run on `main` uploads `image-digest.txt`.
   Replace `ghcr.io/onemindos/omos-geofence-service:0.1.0` in
   `platform/omos/geofence/deployment.yaml` with `…:0.1.0@sha256:<digest>` (PR).
4. Sync by hand in Argo CD (`platform-omos-geofence`, manual sync). Then check read-only:
   ```bash
   kubectl --context do-nyc3-omos-cloud -n omos get deploy omos-geofence-service
   kubectl --context do-nyc3-omos-cloud -n omos logs deploy/omos-geofence-service | grep -E '"msg":"(restored|geofence service ready)"'
   ```
   Expected: 1/1 ready; the log shows `geofence service ready` with `"signature_required":true`
   and `"keys":["policy-2026-10"]`. If no key is mounted, the pod exits at start (fail closed).
5. Read-only bus check (any client with the NATS credentials, from the owner's Mac):
   ```bash
   nats req svc.geofence.alerts '{}'
   nats stream view EVT --subject 'evt.so.alert.>' --last 5     # alert stream
   nats sub 'evt.map.geofence.>'                                 # crossings and command results
   ```
   Expected: `{"v":1,"alerts":[...]}`. Feed outage alerts appear for any feed whose
   `tel.feed.{source}.status` says `stale: true` or goes quiet for 90 s.

## 4. Acceptance on the owner's stack (Mac and client)

- **D1** (FEATURES: "Drawing an AOI changes what TAK receives"): in the client (or ATAK plugin),
  draw an AOI over a quiet area where an aircraft is visible on the OneMind map but not on ATAK.
  Then confirm the `zone.create` proposal as a human. Expected: within 15 s the aircraft appears
  on ATAK; deleting the zone (`zone.remove`, confirm) removes it after `AOI_EXIT_GRACE_S` (60 s).
  On this build machine the projector half was proven with `tak.cot.inject` (phase 1 of
  `e2e-transcript.txt`), not with an ATAK device.
- **D6** ("A sim-* drone leaving the geofence raises an alert"): draw a keep-in around
  sim-x1's home with `applies_to: ["sim-x1"]` and confirm it. Fly sim-x1 out with a confirmed
  `asset.goto` (D3 path). Expected: `evt.map.geofence.{zone}.exit` and an `evt.so.alert.raised`
  with `alert_kind: geofence`, `level: warning`, `reason: left_keep_in`. Note: the build-machine
  run started the scripted sim already airborne (`harness/sim-airborne.diff`), because arm and
  takeoff need the real policy service and a human confirm, which that run did not fake.
- **Hermes** (needs a real model, so not run here): ask Hermes to "draw a keep-out around the
  barn". Expected: Hermes calls `svc.policy.command.propose` with `zone.create`, and the zone is
  written only after a human confirm. If Hermes publishes `cmd.map.geofence.*` directly, the
  service refuses it with `evt.map.geofence.refused`, `code: unsigned` (proven here with a
  stand-in publish, phase 5).

## 5. Re-run the build-machine evidence

From an omos checkout of `feat/geofence-alerts`, with `nats-server` on PATH:
```bash
cd services/geofence && go vet ./... && NATS_SERVER_BIN=$(command -v nats-server) go test -race -count=1 ./...
```
Expected: `ok` for internal/bus, engine, geo, sig and zone (30 tests). The full-chain run is
`harness/e2e.sh`. It needs the binaries of #550, #552, #563 and this PR built into one folder,
plus the scratch driver `harness/e2e-driver.go.txt` built inside a copy of `services/geofence`;
its paths are this build machine's.
