# Contract: layer delta

Status: **M0, proposed.** Date: 2026-10-29. Owner of the bus side: omos (FEATURES A4, A5,
A14). Consumer: the OneMind client (`OneMindLayers`, B4/B14), the L0 web client (J1).

This contract says how a client follows **changes to a layer** after loading its snapshot
([`layer-snapshot.md`](layer-snapshot.md)). Entity fields are defined in
[`entity.md`](entity.md). Bus layout and permissions:
[`../design/BUS-DESIGN.md`](../design/BUS-DESIGN.md).

## Sources of truth in omos

| Item | omos file | State |
|---|---|---|
| Subject `ent.{entity_type}.{entity_id}.state` | `CLAUDE.md` (grammar block) | exists |
| Stream `ENT` with per-message TTL and delete markers | `platform/nats/streams/ent.yaml` | **proposed** (BUS-DESIGN §4.1) |
| Layer manifest key `{layer}.meta` (layer-level changes) | `platform/nats/streams/kv-world-state.yaml` | exists, new purpose **proposed** |
| `evt.entity.{entity_id}.{event}` (lifecycle, optional for clients) | `CLAUDE.md` pattern `evt.{domain}.{entity}.{event}`; `EVT` binding in `platform/nats/streams/evt.yaml` | **proposed**, owner proposal BUS-P5 |
| `svc.layer.watch`, `svc.layer.unwatch` | `services/layer/` (A5) | **proposed** |

## 1. Two ways to follow a layer

| Path | Who | AOI filter | Transport |
|---|---|---|---|
| **Watch** (required) | every client, including the `CLIENT` account | done by the layer service | `svc.layer.watch` request; batches stream to the caller's reply inbox |
| **Direct** (optional) | trusted clients in `ONEMIND` or at a site; `small`/`medium` layers or LAN only | done by the client | ordered consumer on `ENT` (the same one that gave the snapshot) |

The grammar has no geographic token, so AOI filtering cannot be a subject filter
(BUS-P8). A plain core subscription to `ent.aircraft.>` is allowed for trusted
clients but **is not a delta contract**: core subscribers never see expiry markers (P10 in
nats-platform.md), so they must expire entities from `expiry_t` themselves.

## 2. Watch path

### 2.1 Start, renew, stop

Request `svc.layer.watch`:

```jsonc
{
  "v": 1,
  "layer": "air",
  "aoi": { "bbox": [-74.3, 40.5, -73.6, 41.0] },   // same shapes as the snapshot; required for large layers
  "zoom": 9,
  "fields": "render",
  "from_seq": 9876544,          // snapshot ent_seq + 1
  "lease_s": 60,                // 10–300; the watch ends if not renewed
  "max_rate_hz": 4,             // batches per second, 1–10
  "watch_id": null              // set to renew or to change AOI/zoom
}
```

First reply: `{ "v":1, "watch_id":"w-01JB…", "lease_s":60, "ent_seq":9876600 }`, then batches
on the same reply subject until the lease ends. Renew by sending the same request with
`watch_id` before `lease_s` runs out (recommended at half the lease). Changing `aoi` or
`zoom` with the same `watch_id` makes the service send `enter`/`exit` ops for the
difference. `svc.layer.unwatch` with `{ "v":1, "watch_id": "…" }` ends it.

If `from_seq` is older than the service can replay (it keeps a bounded recent window,
proposed 5 minutes), the first reply is `{"error":"resnapshot"}` and the client loads a new
snapshot.

### 2.2 Delta batch

```jsonc
{
  "v": 1,
  "watch_id": "w-01JB…",
  "layer": "air",
  "n": 42,                       // batch counter for this watch, +1 every batch, starts at 1
  "from_seq": 9876544,           // ENT sequence range covered (not contiguous after AOI filtering)
  "to_seq": 9876610,
  "as_of": 1793275201000,
  "mode": "entities",            // entities | cells
  "ops": [
    { "op": "upsert", "e": { /* render entity, layer-snapshot.md §4 */ } },
    { "op": "enter",  "e": { /* render entity that moved into the AOI or matched after an AOI change */ } },
    { "op": "exit",   "id": "icao-a1b2c3", "reason": "left_aoi" },
    { "op": "remove", "id": "icao-d4e5f6", "reason": "expired" },     // expired | deleted | not_live | released_out
    { "op": "cells",  "z": 4, "cells": [[x, y, count, lat, lon]] }   // cell mode only: changed cells
  ],
  "meta": null                   // the layer manifest, included only when it changed (freshness, count, object version)
}
```

Rules:
- **Coalescing:** within one batch the service sends at most one op per entity (the newest).
- **Size:** a batch is at most 256 KiB and at most 1,000 ops; a busy layer sends more
  batches up to `max_rate_hz`, then coalesces harder. It never queues without bound
  (run prompt reliability list "Backpressure and limits").
- **Heartbeat:** with no changes, an empty batch (`ops: []`) is sent every 10 s, so the client
  can tell a quiet layer from a dead watch.
