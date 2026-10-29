# M2 lane B: owner steps on the Mac (S1, F1, S5, 10,000 entities, C1, C2)

These steps need the owner's Apple Silicon Mac, Unreal Engine 5.8 and, for C1/C2, an ATAK device
on the same Wi-Fi. None of them could run on the build machine (no Unreal, no LAN with ATAK,
[PREFLIGHT.md](../../../PREFLIGHT.md)). Every feature below is **built as source, awaiting owner check**.
Run the sections in order. Write each result (pass or fail, numbers, screenshots) into the file
named in the step.

Set these once in every Terminal window:

```bash
export WORK="$HOME/work"                          # folder holding onemind-xr/ and omos/
export OM="$WORK/onemind-xr"
export UE="/Users/Shared/Epic Games/UE_5.8"       # Epic Launcher default; change if installed elsewhere
export UPROJECT="$OM/client/OneMind/OneMind.uproject"
export APPDIR="$HOME/onemind-build/mac"           # where the packaged apps go: $APPDIR/Development, $APPDIR/Shipping
mkdir -p "$OM/docs/evidence/M2/B/owner"
```

**Build configuration.** Every check in sections 1–5 uses the **Development** package (the
default of `build-and-run.sh package`): it has the console (backtick, `OneMindMarker`,
`OneMindChat`), writes `~/Library/Logs/OneMind/OneMind.log` and supports `-trace`. A Shipping
package strips all three, so it is used only for the F1 install check (section 1 step 5,
`build-and-run.sh package-shipping`). Frame rate does not need the console: the first line of the
overlay (top-left) reads `fps … | frame … ms avg | worst … ms (last 5 s)`.

Branch: the client is on `run/m2-client` (draft PR in `onemindos/onemind-xr`) until it merges:
`cd "$OM" && git fetch origin && git checkout run/m2-client`.

## 0. Tools for the bus checks (once)

```bash
cd "$WORK" && mkdir -p tools && cd tools
curl -fLO https://github.com/nats-io/nats-server/releases/download/v2.15.2/nats-server-v2.15.2-darwin-arm64.zip
curl -fLO https://github.com/nats-io/natscli/releases/download/v0.5.1/nats-0.5.1-darwin-arm64.zip
unzip -o -j nats-server-v2.15.2-darwin-arm64.zip '*/nats-server' && unzip -o -j nats-0.5.1-darwin-arm64.zip '*/nats'
python3 -m venv "$WORK/tools/py" && "$WORK/tools/py/bin/pip" install nats-py takproto
export PATH="$WORK/tools:$PATH"
nats-server --version && nats --version
```

Expected: `nats-server: v2.15.2` and `0.5.1`. (Versions as installed on the build machine,
PREFLIGHT.md. If the zip layout differs, `unzip -l` the archive and extract the binary by its path.)

Local bus with a WebSocket listener (the client connects to `ws://127.0.0.1:9222`). Until lane A's
Mac leaf config adds a WebSocket block, use this standalone server:

```bash
cat > "$WORK/onemind-local-nats.conf" <<EOF
listen: 127.0.0.1:4222
http: 127.0.0.1:8222
jetstream { store_dir: "$HOME/onemind-nats", max_file_store: 4GB }
websocket { listen: "127.0.0.1:9222", no_tls: true }
EOF
nats-server -c "$WORK/onemind-local-nats.conf" &
nats server check jetstream
```

Expected: `OK JetStream`. (No TLS: localhost only. For the headset, LAN binding is a lane E step.)

## 1. S1: build, package and run (B1, B2, F1)

1. Host checks and install (unreal-platform.md U0, U1): Xcode must be **26.0 to 26.3** (26.4 is not
   compatible with UE 5.8, Epic macOS requirements page). Install UE 5.8 from the Epic Games
   Launcher and **Cesium for Unreal 2.30.0 or newer** from Fab ("Install to Engine 5.8").

   ```bash
   "$OM/client/OneMind/Tools/mac/build-and-run.sh" check
   ```
   Expected: macOS ≥ 14.5 arm64, Xcode 26.0–26.3, Cesium `"VersionName": "2.30.0"` (or newer), and no `STOP:` line.

