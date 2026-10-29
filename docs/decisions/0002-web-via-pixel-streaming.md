---
status: proposed. S2 has not run, because Unreal is not installed on the build machine. The owner must run S2 on the Mac
date: 2026-10-29
updated: 2026-10-29 (M0 synthesis, from spike results)
---

# 0002 — The web client is the same app, via Pixel Streaming 2

## Decision (unchanged intent)

Browsers get the full client through Pixel Streaming 2 (PS2), so the web version never
drifts from the desktop app. A light L0 web view (CesiumJS on the same contracts)
serves phones and weak connections (FEATURES F2, J1).

## Spike S2 result: not run

| Item | Result | Source |
|---|---|---|
| Could S2 run on the build machine? | **No.** Unreal Engine 5.8 is not installed. It needs admin rights and about 150 GB, and only 25 GiB was free | `docs/PREFLIGHT.md` (Resources, Tools); `docs/research/unreal-platform.md` (lane header) |
| S1 (the globe in UE 5.8), which S2 depends on | Not run, for the same reason | `docs/research/unreal-platform.md` §S1 |
| Does Epic say PS2 works on a Mac? | Unclear. The UE 5.8 reference says both "supports … Mac" and "OSX is not supported". It was tested on macOS Ventura 13.5.1 and needs VideoToolbox hardware | `docs/research/unreal-platform.md` fact 8 [S-PSREF] |
| Is there signalling infrastructure for 5.8 on Apple Silicon? | Yes. The `UE5.8` branch of `PixelStreamingInfrastructure` has bash scripts that download darwin-arm64 Node v22.14.0 and a Mac arm64 coturn | fact 9 [G-PSI] |
| Are there field reports from Macs? | One, for UE 5.7 on an M4 (2026-06-21): the stream connects but the browser shows a flat colour. There are no replies and no fix | fact 10 [F-PS-MAC] |
| Overall | **Unproven.** Pixel Streaming 2 on macOS for UE 5.8 is *unverified* | — |

S2 did not fail. It could not run. So the 0004 #1 fallback has **not been triggered**,
and the Mac PS2 path is still the default until the owner's run says otherwise.

## What the run does now

| Topic | What applies | Why |
|---|---|---|
| Where the build and tests happen | Decision 0004 **#17** (a default, not a fallback): build on the build machine, write the Mac steps into the owner checklist, and mark F2 "built, awaiting owner check" | The build machine is not the owner's Mac (`docs/PREFLIGHT.md`) |
| L0 web client (J1) | Built in this run as planned. It is in M3 either way (`docs/ROADMAP.md` M3) and is also the ready 0004 #1 fallback | It needs no Unreal and runs on the build machine's GPU (`docs/PREFLIGHT.md`, Resources) |
| Bus connection for the L0 client | **0004 #12 fallback in use:** the local nats-server WebSocket listener `ws://127.0.0.1:9222`. The server side was proven for KV get and subscriptions | `docs/PREFLIGHT.md` (Local stack, Access); `docs/research/unreal-platform.md` [C-WS] |
| Cluster bus for the browser later | The hub already has a WebSocket listener on 8080, exposed at `ws.onemindos.dev` behind the Cloudflare allowlist. The run has no NATS user, so it is not used | omos `platform/nats/ws-ingress.yaml`; `docs/research/nats-platform.md` (live-state table, `ws_port 8080`) |
| Basemap and terrain | **0004 #4 fallback in use:** open imagery and terrain. No Cesium ion token was found, and Google Photorealistic 3D Tiles stay off | `docs/PREFLIGHT.md` (Access: `cesium-ion` not found) |
| Linux GPU streaming host | Not provisioned (0004 #5). It is documented only if S2 fails | `docs/decisions/0004-owner-defaults-for-the-run.md` #1, #5 |

The data the L0 client reads follows the client contracts in `docs/contracts/`. Those
contracts cite the omos definitions, for example KV bucket `world-state`
(omos `platform/nats/streams/kv-world-state.yaml`) and TAK CoT on `tak.cot.out` in stream
`COT` (omos `docs/NATS-DOCTRINE.md` §9.8; decision 0004 #7). This decision adds no new
subject.

## How the owner closes S2

Run step **U4 (S2)** in `docs/research/unreal-platform.md` (Owner checklist steps) after
U1–U2 (S1) pass. The plan is:

1. Package `OneMind.app` with the PS2 plugin. Start the `UE5.8` signalling server on the same
   Mac with `--publicip <LAN IP>` (so the script does not call `api.ipify.org`) and
   `--player_port=8080`. Use H.264 through VideoToolbox.
2. Open `http://<mac-lan-ip>:8080` from another device on the LAN. Try windowed mode first, then
   `-RenderOffscreen`, then `-ResX=1280 -ResY=720`. Write down which one shows the
   scene in `docs/evidence/M3/S2-result.md`.
3. **Pass:** a live picture and input latency under 150 ms on the LAN (`docs/ROADMAP.md` S2;
   FEATURES F2). If it passes, this decision becomes *accepted* and PS2 from the Mac is the M3 path.
4. **Fail** (no variant shows the scene): apply **0004 #1**. The L0 CesiumJS client becomes
   the web path, and a Linux NVIDIA streaming host is **documented, not provisioned**.
   Record the failure in this file and in the 0004 run log.

## Still true either way

The browser is never offline-capable. Disconnected use stays with the native clients
and with TAK on the mesh. The mesh path is the omos mesh gateway only, because S4
failed (0004 #3; `docs/research/tak-server-and-bridges.md` §S4).

## Risks

- PS2 shows the wrong picture on the Mac (high). The only report has no fix, and Epic's page
  contradicts itself (`docs/research/unreal-platform.md` Risks #2). The mitigation is that the
  L0 fallback is being built now.
- Running the Linux host would need paid infrastructure, which decision 0004 #5 forbids in this
  run. Its cost is *unverified* until someone documents it.
