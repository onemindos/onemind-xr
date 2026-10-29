# Fusion and tracking: the OneMind entity service

Lane: fusion (Phase 1 / M0, research item 15). Date: 2026-10-29.
Scope: entity resolution across feeds, multi-target tracking (Kalman, IMM, JPDA, MHT),
track-to-track correlation, confidence and provenance, stale and conflicting data, the
algorithm and libraries for feature A4 (entity service), and how it consumes JetStream and
writes `ent.*.state` and KV.

Repos read at: omos `62e3d22b` (fresh clone, `/Users/danielcarku/Desktop/work/omos`),
onemind-xr working tree. Nothing was committed and nothing on the cluster was touched; this
lane used no cluster command and relies on `omos-live-state.md` for live state.

---

## Sources

### omos and onemind-xr (primary, in repo)

| ID | Source |
|---|---|
| R1 | `omos/CLAUDE.md`: locked grammar, `ent.{entity_type}.{entity_id}.state`, `det.{domain}.{type}.{id}`, streams applied by NACK, "a new stream is a new CR file" |
| R2 | `omos/docs/NATS-DOCTRINE.md` §3 (JetStream rules: idempotent consumer create, Direct Get, expected-sequence headers), §5 (`source_id` in payloads), §9.2 geo envelope (`uid,type,lat,lon,hae,course,speed,callsign,stale,t,meta`), §9.8 TAK wire decision |
| R3 | `omos/platform/nats/streams/*.yaml`: subject lists of all 39 CR files (38 applied by `platform-nats-streams`, plus `kv-feed-config.yaml`, which is excluded; `ls platform/nats/streams` and `omos/CLAUDE.md` line 101) [verifier 2026-10-29: was "38 CRs"]; `kv-world-state.yaml` (bucket `world-state`, R3, history 1, ttl 5m); `kv-fleet-state.yaml` (`fleet-state`, R3, history 3, ttl 30s); `cot.yaml` (`COT`, `tak.cot.>`, 720h); `tel.yaml`; `world.yaml`; `intel.yaml`; `geo.yaml` |
| R4 | `omos/platform/omos/tak-projector.yaml`: `projector.py`, a core-NATS consumer of `ent.*.*.state` (not the only one: see F1 correction), its `entity_to_cot()` field contract and `COT_TYPE_MAP` |
| R5 | `omos/services/nex/overwatch/om-overwatch-state/main.go` (554 lines, Go, `nats.go v1.37.0`): existing fusion prior art, not deployed |
| R6 | `omos/services/schemas/envelope/README.md` (envelope v1: `v,id,ts,type,source,subject,data,corr,causation,trace`) and `services/schemas/registry/entity.schema.json` (registry `kind` enum, `subjects.state`) |
| R7 | `onemind-xr/docs/research/omos-live-state.md` §3–§5: live feeds, rates, empty `world-state`/`fleet-state`, `ent=0`, no fusion pod running |
| R8 | `onemind-xr/docs/FEATURES.md` A4, H1; `docs/ARCHITECTURE.md` line 13; `docs/decisions/0004-owner-defaults-for-the-run.md` #7, #8; `docs/research/tak-protocols.md` line 42–44 (CoT `how`, `stale`, `ce`/`le`, 999999 unknown) |
| R9 | `omos/docs/references/ai/orp.md` (ORP, Apache-2.0, Rust: dedup + entity resolution reference) |

### Libraries (licence and activity checked with `gh api repos/<r>` and PyPI JSON, 2026-10-29)

| ID | Library | Licence | Latest | Evidence |
|---|---|---|---|---|
| L1 | Stone Soup (`dstl/Stone-Soup`, PyPI `stonesoup`) | MIT (Crown Copyright Dstl, DRDC, Univ. Liverpool) | 1.9.1, 2026-06-24; pushed 2026-10-28; Python >=3.10 | `LICENSE` file; PyPI JSON |
| L2 | FilterPy (`rlabbe/filterpy`) | MIT | 2.0.0, 2026-10-26 (first release in years; commits 2026-10-26/27) | PyPI JSON; `gh api .../commits` |
| L3 | Splink (`moj-analytical-services/splink`) | MIT | 5.0.0, 2026-09-28 | PyPI JSON |
| L4 | dedupe (`dedupeio/dedupe`) | MIT | last push 2025-07-29 | `gh api` |
| L5 | Zingg (`zinggAI/zingg`) | **AGPL-3.0** | — | `gh api` (excluded) |
| L6 | gonum (`gonum/gonum`) | BSD-3-Clause | pushed 2026-10-25 | `gh api` |
| L7 | h3-go / h3-py (`uber/h3-go`, `uber/h3-py`) | Apache-2.0 | h3-py 4.5.0, 2026-05-30 | `gh api`, PyPI |
| L8 | nats.go / nats.py | Apache-2.0 | nats-py 2.16.0, 2026-09-16 | `gh api`, PyPI |
| L9 | SciPy (`linear_sum_assignment`) | BSD-3-Clause | 1.18.1, 2026-08-21 | `gh api`, PyPI |
| L10 | konimarti/kalman (Go) | MIT | last push 2022-03-25 | `gh api` |
| L11 | rosshemsley/kalman (Go) | Apache-2.0 | last push 2019-06-15 | `gh api` |
| L12 | arthurkushman/go-hungarian (Go) | MIT | pushed 2026-09-21, 9 stars | `gh api` |
| L13 | golang/geo (S2) | Apache-2.0 | pushed 2026-10-26 | `gh api` |
| L14 | Anduril `lattice-sdk-python` / `-go` | **proprietary** "Anduril Lattice SDK License Agreement" (SPDX `NOASSERTION`) | — | `LICENSE` file via `gh api` (design reference only, no code) |