2. Build the editor target (Development):
   ```bash
   "$OM/client/OneMind/Tools/mac/build-and-run.sh" build 2>&1 | tee "$OM/docs/evidence/M2/B/owner/S1-build.log"
   ```
   Expected: ends with `Result: Succeeded` (or a total execution time with no `error:` line).
   This is the **first compile of the OneMind plugins** (nothing was compiled in Unreal before).
   If it fails, keep the log; the failing file and line are what lane B needs.

   2b. If the log says `/Engine/Maps/Entry` cannot be found, open the project in the editor
   (`open "$UPROJECT"`), create an empty level (File > New Level > Empty Level), save it as
   `/Game/Maps/OneMind`, then set `GameDefaultMap`, `EditorStartupMap` and the `MapsToCook` entry to
   `/Game/Maps/OneMind` in `Config/DefaultEngine.ini` and `Config/DefaultGame.ini`, and rebuild.

3. Package (**Development**) and run:
   ```bash
   "$OM/client/OneMind/Tools/mac/build-and-run.sh" package 2>&1 | tee "$OM/docs/evidence/M2/B/owner/S1-package.log"
   "$OM/client/OneMind/Tools/mac/build-and-run.sh" run
   export APP=$(ls -d "$APPDIR"/Development/Mac*/OneMind.app | head -1)
   ```
   Expected: `BUILD SUCCESSFUL` and `AutomationTool exiting with ExitCode=0 (Success)`;
   `$APPDIR/Development/Mac/OneMind.app` (or `MacNoEditor`) exists; the app opens on a globe over
   Austin, TX at 8 km with OpenStreetMap imagery, a sun and sky, the overlay top-left
   (`fps ...`, `bus: ...`, layers `[1] fleet`, `[2] tak` ...) and
   `Map data (c) OpenStreetMap contributors` bottom-right.
   Read the overlay's first line after 10 s idle. **Pass B1: `fps` ≥ 60 and `worst` ≤ 16.7 ms on
   the idle globe.** For the detail, press backtick and type `stat unit` (Development only).
   Screenshot: `owner/S1-globe.png`.

4. Offline cache (B2): fly around the start area for a minute, quit, turn Wi-Fi off, run the app again
   and look at the same area. **Pass B2: the imagery still shows from Cesium's request cache**
   (`MaxCacheItems=200000` in `DefaultEngine.ini`). Screenshot `owner/S1-offline.png`. Wi-Fi on again.

