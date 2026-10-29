# Palantir patterns and their OneMind equivalents

Lane: palantir (Phase 1 / M0, item 12 of `ULTRACODE-PROMPT.md`). Date: 2026-10-29.

This lane uses public material only. It copies **designs**, never Palantir code,
documents, product names, trademarks or branding. OneMind names below are our own.
"Ontology", "action" and "function" are generic computer-science words and are used
here in that generic sense, not as Palantir product names.

## Sources

Palantir primary documentation (fetched 2026-10-29 with WebFetch):

- P1 Ontology overview: https://www.palantir.com/docs/foundry/ontology/overview/
- P2 Object types: https://www.palantir.com/docs/foundry/object-link-types/object-types-overview/
- P3 Link types: https://www.palantir.com/docs/foundry/object-link-types/link-types-overview/
- P4 Interfaces: https://www.palantir.com/docs/foundry/interfaces/interface-overview/
- P5 Action types: https://www.palantir.com/docs/foundry/action-types/overview/
- P6 Action permissions: https://www.palantir.com/docs/foundry/action-types/permissions/
- P7 Function-backed actions: https://www.palantir.com/docs/foundry/action-types/function-actions-getting-started/
- P8 Action log: https://www.palantir.com/docs/foundry/action-types/action-log/
- P9 Functions: https://www.palantir.com/docs/foundry/functions/overview/
- P10 How edits are applied (writeback): https://www.palantir.com/docs/foundry/object-edits/how-edits-applied/
- P11 Why an ontology: https://www.palantir.com/docs/foundry/ontology/why-ontology
- P12 Ontology SDK: https://www.palantir.com/docs/foundry/ontology-sdk/overview/
- P13 AIP overview: https://www.palantir.com/docs/foundry/aip/overview/
- P14 AIP Logic: https://www.palantir.com/docs/foundry/logic/
- P15 Agent tools: https://www.palantir.com/docs/foundry/agent-studio/tools/
- P16 Apollo introduction: https://www.palantir.com/docs/apollo/core/introduction/
- P17 Apollo overview (Hub, Spoke, Spoke Control Plane): https://palantir.com/docs/apollo/core/overview/
- P18 Palantir S-1/A (SEC, 2020): https://www.sec.gov/Archives/edgar/data/1321655/000119312520250103/d904406ds1a.htm (first 200k of 472k characters read)

Maven Smart System and Gotham (secondary, reporting on public records):

- M1 DefenseScoop, 2025-05-23, contract ceiling: https://defensescoop.com/2025/05/23/dod-palantir-maven-smart-system-contract-increase/
- M2 DefenseScoop, 2025-09-12, Marine Corps MARADMIN: https://defensescoop.com/2025/09/12/marine-corps-maradmin-maven-smart-system-mss-palantir-rollout/
- M3 Search-result summaries of DefenseScoop 2026-04-03 / 2026-04-15 (Feinberg memo, transfer to CDAO): https://defensescoop.com/2026/04/15/palantir-maven-smart-system-pentagon-program-transition-feinberg/ (**not fetched directly**)
- G1 Search-result summary of the S-1 describing Gaia, Graph and Dossier (the passage was not inside the 200k characters P18 read): see **Unverified items**

NATS primary documentation:

- N1 JetStream on leaf nodes (domains): https://docs.nats.io/running-a-nats-service/configuration/leafnodes/jetstream_leafnodes
- N2 Sources and mirrors: https://docs.nats.io/nats-concepts/jetstream/source_and_mirror

OneMind repositories (local clones, read 2026-10-29):

