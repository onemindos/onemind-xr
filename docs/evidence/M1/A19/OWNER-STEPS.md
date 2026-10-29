# A19 owner steps: per-service NATS identities

Everything below needs live cluster access (ConfigMap changes, pod restarts, SealedSecrets), so
this run did not do any of it. The full runbook with rollback is in omos PR #562,
`platform/nats/IDENTITIES.md`. The commands below are the short path. Use a private directory
outside any Git repository:

```bash
W=~/onemind-private/nats; mkdir -p "$W"; chmod 700 "$W"
cd <omos checkout on branch feat/nats-identities or main after merge>
```

## 1. Rollback point and passwords

```bash
kubectl --context do-nyc3-omos-cloud -n nats get cm nats-config -o yaml > "$W/nats-config.before.yaml"
kubectl --context do-nyc3-omos-cloud -n nats get cm nats-config -o jsonpath='{.data.nats\.conf}' > "$W/live.conf"
python3 scripts/nats_identities.py gen-passwords --out "$W/identities.json" \
  --users policy,entity,ingest,layer,projector,tak-client,bridge-mavlink-home,mesh-gateway,agent-fabric,leaf-tak,leaf-site-mac,hermes,zeus
```

Expected: `wrote 13 passwords to .../identities.json (0600)`. Save the file in the password
manager.

## 2. Stage A1 (reload, nothing breaks)

```bash
python3 scripts/nats_identities.py live --live-conf "$W/live.conf" --passwords "$W/identities.json" --stage A1 --out "$W/nats.conf.A1"
diff "$W/live.conf" "$W/nats.conf.A1"          # only "accounts" differ
kubectl --context do-nyc3-omos-cloud -n nats create configmap nats-config --from-file=nats.conf="$W/nats.conf.A1" --dry-run=client -o yaml \
  | kubectl --context do-nyc3-omos-cloud -n nats apply -f -
for p in nats-0 nats-1 nats-2; do kubectl --context do-nyc3-omos-cloud -n nats logs $p -c nats --since=5m | grep -i reload; done
```

Expected: `Reloaded server configuration` on each pod within about a minute; leafz still shows
3 leafs; no new `Authorization Violation`.

Check that a new user can log in and is restricted (as `zeus`, from a client device or nats-box):

```bash
NATS_USER=zeus NATS_PASSWORD=<from identities.json> nats -s tls://<hub> pub cmd.fleet.drone.sim-x1.goto '{}'
```

Expected: `Permissions Violation for Publish to "cmd.fleet.drone.sim-x1.goto"`.

## 3. Seal the per-workload Secrets and move workloads (PRs)

Use the kubeseal pattern in IDENTITIES.md step 2: `omos/omos-entity-nats`,
`omos/omos-ingest-nats`, `omos/omos-tak-projector-nats`, `agent-fabric/agent-fabric-nats`,
`onemind-core/policy-service-runtime` (plus `OPENFGA_TOKEN`), `leaf.conf` for
`takserver/tak-nats-leaf` (user `leaf-tak`) and `hermes/hermes-leaf-conf` (user `hermes`).
Render Hermes' whole `leaf.conf` with
`python3 scripts/nats_identities.py leaf-conf --user hermes --passwords "$W/identities.json" --out "$W/hermes-leaf.conf"`
(IDENTITIES.md step 2). Do not hand-edit it: its `local` user carries Hermes' allow-lists. After
Hermes moves, check that a forbidden publish is refused and logged at the sidecar:

```bash
kubectl --context do-nyc3-omos-cloud -n hermes logs deploy/hermes -c nats-leaf --since=10m | grep Violation
```

Expected: any `cmd.*` (except `cmd.map.>`) or `tak.cot.inject` attempt by the agent appears as
`"OMOS/user:local" - Publish Violation - Subject "..."`, and the agent gets
`Permissions Violation for Publish to "..."`.
Repoint one workload at a time. After each one, check its logs show no
`Authorization Violation` or `Permissions Violation`.

## 4. Stage A2 (needs a rolling restart)

Before you apply it, confirm with `connz?auth=1` (IDENTITIES.md step 4) that `onemind` is used
only over STANDARD and WEBSOCKET, and `sys` and `monitor` only over STANDARD. From A2 these
users cannot log in as leafs.

Render `--stage A2` and apply it as in step 2. Expected: the reloader logs a refusal
(`config reload not supported for LeafNode: field "Users"`) and the pods keep running A1. Then
restart the pods, highest ordinal first:

```bash
kubectl --context do-nyc3-omos-cloud -n nats delete pod nats-2
kubectl --context do-nyc3-omos-cloud -n nats wait --for=condition=Ready pod/nats-2 --timeout=300s
nats --context <sys> server report jetstream     # all current before the next pod
# then nats-1, then nats-0
```

Expected: TAK leaf, Hermes and zeus-mac reconnect (still as `leaf_tak`). Then move the TAK
leaf to `leaf-tak` and Hermes to `hermes`. `curl :8222/leafz` should then show Hermes in
account `AGENTS`.

## 5. Stage B (reload) after the projector and Hermes have moved

Render `--stage B` and apply it. Then check:

```bash
for p in nats-0 nats-1 nats-2; do kubectl --context do-nyc3-omos-cloud -n nats logs $p -c nats --since=10m | grep Violation; done
```

Expected: violations only from deliberate tests. If a legitimate client appears, roll back to
A2 (apply `nats.conf.A2`, reload) and move that client first.

## 6. On the Mac (site leaf, later with A23)

Switch `infra/edge/mac-leaf/nats-server.conf`'s remote user to `leaf-site-mac` once the Mac's
legacy bridges (HA `cmd.ha.*`, cameras) have been moved or covered (proposal A19-P2). Until
then the Mac stays on `leaf_tak`. Expected after the switch: on the hub, `leafz` shows
`zeus-mac` with account `ONEMIND`; `cmd.fleet.drone.sim-x2.*` never reaches the Mac.

## Rollback (any time)

`kubectl apply -f "$W/nats-config.before.yaml"`. If stage A2 was applied, follow it with the
same one-pod-at-a-time restart.
