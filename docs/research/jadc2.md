# Research lane: JADC2 and CJADC2 (Phase 1 / M0)

Date: 2026-10-29. Lane: jadc2. Scope: JADC2 and CJADC2 strategy documents, the
Sense / Make Sense / Act functions, the data fabric, mission partner environments,
zero trust and DDIL operations. The output maps each JADC2 capability to a civilian
OneMind capability (homestead, neighbourhood, response team) and says where OneMind
deliberately stops: no weapons and no targeting of people. This feeds
`docs/design/JADC2-MAP.md` and `docs/design/GAP-ANALYSIS.md`.

Repos read: omos at `62e3d22b`, onemind-xr at `0b3f20c`.

Method: `defense.gov`, `media.defense.gov`, `congress.gov` and `crsreports.congress.gov`
returned HTTP 403 to both WebFetch and curl from this machine. The primary PDFs were
fetched from Internet Archive `id_` snapshots (byte-exact copies of the original
files), converted with `pdftotext -layout` and read in full or by section. The
working copies are in the session scratchpad and are not committed. Where only a
search-engine summary was available, the claim is marked **unverified**.

## Sources

### Primary documents (read)

| Ref | Document | How obtained | SHA-256 of the PDF |
|---|---|---|---|
| S1 | *Summary of the Joint All-Domain Command & Control (JADC2) Strategy*, DoD, March 2022, 8 pp. Original URL: `https://media.defense.gov/2022/Mar/17/2002958406/-1/-1/1/SUMMARY-OF-THE-JOINT-ALL-DOMAIN-COMMAND-AND-CONTROL-STRATEGY.PDF` | `https://web.archive.org/web/2023id_/<original>` | `99c73732…0a55` |
| S2 | CRS In Focus IF11493, *Joint All-Domain Command and Control (JADC2)*, J. R. Hoehn, version 16, updated 21 January 2022. Original: `https://crsreports.congress.gov/product/pdf/IF/IF11493` | Wayback `2024id_` | `f5a5dbf8…dc6` |
| S3 | DoD News, "Hicks Announces Delivery of Initial CJADC2 Capability", J. Clark, 21 February 2024. Original: `https://www.defense.gov/News/News-Stories/Article/Article/3683482/hicks-announces-delivery-of-initial-cjadc2-capability/` | Wayback `2024id_` (HTML) | n/a |
| S4 | *DoD Zero Trust Strategy*, DoD CIO, version 1.0, 21 October 2022 (cleared 7 November 2022). Original: `https://dodcio.defense.gov/Portals/0/Documents/Library/DoD-ZTStrategy.pdf` | Wayback `2023id_` | `cba8e609…a635` |
| S5 | NIST SP 800-207, *Zero Trust Architecture*, August 2020. `https://nvlpubs.nist.gov/nistpubs/SpecialPublications/NIST.SP.800-207.pdf` (DOI 10.6028/NIST.SP.800-207) | direct | n/a |
| S6 | DoDI 8110.01, *Mission Partner Environment Information Sharing Capability Implementation for the DoD*, effective 30 June 2021. Original: `https://www.esd.whs.mil/Portals/54/Documents/DD/issuances/dodi/811001p.pdf` | Wayback `2023id_` | n/a |
| S7 | DoDD 3000.09, *Autonomy in Weapon Systems*, effective 25 January 2023. Original: `https://www.esd.whs.mil/portals/54/documents/dd/issuances/dodd/300009p.pdf` | Wayback `2024id_` | n/a |
| S8 | *DoD Data Strategy*, cleared 30 September 2020. Original: `https://media.defense.gov/2020/Oct/08/2002514180/-1/-1/0/DOD-DATA-STRATEGY.PDF` | Wayback `2023id_` | n/a |
| S9 | *DoD Cloud Strategy*, December 2018. Original: `https://media.defense.gov/2019/Feb/04/2002085866/-1/-1/1/DOD-CLOUD-STRATEGY.PDF` | Wayback `2023id_` | n/a |
| S10 | *DoD Data, Analytics, and AI Adoption Strategy*, cleared 27 June 2023. Original: `https://media.defense.gov/2023/Nov/02/2003333300/-1/-1/1/DOD_DATA_ANALYTICS_AI_ADOPTION_STRATEGY.PDF` | Wayback `2024id_` | n/a |
| S11 | DoD release, "DOD Adopts Ethical Principles for Artificial Intelligence", 24 February 2020. Original: `https://www.defense.gov/News/Releases/Release/Article/2091996/dod-adopts-ethical-principles-for-artificial-intelligence/` | Wayback `2023id_` (HTML) | n/a |
| S12 | FEMA, *National Incident Management System*, third edition, October 2017. Original: `https://www.fema.gov/sites/default/files/2020-07/fema_nims_doctrine-2017.pdf` | Wayback `2024id_` | n/a |

