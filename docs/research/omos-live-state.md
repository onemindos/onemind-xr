# omos live state (research lane: omos-live)

Observed 2026-10-29 between 08:04 and 08:11 UTC with read-only commands only
(`kubectl get/describe/logs/top`, `kubectl get --raw` to the NATS and Redpanda
Connect monitoring ports). Context `do-nyc3-omos-cloud`. No Secret was read. No
`exec`. omos source checked at `/Users/danielcarku/Desktop/work/omos`, commit
`62e3d22b` (2026-10-28).

## Sources

Cluster (commands run from this machine):

- `kubectl get --raw /api/v1/namespaces/nats/pods/nats-{0,1,2}:8222/proxy/jsz?streams=1&config=1`: streams, sequences, subjects, replicas
- same proxy path with `connz?limit=1024&subs=1`, `varz`, `leafz?subs=1`: clients, per-client in/out message counts, subscriptions
- two `jsz` samples 60 s apart (08:04:50 and 08:05:50): stream growth rates
- `kubectl get pods -A -o wide`: maps client IPs to pods
- `kubectl get --raw /api/v1/namespaces/omos/pods/redpanda-connect-bbc8b54d5-gcj79:4195/proxy/{streams,metrics}`: Redpanda Connect running streams and counters
- `kubectl -n omos get deploy redpanda-connect -o jsonpath=...`, `get cm redpanda-connect-pipelines` (keys only)
- `kubectl logs` for `omos/{redpanda-connect,omos-tak-projector,sink-clickhouse,feed-satellites,feed-intel}`, `takserver/{tak-nats-leaf,takserver-plugins,takserver-messaging}`, `hermes/onemind-mcp`
- `kubectl get --raw /api/v1/namespaces/takserver/pods/takserver-nats-0:8222/proxy/{varz,connz?subs=1,leafz}`
- `kubectl top nodes`, `kubectl describe nodes`, `kubectl get nodes -L doks.digitalocean.com/node-pool`
- `kubectl -n argocd get applications.argoproj.io ...`

omos repository files:

- `platform/data/redpanda-connect/deployment.yaml` (pipelines, args)
- `platform/omos/{feeds.yaml,feed-scripts.yaml,intel-feeds.yaml,intel-scripts.yaml,tak-projector.yaml,sinks.yaml}`
- `platform/nats/streams/kv-world-state.yaml`, `kv-fleet-state.yaml`
- `platform/takserver/values-onemind.yaml`, `platform/takserver/deploy/{tak-nats-leaf.yaml,takserver-plugins-deployment.yaml}`
- `services/takserver/plugin/tak.server.plugins.NatsBridgePlugin.yaml`, `services/takserver/plugin-src/src/main/java/tak/server/plugins/NatsBridgePlugin.java`
- `.github/workflows/build-takserver.yml`
- `platform/hermes/mcp/apps.yaml`, `services/onemind-mcp/README.md`, `services/onemind-mcp/onemind_mcp/adapters/*.py`, `platform/hermes/agent/onemind_mcp_config.py`, `platform/hermes/agent/plugins/onemind-vision/__init__.py`
- `docs/STACK.md` (rows 133-134), `docs/NATS-DOCTRINE.md` §9.8, `docs/architecture/OneMind-OS-Architecture.md` (lines 315, 331)
- `git log -S'feed-mta.yaml"]' -- platform/data/redpanda-connect/deployment.yaml`

## Verified facts

### 1. NATS overview

- Server `nats-0`: nats-server 2.14.2, uptime 10d20h, 0 slow consumers, 1 account (`varz`).
- JetStream: 38 streams visible from nats-0 and nats-1, 39 from nats-2. The extra one is `KV_feed-config`, R1, 1 message, hosted only on nats-2 (`jsz` on nats-2). All others are R3.
- Total 9,647,845 messages and 5.06 GB stored. `COT` holds 9,535,642 messages and 4.95 GB of that (`jsz`).
- ~~Only 2 JetStream consumers exist, both on `INTEL`.~~ **Corrected [verifier 2026-10-29]:** nats-0 reports `consumers: 2`, but nats-2 reports `consumers: 9`: the 2 durables on `INTEL` plus 7 ephemeral consumers on the R1 `KV_feed-config` bucket hosted on nats-2 (KV watchers, one per `feed-intel` connection; `jsz?streams=1&consumers=1` on nats-2, 08:26 UTC). The `INTEL` durables are `sink-tak` (projector, `tak-projector.yaml` line 423) and `sink-clickhouse` (`intel-scripts.yaml` lines 246-249: `durable_name="sink-clickhouse"`, `deliver_subject="_INBOX.sink-ch"`, `filter_subject="intel.>"`). No consumer exists on `COT`.

