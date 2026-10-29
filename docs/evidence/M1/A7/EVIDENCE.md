# A7 — TAK mesh gateway PR: evidence

- PR: https://github.com/onemindos/omos/pull/551 (draft), branch `feat/tak-mesh-gateway`, commit `58e47759` on top of `977b7eb2`. The branch was already up to date with `origin/main` (`62e3d22b`), so the merge was a no-op and nothing was force-pushed.
- Worktree: `/Users/danielcarku/Desktop/work/wt/a7-mesh`
- Build machine: macOS arm64, Go 1.27.3, nats-server v2.15.2, takproto 3.0.1 (`/Users/danielcarku/Desktop/work/.tools/py`), OpenSSL 4.0.3. No cluster access was used.

## Files here

| File | What |
|---|---|
| `go-test-race.txt` | `go vet` clean; `go test -race -count=1 -v ./...`: 22/22 PASS, 0 skipped |
| `regression-without-fix.txt` | `TestTransmitOnNamedInterface` against a copy without `IP_MULTICAST_IF`. It fails on the interface check, and also on delivery when that check is bypassed (the default route for 239.2.3.1 is `en0`) |
| `e2e-takproto-peer.txt` | End-to-end: real gateway binary, disposable nats-servers, scripted takproto peer on `lo0`: **23/23 checks pass** |
| `e2e-gateway-logs.txt` | Gateway logs from that run |
| `mesh_peer_e2e.py` | The end-to-end harness and scripted mesh peer, which can be rerun (`python -I mesh_peer_e2e.py <gateway-binary>`) |
| `run-mac-smoke.txt` | `run-mac.sh`: refuses to start without `MESH_INTERFACE`, refuses when the leaf is down, and runs on `lo0` |
| `ci-equivalent.txt` | Changed-YAML gate, yamllint, shellcheck, manifest and Helm render checks, and a manual secret scan |

## Acceptance (FEATURES A7, build machine)

| Criterion | Result |
|---|---|
| `go test ./...` passes, including a transmit-on-named-interface test | 22/22 with `-race`. `TestTransmitOnNamedInterface` passes with the fix and fails without it |
| A `takproto` peer round-trips through `tak.cot.mesh.rx` and `tak.cot.mesh.tx` (cot-projection §12 test 7) | e2e checks 1, 1b, 3, 3b: a v1 SA reaches `tak.cot.mesh.rx` with `Tak-Proto: 1`, and a projected `OM.drone.sim-x1` sent as a core publish on `tak.cot.mesh.tx` arrives as v1 (`bf01bf`, 174 bytes) and decodes with takproto |
| §12 test 8: `MESH_TO_TAK_SERVER` | Default `false`: nothing on `tak.cot.inject` (check 2). With `true`: the PLI is copied once, and `OM.*`, `INTEL.*` and `omos-gw-*` are not (checks 8, 8b) |
| GeoChat, mesh crypto, data-package getfile | Checks 4, 4b; 9–9d (two gateways on separate buses over an encrypted mesh, an OpenSSL-encrypting peer, plaintext dropped); 6–6d |
| Second gateway / scripted peer on this machine | Gateways C and D in check 9; the scripted takproto peer throughout |

Bug found by the takproto peer and fixed in the same PR: takproto attaches a TakControl with only `contactUid` set to every GeoChat message. The gateway read the unset version as v0 and switched the whole mesh to XML. `takcontrol.proto` says 0 means 1, so the gateway now reads it that way (check 7b, `TestNegotiation`).

Not covered here: `MESH_TO_TAK_SERVER=true` with a real NatsBridgePlugin (A21), and the `ent.person.tak-*` half of §12 test 7, which belongs to the entity service (A4).

## Owner checklist step

The LAN test with real ATAK. It needs the owner's Mac on the same Wi-Fi as an ATAK-CIV 5.x phone, the local leaf running (`infra/edge/mac-leaf/README.md`), and the PR branch checked out. If macOS asks whether `tak` may accept incoming connections, click **Allow**.

```bash
cd ~/path/to/omos && git fetch origin && git checkout feat/tak-mesh-gateway
cd services/bridge/bridges/tak
ifconfig en0 | awk '/inet /{print $2}'          # the Mac's LAN IP, used below as MAC_IP
route -n get 239.2.3.1 | grep interface          # what the system would pick on its own
mkdir -p ~/onemind-datapkg
MESH_INTERFACE=en0 GATEWAY_ID=tak-mesh GATEWAY_CALLSIGN=OneMind GATEWAY_ENDPOINT_HOST=MAC_IP \
  DATAPKG_DIR=~/onemind-datapkg DATAPKG_HTTP_PORT=8081 ./run-mac.sh
```

Expected: the log shows `transmit interface en0` and `ready`. In a second terminal:

