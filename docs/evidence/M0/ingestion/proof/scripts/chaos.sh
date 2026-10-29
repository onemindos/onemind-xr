#!/bin/sh
# Chaos run under load (AIS WebSocket at 50 msg/s plus 10 other sources).
# usage: chaos.sh bento|go|rpc  -> results/<engine>-chaos/
# Timeline (seconds from engine start):
#   0     start engine
#   45    kill -9 nats-server; restart at 70           (NATS down 25 s)
#   110   upstream stub HTTP endpoints return 503       (upstream down 25 s); snapshot status at 130
#   135   upstream ok
#   160   kill -9 collector; restart at 190             (collector down 30 s > 20 s dupe window)
#   220   upstream serves malformed records for 15 s
#   235   upstream ok
#   300   verify (ids emitted in the last 40 s are ignored)
E=$1
W=/Users/danielcarku/Desktop/work
P=$W/onemind-xr/docs/evidence/M0/ingestion/proof
OUT=$W/onemind-xr/docs/evidence/M0/ingestion/results/$E-chaos
S=$P/scripts/proof.sh
R=$W/.run/ingest
mkdir -p "$OUT"
T0=$(date +%s)
mark() { echo "$(( $(date +%s) - T0 ))s $(date -u +%H:%M:%SZ) $*" | tee -a "$OUT/timeline.txt"; }
until_t() { while [ $(( $(date +%s) - T0 )) -lt "$1" ]; do sleep 1; done; }
metrics() {
  if [ "$E" = go ]; then curl -s 127.0.0.1:38196/metrics; else curl -s 127.0.0.1:38195/metrics | grep -E '^(output_sent|output_error|input_received|processor_error)' | grep -E 'stream="(01|04|05|06)' ; fi
}
: > "$OUT/timeline.txt"
$S reset; $S stub; sleep 1
T0=$(date +%s)
mark "start engine $E"; $S "$E"
until_t 45;  mark "NATS kill -9"; $S nats-down
until_t 55;  mark "webhook responses while NATS down:"; tail -4 $R/emitted/webhook-responses.log >> "$OUT/timeline.txt"
until_t 70;  mark "NATS restart"; $S nats-up
until_t 110; mark "upstream down (503)"; curl -s "127.0.0.1:38080/control?mode=down" >/dev/null
until_t 130; mark "status snapshot during upstream outage"
$W/.tools/py/bin/python -I $P/scripts/verify.py --emit-dir $R/emitted --label snap 2>/dev/null \
  | /usr/bin/python3 -c "import json,sys;d=json.load(sys.stdin);print(json.dumps(d['latestStatus'],indent=1))" > "$OUT/status-during-upstream-outage.json"
until_t 135; mark "upstream ok"; curl -s "127.0.0.1:38080/control?mode=ok" >/dev/null
until_t 160; mark "collector kill -9"; metrics > "$OUT/metrics-before-collector-kill.txt"; SIG=-KILL $S stop-engine
until_t 190; mark "collector restart"; $S "$E"
until_t 220; mark "upstream malformed"; curl -s "127.0.0.1:38080/control?mode=bad" >/dev/null
until_t 235; mark "upstream ok"; curl -s "127.0.0.1:38080/control?mode=ok" >/dev/null
until_t 300; mark "verify"
metrics > "$OUT/metrics-end.txt"
$S verify "$E-chaos" > "$OUT/verify.json" 2>&1
cp $R/emitted/webhook-responses.log "$OUT/webhook-responses.log"
grep -i -E '"level":"(warn|error)"|level=(warn|error)' $R/logs/engine-$E*.log | cut -c1-400 | sort | uniq -c | sort -rn | head -40 > "$OUT/engine-warnings.txt"
mark "stop engine"; $S stop-engine
sleep 1; cat $R/logs/engine-$E*.log | gzip -9 > "$OUT/engine.log.gz"
mark done
