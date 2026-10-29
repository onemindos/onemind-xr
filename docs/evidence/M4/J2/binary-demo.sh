#!/usr/bin/env bash
# J2 transcript with real binaries: nats-server v2.14.2 (built from the module cache, the live
# version), the auth-callout binary, the nats CLI, and a stand-in Authentik (runtime RSA key).
# Every key, password and token is generated in $R at run time and deleted at the end.
# Build the binaries into $R first (they were deleted after the recorded run):
#   cd <omos>/services/auth-callout && go build -o $R/nats-server-2.14.2 github.com/nats-io/nats-server/v2 \
#     && go build -o $R/auth-callout ./cmd/auth-callout
#   (cd $GOMODCACHE/github.com/nats-io/nkeys@v0.4.16 && go build -o $R/nk ./nk)
#   $R/idp-bin: a stdlib-only stand-in IdP (runtime RSA key, /jwks, <user>.tok/.sub files), not kept
set -u
R=/Users/danielcarku/Desktop/work/.run/tmp-j2
N=/Users/danielcarku/Desktop/work/.tools/bin/nats
ROLES=/Users/danielcarku/Desktop/work/wt/j2-roles/platform/security/roles/onemind-roles.json
cd "$R" && rm -rf run && mkdir run && cd run
port() { python3 -c 'import socket;s=socket.socket();s.bind(("127.0.0.1",0));print(s.getsockname()[1])'; }
P=$(port); WS=$(port); IDP=$(port)
"$R/nk" -gen account -pubout > issuer.nk; "$R/nk" -gen curve -pubout > xkey.nk
head -1 issuer.nk > issuer.seed; head -1 xkey.nk > xkey.seed
pw() { python3 -c 'import secrets;print(secrets.token_hex(16))'; }
CPW=$(pw); PPW=$(pw)
"$R/idp-bin" "$PWD" "https://auth.example.test/application/o/onemind-xr/" "127.0.0.1:$IDP" > idp.log 2>&1 &
cat > nats.conf <<CONF
listen: "127.0.0.1:$P"
websocket { listen: "127.0.0.1:$WS", no_tls: true }
system_account: SYS
accounts {
  SYS { users: [ { user: sys, password: "$(pw)" } ] }
  AUTH { users: [ { user: auth-callout, password: "$CPW", allowed_connection_types: [STANDARD], permissions: {
      publish: { deny: [">"] }, subscribe: { allow: ["\$SYS.REQ.USER.AUTH"] }, allow_responses: { max: 1, expires: "5s" } } } ] }
  ONEMIND {
    users: [ { user: policy, password: "$PPW" } ]
    exports: [ { service: "svc.policy.>" }, { service: "svc.layer.>" }, { service: "svc.entity.>" }, { stream: "cmd.map.>" } ]
  }
  CLIENT {
    imports: [
      { service: { account: ONEMIND, subject: "svc.policy.>" }, share: true }
      { service: { account: ONEMIND, subject: "svc.layer.>" }, share: true }
      { service: { account: ONEMIND, subject: "svc.entity.>" }, share: true }
      { stream: { account: ONEMIND, subject: "cmd.map.>" } }
    ]
  }
}
authorization { auth_callout {
  issuer: "$(sed -n 2p issuer.nk)"
  xkey: "$(sed -n 2p xkey.nk)"
  auth_users: [ auth-callout ]
  account: AUTH
  allowed_accounts: [ AUTH ]
} }
CONF
"$R/nats-server-2.14.2" -c nats.conf -l server.log &
sleep 1.5
NATS_URL=nats://127.0.0.1:$P NATS_USER=auth-callout NATS_PASSWORD=$CPW CALLOUT_ISSUER_SEED_FILE=$PWD/issuer.seed \
  CALLOUT_XKEY_SEED_FILE=$PWD/xkey.seed ROLES_FILE=$ROLES OIDC_ISSUER=https://auth.example.test/application/o/onemind-xr/ \
  OIDC_AUDIENCE=onemind-xr OIDC_JWKS_URL=http://127.0.0.1:$IDP/jwks HEALTH_ADDR=127.0.0.1:$(port) "$R/auth-callout" > callout.log 2>&1 &
sleep 1
# The "policy service" and "layer service" in ONEMIND (nats reply), and a header logger.
$N -s nats://127.0.0.1:$P --user policy --password "$PPW" reply 'svc.policy.>' '{"ok":true,"from":"policy"}' > /dev/null 2>&1 &
$N -s nats://127.0.0.1:$P --user policy --password "$PPW" reply 'svc.layer.>' '{"ok":true,"from":"layer"}' > /dev/null 2>&1 &
$N -s nats://127.0.0.1:$P --user policy --password "$PPW" sub 'svc.policy.>' > policy-seen.log 2>&1 &
sleep 0.8
as() { local who=$1; shift; echo "\$ nats --server ws://127.0.0.1:WS --token <$who token> --inbox-prefix _INBOX.<$who sub> $*"
  $N -s ws://127.0.0.1:$WS --token "$(cat $who.tok)" --inbox-prefix "_INBOX.$(cat $who.sub)" --timeout 1s "$@" 2>&1 | sed 's/^/    /'; }
echo "== nats-server $("$R/nats-server-2.14.2" --version) with auth_callout; callout: $(grep -o '"msg":"auth-callout ready.*' callout.log)"
echo; echo "== newcomer nina (onemind-newcomers): layers yes, asset commands no"
as nina req svc.layer.list '{"v":1}'
as nina req svc.policy.command.propose '{"v":1,"action_type":"asset.goto"}'
as nina req svc.policy.action.get '{"v":1}'
as nina sub 'cmd.map.>' --count 1
echo; echo "== viewer vera (onemind-viewers): can read an action, cannot propose"
as vera req svc.policy.action.get '{"v":1}'
as vera req svc.policy.command.propose '{"v":1}'
echo; echo "== operator olga (onemind-operators, argocd-admins): reaches the policy service"
as olga req svc.policy.command.propose '{"v":1,"action_type":"asset.goto"}'
echo; echo "== refused logins"
as nobody req svc.layer.list '{}'
as forged req svc.layer.list '{}'
echo "\$ nats --user policy --password wrong req svc.layer.list"; $N -s nats://127.0.0.1:$P --user policy --password wrong --timeout 1s req svc.layer.list '{}' 2>&1 | sed 's/^/    /'
sleep 0.5
echo; echo "== what the policy responder saw (Nats-Request-Info, server-set)"
grep -o 'Nats-Request-Info: .*' policy-seen.log | sed 's/"start":"[^"]*",//; s/"rtt":[0-9]*,//' | sed 's/^/    /'
echo; echo "== callout decisions (callout.log)"
grep -o '"msg":"\(allowed\|denied\)".*' callout.log | sed 's/"host":"[^"]*"//' | sed 's/^/    /'
echo; echo "== server log: permission and authorization violations"
grep -E 'Violation|Authorization' server.log | sed 's/^\[[0-9]*\] [0-9/ :.]*//' | sed 's/^/    /'
kill %1 %2 %3 %4 %5 %6 2>/dev/null; sleep 0.5; cd "$R" && rm -rf run
