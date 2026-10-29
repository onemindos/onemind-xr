#!/usr/bin/env bash
# E-ENT live check: disposable nats-server + entity-service built from feat/entity-service.
set -u
export PATH=/Users/danielcarku/Desktop/work/.tools/bin:$PATH GOPATH=/Users/danielcarku/Desktop/work/.tools/gopath GOMODCACHE=/Users/danielcarku/Desktop/work/.tools/gopath/pkg/mod
TMP=/Users/danielcarku/Desktop/work/.run/tmp-e-ent; rm -rf $TMP; mkdir -p $TMP
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
show(){ $N req svc.entity.get "{\"entity_id\":\"$1\"}" --raw 2>&1 | python3 -c 'import json,sys;e=json.load(sys.stdin);print(json.dumps({k:e.get(k) for k in ["entity_id","lifecycle","cot_type","marking","health"]}))'; }
echo "# E-ENT live check $(date -u +%FT%TZ)"
echo "\$ nats pub tel.fleet.drone.sim-x1.position (stale 3 s, meta.connection online)"
$N pub tel.fleet.drone.sim-x1.position "{\"uid\":\"sim-x1\",\"type\":\"fleet.drone\",\"lat\":30.2672,\"lon\":-97.7431,\"hae\":152.4,\"course\":-1,\"speed\":0,\"callsign\":\"SIM-X1\",\"stale\":3,\"t\":$NOW,\"meta\":{\"battery_pct\":80,\"connection\":\"online\",\"kind\":\"multirotor\",\"simulated\":true}}" >/dev/null 2>&1
$N pub tel.fleet.robot.sim-tb1.position "{\"uid\":\"sim-tb1\",\"type\":\"fleet.robot\",\"lat\":30.27,\"lon\":-97.74,\"hae\":150,\"stale\":30,\"t\":$NOW,\"meta\":{\"kind\":\"ugv\",\"connection\":\"online\",\"simulated\":true}}" >/dev/null 2>&1
$N pub tel.fleet.drone.x47.position "{\"uid\":\"x47\",\"type\":\"fleet.drone\",\"lat\":30.1,\"lon\":-97.7,\"stale\":30,\"t\":$NOW,\"meta\":{\"connection\":\"online\",\"kind\":\"fixed_wing\"}}" >/dev/null 2>&1
sleep 1
echo "## fresh"; show sim-x1; show sim-tb1; show fleet-x47
echo "## ENT headers for sim-x1"; $N stream get ENT --last-for ent.drone.sim-x1.state 2>&1 | grep -i "OneMind-"
sleep 4
echo "## after 4 s of silence (stale=3 s)"; show sim-x1
echo "## unknown id: nats req svc.entity.get {\"entity_id\":\"nope\"}"
$N req svc.entity.get '{"entity_id":"nope"}' 2>&1 | grep -v "^[0-9:]* Sending" 
kill $EPID $NPID; wait 2>/dev/null; rm -rf $TMP
