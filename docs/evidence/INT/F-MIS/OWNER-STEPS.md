# F-MIS owner steps

Branches: omos `feat/mission-service` (#568) `e69ea4c9`, `feat/mavlink-bridge-fleet` (#550) `6e4f29ea`.

## 1. Decide D2-P2 and D2-P14

Both are in `docs/proposals/2026-10-29-mission-service-owner-proposals.md` on #568.
- D2-P2: ratify bridge-side arrival (the bridge reports `done_ok` once within 5 m / 5 m of the
  goto target). Then decide whether to keep `MISSION_ARRIVAL` (task-service telemetry check,
  default `false`) as a fallback until the ROS 2 bridge (#566) reports arrival, or remove it.
- D2-P14: amend mission-task.md §5 so `sent` is set on the dispatch audit or on the first
  assignee report, whichever comes first.

## 2. Real-autopilot arrival tolerance (needs ArduPilot or PX4 SITL; not available here)

```
# ArduCopter SITL on udp 127.0.0.1:14550, then from omos services/bridge/bridges/mavlink:
NATS_SERVER=$(command -v nats-server) MAVLINK_ENDPOINT=udpserver:127.0.0.1:14550 ./test/e2e.sh
```
Expected: case "goto task arrives: done_ok from the bridge" passes within 45 s. The transcript's
`last.result` shows `distance_m` <= 5 and `vertical_m` <= 5. If SITL settles outside 5 m
vertically (EKF altitude error or WPNAV behaviour), raise `ARRIVAL_VERTICAL_M` and record the
value in D2-P2.

## 3. After merge

Nothing to change in `platform/mission`: `MISSION_ARRIVAL` stays `"false"`. Goto tasks complete
on the bridge's report.