5. F1 (bundle id and local signing, decision 0004 #16). This is the only step that uses a
   **Shipping** package:
   ```bash
   "$OM/client/OneMind/Tools/mac/build-and-run.sh" package-shipping 2>&1 | tee "$OM/docs/evidence/M2/B/owner/F1-package-shipping.log"
   SHIP=$(ls -d "$APPDIR"/Shipping/Mac*/OneMind.app | head -1)
   defaults read "$SHIP/Contents/Info" CFBundleIdentifier
   codesign -dv --verbose=2 "$SHIP" 2>&1 | grep -E 'Identifier|Signature|TeamIdentifier'
   ```
   Expected: `org.onemindos.onemind`. If it shows another id, open Project Settings > Platforms >
   Xcode Projects, set the bundle identifier to `org.onemindos.onemind`, note the ini key the editor
   writes (the key `BundleIdentifier` under `[/Script/MacTargetPlatform.XcodeProjectSettings]` is
   **unverified** for 5.8) and repackage. If `codesign` says "not signed", sign for local use with
   `codesign --force --deep --sign - "$SHIP"` and repeat. **Pass F1: the Shipping app installs (copy
   to /Applications) and runs** (it shows the globe and overlay; it writes no log and has no console,
   by design).

Record `owner/S1-result.md`: B1 fps, B2 offline yes/no, F1 bundle id and signature line.

## 2. S5: NATS in the app (B3)

The app connects to `ws://127.0.0.1:9222` (Project Settings > OneMind Bus; command line
`-OneMindBusUrl=ws://host:port`). Start the local bus from section 0 first, then the
**Development** app (`build-and-run.sh run`; Shipping writes no log).

1. ```bash
   tail -f ~/Library/Logs/OneMind/OneMind.log | grep -E 'OneMind bus|S5 probe' &
   ```
   Expected: `OneMind bus connected (server 2.15.2, reconnects 0)`. (Log path is UE's default for a
   packaged Mac app; **unverified** for 5.8. If empty, search `~/Library/Logs` for `OneMind.log`.)
2. Marker from KV within 1 s:
   ```bash
   nats kv add world-state --history=1 --ttl=5m 2>/dev/null
   nats kv put world-state probe.sim-x1 '{"lat":30.2672,"lon":-97.7431,"hae":300,"callsign":"SIM-X1"}'
   ```
   Expected: within 1 s the log shows `S5 probe: probe.sim-x1 at 30.267200, -97.743100` and a marker
   appears at the centre of the view. (`world-state` holds layer manifests `{layer}.meta`; the client
   reads only `probe.*` keys there for this check. See proposal B-P2.)
3. Reconnect: stop and restart the server, then move the marker:
   ```bash
   pkill nats-server; sleep 3; nats-server -c "$WORK/onemind-local-nats.conf" & sleep 2
   nats kv put world-state probe.sim-x1 '{"lat":30.2800,"lon":-97.7431,"hae":300,"callsign":"SIM-X1"}'
   ```
   Expected: the log shows `OneMind bus disconnected`, then `OneMind bus reconnected (1 reconnects so far)`,
   then a new `S5 probe` line; the marker moves north. **Pass B3: steps 2 and 3.**
   On the build machine the same client core reconnected 1.57 s after a server restart and delivered a
   KV put in 3 ms (`integration-tests.txt`, `it_reconnect_after_server_restart`).

Record `owner/S5-result.md`.

## 3. 10,000 entities at 60 fps (B4, B5)

1. With the local bus and the Development app running (the `fleet` layer is on by default, reads
   `ENT` directly and loads the whole layer whatever the camera shows; only watch-path and large
   layers follow the camera AOI):
   ```bash
   "$WORK/tools/py/bin/python" "$OM/client/OneMind/Tools/entity_load.py" --url nats://127.0.0.1:4222 --count 10000 --rate 1 --duration 300
   ```
   Expected: `round N: 10000 entities published in ~0.5 s` lines; the overlay shows
   `[1] fleet on live 10000` and the renderer line `entities 10000 | icons ... | models ... | actors ...`.
   (The headless core does the same with the default-start camera box:
   `load-10k/run-camera-aoi.txt`, 10,000.)
2. Read the overlay's first line. **Pass B4: `fps` ≥ 60 with 10,000 entities moving**, and `worst`
   ≤ 16.7 ms in the 5 s windows after step 1 starts (no frame drop on layer load). `stat unit`
   (backtick) gives the game/draw/GPU split. Capture 30 s with the Development app: quit, then run
   `"$APP/Contents/MacOS/OneMind" -trace=cpu,gpu,frame -tracefile="$OM/docs/evidence/M2/B/owner/B4-10k.utrace"`
   while `entity_load.py` runs, and open the trace in Unreal Insights: frames ≤ 16.7 ms.
3. B5 look: fly down toward one entity (mouse and WASD of the Cesium pawn). Expected: far away they
   are small spheres (icons) or yellow cluster spheres where dense; within 60 km they become cones
   pointing along their course; within 2 km the nearest 64 become actors with a label. No visible
   hitch when step 1 starts (layer load). Screenshots `owner/B5-far.png`, `owner/B5-near.png`.
4. Layer off means no subscription: press `1` (fleet off), then
   ```bash
   curl -s 'http://127.0.0.1:8222/connz?subs=1' | python3 -c 'import sys,json;[print(c["name"],c["subscriptions"]) for c in json.load(sys.stdin)["connections"] if c.get("name")=="onemind-unreal-mac"]'
   curl -s 'http://127.0.0.1:8222/jsz?consumers=true' | python3 -c 'import sys,json;print("consumers",json.load(sys.stdin)["consumers"])'
   ```
   Expected: with fleet on, `onemind-unreal-mac` has ≥ 4 subscriptions (two KV watches plus one ordered
   consumer per entity type) and `consumers` ≥ 4; after `1`, the fleet's consumers are gone (consumer
   count drops by 4) and the overlay shows `[1] fleet off`. Press `1` again: entities reload.
   The same check passed on the build machine at the server (`it_layer_off_means_no_subscription_on_the_server`).
5. Remove the load: `"$WORK/tools/py/bin/python" "$OM/client/OneMind/Tools/entity_load.py" --count 10000 --remove`.
   Expected: the fleet layer empties (live:false removal).

Record `owner/B4-result.md` with fps, frame time, the renderer line and the subscription counts.

## 4. C1: ATAK on the LAN appears in the client

