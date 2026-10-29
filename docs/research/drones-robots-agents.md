# Research lane: drones, robots, agents (Phase 1 / M0)

Date: 2026-10-29. Lane owner: robotics research agent. Scope: PX4/ArduPilot SITL on
macOS, MAVLink, ROS 2 to NATS, CoT for UAVs and UGVs, swarm tasking, onboard
detection, and Hermes agent tools over NATS. Feeds features H1–H9, D3–D5 and I1–I5 in
`docs/FEATURES.md`.

Evidence of the local bridge test is in `/Users/danielcarku/Desktop/work/.run/scratch/`
(throwaway, not in a repo): `mavlink_nats_bridge_test.py`, `bridge.out`, `sub.out`,
`wire.out`, `bridge2.out`, `sub2.out`.

## Sources

Local (read on 2026-10-29; omos at `62e3d22b`):

| Ref | Source |
|---|---|
| L1 | `omos/CLAUDE.md`, NATS subject grammar block (lines 85–94; corrected by verifier from "85–92": `grep -n` shows `tel.{node}` on 85 and `tak.cot.>` on 94) |
| L2 | `omos/docs/NATS-DOCTRINE.md` §2 (grammar), §8 (agent layer), §9.1–9.5 (geo-envelope), §9.8 (TAK subjects) |
| L3 | `omos/platform/nats/streams/tel.yaml`, `cmd.yaml`, `env.yaml`, `sys.yaml`, `mission.yaml` |
| L4 | `omos/services/bridge/bridges/mavlink/{README.md,main.go,go.mod}`; `omos/services/bridge/LICENSE` (Apache-2.0) |
| L5 | `omos/services/onemind-core/{README.md,requirements.txt,onemind/bus.py,onemind/capabilities.py}` |
| L6 | `omos/platform/hermes/agent/{FABRIC-REFERENCE.md,deployment.yaml,plugins/README.md,plugins/onemind-vision/__init__.py}`, `platform/hermes/agent-fabric/README.md`, `platform/hermes/mcp/apps.yaml` |
| L7 | `onemind-xr/scripts/sim/mavlink_sim_drone.py`; `onemind-xr/docs/PREFLIGHT.md`; `onemind-xr/docs/FEATURES.md` rows D3–D5, H1–H9, I1–I5 |
| L8 | `work/.tools/py`: pymavlink 2.4.50 (`pymavlink/mavutil.py` lines 61, 122, 297; `dialects/v20/common`), takproto 3.0.1 (`takproto/proto/*_pb2.py`), nats-py 2.16.0 |
| L9 | Local bridge test, command output below |

Primary web and GitHub sources (fetched 2026-10-29):

| Ref | Source |
|---|---|
| W1 | PX4 macOS dev environment: https://docs.px4.io/main/en/dev_setup/dev_env_mac.html |
| W2 | PX4 SIH simulation: https://docs.px4.io/main/en/sim_sih/ |
| W3 | PX4 Gazebo multi-vehicle: https://docs.px4.io/main/en/sim_gazebo_gz/multi_vehicle_simulation.html |
| W4 | ArduPilot macOS build setup: https://ardupilot.org/dev/docs/building-setup-mac.html |
| W5 | ArduPilot SITL setup landing page: https://ardupilot.org/dev/docs/SITL-setup-landingpage.html |
| W6 | MAVLink message signing: https://mavlink.io/en/guide/message_signing.html |
| W7 | MAVLink common messages (HEARTBEAT, GLOBAL_POSITION_INT): https://mavlink.io/en/messages/common.html |
| W8 | gomavlib: https://github.com/bluenviron/gomavlib |
| W9 | zenoh-plugin-ros2dds: https://github.com/eclipse-zenoh/zenoh-plugin-ros2dds |
| W10 | rmw_zenoh: https://github.com/ros2/rmw_zenoh |
| W11 | rosbridge_suite: https://github.com/RobotWebTools/rosbridge_suite and its `ROSBRIDGE_PROTOCOL.md` |
| W12 | ATAK-CIV source (`gh api repos/deptofdefense/AndroidTacticalAssaultKit-CIV`, branch `main`): `atak/ATAK/app/src/main/java/com/atakmap/android/video/cot/VideoDetailHandler.java`, `.../cot/detail/SensorDetailHandler.java`, `.../cot/detail/StatusDetailHandler.java`, assets `atak/ATAK/app/src/main/assets/mil-std-2525c/*.png` |
| W13 | Ultralytics licence section: https://github.com/ultralytics/ultralytics |
| W14 | `gh api repos/<r>` licence checks: YOLOX, RT-DETR, D-FINE (Apache-2.0), MAVSDK (BSD-3-Clause), MediaMTX (MIT), skybrush-server (GPL-3.0), mavros (NOASSERTION) |
| W15 | Synadia agent repos: `gh api search/repositories?q=org:synadia-ai` (synadia-agents, hermes-nats-gateway, agent-fabric-sdk; Apache-2.0) |
| W16 | `onemindos/hermes-plugin-tak` and `onemindos/hermes-plugin-nats-context` READMEs (`gh api repos/<r>/readme`) |
| W17 | Secondary (not authoritative): OmniTAK `cot_types.json` (`engindearing-projects/OmniTAK-Android`), pytak docs, sgofferj/TAK-ADSB-ID (via `gh search code 'a-f-A-M-F-Q'`) |