| # | Command / action | Expected result |
|---|---|---|
| 1 | `nats sub tak.cot.mesh.rx` | Within about 30 s, the phone's SA (`type="a-f-G-U-C"`, its callsign) arrives with `Tak-Delivery: mesh` and `Tak-Proto: 1` (or `0` if ATAK is set to XML) |
| 2 | On the phone, open Contacts | **OneMind** is listed (the gateway's own SA, every 30 s) |
| 3 | `T=$(date -u +%Y-%m-%dT%H:%M:%SZ); S=$(date -u -v+10M +%Y-%m-%dT%H:%M:%SZ); nats pub tak.cot.mesh.tx "<event version=\"2.0\" uid=\"OM.marker.owner-1\" type=\"b-m-p-s-m\" how=\"h-g-i-g-o\" time=\"$T\" start=\"$T\" stale=\"$S\"><point lat=\"LAT\" lon=\"LON\" hae=\"0\" ce=\"9999999\" le=\"9999999\"/><detail><contact callsign=\"OneMind test\"/></detail></event>"` (use a LAT/LON near the phone) | A spot marker "OneMind test" appears on the ATAK map within a few seconds. The status in step 7 shows `tx` incremented |
| 4 | On the phone: Chat → All Chat Rooms → send `hello onemind` | `tak.cot.mesh.rx` gets a `b-t-f` event with `Tak-Delivery: chat` and `<remarks …>hello onemind</remarks>` |
| 5 | `T=$(date -u +%Y-%m-%dT%H:%M:%SZ); S=$(date -u -v+10M +%Y-%m-%dT%H:%M:%SZ); nats pub tak.cot.mesh.tx "<event version=\"2.0\" uid=\"GeoChat.omos-gw-tak-mesh.All Chat Rooms.$RANDOM\" type=\"b-t-f\" how=\"h-g-i-g-o\" time=\"$T\" start=\"$T\" stale=\"$S\"><point lat=\"0\" lon=\"0\" hae=\"9999999\" ce=\"9999999\" le=\"9999999\"/><detail><__chat parent=\"RootContactGroup\" groupOwner=\"false\" chatroom=\"All Chat Rooms\" id=\"All Chat Rooms\" senderCallsign=\"OneMind\"><chatgrp uid0=\"omos-gw-tak-mesh\" uid1=\"All Chat Rooms\" id=\"All Chat Rooms\"/></__chat><link uid=\"omos-gw-tak-mesh\" type=\"a-f-G-E-S\" relation=\"p-p\"/><remarks source=\"BAO.F.ATAK.omos-gw-tak-mesh\" to=\"All Chat Rooms\" time=\"$T\">hello from OneMind</remarks></detail></event>"` | "hello from OneMind" appears in ATAK under All Chat Rooms, from OneMind |
| 6 | On the phone: Data Package → new package with any small file → Send → pick contact **OneMind** | A file `<sha256>-<name>.zip` appears in `~/onemind-datapkg/inbox`, and ATAK reports the transfer delivered (the gateway's ack). `nats sub tak.cot.mesh.rx` shows the `b-f-t-r` with `Tak-Delivery: direct` |
| 7 | `nats sub 'tel.fleet.edge.tak-mesh.status'` | Every 30 s: `tx_interface: "en0"`, `proto_version: 1`, `contacts` ≥ 1, `datapkg_rx` ≥ 1 after step 6, `to_tak_server: false` |
| 8 | `nats sub tak.cot.inject` during steps 1–6 | Nothing (`MESH_TO_TAK_SERVER` defaults to `false`) |
| 9 | Optional: stop the gateway, run it again with `MESH_INTERFACE` set to the Tailscale `utunN` interface, and repeat step 3 | The marker does **not** appear on the phone. This is the old default-route failure, shown on purpose. Restore `en0` afterwards |

Record any differences: the ATAK version, whether step 1 shows `Tak-Proto` 0 or 1, and whether step 6 shows "delivered". Mesh encryption is not on this list, because how ATAK 5.6 imports the mesh key pair was not verified here (owner proposal P5). If the owner enables it on the phone, add `MESH_AUTH_KEY`/`MESH_CIPHER_KEY` from the Keychain to the environment and repeat steps 1 and 3.

## Review fixes (bef11eb1, v0.4.0)

- `review-fix-go-test-race.txt`: `go test -race -v` after the fixes, 27 tests pass, including the new `hardening_test.go`.
- `review-fix-regression-without-check.txt`: with the xmlDetail check disabled, `TestXMLDetailMustBeBalancedFragment` fails and prints the smuggled second `<event>`.
- `review-fix-e2e.txt`: `mesh_peer_e2e.py` passes 26 of 26 checks. New checks: 3e (the gateway refuses `a-h-*` on tx), 7c (acked rx is stored in stream `COT`), 8c (a smuggled `</detail></event>` reaches neither inject nor rx).
- `review-fix-e2e-gateway-logs.txt`: gateway logs from that run. They show the message dropped as `xmlDetail: XML syntax error ... unexpected end element </detail>` and `/getfile` bound to 127.0.0.1.
