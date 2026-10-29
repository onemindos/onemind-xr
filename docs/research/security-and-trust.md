# Security and trust — research lane (M0)

Date: 2026-10-29. Lane: security. Scope: zero trust across sites, mTLS and certificates,
NATS accounts, ABAC, the CoT `access`/`caveat`/`releasableTo` fields, data classification
and sharing between groups, supply-chain security, and a threat model for a C2 platform,
including a compromised agent or node.

How this was done: I read the omos repo at `62e3d22b` (2026-10-28), ran read-only
`kubectl get` against `do-nyc3-omos-cloud` (no Secret values read: the NATS config was
read from its ConfigMap with credential values masked), read TAK Server and ATAK-CIV
source through `gh api`, read vendor and standards docs, and ran one local NATS leaf
permission test on throwaway ports. This lane builds on the account layout in
`nats-platform.md` and the TAK API map in `tak-protocols.md` and does not repeat them.

---

## Sources

### omos (repo and live cluster, read-only)
- O1 `omos/platform/security/{README.md, kyverno-policies-audit.yaml, ecosystem-*.yaml, namespaces.yaml}`
- O2 `omos/docs/operations/cluster-review-2026-10-09.md` (C1, H3, M4, A8, A12, A17, A18)
- O3 `omos/docs/NATS-DOCTRINE.md` §5 "Security Model"
- O4 `omos/platform/hermes/agent/deployment.yaml` (init container lines 100-122, leaf sidecar lines 228-245)
- O5 `omos/platform/core/policy.hujson` (Tailscale grants) and `.github/workflows/tailscale.yml`
- O6 `omos/platform/takserver/deploy/CoreConfig.xml` (Git copy; lines 4-11, 102)
- O7 `omos/platform/ecosystem/resources/authorization-context.yaml` (OpenFGA bootstrap)
- O8 `omos/.github/workflows/*` (grep for `uses:`, `sbom`, `provenance`, `cosign`)
- L1 `kubectl get clusterpolicy` → 4 policies, all Ready (warning: `kyverno.io/v1 ClusterPolicy is deprecated`)
- L2 `kubectl get cm -n nats nats-config` (values masked): accounts `ONEMIND` (one user `onemind`, no `permissions` block) and `SYS` (`sys`, `monitor`); client `tls{}` with no `verify`; `cluster.tls` and `leafnodes.tls` with the private CA; leaf user `leaf_tak` bound to `ONEMIND`; `websocket { no_tls: true, port: 8080 }`
- L3 `kubectl get certificate -n nats`: `nats-ca` (self-signed, 87600h = 10 y), `nats-server-tls` (issuer `nats-ca-issuer`, 8760h)
- L4 `kubectl get networkpolicy -A`: 100 NetworkPolicies in 27 namespaces; `nats/nats-clients` admits 4222 from the `nats` namespace plus `tailscale`, `omos`, `onemind-core`, `takml`, `takserver`, `agent-fabric`. CiliumClusterwideNetworkPolicy `deny-imds-egress`.
- L5 `kubectl get ingress -A` (class `nginx`): `matrix.onemindos.dev` and `onemindos.dev` (matrix-wellknown) have no `whitelist-source-range`; `ws.onemindos.dev` (NATS WebSocket), `auth.`, `onemindos.dev`, `tileserver.` have the Cloudflare allowlist.
- L6 `kubectl get pods -n spire-system` → none. `kubectl get deploy -A`: `openfga` (onemind-core, 1 replica), Authentik server ×2, Falco DaemonSet ×4 + falcosidekick.
- L7 `kubectl get pods -n hermes` images: `hermes-legacy` runs `nats:2.10-alpine` and `nousresearch/hermes-agent:v2026.9.24` **without a digest**; other containers are digest-pinned.
- L8 `gh repo view onemindos/omos` → `PUBLIC`; `gh api orgs/onemindos` → plan `free`.

### TAK and CoT (primary source code)
- T1 https://github.com/TAK-Product-Center/atak-civ/blob/main/takproto/cotevent.proto (lines 22-38)
- T2 https://github.com/deptofdefense/AndroidTacticalAssaultKit-CIV/blob/master/takproto/cotevent.proto (older copy: no `caveat`/`releasableTo`)
- T3 TAK Server 5.7 source, `TAK-Product-Center/Server`: `takserver-core/takserver-war/.../classification/service/ClassificationServiceImpl.java`
- T4 same repo, `takserver-core/src/main/java/com/bbn/marti/service/DistributedSubscriptionManager.java` (lines 140, 246-247, 454-458, 472, 488)
- T5 same repo, `takserver-common/src/main/xsd/CoreConfig.xsd` (lines 291-348, 672-692, 1564-1576)
- T6 same repo, `takserver-plugins/.../cot/CotEventContainer.java` (lines 173, 263-267); `takserver-schemamanager/.../V87__add_caveat_releaseableTo_columns_in_table_cot_router.sql`

