# Contract: CoT profile for drones, robots and own vehicles

Status: M0, 2026-10-29. Version `v: 1`. Nothing here is deployed. Feature: FEATURES H9 (and K3
for video, H1 for SITL on ATAK).

This profile says how OneMind's **own** taskable assets (drones, ground robots, robot dogs,
uncrewed boats, own vehicles) appear in TAK: CoT type, detail elements, sensor footprints,
video links, and how a command on the asset is echoed to TAK users. It is a specialisation of
[cot-projection.md](cot-projection.md): every gate, uid rule, stale rule, rate limit and subject
there applies, and this file only adds what is specific to robots.

In this run every asset is simulated and its id starts `sim-` (decision 0004 #13). The profile
is written so a real asset later needs no change except `indicators.simulated: false`.

Inputs: [drones-robots-agents.md](../research/drones-robots-agents.md) ("CoT for UAVs and UGVs",
"How it applies" 3, Recommendation 3), [standards.md](../research/standards.md) (CoT types, MISB
KLV), [tak-protocols.md](../research/tak-protocols.md) (§1 detail XSDs, §2 v1 typed fields),
[video-and-plugins.md](../research/video-and-plugins.md) (§D TAK video, "How it applies" 7–8),
[asset-command.md](asset-command.md), [mission-task.md](mission-task.md), [entity.md](entity.md).

---

## 1. Subjects and who does what

| Data | Subject or bucket | Defined in (omos) | State |
|---|---|---|---|
| Asset position and status (bridge output) | `tel.fleet.{class}.{id}.position`, `tel.fleet.{class}.{id}.status` | `docs/NATS-DOCTRINE.md` §9.1; stored by `platform/nats/streams/tel.yaml` (`tel.fleet.>`) | pattern and stream exist; the MAVLink bridge still publishes `omos.air.*` and must move (FEATURES H2, [drones-robots-agents.md](../research/drones-robots-agents.md) Recommendation 1) |
| Sensor pose and frame footprint (KLV extractor) | `tel.fleet.drone.{id}.sensor`, `tel.fleet.drone.{id}.footprint` | same grammar line; [BUS-DESIGN.md](../design/BUS-DESIGN.md) line 73 | metric names beyond `position`/`status` need owner ruling **BUS-P7** (BUS-DESIGN §10) |
| Fused asset state (input to the projector) | `ent.{drone\|robot\|vehicle}.{id}.state` | `CLAUDE.md` grammar; `platform/nats/streams/ent.yaml` | `ENT` **proposed** ([entity.md](entity.md) §1) |
| Command outcomes from the bridge | `evt.{class}.{id}.command` | grammar `evt.{domain}.{entity}.{event}`; `platform/nats/streams/evt.yaml` binds `evt.drone.>` and `evt.robot.>` (lines 18–19); **`evt.vehicle.>` is not bound** (ONT-P13) | exists for drone and robot |
| Action audit (the echo source) | `evt.so.action.{event}` | `platform/nats/streams/evt.yaml` (`evt.so.>`, line 11); [asset-command.md](asset-command.md) §1, §5 | pattern and stream exist; policy service **proposed** |
| Video stream registry | KV `OVERWATCH_VIDEO` | `platform/nats/streams/kv-overwatch-video.yaml` | bucket exists; no writer today ([video-and-plugins.md](../research/video-and-plugins.md) Unverified) |
| TAK output | `tak.cot.inject` (hub), `tak.cot.mesh.tx` (site) | [cot-projection.md](cot-projection.md) §1 | as there |
| TAK Server video feed list | `POST /Marti/api/video` (FeedV2) | TAK Server REST, read from 5.7-RELEASE-14 source ([tak-protocols.md](../research/tak-protocols.md) §4); 5.8 **unverified** | done by the planned `tak-client`, not the projector ([tak-protocols.md](../research/tak-protocols.md) Level 1 item 3) |

Split of work:

| Component | Does |
|---|---|
| Asset bridge (MAVLink, ROS adapter) | fills `tel.fleet.*` with the doctrine §9.2 envelope: ellipsoid `hae` (MSL converted), course, speed, battery, mode, link ([drones-robots-agents.md](../research/drones-robots-agents.md) Risks "hae filled from MSL") |
| Entity service | fills `kind`, `cot_type` (§2), `sensors[]`, `media[]`, `health`, `current_task` |
| Projector | renders the CoT of §3–§6 |
| `tak-client` (planned) | registers video feeds on TAK Server |

`{class}` ∈ `drone`, `robot`, `vehicle` (doctrine §9.1 class vocabulary; [asset-command.md](asset-command.md)
§1). An uncrewed boat or submersible is class `robot` with `environment: surface` or
`subsurface`.

---

## 2. CoT types

Own assets always have affiliation `f` (friendly). Each type below was checked against MITRE
`CoTtypes.xml` (Apache-2.0 copy at https://github.com/dB-SPL/cot-types/blob/main/CoTtypes.xml,
read with `gh api` on 2026-10-29) and, for icons, against the ATAK-CIV `mil-std-2525c` asset
list ([drones-robots-agents.md](../research/drones-robots-agents.md) "CoT for UAVs and UGVs", W12).

| Asset | `entity_type` / `kind` | CoT type | `CoTtypes.xml` text | ATAK icon |
|---|---|---|---|---|
| Multirotor drone | `drone` / `multirotor` | `a-f-A-M-H-Q` | Air/Mil/Rotor/Drone,RPV,UAV | `sfapmhq` ships |
| Fixed-wing drone | `drone` / `fixed_wing` | `a-f-A-M-F-Q` | Air/Mil/Fixed/Drone,RPV,UAV | `sfapmfq` ships |
| VTOL drone | `drone` / `vtol` | `a-f-A-M-F-Q` | as fixed wing | as fixed wing |
| Drone, airframe unknown | `drone` / missing | `a-f-A-M-F-Q` | as fixed wing | keeps today's projector mapping (`tak-projector.yaml` line 67) |
| Ground robot, robot dog | `robot` / `ugv`, `quadruped`, missing | `a-f-G-E-V` | Gnd/Equip/Vehicle ("GROUND VEHICLE") | `sfgpev…` family ships; exact generic icon **unverified** |
| Uncrewed boat | `robot` + `environment: surface` | `a-f-S-X` | Surface/NON MILITARY | **unverified** |
| Uncrewed submersible | `robot` + `environment: subsurface` | `a-f-U-S-U` | SubSurf/UNMANNED UNDERWATER VEHICLE (UUV) | **unverified** |
| Own car, truck, ATV | `vehicle` / any land kind | `a-f-G-E-V-C` | Gnd/Equip/Vehic/Civilian | **unverified** |
| Own boat (crewed) | `vehicle` + `environment: surface` | `a-f-S-X` | Surface/NON MILITARY | **unverified** |

**Why these and not others.**

- The `M` in `A-M-…-Q` is the MIL-STD-2525 tree where the UAV symbols live, and it is what ATAK
  ships icons for. It is a symbol code, not a claim of military use. `CoTtypes.xml` also lists a
  civil drone, `a-.-A-C-F-q` ("Air/Civ/fixed/rpv, drone, uav"), but its lower-case `q` is a CoT
  extension outside 2525, so ATAK most likely has no icon for it (*unverified*). It is not used.
- **Robots settle ONT-P12.** Today's projector sends robots as `a-f-G-U-C-R`
  (`tak-projector.yaml` line 78), which `CoTtypes.xml` defines as "Gnd/Combat/Recon" (a combat
  reconnaissance unit), and own vehicles as `a-f-G-U-C-V` (line 77), which it defines as
  "Gnd/Combat/Aviation". Both are wrong for a civilian robot or car. No dedicated UGV type was
  found in 2525C or `CoTtypes.xml` ([drones-robots-agents.md](../research/drones-robots-agents.md)
  "CoT for UAVs and UGVs"), so robots use the generic ground vehicle `a-f-G-E-V`.
  `a-f-G-E-V-U` ("Utility Vehicle") is not used, because it reads as a crewed utility truck.

**Choosing `kind` from MAVLink.** The bridge reads `HEARTBEAT.type`
([drones-robots-agents.md](../research/drones-robots-agents.md) "How it applies" 3). The mapping
from `MAV_TYPE` names is below. The enum names come from MAVLink `common.xml`, which this lane
did not re-read, so the exact list is *unverified*; unknown values give `kind` missing.

| `MAV_TYPE` (name) | `class` / `kind` |
|---|---|
| `QUADROTOR`, `HEXAROTOR`, `OCTOROTOR`, `TRICOPTER`, `COAXIAL`, `HELICOPTER`, `DODECAROTOR` | `drone` / `multirotor` |
| `FIXED_WING` | `drone` / `fixed_wing` |
| `VTOL_*` | `drone` / `vtol` |
| `GROUND_ROVER` | `robot` / `ugv` |
| `SURFACE_BOAT` | `robot` / `usv`, `environment: surface` |
| `SUBMARINE` | `robot` / `uuv`, `environment: subsurface` |

ROS 2 robots set `kind` from the robot registry, because ROS has no equivalent field
([drones-robots-agents.md](../research/drones-robots-agents.md) "How it applies" 6).

---

## 3. Root and detail elements

Root attributes follow [cot-projection.md](cot-projection.md) §5.1, with these fixed values for
own assets:

| Attribute | Value | Why |
|---|---|---|
| `uid` | `OM.{drone\|robot\|vehicle}.{id}`, e.g. `OM.drone.sim-x1` | cot-projection §5.1 |
| `how` | `m-s` when simulated; `m-g` for a real GPS-fed asset | MITRE `types.txt` how tree (`m-s` "out of a simulation", `m-g` GPS) |
| `opex` | `s` when simulated | [lattice-patterns.md](../research/lattice-patterns.md) (C2) |
| `stale` | asset default 60 s, clamped per cot-projection §5.3 | [entity.md](entity.md) §4.3 fleet `Nats-TTL` |

**Simulation must be obvious to a TAK user.** A simulated asset carries all three of:
`opex="s"`, a callsign that starts `SIM-` (the entity service prefixes it if the registry name
does not), and `SIMULATED` in `<remarks>`. A TAK user must never mistake a `sim-*` drone for a
real aircraft (run rule 6; decision 0004 #13).

| Element | Content | Entity field | Over TAK Protocol v1 |
|---|---|---|---|
| `<contact callsign>` | e.g. `SIM-X1` | `callsign` | typed field ([tak-protocols.md](../research/tak-protocols.md) §2) |
| `<track course speed>` | degrees true, ground speed m/s | `location.course_deg`, `location.speed_mps` | typed |
| `<status battery>` | integer percent | `health.battery_pct` | typed |
| `<precisionlocation geopointsrc="GPS" altsrc="GPS">` | only when `hae` comes from a GPS ellipsoid height | provenance | typed |
| `<remarks>` | `SIMULATED · {mode} · bat {n}% · link {rssi} dBm · {task line} · {command line}` | `status.mode`, `health`, `current_task`, §6 | `xmlDetail` |
| `<sensor …>` | §4 | `sensors[]` | `xmlDetail` |
| `<__video …>` | §5 | `media[]`, KV `OVERWATCH_VIDEO` | `xmlDetail` |
| `<onemind …>` | cot-projection §5.4 attributes, plus a child `<asset mode connection link_rssi agl task task_state/>` and the `<command>` child of §6 | entity record | `xmlDetail` |
| `<_flow-tags_>` | as cot-projection | — | `xmlDetail` |

`<status readiness>` exists in ATAK's `StatusDetailHandler`
([drones-robots-agents.md](../research/drones-robots-agents.md) "CoT for UAVs and UGVs"), but its
meaning is not documented in the sources read, so it is not sent.

**Link state.**

| `health.connection` | Projection |
|---|---|
| `online` | normal |
| `degraded` | `<remarks>` starts `LINK DEGRADED` |
| `offline` | heartbeat stops; `<remarks>` starts `LINK LOST` on the last message; TAK greys the marker at its `stale` |

**Where the values come from (MAVLink).** The scripted drone sends `HEARTBEAT`, `SYS_STATUS`,
`GLOBAL_POSITION_INT`, `ATTITUDE` and `VFR_HUD` ([PREFLIGHT.md](../PREFLIGHT.md)). The bridge maps
position and ellipsoid height from `GLOBAL_POSITION_INT` (with the MSL-to-HAE correction or
`GPS_RAW_INT.alt_ellipsoid` where present, *unverified* per autopilot), ground speed and heading
from `VFR_HUD`, battery from `SYS_STATUS` or `BATTERY_STATUS`, and mode from `HEARTBEAT`
([drones-robots-agents.md](../research/drones-robots-agents.md) "Existing omos MAVLink bridge",
Risks). Field-level choices are the bridge's (H2) and are tested there.

---

## 4. Sensor footprints

### 4.1 Field-of-view cone (`<sensor>`)

For an asset with `observable` in `traits`, the projector adds one `<sensor>` element for the
first `sensors[]` entry with `type: camera` and `state: operational`. Attribute names come from
ATAK's `SensorDetailHandler` ([drones-robots-agents.md](../research/drones-robots-agents.md) "CoT
for UAVs and UGVs"); ATAK draws the cone with uid suffix `-fov`.

| Attribute | Value | Entity field |
|---|---|---|
| `azimuth` | boresight bearing, degrees true | `sensors[].fov.az_deg` |
| `elevation` | boresight elevation, degrees (negative = down) | `sensors[].fov.el_deg` |
| `fov` | horizontal field of view, degrees | `sensors[].fov.h_deg` |
| `vfov` | vertical field of view, degrees | `sensors[].fov.v_deg` |
| `range` | drawing range, metres | `sensors[].fov.range_m` |
| `roll` | camera roll, degrees; `0` if unknown | gimbal or KLV |
| `model` | sensor id, e.g. `cam0` | `sensors[].id` |
| `displayMagneticReference` | `0` (true north) | fixed |
| `hideFov` | `false` | fixed |
| `fovAlpha`, `fovRed`, `fovGreen`, `fovBlue`, `strokeColor`, `strokeWeight` | cyan, alpha 0.3 (projector config) | fixed |

**Unverified:** whether ATAK treats `azimuth` as an absolute bearing or relative to the platform
heading, and the units of the colour attributes. OneMind sends an absolute bearing. The owner
checklist (§9) confirms it with a drone flying east and the camera looking north.

Gimbal pose comes from the KLV extractor (`tel.fleet.drone.{id}.sensor`) when the stream carries
MISB ST 0601, or from MAVLink gimbal messages (*unverified* which ones) or from the camera
registry for fixed cameras ([standards.md](../research/standards.md) MISB row;
[video-and-plugins.md](../research/video-and-plugins.md) "How it applies" 7). A second or third
sensor on one asset is not drawn in `v: 1`.

### 4.2 Ground footprint (from KLV)

When `tel.fleet.drone.{id}.footprint` has a fresh frame footprint (frame centre and four corners
from ST 0601, [video-and-plugins.md](../research/video-and-plugins.md) "How it applies" 7), the
projector sends a child shape:

| Property | Value |
|---|---|
| uid | `OM.drone.{id}.footprint` |
| type | `u-d-f` (closed polygon, [cot-projection.md](cot-projection.md) §5.5) |
| geometry | four corners as `<link point="lat,lon"/>`, first repeated |
| parent | `<link uid="OM.drone.{id}" relation="p-p"/>` |
| style | `geo.kind: footprint` colours (cyan) |
| `stale` | 5 s, so a frozen footprint disappears quickly |
| rate | at most 1 per second; hub target only (not on the mesh unless `MESH_WORLD_FEEDS=true`) |

A frame-centre marker (`b-m-p-s-p-i`, "spi" in `CoTtypes.xml`) is **off** by default
(`FOOTPRINT_SPI=false`).

If ATAK also reads the KLV in the video stream itself (`ignoreEmbeddedKLV="false"`, §5), ATAK may
draw its own footprint next to OneMind's. Whether that happens is *unverified*; the owner
checklist decides whether OneMind's footprint stays on for ATAK. iTAK and other clients get only
OneMind's footprint.

**Safety.** A footprint shows where a camera looks. It is never computed toward a person:
`observe` and `orbit` refuse a person or person-class target ([asset-command.md](asset-command.md)
§3), and the footprint of a free-looking camera carries no identity.

---

## 5. Video links

The projector adds `<__video>` to the asset's CoT (and to fixed `camera` entities) when the
asset has a `media[]` entry with `kind: video` and a TAK-reachable URL. Attribute names: the TAK
XSD defines `url`; ATAK's `VideoDetailHandler` also reads `uid` and a nested `<ConnectionEntry>`
([tak-protocols.md](../research/tak-protocols.md) §1; [video-and-plugins.md](../research/video-and-plugins.md)
§D).

```xml
<__video uid="OM.drone.sim-x1.video" url="rtsp://{media-host}:8554/drone/sim-x1">
  <ConnectionEntry uid="OM.drone.sim-x1.video" alias="SIM-X1 cam0" protocol="rtsp"
                   address="{media-host}" port="8554" path="/drone/sim-x1"
                   networkTimeout="5000" bufferTime="-1" rtspReliable="1"
                   ignoreEmbeddedKLV="false" roverPort="-1"/>
</__video>
```

`{media-host}` is configuration, not a value in Git.

| Rule | Detail | Source |
|---|---|---|
| URL source | the stream's entry in KV `OVERWATCH_VIDEO`, field for the TAK audience (`tak_url` for the hub target, `site_url` for the mesh target); the entity's `media[].url` only if the registry has no entry | [BUS-DESIGN.md](../design/BUS-DESIGN.md) §5 `OVERWATCH_VIDEO`; [video-and-plugins.md](../research/video-and-plugins.md) Recommendation 3 |
| Reachable only | never `127.0.0.1`, `localhost`, link-local or a pod IP. [entity.md](entity.md) §4.2 shows `rtsp://127.0.0.1:8554/sim-x1` for the local run; the projector omits `<__video>` for such a URL | TAK clients cannot open the build machine's loopback |
| No credentials | never `user:pass@` or a token in the URL. Access to MediaMTX for TAK users comes from the tailnet and MediaMTX read permissions | onemind-xr `CLAUDE.md` rule 4; [video-and-plugins.md](../research/video-and-plugins.md) Recommendation 1 (tailnet-only RTSP/SRT) |
| Protocol | `rtsp` or `srt` (MPEG-TS) when the stream carries KLV, so ATAK can read it; otherwise `rtsp`. HLS (`https`) only for clients that cannot play RTSP | [video-and-plugins.md](../research/video-and-plugins.md) "How it applies" 8; ATAK `ConnectionEntry.Protocol` list (CIV mirror; ATAK 5.6 list **unverified**) |
| `networkTimeout` | 5000 ms (ATAK default) | [video-and-plugins.md](../research/video-and-plugins.md) §D |
| Mesh | sent on `tak.cot.mesh.tx` only when a `site_url` exists (LAN peers may have no tailnet) | §6.2 of cot-projection |
| TAK Server list | the `tak-client` registers the same stream as a FeedV2 (`alias`, `url`, `latitude`, `longitude`, `fov`, `heading`, `range`, `ignoreEmbeddedKLV`) with `POST /Marti/api/video`, and updates it when the URL changes or at most every 60 s for position (estimate) | [tak-protocols.md](../research/tak-protocols.md) §4, Level 1 item 3 |

**Unverified:** how ATAK uses `uid` and multiple `ConnectionEntry` children inside one
`<__video>` ([tak-protocols.md](../research/tak-protocols.md) Unverified), and the meaning of
`bufferTime="-1"`. One stream per asset in `v: 1`.

---

## 6. Command echo

TAK users should see when an asset is being commanded and what happened, without TAK ever
becoming a command path. The echo is **information only**: nothing in it asks a TAK client or
another system to act.

### 6.1 Outbound rules

| # | Rule | Source |
|---|---|---|
| E1 | The projector watches `evt.so.action.*` for actions on an asset it projects | [asset-command.md](asset-command.md) §1, §4 |
| E2 | Only approved actions are echoed: `dispatched`, `acknowledged`, `executed`, `failed`, `refused`. `proposed`, `denied`, `rejected` and `expired` stay in the OneMind client | an unapproved proposal on TAK would read as an order |
| E3 | On each echoed event the asset's CoT is sent at once (one bypass of the per-uid interval) with `<onemind><command action="{action_id}" verb="{verb}" state="{event}" t="{epoch ms}"/></onemind>` and a remarks line `CMD {verb}: {state}`. The `<command>` child is removed 60 s after a terminal state | [cot-projection.md](cot-projection.md) §6.1 |
| E4 | For `goto`, `orbit` and `land` with a position, the projector draws the destination: `b-m-p-w` ("waypoint" in `CoTtypes.xml`) at the target point, uid `OM.{class}.{id}.cmd.{action_id}`, `<link uid="OM.{class}.{id}" relation="p-p"/>`, callsign `{asset callsign} {verb}`; for `orbit` also a `u-d-c-c` circle of `radius_m`. `stale` = the command's `expires_at`, and the marker is removed on a terminal state | [asset-command.md](asset-command.md) §3 parameters, §6.3 `expires_at` |
| E5 | `estop` is shown as `<remarks>` `E-STOP` and a red `<color>` on the asset, **not** as `<emergency>`, which is the EUD distress element and would alarm every ATAK user | [tak-protocols.md](../research/tak-protocols.md) §1 (`emergency` element); ONTOLOGY §3 alert row |
| E6 | The echo never names the proposer or approver; it carries the `action_id`. The full audit stays on `evt.so.action.*` | [asset-command.md](asset-command.md) §5 |
| E7 | No tasking CoT (`t-*`) and no reply CoT (`y-*`) are sent in `v: 1` (`ECHO_TASK_COT=false`). ATAK and iTAK behaviour for them is unverified ([mission-task.md](mission-task.md) §5; ONTOLOGY §10). If the owner turns it on later, the mapping is mission-task §5's table, and `t-k-*` (strike) is never used | [lattice-patterns.md](../research/lattice-patterns.md) R2 |
| E8 | Echo messages count against the target's rate ceiling with priority 2 (after own-asset positions) | [cot-projection.md](cot-projection.md) §3.3, §6.1 |

### 6.2 Inbound: a TAK user asks an asset to do something

Nothing from TAK commands an asset directly. A tasking CoT from ATAK (later from the OneMind ATAK
plugin) that links to an `OM.{class}.{id}` uid becomes a `svc.policy.*.propose` request (`command` for one asset verb, `task` for a task) on behalf
of that TAK user, and then needs policy, audit and a human confirm in OneMind like any other
action ([cot-projection.md](cot-projection.md) §8.3; [asset-command.md](asset-command.md) §4;
[tak-protocols.md](../research/tak-protocols.md) Level 4 item 3).

| Incoming CoT (MITRE `t-*` tree) | Proposed action |
|---|---|
| `t-r` (relocate) | `asset.goto` |
| `t-s-v`, `t-s-i` (video, imagery surveillance) | `asset.observe` (target never a person) |
| `t-z` (abort) | `task.abort` |
| `t-u-z` (cancel a request) | reject the matching pending action, if the same TAK user proposed it |
| anything else, including every `t-k-*` | refused and logged |

Tasking types from [lattice-patterns.md](../research/lattice-patterns.md) (C1). The exact CoT an
ATAK user or plugin sends is **unverified** ([tak-protocols.md](../research/tak-protocols.md)
Unverified "CoT type to use for tasking"). Mapping a TAK identity to a OneMind principal that may
propose is part of the policy service (I4).

---

## 7. Example: `sim-x1` on TAK Server

From the entity in [entity.md](entity.md) §4.2, with a `goto` executing, and a tailnet video URL
in the registry:

```xml
<?xml version="1.0" encoding="UTF-8" standalone="yes" ?>
<event version="2.0" uid="OM.drone.sim-x1" type="a-f-A-M-H-Q" how="m-s" opex="s"
       time="2026-10-29T12:00:00.000Z" start="2026-10-29T12:00:00.000Z" stale="2026-10-29T12:01:00.000Z">
  <point lat="30.267200" lon="-97.743100" hae="152.4" ce="5.0" le="3.0"/>
  <detail>
    <contact callsign="SIM-X1"/>
    <track course="90.0" speed="12.50"/>
    <status battery="82"/>
    <precisionlocation geopointsrc="GPS" altsrc="GPS"/>
    <sensor azimuth="90" elevation="-30" fov="82" vfov="52" range="300" roll="0" model="cam0"
            displayMagneticReference="0" hideFov="false"/>
    <__video uid="OM.drone.sim-x1.video" url="rtsp://{media-host}:8554/drone/sim-x1">
      <ConnectionEntry uid="OM.drone.sim-x1.video" alias="SIM-X1 cam0" protocol="rtsp"
                       address="{media-host}" port="8554" path="/drone/sim-x1"
                       networkTimeout="5000" bufferTime="-1" rtspReliable="1"
                       ignoreEmbeddedKLV="false" roverPort="-1"/>
    </__video>
    <remarks>SIMULATED · AUTO · bat 82% · link -67 dBm · task goto (executing) · CMD goto: acknowledged</remarks>
    <_flow-tags_ omos-tak-projector="2026-10-29T12:00:00.000Z"/>
    <onemind v="1" id="sim-x1" type="drone" seq="42" conf="0.97" lc="active" tlp="AMBER+STRICT" sim="1">
      <asset mode="AUTO" connection="online" link_rssi="-67" agl="40.0"
             task="tsk-01jbx7k2m9q4r8s6t0v2w4y6z8" task_state="executing"/>
      <command action="act-01jbx7m3c4d5e6f7g8h9j0k1m2" verb="goto" state="acknowledged" t="1793275199000"/>
    </onemind>
  </detail>
</event>
```

`AMBER+STRICT` with `releasable_to: [homestead]` reaches TAK only when the target's
`TAK_RELEASE_GROUPS` contains `homestead` ([cot-projection.md](cot-projection.md) §3.4). On the
mesh target the same event is sent without `<onemind>` and with trimmed remarks, and with
`<__video>` only if a `site_url` exists.

Over TAK Protocol v1, `contact`, `track`, `status` and `precisionlocation` become typed fields
and the rest stays in `xmlDetail` (whole-element rule; the takproto spike in
[tak-protocols.md](../research/tak-protocols.md) §8 shows the same split for `sim-x1`).

---

## 8. Reverse: robots that other systems put on TAK

A drone or robot that arrives on `tak.cot.out` or `tak.cot.mesh.rx` from another system (an ATAK
UAS tool, a third-party ground station) follows [cot-projection.md](cot-projection.md) §8: it
becomes a `track` (with `kind` from the CoT function, e.g. `multirotor` for `A-M-H-Q`), never a
taskable asset, and it can never be commanded, because asset commands require an own `sim-*` id
in this run ([asset-command.md](asset-command.md) §1). Its `<__video>` becomes a `media[]` entry
and its `<sensor>` a `sensors[]` entry, so the OneMind client can show the stream and the cone.
It is not projected back to TAK (cot-projection gate G5).

---

## 9. Acceptance tests (H9, K3, H1)

Local, on the build machine (decision 0004 #11–12):

1. **Drone type and detail.** The scripted drone `sim-x1` (`MAVLINK20=1`) through the bridge, the
   entity service and the projector produces CoT on `tak.cot.inject` with type `a-f-A-M-H-Q`,
   `how="m-s"`, `opex="s"`, callsign `SIM-X1`, and `<track>`, `<status>`, `<sensor>`. A
   `takproto` decode of the gateway's v1 output shows `contact`, `track` and `status` as typed
   fields and `sensor` and `__video` in `xmlDetail`.
2. **Robot type.** A scripted ground robot with `HEARTBEAT.type = GROUND_ROVER` projects as
   `a-f-G-E-V`; no fixture produces `a-f-G-U-C-R` or `a-f-G-U-C-V`.
3. **Video URL hygiene.** An entity whose only video URL is `rtsp://127.0.0.1:8554/sim-x1`
   produces no `<__video>`; a registry entry with a tailnet `tak_url` produces it; no output URL
   contains `@` or a query string.
4. **Footprint.** A synthetic ST 0601 stream (video lane `klv_ts_gen.py`) through MediaMTX and the
   KLV extractor produces `OM.drone.sim-x1.footprint` (`u-d-f`, 5 vertices, stale 5 s) at most once
   per second.
5. **Command echo.** An approved `asset.goto` on `sim-x1` produces a `b-m-p-w` marker
   `OM.drone.sim-x1.cmd.{action_id}` and a `<command>` child that moves through `dispatched`,
   `acknowledged`, `executed`; the marker is removed after `executed`. A denied or expired action
   produces no TAK message at all.
6. **No TAK command path.** A `t-r` CoT on `tak.cot.mesh.rx` that links to `OM.drone.sim-x1`
   produces a `svc.policy.command.propose` request and no `cmd.fleet.*` message until a human confirms.

Owner hardware (`docs/OWNER-CHECKLIST.md`; not passable here):

- ATAK shows `SIM-X1` with the UAV icon, correct heading and speed, and opens the video from the
  marker (FEATURES H9, K3).
- With the drone flying east and the camera at `azimuth=0`, record whether ATAK draws the cone to
  the north (absolute) or to the east (relative).
- Record whether ATAK draws its own KLV footprint next to OneMind's.
- A ground robot shows a sensible ground-vehicle icon.

---

## 10. Unverified and open

- `MAV_TYPE` enum names (MAVLink `common.xml` not re-read in this lane).
- ATAK icons for `a-f-G-E-V`, `a-f-S-X`, `a-f-U-S-U` and `a-f-G-E-V-C`; only the drone icons are
  confirmed from the ATAK-CIV asset list.
- `<sensor>` azimuth reference and colour units; `<status readiness>` meaning.
- `<__video>` `uid`, multiple `ConnectionEntry` children, `bufferTime` meaning, and the ATAK 5.6
  protocol list.
- Whether ATAK draws a duplicate footprint from embedded KLV.
- TAK Server 5.8 `/Marti/api/video` (paths read from 5.7 source only).
- ATAK behaviour for `t-*` tasking and `y-*` replies; the CoT an ATAK user sends to task an asset.
- `tel.fleet.drone.{id}.sensor` and `.footprint` wait on owner ruling BUS-P7 (BUS-DESIGN §10); the
  profile uses them as BUS-DESIGN does until then.
- `evt.vehicle.>` is not stored by any stream (ONT-P13), so own-vehicle command outcomes are
  not echoed until that is ruled.
- One sensor and one video stream per asset in `v: 1`.
