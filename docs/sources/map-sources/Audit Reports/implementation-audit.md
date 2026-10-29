# OneMind feed implementation / control audit

## Scope and provenance

Read-only inspection of `/Users/zeus/omos` over the authorized Mac SSH route, completed **2026-10-28 02:47 UTC**. No Mac files changed, no fetch/pull/checkout, no deployments/restarts, no message injection or source-code execution, no secret/.env contents, and no live Kubernetes/NATS/TAK reads. Exported design documents were context, not evidence of current behavior.

- Clean working tree, HEAD `96fea4867b92696cea3f5905d147fa830167017e`, branch `codex/owner-merge-prep-handoff`; commit timestamp October 27, 2026.
- Cached `origin/main` is `7082e87ca17f26e370212b58c1e14a078543d0e7`, **189 commits ahead** of this checkout; no network fetch was performed. Local `main` is a third revision. Do not call the checked-out branch the deployed/main revision.
- All nine core feed/projector/sink/plugin files checked for parity are identical between HEAD and cached `origin/main` (see `read-only-verification.json`). Main's Argo/NATS stream-adoption files differ and were inspected separately from Git objects.
- **This is verified implementation/configuration, not verified live traffic or client visibility.** Live Argo revision, mounted scripts, running args, installed dependencies, stream/KV configuration, source credentials, TAK injection and EUD display remain unverified.

Machine-readable evidence: `implementation-evidence.json`. Its evidence IDs below resolve to exact repository path, ref, line range and sanitized snippet. `implementation-sources.json` contains source-file SHA-256 values and line-preserving sanitized snapshots.

## Findings that change the proposed operating model

1. **There is no single full-catalog feed engine running all mounted definitions in the inspected manifests.** Connect explicitly starts only `feed-mta.yaml`. Hazards, ships and aircraft are defined but not named in its launch arguments. Satellites covers `stations`, not every satellite group. Seven separate intel modules share one Python process/Deployment. [F03; `connect-launch`, `satellite-launch`, `runner`]
2. **The advertised intel runtime toggles have a static watcher defect.** All seven `watch_config()` loops stop when initial KV replay finishes. They do not continuously apply later `feed.{id}.enabled`/filter changes. [F02; `watch-noaa` through `watch-eonet`]
3. **JetStream persistence does not extend through TAK delivery.** Intel publishers use PubAck and message IDs, but projector output and direct feeds are Core NATS, and the TAK plugin subscribes with a Core dispatcher. Projector INTEL ACK occurs after Core publish, not after TAK/EUD receipt. [F04; `projector-handlers`, `plugin-core`, `plugin-inject`]
4. **The inspected ClickHouse sink intentionally loses data during failures.** It uses `ack_policy="none"`, RAM-only batches, clears the batch before HTTP, and logs/discards failures. It does not consume raw `tak.cot.>` or `evt.world.>`. [F05; `ch-subscription`, `ch-buffer-clear`, `ch-loss-on-failure`]
5. **Collection pause, shared TAK output pause and individual-client visibility are distinct controls.** Current code has incomplete versions of the first two, not a catalog-wide visible/applied-state UI or per-client visibility control. [F06; `projector-controls`, scoped search artifacts]

## Actual GitOps entry points

| Purpose | Actual path / reference |
|---|---|
| Feed/sink/projector Argo source | `platform/argocd/platform-omos.yaml:32-45` → `main`, `platform/omos`, non-recursive, automated prune/self-heal |
| Connect Argo source | `platform/argocd/platform-redpanda-connect.yaml:34-47` → `main`, `platform/data/redpanda-connect`, non-recursive, excludes standalone `pipelines/` and proto files |
| Effective Connect pipelines / proto / Deployment | **Embedded ConfigMaps and Deployment in** `platform/data/redpanda-connect/deployment.yaml`; mounted directory `/config/pipelines`; launch line **528** names only `/config/pipelines/feed-mta.yaml` |
| Python orbital feed | `platform/omos/feed-scripts.yaml:12-125` (`feed_satellites.py`), `platform/omos/feeds.yaml:15-70` (`feed-satellites`) |
| Seven intel adapters + runner + CH sink | `platform/omos/intel-scripts.yaml`; `platform/omos/intel-feeds.yaml` (`feed-intel`); `platform/omos/sinks.yaml` (`sink-clickhouse`) |
| TAK projection | `platform/omos/tak-projector.yaml` contains both script and `omos-tak-projector` Deployment |
| TAK Core NATS injector | `services/takserver/plugin-src/src/main/java/tak/server/plugins/NatsBridgePlugin.java:98-137` |
| Plugin defaults | `services/takserver/plugin-src/src/main/resources/plugin-config.yaml:1-5` → `tak.cot.inject`, `tak.cot.out`, `__ANON__` |
| TAK NATS leaf | `platform/takserver/deploy/tak-nats-leaf.yaml`; leaf credentials/config are an out-of-band Secret, not inspected |

