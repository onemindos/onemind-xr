#!/usr/bin/env bash
# D1/D6 end-to-end on a disposable nats-server: MAVLink sim -> H2 bridge (#550) -> entity
# service (#552) -> geofence service (this PR) -> alerts; projector (#563) reads the zones.
set -u
export PATH=/Users/danielcarku/Desktop/work/.tools/bin:$PATH
B=/Users/danielcarku/Desktop/work/.run/tmp-d1/bin
export E2E_DIR=/Users/danielcarku/Desktop/work/.run/tmp-d1/e2e
rm -rf "$E2E_DIR"; mkdir -p "$E2E_DIR/js" "$E2E_DIR/logs"
PORT=4291; export NATS_URL=nats://127.0.0.1:$PORT
D="$B/e2e-driver"
pids=()
cleanup() { for p in "${pids[@]}"; do kill "$p" 2>/dev/null; done; wait 2>/dev/null; }
trap cleanup EXIT
nats-server -js -a 127.0.0.1 -p $PORT -sd "$E2E_DIR/js" > "$E2E_DIR/logs/nats.log" 2>&1 & pids+=($!)
sleep 1
"$D" setup || exit 1
LEASE_BUCKET=none START_LOOKBACK=0 HTTP_ADDR=127.0.0.1:18081 "$B/entity-service" > "$E2E_DIR/logs/entity.log" 2>&1 & pids+=($!)
start_geofence() {
  POLICY_KEYS_DIR="$E2E_DIR/keys" TICK=1s FEED_SILENCE=20s HTTP_ADDR=127.0.0.1:18082 "$B/geofence-service" >> "$E2E_DIR/logs/geofence.log" 2>&1 &
  GF=$!; pids+=($GF)
}
start_geofence
HTTP_ADDR=127.0.0.1:18083 "$B/tak-projector" > "$E2E_DIR/logs/projector.log" 2>&1 & pids+=($!)
/Users/danielcarku/Desktop/work/.tools/py/bin/python /private/tmp/claude-501/-Users-danielcarku-Desktop-ONEMIND/28794bd7-008f-413a-92c6-5afd0f9c2e20/scratchpad/e2e/sim_airborne.py \
  --out udpout:127.0.0.1:14651 --lat 30.2672 --lon -97.7431 --radius 150 --alt 60 > "$E2E_DIR/logs/sim.log" 2>&1 & pids+=($!)
ASSET_ID=sim-x1 MAVLINK_ENDPOINT=udpserver:127.0.0.1:14651 POLICY_PUBKEYS="policy-e2e=$(cat "$E2E_DIR/policy-e2e.pub.b64")" \
  "$B/bridge-mavlink" > "$E2E_DIR/logs/bridge.log" 2>&1 & pids+=($!)
sleep 3
echo "NOTE: the MAVLink sim is a scratch copy of onemind-xr scripts/sim/mavlink_sim_drone.py that starts armed and airborne (one-line diff in this evidence dir), because arm and takeoff are asset commands that need the real policy service and a human confirm, which this run does not fake."
echo "== phase 1: AOI drives what TAK receives (D1)"; "$D" aoi || exit 1
echo "== phase 2: sim-x1 leaves the keep-in (D6)"; "$D" keepin || exit 1
echo "== restart the geofence service while the alert is open"
kill "$GF"; wait "$GF" 2>/dev/null; sleep 1; start_geofence; sleep 2
curl -s 127.0.0.1:18082/readyz
echo "== phase 3: after the restart"; "$D" after-restart || exit 1
echo "== phase 4: feed outage (D6)"; "$D" feed || exit 1
echo "== phase 5: unsigned zone write refused"; "$D" rogue || exit 1
echo "== verify invariants over the whole run"; "$D" verify || exit 1
echo "== geofence service metrics"; curl -s 127.0.0.1:18082/metrics
echo "== geofence service log (zone commands, restore)"; grep -E '"msg":"(zone command|restored|geofence service ready)"' "$E2E_DIR/logs/geofence.log"
echo "E2E PASS"
