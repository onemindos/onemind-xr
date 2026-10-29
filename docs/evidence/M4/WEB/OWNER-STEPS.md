# WEB owner steps: the Act features on the Mac, a phone, Hermes and ATAK

These are the checks the build machine cannot do: a real GPU browser on the owner's Mac, a phone
on the LAN, the real Hermes with its model, an ATAK device, and TAK Server. Everything else
already passed here (`vitest-run.txt`, `playwright-run.txt`, `screens/`, `act-transcript.json`).
Mark each feature **built, awaiting owner check** until its step below passes.

## 0. Prerequisites (Mac)

```bash
export WORK=~/work                 # the folder holding onemind-xr/ and omos/
cd $WORK/onemind-xr && git fetch origin && git checkout run/m4-web
cd $WORK/omos && git fetch origin
git worktree add $WORK/wt/i4-policy origin/feat/policy-service          # omos #554 (the suite builds a temp copy with D1-P4 applied)
git worktree add $WORK/wt/h2-mavlink origin/feat/mavlink-bridge-fleet   # omos #550
# Tools: Node 22; Go 1.26 or newer (the policy go.mod says 1.25.0, the bridge module 1.26.0);
# nats-server (verified here with v2.15.2); Python 3 with pymavlink:
python3 -m venv $WORK/.tools/py && $WORK/.tools/py/bin/pip install pymavlink
cd $WORK/onemind-xr/web && npm ci
npx playwright install --only-shell chromium
```

## 1. Automated Act suite on the Mac (D1, D3, D6, B20, D4, C8, B11, J2)

```bash
cd $WORK/onemind-xr/web
ONEMIND_WORK=$WORK PYTHON=$WORK/.tools/py/bin/python npx vitest run
ONEMIND_WORK=$WORK PYTHON=$WORK/.tools/py/bin/python npx playwright test e2e/act.spec.ts
# watch it in a real browser window:
ONEMIND_WORK=$WORK PYTHON=$WORK/.tools/py/bin/python npx playwright test e2e/act.spec.ts --headed
```

Expected: `98 passed | 1 skipped` from vitest. From Playwright, `12 passed`, or 15 passed and
1 skipped when you run the whole e2e folder. In headed mode you see this sequence:

1. Sharing a purple AOI rectangle opens **Confirm action** with `zone.create (level A2)`, the
   zone parameters and `hash verified`. The zone appears only after **Confirm**. A direct,
   unsigned `cmd.map.geofence.zone-rogue.create` is refused `unsigned` and writes nothing.
2. Clicking east of home opens **Confirm action** with `asset.goto`, the exact lat/lon,
   `"hae_m":60` and a `sha256:` hash. Nothing moves until **Confirm**.
3. SIM-X1 flies to the pink marker, which turns green with `executed`.
4. Alerts lists `sim-x1 left zone-d…`.
5. A `NO FLY` circle shared as keep-out (the dialog warns it will constrain assets) makes the
   next goto inside it show `denied` with `precondition_failed (keep_out)`. Re-sharing it as an
   AOI opens the dialog with `changes a keep-out zone to aoi`; rejecting it leaves the keep-out.
   **Remove zone** opens the dialog with `removes a keep-out zone`.
5a. A hold proposed for `nonsim-x9` shows `denied` with `not_simulated` in Tasks.
6. Gaps names `nws-alerts`: `Weakens layers: alerts, hazards`.
7. Hermes answers `1 moving near NORTH FIELD: WALKER …` with a pink ring on WALKER. Its proposal
   opens a dialog marked `PROPOSED BY AN AGENT`.
8. Chat shows VIPER's reply `copy: hello from the web`. Switching the channel to Cyan hides HAWK.
9. Every drawing tool draws. CoT shows `u-d-c-c` for the circle.
10. In Newcomer mode the Go to point button is gone.
11. A second window logged in as `carol` (the omos #562 CLIENT permissions) shows
    `evt.so.action.> — action audit: not permitted` in Tasks. Its RTL still reaches `executed`
    through `svc.policy.action.get` polling alone.

## 2. On the Mac GPU: interaction and frame rate

Run the stack and keep it running with `--headed --debug`, or use the J1 manual stack
(`docs/evidence/M3/J1/OWNER-STEPS.md` §2–§5) and open
`http://127.0.0.1:4173/?nats=ws://127.0.0.1:19222`. In **Mode**, pick **Observer**, then:

1. **Draw**: draw a polygon with 6 vertices and press Finish. Expected: the yellow rubber band
   follows the pointer; once you finish, no yellow line remains.
2. Draw a sector and range rings. Expected: the labels show the radius and area. Pan and zoom
   stay smooth (no stutter while drawing).
3. Select a drawing, then **CoT** then **Download .xml**. Expected: a file
   `d….cot.xml` containing one `<event>` per shape (rings give three).

## 3. Phone over the LAN (J2 and the touch layout)

With the J1 LAN setup (`docs/evidence/M3/J1/OWNER-STEPS.md` §2–§5), open
`http://<LANIP>:4173/?nats=ws://<LANIP>:19222` on the phone and log in on the Connection tab.

1. **Mode** reads Newcomer on first visit. Tap SIM-X1. Expected: Detail says
   `Asset commands are hidden in Newcomer mode.`, and there is no **Draw** tab.
2. Switch to Operator. Expected: with the J1 fixture (no policy service) the chip reads
   `observer: …` and commands stay hidden, because there is no policy grant (fail closed).
3. The tab bar scrolls sideways and the page itself never scrolls sideways.

## 4. Real Hermes (needs the model API, owner only)

The build machine may not call a paid model. After omos #562 gives you a `CLIENT` user, connect
the client to the cluster bus over the tailnet:
`?nats=wss://<nats-websocket-on-tailnet>&hermes=agents.prompt.hermes.zeus.legacy`.

1. Select an area with moving tracks and ask **What's moving near the north field?** (any zone
   name you have). Expected: a text answer that names entities from the map context. Hermes'
   `cmd.map.highlight.*` rings appear on those entities, and the Hermes tab logs
   `Map: highlighted …`.
