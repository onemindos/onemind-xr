#!/usr/bin/env bash
set -u
export PATH=/Users/danielcarku/Desktop/work/.tools/bin:$PATH GOPATH=/Users/danielcarku/Desktop/work/.tools/gopath GOMODCACHE=/Users/danielcarku/Desktop/work/.tools/gopath/pkg/mod
EV=/Users/danielcarku/Desktop/work/onemind-xr/docs/evidence/INT/CAP; TMP=/Users/danielcarku/Desktop/work/.run/tmp-cap; rm -rf $TMP; mkdir -p $TMP
PORT=$(python3 -c 'import socket;s=socket.socket();s.bind(("127.0.0.1",0));print(s.getsockname()[1])'); URL=nats://127.0.0.1:$PORT
nats-server -js -a 127.0.0.1 -p $PORT -sd $TMP/js > $TMP/nats.log 2>&1 & NPID=$!
sleep 1
N="nats -s $URL"
while read name subj; do $N stream add $name --subjects "$subj" --storage file --defaults >/dev/null; done <<EOF
TEL tel.fleet.>
COT tak.cot.>
WORLD evt.world.>
INTEL intel.>
SYS evt.sys.>
EOF
$N stream add ENT --subjects 'ent.*.*.state' --storage file --max-msgs-per-subject 1 --allow-direct --allow-msg-ttl --defaults >/dev/null
$N kv add entity-correlation --history 5 >/dev/null
(cd /Users/danielcarku/Desktop/work/wt/a4-entity/services/entity && go build -o $TMP/entity-service ./cmd/entity-service)
NATS_URL=$URL HTTP_ADDR=127.0.0.1:0 START_LOOKBACK=0 $TMP/entity-service > $TMP/entity.log 2>&1 & EPID=$!
sleep 3
NOW=$(python3 -c 'import time;print(int(time.time()*1000))')
pub(){ echo "\$ nats pub $1 '$2'"; $N pub "$1" "$2" 2>&1; }
{
echo "# INT/CAP cross-PR e2e $(date -u +%FT%TZ): disposable nats-server, entity-service built from feat/entity-service"
pub tel.fleet.drone.sim-x1.position "{\"uid\":\"sim-x1\",\"type\":\"fleet.drone\",\"lat\":30.2672,\"lon\":-97.7431,\"hae\":152.4,\"course\":-1,\"speed\":0,\"callsign\":\"SIM-X1\",\"stale\":60,\"t\":$NOW,\"meta\":{\"battery_pct\":80,\"mode\":\"GUIDED\",\"kind\":\"multirotor\",\"simulated\":true}}"
pub tel.fleet.drone.sim-x1.status "{\"uid\":\"sim-x1\",\"type\":\"fleet.drone\",\"lat\":30.2672,\"lon\":-97.7431,\"hae\":152.4,\"course\":-1,\"speed\":0,\"callsign\":\"SIM-X1\",\"stale\":60,\"t\":$((NOW+100)),\"meta\":{\"battery_pct\":80,\"mode\":\"GUIDED\",\"armed\":true,\"connection\":\"online\",\"link_rssi\":-1,\"kind\":\"multirotor\",\"simulated\":true,\"commands_accepted\":\"goto,rtl,land,arm,disarm,estop,takeoff\"}}"
pub tel.fleet.robot.sim-tb1.position "{\"uid\":\"sim-tb1\",\"type\":\"fleet.robot\",\"lat\":30.27,\"lon\":-97.74,\"hae\":150,\"course\":-1,\"speed\":0,\"callsign\":\"SIM-TB1\",\"stale\":30,\"t\":$NOW,\"meta\":{\"kind\":\"ugv\",\"battery_pct\":90,\"simulated\":true}}"
pub tel.fleet.robot.sim-tb1.status "{\"uid\":\"sim-tb1\",\"type\":\"fleet.robot\",\"lat\":30.27,\"lon\":-97.74,\"hae\":150,\"course\":-1,\"speed\":0,\"callsign\":\"SIM-TB1\",\"stale\":30,\"t\":$((NOW+100)),\"meta\":{\"kind\":\"ugv\",\"battery_pct\":90,\"mode\":\"idle\",\"connection\":\"online\",\"link_rssi\":-1,\"simulated\":true,\"commands_accepted\":\"goto,hold,rtl,estop,estop_release\"}}"
pub tel.fleet.drone.x47.position "{\"uid\":\"x47\",\"type\":\"fleet.drone\",\"lat\":30.1,\"lon\":-97.7,\"stale\":60,\"t\":$NOW,\"meta\":{\"battery_pct\":70}}"
pub tel.fleet.drone.x47.status "{\"uid\":\"x47\",\"type\":\"fleet.drone\",\"lat\":30.1,\"lon\":-97.7,\"stale\":60,\"t\":$((NOW+100)),\"meta\":{\"battery_pct\":70,\"connection\":\"online\",\"commands_accepted\":\"goto,rtl,land\"}}"
sleep 2
for q in '{"entity_type":"drone","entity_id":"sim-x1"}' '{"entity_type":"robot","entity_id":"sim-tb1"}' '{"entity_type":"drone","entity_id":"fleet-x47"}'; do
 echo "\$ nats req svc.entity.get '$q'"; $N req svc.entity.get "$q" --raw 2>&1 | python3 -c 'import json,sys;d=json.load(sys.stdin);e=d.get("entity",d);print(json.dumps({k:e.get(k) for k in ["entity_id","entity_type","kind","traits","capabilities","health","indicators"]}))'
done
echo "## policy #554 resolver NATSEntities.Get + checkAsset rules (temporary test file in feat/policy-service worktree, removed after the run, not committed)"
(cd /Users/danielcarku/Desktop/work/wt/i4-policy/services/policy && CAP_E2E_NATS=$URL go test ./internal/policy -run TestCapE2E -v -count=1 2>&1)
} > $EV/e2e-policy-gate.txt 2>&1
kill $EPID $NPID; wait 2>/dev/null
cp $TMP/entity.log $EV/e2e-entity-service.log; rm -rf $TMP
