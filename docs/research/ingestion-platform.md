# Ingestion platform: choice, proof and migration plan (M0, ingestion lane part c)

Date: 2026-10-29. Lane: run prompt §10c (`ULTRACODE-PROMPT.md` lines 242-281).
Decision this feeds: 0004 #18 ("Pipeline technology").
Evidence: `docs/evidence/M0/ingestion/` (proof code in `proof/`, run outputs in `results/`).

**Verdict:** make **plain Go collectors on `nats.go`** the standard, as one framework
(`omos-ingest`) inside omos `services/bridge`. Retire Redpanda Connect feed by feed. Bento
(MIT) came a close second and is the fallback if the framework slips. Both were built and
chaos-tested here against eleven sources (one per delivery mode) under load. The final Go
run had 0 duplicates and 0 losses on every polled, long-poll, SSE and WebSocket source
through a NATS kill, an upstream outage, a collector kill and malformed data. Bento
cannot do that by design, because its state is in memory and its SSE input cannot resume.
The Go result needed three fixes to the framework core that the chaos runs found (lease,
checkpoint read, webhook hold); all three are now covered by tests.

## Sources

Primary sources read in this lane (commands run 2026-10-29 unless noted):

| # | Source | What it proves |
|---|---|---|
| S1 | `gh api repos/<r>` for redpanda-data/connect, warpstreamlabs/bento, wombatwisdom/wombat, apache/nifi, vectordotdev/vector, airbytehq/airbyte, meltano/meltano, temporalio/temporal, synadia-io/nex, nats-io/nats.go, redpanda-data/benthos | SPDX licence, last push, latest release |
| S2 | redpanda-data/connect @ `3080220410db` (shallow clone): `licenses/README.md`, `licenses/rcl_header.go.txt`, `internal/plugins/info.csv`, `public/bundle/free/package.go`, `public/components/community/package.go`, `cmd/redpanda-connect{,-community}/main.go`, `internal/license/shared_service.go`, `internal/impl/nats/output_jetstream.go`, `go.mod` | Which connectors are Apache-2.0 vs RCL; how enterprise is enforced; how the JetStream output publishes |
| S3 | https://docs.redpanda.com/redpanda-connect/get-started/licensing/ | Enterprise features need a licence key; trial key valid 30 days; "All enterprise connectors are blocked" without one |
| S4 | warpstreamlabs/bento @ `8f70f97f4c5e` (shallow clone): `LICENSE`, `internal/impl/nats/output_jetstream.go`, `internal/impl/otlp/tracer_otlp.go`, `internal/impl/sql/buffer_sqlite.go`, `website/docs/components/processors/{http,dedupe}.md`, `website/docs/components/inputs/{http_server,http_client,websocket,mqtt}.md`, `website/docs/guides/sync_responses.md` | Bento licence and features; documented dedupe delivery caveat |
| S5 | `gh api repos/vectordotdev/vector/contents/src/sinks/nats/config.rs`, `src/sources/http_client/client.rs`, `src/sources`, `src/transforms` | Vector JetStream sink with `Nats-Msg-Id`; http_client auth types; available sources and transforms |
| S6 | `gh api repos/apache/nifi/git/trees/main?recursive=1` (path grep) | NiFi has HTTP, WebSocket, MQTT and file processors but no NATS processor in the main tree |
| S7 | `gh api repos/airbytehq/airbyte/contents/LICENSE`; `.../airbyte-integrations/connectors` listing | Airbyte repo licence is Elastic License 2.0; no NATS destination among 703 connectors |
| S8 | `gh api repos/synadia-io/nex/contents/README.md`, `gh api repos/synadia-io/nex/license` (404) | NEX is a workload runner; no licence file detected by GitHub |
| S9 | https://docs.nats.io/nats-concepts/jetstream/streams | Duplicate window default 2 min; discard Old default; byte/age limits unlimited by default |
| S10 | omos @ `62e3d22b`: `platform/data/redpanda-connect/deployment.yaml`, `platform/omos/{feeds,feed-scripts,intel-feeds,intel-scripts,tak-projector,sinks}.yaml`, `platform/nats/streams/*.yaml`, `services/bridge/{README.md,LICENSE}`, `services/schemas/README.md`, `docs/NATS-DOCTRINE.md` §9.1 | Current feeds, stream limits, existing Go bridge framework, envelope |
| S11 | Sibling lanes: `docs/research/omos-live-state.md` (live feed rates, broken Redpanda pipelines), `docs/research/nats-platform.md` (R10, stream table), `docs/research/source-registry-method.md` (catalog, CelesTrak 403 rule) | Live state and catalog facts reused here, not re-measured |
| S12 | This lane's proof: `docs/evidence/M0/ingestion/proof/` and `results/` (listed under Evidence) | Everything marked "measured" |

## Verified facts

### Candidates

**Redpanda Connect** (the incumbent; omos runs `redpandadata/connect@sha256:b462939a…`, S10 `deployment.yaml` line 527)
- Two licences: "Apache-2.0 covers the majority of connectors and functionality, and RCL
  (Redpanda Community License) which covers enterprise features" (S2 `licenses/README.md`).
  The engine module `redpanda-data/benthos` is MIT (S1).