### 2. Streams that hold data

| Stream | Subjects | Messages | Last write | Measured growth |
|---|---|---|---|---|
| `COT` | `tak.cot.>` | 9,535,642 (seq 26,821,196 to 36,356,837), `num_subjects: 1`, 30-day max age | 08:04:09 | +78 in 60 s (1.3 msg/s) |
| `INTEL` | `intel.>` | 92,074, 29,256 distinct subjects | 07:59:52 | 0 in 60 s |
| `AGENTS` | `agents.hb.>`, `agents.status.>`, `agents.discover`, `agents.heartbeat` | 20,118 | 08:03:59 | +2 in 60 s |
| `EVT` | `evt.so.>`, `evt.drone.>` etc. | 9 | 2026-10-09 | none |
| `CMD` | `cmd.>` | 2 | 2026-10-01 | none |

Every other stream and KV bucket holds 0 messages, including `TEL`, `GEO`
(`evt.tak.cot.>`), `WORLD` (`evt.world.>`), `MISSION`, `ENV`, `SYS` and all
`KV_*` buckets except `feed-config`.

### 3. What each feed publishes (subject, rate)

Publishers identified by matching `connz` client IPs to pod IPs. `in` is messages
the server received from the client over the connection's 10d20h lifetime.

| Feed (pod) | Source | Subject | Live rate (evidence) |
|---|---|---|---|
| Redpanda Connect, stream `feed-mta` only (`omos/redpanda-connect-bbc8b54d5-gcj79`) | MTA Metro-North and LIRR GTFS-RT, 30 s poll | `tak.cot.inject` (CoT XML) | `output_sent` 2,869,654 over 260h41m ≈ **3.1 msg/s average**. `connz in` 2,869,325 |
| `feed-satellites` (`omos/feed-satellites-6b4b8b4998-dbnhp`) | CelesTrak TLE group `stations` with SGP4, every 30 s | `tak.cot.inject` | log: `published 16/16 positions` every 30 s (0.53 msg/s). `connz in` 515,756 |
| `feed-intel` (one pod, 7 connections: noaa, usgs, acled, firms, gdacs, emsc, eonet) | public APIs; default polls: NOAA 300 s, USGS 60 s, ACLED 3600 s, FIRMS 600 s, GDACS 600 s, EMSC 120 s, EONET 300 s; config from KV `feed-config` | JetStream `intel.{domain}.{source}.…`, for example `intel.weather.noaa.alert.{state}.{event}`, `intel.seismic.usgs.{bucket}.{id}`, `intel.conflict.acled.{iso3}.{type}`, `intel.fire.nasa.firms.{region}`, `intel.disaster.gdacs.{type}.{id}`, `intel.seismic.emsc.{bucket}.{id}`, `intel.natural.nasa.eonet.{cat}.{id}`; plus core `tel.feed.{id}.status` every 30 s per feed | new events are sparse: logs 06:29–07:59 show 1–2 EMSC events every 2–12 min, one USGS event, GDACS about hourly. `INTEL` grew 0 in the 60 s sample. The status heartbeats explain most of `connz in` (about 31,700 per feed ≈ one per 30 s) |
| `omos-tak-projector` (`omos/omos-tak-projector-fc958b968-ldwft`) | consumes `intel.>` (durable `sink-tak`) and `ent.*.*.state` | `tak.cot.inject` | log heartbeat at 08:06: `ent=0 intel=3836` since its 2026-10-18 start. `connz in` 6,906 |
| `sink-clickhouse` | consumes `intel.>` | ClickHouse HTTP | log: `flushed batch=1 status=200` every few minutes |
| Hermes `hermes-legacy-leaf` | n/a | (publishes) | leaf connection on nats-1 (`leafz`, not `connz`): in 14,325 / out 556,615 at 08:27. **Corrected [verifier 2026-10-29]:** the leaf carries 33 interest subjects, not 5; besides `agents.prompt.hermes.zeus.legacy`, `tel.>`, `sys.>`, `svc.bridge.>`, `$SRV.*.agents` it includes `tak.cot.>`, `tak.cot.out`, `ent.>`, `intel.>`, `det.>`, `evt.>`, `cmd.alert.>` and `agents.hb.>` (`leafz?subs=1` on nats-1). So the hub forwards `tak.cot.*` CoT to the Hermes leaf; whether the agent processes it is **unverified** |

