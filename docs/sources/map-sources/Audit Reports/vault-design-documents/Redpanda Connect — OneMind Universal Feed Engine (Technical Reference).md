---
id: "notion-3c0d0cf70a7e817b857dcf449e2ddf69"
title: "Redpanda Connect — OneMind Universal Feed Engine (Technical Reference)"
type: "reference"
status: "active"
created: "2026-10-16"
updated: "2026-10-16"
owner: "zeus"
tags: ["type/reference", "domain/so", "collection/so", "kind/knowledge"]
collection: "so"
kind: "knowledge"
notion_database: "Sovereign Operations "
notion_status: "Not started"
notion_area_2: "🌐 Fabric"
notion_category: "Integrations"
notion_id: "3c0d0cf70a7e817b857dcf449e2ddf69"
notion_url: "https://app.notion.com/p/Redpanda-Connect-OneMind-Universal-Feed-Engine-Technical-Reference-3c0d0cf70a7e817b857dcf449e2ddf69"
notion_created: "2026-08-18"
notion_last_edited: "2026-09-14T03:38:00.000Z"
imported: "2026-10-16"
---

# Redpanda Connect — OneMind Universal Feed Engine (Technical Reference)

# Redpanda Connect (Benthos) — OneMind Universal Feed Engine
## Complete Technical Reference & Dynamic Tasking Architecture
**Author:** Zeus Delacruz / Legacy (Mother)
**Date:** August 5, 2026
**Version:** 1.0
**Purpose:** Definitive technical reference for Redpanda Connect as the OneMind feed standard
---
## 1. What Is Redpanda Connect?
Redpanda Connect (formerly Benthos) is a **declarative stream processor** — a single compiled Go binary that moves data between systems with transforms in the middle.
**Key facts:**
- **Single binary:** ~50MB, no runtime dependencies, no JVM, no Python, no Node
- **YAML-only config:** No code required for 90% of pipelines
- **200+ native components:** Inputs, processors, outputs — all compiled in, ready to use
- **Streams mode:** One binary runs N independent pipelines simultaneously
- **REST API:** Add/remove/update pipelines at runtime without restart
- **Docker image:** `redpandadata/connect:latest` (updated daily, multi-arch)
### What "Native" Means
When we say a component is **native**, we mean:
> It is part of the compiled binary. You write its name in YAML and it works.
> There is NOTHING to install, download, or configure beyond the YAML itself.
> No pip install. No npm. No plugins. No Go modules. No compilation.
> The binary ships with every component already inside it.
This is fundamentally different from Python/Node ecosystems where every capability requires installing packages. Redpanda Connect's approach: **everything is already there.**
---
## 2. Native Components We Use
### INPUTS (Sources — how data enters the pipeline)
| Component | Protocol | Our Use Cases | Polling/Streaming |
| `http_client` | HTTP/HTTPS REST | MTA, USGS, NOAA, ACLED, OpenSky, GDELT, FIRMS, cameras | Polling (configurable timer) |
| `websocket` | WebSocket | AIS maritime (aisstream.io), live news streams | Streaming (persistent connection) |
| `mqtt` | MQTT 3.1.1/5.0 | Meshtastic mesh radio, IoT sensors, Home Assistant | Streaming (subscribe to topics) |
| `socket` | Raw TCP/UDP | APRS-IS ham radio firehose | Streaming (persistent connection) |
| `http_server` | HTTP webhook receiver | Any service that can POST to us | Push (listens for incoming) |
| `generate` | Internal timer | Heartbeats, scheduled synthetic events | Polling (cron-like) |
| `nats_jetstream` | NATS JetStream | Cross-pipeline chaining, replay | Streaming (durable consumer) |
| `nats_kv` | NATS Key-Value | Watch for entity changes | Streaming (watch) |
### PROCESSORS (Transforms — what happens to data in transit)
| Component | What It Does | Our Use Cases |
| `protobuf` | Decode/encode Protocol Buffers binary ↔ JSON | MTA GTFS-RT train positions |
| `mapping` | Transform fields using Bloblang DSL | Normalize any source to common schema |
| `xml` | Parse/create XML documents | CoT event generation, RSS feeds, NOAA alerts |
| `json_schema` | Validate against JSON Schema | Ensure data quality before hitting bus |
| `dedupe` | Content-hash deduplication | Same event from multiple sources |
| `rate_limit` | Throttle throughput | Respect API rate limits (OpenSky: 1 req/10s) |
| `branch` | Conditional processing paths | Military vs civilian aircraft handling |
| `switch` | If/else routing | Route by source type or priority |
| `cache` | Read/write mid-pipeline caches | Delta filtering (has position changed?) |
| `http` | Make HTTP calls mid-pipeline | Enrich with external lookups (owner DB, etc.) |
| `log` | Debug logging | Development and troubleshooting |
| `javascript` | Run JS code inline | Complex transforms that exceed Bloblang |
| `jq` | JQ expressions | JSON filtering for complex nested structures |
| `command` | Run external binary | Shell-out for edge cases |
### OUTPUTS (Destinations — where processed data goes)
| Component | Protocol | Our Use Cases |
| `nats_jetstream` | NATS JetStream | **PRIMARY** — all feeds → JetStream streams |
| `nats` | NATS core | Lightweight fire-and-forget publish |
| `nats_kv` | NATS Key-Value | Update entity store, pattern cache |
| `broker` | Fan-out to multiple | Same message → JetStream + KV + metrics |
| `switch` | Conditional routing | Route military aircraft to special stream |
| `http_client` | HTTP POST | Webhook notifications, external APIs |
| `drop` | /dev/null | Intentionally discard (filtered events) |
### CACHES (Shared state between processors)
| Component | Backing Store | Our Use Cases |
| `memory` | In-process RAM | Delta filtering (last position per entity) |
| `nats_kv` | NATS KV bucket | Shared entity state across pipelines |
| `redis` | Redis server | If we need persistence beyond NATS KV |
### RATE LIMITS (Throttling)
| Component | How It Works | Our Use Cases |
| `local` | Token bucket per pipeline | Respect OpenSky 1req/10s, MTA 1req/30s |
---
## 3. Pipeline Anatomy
Every pipeline follows this structure:
```yaml
# metadata
label: "feed-name"

input:
  # WHERE data comes from
  http_client:
    url: "https://api.example.com/data"
    verb: GET
    rate_limit: "my_limit"

pipeline:
  processors:
    # WHAT happens to the data (in order)
    - protobuf:
        operator: to_json
        message: "transit_realtime.FeedMessage"
        import_paths: ["/proto/"]
    - mapping: |
        root.uid = this.vehicle.trip.trip_id
        root.lat = this.vehicle.position.latitude
        root.lon = this.vehicle.position.longitude
    - dedupe:
        cache: delta_cache
        key: "${! this.uid + this.lat + this.lon}"

output:
  # WHERE data goes
  nats_jetstream:
    urls: ["nats://nats:4222"]
    subject: "intel.raw.transit"

cache_resources:
  - label: delta_cache
    memory:
      default_ttl: "120s"

rate_limit_resources:
  - label: my_limit
    local:
      count: 1
      interval: "30s"
```
---
## 4. Streams Mode — One Binary, Many Pipelines
In **streams mode**, Redpanda Connect runs multiple independent pipelines from a directory of YAML files:
```bash
rpk-connect run --streams /config/pipelines/*.yaml
```
Each YAML file is an independent pipeline. They share nothing except:
- Cache resources (if configured globally)
- Rate limit resources (if configured globally)
- The NATS connection (efficient multiplexing)
**Adding a new feed = dropping a new YAML file in the directory.**
---
## 5. The REST API — Dynamic Pipeline Management
When running in streams mode with the API enabled:
```yaml
# Main config enables API
http:
  enabled: true
  address: "0.0.0.0:4195"
```
### Available Endpoints
| Method | Path | What It Does |
| GET | `/streams` | List all running pipelines |
| GET | `/streams/{id}` | Get config of specific pipeline |
| POST | `/streams/{id}` | **Create** a new pipeline (hot, no restart) |
| PUT | `/streams/{id}` | **Update** an existing pipeline |
| DELETE | `/streams/{id}` | **Stop and remove** a pipeline |
| GET | `/ready` | Health check |
| GET | `/metrics` | Prometheus metrics |
---
## 6. Dynamic Tasking — AI Creates Collection Pipelines
This is the killer feature. Legacy AI can create and destroy collection pipelines **on demand** via the REST API, triggered by TAK commands.
### The Flow
```plain text
┌──────────────────────────────────────────────────────────────────────┐
│                                                                       │
│  STEP 1: Zeus draws a marker in ATAK                                 │
│                                                                       │
│    Marker: "NAI-WAREHOUSE"                                           │
│    Type: a-n-G (Named Area of Interest)                              │
│    Remarks: "COLLECT - ADS-B - 5km - 72hr"                          │
│    Location: 40.7128, -74.0060                                       │
│                                                                       │
│  STEP 2: CoT flows through the bus                                   │
│                                                                       │
│    TAK Server → cot-tap → NATS tak.cot.out                          │
│    Legacy subscribes to tak.cot.out                                   │
│    Legacy parses: "Zeus wants ADS-B collection at 40.71,-74.00"      │
│                                                                       │
│  STEP 3: Legacy creates a pipeline via REST API                      │
│                                                                       │
│    POST http://redpanda-connect:4195/streams/tasking-adsb-nai-wh     │
│                                                                       │
│    Body (YAML):                                                       │
│    ┌──────────────────────────────────────────────────────────────┐  │
│    │ input:                                                        │  │
│    │   http_client:                                                │  │
│    │     url: "https://api.adsb.exchange/v2/lat/40.71/lon/-74.00/ │  │
│    │           dist/5/"                                            │  │
│    │     headers:                                                  │  │
│    │       api-auth: "${ADSB_KEY}"                                 │  │
│    │ 
```
```plain text
    rate_limit: "adsb_tasking"                                │  │
│    │                                                               │  │
│    │ pipeline:                                                     │  │
│    │   processors:                                                 │  │
│    │     - mapping: |                                              │  │
│    │         root.entity_id = this.hex                             │  │
│    │         root.callsign = this.flight.trim()                    │  │
│    │         root.lat = this.lat                                   │  │
│    │         root.lon = this.lon                                   │  │
│    │         root.alt = this.alt_baro                              │  │
│    │         root.military = this.mil                              │  │
│    │         root.nai = "NAI-WAREHOUSE"                            │  │
│    │         root.tasking_id = "tasking-adsb-nai-wh"              │  │
│    │         root.timestamp = now()                                │  │
│    │                                                               │  │
│    │ output:                                                       │  │
│    │   nats_jetstream:                                             │  │
│    │     urls: ["nats://nats:4222"]                                │  │
│    │     subject: "intel.tasking.collection.adsb"                  │  │
│    │                                                               │  │
│    │ rate_limit_resources:                                         │  │
│    │   - label: adsb_tasking                                       │  │
│    │     local:                                                    │  │
│    │       count: 1                                                │  │
│    │       interval: "5s"                                          │  │
│    └──────────────────────────────────────────────────────────────┘  │
│                                                                       │
│
```
```plain text
    Pipeline starts collecting IMMEDIATELY.                            │
│                                                                       │
│  STEP 4: Legacy monitors and correlates                              │
│                                                                       │
│    Subscribes to intel.tasking.collection.adsb                       │
│    Sees same Cessna N12345 at 0200 three nights in a row             │
│    Queries ClickHouse for historical pattern                         │
│    Generates intel product CoT → pushes to TAK                       │
│                                                                       │
│  STEP 5: Tasking expires (72hr TTL)                                  │
│                                                                       │
│    Legacy timer fires after 72hr                                     │
│    DELETE http://redpanda-connect:4195/streams/tasking-adsb-nai-wh   │
│    Collection stops cleanly. No orphaned processes.                   │
│                                                                       │
│  STEP 6: Zeus sees the product in ATAK                               │
│                                                                       │
│    New marker linked to NAI-WAREHOUSE:                               │
│    "PATTERN: Cessna N12345 (XYZ LLC) — 0200 circling x3 nights"    │
│    Zeus drops new marker: "EXPAND COLLECTION — XYZ LLC properties"   │
│    → Back to Step 1                                                   │
│                                                                       │
└──────────────────────────────────────────────────────────────────────┘
```
### What Makes This Powerful
1. **No deploy cycle** — Legacy doesn't need kubectl. It POSTs YAML to a REST endpoint.
2. **No orphaned processes** — DELETE the stream and collection stops cleanly.
3. **Resource bounded** — rate limits prevent runaway API calls.
4. **Auditable** — GET /streams shows all active taskings.
5. **Operator-triggered** — Zeus commands from a map. No terminal needed.
6. **AI-autonomous** — Legacy can create collection tasks WITHOUT Zeus (based on detected patterns).
7. **Composable** — tasking pipelines use the same YAML as permanent feeds.
### Legacy's Tasking Interface (Python)
```python
import httpx

CONNECT_API = "http://redpanda-connect.omos.svc.cluster.local:4195"

async def create_tasking(tasking_id: str, pipeline_yaml: str, ttl_hours: int = 72):
    """Create a dynamic collection pipeline triggered by TAK command."""
    # Create the pipeline
    resp = await httpx.AsyncClient().post(
        f"{CONNECT_API}/streams/{tasking_id}",
        content=pipeline_yaml,
        headers={"Content-Type": "application/yaml"}
    )
    assert resp.status_code == 200

    # Schedule TTL expiry
    await schedule_deletion(tasking_id, hours=ttl_hours)

async def delete_tasking(tasking_id: str):
    """Stop a collection pipeline."""
    resp = await httpx.AsyncClient().delete(
        f"{CONNECT_API}/streams/{tasking_id}"
    )
    return resp.status_code == 200

async def list_active_taskings():
    """Get all running pipelines (permanent + dynamic)."""
    resp = await httpx.AsyncClient().get(f"{CONNECT_API}/streams")
    return resp.json()
```
---
## 7. The Protobuf Story (MTA GTFS-RT)
MTA delivers train positions as Protocol Buffers binary. Here's exactly how Redpanda Connect handles it:
### What you need:
1. The `.proto` file (Google's GTFS-RT spec: `gtfs-realtime.proto`)
2. Mount it in the container at `/proto/gtfs-realtime.proto`
3. Reference it in the pipeline YAML
### The proto file (standard, from Google):
```protobuf
// gtfs-realtime.proto — standard Google Transit spec
syntax = "proto2";
package transit_realtime;

message FeedMessage {
  required FeedHeader header = 1;
  repeated FeedEntity entity = 2;
}

message FeedEntity {
  required string id = 1;
  optional TripUpdate trip_update = 3;
  optional VehiclePosition vehicle = 4;
  optional Alert alert = 5;
}

message VehiclePosition {
  optional TripDescriptor trip = 1;
  optional Position position = 2;
  optional string stop_id = 7;
}

message Position {
  required float latitude = 1;
  required float longitude = 2;
  optional float bearing = 3;
  optional float speed = 5;
}

message TripDescriptor {
  optional string trip_id = 1;
  optional string route_id = 5;
}
// ... (full spec at github.com/google/transit)
```
### Pipeline YAML:
```yaml
pipeline:
  processors:
    - protobuf:
        operator: to_json
        message: "transit_realtime.FeedMessage"
        import_paths: ["/proto/"]
```
**That's it.** Protobuf binary in → JSON out. The `mapping` processor then extracts fields from the JSON structure. No `pip install protobuf`. No `gtfs-realtime-bindings`. No compiled descriptors. Just the raw `.proto` file.
---
## 8. CoT XML Generation in Bloblang
The final transform before publishing to `tak.cot.inject` converts our normalized data to CoT XML:
```yaml
- mapping: |
    let ts = now().format_timestamp("2006-01-02T15:04:05.000Z")
    let stale = now().ts_add_duration("120s").format_timestamp("2006-01-02T15:04:05.000Z")

    root = """<?xml version="1.0" encoding="UTF-8" standalone="yes" ?>
    <event version="2.0" type="%s" uid="%s" how="m-g" time="%s" start="%s" stale="%s">
    <point lat="%f" lon="%f" hae="0" ce="50" le="50"/>
    <detail>
    <contact callsign="%s"/>
    <track speed="%f" course="%f"/>
    <remarks>%s</remarks>
    <marti><dest mission="%s"/></marti>
    </detail></event>""".format(
      this.cot_type,
      this.uid,
      $ts, $ts, $stale,
      this.lat, this.lon,
      this.callsign,
      this.speed, this.course,
      this.remarks,
      this.mission
    )
```
---
## 9. Deployment Architecture
### Kubernetes Manifests
```yaml
# redpanda-connect-deployment.yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: redpanda-connect
  namespace: omos
  labels:
    app.kubernetes.io/name: redpanda-connect
    app.kubernetes.io/part-of: omos-intel
spec:
  replicas: 1
  selector:
    matchLabels:
      app.kubernetes.io/name: redpanda-connect
  template:
    metadata:
      labels:
        app.kubernetes.io/name: redpanda-connect
    spec:
      containers:
        - name: connect
          image: redpandadata/connect:latest
          args:
            - "run"
            - "--streams"
            - "/config/pipelines/*.yaml"
          ports:
            - containerPort: 4195
              name: api
          env:
            - name: NATS_URL
              value: "nats://nats.nats.svc.cluster.local:4222"
            - name: NATS_USER
              valueFrom:
                secretKeyRef:
                  name: omos-nats-creds
                  key: user
            - name: NATS_PASSWORD
              valueFrom:
                secretKeyRef:
                  name: omos-nats-creds
                  key: password
          volumeMounts:
            - name: pipelines
              mountPath: /config/pipelines
            - name: proto
              mountPath: /proto
          resources:
            requests:
              cpu: 100m
              memory: 128Mi
            limits:
              cpu: 500m
              memory: 256Mi
          livenessProbe:
            httpGet:
              path: /ready
              port: 4195
            initialDelaySeconds: 5
            periodSeconds: 30
          readinessProbe:
            httpGet:
              path: /ready
              port: 4195
            initialDelaySeconds: 3
            periodSeconds: 10
      volumes:
        - name: pipelines
          configMap:
            name: redpanda-connect-pipelines
        - name: proto
          configMap:
            name: redpanda-connect-proto
---
apiVersion: v1
kind: Service
metadata:
  na
```
```yaml
me: redpanda-connect
  namespace: omos
spec:
  selector:
    app.kubernetes.io/name: redpanda-connect
  ports:
    - port: 4195
      targetPort: 4195
      name: api
```
### Service for Legacy to reach the API:
```plain text
http://redpanda-connect.omos.svc.cluster.local:4195
```
---
## 10. Monitoring & Observability
### Health Check
```bash
curl http://redpanda-connect:4195/ready
# Returns 200 if all pipelines are healthy
```
### List Active Streams
```bash
curl http://redpanda-connect:4195/streams
# Returns JSON map of all pipeline IDs and their configs
```
### Prometheus Metrics (built-in)
```plain text
# Per-pipeline metrics available at /metrics
input_received_total{label="feed-mta"}
output_sent_total{label="feed-mta"}
processor_error_total{label="feed-mta"}
input_latency_ns{label="feed-mta"}
```
Wire to Grafana for a feed health dashboard, or scrape from omos-ui.
---
## 11. Error Handling & Dead Letter Queue
```yaml
output:
  broker:
    pattern: fan_out
    outputs:
      # Primary: send to NATS
      - nats_jetstream:
          urls: ["nats://nats:4222"]
          subject: "tak.cot.inject"
      # Fallback: failed messages go to DLQ
      - fallback:
          - nats_jetstream:
              urls: ["nats://nats:4222"]
              subject: "intel.dlq.${! meta("pipeline_id")}"
```
Failed events don't vanish — they go to a dead letter queue stream that Legacy can inspect and reprocess.
---
## 12. Security
### NATS Authentication
```yaml
output:
  nats_jetstream:
    urls: ["nats://nats:4222"]
    auth:
      user: "${NATS_USER}"
      password: "${NATS_PASSWORD}"
    tls:
      enabled: true
      skip_cert_verify: true  # Internal cluster TLS
```
### API Keys for Sources
```yaml
input:
  http_client:
    url: "https://api.adsb.exchange/v2/..."
    headers:
      api-auth: "${ADSB_KEY}"  # Resolved from env var
```
All secrets come from k8s Secrets → env vars. No keys in YAML.
---
## 13. Comparison: Python Feed Pod vs Redpanda Connect
| Aspect | Python Feed Pod | Redpanda Connect Pipeline |
| **Language** | Python 3.12 | YAML (Bloblang for transforms) |
| **Dependencies** | pip install at boot (slow, fragile) | Zero (compiled into binary) |
| **Startup time** | 15-30s (pip install) | <1s |
| **Memory per feed** | ~50MB (Python runtime) | ~3-5MB (goroutine) |
| **Adding a feed** | Write Python + ConfigMap + Deployment | Write YAML file |
| **Error handling** | Manual try/except | Built-in retry, DLQ, backoff |
| **Rate limiting** | Manual sleep/counter | Declarative `rate_limit` |
| **Deduplication** | Manual dict tracking | Declarative `dedupe` + cache |
| **Health checks** | Custom probe | Built-in `/ready` endpoint |
| **Metrics** | Manual logging | Built-in Prometheus |
| **Hot reload** | kubectl rollout restart | POST/PUT to REST API |
| **Delta filtering** | 20+ lines haversine code | Cache + dedupe processor |
| **Protobuf** | pip install protobuf + bindings | Native processor + .proto file |
| **Container image** | python:3.12-alpine (50MB+) | redpandadata/connect (30MB) |
| **Deploy footprint** | 1 Deployment + 1 ConfigMap per feed | 1 Deployment total + YAML per feed |
---
## 14. Operational Runbook
### Add a new feed
```bash
# 1. Write pipeline YAML
# 2. Add to ConfigMap
kubectl create configmap redpanda-connect-pipelines \
  --from-file=pipelines/ -n omos --dry-run=client -o yaml | kubectl apply -f -
# 3. Restart (picks up new ConfigMap)
kubectl rollout restart deploy/redpanda-connect -n omos
```
### Add a feed WITHOUT restart (hot add via API)
```bash
curl -X POST http://redpanda-connect:4195/streams/new-feed \
  -H "Content-Type: application/yaml" \
  -d @pipelines/new-feed.yaml
```
### Remove a feed
```bash
curl -X DELETE http://redpanda-connect:4195/streams/feed-name
```
### Check feed health
```bash
curl http://redpanda-connect:4195/streams | jq
curl http://redpanda-connect:4195/metrics | grep output_sent_total
```
### Debug a pipeline
```yaml
pipeline:
  processors:
    - log:
        level: DEBUG
        message: "Received: ${! content()}"
    # ... rest of processors
```
---
## 15. Migration Checklist
- [ ] Deploy Redpanda Connect pod with API enabled
- [ ] Mount GTFS-RT .proto file as ConfigMap
- [ ] Write MTA pipeline YAML (first test)
- [ ] Verify: trains appear in TAK Server
- [ ] Scale feed-mta to 0 replicas
- [ ] Confirm no data loss (trains still flowing)
- [ ] Port remaining 11 feeds to YAML pipelines
- [ ] Scale all old feed-* pods to 0
- [ ] Add new feeds from omos-intel catalog
- [ ] Delete omos-intel droplet ($48/mo saved)
- [ ] Wire Legacy → REST API for dynamic tasking
- [ ] Document and teach to students
---
*One binary. YAML configs. Native NATS. Dynamic tasking via REST.*
*This is the universal collection engine for the OneMind Intelligence Fabric.*