Comments saying Connect runs every pipeline concurrently, old per-feed scaling examples, and references to a separate `sink-tak` workload are **not the operative configuration**. In this code, `sink-tak` is the projector's durable consumer name; the Deployment is `omos-tak-projector`.

## Feed definitions and contracts

“Configured to start” below means desired launch configuration, **not** a verified active source. There are twelve feed definitions in this inspected slice: four Connect pipelines, one satellite adapter and seven intel modules. MTA itself fetches two rail endpoints. Do not confuse definition count with provider/catalog coverage or health.

### Direct-to-CoT adapters

| Definition | Configured to start | Input / cadence | Output and policy |
|---|---|---|---|
| MTA | Yes, selected Connect argument | MNR + LIRR GTFS-RT protobuf, 30 seconds | UTF-8 CoT XML → Core `tak.cot.inject`; UIDs `MTA-MNR-*` / `MTA-LIRR-*`, `Land`, 120-second stale; no movement delta cache |
| Hazards | **No**, definition only | USGS + NWS, 60 seconds | CoT → Core `tak.cot.inject`, `Hazards`; ID dedupe in memory, TTL six hours / cap 10,000; separate from intel NOAA/USGS adapters |
| Ships | **No**, definition only | AISstream WebSocket | CoT → Core `tak.cot.inject`, `Sea`; world bounding box, no per-vessel movement/rate budget visible |
| Aircraft | **No**, definition only | OpenSky REST, 15 seconds | CoT → Core `tak.cot.inject`, `Air`, 90-second stale; optional username/password environment references exist, but the HTTP processor does not use them for authentication |
| Satellites | Yes, its Deployment has one desired replica | CelesTrak `GROUP=stations`; SGP4 every 30 seconds, TLE refresh four hours | CoT → Core `tak.cot.inject`, `Air`, `SAT-{satnum}`, stale interval × 3; no KV toggle or movement delta |

Evidence: `connect-mta`, `connect-hazards-dedup`, `connect-hazards-end`, `connect-ships`, `connect-aircraft`, `satellite-launch`, `satellite-cot`, `satellite-loop`. Connect's image is pinned by digest; the Python images/dependencies are not equivalently pinned.

### Intel adapters in `feed-intel`

All default `enabled=true`, load keys under KV bucket `feed-config`, prefix `feed.{id}.`, and emit process status on Core `tel.feed.{id}.status`. Each has a supervised feed-main restart loop, but its independent config watcher is not supervised after completing initial replay. API key availability was **not** checked.

| ID | Exact subject shape | Poll default | Additional config |
|---|---|---:|---|
| `noaa` | `intel.weather.noaa.alert.{state_lower}.{event_slug}` | 300 s | `areas=US` |
| `usgs` | `intel.seismic.usgs.{magnitude_bucket}.{clean_id}` | 60 s | `min_magnitude=2.5` |
| `acled` | `intel.conflict.acled.{iso3}.{event_slug}` | 3600 s | `api_key`, `email` required; `countries` optional |
| `firms` | `intel.fire.nasa.firms.{region}` | 600 s | `api_key` required, `area=world`, `days=1` |
| `gdacs` | `intel.disaster.gdacs.{event_slug}.{event_id}` | 600 s | no additional filter in `get_config()` |
| `emsc` | `intel.seismic.emsc.{magnitude_bucket}.{clean_id}` | 120 s | `min_magnitude=2.5` |
| `eonet` | `intel.natural.nasa.eonet.{category_slug}.{clean_id}` | 300 s | no additional filter in `get_config()` |

