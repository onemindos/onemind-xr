# E-MAV owner steps: verify `hold` against real autopilots (SITL)

These steps check the hold mapping on real autopilots. They need a machine with PX4 or ArduPilot SITL, which this run did not have. The hold mapping is `MAV_CMD_DO_REPOSITION` to the current position, sent over COMMAND_INT, frame GLOBAL, with `CHANGE_MODE` set. Branch: omos `feat/mavlink-bridge-fleet` (#550), commit 118978a0 or later.

## 1. ArduPilot Copter SITL

```bash
sim_vehicle.py -v ArduCopter --out=udp:127.0.0.1:14550 -L CMAC   # in the ardupilot checkout
cd omos/services/bridge/bridges/mavlink && go build -o bridge-mavlink .
ASSET_ID=sim-x1 SIG_REQUIRED=false GEOFENCE_FILE=geofence.example.json ./bridge-mavlink
```

The bridge has no command-line client, so send arm, takeoff, goto and then hold as approved commands. The simplest way is the `-tags e2e` harness, with `MAVLINK_ENDPOINT=udpserver:127.0.0.1:14550`, `OUT_VERSION=2` and `SIM_DRONE` unset (see `test/e2e.sh`). Move the geofence keep-in to the SITL home location (CMAC) first.

Expected results:
- A hold sent during a goto is `acknowledged,executed`, with `mavlink.command` = `MAV_CMD_DO_REPOSITION` and `result` 0.
- The vehicle mode becomes or stays GUIDED.
- `tel.fleet.drone.sim-x1.position` shows the speed falling to about 0 within a few seconds. The position stays within about 5 m of where the hold was sent.
- Altitude: confirm that z (MSL, frame GLOBAL) is held and the vehicle does not climb or sink. If ArduPilot rejects COMMAND_INT, check that the COMMAND_LONG fallback is used and is accepted.

## 2. PX4 SITL (multicopter)

```bash
make px4_sitl gz_x500     # PX4 checkout; MAVLink GCS link on udp 14550
```

Run the same sequence as in step 1.

Expected results:
- The hold is `executed`.
- The nav state becomes Hold (AUTO_LOITER). In the bridge status, `meta.mode` or the custom mode should show Hold.
- The vehicle loiters at the reported point.

## 3. Abort path through policy (with #554)

Run the #568 e2e harness or the policy service. Propose and confirm a `task.abort` for a running asset task on sim-x1.

Expected results:
- `cmd.fleet.drone.sim-x1.hold` carries `{"reason":"abort"}` and the task.abort `params_hash`.
- The bridge reports `executed`.
- `evt.drone.sim-x1.task` reports `state: done_not_ok`, `error.code: cancelled`.

## Known limits to judge

- `duration_s` is validated but not enforced. The vehicle holds until its next command.
- A hold on the ground is reported as `vehicle_rejected` if the autopilot refuses DO_REPOSITION while landed.
