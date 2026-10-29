# A1 owner steps (omos PR #558, branch feat/feed-grammar-fixes)

The full runbook is in omos `platform/data/redpanda-connect/CUTOVER.md`. Merging #558 changes
nothing live. Each later stage is its own small PR that the owner reviews and merges.

## 0. Before merging #558

- Rule on owner proposals A1-P1 to A1-P4 (omos `docs/proposals/2026-10-29-feed-grammar-cutover.md`).
  A1-P1 (`evt.world.rail.{id}`, BUS-P6) must be ruled before Stage 1.

## 1. After merging #558 (Stage 0, read-only check)

```bash
kubectl --context do-nyc3-omos-cloud -n omos get deploy redpanda-connect redpanda-connect-grammar feed-satellites
```
Expected: `redpanda-connect 1/1`, `redpanda-connect-grammar 0/0`, `feed-satellites 1/1`.

```bash
POD=$(kubectl --context do-nyc3-omos-cloud -n omos get pod -l app.kubernetes.io/name=redpanda-connect -o name | cut -d/ -f2)
kubectl --context do-nyc3-omos-cloud get --raw "/api/v1/namespaces/omos/pods/$POD:4195/proxy/streams"
```
Expected: only `feed-mta` active (the legacy pod is unchanged).

## 2. Stage 1 PR: `redpanda-connect-grammar` `replicas: 1`

After Argo syncs:
```bash
kubectl --context do-nyc3-omos-cloud -n omos get pod -l app.kubernetes.io/name=redpanda-connect-grammar -o wide
kubectl --context do-nyc3-omos-cloud -n omos logs deploy/redpanda-connect-grammar | grep -iE "error|permission" | head
```
Expected: 1/1 Running on `onemind-ecosystem-cpu-*`; no error or "Permissions Violation" lines.
If there is a permission violation, the `omos-nats-creds` user cannot publish `evt.world.>`,
`evt.sys.>` or `tel.feed.>`: revert to `replicas: 0` and widen that user's permissions first.

With a NATS user on the tailnet:
```bash
nats sub 'tel.feed.>' --count 4
nats stream subjects WORLD 'evt.world.rail.>' | head
nats stream subjects INTEL 'intel.space.>'
```
Expected: `tel.feed.mta.status` every 30 s with `stale:false`, `published` > 0 and
`lastError: null`; one `evt.world.rail.mta-*` subject per running train;
`intel.space.celestrak.tle` present. Keep running for 7 days.

## 3. Stage 2 PR: `redpanda-connect` and `feed-satellites` `replicas: 0` (one PR)

Do this only after 7 clean days and once the entity service accepts `rail` and TLEs (A1-P4), or
once the owner accepts trains and satellites being off TAK. Today nothing reaches ATAK from
`tak.cot.inject` anyway (A21).
```bash
kubectl --context do-nyc3-omos-cloud -n omos get deploy redpanda-connect feed-satellites
nats sub tak.cot.inject --count 20 --timeout 90s | grep -cE 'MTA-(MNR|LIRR)-|SAT-'
```
Expected: both Deployments `0/0`; count `0`.
Rollback: revert the Stage 2 PR.

## 4. Optional: rerun the replay test yourself

On a machine with `nats-server`, `nats`, `redpanda-connect` and Python with PyYAML, from an omos checkout:
```bash
platform/data/redpanda-connect/test/replay.sh /tmp/a1-replay
```
Expected: the last line is `RESULT: PASS (0 failed)`. If you can, also run it with Redpanda
Connect 4.103.1, the version the cluster runs: downloading that binary was denied on the build
machine.
