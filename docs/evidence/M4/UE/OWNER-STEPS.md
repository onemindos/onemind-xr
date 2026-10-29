# M4 lane UE: owner steps (Unreal build and on-device checks)

Lane UE, branch `run/m4-client` (stacked on `run/m2-client`, onemind-xr PR #3), features **B11,
B20, C3, C7, C8, D3**. The engine-independent core for all six is built and tested on the build
machine (`EVIDENCE.md` in this folder). The Unreal code (OneMindCoT additions, the new
OneMindMission plugin, the gaps panel in OneMindLayers, the overlay's Act panel and the player
controller keys) is **written but not compiled**: Unreal is not installed on the build machine
(`docs/PREFLIGHT.md`). Every feature below is **built, awaiting owner check**.

All commands run on the owner's Apple Silicon Mac from the repository root, with Unreal 5.8 at
`/Users/Shared/Epic Games/UE_5.8` (as `client/OneMind/Tools/mac/build-and-run.sh` assumes). Do the
M2 steps first (S1, S5, the 10,000-entity check, C1, C2 in
`docs/evidence/M2/B/OWNER-STEPS.md`): they set up the local leaf, the mesh gateway and the app.

Nothing here needs a secret in a file. The TAK user's password is read from the environment
variable `ONEMIND_TAK_ENROLL_PASSWORD` once, and the enrolled key and certificate go to the login
keychain (service "OneMind TAK").

---

## UE-1. Build the M4 code (compile check)

```bash
git fetch origin && git checkout run/m4-client
cd client/OneMind
UE="/Users/Shared/Epic Games/UE_5.8" Tools/mac/build-and-run.sh check
UE="/Users/Shared/Epic Games/UE_5.8" Tools/mac/build-and-run.sh build 2>&1 | tee ~/onemind-m4-build.log
grep -E "error|warning: .*OneMind" ~/onemind-m4-build.log | head -50
```

Expected: the build ends with `Result: Succeeded`, and the new modules appear in the log:
`OneMindMission`, plus `OneMindCoT` compiling `OneMindTakSubsystem.cpp`, `OneMindUeTlsTransport.cpp`,
`OneMindKeychain.cpp` and `Tls/OmTlsSession.cpp`.

Things the build machine could not prove, most likely first if it fails:

1. `AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenSSL")` in
   `Plugins/OneMindCoT/Source/OneMindCoT/OneMindCoT.Build.cs` links the engine's OpenSSL on Mac.
   If the linker reports missing `SSL_*` or `X509_*` symbols, check
   `Engine/Source/ThirdParty/OpenSSL/OpenSSL.Build.cs` for the Mac library names and report them.
   `OmTlsSession.cpp` uses only APIs common to OpenSSL 1.1.1 and 3.x.
2. `PublicFrameworks.Add("Security")` for the keychain (Mac only).
3. UHT: `FOneMindTakEnrollPlatform` is forward-declared at file scope in `OneMindTakSubsystem.h`
   and held in a `std::shared_ptr`; the subsystems hold `std::unique_ptr` to core types.
4. `ISocketSubsystem::GetAddressInfo` / `FSocket::Wait` signatures in UE 5.8
   (`OneMindUeTlsTransport.cpp`).

OpenSSL licence check (decision UE-P6 below): report the bundled version.

```bash
ls "/Users/Shared/Epic Games/UE_5.8/Engine/Source/ThirdParty/OpenSSL"
grep -rh "define OPENSSL_VERSION_TEXT" "/Users/Shared/Epic Games/UE_5.8/Engine/Source/ThirdParty/OpenSSL" | sort -u | head
```

Expected: a version directory and a line such as `OpenSSL 1.1.1t  7 Feb 2023` or `OpenSSL 3.x.y`.
If it is 1.1.1, record the UE-P6 decision before distributing a build (`client/OneMind/THIRD_PARTY.md`).

If it fails, save `~/onemind-m4-build.log` and report the first 50 error lines; the host tests
already cover the logic, so fixes should be mechanical.

## UE-2. Package and run

```bash
UE="/Users/Shared/Epic Games/UE_5.8" Tools/mac/build-and-run.sh package
UE="/Users/Shared/Epic Games/UE_5.8" Tools/mac/build-and-run.sh run
```

Expected: the app starts on the globe as in M2, with a new panel at the bottom left. With a feed
configured but not running it reads `INTELLIGENCE GAPS (1)  MISSING nws-alerts: never heard
weakens hazards  alerts severe_weather` (B20 default config in `Config/DefaultGame.ini`), then
`TAK: not enrolled`.

## UE-3 (D3). Click-to-task with a human confirm, on the local stack

Needs: the local leaf with the per-identity layout (omos PR #562, or the account block used by
`client/OneMind/HostTests/scripts/e2e_d3_policy.py`), the policy service (omos PR #554) and the
MAVLink bridge (PR #550) with the scripted `sim-x1` drone (`scripts/sim/mavlink_sim_drone.py`).
The client user (`zeus` in the CLIENT account) must be able to subscribe `evt.so.action.>`
(proposal UE-P3 below).

1. In the app, select `sim-x1` in the Fleet layer (left click), point the cursor at a spot on the
   map inside the home keep-in zone, press **T**.
2. Expected: the Act panel shows
   `TASK goto sim-x1 [A3] awaiting_confirmation`, the exact `params {"hae_m":…,"lat":…,"lon":…}`,
   the first 23 characters of the hash, `Y = confirm, N = reject (… s left)`, and the line
   `hae_m = surface under cursor S m HAE + 120 m above it`. `hae_m` is height above the WGS84
   ellipsoid: S plus `TaskHeightAboveGroundMeters` (`Config/DefaultGame.ini`
   `[/Script/OneMind.OneMindPlayerController]`, default 120). With the default ellipsoid globe (no
   terrain tileset) S is about 0, so the goto is 120 m HAE, which is below ground near Austin
   (about 120-170 m HAE there); harmless for `sim-x1`, but configure a terrain tileset or a
   larger height for a realistic D3 demo. `sim-x1` does **not** move.
   Press Y immediately as the dialog appears: expected log `confirm …: the parameters were shown
   for less than 500 ms` and nothing on `cmd.fleet.>`; Y after half a second is accepted.
3. In a terminal: `nats --server nats://127.0.0.1:4222 sub 'cmd.fleet.>'` shows nothing yet.
4. Press **Y**. Expected: the panel's audit line becomes
   `proposed > approved > dispatched > acknowledged > executed`, one message appears on
   `cmd.fleet.drone.sim-x1.goto`, and `sim-x1` flies to the point (its entity moves on the map).
5. Repeat with the cursor inside the pond keep-out zone: expected `denied`,
   `precondition_failed`, nothing on `cmd.fleet.>`.
6. Repeat and press **N**: expected `rejected`, nothing on `cmd.fleet.>`.
7. Console: `OneMindTask x1 drone 30.27 -97.74 120`. Expected: refused at once with
   `not_simulated` (the client never sends it).

## UE-4 (C3). Enroll and become a TAK contact

Needs a TAK user for this client on TAK Server (owner action: create user `onemind-mac` in the TAK
Server user manager, group `__ANON__` or the team's group) and the tailnet (`tak.tail717ba0.ts.net`
exposes 8089 and 8446, omos `CLAUDE.md` "TAK Server"). Trust for enrollment: the TAK Server CA
**must** be in the keychain first. Without it the client refuses to enroll (log
`TAK enrollment refused: no TAK Server CA in keychain (server-ca)`, nothing is sent, the password
never leaves the Mac). The CA chain the server returns is not stored; `server-ca` stays the CA you
loaded, and 8089 is verified against it too. Get `takserver-ca.pem` from the TAK Server's
certificate directory over a trusted path (not by downloading it from the server you are about
to trust), and compare its fingerprint with the server's before loading it:

```bash
openssl x509 -in ~/Downloads/takserver-ca.pem -noout -fingerprint -sha256   # compare with the server's CA
security add-generic-password -U -s "OneMind TAK" -a server-ca -w "$(cat ~/Downloads/takserver-ca.pem)"
security find-generic-password -s "OneMind TAK" -a server-ca -w | head -1   # expect -----BEGIN CERTIFICATE-----
export ONEMIND_TAK_ENROLL_PASSWORD='<TAK user password>'   # in the shell that starts the app
UE="/Users/Shared/Epic Games/UE_5.8" client/OneMind/Tools/mac/build-and-run.sh run
```

In the app console (backquote key):

```
OneMindTakEnroll onemind-mac
OneMindTakConnect
```

Expected:
- log line `TAK enrollment: ok`; `security find-generic-password -s "OneMind TAK" -a client-cert`
  finds the item (the key is under `-a client-key`);
- the Act panel shows `TAK: connected v1 5.8-RELEASE-69, N contacts` (or `XML` if the server does
  not offer v1);
- **on an ATAK device connected to the same TAK Server, the contact list shows `ONEMIND`** (team
  Cyan) at the camera's position. This is the FEATURES C3 owner check.

Afterwards `unset ONEMIND_TAK_ENROLL_PASSWORD`. To remove the identity:
`security delete-generic-password -s "OneMind TAK" -a client-key` (and `client-cert`, `server-ca`).

## UE-5 (C8). Chat both ways and channels

With UE-4 connected and an ATAK user `ALPHA` on the server:

1. On ATAK, send a direct chat to `ONEMIND`. Expected: it appears in the client
   (`GetConversation(<ALPHA uid>)`; the log shows the message).
2. In the client console: `OneMindTakChat <ALPHA uid> copy that` (the uid is shown in the contact
   list). Expected: ALPHA sees it in the chat with `ONEMIND`.
3. `OneMindTakChat All hello all`. Expected: every ATAK user sees it in "All Chat Rooms", and the
   last three All Chat Rooms lines show in the Act panel.
4. `OneMindChannel Red` while ALPHA is Cyan. Expected: ALPHA disappears from the client's
   contact list; `OneMindChannel` (no argument) brings it back.
   Unverified on a live server: the direct-chat element values (`chatroom` = callsign, `id` = uid,
   `<marti><dest callsign>`) and the server-side active-group change (`PUT
   /Marti/api/groups/active`), which the client does not send yet.

Mesh variant (no server), with the mesh gateway from M2 on the ATAK Wi-Fi: send chat from ATAK
to "All Chat Rooms"; it arrives on `tak.cot.mesh.rx`; a chat sent with `svc.cot.send` kind `chat`
reaches ATAK. This path was proven on the build machine with the gateway's directed TCP
(`e2e-c3/c3-output.txt`, part A).

## UE-6 (B11). Shapes on ATAK with the same geometry

Shapes go through the projector's `svc.cot.send` (proposal UE-P1), so this needs the projector
serving kind `shape` (or the mesh gateway path). The copy over the client's own TAK stream is off
(`bSendShapesOnOwnStream=False`) until the owner ratifies UE-P5; to test it anyway on a test
server, set `bSendShapesOnOwnStream=True` under `[/Script/OneMindCoT.OneMindTakSettings]`.

1. Press **B** until the panel shows `DRAW circle`, click a centre and a point on the edge, press
   Enter. Do the same for rectangle (2 corners), polygon (3+ points), line (2+ points), sector
   (centre, start, end), range rings (centre, outer edge: three rings), range and bearing (from,
   to) and buffer (a line; 100 m).
2. Expected on ATAK: each shape appears with the same vertices, radius and label; colours yellow
   (circle, polygon, rectangle), red (rings, R&B), orange (buffer).
3. Record which shapes match. Unverified until this check: whether ATAK's `ellipse major/minor`
   are radii (the client sends the radius), and whether an open `u-d-f` (line) shows as a line.

## UE-7 (C7). Data Sync missions

Needs tak-client to serve `svc.tak.mission.*` (proposal UE-P2; not implemented in PR #560 yet).
Until then this step cannot run. When it can:

1. ATAK: create mission `patrol-1` in Data Sync and add a marker.
2. Client console: `OneMindMissionList`, then `OneMindMissionOpen patrol-1`. Expected: the Act
   panel shows `MISSION patrol-1: 1 items` with the marker's callsign.
3. ATAK adds a second marker. Expected: the count goes to 2 within a few seconds.
4. Client: point at the map, `OneMindMissionAdd patrol-1 Water-point`. Expected: ATAK shows the
   marker in the mission.

## UE-8 (B20). Gaps panel with a real feed

With omos-ingest (PR #553) running against the local leaf with `nws-alerts`:

1. Expected: the panel shows no gap for `nws-alerts` once its first heartbeat arrives.
2. Stop omos-ingest. Expected: within the feed's `staleAfter` (plus 3 heartbeats + 5 s at most)
   the panel shows `STALE` or `MISSING nws-alerts … weakens hazards alerts severe_weather`.
3. Start it again: the line disappears.

---

## Proposed OWNER-CHECKLIST.md text (for the orchestrator)

- [ ] **UE-1** M4 client compiles in UE 5.8 on the Mac (OpenSSL, Security framework, new
      OneMindMission plugin), and the bundled OpenSSL version is reported. Command and expected
      output: `docs/evidence/M4/UE/OWNER-STEPS.md` UE-1.
- [ ] **UE-P5** Decide the own-TAK-stream exception in `cot-projection.md` (SA + chat only).
- [ ] **UE-P6** If UE 5.8's OpenSSL is 1.1.1: linking exception or OpenSSL 3.x.
- [ ] **D3** `sim-x1` flies to a clicked point only after pressing Y in the dialog; N and
      keep-out are refused; nothing on `cmd.fleet.>` before Y. (UE-3)
- [ ] **C3** With the TAK Server CA loaded in the keychain, `OneMindTakEnroll` + `OneMindTakConnect`;
      ATAK's contact list shows ONEMIND. Without the CA, enrollment is refused. (UE-4)
- [ ] **C8** Chat ONEMIND ↔ ATAK both ways; channel switch hides other teams. (UE-5)
- [ ] **B11** Each drawn shape appears on ATAK with the same geometry. (UE-6)
- [ ] **C7** A Data Sync feature added in ATAK appears in the client and the reverse (after
      tak-client implements UE-P2). (UE-7)
- [ ] **B20** Stopping omos-ingest shows `nws-alerts` in the gaps panel naming the hazards layer. (UE-8)

## Owner proposals from this lane

These are to be added to the omos grammar register (PR #556) by reference; this lane does not
edit that PR.

| ID | Decision asked | Where | Blocks |
|---|---|---|---|
| UE-P1 | `svc.cot.send` gains kind `shape` (B11 types `b-m-p-s-m`, `u-d-f`, `u-d-r`, `u-d-c-c`, `u-r-b-c-c`, `u-rb-a`) | `client/OneMind/Docs/svc-cot-send.md` (extends the C2 proposal) | B11 through the projector (the client also sends shapes over its own TAK stream, C3) |
| UE-P2 | tak-client serves `svc.tak.mission.{list,get,changes,logs,subscribe,unsubscribe,content.put}` and publishes `evt.mission.tak-{token}.changed` | `client/OneMind/Docs/svc-tak-mission.md` | C7 live check |
| UE-P3 | CLIENT account (A19, PR #562) imports streams `evt.so.action.>`, `evt.mission.*.changed`, `tel.feed.*.status` and service `svc.tak.mission.>`; the client users may subscribe them | `client/OneMind/Docs/svc-tak-mission.md` "Client permissions" | D3 outcome display, C7, B20 on the live layout |
| UE-P4 | One TAK user per OneMind client device, enrolled through `/Marti/api/tls/signClient/v2` on 8446, key in the device keychain; the TAK Server CA is provisioned by the owner (keychain `server-ca`), never learned from the server | this file, UE-4 | C3 live check |
| UE-P5 | Amend `docs/contracts/cot-projection.md` §1 "Who may publish": the client's own TAK stream (C3) is a named exception limited to its own SA and human-typed direct/group chat; shapes and everything else go through `svc.cot.send`. Open question: must G7 marking also cover chat (then chat moves to `svc.cot.send`)? | `client/OneMind/Docs/tak-own-stream.md` | C3/C8 as built; until ratified, shapes use the projector only (`bSendShapesOnOwnStream` off) |
| UE-P6 | If UE 5.8 bundles OpenSSL 1.1.1 (UE-1 check): add an OpenSSL linking exception to the client's GPL-3.0-or-later licence, or link an OpenSSL 3.x (Apache-2.0) build instead | `client/OneMind/THIRD_PARTY.md` | distributing an Unreal build with C3 |

Register lines (the format `scripts/check_proposal_subjects.py` reads in PR #556):

```subjects
proposed  svc.tak.mission.list
proposed  svc.tak.mission.get
proposed  svc.tak.mission.changes
proposed  svc.tak.mission.logs
proposed  svc.tak.mission.subscribe
proposed  svc.tak.mission.unsubscribe
proposed  svc.tak.mission.content.put
proposed  evt.mission.{tak-token}.changed
```