## Verified facts

### Local bridge test (MAVLink -> NATS)

Setup: the scripted drone `sim-x1` (`mavlink_sim_drone.py --lat 0 --lon 0`, PID 13336)
sends to UDP 127.0.0.1:14550; local nats-server 2.15.2 on 127.0.0.1:4222 (no auth;
`.run/nats.conf`). The test script binds `udpin:127.0.0.1:14550`, waits for HEARTBEAT,
converts GLOBAL_POSITION_INT plus the latest SYS_STATUS/HEARTBEAT into the doctrine §9.2
geo-envelope, and publishes to `tel.sim-x1.drone.position`. An independent
`nats sub` received each message.

Command (from `/Users/danielcarku/Desktop/work`):

```
(nats --server nats://127.0.0.1:4222 sub tel.sim-x1.drone.position --count 3 > .run/scratch/sub2.out &)
.tools/py/bin/python -I .run/scratch/mavlink_nats_bridge_test.py --count 3
```

Output (`bridge2.out`):

```
heartbeat sysid=1 type=2 autopilot=3 mavlink_version=3
published tel.sim-x1.drone.position {"uid": "sim-x1", "type": "fleet.drone", "lat": -0.0006724, "lon": 0.0011676, "hae": 59.999, "course": 209.93, "speed": 11.99, "callsign": "SIM-X1", "stale": 10, "t": 1793261149831, "meta": {"battery_pct": 94, "armed": true, "relative_alt_m": 59.999, "mav_sysid": 1, "source": "mavlink-bridge-test"}}
published tel.sim-x1.drone.position {... "course": 210.42 ...}
published tel.sim-x1.drone.position {... "course": 210.88 ...}
done: 3 messages
```

Independent subscriber: `grep -c Received .run/scratch/sub2.out` → `3`. The first run
(`bridge.out` / `sub.out`) published and received 5 of 5. That run reported `armed: false`
because the script ignored the first HEARTBEAT. I fixed that in the test script; it was
not a drone fault.

Findings from the test:

1. **The scripted drone speaks MAVLink 1, not MAVLink 2.** All 40 sampled datagrams on
   14550 start with `0xfe` (the v1 magic; v2 is `0xfd`) (`wire.out`). The reason is
   pymavlink, which encodes v2 only when the `MAVLINK20` environment variable is set
   (`mavutil.py` lines 61 and 122). `PREFLIGHT.md` and the script docstring both say
   "MAVLink 2", and that is wrong. MAVLink 2 matters because signing exists only in v2
   (W6).
2. `HEARTBEAT.mavlink_version=3` and `autopilot=3` (ArduPilotMega), `type=2` (quadrotor),
   which matches the script (L7).
