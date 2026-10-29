# Lane C video (K1–K4, K6, K9): owner steps

omos PR: https://github.com/onemindos/omos/pull/559 (branch `feat/mediamtx-video`, draft).
Nothing below can be run on the build machine: each step needs the cluster (sync), the tailnet,
the owner's Mac, a second LAN device or ATAK. Passwords are never typed on a command line: read
them with `read -rs P` or from the keychain.

## 1. Before syncing `platform-onetak-media` (K1, K2)

1. Seal the two Secrets exactly as in `omos/platform/onetak/media/SEALING.md`; commit
   `onemind-media-users.sealedsecret.yaml` and `video-registry-nats.sealedsecret.yaml` and add
   both to `resources:` in `platform/onetak/media/kustomization.yaml`.
   Expected: `kubeseal` writes two files containing only `encryptedData`.
2. After the PR is merged, the `Build video registry` workflow on `main` uploads
   `image-digest.txt`; replace `ghcr.io/onemindos/omos-video-registry:0.1.0` in
   `platform/onetak/media/video-registry.yaml` with `…:0.1.0@sha256:<digest>`.
3. Approve the 20 Gi `do-block-storage-retain` volume and the 6 h (drones/robots) / 2 h (cameras) retention sized to it (proposal K-P4; alerts `OneMindMediaRecordingsFillingUp` 75% and `OneMindMediaRecordingsAlmostFull` 90%), then sync by hand:
   `argocd app sync platform-onetak-media`.
   Expected: `kubectl -n onetak get pods -l app.kubernetes.io/part-of=omos-video -o wide` shows
   `onemind-media-…` and `video-registry-…` Running on `onemind-ecosystem-cpu-*`;
   `kubectl -n onetak get svc onemind-media-tailnet` gets a tailnet address; a device `media`
   appears in the Tailscale admin console.
4. If MediaMTX does not start as UID 65532 (CrashLoop with a permission error), record the log;
   that check is unverified on the build machine.

## 2. K1 acceptance on the tailnet (Mac)

```bash
read -rs P   # the mavlink-bridge password
ffmpeg -re -f lavfi -i testsrc2=size=1280x720:rate=30 -c:v libx264 -preset ultrafast -tune zerolatency \
  -g 30 -f rtsp -rtsp_transport tcp "rtsp://mavlink-bridge:$P@media.tail717ba0.ts.net:8554/drone/sim-x1"
```

- Expected: ffmpeg keeps publishing. The same command without `mavlink-bridge:$P@` fails with
  `401 Unauthorized`.
- In Safari open `http://media.tail717ba0.ts.net:8888/drone/sim-x1/`, sign in as
  `onemind-client`. Expected: the test pattern plays (LL-HLS). Cancelling the sign-in gives 401.
- `kubectl -n onetak exec deploy/onemind-media -- ls /recordings/drone/sim-x1` after 2 minutes.
  Expected: at least two `.mp4` segments.

## 3. K2 acceptance

```bash
nats req svc.registry.video.get '{"entity":"ent.drone.sim-x1.state"}'
```

Expected: `"live": true`, `"entity_subject": "ent.drone.sim-x1.state"`,
`"tak_url": "rtsp://media.tail717ba0.ts.net:8554/drone/sim-x1"`. In the OneMind client,
selecting `sim-x1` shows the stream (once the client reads `svc.registry.video.get`, or the
entity's `media[]` after proposal K-P2).

## 4. K3: ATAK opens the drone stream (needs the A6 projector)

1. In ATAK, Video tool, add alias `SIM-X1 cam0`, URL
   `rtsp://media.tail717ba0.ts.net:8554/drone/sim-x1`, with the `tak-viewer` credentials
   (proposal K-P3: CoT URLs never carry credentials).
2. When the projector emits `<__video>` from `internal/takvideo`, tap the SIM-X1 marker and
   choose the video. Expected: the test pattern plays. Record whether ATAK asks for credentials
   or reuses the alias.

## 5. K9: edge MediaMTX on the Mac

```bash
cd omos
infra/edge/mac-media/edge-media.sh selftest          # expected last line: SELFTEST OK
infra/edge/mac-media/edge-media.sh keychain-set      # edge-home = the sealed value; two new LAN passwords
infra/edge/mac-media/edge-media.sh run
# second terminal: the Mac camera as cam/maccam
ffmpeg -f avfoundation -framerate 30 -i "0" -c:v libx264 -preset ultrafast -tune zerolatency -g 30 \
  -f rtsp -rtsp_transport tcp \
  "rtsp://edge-publisher:$(security find-generic-password -s onemind-media -a edge-publisher -w)@127.0.0.1:8554/cam/maccam"
```

- Expected: on a second LAN device, VLC opens `rtsp://<mac-lan-ip>:8554/cam/maccam` as
  `lan-viewer` and shows the camera (K9 owner check).
- On a tailnet device, `rtsp://media.tail717ba0.ts.net:8554/cam/maccam` as `onemind-client`
  shows the same stream (forwarded over SRT). If this fails while the LAN read works, UDP 8890
  through the Tailscale operator is the suspect (K-P10).
- The terminal running `run` must never print the edge-home password (it shows `<redacted>`).

## 6. K4: client panels (Mac app and headset; client lane)

The Unreal `OneMindVideo` plugin is not built here (no Unreal on the build machine). When it
exists: attach a panel to the `sim-x1` actor that plays the registry's `urls.hls` with Electra.
Expected: the panel follows the drone. Measure glass-to-glass latency by filming a stopwatch
next to the screen, and record the HLS number and later the native RTSP/WHEP number. The
target is ≤ 1 s on the LAN, which Electra on HLS is not expected to meet (video-and-plugins.md fact E).
