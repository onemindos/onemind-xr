# OneMind feed platform — revised assessment

**Audit date:** October 28, 2026 (UTC)  
**Mode:** read-only Mac/vault/source inspection; no deployment or configuration changes.  
**Decision status:** architecture recommendation, not a claim that the platform is already reliable or that any upstream endpoint has passed a live test.

## Bottom line

The requirement is a fabric-native **feed registry, ingestion platform, independent TAK publication layer and operator control surface**, not simply a visual replacement for an ETL pod. My earlier recommendation to start by replacing Connect with n8n was premature.

Keep NATS/JetStream as the fabric contract. Retain Connect provisionally where it fits collection/transformation. Use dedicated adapters for demonstrated codec, connection, hardware or computation requirements. Use n8n for surrounding automation, enrichment and alerts, not as the compulsory per-observation transport. Do not introduce Node-RED just to obtain toggles: the control contract must work independently of the collector runtime. A bounded workload evaluation can later decide whether Node-RED or another engine materially improves operations.

## 1. What was actually read and verified

Exact vault root on the Mac:

`/Users/zeus/OneMind - 10-23-26/notion-transfer/so/`

Relevant catalogs:

- `01 The Fabric/Geo/Map Layers/`
- `00 Core/Platform/Fabric Feeds/`

Context included the Redpanda Universal Feed Engine technical reference, before/after feed architecture, TAK CoT source guide, Geo DB/terrain and KMZ documents, and the planned OneTAK feed-control UI entries.

| Scope | Verified count | Interpretation |
|---|---:|---|
| Main Fabric Feeds database records | 160 | Database members, not 160 working integrations |
| Preserved older LiveUAMap top-level copy | 1 | Kept separately; explains the folder's extra row |
| Map Layers records | 79 | Includes datasets, websites, tools, platforms and standards |
| Total feed/map records retained | 240 | Every row preserved by original ID/path |
| Supporting context files retrieved | 21 | Not added to the 240-record catalog total |

The two catalog directories contain 244 files: 240 records plus four host/view files. There are no duplicate record IDs. Duplicate-title/collection-display overlaps remain explicit rather than silently merged. Historical Live labels are not current health checks. Most record bodies are title-only; frontmatter describes intent, not an executable source contract.

Analyst primary-delivery classification of the 240 records: 32 live-entity, 53 event/observation, 32 imagery/tile, 40 static-overlay, 13 media and 70 non-geospatial/reference. These are **record classifications, not runnable-source counts**. Role classification identifies 129 source/dataset candidates; the others include websites, commercial product references, standards and tooling. No candidate was certified currently accessible.

Parent verification rechecked all 240 raw contents and hashes against the retrieved input, every recorded property line, nine implementation files directly on the Mac and nine key implementation evidence ranges. Detailed evidence is in `parent-verification.json`.

## 2. The workload needs multiple delivery paths

| Workload | Catalog examples | Required output boundary |
|---|---|---|
| Moving entities | Aircraft, vessels, trains, satellites, APRS, robots/drones | Time-aware normalized state and controlled CoT projection with stable IDs, movement/rate/AOI policy and freshness |
| Alerts and observations | USGS, NOAA/NWS, FIRMS, GDACS, conflict and environmental readings | Revision/cancellation/expiry-aware features and selective CoT/geometry projection |
| Maps, imagery and terrain | WMS/WMTS/XYZ, PMTiles, STAC assets, GeoTIFF/DTED | Authorized tile/asset serving and tested client configuration; not conversion of every tile into CoT |
| Static overlays | Infrastructure, boundaries, rail lines, POIs | Feature services, layers or data packages with explicit refresh/size policy |
| Media | Camera catalogs, video, scanner audio and radio transcription | Metadata on the fabric; appropriate media transport/storage and derived observations separately |
| Reference/enrichment | News, economics, cyber/reputation, sanctions | Typed non-geospatial records; no invented map coordinates |