Publishers call `await js.publish(..., headers={"Nats-Msg-Id": ...})` and add to `seen` only after success. This is a real server persistence acknowledgment plus broker duplicate-window support, **not** proof of downstream projection, archival, TAK injection or client display. Source IDs and event-ID-only seen sets suppress revised same-ID observations as well as duplicates. NOAA clears its seen set after six hours; six other adapters have no periodic pruning/reset in the inspected source. `acled` and `firms` skip polls without their required KV credentials. [F08; adapter publish/key-gate evidence]

### What `omos-tak-projector` actually accepts

**Entity input:** Core NATS **`ent.*.*.state`**, not a universal arbitrary normalized-observation subject. The body must expose top-level `lat`/`lon` (or `latitude`/`longitude`), `entity_id`, `entity_type`, optional `confidence`, and an optional `properties` object containing names/callsigns/flight/MMSI/ICAO/tail, altitude/HAE, speed/SOG/velocity and course/heading/COG/track. Missing `entity_id` generates a random UUID, so callers need a stable ID to avoid new markers. UIDs are `OM.{entity_type}.{entity_id}`. CoT time/stale is regenerated at projection time using the type map. Only a five-second per-UID minimum interval is applied. [Exact implementation: `platform/omos/tak-projector.yaml:153-226,342-373`]

**Intel input:** JetStream **`intel.>`**, durable **`sink-tak`**, `DeliverPolicy.NEW` when the consumer is initially created. Body fields are **top-level** `id`, `type`, `source`, `lat`, `lon`, `timestamp`, `expires`, `properties`. `properties.headline`/`event` supplies the label. CoT UIDs are `INTEL.{id}`. Geometries, nested `location`, other observation envelopes or `evt.world.>` are not decoded by this handler. Missing ID defaults to `unknown`, which can collapse unrelated markers onto one UID. [Exact implementation: `platform/omos/tak-projector.yaml:231-297,375-427`]

**Output:** Both accepted inputs become CoT XML published with `nc.publish()` to **`tak.cot.inject`**, default group **`__ANON__`**. Projected XML includes `__group`, but no per-source `<marti><dest mission=...>` routing. Direct MTA/satellite/other pipeline XML bypasses the projector and carries its own mission tags. A generic new event schema is therefore a target architecture, **not** a drop-in contract this implementation already accepts.

## Reliability boundaries

```text
MTA Connect ───────── Core tak.cot.inject ────────────┐
Satellites ────────── Core tak.cot.inject ────────────┤
                                                   v
INTEL adapter ─ js.publish + PubAck ─ INTEL stream ─ projector ─ Core inject
                                                  |             |
                                                  |             v
                                                  |      TAK Core dispatcher -> send(takMessage)
                                                  v
                                           CH push consumer -> RAM batch -> HTTP insert
                                           ack policy NONE    failures discarded

Entity mirrors ─ Core ent.*.*.state ─ projector / CH sink
```

- The main-source INTEL/COT streams capture `intel.>` / `tak.cot.>` with file storage, three replicas and a two-minute duplicate window; `WORLD` captures `evt.world.>`. **Matching stream capture does not turn a Core subscriber into a replaying/durable consumer.** The plugin never reads a COT durable. These are inspected manifests, not verified live stream/account configuration. [Main-ref `main-intel-stream`, `main-cot-stream`, `main-world-stream`; `plugin-core`]
- Projector ACKs INTEL after Core publish. Disabled INTEL, malformed JSON, and unprojectable coordinates are ACKed/skipped. There is no TAK delivery ACK, retry ledger or dead-letter path at that boundary. Its Core entity input has no restart replay. NEW applies on initial creation; an existing durable's actual state was not inspected. [F04, F06]
- Entity projection has a time throttle, not movement delta, source-specific rate budgets, AOI gating or age validation. Minimum interval is cached before publish. There is no corresponding intel throttle, no catalog-wide AOI/policy engine, and no enforced client-specific filter. Direct CoT paths bypass projector policy entirely. [F07]
- ClickHouse receives **only `intel.>` and `ent.*.*.state` in this sink**. Direct raw CoT trains/satellites are not archived by it. Initial CH `LAST_PER_SUBJECT` delivers only the last retained item per subject, not every retained event: NOAA state/event, ACLED country/event and FIRMS region subjects can contain many distinct events. Existing durable history was not inspected. [F05, F08]
- CH uses `ack_policy="none"` even with `manual_ack=True`; there is no successful-INSERT acknowledgment gate. The list is removed from RAM before HTTP, failures are deliberately discarded, and there is no durable retry/outbox/idempotency gate. Its KEDA object is explicitly **observe-only, min=max=1**, not horizontal/failover scaling; comments explain no deliver group for multiple replicas and loss of Core input at zero replicas. [F05; `ch-keda`]

