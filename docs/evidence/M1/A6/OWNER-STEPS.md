# A6 owner steps (need the owner's cluster approval, Mac or ATAK device)

PR: https://github.com/onemindos/omos/pull/563 (draft, stacked on #561 → #552). Proposals:
omos `docs/proposals/2026-10-29-projector-aoi.md` (A6-P1..P5).

## 1. Before syncing (cluster, owner)

1. Approve A4-P1 (ENT) and sync `platform-omos-entity` first; confirm it runs:
   `kubectl --context do-nyc3-omos-cloud -n omos get deploy omos-entity-service`
   Expected: 1/1 available.
2. Merging #563 auto-syncs `platform-omos` and restarts `omos-tak-projector` (legacy Python) with
   `ENT_PROJECTION=false`. Check its log:
   `kubectl --context do-nyc3-omos-cloud -n omos logs deploy/omos-tak-projector | grep ENT_PROJECTION`
   Expected: `ent.*.*.state not projected here (ENT_PROJECTION=false; see omos-tak-projector-gated)`.
3. After the first `Build TAK projector` run on `main`, download its `omos-tak-projector-image`
   artifact and replace `image: ghcr.io/onemindos/omos-tak-projector:0.1.0` in
   `platform/omos/projector/deployment.yaml` with `...:0.1.0@sha256:<digest>` (follow-up PR).
4. Sync by hand: `argocd app sync platform-omos-projector`.
   Expected: pod `omos-tak-projector-gated-*` Ready; log line `projector ready`.

## 2. Live check (read-only)

```
kubectl --context do-nyc3-omos-cloud -n omos port-forward deploy/omos-tak-projector-gated 18080:8080 &
curl -s localhost:18080/status | jq '{ready, counters, entities: .entities, admitted: .admitted, aoi_active}'
nats --context <omos> sub tak.cot.inject --headers-only   # OneMind-Projector: hub on every message
```
Expected: until D1 writes AOIs to KV `geofences`, only own `sim-*` assets, tasked entities and
pinned places/zones/structures are sent; `blocked_g4_person` and `blocked_g8_not_relevant`
counters grow; no `OM.person.*` or `OM.track.*` uid with a person kind ever appears.

## 3. ATAK display (owner LAN, cot-projection.md §12 owner checks; OWNER-CHECKLIST step 9)

Run the local stack on the Mac (local `nats-server`, entity service and projector from the
branches) or use the cluster after step 1:

```
cd omos/services/projector && NATS_URL=nats://127.0.0.1:4222 PROJECTOR_TARGET=server go run ./cmd/tak-projector
```
Publish a `sim-x1` record and a vessel record to `ent.*.*.state` (fixtures in
`internal/projector/fixtures_test.go`), put an AOI in KV `geofences`
(`nats kv put geofences aoi-home '{"zone_id":"aoi-home","active":true,"geo":{"shape":"polygon","kind":"aoi","coords":{"type":"Polygon","coordinates":[[[lon1,lat1],[lon2,lat1],[lon2,lat2],[lon1,lat2],[lon1,lat1]]]}}}'`).

| Check | Expected (record what ATAK actually shows) |
|---|---|
| `sim-x1` marker | friendly UAV icon `a-f-A-M-H-Q`, callsign, remarks end `SIMULATED` |
| vessel in the AOI | neutral surface icon `a-n-S-X-M-C` |
| earthquake event `a-n-X-i-g-e` | record the icon ATAK shows (unverified) |
| delete the AOI | after 60 s the vessel gets a removal (stale = time + 1 s): record whether ATAK deletes or greys it |
| markers without `<__group>` | look the same as before |
