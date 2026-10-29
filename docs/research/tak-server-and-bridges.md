# TAK Server 5.8, the omos bridges, the mesh gateway and the ATAK plugin SDK

Lane: tak-server (Phase 1 / M0, research item 2). Written 2026-10-29 on the build
machine. Includes spike **S4** (Common Commo for macOS arm64).

Read-only work only: no omos files were changed, nothing was committed, and the
cluster was queried with `get` and `logs` only.

## Sources

Primary sources read for this lane. `atak-civ` is pinned at commit `9f6893dd` (2025-12-01, `main`), and `Server` at tag `5.7-RELEASE-14`.

| # | Source | What was read |
|---|---|---|
| S-1 | `omos` main `62e3d22b`: `services/takserver/plugin-src/` (`NatsBridgePlugin.java`, `build.gradle`, `plugin-config.yaml`), `services/takserver/plugin/tak.server.plugins.NatsBridgePlugin.yaml` | NatsBridgePlugin source and deployed config |
| S-2 | `omos` main: `services/takserver/cot-tap/{README.md,tap.py,deployment.yaml}` | cot-tap |
| S-3 | `omos` branch `origin/feat/tak-mesh-gateway` `977b7eb2` (2026-10-29): `services/bridge/bridges/tak/*`, `docs/architecture/decisions/0003-tak-native-clients-and-mesh.md`, the diff to `docs/NATS-DOCTRINE.md` | Mesh gateway v0.2.0 |
| S-4 | `omos` main: `platform/nats/streams/cot.yaml`, `geo.yaml`; `docs/NATS-DOCTRINE.md` §9 (lines 188–230, 330–355); `platform/takserver/values-onemind.yaml`; `services/takserver/build/README-BUILDS.md` | Streams, grammar, image provenance |
| S-5 | https://github.com/TAK-Product-Center/atak-civ `takproto/README.txt` | TAK Protocol framing, streaming and mesh negotiation |
| S-6 | atak-civ `commoncommo/` (`core/impl/Makefile`, `core/ChangeLog.txt`, `core/missionpackage-rules.txt`, `core/impl/missionpackagemanager.cpp`, `core/impl/enrollmentmanager.cpp`, `jni/native/Makefile`, `objc/`, `README-DISTRIBUTION.txt`) | Commo build, versions, data-package and enrollment URLs |
| S-7 | atak-civ `takthirdparty/` (`Makefile`, `host-detect.mk`, `target-config/macos-64.mk`, `mk/commoncommo*.mk`, `distfiles/README`, `distfiles/*` LFS pointers), `BUILDING.md`, `.gitattributes` | Commo dependency build system |
| S-8 | atak-civ `plugin-examples/helloworld/{README.md,app/build.gradle}`, `atak-gradle-takdev/`, `pluginsdk.zip` (size only), `VERSION.md` | ATAK plugin SDK |
| S-9 | https://github.com/TAK-Product-Center/Server tag `5.7-RELEASE-14`: `takserver-war/.../sync/api/MissionApi.java`, `SubscriptionApi.java`, `sync/MetadataApi.java`, `sync/ContentServlet.java`, `sync/MissionPackageQueryServlet.java`, `tak/tls/CertManagerApi.java`, `sync/api/ContactsApi.java`, `sync/api/CotApi.java`, `util/VersionApi.java`, `network/FederationApi.java`, `network/BaseRestController.java`, `oauth/OAuthApi.java`, `groups/GroupsApi.java`; the repo tree (12,123 paths per `gh api repos/TAK-Product-Center/Server/git/trees/5.7-RELEASE-14?recursive=1`, not truncated; corrected by the verifier from 12,124); the releases list | TAK Server REST surface |
| S-10 | `gh api repos/TAK-Product-Center/Server/releases`: newest is `5.7-RELEASE-14` (2026-04-03). Tags: 5.7-RELEASE-14, 5.6-RELEASE-35, 5.5-RELEASE-82, … | No public 5.8 source |
| S-11 | Cluster, read-only (`kubectl --context do-nyc3-omos-cloud -n takserver get deploy,sts`; `logs pod/takserver-plugins-5db4b57b98-8rrcc`; `get deploy cot-tap`) | Live state on 2026-10-29 |
| S-12 | Local commands on the build machine (outputs below): `go test -v ./...` for the gateway; a gateway run against the local nats-server; `takproto` packets sent from Python | Gateway proof |
| S-13 | Web search, secondary only: https://hub.docker.com/r/pvarki/tak-server-dist/tags (lists a `5.8-RELEASE-69` build); https://cloudrf.com/?p=11132 and https://en.wikipedia.org/wiki/Android_Team_Awareness_Kit (plugin signing and the Play Store build); https://github-users.yhajoa.workers.dev/engindearing-projects/omni-COT/releases ("TAK.gov Third-Party Pipeline") | Context only; each claim taken from these is marked unverified |