### NATS
- N1 https://docs.nats.io/running-a-nats-service/configuration/securing_nats/tls (`verify`, `verify_and_map`, `handshake_first`)
- N2 https://docs.nats.io/running-a-nats-service/configuration/securing_nats/authorization (allow/deny, `allow_responses`, "a user with no `permissions` block can do anything on the server"; `default_permissions` applies to users without their own block) [verifier: quote corrected 2026-10-29, page fetched]
- N3 https://docs.nats.io/learn/security/operator-mode (full resolver; "revoke the user and re-push the account")
- N4 https://docs.nats.io/reference/config/leafnodes/authorization
- N5 https://github.com/nats-io/nkeys README ("A public-key signature system based on Ed25519")
- N6 https://github.com/nats-io/nats-server `server/opts.go` (`TLSVerify`, `OCSPConfig`)
- P1 Local test, this lane: `work/.run/security-lane/{hub.conf, leaf.conf, hubsub.txt, leafsub.txt}`, nats-server v2.15.2

### Standards, policy and supply chain
- S1 NIST SP 800-207 Zero Trust Architecture, https://csrc.nist.gov/pubs/sp/800/207/final
- S2 OWASP GenAI Top 10 2025, LLM06 Excessive Agency, https://genai.owasp.org/llmrisk/llm062025-excessive-agency/
- S3 OpenFGA conditions, https://openfga.dev/docs/modeling/conditions
- S4 Kyverno ImageValidatingPolicy, https://kyverno.io/docs/policy-types/image-validating-policy/
- S5 SLSA v1.0 levels, https://slsa.dev/spec/v1.0/levels
- S6 GitHub artifact attestations, https://docs.github.com/en/actions/security-for-github-actions/using-artifact-attestations/using-artifact-attestations-to-establish-provenance-for-builds
- S7 FIRST Traffic Light Protocol 2.0, https://www.first.org/tlp/

---

## Verified facts

### A. omos today: what the security layer actually does
1. `platform-security` deploys Kyverno, Falco and OpenFGA (O1). Kyverno **enforces** only two
   rules: no untagged or `:latest` images, and a layer label on namespaces. Requests/limits
   and privileged/hostPath are **audit only**. All policies use `failurePolicy: Ignore`, so
   "Enforce fails open" when the single Kyverno replica is down (O1 comment block). All 4
   are Ready live (L1).
2. The Kyverno image rule rejects `:latest` only. A mutable tag such as `nats:2.10-alpine`
   passes, and it runs live in the Hermes pod without a digest (L7).
3. There is **no image signature or provenance check** at admission: no
   `verifyImages`/ImageValidatingPolicy in `platform/security` (O1). In CI, only
   `build-nodered-tak.yml` emits `provenance: mode=max` and `sbom: true`; no workflow signs
   images with cosign or produces attestations (O8 grep).
4. 80 of 83 GitHub Action `uses:` lines are SHA-pinned. The three that are not are all in
   `tailscale.yml` (`actions/checkout@v4`, `tailscale/gitops-acl-action@v1` twice), which is
   the workflow that holds the tailnet API credential and pushes the tailnet policy (O5, O8).
5. The Hermes init container runs `uv pip install nats-py nkeys synadia-ai-agents
   synadia-ai-agent-service` **at pod start, unpinned**, then patches a plugin file
   (O4 lines 110-122). Each Hermes restart can pull new code from PyPI.
6. Falco runs on all 4 nodes (L6). Its alerts go to Alertmanager (O1), and on 2026-10-10
   Alertmanager's only receiver was `null` (O2 A17). Whether a receiver exists now is
   **unverified** (the config is in a Secret, which this lane does not read).
7. SPIRE is not running: `spire-system` has no pods (L6), although the cluster review found
   3 SPIRE containers on 2026-10-10 (O2 §1.4). There is no workload identity (SPIFFE)
   in use today.
8. OpenFGA runs (1 replica) and is used by Backstage Missions only (O1, L6). Its store and
   model ids are filled at sync time by a bootstrap Job (O7).
9. NetworkPolicies exist in 27 namespaces (100 objects), and pod egress to the cloud
   metadata service is denied cluster-wide (L4). NATS client port 4222 is open to 6 named
   namespaces plus the `nats` namespace itself (L4).
10. The Kubernetes API is public with no control-plane firewall (O2 H3; repeated in omos
    `CLAUDE.md`). TAK enrollment 8446 had `clientAuth="false"` (O6 line 7), which is how
    enrollment works by design; TAK is now tailnet-only (omos `CLAUDE.md`).
11. The omos repo is **public** (L8). Everything in it, including the Tailscale host IPs in
    `policy.hujson` (O5) and the TAK `CoreConfig.xml` (O6), is world-readable. SealedSecrets
    are encrypted, so this is acceptable only as long as no plaintext credential is committed
    (omos gitleaks baseline lists 9 pre-existing hits, omos `CLAUDE.md`).

