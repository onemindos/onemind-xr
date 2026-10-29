#!/usr/bin/env bash
# A19 smoke: the stage-B identity config (bcrypt hashes, as live) on a disposable nats-server
# binary, with the real entity, policy, omos-ingest and MAVLink bridge binaries built from pristine
# archives of their PR branches, each logged in as its own user. Hermes, the TAK leaf and the Mac
# site leaf are real leaf nodes. Test-only passwords live in $R (outside Git) and are never printed.
set -u
export PATH=/Users/danielcarku/Desktop/work/.tools/bin:$PATH
export GOPATH=/Users/danielcarku/Desktop/work/.tools/gopath GOMODCACHE=/Users/danielcarku/Desktop/work/.tools/gopath/pkg/mod
WT=/Users/danielcarku/Desktop/work/wt/a19-identities
OM=/Users/danielcarku/Desktop/work/omos
PY=/Users/danielcarku/Desktop/work/.tools/py/bin/python
EV=/Users/danielcarku/Desktop/work/onemind-xr/docs/evidence
R=/Users/danielcarku/Desktop/work/.run/tmp-a19/smoke
rm -rf "$R"; mkdir -p "$R/src" "$R/js"; chmod 700 "$R"
PIDS=()
cleanup() { for p in "${PIDS[@]}"; do kill "$p" 2>/dev/null; done; wait 2>/dev/null; }
trap cleanup EXIT
port() { python3 -c 'import socket;s=socket.socket();s.bind(("127.0.0.1",0));print(s.getsockname()[1])'; }
pw() { python3 -c 'import json,sys;print(json.load(open(sys.argv[1]))[sys.argv[2]])' "$R/passwords.json" "$1"; }
step() { echo; echo "== $*"; }

step "0. render stage B with bcrypt hashes (scripts/nats_identities.py, branch feat/nats-identities)"
$PY "$WT/scripts/nats_identities.py" gen-passwords --out "$R/passwords.json"
$PY "$WT/scripts/nats_identities.py" conf --passwords "$R/passwords.json" --stage B --hash --out "$R/frag.json"
python3 -c 'import json,sys;d=json.load(open(sys.argv[1]));print("accounts:",{a:[u["user"] for u in v.get("users",[])] for a,v in d["accounts"].items()});print("password fields are bcrypt:",all(u["password"].startswith("$2") for v in d["accounts"].values() for u in v.get("users",[])))' "$R/frag.json"

P=$(port); LP=$(port); WP=$(port)
URL=nats://127.0.0.1:$P
python3 - "$R" "$P" "$LP" "$WP" <<'EOF'
import json, sys
r, p, lp, wp = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4])
frag = json.load(open(f"{r}/frag.json"))
conf = {"server_name": "hub", "host": "127.0.0.1", "port": p,
        "jetstream": {"store_dir": f"{r}/js", "max_file_store": 50 << 30, "max_memory_store": 64 << 20},
        "leafnodes": {"host": "127.0.0.1", "port": lp},
        "websocket": {"host": "127.0.0.1", "port": wp, "no_tls": True}}
conf.update(frag)
json.dump(conf, open(f"{r}/hub.json", "w"), indent=2)
EOF
chmod 600 "$R/hub.json"
nats-server -c "$R/hub.json" > "$R/hub.log" 2>&1 & PIDS+=($!)
sleep 1
echo "hub: $(nats-server --version) on $URL, leaf port $LP, websocket $WP"

leaf() { # leaf <hub user> -> starts a leaf whose local clients need no credentials; prints port
  local u=$1 lport; lport=$(port)
  python3 - "$R" "$u" "$lport" "$LP" "$(pw "$u")" <<'EOF'
import json, sys
r, u, port, lp, pw = sys.argv[1], sys.argv[2], int(sys.argv[3]), sys.argv[4], sys.argv[5]
conf = {"server_name": f"leaf-{u}", "host": "127.0.0.1", "port": port,
        "accounts": {"LOCAL": {"users": [{"user": "local"}]}}, "no_auth_user": "local",
        "leafnodes": {"reconnect": "500ms", "remotes": [{"url": f"nats://{u}:{pw}@127.0.0.1:{lp}", "account": "LOCAL"}]}}
json.dump(conf, open(f"{r}/leaf-{u}.json", "w"))
EOF
  chmod 600 "$R/leaf-$u.json"
  nats-server -c "$R/leaf-$u.json" > "$R/leaf-$u.log" 2>&1 & PIDS+=($!)
  echo "$lport"
}
as() { local u=$1; shift; NATS_USER=$u NATS_PASSWORD="$(pw "$u")" nats -s "$URL" "$@"; }