## Verified facts

### 1. TAK Server version and where 5.8 comes from

- The cluster runs **TAK Server 5.8-RELEASE-69**. The plugins pod logs `Starting PluginService v5.8-RELEASE-69 using Java 17.0.20` (S-11). The images are `ghcr.io/onemindos/takserver-base:*-5.8-onemind`, built from `takserver-docker-hardened-5.8-RELEASE-69` (`values-onemind.yaml:12`, S-4). FedHub and LDAP come from `5.8-RELEASE-79` zips (`README-BUILDS.md:13-14`).
- **5.8 source is not public on GitHub.** The newest GitHub release of `TAK-Product-Center/Server` is `5.7-RELEASE-14` (S-10). No official 5.8 release notes were found (S-13). The 5.8 distribution comes from tak.gov, which needs an account. So every endpoint below is **verified against 5.7 source** and only **assumed** for 5.8.

### 2. TAK Server REST surface (5.7-RELEASE-14 source, S-9)

Every `*Api` controller read here extends `BaseRestController`, which is annotated `@RequestMapping("/Marti/api")` (`BaseRestController.java:8`), **except `OAuthApi`**: it is a bare `@RestController` with no class-level mapping (`OAuthApi.java:66-67`), so its paths are **not** under `/Marti/api` (verifier correction). All other paths below are under `https://<host>:8443/Marti/api`; the servlets are under `/Marti`; `OAuthApi` paths are at the web-app root (inferred from the absolute `/Marti/api` prefix of the other controllers; the context path itself is **unverified**).

