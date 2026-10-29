# Open Questions

The build run does not wait on these. Each has a default or fallback in
[decision 0004](decisions/0004-owner-defaults-for-the-run.md); the owner can revisit any of them.

| # | Question | Owner | Resolved by |
|---|---|---|---|
| 1 | Pixel Streaming 2 on macOS with UE 5.8? | build | Spike S2 |
| 2 | Quest Pro path: standalone, streamed or WebXR? | build | Spike S3 |
| 3 | Commo on macOS arm64? | build | Spike S4 |
| 4 | Which photorealistic basemap, and does OneMind's NGO use fit its terms (Cesium ion Community is personal and non-commercial; Google is metered)? | owner | Decision record |
| 5 | Does a GPU host exist for streaming (home Linux box or DigitalOcean GPU node)? | owner | Before M3 if S2 fails |
| 6 | Should mesh CoT also go to TAK Server by default (`MESH_TO_TAK_SERVER`)? | owner | omos ADR 0003 |
| 7 | `evt.tak.cot.{uid}` vs `tak.cot.*` as the raw TAK wire (omos NATS-DOCTRINE §9.8) | owner | Before the entity service reads TAK |
| 8 | What do the existing `kv-world-state` and `kv-fleet-state` buckets hold, and who writes them? | build | M0 read-only check |
| 9 | Is this repo public or private long-term? GPL-3.0 needs source for any distributed binary either way. | owner | Before first release |