- `internal/plugins/info.csv` lists 328 components: 216 `certified`, 72 `community`, 40
  `enterprise` (S2; corrected by the verifier from "329 … 1 other": the file has 329 lines
  including the header and no fourth tier, `gh api …/contents/internal/plugins/info.csv?ref=3080220410db`). **Enterprise (RCL):** every CDC input (postgres, mysql,
  mongodb, mssql, oracledb, dynamodb, spanner), `iceberg`, `snowflake_*`, `splunk`/`splunk_hec`,
  `salesforce*`, `slack*`, `jira`, `google_drive_*`, `gcp_bigquery_write_api`, `redpanda_common`,
  `gateway`, `a2a_message`, `sap_hana`, and **`otlp_grpc`/`otlp_http` and the
  `open_telemetry_collector` tracer and metric exporters**.
- **Apache-2.0 and certified, covering every OneMind delivery mode:** `http_client`
  (input and output), `http` processor, `http_server`, `websocket`, `mqtt`, `nats`,
  `nats_jetstream`, `nats_kv`, `file`, `sftp`, `aws_s3`, `generate`, `local` and `redis`
  rate limits, `memory` buffer, `cache`, `dedupe`, `json_schema`, `retry`, `workflow`,
  `unarchive`, `prometheus` metrics. `xml`, `subprocess` and the `sqlite` buffer are
  `community` tier, which is still Apache. The `jaeger` tracer is community (S2).
- The released `darwin_arm64` and Docker binaries are the full build. It imports
  `public/components/all`, which includes RCL code, and gates it at runtime:
  `CheckRunningEnterprise` returns "this feature requires a valid Redpanda Enterprise
  Edition license" (S2 `internal/license/shared_service.go`; S3). An Apache-only binary
  exists only as `cmd/redpanda-connect-community`, built from source. GitHub release
  v4.115.1 ships no community asset, only `redpanda-connect_*` and `redpanda-connect-cloud_*`
  (S2, `gh api …/releases/latest`).
- `nats_jetstream` output: `js.PublishMsg(ctx, jsmsg)` is a synchronous publish that
  waits for the PubAck. It has an interpolated `headers` map, so `Nats-Msg-Id` can be set
  per message. `max_in_flight` defaults to 1024. On `ErrConnectionClosed` it reconnects
  (S2 `output_jetstream.go` lines 173-214). It is built on nats.go v1.50.0 (S2 `go.mod`).
- Binary: 318 MB `redpanda-connect` 4.115.1 darwin-arm64. Checksum matched the published
  `.sha256` (`a935483e…`).
- Activity: 100 or more commits since 2026-07-01 (`gh api …/commits?since=2026-07-01`).

**Bento** (warpstreamlabs, fork of Benthos)
- MIT for the whole tree: "Copyright (c) 2024-present Bento contributors", plus the
  original 2020-2024 Benthos MIT notice (S4 `LICENSE`). GitHub reports `NOASSERTION`
  because of the dual notice (S1).
- Has the same components that matter here, all MIT: `http` processor with
  `rate_limit`, `backoff_on`, `drop_on`, `successful_on`, `oauth2` (client credentials),
  `jwt`, `basic_auth`, mTLS `client_certs` and `extract_headers`; `http_client` with
  `stream` mode (SSE and long-poll); `http_server` with `sync_response`; `websocket`;
  `mqtt`; `nats_jetstream`; `nats_kv`; NATS object store (`output_os.go`); `local`
  and `redis` rate limits; `sqlite` buffer; an OTLP tracer (`tracer_otlp.go`) (S4).
- `nats_jetstream` output uses `jCtx.PublishMsg(jsmsg)`, a synchronous publish that waits
  for the ack, and has interpolated headers (S4 `output_jetstream.go` line 189).
- Bento's own docs warn: "Performing deduplication on a stream using a distributed cache
  voids any at-least-once guarantees … the cache will preserve message signatures even if
  the message fails to leave the Bento pipeline" (S4 `processors/dedupe.md`).
- Release v1.22.0 (2026-10-09), 238 MB darwin-arm64 binary, checksum matched
  `bento_1.22.0_checksums.txt` (`432b7e86…`). 30 contributors; 100 or more commits since
  2026-07-01 (S1).

**Wombat**: MIT, v1.0.7 (2026-01-16), last commit 2026-02-09, 58 stars (S1, `gh api …/commits`).
Same Benthos lineage. It is too quiet to standardise on.

**Apache NiFi**: Apache-2.0, 2.12.0 (2026-09-13). It has `InvokeHTTP`, `ListenHTTP`,
`ConnectWebSocket`, `ConsumeMQTT` and `GetFile`. **No NATS processor** exists in the
apache/nifi tree (S6), so JetStream delivery would need a custom processor. It is a JVM
server. The edge variant is MiNiFi C++, Apache-2.0 (S1).

**Vector**: MPL-2.0, v0.60.0. The NATS sink has a `jetstream` mode and a header config
that sets `NATS_MESSAGE_ID` from an event field (S5 `config.rs` lines 22-64). Sources include
`http_client` (fixed `interval`, auth only `Basic`/`Bearer`/`Custom`, S5 `client.rs` lines
448-486), `http_server`, `websocket`, `mqtt`, `file`, `exec` and `kafka`. Transforms include
`throttle`, `dedupe` and `remap` (VRL). There is no SSE source, no OAuth2 refresh flow, and
no conditional-request (ETag/If-Modified-Since) handling in `client.rs` (grep found
none). It ships as a Rust single binary.