| Area | Endpoints (method and path) | Source |
|---|---|---|
| Missions (Data Sync) | `GET/PUT/POST/DELETE /missions/{name}`, `GET /missions`, `GET /missions/guid/{guid}`, `PUT /missions/{name}/copy`, `GET /missions/{name}/archive`, `POST /missions/{name}/send`, `PUT/DELETE /missions/{name}/contents`, `PUT /missions/{name}/contents/missionpackage`, `GET /missions/{name}/changes`, keyword endpoints, `GET /missions/{name}/token`, `GET/PUT/POST/DELETE /missions/{name}/subscription`, `GET /missions/all/subscriptions`, `GET /missions/{name}/subscriptions[/roles]`, `GET/PUT /missions/{name}/role`, `GET /sync/search`; each also exists in a `/missions/guid/{guid}/…` form | `MissionApi.java` |
| Data packages (mission packages) | `GET /Marti/sync/missionquery?hash=` (servlet), `/Marti/sync/missionupload` (upload), `/Marti/sync/content` (download, servlet), `PUT /Marti/api/sync/metadata/{hash}/{metadata}` and `…/keywords`, `…/expiration` | `MissionPackageQueryServlet.java:37`, `ContentServlet.java:53`, `MetadataApi.java`; Commo `missionpackagemanager.cpp:1119-1186` builds exactly `/Marti` + `/sync/missionquery`, `/sync/missionupload`, `/api/sync/metadata/<sha256>/tool` |
| Certificate enrollment | `GET /tls/config`, `POST /tls/signClient`, `POST /tls/signClient/v2`, `GET /tls/makeClient`, `GET /tls/makeClientKeyStore`, `POST /tls/signServer` | `CertManagerApi.java`; Commo `enrollmentmanager.cpp:434` uses `https://host:port/Marti/api/tls/` |
| CoT query | `GET /cot/xml/{uid}`, `GET /cot/xml/{uid}/all`, `GET/POST /cot`, `GET /cot/sa`, `GET /cot/matchUid` | `CotApi.java` |
| Contacts, groups and subscriptions | `GET /contacts/all[/lite|/full]`; `GET /groups/all`, `GET /groups/user`, `GET /users/all`; `GET /subscriptions/all`, `POST /subscriptions/add`, `DELETE /subscriptions/delete/{uid}`, `/subscriptions/{clientUid}/filter`, `PUT /groups/active` | `ContactsApi.java`, `GroupsApi.java`, `SubscriptionApi.java` |
| Federation (admin) | `GET /federates`, `GET/POST/PUT /outgoingconnections`, `GET /activeconnections`, `/federategroups*`, `/federategroupsmap/{id}`, `/federatecagroups*`, `/federate-outbound-groups-hop-limit/{id}`, `GET/POST /federatecertificates`, `PUT /federatemissions/{id}`, `POST /generateFederationJwtToken`, `POST /federatecatokenauth` | `FederationApi.java` |
| Auth and version | `OAuthApi` (no `/Marti/api` prefix, see above): `GET /login/auth`, `/login/refresh`, `/token/access`, `/login/.well-known/openid-configuration`. `VersionApi` (under `/Marti/api`): `GET /version`, `/version/info`, `/version/config`, `/node/id` | `OAuthApi.java:77,281,340,358`, `VersionApi.java:35,45,55,70` |
| Server plugin API (Java, in-process) | `PluginApi`, `PluginMissionApi`, `PluginFileApi`, `PluginDataFeedApi`, `PluginCoreConfigApi`, `SystemInfoApi`, `PluginSelfStopApi`, plus their `Distributed*` cluster versions in `takserver-core/.../tak/server/api` | repo tree (S-9) |

Commo's data-package rules (S-6, `missionpackage-rules.txt`): the sender first queries `missionquery` by hash, then uploads, then sets `tool=private`, then sends a CoT file-transfer request with the URL. Mesh recipients get a URL from a local `http://<ip>:<port>/getfile?file=<id>` web server (with `peerHosted`/`httpsPort` attributes after 2020-05-27). HTTPS server certificates are checked against the streaming trust chain, with no hostname verification.

### 3. TAK Protocol (S-5, `takproto/README.txt`)

- **Mesh** carries one CoT per UDP datagram. **Directed** messages open a TCP connection, send, and close. **Streaming** (to a server) sends XML separated by `<?xml …?>` headers, or negotiates v1 through a `TakControl` offer, request and response; the version is the same in both directions.
- The v1 mesh header is `0xbf 0x01 0xbf` followed by a `TakMessage` protobuf. Detail extensions are used only when the whole element can be represented (the "whole-element rule").
- **Mesh negotiation rules 1–5:** send TakControl at least every 60 s; track min/max version per contact; contacts fall back after 2 minutes without TakControl (3c); broadcast at the highest version **all known contacts, including the sender itself,** support (rule 4); send TakControl immediately when that version changes (rule 5). So a gateway that hears no other contacts correctly sends v1.

### 4. omos NatsBridgePlugin (S-1)