step "1. streams and buckets created by the shared onemind user (as NACK does)"
for x in "COT tak.cot.>" "TEL tel.>" "WORLD evt.world.>" "INTEL intel.>" "DET det.>" "SYS evt.sys.>" "CMD cmd.fleet.> cmd.mission.> cmd.entity.>" "EVT evt.so.> evt.drone.> evt.robot.> evt.entity.>" "MISSION evt.mission.>"; do
  set -- $x; n=$1; shift; args=(); for s in "$@"; do args+=(--subjects "$s"); done
  as onemind stream add "$n" "${args[@]}" --storage file --replicas 1 --dupe-window 2m --allow-direct --defaults >/dev/null && echo "  stream $n ($*)"
done
as onemind stream add --config "$EV/M1/A4/ent.json" >/dev/null && echo "  stream ENT (A4 ent.json)"
for b in entity-correlation geofences mission-state ingest-locks ingest-state feed-config; do as onemind kv add "$b" --history 5 --storage file --replicas 1 >/dev/null && echo "  kv $b"; done
as onemind kv put geofences zone-home '{"entity_id":"zone-home","geo":{"shape":"circle","kind":"keep_in","radius_m":20000,"coords":{"type":"Point","coordinates":[0.0,0.0]}}}' >/dev/null

HL=$(leaf hermes); TL=$(leaf leaf-tak); ML=$(leaf leaf-site-mac)
sleep 2
echo "leafs: hermes :$HL  leaf-tak :$TL  leaf-site-mac :$ML"
curl -s "http://127.0.0.1:0" >/dev/null 2>&1
grep -h -E 'Leafnode connection created|Authorization Violation' "$R/hub.log" | sed -E 's/^\[[0-9]+\] //' | sed -E 's/[0-9]{4}\/[0-9]{2}\/[0-9]{2} [0-9:.]+ //'

step "2. FEATURES A19 acceptance with the nats CLI"
echo "\$ (as zeus, CLIENT) nats pub cmd.fleet.drone.sim-x1.goto '{}'"
as zeus pub cmd.fleet.drone.sim-x1.goto '{}' 2>&1 | tail -2
echo "\$ (as zeus, CLIENT) nats pub ent.drone.sim-x1.state '{}'"
as zeus pub ent.drone.sim-x1.state '{}' 2>&1 | tail -2
echo "\$ (as zeus, CLIENT) nats pub tak.cot.inject '<event/>'"
as zeus pub tak.cot.inject '<event/>' 2>&1 | tail -2
echo "\$ (as hermes, a leaf-only credential) nats pub agents.hb.hermes.zeus.legacy '{}' directly on the hub"
as hermes pub agents.hb.hermes.zeus.legacy '{}' 2>&1 | tail -1
echo "\$ observers: TAK leaf subscribes tak.cot.inject; site leaf subscribes cmd.fleet.drone.sim-x1.>; onemind subscribes cmd.ha.>"
nats -s nats://127.0.0.1:$TL sub tak.cot.inject > "$R/obs-tak.txt" 2>&1 & PIDS+=($!)
nats -s nats://127.0.0.1:$ML sub 'cmd.fleet.drone.sim-x1.>' > "$R/obs-site.txt" 2>&1 & PIDS+=($!)
as onemind sub 'cmd.ha.>' > "$R/obs-ha.txt" 2>&1 & PIDS+=($!)
sleep 1
echo "\$ (Hermes agent on its in-pod leaf) nats pub tak.cot.inject / cmd.fleet.drone.sim-x1.goto / cmd.ha.light.on"
for s in tak.cot.inject cmd.fleet.drone.sim-x1.goto cmd.ha.light.on; do nats -s nats://127.0.0.1:$HL pub "$s" '{"from":"hermes"}' 2>&1 | tail -1; done
echo "\$ (projector, positive control) nats pub tak.cot.inject '<event uid=\"proj\"/>'"
as projector pub tak.cot.inject '<event uid="proj"/>' 2>&1 | tail -1
sleep 1
echo "messages seen by the TAK leaf on tak.cot.inject: $(grep -c 'Received on' "$R/obs-tak.txt") (want 1: the projector's, none from Hermes)"
grep -A2 'Received on' "$R/obs-tak.txt" | grep -v '^--' | grep -v '^$' | head -3
echo "messages seen by the site leaf on cmd.fleet.drone.sim-x1.>: $(grep -c 'Received on' "$R/obs-site.txt") (want 0)"
echo "messages seen by onemind on cmd.ha.>: $(grep -c 'Received on' "$R/obs-ha.txt") (want 0)"
echo "hermes leaf still connected: $(grep -c 'Leafnode connection created' "$R/leaf-hermes.log") connect(s), $(grep -c -i 'closed' "$R/leaf-hermes.log") close(s)"