### NATS (primary)

| ID | Source |
|---|---|
| N1 | ADR-8 JetStream KV, `nats-io/nats-architecture-and-design/adr/ADR-8.md`: `Update(key, value, last)` "only succeeds when the most recent operation on a key has the expected revision"; `Watch` "the same Entry might be delivered more than once"; `History`; `LimitMarkerTTL` |
| N2 | ADR-43 per-message TTL (Status Implemented, tag 2.11): `Nats-TTL` header, `AllowMsgTTL` stream flag ("can be enabled on existing streams but not disabled"), `SubjectDeleteMarkerTTL` markers with `Nats-Marker-Reason: MaxAge/Remove/Purge`, publish with `Nats-TTL` to a stream without the flag "will be rejected" |
| N3 | https://docs.nats.io/nats-concepts/jetstream/key-value-store (per-key TTL, compare-and-swap, "a `watch` of a bucket opens a consumer") |
| N4 | Local `nats-server --version` → `v2.15.2`; `nats server check jetstream` → `OK JetStream` (command output, 2026-10-29) |

### Algorithms (DOIs resolved through `api.crossref.org/works/<doi>`, 2026-10-29)

| ID | Paper |
|---|---|
| P1 | Kalman, "A New Approach to Linear Filtering and Prediction Problems", J. Basic Eng., 1960, doi:10.1115/1.3662552 |
| P2 | Reid, "An algorithm for tracking multiple targets", IEEE TAC, 1979, doi:10.1109/TAC.1979.1102177 (MHT) |
| P3 | Fortmann, Bar-Shalom, Scheffe, "Sonar tracking of multiple targets using joint probabilistic data association", IEEE J. Oceanic Eng., 1983, doi:10.1109/JOE.1983.1145560 (JPDA) |
| P4 | Blom, Bar-Shalom, "The interacting multiple model algorithm for systems with Markovian switching coefficients", IEEE TAC, 1988, doi:10.1109/9.1299 (IMM) |
| P5 | Bar-Shalom, Campo, "The Effect of the Common Process Noise on the Two-Sensor Fused-Track Covariance", IEEE TAES, 1986, doi:10.1109/TAES.1986.310815 (track-to-track fusion with cross-covariance) |
| P6 | Julier, Uhlmann, "A non-divergent estimation algorithm in the presence of unknown correlations", ACC 1997, doi:10.1109/ACC.1997.609105 (covariance intersection) |
| P7 | Fellegi, Sunter, "A Theory for Record Linkage", JASA, 1969, doi:10.1080/01621459.1969.10501049 (probabilistic entity resolution; basis of Splink) |
| P8 | Blackman, "Multiple hypothesis tracking for multiple target tracking", IEEE AESM, 2004, doi:10.1109/MAES.2004.1263228 |

### Identity keys and the Lattice entity model

