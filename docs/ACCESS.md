# Access for the AI build run

Least privilege: give each capability only where a milestone needs it. Platform
changes reach the cluster **through omos PRs and Argo**, not direct writes, so a bad
run can be reverted with Git.

## Repositories

| Repo | Access | Why |
|---|---|---|
| `onemindos/onemind-xr` | Write (branches, PRs; merge allowed here) | The client and its docs |
| `onemindos/omos` | Write to branches, open PRs. **No merge to `main`**; owner merges. Keep `main` protected. | Spine: feeds, entity service, projector, bridges, streams |
| Public repos (TAK-Product-Center, CesiumGS, the four dashboards) | Read, no setup | Research and donor code |

The person running the session needs to be a collaborator on both repos.

## Cluster (`omos-cloud`, context `do-nyc3-omos-cloud`)

| Grant | Scope | Why |
|---|---|---|
| Read-only kubeconfig: a dedicated ServiceAccount bound to the built-in `view` ClusterRole (no Secrets) | Cluster-wide | Verify live state, read logs, check Argo sync, find what writes `kv-world-state` |
| Optional sandbox: `admin` RoleBinding in one new namespace, e.g. `onemind-dev` (created by an omos PR) | That namespace only | Spikes that need a pod in the cluster |
| Argo CD read-only account | Argo UI and CLI | Watch syncs after the owner merges |
| **Not granted** | — | Writes outside the sandbox, Secrets, SealedSecrets keys, `helm upgrade` of `nats`, `takserver` or `argocd`, Terraform, DigitalOcean API writes, Cloudflare DNS |

## NATS

A dedicated NATS user for the run (sealed into omos, or issued to the local leaf):

| Permission | Subjects |
|---|---|
| Subscribe | `>` (read everything) |
| Publish | `tak.cot.mesh.tx`, `cmd.fleet.*.sim-*.>` (simulated assets only), `svc.>` requests, `_INBOX.>` |
| JetStream / KV | Read all; write only buckets created for the run |
| **Not granted** | Publish on `tak.cot.inject` or `cmd.*` for real assets, until M4 with owner sign-off |

## TAK Server

| Grant | Why |
|---|---|
| A normal (non-admin) TAK user and client certificate for "onemind-xr", enrolled through TAK Portal (`enroll.tail717ba0.ts.net`) | The client and tests connect as a TAK contact on 8089 |
| **Not granted:** the TAK admin certificate, and the `takserver` Helm release | TAK Server config changes are Helm CLI today. They go to the owner as a reviewed change in `platform/takserver/`. |

## Network

| Grant | Why |
|---|---|
| Tailscale: a tagged node (for example `tag:onemind-agent`) with ACLs to `tak.tail717ba0.ts.net` (8089, 8443), the `omos-nats` leaf port 7422, and the read-only admin UIs (Grafana viewer, Argo) | Reach TAK, the bus and dashboards |
| LAN access on the owner's Mac | Mesh gateway multicast with ATAK devices |

## Build machine (where the run happens)

| Item | Why |
|---|---|
| Claude Code, Git, Go, Node, Python, Docker, the `nats` CLI | omos services, SITL drones, tests |
| Unreal Engine 5.8 (Epic account) and about 150 GB free disk; a GPU if possible | Build the client; a Mac build machine can package the macOS app directly |
| Tailscale node and the cluster and NATS grants above | Reach the bus, TAK Server and dashboards |

## Owner's Mac (acceptance, run by the owner from `docs/OWNER-CHECKLIST.md`)

| Item | Why |
|---|---|
| Epic Games account and Unreal Engine 5.8, Xcode | Build the client |
| Cesium ion access token in the macOS keychain | Terrain and imagery; never in Git |
| Android SDK/NDK and Meta Quest developer mode on the Quest Pro (DK94EC) | Spike S3 |
| Go, Node, Python, the `nats` CLI, Docker | omos services, SITL drones, tests |
| An ATAK phone or tablet on the same Wi-Fi | Mesh and TAK tests |

The run happens on the build machine. Mac-only steps (packaging on the owner's Mac,
streaming from it, the headset, the LAN mesh with ATAK) become exact steps in
`docs/OWNER-CHECKLIST.md`, which the owner runs.