- It is a 163-line Java class that extends `MessageSenderReceiverBase` and builds against a local `tak-server-sdk` JAR (`compileOnly`; tak.gov credentials not needed). It shades `io.nats:jnats:2.20.2` and targets Java 17. The JAR is committed at `services/takserver/plugin/takserver-nats-plugin-1.0.0.jar`.
- **NATS to TAK:** it subscribes to `natsConsumerSubject` (`tak.cot.inject`), converts each message with `getConverter().cotStringToDataMessage(xml, groups, …)` and calls `send()`. The deployed config uses groups `__ANON__` and `Red`, and `natsUrl: nats://tak-nats-leaf.takserver.svc.cluster.local:4222`.
- **TAK to NATS:** `onMessage` converts with `StreamingProtoBufHelper.proto2cot` and publishes XML to `tak.cot.out`. omos's cot-tap README says that in cluster mode `onMessage` never receives anything ("the plugin's cluster-NATS connection shows `subs=[]`"). That claim was **not** re-checked against the 5.8 source, which is not public.
- The NATS options set only the server URL, infinite reconnect and a connection name: no credentials and no TLS. Authentication depends entirely on the `tak-nats-leaf` sidecar deployment. Core NATS publish has no acknowledgement. Messages are dropped (with a log warning) while disconnected.
- **Live (S-11):** the plugins pod (image digest `sha256:f4824fba…`, Helm tag `plugins-upstream-5.8-onemind`) logs `starting PluginLoader` and `starting PluginMessenger`. In its 379 log lines there is **no** `NatsBridgePlugin` line, although the plugin logs its config at construction and `connecting to` on start. The values file comment says "Plugin is baked into this image". **Whether the bridge is loaded live is unverified, and the evidence leans towards "not loaded".**

### 5. cot-tap (S-2)

- It is a 139-line Python script. It opens a mutual-TLS client connection to `takserver-messaging-service:8089`, splits the byte stream on `<event`…`</event>`, and publishes each event to `tak.cot.out` on `nats://tak-nats-leaf:4222` with `nats-py`. Server certificate verification is **off by default** (`VERIFY_SERVER=false`, `ssl.CERT_NONE`).
- The image is `python:3.12-slim` with `pip install nats-py` at start, so the image is unpinned and dependencies install at runtime. The ConfigMap and Secret are applied with `kubectl`, and there is no Argo Application.
- **Live (S-11):** `deployments.apps "cot-tap" not found`. **cot-tap is not running**, so nothing in the cluster provides the TAK-to-bus leg (`tak.cot.out`), unless the plugin does after all (see §4, unverified).
- The README contains example credentials in plain text (a CA key passphrase and a TAK user password). The values are not repeated here.

### 6. TAK mesh gateway, `feat/tak-mesh-gateway` (S-3, S-12)

