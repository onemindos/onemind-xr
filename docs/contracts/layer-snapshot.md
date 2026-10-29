# Contract: layer snapshot

Status: **M0, proposed.** Date: 2026-10-29. Owner of the bus side: omos (FEATURES A4 entity
service, A5 layer snapshots, A14 world-tier cells). Consumer: the OneMind client
(`OneMindBus`, `OneMindLayers`, FEATURES B3/B4), the L0 web client (J1) and Hermes
(`svc.layer.list`/`meta` only).

This contract says how a client gets the **current state of one map layer** in one read.
Changes after that read are in [`layer-delta.md`](layer-delta.md). The fields of one entity
are defined in [`entity.md`](entity.md); this file uses only the "render" subset listed below.
Bus layout and permissions: [`../design/BUS-DESIGN.md`](../design/BUS-DESIGN.md).

## Sources of truth in omos

| Item | omos file | State |
|---|---|---|
| Subject `ent.{entity_type}.{entity_id}.state` | `CLAUDE.md` (grammar block), `docs/NATS-DOCTRINE.md` | exists (no producer today, omos-live-state.md §4) |
| Stream `ENT` (`ent.*.*.state`, 1 msg per subject) | `platform/nats/streams/ent.yaml` | **proposed** (BUS-DESIGN §4.1) |
| KV `world-state` (layer manifests and cells) | `platform/nats/streams/kv-world-state.yaml` | exists, never written; new purpose **proposed** (BUS-DESIGN D3) |
| KV `fleet-state` (site status) | `platform/nats/streams/kv-fleet-state.yaml` | exists, never written; new purpose **proposed** |
| Feed status `tel.feed.{source}.status` in `TEL` | `platform/nats/streams/tel.yaml` | subject published core-only today by `feed-intel`; `tel.feed.>` binding **proposed** |
| Object Store bucket `overlays` (static layers) | `platform/nats/streams/obj-overlays.yaml` | **proposed** |
| `svc.layer.*` endpoints | `services/layer/` (A5) | **proposed**; the service may be a module of the entity service |