### Local files

| Ref | File |
|---|---|
| L1 | `onemind-xr/docs/VISION.md` lines 1-11 (Sense / Make Sense / Act table) |
| L2 | `onemind-xr/docs/FEATURES.md` rows A0-A17, B6, B20, C1-C8, D1-D12, F9, F10, H1-H3, I1-I5, J2, K1-K7 |
| L3 | `onemind-xr/docs/research/competitive-matrix.md` lines 433-441, "Deliberate non-goals" table (weapon inventory, active scanning of third-party hosts, crypto wallet tracing, ALPR camera layer). Verifier correction: earlier cited as lines 426-432, which hold the positioning paragraph. L3 does not mention face recognition or cross-camera re-identification |
| L4 | `onemind-xr/docs/decisions/0004-owner-defaults-for-the-run.md` rows 5, 12, 13 |
| L5 | `onemind-xr/ULTRACODE-PROMPT.md`, Mission, Rules 6 and 10, lane 14 |
| L6 | `omos/CLAUDE.md`, "NATS Subject Grammar (LOCKED)", "Bus Authority" |
| L7 | `omos/docs/NATS-DOCTRINE.md` lines 78-105 (leaf store-and-forward, accounts as security domains) |
| L8 | `omos/docs/MISSION.md` lines 1-60 (requirements 2, 6, 7, 9) |
| L9 | `omos/platform/security/` (`ecosystem-openfga.yaml`, `ecosystem-kyverno.yaml`, `ecosystem-falco.yaml`) |

## Verified facts

### 1. What JADC2 is (S1, S2)

- S1 (executive summary, p. 2): JADC2 "is intended to produce the warfighting
  capability to sense, make sense, and act at all levels and phases of war, across
  all domains, and with partners, to deliver information advantage at the speed of
  relevance. As an approach, JADC2 transcends any single capability, platform, or
  system."
- S1 sets out **three C2 functions**: Sense, Make Sense and Act. It also sets out
  **five lines of effort (LOEs)**: (1) the JADC2 Data Enterprise, (2) the Human
  Enterprise, (3) the Technical Enterprise, (4) integrating Nuclear C2 and
  Communications (NC2/NC3), and (5) modernizing Mission Partner Information Sharing.
- S1 lists **six guiding principles**: (1) information sharing designed and scaled
  at enterprise level; (2) layered security; (3) a data fabric of "efficient,
  evolvable, and broadly applicable common data standards and architectures";
  (4) resilience in degraded and contested electromagnetic environments; (5) unified
  development processes; (6) faster delivery.
- S1 says a JADC2 Strategy Implementation Plan exists, and the JADC2
  Cross-Functional Team oversees it. The plan itself is not public. Air & Space
  Forces Magazine titled its article "Pentagon Announces Classified JADC2
  Implementation Plan, Unclassified Strategy" (search result only; **unverified**
  beyond the headline).
- S2 describes JADC2 as DoD's concept "to connect sensors from all of the military
  services … into a single network", using a "cloud-like environment". It describes
  the use of AI "to identify targets, then recommending the optimal weapon … to
  engage the target". S2 also records analysts' questions about "how much a human will be
  needed" and "whether it is appropriate to reduce the amount of human involvement
  in military-related decisions". Service programs named in S2: Air Force ABMS,
  Army Project Convergence and Navy Project Overmatch.