**Airbyte**: the repo root `LICENSE` is "Elastic License 2.0 (ELv2)" (S7). That is not an
OSI open-source licence, so it fails decision 0004 #18 ("must be open source"). Among 703
connector directories there is no NATS destination; there are `destination-kafka`, `-mqtt`,
`-pubsub` and `-redpanda` (S7).

**Meltano**: MIT (S1), a runner for Singer taps and targets in batch ELT. A GitHub search
for `target-nats singer` returned nothing (S1 search). It is a bulk/batch tool only.

**Temporal**: MIT, v1.32.1 (S1). It is a workflow orchestrator that needs its own server
and database. It does not collect data.

**NEX** (synadia-io/nex): v0.5.0 (2026-09-03). The README calls it "the NATS Execution
Engine … run native binaries", where nodes auction workloads (S8).
`gh api repos/synadia-io/nex/license` returns 404, so no licence file was detected. omos
already has Nexfiles for its overwatch services (`services/nex/overwatch/*/Nexfile`). NEX
runs collectors; it is not a collector framework.

**nats.go**: Apache-2.0, v1.55.0 (2026-10-23) (S1). The `jetstream` package gives
`PublishMsg` with PubAck (`ack.Duplicate`), `jetstream.MsgIDHeader`, KV
`Create`/`Update(rev)` with optimistic concurrency, and bucket TTL. All of these were used
and exercised in the proof (S12).

### omos today (S10, S11)

| Feed | Runtime | Upstream | Publishes | Live state |
|---|---|---|---|---|
| `feed-mta` | Redpanda Connect stream (only one running) | MTA Metro-North and LIRR GTFS-RT, 30 s | CoT XML to `tak.cot.inject` | running, about 3.1 msg/s (S11) |
| `feed-aircraft` | Redpanda Connect | OpenSky `/api/states/all`, 15 s | CoT to `tak.cot.inject` | **off** since 2026-08-27, "broken bloblang" (commit `129725ab`, S11) |
| `feed-ships` | Redpanda Connect | `wss://stream.aisstream.io/v0/stream`, global box | CoT to `tak.cot.inject` | **off** |
| `feed-hazards` | Redpanda Connect | USGS `2.5_hour.geojson` 60 s, NWS active alerts | CoT to `tak.cot.inject` | **off**; duplicates `feed-intel` sources |
| `feed-satellites` | Python Deployment (`python:3.12-alpine`) | CelesTrak `gp.php?GROUP=` + SGP4, 30 s | core `nc.publish` to `tak.cot.inject` (`feed-scripts.yaml` line 120); no JetStream ack | running, 16 positions/30 s |
| `feed-intel` (7 scripts) | Python Deployment | NOAA/NWS 300 s, USGS 60 s, ACLED 3600 s, FIRMS 600 s, GDACS 600 s, EMSC 120 s, EONET 300 s | JetStream `intel.…` with `Nats-Msg-Id` (`intel-scripts.yaml` lines 528-529, 769-770); `tel.feed.{id}.status` every 30 s | running; sparse events (S11) |

- Every omos stream has `duplicateWindow: 2m`, and only `WORLD` sets `maxBytes`
  (2 GB) (`platform/nats/streams/*.yaml`; NATS lane S11 line 91).
- `feed-intel` dedup is "in-memory set on alert id; cleared every 6 hours" plus
  `Nats-Msg-Id` (`intel-scripts.yaml` lines 284-289). After a pod restart, any item older
  than 2 min that the upstream still lists is republished. This is inferred from code and
  was **not observed** in the live stream (see Unverified).
- omos already has an Apache-2.0 Go bridge framework, `services/bridge` (`LICENSE`;
  README: "one subject pattern, one message envelope, one process interface"), with
  mavlink, tak, homeassistant, webhook and mqtt bridges. Its subjects predate the locked
  grammar, which this lane did not audit.
- The bus envelope is defined in `services/schemas/envelope/` (`v`, `id`, `ts`, `type`,
  `source`, `subject`, `data`) (`services/schemas/README.md`).

### Measured in this lane's proof (S12)

Setup: a dedicated `nats-server` 2.15.2 on `127.0.0.1:34222`, separate from the shared
`:4222` server used by other lanes, with streams `INTEL` (`intel.>`), `WORLD`
(`evt.world.>`, 100 msgs per subject), `TEL` (`tel.>`) and `FEEDEVT` (`evt.feed.>`). All
have `max_bytes` and `max_age` set, and a **20 s** duplicate window (omos uses 2 m; scaled
down so expiry can be tested in minutes). KV buckets: `ingest-state`, `ingest-locks` (TTL
10 s) and `overwatch-video`. A second `nats-server` is the upstream MQTT broker
(`:31883`). A private MediaMTX (API `:39997`, `moq: no`) serves the video source. A Python
stub (`proof/stubs/upstream_stub.py`) imitates the other upstreams with synthetic data and
logs every id it emits. `proof/scripts/verify.py` reads every stored message and reports
duplicates by `Nats-Msg-Id`, ids emitted upstream but missing from the bus, sequence gaps,
DLQ records, feed status and the video KV.

