# A21 owner steps: TAK Server leg (omos PR #560, branch `feat/tak-server-leg`)

These steps need live cluster write access, the TAK CA key, or the TAK Server SDK from
tak.gov. None of them was possible on the build machine. Run them after the owner merges #560.

## 1. tak-client live (TAK to bus and bus to TAK)

1. Mint the TAK client certificate for CN `omos-tak-client` and give it the same groups the
   plugin uses (`__ANON__`, `Red`). Exact commands: omos `platform/takserver/tak-client/README.md`,
   step 1.
2. Seal `tak-client-tls` (keys `tls.crt`, `tls.key`, `ca.crt`) and `tak-client-nats` (keys
   `user`, `password`) into `platform/takserver/tak-client/` with `kubeseal` (same README,
   step 2). Open a PR with the two `.sealedsecret.yaml` files.
3. After `build-tak-client` runs on `main`, put the digest from its `omos-tak-client-image`
   artifact into `platform/takserver/tak-client/deployment.yaml` (`…:0.1.0@sha256:<digest>`)
   in the same or a later PR.
4. Sync: `argocd app sync platform-takserver-tak-client`.

Expected results:

```
$ kubectl -n takserver logs deploy/omos-tak-client
tak-client v0.1.0: TAK takserver-messaging-service.takserver.svc.cluster.local:8089 (protocol auto), NATS nats://nats.nats.svc.cluster.local:4222
tak-client: connected to takserver-messaging-service...:8089 (TLS TLS 1.2 or 1.3)
tak-client: injecting from tak.cot.inject (queue tak-cot-inject)
(if TAK offers v1) tak-client: TAK Server version 5.8-RELEASE-69 / switched to TAK Protocol v1 streaming
$ nats stream subjects COT
tak.cot.inject  …
tak.cot.out     …      <- new; was absent for 30 days
$ nats stream get COT --last-for tak.cot.out
Nats-Msg-Id: takc-…   Tak-Client: omos-tak-client   Tak-Proto: 0|1
$ kubectl -n takserver port-forward deploy/omos-tak-client 8080 & curl -s localhost:8080/metrics | grep -E 'tak_client_(tak_connected|published_total|injected_total)'
tak_client_tak_connected 1
```

ATAK check (owner device on the tailnet, connected to `tak.tail717ba0.ts.net:8089`):
- Your ATAK position appears on `tak.cot.out` within 10 s (`nats sub tak.cot.out` shows your uid and callsign).
- A projected entity on `tak.cot.inject` (for example `OM.drone.sim-x1` from the projector) shows on ATAK once, not twice.

## 2. NatsBridgePlugin 1.1.0 loaded (A21-P1)

Needs the TAK Server 5.8 plugin SDK JARs (tak.gov) in `services/takserver/tak-server-sdk/tak-server-sdk/`.

```sh
cd services/takserver/plugin-src
gradle clean shadowJar test
unzip -l build/libs/takserver-nats-plugin-1.1.0.jar | grep -E ' (io/nats|org/slf4j)/'   # expect no output (relocated)
git rm ../plugin/takserver-nats-plugin-1.0.0.jar && cp build/libs/takserver-nats-plugin-1.1.0.jar ../plugin/
```

Open a PR. After it merges, `build-takserver.yml` pushes `ghcr.io/onemindos/takserver-base:plugins-5.8-onemind`.
Pin that digest in `platform/takserver/deploy/takserver-plugins-deployment.yaml` in a PR.
`platform-takserver` auto-syncs, so this PR restarts the plugins pod.

Expected results:

```
$ kubectl -n takserver logs deploy/takserver-plugins | head -1
NOTE: Picked up JDK_JAVA_OPTIONS: -Dloader.path=WEB-INF/lib-provided,WEB-INF/lib,WEB-INF/classes,lib/ …
$ kubectl -n takserver logs deploy/takserver-plugins | grep NatsBridgePlugin
NatsBridgePlugin v1.1.0 loaded: url=nats://tak-nats-leaf.takserver.svc.cluster.local:4222 consumer=tak.cot.inject queue=tak-cot-inject producer=tak.cot.out groups=[__ANON__, Red] status=60s
NatsBridgePlugin subscribed to tak.cot.inject (queue tak-cot-inject)
NatsBridgePlugin v1.1.0 status nats=CONNECTED injected=… (every 60 s)
$ nats server request leafz      # the tak-nats-leaf connection lists a tak.cot.inject subscription
```

Then set `natsProducerEnabled: false` in `services/takserver/plugin/tak.server.plugins.NatsBridgePlugin.yaml`
(A21-P3), so tak-client is the only producer of `tak.cot.out`.

## 3. Housekeeping

- A21-P4: rotate the TAK CA key passphrase and the `omos-cot-tap` user password. They were in
  `services/takserver/cot-tap/README.md` in plain text, and they are still in Git history.
- A21-P5: after a week of `tak.cot.out` from tak-client, delete `services/takserver/cot-tap/`
  and revoke the `omos-cot-tap` user and certificate.