3. Local nats-server has **no JetStream streams** (`nats stream ls` prints nothing), so
   the test proves core pub/sub only, not persistence into `TEL`.

### Subject grammar and streams (omos)

- `omos/CLAUDE.md` locks `tel.{node}.{class}.{metric}` (example `tel.drone.x47.position`)
  and `cmd.{domain}.{entity}.{action}` (example `cmd.drone.x47.intercept`) (L1).
- NATS-DOCTRINE §9.1 (also "LOCKED") instead defines fleet assets as
  `tel.fleet.{class}.{id}.position|status` and `cmd.fleet.{class}.{id}.{verb}` with
  `verb ∈ goto|estop|rtl|…`, and `cmd.mission.{id}.{verb}` (L2). FEATURES D3 uses the §9.1
  form (L7).
- The `TEL` stream captures `tel.fleet.>`, `tel.drone.>`, `tel.robot.>`, `tel.edge.>`,
  `tel.iot.>`, `tel.industrial.>`, `tel.sat.>` (R3, file, 168 h, `preventDelete`) (L3).
  **`tel.sim-x1.drone.position`, the subject this lane was asked to use, is not captured by
  any omos stream.** Its first token after `tel.` is `sim-x1`, which no `TEL`, `ENV` or
  `SYS` filter matches. `tel.fleet.drone.sim-x1.position` is captured.
- The `CMD` stream captures `cmd.>` (R3, 720 h) (L3). There is **no stream for `det.>`** in
  `platform/nats/streams/` (grep for `det\.` returned nothing).
- The geo-envelope requires `uid`, `type`, `lat`, `lon` and `t`. `hae` is metres above the
  ellipsoid, `t` is epoch ms, `stale` is seconds, and fleet `meta` carries `battery_pct`,
  `mode`, `link_rssi`, `mission_id` and `node_id` (L2 §9.2, §9.4).

### Existing omos MAVLink bridge

- `services/bridge/bridges/mavlink` (Go, gomavlib v3.3.5, nats.go v1.52.0, Apache-2.0)
  publishes `omos.air.{entity_id}.telemetry`, subscribes `omos.air.{entity_id}.command`,
  and reports health on `omos.bridge.{name}.health|errors` (L4: `main.go` lines 166, 334,
  353, 363). **None of these subjects is in the locked grammar.** It uses a OneMind
  envelope (`id, ts, source, entity, domain, payload`), not the §9.2 geo-envelope.
- It parses HEARTBEAT, GLOBAL_POSITION_INT, ATTITUDE, BATTERY_STATUS and VFR_HUD (L4
  README). The README says commands are "passthrough v0.3".
- gomavlib decodes and encodes MAVLink v1 and v2, supports v2 signatures, is MIT
  licensed, and has UDP server/client/broadcast, TCP, serial and custom endpoints (W8).

### MAVLink

- GLOBAL_POSITION_INT (id 33): `lat`/`lon` degE7, `alt` is **mm above MSL**,
  `relative_alt` is mm above home, `vx/vy/vz` cm/s, `hdg` cdeg with UINT16_MAX meaning
  unknown (W7). The doctrine field `hae` is therefore **not** `alt`: it needs a geoid
  undulation correction. My test wrote MSL into `hae`, which is wrong; a production bridge
  must convert.
- Command ids from the generated common dialect (L8): `MAV_CMD_DO_REPOSITION` 192
  (guided goto), `MAV_CMD_NAV_RETURN_TO_LAUNCH` 20, `MAV_CMD_NAV_TAKEOFF` 22,
  `MAV_CMD_NAV_LAND` 21, `MAV_CMD_COMPONENT_ARM_DISARM` 400, `MAV_CMD_DO_SET_MODE` 176,
  `MAV_CMD_DO_FLIGHTTERMINATION` 185, `MAV_CMD_DO_FENCE_ENABLE` 207. Messages:
  `MISSION_ITEM_INT` 73, `VIDEO_STREAM_INFORMATION` 269 (has `uri`, `hfov`),
  `SETUP_SIGNING` 256, `OPEN_DRONE_ID_LOCATION` 12901.
