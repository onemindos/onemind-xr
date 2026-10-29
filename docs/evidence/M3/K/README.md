# Evidence: Lane C video, item K (K1–K4, K6, K9), 2026-10-29

omos PR https://github.com/onemindos/omos/pull/559, branch `feat/mediamtx-video`, worktree
`work/wt/k-video`. Every test ran on the build machine (macOS arm64) with MediaMTX v1.21.3,
ffmpeg 8.1.1 and nats-server 2.15.2. Each test used its own MediaMTX and nats-server on free or
offset ports with throwaway passwords; the shared session instances were not used.

| File | Proves |
|---|---|
| `video-registry-go-test.txt` | `go vet` and `go test -race`: 13 tests pass. `TestE2EMediaMTX` runs the real MediaMTX with the PR's `mediamtx.yml` and covers K1 (refusals, LL-HLS, recordings roll) and K2 (registry link to `ent.drone.sim-x1.state`, online and offline events) |
| `sim-x1-hls-frame.png` | a frame of sim-x1's test pattern, decoded over LL-HLS from the URL the registry returned, read as `onemind-client` |
| `k2-registry-binary-transcript.txt` | the real `video-registry` binary: `select ent.drone.sim-x1.state` returns not_found, then the live record, then `live=false` after the camera stops; the KV value, `/readyz`, `/metrics` and the `evt.video.sim-x1.*` events |
| `k6-plugin-manifests.txt` | the 4 examples (all 5 kinds) validate; the off-grammar, reserved-writer and unsafe-action fixtures fail; 11 unittest cases pass |
| `k9-edge-selftest.txt` | the edge script's SRT forward loop against the cluster config, under bash 3.2 |
| `omos-ci-local.txt` | omos CI equivalents: changed-YAML gate, kubeconform, the rendered kustomization, MCP coverage, plugin manifests, ShellCheck, gitleaks on the tree and on the commit range |
| `omos-ci-github.txt` | GitHub CI on PR #559: every check passes; the `test` job ran `TestE2EMediaMTX` on Linux with the real MediaMTX (30.8 s) |
| `OWNER-STEPS.md` | what needs the cluster sync, the tailnet, the Mac, a LAN device or ATAK |

Feature state:

| Feature | State |
|---|---|
| K1 | Built and proven locally against the real binary with the cluster config. **Awaiting sync and owner check**: browser LL-HLS over the tailnet, UDP through the operator, non-root pod |
| K2 | Built and proven locally (registry, KV, RPC, events). Clients see the stream through `svc.registry.video.get`. The entity `media[]` merge waits on proposal K-P2 (entity service, PR #552) |
| K3 | Renderer and hygiene built and tested (robotics-cot-profile §9 test 3). Not wired: the projector (A6) does not exist in omos. `/Marti/api/video` posting is left to the planned tak-client; the feed body for TAK 5.8 is unverified |
| K4 | **Not built here**: needs the Unreal `OneMindVideo` plugin (client lane, no Unreal on this machine). The omos data path it needs (registry URLs and latency classes) is done. Owner steps in section 6 |
| K6 | Built in omos: manifest schema, CI grammar check, one example per kind, failing fixtures. The client-side loading of manifests (Unreal, L0, ATAK) belongs to the client lane |
| K9 | Script built. The build-machine SRT forward loop passes. **Awaiting owner check** on the Mac with a second LAN device |

Findings worth carrying forward:
- MediaMTX v1.21.3 takes `authInternalUsers` passwords from `MTX_AUTHINTERNALUSERS_<i>_PASS`. This resolves a research unverified item, and passwords stay out of the ConfigMap.
- An authenticated publisher on a *configured* path outside its grant gets 401. This resolves the second research unverified item.
- MediaMTX logs the full SRT forward URL, including the password in the streamid. The edge script redacts it, and the self-test asserts that no log contains it.

## Review fixes (PR #559 commit ad2a8be8)

Applied all six independent-review findings; transcript in `review-fixes.txt`. Retention is now
6 h drones/robots and 2 h cameras (sized to 20 Gi, disk alerts at 75%/90%); HLS/WebRTC CORS
origins are empty (e2e asserts no Access-Control-Allow-Origin for a foreign Origin); the plugin
validator refuses wildcard publishes covering reserved writers; the usgs-quakes layer uses
`kind: earthquake` / `sizeField: mag`; video-registry no longer holds its state mutex across KV
I/O. The PR body states K4 is out of scope and K3 is renderer-only.
