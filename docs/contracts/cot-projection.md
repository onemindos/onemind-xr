# Contract: CoT projection (entities to TAK, and TAK back to entities)

Status: M0, 2026-10-29. Version `v: 1`. Nothing here is deployed.
Ontology: [ONTOLOGY.md](../design/ONTOLOGY.md) §3 (CoT column) and §7.3 (markings).
Entity record: [entity.md](entity.md). Bus: [BUS-DESIGN.md](../design/BUS-DESIGN.md).
Own assets (drones, robots, vehicles): [robotics-cot-profile.md](robotics-cot-profile.md).

This contract defines two directions:

- **Out (projection).** How a fused entity on `ent.{entity_type}.{entity_id}.state` becomes a
  Cursor-on-Target (CoT) event for TAK: which entities go, the CoT type, the uid, the times, the
  detail elements, the AOI and marking gates, the rate limits, and which subject carries it.
- **In (reverse).** How CoT that arrives from TAK Server or the TAK mesh becomes observations
  that the entity service turns into entities.

The projector and the entity service are omos services. Per onemind-xr `CLAUDE.md`, omos is
the source of truth for every subject and bucket; this file cites the omos file for each one.
The OneMind client never speaks TAK directly. It reads entities from the bus and sends CoT only
through the projector ([tak-protocols.md](../research/tak-protocols.md) Level 1, design item 5).

omos is read at commit `62e3d22b`; the mesh gateway at branch `feat/tak-mesh-gateway`,
commit `977b7eb2`.

---

## 1. Subjects, streams and services

