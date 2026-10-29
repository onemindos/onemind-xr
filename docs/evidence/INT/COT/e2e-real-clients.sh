#!/bin/bash
# svc.cot.send end to end with the real clients' code against the real projector binary
# (omos #563, branch feat/projector-aoi):
#   - Unreal client core: onemind_headless (onemind-xr branch run/m2-client, HostTests) over
#     WebSocket as CLIENT user zeus, --send-marker and --send-chat (CotSendRequest::ToJson).
#   - L0 web client: the body is produced by web/src/act/chat.ts geochatRequest itself (the
#     function source is extracted and run with node --experimental-strip-types) and sent as
#     CLIENT users over NATS (nats-py; the venv has no aiohttp, so TCP, not WebSocket).
# Disposable nats-server with the omos #562 account layout (ONEMIND exports svc.cot.send to
# CLIENT, CLIENT imports it with share: true). Passwords are generated per run in a temp dir.
# Usage: e2e-real-clients.sh <onemind_headless> <tak-projector> <chat.ts> <out.txt>
set -uo pipefail
HEADLESS=$1 PROJ=$2 CHAT_TS=$3 OUT=$4
TOOLS=/Users/danielcarku/Desktop/work/.tools
PY=$TOOLS/py/bin/python
TMP=$(mktemp -d /Users/danielcarku/Desktop/work/.run/tmp-COT/e2e-XXXX)
trap 'kill $(jobs -p) 2>/dev/null; wait 2>/dev/null; rm -rf "$TMP"' EXIT
pw() { openssl rand -hex 12; }
P_PROJ=$(pw) P_ONE=$(pw) P_ZEUS=$(pw) P_CAROL=$(pw)
PORT=$($PY -c 'import socket;s=socket.socket();s.bind(("127.0.0.1",0));print(s.getsockname()[1])')
WS=$($PY -c 'import socket;s=socket.socket();s.bind(("127.0.0.1",0));print(s.getsockname()[1])')
HTTP=$($PY -c 'import socket;s=socket.socket();s.bind(("127.0.0.1",0));print(s.getsockname()[1])')
umask 077
cat > "$TMP/nats.conf" <<EOF
listen: "127.0.0.1:$PORT"
websocket { listen: "127.0.0.1:$WS", no_tls: true }
jetstream { store_dir: "$TMP/js" }
accounts {
  ONEMIND {
    jetstream: enabled
    users: [ {user: projector, password: "$P_PROJ"}, {user: onemind, password: "$P_ONE"} ]
    exports: [ {service: "svc.cot.send", accounts: [CLIENT]} ]
  }
  CLIENT {
    users: [ {user: zeus, password: "$P_ZEUS"}, {user: carol, password: "$P_CAROL"} ]
    imports: [ {service: {account: ONEMIND, subject: "svc.cot.send"}, share: true} ]
  }
}
EOF
exec > >(tee "$OUT") 2>&1
echo "== $(date -u +%FT%TZ) svc.cot.send e2e with real client code"
"$TOOLS/bin/nats-server" -c "$TMP/nats.conf" > "$TMP/nats.log" 2>&1 &
sleep 0.5
NATS_URL=nats://127.0.0.1:$PORT NATS_USER=projector NATS_PASSWORD=$P_PROJ ENT_MODE=core HTTP_ADDR=127.0.0.1:$HTTP \
  "$PROJ" > "$TMP/projector.log" 2>&1 &
# Recorder of tak.cot.inject and tak.cot.mesh.tx (ONEMIND account).
cat > "$TMP/rec.py" <<'EOF'
import asyncio, json, os, sys
import nats
async def main():
    nc = await nats.connect(os.environ["URL"], user="onemind", password=os.environ["PW"])
    out = open(sys.argv[1], "w")
    async def cb(m):
        out.write(json.dumps({"subject": m.subject, "headers": m.headers or {}, "xml": m.data.decode()}) + "\n"); out.flush()
    await nc.subscribe("tak.cot.inject", cb=cb); await nc.subscribe("tak.cot.mesh.tx", cb=cb)
    await nc.flush()
    open(sys.argv[1] + ".ready", "w").close()
    await asyncio.sleep(3600)
