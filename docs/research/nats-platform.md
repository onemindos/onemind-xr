# NATS platform research (lane 7): the OneMind authority bus

Date: 2026-10-29. Lane: Phase 1 / M0, "NATS, all of it". Status: research complete;
independently verified 2026-10-29 (see "Verifier report" at the end; corrections are marked
**[verifier]** in place).

Method: primary sources (ADRs, server and client source, official docs, GitHub release
metadata), a read-only look at the live cluster (`kubectl get` and `get --raw` to the NATS
monitoring port only; no Secret was read), and proofs on this machine. The proofs ran on a
**private hub and leaf pair** (nats-server 2.15.2, ports 14222/24222, data under
`work/.run/nats-lane/`, deleted afterwards), so the shared run server on 4222 was not touched.
Every command and its output is in
[`docs/evidence/M0/nats-lane-proofs.txt`](../evidence/M0/nats-lane-proofs.txt), with proof IDs
P1–P10 referenced below. The throwaway localhost passwords are redacted there.

---

## Sources

**Primary specs and docs**
- S1 ADR-43, JetStream per-message TTL: https://github.com/nats-io/nats-architecture-and-design/blob/main/adr/ADR-43.md
- S2 nats-server releases (2.14.x, 2.15.x): https://github.com/nats-io/nats-server/releases
- S3 MQTT: https://docs.nats.io/running-a-nats-service/configuration/mqtt
- S4 JetStream on leaf nodes (domains): https://docs.nats.io/running-a-nats-service/configuration/leafnodes/jetstream_leafnodes
- S5 Leaf nodes: https://docs.nats.io/running-a-nats-service/configuration/leafnodes
- S6 Mirrors and sources: https://docs.nats.io/nats-concepts/jetstream/source_and_mirror
- S7 WebSocket: https://docs.nats.io/running-a-nats-service/configuration/websocket
- S8 Operator mode and resolver: https://docs.nats.io/learn/security/operator-mode
- S9 nats-server source: `server/opts.go` lines 272-273 and 3209-3222 on `main` (leaf remote
  `deny_imports`/`deny_exports`; **[verifier]** at tag v2.15.2 the same code is at lines 267-268
  and 3200-3213); `server/mqtt.go` v2.15.2 lines 3874-3881 (CONNECT protocol level must be 4,
  otherwise "unacceptable protocol version"); `server/opts.go` v2.15.2 lines 6223-6231 and
  `server/const.go` `DEFAULT_HOST = "0.0.0.0"` (WebSocket and MQTT listeners default to all
  interfaces when no host is set); `server/websocket.go`, `server/mqtt.go`, `server/leafnode.go`
  exist (`gh api repos/nats-io/nats-server/contents/server`)

**Clients and tooling (GitHub API, 2026-10-29)**
- S10 nats.c v3.14.0 (2026-09-17), Apache-2.0, README and `src/` listing (`js.c`, `kv.c`,
  `object.c`, `micro*.c`, no WebSocket source file): https://github.com/nats-io/nats.c
- S11 nats.go v1.55.0 (2026-10-23), Apache-2.0
- S12 nats.js v3.4.0 (2026-05-08), Apache-2.0, `@nats-io/*` modules (Core, JetStream, Kv,
  Obj, Services; Deno, Node/Bun, browser WebSocket): https://github.com/nats-io/nats.js
- S13 natscli v0.5.1 (2026-10-26); nack v0.25.0 (2026-10-21); nats.py (Apache-2.0)
- S14 Synadia Agents (Apache-2.0, python-agent-service-v0.5.0 2026-10-14), README:
  https://github.com/synadia-ai/synadia-agents
- S15 Orbit: orbit.go (Apache-2.0; modules `counters jetstreamext kvcodec natscontext natsext
  natssysclient ntf ntf-client pcgroups`), orbit.js (Apache-2.0; `counters fastingest
  messagepipeline muxsub nhgc`)
- S16 NEX: https://github.com/synadia-io/nex, latest 0.5.0 (2026-09-03); GitHub reports
  `license=null` and the root listing has no LICENSE file