Needs: omos branches `feat/entity-service` (PR #552) and `feat/tak-mesh-gateway` (PR #551), Go, an
ATAK device on the same Wi-Fi with SA multicast on (ATAK default `239.2.3.1:6969`).

```bash
cd "$WORK/omos" && git fetch origin
nats stream add COT --subjects 'tak.cot.>' --storage file --replicas 1 --retention limits --discard old --max-age 72h --dupe-window 2m --defaults
nats stream add ENT --config "$OM/client/OneMind/HostTests/fixtures/ent-stream.json"
git worktree add "$WORK/omos-entity" origin/feat/entity-service
(cd "$WORK/omos-entity/services/entity" && go build -o "$WORK/tools/entity-service" ./cmd/entity-service)
NATS_URL=nats://127.0.0.1:4222 LEASE_BUCKET=none INPUTS='COT=tak.cot.>' HTTP_ADDR=127.0.0.1:18080 "$WORK/tools/entity-service" &
git worktree add "$WORK/omos-mesh" origin/feat/tak-mesh-gateway
cd "$WORK/omos-mesh/services/bridge/bridges/tak" && MESH_INTERFACE=en0 ./run-mac.sh &
```

Expected: `curl -s 127.0.0.1:18080/readyz` answers ready; the gateway logs
`ready: tak.cot.mesh.tx -> mesh, mesh -> tak.cot.mesh.rx`. (`en0` is the Wi-Fi interface on most
Macs: check with `route -n get 239.2.3.1`.) Then open ATAK and wait for its next position report.

**Pass C1:** the `[2] tak` layer counts the device and a cyan marker appears at its position with its
callsign, wherever the camera is (the `tak` layer is small and loads whole; fly to the device if it
is off screen). Click it: the inspector shows `entity_type person`, `source 1 tak-mesh (cot)`,
`subject tak.cot.mesh.rx`, the confidence parts, `tlp AMBER+STRICT`, and a `RAW COT` block with the
device's event. Screenshot `owner/C1-atak-in-onemind.png`. On the build machine the same chain with a
`takproto` peer in place of ATAK passed (`e2e-c1-c2/run.txt`, checks C1a–C1e).

## 5. C2: a marker from the client appears on ATAK

The projector's `svc.cot.send` is not written in omos yet (blocker), so run the stand-in, which
applies the same gates and core-publishes to `tak.cot.mesh.tx`:

```bash
"$WORK/tools/py/bin/python" "$OM/client/OneMind/HostTests/scripts/svc_cot_send_standin.py" --url nats://127.0.0.1:4222 &
```

In the **Development** app, point at the map near the ATAK device and press `M` (or open the
console with backtick and type `OneMindMarker 30.27 -97.74 OM-TEST` with coordinates near you). Expected: the stand-in prints
`svc.cot.send marker uid=onemind-mac-... -> ['mesh']`, the app log shows `svc.cot.send ... ok`.
**Pass C2:** within a few seconds ATAK shows a marker `OM-MARK` (unknown ground, type `a-u-G`) with
remark `marker from the OneMind client · SIMULATED`. Screenshot from the phone `owner/C2-marker-on-atak.png`.
Also try `OneMindChat hello from OneMind`: ATAK's All Chat Rooms should show it (GeoChat layout is
**unverified**, cot-projection.md §13; record what ATAK shows).

Stop everything: `pkill -f svc_cot_send_standin; pkill -f entity-service; pkill -x tak; pkill nats-server`.

## Proposed text for docs/OWNER-CHECKLIST.md (for the orchestrator)

> **M2 client (lane B).** Branch `run/m2-client`. Follow `docs/evidence/M2/B/OWNER-STEPS.md`:
> section 0 (tools, local bus), 1 (S1: build, Development package, run; B1 ≥ 60 fps idle read from
> the overlay's fps line; B2 offline cache; F1 Shipping package, bundle id `org.onemindos.onemind`), 2 (S5: KV probe marker within 1 s, reconnect after server
> restart), 3 (10,000 entities at ≥ 60 fps, icons far / models near, layer off drops its
> subscriptions), 4 (C1: ATAK PLI appears with inspector), 5 (C2: marker from the client appears on
> ATAK via the `svc.cot.send` stand-in). Results go in `docs/evidence/M2/B/owner/`. The first
> Unreal compile of the OneMind plugins happens in section 1 step 2: keep the log if it fails.

Status rows (FEATURES section "Owner checks"): B1, B2, F1 → section 1; B3 → section 2; B4, B5 →
section 3; B6 → sections 3–4 (inspector); C1 → section 4; C2 → section 5.
