# `svc.cot.send` v1 request body (proposed, feature C2)

Status: **proposed** by lane B on 2026-10-29. The endpoint belongs to the omos projector
([cot-projection.md](../../../docs/contracts/cot-projection.md) §1 row "Ephemeral client sends",
§9 "Sending": "Its request body is defined with FEATURES C2"). Nothing in omos serves it yet; the
build-machine test uses a stand-in in `HostTests/scripts/e2e_c1_c2.py`. When the projector
implements it, the omos file that defines it becomes the source of truth and this page links to it.

Subject: `svc.cot.send` (grammar `svc.{service}.{endpoint}`; listed in
[BUS-DESIGN.md](../../../docs/design/BUS-DESIGN.md) §3.4). Request/reply, one reply.

## Request

```json
{
  "v": 1,
  "kind": "marker",
  "targets": ["server", "mesh"],
  "client_id": "mac",
  "client_ref": "onemind-mac-12-k3J9aQ2x",
  "marking": { "tlp": "CLEAR" },
  "dest_uid": null,
  "cot": "<event version=\"2.0\" uid=\"onemind-mac-12-k3J9aQ2x\" type=\"a-u-G\" how=\"h-e\" ...>...</event>"
}
```

| Field | Rule |
|---|---|
| `v` | `1` |
| `kind` | `marker` (CoT atom), `spi` (`b-m-p-s-p-i`), `route` (`b-m-r`), `geofence` (`u-d-f`), `chat` (`b-t-f`) |
| `targets` | subset of `server` (projector publishes `tak.cot.inject`) and `mesh` (site projector publishes `tak.cot.mesh.tx`) |
| `client_id` | the sending client instance (also in the uid) |
| `client_ref` | idempotency key; the client sends the CoT uid. A projector that has seen it within the stale time answers the stored reply instead of sending again |
| `marking.tlp` | the projector's marking gate G7 applies (cot-projection.md §3.4): only `CLEAR` and `GREEN` pass unless `TAK_RELEASE_GROUPS` allows more |
| `dest_uid` | optional directed send to one TAK contact (mesh gateway header `Tak-Dest-Uid`) |
| `cot` | CoT XML built by the client (`om::Build*` in `OneMindCore/CoT.h`) |

What the client guarantees before sending (`om::ValidateOutgoingCot`): uid is not `OM.*` or
`INTEL.*` (reserved, cot-projection.md §5.1), no hostile atoms (`a-h`, `a-j`, `a-k`), no tasking
types (`t-k`, `t-e`; tasking goes through `svc.policy.task.propose`), valid `time` and `stale`,
at most 64 KiB. Everything sent in this run carries `opex="s"` (simulated) and a `SIMULATED`
remark (decision 0004 #13).

What the projector must still check (defence in depth; the client is not trusted): the same rules,
the marking gate, its per-target rate limits (cot-projection.md §6), and that the caller may send
to the target (account permissions, BUS-DESIGN §7).

## Reply

```json
{ "v": 1, "ok": true, "uid": "onemind-mac-12-k3J9aQ2x", "targets": ["mesh"] }
{ "v": 1, "error": "forbidden", "detail": "marking gate" }
```

`error` ∈ `invalid`, `forbidden`, `rate_limited`, `target_failed`. `targets` lists the targets the
projector published to.

## Publishing to the mesh: core publish, not request (CP-3)

The build-machine run found that a **request** on `tak.cot.mesh.tx` is answered by JetStream
with a PubAck (`{"stream":"COT","seq":N}`) before the gateway's `ok`, because the `COT` stream binds
`tak.cot.>` (omos `platform/nats/streams/cot.yaml`). Proof: `docs/evidence/M2/B/e2e-c1-c2/cp3-puback-probe.txt`.
So the projector should core-publish to `tak.cot.mesh.tx` (proposal CP-3) and confirm delivery from
the gateway's `tx` counter on `tel.fleet.edge.{gateway}.status`, not from a reply.

## Open (owner proposals B-P1, B-P2 in `docs/evidence/M2/B/OWNER-STEPS.md`)

- FEATURES C2 sends markers, routes and geofences through `svc.cot.send`; cot-projection.md §9
  sends persistent items as entities through `entity.create` in policy and keeps `svc.cot.send` for
  ephemeral items. The client supports both kinds of call; which one markers use is the owner's call.
- The FEATURES B3 test uses a `kv put` on `world-state`; BUS-DESIGN D3 reserves `world-state` for
  layer manifests. The client reads only `probe.*` keys there, for the S5 check.
