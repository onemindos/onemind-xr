# INT/COT owner steps: svc.cot.send in the projector (omos #563)

Built and tested on the build machine. These checks need live TAK, a device or the cluster.
Nothing here is deployed. Run them only after the sync order in omos
`docs/proposals/2026-10-29-projector-aoi.md`, and after #562 (identities) stage A1 is applied.

## 1. Read-only pre-checks (cluster)

```bash
kubectl --context do-nyc3-omos-cloud -n nats exec deploy/nats-box -- \
  nats --server nats://nats.nats.svc.cluster.local:4222 server request connections --account CLIENT 2>/dev/null | head
```
Expected: the `CLIENT` account exists (applied by #562). If it does not, svc.cot.send refuses every
caller with `forbidden` ("caller identity not verified"), because the endpoint needs the
server-set `Nats-Request-Info` header of the `share: true` import.

## 1a. Precondition before enabling svc.cot.send (review fix, omos #563 commit 3d385a10)

The endpoint ships **off** (`COT_SEND_ENABLED=false` in `platform/omos/projector/deployment.yaml`).
A user of the `ONEMIND` account other than the projector (the shared `onemind` and `leaf_tak`
logins) can hand-write `Nats-Request-Info: {"acc":"CLIENT","user":"zeus"}`; nats-server passes
it unchanged and the projector cannot tell it from a real CLIENT request.

1. Apply owner proposal A6-P13 through omos #562: `publish_deny: ["svc.cot.>"]` for every
   `ONEMIND` user except the projector (and the same for `svc.entity.>`, `svc.layer.>`,
   `svc.policy.>` where those services read the header the same way).
2. Read-only check after #562 is applied (from a machine holding the shared `onemind` login):
   ```bash
   nats --server <bus url> --user onemind --password '<from the existing secret>' \
     request svc.cot.send '{}' -H 'Nats-Request-Info:{"acc":"CLIENT","user":"zeus"}' --timeout 2s
   ```
   Expected: `Permissions Violation for Publish to "svc.cot.send"` (no reply, nothing on
   `tak.cot.inject`). Only then set `COT_SEND_ENABLED` to `"true"` in the deployment by PR.
3. `COT_SEND_PERSISTENT_KINDS` stays `"false"` until A6-P6 is ruled; until then the Unreal
   client's marker, route, geofence and shape sends return `forbidden`, and only `spi`, `chat`
   and the web GeoChat are served (step 4 below applies to those only).

## 2. After syncing `platform-omos-projector` (manual sync, owner)

```bash
kubectl --context do-nyc3-omos-cloud -n omos logs deploy/omos-tak-projector-gated | grep 'serving client CoT'
```
Expected: one line `"subject":"svc.cot.send","accounts":["CLIENT"],"allow_unverified":false`.

## 3. Web client GeoChat reaches ATAK (WEB-P4)

1. Open the L0 web client as a CLIENT user (for example `zeus`). Open the Chat tab, room
   "All Chat Rooms", and send `owner check 1`.
2. Expected in the web client: the message state goes from `sending` to `sent`.
3. Expected on an ATAK EUD connected to `tak.tail717ba0.ts.net`: GeoChat "All Chat Rooms" shows
   `owner check 1` from callsign `ZEUS`.
4. Projector log: `"msg":"svc.cot.send","caller":"CLIENT/zeus","kind":"geochat"`.

Unverified until this check: whether ATAK shows a GeoChat whose `<remarks source>` is
`BAO.F.OneMind.{uid}` (ATAK itself writes `BAO.F.ATAK.{uid}`), and whether NatsBridgePlugin
delivers `b-t-f` from `tak.cot.inject` to connected EUDs (the TAK→bus leg is not proven live,
cot-projection.md §1).

## 4. Unreal client marker, drawing and chat (B-P4, UE-P1)

On the Mac, with the client's `NATS_USER` set to a CLIENT user:

1. Drop a marker (C2), draw a circle and a range ring (B11), and send a chat.
2. Expected on ATAK: the marker `OM-C2-MARK`-style callsign, the circle and the ring, and the chat
   line. Each reply in the client log has `ok=1`.
3. Expected refusal: none. If a drawing is refused, the projector log gives the code and reason
   (`svc.cot.send refused`). Send that line back to the run.

## 5. Abuse checks on the live bus (optional, owner)

These commands use the `nats` CLI as a CLIENT user. Keep the password in the environment, never on
the command line in shared shells.

```bash
export NATS_URL=nats://<cluster-or-tailnet-host>:4222 NATS_USER=carol NATS_PASSWORD=...   # keychain
nats req svc.cot.send '{"v":1,"kind":"geochat","target":"server","message_uid":"GeoChat.OM-WEB-zeus.Cyan.x1","room":{"id":"Cyan","name":"Cyan","kind":"group"},"text":"spoof","sender":{"uid":"OM-WEB-zeus","callsign":"ZEUS"}}'
```
Expected: `{"error":{"code":"forbidden","message":"sender.uid \"OM-WEB-zeus\" is not bound to the calling user"},"ok":false,"v":1}`.
Nothing new appears on ATAK.

## 6. Decisions to rule

A6-P6 to A6-P12 in omos `docs/proposals/2026-10-29-projector-aoi.md` (client XML form, reply
shape, `client_id` binding, chat default marking, AGENTS access, hub/site queue group, ceiling),
as additions to INT-R4 in the #556 register.
