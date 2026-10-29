#!/usr/bin/env bash
# Smoke run of the policy binary against a disposable nats-server (passwords random, never printed).
set -u
export PATH=/Users/danielcarku/Desktop/work/.tools/bin:$PATH
R=/Users/danielcarku/Desktop/work/.run/tmp-i4
eval "$(sed -n 1p $R/env)"; eval "$(sed -n 2p $R/env)"
S="nats://127.0.0.1:$PORT"
A=(--server "$S" --user admin --password "$P_ADM")
Z=(--server "$S" --user zeus --password "$P_Z")
H=(--server "$S" --user hermes --password "$P_H")
nats "${A[@]}" stream add CMD --subjects 'cmd.>' --storage file --replicas 1 --discard new --dupe-window 2m --defaults >/dev/null
nats "${A[@]}" stream add EVT --subjects 'evt.so.>' --subjects 'evt.drone.>' --subjects 'evt.robot.>' --storage file --replicas 1 --dupe-window 2m --defaults >/dev/null
nats "${A[@]}" stream add MISSION --subjects 'evt.mission.>' --storage file --replicas 1 --defaults >/dev/null
nats "${A[@]}" kv add geofences --history 10 >/dev/null
nats "${A[@]}" kv put geofences zone-home '{"entity_id":"zone-home","geo":{"shape":"circle","kind":"keep_in","radius_m":20000,"coords":{"type":"Point","coordinates":[-97.7431,30.2672]}}}' >/dev/null
echo "== stack: nats-server $(nats-server --version) on $S; streams CMD EVT MISSION, KV geofences"
# entity service stand-in
nats --server "$S" --user entity --password "$P_ENT" reply svc.entity.get '{"entity":{"entity_id":"sim-x1","entity_type":"drone","location":{"lat":30.2672,"lon":-97.7431,"hae_m":150},"health":{"connection":"online"},"indicators":{"simulated":true},"capabilities":["onemind.task.goto.v1"]}}' >/dev/null 2>&1 &
EP=$!
nats "${A[@]}" sub 'cmd.>' > $R/cmd-sub.txt 2>&1 &
CP=$!
nats "${A[@]}" sub 'evt.so.action.>' > $R/audit-sub.txt 2>&1 &
AP=$!
NATS_URL=$S NATS_USER=policy NATS_PASSWORD=$P_POL POLICY_AUTHZ=local POLICY_MODEL_FILE=/Users/danielcarku/Desktop/work/wt/i4-policy/platform/security/openfga/onemind-actions.json \
 POLICY_TUPLES_FILE=$R/tuples.json POLICY_PRINCIPALS_FILE=$R/principals.json POLICY_SCHEMA_DIR=/Users/danielcarku/Desktop/work/wt/i4-policy/services/schemas \
 POLICY_SIGNING_KEY_FILE=$R/sign.pem POLICY_SIGNING_KID=smoke HEALTH_ADDR=127.0.0.1:0 $R/policy > $R/policy.log 2>&1 &
PP=$!
sleep 1.5
echo "== policy log (start)"; head -2 $R/policy.log
u() { uuidgen | tr A-Z a-z; }
echo; echo "== 1. zeus (CLIENT) proposes asset.goto for sim-x1"
REQ=$(printf '{"v":1,"request_id":"%s","action_type":"asset.goto","action_version":1,"target":{"entity_type":"drone","entity_id":"sim-x1"},"params":{"lat":30.27,"lon":-97.742,"hae_m":160},"requested_by":{"kind":"user","id":"user:claimed"},"reason":"smoke"}' "$(u)")
OUT=$(nats "${Z[@]}" req svc.policy.command.propose "$REQ" --raw 2>&1); echo "$OUT"
AID=$(echo "$OUT" | python3 -c 'import sys,json;print(json.load(sys.stdin)["action_id"])'); HASH=$(echo "$OUT" | python3 -c 'import sys,json;print(json.load(sys.stdin)["params_hash"])')
echo; echo "== 2. hermes (AGENTS) tries to confirm it (server permission)"
nats "${H[@]}" req svc.policy.command.confirm "{\"v\":1,\"action_id\":\"$AID\",\"params_hash\":\"$HASH\",\"decision\":\"approve\"}" --raw --timeout 2s 2>&1 | tail -1
echo; echo "== 3. hermes proposes asset.strike (stop line)"
nats "${H[@]}" req svc.policy.command.propose "$(printf '{"v":1,"request_id":"%s","action_type":"asset.strike","action_version":1,"target":{"entity_type":"drone","entity_id":"sim-x1"},"params":{},"requested_by":{"kind":"agent","id":"x","on_behalf_of":"user:zeus"}}' "$(u)")" --raw 2>&1
echo; echo "== 4. zeus proposes a goto for a non-sim asset"
nats "${Z[@]}" req svc.policy.command.propose "$(printf '{"v":1,"request_id":"%s","action_type":"asset.goto","action_version":1,"target":{"entity_type":"drone","entity_id":"real-1"},"params":{"lat":30.27,"lon":-97.742,"hae_m":160},"requested_by":{"kind":"user","id":"user:zeus"}}' "$(u)")" --raw 2>&1
echo; echo "== 5. zeus confirms the goto with the shown params_hash"
nats "${Z[@]}" req svc.policy.command.confirm "{\"v\":1,\"action_id\":\"$AID\",\"params_hash\":\"$HASH\",\"decision\":\"approve\"}" --raw 2>&1
echo; echo "== 6. zeus replays the same approval"
nats "${Z[@]}" req svc.policy.command.confirm "{\"v\":1,\"action_id\":\"$AID\",\"params_hash\":\"$HASH\",\"decision\":\"approve\"}" --raw 2>&1
sleep 0.5
echo; echo "== 7. hermes reads the action (not its own)"
nats "${H[@]}" req svc.policy.action.get "{\"v\":1,\"action_id\":\"$AID\"}" --raw 2>&1
echo; echo "== commands seen on cmd.> (admin subscriber)"; cat $R/cmd-sub.txt
echo; echo "== audit subjects seen on evt.so.action.>"; grep 'Received on "evt.so.action' $R/audit-sub.txt | sed -E 's/.*on "([^"]+)".*/\1/' | sort | uniq -c
echo; echo "== CMD stream"; nats "${A[@]}" stream info CMD --json | python3 -c 'import sys,json;s=json.load(sys.stdin)["state"];print("messages:",s["messages"])'
kill $PP $EP $CP $AP 2>/dev/null; wait 2>/dev/null
echo; echo "== policy log (last lines)"; tail -4 $R/policy.log