### B. NATS: identity, TLS and permissions (live and tested)
12. Live NATS has **one application user** `onemind` in account `ONEMIND` with no
    `permissions` block (L2). Per N2 "a user with no `permissions` block can do anything on the server" (unless an `authorization.default_permissions` block exists, N2; the live config was not re-read by the verifier): any holder can publish
    `cmd.>`, `tak.cot.inject` and read everything. This is the same finding as
    `nats-platform.md` R2; confirmed independently here.
13. Client TLS on 4222 has no `verify` (L2). Per N1, TLS "encrypts the link and proves the server to the client", and "TLS on without
    `verify` is encryption, not authentication" [verifier: quote corrected, N1 fetched 2026-10-29];
    clients are not authenticated by certificate.
    Routes and leaf connections use TLS with the private CA (L2). The CA is self-signed for
    10 years and server certificates last 1 year, issued by cert-manager (L3).
14. The WebSocket listener has `no_tls: true` (L2): TLS ends at ingress-nginx for
    `ws.onemindos.dev`, which is allowlisted to Cloudflare ranges only (L5). A Cloudflare
    allowlist restricts the network path, not the user: anyone on the internet who reaches
    the hostname through Cloudflare reaches the NATS WebSocket and then needs only the
    shared password.
15. The only leaf credential on the hub is `leaf_tak` (L2), reused by the TAK leaf and Hermes'
    sidecar (`nats-platform.md` R3). Hermes' agent container speaks plaintext to its in-pod
    leaf on `localhost:4222` (O4 lines 160-161).
16. **Proved locally (P1):** a leaf that authenticates as a hub user carrying
    `permissions` is filtered in **both directions** by that user's permissions, seen from
    the hub. With `publish.allow=["tel.site1.>"]` and `subscribe.allow=["evt.>"]`:
    leaf→hub, `tel.site1.x.pos` arrived but `tel.site2.x.pos` and `cmd.drone.sim-x1.land`
    did not; hub→leaf, `evt.mission.p1.done` arrived but `cmd.drone.sim-x1.land` and
    `intel.a.b.c` did not (`hubsub.txt`, `leafsub.txt`). So the hub can confine a
    compromised site to its own subjects without trusting the site's config.
17. Also found in P1: in nats-server 2.15.2, a `permissions` block inside
    `leafnodes.authorization.users` is rejected (`unknown field "permissions"`). Per-leaf
    permissions therefore come from an account user (accounts block) or from JWT user
    claims, not from the leafnode users list.
18. NATS core has no publisher identity; the doctrine says to embed `source_id` in payloads
    where audit matters (O3 §5). nkeys are Ed25519 (N5), so a service can sign payloads with
    the same key type the server uses for identity.
19. In operator/JWT mode, a leaked user credential is cut off by revoking the user in the
    account JWT and pushing the account to the full resolver (N3). nats-server has OCSP
    stapling support (N6).

### C. TAK: CoT security fields and how TAK Server uses them
20. CoT event fields `access` (2), `caveat` (16) and `releasableTo` (17) exist in the
    current TAK Protocol v1 proto (T1). The older DoD copy has `access`, `qos`, `opex` but
    not `caveat`/`releasableTo` (T2), so older peers may drop them.
21. T1 on `access`: "field was optional in early cot implementations but now required in
    MIL-STD-6090 … updated/new clients should be populating on all outgoing message and
    should treat any missing/empty value as CoT value 'Undefined'", and "Undefined" is sent
    by omitting the field. The proto gives **no value list** for any of the three fields.
22. TAK Server reads all three from the XML root (`getAccess`, `getCaveat`,
    `getReleaseableTo`, T6) and stores `caveat` and `releaseableto` columns in
    `cot_router` (T6 migration V87).
23. TAK Server's `ClassificationServiceImpl` is "based on the Information Security Marking
    Metadata specification" (IC ISM). For a CoT event it builds
    `access//caveat//releasableTo` and asks an external ISM service
    (`ismControllerCanAccess`) whether the receiving user (classifications, SCI controls,
    ACCMs, country) can see it. Results are cached in Ignite (T3).
24. That filter runs only when `<vbm enabled="true">` (T4 lines 246-247, 454-458). Defaults
    are `enabled=false`, `ismStrictEnforcing=false`, and with no `ismUrl` or no marking the
    check **allows** unless strict mode is on (T3, T5 lines 1564-1576). [verifier: with VBM on and no
    marking on the event, T3 `canAccess` first falls back to `<vbm networkClassification>` (ClassificationServiceImpl.java
    lines 46-57), and with no `ismUrl` returns `!ismStrictEnforcing` (lines 130-136).] omos has `<vbm/>`,
    so it is off (O6 line 102).
