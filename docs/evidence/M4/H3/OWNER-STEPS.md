# H3 owner steps: real ROS 2 TurtleBot3 in Gazebo

What was proved on the build machine is in `README.md` next to this file. The steps below
need a Linux host with ROS 2 (the build machine has no ROS 2 and no Docker, and macOS is
Tier 3 for ROS 2, standards.md W11). They check the three things marked **unverified** in
the bridge README: the zenoh-bridge-ros2dds key names for the NavigateToPose action, its
payload framing, and Nav2 behaviour against the bridge.

Host: Ubuntu 24.04 with ROS 2 Jazzy (REP-2000 LTS). The omos checkout must contain branch
`feat/ros2-bridge` (PR for H3) and `origin/feat/policy-service` (PR #554). All assets stay
`sim-*`.

## 1. Gazebo TurtleBot3 with Nav2

```bash
sudo apt install ros-jazzy-navigation2 ros-jazzy-nav2-bringup ros-jazzy-turtlebot3*   # package names for Jazzy: unverified
source /opt/ros/jazzy/setup.bash
export TURTLEBOT3_MODEL=burger
ros2 launch nav2_bringup tb3_simulation_launch.py headless:=True
```

In a second shell, give AMCL its start pose if the launch did not (the tb3 world spawns the
robot near x=-2.0, y=-0.5):

```bash
source /opt/ros/jazzy/setup.bash
ros2 topic pub --once /initialpose geometry_msgs/msg/PoseWithCovarianceStamped \
  "{header: {frame_id: map}, pose: {pose: {position: {x: -2.0, y: -0.5}, orientation: {w: 1.0}}}}"
ros2 topic list | grep -E '^/(odom|amcl_pose|cmd_vel|battery_state)$'
ros2 action list | grep navigate_to_pose
```

Expected: `/odom`, `/amcl_pose`, `/cmd_vel` listed (Gazebo may not publish `/battery_state`;
the bridge then reports `battery_pct: -1`), and `/navigate_to_pose`.

## 2. zenoh-bridge-ros2dds beside the robot

```bash
mkdir -p ~/h3 && cd ~/h3
curl -LO https://github.com/eclipse-zenoh/zenoh-plugin-ros2dds/releases/download/1.10.2/zenoh-plugin-ros2dds-1.10.2-x86_64-unknown-linux-gnu-standalone.zip
unzip zenoh-plugin-ros2dds-1.10.2-x86_64-unknown-linux-gnu-standalone.zip
cp <omos>/services/bridge/bridges/ros2/zenoh-bridge-ros2dds.json5 .
./zenoh-bridge-ros2dds -c zenoh-bridge-ros2dds.json5
```

## 3. Check the key names and payloads (the unverified part)

```bash
cd <omos>/services/bridge/bridges/ros2
python3 -m venv .venv && . .venv/bin/activate && pip install -r requirements-dev.txt
python - <<'EOF'
import json, time, zenoh
from ros2bridge import msgs
c = zenoh.Config(); c.insert_json5("mode", '"client"'); c.insert_json5("connect/endpoints", '["tcp/127.0.0.1:7447"]')
s = zenoh.open(c)
seen = {}
sub = s.declare_subscriber("sim_tb1/**", lambda smp: seen.__setitem__(str(smp.key_expr), bytes(smp.payload.to_bytes())))
time.sleep(3)
print("keys:", json.dumps({k: v[:4].hex() for k, v in seen.items()}, indent=1))
for r in s.get("@/*/ros2/route/**", timeout=3):
    if r.ok: print("route:", r.ok.key_expr)
o = msgs.decode_odometry(seen["sim_tb1/odom"])
print("odom decoded:", o.header.frame_id, round(o.pose.x, 2), round(o.pose.y, 2))
s.close()
EOF
```

Expected:
- `sim_tb1/odom` and `sim_tb1/amcl_pose` among the keys, each payload starting `00010000`
  (little-endian CDR header).
- A route line ending in `navigate_to_pose` (action server). If the bridge names the
  queryables differently from `sim_tb1/navigate_to_pose/_action/send_goal`, note the real
  key: set `NAV_ACTION` accordingly, or record it as a bug in the H3 PR.
- `odom decoded: odom -2.0 -0.5` (approximately).

## 4. The H3 acceptance run against Gazebo

Runs nats-server, the policy service (built from PR #554), a stub entity service and the
bridge on this host, and drives the robot as a human: propose `asset.goto`, confirm, the
robot drives in Gazebo, telemetry arrives at the target, then every refusal case.

```bash
cd <omos>/services/bridge/bridges/ros2
NATS_SERVER=$(command -v nats-server) PYTHON=$PWD/.venv/bin/python \
E2E_ZENOH_CONNECT=tcp/127.0.0.1:7447 E2E_ROS_NAMESPACE=sim_tb1 E2E_POSE_TOPIC=amcl_pose \
E2E_GOAL_X=-0.5 E2E_GOAL_Y=-0.5 E2E_ARRIVE_TIMEOUT=120 E2E_EVIDENCE_DIR=$HOME/h3/evidence \
tests/e2e/run.sh
```

(`nats-server` v2.12 or later on PATH; Go 1.25 or later to build the policy service.) The
map origin in the test is 30.2672, -97.7431, so `map` (0, 0) is placed there; the goal
(-0.5, -0.5) is an open spot in turtlebot3_world (unverified: pick any free cell if Nav2
rejects it).

Expected: `1 passed`; the robot visibly drives to the goal in Gazebo (`headless:=False` to
watch); `~/h3/evidence/e2e-report.json` shows `arrived_within_m` below 0.35, `agent_confirm:
agent_cannot_confirm`, `replay: replayed`, `unsigned: bad_signature`, `bridge_fence:
precondition_failed/keep_in`, `policy_fence: precondition_failed`, `non_sim: not_simulated`,
`estop_latch: estopped`. After the e-stop step the robot stops and stays stopped.

Send `~/h3/evidence/` back to be filed under `onemind-xr/docs/evidence/M4/H3/owner/`.

## 5. rosbridge fallback (optional)

```bash
sudo apt install ros-jazzy-rosbridge-server
ros2 launch rosbridge_server rosbridge_websocket_launch.xml   # ws://0.0.0.0:9090
```

Then run the bridge by hand with `TRANSPORT=rosbridge ROSBRIDGE_URL=ws://127.0.0.1:9090
ROS_NAMESPACE= POSE_TOPIC=amcl_pose` (README "Run") and confirm `tel.fleet.robot.sim-tb1.position`
appears (`nats sub 'tel.fleet.robot.>'`). Expected: positions follow the Gazebo robot; a
confirmed goto is published on `/goal_pose` and Nav2 drives there (no goal acknowledgement on
this path, by design).

## 6. Map on the client (FEATURES H3 acceptance text)

"A ROS 2 turtlebot sim moves on the map" needs the entity service (PR #552) and a client
(J1 web, PR onemind-xr #2) on the same NATS: the robot appears as `ent.robot.sim-tb1.state`
and moves. Note owner proposal H3-P4: #552 does not yet fill `capabilities`, so the real
policy path needs either H3-P4 or the stub entity service used in step 4.
