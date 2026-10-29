# WEB (M4 Act in the L0 web client): owner proposals

Decision 0004 #8: no new subject pattern was invented. Every item below uses an existing grammar
pattern. Each item names the gap, the evidence, and what the client does until it is decided.
For the master register (omos #556, `docs/proposals/2026-10-29-grammar-register/`): append
WEB-P1 to WEB-P9 by reference to this file. The related register entries are named in each item.

| ID | Gap | Evidence | Proposal | Client today |
|---|---|---|---|---|
| WEB-P1 | **Zone writes go through policy (A2).** ONTOLOGY §6.2 lists `zone.create/update/remove` at A2, executed as `cmd.map.geofence.{id}.{verb}`. omos #569 (geofence service) applies only commands carrying a valid policy `sig`, a future `expires_at` and `approval.status: approved`, and defines the params in `services/schemas/actions/zone.{create,update,remove}.v1.schema.json`. The #554 catalogue has no `zone.*` entries yet (#569 owner proposal D1-P4) | `src/ui/act.ts` `shareZone`/`unshareZone`; `src/act/drawing.ts` `zoneParams`; `src/act/policy.ts` `zoneRequest`; register D1-P4, POL-9, A6-P2 | Adopt D1-P4 in #554 (patch text in #569's proposal doc; the exact patch the e2e builds with is `web/test/standins/policy-zone-actions.patch`). All five kinds are A2, because an AOI changes what TAK receives (A6) and any `cmd.map.>` holder could otherwise loosen a constraint without an audit record. Params: `{name, geo: {shape: polygon\|circle, kind, coords: GeoJSON, radius_m?}, watch, applies_to?}`. `geo.shape` is only `polygon` or `circle`, because the policy service fails closed on any other keep-in or keep-out shape | Proposes `zone.create`, `zone.update` or `zone.remove` on `svc.policy.command.propose` with target `{entity_type: zone, entity_id}`. The existing confirm dialog confirms every change and warns when a change creates, loosens or removes a keep-in or keep-out. The client publishes no `cmd.*` subject. Rectangles, sectors and buffers become `polygon`. **Works against #554 only with D1-P4 applied** |
| WEB-P2 | **CLIENT account subscriptions.** omos #562 lets a `CLIENT` user subscribe only `cmd.map.>` and `_INBOX.>`. So a browser cannot see `evt.so.action.*` (live action status), `tel.feed.*.status` (B20) or GeoChat on `tak.cot.out` / `tak.cot.mesh.rx` (C8) | `values.identities.stage-a1.template.yaml` CLIENT block in omos #562; `src/ui/act.ts` `attach()` | (a) Action status: keep polling `svc.policy.action.get` (works today), or add a CLIENT stream import of `evt.so.action.>` filtered to the caller's actions (needs a per-user transform; **unverified**). (b) Feed status: a read-only CLIENT stream import of `tel.feed.>` (small, no sensitive data). (c) Chat: either a CLIENT import of `tak.cot.out` and `tak.cot.mesh.rx` for authenticated humans (this exposes all TAK CoT, like the `tak` layer), or a request/reply `svc.cot.chat` history endpoint served by the projector (pattern `svc.{service}.{endpoint}`) | Subscribes. When refused, falls back to `action.get` polling and layer-manifest freshness, and shows which subscriptions were refused (Tasks tab). Chat receive needs (c). The e2e user `carol` has exactly the #562 CLIENT permissions: probe, propose, confirm and status by polling alone pass |
| WEB-P3 | **Policy dry run checks preconditions before authorization.** `svc.policy.check {v, action}` runs `checkAsset` (online, geofence) before `authorizePropose` (omos #554 `service.go`, `propose`). A dry run of `goto` on an offline asset never says whether the caller is an operator | `src/act/policy.ts` `roleProbe` | Either authorize first in dry runs, or add `{v, can: {action_type, target}}` to `svc.policy.check` that answers only the OpenFGA question for the caller's verified identity | Probes with a dry run of `asset.estop` (unblockable, so it skips asset preconditions). Any answer other than allowed or `not_authorized` keeps commands hidden (fail closed) |
| WEB-P4 | **`svc.cot.send` body.** cot-projection.md §9 leaves the body to FEATURES C2, and no projector serves the endpoint yet (searched omos #560, #563) | `src/act/chat.ts` `geochatRequest`; `test/standins/stack.ts` `TakStandin` | GeoChat: `{v: 1, kind: "geochat", target: server\|mesh\|both, message_uid, room: {id, name, kind}, text (≤ 1000), sender: {uid, callsign}}`. Reply `{ok: true, uid}` or `{ok: false, error: {code, message}}`. The projector builds the CoT itself, so the client never sends XML. It must take the sender's identity from the connection; the body's `sender` is the display callsign only | Sends this body; a test stand-in answers |
| WEB-P5 | **Entity records carry no `capabilities`.** The policy service denies an asset verb unless the target entity lists the matching task type (`not_capable`). The entity service in omos #552 never sets `capabilities` (`grep -ri capabilit services/entity` returns nothing), but the MAVLink bridge in omos #550 publishes `commands_accepted` in its status meta. With #552 and #554 as they are, **every goto is denied** | `stack.ts` header; bridge `telemetry.go` `statusEnvelope` | #552 maps the bridge's `commands_accepted` (or a registry entry) to `capabilities: ["onemind.task.goto.v1", …]` on fleet entities | E2E uses an entity stand-in that sets them. A cross-PR finding for #552, #550 and #554 |
| WEB-P6 | **Client↔Hermes prompt framing.** The gateway takes a text prompt (`nats req agents.prompt.hermes.zeus.legacy "<task>"`, omos `platform/hermes/agent/FABRIC-REFERENCE.md`). Its reply framing (one reply or chunks) is **unverified** | `src/act/hermes.ts` | Prompt = the question, then a `[map-context v1]` JSON block (selection, viewport, layers, ≤ 50 visible entities, zones, gaps). Reply = a JSON `{text, proposals: [action_id]}`, or plain text in one or more chunks ending with an empty message. Ratify with the I2 Hermes tools lane | Accepts both reply forms. The real gateway is an owner check (OWNER-STEPS §4) |
| WEB-P7 | **`evt.map.geofence.{id}.{enter\|exit}` payload.** Same as J1-P3 | J1 OWNER-PROPOSALS | `{entity_id, zone_id, t}` | Reads it (the test stand-in publishes it) |
| WEB-P8 | **CoT types for shapes the contract does not map.** cot-projection.md §5.5 has no type for a sector, range rings or range-and-bearing, and its `<ellipse>` attributes and `u-d-r` corner order are **unverified** (§12) | `src/act/cot.ts` header; `b11-cot-export.xml` | Read the atak-civ `takcot` drawing-shape and range-and-bearing XSDs (tak-protocols.md §1) and add native types if ATAK has them | Uses only contract types: sector and buffer as closed `u-d-f`, rings as one `u-d-c-c` per ring, range and bearing as a two-point `u-d-f` with range and bearing in `<remarks>` |
| WEB-P9 | **Persistent drawings.** cot-projection.md §9 says markers, routes and geofences are created through `entity.create` in policy. The #554 catalogue has no `entity.create` | `catalogue.go` in omos #554 | Add `entity.create` (A2) for `place` and route `zone` records, or state that drawings stay client-side until shared as zones or CoT | Drawings live in the page until shared as a zone (WEB-P1) or exported as CoT |

## Integration findings (bugs found while wiring real services)

- omos #550 and the scripted drone: the drone accepts `DO_REPOSITION` only when armed and
  airborne, and `arm` and `takeoff` are A4 (two humans). The e2e setup arms and takes off through
  the policy service with two users (`zeus` step 1, `alice` step 2), as an operator pair would.
  The trail is in `act-transcript.json` `setup`.
- A start-up race: the bridge refuses a command (`precondition_failed`, "no HEARTBEAT from sysid
  1 within 3s") if it arrives before the bridge has seen a heartbeat, even though position
  telemetry is already flowing. An entity should report `health.connection` only from the
  bridge's status, never default it to online. The stand-in now does that (WEB-P5 context).
- A refused subscription made a browser connection deaf (fixed in this PR). nats-core 3.4.0 passes a
  Permissions Violation to the subscription callback with an empty message. The J1 callbacks for
  `evt.map.geofence.*.*` (`main.ts`) and `ent.*.*.state` (`layerclient.ts`) read the message, and
  `alertFromGeofence(undefined)` threw inside nats-core's read loop. After that the connection
  processed no inbound data, including request replies, so a #562 CLIENT user's policy check
  timed out. The server's monitor showed the replies delivered to the inbox (`msgs: 5`) while the
  client's `inMsgs` stood still. All four callbacks now return on an error. The earlier e2e
  accounts could subscribe everything, so they never hit it.
- omos #550's MAVLink bridge accepts `goto,rtl,land,arm,disarm,estop,takeoff` (status meta
  `commands_accepted`), not `hold`; a `hold` reaches it and is refused `unknown_action`. The entity
  stand-in had listed `onemind.task.hold.v1`; it now matches the bridge, so the policy denies
  `hold` `not_capable`. The client still offers a Hold button (follow-up below).
- omos #554 has no `zone.*` catalogue entries (D1-P4), so in the cluster no zone can be written
  through the client until #554 carries them. The e2e builds #554 with the D1-P4 patch.

## Follow-ups (not done here)

- Offer Hold, RTL and the other verbs only when the selected asset's entity lists the matching
  capability (`capabilities`, WEB-P5). Today the buttons show and the policy denies `not_capable`.
- J1 still subscribes `ent.*.*.state` on the interim path, which a #562 CLIENT user may not do. A
  #562 user sees entities only through `svc.layer.*` (omos #561).