asyncio.run(main())
EOF
URL=nats://127.0.0.1:$PORT PW=$P_ONE $PY -I "$TMP/rec.py" "$TMP/rec.jsonl" &
for _ in $(seq 50); do [ -f "$TMP/rec.jsonl.ready" ] && curl -sf "http://127.0.0.1:$HTTP/readyz" >/dev/null && break; sleep 0.2; done
echo "projector: $(curl -s http://127.0.0.1:$HTTP/readyz)"
grep -o '"msg":"serving client CoT"[^}]*' "$TMP/projector.log"

echo; echo "== 1. Unreal client core (onemind_headless) as CLIENT/zeus over WebSocket: marker + chat"
NATS_USER=zeus NATS_PASSWORD=$P_ZEUS "$HEADLESS" --url "ws://127.0.0.1:$WS" --send-marker 30.27,-97.74,OM-C2-MARK \
  --send-chat "hello from the OneMind client" --duration 3 2>&1 | grep -E "svc.cot.send|chat reply|<event" | sed 's/^/  /'

echo; echo "== 2. L0 web client body from chat.ts geochatRequest"
FN=$(awk '/^export function geochatRequest/,/^}/' "$CHAT_TS")
cat > "$TMP/body.ts" <<EOF
interface Room { id: string; name: string; kind: "all" | "group" | "direct"; unread: number }
$FN
const [user, roomId, kind, text] = process.argv.slice(2);
const self = \`OM-WEB-\${user}\`;
const room: Room = { id: roomId, name: roomId, kind: kind as Room["kind"], unread: 0 };
console.log(JSON.stringify(geochatRequest(room, text, { uid: self, callsign: user.toUpperCase() }, \`GeoChat.\${self}.\${room.id}.\${crypto.randomUUID()}\`)));
EOF
cat > "$TMP/req.py" <<'EOF'
import asyncio, json, os, sys
import nats
async def main():
    nc = await nats.connect(os.environ["URL"], user=sys.argv[1], password=os.environ["PW"])
    try:
        r = await nc.request("svc.cot.send", sys.stdin.buffer.read(), timeout=3)
        print(r.data.decode())
    except Exception as e:
        print("ERROR", type(e).__name__, e)
    await nc.close()
asyncio.run(main())
EOF
webreq() { # user password body-args...
  local u=$1 p=$2; shift 2
  local body; body=$(node --experimental-strip-types --no-warnings "$TMP/body.ts" "$@")
  echo "  request ($u): $body"
  echo "$body" | URL=nats://127.0.0.1:$PORT PW=$p $PY -I "$TMP/req.py" "$u" | sed 's/^/  reply:   /'
}
webreq zeus "$P_ZEUS" zeus "All Chat Rooms" all "eyes on north field"
echo "  -- carol sending with zeus's sender uid (impersonation):"
webreq carol "$P_CAROL" zeus "Cyan" group "I am zeus"
echo "  -- same-account caller (ONEMIND/onemind, no verified identity):"
webreq onemind "$P_ONE" onemind "Cyan" group "hi"

sleep 0.5
echo; echo "== 3. What reached TAK (tak.cot.inject / tak.cot.mesh.tx)"
$PY -I - "$TMP/rec.jsonl" <<'EOF'
import json, sys
rows = [json.loads(l) for l in open(sys.argv[1])]
for r in rows:
    h = r["headers"]
    print(f'  {r["subject"]}  sender={h.get("OneMind-Sender")} origin={h.get("OneMind-Origin")} marking={h.get("OneMind-Marking")}')
    print(f'    {r["xml"][:400]}')
subj = {r["subject"] for r in rows}
ok = len(rows) == 3 and subj == {"tak.cot.inject"} and all(r["headers"].get("OneMind-Sender") == "CLIENT/zeus" for r in rows)
print(f'RESULT {"PASS" if ok else "FAIL"}: {len(rows)} publishes (want 3: UE marker, UE chat, web GeoChat; all CLIENT/zeus; none from refused requests)')
EOF
echo; echo "== projector log (svc.cot.send lines)"
grep -E 'svc.cot.send' "$TMP/projector.log" | sed 's/^/  /'
