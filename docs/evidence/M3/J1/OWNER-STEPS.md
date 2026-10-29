# J1 owner steps: the L0 web client on the Mac and on a phone over the LAN

Feature J1 (FEATURES.md): "Phone browser shows mission tier and alerts from the local
nats-server WebSocket". Built and tested on the build machine (headless Chromium, desktop and
phone viewports: `playwright-run.txt`, `screens/`). The phone check needs the owner's Mac, phone
and Wi-Fi, so J1 is **built, awaiting owner check**.

Everything below runs on the Mac. No password is written to a file in Git; the NATS password
lives in the macOS keychain and reaches nats-server and the fixture through environment
variables only. All published records are simulated (`sim-*`).

## 0. Variables and prerequisites

```bash
export WORK="$HOME/work"
export OM="$WORK/onemind-xr"
export LANIP=$(ipconfig getifaddr en0)    # Wi-Fi address; use en1 if en0 prints nothing
echo "$LANIP"
node --version                            # needs v22 or newer
"$WORK/.tools/bin/nats-server" --version  # v2.15.2 (OWNER-CHECKLIST section 2)
"$WORK/.tools/bin/mediamtx" --version     # v1.21.3 (OWNER-CHECKLIST section 11 step 2)
ffmpeg -version | head -1
```

**Expected:** a LAN address such as `192.168.1.20`, `v22.x`, `nats-server: v2.15.2`,
`v1.21.3`, and an ffmpeg version line. Check out the branch that holds `web/` (PR `run/m3-web`,
or `main` after it merges).

## 1. Build the client

```bash
cd "$OM/web"
npm ci
npm run build
```

**Expected:** `tsc` prints nothing, then Vite prints `✓ built in …` and `dist/index.html`.

## 2. A NATS WebSocket the phone can reach, with a password

The session server in OWNER-CHECKLIST section 2 binds WebSocket to `127.0.0.1:9222`, which a phone
cannot reach. Start a second, LAN-facing server for this check only, with one user whose password
is in the keychain.

```bash
security add-generic-password -a l0 -s onemind-l0-nats -w     # prompts; choose a password
mkdir -p "$WORK/.run/l0-nats"
cat > "$WORK/.run/l0-nats/nats.conf" <<'EOF'
listen: "127.0.0.1:14222"
websocket { listen: "0.0.0.0:19222", no_tls: true }
authorization { user: l0, password: $L0_NATS_PASSWORD }
EOF
export L0_NATS_PASSWORD=$(security find-generic-password -a l0 -s onemind-l0-nats -w)
"$WORK/.tools/bin/nats-server" -c "$WORK/.run/l0-nats/nats.conf"
```

**Expected:** the log shows `Listening for websocket clients on ws://0.0.0.0:19222` and
`Server is ready`. If macOS asks whether `nats-server` may accept incoming connections, click
Allow. Leave it running (terminal A). `no_tls` means the password crosses your Wi-Fi in clear
text: use this only on your home network, and stop the server after the check.

## 3. Video: MediaMTX on the LAN with the sim-x1 test pattern

For this check, reading is open on the LAN and publishing is allowed only from the Mac itself.
(The authenticated lane setup is OWNER-CHECKLIST section 11; the L0 player sends no credentials
yet.)

```bash
mkdir -p "$WORK/.run/l0-mtx"
cat > "$WORK/.run/l0-mtx/mediamtx.yml" <<EOF
authMethod: internal
authInternalUsers:
  - user: any
    pass:
    ips: ["127.0.0.1", "::1"]
    permissions: [{action: publish}, {action: read}]
  - user: any
    pass:
    ips: []
    permissions: [{action: read}]
rtspAddress: 127.0.0.1:8554
rtspTransports: [tcp]
rtmp: false
srt: false
moq: false
hlsAddress: :8888
hlsAllowOrigins: ["*"]
hlsVariant: lowLatency
webrtcAddress: :8889
webrtcAllowOrigins: ["*"]
webrtcLocalUDPAddress: :8189
webrtcAdditionalHosts: ["$LANIP"]
paths:
  all_others:
EOF
cd "$WORK/.run/l0-mtx" && "$WORK/.tools/bin/mediamtx" mediamtx.yml
```

**Expected:** MediaMTX logs listeners on 8554, 8888 and 8889 (terminal B; allow incoming
connections if asked). In terminal C publish the test pattern:

```bash
ffmpeg -re -f lavfi -i testsrc2=size=1280x720:rate=30 -c:v libx264 -preset ultrafast \
  -tune zerolatency -g 30 -bf 0 -pix_fmt yuv420p -profile:v baseline \
  -f rtsp -rtsp_transport tcp rtsp://127.0.0.1:8554/drone/sim-x1
```

**Expected:** terminal B logs `is publishing to path 'drone/sim-x1'`.

## 4. Entities on the bus

Either the fixture publisher (quick), or the real services (better, needs omos PRs #552 and #550
running locally). Fixture, terminal D:

```bash
cd "$OM/web"
export NATS_PASSWORD=$(security find-generic-password -a l0 -s onemind-l0-nats -w)
node tools/fixture-publisher.mjs --nats ws://127.0.0.1:19222 --user l0 --mode entity \
  --video rtsp://127.0.0.1:8554/drone/sim-x1 --remove-after 90 --expire-after 60
```

**Expected:** `fixture: mode=entity publishing to ws://127.0.0.1:19222`, then every 10 s a line
with the record count.

