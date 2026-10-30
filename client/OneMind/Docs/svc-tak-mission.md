# TAK Data Sync missions through tak-client (proposed, feature C7)

Status: **proposed** by lane UE (M4) on 2026-10-29, owner proposals **UE-P2** and **UE-P3**.
Nothing in omos serves these subjects yet. omos tak-client (A21, PR #560) holds the TAK identity
and states "Not in this version: Data Sync missions (FEATURES C7)"
(`services/bridge/bridges/tak-client/README.md` on branch `feat/tak-server-leg`). The client side
is built and tested against a stub that follows this page (`HostTests/tests/it_act.cpp`,
`ItMissionsBothWays`). When tak-client implements it, the omos file that defines it becomes the
source of truth and this page links to it. Per the run rules these entries belong in the omos
grammar register (PR #556) by reference; that PR is not this lane's to edit, so the register
text is in `docs/evidence/M4/UE/OWNER-STEPS.md`.

## Why through tak-client

The client could call TAK Server's Marti API itself with its own certificate (C3), but then
every client would hold mission tokens and talk to 8443 directly, and the Quest and browser
clients cannot. tak-client already runs next to TAK Server with a TAK identity, reads the
stream (where TAK Server pushes mission change CoT `t-x-m-c`, tak-protocols.md §5) and can map
these requests onto `MissionApi` (`/Marti/api/missions/{name}`, `/subscription`, `/changes`,
`/contents`, logs; TAK-Product-Center/Server 5.7-RELEASE-14 `MissionApi.java`; 5.8 unverified).

## Subjects (UE-P2)

| Subject | Body | Reply | Maps to (MissionApi, 5.7) |
|---|---|---|---|
| `svc.tak.mission.list` | `{v}` | `{v, missions:[summary]}` | `GET /missions` |
| `svc.tak.mission.get` | `{v, name}` | `{v, mission}` | `GET /missions/{name}` |
| `svc.tak.mission.changes` | `{v, name, since_ms?}` | `{v, changes:[change]}` | `GET /missions/{name}/changes` |
| `svc.tak.mission.logs` | `{v, name}` | `{v, logs:[log]}` | mission log entries |
| `svc.tak.mission.subscribe` / `.unsubscribe` | `{v, name, client_uid}` | `{v, ok}` | `PUT` / `DELETE /missions/{name}/subscription` |
| `svc.tak.mission.content.put` | `{v, name, client_uid, cot}` | `{v, ok, uid}` | send the CoT to TAK, then `PUT /missions/{name}/contents` with the uid |
| `evt.mission.tak-{token}.changed` | event envelope v1, `data {name, token, change}` | — | on each `t-x-m-c*` CoT for a subscribed mission |

Errors: `{v, error: "not_found" | "forbidden" | "invalid" | "tak_unavailable"}`.

Grammar fit: `svc.{service}.{endpoint}` with service `tak` and a dotted endpoint, as
`svc.policy.command.propose` (asset-command.md §1). `svc.mission.*` is not used because it is
OneMind's own task service (ONTOLOGY §6.3). Change events use `evt.{domain}.{entity}.{event}`
with entity `tak-{token}`, so TAK mission names never collide with OneMind's `msn-`/`tsk-` ids on
`evt.mission.*` (mission-task.md §1); stream `MISSION` (`evt.mission.>`) already stores them.
`{token}` = `TakMissionToken(name)`: the lowercased name if it fits `[a-z0-9_-]` and 40
characters, else the first 26 characters of base32(sha256(name)); the event carries the full name.

Records (all times epoch ms):

```jsonc
// summary
{ "name": "patrol-1", "token": "tak-patrol-1", "description": "", "creator_uid": "ANDROID-1",
  "create_time": 1793275200000, "subscribed": true, "uid_count": 3, "content_count": 1 }
// mission = summary + items
{ ..., "uids": [ { "uid": "u1", "type": "a-u-G", "callsign": "Car", "lat": 30.1, "lon": -97.1,
                   "creator_uid": "ANDROID-1", "timestamp": 1793275200000 } ],
       "contents": [ { "hash": "sha256…", "filename": "plan.pdf", "mime_type": "application/pdf",
                       "size": 1024, "submitter": "bob", "timestamp": 1793275200000 } ] }
// change
{ "type": "ADD_CONTENT", "uid": "u1", "hash": "", "callsign": "Car", "cot_type": "a-u-G",
  "creator_uid": "ANDROID-1", "timestamp": 1793275200000 }
// log
{ "id": "…", "content": "…", "creator_uid": "ANDROID-1", "timestamp": 1793275200000 }
```

Write level: `content.put` is a shared marker in a TAK mission, the same class as `svc.cot.send`
(ONTOLOGY §6.1 level A1, logged, undoable). The client validates the CoT first
(`om::ValidateOutgoingCot`: no reserved uid, no hostile atom, no tasking type). If the owner wants
mission writes behind policy, tak-client can require an `evt.so.action.approved` for it instead;
the client contract does not change.

Unverified: the exact `<marti><dest mission=…/>` form for sending CoT to a mission
(tak-protocols.md "Unverified"); TAK Server 5.8 MissionApi behaviour.

## Client permissions (UE-P3)

The A19 identity layout (omos PR #562, `platform/nats/identities/identities.yaml`) gives the
`CLIENT` account requests to `svc.{policy,entity,layer}.>` and `svc.cot.send`, and `cmd.map.>`.
The M4 client also needs, read-only:

| What | Why | Add to `identities.yaml` |
|---|---|---|
| stream `evt.so.action.>` to CLIENT | D3: the confirm dialog follows its action to `executed`/`failed` (asset-command.md §2: the client subscribes `evt.so.action.>`) | ONEMIND export `stream: "evt.so.action.>"`, CLIENT import; the client user may subscribe it |
| stream `evt.mission.*.changed` to CLIENT | C7 mission view updates | same pattern |
| stream `tel.feed.*.status` to CLIENT | B20 gaps panel heartbeats | same pattern |
| service `svc.tak.mission.>` to CLIENT | C7 (once tak-client serves it) | export from ONEMIND to CLIENT, `share: true` import |

The D3 run on the build machine used exactly this export and import for `evt.so.action.>`
(`HostTests/scripts/e2e_d3_policy.py`); without it the client still gets the proposal and
confirm replies but not the outcome.