The eleven sources (one per delivery mode; same subjects and message ids for both engines):

| # | Mode | Upstream (stub) | Subject (locked grammar) | `Nats-Msg-Id` |
|---|---|---|---|---|
| 1 | REST JSON poll, ETag, 429 every 7th request | `/usgs.geojson` | `intel.seismic.usgs.event` | `usgs:{id}` |
| 2 | HTTP long poll with cursor | `/nws/longpoll?cursor=` | `intel.weather.nws.alert` | `nws:{id}` |
| 3 | SSE stream | `/eonet/sse` | `intel.natural.eonet.event` | `eonet:{id}` |
| 4 | WebSocket, 50 msg/s (load source) | `ws://…:38081/ais` | `evt.world.sea.{mmsi}` (doctrine §9.1) | `ais:{mmsi}:{ts}` |
| 5 | Webhook push | stub POSTs to `:38090/webhook/cap` | `intel.alert.cap.alert` | `cap:{identifier}` |
| 6 | MQTT QoS 1, persistent session | `sensors/+/reading` | `tel.ws1.weather.reading` | `mqtt:{station}:{seq}` |
| 7 | Bulk file, If-Modified-Since | `/celestrak/stations.txt` | `intel.space.celestrak.tle` | `tle:{norad}:{epoch}` |
| 8 | RSS | `/gdacs/rss.xml` | `intel.disaster.gdacs.event` | `gdacs:{guid}` |
| 9 | HTML scraping | `/outages.html` | `intel.infra.outages.event` | `outages:{id}:{customers}` |
| 10 | Map tiles (XYZ) | `/tiles/3/2/3.png`, HEAD probe only | none: tiles never go on the bus; health only | n/a |
| 11 | Video | MediaMTX `/v3/paths/list`, real RTSP test stream | KV `overwatch-video` key `{path}` | n/a |
| all | Health | n/a | `tel.feed.{source}.status`; rejects to `evt.feed.{source}.rejected` | |

Chaos timeline (`proof/scripts/chaos.sh`), under AIS load: `kill -9` nats-server at 45 s,
restart at 70 s; upstream returns 503 from 110 to 135 s; `kill -9` the collector at 160 s,
restart at 190 s (30 s down, longer than the 20 s dedup window); upstream serves malformed
records from 220 to 235 s; verify at 300 s. Ids emitted in the last 40 s are ignored.

**Chaos results.** Each cell gives duplicates stored / ids emitted upstream but never stored.
Files: `results/<run>/verify.json`, `timeline.txt`, `status-during-upstream-outage.json`,
`webhook-responses.log`, `metrics-*.txt`, `engine-warnings.txt`, and (from run 3)
`engine.log.gz`.

| Source | Bento run 1 | Bento run 2 (fixed configs) | Go run 1 | Go run 3 | Go run 4 (lease fix) | **Go run 5 (final)** |
|---|---|---|---|---|---|---|
| usgs REST/ETag | 14 / 0 | 13 / 0 | 0 / 0 | 0 / 0 | 0 / 0 | **0 / 0** |
| nws long poll | 3 / 0 | 3 / 0 | 0 / 0 | 0 / 0 | 0 / 0 | **0 / 0** |
| eonet SSE | 0 / 2 (`e81`, `e82`); gaps **unverified** (run 1 `verify.json` has no sequence-gap fields) | 0 / 2, gaps `56..67`, `81..94` | 0 / 0 | 0 / 0 | 0 / 0 | **0 / 0, no gaps** |
| ais WebSocket (load) | 0 / 0 | 0 / 1 | 0 / **366** | 0 / **340** | 0 / 0 | **0 / 0** (11,239 stored) |
| cap webhook | 0 / 2 | 0 / 2 | 0 / 2 | 0 / 4 | 0 / 2 | **0 / 2** (both while receiver killed) |
| mqtt QoS 1 | 1 / 0 | **239** / 0 | 0 / 1 (`ws1-1`; verifier correction from 0 / 0, `go-chaos-run1/verify.json`) | 129 / 0 | 87 / 0 | **86** / 0 |
| celestrak bulk | 0 / n.a. | 0 / n.a. | 0 | 0 | 0 | **0** |
| gdacs RSS | 7 / 0 | 7 / 0 | 0 / 0 | 0 / 0 | 5 / 0 | **0 / 0** |
| outages scrape | 3 / 0 | 3 / 0 | 0 / 0 | 0 / 0 | 3 / 0 | **0 / 0** |
| DLQ (`evt.feed.*.rejected`) | 2 records (repeats collapsed) | 16 records with reasons | 3 | 15 | 16 | **17** with reasons |
| video KV `overwatch-video` | `sim-cam1` | `sim-cam1` | `sim-cam1` | `sim-cam1` | `sim-cam1` | **`sim-cam1`** |