### Stale/data-quality defects relevant to full-catalog visibility

- `parse_iso()` handles Z/naive/date forms but not timezone offsets; parse failure silently substitutes “now.” NOAA forwards upstream `sent`/`expires` unchanged. An offset-bearing expires value therefore becomes approximately the projection time, rather than its intended expiration. [Projector `153-160`, NOAA payload `459-468`]
- Intel missing expires becomes **event timestamp + 30 minutes**, not ingestion time plus a source-specific retention. ACLED emits event dates at midnight with empty expires; FIRMS/USGS/EONET similarly emit older observation timestamps and empty expires. Such rows can be published already stale; no expiry rejection or refresh policy exists in the projector. [F07; `intel-contract`, `acled-payload`, `firms-payload`, `usgs-payload`, `eonet-publish`]
- Entity `lat or latitude` / `lon or longitude` treats numeric zero as absent. If aliases are missing, valid equator/prime-meridian values are rejected. Entity observation age is not checked before setting a fresh CoT timestamp. [Projector `165-201`]
- Several intel adapters default missing/failed coordinates to `(0,0)`, which passes the intel projector's range check. There is no “unknown position” contract or validation beyond numeric range. [e.g. `acled-payload`, `eonet-publish`, `intel-contract`]

## Existing controls: collection vs shared delivery vs client visibility

| Control | Actual effect | Gap / caution |
|---|---|---|
| KV `feed.{id}.enabled` | Intended to stop one of seven intel pollers; also supports poll interval/source-specific filters | All seven watchers end after replay (`if entry is None: break`). Startup uses only a 0.5-second yield, then enabled defaults true. No applied-version ACK; deletion becomes empty string, not a validated desired-state protocol. Does not hide existing markers or stop MTA/satellites. |
| Core `cmd.projector.disable` / `.enable` | Sets one process-global `_enabled` bool; affects projected entities and intel | No payload validation, persistent desired state, request/reply ACK or per-source selector. Defaults enabled again after process start. Disable ACKs/discards incoming INTEL and drops live Core entities. Direct CoT still flows. |
| Deployment scale / manifest changes | Stops a workload's collection or processing | All seven intel sources share one Deployment. Connect sources share one Deployment. Desired replicas are one; Argo automated self-heal owns these resources and does not ignore replicas, so ad-hoc scale is not a durable catalog toggle. No scale command was run. |
| CoT mission/group attributes | Direct feeds tag Land/Air/Sea/Hazards; plugin uses configured groups; projector uses `__ANON__` | Routing metadata is not an implementation of per-client, per-source visibility. Actual client subscriptions, groups and mission behavior not inspected. No marker-removal or immediate-hide event is emitted on disable. |
| `tel.feed.{id}.status` | Process counters, enabled, last poll/error/timestamp | Core telemetry, not a delivery receipt. Long polling sleeps prevent guaranteed 30-second heartbeat; many HTTP failures return zero instead of raising, leaving outer `last_error` empty. No collector→TAK→EUD applied-state chain. |

