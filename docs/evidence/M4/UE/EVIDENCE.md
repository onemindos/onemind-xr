# M4 lane UE: evidence (B11, B20, C3, C7, C8, D3 in the Unreal client)

Branch `run/m4-client` (onemind-xr), stacked on `run/m2-client` (PR #3). Build machine: macOS
arm64, Apple clang, CMake 4.4.3, Homebrew OpenSSL 3, nats-server from `.tools`, Go 1.27.3,
Python venv with `nats-py` and `takproto`. No Unreal here: the engine-side code is written and
**not compiled**; its build and on-device checks are in `OWNER-STEPS.md`.

Everything below was run on 2026-10-29 from `client/OneMind/HostTests`.

## What runs where

| Feature | Core (built and tested here) | Engine side (uncompiled) | Acceptance on this machine | Owner check |
|---|---|---|---|---|
| C3 TAK contact (8089 mTLS, enrollment) | `TakProto`, `TakStream`, `TakEnroll` (+ `HttpExchange`), `Tls/OmTlsSession` (OpenSSL) | `UOneMindTakSubsystem`, `FOneMindUeTlsTransport`, `OneMindKeychain` | passes against the mesh gateway's directed TCP (#551 built from source) and a scripted mTLS TAK-stream peer, with a certificate enrolled through a scripted `signClient/v2` | UE-4: ATAK's contact list shows the client |
| C7 Data Sync missions | `TakMissions` | `UOneMindMissionSubsystem` | both directions against a stub of the proposed tak-client RPC (UE-P2) | UE-7, after tak-client implements UE-P2 |
| C8 contacts, chat, channels | `TakContacts` | `UOneMindTakSubsystem`, player controller console | chat both ways with the TLS peer and through the gateway; channel switch hides other teams | UE-5 |
| B11 drawing | `Drawing` | `UOneMindCoTSubsystem::Send*`, player controller B/Enter | all 11 shapes decoded by the TLS peer (TAK Protocol v1 via `takproto`) with the drawn geometry; all reach `tak.cot.mesh.rx` via the gateway | UE-6 |
| B20 gaps panel | `FeedGaps` | `UOneMindLayerSubsystem::GetGaps`, overlay Act panel | a stopped `tel.feed.nws-alerts.status` heartbeat shows within its limit naming the layer | UE-8 |
| D3 click-to-task | `AssetTasking` (+ JCS) | `UOneMindMissionSubsystem`, player controller T/Y/N | **against the real omos policy service** (#554 built from source): waits for the human, one signed `cmd.fleet.drone.sim-x1.goto` after confirm, audit to `executed`; keep-out, reject, non-sim and client publish to `cmd.fleet.>` all refused | UE-3 |

## Results

### Unit tests (`om_unit_tests`)

57 tests, 57 passed, 1028 checks (C++17, `-Wall -Wextra -Wshadow -Werror -fno-exceptions
-fno-rtti`), including the 31 pre-existing M2 tests and 26 new ones (17 in `unit_tak.cpp`, 9 in
`unit_act.cpp`). Same result under AddressSanitizer + UBSan and in the C++20 build. Output:
`full-run.txt`.

New unit coverage: v1 varint and framing (the `bf9602` header and the TakControl bytes match the
M0 `takproto` spike), CoT XML <-> TakMessage round trip with typed fields, the whole-element rule,
receiver rule 5 (xmlDetail wins), XML stream splitting across reads, pings at 15 s / 4.5 s and
reset at 25 s, v1 negotiation with the answer and the first v1 frame in one read, decline, no
answer (disconnect, XML afterwards), bounded outgoing queue with age limit, oversize input,
directed URLs and one-shot sends, the self SA as a contact card, `tls/config` and `signClient/v2`
parsing (JSON and XML, bare and PEM bodies), the enrollment flow (URLs, CN + nameEntries subject,
CSR body without banners, Basic auth), `HttpExchange` (Content-Length, chunked, timeout),
contacts/channels/expiry, chat both ways with dedup; every B11 shape through TAK Protocol v1 and
back with the same geometry, buffer and lasso; gap states (failed, stale by age, missing by
silence) and layer health; JCS numbers and `params_hash` equal to the policy service's `jcs.go`
(`jcs-golden.txt`); tasking pre-checks; mission tokens and record parsing.

### Integration tests (`om_integration_tests`, disposable nats-server over WebSocket)

12 tests, 12 passed (9 M2 + 3 new in `it_act.cpp`), also under ASan/UBSan:
- `ItMissionsBothWays`: an ATAK-side addition (stub tak-client relaying `t-x-m-c`) appears in the
  client's mission view; a marker the client publishes appears in another client's view; hostile
  markers never leave; OneMind's own `evt.mission.msn-*` facts are ignored.
- `ItFeedGapStoppedFeed`: heartbeats every 200 ms with a 1 s limit; after the feed stops, the panel
  shows `nws-alerts stale (data 1 s old (limit 1 s)), weakens layer weather` **806 ms** later
  (limit 1 s), and clears when the feed returns.
- `ItTaskingStubPolicy`: no confirm before the dialog was shown, a policy reply whose hash differs
  from the client's is refused (`hash_mismatch`), events that arrive before the propose reply are
  kept, a second confirm is refused, a non-sim id never reaches policy.

### C3 / C8 / B11 end to end (`scripts/e2e_c3_tak.py`): 32/32 checks, run twice

Output `e2e-c3/c3-output.txt`, full transcript `e2e-c3/c3-transcript.json`, gateway log
`e2e-c3/c3-gateway.log`.
- **A. Mesh gateway** (omos `feat/tak-mesh-gateway` @ `bef11eb1`, built from source; JetStream
  `COT` stream as in `cot.yaml`): the client's SA (contact card with its own
  `127.0.0.1:<port>:tcp` endpoint), a chat and all 11 shapes arrive on `tak.cot.mesh.rx` with
  `Tak-Delivery: direct` and `Tak-Proto: 1` (v1) or `0` (XML); a chat published on
  `tak.cot.mesh.tx` with `Tak-Dest-Uid` reaches the client's own endpoint.
- **B. Enrollment** against a scripted `/Marti/api/tls/config` + `/Marti/api/tls/signClient/v2`
  over HTTPS (per-run test CA, Basic auth): the CSR subject is `CN=omclient, O=OneMind-Test,
  OU=TAK` with a valid self-signature; `clientUid` is passed; the key file is `0600`; a wrong
  password and an untrusted server are refused.
- **C. mTLS TAK-stream peer** (written in the script with `takproto` for v1): with the enrolled
  certificate the server sees `CN=omclient`; TAK Protocol v1 is negotiated; the server version is
  read from the offer; the client's SA reaches the server (it is a TAK contact); the server's user
  appears in the client's contact list; chat both ways (direct, with `marti/dest`); pings answered;
  all 11 shapes decoded by the peer with the drawn vertices, radius, range and bearing. An
  XML-only server keeps XML; channel `Red` hides the Cyan user; a certificate from another CA gets
  no session; a server signed by an unknown CA is refused by the client.

### D3 end to end (`scripts/e2e_d3_policy.py`): 15/15 checks

Output `e2e-d3/d3-output.txt`, transcript `e2e-d3/d3-transcript.json`, policy log (passwords
masked) `e2e-d3/d3-policy.log`. omos `feat/policy-service` @ `66900872` built from source and run
with local authz on a disposable nats-server using the per-identity account layout of its own test
harness (ONEMIND, CLIENT with a `share: true` service import) plus a stream export of
`evt.so.action.>` to CLIENT (proposal UE-P3). Stub entity service and stub bridge.
1. The proposal waits (`awaiting_confirmation`, `A3`); the client's `params_hash` equals the
   policy service's; nothing on `cmd.fleet.>` for the 1.5 s before the human confirms.
2. After confirm: exactly one `cmd.fleet.drone.sim-x1.goto` carrying the clicked point
   (`30.2751235, -97.7398765`), `approver: user:zeus`, the same `params_hash`, `simulated: true`;
   the client follows `proposed > approved > dispatched > acknowledged > executed`.
3. Keep-out goto: `denied`, `precondition_failed`, nothing dispatched.
4. Human rejection: `rejected`, nothing dispatched.
5. A non-sim id proposed by hand: `not_simulated` from policy.
6. The client's NATS user publishing `cmd.fleet.drone.sim-x1.goto`: server permissions violation.

The first D3 run found a client bug (14/15): the policy service publishes
`evt.so.action.proposed` before it replies, and the client dropped events for an action id it did
not know yet. Fixed (early events are buffered and replayed), covered by `ItTaskingStubPolicy`.

## Full suite

`scripts/run-host-tests.sh` output: `full-run.txt` (see the summary at its end).

## Not verified here

- That the engine-side code compiles in UE 5.8 (OpenSSL linking, Security framework, UHT): UE-1.
- ATAK's rendering of the shapes, the direct-chat element values, the client in ATAK's contact
  list, active-group switching on a live TAK Server (UE-4 to UE-6).
- TAK Server 5.8 behaviour of `tls/config`, `signClient/v2` and the stream negotiation (5.8
  source is not public; the client follows the spec and 5.7 source).
- C7 against a real tak-client: the RPC is a proposal (UE-P2); tak-client 0.x has no missions.
- B20 against the live omos-ingest (the heartbeat format is taken from its `status.go`).