A provider homepage, dashboard or protocol specification is not a proven API. Paid/restricted, device-dependent and undocumented sources remain blocked from activation until an authorized executable contract is established. The catalog's USPTO/USAspending mismatch, duplicate LiveUAMap auth/status values, unofficial tile templates and North Star marketing/UI references require review, not guessed repairs.

## 3. Recommended target architecture — not the currently accepted universal schema

```text
Source registry + authorized feed-control API / OneTAK operations UI
                           |
                persisted desired state on the fabric
                           |
       Connect pipelines / specialized adapters / edge collectors
                           |
               validation + versioned observations
                           |
                    NATS / JetStream
                 /          |             \
         durable archive   TAK publisher   other consumers
                            |              AI / alerts / local views
                       TAK Server
                            |
                   selected TAK clients

Tiles / terrain / large assets -> asset and map services -> client layers
Audio / video                  -> media services         -> client players
```

The registry is shared across runtimes. It defines canonical provider/dataset/interface IDs, approved endpoint/subscription, source schema, secret reference, quotas versus refresh cadence, geographic scope, expected freshness, output policy, retention and ownership. Not every listed source becomes a separate pod or workflow.

This target requires adapters/migration. The inspected projector currently accepts top-level entity state on Core `ent.*.*.state` and a different top-level intel envelope on JetStream `intel.>`; it does not already accept an arbitrary new normalized event envelope.

## 4. Three separate on/off controls

| Operator control | Scope | Off means | Must not implicitly stop |
|---|---|---|---|
| Collect source | Canonical source/dataset; shared authorized operation | Pause new polling/subscription work with explicit in-flight drain policy | Other sources, retained history or unrelated consumers |
| Publish to TAK | Dataset plus TAK sink/mission/audience | Stop eligible publication there; apply explicit expiry/withdraw policy | Collection, archive, agents or other destinations |
| Show on my map | User/device/view and layer | Hide/unsubscribe in that view only | Global collection, shared TAK publication or another operator's view |

Persist desired collection/publication settings and show the worker's applied revision separately. Store local visibility in the appropriate per-user/device scope. Changes need authorization, audit and reconciliation after restarts. An accepted request is not proof it applied.

Each feed's control row should show desired/applied collection state, desired/applied TAK state, local visibility, last successful source response, source event age, validation failures, durable publication status, queue age/depth, projector forwarding and the latest available TAK/client evidence. Empty legitimate polling results must be distinguishable from HTTP/auth/parser failures.

Turning publication off is not automatically immediate removal of an already-visible remote marker. Test supported stale/withdraw behavior on each selected client. A native local view can keep receiving bus data while TAK output is off; a view that only mirrors TAK cannot promise that behavior without another data path.

## 5. Verified code defects that a runtime swap would not fix

These are source/configuration findings, **not verified live-process observations**.

| Finding | Exact repository evidence | Operational consequence |
|---|---|---|
| Only MTA selected in Connect launch args | `platform/data/redpanda-connect/deployment.yaml:528` | Other mounted pipeline definitions are not automatically active |
| Intel watchers terminate after initial KV replay | `platform/omos/intel-scripts.yaml:346-367`; analogous loops for all seven adapters | Later on/off/config changes are not continuously applied by these loops |
| Projector publishes to Core NATS, then ACKs intel | `platform/omos/tak-projector.yaml:375-399` | ACK does not establish TAK receipt or map rendering |
| TAK injector is a Core subscriber | `services/takserver/plugin-src/src/main/java/tak/server/plugins/NatsBridgePlugin.java:98-137` | Current code does not provide durable consumer recovery at this final boundary |
| ClickHouse batches are cleared before HTTP and discarded on failure; consumer ACK policy is none | `platform/omos/intel-scripts.yaml:116-167,244-262` | Archival loss is an explicit failure path |
| Projector enable is process-global and volatile | `platform/omos/tak-projector.yaml:401-427` | Not per-feed, resets after restart, and direct CoT adapters bypass it |

