#!/bin/sh
# Redpanda Connect 4.115.1 runs the unchanged Bento stream configs for 120 s. -> results/rpc-compat/
W=/Users/danielcarku/Desktop/work
P=$W/onemind-xr/docs/evidence/M0/ingestion/proof
OUT=$W/onemind-xr/docs/evidence/M0/ingestion/results/rpc-compat
S=$P/scripts/proof.sh
mkdir -p "$OUT"
$S reset; $S stub; sleep 1
$W/.tools/bin/redpanda-connect --version > "$OUT/version.txt"
$S rpc; sleep 120
$S verify rpc-compat > "$OUT/verify.json" 2>&1
grep -i -E '"level":"(warn|error)"|level=(warn|error)' $W/.run/ingest/logs/engine-rpc.log | cut -c1-300 | sort | uniq -c | sort -rn | head -20 > "$OUT/engine-warnings.txt"
$S stop-engine