Until these merge, the client lane runs the proposed services from their omos branches
against the local nats-server (decision 0004 #11).

## 1. What a layer is

A layer is a named filter over entities, plus a manifest. Every dynamic layer reads from
`ENT`. Static layers (large catalogs that rarely change) are an object in the `overlays`
bucket. The entity type names below follow the Lattice lane (lattice-patterns.md §A) and are
**provisional** until `entity.md` and `ONTOLOGY.md` fix the vocabulary.

| Layer id | Entity types (`ENT` filter) | Fed by (BUS-DESIGN §3.1) | Tier | Expected count (estimate) | Default `Nats-TTL` |
|---|---|---|---|---|---|
| `fleet` | `drone robot vehicle node edge` (`ent.drone.>` …) | `tel.fleet.>` | small | < 100 | 60 s |
| `tak` | `person place` and TAK shapes as `zone` | `tak.cot.out`, `tak.cot.mesh.rx` | small | < 500 | CoT `stale` |
| `zones` | `zone` | KV `geofences` | small | < 1,000 | none (`pinned`) |
| `hazards` | `hazard` | `intel.weather.*`, `intel.seismic.*`, `intel.fire.*`, `intel.disaster.*`, `intel.natural.*` | medium | < 5,000 | per source, up to 30 d |
| `tracks` | `track` | `det.>` | medium | < 2,000 | 30 s (unverified) |
| `space` | `satellite` | `intel.space.celestrak.tle` (propagated) | medium | 16 today, thousands later | 120 s, heartbeated |
| `transit` | `train` | MTA GTFS-RT (BUS-P6) | medium | < 2,000 | 120 s (unverified) |
| `rf` | `station` | APRS, Meshtastic (SOURCE-REGISTRY F017, M041) | medium | < 5,000 | 30 min (unverified) |
| `cameras` | owned `camera` | camera registry, KV `OVERWATCH_VIDEO` | small | < 100 | heartbeated |
| `air` | `aircraft` | `evt.world.air.{id}` | **large** | AOI-bounded: hundreds to tens of thousands | 120 s |
| `sea` | `vessel` | `evt.world.sea.{mmsi}` | **large** | up to 50,000 (FEATURES A14 fixture) | 30 min |
| `cameras-public` | static | `overlays/cameras-public/{version}.pmtiles` | **static** | 200,000 (FEATURES A16) | — |

Tier rules:
- **small / medium**: a client may load the whole layer and cull on the GPU.
- **large**: the client must pass an AOI; at wide zoom it gets density cells, not entities
  (FEATURES A14: no layer sends more than its cap).
- **static**: one object download, then a version check.

## 2. The layer manifest (KV `world-state`, key `{layer}.meta`)

Written only by the layer service, every 30 s and on change. The bucket TTL is 5 minutes
(`kv-world-state.yaml`), so **a missing manifest means the layer is down**, not empty.

```jsonc
{
  "v": 1,
  "layer": "air",
  "title": "Aircraft",
  "tier": "large",                       // small | medium | large | static
  "entity_types": ["aircraft"],
  "filter": "ent.aircraft.>",           // ENT subject filter (dynamic layers)
  "site": "hub",                        // which layer service wrote it (hub or site-<name>)
  "count": 1234,                        // live entities in the layer (whole layer, not AOI)
  "as_of": 1793275200000,               // epoch ms when this manifest was computed
  "ent_seq": 9876543,                   // ENT stream last sequence seen at as_of
  "cap": 5000,                          // max entities returned per AOI request
  "cells": { "zooms": [2, 4, 6], "below_zoom": 7 },   // large layers: cells used when zoom < below_zoom
  "object": null,                       // static layers: { "bucket": "overlays", "name": "cameras-public/2026-10-29.pmtiles", "sha256": "…", "bytes": 41234567 }
  "freshness": {
    "status": "fresh",                  // fresh | degraded | stale | down
    "limit_s": 60,                      // the layer's freshness limit (from the feed registry)
    "feeds": [
      { "source": "opensky", "fetchedAt": 1793275195000, "count": 1234,
        "lastError": null, "stale": false }
    ]
  },
  "default_stale_s": 30,
  "marking": "CLEAR",                   // TLP 2.0 label (security-and-trust.md §5)
  "style": "air.default.v1"
}
```

Field rules:
- `freshness.feeds[]` copies the latest `tel.feed.{source}.status` message of every feed of
  the layer (read by Direct Get on `TEL`). Fields `fetchedAt`, `count`, `lastError`, `stale`
  are the ingestion lane's status fields (ingestion-platform.md Rec. "Freshness").
- `status` is `fresh` when every feed is within `limit_s`, `degraded` when some are,
  `stale` when none are, `down` when the layer service cannot read `ENT`.
- The intelligence-gaps panel (FEATURES B20) reads `freshness` from every manifest.
- Size: a manifest is under 8 KiB.

Density cells: key `{layer}.cells.z{z}`, same writer, refreshed every 30 s.

```jsonc
{ "v": 1, "layer": "sea", "z": 4, "as_of": 1793275200000, "ent_seq": 9876543,
  "scheme": "webmercator-tile",          // cell = XYZ tile at zoom z
  "cells": [ [x, y, count, lat_centroid, lon_centroid] ] }
```

A cell record is at most 512 KiB (live `max_payload` is 1 MiB, nats-platform.md §1). If a
zoom level would exceed it, the layer service drops that zoom from `cells.zooms`.

Site status (KV `fleet-state`, key `{site}`, TTL 30 s): `{ "v":1, "site":"site-mac",
"connected": true, "last_contact": ms, "source_lag_s": { "SITE_TEL": 3, "SITE_ENT": 1 } }`.
A missing key means the site is disconnected.

## 3. How to read a snapshot

### 3.1 Required path: `svc.layer.snapshot` (all clients, including the `CLIENT` account)

Request (`svc.layer.snapshot`, JSON):

```jsonc
{
  "v": 1,
  "layer": "air",
  "aoi": { "bbox": [-74.3, 40.5, -73.6, 41.0] },   // or { "polygon": [[lon,lat],…] } or { "zone_id": "zone-home" } or null
  "zoom": 9,                                       // camera zoom (XYZ scale); decides entities vs cells
  "fields": "render",                              // render | full
  "max": 5000                                      // ≤ manifest cap
}
```

Reply: request-many, terminated by a chunk with `"last": true` (omos
`docs/NATS-DOCTRINE.md` §6 "known count" termination).

```jsonc
{ "v": 1, "layer": "air", "part": 0, "last": false,
  "mode": "entities",                 // entities | cells | object
  "ent_seq": 9876543,                 // the ENT sequence this snapshot is consistent with
  "as_of": 1793275200000,
  "truncated": false,                 // true if more than `max` matched; client should zoom in
  "items": [ /* render entities, or cells, or one object reference */ ] }
```

Rules:
- `aoi` is **required** for `large` layers; the service answers `{"error":"aoi_required"}`
  without it. For `small`/`medium` layers `aoi: null` returns the whole layer.
- If `zoom < cells.below_zoom` on a large layer, `mode` is `cells` and `items` is the matching
  cells inside the AOI.
- A chunk is at most 512 KiB and at most 2,000 entities.
- `fields: "full"` is allowed only for `small` layers or with `max ≤ 200`; it returns the
  whole entity from `entity.md`. Use `svc.entity.get` for one selected entity.
- Errors: `{"v":1,"error":"unknown_layer|aoi_required|too_many|layer_down|forbidden","detail":"…"}`.
- Releasability: the service drops entities whose `classification.releasable_to` does not
  include the caller's group (security-and-trust.md §5).

### 3.2 Optional path: direct `ENT` read (trusted clients in `ONEMIND` or at a site)

1. Read the manifest key `{layer}.meta` from `world-state`.
2. Create an **ordered** consumer on `ENT` with `filter_subject` = the manifest `filter`,
   `deliver_policy: last_per_subject`. The messages up to `NumPending == 0` are the
   snapshot; the same consumer then delivers deltas ([`layer-delta.md`](layer-delta.md)).
   Proved shape: nats-platform.md P10; lattice-patterns.md §A "Snapshot + live".
3. Filter by AOI on the client.

Use this path only for `small`/`medium` layers or on a LAN. Never create a durable consumer
per client (2.15 defaults to 1,000 consumers per stream; nats-platform.md R15).

### 3.3 One entity

Direct Get of the last message on `ent.{type}.{id}.state` (`$JS.API.DIRECT.GET.ENT`),
proved in P1/P10, or `svc.entity.get` from the `CLIENT` account.

### 3.4 Static layers

Download the object named in `manifest.object` from the `overlays` bucket (chunked,
SHA-256 checked, P3), or through `svc.layer.snapshot` with `mode: "object"`, which returns
the same reference. Check the digest before use.

## 4. Render entity (the subset in `fields: "render"`)

These names come from the doctrine §9.2 geo-envelope and the Lattice lane's entity payload
(lattice-patterns.md §A). `entity.md` is authoritative for their meaning.

| Field | Type | Meaning |
|---|---|---|
| `entity_id` | string | stable id, subject-safe `[A-Za-z0-9_-]{1,64}` |
| `entity_type` | string | second subject token |
| `lat`, `lon`, `hae` | number | WGS-84 degrees; metres above ellipsoid |
| `course`, `speed` | number | degrees true, m/s; `-1` if unknown |
| `callsign` | string | display name |
| `cot_type` | string | CoT type for symbology (B7) |
| `disposition` | string | `friendly assumed_friendly neutral unknown suspicious pending` |
| `t` | int | **source time** of the latest observation, epoch ms |
| `stale` | int | seconds after `t` when the entity is shown as stale |
| `expiry_t` | int | epoch ms when the entity disappears (matches `Nats-TTL`) |
| `seq` | int | entity-service revision, monotonic per entity |
| `live` | bool | `false` means remove |
| `origin` | string | owning site (`hub`, `site-mac` …), for conflict rules |
| `simulated` | bool | `indicators.simulated`; every asset in this run is `sim-*` |

Size: about 250–400 bytes per render entity as JSON (estimate from the field list). A
10,000-entity layer (the B4 target) is therefore about 2.5–4 MB, delivered in 5–8 chunks.

## 5. Freshness: what the client must show

| Level | Field | Rule |
|---|---|---|
| Entity | `t`, `stale`, `expiry_t` | dim when `now - t > stale`; remove at `expiry_t` or on a removal delta |
| Layer | manifest `freshness.status`, manifest present | badge "stale" unless `fresh`; "layer down" when the key is missing |
| Snapshot | reply `as_of`, `ent_seq` | if `now - as_of > 2 × freshness.limit_s`, request again |
| Site | `fleet-state` `{site}` | "site syncing" when any `source_lag_s` > 10; "disconnected" when the key is missing |

Clock skew: client freshness uses the client clock against `t`, which is the source clock.
Show skew only when the entity reports it (`entity.md`); do not correct `t`.

## 6. Consistency with deltas

The snapshot carries `ent_seq`. The client starts the delta watch with `from_seq = ent_seq + 1`
([`layer-delta.md`](layer-delta.md) §2), so no change between the snapshot and the first
delta is lost. On the direct path, the ordered consumer gives this for free.

## 7. Acceptance tests (Phase 2)

| Test | Pass when |
|---|---|
| A5 one read | A new client loads a 10,000-entity `fleet`-style fixture layer with one `svc.layer.snapshot` call in under 2 s on the LAN, and the item count equals the manifest `count` |
| AOI | A 50,000-vessel fixture returns ≤ 5,000 cells at zoom 3 and individual vessels inside a 50 km bbox at zoom 10 (FEATURES A14) |
| Layer down | Stopping the layer service makes `{layer}.meta` vanish within 5 minutes and the client shows "layer down" |
| Stale feed | Stopping a collector moves `freshness.status` off `fresh` within its `limit_s` (FEATURES A3, B20) |
| Releasability | An entity marked `AMBER+STRICT` for `team-a` is absent from a `CLIENT` snapshot for `team-b` |

## Unverified

- NACK support for the `ENT` TTL fields and for Object Store CRs (BUS-DESIGN §12).
- Streamed service responses across accounts (needed by `svc.layer.snapshot` from `CLIENT`).
- Per-type TTLs, counts and sizes above are estimates until the entity service runs on
  replayed `COT`/`INTEL` data.