- O1 `/Users/danielcarku/Desktop/work/omos/CLAUDE.md`: locked subject grammar, deployment pattern
- O2 `/Users/danielcarku/Desktop/work/omos/services/schemas/command/envelope.schema.json`: command envelope with `approval {required, approver, status, reason}` and result events `evt.command.accepted|rejected|executed|failed`
- O3 `/Users/danielcarku/Desktop/work/omos/services/schemas/registry/entity.schema.json`: entity record, `kind` enum
- O4 `/Users/danielcarku/Desktop/work/omos/platform/nats/streams/*.yaml`: stream and KV CRs (CMD `cmd.>` 720h; EVT with listed `evt.*` domains; MISSION `evt.mission.>`, `evt.map.>`; SYS `evt.sys.>`, `sys.>`, `tel.host.>`; COT `tak.cot.>`; KV `om-schema` history 5, `world-state` TTL 5m, `task-index`, `node-registry`)
- O5 `/Users/danielcarku/Desktop/work/omos/services/onemind-core/README.md`: audited mission requests, Postgres-retained requests and idempotency keys, read credentials cannot mutate
- O6 `/Users/danielcarku/Desktop/work/omos/platform/hermes/agent/AGENTS.yaml`: agent registry, `agents.prompt.{framework}.{owner}.{session}`
- O7 `/Users/danielcarku/Desktop/work/omos/platform/security/README.md`: OpenFGA deployed, used by Backstage Missions only
- O8 `/Users/danielcarku/Desktop/work/omos/services/nex/overwatch/om-overwatch-state/ontology.go`: existing counter-UAS entity types and simplified 2525 affiliation
- O9 `/Users/danielcarku/Desktop/work/onemind-xr/ULTRACODE-PROMPT.md` (Rules 2, 6) and `docs/decisions/0004-owner-defaults-for-the-run.md` (#8, #13)

## Verified facts

### Foundry ontology

- F1. The ontology is "an operational layer for the organization", sitting on datasets,
  virtual tables and models and linking them to real-world counterparts, usable as a
  digital twin. It has **semantic** elements (objects, properties, links) and
  **kinetic** elements (action types, functions) (P1).
- F2. An object type is "the schema definition of a real-world entity or event"; an
  object is one such entity or event. Objects are produced by adding backing
  datasources to the object type (P2).
- F3. A link type is "the schema definition of a relationship between two object
  types". Links are bidirectional with a named side for each type; several distinct
  link types may join the same two types; the docs compare a link type to a join and a
  link to a joined row; many-to-many links are backed by their own datasource (P3).
- F4. An interface is an abstract type describing "the shape of an object type and its
  capabilities"; it is not backed by data; one object type may implement several
  interfaces and one interface many object types (example: a Facility interface over
  Airport, Plant, Hangar) (P4).
- F5. An action type is "the definition of a set of changes or edits to objects,
  property values, and links", including side effects on submission; one action is a
  single transaction (P5). Its parts named in the docs: parameters, rules (for example
  automatically creating a link), submission criteria, permissions, side effects
  (notifications, webhooks), function-backed actions, action log (P5).
- F6. Applying an action needs read access to the edited types and passing the
  submission criteria; criteria "can require a specific user ID or group ID" combined
  with parameter values; if criteria fail, side effects do not run. Row and column read
  controls "do not extend to the action's write" (P6).
- F7. A function-backed action has exactly one function rule (it cannot be combined with
  other rules), takes the function's inputs as parameters, and is **pinned to a
  function version** unless auto-upgrade is turned on; the docs warn auto-upgrade lets
  people without edit rights on the action change its behaviour, and that edits outside
  the pinned version's declared scope fail at run time (P7).
- F8. The action log turns each successful submission into an object recording
  submission id, action type and version, UTC timestamp, user id, primary keys of every
  edited object, optional parameters, side-effect provenance and later reversion. It
  does **not** record failed submissions, edits made outside actions, or anything before
  logging was enabled (P8).
- F9. Functions run server-side in isolation, in TypeScript or Python, read properties,
  traverse links, can make ontology edits, and can stream results for model-driven
  experiences (P9).
- F10. Writeback: action edits hit the object index immediately and are flushed
  periodically to a persistent edits store; a merged view combines source data with
  edits. Two conflict strategies: **user edits win** (default) or **most recent value
  wins** (needs a UTC timestamp property). Deletions are not edits; a deleted object
  stays hidden and a recreated one does not inherit old edits (P10).
- F11. Palantir frames the ontology as "decision-centric": humans and agents share it,
  "every agentic or human action depends on precise authorization grants", proposed
  changes can be packaged "into a sandboxed subset" (scenarios) before commit, and
  decisions are captured as data (P11).
- F12. External apps use an SDK whose token is scoped to chosen ontology entities and
  also applies the user's permissions; change subscriptions exist (TypeScript and
  WebSocket guides listed, not read) (P12).