Connected but idle (0 in, 0 out): `mumble-bridge`, `agent-console` (`testui`),
both `onemind-core` pods, `onemind-ingestion`, `takml-vision-worker`, an
unnamed client subscribed to `tel.fleet.>`, `evt.world.>`, `evt.tak.cot.>`, and
the two leaf nodes (`zeus-mac` through `ts-omos-nats`, and `tak-nats-leaf`).

Redpanda Connect detail:

- The ConfigMap `redpanda-connect-pipelines` contains four pipelines (`feed-aircraft.yaml`, `feed-hazards.yaml`, `feed-mta.yaml`, `feed-ships.yaml`), all with output subject `tak.cot.inject` (`deployment.yaml` lines 104, 213, 265, 327).
- The live container args are `["streams","--chilled","/config/pipelines/feed-mta.yaml"]`, the same as Git (line 528; corrected [verifier 2026-10-29]). The streams API returns only `{"feed-mta":{"active":true,...}}`.
- Commit `129725ab` (2026-08-27): "fix(redpanda-connect): run only feed-mta pipeline (others have broken bloblang)".
- `output_error` is 0. 12 processor errors from MTA HTTP timeouts on 2026-10-19 and 2026-10-20 (logs).

### 4. Claim check: "Redpanda Connect pushes every aircraft, ship and transit item as CoT straight to `tak.cot.inject`, and nothing publishes `ent.*.state`"

- **Part 1, partly refuted.** The design is as stated: all four pipelines target `tak.cot.inject`. But live, only the transit pipeline (`feed-mta`) runs. **Aircraft (OpenSky) and ships (aisstream.io) are not running**, and have not been since 2026-08-27 (commit above, streams API, ConfigMap). `feed-satellites` and `omos-tak-projector` also publish CoT directly to `tak.cot.inject`, outside Redpanda.
- **Part 2, confirmed.** No client publishes `ent.*.*.state`. The projector, which subscribes to it, counts `ent=0`. Its 3,453 delivered messages (`connz out`) are `intel.>` deliveries: `sink-clickhouse` shows the same 3,453 and subscribes to the same `intel.>` consumer stream. No omos source file writes `ent.` state. The projector docstring calls the intended producer "ORP fused entity states". No pod matching ORP, overwatch, fusion or entity runs (`pods.txt` grep).

### 5. Who writes `kv-world-state` and `kv-fleet-state`

- **Nothing.** `KV_world-state` (TTL 5 m, history 1) and `KV_fleet-state` (TTL 30 s, history 3) both show `first_seq 0, last_seq 0` since they were created on 2026-08-04. A sequence of 0 means the bucket has never accepted a write. TTL expiry would leave `last_seq` above 0.
- No code under `services/` or `platform/` (excluding the generated portal catalog) references either bucket. (Verifier: `grep -rln 'world-state|fleet-state'` also hits `services/nex/overwatch/*`, but only as the prose term "world-state" for the `OVERWATCH_STATE` bucket, not these buckets; claim holds.) They appear only in their CRs and in the architecture table (`OneMind-OS-Architecture.md` lines 315 and 331: "Latest known state for every fleet asset" and "Latest world state snapshot (AIS, ADS-B, etc.)").
- The related `KV_OVERWATCH_STATE` has a writer in source (`services/nex/overwatch/om-overwatch-state/main.go`). That service is not deployed, and the bucket is empty.

### 6. TAK Server and NatsBridgePlugin health