step "3. real MAVLink bridge (PR #550) on the Mac site leaf, no credentials of its own; hub enforces leaf-site-mac"
mkdir -p "$R/src/mavlink"
git -C "$OM" archive origin/feat/mavlink-bridge-fleet services/bridge | tar -x -C "$R/src/mavlink"
openssl genpkey -algorithm ed25519 -out "$R/sign.pem" 2>/dev/null
PUB=$(openssl pkey -in "$R/sign.pem" -pubout -outform DER 2>/dev/null | tail -c 32 | base64)
(cd "$R/src/mavlink/services/bridge/bridges/mavlink" && go build -o "$R/bridge-mavlink" . ) || echo "BUILD FAILED: bridge"
# A private sim-x1 instance on its own UDP port (udp 14550 belongs to the shared sim and another
# lane's bridge); same onemind-xr scripts/sim/mavlink_sim_drone.py, simulated asset only.
MP=$(port)
$PY /Users/danielcarku/Desktop/work/onemind-xr/scripts/sim/mavlink_sim_drone.py --out udpout:127.0.0.1:$MP > "$R/drone.log" 2>&1 & PIDS+=($!)
echo "private sim-x1 drone -> udp 127.0.0.1:$MP"
(cd "$R/src/mavlink/services/bridge/bridges/mavlink" && NATS_URL=nats://127.0.0.1:$ML BRIDGE_NAME=bridge-mavlink ASSET_ID=sim-x1 MAV_SYSID=1 \
  MAVLINK_ENDPOINT=udpserver:127.0.0.1:$MP POLICY_PUBKEYS="smoke=$PUB" GEOFENCE_FILE=geofence.example.json "$R/bridge-mavlink" > "$R/bridge.log" 2>&1) & PIDS+=($!)
sleep 4
echo "\$ (as onemind) nats stream info TEL: messages from tel.fleet.drone.sim-x1.* that crossed the site leaf"
as onemind stream subjects TEL 2>&1 | grep -E 'sim-x1|Subject' | head -4
head -4 "$R/bridge.log"

step "4. real entity service (PR #552) as user entity"
mkdir -p "$R/src/entity"
git -C "$OM" archive origin/feat/entity-service services/entity services/schemas | tar -x -C "$R/src/entity"
(cd "$R/src/entity/services/entity" && go build -o "$R/entity-service" ./cmd/entity-service) || echo "BUILD FAILED: entity"
T0=$(( $(date +%s) * 1000 - 45000 ))
python3 "$R/src/entity/services/entity/testdata/fixtures/gen_fixtures.py" $T0 > "$R/live.jsonl"
echo "\$ pubfx.py: A4 fixtures, each published (acked JetStream) as the user owning its subject"
python3 "$EV/M1/A19/pubfx.py" "$URL" "$R/passwords.json" "$R/live.jsonl"
NATS_URL=$URL NATS_USER=entity NATS_PASSWORD="$(pw entity)" START_LOOKBACK=0 HTTP_ADDR=127.0.0.1:18089 POD_NAME=smoke "$R/entity-service" > "$R/entity.log" 2>&1 & PIDS+=($!)
sleep 4
echo "\$ curl /readyz: $(curl -s http://127.0.0.1:18089/readyz)"
echo "\$ entity log lines mentioning permission: $(grep -c -i 'permission' "$R/entity.log")"
as onemind stream info ENT --json | python3 -c 'import sys,json;s=json.load(sys.stdin)["state"];print("ENT messages:",s["messages"],"subjects:",s.get("num_subjects"))'
echo "\$ (as zeus, CLIENT, through the svc.entity.> import) nats req svc.entity.get '{\"entity_id\":\"sim-x1\"}'"
as zeus req svc.entity.get '{"entity_id":"sim-x1"}' --raw 2>&1 | python3 -c 'import sys,json
raw=sys.stdin.read()
try:
  e=json.loads(raw); print({k:e.get(k) for k in ["entity_id","entity_type","lifecycle","live"]}, "sources:", sorted({s["integration"] for s in e.get("sources",[])}))