| Purpose | Subject or bucket | Defined in (omos) | State |
|---|---|---|---|
| Entity input | `ent.{entity_type}.{entity_id}.state` (core subscription today; ordered consumer on `ENT` once it exists) | `CLAUDE.md` grammar ("fused entity state"); stream `platform/nats/streams/ent.yaml` | pattern exists; `ENT` **proposed** ([entity.md](entity.md) §1) |
| Out to TAK Server | `tak.cot.inject` | `CLAUDE.md` grammar ("CoT → TAK inject"); consumed by NatsBridgePlugin, `services/takserver/plugin/tak.server.plugins.NatsBridgePlugin.yaml` (`natsConsumerSubject: tak.cot.inject`); stored by `platform/nats/streams/cot.yaml` (`tak.cot.>`) | exists; whether the plugin is loaded live is **unverified** ([tak-server-and-bridges.md](../research/tak-server-and-bridges.md) §4) |
| Out to the TAK mesh | `tak.cot.mesh.tx` | mesh gateway `services/bridge/bridges/tak/main.go` lines 10, 37, 214 (branch `feat/tak-mesh-gateway`); under the `tak.cot.>` grammar entry, which omos `CLAUDE.md` line 94 defines as "CoT from TAK", although this subject carries CoT **toward** the mesh; owner proposal CP-4 (§11) | exists on the branch; no PR yet (FEATURES A7) |
| In from TAK Server | `tak.cot.out` | NatsBridgePlugin `natsProducerSubject: tak.cot.out` (same YAML); `docs/NATS-DOCTRINE.md` §9.8; stream `cot.yaml` | exists; the TAK→bus leg is **not proven live** (no cot-tap Deployment, no plugin log line; [tak-server-and-bridges.md](../research/tak-server-and-bridges.md) §4–5) |
| In from the TAK mesh | `tak.cot.mesh.rx` | mesh gateway `main.go` lines 9, 36, 167–189; stored by `COT` (`tak.cot.>`) at the hub and `SITE_COT` at a site; ratified with CP-4 (§11) | exists on the branch |
| Mesh copy to TAK Server | `tak.cot.inject`, only when `MESH_TO_TAK_SERVER=true` | mesh gateway `main.go` lines 13, 97, 184–188 | default `false` (decision 0004 #6) |
| Areas of interest and geofences | KV `geofences` | `platform/nats/streams/kv-geofences.yaml` | bucket exists; A6 use **proposed** ([BUS-DESIGN.md](../design/BUS-DESIGN.md) §5) |
| Active missions and their AOIs | KV `mission-state` | `platform/nats/streams/kv-mission-state.yaml` | bucket exists; record shape in [mission-task.md](mission-task.md) |
| Video URLs for TAK | KV `OVERWATCH_VIDEO` | `platform/nats/streams/kv-overwatch-video.yaml`; no writer in omos today ([video-and-plugins.md](../research/video-and-plugins.md) Unverified) | **proposed** writer (video registry) |
| Ephemeral client sends (chat, SPI) | `svc.cot.send` (request/reply, served by the projector) | grammar `svc.{service}.{endpoint}`; [BUS-DESIGN.md](../design/BUS-DESIGN.md) line 131 | **proposed** |
| Projector on/off | `cmd.projector.disable`, `cmd.projector.enable` | `platform/omos/tak-projector.yaml` lines 26–28, 412–413 | live, but **three tokens, outside** `cmd.{domain}.{entity}.{action}`; see proposal CP-1 (§11) |

**Who may publish the outbound subjects.** Only the projector publishes `tak.cot.inject`
(hub) and `tak.cot.mesh.tx` (site), so that every CoT that reaches TAK has passed the gates in
§3 ([security-and-trust.md](../research/security-and-trust.md) T4;
[BUS-DESIGN.md](../design/BUS-DESIGN.md) lines 116–117, 266, 281). Two exceptions exist today
and are on the way out:

| Publisher today | What it does | Fate |
|---|---|---|
| Redpanda Connect feeds | push aircraft, ships and transit straight to `tak.cot.inject` | retired by FEATURES A1 (run prompt "Facts already verified"); their data reaches TAK only as projected entities |
| The projector's own `intel.>` path | turns `intel.*` events into `INTEL.{id}` CoT without an entity (`tak-projector.yaml` lines 231–297, 419–426) | retired once the entity service makes `event` entities from `intel.*`; until then its output keeps the `INTEL.` uid prefix so the reverse path drops it (§8.2) |
| Mesh gateway with `MESH_TO_TAK_SERVER=true` | copies mesh CoT to `tak.cot.inject` | allowed only in the owner's local Mac config (decision 0004 #6); see §7 |

**Payload on every `tak.cot.*` subject is CoT XML** (UTF-8). The plugin converts XML with
`cotStringToDataMessage` ([tak-server-and-bridges.md](../research/tak-server-and-bridges.md) §4).
The gateway parses XML on `tak.cot.mesh.tx` and chooses XML or TAK Protocol v1 on the wire by
negotiation (`main.go` lines 214–232; [tak-protocols.md](../research/tak-protocols.md) §2).
The projector never sends protobuf on NATS.

---

## 2. Two projectors, two targets

| Projector | Runs | Reads | Publishes | TAK audience |
|---|---|---|---|---|
| Hub projector (`omos-tak-projector`, A6) | cluster, `omos` namespace (`platform/omos/tak-projector.yaml`) | hub `ENT` | `tak.cot.inject` | everyone on TAK Server in the plugin's groups (`[__ANON__, Red]` in the JAR-side YAML; which config the live pod loads is **unverified**, [tak-protocols.md](../research/tak-protocols.md) §7) |
| Site projector | site host next to the leaf and the mesh gateway ([BUS-DESIGN.md](../design/BUS-DESIGN.md) line 282) | site `SITE_ENT` plus the `HUB_ENT` mirror | `tak.cot.mesh.tx` | ATAK, iTAK and WinTAK peers on the site LAN |

Both run the same code with a different `PROJECTOR_TARGET` (§10). The site projector keeps
working when the site is cut off from the hub, because it reads only site streams
([BUS-DESIGN.md](../design/BUS-DESIGN.md) §8 site topology).

**Routing groups are fixed per target.** NatsBridgePlugin injects every message into the groups
in its own config ([tak-server-and-bridges.md](../research/tak-server-and-bridges.md) §4). The
projector cannot choose a TAK group per message, and the CoT `<__group>` element does not route
on TAK Server (it is the sender's team colour and role, [tak-protocols.md](../research/tak-protocols.md)
§2 typed field Group). So the marking gate (§3.4) decides per **target**, not per message group.
Finer routing needs one plugin instance or TAK Server input per group (*unverified*, owner
decision).

---

## 3. Which entities are projected (the gates)

The projector evaluates every entity update against these gates, in order. The first gate that
says no stops the entity. If the entity was on TAK before, the projector sends a removal (§5.3).

| # | Gate | Projected only if | Source |
|---|---|---|---|
| G1 | Enabled | projector is enabled and the target is configured | `tak-projector.yaml` lines 55, 345 |
| G2 | Alive and located | `live: true`; `lifecycle` ∈ `active`, `stale`; `locatable` ∈ `traits`; `lat`/`lon` in range (or `geo` set) | [entity.md](entity.md) §4.1, §5; `tak-projector.yaml` lines 166–171 |
| G3 | Type allowed | `entity_type` has a row in §4.1 that is not "never" | §4.1 |
| G4 | People safety | never a `person` entity; never a `track` with `kind: person`; never a `station` (RF stations are often people) unless the owner enables it | [entity.md](entity.md) §5 "Person safety"; ONTOLOGY §3.3; BUS-DESIGN D9 |
| G5 | Not already on TAK | no `sources[]` entry with `subject` ∈ `tak.cot.out`, `tak.cot.mesh.rx` and `last_seen_t` newer than now − `stale` | [fusion-and-tracking.md](../research/fusion-and-tracking.md) Risk 1 (duplicate dots) |
| G6 | Egress allowed | `indicators.egress_tak: true` | [entity.md](entity.md) §4.1; [lattice-patterns.md](../research/lattice-patterns.md) mapping row `indicators.egress_tak` |
| G7 | Marking | §3.4 | [security-and-trust.md](../research/security-and-trust.md) §5 enforcement point 2 |
| G8 | Relevance tier (A6) | §3.2 | FEATURES A6, D1 |
| G9 | Budget | §3.3 cap and §6 rate limits | layer-delta `tak` budget ([layer-delta.md](layer-delta.md) line 125) |

G5 means TAK-native contacts (ATAK EUDs, markers that TAK users drew) are never sent back to TAK
under a second uid, and a mesh-only contact is never forwarded to TAK Server by the projector.
The only mesh-to-server path is `MESH_TO_TAK_SERVER` (§7).

### 3.1 Who sets `egress_tak`

The entity service sets `indicators.egress_tak` when it builds the record:

| Entity | Default | Why |
|---|---|---|
| own assets (`sim-*` in this run), places, zones, structures | `true` | they exist to be shared with the team |
| world feed entities from CLEAR feeds (aircraft, vessels, hazards) | `true` | relevance is then decided by the AOI gate |
| any `track` with `kind: person`, any `person`, `station`, `agent`, `team`, `node` | `false` | G4, and nothing useful to draw |
| anything sourced only from TAK | `false` | already on TAK (G5) |

An operator can change it with the `entity.override` action (`field: indicators.egress_tak`),
which goes through policy like every edit ([entity.md](entity.md) §6).

### 3.2 Relevance tier (the A6 AOI filter)

An entity that passed G1–G7 is projected when **any** of these is true:

| Tier reason | Test |
|---|---|
| Own asset | `template: asset` and `taskable` ∈ `traits` and the id starts `sim-` (decision 0004 #13) |
| Tasked or targeted | `current_task` is not null, or the entity is the `targets` end of an active task in KV `mission-state` |
| In an AOI | its position (or any vertex of its `geo`) is inside an active zone from KV `geofences` with `geo.kind` ∈ `aoi`, `keep_in`, `keep_out`, `hazard`, `control_area`, or inside the `aoi` zone of an active mission; altitude bands `alt_min_m`/`alt_max_m` apply when set |
| Pinned map content | `entity_type` ∈ `place`, `zone`, `structure` and `marking` passes G7 |
| Starred | `indicators.starred: true` |

Rules:

- **Hysteresis.** An entity that leaves every AOI stays projected for `AOI_EXIT_GRACE_S`
  (default 60 s) before the removal, so tracks on a boundary do not flicker.
- **Recompute** on every entity update and on every change in `geofences` or `mission-state`
  (KV watch). A new AOI projects the entities already inside it at once.
- **AOI filtering is done here, not with subjects.** The grammar has no geographic token, and
  none will be added ([BUS-DESIGN.md](../design/BUS-DESIGN.md) BUS-P8).
- Point-in-polygon runs on WGS-84 coordinates with a planar test. That is fine for AOIs of a few
  tens of kilometres; larger or polar AOIs need a geodesic test (*unverified* error size).

### 3.3 Cap per target

At most `MAX_PROJECTED` entities are live on a target at once (default **500** for the hub,
**100** for a site; estimates from the `tak` layer size in
[layer-snapshot.md](layer-snapshot.md) §1, not measured). Over the cap, the projector keeps
entities in this order and stops the rest, logging the count:

1. own assets; 2. tasked or targeted; 3. zones and places; 4. starred; 5. others by distance to
   the nearest AOI centroid.

### 3.4 Marking gate (G7)

Every entity carries `marking.tlp` and `marking.releasable_to[]` ([entity.md](entity.md) §4.1;
ONTOLOGY §7.3). The gate compares them with the target's configuration:

| `marking.tlp` | Hub target (`tak.cot.inject`) | Site mesh target (`tak.cot.mesh.tx`) |
|---|---|---|
| `CLEAR`, `GREEN` | yes | yes |
| `AMBER`, `AMBER+STRICT` | only if `releasable_to` contains a group in `TAK_RELEASE_GROUPS` for that target (the owner's own TAK, e.g. `homestead`) | same, with the site's `TAK_RELEASE_GROUPS` |
| `RED` | never | never |

Source: [security-and-trust.md](../research/security-and-trust.md) §5 ("Only CLEAR/GREEN (or AMBER
to the owner's own TAK) go out"). TAK groups are mapped one-to-one to OneMind groups on the TAK
side (same source, enforcement point 3). Federation partners receive OneMind data only through
TAK Server federation group mapping, configured by the owner
([tak-protocols.md](../research/tak-protocols.md) Level 3); the projector never targets a
partner directly.

---

## 4. CoT type

### 4.1 Rules

1. If the entity has `cot_type` and it matches `^[abtuy](-[A-Za-z0-9.]+)+$` and passes the
   civilian check below, use it. The entity service precomputes it from this table
   ([entity.md](entity.md) §4.1 `cot_type`).
2. Otherwise build it from the table, using affiliation and dimension from §4.2.
3. Own drones, robots and vehicles use [robotics-cot-profile.md](robotics-cot-profile.md) §2.
4. **Civilian check.** The projector never emits an affiliation of `h` (hostile), `j` (joker) or
   `k` (faker), never a `t-k-*` (strike) or `t-e-*` (engage, deprecated) type, and never a
   weapon atom (`a-.-G-E-W-*`, `a-.-U-W-*`). A record that asks for one is projected with
   affiliation `s` (suspect) or not at all, and the projector counts it
   ([lattice-patterns.md](../research/lattice-patterns.md) R2; ONTOLOGY §6.2). Today's projector
   maps `threat`, `hostile` and `conflict_event` to `a-h-G` (`tak-projector.yaml` lines 92,
   99–100); that mapping is removed.
5. An `entity_type` with no row is **not projected**, and the projector counts it. Today's
   projector falls back to `a-f-G-U-C` "friendly combat unit" (`tak-projector.yaml` line 105),
   which mislabels anything unknown.

Every `a-` type below was checked against MITRE `CoTtypes.xml` (Apache-2.0 copy,
https://github.com/dB-SPL/cot-types/blob/main/CoTtypes.xml, read with `gh api` on 2026-10-29;
the file is the canonical table per [standards.md](../research/standards.md) "CoT types"). The
`b-`/`u-` drawing types come from the TAK examples (C3 in
[lattice-patterns.md](../research/lattice-patterns.md)). `{a}` is the affiliation letter (§4.2).

| `entity_type` | `kind` (or condition) | CoT type | `CoTtypes.xml` meaning | Notes |
|---|---|---|---|---|
| `drone`, `robot`, own `vehicle` | — | robotics profile | — | [robotics-cot-profile.md](robotics-cot-profile.md) §2 |
| `aircraft` | `fixed_wing` or airliner category | `a-{a}-A-C-F` | Air/Civ/fixed | default `{a}` = `n` |
| | `rotary`, `helicopter` | `a-{a}-A-C-H` | Air/Civ/rotary | |
| | `balloon`, `blimp` | `a-{a}-A-C-L` | Air/Civ/Blimp | |
| | unknown category | `a-{a}-A` | Air (track) | OneMind does not classify aircraft as military (BUS-DESIGN D9) |
| `vessel` | `cargo` | `a-{a}-S-X-M-C` | Surface/CARGO | default `{a}` = `n` |
| | `tanker` | `a-{a}-S-X-M-O` | Surface/OILER/TANKER | |
| | `passenger` / `ferry` | `a-{a}-S-X-M-P` / `a-{a}-S-X-M-F` | PASSENGER / FERRY | |
| | `fishing` | `a-{a}-S-X-F` | Surface/FISHING | |
| | `tug` / `towing` | `a-{a}-S-X-M-T-U` / `a-{a}-S-X-M-T-O` | TUG / TOWING VESSEL | |
| | `leisure` | `a-{a}-S-X-R` | Surface/LEISURE CRAFT | |
| | `hazmat` | `a-{a}-S-X-M-H` | Surface/HAZMAT | |
| | other or unknown | `a-{a}-S-X` | Surface/NON MILITARY | |
| `track` | any (never `person`) | `a-{a}-{dim}` | generic track (`a-.-A`, `a-.-G`, `a-.-S`, `a-.-U` exist) | default `{a}` = `u` |
| `sensor` | — / emplaced | `a-{a}-G-E-S` / `a-{a}-G-E-S-E` | Gnd/Equip/Sensor (/Emplaced) | |
| `camera` | — | `a-{a}-G-E-S-E` plus `<sensor>` and `<__video>` | Emplaced sensor | today `a-f-G-I-S-C`, which is **not** in `CoTtypes.xml` |
| `structure` | — | `a-{a}-G-I-c` at the centroid, plus an optional `u-d-f` footprint (§5.5) | Gnd/Structure/Civilian | settles the ONTOLOGY §3 placeholder |
| `place` | — | `b-m-p-s-m` | TAK spot marker (C3) | |
| `zone` | `geo.shape` = circle / polygon / rectangle / line / corridor | `u-d-c-c` / `u-d-f` / `u-d-r` / `b-m-r` / `u-d-f` (buffered) | TAK shapes and route (C3) | §5.5 |
| `event` | earthquake | `a-n-X-i-g-e` | INCIDENT/GEO/EARTHQUAKE | event area also drawn as `u-d-f` when `geo` is a polygon |
| | wildfire / fire hot spot | `a-n-X-i-f-w` / `a-n-X-i-f-h` | FIRE/WILD / FIRE/HOT SPOT | |
| | flood | `a-n-X-i-m-f` | MET/FLOOD | |
| | tornado / cyclone or hurricane / thunderstorm | `a-n-X-i-m-t` / `a-n-X-i-m-c` / `a-n-X-i-m-u` | MET/… | |
| | tsunami / snow or winter / hail / fog / drought | `a-n-X-i-m-n` / `-m-s` / `-m-h` / `-m-g` / `-m-d` | MET/… | |
| | volcano / landslide / avalanche | `a-n-X-i-g-v` / `a-n-X-i-g-l` / `a-n-X-i-g-a` | GEO/… | |
| | pollution / public health / transport accident | `a-n-X-i-e` / `a-n-X-i-h` / `a-n-X-i-t` | INCIDENT/… | |
| | conflict or security report (ACLED) | `a-n-X-i-l` | INCIDENT/SECURITY | replaces today's `a-h-G` |
| | other | `a-n-X-i` | Other/INCIDENT | |
| `alert` | — | **not projected** in v1 | — | §5.6 |
| `person`, `team`, `agent`, `node`, `station` | — | **never** (G4, G6) | — | |
| `satellite` (provisional, [layer-snapshot.md](layer-snapshot.md)) | — | `a-n-P` | Space dimension (in `types.txt`; no `a-.-P` atom in `CoTtypes.xml`) | ATAK icon **unverified** |

Weather alerts (NWS CAP) project as the area shape plus the matching `a-n-X-i-m-*` marker. They
no longer use `a-f-G-U-W-*`, which `CoTtypes.xml` does not list and which reads as a friendly
ground unit ([standards.md](../research/standards.md) Risk 1;
`tak-projector.yaml` lines 109–137).

**Unverified:** how ATAK draws the `X-i` incident atoms. MIL-STD-2525C has no icon for the `X`
(other) dimension in ATAK's `mil-std-2525c` assets as far as this lane checked
([drones-robots-agents.md](../research/drones-robots-agents.md) "CoT for UAVs and UGVs" lists only
air and ground icons). ATAK may show a default marker. The owner checklist step in §12 confirms
it; if ATAK shows nothing useful, the fallback is `b-m-p-s-m` with a `<usericon>` (*unverified*).

### 4.2 Affiliation and dimension

| `disposition` | `{a}` | | `environment` | `{dim}` |
|---|---|---|---|---|
| `friendly` | `f` | | `air` | `A` |
| `assumed_friendly` | `a` | | `surface` | `S` |
| `neutral` | `n` | | `subsurface` | `U` |
| `unknown` | `u` | | `land` | `G` |
| `suspicious` | `s` | | `space` | `P` |
| `pending` | `p` | | other or missing | `X` |
| missing | own asset `f`; world feed `n`; track `u` | | | |

Letters from MITRE `types.txt` (https://github.com/TAK-Product-Center/atak-civ/blob/main/takcot/mitre/types.txt,
read 2026-10-29). There is no `hostile` disposition in OneMind ([entity.md](entity.md) §4.1).
Today's projector sends every aircraft and ship as friendly (`a-f-…`, `tak-projector.yaml`
lines 63–73); world feeds become neutral.

---

## 5. The CoT event

### 5.1 Root attributes

| Attribute | Value | Source |
|---|---|---|
| `version` | `2.0` | `tak-projector.yaml` line 195 |
| `uid` | `OM.{entity_type}.{entity_id}` | `tak-projector.yaml` line 191 (kept, so existing echo filters work) |
| `type` | §4 | |
| `how` | `provenance.how` if it is one of `m-g`, `m-f`, `m-s`, `m-p`, `m-a`, `h-e`, `h-c`; otherwise `m-f` for multi-source entities, `m-p` for single-source feed data | `how` tree in MITRE `types.txt` (`m-s` simulated, `m-f` fused, `m-p` passed through a gateway, `m-a` algorithmic tracker, `h-e` human estimated). Today always `m-g` (line 198) |
| `time` | send time, UTC, `YYYY-MM-DDThh:mm:ss.sssZ` | CoT base schema ([tak-protocols.md](../research/tak-protocols.md) §1) |
| `start` | `max(entity.t, send time − stale_s)` | keeps a late heartbeat valid |
| `stale` | §5.3 | |
| `access` | **omitted** by default; set only from a per-federation config that names the exact marking string | [security-and-trust.md](../research/security-and-trust.md) §5, R5. Today `Unclassified` (line 202) |
| `opex` | `s` if `indicators.simulated`, `e` if `indicators.exercise`, else omitted | `opex` semantics, [lattice-patterns.md](../research/lattice-patterns.md) (C2) |
| `caveat`, `releasableTo` | omitted | never put OneMind group names there ([security-and-trust.md](../research/security-and-trust.md) §5) |

**uid rules.**

- `entity_id` is already lowercase `[a-z0-9_-]{1,64}` ([entity.md](entity.md) §3), so the uid is
  safe in XML and in NATS headers.
- The uid of one entity never changes. A changed `entity_type` (a `track` classified as a
  `vessel`) keeps `entity_type: track` and changes only `kind` ([entity.md](entity.md) §3), so the
  uid stays stable.
- Child objects use the parent uid plus a suffix: `.area` (event area), `.footprint` (sensor
  footprint), `.wp{n}` (route waypoint), `.cmd.{action_id}` (command echo). ATAK itself adds
  `-fov` to a sensor cone ([drones-robots-agents.md](../research/drones-robots-agents.md) "CoT for
  UAVs and UGVs"), so OneMind never uses that suffix.
- The prefix `OM.` is reserved for the projector. `INTEL.` is reserved for the legacy intel path.
  Nobody else may publish CoT with either prefix.

### 5.2 Point

| Attribute | Value |
|---|---|
| `lat`, `lon` | `location.lat`, `location.lon` (or `lat`/`lon`), 6 decimals (`tak-projector.yaml` lines 205–206) |
| `hae` | `location.hae_m` (metres above the WGS-84 ellipsoid, doctrine §9.2) |
| `ce`, `le` | `uncertainty.ce_m`, `uncertainty.le_m` (1-sigma metres, [entity.md](entity.md) §4.1) |
| unknown value | `9999999.0`, as the projector does today (line 53). The v1 proto comment uses `999999` ([tak-protocols.md](../research/tak-protocols.md) §1); receivers treat any value ≥ 999999 as unknown |

For shapes, the point is the centroid (polygon), the centre (circle) or the first vertex
(route).

### 5.3 Stale time, heartbeat and removal

**Stale.** `stale = send time + stale_s`, where

```
stale_s = clamp(entity.stale  or the type default below,
                lower = 2 × per-uid interval (§6) + 5 s,
                upper = 3600 s for moving entities, 86400 s for place/zone/structure)
```

| Type default when the entity has no `stale` | Seconds | Source |
|---|---|---|
| own assets | 60 | [entity.md](entity.md) §4.3 fleet `Nats-TTL` 60 s |
| aircraft | 60 | `tak-projector.yaml` line 63 |
| vessel | 120 | line 70 |
| track | 120 | line 101 (`unknown_track`) |
| sensor, camera | 600 | lines 84–86 |
| event | 3600 | lines 88–97 |
| place, zone, structure | 86400 | pinned; heartbeated (below) |

**Heartbeat.** A projected entity that has not changed is re-sent every
`min(stale_s / 2, 300 s)` while it stays projected, so ATAK does not grey it out. This matches
the entity service's own heartbeat rule ([entity.md](entity.md) §5 "Change-only publish").

**Entity goes stale.** When `lifecycle` becomes `stale`, the projector stops heartbeating. TAK
clients then grey the marker on their own when the last CoT's `stale` passes.

**Removal.** When an entity fails a gate it passed before (`live: false`, `lost`, `expired`,
left the AOI after the grace period, `egress_tak` turned off, marking tightened, cap), the
projector sends the same uid and type once more with `stale = time + 1 s` and
`<remarks>` "removed". **Unverified:** whether ATAK removes or only greys such a marker, and
whether a TAK delete event (`t-x-d-d` with a `link` to the uid, seen in third-party tools) works
from a server inject. Neither is in `CoTtypes.xml` or the research files. The owner checklist
(§12) tests both; the better one becomes `v: 1` behaviour without a contract break.

### 5.4 Detail elements

Only TAK-registered detail elements are used (XSD list in
[tak-protocols.md](../research/tak-protocols.md) §1), plus one OneMind element in plain XML.
OneMind never uses binary `extensionDetails`: unregistered extension ids are forbidden in public
deployments (same source §2, Level 4).

| Element | When | Content | Source field |
|---|---|---|---|
| `<contact callsign>` | always | `callsign`, else `aliases.name`, else `entity_id`; at most 64 characters (`tak-projector.yaml` line 182). No `endpoint`: the projector is not a reachable contact | `callsign`, `aliases.name` |
| `<track course speed>` | `trackable` | degrees true; m/s | `location.course_deg`, `location.speed_mps` |
| `<status battery>` | `health.battery_pct` set | integer percent | `health.battery_pct` |
| `<precisionlocation geopointsrc altsrc>` | own assets with GPS | `GPS` / `GPS` (`DTED0` etc. not used) | provenance |
| `<remarks>` | always | one line: `OneMind {kind or type} · conf {confidence:.2f} · {lifecycle} · SIMULATED` (the last part only when `indicators.simulated`) · source summary | `confidence`, `lifecycle`, `sources[].integration` |
| `<link>` | shapes and children | vertices (`point`), parent (`uid`, `relation="p-p"`) | §5.5 |
| `<color argb>`, `<strokeColor value>`, `<fillColor value>`, `<strokeWeight value>`, `<labels_on value>` | zones, event areas, footprints | §5.5 | `geo.kind` |
| `<sensor …>` and `<__video …>` | observable assets and cameras | [robotics-cot-profile.md](robotics-cot-profile.md) §4–5 | `sensors[]`, `media[]` |
| `<__group name role>` | **not sent** for projected entities | — | — |
| `<_flow-tags_ omos-tak-projector="{time}">` | always | marks projector output (second echo filter) | `tak-projector.yaml` lines 212–213 |
| `<onemind …>` | hub target only (dropped on the mesh, §6.2) | `v="1" id="{entity_id}" type="{entity_type}" seq="{seq}" conf="{confidence}" lc="{lifecycle}" tlp="{marking.tlp}" sim="{0\|1}"` | entity record |

Notes:

- `<__group>` is the sender's team colour and role for an EUD ([tak-protocols.md](../research/tak-protocols.md)
  §2). Today's projector sends `name="__ANON__" role="Team Member"` on every entity
  (`tak-projector.yaml` lines 221–223). It does not route on TAK Server (§2), and ATAK may show
  non-people as team members, so it is dropped. **Unverified:** that ATAK renders the markers the
  same without it; checked in §12.
- `<onemind>` carries no secrets and no personal data. The ATAK plugin (FEATURES F-lane) reads it
  for provenance and confidence; other TAK clients ignore unknown elements. Over TAK Protocol v1
  it travels in `xmlDetail` under the whole-element rule
  ([tak-protocols.md](../research/tak-protocols.md) §2).
- No `<link relation="p-s">` per source: those would point at observation ids that do not exist
  in TAK. The source summary goes in `<remarks>` and `<onemind>`.

### 5.5 Shapes (zones, event areas, structure footprints, routes)

| `geo.shape` | CoT | Geometry detail |
|---|---|---|
| `polygon` | `u-d-f` | one `<link point="lat,lon"/>` per vertex, closed (first vertex repeated) |
| `circle` | `u-d-c-c` | centre in `<point>`; radius in `<shape><ellipse major minor angle/></shape>` (attribute names **unverified**; read from atak-civ `takcot/examples` before building) |
| `rectangle` (axis-aligned bbox) | `u-d-r` | four `<link point>` corners (*unverified* corner order) |
| `line` (route) | `b-m-r` | `<link uid="{uid}.wp{n}" type="b-m-p-w" relation="c" point="lat,lon"/>` per waypoint ([lattice-patterns.md](../research/lattice-patterns.md) C3: route links `b-m-p-w` with `relation='c'`) |
| `corridor` | `u-d-f` | the corridor buffered to a polygon by the projector |

Colours (`argb` signed 32-bit, as the projector writes `-65536` for red, line 291):

| `geo.kind` | Stroke | Fill |
|---|---|---|
| `aoi` | yellow | none |
| `keep_in` | green | none |
| `keep_out` | red | red, 25 % alpha |
| `hazard` (and event areas) | orange | orange, 25 % alpha |
| `control_area` | blue | none |
| `footprint` | cyan | cyan, 15 % alpha |
| `route` | white | — |

Shapes larger than `MAX_VERTICES` (default 200, estimate) are simplified (Douglas-Peucker) before
sending. Shapes never go to the mesh target unless they are site zones (§6.2).

### 5.6 Alerts

`alert` entities are not projected as their own markers in `v: 1`. CoT has no verified alert
type, and `<emergency>` is the EUD distress element (ONTOLOGY §3 alert row;
[tak-protocols.md](../research/tak-protocols.md) §1). Instead, while an alert with
`level: warning` is `open`, the `about` entity's `<remarks>` starts with `ALERT: {alert_kind}`.
A TAK-visible alert type is an open item (§13).

### 5.7 Example: a world vessel inside an AOI

```xml
<?xml version="1.0" encoding="UTF-8" standalone="yes" ?>
<event version="2.0" uid="OM.vessel.mmsi-367000001" type="a-n-S-X-M-C" how="m-p"
       time="2026-10-29T12:00:00.000Z" start="2026-10-29T12:00:00.000Z" stale="2026-10-29T12:02:00.000Z">
  <point lat="29.712340" lon="-95.012345" hae="0.0" ce="10.0" le="9999999.0"/>
  <detail>
    <contact callsign="EXAMPLE CARRIER"/>
    <track course="92.0" speed="6.10"/>
    <remarks>OneMind cargo · conf 0.91 · active · aisstream</remarks>
    <_flow-tags_ omos-tak-projector="2026-10-29T12:00:00.000Z"/>
    <onemind v="1" id="mmsi-367000001" type="vessel" seq="88" conf="0.91" lc="active" tlp="CLEAR" sim="0"/>
  </detail>
</event>
```

The callsign and MMSI are invented for the example. The own-drone example is in
[robotics-cot-profile.md](robotics-cot-profile.md) §7.

---

## 6. Rate limits and size

### 6.1 Per target

| Limit | Hub (`tak.cot.inject`) | Site mesh (`tak.cot.mesh.tx`) | Today |
|---|---|---|---|
| Per-uid minimum interval, own assets | 1 s | 5 s | 5 s for all (`MIN_INTERVAL_SEC`, `tak-projector.yaml` lines 49, 357–359) |
| Per-uid minimum interval, other moving entities | 5 s | 15 s | 5 s |
| Shapes, places, structures | on change, then heartbeat | on change, then heartbeat | — |
| Global ceiling | 50 msg/s | 10 msg/s | none |
| Over the ceiling | queue keeps only the newest message per uid; priority order of §3.3 | same | — |

All numbers are **estimates**. The hub ceiling is the `tak` layer budget in
[layer-delta.md](layer-delta.md) (line 125: under 50 msg/s); ATAK PLI is every 5–30 s per device
(same line). The mesh numbers are deliberately low because a team LAN may include radio bridges
(Meshtastic PLI default every 5 minutes, packets over 231 bytes fountain-coded,
[edge-and-ddil.md](../research/edge-and-ddil.md) line 185). The A6 and A7 tests measure them.

Rate limiting happens **after** the gates: a dropped intermediate update is fine, because the
next one carries the full state (CoT has no deltas).

### 6.2 Mesh profile

For `tak.cot.mesh.tx` the projector:

- sends only own site assets, site zones, active-mission entities and tasked entities (no world
  feeds unless `MESH_WORLD_FEEDS=true`);
- drops `<onemind>` and trims `<remarks>` to 80 characters;
- keeps each CoT XML under `MESH_MAX_BYTES` (default 1,200 bytes, estimate below a 1,500-byte
  Ethernet MTU; v1 encoding shrinks it further, 281 vs 621 bytes in the spike,
  [tak-protocols.md](../research/tak-protocols.md) §8).

### 6.3 Publishing

| Target | How | Headers |
|---|---|---|
| `tak.cot.inject` | core publish, as today (`tak-projector.yaml` line 368). `COT` stores it because it captures `tak.cot.>` | `Nats-Msg-Id: {uid}:{time_ms}`; `OneMind-Marking`, `OneMind-Releasable-To` copied from the entity; `OneMind-Projector: {site or hub}` |
| `tak.cot.mesh.tx` | **core publish with no reply subject**; delivery is confirmed by the gateway's `tx` counter on `tel.fleet.edge.{id}.status` (`main.go` lines 231, 243–257) | same |

Why no request or JetStream publish on `tak.cot.mesh.tx`: the gateway answers any message that
has a reply subject with `ok` (`main.go` lines 215–219, 232). A JetStream publish carries a reply
subject for its PubAck, so the gateway's `ok` and the stream's PubAck would race on the same
inbox ([BUS-DESIGN.md](../design/BUS-DESIGN.md) §12, PubAck race, reasoned and not yet run). A
core publish has no reply subject, so there is nothing to race. BUS-DESIGN §3.3 now says the same
(aligned 2026-10-29); the A7 test confirms it. `Nats-Msg-Id` uses the send time,
not `seq`, because heartbeats repeat a `seq` and the `COT` stream's 2-minute dedup window would
drop them (`cot.yaml` `duplicateWindow: 2m`).

---

## 7. `MESH_TO_TAK_SERVER`

| Setting | Behaviour | Where allowed |
|---|---|---|
| `false` (**default**) | Mesh and TAK Server stay separate. Mesh contacts reach the bus on `tak.cot.mesh.rx` and become entities; the projector does not forward them to TAK Server (gate G5) | everywhere (gateway `main.go` line 97 default; decision 0004 #6) |
| `true` | The gateway also copies every received mesh CoT to `tak.cot.inject` (`main.go` lines 184–188), so mesh devices appear on TAK Server | only the owner's local Mac config, for testing (decision 0004 #6) |

When `true`, two things are required so that the copy does not echo or bypass the gates. Both
are changes to the gateway, to be made in the A7 PR (proposal CP-2, §11):

1. The gateway skips uids that start `OM.` or `INTEL.`. Otherwise a site projector's mesh output,
   heard by a second gateway, is injected into TAK Server a second time, without the hub's
   marking gate.
2. The gateway's own presence SA (`GATEWAY_COT_TYPE`, default `a-f-G-E-S`, `main.go` line 99) is
   not copied.

The gateway already drops its own multicast datagrams looped back within 10 s
(`mesh.go` lines 28, 211), and `Nats-Msg-Id` de-duplicates two gateways hearing the same SA
(`main.go` line 170; [tak-server-and-bridges.md](../research/tak-server-and-bridges.md) §6).

---

## 8. Reverse direction: TAK to entities

### 8.1 Inputs

The entity service consumes exactly two subjects inside `tak.cot.>`, as one input class
(`tak` integration), from the `COT` stream (hub) or `SITE_COT` (site):

| Subject | Producer | Headers it can use |
|---|---|---|
| `tak.cot.out` | NatsBridgePlugin, cot-tap or the planned `tak-client` ([tak-protocols.md](../research/tak-protocols.md) Recommendation 1) | none today |
| `tak.cot.mesh.rx` | mesh gateway | `Nats-Msg-Id`, `Tak-Gateway`, `Tak-Delivery` (`mesh`/`direct`/`udp`), `Tak-Proto`, `Tak-From`, `Tak-Undecoded-Extensions` (`main.go` lines 168–178) |

It does **not** consume `tak.cot.inject` or `tak.cot.mesh.tx`. Those carry OneMind's own output
and, until A1, raw feed CoT from Redpanda Connect; reading them would ingest every entity twice.
This is why the input is two named subjects and not the `tak.cot.>` wildcard. Both payloads are
the same CoT XML ([tak-server-and-bridges.md](../research/tak-server-and-bridges.md) "Client
contract consequences"), and per decision 0004 #7 the raw wire stays on `tak.cot.*` while the
entity service derives per-uid state.

### 8.2 Drop before parsing

| Drop | Test | Why |
|---|---|---|
| OneMind echoes | uid starts `OM.` or `INTEL.`, or `<_flow-tags_>` has `omos-tak-projector` | [entity.md](entity.md) §5 "Echo filter"; [fusion-and-tracking.md](../research/fusion-and-tracking.md) Risk 1 |
| Protocol control | type `t-x-takp-*`, `t-x-c-t`, `t-x-c-t-r`, or uid ending `-ping` | negotiation and keep-alive ([tak-protocols.md](../research/tak-protocols.md) §2–3, §6) |
| Mission sync | type `t-x-m-c*` | handled by the `tak-client` mission worker, not as entities ([tak-protocols.md](../research/tak-protocols.md) Level 1 item 2) |
| Chat | type `b-t-f*` | GeoChat goes to the chat path (Hermes), not to entities (*unverified* type prefix; GeoChat XSD in [tak-protocols.md](../research/tak-protocols.md) §1) |
| Undecodable | `Tak-Undecoded-Extensions` > 0 and no position | keep as an observation only, counted |

### 8.3 Mapping

| CoT | Becomes | Rule |
|---|---|---|
| `a-*` with `<takv>` or `<contact endpoint>` (an EUD's own PLI) | `person` (a consenting team member) | the only path to `ent.person.*` ([entity.md](entity.md) §3; BUS-DESIGN D9) |
| `a-*` without `<takv>` (a marker a TAK user dropped) | `track`, `kind` from the CoT function | never `person`, even if the type is a ground unit |
| `b-m-p-*` (spot, waypoint, map point) | `place` | `pinned: true` only if the user marked it so in a mission; else follows CoT `stale` |
| `u-d-f`, `u-d-c-c`, `u-d-r` | `zone`, `geo.kind: control_area` unless the CoT carries `<__geofence>` | `<__geofence>` → `keep_in` or `keep_out` per its attributes (*unverified* attribute names) |
| `b-m-r` with `b-m-p-w` links | `zone`, `geo.shape: line`, `geo.kind: route` | |
| `t-*` tasking from an ATAK user or plugin | **no entity**, and never executed. It becomes a `svc.policy.*.propose` request (`command` for one asset verb, `task` for a task) on behalf of that TAK user, and waits for policy and human confirm ([asset-command.md](asset-command.md) §4) | [tak-protocols.md](../research/tak-protocols.md) Level 4 item 3; run rule 6 |
| anything else | observation only | |

Field mapping:

| CoT | Entity field |
|---|---|
| `uid` | `entity_id = "tak-" + uid` lowercased if it fits `[a-z0-9_-]`, else `tak-{base32(sha256(uid))[0:26]}`; original in `aliases.ids` as `{type: cot_uid}` ([entity.md](entity.md) §3) |
| `type` | `cot_type`. Affiliation `h`, `j` or `k` is rewritten to `s` and `disposition: suspicious`; the original string goes in `meta.cot_type_original` (civilian scope, BUS-DESIGN D9) |
| affiliation, dimension letters | `disposition`, `environment` (§4.2 in reverse; `o` and `x` → `unknown`) |
| `how` | `provenance.how` |
| `time`, `start`, `stale` | `t` (from `time`, epoch ms), `stale` = `stale − time` in seconds; `Nats-TTL` on `ENT` = CoT stale ([entity.md](entity.md) §4.3) |
| `<point lat lon hae ce le>` | `location`, `uncertainty.ce_m`, `le_m`; values ≥ 999999 → unknown |
| `<contact callsign endpoint>` | `callsign`, `aliases.ids {type: tak_callsign}`; the endpoint is kept only in the contact table, not in the entity |
| `<__group name role>` | `relationships member_of team.team-{name lowercased}`; `status.role` |
| `<track course speed>` | `location.course_deg`, `speed_mps` |
| `<status battery>` | `health.battery_pct` |
| `<takv device platform os version>` | `meta.takv` |
| `<__video>`, `<sensor>` | `media[]`, `sensors[]` ([robotics-cot-profile.md](robotics-cot-profile.md) §4–5 in reverse) |
| `opex` `s` / `e` | `indicators.simulated` / `indicators.exercise` |
| `access`, `caveat`, `releasableTo` | `cot_security`, verbatim ([security-and-trust.md](../research/security-and-trust.md) §5) |
| header `Tak-Gateway`, subject | `sources[]` entry (`integration: tak-mesh` or `tak-server`) |

Marking and flags on TAK-sourced entities:

| Input | `marking.tlp` | `indicators.egress_tak` |
|---|---|---|
| `tak.cot.out` | `AMBER+STRICT` (homestead default, ONTOLOGY §7.3) | `false` |
| `tak.cot.mesh.rx` | `AMBER+STRICT`; the mesh is untrusted input ([security-and-trust.md](../research/security-and-trust.md) T4) | `false` |
| any input with a non-empty `access` other than an unclassified value | `AMBER+STRICT` until a human reviews it | `false` |

**Same uid on both paths.** An EUD heard on the mesh and through TAK Server has one `cot_uid`, a
hard key, so the entity service merges the two into one entity with two `sources[]`
([entity.md](entity.md) §5 "Correlation"). Per-source monotonic time drops the older report.

**Delete.** A TAK delete (`t-x-d-d`, *unverified* form) for a uid ends that `tak-*` entity
early (`live: false`). Otherwise TAK entities expire at their CoT stale.

---

## 9. Client view

The client does not project. It sees the result in two ways:

- **What TAK received.** The `tak` layer shows TAK-native entities ([layer-snapshot.md](layer-snapshot.md)
  `tak` row). A planned `svc.cot.status` endpoint (not in this version) may later list the uids
  live on each target.
- **Sending.** Persistent items (markers, routes, geofences) are created as entities through
  `entity.create` in policy ([entity.md](entity.md) §6), so the bus holds them first (onemind-xr
  `CLAUDE.md` rule 3) and the projector sends them by the normal gates. Only ephemeral items
  (GeoChat, a sensor point of interest `b-m-p-s-p-i`, which is in `CoTtypes.xml`) go through
  `svc.cot.send`. Its request body is defined with FEATURES C2; the projector applies G4, G7 and
  §6 to it.

---

## 10. Projector configuration

Environment variables. The first four exist today (`tak-projector.yaml` lines 44–49 and
470–490); the rest are new. No secret is ever in Git: NATS credentials come from the
`omos-nats-creds` SealedSecret as today, and the per-service user replaces it later
([BUS-DESIGN.md](../design/BUS-DESIGN.md) §7).

| Variable | Default | Meaning |
|---|---|---|
| `NATS_URL`, `NATS_USER`, `NATS_PASSWORD` | in-cluster URL; Secret | as today |
| `TAK_INJECT_SUBJECT` | `tak.cot.inject` | hub output subject |
| `MIN_INTERVAL_SEC` | `5` | per-uid interval for non-own entities (§6.1) |
| `PROJECTOR_TARGET` | `server` (hub) / `mesh` (site) | which subject and profile |
| `MESH_TX_SUBJECT` | `tak.cot.mesh.tx` | site output subject |
| `OWN_MIN_INTERVAL_SEC` | `1` (server) / `5` (mesh) | §6.1 |
| `MAX_RATE_PER_SEC` | `50` / `10` | §6.1 |
| `MAX_PROJECTED` | `500` / `100` | §3.3 |
| `TAK_RELEASE_GROUPS` | empty (CLEAR and GREEN only) | §3.4 |
| `AOI_EXIT_GRACE_S` | `60` | §3.2 |
| `MESH_WORLD_FEEDS` | `false` | §6.2 |
| `MESH_MAX_BYTES` | `1200` | §6.2 |
| `MAX_VERTICES` | `200` | §5.5 |
| `TAK_GROUP` | `__ANON__` | **deprecated**; `<__group>` is no longer sent (§5.4) |

---

## 11. Owner proposals (decision 0004 #8)

| # | Gap | Proposed ruling | Used until ruled |
|---|---|---|---|
| CP-1 | `cmd.projector.disable` / `.enable` have three tokens, outside `cmd.{domain}.{entity}.{action}` (omos `CLAUDE.md`) | `cmd.projector.{instance}.{disable\|enable}`, with `{instance}` = `hub` or the site id; the policy service publishes it after an approved `projector.disable` action (A2) | keep the live three-token subjects; record them with BUS-P4 legacy subjects |
| CP-2 | The gateway forwards OneMind's own CoT when `MESH_TO_TAK_SERVER=true` (§7) | gateway skips `OM.*`, `INTEL.*` and its own presence SA on the inject copy; test in A7 | flag stays `false` (decision 0004 #6) |
| CP-3 | `tak.cot.mesh.tx` sender semantics | core publish without reply; confirm with the gateway `tx` counter (§6.3) | as proposed; BUS-DESIGN §3.3 aligned 2026-10-29 |
| CP-4 | `tak.cot.mesh.rx` and `tak.cot.mesh.tx` sit under `tak.cot.>`, which the locked grammar defines as "CoT from TAK (cot-tap output)" (omos `CLAUDE.md` line 94). `tak.cot.mesh.tx` carries CoT **toward** TAK devices (the inject direction), and the hub `COT` stream captures all of `tak.cot.>` (`platform/nats/streams/cot.yaml` line 11), so it would also store mesh output. The subjects already exist on branch `feat/tak-mesh-gateway` (`main.go` lines 36–37) | Ratify both subjects as the mesh pair: `tak.cot.mesh.rx` = mesh to bus, `tak.cot.mesh.tx` = bus to mesh; amend the grammar line to "CoT to and from TAK"; state that `COT` (hub) and `SITE_COT` (site) store both, under the `COT` caps in [BUS-DESIGN.md](../design/BUS-DESIGN.md) §4.1 | the branch's subjects, as designed here |

None of these creates a new subject pattern. All four are in the combined register, [BUS-DESIGN.md](../design/BUS-DESIGN.md) §10.1.

---

## 12. Acceptance tests (A6, C1, C2, H9)

Local tests run against the local `nats-server` and the mesh gateway from its branch, with
`takproto`-generated peers (decision 0004 #11–12; PREFLIGHT).

1. **Type table.** For each row of §4.1, an entity fixture produces a CoT whose `type` is in
   `CoTtypes.xml` (or is a listed `b-`/`u-` TAK type), and no fixture produces `a-h-*`,
   `a-j-*`, `a-k-*`, `t-k-*` or `t-e-*`.
2. **AOI (A6).** 2,000 aircraft entities, one AOI containing 20 of them: exactly 20 `OM.aircraft.*`
   uids appear on `tak.cot.inject`. Deleting the AOI produces 20 removals within
   `AOI_EXIT_GRACE_S` + 5 s, and then no more messages.
3. **Marking.** An `AMBER+STRICT` entity with `releasable_to: [homestead]` reaches the target only
   when `TAK_RELEASE_GROUPS` contains `homestead`; a `RED` entity never does.
4. **People.** A `track` with `kind: person` and a `person` entity never produce CoT on either
   target ([entity.md](entity.md) §10 test 5).
5. **Echo.** Feeding the projector's own output back on `tak.cot.out` and `tak.cot.mesh.rx`
   creates no new entity and no second CoT.
6. **Rate.** A `sim-*` drone updating at 10 Hz yields at most 1 msg/s on `tak.cot.inject` and at
   most 1 per 5 s on `tak.cot.mesh.tx`; the global ceiling holds with 5,000 entities in AOIs.
7. **Mesh round trip (C1, C2).** A projected `sim-x1` sent on `tak.cot.mesh.tx` is received as v1
   by a `takproto` listener on the mesh group; a `takproto` EUD PLI sent to the mesh becomes
   `ent.person.tak-….state` with `marking.tlp: AMBER+STRICT` and `egress_tak: false`.
8. **`MESH_TO_TAK_SERVER`.** With the default `false`, a mesh PLI never appears on
   `tak.cot.inject`. With `true` and CP-2 in place, it appears once, and an `OM.*` CoT does not.

On-Mac and ATAK checks for `docs/OWNER-CHECKLIST.md` (owner hardware; not passable here):

- ATAK on the owner's LAN shows a projected `sim-x1` and a projected vessel with the expected
  icons, and shows an `a-n-X-i-g-e` earthquake marker (or record what it shows).
- After a removal (§5.3), record whether ATAK deletes or greys the marker; repeat with `t-x-d-d`.
- Markers look the same with and without `<__group>`.

---

## 13. Unverified and open

- Whether NatsBridgePlugin is loaded in the live TAK 5.8 plugins pod, and whether the TAK→bus leg
  works at all live ([tak-server-and-bridges.md](../research/tak-server-and-bridges.md) §4–5).
- ATAK behaviour for: `X-i` incident atoms, removal by `stale`, `t-x-d-d`, markers without
  `<__group>`, the `u-d-c-c` ellipse attributes and `u-d-r` corner order.
- Rate and cap defaults are estimates until A6 and A7 measure them.
- GeoChat type prefix `b-t-f` is from general TAK usage, not from a file read in this run.
- A TAK-visible alert type (§5.6) and per-TAK-group routing (§2) are owner decisions for later.
- Planar point-in-polygon error for large AOIs.