- All TAK pods are Running: api, config, messaging, plugins, ignite, postgresql, takserver-nats, tak-nats-leaf, fedhub, mumble, murmur, ldap, lb-proxy. Postgres backups and `cot-retention` CronJobs completed. Argo `platform-takserver` is Synced/Healthy. There are no Warning events.
- `takserver-messaging` logs an `OpenSslHandshakeException` every 30 s, likely a TCP probe or LB health check on a TLS port (cause **unverified**).
- **The CoT path from the main bus into TAK is broken.**
  - The plugin config targets `natsUrl: nats://tak-nats-leaf.takserver.svc.cluster.local:4222`, consuming `tak.cot.inject` and producing `tak.cot.out` (`services/takserver/plugin/tak.server.plugins.NatsBridgePlugin.yaml`).
  - On the hub, the `tak-nats-leaf` leaf connection has `in_msgs 0, out_msgs 0` and only 5 subscriptions, all `$SYS.*`/`$LDS.*` (`leafz`, `connz`). So no client behind that leaf subscribes to `tak.cot.inject`, and the hub forwards nothing to it.
  - `tak-nats-leaf` logs one leafnode connection, to the hub service `nats` (10.108.54.164:7422). Its own monitor port could not be reached through the API proxy (`dial tcp 10.109.3.78:8222: i/o timeout`).
  - TAK's internal NATS `takserver-nats-0` (v2.9.21) has `leafnodes 0` and `in_msgs 1, out_msgs 1` over 3 days. Its clients are messaging, plugins (two connections) and api, all with 0 messages.
  - `COT` has `num_subjects: 1`. All three publishers above use `tak.cot.inject`, so that single subject is `tak.cot.inject`. Within the 30-day retention, nothing has published `tak.cot.out`, which means no EUD CoT is leaving TAK.
  - In short, the about 3.6 msg/s sent to `tak.cot.inject` is stored in JetStream `COT`, and no consumer reads it into TAK. (Verifier: the only live subscriber is the Hermes leaf via `tak.cot.>`, see section 3. The `tak-nats-leaf` pod restarted 2026-10-27 22:41 UTC per its log, so its zero leaf counters cover about 34 h, not the full 30-day window.)
- **The NatsBridgePlugin is probably not in the running image.** `values-onemind.yaml` line 54 pins the plugins tier to `takserver-base:plugins-upstream-5.8-onemind`, while its comment says "Plugin is baked into this image". `build-takserver.yml` lines 15-17, 112-140 define the `plugins-upstream` tag as the build without the NatsBridgePlugin overlay. The live plugins Deployment is `platform/takserver/deploy/takserver-plugins-deployment.yaml`, Argo-managed, with digest `sha256:f4824fba…`. I could not map that digest to a tag (`gh api` needs the `read:packages` scope). The `takserver-plugins` stdout contains no `NatsBridge` or `nats` line in its 379 lines since 2026-10-18. It shows only Java NATS client `Connection refused` retries at 2026-10-26 07:52, when takserver-nats restarted. The plugin application log goes to a file in the container, which I did not read (no exec).

### 7. Hermes tools

- Pods: `hermes-legacy` (3/3, on the ecosystem node; containers `hermes` = `nousresearch/hermes-agent:v2026.9.24`, which is a tag, not a digest; `nats-leaf` = `nats:2.10-alpine`; `paperclip-api-tls`), `hermes-fresh` (1/1, hub-core) and `onemind-mcp` (1/1).
- MCP server `onemind` at `http://onemind-mcp.hermes.svc.cluster.local:8000/mcp`, wired into Hermes config by `platform/hermes/agent/onemind_mcp_config.py`.
- Tools: `apps_list`, `apps_describe`, NATS `nats_server_stats` / `nats_jetstream_stats` (monitoring only; no message read or publish), `prometheus_query`, and read adapters for STAC, Ollama, Alertmanager, Loki and OpenCost. **Corrected [verifier 2026-10-29]:** Tempo is not an available read tool: the README lists it among the credential-gated adapters ("bookmarks, workflows, Paperclip, Grafana, Tempo and MediaMTX", `services/onemind-mcp/README.md` line 12) and `platform/hermes/mcp/apps.yaml` lines 68-73 set `tempo` to `enabled: false` ("confirm trace service endpoint ... before enabling"). Karakeep (bookmarks), n8n (workflows), Paperclip (including the write `create_issue`), Grafana, Tempo and MediaMTX are credential-gated or disabled (`README.md` lines 8-24, `adapters/applications.py`).
- `apps.yaml` marks MediaMTX disabled ("HTTP 401") and lists tak, fedhub, webodm, tiles, tile38, cloudtak, takml, home_assistant, litellm, onemind_core, agent_fabric, kubernetes and others as blocked, each with a stated reason.
- In-pod plugin `onemind-vision` registers `vision_vision_query`, `vision_analyze_feed`, `vision_sitrep`, `vision_anomaly_check` and `vision_recommend`.
- Bus-side tools: Hermes subscribes to `svc.bridge.>` and serves a NATS micro service `agents` (`$SRV.INFO.agents`).
- No `tool_call` lines appear in the last 500 `onemind-mcp` log lines (startup only, 33 h ago).

### 8. Node capacity per pool (`kubectl describe nodes`, `kubectl top nodes`)

