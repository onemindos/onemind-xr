# Owner Checklist

These are the steps only the owner can run, on the Apple Silicon Mac, the Meta Quest Pro and the home
Wi-Fi with ATAK. The build run writes them and keeps them in order (run prompt "Where this runs";
decision 0004 #17). Run them from top to bottom. Each step gives a command to copy and paste,
followed by **Expected:** with the result you should see.

Updated 2026-10-29 (M0 synthesis). **State: nothing is built yet** ([STATUS.md](STATUS.md)), so no
feature is "built, awaiting owner check". Some sections can run today. Others say what they wait
for. Commands marked *planned* name a script that a later lane will commit. If that script is not
in the repo yet, skip the step.

Rules for every step:

- Simulated assets only. Every drone or robot id starts with `sim-` (decision 0004 #13). Never
  point anything here at a real aircraft or vehicle.
- No secrets in Git. Passwords go in the macOS keychain or are typed at a prompt (run Rule 4).
  Never commit a certificate, key or password file.
- Save the proof to the path each step names, under `onemind-xr/docs/evidence/<milestone>/`
  (run Rule 7). Then commit it on a branch and push, or send it back to the run.
- If a step fails, save the log it names and write what happened in the result file. Then go on
  to the next section unless the section says to stop.

## Overview

| # | Section | Closes | Ready now? |
|---|---|---|---|
| 0 | What you need | — | yes |
| 1 | Host checks | — | yes |
| 2 | Repos, tools and the local bus | everything below | yes |
| 3 | S1: Unreal 5.8 and Cesium, package and run | B1, B2, F1, J3 (Mac) | install steps yes; build waits for lane B's project |
| 4 | S5: NATS in the app | B3 | waits for lane B's `OneMindBus` |
| 5 | 10,000 entities at 60 fps | B4, B5, G2 | waits for lane B |
| 6 | S2: Pixel Streaming 2 from the Mac | F2 | signalling server yes; stream waits for section 3 |
| 7 | S3: Quest Pro | F4, J3 (headset) | toolchain and developer mode yes; build waits for section 3 |
| 8 | LAN mesh test with ATAK | A7, C1, C2 | **yes**, with the gateway from the omos branch (one known defect, see 8.4) |
| 9 | ATAK display checks | A6, H9, K3, D2 | waits for lane A rows 8–12 and D2 |
| 10 | Real SITL (PX4) | H1, H2, A8 | SITL yes; bridge part waits for H2 |
| 11 | Video on the LAN | K9, K4 | MediaMTX part yes; client panel waits for K4 |
| 12 | Offline map pack and ATAK import | B2, B15 | yes |
| 13 | TAK Server checks | C3, A21 | version check yes (needs your TAK certificate); the rest waits for A21 and C3 |
| 14 | Site leaf and disconnected mode | A23, F9 | waits for A23 and F9 |
| 15 | Later checks | E4, A20, C9, H5 | later |
| 16 | Owner decisions (no commands) | — | any time |

---

## 0. What you need

| Item | Why |
|---|---|
| The Apple Silicon Mac, macOS 14.5 or newer, at least 200 GiB free | Unreal about 150 GB, plus the Android SDK and build output ([unreal-platform.md](research/unreal-platform.md), U0) |
| An Apple ID (for Xcode) and an Epic Games account | Unreal and Cesium install through the Epic Launcher and Fab |
| The Meta Quest Pro, a USB-C data cable, the Meta Horizon phone app and a Meta developer organization | Developer mode needs a developer organization (decision 0003) |
| An Android phone with ATAK-CIV on the same Wi-Fi as the Mac | LAN mesh test ([tak-protocols.md](research/tak-protocols.md) Rec. 2) |
| A second device with a browser on the same Wi-Fi (phone, tablet or laptop) | Pixel Streaming and video tests |
| Your TAK user certificate (`.p12`) and Tailscale on the owner tailnet | Only for section 13 |
| Homebrew (optional) | Only if you prefer `brew install ffmpeg cmake` to the official downloads |

## 1. Host checks

Run these before you install anything ([unreal-platform.md](research/unreal-platform.md), U0).

1. `sw_vers -productVersion`
   **Expected:** `14.5` or higher.
2. `uname -m`
   **Expected:** `arm64`.
3. `xcodebuild -version`
   **Expected:** `Xcode 26.0` to `26.3` (26.1.1 recommended). If it shows **26.4 or later**, download Xcode 26.1.1 from https://developer.apple.com/download/all/ and put it at `/Applications/Xcode-26.1.1.app`. Then run `sudo xcode-select -s /Applications/Xcode-26.1.1.app` and repeat this step. Unreal 5.8 does not support Xcode 26.4, according to Epic's macOS requirements page.
4. `df -h "$HOME" | tail -1`
   **Expected:** at least 200 GiB available.
5. `sysctl -n hw.memsize | awk '{print $1/1073741824 " GiB"}'`
   **Expected:** 16 or more (32 recommended).

## 2. Repos, tools and the local bus

Everything goes under `~/work`. Nothing is installed system-wide. To remove it all, delete
`~/work/.tools` and `~/work/.run`.

Set these in **every** new Terminal window you use for this checklist:

```bash
export WORK="$HOME/work"
export OM="$WORK/onemind-xr"
export PATH="$WORK/.tools/go/bin:$WORK/.tools/bin:$PATH"
```

### 2.1 Get the repos

```bash
mkdir -p "$WORK/.tools/bin" "$WORK/.tools/dl" "$WORK/.run" && cd "$WORK"
git clone git@github.com:onemindos/onemind-xr.git
git clone git@github.com:onemindos/omos.git
mkdir -p "$OM/docs/evidence/"{M1,M2,M3,M5}
```

**Expected:** two folders, `~/work/onemind-xr` and `~/work/omos`. If you already have them, run
`git -C "$OM" pull` and `git -C "$WORK/omos" fetch origin` instead.

### 2.2 Go (for the mesh gateway and the NATS tools)

```bash
curl -fsSL -o "$WORK/.tools/dl/go.tgz" https://go.dev/dl/go1.27.3.darwin-arm64.tar.gz
tar -C "$WORK/.tools" -xzf "$WORK/.tools/dl/go.tgz"
go version
```

**Expected:** `go version go1.27.3 darwin/arm64` (the version the run used, [PREFLIGHT.md](PREFLIGHT.md)).

### 2.3 NATS server and CLI

```bash
GOBIN="$WORK/.tools/bin" go install github.com/nats-io/nats-server/v2@v2.15.2
GOBIN="$WORK/.tools/bin" go install github.com/nats-io/natscli/nats@v0.5.1
nats-server --version
nats --version
```

**Expected:** `nats-server: v2.15.2`, and a `nats` version line containing `0.5.1`. These are the
versions the run used ([PREFLIGHT.md](PREFLIGHT.md)). The exact `nats --version` wording is
*unverified*.

### 2.4 Python tools (ATAK test messages and MAVLink)

```bash
python3 --version
python3 -m venv "$WORK/.tools/py"
"$WORK/.tools/py/bin/pip" install takproto pymavlink nats-py
```

**Expected:** Python 3.10 or newer, then `Successfully installed …` listing the three packages. The
run used Python 3.12 ([PREFLIGHT.md](PREFLIGHT.md)). The minimum version these packages need is
*unverified*. If `python3` is older than 3.10, install Python 3.12 from
https://www.python.org/downloads/macos/ and repeat.

### 2.5 Start the local bus

This is the same local stack the run uses: JetStream, WebSocket on 9222 and leaf on 7422, all bound
to `127.0.0.1` ([PREFLIGHT.md](PREFLIGHT.md), Local stack). Later, lane A replaces this step with
one script (*planned*: `scripts/mac/run-local-bus.sh`).

```bash
mkdir -p "$WORK/.run/js"
cat > "$WORK/.run/nats-local.conf" <<EOF
listen: 127.0.0.1:4222
http: 127.0.0.1:8222
jetstream {
  store_dir: "$WORK/.run/js"
  max_file_store: 4GB
  max_memory_store: 512MB
}
websocket {
  listen: "127.0.0.1:9222"
  no_tls: true
}
leafnodes {
  listen: "127.0.0.1:7422"
}
EOF
nats-server -c "$WORK/.run/nats-local.conf" > "$WORK/.run/nats.log" 2>&1 &
sleep 2
nats server check jetstream
```

**Expected:** `OK JetStream`. Leave the server running for the rest of the checklist. To stop it
later, run `pkill -f nats-local.conf`.

---

## 3. S1: Unreal 5.8 and Cesium, package and run (B1, B2, F1)

Source: [unreal-platform.md](research/unreal-platform.md) "Owner checklist steps" U1–U2; decision
0004 #14. The project file comes from lane B. `OneMind.uproject` is its planned name.

```bash
export UE="/Users/Shared/Epic Games/UE_5.8"    # Epic Launcher default; change it if you installed elsewhere
export UPROJECT="$OM/OneMind.uproject"         # planned; use the path lane B commits
```

### 3.1 Install Unreal 5.8 and Cesium for Unreal (ready now)

1. Install the Epic Games Launcher from https://www.unrealengine.com/download and sign in. Go to **Unreal Engine > Library > Engine Versions > +**, choose **5.8.x** (the latest) and click **Install**. If you will do section 7, tick **Android** under **Options**.
   **Expected:** `ls "$UE/Engine/Binaries/Mac/UnrealEditor.app"` prints the bundle.
2. In the Launcher, open **Fab**, search for **Cesium for Unreal**, then click **Install to Engine 5.8**.
   **Expected:** `ls "$UE/Engine/Plugins/Marketplace" | grep -i cesium` prints a Cesium folder, and `grep -h '"VersionName"' "$UE"/Engine/Plugins/Marketplace/*/CesiumForUnreal.uplugin` shows `2.30.0` or newer.
3. `open "$UE/Engine/Binaries/Mac/UnrealEditor.app"`
   **Expected:** the Project Browser opens without crashing. Close it.

### 3.2 Build, package and run OneMind (waits for lane B's project)

1. `cd "$OM" && git pull`
   **Expected:** up to date with `main`, and `ls "$UPROJECT"` prints the file.
2. Build the editor target:
   `"$UE/Engine/Build/BatchFiles/Mac/Build.sh" OneMindEditor Mac Development -Project="$UPROJECT" -WaitMutex`
   **Expected:** ends with `Result: Succeeded`, and there are no `error:` lines.
3. Package a Shipping build:
   `"$UE/Engine/Build/BatchFiles/RunUAT.sh" BuildCookRun -project="$UPROJECT" -platform=Mac -clientconfig=Shipping -build -cook -stage -pak -archive -archivedirectory="$WORK/build/mac" -utf8output`
   **Expected:** the last lines include `BUILD SUCCESSFUL` and `AutomationTool exiting with ExitCode=0 (Success)`. Then `ls "$WORK/build/mac"/Mac*/` lists `OneMind.app`.
4. `open "$WORK/build/mac"/Mac*/OneMind.app`
   **Expected:** the globe renders with open terrain and imagery (decision 0004 #4). Press the backtick key, then type `stat fps`, then `stat unit`. You should see **≥ 60 fps on the idle globe** (B1). Take a screenshot (Cmd-Shift-4) and save it as `$OM/docs/evidence/M2/S1-globe-mac.png`.
5. Offline cache (B2): quit the app, turn Wi-Fi off, reopen the app and fly to the same view.
   **Expected:** the globe still renders from the cache. Save a screenshot as `$OM/docs/evidence/M2/S1-offline-cache.png`. Turn Wi-Fi back on.
6. If step 2 or 3 fails, save the logs from `~/Library/Logs/Unreal Engine/LocalBuildLogs/` and the UAT log path printed on failure to `$OM/docs/evidence/M2/S1-failure.log`. Do not use the DebugGame configuration (Cesium issue #1850).

**Pass:** steps 4 and 5. Write `pass` or `fail` and the fps figure in `$OM/docs/evidence/M2/S1-result.md`.
If S1 fails, the run pins the newest working Unreal and Cesium pair (decision 0004 #14).

## 4. S5: NATS in the app (B3)

Source: [unreal-platform.md](research/unreal-platform.md) U3. Waits for lane B's `OneMindBus` and
the packaged app from section 3. The local bus from section 2.5 must be running.

1. Build nats.c for the plugin (*planned* script `scripts/unreal/build-natsc-mac.sh`). Until the script exists, run:
   ```bash
   cmake --version || echo "install CMake from https://cmake.org/download/ or: brew install cmake"
   cd "$WORK/.tools/dl" && curl -fsSL -o natsc.tgz https://github.com/nats-io/nats.c/archive/refs/tags/v3.14.0.tar.gz && tar xzf natsc.tgz
   cmake -S nats.c-3.14.0 -B build-natsc -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 -DNATS_BUILD_STREAMING=OFF -DNATS_BUILD_EXAMPLES=OFF -DBUILD_TESTING=OFF -DNATS_BUILD_LIB_SHARED=OFF -DNATS_BUILD_WITH_TLS=OFF && cmake --build build-natsc -j
   lipo -info build-natsc/src/libnats_static.a
   ```
   **Expected:** `lipo` reports `arm64`.
2. Start the packaged app (section 3.2 step 4), then publish a test entity:
   ```bash
   nats kv add world-state --history=1 --ttl=5m 2>/dev/null
   nats kv put world-state ent.drone.sim-x1.state '{"lat":40.7128,"lon":-74.0060,"alt":120}'
   ```
   **Expected:** a `sim-x1` marker appears over New York within one second. This is a plugin smoke test only. In the target design, `world-state` holds layer manifests, not entities ([BUS-DESIGN.md](design/BUS-DESIGN.md) D3).
3. Reconnect test:
   ```bash
   pkill -f nats-local.conf; sleep 3
   nats-server -c "$WORK/.run/nats-local.conf" > "$WORK/.run/nats.log" 2>&1 &
   sleep 2; nats kv put world-state ent.drone.sim-x1.state '{"lat":40.7200,"lon":-74.0060,"alt":120}'
   ```
   **Expected:** the marker moves north, and the app log shows a reconnect. The log path `~/Library/Logs/OneMind/OneMind.log` is *unverified*.

**Pass:** steps 2 and 3. Record the result in `$OM/docs/evidence/M2/S5-result.md`. If nats.c does not
work in the plugin, the fallback is WebSocket NATS from C++ (ROADMAP S5).

## 5. 10,000 entities at 60 fps (B4, B5, G2)

Source: [unreal-platform.md](research/unreal-platform.md) U6. Waits for lane B.

1. Start the synthetic load (*planned*: `"$WORK/.tools/py/bin/python" "$OM/scripts/sim/entity_load.py" --count 10000 --rate 1`).
   **Expected:** it reports 10,000 `sim-*` entities publishing.
2. Open the packaged app, turn on the synthetic layer, and run `stat fps` and `stat unit` in the console.
   **Expected:** **≥ 60 fps** with all 10,000 in view at global zoom, and no hitch when you toggle the layer.
3. Capture 30 s with Unreal Insights. Start the app from Terminal:
   `"$WORK/build/mac"/Mac*/OneMind.app/Contents/MacOS/OneMind -trace=cpu,gpu,frame -tracefile="$OM/docs/evidence/M2/B4-10k.utrace"`
   **Expected:** the file exists, and the frame track shows frames of 16.7 ms or less.

## 6. S2: Pixel Streaming 2 from the Mac (F2)

Source: [unreal-platform.md](research/unreal-platform.md) U4; decision 0002. PS2 on macOS is
*unverified*: the only field report (UE 5.7 on an M4) shows a flat colour.

1. `cd "$WORK" && git clone --branch UE5.8 --depth 1 https://github.com/EpicGamesExt/PixelStreamingInfrastructure.git && cat PixelStreamingInfrastructure/NODE_VERSION`
   **Expected:** `v22.14.0`.
2. `LANIP=$(ipconfig getifaddr en0); echo $LANIP`
   **Expected:** your Mac's LAN address, for example `192.168.1.20`. If it is empty, your Wi-Fi is not `en0`. Run `networksetup -listallhardwareports` to find the right device.
3. `cd "$WORK/PixelStreamingInfrastructure/SignallingWebServer/platform_scripts/bash" && chmod +x *.sh && ./start.sh --publicip "$LANIP" -- --player_port=8080`
   **Expected:** on the first run it downloads Node v22.14.0 (darwin-arm64) and builds the frontend. It then logs that it is listening for streamers on 8888 and players on 8080. Leave it running. If macOS asks whether to allow incoming connections for `node`, click **Allow**.
4. Waits for section 3 and lane C (the app must be packaged with the Pixel Streaming 2 plugin on). In a second Terminal:
   `"$WORK/build/mac"/Mac*/OneMind.app/Contents/MacOS/OneMind -PixelStreamingURL=ws://127.0.0.1:8888 -ResX=1920 -ResY=1080 -Windowed`
   **Expected:** the app window opens, and the signalling log shows a streamer connected.
5. On the second device, open `http://<LANIP>:8080` and click to start.
   **Expected:** the **live globe** (not a solid colour), and mouse or touch control works. Open the frontend's stats panel and note the latency. **Pass: a correct picture and input latency under 150 ms.**
6. If the picture is a flat colour, quit the app and try again with `-RenderOffscreen` added, then with `-ResX=1280 -ResY=720`. Write down which variant works in `$OM/docs/evidence/M3/S2-result.md`, with screenshots taken on the viewing device.
7. If no variant shows the scene, S2 has failed. Record that. The L0 web client becomes the web path, and a Linux GPU host is documented, not provisioned (decision 0004 #1). Stop the server with Ctrl-C.

## 7. S3: Quest Pro standalone build (F4, J3)

Source: [unreal-platform.md](research/unreal-platform.md) U5; decision 0003. Try the options in
this order: standalone, then WebXR, then streaming (decision 0004 #2).

### 7.1 Toolchain and headset (ready now)

1. Install Android Studio from https://developer.android.com/studio. In **SDK Manager**, install **Android SDK Platform 35** and **34**, **Build-Tools 35.0.1**, **NDK 27.2.12479018 (r27c)** and **Platform-Tools**. Then run:
   ```bash
   export ANDROID_HOME="$HOME/Library/Android/sdk"
   export NDKROOT="$ANDROID_HOME/ndk/27.2.12479018"
   export JAVA_HOME="/Applications/Android Studio.app/Contents/jbr/Contents/Home"
   export PATH="$ANDROID_HOME/platform-tools:$PATH"
   adb version && "$JAVA_HOME/bin/java" -version
   ```
   **Expected:** adb prints a version, and Java prints `21.x`. The r27c package version and the bundled JBR 21 are *unverified*. If the folder names differ, use the ones SDK Manager shows.
2. Turn on developer mode. On your phone, open the Meta Horizon app, tap the headset icon, tap your Quest Pro, then go to **Headset Settings > Developer Mode** and switch it on. Connect the Quest Pro to the Mac with the USB-C data cable. In the headset, accept **Allow USB debugging > Always allow from this computer**.
3. `adb devices && adb shell getprop ro.product.model`
   **Expected:** one line ending in `device` (not `unauthorized`), then a model name. The exact model string is *unverified*. Write it down.

### 7.2 Package, install and measure (waits for section 3 and lane E)

4. Check the project settings (lane E commits them in `Config/DefaultEngine.ini`) under **Edit > Project Settings**:
   - OpenXR plugin **on**. Meta XR / OculusXR plugin **off**: it fails to load on UE 5.8 (Meta investigation 1053459897316880).
   - Start in VR on.
   - Android: Support arm64 on, Vulkan on, OpenGL ES 3.2 off, Minimum SDK 32 or higher, Target SDK 34 or higher.
   - Forward Shading on, MSAA 4x, Mobile HDR off, Mobile Multi-View on.
   - Cesium experimental occlusion culling on, terrain Maximum Screen Space Error 256.
5. Make a full package, never a Quick Launch (Cesium issue #1712):
   `"$UE/Engine/Build/BatchFiles/RunUAT.sh" BuildCookRun -project="$UPROJECT" -platform=Android -cookflavor=ASTC -clientconfig=Development -build -cook -stage -pak -package -archive -archivedirectory="$WORK/build/quest" -utf8output`
   **Expected:** `BUILD SUCCESSFUL` and `ExitCode=0`. Then `ls "$WORK/build/quest"/Android*/*.apk` lists one APK.
6. `"$ANDROID_HOME/build-tools/35.0.1/aapt" dump xmltree "$WORK/build/quest"/Android*/*.apk AndroidManifest.xml | grep -A1 supportedDevices`
   **Expected:** a value that contains `questpro`. If it is missing, record that: the app may run in compatibility mode, and lane E then adds the entry.
7. `adb install -r "$WORK/build/quest"/Android*/*.apk`
   **Expected:** `Success`. In the headset, open **Library > Unknown Sources > OneMind**. You should see the globe in VR.
8. Make the bus reachable from the headset. This is for the test network only, with simulated data. Stop it after the test.
   ```bash
   sed -e 's/listen: "127.0.0.1:9222"/listen: "0.0.0.0:9222"/' "$WORK/.run/nats-local.conf" > "$WORK/.run/nats-lan.conf"
   pkill -f nats-local.conf; sleep 2
   nats-server -c "$WORK/.run/nats-lan.conf" > "$WORK/.run/nats.log" 2>&1 &
   ```
   In the app's bus setting, enter `ws://<LANIP>:9222`. Then run the `kv put` from section 4 step 2.
   **Expected:** the `sim-x1` marker updates in the headset. Afterwards, run `pkill -f nats-lan.conf` and restart the local config from section 2.5.
9. `adb logcat -s VrApi | grep -m 20 FPS`
   **Expected:** lines with `FPS=72` (or 90). **Pass: it holds 72 fps with the globe and one layer.** The VrApi tag is *unverified* for OpenXR apps. If nothing prints, use the in-app `stat fps` overlay and write down the number. Save the log: `adb logcat -d > "$OM/docs/evidence/M5/S3-logcat.txt"`.
10. If S3 fails (a crash, a black screen, or under 72 fps after lowering the layer budget), write the cause in `$OM/docs/evidence/M5/S3-result.md`. Then try the WebXR fallback. In the headset browser, open the L0 client's HTTPS address (lane C provides it, because WebXR needs a secure context) and tap **Enter VR**.
    **Expected:** the globe and one layer in VR.

---

## 8. LAN mesh test with ATAK (A7, C1, C2)

Source: [tak-server-and-bridges.md](research/tak-server-and-bridges.md) §6 and Rec. 1;
[tak-protocols.md](research/tak-protocols.md) Rec. 2; the gateway README on omos branch
`feat/tak-mesh-gateway` (`services/bridge/bridges/tak/README.md`). **Ready now** with the branch.
After the A7 PR exists, use the branch that [STATUS.md](STATUS.md) names.

Before you start:

- The Mac and the ATAK phone are on the same Wi-Fi.
- ATAK's mesh encryption is **off**. The gateway has no mesh crypto until the A7 PR adds it
  ([tak-protocols.md](research/tak-protocols.md) Risks 3).
- The local bus from section 2.5 is running.
- `MESH_TO_TAK_SERVER` stays `false` (decision 0004 #6).

### 8.1 Build and test the gateway

```bash
cd "$WORK/omos" && git fetch origin && git checkout feat/tak-mesh-gateway && git log -1 --oneline
cd services/bridge/bridges/tak && go test ./... && go build -o "$WORK/.tools/bin/tak-mesh" .
```

**Expected:** the log line shows `977b7eb2` or a newer commit, `go test` prints `ok` with no
`FAIL`, and `ls "$WORK/.tools/bin/tak-mesh"` prints the binary. The run saw 9/9 tests pass.

### 8.2 Find the Wi-Fi interface

```bash
networksetup -listallhardwareports | grep -A1 Wi-Fi
LANIP=$(ipconfig getifaddr en0); echo $LANIP
route -n get 239.2.3.1 | grep interface
```

**Expected:** the Wi-Fi device (usually `en0`), your LAN address, and `interface: en0`. Replace `en0`
below if your Wi-Fi device is different.

### 8.3 Watch the mesh (Terminal A)

```bash
nats sub tak.cot.mesh.rx
```

Leave it running.

### 8.4 Run the gateway (Terminal B)

Set your approximate position in place of `<lat>` and `<lon>` first.

```bash
NATS_URL=nats://127.0.0.1:4222 GATEWAY_ID=mac-lan MESH_GROUP=239.2.3.1:6969 MESH_INTERFACE=en0 \
DIRECT_TCP_PORT=4242 MESH_TO_TAK_SERVER=false GATEWAY_CALLSIGN=ONEMIND-MAC \
GATEWAY_ENDPOINT_HOST="$LANIP" GATEWAY_LAT=<lat> GATEWAY_LON=<lon> "$WORK/.tools/bin/tak-mesh"
```

**Expected:** the gateway starts and keeps running. If macOS asks whether to allow incoming
connections, click **Allow**.

**Known defect (until the A7 PR fixes it):** the gateway receives on `MESH_INTERFACE` but sends on
the default route (`mesh.go:84`). If step 8.2 did not print `interface: en0`, turn off Tailscale and
unplug Ethernet for this test, so that Wi-Fi is the default route.

### 8.5 Checks

1. On ATAK, make sure it is reporting a position (GPS fix, or set your location manually).
   **Expected:** within ATAK's reporting interval, Terminal A shows a CoT event with your callsign in `<contact callsign=…>` and the headers `Tak-Delivery: mesh` and `Tak-Proto: 0` or `1`. Save five messages: `nats sub tak.cot.mesh.rx --count=5 > "$OM/docs/evidence/M1/A7-lan-rx.txt"`.
2. On ATAK, open the contact list.
   **Expected:** `ONEMIND-MAC` appears near `<lat>`, `<lon>`. The gateway sends its own position report every 30 s.
3. Send a marker to ATAK (Terminal C). Put a point near you in place of `<lat>` and `<lon>`:
   ```bash
   NOW=$(date -u +%Y-%m-%dT%H:%M:%SZ); STALE=$(date -u -v+1H +%Y-%m-%dT%H:%M:%SZ)
   nats pub tak.cot.mesh.tx "<event version=\"2.0\" uid=\"sim-marker-1\" type=\"b-m-p-s-m\" how=\"h-g-i-g-o\" time=\"$NOW\" start=\"$NOW\" stale=\"$STALE\"><point lat=\"<lat>\" lon=\"<lon>\" hae=\"0\" ce=\"9999999\" le=\"9999999\"/><detail><contact callsign=\"OneMind test\"/></detail></event>"
   ```
   **Expected:** an `OneMind test` marker appears on ATAK at that point within a few seconds, and the `tx` counter in the next status message (check 4) has gone up by at least 1. This uses a core publish with no reply subject, as the projector does ([cot-projection.md](contracts/cot-projection.md) §6.3): if a stream on your local bus captured `tak.cot.mesh.tx`, a `nats req` would get the gateway's `ok` and the stream's PubAck racing on one inbox. A `nats req` reply of `ok` only proves delivery when no local stream captures `tak.cot.mesh.tx`, which was not checked for the §2.5 local bus. Take a screenshot on the phone and save it as `$OM/docs/evidence/M1/A7-lan-marker.png`.
4. `nats sub 'tel.fleet.edge.*.status' --count=1`
   **Expected:** a JSON status with `contacts` of 1 or more, `rx` above 0 and `tx` above 0 (from check 3).
5. `nats sub tak.cot.inject` for about one minute, then press Ctrl-C.
   **Expected:** nothing arrives, because `MESH_TO_TAK_SERVER=false`.
6. GeoChat (only once the A7 PR adds it). In ATAK, send a chat message to `ONEMIND-MAC`.
   **Expected:** Terminal A shows it with `Tak-Delivery: direct`.
7. Mesh encryption (only once the A7 PR adds it). Turn ATAK mesh encryption on with a test key. Give the gateway the same key through the environment variable the PR names, never in a file in Git. Repeat check 1.
   **Expected:** the same result as check 1.
8. Stop the gateway with Ctrl-C. Write `pass` or `fail` for each check in `$OM/docs/evidence/M1/A7-lan-result.md`.

**Pass (A7 owner check):** checks 1 and 3.

## 9. ATAK display checks (A6, H9, K3, D2)

Source: [cot-projection.md](contracts/cot-projection.md) §12 (owner hardware);
[robotics-cot-profile.md](contracts/robotics-cot-profile.md) §9 (owner hardware);
[mission-task.md](contracts/mission-task.md) §10 test 7. **Waits for** lane A rows 8–12 (MAVLink
bridge, entity service, projector) and D2. The run will add a script that starts the entity service,
MAVLink bridge, projector and gateway from their branches against the local bus (*planned*:
`scripts/mac/run-local-stack.sh`).

1. Start the gateway as in section 8.4, then start the local stack script (*planned*).
2. Start the scripted drone near you (it is the fallback for SITL; it never talks to hardware):
   `"$WORK/.tools/py/bin/python" "$OM/scripts/sim/mavlink_sim_drone.py" --out udpout:127.0.0.1:14550 --lat <lat> --lon <lon>`
   **Expected:** ATAK shows `SIM-X1` with the UAV icon, the correct heading and speed (H9).
3. Tap the `SIM-X1` marker and open its video (needs section 11 running with path `drone/sim-x1`).
   **Expected:** ATAK opens the stream (K3).
4. Record what ATAK does, in `$OM/docs/evidence/M1/atak-display.md`:
   - A projected vessel shows the expected icon. An `a-n-X-i-g-e` earthquake marker shows, or write down what shows instead.
   - After an entity is removed, does ATAK delete the marker or grey it out? Repeat with a `t-x-d-d` removal.
   - Do markers look the same with and without `<__group>`?
   - With the drone flying east and the camera at `azimuth=0`, is the cone drawn to the north (absolute) or to the east (relative)?
   - Does ATAK draw its own KLV footprint next to OneMind's?
   - Does a ground robot (`a-f-G-E-V`) show a sensible ground-vehicle icon?
5. Draw an AOI in the client around the drone (A6, after D1).
   **Expected:** ATAK keeps showing `SIM-X1`. After you delete the AOI, the marker is removed.
6. Create a mission in the client with a route (D2).
   **Expected:** ATAK shows it as a route.

## 10. Real SITL with PX4 (H1, H2, A8)

Source: [drones-robots-agents.md](research/drones-robots-agents.md) Rec. 7 and Risks;
[PREFLIGHT.md](PREFLIGHT.md) (SITL could not be built on the build machine). The run names every
simulated vehicle `sim-*`.

### 10.1 PX4 SITL (ready now)

1. ```bash
   cd "$WORK" && git clone --recursive https://github.com/PX4/PX4-Autopilot.git
   cd PX4-Autopilot && bash ./Tools/setup/macos.sh
   ```
   **Expected:** the setup script finishes without errors. It may ask for your password, which is why it is on this checklist.
2. Set the home position near you, then start the SIH quadcopter (no Gazebo):
   ```bash
   export PX4_HOME_LAT=<lat> PX4_HOME_LON=<lon> PX4_HOME_ALT=<alt metres>
   make px4_sitl_sih sihsim_quadx
   ```
   **Expected:** the build finishes and a `pxh>` prompt appears. Whether SIH builds on macOS is *unverified*. If it fails, run `make px4_sitl gz_x500` (Gazebo) instead and save the SIH error to `$OM/docs/evidence/M1/sitl-sih-failure.log`.
3. In another Terminal:
   `"$WORK/.tools/py/bin/python" -c "from pymavlink import mavutil; m=mavutil.mavlink_connection('udpin:127.0.0.1:14550'); print(m.wait_heartbeat(timeout=30))"`
   **Expected:** a `HEARTBEAT` message is printed. It is *unverified* that PX4 SITL sends to UDP 14550 by default. If nothing prints within 30 s, run `mavlink status` at the `pxh>` prompt and write down the ports it lists.

### 10.2 Bridge and entity (waits for H2)

4. Start the MAVLink bridge from its branch with the asset id `sim-px4-1`, using the variables in the H2 PR's README. The planned values are `MAVLINK_URL=udpin:127.0.0.1:14550` and `ASSET_ID=sim-px4-1` (*planned*, *unverified*).
5. `nats sub 'tel.fleet.drone.sim-px4-1.>' --count=5`
   **Expected:** `position` and `status` messages with battery and mode (H1, H2).
6. `nats stream get ENT --last-for=ent.drone.sim-px4-1.state`
   **Expected:** one entity record with `sources[]` and a position near your home point (A8). This needs the `ENT` stream from lane A row 1 on the local bus.
7. In the client (after D3), click a point near home, choose **goto** and confirm.
   **Expected:** a confirm dialog shows the exact parameters. After you confirm, the drone flies there in SITL, and `nats sub 'evt.so.action.>'` shows `proposed`, `approved` and `executed`. Save that output to `$OM/docs/evidence/M4/D3-sitl.txt`.

## 11. Video on the LAN (K9, K4)

Source: [video-and-plugins.md](research/video-and-plugins.md) Rec. 2 and item 12; the lane
config `docs/evidence/M0/video/mediamtx-lane.example.yml`. Passwords go in the keychain.

1. Store the two stream passwords (you choose them; each command prompts for one):
   ```bash
   security add-generic-password -a "$USER" -s onemind-mediamtx-pub -w
   security add-generic-password -a "$USER" -s onemind-mediamtx-read -w
   ```
   **Expected:** no output and no error.
2. Install MediaMTX v1.21.3 for macOS arm64 from https://github.com/bluenviron/mediamtx/releases/tag/v1.21.3 into `$WORK/.tools/bin`. The asset name `mediamtx_v1.21.3_darwin_arm64.tar.gz` is *unverified*: pick the darwin arm64 archive on that page. Then:
   ```bash
   tar -C "$WORK/.tools/bin" -xzf ~/Downloads/mediamtx_v1.21.3_darwin_arm64.tar.gz mediamtx
   mediamtx --version
   ```
   **Expected:** `v1.21.3`.
3. Make a LAN copy of the lane config and start it:
   ```bash
   cd "$WORK/.run"
   sed -e 's/^rtspAddress: 127.0.0.1:/rtspAddress: :/' -e 's/^hlsAddress: 127.0.0.1:/hlsAddress: :/' \
       -e 's/^webrtcAddress: 127.0.0.1:/webrtcAddress: :/' -e 's/^webrtcLocalUDPAddress: 127.0.0.1:/webrtcLocalUDPAddress: :/' \
       "$OM/docs/evidence/M0/video/mediamtx-lane.example.yml" > mediamtx-lan.yml
   export PUBPASS=$(security find-generic-password -a "$USER" -s onemind-mediamtx-pub -w)
   export RDPASS=$(security find-generic-password -a "$USER" -s onemind-mediamtx-read -w)
   mediamtx mediamtx-lan.yml
   ```
   **Expected:** MediaMTX logs listeners for RTSP 18554, HLS 18888, WebRTC 18889 and SRT 18890. Whether MediaMTX expands `${PUBPASS}` in the config is *unverified* ([REFERENCE-ARCHITECTURE.md](design/REFERENCE-ARCHITECTURE.md) §14). If the publish in step 4 is refused as unauthorised, record it, and lane C changes the config.
4. Publish a test stream (needs ffmpeg: `ffmpeg -version`; otherwise `brew install ffmpeg`):
   `ffmpeg -re -f lavfi -i testsrc=size=1280x720:rate=30 -c:v libx264 -preset ultrafast -tune zerolatency -g 30 -pix_fmt yuv420p -f rtsp -rtsp_transport tcp "rtsp://pub-drone:$PUBPASS@127.0.0.1:18554/drone/sim-x1"`
   **Expected:** the MediaMTX log shows a publisher on path `drone/sim-x1`.
5. On the second device, open `http://<LANIP>:18888/drone/sim-x1/` and log in as `viewer` with the read password.
   **Expected:** the test pattern plays (K9). `ls "$WORK/.run/recordings/drone/sim-x1"` lists fMP4 recording files.
6. Measure glass-to-glass latency. Stop the ffmpeg from step 4, then list the capture devices and stream your screen:
   ```bash
   ffmpeg -f avfoundation -list_devices true -i ""
   ffmpeg -f avfoundation -framerate 30 -capture_cursor 0 -i "<screen index>:none" -c:v libx264 -preset ultrafast -tune zerolatency -g 30 -pix_fmt yuv420p -f rtsp -rtsp_transport tcp "rtsp://pub-drone:$PUBPASS@127.0.0.1:18554/drone/sim-x1"
   ```
   macOS will ask you to allow screen recording for Terminal. Allow it, then run the command again. Show a running clock with milliseconds on the Mac screen. Open `http://<LANIP>:18889/drone/sim-x1/` (WebRTC) on the second device, hold it next to the Mac screen and photograph both. Then do the same with the HLS address from step 5.
   **Expected:** WebRTC at or under 300 ms and HLS within 2–5 s ([video-and-plugins.md](research/video-and-plugins.md) item 12 targets). Write both numbers in `$OM/docs/evidence/M3/video-latency.md`.
7. Video panel in the client (waits for K4). Select `sim-x1` in the app.
   **Expected:** the panel follows the drone, with **under 1 s** glass-to-glass on the LAN, measured as in step 6.

## 12. Offline map pack and ATAK import (B2, B15)

Source: [edge-and-ddil.md](research/edge-and-ddil.md) §4 (proof E2) and "Unverified items" (ATAK
import of PMTiles, MBTiles or GeoPackage is unconfirmed). **Ready now.**

1. ```bash
   GOBIN="$WORK/.tools/bin" go install github.com/protomaps/go-pmtiles@v1.31.2
   go-pmtiles version
   ```
   **Expected:** a version line for 1.31.2. The binary name `go-pmtiles` is *unverified*. If the command is not found, run `ls "$WORK/.tools/bin"` to see the name.
2. Cut a small pack around the homestead. Use a box of about 0.1° × 0.1°, in the order min lon, min lat, max lon, max lat. Pick a current build from https://build.protomaps.com; the run used `20261028.pmtiles`, which may have been removed.
   `go-pmtiles extract https://build.protomaps.com/20261028.pmtiles "$WORK/.run/homestead.pmtiles" --bbox=<minlon>,<minlat>,<maxlon>,<maxlat> --maxzoom=15 && go-pmtiles verify "$WORK/.run/homestead.pmtiles"`
   **Expected:** a file of a few MB (the run got 4.4 MB for a 0.1° box), and `verify` passes. The data is ODbL and needs OpenStreetMap attribution.
3. Copy the pack to the ATAK phone (needs `adb` from section 7.1 and USB debugging on the phone):
   `adb push "$WORK/.run/homestead.pmtiles" /sdcard/atak/imagery/`
   **Expected:** `1 file pushed`. In ATAK, open the map layers list and record whether the pack is listed and renders. The `atak/imagery` folder and PMTiles support are both *unverified*. Write the result in `$OM/docs/evidence/M5/atak-offline-pack.md`.

## 13. TAK Server checks (C3, A21)

Source: [tak-server-and-bridges.md](research/tak-server-and-bridges.md) §1–2 and Rec. 2–4;
[tak-protocols.md](research/tak-protocols.md) Risks 1. These checks need your TAK certificate and
the owner tailnet. The build machine has neither ([PREFLIGHT.md](PREFLIGHT.md), Access).

1. Convert your `.p12` to PEM once. Keep it outside the repos and never commit it.
   ```bash
   mkdir -p ~/.onemind-tak && chmod 700 ~/.onemind-tak
   openssl pkcs12 -in <your-cert>.p12 -out ~/.onemind-tak/user.pem -nodes && chmod 600 ~/.onemind-tak/user.pem
   ```
   **Expected:** it prompts for the import password and writes `user.pem`. If OpenSSL 3 refuses an old `.p12`, add `-legacy`.
2. Check the server version and that the 5.7 API map still holds on 5.8:
   ```bash
   curl -sk --cert ~/.onemind-tak/user.pem https://tak.tail717ba0.ts.net:8443/Marti/api/version/config
   curl -sk --cert ~/.onemind-tak/user.pem -o /dev/null -w '%{http_code}\n' https://tak.tail717ba0.ts.net:8443/Marti/api/missions
   curl -sk --cert ~/.onemind-tak/user.pem -o /dev/null -w '%{http_code}\n' https://tak.tail717ba0.ts.net:8443/Marti/api/contacts/all
   ```
   **Expected:** JSON with a version containing `5.8-RELEASE-69`, then `200` for both endpoints. (`-k` skips server-certificate checking for this read-only test only.) Save the output to `$OM/docs/evidence/M1/tak-5.8-api.txt`. Any status other than 200 means the 5.7 API map needs a correction.
3. TAK leg after the A21 PR is merged and deployed. This is read-only; the pod name and port are *unverified*:
   ```bash
   kubectl --context do-nyc3-omos-cloud -n nats port-forward pod/nats-0 8222:8222 &
   sleep 2; curl -s 'http://127.0.0.1:8222/leafz?subs=1' | grep -c 'tak.cot.inject'; kill %1
   ```
   **Expected:** a count of 1 or more, meaning the TAK leaf subscribes to `tak.cot.inject` ([omos-live-state.md](research/omos-live-state.md) Rec. 2).
4. Client as a TAK contact (after C3). Start the app with your certificate configured as C3's README says.
   **Expected:** the client appears in ATAK's contact list.

## 14. Site leaf and disconnected mode (A23, F9)

Source: [edge-and-ddil.md](research/edge-and-ddil.md) Rec. 2; [BUS-DESIGN.md](design/BUS-DESIGN.md)
§8–§9; stop-line test 12 in [JADC2-MAP.md](design/JADC2-MAP.md) §5.3. **Waits for** A23 (site
leaf template in omos `infra/edge/`) and F9.

1. Install the Mac site leaf with the A23 PR's script (*planned*). It sets domain `site-mac`, a 72 h edge stream, CA-verified TLS and its own credential.
   **Expected:** the script's check reports the leaf connected to the hub with domain `site-mac`.
2. Rerun the P7 and E1 tests from the A23 PR on the Mac.
   **Expected:** telemetry published while the leaf is cut off reaches the hub after reconnect, with no loss and no duplicates. Write down the catch-up time; local runs saw 7–41 s ([edge-and-ddil.md](research/edge-and-ddil.md) Risks ER4).
3. Disconnected mode (F9). Unplug the router's internet uplink, but keep the Wi-Fi up.
   **Expected:** the client still shows entities, tiles and terrain, and Hermes (local model) answers a question. A goto proposed while OpenFGA is unreachable is denied `policy_unavailable`. Save screenshots to `$OM/docs/evidence/M5/F9-offline/`.

## 15. Later checks

| Check | Feature | Waits for | Expected | Source |
|---|---|---|---|---|
| Video decals on Cesium tiles in the S1 build | E4, K5 | E4 | the footprint decal draws on the tiles; if not, record it so the Cesium material-layer fallback is used | [video-and-plugins.md](research/video-and-plugins.md) item 11 |
| Cesium offline terrain from a local quantized-mesh `layer.json` | B2, F9 | S1 | terrain renders from `file:///` with Wi-Fi off | [edge-and-ddil.md](research/edge-and-ddil.md) "Unverified items" |
| A real Meshtastic node | A20 | A20 | its position appears as an entity | [edge-and-ddil.md](research/edge-and-ddil.md) Rec. 5 |
| `OneMindATAK` plugin build and install | C9 | C9; ATAK SDK access and plugin signing (*unverified*) | ATAK shows OneMind provenance for an entity | [tak-protocols.md](research/tak-protocols.md) Level 4, Risks 6 |
| Three PX4 SITL drones search a drawn area | H5 | H5 | each child task gets its own approval and completes | [drones-robots-agents.md](research/drones-robots-agents.md) Rec. 7 |
| Retry the Commo build on a host that allows it | C4 (blocked) | any time | `make TARGET=macos-64 GDAL_USE_KDU=no commoncommo_BUILDJAVA= commoncommo_BUILDOBJC=yes build_commoncommo` links | [tak-server-and-bridges.md](research/tak-server-and-bridges.md) §S4, Rec. 6 |

## 16. Owner decisions (no commands)

The run does not wait on these. Each one has a default ([decision 0004](decisions/0004-owner-defaults-for-the-run.md)).
Record your answer in the decision file named, or in an omos PR where it says so.

| # | Decision | Default the run uses | Record in | Source |
|---|---|---|---|---|
| 1 | Add CloudTAK (AGPL-3.0) to decision 0004 #15 as design-only | treated as design-only | decision 0004 | decision 0004 run log; [competitive-matrix.md](research/competitive-matrix.md) Rec. 2 |
| 2 | Rule on every grammar proposal in the combined register: BUS-P1–BUS-P11, ONT-P1–ONT-P14, RA1–RA4, CP-1–CP-4 and the `intercept` example (F1). This includes the policy subject names (ONT-P14, RA2), the missing `evt.vehicle.>` binding (ONT-P13) and the mesh subjects' direction (CP-4) | the "used until ruled" reading in each source | omos proposal PR (A24) | [BUS-DESIGN.md](design/BUS-DESIGN.md) §10.1; [ONTOLOGY.md](design/ONTOLOGY.md) §9; [REFERENCE-ARCHITECTURE.md](design/REFERENCE-ARCHITECTURE.md) §13; [cot-projection.md](contracts/cot-projection.md) §11; [JADC2-MAP.md](design/JADC2-MAP.md) §6 F1 |
| 3 | Allow person presence alerts (no identity) on your own cameras? | no: detection of vehicles and animals only | new decision record | [JADC2-MAP.md](design/JADC2-MAP.md) §6 F3 |
| 4 | Does one confirm screen for all children of a composite task meet Rule 6, or one click per child? | one approval record per child | decision 0004 | [mission-task.md](contracts/mission-task.md) §11 |
| 5 | `estop` mapped to flight termination before any real hardware | review before real hardware (not in this run) | H6 review | [JADC2-MAP.md](design/JADC2-MAP.md) §6 F5 |
| 6 | Narrow the `tag:k8s` → `omos-mbp:22` Tailscale grant; turn on the DOKS control-plane firewall; confirm an Alertmanager receiver; a TAK CRL process for lost devices; Authentik with auth callout for browser clients | none applied (cluster is read-only for the run) | omos PRs you approve | [security-and-trust.md](research/security-and-trust.md) Rec. 5 |
| 7 | Rotate and remove the example credentials in omos `services/takserver/cot-tap/README.md` | reported, not changed | omos PR | [tak-server-and-bridges.md](research/tak-server-and-bridges.md) §5, Risks 4 |
| 8 | Photorealistic basemap and licence tier (Cesium ion Community is personal and non-commercial; Google is metered) | open data; Google off | new decision record (E5) | [OPEN-QUESTIONS.md](OPEN-QUESTIONS.md) #4; decision 0004 #4 |
| 9 | A GPU host for streaming if S2 fails | documented, not provisioned | decision 0002 | [OPEN-QUESTIONS.md](OPEN-QUESTIONS.md) #5; decision 0004 #1, #5 |
| 10 | Should mesh CoT also go to TAK Server by default (`MESH_TO_TAK_SERVER`)? | `false` | omos ADR 0003 | decision 0004 #6 |
| 11 | Repository visibility long term (GPL-3.0 needs source for any distributed binary) | private | decision 0004 | decision 0004 #9; [OPEN-QUESTIONS.md](OPEN-QUESTIONS.md) #9 |
| 12 | Hardware for later sites: T1 Pi 5 field relay, T2 Jetson Orin Nano Super for detection, T3 Mac mini or this Mac as home server | buy nothing in this run | new decision record | [edge-and-ddil.md](research/edge-and-ddil.md) Rec. 6; decision 0004 #5 |

---

## Feature status for owner checks

| Feature | Section | State |
|---|---|---|
| B1, B2, F1 | 3 | not built |
| B3 | 4 | not built |
| B4, B5, G2 | 5 | not built |
| F2 | 6 | not built |
| F4, J3 | 7 | not built |
| A7, C1, C2 | 8 | not built (gateway on omos branch; can be tried now) |
| A6, H9, K3, D2 | 9 | not built |
| H1, H2, A8 | 10 | not built |
| K9, K4 | 11 | not built |
| B2, B15 (offline pack) | 12 | not built |
| C3, A21 | 13 | not built |
| A23, F9 | 14 | not built |

No feature is "built, awaiting owner check" yet. When a lane finishes a feature on the build
machine, it changes that row to **built, awaiting owner check** and links the evidence.