Go run 2 is excluded: the harness reset the streams but not KV `ingest-state`, and the
stub's ids restarted at 0, so every id looked "seen" (`results/go-chaos-run2`). Lesson:
checkpoints must be reset whenever a stream is rebuilt or an upstream reuses ids. The
harness now does this (`proof.sh reset`).

**Single owner** (`proof/scripts/leader.sh`, `results/go-leader-run2/`). Instances `a` and
`b` ran together. All 11 leases went to `a`. After `kill -9 a` at 09:13:09, `b` held all
11 leases by 09:13:34 (in the clean run 1 the takeover took 8 s against a 10 s TTL:
`go-leader-run1/lease-log.txt`). Result: 0 duplicates and 0 missing on every source.
`go-leader-stale-state/` is the same test before the reset fix.

**Redpanda Connect compatibility** (`results/rpc-compat/`). Redpanda Connect 4.115.1 ran
the unchanged Bento configs for 120 s. It stored all 9 bus sources with 0 missing and 0
rejects, and wrote `sim-cam1` to the video KV (tiles never go on the bus) (verifier
correction from "all 11 sources", `rpc-compat/verify.json`). "Linted clean" is
**unverified**: no lint step is in `rpc_compat.sh` or `proof.sh`, and the only artefact is
an empty `engine-warnings.txt`. It also showed 94 MQTT duplicates.

**What the chaos runs found (each one is a reusable rule):**
1. **In-memory state duplicates after a restart.** Bento keeps ETag, cursor and seen-set
   in memory, because writing them to a shared cache before the ack voids at-least-once
   (S4). A collector outage longer than the dedup window therefore republishes every item
   the upstream still lists: usgs 13-14, gdacs 7, nws 3, outages 3 per restart. Go
   checkpoints to KV after the ack: 0.
2. **SSE needs `Last-Event-ID`.** Bento's `http_client` stream cannot send a dynamic
   `Last-Event-ID`, so each reconnect skips events (gaps `56..67` during the NATS outage
   and `81..94` across the restart). Go resumes from the checkpoint with no gaps.
3. **A lease held on the bus must survive a bus outage.** Go runs 1 and 3 lost 340-366
   AIS positions because a failed lease renewal stopped the source and discarded its
   queue (`go-chaos-run3/engine.log.gz`: `lease lost … context deadline exceeded`
   04:59:48, re-acquired 05:00:18). Fix: treat "bus unreachable" as "renewal deferred" and
   stop only when another instance holds the lease. Runs 4-5: 0 lost, and leader takeover
   still works.
4. **A failed checkpoint read is not an empty checkpoint.** Run 4: polls during the NATS
   outage got an error from KV, treated it as "no state" and republished (gdacs 5,
   outages 3). Fix: skip the poll. Run 5: 0, with 7 `checkpoint read` skips logged.
5. **Webhook receivers should hold the response until the ack.** Bento holds the request
   until the output acks, and lost nothing during the NATS outage. Go's "fail fast with
   503" (runs 2-4) used up the sender's 5-attempt budget and lost 1-2 alerts. Holding up
   to 4.5 s (run 5) lost 0. Alerts sent while the only receiver is dead are lost in both
   engines (`c37`/`c38`, `c38`/`c39`). That is why push receivers need two or more replicas.
6. **Bloblang numbers are float64.** `this.mmsi.string()` produced subject
   `evt.world.sea.3.66000184e+08`, which adds tokens to the subject (smoke run). Ids need
   `.int64()`. Go decodes ids as integers.
7. **Benthos-family pitfalls found and fixed in configs:**
   - the `http` processor sends the message as the request body, so a `generate` input
     with `root = {}` sent a `{}` body on every GET. The stub answered `501 Unsupported
     method ('{}GET')` (`bento-chaos-run1/verify.json` `lastError`). Fixed with
     `root = ""`;
   - later processors still run on an errored message, so the DLQ reason shows the last
     error, not the schema error.
8. **DLQ ids must be unique per occurrence.** A payload hash as `Nats-Msg-Id` collapsed 14
   identical bad frames into 1 record (Bento run 1). Fixed in both engines (hash plus
   nanoseconds).
9. **MQTT QoS 1 redelivers after `ack_wait`.** nats-server MQTT redelivers a QoS 1 message
   with no PUBACK after `ack_wait`, default 30 s
   (https://docs.nats.io/running-a-nats-service/configuration/mqtt/mqtt_config). That is
   longer than the 20 s test window, so both engines stored redeliveries (Go 0-129: 0 in run 1, 86-129 in runs 3-5, verifier correction;
   Bento 1-239; Redpanda Connect 94 with no chaos at all). Both engines PUBACK only after
   the output ack (Redpanda Connect `internal/impl/mqtt/input.go` lines 245-249; Go
   `SetAutoAckDisabled` plus `msg.Ack()`). Why PUBACKs are late or missing is
   **unverified**. Mitigation: a dedup window of at least the broker's `ack_wait` (omos's
   2 m covers 30 s) or a KV seen-set for MQTT ids.
10. **Freshness works in both engines.** During the upstream outage the status snapshot
    marked streaming sources stale (ais, mqtt `stale: true`) and recorded `lastError`
    (`http 503`, `sse stream ended`) (`go-chaos-run5/status-during-upstream-outage.json`).
    Bento's polled sources showed `lastError: null` 20 s into the outage because its
    `http` processor was still retrying.

