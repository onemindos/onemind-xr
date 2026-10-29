# M3 / J1 evidence: L0 light web client

Branch `run/m3-web` (commit 8e8964b), PR https://github.com/onemindos/onemind-xr/pull/2, code in `web/`.
State: **built, awaiting owner check** (phone over the LAN).

| File | What it proves |
|---|---|
| `vitest-run.txt` | `tsc` clean; 35 unit tests (merge rules, layer client, mission tier, alerts, video URLs) and 17 integration tests against a disposable nats-server over WebSocket in three fixture modes pass (1 skipped by design) |
| `playwright-run.txt` | 4 Playwright tests (chromium only) pass: desktop interim path with WHEP video, removal, expiry, mission tier; phone 390x844 layer path with alerts, mission tier, detail and LL-HLS video; core-only path; online OSM + terrarium terrain with no Cesium ion request |
| `screens/desktop-detail-video.png` | entity detail with provenance and the sim-x1 WHEP video at 1440x900 |
| `screens/desktop-mission-alerts.png` | mission tier on, AOI, warning and geofence alerts |
| `screens/desktop-osm-terrarium.png` | OSM imagery with terrarium terrain, no token |
| `screens/phone-mission-alerts.png` | J1 acceptance shape: phone shows mission tier and alerts |
| `screens/phone-detail.png`, `phone-detail-video.png`, `phone-entities.png` | phone detail, LL-HLS video, entity list after a removal |
| `webgl-probe.txt` | why the e2e browser runs with `--in-process-gpu` on this machine |
| `owner-steps-verification.txt` | the OWNER-STEPS NATS auth and MediaMTX config checked on loopback |
| `OWNER-STEPS.md` | Mac and phone steps with expected results, plus the proposed OWNER-CHECKLIST entry |
| `OWNER-PROPOSALS.md` | contract gaps J1-P1..P5 (no new subjects) |

Installs made for this item (remove by deleting them): Playwright Chrome for Testing headless
shell 156.0.8078.4 and Playwright ffmpeg in `work/.tools/ms-playwright` (206 MB,
`PLAYWRIGHT_BROWSERS_PATH`); npm packages in `work/wt/j1-web/web/node_modules` (about 250 MB).
