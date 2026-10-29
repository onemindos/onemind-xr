# TAK protocols: how OneMind becomes a first-class TAK citizen

Lane: tak-deep (Phase 1 / M0). Date: 2026-10-29. Author: research agent, OneMind build run.
Status: research. Nothing here has been deployed or committed to omos.

Every claim cites a file, URL or command output. **Unverified** marks anything not proven.

**Independent verification (2026-10-29).** A second agent re-fetched S1–S2, S3–S5, S6 (`__video.xsd`), S7, S9 `VERSION.md`, S10 (5.7-RELEASE-14 tags, releases, `CoreConfig.example.xml`, `BaseRestController`, `CertManagerApi`, `VideoConnectionManagerV2`, `FeedV2`, `MissionApi`, `GroupsApi`, `SubscriptionApi`, `FederationApi`, `DistributedSubscriptionManager`, `detail/takmessage/fig/streaminginput.proto`, `LICENSE.md`), S11–S13, S14–S15 in the omos clone and S16 (read-only `kubectl get`), and checked 34 claims. 28 held. 6 were corrected in place (marked *verifier correction*): the HMAC input, iTAK's version, the gateway grep result, node-tak's `t-x-takp-v` handling, the plugin group config, and the wording of the takproto `access` quirk; one port note was clarified. No licence-rule or cluster-safety violation was found: no CloudTAK (AGPL) or GPL code is copied, cluster access was `get`/`logs` only, and the cot-tap CA password was not reproduced.

---

## Sources

Primary sources read in this lane (fetched with `gh api` or `curl` from raw.githubusercontent.com, 2026-10-29):