- The branch has one commit, `977b7eb2` (2026-10-29), on top of main `62e3d22b`: 13 files, +1812/−475. It adds ADR 0003 and a §9 paragraph in the doctrine. **No PR exists yet** (the run assigns this to feature A7).
- It is written in Go (`go 1.25.0`, `nats.go v1.52.0`, `protobuf v1.36.12`) under the Apache-2.0 licence. The codec is hand-written with `protowire` from the spec; no GPL code is used.
- Subjects: `tak.cot.mesh.rx` (publish, with headers `Nats-Msg-Id`, `Tak-Gateway`, `Tak-Delivery`, `Tak-Proto`, `Tak-From`), `tak.cot.mesh.tx` (subscribe, request/reply `ok`), optional `tak.cot.inject` (`MESH_TO_TAK_SERVER`, default `false`, which matches decision 0004 #6), and `tel.fleet.edge.{id}.status`. All four fit the locked grammar: `tak.cot.>` is listed, and `edge` is in the fleet class vocabulary (`NATS-DOCTRINE.md:218`). They land in stream `COT` (`tak.cot.>`, R3, 30 days, `duplicateWindow: 2m`, `cot.yaml`), so `Nats-Msg-Id` deduplication across gateways works with the existing stream.
- **Unit tests pass here:** `go test -v ./...` (Go 1.27.3, darwin/arm64) gave 9/9 PASS in 0.328 s: SA protobuf round trip, whole-element rule, typed copy ignored, TakControl wire, garbage rejected, negotiation, dedup ID stable across protocols, multicast loopback, directed TCP. `go vet` was clean.
- **End-to-end, mesh to bus (pass):** I ran the gateway (`GATEWAY_ID=s4lane MESH_GROUP=239.2.3.1:16969 MESH_INTERFACE=lo0 DIRECT_TCP_PORT=0`) against the local nats-server. A v1 packet built with `takproto.xml2proto(xml, TAKProtoVer.MESH)` (header `bf01bf`, 124 bytes) and multicast on 127.0.0.1 appeared on `tak.cot.mesh.rx` with `Tak-Proto: 1`, `Tak-Delivery: mesh`, `Nats-Msg-Id: cot-f00b…`. The XML was intact, including `__group`.
- **End-to-end, bus to mesh (pass, with one defect):** `nats req tak.cot.mesh.tx '<event …>'` replied `ok`. The datagram was **not** seen by a listener joined on 127.0.0.1. It **was** seen by a listener joined on the default interface (`route -n get 239.2.3.1` gives `en0`) as a v1 frame (`bf01bf`, 113 bytes). The cause is `mesh.go:84`, `send, err := net.DialUDP("udp4", nil, cfg.Group)`: `MESH_INTERFACE` is applied only to the receive socket (`ListenMulticastUDP`, `mesh.go:79`), never to transmit.
- That listener received 4 identical copies. Cause found afterwards: 4 of my test gateway processes were still running (earlier `pkill` patterns missed them because they started as `./takgw`), and all of them were subscribed to `tak.cot.mesh.tx`. This was a test artifact, not a gateway defect. All 4 were stopped.

### 7. ATAK plugin SDK (S-8)

- atak-civ ships `plugin-examples/` with 27 examples (helloworld, plugintemplate, plugintemplate-compose, cotinjector, commout-simplesocket, hello3d, hellojni, videooverlay, …), the `atak-gradle-takdev` Gradle plugin source, and `pluginsdk.zip` (244,262 bytes). Verifier note: in the repo `pluginsdk.zip` is a **Git LFS pointer** (131 bytes, `oid sha256:f2111985…`, `size 244262`, read with `gh api repos/TAK-Product-Center/atak-civ/contents/pluginsdk.zip?ref=9f6893dd`); the size is the pointer's declared size, and the object itself was not downloaded.
- `helloworld/app/build.gradle` targets `ATAK_VERSION = '5.6.0'` and `takdevVersion = '3.+'`. It resolves the takdev plugin from `takrepo.url` with `takrepo.user`/`takrepo.password` (defaults `https://localhost/`/`invalid`), or from a local `atak-gradle-takdev.jar`. Its README says private-plugin interfaces "will likely change from version to version".
- In atak-civ, `VERSION.md` begins at 4.6.1.0. The public source lags the 5.x ATAK that plugins target (example targets 5.6.0).

### 8. Common Commo build facts (S-6, S-7)

- The latest ChangeLog entry is **Commo 1.17.2** (2025-09-07), with takthirdparty 4.2.5 (`support/takthirdparty-version: release-4.2.5`).
- It is built by `takthirdparty`: `make TARGET=<target> build_commoncommo`. The build order is zlib, libiconv, libxml2, openssl (quictls 3.0.14+quic), nghttp2, curl 8.9.1, ngtcp2 1.6.0, protobuf cpp-3.21.0, libmicrohttpd 1.0.1, then commoncommo (`Makefile:55-68`, `distfiles/README`).
- Targets include `macos-64` (plus `ios-arm64`, `linux-arm64v8`, `win64`, `android-*`). `host-detect.mk` recognises Darwin. `BUILDING.md` warns: "The TAK Third Party build system is only confirmed to work on Linux."
- `macos-64.mk` was written for Intel: it uses `CC=gcc`, has no `-arch` flag (so the build follows the host, arm64 here), sets `kdu_PLATFORM=MAC-x86-64-gcc` (KDU is not used by commo), configures openssl `no-asm no-shared`, and sets **`commoncommo_BUILDJAVA=yes`**. That requires `ant` and a JDK, which this host lacks: `/usr/bin/java` says "Unable to locate a Java Runtime".
- `jni/native/Makefile` defaults to `PGSCUTILS_LIBARCHDIR := linux/x86_64` with `macosx/x86_64` commented out. There is no arm64 macOS path.
- The Objective-C binding (`objc/commoncommo/impl/*.mm`, builds `libcommoncommoobjc.dylib`) exists, but `commoncommo_BUILDOBJC` is not set for `macos-64`. It is in the iOS config path.
- The core library compiles with `-Wall -Werror -std=c++11` (`core/impl/Makefile`). Newer Apple clang may raise warnings that become errors (**unverified**, because the build did not run).
- All third-party source tarballs in `distfiles/` are **Git LFS pointers** (`.gitattributes`: `*.tar.gz`, `*.tar.bz2`, `*.zip`). The eight needed were downloadable from `media.githubusercontent.com`, and each matched its pointer's SHA-256 (see S4).

## S4: Common Commo for macOS arm64

**Result: blocked by the permission policy at the build step. The decision 0004 #3 fallback applies.**

Steps that ran, all inside `/Users/danielcarku/Desktop/work/.tools/src` (52 MB used; free space stayed at 24 GiB):

```sh
mkdir -p /Users/danielcarku/Desktop/work/.tools/src && cd /Users/danielcarku/Desktop/work/.tools/src
GIT_LFS_SKIP_SMUDGE=1 git clone --filter=blob:none --no-checkout --depth 1 \
  https://github.com/TAK-Product-Center/atak-civ.git
cd atak-civ
git sparse-checkout set --no-cone /commoncommo/ /takthirdparty/ /takproto/ /LICENSE.md /VERSION.md
git -c filter.lfs.smudge= -c filter.lfs.required=false checkout main   # HEAD 9f6893dd, 18 MB
# git-lfs is not installed: fetch each LFS object and check it against its pointer
cd takthirdparty/distfiles
for f in zlib.tar.gz libiconv.tar.gz openssl.tar.gz nghttp2.tar.bz2 curl.tar.bz2 \
         ngtcp2.tar.bz2 protobuf.tar.gz libmicrohttpd.tar.gz; do
  curl -sSL -o $f.dl "https://media.githubusercontent.com/media/TAK-Product-Center/atak-civ/main/takthirdparty/distfiles/$f"
  # sha256 of $f.dl compared with the "oid sha256:" line of the pointer, then mv $f.dl $f
done
# all 8 OK: zlib 1512791, libiconv 5264188, openssl 15475729, nghttp2 1999855,
#           curl 3278263, ngtcp2 807796, protobuf 4838725, libmicrohttpd 2239593 bytes
```

The step that was denied:

```sh
cd /Users/danielcarku/Desktop/work/.tools/src/atak-civ/takthirdparty
make TARGET=macos-64 GDAL_USE_KDU=no build_commoncommo
```

The error, verbatim: `Permission for this action was denied by the Claude Code auto mode classifier. Reason: [Code from External].` Following the run rules, I did not try another way around it. This is the same class of denial PREFLIGHT.md recorded for the ArduPilot SITL build.

**Expected blockers if the build is allowed** (from reading the source, S-7). These are predictions, not observed errors:

1. `commoncommo_BUILDJAVA=yes` needs `ant` and a JDK, and neither is on this host. Override with `make … commoncommo_BUILDJAVA=` for a C++-only build, or install a JDK and ant under `.tools`.
2. `jni/native/Makefile` assumes `x86_64` paths for macOS.
3. Possible autotools needs (libiconv/libxml2 patches, `autoconf`/`automake` not installed) and `-Werror` with newer clang.
4. No arm64-specific macOS config: openssl `./config` should detect `darwin64-arm64` (**unverified**).

**Fallback applied (decision 0004 #3):** the omos mesh gateway is the **only mesh path**. Feature **C4** (Commo inside the client) moves to **blocked**, with the error above. The owner can retry with the exact commands above, on a machine that allows running fetched build code, after setting `commoncommo_BUILDJAVA=` and adding `commoncommo_BUILDOBJC=yes` for the Objective-C dylib. The gateway already passes v0/v1 mesh tests on this Mac (§6), so nothing in M0–M2 depends on C4.

## How it applies to OneMind

OneMind joins a TAK network at four levels:

| Level | Path | Status |
|---|---|---|
| **Mesh peer** (no server) | omos mesh gateway on the edge host with a local leaf. The client reads `tak.cot.mesh.rx` and writes `tak.cot.mesh.tx` (C1, C2, A7) | Works locally (§6). Fix the TX interface before LAN testing on the Mac |
| **Server feed** | Bus to TAK: `tak.cot.inject` through NatsBridgePlugin. TAK to bus: `tak.cot.out` through cot-tap or the plugin | **Both legs unproven live.** cot-tap is not deployed, and the plugin shows no log line. A6/A1 depend on the inject leg |
| **Server client** (C3) | The client opens its own mutual-TLS streaming connection on 8089 (XML, then optional v1 negotiation, §3), enrolls through `/Marti/api/tls/signClient/v2`, and uses mission sync (`/Marti/api/missions/*`, subscriptions, changes) and data packages (`/Marti/sync/missionquery`, `missionupload`, `content`) | APIs mapped from 5.7 source. Needs a TAK user certificate, which this machine lacks (PREFLIGHT) |
| **Plugin inside ATAK** | ATAK plugin built with `atak-gradle-takdev` against ATAK 5.6 that talks NATS (WebSocket) or the gateway | Later. Needs takrepo access, and signing for distribution (§Unverified) |
| **Federation partner** | FedHub is already live (`deployment.apps/fedhub 1/1`, S-11). The admin API is in `FederationApi` | Later. Not needed for M0–M2 |

Client contract consequences:

- Treat `tak.cot.out` and `tak.cot.mesh.rx` as the same payload (CoT XML). Both are in stream `COT`. Derive per-UID state in the entity service (decision 0004 #7, option 1), not in the client.
- Data packages: implement the Commo flow (query by hash, upload, file-transfer CoT) for server mode. For mesh mode, the gateway would need a `getfile` HTTP server. It does not have one today, so C5 over the mesh is a gateway extension.

## Risks

1. **The TAK Server leg may be broken live.** No cot-tap deployment, and no NatsBridgePlugin log line in the 5.8 plugins pod. If true, nothing from Redpanda Connect on `tak.cot.inject` reaches ATAK, and nothing from TAK reaches the bus. This affects A1, A6, C1 and C2 over the server path. The live check needs NATS credentials or a log check during a plugin restart. Both are outside this lane's read-only scope.
2. **Gateway transmits on the wrong interface.** `mesh.go:84` ignores `MESH_INTERFACE`. On the owner's Mac (Wi-Fi plus Tailscale `utun` plus perhaps Ethernet), SA may leave by the default route instead of the ATAK LAN.
3. **5.8 is closed.** The API map is from 5.7 source, and 5.8 changes are unknown until someone with tak.gov access reads its release notes.
4. **Security of the bridges.** NatsBridgePlugin connects without credentials or TLS (it relies on the leaf). cot-tap disables server certificate verification by default, installs dependencies at runtime from an unpinned `python:3.12-slim`, and is deployed with `kubectl apply`, which is against omos rules. The cot-tap README commits example credentials in plain text, which should be rotated and removed. Anyone who can publish `tak.cot.mesh.tx` or `tak.cot.inject` can put markers on every device. Restrict both with NATS permissions.
5. **Plugin cluster-mode limitation.** If the cot-tap README is right (no `onMessage` delivery in cluster mode), the plugin can never be the TAK-to-bus leg, and cot-tap (or a OneMind streaming client) must exist.
6. **Commo build fragility.** Linux-only support statement, x86-era macOS config, Java on by default, LFS-hosted dependencies (GitHub LFS bandwidth could run out).
7. **ATAK plugin model churn.** Private plugin APIs change per ATAK version, and distribution signing is gated by tak.gov.

## Recommendation

1. **Keep the mesh gateway as the only mesh path for this run** (decision 0004 #3 fallback; C4 blocked). Open the omos PR for `feat/tak-mesh-gateway` (A7) and fix it in that PR: bind the transmit socket to `MESH_INTERFACE` (set `IP_MULTICAST_IF`, for example with `golang.org/x/net/ipv4` `PacketConn.SetMulticastInterface`), and add a test that transmits on a named interface. Also note in the README that every running gateway on a subject transmits, so run one gateway per mesh interface.
2. **Prove or fix the TAK Server leg before relying on it.** Write an omos issue or PR that (a) makes the plugins image's NatsBridgePlugin load visibly (log line, `tak.cot.inject` to ATAK test), and (b) moves cot-tap into Git as an Argo-managed Deployment with a pinned GHCR image, a SealedSecret for its certificate, `VERIFY_SERVER=true`, requests and limits, and a `docs/STACK.md` row. Also remove the example credentials from its README. Until then, client lanes use the local fallback: gateway plus `takproto` messages.
3. **C3 (client as TAK contact):** implement a streaming client in the OneMind contracts (8089 mTLS, XML first, v1 negotiation per `takproto/README.txt`), with enrollment through `/Marti/api/tls/signClient/v2`. Test it against the gateway's directed TCP locally. Put the TAK Server test on the owner checklist (needs a certificate).
4. **Missions and data packages (D2, C5):** use the 5.7 endpoint map above as the contract. Mark 5.8 verification as an owner or tak.gov task.
5. **ATAK plugin:** defer. When it starts, build from `plugin-examples/plugintemplate` against ATAK 5.6.0 with `atak-gradle-takdev`, connecting to the bus through NATS WebSocket. Note the takrepo and signing prerequisites.
6. Retry S4 only on a host whose policy allows the build, using the commands above with `commoncommo_BUILDJAVA=` and `commoncommo_BUILDOBJC=yes`.

## Independent verification (2026-10-29)

A second lane re-checked 30 claims against the primary sources: the local atak-civ clone at `9f6893dd`; `gh api` reads of atak-civ (`BUILDING.md`, `.gitattributes`, `plugin-examples/`, `pluginsdk.zip`) and of `TAK-Product-Center/Server` at `5.7-RELEASE-14` (13 controller and servlet files plus the tree); omos `62e3d22b` and `origin/feat/tak-mesh-gateway` `977b7eb2`; and the cluster, read-only (`get deploy,sts`, `logs` with grep counts only; no Secret was read). 27 claims were confirmed as written, and 3 were corrected in place:

1. `OAuthApi` does not extend `BaseRestController`, so its paths have no `/Marti/api` prefix (§2).
2. The Server tree at 5.7-RELEASE-14 has 12,123 paths, not 12,124 (S-9).
3. `pluginsdk.zip` is a Git LFS pointer; 244,262 bytes is the pointer's declared size (§7).

The gateway's unit tests were reproduced: 9/9 PASS in 0.269 s (`go test -count=1 -v ./...`, Go 1.27.3 darwin/arm64, offline with `GOPROXY=off`), on a scratch export that is byte-identical to the branch files. The end-to-end multicast runs in §6 were not repeated. No licence violations were found: the gateway is Apache-2.0 with SPDX headers and depends only on `nats.go` and `protobuf`, and no worldmonitor or worldwideview code is present. The cluster checks stayed within the safety rules.

## Unverified items

- Whether NatsBridgePlugin is loaded in the live 5.8 plugins pod. The evidence (no log line, an `upstream` image tag) leans towards no.
- The cot-tap README claim that TAK Server cluster-mode plugins receive no `onMessage` traffic. Not checked against source, because 5.8 is not public.
- All TAK Server **5.8** API behaviour. Endpoints are verified only in 5.7-RELEASE-14.
- Every predicted Commo build blocker in S4. The build never ran.
- ATAK plugin signing: plugins need TAK.gov signing or the third-party pipeline, and the Play Store ATAK-CIV build is not plugin-capable. Secondary sources only (S-13).
- The contents of `pluginsdk.zip` (only the LFS pointer's declared size, 244,262 bytes, was read; the LFS object was not fetched).
- Whether openssl's `./config` picks `darwin64-arm64` under `macos-64.mk`.