- MAVLink 2 signing: a 6-byte signature (first 48 bits of SHA-256 over the packet, a
  48-bit timestamp and a 32-byte shared key). Receivers reject stale or out-of-order
  timestamps per (sysid, compid, link). It authenticates, it **does not encrypt**, and
  libraries without signing support drop signed packets (W6).

### SITL on macOS

- **PX4:** the macOS dev environment works on Intel and Apple Silicon (CI runs on Apple
  Silicon only). Setup is `./Tools/setup/macos.sh --sim-tools`, which installs Gazebo
  Harmonic through pixi/conda-forge plus XQuartz and **prompts for the password**. Raise
  the open-file limit with `ulimit -S -n 2048`, then smoke-test with
  `make px4_sitl gz_x500` (W1).
- **PX4 SIH** runs as SITL without Gazebo (`px4_sitl_sih` target). Only the quadrotor
  `sihsim_quadx` is stable; hex, airplane, VTOLs and an Ackermann rover are experimental.
  Ports: 14550 to GCS, 14540+N for MAVSDK/offboard (listening on 14580+N), and 8888 for
  uXRCE-DDS (ROS 2). It is headless (W2). The page does not state macOS support.
- **PX4 multi-vehicle** with Gazebo is **Linux only**. On Linux, each instance is
  `px4 -i N` with `PX4_GZ_STANDALONE=1` after the first, and ROS 2 topics are namespaced
  `/px4_N/fmu/...` (W3).
- **ArduPilot:** the macOS page gives Homebrew prerequisites and
  `Tools/environment_install/install-prereqs-mac.sh`, with no mention of Apple Silicon (W4).
  The SITL setup landing page lists Linux, Windows (WSL, Cygwin, VM) and Vagrant, but **no
  macOS page** (W5).
- On this build machine, building ArduPilot SITL from source was blocked by the agent
  permission policy (PREFLIGHT). Docker is absent (PREFLIGHT).

### ROS 2 to NATS

- **zenoh-plugin-ros2dds / `zenoh-bridge-ros2dds`** bridges all ROS 2 DDS traffic over
  Zenoh. It ships as a router plugin or standalone binary (zip, Debian package, Docker
  image `eclipse/zenoh-bridge-ros2dds`), maps services and actions to Zenoh queryables,
  supports a namespace prefix, and is licensed EPL-2.0 / Apache-2.0 (W9).
- **rmw_zenoh** is an Apache-2.0 ROS 2 RMW on zenoh-cpp. It needs a Zenoh router for
  discovery by default, and Humble cannot talk to Iron or newer because of type hashes in
  key expressions (W10).
- **rosbridge_suite** (BSD-3-Clause) is a JSON protocol over WebSocket or TCP. Its `ros2`
  branch covers Jazzy and later; Humble has its own branch (W11).
- I found **no maintained Zenoh-to-NATS or ROS 2-to-NATS bridge** in the search (see
  Unverified). Either path needs an omos-written adapter.

### CoT for UAVs and UGVs

- ATAK-CIV ships MIL-STD-2525C icons as `assets/mil-std-2525c/sfapmfq--------.png`
  (friendly / air / military / fixed-wing / `Q`) and 22 variants from `sfapmfqa` to
  `sfapmfqy` (including `sfapmfqrw/rx/rz`). It also ships `sfapmhq--------.png`, so the
  multirotor `a-f-A-M-H-Q` has a primary icon too (verifier: `gh api
  .../git/trees/main?recursive=1`). ATAK therefore renders `a-f-A-M-F-Q`, `a-f-A-M-H-Q`
  and the fixed-wing sub-types (W12). The `Q` = UAV meaning is
  corroborated by secondary lists that label `a-f-A-M-F-Q` "UAS Fixed Wing" and
  `a-f-A-M-H-Q` "UAS Multirotor" (W17). The 2525C standard text itself was not read.