- **Gaps:** if `n` skips, or no batch (including heartbeats) arrives for 30 s, the client
  drops the watch and loads a new snapshot.
- **Order:** apply ops in array order. For the same entity across batches, ignore an
  `upsert` whose `seq` is not greater than the one held (entity revision rule,
  lattice-patterns.md §A).
- **Static layers:** the only delta is `meta` with a new `object` version; the client
  downloads the new object and swaps it in.

## 3. Direct path (`ENT` ordered consumer)

Each message on `ent.{type}.{id}.state` is a full entity (not a patch). Map it to ops:

| What arrives | Headers | Client op |
|---|---|---|
| Entity message, `live: true` | `Nats-Msg-Id: {entity_id}:{seq}`, `Nats-TTL` | upsert (if `seq` is newer) |
| Entity message, `live: false` | `Nats-TTL: 60` | remove (`deleted`/`not_live`) |
| Empty message with `Nats-Marker-Reason: MaxAge` (subject delete marker) | `Nats-Rollup: sub` | remove (`expired`) |
| Empty message with `Nats-Marker-Reason: Remove` or `Purge` | — | remove (`deleted`) |

Marker behaviour is proved for `MaxAge` (nats-platform.md P10). `Remove`/`Purge` reasons come
from ADR-43 (nats-platform.md S1) and were not run here.

Layer-level changes on the direct path: watch the KV key `{layer}.meta` in `world-state`.

If the TTL fields are not available in NACK (BUS-DESIGN §4.1 prerequisite 2), no markers are
produced. The client then also removes an entity when `now > expiry_t`.

## 4. Rates and sizes (estimates for planning)

| Layer | Update source | Typical rate into `ENT` | Watch traffic for one AOI view |
|---|---|---|---|
| `fleet` | `tel.fleet.>` at 1–10 Hz, published on change beyond `ce` or heartbeat | < 50 msg/s | < 15 KB/s |
| `tak` | CoT PLI, about every 5–30 s per EUD | < 50 msg/s | < 15 KB/s |
| `air` | AOI poll every 10 s (OpenSky anonymous, SOURCE-REGISTRY F112) | 2,000 aircraft → about 200 msg/s | about 50 KB/s at 250 B per entity, 4 batches/s of about 12 KB |
| `sea` | AIS stream, minutes per vessel | 50,000 vessels → about 100–500 msg/s (unverified) | cell mode at wide zoom: changed cells only |
| `hazards`, `space`, `transit`, `rf` | seconds to hours | < 50 msg/s | small |

Entity service rate control (fusion-and-tracking.md "Rate control"): publish only on a real
change (moved more than its `ce`, class or status changed) or at a heartbeat at least every
`min(stale/2, 2 min)`. Size of one full entity on `ENT`: recommended under 4 KiB, hard limit
64 KiB (the live `max_payload` is 1 MiB; larger media goes to the Object Store).

Client budget: apply deltas once per rendered frame; the B4 target is 10,000 entities at
60 fps on the Mac.

## 5. Freshness fields in deltas

| Field | Where | Use |
|---|---|---|
| `t` | each entity | source time of the newest observation; staleness fade (B14) |
| `stale` | each entity | seconds after `t` before the entity dims |
| `expiry_t` | each entity | removal time if no update arrives |
| `seq` | each entity | revision order; drop older |
| `origin` | each entity | owning site (edge conflict rules, BUS-DESIGN §9) |
| `as_of` | each batch | when the service built the batch |
| `meta.freshness` | batch, when changed | layer status and per-feed `fetchedAt`, `count`, `lastError`, `stale` |

## 6. Lifecycle events (optional for clients)

`evt.entity.{entity_id}.{event}` with `event` ∈ `created deleted expired correlated
decorrelated overridden conflict alert` (lattice-patterns.md §D) is for the timeline,
alerts and Hermes, not for rendering. Rendering uses only §2 or §3. This subject needs owner
proposal BUS-P5 (BUS-DESIGN §10) and is not published until it is accepted.

## 7. Acceptance tests (Phase 2)

| Test | Pass when |
|---|---|
| No gap | Snapshot then watch from `ent_seq + 1` during a 200 msg/s replay: the client's entity set equals a reference built from the whole stream |
| Expiry | An aircraft with no new observation disappears within 5 s after its 120 s TTL, on both paths |
| AOI move | Panning the AOI produces `exit` for entities that left and `enter` for new ones; no entity outside the AOI stays on screen |
| Backpressure | A 50,000-vessel burst keeps every batch ≤ 256 KiB and ≤ `max_rate_hz`; service memory stays bounded |
| Dead watch | Killing the layer service makes the client re-snapshot within 30 s of the last heartbeat |
| Sim only | Every fleet entity in the test has `simulated: true` and an id starting `sim-` (decision 0004 #13) |

## Unverified

- Streamed replies to a `CLIENT` inbox through a service import (only single replies were
  proved, nats-platform.md P6).
- Marker reasons other than `MaxAge`.
- The rates in section 4 for `sea` and the final per-type TTLs.