**Visible control surface:** scoped tracked-source searches of this checkout and cached `origin/main` found the KV consumers, projector commands, manifests and old CLI examples, but no corresponding full-catalog control API/UI writer. The broader main scan examined 1,396 candidate files, including TS/JS/JSX/HTML/CJS/Vue/Svelte, and found controls only in the recorded manifest/script/document paths. This does **not** rule out an out-of-repo application or installed flow. See `provenance-control-scan.json`, `main-control-scan.json` and their exclusions. [F06]

## Direct TAK TLS route: present plumbing, not the feed output path

- `cot-tap` opens mutual TLS to TAK `8089`, **reads** the CoT firehose, and republishes to Core `tak.cot.out`. It is **TAK→NATS**, not a durable feed→TAK writer. Deployment target is `takserver-messaging-service:8089`. Default `VERIFY_SERVER=false`; the Secret/certs were not read. [F10; `tap-direction`, `tap-target`, `tap-tls`]
- Its committed ConfigMap contains only a placeholder and documents an out-of-band replacement, so that manifest alone cannot establish the live mounted code. [F10; `tap-deployment`]
- Plugin source parses incoming `tak.cot.inject` XML, converts it and calls `send(takMessage)`; its live loaded JAR/config/connection was not checked. TAK leaf manifest describes TLS upstream, but leaf Secret configuration was not inspected. [F04; `plugin-inject`, `plugin-core`, `tak-leaf`]
- `services/bridge/bridges/tak/main.go:373-398` is a separate legacy UDP command forwarder on `omos.field.*.command`, not a TLS feed sink. [F10]

## Security and desired-state reliability gaps

- Every intel watcher logs `config[key]` values; this includes `api_key` and `email` keys used by ACLED/FIRMS. **No actual credential values were retrieved in this audit.** Secrets should not be part of plaintext applied-config logging. [F09; `watch-acled`, `watch-firms`]
- Feed/projector/sink NATS contexts disable certificate verification; Connect outputs set `skip_cert_verify: true`; tap defaults verification off. Encryption alone is not server-identity validation. [F09; `connect-mta`, `satellite-loop`, projector/sink/tap source]
- Python workloads install unpinned `nats-py`/other libraries at boot. The KV watcher behavior therefore cannot be tied to a verified deployed client version; the explicit break is wrong both with older iteration that stops on the None marker and newer iteration that yields it. Upstream primary-source verification is preserved in `nats-semantics-primary.json`; no installed package or live process was inspected. [F02, F09]
- Cached main explicitly excludes `kv-feed-config.yaml` from `platform-nats-streams`; that file's comments describe an app-created/R1 bucket and warn history/TTL/maxBytes are not verified. The HEAD file has a different replica declaration. **Neither should be presented as live truth or “fixed” without a live config read and approved change.** [F12; `main-kv-excluded`, `main-kv-context`]

## Full-catalog acceptance gaps (not deployment instructions)

The existing code is a useful partial implementation, but it does not yet establish:

1. One enumerated source registry with provider, credentials readiness, licensing/limits, AOI, collection cadence, desired state and actual health for every catalog entry.
2. Persisted and continuously watched collection controls with validated values, revisions/CAS, startup readiness, applied-state ACKs and no credentials in logs.
3. Independent per-source TAK delivery controls plus per-client layer visibility; these must not be conflated with collection pause or global projector disable. Disable behavior must specify expiry/removal and replay semantics.
4. A versioned accepted observation contract, stable source/entity/event IDs, consistent coordinates/units/time/expiry semantics, and explicit adapters from each catalog source.
5. Durable ingestion and replay/backfill, source-appropriate update dedupe, bounded caches, AOI/movement/rate/stale policy, and a retry/ACK boundary that does not claim Core publish means TAK receipt.
6. ACK-after-commit archival with retry/dead-letter/idempotency behavior and explicit coverage for direct raw CoT as well as intel/entity data.
7. Stage-specific health: API fetch → persisted observation → projection → TAK acceptance → actual EUD visibility, with visible on/off state and failure reasons.
8. Approved, non-injecting live readback to establish actual Argo revisions/args/scripts, stream/KV configuration, loaded plugin and existing passive traffic. Any deployment or fault-injection testing requires separate authorization.

No changes were made to implement these items. Parent review should verify the cited source snippets and independently distinguish running state from the desired manifests before making deployment claims.
