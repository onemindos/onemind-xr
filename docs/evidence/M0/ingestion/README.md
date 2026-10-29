# Ingestion proof evidence (M0, ingestion lane part c)

The analysis is in `docs/research/ingestion-platform.md`. Everything ran on 127.0.0.1 against
a dedicated nats-server on :34222, not the shared :4222 server. All upstream data is synthetic.

| Path | What it is |
|---|---|
| `proof/stubs/upstream_stub.py` | Upstream stubs, one per delivery mode; logs every emitted id |
| `proof/bento/` | Bento/Redpanda Connect configs: `main.yaml`, `resources.yaml`, `streams/01..12` |
| `proof/go/` | Go collector on nats.go (`main.go`, `go.mod`, `go.sum`) |
| `proof/scripts/` | `proof.sh` (control), `setup_streams.sh`, `chaos.sh`, `leader.sh`, `rpc_compat.sh`, `verify.py`, server configs |
| `results/bento-chaos-run{1,2}` | Bento chaos runs (run 2 = fixed configs) |
| `results/go-chaos-run{1..5}` | Go chaos runs; run 2 invalid (stale KV state); **run 5 is final** |
| `results/go-leader-run{1,2}` | Two-instance lease takeover; `go-leader-stale-state` predates the reset fix |
| `results/rpc-compat` | Redpanda Connect 4.115.1 running the unchanged Bento configs |

Reproduce: start the servers from `proof/scripts/*.conf` / `mediamtx-ingest.yml`, run
`setup_streams.sh`, build `proof/go` into `work/.tools/bin/ingestd-proof`, then
`chaos.sh go|bento`, `leader.sh`, `rpc_compat.sh`.
