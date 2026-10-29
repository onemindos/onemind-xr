#!/bin/sh
# Control script for the ingestion proof. All processes bind 127.0.0.1 and use the
# dedicated nats-server on :34222 (never the shared :4222 server).
# usage: proof.sh reset|stub|bento|rpc|go|stop-engine|nats-up|nats-down|verify LABEL|report
W=/Users/danielcarku/Desktop/work
P=$W/onemind-xr/docs/evidence/M0/ingestion/proof
R=$W/.run/ingest
N="$W/.tools/bin/nats -s nats://127.0.0.1:34222"
mkdir -p $R/logs $R/emitted
cd $R || exit 1
case "$1" in
  reset)
    [ -f $R/stub.pid ] && kill "$(cat $R/stub.pid)" 2>/dev/null
    sleep 1
    rm -f $R/emitted/* $R/logs/engine-*.log
    for s in INTEL WORLD TEL FEEDEVT; do $N stream purge $s --force >/dev/null; done
    # checkpoints must be reset together with the streams and the upstream ids (run-2 lesson)
    $N kv rm ingest-state -f >/dev/null 2>&1; $N kv add ingest-state --history 1 --storage file --replicas 1 >/dev/null
    $N kv rm ingest-locks -f >/dev/null 2>&1; $N kv add ingest-locks --history 1 --ttl 10s --storage file --replicas 1 >/dev/null
    $N kv rm overwatch-video -f >/dev/null 2>&1; $N kv add overwatch-video --history 1 --ttl 5m --storage file --replicas 1 >/dev/null
    ;;
  stub)
    nohup $W/.tools/py/bin/python -I $P/stubs/upstream_stub.py --emit-dir $R/emitted --ais-rate "${AIS_RATE:-50}" \
      --webhook-url http://127.0.0.1:38090/webhook/cap > $R/logs/stub.log 2>&1 &
    echo $! > $R/stub.pid
    ;;
  bento)
    nohup $W/.tools/bin/bento -c $P/bento/main.yaml -r $P/bento/resources.yaml streams $P/bento/streams/*.yaml \
      >> $R/logs/engine-bento.log 2>&1 &
    echo $! > $R/engine.pid
    ;;
  rpc)
    nohup $W/.tools/bin/redpanda-connect -c $P/bento/main.yaml -r $P/bento/resources.yaml streams $P/bento/streams/*.yaml \
      >> $R/logs/engine-rpc.log 2>&1 &
    echo $! > $R/engine.pid
    ;;
  go)
    nohup $W/.tools/bin/ingestd-proof -nats nats://127.0.0.1:34222 -instance "${INSTANCE:-a}" -metrics "${METRICS:-127.0.0.1:38196}" \
      >> $R/logs/engine-go-${INSTANCE:-a}.log 2>&1 &
    echo $! > $R/engine${INSTANCE:+-$INSTANCE}.pid
    ;;
  stop-engine)
    f=$R/engine${INSTANCE:+-$INSTANCE}.pid
    [ -f "$f" ] && kill "${SIG:--TERM}" "$(cat "$f")" 2>/dev/null
    ;;
  nats-down)
    kill -KILL "$(cat $R/nats-ingest.pid)"
    ;;
  nats-up)
    nohup $W/.tools/bin/nats-server -c $P/scripts/nats-ingest.conf >> $R/logs/nats-ingest.log 2>&1 &
    echo $! > $R/nats-ingest.pid
    ;;
  verify)
    $W/.tools/py/bin/python -I $P/scripts/verify.py --emit-dir $R/emitted --label "$2"
    ;;
esac