**Footprint** (measured): Go `ingestd-proof` binary 13.7 MB; Bento 238 MB; Redpanda Connect
318 MB. Go module cache plus build cache in `.tools`: 526 MB.

### Scores

Scale 0-3 (3 = meets the requirement out of the box; 0 = absent or disqualifying).
"Proof" marks rows backed by this lane's runs; the other rows are from source and docs only.

| Candidate | All delivery modes | Per-source rate limit and quota | Licence | Single-binary edge | Config as code | JetStream guarantees | Reliability list | Total /21 |
|---|---|---|---|---|---|---|---|---|
| **Go collectors on nats.go** (proof) | 3: all 11 built | 3: `x/time/rate` per source, Retry-After | 3: Apache-2.0 | 3: 13.7 MB static binary | 2: code plus a source registry; no DSL | 3: sync PubAck, `Nats-Msg-Id`, KV checkpoint after ack | 2: run 5 clean except MQTT redeliveries beyond the window and push loss while the only receiver is down (both shared with Bento) | **19** |
| **Bento 1.22** (proof) | 3: all 11 built (regex for HTML) | 3: `rate_limit` resources | 3: MIT, OTLP included | 2: 238 MB single binary | 3: YAML/Bloblang | 2: sync PubAck and headers; no post-ack checkpoint | 1: duplicates after restart, no SSE resume, no single-owner primitive | **17** |
| **Redpanda Connect 4.115** (compat run) | 3: same configs as Bento | 3 | 2: Apache core, but release binary carries RCL code; OTLP tracer is enterprise; Apache-only build needs source | 2: 318 MB | 3 | 2: same as Bento | 1: same as Bento | **16** |
| Wombat 1.0.7 | 3 (Benthos lineage, not built) | 3 | 3: MIT | 2 | 3 | 2 | 1 | 17, but **dormant** (last commit 2026-02) |
| Vector 0.60 | 1: no SSE, no OAuth2, no ETag | 1: `throttle` limits events, not upstream calls | 3: MPL-2.0 | 3: Rust binary, disk buffers | 3 | 2: JetStream sink with msg-id header | 1 | 14 |
| Apache NiFi 2.12 | 3 | 2 | 3 | 0: JVM server; MiNiFi C++ for edge | 1: flow JSON | 0: no NATS processor | 1 | 10 |
| Meltano + Singer | 1: batch only | 1 | 3: MIT | 1: Python | 3 | 0: no NATS target found | 1 | 10 (bulk only) |
| Airbyte | 1: batch | 2 | **0: ELv2, fails 0004 #18** | 0: platform on Kubernetes | 2 | 0: no NATS destination | 1 | disqualified |
| Temporal (orchestrator) | n/a: collects nothing | n/a | 3: MIT | 0: server plus DB | 2 | n/a | n/a | not a collector; overkill vs CronJob |
| NEX 0.5 (runner) | n/a: runs binaries | n/a | **unclear: no licence file** (S8) | 2 | 2: Nexfile | n/a | n/a | runtime option, not a framework |

## How it applies to OneMind

1. **Where each kind of source goes** (run prompt §10b, consistent with
   `source-registry-method.md`). The collector framework handles:
   - observations and events: JetStream (`intel.*`, `evt.world.*`, `tel.*`);
   - latest state: KV. The entity service writes `ent.*.state`, not the collector;
   - static overlays and bulk files: a one-shot run of the same binary in a Kubernetes
     `CronJob`, writing to Object Store or PMTiles/GeoPackage. No Temporal is needed;
   - map tiles: never on the bus. Only a health probe goes in `tel.feed.*.status`;
   - video: stays in MediaMTX. Only the registry goes to KV (`overwatch-video`, which
     exists in omos `platform/nats/streams/kv-overwatch-video.yaml`).
   Catalog shape (240 records, `Audit Reports/source-catalog-240-records.csv`, primary
   delivery): `live_entities` 32 and `event_alert_features` 53 go to the collector and
   JetStream; `static_overlays` 40 go to the bulk CronJob; `raster_vector_imagery` 32 go
   to the tile proxy; `media` 13 go to MediaMTX; `non_geospatial_reference` 70 go to the
   Hermes knowledge store.
2. **One framework, many sources.** The proof's Go core is 896 lines including all
   eleven sources (`proof/go/main.go`). It has seven primitives:
   - lease per source;
   - publish with ack, retry and msg-id;
   - checkpoint after ack;
   - JSON Schema validation with a DLQ;
   - conditional, rate-limited fetch;
   - status heartbeat;
   - metrics.
   A new source of a known kind (REST/RSS/bulk/SSE/WS/MQTT/webhook) is then a registry
   entry plus a parse function. Move the core into omos `services/bridge` (Go, Apache-2.0,
   already the "standard contract" home) and wrap payloads in the
   `services/schemas/envelope` envelope.
3. **The bus must change too, whichever engine is chosen.** A 2 min duplicate window
   cannot protect a feed that re-lists an item for an hour, such as USGS `all_hour`. This
   is the same as NATS-lane risk R10. The proof shows that collector-side checkpoints fix
   it without enlarging the window. Even so, set `duplicateWindow` per stream to the
   longest replay horizon of its feeds (the NATS lane proposes 1 h for `INTEL`), and set
   `maxBytes` on every stream.
