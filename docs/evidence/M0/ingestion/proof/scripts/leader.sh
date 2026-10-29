#!/bin/sh
# Single owner per source: two Go collector instances (a, b) run at once; only the lease holder
# publishes. Kill -9 the leader; the other takes over after the lease TTL (10 s). Then verify no
# duplicates and no gaps beyond the takeover window.  -> results/go-leader/
W=/Users/danielcarku/Desktop/work
P=$W/onemind-xr/docs/evidence/M0/ingestion/proof
OUT=$W/onemind-xr/docs/evidence/M0/ingestion/results/go-leader
S=$P/scripts/proof.sh
R=$W/.run/ingest
N="$W/.tools/bin/nats -s nats://127.0.0.1:34222"
mkdir -p "$OUT"; : > "$OUT/timeline.txt"
T0=$(date +%s)
mark() { echo "$(( $(date +%s) - T0 ))s $(date -u +%H:%M:%SZ) $*" | tee -a "$OUT/timeline.txt"; }
until_t() { while [ $(( $(date +%s) - T0 )) -lt "$1" ]; do sleep 1; done; }
$S reset; $S stub; sleep 1
T0=$(date +%s)
mark "start instance a"; INSTANCE=a METRICS=127.0.0.1:38196 $S go
until_t 3; mark "start instance b"; INSTANCE=b METRICS=127.0.0.1:38197 $S go
until_t 30; mark "leases:"; $N kv ls ingest-locks 2>/dev/null >/dev/null
for k in usgs nws eonet ais cap mqtt celestrak gdacs outages tiles mediamtx; do printf '%s=%s ' $k "$($N kv get ingest-locks $k --raw 2>/dev/null)"; done | tee -a "$OUT/timeline.txt"; echo >> "$OUT/timeline.txt"
curl -s 127.0.0.1:38196/metrics | grep -E 'leader|published_total' > "$OUT/metrics-a-before.txt"
curl -s 127.0.0.1:38197/metrics | grep -E 'leader|published_total' > "$OUT/metrics-b-before.txt"
until_t 35; mark "kill -9 instance a (leader of most sources)"; INSTANCE=a SIG=-KILL $S stop-engine
until_t 60; mark "leases after takeover:"
for k in usgs nws eonet ais cap mqtt celestrak gdacs outages tiles mediamtx; do printf '%s=%s ' $k "$($N kv get ingest-locks $k --raw 2>/dev/null)"; done | tee -a "$OUT/timeline.txt"; echo >> "$OUT/timeline.txt"
until_t 120; mark verify
curl -s 127.0.0.1:38197/metrics > "$OUT/metrics-b-end.txt"
grep -E '"lease' $R/logs/engine-go-a.log $R/logs/engine-go-b.log | cut -c1-300 > "$OUT/lease-log.txt"
$S verify go-leader > "$OUT/verify.json" 2>&1
INSTANCE=b $S stop-engine
mark done
