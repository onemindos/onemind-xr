# d11-no-ack-run

## 1. Fly to SIM-X1.

- `onemind_map_fly_to` {"entity_id": "sim-x1", "range_m": 600} -> `{"ok": false, "error": "no_client_ack", "subject": "cmd.map.viewport.set", "action_id": "67319ac16c1042e0a475333fbfababad"}`

Answer: I could not fly to SIM-X1: the map did not confirm it (no_client_ack).

## 2. Frame all the drones.

- `onemind_entity_query` {"types": ["drone"]} -> `{"count": 3, "entities": ["sim-x1", "sim-x2", "sim-x3"]}`
- `onemind_map_frame` {"entity_ids": ["sim-x1", "sim-x2", "sim-x3"]} -> `{"ok": false, "error": "no_client_ack", "subject": "cmd.map.viewport.set", "action_id": "6919663891b540ee83f44686c0454c67"}`

Answer: I could not frame 3 drones: the map did not confirm it (no_client_ack).

## 3. Hide the aircraft layer.

- `onemind_map_layers` {"hide": ["air"]} -> `{"ok": false, "error": "no_client_ack", "subject": "cmd.map.filter.set", "action_id": "27ab3f2df2024def8415ed025e28726e"}`

Answer: I could not hide the aircraft layer: the map did not confirm it (no_client_ack).

## 4. Show the alerts layer.

- `onemind_map_layers` {"show": ["alerts"]} -> `{"ok": false, "error": "no_client_ack", "subject": "cmd.map.filter.set", "action_id": "ea3a2d091d614383a11a680837d81a5b"}`

Answer: I could not show the alerts layer: the map did not confirm it (no_client_ack).

## 5. Select the tractor.

- `onemind_entity_query` {"name": "tractor"} -> `{"count": 1, "entities": ["sim-tractor-1"]}`
- `onemind_map_select` {"entity_id": "sim-tractor-1", "label": "Tractor"} -> `{"ok": false, "error": "no_client_ack", "subject": "cmd.map.highlight.sim-tractor-1", "action_id": "0a2f27954b6746a195018d3a85814f92"}`

Answer: I could not select sim-tractor-1: the map did not confirm it (no_client_ack).