- For ground vehicles, ATAK ships `sfgpevu--------.png` and `sfgpevua/aa/b/l/r/s/sh/sl/sm/t/th/tl/x`
  (W12; verifier corrected the variant list from the tree listing).
  Secondary lists label `a-f-G-E-V-U` "Utility Vehicle", **not** UGV (W17). FEATURES H1's
  `a-f-G-E-V` is the generic ground vehicle. **No primary source found for a dedicated
  UGV type in 2525C.**
- `<__video uid=".." url="..">` is parsed by `VideoDetailHandler` (`super("__video")`). It
  can carry a nested `<ConnectionEntry>` with attributes `uid, alias, address, port,
  roverPort, path, protocol, networkTimeout, bufferTime, rtspReliable, ignoreEmbeddedKLV`
  (W12).
- `<sensor>` attributes (`SensorDetailHandler` constants) are `range, rangeLines, azimuth,
  fov, vfov, roll, elevation, model, displayMagneticReference, hideFov, fovLabels,
  fovAlpha/Red/Green/Blue, strokeColor, strokeWeight`. The FOV cone's uid suffix is
  `-fov` (W12).
- `<status battery=".." readiness="..">` is parsed by `StatusDetailHandler` (W12).
- TAK Protocol v1 protobuf `Detail` has typed fields only for `contact` (endpoint,
  callsign), `group` (name, role), `precisionLocation`, `status` (battery), `takv`
  (device, platform, os, version) and `track` (speed, course). Everything else, including
  `__video` and `sensor`, travels in `xmlDetail` (L8, takproto 3.0.1 descriptors).

### Swarm tasking

- MAVLink provides per-vehicle mission upload (`MISSION_ITEM_INT`) and guided commands
  (`DO_REPOSITION`) (L8). It has no multi-vehicle task allocation; that belongs to the
  ground side.
- PX4 multi-vehicle SITL requires Linux with Gazebo (W3). For a test fleet on macOS, run
  several scripted drones with distinct `--sysid` and ports (L7 supports `--sysid` and
  `--out`), or several SIH instances if SIH builds on macOS (unverified).
- Skybrush server (drone-show and swarm GCS) is GPL-3.0 (W14). That licence is compatible
  with onemind-xr (GPL-3.0) but **not** with the Apache-2.0 omos bridge tree (L4).

### Onboard detection

- Ultralytics YOLO is AGPL-3.0 or a paid Enterprise licence (W13). It cannot be used in
  omos (Apache-2.0 bridge tree) or distributed in onemind-xr without accepting AGPL
  network terms.
- Apache-2.0 detectors are available: YOLOX, RT-DETR and D-FINE (W14).
- No `det.>` stream exists in omos (L3). The grammar has `det.{domain}.{type}.{id}` (L1).

### Hermes and onemind-core agent tools

- onemind-core exposes **named capabilities only**, so clients cannot supply URLs or NATS
  subjects. The four are `inspect-system`, `summarize-site`, `delegate-agent` and
  `restart-worker`, with the last two marked `mutates: True`. `delegate-agent` discovers
  agents through `synadia_ai.agents.Agents.discover()` and prompts one, with a
  dispatch-once guard (`store.claim_dispatch`). `restart-worker` is limited to two
  allowlisted deployments (L5 `capabilities.py`). Dependency: `synadia-ai-agents==0.7.1`
  (L5). The NATS connection uses TLS plus user/password from the environment (L5
  `bus.py`). **onemind-core has no drone, robot or map capability.**
- Hermes registers `agents.prompt.hermes.zeus.legacy` through the `nats-platform` plugin,
  which is upstream `synadia-ai/hermes-nats-gateway` 0.1.2 (Apache-2.0) plus a two-line
  patch (L6, W15). It runs a `nats-leaf` sidecar (image `nats:2.10-alpine`, not pinned by
  digest) (L6 `deployment.yaml` line 232).
- Installed Hermes plugins: `onemind-vision` (ClickHouse queries: `vision_vision_query`,
  `vision_analyze_feed`, `vision_sitrep`, `vision_anomaly_check`, `vision_recommend`),
  `nats-context`, the TAK platform plugin, and `memory-wiki` (L6 `plugins/README.md`).