4. **Webhooks are the exception to "one owner".** A push source cannot be led by a lease,
   because a dead receiver loses whatever the sender gives up on. Proof: `cap` ids lost
   only while the collector was down (`webhook-responses.log`). Run push receivers as two
   or more stateless replicas behind a Service and rely on `Nats-Msg-Id`. Everything
   else runs one owner per source through the KV lease.

## Risks

- **Engineering cost.** Go collectors are code. Each new parser is a Go function and
  needs review. Mitigation: generic source kinds driven by a registry, plus golden tests
  from recorded fixtures (§10 "Replayable").
- **Ephemeral streams under long outages.** The Go AIS reader buffers 20,000 positions in
  memory (400 s at 50 msg/s), then drops the oldest and counts it (`ingest_dropped_total`;
  0 in every run). That is a policy for positions, not a guarantee. Outages longer than
  that, or a crash during one, lose the queue. A disk spool (Bento has a `sqlite` buffer)
  is not built.
- **Lease semantics.** Because a lease is kept alive through a bus outage, an instance
  that is partitioned from the bus while another instance can reach it would keep
  reading upstream until it reconnects and sees the lease taken. Its publishes cannot
  land while it is partitioned, and ids dedupe, so the cost is double upstream load for
  that time, not double data. Not tested.
- **No upstream sends a sender-side replay for webhooks.** Loss while all receivers are
  down depends on the sender's retry budget. Measured: the stub's 5 attempts over about
  20 s were not enough for a 30 s outage.
- **Full disk was not tested.** Disk budget on this machine (23 GiB free, 15 GiB floor)
  ruled out filling a volume. By code, a publish error makes the Go collector back off and
  block, without dropping. JetStream's own behaviour on a full disk is not shown here.
- **Clock skew.** Dedup keys are upstream ids, never local time, so skew cannot cause
  duplicates or drops. `fetchedAt` and stale flags use the collector's clock and would be
  wrong under skew. Not tested.
- **Licence drift.** Redpanda moved enterprise connectors under RCL. A future change
  could move more. Choosing nats.go (Apache) removes this dependency.