### 2. Sense / Make Sense / Act (S1, pp. 4-5)

- **Sense** ("Integrate Information Across All Domains"): "the ability to discover,
  collect, correlate, aggregate, process, and exploit data from all domains and
  sources (friendly, adversary, and neutral)". It uses "remote sensors,
  intelligence assets, and open sources" through "federated data 'fabrics'".
- **Make Sense** ("Understand the Operational Environment"): "analyzing information
  to better understand and predict the operational environment … It is in
  sense-making that data transforms into information and information transforms
  into knowledge." It "requires the ability to fuse, analyze, and render validated
  data", and it "will leverage Artificial Intelligence and Machine Learning" with
  "automatic machine-to-machine transactions".
- **Act** ("Decide and Disseminate"): "to make and disseminate decisions … It
  combines the human elements of decision-making with the technical means". It
  relies on "resilient and redundant communication systems … and flexible data
  formats". Under **Mission Command**, subordinates "act with confidence and
  authority through understanding a senior commander's operational intent while
  retaining the ability to act when communications linkages are broken".

### 3. Data fabric (S1, S8)

- S1 footnote 1, working definition: "JADC2 data fabric … A DoD federated data
  environment for sharing information through interfaces and services to discover,
  understand and exchange data with partners across all domains, echelons and
  security levels."
- S1 LOE 1 objectives: "minimum metadata tagging criteria", "standardized data
  interfaces", "common data availability and access practices", "data security
  best practices", "JADC2 conformant IT standards", and the data goals "Visible,
  Accessible, Understandable, Linked, Trustworthy, Interoperable, Secure" (VAULTIS).