- `hermes-plugin-nats-context` subscribes to 13 wildcards, including `tak.cot.>`, `tel.>`,
  `intel.>` and `agents.hb.>`. It also exposes **`nats_publish(subject, payload)` for "any
  valid NATS subject"** and `nats_request` (W16, README lines 494–519). Verifier caveat:
  `plugins/README.md` says the installed `nats-context` is only "Probably"
  this repo and was "Not compared with the live copy" (same for the TAK plugin), so
  whether the **live** Hermes has `nats_publish` is **unverified**.
- `hermes-plugin-tak` lists 22 tools, including `tak_publish_cot`,
  `tak_create_geofence`, `tak_create_mission`, `tak_send_emergency` and `takml_detect`.
  The repo has no licence file (W14, W16).
- The agent-fabric NATS login is currently the **shared `onemind` login**. A dedicated
  user limited to `agents.>` and `_INBOX.>` is planned (L6 `agent-fabric/README.md`).

## How it applies to OneMind

1. **H2 (MAVLink bridge) must move the existing omos bridge onto grammar subjects.**
   `omos.air.*` breaks the locked grammar and lands in no stream. Publish
   `tel.fleet.drone.{id}.position` and `tel.fleet.drone.{id}.status` with the §9.2
   envelope, and subscribe to `cmd.fleet.drone.{id}.{goto|rtl|land|arm|disarm|estop}`.
   These are captured by `TEL` and `CMD` today with no stream change. Keep gomavlib: it
   handles v1 and v2 and signing (W8).