except Exception: print(raw[:300])'
echo "\$ (Hermes, AGENTS, through its leaf) nats req svc.entity.get '{\"entity_id\":\"sim-x1\"}'"
nats -s nats://127.0.0.1:$HL req svc.entity.get '{"entity_id":"sim-x1"}' --raw 2>&1 | head -c 160; echo

kill %?entity-service 2>/dev/null; pkill -f "$R/entity-service" 2>/dev/null; sleep 1

step "5. real policy service (PR #554) as user policy; zeus confirms, Hermes cannot"
echo "(the fixture sim-x1 advertises no task capabilities, so a stand-in svc.entity.get responder runs as user entity, as in the I4 smoke)"
as entity reply svc.entity.get '{"entity":{"entity_id":"sim-x1","entity_type":"drone","location":{"lat":0.0005,"lon":0.0005,"hae_m":60},"health":{"connection":"online"},"indicators":{"simulated":true},"capabilities":["onemind.task.goto.v1","onemind.task.hold.v1","onemind.task.rtl.v1"]}}' > "$R/entity-standin.log" 2>&1 & PIDS+=($!)
mkdir -p "$R/src/policy"
git -C "$OM" archive origin/feat/policy-service services/policy services/schemas platform/security/openfga platform/policy | tar -x -C "$R/src/policy"
(cd "$R/src/policy/services/policy" && go build -o "$R/policy" ./cmd/policy) || echo "BUILD FAILED: policy"
python3 - "$R" <<'EOF'
import json, sys
r = sys.argv[1]
json.dump({"own_account": "ONEMIND", "agent_accounts": ["AGENTS"], "principals": [
    {"account": "CLIENT", "user": "zeus", "kind": "user", "id": "user:zeus"},
    {"account": "AGENTS", "user": "hermes", "kind": "agent", "id": "agent:hermes/zeus/legacy"}]}, open(f"{r}/principals.json", "w"))
t = [{"user": "user:zeus", "relation": "operator", "object": "asset:sim-x1"}]
for a in ["asset.goto", "asset.rtl", "asset.hold"]:
    o = "action_type:" + a
    t += [{"user": "user:zeus", "relation": rel, "object": o} for rel in ("proposer", "approver", "second_approver")]
    t += [{"user": "agent:hermes/zeus/legacy", "relation": "proposer", "object": o}]
json.dump(t, open(f"{r}/tuples.json", "w"))
EOF
PS=$R/src/policy
NATS_URL=$URL NATS_USER=policy NATS_PASSWORD="$(pw policy)" POLICY_AUTHZ=local POLICY_MODEL_FILE=$PS/platform/security/openfga/onemind-actions.json \
  POLICY_TUPLES_FILE=$R/tuples.json POLICY_PRINCIPALS_FILE=$R/principals.json POLICY_SCHEMA_DIR=$PS/services/schemas \
  POLICY_SIGNING_KEY_FILE=$R/sign.pem POLICY_SIGNING_KID=smoke HEALTH_ADDR=127.0.0.1:0 "$R/policy" > "$R/policy.log" 2>&1 & PIDS+=($!)