| Node | Pool | Allocatable | Requests (CPU / mem) | Limits (CPU / mem) | Actual usage |
|---|---|---|---|---|---|
| onemind-ecosystem-cpu-3rhwrp | onemind-ecosystem-cpu (1 node) | 7880m / 28.1 GiB | 5794m (73%) / 23417 Mi (81%) | 977% / 290% | 1058m (13%) / 15435 Mi (53%) |
| onemind-hub-core-v2-3rtf47 | onemind-hub-core-v2 | 7880m / 13.3 GiB | 4289m (54%) / 10054 Mi (73%) | 373% / 301% | 1921m / 7873 Mi (57%) |
| onemind-hub-core-v2-3rtf4m | onemind-hub-core-v2 | 7880m / 13.3 GiB | 3437m (43%) / 10384 Mi (76%) | 307% / 272% | 2472m / 7475 Mi (54%) |
| onemind-hub-core-v2-3rtf4q | onemind-hub-core-v2 | 7880m / 13.3 GiB | 5467m (69%) / 9756 Mi (71%) | 268% / 186% | 735m / 9290 Mi (68%) |

Schedulable headroom by memory requests: ecosystem about 5.3 GiB, hub-core
about 3.3–3.8 GiB per node. The ecosystem pool autoscales from 1 to 4 nodes
(omos `CLAUDE.md`). A new node costs money, and decision 0004 #5 forbids that
here.

## How it applies to OneMind

