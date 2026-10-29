# Vision

OneMind is an open-source NGO platform that brings TAK-class situational awareness
to civilians and their communities: homesteads, neighbourhoods, response teams. It
uses the JADC2 loop.

| Step | What it means here |
|---|---|
| **Sense** | Everything that can report a position or an observation reaches one bus: TAK users, drones, robots, vehicles, home sensors, cameras, weather, seismic, fire, air and sea traffic, conflict and news feeds. |
| **Make Sense** | A fusion layer turns raw reports into entities with identity, history and confidence. The homestead becomes a live 3D digital twin rebuilt from drone imagery. Hermes, the AI, reasons over entities, not raw feeds. |
| **Act** | People and Hermes create missions, geofences and tasks, and command assets. Commands reach TAK clients over TAK Server and over the mesh, and robots and drones over their own bridges. Every action is audited. |

## What exists elsewhere, and the gap

- **Intel dashboards** (worldmonitor, osiris, gods-eye-view, worldwideview, Argos
  Atlas) show a lot: tens of sources, thousands of entities, photorealistic 3D. They
  only watch. They have no TAK, no mesh, no missions and no way to act.
- **TAK clients** (ATAK, iTAK, WinTAK, VR-TAK, TAKX) act and mesh. They do not carry
  world-scale intel layers, a fused entity model or an AI that acts.
- **ARTAK** (Eolian) is the closest product: multi-domain planning and C2 on AR, VR,
  tablet and PC, ATAK integration, 3D maps from drone imagery, and a disconnected
  mode. It is commercial and closed.

**OneMind's edge:** intel-dashboard breadth, TAK-native action and mesh, a homestead
digital twin, and an AI that acts within audited permissions. All of it open source,
local-first, and able to keep working when the cloud is gone.

## Principles

1. **TAK-native.** CoT is the common language for anything with a position. OneMind
   joins TAK networks as a peer: TAK Server streaming plus the mesh, via the omos
   mesh gateway and later Commo inside the client.
2. **The bus is the authority.** NATS (JetStream, KV and leaf nodes) carries
   everything. Clients are views and input devices.
3. **Two tiers on the map.** The *world tier* (bulk intel) renders in the 3D client
   from snapshots. The *mission tier* (tracks that matter to a mission) also goes
   to TAK as CoT, filtered by area of interest, geofence and tasking. Phones never
   receive the whole world.
4. **Local-first.** It works on one Mac with a local leaf node, and keeps working
   offline with the mesh and cached tiles.
5. **One client, many surfaces.** The same Unreal app runs on desktop, streams to
   browsers and runs on the headset. No separate web app drifts away from it.
6. **The AI acts within limits.** Hermes proposes and executes through the same
   command subjects as people, with policy checks and an audit trail.