- S17 Redpanda Connect v4.115.1 (2026-10-23): `licenses/README.md` ("Apache-2.0 covers the
  majority of connectors… RCL… covers enterprise features"); `internal/impl/nats/input_jetstream.go`
  carries an Apache-2.0 header
- S18 Redpanda broker `licenses/bsl.md` (BSL with Additional Use Grant forbidding a
  "Streaming or Queuing Service" offered to third parties)
- S19 Apache Kafka (Apache-2.0), Fast DDS (Apache-2.0), Zenoh 1.10.2 (EPL-2.0 or
  Apache-2.0 per https://zenoh.io/docs/getting-started/first-app/), `ros2/rmw_zenoh` README
  (a Zenoh router is needed for discovery; multicast discovery off by default)

**omos (fresh clone `work/omos`, commit 62e3d22b, 2026-10-28)**
- O1 `CLAUDE.md` (locked grammar, streams via NACK, bus authority)
- O2 `docs/NATS-DOCTRINE.md` §§3-8, §9.8
- O3 `platform/nats/streams/*.yaml` (14 Stream + 25 KeyValue CRs)
- O4 `platform/nats/values.template.yaml`, `nats-statefulset.yaml` (image `nats:2.14.2-alpine`), `ws-ingress.yaml`
- O5 `infra/edge/mac-leaf/nats-server.conf`
- O6 `platform/data/redpanda-connect/deployment.yaml` and `pipelines/*.yaml`
- O7 `platform/hermes/agent/deployment.yaml` lines 132-271 (leaf sidecar)
- O8 sibling lane: `docs/research/omos-live-state.md` (feeds, writers, TAK path)

**Live cluster (read-only, context `do-nyc3-omos-cloud`, 2026-10-29 ~08:00Z)**
- L1 `kubectl -n nats get pods,streams.jetstream.nats.io,keyvalues.jetstream.nats.io`
- L2 `kubectl get --raw /api/v1/namespaces/nats/pods/nats-0:8222/proxy/jsz?streams=true`
- L3 same proxy: `varz`, `accountz`, `connz`, `leafz` on nats-0/1/2

---

## Verified facts

### 1. Live omos bus today

| Fact | Evidence |
|---|---|
| 3 NATS pods, nats-server **2.14.2** (go1.26.3), max_payload 1 MiB, 6 client connections (**[verifier]** point-in-time and per pod; a re-read at ~09:30Z showed 5 / 11 / 2 on nats-0/1/2) | L1, L3 `varz` |
| Accounts: `$G`, `ONEMIND`, `SYS`. All application traffic is in **one account** `ONEMIND` | L3 `accountz`; O4 |
| Leafs: `zeus-mac` and `tak-nats-leaf-…` on nats-0, `hermes-legacy-leaf` on nats-1, all bound to `ONEMIND` | L3 `leafz` |
| Users are passwords in Helm values: `onemind` (app), `leaf_tak` (leafs), `sys`, `monitor`; Hermes' leaf config uses the `leaf_tak` credentials | O4 lines 39-68; O7 line 271 comment "leaf.conf w/ leaf_tak creds" |
| WebSocket listener 8080, exposed publicly at `ws.onemindos.dev` through ingress-nginx with the Cloudflare allowlist; no MQTT listener on the hub | L3 `varz` (`ws_port 8080`, `mqtt_port null`); O4 `ws-ingress.yaml` |
| JetStream totals: 38 streams, 2 consumers, 9,647,814 messages, 5.06 GB | L2 |
| **`COT` holds 9,535,611 messages / 4.95 GB with 0 consumers** (98% of all stored bytes) | L2 |
| `INTEL` 92,074 msgs / 104 MB / 2 consumers; `AGENTS` 20,118 msgs; `EVT` 9; `CMD` 2; every other stream and every KV bucket is empty (including `TEL`, `GEO`, `KV_world-state`, `KV_fleet-state`) | L2 |
| Only `WORLD` sets `maxBytes` (2 GB). No other stream has a byte cap; all are `retention: limits`, `discard: old`, `duplicateWindow: 2m`, R3 file, `preventDelete: true` | O3 summary run in this lane |
| Retention today: TEL/SYS/AGENTS 168h; COT/CMD/ENV/GEO/VOICE 720h; EVT/INTEL/MISSION/SAAS 2160h; ZEUS 8760h; WORLD 24h | O3 |
| Redpanda Connect: 4 pipelines, every one uses the **core `nats` output** to `tak.cot.inject` (no JetStream publish ack) | O6 lines 102-104, 211-213, 263-265, 325-327 |
| Nothing publishes `ent.*.*.state`; nothing writes `KV_world-state`/`KV_fleet-state`; the CoT path from bus into TAK is broken (inject stored in `COT`, read by no one) | O8 §§4-6 |
| Mac leaf config in Git: `tls { insecure: true }` on the remote, `no_auth_user`, WebSocket 8080 `no_tls` and MQTT 1883 with no `listen` host (so all interfaces) | O5; S9 (`DEFAULT_HOST = "0.0.0.0"`) |
| **[verifier]** The same Mac config enables JetStream (`jetstream: true` on account `OMOS`, 10 GB file store) with **no `domain`**. Per S4 a JetStream leaf without a domain does not suppress JetStream API traffic and can extend the hub's JetStream; the live effect was not checked (**unverified**) | O5; S4 lines 13-25 |
| Doctrine plans accounts `SYS / CORE / FABRIC / INTERFACE`, config-based now, JWT at ">~20 nodes or first non-family device class" | O2 §5 |

### 2. Core NATS (proved locally)
- Subject-based pub/sub, queue groups and request/reply on one connection. Cross-account
  request/reply through a service export/import works, and the responder receives a
  `Nats-Request-Info` header naming the caller's account (`{"acc":"CLIENT",…}`) (P6).
- Core NATS is at-most-once: a message with no live interest is dropped (P7d: 3 core
  publishes at a disconnected edge never reached the hub).

### 3. JetStream (proved locally on 2.15.2 unless marked)
- **Dedup:** two publishes with the same `Nats-Msg-Id` inside the 2 min window store one
  message (P1).
- **Max messages per subject:** 150 publishes to one subject with `--max-msgs-per-subject 100`
  keep exactly 100; other subjects are unaffected (P1). This is the "latest N per entity" tool.
- **Direct Get** of the last message for a subject, no consumer needed (P1, P10).
- **Per-message TTL** (`Nats-TTL`, ADR-43, server 2.11+): a 2 s message vanished from its
  stream after 3 s (P1). Minimum TTL is 1 s; unless `MaxMsgsPer` is 1, the stream's
  `SubjectDeleteMarkerTTL` is the floor and the server **rewrites** smaller TTLs (S1). Seen
  locally: a KV with a 1 min marker TTL stored `Nats-TTL: 60` for 2 s and 5 s requests (P2b).
- **Subject delete markers:** when TTL or MaxAge empties a subject, the server stores a marker
  (`Nats-Marker-Reason: MaxAge`, `Nats-Rollup: sub`, empty body) (S1, P10). The marker is
  delivered to a JetStream consumer (`--last-per-subject`), **not to plain core subscribers**
  (P10).
- **Republish:** a stream can re-emit stored messages on another subject with
  `Nats-Stream/Nats-Sequence/Nats-Subject` headers (P1). (The demo subject `rt.tel.>` is
  outside the grammar and is not recommended.)
- **Consumers:** durable pull consumer with explicit ack, subject filter and `max-deliver`;
  ack floor and pending counts advance as expected (P4). 2.15.0 sets a default limit of 1000
  consumers per stream (`default_max_consumers`) (S2).
- **Mirror:** an exact copy keeping upstream sequence numbers (TEL_MIRROR first/last 1/153,
  same as TEL) (P5, S6).
- **Source with subject transform:** a sourced aggregate renamed `tel.*.drone.position` to a
  new subject using `{{wildcard(1)}}` (P5). Sources interleave and renumber (S6). **[verifier]
  corrected:** S6 does not say mirrors cannot be transformed and filtered. It says that in a
  source or mirror config, `FilterSubject` and `SubjectTransforms` cannot be used together,
  and that each `SubjectTransforms` entry already acts as a filter (its source) with an
  optional destination. Cross-domain sourcing uses the `external` API prefix (S6 line 36). The
  quote that wrong export types "fail silently" is **not in S6** and is **unverified**.
- **2.14/2.15 changes that matter:** 2.14.8 and 2.15.1 fix stale per-message TTL entries; 2.15.0
  adds a desired-state metalayer, evacuate endpoints, backup/restore v2 and source-recreation
  detection; 2.15.2 and 2.14.9 (26 Oct 2026) are fix releases (S2). The live cluster is on
  2.14.2, before the TTL fix.

### 4. Key-Value (proved locally)
- History (5 revisions kept), optimistic concurrency (`kv update` with a stale revision fails
  with `err_code=10071 wrong last sequence`), and watch on a key wildcard (`air.>`) delivering
  current values then live updates (P2).
- Bucket TTL, per-key TTL (`Per-Key TTL Supported: true`) and limit markers (P2).
- **Gotcha, proved:** per-key TTL deletes only *that revision*. With `history > 1`, an older
  revision resurfaces: key `sea.mmsi-456` went back to its earlier value `x` once the TTL'd
  revision expired (P2c). Staleness expiry therefore needs `history: 1` (or
  `max_msgs_per_subject: 1`).

### 5. Object Store (proved locally)
- A 2.9 MiB file stored as 23 chunks with a SHA-256 digest, read back byte-identical; backed
  by stream `OBJ_<bucket>` on `$O.<bucket>.C.>` / `$O.<bucket>.M.>` (P3). Chunking is how
  blobs get past the 1 MiB live `max_payload` (L3).

### 6. Leaf nodes, domains and disconnected sites (proved locally)
- A leaf dials the hub; its local account (`LOCAL`) binds to the hub account chosen by the
  leaf's credentials (`FABRIC`); log: "JetStream using domains: local edge1, remote hub" (P7).
  The local account name need not match the hub's (live `zeus-mac` has local account `OMOS`,
  O5, and appears in hub account `ONEMIND`, L3).
- A leaf with JetStream needs its own domain, or it extends the hub's JetStream (S4).
- **Store-and-forward needs an edge stream.** With the hub down, 5 publishes went into the
  edge stream `SITE_EVT`; after reconnect the hub stream `HUB_SITE_EVT`, sourcing
  `$JS.edge1.API`, caught up to 8/8, then 10/10 on a second outage (P7, P7c).
- **Re-sync is not instant:** the leaf reconnected 2 s after the hub restarted, but the
  sourced stream needed **41 s** to catch up (P7c). Treat the exact delay as unverified
  (likely source-retry backoff).
- **Without an edge stream, nothing is buffered:** core publishes during the outage were lost
  (P7d). This contradicts doctrine §4's "the leaf node buffers during disconnection" unless an
  edge stream exists.
- **Overlap trap, proved:** while the leaf is up, a hub stream whose subjects overlap edge
  subjects (hub `TEL` on `tel.>`) captures edge publishes directly over the leaf (`last_seq`
  154 → 155, P7b). If the hub also sources the edge stream, every message lands twice.
  **[verifier]** S4 documents this: "if the same subject is subscribed to by different streams
  in the same account in different domains, as long as the underlying leaf node was connected
  at the time, each stream will store the message" (S4 line 29).
- Leaf remotes support `deny_imports` and `deny_exports` subject lists (S9), which is how a
  site keeps local-only subjects off the hub. Not exercised locally.

### 7. Security: accounts, permissions, exports/imports (proved locally, config mode)
- Account isolation: a `CLIENT` user subscribed to `tel.>` received nothing published in
  `FABRIC` (P6).
- Per-user permissions: a feeder allowed `intel.>`/`tel.>` was refused publishing
  `cmd.drone.sim-x1.land` with "Permissions Violation for Publish" (P6).
- Stream export/import (`FABRIC` exports `ent.>`, `CLIENT` imports it) and service
  export/import (`svc.entity.>`) both work (P6).
- **Decentralized JWT:** natscli 0.5.1 `nats auth operator add` created an operator with a
  `SYSTEM` account in an nsc-compatible store, and nats-server accepted an operator-mode
  config with a MEMORY resolver (`-t` valid). `nats auth account add` refused to run without
  a terminal, so account/user issuance and a scoped JWT login were **not** exercised (P9).
  Docs: the full NATS-based resolver is recommended, `nats auth` pushes account JWTs only to
  a full resolver, user JWTs are never pushed, and an edited account changes nothing until
  pushed (S8).

### 8. WebSocket and MQTT (proved locally)
- A `ws://` client (nats CLI, Go client) published into the same account and stream as TCP
  clients (P8). **[verifier] corrected source:** S7 (the WebSocket config page) lists no
  clients, so "six of seven Tier-1 clients have a WebSocket transport" is **unverified**. The
  C client gap is supported by S10 instead: the nats.c README never mentions WebSocket and
  `src/` has no WebSocket source file.
- MQTT 3.1.1: a raw-socket MQTT client (CONNECT and QoS 0 PUBLISH, `work/.run/nats-lane/mqtt_pub.py`)
  got CONNACK 0, and its topic `tel/sim-s1/soil/moisture` arrived on NATS as
  `tel.sim-s1.soil.moisture` with an `Nmqtt-Pub` header (P8). The server supports MQTT 3.1.1
  QoS 0/1/2 and needs a JetStream-enabled account (S3). **[verifier] corrected sources:** "QoS 2
  from 2.10" is not in S3; it comes from the v2.10.0 release notes ("Add support for QoS2
  exactly-once delivery", S2). S3 only says NATS follows MQTT 3.1.1. That **MQTT v5 is
  rejected** comes from S9 (`mqtt.go` refuses any protocol level other than 4 with "unacceptable
  protocol version").

### 9. Clients
| Client | Version | Facts | Source |
|---|---|---|---|
| nats.c (Unreal) | v3.14.0 | Apache-2.0; CMake; OpenSSL ≥ 1.1.1; JetStream, KV, Object Store (`object.c`) and micro services (`micro*.c`) in `src/`; README lists Linux, macOS, Windows only; **no WebSocket**; Android/iOS not listed | S10 (**[verifier]** README lines 7 and 1283-1286; S7 does not cover clients) |
| nats.go | v1.55.0 | Apache-2.0; full feature set; used by natscli (ws:// proved, P8) | S11 |
| nats.js | v3.4.0 | Apache-2.0; modular `@nats-io/*` (Core, JetStream, Kv, Obj, Services); browser WebSocket is in Core | S12 |
| nats.py | — | Apache-2.0; used by omos feeds and Hermes | S13 |

### 10. Synadia agent tooling, Orbit and NEX
- **Synadia Agent Protocol:** agents register as a NATS micro service named `agents`;
  subjects are `agents.{verb}.{token}.{owner}.{session}`, verbs `prompt`, `hb`, `status`.
  Caller SDKs `@synadia-ai/agents` / `synadia-ai-agents`, host SDKs `@synadia-ai/agent-service`
  / `synadia-ai-agent-service`; ready plugins for Claude Code, Codex, OpenClaw, PI and others;
  **Hermes is "work in progress"** upstream (S14). This matches the `agents.*` line of the
  locked grammar (O1).
- **Orbit** extension libraries exist for Go (counters, JetStream extensions, KV codecs,
  partitioned consumer groups `pcgroups`, system client) and JS (counters, fastingest,
  muxsub and others) (S15).
- **NEX** 0.5.0 (2026-09-03) is "the NATS Execution Engine"; "Nodes auction work to
  nexlets". **The repository has no LICENSE file** (GitHub `license=null`) (S16). omos has
  NEX-shaped services in `services/nex/overwatch/` (11 services), none deployed (O8 §5).

### 11. Comparators (licences and facts verified; capability comparison below is analysis)
| System | Verified | Source |
|---|---|---|
| Apache Kafka | Apache-2.0 | S19 |
| Redpanda (broker) | BSL; may not be offered as a third-party "Streaming or Queuing Service"; converts to Apache-2.0 after the change date | S18 |
| Redpanda Connect | Apache-2.0 for most connectors (NATS JetStream/KV files carry Apache headers), RCL for enterprise features | S17 |
| Zenoh | EPL-2.0 / Apache-2.0; key/value pub/sub, queries, router storage plugins; `rmw_zenoh` needs a Zenoh router for ROS 2 discovery | S19 |
| DDS | Fast DDS Apache-2.0 | S19 |
| MQTT | NATS has a built-in 3.1.1 gateway (P8) | S3 |

---

## How it applies to OneMind

### A. Which technology for which OneMind job

| OneMind job | Best fit | Why (evidence or reasoning) |
|---|---|---|
| Authority bus: events, telemetry, commands, TAK wire, agents | **NATS JetStream** (keep) | One server gives streams, KV, objects, request/reply, WebSocket and MQTT (P1–P8); already the omos authority (O1) |
| Latest state per entity (the "snapshot cache per layer" from the run prompt) | **JetStream, KV-shaped stream on `ent.*.*.state`** | 1 msg/subject + Direct Get + TTL + expiry markers proved on the locked subject (P10) |
| Blobs: homestead 3D Tiles bundles, models, data packages, imagery snapshots | **NATS Object Store** for site-replicated blobs; HTTP tile proxy for basemap tiles (run prompt rule 10b) | Chunked, digest-checked (P3); 1 MiB payload limit (L3) rules out plain messages |
| Edge sites (homestead, field kits) that disconnect | **NATS leaf + edge JetStream domain + hub sources** | Proved buffer and re-sync (P7) and proved loss without it (P7d) |
| ROS 2 robots | **Zenoh or DDS on the robot/LAN, one bridge to NATS** | ROS 2 speaks DDS natively and `rmw_zenoh` needs a router (S19); the robotics lane owns the bridge choice |
| Cheap sensors / Home Assistant | **NATS built-in MQTT 3.1.1** | Proved (P8); no extra broker; MQTT v5 devices need a bridge (S3) |
| Long-term analytics history (months/years) | JetStream → ClickHouse sink (omos already runs `sink-clickhouse`, O8) | Not Kafka: no new cluster on a memory-saturated pool (O1); Redpanda broker's BSL is a constraint for an NGO offering services to others (S18) |
| Browser (L0 CesiumJS), Quest WebXR | **nats.js over `wss://`** into a restricted CLIENT account | Browser WS in nats.js Core (S12); account isolation proved (P6) |
| Unreal client (Mac, Windows) | **nats.c over TCP/TLS** (`OneMindBus` plan in `ARCHITECTURE.md`) | Has JetStream/KV/Object/micro (S10); no WebSocket (S7) |
| Unreal on Quest standalone (Android) | nats.c **unverified** on Android; fallback WebXR + nats.js | Android not listed in nats.c README (S10) |
| Agents (Hermes, Claude Code, others) | **Synadia Agent Protocol** on `agents.*` | Matches locked grammar; Apache-2.0 SDKs (S14) |
| Ingestion engine | Decided by the ingestion lane (run prompt §10c). Whatever wins must publish with **JetStream acks** (`nats_jetstream` output) | Today's core `nats` output has no ack (O6) |
| Video | MediaMTX, never NATS | Run prompt rule 10b |

### B. Mapping onto omos

1. **Streams match the grammar mostly, with debt.** `cmd.>`, `intel.>`, `tak.cot.>`, the
   `evt.*` and `tel.*` filters and `agents.hb/status` follow O1. Outside it: `saas.>`,
   `voice.>`, `zeus.>`, `op.zeus.>`, `sys.>`, `agents.discover`, `agents.heartbeat` (O3).
   Live `intel` subjects carry extra tokens beyond `intel.{domain}.{source}.{type}`
   (for example `intel.weather.noaa.alert.{state}.{event}`, O8). These are recorded, not
   changed (decision 0004 #8).
2. **Grammar ambiguity.** O1 defines `tel.{node}.{class}.{metric}` with example
   `tel.drone.x47.position`, and stream `TEL` filters `tel.drone.>`, `tel.robot.>`, so the
   second token is used as a class, not a node. A `sim-x1` drone publishing
   `tel.sim-x1.drone.position` (the shape this run's PREFLIGHT used) is **not captured by
   live `TEL`**. This needs one owner ruling (proposal below).
3. **The entity layer is missing.** `ent.*.*.state` has no producer and `KV_world-state` /
   `KV_fleet-state` were never written (O8). The entity service is the first platform job
   (run prompt "Facts already verified").
4. **COT is the storage risk.** 4.95 GB, 0 consumers, 30 days, no byte cap (L2, O3). At R3
   that is about 15 GB of cluster disk for messages nobody reads. The JetStream PVC in the
   values template is 20 Gi (O4); live `varz` reports a 45 GiB JetStream max (L3).
   **[verifier] resolved:** `kubectl -n nats get pvc` shows `nats-js-nats-0/1/2` at **50Gi**
   each, class `do-block-storage`, age 116d. Git (`nats-statefulset.yaml` line 232 and the
   values template) still says 20Gi, so Git and live disagree. `varz` `max_storage` is
   48318382080 bytes, exactly 45 GiB. COT uses about 4.95 GB of the 45 GiB on each node.
5. **Leaf setup.** Three leafs share the `leaf_tak` user into `ONEMIND` (L3, O7). No site has
   an edge stream, so the Mac leaf does not store-and-forward today (P7d, O5).
6. **Redpanda Connect role.** It is a collector/transformer that today bypasses the entity
   layer and JetStream acks (O6). Its NATS connectors are Apache-2.0 (S17), so it can stay as
   one collector option while the ingestion lane decides.

---

## Risks

| # | Risk | Evidence | Mitigation (in Recommendation) |
|---|---|---|---|
| R1 | JetStream disk fills: only WORLD has `maxBytes`; COT 4.95 GB unread | L2, O3 | Byte caps on every stream, COT cut to 72 h |
| R2 | One account, shared passwords: any holder of `onemind` (including through public `ws.onemindos.dev`) can publish `cmd.>` and read everything | O4, L3 | CLIENT account with import-only reads; per-service users |
| R3 | `leaf_tak` reused by TAK leaf and Hermes | O7, L3 | One credential per leaf |
| R4 | Mac leaf: `tls insecure`, `no_auth_user`, WS/MQTT on all interfaces without auth (Git config; live Mac **unverified**) | O5 | Bind to 127.0.0.1, CA-verified TLS, leaf credentials |
| R5 | Doctrine assumes leaf buffering; core messages are lost during outages | P7d vs O2 §4 | Edge streams for anything that must survive |
| R6 | Duplicate capture when hub streams overlap sourced edge subjects | P7b | Source-only hub aggregates, `deny_exports` on leafs |
| R7 | Re-sync lag (41 s locally) after reconnect | P7c | Accept; show "site syncing" in UI from source lag |
| R8 | KV/TTL surprises: older revision resurfaces; TTL floor rewrite; core subscribers never see expiry markers | P2c, P2b, P10, S1 | `history 1`; clients use a JetStream ordered consumer for `ent` |
| R9 | Live server 2.14.2 predates the TTL cleanup fix (2.14.8) | S2, L3 | omos PR: 2.14.9 (fix-only line) before relying on TTL |
| R10 | 2m dedup window is too short for polled feeds that re-see an item for hours | O3, O8 poll intervals | `Nats-Msg-Id` = source id and a window matching the feed horizon, or idempotent entity upserts |
| R11 | NEX has no LICENSE file; doctrine §7 says "use as-is" | S16 | Do not ship NEX-run collectors until a licence exists |
| R12 | nats.c lacks WebSocket and lists no Android support | S7, S10 | TCP/TLS for Unreal; Quest fallback to WebXR + nats.js |
| R13 | MQTT v5 devices are rejected | S3 | Bridge v5 devices or require 3.1.1 |
| R14 | Grammar debt and the `tel` ambiguity break wildcard capture | O1, O3 | Owner proposal (decision 0004 #8) |
| R15 | 2.15 limits streams to 1000 consumers by default; consumer-per-client designs break | S2, O2 §3 | Clients use ordered ephemeral consumers or Direct Get, never durables per client |

---

## Recommendation: target bus design

**Keep NATS JetStream as the single authority bus.** Add an entity layer on `ent.*.*.state`,
put byte caps on every stream, split clients into their own account, and give each edge site
its own JetStream domain that the hub sources from. Everything below uses existing grammar
patterns only. Platform changes are omos PRs (NACK CRs in `platform/nats/streams/`), never
`nats stream add` on the cluster.

### 1. Streams vs KV vs Object Store

| Data | Primitive | Subject / bucket | Why |
|---|---|---|---|
| Observations, detections, intel, events, commands, raw TAK wire | **Stream** | existing `tel/det/intel/evt/cmd/tak.cot` | Replayable history, consumers, dedup |
| Latest state of every entity (aircraft, ship, drone, robot, TAK EUD, sensor) | **KV-shaped stream** `ENT` | `ent.{type}.{id}.state` | Keeps the locked subject (a KV bucket would move it to `$KV.*`); Direct Get for snapshot, ordered consumer `--last-per-subject` for snapshot plus live deltas, per-type TTL with expiry markers (P10) |
| Config, registries, schemas, mission state, presence | **KV** (existing buckets) | `agent-*`, `device-registry`, `node-registry`, `om-schema`, `mission-state`, `geofences`… | Low-rate, CAS updates (P2) |
| Homestead 3D Tiles and models, data packages, imagery stills, ML models | **Object Store** | buckets `tiles`, `models`, `datapkg`, `imagery` | Chunked and digest-checked (P3); site-replicable via mirror of `OBJ_*` |
| Basemap tiles, video | **not NATS** | tile proxy, MediaMTX | Run prompt rule 10b |

`KV_world-state` and `KV_fleet-state` are empty and unused (O8). Recommend replacing them with
`ENT` rather than writing to both. Removal is a later owner decision; `preventDelete` keeps
the data safe either way.

### 2. Retention per stream (proposed CR values; R3 file unless noted)

| Stream | Subjects | maxAge | maxBytes | Other | Change from today |
|---|---|---|---|---|---|
| `ENT` (new) | `ent.*.*.state` | none (TTL per message) | 1 GiB | `maxMsgsPerSubject: 1`, `allowDirect`, `allowMsgTtl`, `subjectDeleteMarkerTtl: 15m`; entity service sets `Nats-TTL` per type (aircraft 120 s, vessel 30 min, fleet asset 60 s, TAK EUD = CoT stale time) | new |
| `COT` | `tak.cot.>` | **72h** | **4 GiB** | dedup by CoT uid+time as `Nats-Msg-Id` | was 720h, uncapped, 4.95 GB unread |
| `TEL` | per owner ruling (below) | 72h | 4 GiB | `maxMsgsPerSubject: 2000`, `allowMsgTtl` | was 168h uncapped |
| `INTEL` | `intel.>` | 90d (keep) | 4 GiB | `Nats-Msg-Id` = source event id; `duplicateWindow` set to the longest feed repeat horizon (start at 1h) | add cap, longer dedup |
| `EVT`, `MISSION` | as today | 90d | 2 GiB each | — | add cap |
| `CMD` | `cmd.>` | **365d** | 512 MiB | audit trail of every command (run rule 6) | was 30d |
| `GEO` | `evt.tak.*` | 7d | 1 GiB | derived projection only (decision 0004 #7) | shorten |
| `AGENTS`, `SYS` | as today | 7d | 1 GiB | — | add cap |
| `ENV`, `VOICE`, `SAAS`, `ZEUS`, `WORLD` | as today | as today | 1-2 GiB each | — | add cap |
| `OBJ_tiles`, `OBJ_models`, `OBJ_datapkg`, `OBJ_imagery` | `$O.*` | none | 2-8 GiB each, sized after the disk check | R3 for `models`/`datapkg`, R1 acceptable for regenerable tiles | new |

All byte caps assume the per-node volume is confirmed first (R1, item B4). **[verifier]** It
is now confirmed: 50Gi PVCs with a 45 GiB JetStream limit per node. `discard: old` stays,
so a full stream drops its oldest data instead of refusing writes.

### 3. Edge-to-cloud replication

- Every site (homestead server, Mac, field kit) runs a **leaf with its own JetStream domain**
  (`site-<name>`), its own leaf credential, and local streams `SITE_TEL`, `SITE_EVT`,
  `SITE_DET` (72h, 1 GiB caps).
- The hub **sources** each site stream into its aggregates through `$JS.site-<name>.API`, with a
  subject filter for what the cloud needs (doctrine "consume/merge"). Hub aggregates that take
  site data are **source-only** for those subjects, and the site leaf remote sets
  `deny_exports` on the subjects it streams, so live core traffic cannot double-capture (P7b, S9).
  Low-latency views at the site come from local core subscriptions.
- Commands go hub → site over the leaf as core request/reply with ack, and are also recorded
  in the hub `CMD` stream. A site that is offline gets no command; the UI shows the site as
  disconnected rather than queueing commands for later replay (safety rule 6).
- The site **mirrors** hub `ENT` (filtered to its area) and the object buckets it needs,
  so the homestead keeps a working picture with the cloud gone.
- Show source lag per site in the UI; 41 s catch-up is normal (P7c).

### 4. Accounts and permissions

Stage 1, config mode (doctrine §5), through omos PRs with SealedSecrets:

| Account | Members | Allowed | Exports / imports |
|---|---|---|---|
| `SYS` | ops, monitor | `$SYS.>` | — |
| `ONEMIND` (data and services; keeps all streams, so no stream migration) | entity service, policy service, collectors (Redpanda Connect or successor, feed-intel), TAK leaf, Hermes, site leafs | **one user per service**, publish limited to its own subjects: collectors `intel.>`/`tel.>`/`det.>` only; entity service `ent.>`; only the policy service may publish `cmd.>`; TAK leaf `tak.cot.>`; each site leaf `tel.<site>…`/`evt.<site>…` per the ruling below | exports stream `ent.>`, `evt.>`, `intel.>`; exports services `svc.>` (queries, policy-gated command requests) and the JetStream API subjects needed for ordered consumers on `ENT` |
| `CLIENT` | OneMind Unreal, L0 web, Quest, ATAK plugin, behind `ws.onemindos.dev` | subscribe imported subjects; request `svc.*` | imports the exports above; **cannot publish `cmd.>`**; a command is a `svc.policy.*` request that passes policy, audit and human confirmation (run rule 6) |
| `AGENTS` (stage 2) | Hermes and other harnesses | `agents.>` plus imported `svc.*` | Agent tools reach the bus only through service imports |

Stage 2, operator/JWT mode with the full resolver: start when the first field kit or
non-family device joins (doctrine trigger). Use one scoped signing key per account role, so
a site or client can be revoked without touching others (S8). Prove it first with an
interactive `nats auth` session (P9 gap).

### 5. Owner proposals (decision 0004 #8; no subject is changed by this lane)
1. Rule on `tel.{node}.{class}.{metric}` vs `TEL`'s class-first filters (B2). Options:
   (a) keep class-first (`tel.drone.<id>.<metric>`) and treat `{node}` as the asset class,
   or (b) change `TEL` to `tel.>` and make the first token the node. Option (a) needs no
   stream change.
2. Ratify the extra tokens in live `intel.*` subjects, or plan a migration.
3. Record the out-of-grammar streams (`saas`, `voice`, `zeus`, `op.zeus`, `sys`,
   `agents.discover`, `agents.heartbeat`) as legacy.

### 6. First omos PRs, in order
1. Byte caps and COT retention (CR edits only; NACK applies; `preventDelete` stays).
2. `ENT` stream CR and the entity service that consumes `COT`/`INTEL` and publishes
   `ent.*.*.state` with `Nats-TTL`.
3. Collectors publish with JetStream acks and `Nats-Msg-Id` (ingestion lane picks the engine).
4. `CLIENT` account and per-service users (SealedSecrets); a separate credential for Hermes'
   leaf.
5. Server 2.14.2 → 2.14.9 (fix-only line, R9).
6. Site leaf template (`infra/edge/`) with domain, edge streams, `deny_exports`, CA-verified
   TLS and 127.0.0.1 listeners; owner-checklist step for the Mac.

---

## Unverified items

- ~~The real per-node JetStream volume size~~ **[verifier] resolved:** 50Gi PVCs (B4).
- **[verifier]** That wrong export types "fail silently" in cross-account sourcing (not in S6).
- **[verifier]** Which Tier-1 clients have a WebSocket transport (S7 has no client list).
- **[verifier]** What the Mac leaf's domain-less JetStream actually does on the live hub.
- Whether any client actually uses `ws.onemindos.dev` today.
- The live Mac leaf config (only the Git copy was read).
- nats.c on Android/Quest, and building nats.c inside Unreal (no Unreal here, PREFLIGHT).
- Decentralized JWT end to end (account/user issuance, scoped login, revocation): CLI needed
  a terminal (P9).
- Leaf remote `deny_exports` behaviour (confirmed in source, not exercised).
- Why source catch-up took 41 s (assumed retry backoff), and how it scales with backlog size.
- Cross-account sourcing (`external` with exports) was not run; only cross-domain was.
- Whether stream republish carries expiry markers.
- The ROS 2 default RMW and DDS behaviour over WAN: docs.ros.org returned an access-denied
  page, so DDS/Zenoh capability statements here come from the `rmw_zenoh` README only. The
  qualitative Kafka/Redpanda/Zenoh/DDS trade-offs in section A are analysis, not
  benchmarked.
- 2.11 and 2.12 release notes (the releases page shows only 2.14.x/2.15.x); features dated to
  2.11 come from ADR-43's tag.
- NEX's intended licence (no LICENSE file is present).

---

## Verifier report (independent, 2026-10-29)

I re-checked 26 load-bearing claims against primary sources fetched fresh: GitHub API
release metadata and repo licences, ADR-43, the nats.docs pages (MQTT, source/mirror,
JetStream leafnodes, WebSocket), the nats.c and synadia-agents READMEs, nats-server source at
v2.15.2 and `main`, the omos clone at 62e3d22b, and read-only `kubectl get` / `get --raw`
calls to the NATS monitoring port. No Secret was read.

**Confirmed (19):** live 2.14.2 / go1.26.3 / 1 MiB max_payload / WS 8080 / no MQTT; accounts
`$G, ONEMIND, SYS`; leafs `zeus-mac` + `tak-nats-leaf-…` on nats-0 and `hermes-legacy-leaf`
on nats-1, all on `ONEMIND`; 38 streams / 2 consumers, COT ≈9.54 M msgs / 4.95 GB / 0
consumers, INTEL 2 consumers; 14 Stream + 25 KeyValue CRs, only WORLD with `maxBytes`, every
retention value listed in §1; image `nats:2.14.2-alpine`; users `onemind/leaf_tak/sys/monitor`
and the Hermes `leaf_tak` comment (deployment.yaml line 271); `ws.onemindos.dev` with the
Cloudflare allowlist; 4 Redpanda Connect core `nats` outputs at the cited lines; Mac leaf
config; doctrine §4 "leaf node buffers" and §5 accounts and JWT trigger; ADR-43 (2.11 tag,
1 s minimum, floor plus rewrite unless `MaxMsgsPer` = 1, `MaxAge` marker reason); release
notes (2.14.8/2.15.1 stale-TTL fix #8595, 2.15.0 metalayer / evacuate / backup v2 /
source-recreation detection / 1000-consumer default, 2.15.2 and 2.14.9 on 2026-10-26); client
versions and dates (nats.c v3.14.0, nats.go v1.55.0, nats.js v3.4.0, natscli v0.5.1, nack
v0.25.0, all Apache-2.0); NEX 0.5.0 with `license=null` and no LICENSE file in the root
listing; synadia-agents (`agents` micro service, `agents.{verb}.{token}.{owner}.{session}`,
Hermes "work in progress"); leaf `deny_imports`/`deny_exports` exist in source; JetStream
domain rule (S4). The local proofs P1–P10 agree with `hub.conf`, `edge.conf`, `hubsrc.json`,
`mqtt_pub.py` and both logs (server 2.15.2, domains hub/edge1, the feeder publish violation
in the hub log, leaf reconnects about 2 s after each hub restart).

**Corrected (4):** mirror "transform and filter" wording (§3); MQTT QoS 2 and v5 sources (§8);
WebSocket client claim and the nats.c source (§8, §9); per-node volume (B4, 50Gi live vs 20Gi
in Git).

**Marked unverified (3):** "fail silently" quote; "six of seven Tier-1 clients have
WebSocket"; live effect of the Mac leaf's domain-less JetStream.

**Point-in-time (noted, not wrong):** connection count and message totals drift (live
re-read: 9,647,394 msgs, 5,058,134,415 bytes).

**Safety and licence checks:** the author's cluster calls were read-only (`get` and `get --raw`
proxy GETs to port 8222). No Secret values were read; the Hermes leaf credential is cited only
from a Git comment. No worldmonitor or worldwideview code appears. The lane's private servers
are stopped, and their data directories and the P9 nsc store are gone. The proofs redact the
passwords; plaintext throwaway localhost passwords remain in `work/.run/nats-lane/*.conf`,
which lie outside both repos. Minor rule deviation: the proofs wrote scratch files under
`/tmp/claude-501/` (outside `work/`); they no longer exist. **New observation, not acted
on:** the live NATS JetStream PVCs use `do-block-storage`, not the retain class (omos
CLAUDE.md item 6 applies to new claims only; changing the PV reclaim policy needs owner
approval).