25. Group reachability is always applied when routing (T4 lines 472, 488). Groups come from
    the auth backend (File, LDAP with `filtergroup`/`groupprefix`) or from client
    certificate fields (`x509groups` default true) (T5 lines 291-348). omos uses the File
    backend in its Git copy of CoreConfig (O6 line 11); an LDAP variant is staged (omos
    `platform/takserver/deploy/coreconfig-ldap-auth.yaml`).
26. TAK revocation: CoreConfig supports a `crlFile` and `enableOCSP` (default `false`)
    (T5 lines 75, 672-692). The omos Git copy sets neither (O6, grep).

### D. Standards and patterns
27. Zero trust (S1): "no implicit trust granted to assets or user accounts based solely on
    their physical or network location"; authentication and authorization "are discrete
    functions performed before a session to an enterprise resource is established".
28. OWASP LLM06 (S2): causes are excessive functionality, permissions and autonomy.
    Mitigations: human-in-the-loop for high-impact actions; least privilege (S2 wording: "Limit the
    permissions that LLM extensions are granted to other systems to the minimum necessary"); "Implement
    authorization in downstream systems rather than relying on an LLM to decide".
29. OpenFGA conditions (S3) add ABAC to ReBAC: a CEL expression with typed parameters
    (timestamp, duration, ipaddress, string, list, map…), merged from tuple context and
    request context, where tuple context wins; CEL cost capped at 100 by default.
30. Kyverno ImageValidatingPolicy is "Stable • Kyverno v1.19" (S4) and verifies cosign
    keyless signatures by `subject`/`issuer` and in-toto attestations. The omos policy
    comments describe Kyverno 1.19.1 (O1); the live version was not queried.
31. SLSA Build L2 needs a hosted build platform with signed provenance; L3 adds isolation
    between runs and keeps signing secrets away from build steps (S5). [verifier: the v1.0 page is marked
    retired; v1.2 is the current SLSA spec, not checked here.] GitHub's `attest`
    action produces signed provenance and SBOM attestations verified with
    `gh attestation verify` (S6).
32. TLP 2.0 labels (S7): RED (individual recipients only), AMBER (need-to-know within the
    organization and its clients; AMBER+STRICT within the organization only), GREEN
    (community, "not via publicly accessible channels"), CLEAR (no limit).

---

## How it applies to OneMind

### 1. Zero trust across sites
Sites are the cloud hub, the owner's homestead (Mac today, home server later), field kits
and partner organizations. Apply S1 literally: being on the tailnet or inside the cluster
grants nothing by itself.

| Layer | Control | Where it lives |
|---|---|---|
| Network | Tailscale grants per tag; NetworkPolicy per namespace | `platform/core/policy.hujson`, `platform/*/networkpolicy*.yaml` |
| Bus identity | One NATS user (later JWT user) per service, per site leaf, per client | omos NATS config / account JWTs |
| Bus authorization | Subject allow-lists on that user, enforced at the hub for leafs (fact 16) | hub config, not the site |
| Action authorization | `svc.policy.*` service checks OpenFGA before any `cmd.*` is emitted | omos `services/` (new) |
| Action integrity | Policy service signs approved commands; the edge bridge verifies before acting | bridge code |
| Audit | Every decision as `evt.policy.{decision}.{result}` on a JetStream stream | grammar `evt.{domain}.{entity}.{event}` |

Consequences for each site:
- **Each site leaf gets its own hub user** whose publish allow-list is that site's subjects
  and whose subscribe allow-list is what the site may receive (fact 16 proves the hub
  enforces this). No more shared `leaf_tak`.
- **Commands to a site are allow-listed by entity id**, e.g. subscribe
  `cmd.drone.sim-x1.>` for the homestead leaf, so a compromised hub user cannot steer
  another site's assets through that leaf.
- **The site keeps its own policy when disconnected.** The edge bridge (MAVLink, ROS) must
  verify the command signature and expiry locally, because the hub may be unreachable or
  compromised.
- **Tailscale grant to review:** `tag:k8s` may reach `omos-mbp` on `tcp:22` (O5). Any
  cluster workload that can use a Tailscale egress proxy can open SSH to the owner's Mac.
  This crosses a site boundary on network trust alone.

### 2. mTLS and certificates
- **NATS routes and leafs** already use TLS with a private CA (fact 13). Add `verify: true`
  on `leafnodes.tls` so a leaf must present a CA-signed client certificate in addition to
  its credential (N1). Edge leafs must verify the hub's certificate (the Mac leaf template
  has `tls insecure`, `nats-platform.md` R4).
- **NATS clients in-cluster:** per-service credentials first (cheap, SealedSecrets), then
  operator/JWT mode. `verify_and_map` (N1) can map a cert-manager-issued client certificate
  to a user, but adds a cert per service; prefer JWT for services and keep mTLS for leafs.
- **CA lifetime:** the NATS CA is 10 years and self-signed (L3). Keep it, but put the CA key
  in the backup set and document rotation; a rotation needs every leaf's trust bundle
  updated.