2. Ask **Send sim-x1 to <zone>**. Expected: Hermes proposes through `svc.policy.command.propose`;
   a **Proposal … review** button appears; the dialog says `PROPOSED BY AN AGENT`. Nothing
   moves until you confirm.
3. Record the reply framing (one message, or chunks?) in WEB-P6: run
   `nats req agents.prompt.hermes.zeus.legacy "ping" --replies=0 --timeout=30s`.

## 5. ATAK chat both ways (C8: needs an ATAK device and a `svc.cot.send` server)

Blocked until a projector serves `svc.cot.send` (WEB-P4), and until the CLIENT account may read
chat (WEB-P2 c). When both exist:

1. Start the mesh gateway (omos #551) and the projector on the Mac, with ATAK on the same Wi-Fi.
2. In the web client **Chat**, set Callsign `ZEUS-WEB` and send `hello from the web` to
   All Chat Rooms. Expected: ATAK shows it in All Chat Rooms from ZEUS-WEB.
3. Reply from ATAK. Expected: the reply appears in the web client within 2 s, and ATAK
   appears in **Contacts** with its team colour as the channel.
4. Switch the channel to another team colour. Expected: the ATAK contact and its group room
   disappear from the lists.

## 6. Drawings on ATAK (B11) and AOIs changing what TAK receives (D1)

1. B11: send the exported `.cot.xml` events to TAK (local TAK Server or the gateway:
   `nats pub tak.cot.mesh.tx "$(sed -n 1p d….cot.xml)"` on the Mac's local bus). Expected: ATAK
   draws each shape with the same vertices (compare with `b11-cot-export.xml` from this run).
   Record any shape ATAK does not draw (WEB-P8).
2. D1: with the A6 projector (omos #563) reading KV `geofences`, the geofence service (omos #569)
   and the policy service carrying D1-P4, draw an AOI around one sim entity and **Share as zone**,
   then **Confirm**. Expected: TAK starts receiving that entity. It stops after **Remove zone**
   and **Confirm**. Before you confirm, TAK receives nothing new.
3. B11 geometry on ATAK: for the circle, polygon and rectangle from step 1, compare the vertex
   coordinates ATAK shows (long-press the shape, then Details) with the `<link point=…>` and
   `<ellipse>` values in `b11-cot-export.xml`. Expected: the same to 6 decimal places, and the
   circle radius within 1 m.

## 7. The real geofence service (omos #569) instead of the stand-in (D1)

The suite's geofence writer is a stand-in that applies the same gate as #569 (policy signature,
`expires_at`, `approval.status`). To check against #569 itself on the Mac:

```bash
cd $WORK/omos && git worktree add $WORK/wt/d1-geofence origin/feat/geofence-alerts
cd $WORK/wt/d1-geofence/services/geofence
NATS_SERVER_BIN=$(command -v nats-server) go test -race -count=1 ./...
```

Expected: the tests pass, including the command gate refusing unsigned, wrongly signed, expired
and unapproved commands. Then run the J1 manual stack with the policy service built from
`$WORK/wt/i4-policy` plus `onemind-xr/web/test/standins/policy-zone-actions.patch`, and with
`geofence-service` (`POLICY_KEYS_DIR` holding the policy public key as `{kid}.pem`). Share an AOI
in the web client and confirm it. Expected: `nats kv get geofences zone-d…` shows the zone, and
`evt.map.geofence.created` carries the action id.

## Proposed OWNER-CHECKLIST entry (for the orchestrator)

```
### M4 WEB: Act features in the L0 web client (D1, D3, B11, B20, D4, D6, J2, C8)
- [ ] Mac: `npx playwright test e2e/act.spec.ts --headed` in onemind-xr/web passes 12/12
      (docs/evidence/M4/WEB/OWNER-STEPS.md §1)
- [ ] Mac GPU: drawing tools smooth, no leftover preview line (§2)
- [ ] Phone: Newcomer has no task button and no Draw tab; no sideways scroll (§3)
- [ ] Real Hermes: "What's moving near <zone>?" highlights entities; a Hermes proposal waits for
      confirm (§4); record the reply framing for WEB-P6
- [ ] ATAK: GeoChat both ways and channel switching (§5; blocked on svc.cot.send, WEB-P2/P4)
- [ ] ATAK: exported shapes match; an AOI changes what TAK receives only after its confirm
      (§6; needs #563, #569 and D1-P4 in #554)
- [ ] Owner decision: adopt #569 D1-P4 (zone.* in the #554 catalogue); until then no zone can be
      written from the client in the cluster (OWNER-PROPOSALS WEB-P1)
- [ ] Real #569 geofence service applies a zone the client proposed and a human confirmed (§7)
```