### AIP

- A1. AIP "connects AI with your data and operations"; builder tools sit "on top of the
  Ontology"; it cites audit trails and evaluations (P13).
- A2. Logic functions take ontology objects or text, call models, and can return
  outputs or make ontology edits. When automated, edits are "applied automatically or
  staged for human review". The model reads only what the invoking user may read, and
  the docs again say read controls do not cover the model's output or the edits it
  composes (P14).
- A3. *(Corrected by the verifier, 2026-10-29, against P15.)* P15 lists **seven** agent
  tool types, though its text says six; one of them is legacy. They are **Action** ("execute an ontology
  edit"), **Object query** (the object types the model may access, with filtering,
  aggregation and link traversal), **Function** (call any function; the latest version
  by default, a published version can be pinned), **Update application variable**,
  **Command** (trigger operations in other applications), **Request clarification**
  (pause and ask the user) and the legacy **Ontology semantic search**. Action tools
  can be configured "to run automatically or to run after confirmation from the user"
  (P15). The earlier text named only three types. Action, Object query and Function
  are the three that the OneMind mapping in section 7 uses.
- A4. Agents are treated like a new team member that starts with limited access and
  earns more (P11).

### Apollo

- AP1. Apollo manages and deploys software across independent cloud, on-premises and
  disconnected (air-gapped) environments; developers merge once and Apollo delivers to
  each environment (P16).
- AP2. Developers declare product constraints (cross-service dependencies, supported
  database schema versions); environment owners declare environment constraints (for
  example change windows); both are enforced (P16). The orchestration engine "can only
  propose changes that do not violate any of the constraints" (P17).
- AP3. **Pull model with release channels**: environments subscribe to a channel (for
  example CANARY, STABLE); developers define promotion criteria and releases move when
  they are met (P16). A product catalog records each release's channel, recall status,
  dependencies and supported schemas (P17).
- AP4. **Hub and Spoke**: a Hub receives telemetry about Spokes and issues Plans; each
  Spoke runs a Spoke Control Plane whose agents report observed state, poll the Hub for
  Plans and execute them (P17).
- AP5. Change approval can use identity-provider groups and change requests are
  tracked for audit; bundled monitors integrate with Prometheus (P16).
- AP6. The S-1 says Apollo lets Palantir "deploy to air-gapped or on-premises networks
  at effectively the same rate as we can in the cloud", including "edge computing
  environments, on laptops" (P18).

### Gotham and Maven Smart System

- G-1. The S-1 says Gotham "facilitates the handoff between analysts and operational
  users, helping operators plan and execute real-world responses" (P18).
- MSS-1. DoD raised the MSS IDIQ ceiling by $795M to nearly $1.3B through 2029 (original
  May 2024 award: $480M); NGA reported 20,000+ active users across 35+ tools in three
  security domains (M1).
- MSS-2. A September 2025 Marine Corps message describes MSS as a user interface and
  data management platform that "simplifies data sharing, workflow integration and
  fusion for AI-enabled battle management" (verifier corrected the quote to match M2),
  presenting data geospatially through
  Foundry, Gaia, Target Workbench, Maverick and LogX, expected to "enhance intelligence,
  targeting and decision-making for joint fires integration" (M2).
