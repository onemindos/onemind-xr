# J2 role modes: owner steps

Everything below needs the owner's cluster access, Authentik admin or a browser login. Proven
on the build machine without them: `auth-callout-tests.txt`, `policy-tests.txt`,
`binary-demo-transcript.txt` (nats-server v2.14.2 binary, callout binary, nats CLI, stand-in IdP),
`ci-local-yaml.txt`.

Order: merge omos #554 (policy) and #562 (identities), run A19 stage A1, then this PR
(stacked on #554). The full runbook with rollback is `platform/auth-callout/README.md` in the
omos PR; the steps here add the checks and the expected results.

## 1. Decide the open proposals first

`docs/proposals/2026-10-29-roles-authentik-j2.md`: J2-P1 (callout before A19 stage A2, or
after operator mode), J2-P4 (web client hostname), J2-P5 (role definitions).

## 2. Keys, SealedSecrets, AUTH account, deploy (runbook steps 1–4)

Expected after the manual sync of `platform-auth-callout`:

```bash
kubectl --context do-nyc3-omos-cloud -n onemind-core get deploy auth-callout     # 2/2 READY
kubectl --context do-nyc3-omos-cloud -n onemind-core logs deploy/auth-callout | head -1
# {"level":"INFO","msg":"auth-callout ready","client_account":"CLIENT","roles":5,"xkey":true}
```

## 3. Authentik (runbook step 5)

First confirm the chart key (read-only):

```bash
helm show values authentik --repo https://charts.goauthentik.io --version 2026.8.2 | grep -n -A4 '^blueprints:'
# expected: blueprints: / configMaps: [] / secrets: []
```

Then the platform-auth-helm values PR. After sync:

- Authentik admin → Customization → Blueprints: "OneMind XR role modes", status successful.
- Applications: `onemind-xr` (provider "OneMind XR", public client, RS256 key).
- Put yourself in `onemind-admins`; put a test account in `onemind-newcomers` only.

```bash
curl -s https://auth.onemindos.dev/application/o/onemind-xr/.well-known/openid-configuration | jq -r .issuer
# https://auth.onemindos.dev/application/o/onemind-xr/
curl -s https://auth.onemindos.dev/application/o/onemind-xr/jwks/ | jq -r '.keys[].kty'
# RSA
```

## 4. OpenFGA (runbook step 6)

Write the #554 actions model, `platform/security/openfga/onemind-roles.tuples.json` (53 tuples)
and `site:sim site asset:sim-x1`. Check with the OpenFGA API (in-cluster, preshared key):

```bash
# user:test is a newcomer: not a proposer
curl -s -H "Authorization: Bearer $OPENFGA_TOKEN" -d '{"tuple_key":{"user":"user:test","relation":"proposer","object":"action_type:asset.goto"},
  "contextual_tuples":{"tuple_keys":[{"user":"user:test","relation":"member","object":"group:onemind-newcomers"}]}}' \
  "$OPENFGA_URL/stores/$STORE/check"            # {"allowed":false}
# the same user as an operator: allowed
#   (group:onemind-operators instead)            # {"allowed":true}
```

## 5. Turn the callout on (runbook step 7) and the J2 acceptance test

Get a token for each test account (the L0 client does this with PKCE; by hand, use the
Authentik "OneMind XR" application's authorize URL in a browser and copy the `access_token`
from the redirect, or an Authentik app password flow if you enable one). Then, with the nats CLI
over the public WebSocket:

```bash
SUB=$(cut -d. -f2 <<<"$TOKEN" | base64 -d 2>/dev/null | jq -r .sub)
nats -s wss://ws.onemindos.dev --token "$TOKEN" --inbox-prefix "_INBOX.$SUB" req svc.layer.list '{"v":1}'
#   newcomer: a reply (once the layer service, #561, is deployed; before that: "no responders")
nats -s wss://ws.onemindos.dev --token "$TOKEN" --inbox-prefix "_INBOX.$SUB" req svc.policy.command.propose '{"v":1}'
#   newcomer: nats: permissions violation: Permissions Violation for Publish to "svc.policy.command.propose"
#   operator: a policy reply (denied or awaiting_confirmation, depending on the body)
nats -s wss://ws.onemindos.dev --token "$TOKEN" --inbox-prefix "_INBOX.$SUB" sub 'cmd.map.>'
#   newcomer: Permissions Violation for Subscription to "cmd.map.>"
```

Server side, the same refusals appear as:

```bash
for p in nats-0 nats-1 nats-2; do kubectl --context do-nyc3-omos-cloud -n nats logs $p -c nats --since=10m | grep 'ak:'; done
# ... "CLIENT/user:ak:<you>;onemind-newcomers" - Publish Violation - Subject "svc.policy.command.propose"
```

And no service was affected:

```bash
kubectl --context do-nyc3-omos-cloud -n onemind-core logs deploy/auth-callout --since=10m | grep '"msg":"denied"' | grep -v websocket
# expected: nothing (no service or leaf login reached the callout)
```

## 6. In the client (needs J1 web client and the Unreal client)

- Log in as the newcomer: only the explore mode shows; no command panel, no asset actions on a
  drone's menu.
- Log in as an operator: the operate mode shows; proposing `asset.goto` on `sim-x1` returns
  `awaiting_confirmation`, confirming dispatches it (policy audit `evt.so.action.*`).
- Hermes: no real-model check here; an agent proposal on behalf of a callout user is denied
  until J2-P3 is ruled (expected, fail closed).