Real services instead (optional): run the omos entity service (PR #552, `services/entity`, with
`NATS_URL=nats://127.0.0.1:14222 NATS_USER=l0 NATS_PASSWORD=… ENT_MODE=core LEASE_BUCKET=none`)
and the MAVLink bridge (PR #550) with the scripted drone
(`"$WORK/.tools/py/bin/python" "$OM/scripts/sim/mavlink_sim_drone.py" --out udpout:127.0.0.1:14550`).
**Expected:** `ent.drone.sim-x1.state` appears (`nats --server nats://l0:…@127.0.0.1:14222 sub 'ent.>'`).
The drone then has no `media[]` unless the video registry (K2) is running, so the video panel
says "This entity has no video stream"; that is correct.

## 5. Serve the page to the LAN

Terminal E:

```bash
cd "$OM/web" && npx vite preview --host 0.0.0.0 --port 4173
```

**Expected:** `Network: http://<LANIP>:4173/`.

## 6. Check on the Mac

Open `http://127.0.0.1:4173/?nats=ws://127.0.0.1:19222` in Safari or Chrome. Open the
**Connection** tab, enter user `l0` and the keychain password, press **Connect**.

**Expected:**
1. The chip next to the title turns green: `bus ws://127.0.0.1:19222`.
2. The globe moves to Austin, TX and shows SIM-X1 (blue, moving in a circle), TRACK 7 (yellow,
   moving east), SIM-V1, SIM-R1, SIM-E1 and the cyan HOME AOI square. SIM-M1 is not shown (its
   marking is for `team-b` only).
3. **Alerts** shows `WARNING SIM-X1 battery below 85%`; within about 10 s `CAUTION trk-… entered
   zone-home` appears.
4. Click SIM-X1. **Detail** shows `SIMULATED`, `MISSION TIER`, Provenance `bridge-mavlink`,
   `m-s (simulated)`, the source `tel.fleet.drone.sim-x1.position`, and the **Video** panel plays
   the test pattern with `WebRTC (WHEP) drone/sim-x1` under it.
5. Press **Mission tier: off** so it reads **on**: SIM-V1 (outside the AOI, untasked) disappears.
6. After about 60 s SIM-E1 disappears (expiry, no message) and after about 90 s SIM-R1
   disappears (final `live: false` record). Restart the fixture to repeat.
7. Press **2D**: the globe flattens to a map with the same entities; press **3D** to go back.

## 7. Check on the phone (the J1 acceptance)

Phone on the same Wi-Fi. Open `http://<LANIP>:4173/` (the page derives
`ws://<LANIP>:19222` from its own address). Enter user `l0` and the password in **Connection**,
press **Connect**, then press **Mission tier: off** once so it reads **on**.

**Expected:**
1. The bus chip is green and shows `ws://<LANIP>:19222`.
2. The **Alerts** tab lists `WARNING SIM-X1 battery below 85%`; the red `Alerts 1` (or 2) chip is
   in the header.
3. The globe shows the HOME AOI with SIM-X1 and the entities inside it; SIM-V1 is hidden.
4. Tap the alert title: the **Detail** tab opens on SIM-X1 with provenance, and the video plays
   (WHEP on Android Chrome; on iPhone Safari either WHEP or `LL-HLS drone/sim-x1`).
5. The page fits the screen width with no sideways scrolling; tabs are easy to tap.

If the video panel says `No video`, check terminal B for a reader and that UDP 8189 is not
blocked by the macOS firewall; HLS (`?video=hls`) needs only TCP 8888.

## 8. Record and stop

Write `pass` or `fail` with a phone screenshot for checks 6.1–6.7 and 7.1–7.5 in
`$OM/docs/evidence/M3/J1/owner-result.md`. Stop terminals A–E with Ctrl-C. Optionally remove the
keychain item: `security delete-generic-password -a l0 -s onemind-l0-nats`.

**Pass (J1 owner check):** 7.1, 7.2 and 7.3 on the phone.

---

## Proposed OWNER-CHECKLIST entry (for the orchestrator)

> ### L0 web client on the Mac and a phone (J1)
>
> Source: `docs/evidence/M3/J1/OWNER-STEPS.md` (full commands). Needs sections 2 and 11 tools
> (nats-server 2.15.2, MediaMTX 1.21.3, ffmpeg) and Node 22.
>
> 1. `cd "$OM/web" && npm ci && npm run build` — **Expected:** `✓ built`.
> 2. Start the LAN-facing NATS (WebSocket `0.0.0.0:19222`, user `l0`, password from the keychain
>    item `onemind-l0-nats`) — **Expected:** `Listening for websocket clients on ws://0.0.0.0:19222`.
> 3. Start MediaMTX with the L0 config and the ffmpeg test pattern on `drone/sim-x1` —
>    **Expected:** `is publishing to path 'drone/sim-x1'`.
> 4. `node tools/fixture-publisher.mjs --nats ws://127.0.0.1:19222 --user l0 --mode entity --video rtsp://127.0.0.1:8554/drone/sim-x1 --remove-after 90 --expire-after 60`
>    (password in `NATS_PASSWORD` from the keychain).
> 5. `npx vite preview --host 0.0.0.0 --port 4173`; on the phone open `http://<LANIP>:4173/`, log
>    in as `l0`, turn **Mission tier** on — **Expected:** the Alerts tab shows `WARNING SIM-X1
>    battery below 85%`, the globe shows the HOME AOI with SIM-X1, and tapping the alert opens
>    SIM-X1's detail with provenance and the video playing.
>
> **Pass (J1):** step 5 on the phone. Record in `docs/evidence/M3/J1/owner-result.md`.