- MSS-3. Its origin is Project Maven, AI to detect, tag and track objects in surveillance
  imagery (M1).

### OneMind today (what the equivalents must build on)

- O-a. The command envelope already carries an `approval` object, and lists protected
  command classes (`cmd.drone.launch`, `cmd.robot.move`, `cmd.intercept.*`, ...) that
  need operator approval by default; results are `evt.command.accepted|rejected|
  executed|failed` (O2).
- O-b. **No stream in Git captures `ent.*` or `evt.command.*`**: `grep` over
  `omos/platform/nats/streams/*.yaml` for `"ent.`, `"evt.>"`, `"evt.command` returns
  nothing (command output, exit 1). EVT captures only `evt.so|hp|le|ge|task|cloud|bio|
  robot|drone|sat.>` (O4).
- O-c. KV `om-schema` (history 5) exists; KV `world-state` has a 5-minute TTL (O4).
- O-d. OpenFGA is deployed but used only by Backstage Missions (O7).
- O-e. onemind-core retains audited requests, idempotency keys and results in
  PostgreSQL, and read credentials cannot mutate (O5).
- O-f. NATS JetStream domains isolate a leaf's JetStream from the hub's; mirrors are
  eventually consistent and catch up from upstream (N1, N2).

## How it applies to OneMind