| # | Source | What was read |
|---|---|---|
| S1 | https://github.com/TAK-Product-Center/atak-civ/blob/main/takproto/README.txt | The whole TAK Protocol spec: v0, v1 framing, extensions, streaming and mesh negotiation, varints |
| S2 | `atak-civ/takproto/*.proto` (takmessage, takcontrol, cotevent, detail, contact, group, track, takv, status, precisionlocation) | v1 field numbers and mapping rules |
| S3 | `atak-civ/commoncommo/core/include/commo.h` | Commo public C++ API |
| S4 | `atak-civ/commoncommo/core/contact-rxtx-rules.txt`, `missionpackage-rules.txt`, `ChangeLog.txt` | Contact discovery, endpoint choice, data-package transfer rules; Commo 1.17.2 (2025-09-07) |
| S5 | `atak-civ/commoncommo/core/impl/cryptoutil.cpp`, `streamingsocketmanagement.cpp` | Mesh encryption, stream ping and timeouts |
| S6 | `atak-civ/takcot/xsd/details/*.xsd`, `takcot/README.md` | Registered CoT detail elements |
| S7 | `atak-civ/plugin-examples/` (list), `plugintemplate/app/build.gradle`, `plugintemplate/app/src/main/assets/plugin.xml`, `PluginTemplate.java`, `atak-gradle-takdev/README.md` | ATAK plugin model |
| S8 | `atak-civ/atak/ATAK/app/src/main/java/com/atakmap/android/missionpackage/file/MissionPackageManifest.java` | Data package format |
| S9 | `atak-civ/takkernel/README.md`, `atak-civ/LICENSE.md`, `atak-civ/VERSION.md` | TAK kernel layout, licence |
| S10 | https://github.com/TAK-Product-Center/Server tag `5.7-RELEASE-14` (newest public tag; no 5.8 tag exists): `BaseRestController.java`, `MissionApi.java`, `CertManagerApi.java`, `GroupsApi.java`, `SubscriptionApi.java`, `CotApi.java`, `ContactsApi.java`, `FederationApi.java`, `DataFeedApi.java`, `VersionApi.java`, `video/VideoConnectionManagerV2.java`, `video/FeedV2.java`, `DistributedSubscriptionManager.java` (via code search hit), `CoreConfig.example.xml`, `takserver-protobuf/src/main/proto/{fig,streaminginput,missionannouncement,takmessage,detail}.proto` | TAK Server APIs, ports, federation, protobuf |
| S11 | https://github.com/dfpc-coe/CloudTAK (README, repo metadata, release v13.116.0 of 2026-10-27) | CloudTAK |
| S12 | https://github.com/dfpc-coe/node-tak (README, `index.ts`, `lib/api/` listing; release v12.33.0, 2026-10-27), https://github.com/dfpc-coe/node-CoT (v14.56.0) | node-tak, node-CoT |
| S13 | `takproto` 3.0.1 Python package in `work/.tools/py` (source: https://github.com/snstac/takproto, MIT) | Encoder/decoder used in the spike |
| S14 | omos `services/takserver/plugin-src/` (NatsBridgePlugin), `services/takserver/cot-tap/`, `platform/takserver/values-onemind.yaml`, `docs/NATS-DOCTRINE.md` §9.8, `CLAUDE.md` | Current OneMind TAK integration |
| S15 | omos branch `origin/feat/tak-mesh-gateway` (commit 977b7eb2): `services/bridge/bridges/tak/*`, `docs/architecture/decisions/0003-tak-native-clients-and-mesh.md` | Mesh gateway v0.2.0 |
| S16 | `kubectl --context do-nyc3-omos-cloud get pods,svc,deploy` and `logs` (read-only) | Live TAK footprint |
| S17 | Web searches: iTAK plugin SDK; TAKX; "TAK Server 5.8" release notes. Results: App Store listing https://apps.apple.com/us/app/itak/id1561656396 ; CTI sheet https://ctistorage.blob.core.usgovcloudapi.net/ctifileshare/CTI%20Slick%20Sheets/TAKX%20-%202025.pdf ; Docker Hub https://hub.docker.com/r/pvarki/tak-server-dist/tags | Little primary material; see Unverified |

Spike artefacts (scratch, not committed): `/private/tmp/claude-501/-Users-danielcarku-Desktop-ONEMIND/28794bd7-008f-413a-92c6-5afd0f9c2e20/scratchpad/tak_spike.py`, its output `tak_spike.out`, and the Go cross-check output `xcheck.out` in the same folder. The script and its output are reproduced below so the evidence lives in this file.

---

## Verified facts

### 1. CoT and detail extensions

- A CoT message is an XML `<event>` with `version, uid, type, how, time, start, stale` (and optional `access, qos, opex, caveat, releasableTo`), one `<point lat lon hae ce le>`, and a free-form `<detail>` (S2 `cotevent.proto` comments map each field to its XML attribute).
- `access` is optional in the proto for legacy reasons but "now required in MIL-STD-6090"; new clients should fill it, and a value of `Undefined` should be sent by omitting the field (S2 `cotevent.proto`, field 2).
- Unknown position uses `999999` for hae/ce/le (S2 `cotevent.proto`, fields 12–14).
- TAK-registered detail elements, as XSDs in `takcot/xsd/details` (S6): `__cep, __chat, __chatreceipt, __geofence, __group, __lob, __milsym, __remotecontrol, __serverdestination, __signalInfo, __video, archive, attachment_list, chatgrp, color, contact, creator, emergency, environment, extrudeMode, fileshare, fillColor, height, height_unit, hierarchy, labels_on, link, mission, precisionlocation, remarks, shape, status, strokeColor, strokeWeight, takv, track, uid, usericon`. Top-level XSDs cover Chat, drawing shapes, Geo Fence, 2525 / icon-set / spot markers, range and bearing, Route.
- `__video` in the XSD has only a `url` attribute (S6 `__video.xsd`). `mission` carries `MissionChanges/MissionChange` with `contentResource` (filename, hash, keywords, mimeType, size, submissionTime, submitter), `externalData` and `details` (S6 `mission.xsd`). `__milsym` has an `id` (S6). `link` has `uid, type, relation, callsign, parent_callsign, production_time, remarks, point` (S6).
- The XSD repo says new details "will be documented here in XSD form for agreement between all of the TAK systems" (S6 `takcot/README.md`).

### 2. TAK Protocol v0 and v1 (S1, S2)

**v0 (legacy XML).** Mesh SA: one CoT XML message per UDP multicast datagram. Directed: open TCP, send one CoT XML, close. Streaming (to a server): XML events back to back; split on `</event>`; each must start with `<?xml ...?>` + newline, and no newline may follow `</event>`.

**Ground rules.** A client sending version V can decode V; every client decodes v0.

**v1 framing.**
- Mesh (UDP and directed TCP): `0xbf <version varint> 0xbf` then the payload. For v1 that is `bf 01 bf`.
- Streaming: `0xbf <payload length varint>` then the payload; no version byte, because the version is negotiated once per connection.
- Payload: exactly one `TakMessage` (protobuf 3) with `takControl = 1` and `cotEvent = 2` (S2 `takmessage.proto`). The proto package is `atakmap.commoncommo.protobuf.v1` (S2); README.txt calls it `atakmap::commoncommo::v1`, a documentation mismatch.
- Varints are unsigned protobuf varints, at most 10 bytes (S1, last section).
- Times are milliseconds since the Unix epoch (S2 `cotevent.proto`).
- `Detail` has typed fields `contact(2), group(3) for <__group>, precisionLocation(4), status(5), takv(6), track(7)`, the leftover XML in `xmlDetail(1)`, and `extensionDetails(8)` for registered binary extensions (S2 `detail.proto`).
- **Whole-element rule:** a typed field may only replace an entire XML element; if any attribute does not map, the whole element stays in `xmlDetail`. Receivers that find the same element in both places keep the XML and ignore the typed copy (S2 `detail.proto` comments).
- Typed fields carry only these attributes (S2): Contact `endpoint, callsign`; Group `name, role`; Track `speed, course`; Takv `device, platform, os, version`; Status `battery`; PrecisionLocation `geopointsrc, altsrc`.
- v1 may only add fields whose ignorance is "100% irrelevant" to every TAK app; anything else needs a new protocol version (S1).

**Detail extensions.** Opt-in, identified by integer IDs from a central TPC registry; unregistered IDs MUST NOT be used in public deployments; an extension replaces a whole element tree; receivers SHOULD flag messages they could not fully decode (S1).

**Streaming negotiation (server connections, S1).** XML first (auth message first if required) → server MAY send `t-x-takp-v` with `<TakControl><TakProtocolSupport version="1"/>` → client sends `t-x-takp-q` with `<TakRequest version="1"/>` and stops sending XML → server answers `t-x-takp-r` `<TakResponse status="true|false"/>` → on true both sides switch to streaming framing at the same version; the client waits at least one minute, then disconnects if there is no answer.

**Mesh negotiation (S1).** Every device that supports v>0 broadcasts `TakControl` (min/max version, decodable extension IDs) at least every 60 s. Each device tracks every contact's min/max: new contacts get the version they were heard on; contacts silent on TakControl for 2 minutes fall back to the version of their last message. Broadcasts use the highest version supported by *all* known contacts, else XML; on a change, send TakControl at once.

### 3. Commo internals (S3, S4, S5)

- Commo is the C++ comms library with JNI, Objective-C and Win32 bindings (directories `jni/`, `objc/`, `win32/` under `commoncommo/`, S9 listing). Latest changelog entry: 1.17.2, 2025-09-07 (S4).
- Public API (S3 `commo.h`): `addBroadcastInterface` (multicast or unicast, filtered by CoT message type), `addInboundInterface` (UDP, with multicast groups), `addTcpInboundInterface`, `addQuicInboundInterface`, `addStreamingInterface` (TAK Server), `sendCoT(ContactList, …, CoTSendMethod)`, `broadcastCoT`, `sendCoTServerControl`, `sendCoTToServerMissionDest`, `registerCoTDetailExtender(extensionId, element, extender)`, `setCryptoKeys`, `setMulticastLoopbackEnabled`, `setTTL`, `setupMissionPackageIO`/`sendMissionPackageInit`, `enableSimpleFileIO`/`simpleFileTransferInit`, `createCloudClient`, `enableEnrollment`/`enrollmentInit`/`enrollmentStart`.
- **Endpoint choice for a directed send** (S4 contact rules): the contact's TCP endpoint if ever seen; UDP if seen more recently; the TAK Server streaming endpoint only if neither has been heard for over 1 minute.
- **Discovery** comes from `<contact endpoint="ip:port:type">` in received UDP messages: `tcp` sets a TCP endpoint; `udp` sets `ip:6969` for SA and `ip:17012` for chat; `srctcp` substitutes the sender's source IP; `stcp` is ignored. A multicast broadcast rewrites the endpoint to `<outIp>:4242:tcp`. Messages relayed by a server are forced to `*:-1:stcp`, and directed sends through a server add `<marti><dest callsign=…/></marti>` (S4).
- **Mesh encryption** (optional, `setCryptoKeys`): two distinct 256-bit keys; AES-256-CBC with a random IV, then HMAC-SHA256 over IV‖ciphertext (the IV is prepended and included in the HMAC input, `cryptoutil.cpp` lines 96–127; wire layout IV | ciphertext | 32-byte HMAC; *verifier correction: the original said "over the ciphertext"*); once on, all inbound non-streaming traffic must be encrypted (S3 lines ~276–292, S5 `MeshNetCrypto::encrypt`).
- **Stream keep-alive:** ping after 15 s without receiving anything, repeat every 4.5 s, reset the connection at 25 s; reconnect every 15 s (S5 constants). The ping UID ends in `-ping` (S5). node-tak treats `t-x-c-t-r` as the pong (S12 `index.ts`).
- **Data package transfer** (S4 mission package rules): a recipient reached through a server gets the file uploaded to `https://<server>:<port>/Marti` (check `sync/missionquery?hash=`, upload `sync/missionupload`, set tool to private via `api/sync/metadata/<hash>/tool`); a mesh recipient gets a URL on the sender's own web server, `http://<ip>:<port>/getfile?file=<id>` (plus `peerHosted="true"` and `httpsPort` for newer receivers). Either way the receiver gets a CoT file-transfer request and replies ack/nack.
- **Enrollment** (S3): `enrollmentInit(host, port, verifyHost, user, password, useTokenAuth, caCert, …, keyLen, clientVersionInfo)` performs certificate enrollment against a TAK Server.

### 4. TAK Server (public source 5.7-RELEASE-14; OneMind runs 5.8)

- The newest public tag of TAK-Product-Center/Server is `5.7-RELEASE-14` (published 2026-04-03); there is **no public 5.8 source** (`gh api repos/TAK-Product-Center/Server/tags`). OneMind's images are built from "takserver-docker-hardened-5.8-RELEASE-69" (omos `platform/takserver/values-onemind.yaml` line 12). Every API fact below is from 5.7 source, so its 5.8 status is **unverified**.
- Licence: GPL-3.0 (S10 `LICENSE.md`).
- **Ports** (S10 `CoreConfig.example.xml`): CoT input TLS `8089` (`coreVersion="2"`), QUIC input `8090`, optional TCP/UDP 8087 and stcp 8088, multicast SA proxy `239.2.3.1:6969`, GeoChat proxy `224.10.10.1:17012` (the 8087/8088 inputs and both multicast proxies are commented out in the example, lines 8–12; *verifier clarification*); HTTPS `8443` (client cert), federation HTTPS `8444`, `8446` (no client cert; enrollment); federation server v1 `9000` (disabled), v2 `9001` (enabled).
- **REST base path** is `/Marti/api` (S10 `BaseRestController.java`).
- **Missions / Data Sync** (S10 `MissionApi.java`): `GET/PUT/POST/DELETE /missions/{name}`, by GUID `/missions/guid/{guid}`, contents `PUT/DELETE /missions/{name}/contents`, `PUT …/contents/missionpackage`, changes `GET /missions/{name}/changes`, subscriptions `GET/PUT/POST/DELETE /missions/{name}/subscription`, tokens `/missions/{name}/token`, roles, invitations, logs (`/missions/logs/entries`), `GET /missions/{name}/cot`, contacts, parent/child missions, KML, external data, passwords, expiration, feeds, map layers, layers, `/missions/{name}/archive`, `/missions/{name}/send`, keywords, `/sync/search`.
- **Mission sync notifications:** the server pushes CoT of type `t-x-m-c` with `<mission type="CHANGE">` to subscribers; sub-types include `t-x-m-c-l` (log) and `t-x-m-c-k` (keyword) (S10 `DistributedSubscriptionManager.java`, found by code search before the search API rate-limited). The `mission` detail format is in S6.
- **Data packages:** zip with `MANIFEST/manifest.xml` whose root is `MissionPackageManifest version="2"` holding `Configuration` and `Contents` (S8). Upload/query servlets exist (`MissionPackageUploadServlet`, `MissionPackageQueryServlet`, S10 tree) and are what Commo calls (S4).
- **Video:** `GET/POST /Marti/api/video`, `GET/PUT/DELETE /Marti/api/video/{uid}` (S10 `VideoConnectionManagerV2.java`). A FeedV2 has `uuid, active, alias, url, order, macAddress, roverPort, ignoreEmbeddedKLV, source, networkTimeout, bufferTime, rtspReliable, thumbnail, classification, latitude, longitude, fov, heading, range, width, height, bitrate` (S10 `FeedV2.java`). A legacy servlet `VideoConnectionManager` also exists.
- **Groups and channels:** `GET /groups/all`, `/groups/user`, `/groups/{name}/{direction}`, `/users/all` (S10 `GroupsApi.java`); a client turns its groups ("channels") on and off with `PUT /groups/active` or `/groups/activebits` (S10 `SubscriptionApi.java`).
- **CoT and contacts:** `GET /cot/xml/{uid}`, `/cot/xml/{uid}/all`, `/cot/sa`, `/contacts/all` (S10 `CotApi.java`, `ContactsApi.java`); data feeds `/datafeeds…` (S10 `DataFeedApi.java`); version `/version`, `/version/config`, `/node/id` (S10 `VersionApi.java`).
- **Certificate enrollment:** `GET /Marti/api/tls/config` (CSR template), `POST /tls/signClient` and `/tls/signClient/v2` (params `clientUid`, `version`; body base64 CSR), plus `/tls/makeClient`, `/tls/makeClientKeyStore`, `/tls/signServer` (S10 `CertManagerApi.java`).
- **Federation:** REST management under `/Marti/api` (`/federates`, `/outgoingconnections`, `/federategroups`, `/federategroupsmap`, `/federatecertificates`, `/federate-outbound-groups-hop-limit`, `/federatemissions/{id}`, `/generateFederationJwtToken`, …) (S10 `FederationApi.java`). The v2 wire is gRPC service `com.atakmap.FederatedChannel`: `ClientEventStream`/`ServerEventStream` (FederatedEvent), `BinaryMessageStream`, `ClientROLStream`/`ServerROLStream` (ROL, used for missions/files), `ServerFederateGroupsStream`/`ClientFederateGroupsStream`, `HealthCheck`, `GetAuthTokenByX509` (S10 `fig.proto`).
- **gRPC CoT input:** service `InputChannel` with `ClientTakMessageStream`/`ServerTakMessageStream` of `TakMessage` and `VersionCheck` (S10 `streaminginput.proto`).
- The server's `TakMessage` adds `submissionTime = 3` and `creationTime = 4` (S10 `takmessage.proto`); its `detail.proto` (5.7) has fields 1–7 only, **no `extensionDetails = 8`** (S10 `detail.proto`).

### 5. Plugin models

- **ATAK:** a plugin is an Android APK whose `assets/plugin.xml` declares `<extension type="gov.tak.api.plugin.IPlugin" impl="…">`; the class implements `gov.tak.api.plugin.IPlugin`, takes an `IServiceController`, and gets services such as `IHostUIService` to add a `ToolbarItem` and `Pane` (S7 `plugin.xml`, `PluginTemplate.java`). The template targets `ATAK_VERSION = '5.6.0'`, `minSdkVersion 21`, `targetSdkVersion 34`, builds with the `atak-takdev-plugin` Gradle plugin (`atak-gradle-takdev 3.+`, from a TAK repo URL with credentials or a local jar), and sets `atakApiVersion = com.atakmap.app@5.6.0.<flavor>` in the manifest (S7 `build.gradle` lines 12–74, 183, 232–233).
- 27 official examples include `cotinjector`, `commout-simplesocket`, `hello3d`, `videocollections`, `videooverlay`, `videomosaic`, `sensortester`, `selfmarkerdata`, `externalbt`, `platformsim`, `dsmmanager`, `plugintemplate-compose` (S7 listing).
- ATAK-CIV is GPL-3.0, with federal-employee work public domain where applicable (S9 `LICENSE.md`, `README.md`). Its `VERSION.md` still tops out at 4.6.1.0 (S9), so it is stale relative to the 5.6.0 template.
- `takkernel` is "Java and C++ code shared across TAK applications": `port → engine → shared`, plus Android AAR bundles and a non-public `controlled` project; it builds with Java, CMake, Python and Conan 1.65 (S9 `takkernel/README.md`).
- **iTAK:** the App Store listing (developer: United States Department of the Army, TAK Product Center; current version 2.12.3 of 2025-08-08, needs iOS 17.6, per `https://itunes.apple.com/lookup?id=1561656396`; *verifier correction: the original said 2.9.5*) mentions only a built-in GoTenna integration; no public plugin SDK was found (S17).
- **TAKX:** only a vendor sheet was found, describing a cross-platform SA tool with spectrum visualisation (S17). Plugin model not found.

### 6. CloudTAK, node-tak, node-CoT

- CloudTAK: "Full Featured in-browser TAK Client" plus ETL into a TAK Server; **licence AGPL-3.0**; release v13.116.0 on 2026-10-27; full ETL needs AWS (S11). It runs in the omos cluster: `cloudtak-api`, `-events`, `-retention`, `-tiles` pods (S16).
- node-tak: **MIT**, "TAK Server NodeJS SDK", v12.33.0. A TLS streaming client that splits on `</event>` (`REGEX_EVENT`), parses with node-CoT, pings and treats `t-x-c-t-r` as the pong, with a bounded write queue; typed REST modules for certificate, contacts, files, groups, mission (and invite, layer, log, property), package, subscriptions, video, oauth, query and more (S12). node-tak streams XML only: `index.ts` does read the server's `t-x-takp-v` offer, but only to store `TakControl/TakServerVersionInfo@serverVersion` as `this.version` (lines 241–251); it never sends `t-x-takp-q`, has no `0xbf` framing, and line 224 says "Eventually Parse ProtoBuf" (*verifier correction: the original said no negotiation code referenced `proto` at all*). Read from source at `main`, not run. Side fact: TAK Server's `t-x-takp-v` carries its version string, which a OneMind client can use to log the server version.
- node-CoT: MIT, v14.56.0 (S12).

### 7. OneMind today (omos)

- **NatsBridgePlugin** (`extends MessageSenderReceiverBase`): `onMessage` publishes CoT XML to `natsProducerSubject` (`tak.cot.out`); it subscribes to `natsConsumerSubject` (`tak.cot.inject`) and injects via `cotStringToDataMessage` (S14, `NatsBridgePlugin.java` lines 43–133; the source config `plugin-src/src/main/resources/conf/plugins/tak.server.plugins.NatsBridgePlugin.yaml` uses `groups: [__ANON__]`, but the copy next to the built JAR, `services/takserver/plugin/tak.server.plugins.NatsBridgePlugin.yaml`, uses `groups: [__ANON__, Red]`; *verifier correction*. Which one the live plugin pod loads is **unverified**).
- **cot-tap** exists because, per its README, cluster-mode TAK does not deliver the firehose to plugin `onMessage`; it is an mTLS 8089 client that republishes to `tak.cot.out` (S14 `cot-tap/README.md`). **In the live cluster no cot-tap Deployment exists** (`kubectl get deploy,statefulset -A | grep -i tap` returns only `agent-fabric/agent-tap`), and the last 300 lines of `takserver-plugins` logs contain no NATS line (S16). Whether TAK→bus traffic currently flows is **unverified** here; that belongs to the omos-live-state lane.
- Live TAK (S16): TAK Server 5.8 pods `api`, `config`, `messaging`, `ignite`, `plugins`; `fedhub:5.8` + MongoDB; `tak-ldap:5.8`; `tak-nats-leaf`; Mumble. Service `tak-tailnet` exposes 8089, 8443, 8446, 64738; `fedhub-tailnet` exposes 9100 and 9443; `fedhub-fed` 9101/9102 is ClusterIP only.
- Doctrine: `tak.cot.*` is the raw wire, entity state is derived (omos `docs/NATS-DOCTRINE.md` §9.8; decision 0004 #7).
- **Mesh gateway** v0.2.0 (S15): multicast SA `239.2.3.1:6969`, directed TCP 4242, optional UDP; v0 and v1; TakControl every 60 s; publishes CoT XML to `tak.cot.mesh.rx` with `Nats-Msg-Id`; transmits from `tak.cot.mesh.tx`; optional copy to `tak.cot.inject`; health on `tel.fleet.edge.{id}.status`. Its source has **no mesh encryption, no GeoChat group `224.10.10.1:17012`, no data-package transfer, no streaming client** (`grep -i "aes|hmac|crypt|17012|224.10|tls|quic"` over `mesh.go` and `main.go`: the only hit is the `crypto/sha256` import in `mesh.go` line 12, which is used for hashing, not encryption; *verifier correction: the original said "no hits"*).

### 8. Spike: takproto v1 encode/decode (this machine)

Script (`tak_spike.py`, run with `work/.tools/py/bin/python -I`): builds an SA for `sim-x1` (type `a-f-A-M-H-Q`) with `contact`, `__group`, `track`, `status`, `takv`, `precisionlocation` and `remarks`, encodes it as mesh and as stream with `takproto.xml2proto`, decodes both with `takproto.parse_proto`, then builds a TakControl-only mesh message.

Output (verbatim):

```
xml bytes: 621
mesh bytes: 281 header: bf01bf
stream bytes: 281 header: bf9602
mesh hex (first 48): bf01bf1293020a0b612d662d412d4d2d482d511209556e646566696e65642a0673696d2d7831308084a0bc9834388084
--- decoded mesh
uid sim-x1 type a-f-A-M-H-Q how m-g access 'Undefined'
sendTime 1793275200000 staleTime 1793275230000 lat 30.2672 lon -97.7431 hae 152.4
contact SIM-X1 192.168.1.20:4242:tcp | group Cyan Team Member
track 90.0 12.5 | battery 87
takv OneMind OneMind-sim macOS 0.1
precisionlocation GPS GPS
xmlDetail '<remarks>OneMind takproto spike</remarks>'
--- decoded stream
(identical to mesh)
--- TakControl mesh message: bf01bf0a0c080110011a0673696d2d7831 decoded: minProtoVersion: 1
maxProtoVersion: 1
contactUid: "sim-x1"
```

What it shows:
- Mesh header `bf 01 bf`; stream header `bf 96 02` = magic + varint 278 (0x96 0x02), which is the payload length (281 − 3). Both match S1.
- v1 is 281 bytes against 621 for XML (45%).
- Every typed element left `xmlDetail`; only `<remarks>` stayed as XML, as the whole-element rule requires.
- `sendTime` 1793275200000 is milliseconds (2026-10-29T12:00:00Z), as S2 requires.
- takproto quirks found in its source (S13 `functions.py`): it copies any non-empty `access` attribute verbatim (`functions.py` lines 101–105), so an input of `Undefined` is encoded rather than omitted as S2 requires; its `format_time` docstring says microseconds but it returns milliseconds; it does not map `caveat` or `releasableTo`. These do not matter for test messages; OneMind's own codec should follow S2.

**Cross-implementation check.** The mesh gateway's 9 Go tests pass on this machine (`go test ./...` on a `git archive` export of `origin/feat/tak-mesh-gateway`, Go 1.27.3 with caches under `work/.tools`: `TestSARoundTripThroughProtobuf … TestDirectTCP` all PASS, `ok … 0.286s`). A throwaway test that fed the Python-encoded mesh bytes into the gateway's `DecodeMeshHeader` + `UnmarshalTakMessage`, printed the XML, and re-encoded with `EncodeMesh` gave:

```
version=1
xml=<event version="2.0" uid="sim-x1" type="a-f-A-M-H-Q" access="Undefined" how="m-g" time="2026-10-29T12:00:00.000Z" ...><detail><remarks>OneMind takproto spike</remarks><contact endpoint="192.168.1.20:4242:tcp" callsign="SIM-X1"/><__group name="Cyan" role="Team Member"/>...<track speed="12.5" course="90"/></detail></event>
go re-encode bytes=281 python bytes=281 identical=true
```

The Go codec and the Python package produce **byte-identical** v1 for this message. The test file was deleted afterwards; nothing in omos was changed.

---

## How it applies to OneMind

OneMind is a TAK citizen at four levels. For each: the TAK role, what OneMind implements, where it lives, and the NATS subjects. All subjects are in the locked grammar (omos `CLAUDE.md`), and `tak.cot.*` stays the raw wire (decision 0004 #7).

### Level 1: TAK Server client

**Role.** A TAK Server client is an mTLS streaming connection on 8089 plus Marti REST on 8443, with a certificate from enrollment on 8446.

**OneMind's current state.** NatsBridgePlugin covers bus → TAK (`tak.cot.inject`). TAK → bus depends on cot-tap, which is not deployed live (S16).

**Design.**
1. **One `tak-client` service in omos** (Go, beside the mesh gateway, replaces cot-tap). It:
   - enrolls through `/Marti/api/tls/config` + `/tls/signClient/v2` on 8446, using a TAK user from a SealedSecret, and keeps the key in a Secret (never in Git);
   - streams on 8089 and negotiates v1 per S1 steps 3–7, falling back to XML;
   - pings every 15 s, as Commo does (S5);
   - publishes every received event as CoT XML to `tak.cot.out` (the doctrine subject) with `Nats-Msg-Id` = hash(uid, time), the same dedup scheme as the gateway;
   - sets its own groups with `PUT /Marti/api/groups/active`, so the bus sees the channels OneMind subscribes to.
2. **Mission sync worker** (same service). It:
   - subscribes to chosen missions (`PUT /missions/{name}/subscription`);
   - on each `t-x-m-c*` CoT, fetches `GET /missions/{name}/changes`;
   - keeps a mission snapshot in a KV bucket;
   - emits `evt.mission.{name}.changed` (grammar `evt.{domain}.{entity}.{event}`; this matches the example `evt.mission.patrol1.completed`).
   - OneMind-originated items go back with `PUT /missions/{name}/contents` and CoT with `<marti><dest mission=…/>`. **Unverified:** the exact dest attribute for missions; check it against `sendCoTToServerMissionDest` in Commo before building.
3. **Video.** Register every MediaMTX stream that OneMind exposes with `POST /Marti/api/video` (FeedV2: alias, url, lat/lon, fov, heading, range), and add `__video url=` to the sensor's CoT. ATAK and CloudTAK users then see the same streams. The KLV footprint is the video lane's job.
4. **Data packages.** Build zips with `MANIFEST/manifest.xml` (version 2, S8) and upload them through `sync/missionupload` (S4), for example for a Hermes-generated plan, overlays or 3D tiles references.
5. **The OneMind client (Unreal / web) never talks TAK directly.** It reads `ent.*.state`, KV snapshots and `tak.cot.>` from NATS. This keeps one TAK identity per deployment and avoids putting certificates on viewers.

### Level 2: Mesh peer (no server, no cloud)

**Role.** A mesh peer does what Commo does: SA multicast `239.2.3.1:6969`, GeoChat `224.10.10.1:17012`, directed TCP 4242, TakControl negotiation, optional AES-256-CBC + HMAC-SHA256 mesh encryption, and peer-hosted data-package transfer.

**OneMind's current state.** The omos mesh gateway already covers SA, directed TCP and v1 negotiation, and it is wire-compatible with takproto (spike above).

**Gaps to close, in order:**
1. **GeoChat group 17012.** Without it, ATAK chat to "All Chat Rooms" never reaches Hermes. Commo's discovery maps `udp` endpoints to `:17012` for chat (S4).
2. **Mesh encryption** (`setCryptoKeys` equivalent). Many TAK teams turn it on, and an unencrypted peer then hears nothing (S3: "everything inbound is expected to be encrypted"). Keys come from the environment or keychain, never Git.
3. **Data-package receive/serve** over `http://ip:port/getfile` with the ack/nack CoT (S4).
4. **Contact table** with the TCP > UDP > stream endpoint rule (S4), exposed as `ent.contact.{uid}.state` by the entity service (A4), not by the gateway.

**Placement.** The gateway runs on an edge host next to a NATS leaf (ADR 0003), because multicast does not reach DOKS pods. The OneMind client gets mesh traffic through the bus.

**Commo in the client** (decision 0004 #3) is still worth spike S4, because Commo has Objective-C bindings (S9 listing). Even if it builds, OneMind should keep the gateway as the one mesh identity per site, so that two stacks do not compete for port 4242 on the same host.

### Level 3: Federation partner

**Role.** Federation runs server-to-server over gRPC `FederatedChannel` (S10 `fig.proto`) on v2 port 9001 or through a Federation Hub. Groups are mapped per federate, with hop limits.

**OneMind's current state.** OneMind already runs FedHub 5.8 and TAK Server 5.8 (S16).

**Design.** **OneMind federates through its TAK Server and FedHub, and does not implement `FederatedChannel` itself.**
- The protocol is large: events, ROL for missions and files, group streams, tokens.
- The `.proto` files sit in a GPL-3.0 repo.
- TAK Server already does group filtering and provenance.

OneMind's part is configuration and policy:
1. Decide which TAK groups carry OneMind-derived entities (for example a `OneMind-COP` group).
2. Map them outbound with `/federategroupsmap` and hop limits (S10 `FederationApi.java`).
3. Enroll partner CAs with `/federatecertificates`.
4. Expose `fedhub-fed` (9101/9102) over the tailnet only when the owner approves; today it is ClusterIP only (S16, omos `CLAUDE.md`).
5. Optionally have the tak-client watch `/Marti/api/federates` and `/activeconnections` and publish federation health on `tel.fleet.edge.{id}.status`, or the closest existing pattern.

### Level 4: Plugin inside ATAK

**Role.** An ATAK plugin is an Android APK implementing `gov.tak.api.plugin.IPlugin` (S7). It is GPL-3.0 in practice because it builds against ATAK-CIV (S9).

**Design.** An `OneMindATAK` plugin in onemind-xr (GPL-3.0 fits):
1. Toolbar item and pane (`IHostUIService`) showing OneMind entities, layers and Hermes chat.
2. **Transport: no new protocol.**
   - Entities arrive as ordinary CoT through the server or the mesh, which works without the plugin.
   - The plugin adds the OneMind-only views: entity provenance, the AI confidence on a track, and the task queue.
   - It reads those over HTTPS from a small OneMind API, or as `xmlDetail` elements under a OneMind namespace.
   - **Unregistered extension IDs are forbidden in public deployments (S1)**, so OneMind detail goes in XML (for example `<onemind src=… conf=…/>`) and never in `extensionDetails`.
3. Tasking from ATAK: the plugin creates a CoT tasking message (`t-…` type; exact type **unverified**). OneMind's policy service turns it into `cmd.{domain}.{entity}.{action}` only after policy, audit and human confirm (run rule 6).
4. Build: Gradle with `atak-gradle-takdev`, ATAK 5.6.0 API. This needs either the takdev jar from `atak-civ` (it is in the repo as `atak-gradle-takdev/` and `pluginsdk.zip`) or TAK repo credentials. Nothing for Android/ATAK is installed on this machine, so the build itself goes into the owner checklist or a later phase.
5. **iTAK and TAKX:** no public plugin SDK found. OneMind reaches them as Level 1/2 peers only, through ordinary CoT, missions, video and data packages, all of which they consume.

### Browser path

- **CloudTAK (AGPL-3.0)** stays a deployed service and the reference browser TAK client. OneMind must not copy its code into onemind-xr: decision 0004 #15 forbids AGPL code.
- **node-tak and node-CoT (MIT)** may be used by the OneMind web client or tools, with their notices. Note that node-tak streams XML only.

---

## Risks

1. **No public TAK Server 5.8 source or release notes.** All REST paths are from 5.7-RELEASE-14. Mitigation: before relying on an endpoint, check it against the running 5.8 server (`GET /Marti/api/version/config` and the endpoint itself) once a TAK certificate exists for this run. None does today (PREFLIGHT).
2. **TAK→bus leg may be down live.** No cot-tap is deployed, and the plugin logs show no NATS activity (S16). OneMind's server-side picture may then be one-way. Hand off to the omos-live-state lane; the `tak-client` service above would fix it.
3. **Mesh encryption and GeoChat gaps** in the gateway mean that on a real team LAN with encryption on, OneMind hears nothing, and Hermes never hears chat.
4. **Licences.**
   - ATAK-CIV, Commo and TAK Server are GPL-3.0. Linking Commo into the omos gateway (Apache-2.0) is not allowed without relicensing; the gateway rightly reimplements from the spec (S15).
   - CloudTAK is AGPL-3.0: designs only.
   - The federation `.proto` and server-side protos are GPL-3.0; reuse only inside GPL code (onemind-xr).
5. **Extension IDs.** A OneMind binary detail extension needs a TPC registration (S1). Until then, XML detail only.
6. **ATAK plugin distribution.** Whether Play Store ATAK-CIV loads only TPC-signed plugins is **unverified**. If it does, the owner needs a TPC signing path or a sideloaded developer build of ATAK.
7. **Spec drift in the reference encoder.** takproto writes `access="Undefined"` (S13) where the spec says omit. Tests should compare semantics, not only bytes, against takproto.
8. **Secret hygiene in omos docs.** `services/takserver/cot-tap/README.md` shows a CA-key password inline in an example command. It is the TAK default, but the command makes it look like the live CA's password. This is out of scope here, so it is not fixed; it is reported as a follow-up.
9. **Rate limits.** GitHub code search hit its limit during this lane. A few cross-checks (the video v1 servlet path, the plugin signing policy) were not completed.

---

## Recommendation

1. **Server client (A-lane, omos PR):** build `services/bridge/bridges/tak-client` in Go, next to the mesh gateway and sharing its v1 codec. It does mTLS enrollment, 8089 streaming with v1 negotiation and ping, publishes `tak.cot.out`, and does mission sync to KV plus `evt.mission.{name}.changed` and video feed registration. It retires cot-tap. Until a TAK certificate exists, test it against the local NATS and a scripted TAK-stream peer that uses the S1 negotiation.
2. **Mesh peer (A7, finish the open gateway PR):** add GeoChat 17012, mesh AES-256-CBC + HMAC-SHA256 (keys from the environment), and data-package getfile serve/receive, each with a test against takproto-generated traffic. Put the ATAK LAN test in `docs/OWNER-CHECKLIST.md`.
3. **Federation:** configuration only, through the existing TAK Server and FedHub; write the group-mapping plan and leave port exposure to the owner. Do not implement `FederatedChannel`.
4. **ATAK plugin (F-lane, onemind-xr, GPL-3.0):** `OneMindATAK` on the 5.6.0 plugin API. It sends and receives ordinary CoT plus OneMind XML detail, and tasking always goes through policy and human confirm. iTAK and TAKX get OneMind through standard CoT, missions, video and data packages.
5. **Codec rule:** keep one spec-conformant v1 codec (the gateway's, Apache-2.0, written from S1/S2) as the OneMind reference, with takproto as an independent cross-check in CI. The byte-identical result above is the first test vector.

---

## Unverified items

- Any difference between TAK Server 5.7 (public source) and 5.8 (running) in the REST paths, the QUIC input, or `detail.proto` field 8.
- Whether TAK→bus traffic currently reaches `tak.cot.out` live (no cot-tap Deployment; no NATS lines in recent plugin logs).
- The exact path of the legacy video servlet (`VideoConnectionManager`); only the v2 `/Marti/api/video` paths are proven.
- How ATAK carries multiple video connections inside `__video` (the XSD shows only `url`).
- The exact CoT/marti form for sending CoT to a mission (Commo `sendCoTToServerMissionDest` exists; its XML was not read).
- The CoT type to use for tasking from the ATAK plugin.
- Whether release ATAK-CIV restricts plugins to TPC-signed builds.
- Whether iTAK and TAKX have any third-party plugin SDK; none was found publicly.
- That TAKX is the Java build of takkernel (stated in omos ADR 0003; takkernel's README says only "shared … for TAK Java based applications").
- That node-tak has no v1 protobuf negotiation at runtime (read from `index.ts`: it parses `t-x-takp-v` only for the server version and never requests v1; not run).
- Which NatsBridgePlugin config (`[__ANON__]` vs `[__ANON__, Red]`) the live plugin pod uses.
- Whether Commo builds for macOS arm64 (spike S4, a separate lane).
- **Blockers met in this lane:** the GitHub code search API rate-limited (HTTP 403) partway through, so the remaining code searches were not run. There is no TAK user certificate or tailnet route to `tak.tail717ba0.ts.net` from this machine (PREFLIGHT), so nothing was tested against the live TAK Server. No permission-system denials occurred.
