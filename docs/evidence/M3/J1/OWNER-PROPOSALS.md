# J1 owner proposals (decision 0004 #8)

Found while building the L0 web client (branch `run/m3-web`, `web/`). None adds a subject
pattern; all stay inside the locked grammar (omos `CLAUDE.md`). Each lists what the client does
until the owner decides.

| ID | Gap | Evidence | Proposal | Client behaviour until decided |
|---|---|---|---|---|
| J1-P1 | The render subset (layer-snapshot.md §4) has no `geo`, so a `zones` layer cannot draw shapes from it, and alert fields are missing | layer-snapshot.md §4 field table | Either add `geo` (and an alert summary) to the render subset, or state in layer-snapshot.md §3.1 that `small` layers are read with `fields: "full"` | Small layers request `fields: "full"` (allowed by §3.1 for small layers); medium and large use `render` (`web/src/bus/layerclient.ts`) |
| J1-P2 | entity.md does not say where an alert's own properties sit (`alert_kind`, `level`, `about`, `raised_by`, `state`, named in ONTOLOGY.md §3) | entity.md §4.1 has no alert component; ONTOLOGY.md §3 alert row | Add an optional `alert` component to entity.md §4.1: `{alert_kind, level: advisory\|caution\|warning, about, raised_by, state: open\|acknowledged\|resolved}` | Reads top level first, then `properties`, then an `alert` object (`web/src/bus/alerts.ts`); the fixture publishes them at the top level |
| J1-P3 | `evt.map.geofence.{id}.{enter\|exit}` has no payload definition | omos `docs/NATS-DOCTRINE.md` (subject lines only) | Define the payload: `{entity_id, zone_id, t}` (epoch ms), plus the doctrine §9.2 geo-envelope of the entity if wanted | Uses the subject tokens; reads `entity_id`, `uid` or `callsign` and `t` if present |
| J1-P4 | Alerts are not a layer in the layer catalog (layer-snapshot.md §1) | layer-snapshot.md §1 table | Add a `small` layer `alerts` (entity type `alert`, pinned until resolved) so every client gets them from the layer service | L0 defines `alerts` client-side (`clientDefined` in `web/src/bus/catalog.ts`) |
| J1-P5 | The layer service (`svc.layer.*`, FEATURES A5) does not exist yet, so the required L0 path is proven only against the fixture's stand-in | no `services/layer/` in omos; PR #552 serves `svc.entity.get/query` only | Build A5 to layer-snapshot.md/layer-delta.md; the L0 client and its tests are ready for it | Falls back to the interim path (PR #552) and then to core `ent.*` |