- **TAK:** one TAK client identity per deployment (the server client bridge in
  `tak-protocols.md`), not certificates on viewers. Lost EUD handling needs a CRL
  (`crlFile`) or OCSP, both off by default (fact 26). Enrollment on 8446 stays
  tailnet-only.
- **WebSocket clients** (L0 web, Quest WebXR): TLS terminates at ingress; identity comes
  from Authentik (already running, L6) via NATS auth callout (O3 §5 lists callout as the
  Phase 7 path) into the `CLIENT` account, so a browser never holds a shared password.

### 3. NATS accounts
Adopt the account plan in `nats-platform.md` §4 (`SYS`, `ONEMIND`, `CLIENT`, later
`AGENTS`), with these security additions:
- Only the policy service may publish `cmd.>` and `tak.cot.inject`; collectors may not.
- Hermes gets its own user and leaf credential; the agent's tools reach the bus only
  through `svc.*` imports (no direct `cmd.>`).
- A **partner account per sharing group** (`PARTNER-<org>`) that imports only a filtered
  export produced by a release service (section 5). Because NATS permissions are
  subject-only (N2) and the grammar is locked, marking-based release cannot be expressed as
  a subject token; the account boundary is the enforcement point.

### 4. ABAC with OpenFGA
OpenFGA is already deployed (fact 8). Use it as the decision point for I4 and D3/D5:
- Relations (ReBAC): `user:zeus` `operator` of `asset:sim-x1`; `agent:hermes` `proposer`
  on `asset_class:drone`; `group:homestead` `viewer` of `layer:fleet`.
- Conditions (ABAC, S3): `in_geofence` (point and polygon parameters, evaluated by the
  policy service before calling Check, since CEL has no geometry type), `within_window`
  (timestamp/duration), `marking_allows` (list of the requester's groups vs the item's
  releasability).
- Rule shape for commands: `can_execute = operator and confirmed_by_human`;
  `can_propose = proposer`. An agent never gets `operator` (S2 "complete mediation").
- OpenFGA runs one replica (L6). The policy service must **fail closed** for `cmd.*`
  (deny if OpenFGA is unreachable), unlike Kyverno's fail-open admission (fact 1).

### 5. CoT access fields, classification and sharing between groups
OneMind is a civilian NGO platform. US classification markings (MIL-STD-6090, IC ISM)
are the wrong vocabulary for its own data, but OneMind must interoperate with TAK servers
that may enforce them.

- **OneMind marking model:** every record carries a marking of a TLP 2.0 label (S7) plus a
  releasability list of OneMind groups. On the bus it travels in message headers
  (`OneMind-Marking`, `OneMind-Releasable-To`) and in the `ent` state payload, never in the
  subject (grammar is locked; header names are not subjects).
- **Default:** owner's homestead data AMBER+STRICT; public open feeds CLEAR; partner-shared
  incident data GREEN or AMBER with an explicit releasability list.
