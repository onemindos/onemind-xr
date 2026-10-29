# Pre-flight — 2026-10-29

Build machine: Daniel's MacBook Pro (Apple M1 Max, 32 GB RAM, 32-core GPU, Metal 4),
macOS 26.5 (25F71). Not the owner's acceptance Mac.

## Resources

| Item | Result | Consequence |
|---|---|---|
| Free disk | **25 GiB** of 926 GiB (98% used), `df -h` | Unreal Engine (about 150 GB) cannot be installed. Run budget: stop any install or build that would leave less than 15 GiB free. |
| Memory | 32 GiB | Enough for local services and agents |
| GPU | Apple M1 Max, 32 cores, Metal 4 | Usable for web/CesiumJS rendering tests |

## Tools

Already on the host (not changed): Git 2.50.1, GitHub CLI 2.88.1, Node 22.22.0 / npm
10.9.4, Python 3.15.0, uv 0.11.15, jq 1.7.1, CMake 4.4.3, Xcode (`/Applications/Xcode.app`),
ffmpeg 8.1.1 (`/opt/homebrew/bin`), kubectl 1.37.1, Tailscale 1.104.2.

Missing and **not installable without admin rights**: Docker, Unreal Engine 5.8.
Unreal work is therefore source code that builds once UE exists, plus owner-checklist steps.

Installed into `work/.tools/` (remove everything by deleting that folder):

| Tool | Version | Source |
|---|---|---|
| Go | 1.27.3 darwin-arm64 | go.dev/dl official tarball |
| nats-server | v2.15.2 | github.com/nats-io/nats-server release |
| nats CLI | v0.5.1 | github.com/nats-io/natscli release |
| MediaMTX | v1.21.3 | github.com/bluenviron/mediamtx release |
| Python 3.12 venv `work/.tools/py` | takproto, pymavlink, MAVProxy, nats-py, pyyaml, pytest | PyPI via uv |

`work/.tools` is 1.3 GiB.

**SITL:** building ArduPilot SITL from source was blocked by this machine's agent
permission policy (running externally fetched build code). Fallback: the scripted MAVLink 2 drone
`scripts/sim/mavlink_sim_drone.py` (pymavlink; HEARTBEAT, SYS_STATUS,
GLOBAL_POSITION_INT, ATTITUDE, VFR_HUD; ACKs arm, takeoff, land, RTL). A real SITL
(ArduPilot or PX4) is an owner-checklist item.

## Local stack (runs for the session; config and data in `work/.run/`)

| Service | Bind | Proof |
|---|---|---|
| nats-server 2.15.2, JetStream (4 GB file, 512 MB memory), WebSocket 9222, leaf 7422 | 127.0.0.1:4222, monitor :8222 | `nats server check jetstream` → `OK JetStream`; published `tel.sim-x1.drone.heartbeat` |
| MediaMTX 1.21.3 | RTSP 127.0.0.1:8554, RTMP :1935, HLS :8888, WebRTC :8889 (all localhost; MoQ off) | listeners in `lsof` |
| `sim-x1` scripted drone | MAVLink UDP → 127.0.0.1:14550 | HEARTBEAT received; arm (400) and takeoff (22) ACK result 0; GLOBAL_POSITION_INT received |

## Access

| Grant (docs/ACCESS.md) | Result | Fallback applied (decision 0004) |
|---|---|---|
| GitHub push `onemindos/onemind-xr` | works (`permissions.push=true`) | — |
| GitHub push `onemindos/omos` | works; run policy: PRs only, never merge | — |
| Cluster read | works, but the local context is **cluster-admin**, not the read-only `view` ServiceAccount | Run treats it as read-only by rule; no sandbox namespace `onemind-dev` exists, so no in-cluster spikes |
| NATS run user / leaf to cluster | none; `omos-nats` not resolvable | #12: local nats-server |
| TAK user certificate, `tak.tail717ba0.ts.net:8089` | none; this machine's Tailscale is on a different tailnet (name does not resolve) | #12: mesh gateway + scripted mesh peer + `takproto` messages |
| Cesium ion token (keychain `cesium-ion`) | not found | #4: open tiles and terrain |
| Argo CD read-only account | not issued; Argo state read through kubectl (read-only) | — |

## Long-run safety

Work is committed and pushed after each finished feature. `docs/STATUS.md` is kept
current; a restarted session reads `STATUS.md` and this file first.
