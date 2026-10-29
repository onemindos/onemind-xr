Follow-up from #555 (A22, BUS-DESIGN section 11 "other 21 kv-*.yaml").

#555 sets `maxBytes` on every Stream CR but not on the KeyValue CRs in `platform/nats/streams/`. These have no `maxBytes` (main plus #555, 2026-10-29):

kv-agent-caps, kv-agent-config, kv-agent-registry, kv-device-registry, kv-fleet-state, kv-geofences, kv-mission-state, kv-node-registry, kv-om-config, kv-om-meta-dedup, kv-om-schema, kv-overwatch-intercept, kv-overwatch-state, kv-overwatch-video, kv-projects, kv-sys-health, kv-task-index, kv-task-notify, kv-webhook-registry, kv-world-state, kv-zeus-context, kv-zeus-focus, kv-zeus-presence

(`kv-feed-config` stays excluded from the Application; `kv-ingest-*` come from #553 and `kv-entity-correlation` from #552, each with its own cap.)

Scope:
- Read each bucket's live size (read-only `jsz`) and set a `maxBytes` with headroom.
- Keep the sum inside the 2 GiB per-node KV budget used by `scripts/tests/test_gitops_stream_caps.py` (`KV_BUDGET`), and extend that test so a KV CR without `maxBytes` fails CI.
- Check that no cap is below a bucket's current size (a lower cap drops data on sync).

🤖 Generated with [Claude Code](https://claude.com/claude-code)