- **Enforcement points**, because the bus cannot read headers for authorization:
  1. **Release service** (cross-domain-guard pattern): consumes from `ONEMIND`, drops
     anything whose marking does not allow the target group, and republishes into that
     group's `PARTNER-*` account export. Audits every drop.
  2. **TAK projection / server client:** before a CoT leaves for a TAK Server or mesh,
     check the marking. Only CLEAR/GREEN (or AMBER to the owner's own TAK) go out.
  3. **TAK groups** do the routing inside TAK (fact 25): map OneMind groups 1:1 to TAK
     groups (File/LDAP), so TAK reachability matches OneMind releasability.
- **Outgoing CoT fields:** TAK Server joins `access//caveat//releasableTo` and sends it
  to an IC ISM service when VBM is on (fact 23). A TLP string there would not be a valid
  ISM marking. So OneMind should **omit `access` (= "Undefined") by default** and set it
  only from a per-federation config that names the exact marking string the partner
  server expects. Never put OneMind group names in `releasableTo` on federated links; in
  ISM that field is a country/organization releasability list (**inference** from T3
  passing `country` to the ISM user; value list unverified).
- **Incoming CoT fields:** preserve `access`, `caveat`, `releasableTo` verbatim in the
  entity record (they are dropped by takproto, `tak-protocols.md`) and treat any non-empty
  `access` other than an unclassified value as AMBER+STRICT until a human reviews it.

### 6. Supply chain
- **Images:** keep Kyverno's tag rule, add digest pinning for everything (fact 2), then add
  a Kyverno ImageValidatingPolicy (fact 30) that requires a cosign keyless signature from
  `https://token.actions.githubusercontent.com` with subject
  `https://github.com/onemindos/omos/.github/workflows/<build>.yml@refs/heads/main` for
  `ghcr.io/onemindos/*`. Start in Audit.
- **Builds:** every `build-*.yml` emits SBOM and provenance (as `build-nodered-tak.yml`
  does), signs with cosign keyless, and attests with `attest` (S6). That meets SLSA Build
  L2 (S5). The self-hosted runner for TAK images weakens L3 isolation; record it.
- **Actions:** SHA-pin the three `tailscale.yml` actions (fact 4).
- **Runtime installs:** bake Hermes' Python dependencies into a GHCR image with a lockfile
  and hashes; remove the start-time `uv pip install` (fact 5).
- **onemind-xr:** lockfiles for npm/Python, `npm ci`, licence check (GPL-compatible, no
  AGPL/Elastic code), and a vendored Cesium/Unreal plugin list with versions and hashes.

### 7. Threat model (C2 platform)

Assets: the command path to drones/robots (`cmd.*` → bridges), the common operating
picture (`ent.*`, `tak.cot.*`), the owner's location and homestead data, credentials
(NATS, TAK certs, tailnet, cluster), and the GitOps repo.

| # | Threat actor / event | What it can do today | Control that stops or limits it |
|---|---|---|---|
| T1 | **Compromised or prompt-injected agent (Hermes)** — a feed item, chat or web page carries instructions (S2) | Hermes uses the shared `leaf_tak` into `ONEMIND` with no subject limits (facts 12, 15): it could publish `cmd.>` or `tak.cot.inject` directly | Own user limited to `agents.>` + `svc.*`; commands only as `svc.policy.*` proposals; OpenFGA `can_propose` ≠ `can_execute`; human confirm in the UI (run rule 6); signed commands; rate limit per agent; audit stream |
| T2 | **Compromised edge node / site leaf** (stolen field kit, malware on the Mac) | With the shared password it is a full `ONEMIND` member | Per-site hub user with both-direction allow-lists (fact 16); leaf mTLS; revoke the site's user (N3); hub never sources edge `cmd.*` |
| T3 | **Compromised cluster service** (RCE in a feed collector) | Same unrestricted `onemind` user; NetworkPolicy lets 6 namespaces reach 4222 (L4) | Per-service publish allow-list (collectors `intel.>`/`tel.>`/`det.>` only); Falco alerts delivered somewhere (fact 6) |
| T4 | **Spoofed or poisoned data** (fake aircraft, fake CoT, mesh injection) | Anyone on `tak.cot.inject` puts markers on every TAK device (`tak-server-and-bridges.md` risk 4); NATS has no publisher identity (fact 18) | Only the projection service may publish `tak.cot.inject`; `source_id` + signature on `ent` updates from bridges; provenance shown in UI; mesh input treated as untrusted (AMBER, `how` checked) |
| T5 | **Command replay or tampering** | A recorded `cmd` message can be re-sent by any publisher | Signed command with nonce, `expires`, target id; bridge keeps a nonce cache; JetStream `Nats-Msg-Id` dedup |
| T6 | **Lost or stolen EUD / client token** | TAK cert stays valid (no CRL/OCSP, fact 26); browser holds shared NATS password | TAK CRL; Authentik-backed short-lived NATS tokens via auth callout; revoke on report |
| T7 | **Supply-chain compromise** (PyPI package, mutable tag, unpinned action) | Hermes installs unpinned packages at start (fact 5); `nats:2.10-alpine` mutable (fact 2); tailnet-policy workflow uses tag-pinned actions (fact 4) | Section 6 |
| T8 | **Lateral movement across sites over the tailnet** | `tag:k8s` → owner Mac SSH (O5) | Remove or narrow the grant; Tailscale SSH with check mode for humans only |
| T9 | **Kubernetes API attack** | Public endpoint, no firewall (fact 10) | Control-plane firewall (O2 H3), owner decision |
| T10 | **Over-sharing to partners / federation** | No marking model; TAK VBM off (fact 24) | Section 5 release service; outgoing CoT checks |
| T11 | **Silent detection failure** | Falco/Alertmanager may deliver nowhere (fact 6); backups same provider (O2 A18) | Alert receiver (Telegram is retained, omos `CLAUDE.md`); off-provider backup copy |
| T12 | **Denial of service** (JetStream disk fill, slow consumers) | Only WORLD has `maxBytes` (`nats-platform.md` R1) | Byte caps; account JetStream limits per account (today all `-1`, L2) |

---

## Risks

| # | Risk | Evidence | Mitigation |
|---|---|---|---|
| R1 | Any holder of the single `onemind` password (including via public `ws.onemindos.dev`) controls the command path | facts 12, 14 | Per-service users and `CLIENT` account before any real asset is connected (decision 0004 #13 keeps everything `sim-*` until then) |
| R2 | Prompt injection turns Hermes into an operator | facts 12, 15; S2 | Section 4 and T1 controls; never give an agent `cmd.>` |
| R3 | Kyverno fails open, so "enforced" policies are bypassed during a Kyverno outage | fact 1 | Accept for hygiene rules; never rely on Kyverno for image trust alone; add a second replica when hub-core memory allows |
| R4 | Signature verification blocks rollouts if CI signing breaks | S4 | Start ImageValidatingPolicy in Audit; exclude third-party images by registry |
| R5 | TLP strings in CoT `access` confuse a VBM/ISM-enforcing partner server and get data dropped or wrongly released | facts 23-24 | Omit `access` by default; per-federation marking config |
| R6 | Older peers drop `caveat`/`releasableTo` | fact 20 | Never rely on CoT fields for enforcement; enforce before send |
| R7 | Public omos repo exposes topology (tailnet IPs, CoreConfig) | fact 11 | Accept; keep secrets sealed; keep gitleaks gating |
| R8 | JWT migration disconnects clients (rotation disconnects by design, O3 §5) | O3 | Do it in a maintenance window with a rollback config |
| R9 | Command signing adds key management on edge devices | — | One Ed25519 public key per policy service, distributed with the bridge config; rotation by key id |
| R10 | Grammar: FEATURES D3 writes `cmd.fleet.{class}.{id}.{verb}` (5 tokens) vs the locked `cmd.{domain}.{entity}.{action}` | `docs/FEATURES.md` line 76; omos `CLAUDE.md` | Permission allow-lists depend on a fixed shape; use `cmd.drone.{id}.{verb}` and raise D3 with the contracts lane |

---

## Recommendation

Make the **command path** zero-trust first, because it is the only place a breach becomes
physical, then the data-sharing path, then the supply chain. All platform items are omos PRs
(PRs only, never merged by this run), and none needs paid infrastructure.

1. **NATS identities and permissions (omos PR, with `nats-platform.md` §6 item 4).**
   Per-service users with publish allow-lists; only the policy service publishes `cmd.>` and
   `tak.cot.inject`; separate Hermes and TAK leaf users; `CLIENT` account for viewers;
   per-site leaf users with both-direction allow-lists (proved, fact 16); `verify: true` on
   `leafnodes.tls`. JetStream limits per account.
2. **Policy service `svc.policy.*` (omos, new service) with OpenFGA.** Proposals in,
   decisions out on `evt.policy.*`, fail closed, human confirmation for every asset command
   (run rule 6), Ed25519-signed commands with nonce and expiry, verified by the MAVLink/ROS
   bridges. This is FEATURES I4/D3/D5. Build and test locally against the local nats-server
   and the `sim-x1` drone.
3. **Marking model (onemind-xr contract + omos release service).** TLP 2.0 + releasability
   in headers and `ent` payloads; release service into `PARTNER-*` accounts; OneMind groups
   mapped to TAK groups; outgoing CoT omits `access` unless a federation config says
   otherwise; incoming CoT security fields preserved.
4. **Supply chain (omos PRs, small).** SHA-pin `tailscale.yml`; digest-pin the Hermes
   `nats` sidecar and agent image; bake Hermes Python deps into an image; add SBOM,
   provenance, cosign keyless signing and `attest` to every `build-*.yml`; Kyverno
   ImageValidatingPolicy in Audit for `ghcr.io/onemindos/*`.
5. **Owner decisions (write to `OWNER-CHECKLIST.md` / final report, do not act):** narrow the
   `tag:k8s` → `omos-mbp:22` grant; enable the DOKS control-plane firewall (O2 H3); confirm
   an Alertmanager receiver; TAK CRL process for lost devices; Authentik + auth callout for
   browser clients.
6. **Workload identity:** do not reintroduce SPIRE now (hub-core memory, O2 H1). NATS JWT
   users plus Kubernetes ServiceAccount tokens cover service identity for this phase.

---

## Unverified items
- Live Alertmanager receiver and live Falco delivery (config is in a Secret; not read).
- Whether the live TAK core uses the Git `CoreConfig.xml` in `deploy/` (the core is a Helm
  CLI release, omos `CLAUDE.md`); VBM, CRL and OCSP state on the live TAK Server.
- TAK Server 5.8 behaviour of the classification filter: all TAK facts are from 5.7 source
  (5.8 is not public, `tak-server-and-bridges.md`).
- MIL-STD-6090 and IC ISM value sets for `access`, `caveat`, `releasableTo` (not public or
  not read here); the claim that `releasableTo` is a country/organization list is an
  inference.
- Why SPIRE disappeared between 2026-10-10 and today (no removal record found by grep).
- Whether GitHub artifact attestations are available for this org (public repo, free plan;
  S6 does not state plan limits).
- NATS auth callout with Authentik, and JWT revocation end to end, were not run
  (`nats-platform.md` P9 gap stands).
- The Mac leaf's live configuration (only the Git copy was read by the NATS lane).
- Whether anything currently connects to `ws.onemindos.dev`.

## Blockers
- None from the permission system in this lane. No Secret values were read, by rule, which
  is why the items above that live in Secrets stay unverified.

## Independent verification (2026-10-29)

Verifier re-checked 24 load-bearing claims against the primary sources. Method: omos repo at
`62e3d22b` (grep/sed), read-only `kubectl get` on `do-nyc3-omos-cloud` (certificates, pod images,
ingress, spire-system pods, clusterpolicy; no Secret or ConfigMap read), `gh api` raw reads of
`TAK-Product-Center/Server` (latest tag `5.7-RELEASE-14`) and `nats-io/nats-server`, WebFetch of
N1, N2, N3, T1, S1-S5, S7, and a re-run of the P1 leaf test on fresh local ports with nats-server v2.15.2.

**Confirmed (18):** fact 1 (two Enforce rules, all `failurePolicy: Ignore`, kyverno-policies-audit.yaml
lines 62-63, 157-158, 237-238, 280-281; 4 Ready live); fact 2/L7 (`nats:2.10-alpine` and
`hermes-agent:v2026.9.24` without a digest in `hermes-legacy`); fact 3 (no `verifyImages`/ImageValidatingPolicy;
only `build-nodered-tak.yml` has provenance/sbom; no cosign/attest); fact 4 (83 `uses:`, exactly the three
`tailscale.yml` lines 24, 47, 57 not SHA-pinned); fact 5 (unpinned `uv pip install`, deployment.yaml ~111-115);
fact 7/L6 (spire-system has no pods; O2 line 86 lists 3 SPIRE containers); fact 10 (CoreConfig line 7
`clientAuth="false"`); fact 13 L3 (`nats-ca` isCA, issuer `nats-selfsigned`, 87600h; `nats-server-tls` 8760h);
fact 14/L5 (`ws.onemindos.dev` allowlisted, matrix and matrix-wellknown not); facts 16 and 17 (P1 re-run
reproduced `hubsub.txt` and `leafsub.txt` line for line; `permissions` in `leafnodes.authorization.users`
gives `unknown field "permissions"`); fact 19 (N3: "revoke the user and re-push the account"; N6 opts.go
has `TLSVerify`, `OCSPConfig`); facts 20-21 (T1 field numbers 2/16/17, no value lists); fact 22 (T6
CotEventContainer lines 173, 263-267; V87 adds `caveat`, `releaseableto`); fact 23 (T3 cites ISM, joins
with `//`, calls `ismControllerCanAccess`, passes country, Ignite cache); fact 25 (T4 lines 472, 488;
T5 `x509groups` default true); fact 26 (T5 line 75 `crlFile`, line 692 `enableOCSP` default false; omos
copy has neither); facts 27, 29, 30, 32 (S1, S3 types/precedence/CEL cost 100, S4 "Stable • Kyverno v1.19",
S7 definitions); T8 grant (`policy.hujson` lines 55-57 `tag:k8s` to `omos-mbp` `tcp:22`); L8 (repo PUBLIC);
O2 A17 (null receiver, line 253); R10 (FEATURES.md line 76 uses `cmd.fleet.{class}.{id}.{verb}`).

**Corrected in place (6):** N2 source line and fact 12 (the quote was a paraphrase; the page says "can do
anything on the server", and `default_permissions` is a caveat); fact 13 (the N1 quote was wrong; it is now the
page wording); fact 24 (added the `networkClassification` fallback); fact 28 ("least privilege" is not S2
wording; the actual sentence is added); fact 31 (SLSA v1.0 page is retired, v1.2 current).

**Marked unverified (0 new):** the existing Unverified list stands. Not re-checked by the verifier: L2 and
L4 (they need the NATS ConfigMap and NetworkPolicy dumps; the verifier did not read a ConfigMap that holds
credential values), fact 8 / O7 (OpenFGA used by Backstage only), N5, S6.

**Licence and cluster safety:** no worldmonitor/worldwideview code or text found in this document. No
cluster mutation found. One flag: L2 says the author read `cm/nats-config` with values masked. That means live
NATS credentials sit in a ConfigMap, not a Secret. Reading it is allowed by the lane rule (it is not a Secret),
but any future reader must mask the values. Moving them to a Secret is an omos follow-up (**unverified** by the
verifier, who did not open that ConfigMap). The `.run/security-lane/*.conf` passwords are throwaway
local test values on 127.0.0.1.

**Verifier process incident:** on the first P1 re-run, the throwaway hub failed to start because
127.0.0.1:38222 was in use. The test publishes then went to the ingestion lane's proof server
(`ingest-proof`, 127.0.0.1:34222, config `docs/evidence/M0/ingestion/proof/scripts/nats-ingest.conf`).
It has 0 leaf connections, so nothing reached the main 4222 bus or the drone. One message, subject
`intel.a.b.c` with body `from-hub:intel.a.b.c`, is now stored in that server's `INTEL` stream; the
other two (`evt.mission.p1.done`, `cmd.drone.sim-x1.land`) matched no stream there. The verifier did
not delete it (it belongs to another lane). The ingestion lane should exclude or purge it before counting INTEL.
