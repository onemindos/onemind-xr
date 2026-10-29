#!/bin/bash
set -u
export PATH=/Users/danielcarku/Desktop/work/.tools/bin:$PATH
S=$(dirname "$0"); WT=/Users/danielcarku/Desktop/work/wt/a4-entity/services/entity
ST=/Users/danielcarku/Desktop/work/.run/tmp-a4/e2e; rm -rf "$ST"; mkdir -p "$ST"
PORT=$(python3 -c 'import socket;s=socket.socket();s.bind(("127.0.0.1",0));print(s.getsockname()[1])')
URL=nats://127.0.0.1:$PORT
echo "\$ nats-server -js -a 127.0.0.1 -p $PORT -sd $ST  ($(nats-server --version))"
nats-server -js -a 127.0.0.1 -p "$PORT" -sd "$ST" >"$ST/server.log" 2>&1 & SRV=$!
sleep 1
echo "\$ create input streams COT TEL WORLD INTEL SYS, ENT (CR settings, R1) and KV entity-correlation"
for x in "COT tak.cot.>" "TEL tel.fleet.>" "WORLD evt.world.>" "INTEL intel.>" "SYS evt.sys.>"; do set -- $x
  nats -s $URL stream add "$1" --subjects "$2" --storage file --replicas 1 --retention limits --discard old --max-age 72h --dupe-window 2m --max-msgs=-1 --max-bytes=-1 --max-msg-size=-1 --max-msgs-per-subject=-1 --max-consumers=-1 --no-allow-rollup --no-deny-delete --no-deny-purge --allow-direct --defaults >/dev/null && echo "  stream $1 ($2) created"; done
nats -s $URL stream add --config "$S/ent.json" >/dev/null && echo "  stream ENT created from ent.json"
nats -s $URL kv add entity-correlation --history 5 --storage file --replicas 1 >/dev/null && echo "  kv entity-correlation created"
T0=$(( $(date +%s) * 1000 - 45000 )); python3 "$WT/testdata/fixtures/gen_fixtures.py" $T0 > "$ST/live.jsonl"
echo "\$ python3 gen_fixtures.py $T0 > live.jsonl  (fixtures dated 45 s before now); python3 pub.py live.jsonl"
python3 "$S/pub.py" "$URL" "$ST/live.jsonl"
nats -s $URL sub 'evt.sys.entity.rejected' --raw > "$ST/rejected.txt" 2>/dev/null & SUB=$!
(cd "$WT" && GOPATH=/Users/danielcarku/Desktop/work/.tools/gopath GOMODCACHE=/Users/danielcarku/Desktop/work/.tools/gopath/pkg/mod go build -o "$ST/entity-service" ./cmd/entity-service)
echo "\$ NATS_URL=$URL START_LOOKBACK=0 HTTP_ADDR=127.0.0.1:18080 entity-service   (run 1)"
NATS_URL=$URL START_LOOKBACK=0 HTTP_ADDR=127.0.0.1:18080 POD_NAME=e2e-1 "$ST/entity-service" > "$ST/run1.log" 2>&1 & SVC=$!
sleep 3
echo "\$ curl /readyz"; curl -s http://127.0.0.1:18080/readyz
echo "\$ curl /metrics"; curl -s http://127.0.0.1:18080/metrics
echo "\$ nats kv get entity-correlation lease.entity-service --raw"; nats -s $URL kv get entity-correlation lease.entity-service --raw; echo
echo "\$ nats stream info ENT --json | state"; nats -s $URL stream info ENT --json | python3 -c 'import sys,json;d=json.load(sys.stdin);s=d["state"];print({k:s[k] for k in ["messages","num_subjects","last_seq"]}, "config:", {k:d["config"].get(k) for k in ["max_msgs_per_subject","allow_msg_ttl","subject_delete_marker_ttl","allow_direct"]})'
echo "\$ nats req svc.entity.get '{\"entity_id\":\"icao-a1b2c3\"}'  (selected fields)"
nats -s $URL req svc.entity.get '{"entity_id":"icao-a1b2c3"}' --raw 2>/dev/null | python3 -c 'import sys,json;e=json.load(sys.stdin);print(json.dumps({k:e[k] for k in ["entity_id","entity_type","seq","lat","lon","confidence","confidence_parts","lifecycle","live","expiry_t"]})); print("sources:",[(s["integration"],s["stream"],s["seq"]) for s in e["sources"]])'
echo "\$ nats req svc.entity.query '{\"types\":[\"vessel\",\"drone\",\"person\"]}'"
nats -s $URL req svc.entity.query '{"types":["vessel","drone","person"]}' --raw 2>/dev/null | python3 -c 'import sys,json;[print(" ",e["entity_id"],e["entity_type"],"seq",e["seq"],"tlp",e["marking"]["tlp"],"egress_tak",e["indicators"]["egress_tak"]) for e in json.load(sys.stdin)["entities"]]'
echo "\$ nats stream get ENT --last-for ent.drone.sim-x1.state (headers)"
nats -s $URL stream get ENT --last-for ent.drone.sim-x1.state 2>/dev/null | grep -E 'Nats-Msg-Id|Nats-TTL|OneMind-' 
echo "\$ dead letters on evt.sys.entity.rejected:"; cat "$ST/rejected.txt"
kill $SVC; wait $SVC 2>/dev/null
LAST1=$(nats -s $URL stream info ENT --json | python3 -c 'import sys,json;print(json.load(sys.stdin)["state"]["last_seq"])')
echo "\$ restart (run 2, same durables, nothing new to read). Expect no record for any already-processed input; a lifecycle change (active->stale) caused by the wall clock is a new seq, not a duplicate."
NATS_URL=$URL START_LOOKBACK=0 HTTP_ADDR=127.0.0.1:18080 POD_NAME=e2e-2 "$ST/entity-service" > "$ST/run2.log" 2>&1 & SVC=$!
sleep 3
curl -s http://127.0.0.1:18080/metrics | grep -E 'restored|published|entities'
LAST2=$(nats -s $URL stream info ENT --json | python3 -c 'import sys,json;print(json.load(sys.stdin)["state"]["last_seq"])')
echo "ENT last_seq before restart=$LAST1 after restart=$LAST2"
[ "$LAST2" -gt "$LAST1" ] && for q in $(seq $((LAST1+1)) $LAST2); do echo "\$ nats stream get ENT $q  (record published after the restart)"; nats -s $URL stream get ENT $q --json 2>/dev/null | python3 -c 'import sys,json,base64;m=json.load(sys.stdin);e=json.loads(base64.b64decode(m["data"]));print(" ",m["subject"],"seq",e["seq"],"lifecycle",e["lifecycle"],"t",e["t"],"stale",e["stale"],"confidence",e["confidence"])'; done
kill $SVC $SUB; wait $SVC 2>/dev/null
grep -h -E '"msg":"(restored entities|lease acquired)"' "$ST/run1.log" "$ST/run2.log"
kill $SRV; wait $SRV 2>/dev/null