2. **The subject conflict needs an owner proposal (decision 0004 #8).** CLAUDE.md's
   `tel.{node}.{class}.{metric}` and doctrine §9.1's `tel.fleet.{class}.{id}.position`
   are both "locked". The lane task's `tel.sim-x1.drone.position` follows the CLAUDE.md
   form but is not persisted. Recommend that BUS-DESIGN adopt §9.1 for fleet assets
   (treating `fleet` as the node token for the fleet aggregate) and that omos `docs/` get a
   proposal amending the CLAUDE.md example. Until then, bridges publish the §9.1 form.
3. **H1/H9 (robotics CoT profile).** Project `tel.fleet.*` to CoT as follows:
   - type: `a-f-A-M-H-Q` (multirotor) or `a-f-A-M-F-Q` (fixed wing) for drones, chosen
     from `HEARTBEAT.type`; `a-f-G-E-V` (generic) or `a-f-G-E-V-U` for ground robots
     until a UGV type is proven.
   - detail: `<track course speed>`, `<status battery>`, `<contact callsign>`;
     `<__video url=rtsp://…>` from MediaMTX, or from `VIDEO_STREAM_INFORMATION.uri`;
     `<sensor azimuth fov vfov range>` for the camera footprint.
   - Over TAK Protocol v1, only track, status and contact are typed protobuf fields; video
     and sensor go in `xmlDetail`.
   - Inject on `tak.cot.inject` (decision 0004 #7).
4. **H6 safety and D5 policy:** treat `cmd.fleet.*` as the only path to an asset. The
   bridge refuses a command unless it carries a policy decision and audit id. Geofence
   checks run in the bridge before `DO_REPOSITION`. Enable MAVLink 2 signing on real links,
   which needs the bridge to send v2. Sim uses `MAVLINK20=1` for the scripted drone.
5. **I2/D5 (Hermes tools):** today Hermes can reach `cmd.*` through the generic
   `nats_publish` tool on a shared login. Asset tools should follow the onemind-core
   pattern instead: named capabilities (`propose-asset-command`, `query-entities`,
   `annotate-map`) whose subjects are fixed in code. Kinetic or movement verbs become a
   *proposal* that waits for human confirmation in the UI (Rule 6), not a direct publish.
   Discovery uses `$SRV.PING.agents` (doctrine §8).
6. **H3 (ROS 2):** run `zenoh-bridge-ros2dds` beside the robot, plus a small omos adapter
   (zenoh-python to nats-py) that maps `/odom`, `/battery_state` and `/navigate_to_pose`
   onto `tel.fleet.robot.{id}.*` and `cmd.fleet.robot.{id}.goto`. rosbridge is the
   fallback for robots where a Zenoh bridge cannot run, or for browser tooling.
7. **H5 (swarm):** task allocation belongs in the OneMind tasking service
   (`cmd.mission.{id}.*`, `evt.mission.*`, both streamed today), with each asset driven
   individually over MAVLink. Test with three scripted drones with distinct sysids and
   ports on macOS, and PX4 Gazebo multi-vehicle on Linux CI.
8. **H8 (detection):** use Apache-2.0 detectors (RT-DETR, D-FINE, YOLOX), never
   Ultralytics. Detections need a new `DET` stream CR for `det.>` in an omos PR.
9. **Sim rig (H2/H7):** on macOS, the scripted drone is the always-available fallback.
   PX4 SIH quad is the best real-autopilot candidate (no Gazebo) but needs the
   `macos.sh` toolchain, which prompts for a password, so it is an owner-checklist item.
   On Linux CI, use PX4 SITL with Gazebo, or ArduPilot SITL.

## Risks

| Risk | Evidence | Mitigation |
|---|---|---|
| Two "locked" fleet grammars; bridges built to the wrong one publish into no stream | L1 vs L2 §9.1; L3; lane test subject not captured | Owner proposal; build to §9.1 now; contract test asserts `TEL` captures every fleet subject |
| omos MAVLink bridge already off-grammar (`omos.air.*`) | L4 | H2 PR rewrites the subjects; no consumers found in omos streams |
| Hermes can publish any subject (`nats_publish`) on a shared login, so it could publish `cmd.fleet.*` directly and bypass policy (**unverified** for the live pod: the installed plugin is only "probably" the W16 repo) | W16, L6 | Dedicated NATS user for Hermes with `cmd.fleet.>` denied; asset commands only through a policy-gated capability; audit on `evt.*` |
| Scripted drone sends MAVLink 1, so signing cannot be tested | `wire.out`; pymavlink `mavutil.py` | Start sim with `MAVLINK20=1`; correct PREFLIGHT wording (owned by another lane) |
| `hae` filled from MSL altitude, so drones render at the wrong height in 3D (geoid offset, tens of metres in places; **unverified magnitude**) | W7 | Bridge applies an EGM96/EGM2008 undulation, or uses `GPS_RAW_INT.alt_ellipsoid` when present (**unverified** field availability per autopilot) |
| No real SITL on the build machine | PREFLIGHT; W1, W5 | Scripted drone here; PX4 SIH/Gazebo on the owner's Mac and in Linux CI |
| Licence contamination (Ultralytics AGPL, Skybrush GPL in Apache omos, unlicensed `hermes-plugin-tak`) | W13, W14 | Apache-2.0 detectors; keep GPL code in onemind-xr only; ask the plugin owner for a licence before reusing its code |
| UGV CoT type unproven; ATAK may render robots as generic ground vehicles | W12, W17 | Use `a-f-G-E-V`; verify a UGV symbol on ATAK in the owner checklist |
| No `det.>` stream | L3 | `DET` stream CR in the H8 PR |

## Recommendation

1. **H2 first (omos PR):** rework `services/bridge/bridges/mavlink` to publish §9.2
   envelopes on `tel.fleet.drone.{id}.position|status` and to accept
   `cmd.fleet.drone.{id}.{verb}`. That means converting MSL to HAE and mapping `goto` →
   `DO_REPOSITION`, `rtl` → 20, `land` → 21, `arm`/`disarm` → 400 and `estop` → 185 behind
   a two-person confirm. Add a contract test that runs the scripted drone (with
   `MAVLINK20=1`) and asserts that a local `TEL`-equivalent stream stores the messages. The
   throwaway test in `.run/scratch` is the seed; it is 51 lines (`wc -l`; verifier
   corrected from "about 60").
2. **Owner proposal (omos `docs/`)** reconciling `tel.{node}.{class}.{metric}` with
   §9.1 `tel.fleet.{class}.{id}.*`, recommending §9.1 for fleet assets.
3. **H9 profile** as `docs/contracts/robotics-cot-profile.md`, using the type and detail
   mapping above (track, status, contact, `__video`, `sensor`).
4. **Agent safety before agent tools (I4 before I2/D5):** a dedicated Hermes NATS user
   that cannot publish `cmd.fleet.>`, and a policy-gated `propose-asset-command`
   capability following the onemind-core pattern.
5. **H3:** zenoh-bridge-ros2dds plus a small zenoh-to-NATS adapter in omos;
   rosbridge as the fallback.
6. **H8:** RT-DETR or D-FINE (Apache-2.0) plus a `DET` stream. Never Ultralytics.
7. **SITL:** scripted drones here; PX4 SIH (`make px4_sitl_sih sihsim_quadx`) and
   Gazebo `gz_x500` go in the owner checklist for the Mac; PX4 multi-vehicle runs in Linux
   CI.

## Unverified items

- Whether PX4 SIH builds and runs on macOS. The SIH page is silent (W2); the macOS page
  covers `px4_sitl` with Gazebo (W1).
- ArduPilot native SITL on Apple Silicon. Neither W4 nor W5 states it.
- Whether a maintained Zenoh-to-NATS or ROS 2-to-NATS bridge exists. One search found
  none; that does not prove absence.
- The 2525C standard text for `A-M-F-Q`/`A-M-H-Q` (= UAV) and for any UGV symbol. Only
  ATAK icon filenames (primary) and third-party labels (secondary) were read.
- The exact semantics of `<__video>` attributes beyond `uid`, `url` and the
  `ConnectionEntry` attribute names. Only the handler's attribute reads were inspected.
- Per-autopilot availability of `GPS_RAW_INT.alt_ellipsoid`, and the size of the geoid
  offset at the owner's site.
- The live permissions of the Hermes NATS user in the cluster. Not checked: no NATS
  credentials, and the lane does not read Secrets.
- Whether `onemind-core` and the Hermes plugins are deployed and live in the cluster
  today. Repo state only; the cluster was not queried in this lane.
- Whether `hermes-plugin-tak` really exposes 22 tools; its README heading says "20 tools"
  while its table lists 22 (W16).

Blockers recorded: none from the permission system in this lane. Real SITL was already
blocked in pre-flight (ArduPilot build denied by agent policy; Docker absent).

## Independent verification (2026-10-29)

The verifier re-checked 30 claims against the primary sources: W1–W7, W9–W11 and W13
fetched; W8, W12, W14–W16 through `gh api`; and local omos `62e3d22b` files, pymavlink
and takproto introspection. It also ran its own re-tests: a 20-datagram UDP sniff on 14550
(all `0xfe`, so MAVLink 1 is confirmed), `nats stream ls` (output "No Streams defined"),
and `nats-server --version` (v2.15.2).
Confirmed: the stream subjects (no stream captures `tel.sim-x1.>` or `det.>`), §9.1/§9.2
fields, bridge subjects and line numbers, gomavlib MIT/v1+v2/signing, all MAV_CMD and
message ids, GLOBAL_POSITION_INT units, signing details, PX4 mac/SIH/multi-vehicle
statements, ArduPilot pages, zenoh/rmw_zenoh/rosbridge facts, ATAK handler attributes,
takproto `Detail` fields, all licences, onemind-core capabilities, the Hermes leaf image
at line 232, and the 13 subscriptions and 22-row tool table. Corrected: the CLAUDE.md line
range, the ATAK icon variant lists (with a primary `sfapmhq` icon added) and the script
length. Marked unverified: `nats_publish` on the live Hermes.
Licence and cluster safety: the verifier found no violation. No AGPL or Elastic code was
copied, the cluster was not queried, no Secret was read, and the test used only a local
NATS with no auth and a simulated asset.
