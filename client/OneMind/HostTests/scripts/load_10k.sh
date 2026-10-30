#!/bin/bash
# 10,000-entity client-core check on the build machine: a disposable nats-server, the
# synthetic load (Tools/entity_load.py, 10,000 sim-load-* drones moving at 1 Hz into ENT)
# and the headless client core on the fleet layer (direct ENT path) over WebSocket.
# Proves the bus and core side of B4/B5; the 60 fps render check is the owner's (S1 Mac).
# Usage: load_10k.sh <headless-binary> <work-dir> [extra headless args, e.g. --camera-aoi ...]
set -euo pipefail
HEADLESS=$1; RUN=$2; shift 2
HERE=$(cd "$(dirname "$0")" && pwd)
TOOLS=${ONEMIND_TOOLS:-${ONEMIND_WORK:-$(cd "$HERE/../../../../.." && pwd)}/.tools}
PY=${ONEMIND_PYTHON:-$TOOLS/py/bin/python}; [ -x "$PY" ] || PY=python3
NATS_SERVER_BIN=${NATS_SERVER_BIN:-$TOOLS/bin/nats-server}; [ -x "$NATS_SERVER_BIN" ] || NATS_SERVER_BIN=nats-server
rm -rf "$RUN"; mkdir -p "$RUN/js"
free_port() { "$PY" -I -c 'import socket;s=socket.socket();s.bind(("127.0.0.1",0));print(s.getsockname()[1])'; }
P=$(free_port); W=$(free_port)
cat > "$RUN/nats.conf" <<CONF
listen: 127.0.0.1:$P
jetstream { store_dir: "$RUN/js", max_file_store: 2GB, max_memory_store: 256MB }
websocket { listen: "127.0.0.1:$W", no_tls: true }
CONF
"$NATS_SERVER_BIN" -c "$RUN/nats.conf" > "$RUN/server.log" 2>&1 & SRV=$!
trap 'kill $SRV 2>/dev/null; wait $SRV 2>/dev/null; rm -rf "$RUN/js"' EXIT
sleep 1
echo "\$ entity_load.py --count 10000 --rate 1 --duration 20  (background)"
"$PY" -I "$HERE/../../Tools/entity_load.py" --url "nats://127.0.0.1:$P" --count 10000 --rate 1 --duration 20 > "$RUN/load.log" 2>&1 & LOAD=$!
sleep 3
echo "\$ onemind_headless --layer fleet:drone,robot,vehicle,node:direct --duration 15 $*"
"$HEADLESS" --url "ws://127.0.0.1:$W" --layer fleet:drone,robot,vehicle,node:direct --duration 15 --print-every 3 "$@" | grep -v '^\[headless\] final fleet '
wait $LOAD || true
echo "--- entity_load.py"; cat "$RUN/load.log"