| ID | Source |
|---|---|
| W1 | https://skybrary.aero/articles/24-bit-aircraft-address and ICAO WACAF guidelines (https://www2023.icao.int/WACAF/Documents/Meetings/2023/Workshop-24-Bits/Guidelines%20for%20ICAO%2024%20Bits%20Adresses%20Assignment.pdf): 24-bit address "assigned by the ICAO member states as stipulated in Annex 10, Volume III, Part I" from each State's block; "any aircraft can be assigned only one address"; on change of State of registry "the new state assigns an address from its own address block" and the old one is "released for re-use" (Skybrary re-read by verifier 2026-10-29; the ICAO WACAF PDF was not re-read) |
| W2 | Global Fishing Watch (https://globalfishingwatch.org/?p=387): spoofing (several vessels on one MMSI) is "less than a quarter of one percent of the AIS signals we receive"; USCG NAVCEN slide (https://www.navcen.uscg.gov/sites/default/files/pdf/AIS/FAQ/16/Winkler%40GMDSS_TF_(2012-01-11)_AIS_Data_Quality.pdf): about 150 duplicated MMSIs in 3 years, largest cause a default value (**unverified**: the verifier's fetch of this PDF returned HTTP 403 on 2026-10-29); arXiv 2511.23016 (Baltic study) drops duplicate-MMSI vessels |
| W3 | https://developer.anduril.com/guides/entities/overview: required `entity_id`, `is_live`, `expiry_time`, `provenance.integration_name`; `provenance.source_update_time` drives updates ("Lattice relies on changes to `provenance.source_update_time` to trigger an update"; it "does _not_ compare the full proto definitions" [verifier 2026-10-29: corrected quote, was "does not compare full entity contents"]); `expiry_time` < 30 days, `noExpiry` option; `is_live=false` deletes; `aliases.name`; components, partial entities expected |
| W4 | Stone Soup source tree (`gh api repos/dstl/Stone-Soup/git/trees/main?recursive=1` plus raw files): class lists quoted under Verified facts |
| W5 | FilterPy `filterpy/kalman/IMM.py` (`class IMMEstimator`, cites Bar-Shalom, Li, Kirubarajan) |

---

## Verified facts

### F1. The slot exists, the producer does not

- The grammar has `ent.{entity_type}.{entity_id}.state` for fused entity state and
  `det.{domain}.{type}.{id}` for detections (R1).
- **No stream captures `ent.>` or `det.>`.** The union of every `subjects:` entry in
  `platform/nats/streams/*.yaml` has no `ent.` or `det.` prefix (R3, grep of all CRs). So
  `ent.*.state` today is core NATS only: published state is lost unless it is also written
  to KV.
- `world-state` (R3, history 1, ttl 5m) and `fleet-state` (history 3, ttl 30s) have never
  had a write (`last_seq 0`) and no code references them (R7 §5).
- ~~The only consumer of `ent.*.*.state` is `omos-tak-projector`~~ **Corrected [verifier
  2026-10-29]:** two omos services subscribe to `ent.*.*.state` on **core** NATS:
  `omos-tak-projector` (`tak-projector.yaml` line 416) and `sink-clickhouse`
  (`platform/omos/intel-scripts.yaml` lines 260–262, `nc.subscribe("ent.*.*.state", ...)`;
  also noted in `platform/omos/scaledobject-sink-clickhouse.yaml` line 9). Both run live
  (R7 §3 table). The projector log shows `ent=0` (R4, R7 §3). Any `ent` payload change
  must therefore also suit the ClickHouse sink.
- Live data volume is tiny: MTA GTFS-RT as CoT ≈ 3.1 msg/s and 16 satellites every 30 s
  (0.53 msg/s), both on `tak.cot.inject`, retained 720h in `COT`. Aircraft (OpenSky) and
  ships (aisstream.io) are off since 2026-08-27 (R7 §3–§4).
- Decision 0004 #7: `tak.cot.*` is the raw TAK wire; the entity service derives per-UID
  state (R8).

### F2. The projector already defines a de facto `ent` payload

`entity_to_cot()` in R4 reads: top-level `lat`/`latitude`, `lon`/`longitude`, `entity_id`,
`entity_type`, `confidence`, and `properties.{name|callsign|flight|mmsi|icao_hex|tail,
altitude|hae|alt, speed|sog|velocity, course|heading|cog|track, ce, le}`. It maps
`entity_type` through `COT_TYPE_MAP` (keys such as `aircraft`, `drone`, `ship`, `vessel`,
`vehicle`, `robot`, `human`, `sensor`, `camera`, `satellite`, `default`). It emits CoT with
`uid = "OM.{entity_type}.{entity_id}"`, `how="m-g"`, a `_flow-tags_` attribute
`omos-tak-projector`, and publishes to **`tak.cot.inject`**, which is inside `COT`
(`tak.cot.>`).

### F3. Prior art in omos: `om-overwatch-state` (R5)

Design choices worth keeping:
- Single KV writer ("the spine stays the single KV writer"); manual entities go through it.
- Human authority: CoT or operator affiliation sets `AffiliationLocked`; machine sensors
  never overwrite it, and proximity association refuses to merge into a locked track of a
  different affiliation ("better to show an extra blip than to hide a hostile").
- Classification never downgrades to "unknown" because a coarser sensor reported.
- Independent new source raises confidence by probabilistic OR `1-(1-c1)(1-c2)`.
- A proximity merge corroborates but does not move the primary track's position.
- Status sweep: no update for 30 s → `lost`; pinned manual entities never go stale.

Defects that the new service must not copy:
- Core `nc.Subscribe` on all inputs: any restart or slow consumer silently loses data; no
  ack, no replay.
- Subjects outside the grammar: `det.cot.{id}` (3 tokens; grammar is 4), `ent.manual.upsert`,
  `ent.manual.delete`, `ent.air.{id}.deleted` (grammar allows only `ent.*.*.state`).
- Association is a fixed 500 m haversine radius with no velocity, time or covariance, and
  it scans every KV key (`kv.Keys()` + `Get` each) per detection: O(n) round trips.
- Uses receive time (`time.Now()`), not source time; no out-of-order handling.
- `kv.Put` without revision check, so a second instance would race.
- ~~Creates its own bucket at runtime (`CreateKeyValue`), against R1.~~ **Corrected
  [verifier 2026-10-29]:** it first opens `OVERWATCH_STATE` with `js.KeyValue(kvBucket)` and
  calls `CreateKeyValue` only if that fails (`main.go` lines 77–88). The bucket is declared
  by a CR, `platform/nats/streams/kv-overwatch-state.yaml` (R3, history 5). The defect is
  narrower: the fallback can still create a bucket outside the CRs (with no replica count,
  so R1). The new service should fail instead of creating buckets.
- Not deployed; bucket empty (R7 §5).

### F4. Algorithms (primary papers P1–P8)

- **Kalman filter** (P1): optimal linear-Gaussian recursive estimator; gives a state and a
  covariance, which is what gating, confidence and `ce`/`le` need.
- **IMM** (P4): runs several motion models (for example constant velocity and coordinated
  turn) and mixes them by Markov switching probabilities; the standard answer for
  manoeuvring targets. FilterPy ships `IMMEstimator` (W5). **Stone Soup has no IMM class**:
  a recursive tree grep for `imm` under `stonesoup/` finds only `tests/test_immutable_class.py` (W4).
- **JPDA** (P3): soft association; each track is updated with a probability-weighted mix of
  all gated measurements. Fits clutter (many false alarms near few targets, such as camera
  detections). Cost grows with the number of joint events in a cluster.
- **MHT** (P2, P8): defers hard decisions by keeping a tree of association hypotheses over
  several scans; best accuracy in dense, ambiguous scenes, highest implementation and
  compute cost, needs pruning (P8 is a survey of the practical techniques).
- **GNN** (global nearest neighbour): one hard assignment per scan that minimises total
  gated cost, solved with the Hungarian/Munkres algorithm (`scipy.optimize.linear_sum_assignment`, L9).
- **Track-to-track fusion**: when two sources each deliver *tracks* (for example a TAK EUD
  self-report and a MAVLink drone's own telemetry, or two ADS-B aggregators), their errors
  are correlated through common process noise; P5 gives the fused covariance when the
  cross-covariance is known. When it is unknown, **covariance intersection** (P6) is
  consistent for any correlation. Stone Soup implements it as `ChernoffUpdater` (its
  docstring: "equal to the Covariance Intersection Algorithm from Julier et al") (W4).
- **Entity resolution** of records without kinematics (registries, intel events, names) is
  record linkage (P7, Fellegi–Sunter); Splink implements it (L3, MIT). It is batch,
  not streaming (Splink runs on DuckDB/Spark backends; **unverified** for streaming use).

### F5. Stone Soup coverage (W4: class names read from `main` source files)

- `predictor/kalman.py`: `KalmanPredictor`, `ExtendedKalmanPredictor`,
  `UnscentedKalmanPredictor`, `SqrtKalmanPredictor`, `CubatureKalmanPredictor`, …
- `updater/kalman.py`: `KalmanUpdater`, `ExtendedKalmanUpdater`, `UnscentedKalmanUpdater`, …
- `dataassociator/neighbour.py`: `NearestNeighbour`, `GlobalNearestNeighbour`, `GNNWith2DAssignment`
- `dataassociator/probability.py`: `PDA`, `JPDA`, `JPDAwithLBP`, `JPDAwithEHM`, `JPDAwithEHM2`
- `dataassociator/mfa/` + `hypothesiser/mfa.py`: multi-frame assignment (an MHT-family method); `tracker/pmht.py` (PMHT)
- `dataassociator/tree.py`: KD-tree and TPR-tree gated GNN/JPDA (spatial indexing)
- `dataassociator/tracktotrack.py`: `TrackToTrackCounting`, `OneToOneTrackAssociator`, …
- `updater/chernoff.py`: `ChernoffUpdater` (covariance intersection)
- `deleter/`: `time.py`, `error.py` (track deletion by age or covariance growth)
- `metricgenerator/ospametric.py`: `GOSPAMetric`, `OSPAMetric`; plus `clearmotmetrics.py`
- Examples: `docs/examples/trackfusion/Track2Track_Fusion_Example.py`, `Track_Stitcher_Example.py`

### F6. Go ecosystem

- No Go library implementing JPDA or MHT was found (one web search; the results were
  MATLAB, Python and Rust projects). **Absence is not proven** (see Unverified).
- Go Kalman packages exist but are small and stale: `konimarti/kalman` (MIT, last push
  2022), `rosshemsley/kalman` (Apache-2.0, last push 2019) (L10, L11).
- `gonum` (BSD-3) supplies matrices and linear algebra (L6). Its tree has no file named
  hungarian/assign/munkres/lsap (path grep), so an assignment solver must come from
  elsewhere: `arthurkushman/go-hungarian` (MIT, 9 stars) or a few hundred lines in-house.
- `h3-go` (Apache-2.0) and `golang/geo` S2 (Apache-2.0) give spatial cells for gating (L7, L13).
- `nats.go` (Apache-2.0) is the reference JetStream client (L8); `om-overwatch-state`
  already uses it (R5).

### F7. NATS mechanics the design relies on

- KV `Update(key, value, lastRevision)` is compare-and-set (N1); KV is a stream, so a
  watcher gets a snapshot then live changes, possibly with duplicates (N1, N3).
- Per-message TTL (`Nats-TTL`) needs `AllowMsgTTL` on the stream; without it the publish
  is rejected. Expiry markers need `SubjectDeleteMarkerTTL`/`LimitMarkerTTL` (N1, N2).
  The omos `kv-world-state.yaml` CR sets neither (R3).
- JetStream rules in R2 §3: create consumers idempotently, ≤300 filters per consumer, use
  expected-sequence headers for optimistic concurrency, Direct Get for latest-value reads.
- The local server is nats-server v2.15.2 with JetStream (N4), above the 2.11 floor of N2.

### F8. Identity keys per feed

- ADS-B: the ICAO 24-bit address is unique per aircraft at any time and changes on
  re-registration (W1). It is a strong key, with a known break on registry change.
- AIS: MMSI is the de facto key, but duplicates and spoofing exist: under 0.25% of signals
  in Global Fishing Watch's data, about 150 duplicated MMSIs in 3 years per USCG, mostly
  default values (W2). It needs a plausibility check (impossible jumps → split).
- TAK: CoT `uid` is the map identity; "same uid updating position = moving dot" (R2 §9.2).
  CoT carries `how`, `stale`, and `ce`/`le` errors, with 999999 for unknown (R8 tak-protocols.md).
- Lattice requires every entity to carry `provenance` and `expiry_time` and updates on
  `provenance.source_update_time` (W3). These are the right fields to copy (design only, L14).

---

## How it applies to OneMind

### Recommended algorithm: identity first, kinematics second

Most OneMind sources are **cooperative and self-identifying** (ADS-B, AIS, TAK PLI,
MAVLink, GTFS-RT vehicles, satellites by NORAD id). There, entity resolution is a key
lookup and tracking is single-target smoothing; the hard multi-target problem only appears
for **keyless detections** (`det.*`: camera, acoustic, RF) and for **cross-source
correlation** of the same object reported under different ids. The pipeline is staged so
each stage only pays for the problem it has:

**Stage 0: normalise.** One adapter per input subject turns a message into an
`Observation`: `source` (feed id), `source_id`, `keys` (for example `{icao:"a1b2c3"}`),
`t_source` (from the payload: CoT `time`, AIS/ADS-B timestamp, R2 §9.2 `t`), `t_received`,
position (WGS-84) with covariance (from CoT `ce`/`le`, AIS position accuracy flag, or a
per-source default), velocity, class, `stale` seconds, and a provenance pointer
`{stream, seq}` from the JetStream metadata. Invalid observations go to a dead-letter
subject with the reason (run reliability list).

**Stage 1: deterministic identity.** Map hard keys to a canonical `entity_id` through an
alias table: `icao-{hex}`, `mmsi-{n}`, `norad-{n}`, `tak-{sanitised uid}`,
`gtfs-{agency}-{vehicle}`, `sim-*` for MAVLink assets. Tokens must not contain `.`, `*`,
`>` or whitespace (NATS tokens); a uid that does needs sanitising or hashing, and the
original is kept as an alias. Plausibility gate: if the implied speed between consecutive
fixes for one key exceeds the class maximum (AIS duplicate or spoof, F8), **split** into a
second entity (`mmsi-{n}~2`) and flag `conflict: duplicate_key`; never average two ships.

**Stage 2: per-entity estimator.** A constant-velocity Kalman filter in a local
East-North-Up plane around the last fix (P1), with altitude filtered separately, and
process noise by class (ship ≪ aircraft ≪ small drone). This smooths positions, gives the
covariance for `ce`/`le` and for gating, and lets the service **predict** position at
render time between sparse updates. Upgrade to **IMM (CV + coordinated turn)** for `air`
and `drone` classes in a later step (P4), using FilterPy's `IMMEstimator` as the reference
(F4). Out-of-order data: per source, drop an observation older than that source's last
accepted `t_source` (count it as a metric); a short reorder buffer (for example 2 s) is
optional.

**Stage 3: keyless detections (`det.*`).** Global nearest neighbour: chi-square gating on
the Mahalanobis distance between the detection and each predicted track (candidates from an
H3 cell lookup, not a full scan), then one Hungarian assignment per scan (F4). Unassigned
detections start *tentative* tracks, confirmed by M-of-N hits (for example 3 of 5) before
they are published as entities; tracks with no hits for their class timeout go stale, then
lost. **JPDA** is the upgrade for a dense-clutter source (a camera producing many false
detections) and **MHT is not recommended** for OneMind: civilian scenes are sparse,
targets mostly self-identify, and MHT's cost is not justified (P2, P8). If an evaluation
shows GNN failing on a recorded scene, run Stone Soup's `JPDA` or MFA offline on the same
fixture first (F5).

**Stage 4: cross-source correlation (track-to-track).** Different ids for one object (a TAK
EUD and the drone it controls are *different* objects; but a MAVLink `sim-x1` and a CoT
`uid sim-x1` from the mesh gateway, or an ADS-B track from two aggregators, are the same).
Periodically compare tracks of compatible classes: gate on the Mahalanobis distance of the
state difference over a window of scans; when they stay consistent for N scans, record an
**alias candidate**. Auto-merge only within a cooperative-key family or when the ids share
a declared link (for example the asset registry says `tak uid X` is `sim-x1`); otherwise emit
a proposal for a human to confirm (the Act loop, with policy and audit). Fuse merged
kinematics with **covariance intersection** (P6), because the cross-covariance between
independent trackers is unknown (P5 needs it). Keep the overwatch rule (F3): a coarser
source corroborates identity and confidence but does not move a precise track.

**Confidence.** One number in [0,1] that consumers can threshold, computed from three
named parts kept in the payload for explainability: `source_prior` (per-feed reliability
from the feed registry; human report and cooperative self-report high, a single camera
detection low), `track_quality` (confirmation state, hit ratio, covariance size), and
`corroboration` (probabilistic OR over independent sources, as in F3, capped below 1). It
decays with age since the last update relative to `stale`.

**Provenance (Lattice-style, W3).** Each entity carries `sources[]`:
`{source, source_id, first_seen, last_seen, t_source, stream, seq, how}`, plus `aliases[]`,
`provenance.source_update_time` (max `t_source` across contributing sources) and
`expiry_time`. The `{stream, seq}` pointers are the "history pointer" A4 asks for: the raw
observations stay in `COT`, `TEL`, `WORLD`, `INTEL`, so any entity can be audited or rebuilt
by replay.

**Stale and expiry.** Lifecycle `tentative → active → stale → lost → expired`.
`stale` = no update within the source's `stale` (CoT `stale`, R2 §9.2 `stale`, or a class
default); clients dim it (R2 §9.2 rule). `lost` = 3 × stale; `expired` = the entity is
deleted from KV (the bucket TTL also does this). Mirror Lattice: `is_live=false` on the last
publish before deletion, and `expiry_time` always set; pinned operator entities have no
expiry (`noExpiry` analogue, as `Pinned` in F3).

**Conflicting data.** Resolve per attribute, not per message:
1. **Human assertion wins** (operator or TAK-declared affiliation and identity) and locks
   the attribute (F3).
2. Then **cooperative self-report** (an asset's own ADS-B/AIS/MAVLink/PLI) for its own
   position and identity.
3. Then **sensor estimates**, fused by the filter.
4. Within one tier, the newer `t_source` wins; classification never downgrades to unknown.
5. Every override is recorded in `conflicts[]` (`attribute`, `kept`, `rejected`, `sources`)
   so the UI and Hermes can show disagreement instead of hiding it.
Clock skew: reject `t_source` more than a tolerance in the future (for example 5 s) and
fall back to `t_received`, counting the event.

### Language and libraries

| Use | Choice | Licence | Why |
|---|---|---|---|
| Service runtime | **Go** + `nats.go` `jetstream` package | Apache-2.0 | One static binary for cloud, home server and Mac (edge lane); matches `om-overwatch-state` (R5) |
| Linear algebra | `gonum/mat` | BSD-3 | Kalman matrices, Mahalanobis |
| Kalman CV / IMM, GNN, CI | **written in-house** (~500–800 lines, **estimate**) | GPL-compatible, owned | No maintained Go MTT library (F6); the maths is standard (P1, P4, P6) |
| Assignment | in-house Hungarian, or `arthurkushman/go-hungarian` after review | MIT | gonum has none (F6) |
| Spatial gating | `uber/h3-go` | Apache-2.0 | Candidate lookup by cell + neighbours, replacing O(n) scans (F3) |
| Reference oracle and metrics | **Stone Soup** (Python, offline and in CI only) | MIT | Kalman/GNN/JPDA/MFA reference outputs on the same fixtures; `OSPAMetric`/`GOSPAMetric` to score the Go tracker (F5) |
| IMM reference | FilterPy 2.0.0 | MIT | `IMMEstimator` (F4); FilterPy 2.0 is days old, so pin the version |
| Batch entity resolution (registries, intel dedupe) | Splink 5 | MIT | Fellegi–Sunter (P7); offline jobs only, not the hot path |
| Excluded | Zingg (AGPL-3.0), Anduril Lattice SDK (proprietary), worldmonitor, worldwideview | — | Licence rules; Lattice is a design reference only |

Python with Stone Soup in the hot path was considered: it has everything except IMM, but it
is a research framework built around simulation (feeders, ground truth), adds NumPy/SciPy
to every edge kit, and its per-detection object overhead is **unverified** at the
10,000-entity target. At today's 3.6 msg/s (F1), either language would do; Go is
chosen for edge deployment and parity with existing omos Go services.

### JetStream sketch

```
inputs (durable pull consumers, explicit ack, MaxAckPending bounded)
  COT    filter tak.cot.>          CoT XML / TAK proto  → adapter cot      (drop echoes: uid "OM.*" or _flow-tags_ omos-tak-projector)
  TEL    filter tel.drone.>, tel.fleet.>, tel.robot.>   → adapter mavlink/fleet
  WORLD  filter evt.world.>        raw AIS/ADS-B (once feeds publish raw, feature A1)
  INTEL  filter intel.>            point events → event entities (no kinematics; ER by source id)
  DET    filter det.>              **needs a new stream CR** (det.> is in the grammar but no stream captures it)

entity-service (single active replica; leader lease = KV Create("lock.entity-service") with TTL refresh)
  normalise → identity (alias map) → KF/IMM update or GNN assign → T2T correlate → resolve conflicts
  in-memory: tracks, H3 index, alias map (rebuilt on start from KV watch, then consumers resume)

outputs
  KV world-state  key "{entity_type}.{entity_id}"   Update(key, json, lastRev)  (CAS; single writer)
  KV fleet-state  key "{entity_type}.{entity_id}"   own assets (sim-*, robots), ttl 30s
  core publish    ent.{entity_type}.{entity_id}.state  (same JSON; what tak-projector subscribes to)
  ack input message only after the KV write succeeded
```

- **Rate control.** Publish `ent.*.state` on change beyond a threshold (position moved more
  than its `ce`, class changed, status changed) or at a heartbeat at least every
  `min(stale/2, 2 min)`. The heartbeat also keeps slow entities alive in `world-state`,
  whose bucket TTL is 5 minutes (F1).
- **Idempotence.** Replays are harmless because the update is a function of the
  observation and the per-source last `t_source` rule drops duplicates; the KV CAS stops two
  writers. The source streams already use a 2 minute `duplicateWindow` for `Nats-Msg-Id` (R3).
- **Rebuild from scratch.** Delete the consumers' state, replay `COT`/`TEL`/`WORLD` from a
  start time, and the KV converges (run reliability list "Replayable").
- **Partitioning later.** Split by `entity_type` with separate consumers and lock keys when
  one replica is not enough (R2 §3: stay far under consumer and filter limits).
- **Payload.** Top-level fields the projector already reads (F2): `entity_id`,
  `entity_type`, `lat`, `lon`, `confidence`, `properties{callsign, altitude, speed,
  course, ce, le}`, plus `status`, `is_live`, `expiry_time`, `sources[]`, `aliases[]`,
  `conflicts[]`, `confidence_parts{}`, `t` (epoch ms, R2 §9.2) and `v`. `entity_type` values
  should use the projector's `COT_TYPE_MAP` keys (`aircraft`, `ship`, `drone`, `robot`,
  `vehicle`, `satellite`, …) so it needs no change. `sink-clickhouse` also subscribes to
  `ent.*.*.state` (F1 correction), so its parser must be checked against this payload too
  [verifier 2026-10-29]. Whether `ent` payloads also wear the
  envelope v1 (R6) is for `docs/contracts/entity.md` (see Risks).

### Owner proposals this lane needs (decision 0004 #8; no new subject pattern)

1. A stream CR for `det.>` (a pattern already in the grammar) so detections are durable.
2. Optional `ENT` stream on `ent.*.*.state` with `MaxMsgsPerSubject` (for example 100) for
   short trails and late joiners, or rely on KV only. Decide in BUS-DESIGN.
3. `AllowMsgTTL` and `SubjectDeleteMarkerTTL`/`LimitMarkerTTL` on `world-state` so
   watchers see expiry markers and long-lived entities can carry `Nats-TTL: never` (N2;
   NACK support for these fields is **unverified**).
4. Lifecycle events (`created`, `merged`, `split`, `lost`) need an `evt.{domain}.…` domain
   that a stream captures; `EVT` captures only listed domains (R3). Instead of inventing
   `ent.*.*.deleted` (the F3 defect), use KV delete markers until the owner picks a domain.

---

## Risks

1. **Feedback loop and duplicate dots in TAK.** The projector publishes entity CoT to
   `tak.cot.inject` (F2), which is inside `COT`, which the entity service reads. Without the
   echo filter (uid prefix `OM.` or the `_flow-tags_` marker) every entity re-ingests itself.
   Also, feed-mta and feed-satellites already put CoT straight on TAK (F1); once the entity
   service turns them into entities, the projector would add a second `OM.*` dot for the same
   train or satellite. Fix: the projector skips entities whose only source is CoT already on
   the TAK wire, or the feeds stop publishing CoT directly (feature A1).
2. **Wrong merges are worse than duplicates.** Auto-merging two real objects hides one
   (the F3 safety rule). Keep auto-merge to shared hard keys and declared links; everything
   else is a human-confirmed proposal.
3. **Key failures.** MMSI duplicates and spoofing (W2), ICAO address change on
   re-registration (W1), TAK uids reused by misconfigured devices (**unverified** frequency).
   Mitigated by the plausibility split and aliases.
4. **Envelope mismatch.** The projector reads top-level fields; envelope v1 puts the payload
   in `data` (R6). Wrapping `ent` messages in the envelope without changing the projector
   breaks TAK output. Decide once in `contracts/entity.md`.
5. **`world-state` TTL 5m, history 1.** Entities that update less often than 5 minutes
   vanish unless heartbeated; history 1 means KV holds no trail. The service heartbeat and
   the source-stream pointers cover both.
6. **Core-only `ent.*.state`.** A consumer that is down misses updates; it must start from a
   KV snapshot (watch) and then follow core updates, or use the optional `ENT` stream.
7. **Covariance defaults.** Many feeds give no accuracy; per-source defaults drive gating.
   Bad defaults cause missed or false associations. Tune on recorded fixtures with OSPA/GOSPA.
8. **Single replica.** A single active writer is a short outage on failover (lease TTL). This
   is acceptable for the reliability list ("exactly one owner"), but the lease TTL bounds it.
9. **FilterPy 2.0.0 is brand new** (released 2026-10-26 after years without a release, L2):
   pin it and treat it as a reference, not a dependency of the service.
10. **Scope creep to MHT/JPDA** before any keyless sensor exists. The live bus has no
    `det.*` traffic today (F1), so Stage 3 can ship with the simulator first.

---

## Recommendation

Build the A4 entity service in **Go** (`nats.go` jetstream, `gonum`, `h3-go`) as one active
replica that runs the staged pipeline: **deterministic key identity with an alias map and
plausibility split → per-entity constant-velocity Kalman filter (IMM for air and drone
later) → GNN with chi-square gating and Hungarian assignment for keyless `det.*` detections,
with M-of-N confirmation → track-to-track correlation with covariance intersection, auto-merge
only on shared keys or declared links and human-confirmed otherwise**. Resolve conflicts per
attribute: human assertion, then self-report, then sensor; newest source time within a tier.
Carry Lattice-style provenance (`sources[]` with stream and sequence pointers, `aliases[]`,
`expiry_time`, `is_live`), a decomposed confidence, and a `tentative/active/stale/lost/expired`
lifecycle. Consume `COT`, `TEL`, `WORLD`, `INTEL` (and `DET` once its stream exists) with
durable pull consumers and ack after a CAS `Update` to `world-state`/`fleet-state`; publish
`ent.{entity_type}.{entity_id}.state` on change and on heartbeat, using the payload fields the
projector already reads. Filter projector echoes. Use **Stone Soup (MIT) offline** as the
reference implementation and OSPA/GOSPA scorer in CI, and FilterPy 2.0.0 (pinned) as the IMM
reference. Do not adopt MHT; use JPDA only if a recorded clutter scene shows GNN failing.
Do not copy `om-overwatch-state`'s subjects, core subscriptions or full-bucket scans; keep its
human-lock and no-downgrade rules.

First slice (enough for A4's acceptance test, "same aircraft from two feeds gives one
entity; a ship's position updates in place"): Stage 0–2 for CoT (MTA, satellites, `sim-x1`
via the mesh gateway) and MAVLink `tel.drone.>`, KV + `ent` output, echo filter, replay
test, against the local nats-server.

---

## Unverified items

- That no maintained Go JPDA/MHT library exists (one web search found none; a GitHub
  language-filtered search was not run).
- Whether gonum has an assignment solver under another name (only file paths were grepped).
- Splink's suitability for streaming or incremental linkage (only batch use is claimed).
- Stone Soup's per-detection throughput at 10,000 entities; no benchmark was run.
- Line-count estimate (500–800 lines) for the in-house Go Kalman/GNN/CI core.
- Whether the NACK version deployed in omos supports `AllowMsgTTL`, `SubjectDeleteMarkerTTL`
  or `LimitMarkerTTL` on Stream and KeyValue CRs.
- How often TAK uids collide across devices in practice.
- Exact Lattice override semantics: `https://developer.anduril.com/guides/entities/override`
  returned HTTP 404; only the overview page (W3) was read.
- Default per-class process noise, gate thresholds, M-of-N and stale values: these are
  starting points to tune on fixtures, not measured values.
- Live cluster state is taken from `omos-live-state.md` (R7), not re-checked by this lane.
- USCG NAVCEN figure (about 150 duplicated MMSIs in 3 years, mostly default values; W2): the
  PDF returned HTTP 403 to the verifier, so it is not independently confirmed.
- How `sink-clickhouse` handles `ent.*.*.state` payloads (it subscribes; its parser was not
  read against the proposed payload).

### Verifier log (independent check, 2026-10-29)

Re-checked against primary sources: omos `62e3d22b` files (stream CRs, `tak-projector.yaml`,
`intel-scripts.yaml`, `om-overwatch-state/main.go` and `go.mod`, `NATS-DOCTRINE.md`), GitHub
API (Stone Soup tree and class definitions, `chernoff.py` docstring, FilterPy `IMM.py`, repo
licences), PyPI JSON (versions and dates), NATS ADR-8 and ADR-43 text, Crossref for all eight
DOIs, the Lattice entities overview, Global Fishing Watch and Skybrary pages, and local
`nats-server --version`. Corrected: the claim that the projector is the only `ent.*.*.state`
consumer (F1, R4); the `CreateKeyValue` defect in F3; the Lattice quote (W3); the Skybrary
wording (W1); the CR count (R3). Marked unverified: the USCG figure (W2). No licence-rule
violation found (no worldmonitor, worldwideview, Zingg or Lattice SDK code is copied; Lattice is
a design reference only). No cluster-safety violation: the lane ran no cluster command.