- S8 defines the same seven goals as "VAULTIS" (e.g. "Make Data Visible – Consumers
  can locate the needed data").
- S1 LOE 2 calls for "predetermined, pre-approved, event-driven, bundled
  authorities that enable rapid, relevant decision-making from the strategic level
  to the tactical edge".
- S1 LOE 3 includes "real-time synchronization and integration of kinetic and
  non-kinetic joint and long-range precision fires" next to shared situational
  awareness, collaboration, planning and logistics.

### 4. CJADC2 (S3)

- On 21 February 2024 Deputy Secretary of Defense Kathleen Hicks announced "its
  initial iteration of the Combined Joint All-Domain Command and Control, or
  CJADC2, capability" at the Advantage DoD 2024 symposium, which CDAO hosted.
- S3: "The initial version represents a minimum viable capability combining
  software applications, data integration and cross-domain operational concepts."
  "CJADC2 is the department's approach to developing both material and
  non-material solutions to deliver information and decision advantage to
  commanders." Hicks said that "due to security reasons she could not reveal where
  or what the initial capability announced today was specifically developed for"
  (S3, reported speech; corrected by the verifier, the earlier wording "where it was
  fielded" is not S3's). She called it "no easy task, especially in six months".
- **unverified**: The "C" (Combined) adds allies and partners to the joint
  (all-services) scope. S3 does not explain the "C": the article contains no
  mention of allies or coalition partners (verifier check of the S3 text,
  2026-10-29). This reading rests on the general doctrinal sense of "combined"
  (forces of two or more allies), which was not checked against the DoD Dictionary
  here.

### 5. Mission Partner Environment (S6)

- DoDI 8110.01 §1.2.a lists mission partners: other federal agencies; "State,
  local, and tribal governments"; "**Non-government organizations**"; "Private
  sector organizations"; allies and coalition members; multinational treaty bodies.
- §1.2.b: "MPE is the operating framework enabling command and control (C2) and
  information sharing … An MPE capability provides the ability for DoD and MPs to
  exchange information with all participants within a specific partnership or
  coalition."
- Glossary: **Federation**, "Association of entities with common goals and
  objectives, each retaining full control of their own capabilities and affairs."
  **FMN** (Federated Mission Networking) is a governed framework "to plan, prepare,
  establish, use, and terminate mission networks". **MPE expeditionary**:
  "mission commander governed … including unanticipated MPs; mostly perishable
  data; risk tolerant". **MPE enterprise**: "day-to-day operations with
  most-trusted partners … mostly non-perishable data; risk averse".
- The human-to-human services the MPE requires: "chat, secure voice, video
  teleconferencing, email … web browsing" (§1.2.c).
- S1 LOE 5: "Ideal mission partner system integration is realized when data from
  each partner's C2 systems can be accessed, viewed, and acted upon by every other
  approved partner."

### 6. Zero trust (S4, S5)

- S4 is the DoD Zero Trust Strategy v1.0 (21 October 2022). It describes a "never
  trust, always verify" mindset and uses **seven pillars**: User; Device;
  Applications & Workloads; Data; Network & Environment; Automation & Orchestration;
  Visibility & Analytics (S4's goals table lists 2.1 User, 2.2 Device,
  2.3 Applications & Workloads, 2.4 Data, 2.5 Network & Environment,
  2.6 Automation & Orchestration and 2.7 Visibility & Analytics). The **Data** pillar is "central" (S4, text after Figure 3).
  Components must reach "Target Level ZT" by the end of FY2027.
- S4 footnote 32 lists the ZT Reference Architecture's five tenets: "Assume a
  Hostile Environment; Presume Breach; Never Trust, Always Verify; Scrutinize
  Explicitly; Apply Unified Analytics". Least privilege shifts trust to
  "multi-attribute-based confidence levels" (footnote 31).
- S5 §2.1 (quoted in S4 footnote 33) gives NIST's seven tenets, including:
  "All communication is secured regardless of network location", "Access … is
  granted on a per-session basis" and "determined by dynamic policy".
- S4 footnote 7 quotes the Digital Modernization Strategy. Tactical components
  must "operate autonomously, should connectivity to the core JIE be lost or
  denied."

### 7. DDIL and degraded operations (S1, S9, S10)

- No official DoD definition of the acronym "DDIL" was found in the documents read.
  Vendor and press sources expand it differently: "denied, disrupted, intermittent,
  and limited", "denied, disconnected, intermittent and limited bandwidth" and
  others (WebSearch, 2026-10-29). The exact expansion is **unverified**. OneMind
  should use the run prompt's wording (L5).
- S1's executive summary (p. 2) refers to "degraded and contested electromagnetic
  spectrum environments" and, in the principles list, "degraded and contested
  electromagnetic environments"; p. 5 mentions a "C2 degraded environment"
  (verifier correction: the phrase is not used "throughout"). Guiding principle 4: "operate with minimum guidance within a degraded or contested
  C2 environment … where sensing and communications are severely impacted or
  completely disabled".
- S9 (section on the tactical edge): "Cloud devices employed by warfighters at the tactical edge will be
  ruggedized and adaptable, providing for automatic synchronization to the greater
  cloud once communication is sufficient or reestablished."
- S10: "Some users may have access to abundant data and high-speed
  processing while other users face limited bandwidth at the tactical edge.
  Designers will identify these constraints early in the development process."

### 8. Human judgment and AI (S7, S11)

- S7 §1.2.a: "Autonomous and semi-autonomous weapon systems will be designed to
  allow commanders and operators to exercise appropriate levels of human judgment
  over the use of force."
- S11, the five DoD AI ethical principles: **Responsible**, **Equitable**, **Traceable**
  ("transparent and auditable methodologies, data sources"), **Reliable** ("explicit,
  well-defined uses … testing and assurance") and **Governable** ("ability to detect
  and avoid unintended consequences, and the ability to disengage or deactivate
  deployed systems that demonstrate unintended behavior").

### 9. The civilian counterpart: NIMS (S12)

- NIMS "guides all levels of government, nongovernmental organizations (NGO), and
  the private sector to work together to prevent, protect against, mitigate,
  respond to, and recover from incidents". It provides "common terminology" and
  standard structures (Incident Command System), and it names "location-enabled
  situational awareness and decision support" for resource management (S12,
  Introduction and resource management sections).
- NIMS is the civilian doctrine that covers OneMind's neighbourhood and response
  team tiers. JADC2 supplies the technical patterns (fabric, zero trust, DDIL), and
  NIMS/ICS supplies the civilian organizational vocabulary.

## How it applies to OneMind

### The capability map

Columns: the JADC2 capability (with its source), OneMind's civilian equivalent at
each tier, how it is built (feature IDs from L2, subjects from L6), and where OneMind
stops.

| # | JADC2 capability | Homestead | Neighbourhood | Response team | OneMind mechanism | Stop line |
|---|---|---|---|---|---|---|
| 1 | **Sense**: collect from all domains and sources, including open sources (S1 p. 4) | Home sensors, owned cameras, weather station, own drones and robots | Shared public feeds: weather, fire, seismic, air and sea traffic, CAP/NWS alerts | TAK members' positions, team drones, incident reports | `tel.*`, `det.*`, `intel.*` on the authority bus; A0-A3, A8, A13, A17, H1-H3, K1-K2 | No sensing aimed at identifying uninvolved people: no ALPR layer (L3) and no face recognition (proposed by this lane; not in L3) |
| 2 | **Sense: friendly, adversary, neutral** (S1) | Family, guests, assets | Consenting members, public infrastructure | Team members, other agencies (via TAK) | CoT affiliation on entities (B7) | No "adversary" class for people. Affiliation is for assets and TAK contacts, and a person's position appears only by their own opt-in (TAK SA) |
| 3 | **Make Sense**: fuse and render validated data; data → information → knowledge (S1 p. 4) | Live 3D twin of the property; entity per asset | Neighbourhood layer of hazards and infrastructure | Common operating picture of the incident | Entity service `ent.{type}.{id}.state` with sources and confidence (A4, A5, B6, E1) | Fusion of people is limited to consenting team members. No cross-feed identity resolution of members of the public |
| 4 | **Make Sense: AI/ML, machine-to-machine** (S1 p. 5; S11) | Hermes briefs, anomaly alerts | Convergence detection across feeds (D9) | AI brief of the incident area (D10) | Hermes over entities via `svc.*`/`agents.*` (I2, I3); audit (I4) | AI never recommends force or a target. S11 Traceable/Governable map to I4 audit and a disengage switch |
| 5 | **Act: decide and disseminate** (S1 p. 5) | Tasks, geofences, drone survey flights | Shared alerts, mission packages | Missions, assignments, incident board | `cmd.*`, `evt.mission.*`, `tak.cot.inject`, mesh tx (C2, D1-D3, D8) | Act covers information, logistics and owned simulated assets only (`sim-*`, L4 #13). No effect on any person |
| 6 | **Mission Command**: act on intent when links break (S1 p. 5) | Local automations keep running offline | Each household keeps local authority | Team lead's standing orders apply in the field | Pre-approved, event-driven rules (S1 LOE 2 "bundled authorities") as watch jobs (I3) with a written envelope | A bundled authority may only alert, inform, or move an owned asset inside a geofence. It may never escalate to force |
| 7 | **Data fabric** (S1 fn. 1; LOE 1; VAULTIS, S8) | One bus, one catalog | Federated leafs, each owner keeps control | Shared mission data | NATS JetStream, KV, leaf nodes, subject grammar (L6, L7); feed registry A2 = "Visible"; contracts in `docs/contracts/` = "Understandable/Interoperable" | Grammar is locked. No new subject patterns |
| 8 | **Minimum metadata tagging** (S1 LOE 1) | Source, time, licence per record | Same | Plus releasability | Provenance fields on entities (A4), feed records (A2), CoT `access`/`releasableTo` (security lane) | Tags are required. Data without a source is not shown as fact (B20 intelligence gaps) |
| 9 | **Mission partner environment / federation** (S6) | Owner's own account | Each household is a federated member that "retains full control" (S6 Federation) | Partners such as fire, EMS and other NGOs join for the incident; "unanticipated MPs", "perishable data" (S6 expeditionary) | NATS accounts as security domains with explicit stream/service exports (L7 §5); TAK Server groups; Data Sync missions (C7) | Sharing is opt-in per export. No partner gets raw camera or home-sensor data by default |
| 10 | **Human-to-human collaboration** (S6 §1.2.c) | Family chat | Neighbourhood channel | Team chat, voice, video | TAK chat (C8), Mumble (C6), video (K1-K4) | None beyond normal privacy |
| 11 | **Zero trust** (S4, S5) | Every device and agent authenticated, including inside the house | Per-household accounts | Per-incident access, revoked after | Tailscale, NATS per-user credentials, OpenFGA policy (L9, I4), Authentik roles (J2), signed plugins (K7) | Login is not authorization to operate equipment (L8 req. 7) |
| 12 | **DDIL / degraded operations** (S1 principle 4; S9; S10) | Whole stack on the home server or Mac with no internet (F9) | Neighbour leafs reach each other over the LAN or radio | Field kit plus ATAK mesh with no cloud | Leaf store-and-forward and JetStream catch-up (L7); mesh gateway (A7); offline tiles (B2); local Hermes model (F9) | Offline mode keeps the same policy and human-confirm rules |
| 13 | **Enterprise-scale sharing** (S1 principle 1) | n/a | Many households on one hub | Multi-team incidents | Cloud hub plus leafs (L6 Bus Authority) | The cloud is never the only authority (L8 req. 2) |
| 14 | **Speed of delivery** (S1 principles 5-6; S3 "six months", MVC) | Continuous deploy to the home server | Same images for all members | Field kits updated before deployment | GitOps (omos Argo), Apollo-style lane (edge lane) | No unreviewed deploy reaches a node that commands assets |
| 15 | **Human Enterprise / training** (S1 LOE 2) | Drills with simulated assets | Exercises | Rehearsal in the 3D twin and headset (L3 tier) | Session playback, synchronized briefing (F8), what-if labelled simulation (D12) | Simulation never drives commands (D12) |
| 16 | **Fires: kinetic and non-kinetic** (S1 LOE 3) | **Not built** | **Not built** | **Not built** | — | Deliberate stop: no weapons, no weapon data (L3 "Deliberate non-goals": "Weapon inventory with range/engagement data"), no electronic or cyber attack, no "non-kinetic" effects against anyone |
| 17 | **Targeting: AI identifies targets and recommends a weapon** (S2) | **Not built** | **Not built** | **Not built** | — | Deliberate stop: no target nomination, no person tracking, no task to follow or intercept a person |
| 18 | **NC2/NC3** (S1 LOE 4) | n/a | n/a | n/a | — | Out of scope |
| 19 | **Autonomy with human judgment** (S7) | Robots and drones run local autonomy (RTL, geofence) | — | Team assets | D3 confirm plus audit; D5 human confirm; Rule 6 (L5) | OneMind applies S7's principle with a stricter bar: every asset command needs a human confirm in the UI, even in simulation (L5 Rule 6) |

### What OneMind takes from JADC2

1. **The loop and the fabric, not the kill chain.** OneMind keeps Sense, Make Sense
   and Act (L1) and the federated data fabric. It removes LOE 3 "fires", LOE 4 NC3
   and the S2 sensor-to-shooter use.
2. **Federation as the MPE model.** S6's "each retaining full control" matches the
   omos rule that the leaf is a forwarding pipe and every household keeps its own
   authority (L6, L7). It also matches the civilian need: an NGO is already a named
   mission partner type (S6 §1.2.a(3)), and NIMS names NGOs as participants (S12).
3. **Enterprise and expeditionary sharing tiers.** OneMind can copy S6's split. A
   standing *neighbourhood* federation is enterprise-like: trusted members, durable
   data, tight security. An *incident* federation is expeditionary: unanticipated
   partners, perishable data, created and torn down per incident. Both become NATS
   account exports plus TAK Server groups or Data Sync missions.
4. **Pre-approved bundled authorities = watch jobs with envelopes.** S1 LOE 2 maps
   directly to I3 watch jobs. Each one gets a written envelope: the trigger, the
   allowed actions, the assets and the area. Anything outside the envelope goes back
   to a human.
5. **Zero trust pillars as a checklist.** The seven S4 pillars give the
   security-and-trust lane a ready structure: User (Authentik), Device (Tailscale
   node identity), Application (signed plugins, Kyverno), Data (classification,
   releasability), Network (tailnet-only admin, leaf auth), Automation (OpenFGA in
   the command path), Visibility (Falco, audit stream).
6. **DDIL as a design input, not a mode.** S10 asks designers to "identify these
   constraints early". For OneMind this means every feature in L2 states its offline
   behaviour, and the two-tier map (L1 principle 3) keeps phones on the mission tier.

### Where OneMind deliberately stops

These are product rules. They should be written into `docs/design/JADC2-MAP.md` and
enforced in code and policy, not only in documentation.

1. **No weapons.** OneMind does not model, carry, control or integrate any weapon,
   weapon inventory, munition or effector. This follows the refusal in L3 and the
   mission in L5. Robots and drones carry sensors and payloads for survey, delivery
   and inspection only.
2. **No targeting of people.** OneMind never nominates, ranks, tracks or follows a
   person as a target, and never recommends an action against a person. This is
   the exact function S2 describes for JADC2 ("identify targets, then recommending
   the optimal weapon"), and OneMind excludes it.
3. **No identity surveillance.** No face recognition, no ALPR layer and no
   cross-camera re-identification of members of the public. (L3 records only the
   ALPR refusal; face recognition and re-identification are this lane's proposal.) On the owner's own
   cameras, a presence alert ("person detected in zone X") is the limit. It carries
   no identity and is not tracked across cameras. (The exact rule for own-property
   cameras is an owner decision; see Unverified items.)
4. **No non-kinetic effects.** No jamming, no cyber operations, no active scanning
   of third-party hosts (L3), and no influence operations.
5. **Command verb allowlist.** `cmd.*` verbs for assets are a closed list of safe
   verbs (for example `goto`, `rtl`, `land`, `loiter`, `survey`, `estop`, as in D3).
   Verbs such as `intercept`, `engage`, `strike`, `track-person` or `follow-person`
   are rejected by policy (I4), whether a person or Hermes issues them.
6. **Human confirm on every physical action.** OneMind sets a stricter bar than S7.
   Every asset command goes through policy, audit and a human confirm in the UI,
   including in simulation (L5 Rule 6). Hermes proposes and a human approves (D5).
7. **Simulation is labelled and inert.** What-if runs never drive commands (D12).

## Risks

| Risk | Evidence | Mitigation |
|---|---|---|
| The omos grammar example `cmd.drone.x47.intercept` shows a military engagement verb as the canonical command | L6 (`omos/CLAUDE.md` line 88) | Owner proposal in omos `docs/` (decision 0004 #8 route) to change the example to `cmd.drone.x47.goto` and to add the verb allowlist. Do not edit omos `CLAUDE.md` from this lane |
| FEATURES D5 says "destructive or kinetic commands need a human confirm". "Kinetic" implies weapons exist, and the confirm rule is weaker than Rule 6 | L2 row D5; L5 Rule 6 | Reword D5 to "every asset command needs a human confirm" and remove "kinetic" |
| Dual use: entity fusion, tracking and drone tasking are the same code a targeting system would use | S2's description of JADC2 AI; L2 A4, B13, D3 | Enforce the stops in policy (verb allowlist, OpenFGA), in the ontology (no target type, no adversary affiliation for people), and in tests (a red-team test where Hermes is asked to follow a person and must refuse) |
| Using JADC2 language in public material may read as a military product, especially for an NGO | S1/S2 framing is warfighting | In public docs, describe the loop as "Sense, Make Sense, Act (after the JADC2 pattern)" and cite NIMS (S12) as the civilian doctrine. Use no DoD logos or branding (L5 Reference systems rule) |
| Federation leaks: partners in an incident federation keep perishable data after the incident | S6 "terminate mission networks" | Incident exports get an expiry and are revoked at close. Audit who received what |
| The DDIL acronym has several expansions, so docs may disagree | §7 above | Use the run prompt's wording consistently |
| Primary DoD URLs block automated fetches. Future verifiers may not be able to re-check | 403 on defense.gov, congress.gov | Cite Wayback `id_` URLs and SHA-256 hashes (Sources table) |
| S1 and S2 are dated 2022. CJADC2 implementation detail is classified or unpublished | S1, S3 | Treat JADC2 as a pattern source only. Do not claim parity with fielded DoD systems |

## Recommendation

1. Adopt JADC2 as the **pattern** for OneMind's loop, fabric, federation, zero trust
   and DDIL design. Adopt NIMS/ICS (S12) as the **civilian doctrine** for the
   neighbourhood and response team tiers. Write `docs/design/JADC2-MAP.md` from the
   19-row table above.
2. Make the stop lines enforceable before any asset command ships (D3, D5, I4):
   a closed `cmd.*` verb allowlist in OpenFGA or the command service; no `target`
   object type and no adversary affiliation for people in `docs/design/ONTOLOGY.md`;
   a human confirm on every asset command (Rule 6); and a red-team acceptance test
   in which Hermes is asked to track or follow a person and refuses, with the refusal
   audited.
3. Model sharing on S6's two tiers: a standing *neighbourhood federation*
   (enterprise-like) and a per-incident *response federation* (expeditionary) that
   expires. Build both from NATS account exports plus TAK groups, so each household
   "retains full control" of its own data.
4. Turn S1 LOE 2 "pre-approved, event-driven, bundled authorities" into I3 watch
   jobs with explicit envelopes. They may alert and move owned assets inside a
   geofence, and they must keep working in DDIL (F9).
5. Use the seven S4 pillars as the section structure of the security-and-trust lane
   and of `REFERENCE-ARCHITECTURE.md` security zones.
6. File two small follow-ups for the owner, without editing them in this lane:
   (a) an omos proposal to replace the `intercept` grammar example and record the
   verb allowlist; (b) a FEATURES D5 rewording that removes "kinetic".

## Unverified items

- **unverified**: What the "C" in CJADC2 adds (allies and partners). S3 does not
  say; see §4.
- **unverified**: The quote "CJADC2 isn't a platform, or a single system" attributed
  to Hicks appeared in a search summary. It is not in the DoD News article read (S3).
- **unverified**: The date the Secretary of Defense signed the JADC2 strategy
  (press reports say 2021) and the classification of the Implementation Plan.
  Only press headlines were seen, and S1 gives only "March 2022" for the summary.
- **unverified**: An official DoD expansion and definition of "DDIL". None was
  found in S1, S4, S9 or S10.
- **unverified**: The FY2024 CJADC2 budget figure ($1.4 billion) from secondary press.
  It was not checked against budget documents and is not used above.
- Note: S4 Figure 3 (the pillar graphic) is an image and was not read. The seven
  pillar names come from S4's goals table text (2.1-2.7).
- **unverified / owner decision**: Whether person *presence* detection on the
  owner's own property cameras is allowed. The recommendation above allows a
  presence alert with no identity and no cross-camera tracking, but that is a
  proposed default, not a recorded owner decision.
- Not researched in this lane (other lanes own them): CoT `access`/`releasableTo`
  semantics (security lane), the Palantir and Anduril mappings, and the Apollo-style
  edge deployment.
- Blockers: direct fetches of defense.gov, media.defense.gov, congress.gov,
  crsreports.congress.gov and fema.gov returned HTTP 403. They were worked around
  with public Internet Archive snapshots of the same files, not with any tool or
  permission override. No tool call was denied by the permission system.
