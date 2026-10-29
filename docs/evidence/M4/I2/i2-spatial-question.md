# i2-spatial-question

## 1. What's moving near the north field?

- `onemind_entity_query` {"name": "north field", "types": ["zone"]} -> `{"count": 1, "entities": ["zone-north-field"]}`
- `onemind_entity_query` {"near": {"entity_id": "zone-north-field", "radius_m": 500}, "moving_only": true, "exclude_ids": ["zone-north-field"]} -> `{"count": 2, "entities": ["trk-p1", "sim-tractor-1"]}`
- `onemind_map_frame` {"entity_ids": ["trk-p1", "sim-tractor-1", "zone-north-field"]} -> `{"ok": true, "subject": "cmd.map.viewport.set", "applied_by": ["stub-map-1"], "view": {"camera": {"lat": 30.28, "lon": -97.743, "range_m": 1133.7669142219672, "heading_deg": 0, "pitch_deg": -90, "bbox": [-97.7469, 30.2774, -97.7391, 30.2826]}, "tour_stops": 0}, "action_id": "f021bde19acd4852a1003416adc20da3"}`
- `onemind_map_select` {"entity_id": "sim-tractor-1"} -> `{"ok": true, "subject": "cmd.map.highlight.sim-tractor-1", "applied_by": ["stub-map-1"], "view": {"selected": ["sim-tractor-1"]}, "action_id": "594f5cce78ab42f08308890a1f9bfe1f"}`

Answer: Moving near the North Field: track trk-p1 (trk-p1, track, 1.2 m/s, 56 m away, source camera-north, observed_t 1793275200000); Tractor (sim-tractor-1, vehicle, 2.5 m/s, 59 m away, source fleet-sim, observed_t 1793275200000)

View check: **pass**