- **Subjects.** The proof's `intel.alert.cap.alert`, `intel.infra.outages.event` and
  `tel.ws1.weather.reading` are closest-fit placements inside the locked grammar. The DLQ
  subject `evt.feed.{source}.rejected` and the existing `tel.feed.{id}.status` need
  confirmation by the bus-design lane (decision 0004 #8). `feed-intel` publishes 5- and
  6-token `intel.` subjects (for example `intel.seismic.usgs.{bucket}.{id}`) that the
  4-token grammar line does not show. Migrations keep current subjects until the
  bus-design lane rules, so consumers (`sink-clickhouse` `intel.>`, projector) do not break.

## Recommendation

**Standard: Go collectors on `nats.go`**, as one framework (`omos-ingest`) in omos
`services/bridge`, one container image per release, one Deployment with
`replicas: 1` per source group (leases make replicas safe), push receivers with
`replicas: 2`, and bulk sources as `CronJob`s of the same binary. Keep Bento's
configs from this proof as the documented fallback. Do not adopt Redpanda Connect for new
work, because its Apache-only build is source-only and its OTLP tracing is enterprise.
Retire it once `feed-mta` moves.

What every collector must have before its PR is ready (the reliability list, with the
test that proves it, taken from this proof):

| Item | Mechanism | Test |
|---|---|---|
| Publish ack, retry, dedup | `js.PublishMsg` + `Nats-Msg-Id` + capped exponential backoff; checkpoint after ack; a failed checkpoint read skips the poll | chaos: NATS `kill -9`, collector `kill -9`; 0 dups / 0 missing (Go run 5) |
| One owner | KV `ingest-locks` lease, `Create` then `Update(rev)` every 3 s, TTL 10 s; "bus unreachable" defers renewal, only a lease held by another instance stops the source; push receivers exempt (2+ replicas) | `leader.sh`: two instances, kill leader |
| Schema at the edge | JSON Schema from `services/schemas`; reject to `evt.feed.{source}.rejected` with the reason; id unique per occurrence | chaos: malformed upstream window |
| Politeness | per-source limiter, `If-None-Match`/`If-Modified-Since`, honour `Retry-After` on 429/503, no retry on 403 (CelesTrak rule, `source-registry-method.md` line 126), terms recorded in `sources.yaml` | stub 429 every 7th call |
| Freshness | `tel.feed.{source}.status` every 10 s (`fetchedAt`, count, `lastError`, stale); Prometheus `ingest_feed_age_seconds`; Grafana alert on age > limit | chaos: upstream 503 snapshot |
| Limits | stream `max_bytes`/`max_age`/discard set per stream; bounded queues; no unbounded memory | stream config review; queue cap |
| Survives failures | reconnect forever; checkpoint after ack; last good data kept and marked stale | chaos: all four failures |
| Replayable | recorded fixtures drive golden tests in CI; streams replay into the entity service | to build in A0 |
| Observable | JSON logs (Loki), Prometheus metrics, OTel traces (Tempo) | to build in A0 |
| Deployable | pinned GHCR image, Argo Application, requests/limits on the ecosystem pool, SealedSecrets for keys | omos checklist |

### Migration plan for current omos feeds (one PR each; old pipeline stays on until the new one matches it for 7 days, per 0004 #18)

Ordered by value to OneMind: life-safety alerts first, then the 3D picture's moving
entities, then reference layers. Each step publishes raw observations to JetStream
(feeding the entity service A4), never CoT straight to `tak.cot.inject`.

| Order | Feed (today) | Kind | Target | Notes |
|---|---|---|---|---|
| 0 | framework PR | n/a | `services/bridge` core + `sources.yaml` + golden-test harness + Grafana staleness alert | Prerequisite. Also bus PR: `maxBytes` on all streams, longer `INTEL` window (NATS lane) |
| 1 | NOAA/NWS alerts (`feed-intel`) | REST poll | `intel.weather.noaa.alert.*` (keep subject) | Highest civilian value; already JetStream. Adds post-ack checkpoint and lease |
| 2 | USGS + EMSC seismic (`feed-intel`) | REST/FDSN poll | `intel.seismic.*` (keep) | Pilot candidate: simplest; delete Redpanda `feed-hazards` (duplicate of 1 and 2) |
| 3 | NASA FIRMS (`feed-intel`) | CSV bulk poll | `intel.fire.nasa.firms.*` (keep) | Map key from SealedSecret `omos-feed-secrets` |
| 4 | Aircraft, OpenSky (Redpanda, **off**) | REST poll | raw observations under `evt.world.air.{id}` (doctrine §9.1) | Revive only with an AOI bounding box and per-source quota; auth mode **unverified** |
| 5 | Ships, aisstream (Redpanda, **off**) | WebSocket | `evt.world.sea.{mmsi}` (as proved) | Key needed; AOI box instead of the global box; bounded queue with drop counter |
| 6 | GDACS, EONET (`feed-intel`) | RSS, REST | `intel.disaster.gdacs.*`, `intel.natural.nasa.eonet.*` (keep) | |
| 7 | Satellites (`feed-satellites`) | bulk TLE + SGP4 | TLEs to `intel.space.celestrak.tle`; positions computed by the entity service | Today core-publishes CoT with no ack; CelesTrak 2-hour/403 rule applies |
| 8 | MTA GTFS-RT (`feed-mta`, the only running Redpanda stream) | protobuf poll | raw trips; transit has no doctrine subject, so a 0004 #8 proposal is needed (closest `evt.world.{class}.{id}`) | Low value to OneMind; migrate last, then remove Redpanda Connect |
| 9 | ACLED (`feed-intel`) | REST poll, key | `intel.conflict.acled.*` (keep) | Terms of use to review before any redistribution |
| n/a | `omos-tak-projector`, `sink-clickhouse` | consumers | unchanged | Not feeds |
| n/a | `services/bridge` mavlink, homeassistant, mqtt, webhook, tak | edge bridges | adopt the framework core (lease, ack, DLQ, status) when next touched | Already Go |

Catalog sources (240 records) follow the same order once lane a's `sources.yaml` assigns
each a kind: free live entities and alerts first, keyed free sources only when a key is
in the environment (0004 #19), and paid sources never.

## Unverified items

- (Verifier, 2026-10-29) That Redpanda Connect `lint` passed on the Bento configs in the
  rpc-compat run; no lint command or output is in the evidence.
- (Verifier) Bento run 1 EONET sequence gaps; only run 2 recorded `storedSeqGaps`.

- That `feed-intel` produces duplicates in the live `INTEL` stream after a restart. This
  is inferred from code (`intel-scripts.yaml` lines 284-289 plus a 2 m window). No live
  check was run.
- OpenSky's current authentication mode and rate limits (needed for step 4). Not read
  in this lane.
- NEX's licence. GitHub detects no licence file (S8). The terms may be stated
  elsewhere; not found.
- Airbyte connector licences may differ from the root ELv2 `LICENSE`. Only the root file
  was read.
- Behaviour of either engine on a full disk, under clock skew, and with OTel traces into
  Tempo. None was tested here.
- Wombat, Vector, NiFi, Meltano and Temporal were scored from source and docs only. None
  was run.
- A real upstream was not called in this lane. All upstreams were local stubs or the
  private MediaMTX. Endpoint testing belongs to lane part a.
- Why MQTT PUBACKs were late or missing, so that nats-server redelivered QoS 1 messages
  after its 30 s `ack_wait`, in both engines (finding 9). Measured, cause not found.
- My first explanation of the Go AIS loss (a reader blocked in a synchronous publish)
  was **wrong**. Decoupling the reader in run 3 did not fix it. The run 3 log showed
  the real cause, the lease (finding 3), and run 4 confirmed it.

### Blockers

- None from the permission system. All downloads (Bento, Redpanda Connect, Go modules,
  two PyPI packages) worked and stayed under `work/.tools`.
- `nats stream purge --force` on two KV backing streams prints "cannot ask for
  confirmation without a terminal" (`results/*/` runs). Harmless: KV contents do not affect
  stream verification.
