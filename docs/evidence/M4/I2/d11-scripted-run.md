# d11-scripted-run

## 1. Fly to SIM-X1.

- `onemind_map_fly_to` {"entity_id": "sim-x1", "range_m": 600} -> `{"ok": true, "subject": "cmd.map.viewport.set", "applied_by": ["stub-map-1"], "view": {"camera": {"lat": 30.27, "lon": -97.74, "range_m": 600.0, "heading_deg": 0, "pitch_deg": -45}, "tour_stops": 0}, "action_id": "844801b6dd5c490bb002c000bc13528d"}`

Answer: Done: fly to SIM-X1.

View check: **pass**

## 2. Frame all the drones.

- `onemind_entity_query` {"types": ["drone"]} -> `{"count": 3, "entities": ["sim-x1", "sim-x2", "sim-x3"]}`
- `onemind_map_frame` {"entity_ids": ["sim-x1", "sim-x2", "sim-x3"]} -> `{"ok": true, "subject": "cmd.map.viewport.set", "applied_by": ["stub-map-1"], "view": {"camera": {"lat": 30.2675, "lon": -97.7425, "range_m": 3428.1566550432576, "heading_deg": 0, "pitch_deg": -90, "bbox": [-97.75225, 30.25775, -97.73275, 30.27725]}, "tour_stops": 0}, "action_id": "36241f7d4fb7419eade1f31c47cfe7b8"}`

Answer: Done: frame 3 drones.

View check: **pass**

## 3. Hide the aircraft layer.

- `onemind_map_layers` {"hide": ["air"]} -> `{"ok": true, "subject": "cmd.map.filter.set", "applied_by": ["stub-map-1"], "view": {"visible_layers": ["fleet", "structures", "tracks", "zones"]}, "action_id": "30e99ea3ecf3418bbf7189963b18b28e"}`

Answer: Done: hide the aircraft layer.

View check: **pass**

## 4. Show the alerts layer.

- `onemind_map_layers` {"show": ["alerts"]} -> `{"ok": true, "subject": "cmd.map.filter.set", "applied_by": ["stub-map-1"], "view": {"visible_layers": ["alerts", "fleet", "structures", "tracks", "zones"]}, "action_id": "8407b456d7e440db814043d8f7858d26"}`

Answer: Done: show the alerts layer.

View check: **pass**

## 5. Select the tractor.

- `onemind_entity_query` {"name": "tractor"} -> `{"count": 1, "entities": ["sim-tractor-1"]}`
- `onemind_map_select` {"entity_id": "sim-tractor-1", "label": "Tractor"} -> `{"ok": true, "subject": "cmd.map.highlight.sim-tractor-1", "applied_by": ["stub-map-1"], "view": {"selected": ["sim-tractor-1"]}, "action_id": "eac6cea67725477a93233bc30b1eff05"}`

Answer: Done: select sim-tractor-1.

View check: **pass**

## 6. Outline the north field.

- `onemind_entity_query` {"name": "north field", "types": ["zone"]} -> `{"count": 1, "entities": ["zone-north-field"]}`
- `onemind_map_annotate` {"entity_id": "zone-north-field", "label": "North field"} -> `{"ok": true, "subject": "cmd.map.annotation.hermes-8015aad4d0.create", "applied_by": ["stub-map-1"], "view": {"annotations": ["hermes-8015aad4d0"]}, "action_id": "8b1e21d3f7814772b4e72c5c4a617190"}`

Answer: Done: outline the North Field boundary.

View check: **pass**

## 7. How far is SIM-X1 from the barn? Show it on the map.

- `onemind_entity_query` {"name": "barn"} -> `{"count": 1, "entities": ["structure-barn"]}`
- `onemind_map_measure` {"entity_ids": ["sim-x1", "structure-barn"]} -> `{"ok": true, "subject": "cmd.map.annotation.hermes-f2820ff7d5.create", "applied_by": ["stub-map-1"], "view": {"annotations": ["hermes-8015aad4d0", "hermes-f2820ff7d5"]}, "measure": {"distance_m": 620.1}, "action_id": "331e640c81464fff9b717f9fdc9689c9"}`

Answer: SIM-X1 is 620 m from the barn (and drew it on the map).

View check: **pass**

## 8. Give me a camera tour of the house, the barn and the pond.

- `onemind_entity_query` {"name": "house"} -> `{"count": 1, "entities": ["structure-house"]}`
- `onemind_entity_query` {"name": "barn"} -> `{"count": 1, "entities": ["structure-barn"]}`
- `onemind_entity_query` {"name": "pond"} -> `{"count": 1, "entities": ["zone-pond"]}`
- `onemind_map_camera_tour` {"stops": [{"entity_id": "structure-house", "dwell_s": 3}, {"entity_id": "structure-barn", "dwell_s": 3}, {"entity_id": "zone-pond", "dwell_s": 3}]} -> `{"ok": true, "subject": "cmd.map.viewport.set", "applied_by": ["stub-map-1"], "view": {"camera": {"lat": 30.2696, "lon": -97.73825, "range_m": 1500, "heading_deg": 0, "pitch_deg": -45}, "tour_stops": 3}, "action_id": "334519cf234a4f3b91087949bfdd0d84"}`

Answer: Done: run a camera tour of the house, barn and pond.

View check: **pass**

## 9. Put a note on the front gate: check lock.

- `onemind_entity_query` {"name": "gate"} -> `{"count": 1, "entities": ["place-gate"]}`
- `onemind_map_annotate` {"entity_id": "place-gate", "label": "Check lock", "note": "check lock tonight"} -> `{"ok": true, "subject": "cmd.map.annotation.hermes-d091c4593c.create", "applied_by": ["stub-map-1"], "view": {"annotations": ["hermes-8015aad4d0", "hermes-d091c4593c", "hermes-f2820ff7d5"]}, "action_id": "4b1d679e9e1b409f815cb877abc72566"}`

Answer: Done: mark the front gate.

View check: **pass**

## 10. Remove the north field outline.

- `onemind_map_remove_annotation` {"annotation_id": "hermes-8015aad4d0"} -> `{"ok": true, "subject": "cmd.map.annotation.hermes-8015aad4d0.remove", "applied_by": ["stub-map-1"], "view": {"annotations": ["hermes-d091c4593c", "hermes-f2820ff7d5"]}, "action_id": "71395a07e8a5432e91f8fd7c372b660e"}`

Answer: Done: remove the North Field outline.

View check: **pass**
