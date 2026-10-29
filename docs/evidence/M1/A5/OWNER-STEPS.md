# A5 layer service: owner steps

omos PR #561 (draft, branch `feat/layer-service`, stacked on #552 `feat/entity-service`).
Everything below needs the owner's approval or live cluster access. The build machine proved the
service against a disposable nats-server (`go-test-race.txt`, `go-test-long-timers.txt`,
`go-test-long-deadwatch.txt` in this folder).

## 1. Rule on the proposals

1. omos `docs/proposals/2026-10-29-entity-service-ent-stream.md` (A4-P1..P5, PR #552). `ENT` must exist first.
2. omos `docs/proposals/2026-10-29-layer-service.md` (A5-P1..P8, PR #561).

Expected result: approvals recorded on the PRs. Merge #552 before #561 (#561's base is #552's branch).

## 2. Before syncing (read-only checks)

```
kubectl --context do-nyc3-omos-cloud -n nats get keyvalues kv-world-state kv-fleet-state
nats --context omos kv info world-state      # expect: TTL 5m, history 1, last_seq 0 (never written)
nats --context omos kv info fleet-state      # expect: TTL 30s, history 3, last_seq 0
nats --context omos stream info ENT          # expect: exists (after A4-P1), subjects ent.*.*.state
kubectl --context do-nyc3-omos-cloud -n omos get deploy omos-entity-service   # expect: 1/1 ready
```

## 3. Image

After #561 merges, the `Build layer service` workflow on `main` pushes
`ghcr.io/onemindos/omos-layer-service:0.1.0` and uploads `image-digest.txt`. Replace the tag
in `platform/omos/layer/deployment.yaml` with `0.1.0@sha256:<digest>` in a follow-up PR.

## 4. Sync (owner action in Argo CD)

Sync `platform-omos-layer` by hand (it has no automated sync). Then:

```
kubectl --context do-nyc3-omos-cloud -n omos rollout status deploy/omos-layer-service
kubectl --context do-nyc3-omos-cloud -n omos logs deploy/omos-layer-service | head   # "lease acquired", "caught up with ENT"
nats --context omos kv ls world-state            # expect: fleet.meta, tak.meta, ..., air.cells.z2, sea.cells.z6, _lease.layer-service
nats --context omos kv get world-state fleet.meta --raw | jq .freshness   # expect: status fresh (no feeds) and count
nats --context omos kv get fleet-state hub --raw  # expect: {"v":1,"site":"hub","connected":true,...}
nats --context omos req svc.layer.list '{"v":1}'  # expect: 11 layers with counts and status
nats --context omos req --replies=0 --timeout=3s svc.layer.snapshot '{"v":1,"layer":"fleet"}'   # chunks; the last one has "last":true
```

Expected: the hazards layer shows `stale` until omos-ingest (#553) publishes `tel.feed.nws-alerts.status`;
then `fresh`.

## 5. On the owner's Mac (client lane, before the cluster sync)

Run the service from its branch against the Mac's local NATS (the entity service from #552 must be
running there too, writing `ENT`):

```
git -C omos fetch origin feat/layer-service && git -C omos switch feat/layer-service
nats kv add world-state --history 1 --ttl 5m
nats kv add fleet-state --history 3 --ttl 30s
cd omos/services/layer && LEASE_BUCKET=world-state NATS_URL=nats://127.0.0.1:4222 SITE=site-mac go run ./cmd/layer-service
nats req svc.layer.list '{"v":1}'                  # expect: layers with counts; fleet count >= 1 once sim-x1 is an entity
nats kv get fleet-state site-mac --raw             # expect: connected true
```

Expected: the OneMind client (`OneMindLayers`) loads `fleet` with one `svc.layer.snapshot` and then
follows `svc.layer.watch`; `sim-x1` moves without re-snapshots.

## 6. Account split (later, with BUS-DESIGN D6)

Export `svc.layer.>` from `ONEMIND` with `response_type: stream` and `response_threshold` of at least
2 minutes; import it into `CLIENT` with `share: true`; add each CLIENT user with groups to the
`principals.json` in `platform/omos/layer/deployment.yaml` (A5-P6, A5-P7).