as onemind sub 'evt.drone.sim-x1.>' > "$R/obs-evt.txt" 2>&1 & PIDS+=($!)
sleep 2
head -2 "$R/policy.log"
u() { uuidgen | tr A-Z a-z; }
echo "\$ (as zeus) nats req svc.policy.command.propose asset.goto for sim-x1 (requested_by claims another user; the server-set identity wins)"
REQ=$(printf '{"v":1,"request_id":"%s","action_type":"asset.goto","action_version":1,"target":{"entity_type":"drone","entity_id":"sim-x1"},"params":{"lat":0.001,"lon":0.001,"hae_m":60},"requested_by":{"kind":"user","id":"user:claimed"},"reason":"A19 smoke"}' "$(u)")
OUT=$(as zeus req svc.policy.command.propose "$REQ" --raw 2>&1); echo "$OUT" | head -c 600; echo
AID=$(echo "$OUT" | python3 -c 'import sys,json;print(json.load(sys.stdin).get("action_id",""))' 2>/dev/null)
HASH=$(echo "$OUT" | python3 -c 'import sys,json;print(json.load(sys.stdin).get("params_hash",""))' 2>/dev/null)
echo "\$ (Hermes via its leaf) nats req svc.policy.command.confirm (the agent tries to approve)"
nats -s nats://127.0.0.1:$HL req svc.policy.command.confirm "{\"v\":1,\"action_id\":\"$AID\",\"params_hash\":\"$HASH\",\"decision\":\"approve\"}" --raw --timeout 2s 2>&1 | tail -1
echo "\$ (Hermes via its leaf) nats req svc.policy.command.propose asset.goto (allowed: propose only)"
HREQ=$(printf '{"v":1,"request_id":"%s","action_type":"asset.goto","action_version":1,"target":{"entity_type":"drone","entity_id":"sim-x1"},"params":{"lat":0.001,"lon":0.001,"hae_m":60},"requested_by":{"kind":"agent","id":"agent:hermes/zeus/legacy","on_behalf_of":"user:zeus"},"reason":"A19 smoke"}' "$(u)")
nats -s nats://127.0.0.1:$HL req svc.policy.command.propose "$HREQ" --raw --timeout 3s 2>&1 | head -c 500; echo
echo "\$ (as zeus) nats req svc.policy.command.confirm the goto (human approval)"
as zeus req svc.policy.command.confirm "{\"v\":1,\"action_id\":\"$AID\",\"params_hash\":\"$HASH\",\"decision\":\"approve\"}" --raw 2>&1 | head -c 600; echo
sleep 3
echo "commands delivered to the site leaf (cmd.fleet.drone.sim-x1.>): $(grep -c 'Received on' "$R/obs-site.txt")"
grep 'Received on' "$R/obs-site.txt" | head -3
echo "bridge outcomes on evt.drone.sim-x1.* seen in ONEMIND (crossed the site leaf):"
grep -o '"outcome":"[a-z_]*"' "$R/obs-evt.txt" | sort | uniq -c
grep -E 'command .* outcome' "$R/bridge.log" | sed -E 's/^[0-9\/]+ [0-9:.]+ //' | head -4
as onemind stream info CMD --json | python3 -c 'import sys,json;print("CMD stream messages:",json.load(sys.stdin)["state"]["messages"])'

step "6. real omos-ingest (PR #553) as user ingest, NWS fixture served locally"
mkdir -p "$R/src/ingest"
git -C "$OM" archive origin/feat/ingest-framework services/bridge/ingest services/schemas | tar -x -C "$R/src/ingest"
IS=$R/src/ingest/services/bridge/ingest
(cd "$IS" && go build -o "$R/omos-ingest" ./cmd/omos-ingest) || echo "BUILD FAILED: ingest"
FP=$(port)
(cd "$IS/testdata/fixtures/nws" && python3 -m http.server "$FP" --bind 127.0.0.1 > "$R/fixture-http.log" 2>&1) & PIDS+=($!)
sed -e "s#https://api.weather.gov/alerts/active?status=actual&area=NY#http://127.0.0.1:$FP/active-2026-10-29.json#" -e 's/interval: 120s/interval: 5s/' "$IS/sources.local.yaml" > "$R/sources.yaml"
NATS_URL=$URL NATS_USER=ingest NATS_PASSWORD="$(pw ingest)" "$R/omos-ingest" -config "$R/sources.yaml" -schemas "$R/src/ingest/services/schemas" -listen 127.0.0.1:0 -once > "$R/ingest.log" 2>&1 &
IP=$!; PIDS+=($IP)
for i in $(seq 1 20); do kill -0 $IP 2>/dev/null || break; sleep 1; done
echo "ingest exit: $(wait $IP 2>/dev/null; echo $?) ; log lines mentioning permission: $(grep -c -i 'permission' "$R/ingest.log")"
as onemind stream info INTEL --json | python3 -c 'import sys,json;s=json.load(sys.stdin)["state"];print("INTEL messages:",s["messages"])'
echo "KV ingest-state keys: $(as onemind kv ls ingest-state 2>&1 | tr '\n' ' ')"
tail -3 "$R/ingest.log" | cut -c1-240

step "7. server-side record"
echo "hub log Authorization Violation lines: $(grep -c 'Authorization Violation' "$R/hub.log")"
echo "hub log Permissions Violation lines: $(grep -c 'Permissions Violation' "$R/hub.log")"
grep 'Violation' "$R/hub.log" | sed -E 's/^\[[0-9]+\] [0-9\/]+ [0-9:.]+ //' | sort | uniq -c | head -20
echo; echo "smoke done"