Additional reviewed gaps: same-ID revisions suppressed as duplicates, inconsistent expiry/offset-time handling, missing AOI/movement policy on some routes, valid zero-coordinate handling versus fabricated `(0,0)`, secret-value logging, disabled server certificate verification and boot-time unpinned dependencies. See `implementation-audit.md` for exact evidence and qualifications.

The Mac working tree was clean at `96fea4867b92696cea3f5905d147fa830167017e`, on `codex/owner-merge-prep-handoff`, not main. Cached `origin/main` was `7082e87ca17f26e370212b58c1e14a078543d0e7`, 189 commits ahead. All nine core implementation files rechecked matched that cached main. Neither cached reference proves the current Argo revision or mounted running code.

## 6. What reliable operation must prove

1. **Source contract:** exact authorized request/subscription, actual schema, quota, credentials readiness, coverage/AOI and expected freshness before enabling a dataset.
2. **Control behavior:** independent source-on/TAK-off, source-on/TAK-on/local-hidden, source-off/local-visible, authorized multi-user behavior, applied revision reporting and restart persistence.
3. **Durable recovery:** persisted ingestion where required, bounded retry/backoff, poison-message quarantine and no premature archival ACK. Prove broker disconnect, worker restart and sink outage recovery.
4. **Correct updates:** duplicate suppression must retain changed positions and revised alerts under the same upstream identity. Reject out-of-order/expired live updates rather than replaying old positions as fresh. Historical recovery and live-map replay are different policies.
5. **TAK path:** show evidence from collection through bus persistence, projection and TAK receipt, then render stable expected UIDs/layers on a selected client. A socket write or broker ACK is not client visibility. Bound claims to the receipts/instrumentation actually available.
6. **Map/media paths:** independently test authorized layer URLs, styles, CRS/tile matrices, asset/offline behavior, media access and selected clients; do not judge them with a CoT-only test.
7. **Operational hardening:** verify TLS identities, isolate secrets from logs/config events, pin dependencies, scope source/control permissions, preserve rollback and monitor per-stage freshness and failures.

Representative pilot set: polled JSON/GeoJSON hazard; GTFS-RT protobuf transit with static metadata where necessary; WebSocket AIS; one tile/raster overlay; and an authorized device/media-metadata path as available. Use fixtures first, then approved bounded live data. No fault injection or pilot publication was performed in this audit.

No system can promise blanket end-to-end exactly-once delivery or permanent upstream availability. The acceptance target is explicit failure behavior, bounded recovery, source-appropriate idempotence and observable evidence—not green pods.

## 7. Current product documentation cross-check

Official primary documentation consulted separately from the historical vault notes:

- Redpanda Connect streams mode supports independent streams and API management. Dynamic controls therefore do not inherently require switching runtimes. Runtime API state still needs an explicit persistent desired-state owner. Reference: `https://docs.redpanda.com/redpanda-connect/guides/streams_mode/`
- Connect has an explicit `nats_jetstream` output with stream persistence acknowledgments. This is a capability to evaluate, not evidence that the current Core-output configuration already has it. Reference: `https://docs.redpanda.com/redpanda-connect/components/outputs/nats_jetstream/`
- JetStream consumers track delivery/ACK state; Core subscriptions do not gain durable replay merely because a stream also captures the subject. Reference: `https://docs.nats.io/nats-concepts/jetstream/consumers`
- NATS Python KV watcher APIs and iteration behavior must be checked against a pinned installed version; the library's internal initial-replay marker is not a command to stop watching future changes. Reference: `https://raw.githubusercontent.com/nats-io/nats.py/main/nats/src/nats/js/kv.py`

These are capability/semantic checks, not throughput/resource benchmarks or a production qualification of any runtime.

## Limits and deliverables

No Mac source files, cluster resources, NATS controls, databases or upstream accounts were modified. No source endpoint, credentials, live Argo revision, loaded TAK plugin, queue configuration or EUD rendering was certified. The earlier pod inventory is not substituted for these missing proofs.

Companion files preserve the complete classified catalog, exact evidence ranges and parent verification. The next implementation decision should be based on the acceptance gates above; replacing the collector first would leave the most serious control/delivery gaps unresolved.