- **The entity layer the client needs does not exist live.** There is no `ent.*.state` producer and nothing in `world-state` or `fleet-state`. The client cannot read a latest-snapshot cache from the cluster today. The entity service (decision 0004 #7: derive per-UID state from `tak.cot.*`) is greenfield, not a migration.
- **The live, high-volume picture data is MTA trains (about 3 msg/s) plus 16 ISS-group satellites, as CoT XML on `tak.cot.inject`.** It is retained 30 days in `COT`. An entity service can bootstrap from `COT` by parsing CoT `uid`/`type`/`point` and writing `ent.{type}.{id}.state` plus a KV snapshot. Aircraft and ship density is not there to test against until those feeds run again.
- **`INTEL` is the richest structured source** (29k subjects, about 92k events over 86 days). It is already on the locked `intel.{domain}.{source}.{type}` grammar and directly usable for an intel layer.
- **TAK is not receiving the feeds.** "TAK as display" is broken today, so client features must not assume TAK Server shows bus data. The bus is the authority for the client; TAK interop must be proven separately (owner checklist).
- **Hermes reaches the bus through `svc.bridge.>`, `agents.prompt.…` and the `onemind` MCP (monitoring/metrics only).** No Hermes tool reads entity state, because none exists yet.
- New cluster workloads must fit about 5 GiB of free memory requests on the ecosystem node.

## Risks

1. **Silent sink.** About 3.6 msg/s goes into `tak.cot.inject` with no JetStream consumer and no TAK delivery. (Corrected [verifier 2026-10-29]: it is not entirely unread; the Hermes leaf holds a core-NATS `tak.cot.>` interest, so the hub forwards the stream to the Hermes pod (`leafz?subs=1` on nats-1).) `COT` stores about 5 GB at R3 (about 15 GB of disk across nodes), out of 5.06 GB used of JetStream. That is storage cost with no display benefit.
2. **Intended producers are not running.** `world-state`/`fleet-state`, `GEO`, `WORLD` and `TEL` were designed but have never been written. Docs (`OneMind-OS-Architecture.md`) read as if they were live.
3. **Plugin image mismatch.** The values file comment claims the NatsBridgePlugin is baked in, but the tag is `plugins-upstream`. A fix may need an image change on a tier that is partly Helm-CLI-managed (omos `CLAUDE.md`: the `takserver` release is hand-applied, while `takserver-plugins` now lives in the Argo overlay).
4. **Redpanda pipelines for aircraft and ships are broken** ("broken bloblang", 2026-08-27). Reviving them as-is would also put global AIS (bounding box `[-90,-180],[90,180]`) at full rate straight into CoT with no entity layer or rate limit.
5. **JetStream "insufficient resources" (10023) appeared 3,030 times on 2026-10-18 11:27–12:39** while `feed-intel` recreated consumers after a restart. It has not recurred since. Cause **unverified**; `reserved_storage` is 2.0 GB on nats-0.
6. **Memory pressure.** Hub-core requests are 71–76% and the ecosystem node is at 81%. Limits are overcommitted (186–301%).
7. Hermes runs a tag-pinned (not digest-pinned) image.

## Recommendation

1. **Build the entity service first (omos PR, ecosystem pool).** Consume `tak.cot.inject`/`tak.cot.>` (and later the raw feeds) and publish `ent.{entity_type}.{entity_id}.state`. Write a per-entity snapshot into the existing `world-state` bucket (TTL 5 m fits moving tracks). This fills two empty, already-provisioned slots and needs no new subject pattern. The existing `omos-tak-projector` already subscribes to `ent.*.*.state`, so TAK gets entities automatically once the TAK path works.
2. **Open an omos issue or PR (owner merges) for the TAK path.** Either the plugins tier runs the `plugins-5.8-onemind` overlay image, or the plugin is confirmed present (file log) and `tak-nats-leaf` routing is fixed. The acceptance test is hub `leafz` showing a `tak.cot.inject` subscription from the TAK leaf and `COT` gaining a second subject `tak.cot.out`. Do not change it from this run (read-only).
3. **For client development, mirror locally.** Point the local nats-server at captured `COT` and `INTEL` samples, or the scripted drone, per decision 0004 #11/#12. Do not wait on the cluster. Do not treat Redpanda aircraft or ships as available.
4. Leave Redpanda aircraft and ships off until the ingestion lane's pipeline choice (decision 0004 #18) can add per-source rate limits and an entity layer.

## Unverified items

- Whether the live `takserver-plugins` digest `sha256:f4824fba…` contains `NatsBridgePlugin`. The tag points to `plugins-upstream`, and the bus shows no plugin traffic. Proof needs the GHCR `read:packages` scope or the in-container plugin log (exec not permitted in this lane).
- What `tak-nats-leaf`'s `leaf.conf` routes (it is a SealedSecret; not read by rule) and why its monitor port 8222 times out through the API proxy (probably a NetworkPolicy; not checked).
- The exact single subject in `COT`. It is inferred as `tak.cot.inject` from `num_subjects: 1` and the three known publishers. A subject listing would need a NATS user (`$JS.API.STREAM.INFO` with `subjects_filter`).
- The cause of the 30 s `OpenSslHandshakeException` on `takserver-messaging`.
- The cause of the 2026-10-18 JetStream 10023 errors.
- Per-source event rates for intel beyond the log window shown. Rates here are a 60 s sample, lifetime counters and about 90 min of logs.
- Hermes native (non-MCP) toolsets. These live in the agent runtime config on its PVC, not in Git.
- The MTA instantaneous rate varies with the service schedule. 1.3 msg/s was sampled at about 04:05 New York time; the 3.1 msg/s lifetime average is the better planning figure.

No tool calls were denied by the permission system during this lane.

## Independent verification (verifier 2026-10-29)

Re-checked 16 claims against the omos clone (commit `62e3d22b`, 2026-10-28 11:53 -0400) and read-only cluster calls (`kubectl get --raw` monitoring proxies, `get`, `logs`, `top`, `describe nodes`; no Secret, no exec, no mutation) at 08:20-08:28 UTC.

Confirmed: omos commit; COT stats (9,535,288 msgs, 4.95 GB, `num_subjects` 1, R3, 30-day max age); `KV_world-state`/`KV_fleet-state` `last_seq 0` with TTL 5 m/30 s and history 1/3; `KV_feed-config` R1 only on nats-2; Redpanda live args and streams API (`feed-mta` only) and four ConfigMap keys; commit `129725ab`; `tak-nats-leaf` hub leaf in 0/out 0 with 5 `$SYS`/`$LDS` subs; TAK NATS 2.9.21, `leafnodes 0`, in/out 1; plugin YAML (`natsUrl`, `tak.cot.inject`, `tak.cot.out`); `values-onemind.yaml` line 54 `plugins-upstream-5.8-onemind` vs `build-takserver.yml` overlay tag `plugins-5.8-onemind`; live plugins digest `sha256:f4824fba...` matches Git; no `NatsBridge` line in plugins stdout; projector `ent=0`; satellites `16/16` every 30 s; Hermes `v2026.9.24` tag-only image; node requests/limits table; vision tool names.

Corrected in place: total consumer count (9, not 2); Hermes leaf subscriptions (includes `tak.cot.>`, so CoT is forwarded to Hermes); Risk 1 wording; Tempo MCP tool (disabled, not available); Redpanda args line number (528).

No licence-rule or cluster-safety violation found: every cluster call the author lists is a read (GET via API proxy, `logs`, `top`, `describe`), ConfigMap keys only, the `leaf.conf` SealedSecret was not read, and no worldmonitor/worldwideview code is present.