The equivalents below stay inside the locked grammar (O1). Where a pattern has no exact
subject, the closest existing pattern is used and the gap is listed under Risks as an
owner proposal (decision 0004 #8).

### 1. Object types, properties and interfaces -> OneMind object types

| Palantir design | OneMind equivalent |
|---|---|
| Object type = schema (F2) | One JSON Schema per object type under `omos/services/schemas/` (pattern of O3), published by name and version into KV `om-schema` (O-c) so clients and Hermes discover types at run time |
| Object = one real thing (F2) | Fused state on `ent.{entity_type}.{entity_id}.state` and the latest value in an entity KV bucket (the entity lane's bucket; `world-state`'s 5m TTL is too short for durable objects, O-c) |
| Backing datasource (F2) | Producers: `tel.*`, `det.*`, `intel.*`, `tak.cot.>` (decision 0004 #7: the entity service derives per-UID state from the raw TAK wire) |
| Property | Typed field in the schema, each with source, observed time and confidence (detail belongs to the Lattice and fusion lanes) |
| Interface (F4) | OneMind **traits** declared in the schema: `locatable` (point, HAE, CE/LE: makes it projectable to CoT), `taskable` (accepts `cmd.*`), `observable` (has a sensor footprint), `trackable` (has kinematics). Client layers and the CoT projector select by trait, not by type, which gives the same polymorphism |

### 2. Link types -> links inside entity state plus link events

- A link is a named, bidirectional relation with a side name per type (F3). OneMind
  stores links on the entity record as `links: [{type, side, target: "<entity_type>.<id>",
  since, source}]` and the entity service maintains the inverse side, so every
  traversal is one KV read per hop.
- Link changes are facts: `evt.{domain}.{entity}.linked` / `.unlinked` (closest existing
  pattern, `evt.{domain}.{entity}.{event}`).
- Many-to-many relations with their own attributes (F3: link-backed datasource), such as
  `team assigned-to mission`, become their own object type (`assignment`) rather than a
  new subject family.

### 3. Action types -> OneMind actions on `cmd.*`

| Palantir design (F5-F8) | OneMind equivalent |
|---|---|
| Action type definition | An action spec in the schema registry: name, target object type, parameter JSON Schema, preconditions, edit rules, side effects, approval level, policy relation |
| Action invocation | A command envelope (O2) on `cmd.{domain}.{entity}.{action}`, for example `cmd.drone.sim-x1.goto` |
| Parameters | `data` validated against the parameter schema before anything runs |
| Submission criteria (F6) | Executor-side checks: OpenFGA relation (`can_command` on the target) plus parameter preconditions (geofence, battery). On failure: `evt.command.rejected`, no side effects (same rule as F6) |
| Rules (edits) | The executor emits the resulting state change through the entity service; it never writes KV directly from a client |
| Side effects | CoT to `tak.cot.inject`, notifications, Home Assistant calls, each recorded in the result event |
| Function-backed action pinned to a version (F7) | Each handler declares its action-spec version; the envelope's `v` must match a supported version; no silent auto-upgrade of handlers |
| Action log (F8) | CMD stream (720h, O4) holds every request; `evt.command.*` holds outcomes **including failures** (F8 omits failures; OneMind should not); onemind-core's Postgres (O-e) is the long-term record |
| Write not covered by read controls (F6, A2) | Authorization is enforced **at the executor that applies the change**, never only in the UI or the agent |

### 4. Functions -> `svc.{service}.{endpoint}` RPC

- Read and compute logic (F9) runs as NATS request/reply services:
  `svc.ontology.query`, `svc.ontology.traverse`, `svc.fusion.explain` (names are
  proposals within `svc.{service}.{endpoint}`).
- Functions that edit are not a separate path: they are action handlers (section 3),
  so every edit leaves a command, an outcome event and a policy decision.

### 5. Writeback -> source layer plus operator overrides, per-property strategy

- Keep source observations and human edits as separate layers and merge them in the
  entity service, the way F10 merges source data and edits.
- Per property, declare the strategy F10 names: `override_wins` (default for human
  fields: name, affiliation, notes) or `newest_wins` (kinematics, battery: a live
  telemetry update beats an old manual edit). Overrides come only from actions.
- Deletion is a tombstone with an actor and reason; a recreated UID starts clean (F10).
- Writeback to outside systems is a side effect of an action: TAK through
  `tak.cot.inject` (cot-projection contract), Home Assistant through its integration
  commands.

### 6. Gotham operational picture -> the OneMind 3D COP

- What is verified: Gotham links analysts and operators to plan and execute responses
  (G-1). The Gaia, Graph and Dossier descriptions are **unverified** (G1).
- OneMind equivalent: one shared picture built from the ontology, not from raw feeds:
  layers load a KV snapshot and then follow `ent.*` deltas (verified intel-dashboard
  pattern in `ULTRACODE-PROMPT.md`, "Facts already verified"), in the Unreal/Cesium
  client and the L0 CesiumJS client, with TAK clients receiving the same objects as CoT.
- A **link view** (graph of an object's links, section 2), a **mission notebook**
  (notes and attachments as `mission` objects with `evt.mission.*` history) and a
  **plan, execute, report** flow on the map. Map annotations already have a home:
  `evt.map.>` in MISSION (O4).

### 7. AIP -> Hermes acting through OneMind actions

- Tools for Hermes mirror three of the seven P15 tool types (A3): **query** (`svc.ontology.query`, restricted to allowed
  object types), **function** (`svc.*` read endpoints) and **action** (publish a command
  envelope). Hermes never gets a raw KV write or a `tak.cot.inject` publish permission.
- Identity: Hermes publishes with its own actor id (`source: agent.legacy`, O2, O6). An
  agent action is authorized for the **intersection** of the agent's grant and the
  requesting human's grant (A2: the model reads only what the user may read; OneMind
  applies the same rule to writes because F6 and A2 say read controls do not cover
  writes).
- Approval levels, as configured per action spec (A3: automatic or after confirmation):
  `auto` (read-only or reversible, low-risk), `confirm` (UI confirm by the requesting
  human), `approve` (named approver role). **Any command to an asset is at least
  `confirm`, even in simulation** (O9, Rule 6), and agent-originated asset commands are
  always `confirm` or above.
- Staging (A2, F11): an agent proposal is a command envelope with
  `approval.status: pending` (O2), shown in the UI and in ATAK as a pending item; only
  an approval by a human actor releases it to the executor. Multi-step plans become a
  `mission` with `task` objects that each carry their own pending commands.
- Trust grows with evidence (A4): new agents start read-only; promotion to `confirm`
  actions is an owner decision recorded in the agent registry (O6).
- Audit and evaluation (A1): every tool call is already on the AGENTS stream subjects
  for status and heartbeats (O4); outcomes are on `evt.command.*`.

### 8. Apollo -> OneMind hub and spokes on Argo CD and NATS leaves

| Apollo design | OneMind equivalent |
|---|---|
| Hub (AP4) | The cloud cluster: Git `main` plus Argo CD (O1) |
| Spoke + Spoke Control Plane (AP4) | Each site (home server, owner's Mac, field kit) runs a small site agent that reports observed versions and health on `tel.{node}.sys.{metric}` and pulls its desired state; the NATS leaf (per CLAUDE.md, forward only) carries the telemetry |
| Pull model and channels (AP3) | Channels as Git refs or OCI digests: `canary` (cloud and Mac), `stable` (home server, field kits). Promotion criteria: CI green plus N hours on canary without `evt.sys.*` alarms |
| Product and environment constraints (AP2) | Each release declares the schema and envelope versions it supports (O2 `v`); each site declares change windows ("not during an active mission"). A site agent refuses a plan that breaks either |
| Disconnected install (AP1, AP6) | A signed offline bundle (OCI images by digest, Git bundle, tiles) carried to the site; telemetry and logs queue in the site's own JetStream domain (N1) and sync back by mirror/source when the link returns (N2) |
| Change approval and audit (AP5) | omos PRs merged by the owner; site upgrades are `cmd.{domain}.{entity}.{action}` commands with `approve` level and normal `evt.command.*` outcomes |

### 9. Maven Smart System -> civilian Sense, Make Sense, Act workbench

- Verified design elements: one interface over a data platform that fuses many sources
  (M2), with computer vision that detects, tags and tracks objects in imagery (MSS-3).
- OneMind equivalent: `det.*` from camera models (MediaMTX streams) and `tel.*` from
  sensors feed the entity service; alerts become `alert` objects; an operator
  **response workbench** turns an alert into a mission with tasks and approved asset
  commands. Every step is an action with an audit record.
- **Where OneMind stops**: no weapons, no targeting, no "joint fires" workflow (M2), and
  no identification or tracking of people as targets. Person objects exist only for the
  team's own members and for safety (missing person, injured). This is a deliberate
  design boundary for the JADC2 lane to restate.

## Risks

1. **Grammar and stream gaps (owner proposals needed, decision 0004 #8).** No stream
   captures `ent.*` or `evt.command.*` (O-b), so action outcomes and entity history
   would be lost today. Link events, `alert` and generic object events also have no EVT
   domain. Needs an omos PR adding stream CRs; do not `nats stream add` (O1).
2. **Authorization only at read time.** Palantir's own docs warn twice that read
   controls do not cover writes (F6, A2). If OneMind enforces policy only in the UI or in
   Hermes' prompt, a compromised agent can publish commands. Enforce at the executor and
   with NATS subject permissions per actor.
3. **OpenFGA is not yet in the command path** (O-d). Until it is, actions need a simple
   static policy and every asset command stays at `confirm`.
4. **Handler drift**: unpinned handler versions change behaviour silently (F7).
5. **Writeback conflict surprises**: "edits always win" hides fresh telemetry (F10);
   needs the per-property strategy.
6. **Audit gaps**: Palantir's log omits failures and non-action edits (F8); OneMind must
   log both, and CMD's 720h retention is shorter than a useful audit horizon (O4).
7. **Trademark and licence**: do not use Palantir product names (Gaia, Foundry, Apollo,
   AIP, Gotham, Maven) as OneMind component names, UI labels or file names. Use generic
   names (object types, actions, hub, sites, workbench).
8. **Ethics boundary**: Maven's stated purpose includes targeting (M2). Copying its
   workflow names or UI would pull OneMind toward that use. Keep the stop line above.

## Recommendation

Adopt the **design** of the Palantir ontology as OneMind's core, on NATS and TAK:

1. In `docs/design/ONTOLOGY.md`, define every object type as a versioned JSON Schema with
   traits (interfaces), links (with inverse sides), per-property writeback strategy, and
   an action spec per mutating verb (parameters, preconditions, policy relation,
   approval level, side effects, handler version).
2. Make **actions the only write path**: command envelope on `cmd.*` -> executor checks
   schema, policy and approval -> entity service edit -> `evt.command.*` outcome -> CoT
   projection. Reads go through `svc.*` and KV.
3. Give Hermes the three tool kinds (query, function, action) and nothing else; agent
   actions use the intersection of agent and user grants, and every asset command needs
   a human confirm (Rule 6).
4. File one omos owner proposal (decision 0004 #8) adding streams for `ent.>` and
   `evt.command.>` and EVT domains for link and alert events.
5. Treat the cloud as the hub and each site as a spoke with a pull-based site agent,
   channels and constraints; carry offline bundles to disconnected sites and sync
   telemetry back through a per-site JetStream domain.
6. Write the civilian stop line (no targeting, no weapons, no tracking of people as
   targets) into the ontology's action list: those actions simply do not exist.

## Unverified items

- **Gotham Gaia, Graph and Dossier** descriptions (shared live map for plan, execute and
  report; whiteboard graph; collaborative document) come from search-result summaries
  of the S-1; the WebFetch reads of P18 (characters 0-200,000 of 472,026) did not reach
  that passage. The palantir.com Gotham page returned only a title.
- **Apollo cross-domain and air-gap transfer mechanics**: the Palantir blog post
  "Deploying Across Security Domains" returned HTTP 403. Remote hubs for isolated
  networks appear only in a 2022 deck cited by a search summary.
- **Who an AIP Logic function or agent runs as** (invoking user, builder or service
  identity): not stated in P14 or P15. The "intersection of grants" rule above is a
  OneMind design choice, not a Palantir fact.
- **Palantir scenarios and staged writes mechanics**: P11 mentions sandboxed proposals;
  the staged-writes page returned 404.
- **Ontology change subscriptions** (WebSocket and TypeScript guides in P12) were listed
  but not read.
- **MSS program-of-record transition to CDAO (March 2026 memo)** (M3) and the Marine
  Corps MARADMIN text (marines.mil returned HTTP 403) were seen only through
  secondary reporting.
- **NATS mirror behaviour across a broken leaf link** (catch-up after an outage): N2
  says mirrors are eventually consistent and catch up from upstream but does not
  describe link outages; the edge lane should test this on the local nats-server.
- **Whether `om-schema` KV is used by any live service today** was not checked against
  the cluster (this lane did not run kubectl).

**Independent verification (2026-10-29).** A verifier re-fetched P1, P3, P5, P6, P7,
P8, P10, P11, P14, P15, P16, P17, P18, M1, M2, N1 and N2, and re-ran the O2 and O4
checks. It checked F1, F3, F5, F6, F7, F8, F10, F11, A2, A3, A4, AP1-AP6, G-1, MSS-1,
MSS-2, MSS-3, O-a, O-b, O-c and O-f. Two claims were corrected in place: A3 (the
tool-type count) and MSS-2 (the quote). All other checked claims match their sources.
P3 also answers at `/docs/foundry/link-types/link-types-overview/` with a 404; the URL
listed above is correct. The verifier found no licence-rule violation: no worldmonitor
or worldwideview code, and no Palantir code, only short quoted phrases. It found no
cluster-safety violation either; no kubectl command was run.

No tool call in this lane was denied by the permission system. Blockers were HTTP 403 or
404 responses on four pages, listed above.
