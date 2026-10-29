// The Act test stack (test fixtures only; nothing here ships in the client build).
//
// Real components, built from their omos branches when available:
//   - policy service, omos #554 feat/policy-service (services/policy), POLICY_AUTHZ=local with the
//     branch's OpenFGA model and a per-run tuples file. Built from a temp copy with
//     policy-zone-actions.patch applied: owner proposal D1-P4 of omos #569 (the `zone.*` catalogue
//     entries, A2, executed on `cmd.map.geofence.{id}.{verb}`), which #554 does not carry yet; the
//     schema dir adds #569's `zone.{create,update,remove}.v1` (test/fixtures/schemas/actions)
//   - MAVLink bridge, omos #550 feat/mavlink-bridge-fleet (services/bridge/bridges/mavlink), with
//     signature checking on (Ed25519 key generated per run)
//   - the scripted MAVLink drone, onemind-xr scripts/sim/mavlink_sim_drone.py (sim-x1)
// Stand-ins for what is unmerged, missing or not runnable here (each says why):
//   - entity: omos #552 never sets `capabilities`, so the real policy would deny every goto
//     `not_capable`; this stand-in turns `tel.fleet.drone.sim-x1.*` into `ent.drone.sim-x1.state`
//     and answers `svc.entity.get` with the capabilities the bridge accepts
//   - geofence writer and watcher (omos #569 is not runnable from this suite yet): like #569, it
//     applies a `cmd.map.geofence.{id}.{verb}` only when it carries a valid policy `sig` (Ed25519
//     over the JCS envelope), a future `expires_at` and `approval.status: approved`; it answers
//     on `evt.map.geofence.{created|updated|deleted|refused}` (D1-P2) so the policy service closes
//     the action, mirrors zones to `ent.zone.*`, and raises `evt.map.geofence.{id}.{enter|exit}`
//   - feeds: `tel.feed.{source}.status` heartbeats in the omos #553 feed-status.v1 shape
//   - Hermes: a scripted model (no paid model API may be called here) on
//     `agents.prompt.hermes.zeus.legacy` with the AGENTS permissions of omos #562
//   - `svc.cot.send` (the projector endpoint is not built anywhere) and a scripted ATAK peer on
//     `tak.cot.out`
// Every password is random per run and lives only in the temp dir; nothing is committed.
import { spawn, spawnSync, type ChildProcess } from "node:child_process";
import { createHash, generateKeyPairSync, randomBytes, randomUUID, verify as edVerify, type KeyObject } from "node:crypto";
import { cpSync, existsSync, mkdirSync, mkdtempSync, readdirSync, rmSync, writeFileSync } from "node:fs";
import { createServer, connect as tcpConnect } from "node:net";
import { createSocket } from "node:dgram";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { headers, wsconnect, type NatsConnection } from "@nats-io/nats-core";
import { splitPrompt } from "../../src/act/hermes";
import { destination, distanceM, type LonLat } from "../../src/act/geo";
import { xmlEscape } from "../../src/act/cot";
import { canonicalize } from "../../src/act/jcs";

const WORK = process.env.ONEMIND_WORK ?? "/Users/danielcarku/Desktop/work";
const TMP_ROOT = process.env.ACT_TMP ?? join(WORK, ".run", "tmp-m4-web");
const POLICY_DIR = process.env.OMOS_POLICY_DIR ?? join(WORK, "wt/i4-policy");
const BRIDGE_DIR = process.env.OMOS_BRIDGE_DIR ?? join(WORK, "wt/h2-mavlink");
const SIM = process.env.SIM_DRONE ?? join(WORK, "onemind-xr/scripts/sim/mavlink_sim_drone.py");
const PY = process.env.PYTHON ?? join(WORK, ".tools/py/bin/python");
const HERE = dirname(fileURLToPath(import.meta.url));
const POLICY_PATCH = join(HERE, "policy-zone-actions.patch");
const ZONE_SCHEMAS = join(HERE, "..", "fixtures", "schemas", "actions");
/** A non-simulated drone record (a fixture: nothing commands it; the policy must refuse it). */
export const NONSIM_ID = "nonsim-x9";

export const CENTER: LonLat = [-97.7431, 30.2672];
/** Fixture places for Hermes, east of home (zones published at start). */
export const NORTH_FIELD: LonLat = destination(destination(CENTER, 0, 900), 90, 900);
export const HOME_FIELD: LonLat = CENTER;
export const TRACK_ID = "trk-sim-walker-01";

const enc = new TextEncoder();
const dec = new TextDecoder();
const pw = () => randomBytes(18).toString("hex");

function bin(name: string): string {
  for (const p of [join(WORK, ".tools/bin", name), `/opt/homebrew/bin/${name}`, `/usr/local/bin/${name}`]) if (existsSync(p)) return p;
  return name;
}

export function freePort(udp = false): Promise<number> {
  if (udp) {
    return new Promise((resolve) => {
      const s = createSocket("udp4");
      s.bind(0, "127.0.0.1", () => {
        const p = s.address().port;
        s.close(() => resolve(p));
      });
    });
  }
  return new Promise((resolve, reject) => {
    const s = createServer();
    s.once("error", reject);
    s.listen(0, "127.0.0.1", () => {
      const { port } = s.address() as { port: number };
      s.close(() => resolve(port));
    });
  });
}

async function waitTcp(port: number, ms = 15000): Promise<void> {
  const start = Date.now();
  while (Date.now() - start < ms) {
    const ok = await new Promise<boolean>((r) => {
      const c = tcpConnect(port, "127.0.0.1", () => {
        c.destroy();
        r(true);
      });
      c.on("error", () => r(false));
    });
    if (ok) return;
    await new Promise((r) => setTimeout(r, 100));
  }
  throw new Error(`port ${port} did not open`);
}

function kill(p: ChildProcess | undefined): Promise<void> {
  return new Promise((resolve) => {
    if (!p || p.exitCode !== null) return resolve();
    p.once("exit", () => resolve());
    p.kill("SIGTERM");
    setTimeout(() => p.exitCode === null && p.kill("SIGKILL"), 3000);
  });
}

/** Build a Go binary from a branch worktree (read-only: -mod=readonly, output in the temp dir). */
function goBuild(dir: string, pkg: string, out: string): string | null {
  const go = bin("go");
  const env = {
    ...process.env,
    PATH: `${join(WORK, ".tools/bin")}:${process.env.PATH}`,
    GOPATH: process.env.GOPATH ?? join(WORK, ".tools/gopath"),
    GOMODCACHE: process.env.GOMODCACHE ?? join(WORK, ".tools/gopath/pkg/mod"),
    GOFLAGS: "-mod=readonly",
  };
  if (!existsSync(dir)) return null;
  const r = spawnSync(go, ["build", "-o", out, pkg], { cwd: dir, env, encoding: "utf8" });
  return r.status === 0 ? out : null;
}

/** The #554 policy service with D1-P4 applied (a temp copy; the branch worktree is never touched). */
function buildPatchedPolicy(out: string): string | null {
  const src = join(POLICY_DIR, "services/policy");
  if (!existsSync(src)) return null;
  const copy = join(TMP_ROOT, "policy-src");
  rmSync(copy, { recursive: true, force: true });
  cpSync(src, copy, { recursive: true });
  const r = spawnSync("patch", ["-p1", "--forward", "-s", "-i", POLICY_PATCH], { cwd: copy, encoding: "utf8" });
  if (r.status !== 0) {
    process.stderr.write(`policy-zone-actions.patch does not apply to ${src}:\n${r.stdout}${r.stderr}\n`);
    return null;
  }
  const bin = goBuild(copy, "./cmd/policy", out);
  rmSync(copy, { recursive: true, force: true });
  return bin;
}

export interface Availability {
  policy: string | null;
  bridge: string | null;
  python: boolean;
  reason: string;
}

let built: Availability | null = null;
export function buildReal(binDir = join(TMP_ROOT, "bin")): Availability {
  if (built) return built;
  mkdirSync(binDir, { recursive: true });
  const policy = process.env.POLICY_BIN && existsSync(process.env.POLICY_BIN) ? process.env.POLICY_BIN : buildPatchedPolicy(join(binDir, "policy"));
  const bridge = process.env.BRIDGE_BIN && existsSync(process.env.BRIDGE_BIN) ? process.env.BRIDGE_BIN : goBuild(join(BRIDGE_DIR, "services/bridge/bridges/mavlink"), ".", join(binDir, "bridge-mavlink"));
  const python = existsSync(PY) && existsSync(SIM) && spawnSync(PY, ["-I", "-c", "import pymavlink"]).status === 0;
  const miss = [!policy && `policy (${POLICY_DIR})`, !bridge && `bridge (${BRIDGE_DIR})`, !python && "python with pymavlink"].filter(Boolean);
  built = { policy, bridge, python, reason: miss.length ? `missing: ${miss.join(", ")}` : "" };
  return built;
}

function natsConf(dir: string, port: number, wsPort: number, p: Record<string, string>): string {
  const clientPerms = `permissions: {
          publish: { allow: ["svc.>", "cmd.map.>", "agents.prompt.hermes.>", "_INBOX.>"], deny: ["cmd.fleet.>"] }
          subscribe: { allow: ["_INBOX.>", "cmd.map.>", "ent.>", "evt.so.action.>", "evt.so.alert.>", "evt.map.>", "tel.feed.>", "tak.cot.>"] } }`;
  // Exactly the CLIENT user of omos #562 (platform/nats/identities/identities.yaml): requests and
  // cmd.map only; no subscription to evt.so.action, tel.feed, tak.cot or ent.
  const client562 = `permissions: {
          publish: { allow: ["svc.layer.>", "svc.entity.>", "svc.policy.>", "svc.cot.send", "cmd.map.>", "agents.prompt.hermes.>"] }
          subscribe: { allow: ["cmd.map.>", "_INBOX.>"] } }`;
  // The CLIENT human of omos #562 as of feat/nats-identities 1f2e33ff (zeus/alice in
  // identities.yaml): requests on the imported services, subscriptions on the read-only stream
  // imports (A19-P11, now with evt.so.alert.>), never ent.*.
  const human562 = `permissions: {
          publish: { allow: ["svc.layer.>", "svc.entity.>", "svc.policy.>", "svc.cot.send", "svc.mission.get", "svc.mission.list", "svc.mission.report", "svc.geofence.alerts", "svc.registry.video.get", "svc.registry.video.list", "svc.tak.mission.>", "cmd.map.>", "agents.prompt.hermes.>"] }
          subscribe: { allow: ["cmd.map.>", "evt.so.action.>", "evt.mission.>", "evt.map.geofence.>", "evt.so.alert.>", "tel.feed.*.status", "tak.cot.out", "tak.cot.mesh.rx", "_INBOX.>"] }
          allow_responses: { max: -1, expires: "5s" } }`;
  // The J2 newcomer role (omos #565 platform/security/roles/onemind-roles.json, plus the A19-P11
  // proposed tel.feed.*.status): layer and entity reads only; no alerts, no policy, no cmd.map.
  const newcomer565 = `permissions: {
          publish: { allow: ["svc.layer.list", "svc.layer.meta", "svc.layer.snapshot", "svc.layer.watch", "svc.layer.unwatch", "svc.entity.get", "svc.entity.search"] }
          subscribe: { allow: ["_INBOX.>", "tel.feed.*.status"] } }`;
  return `
listen: "127.0.0.1:${port}"
websocket { listen: "127.0.0.1:${wsPort}", no_tls: true }
jetstream { store_dir: "${join(dir, "js")}", max_memory_store: 64MB, max_file_store: 256MB }
accounts {
  ONEMIND {
    jetstream: enabled
    users: [
      { user: admin, password: "${p.admin}" }
      { user: policy, password: "${p.policy}", permissions: {
          publish: { allow: ["cmd.>", "evt.so.action.>", "svc.entity.get", "$JS.API.>", "_INBOX.>"] }
          subscribe: { allow: ["svc.policy.>", "evt.drone.>", "evt.robot.>", "evt.vehicle.>", "evt.mission.>", "evt.entity.>", "evt.map.geofence.*", "_INBOX.>"] }
          allow_responses: true } }
      { user: bridge, password: "${p.bridge}", permissions: {
          publish: { allow: ["tel.fleet.drone.sim-x1.>", "evt.drone.sim-x1.>"] }
          subscribe: { allow: ["cmd.fleet.drone.sim-x1.>", "_INBOX.>"] } } }
    ]
    exports: [
      { service: "svc.policy.>" }, { service: "svc.entity.>" }, { service: "svc.cot.send" }, { service: "svc.geofence.alerts" }
      { stream: "ent.>" }, { stream: "evt.so.action.>" }, { stream: "evt.so.alert.>" }, { stream: "evt.map.>" }, { stream: "tel.feed.>" }, { stream: "tak.cot.>" }
    ]
    imports: [
      { stream: { account: CLIENT, subject: "cmd.map.>" } }
      { stream: { account: AGENTS, subject: "cmd.map.>" } }
    ]
  }
  CLIENT {
    users: [
      { user: zeus, password: "${p.zeus}", ${clientPerms} }
      { user: alice, password: "${p.alice}", ${clientPerms} }
      { user: newbie, password: "${p.newbie}", ${clientPerms} }
      { user: carol, password: "${p.carol}", ${client562} }
      { user: dana, password: "${p.dana}", ${human562} }
      { user: rookie, password: "${p.rookie}", ${newcomer565} }
    ]
    exports: [ { stream: "cmd.map.>" } ]
    imports: [
      { service: { account: ONEMIND, subject: "svc.policy.>" }, share: true }
      { service: { account: ONEMIND, subject: "svc.entity.>" }, share: true }
      { service: { account: ONEMIND, subject: "svc.cot.send" }, share: true }
      { service: { account: ONEMIND, subject: "svc.geofence.alerts" }, share: true }
      { service: { account: AGENTS, subject: "agents.prompt.hermes.>" }, share: true }
      { stream: { account: ONEMIND, subject: "ent.>" } }
      { stream: { account: ONEMIND, subject: "evt.so.action.>" } }
      { stream: { account: ONEMIND, subject: "evt.so.alert.>" } }
      { stream: { account: ONEMIND, subject: "evt.map.>" } }
      { stream: { account: ONEMIND, subject: "tel.feed.>" } }
      { stream: { account: ONEMIND, subject: "tak.cot.>" } }
      { stream: { account: AGENTS, subject: "cmd.map.>" } }
    ]
  }
  AGENTS {
    users: [
      { user: hermes, password: "${p.hermes}", permissions: {
          publish: { allow: ["svc.policy.command.propose", "svc.policy.task.propose", "svc.policy.action.get", "svc.entity.>", "agents.>", "cmd.map.>", "_INBOX.>"] }
          subscribe: { allow: ["_INBOX.>", "agents.prompt.hermes.>"] }
          allow_responses: true } }
    ]
    exports: [ { service: "agents.prompt.hermes.>" }, { stream: "cmd.map.>" } ]
    imports: [
      { service: { account: ONEMIND, subject: "svc.policy.command.propose" }, share: true }
      { service: { account: ONEMIND, subject: "svc.policy.task.propose" }, share: true }
      { service: { account: ONEMIND, subject: "svc.policy.action.get" }, share: true }
      { service: { account: ONEMIND, subject: "svc.entity.>" }, share: true }
    ]
  }
}
`;
}

function actionTypes(): string[] {
  return ["goto", "hold", "rtl", "land", "orbit", "observe", "estop", "disarm", "arm", "takeoff", "estop_release", "geofence_override"]
    .map((v) => `asset.${v}`)
    .concat(["alert.acknowledge", "alert.resolve", "zone.create", "zone.update", "zone.remove"]);
}

function tuples(): { user: string; relation: string; object: string }[] {
  const t: { user: string; relation: string; object: string }[] = [
    { user: "user:zeus", relation: "operator", object: "asset:sim-x1" },
    { user: "user:alice", relation: "operator", object: "asset:sim-x1" },
    // carol: the #562 CLIENT permission run (status by polling only)
    { user: "user:carol", relation: "operator", object: "asset:sim-x1" },
    ...["asset.rtl", "asset.goto", "asset.estop"].flatMap((a) => ["proposer", "approver"].map((r) => ({ user: "user:carol", relation: r, object: `action_type:${a}` }))),
  ];
  for (const a of actionTypes()) {
    for (const u of ["user:zeus", "user:alice"]) {
      for (const r of ["proposer", "approver", "second_approver"]) t.push({ user: u, relation: r, object: `action_type:${a}` });
    }
    t.push({ user: "agent:hermes/zeus/legacy", relation: "proposer", object: `action_type:${a}` });
  }
  return t; // user:newbie has no tuple: the newcomer of the J2 test
}

// ---- stand-in services --------------------------------------------------------------------------

interface DroneState {
  lat: number;
  lon: number;
  hae: number;
  course: number;
  speed: number;
  connection: string;
  battery?: number;
  mode?: string;
  t: number;
}

class EntityStandin {
  seq = new Map<string, number>();
  drone: DroneState | null = null;
  timers: NodeJS.Timeout[] = [];
  constructor(private nc: NatsConnection) {}

  next(id: string): number {
    const s = (this.seq.get(id) ?? 0) + 1;
    this.seq.set(id, s);
    return s;
  }

  publish(rec: Record<string, unknown>): void {
    const h = headers();
    h.set("Nats-Msg-Id", `${rec.entity_id}:${rec.seq}`);
    this.nc.publish(`ent.${rec.entity_type}.${rec.entity_id}.state`, enc.encode(JSON.stringify(rec)), { headers: h });
  }

  /** The drone's entity record; `seq` defaults to a new one (reads pass the current seq). */
  droneRecord(seq = this.next("sim-x1")): Record<string, unknown> | null {
    const d = this.drone;
    if (!d) return null;
    return {
      v: 1, uid: "sim-x1", entity_id: "sim-x1", entity_type: "drone", type: "onemind.drone", kind: "multirotor", callsign: "SIM-X1",
      live: true, lifecycle: "active", seq, t: d.t, stale: 10, expiry_t: d.t + 60_000, pinned: false,
      lat: d.lat, lon: d.lon, hae: d.hae, course: d.course, speed: d.speed, disposition: "friendly", cot_type: "a-f-A-M-H-Q",
      location: { lat: d.lat, lon: d.lon, hae_m: d.hae, course_deg: d.course, speed_mps: d.speed, measured_t: d.t },
      health: { overall: d.connection === "online" ? "ok" : "degraded", connection: d.connection, battery_pct: d.battery },
      status: { mode: d.mode },
      // The task types the bridge accepts (omos #550 status meta commands_accepted:
      // goto,rtl,land,arm,disarm,estop,takeoff; it has no hold, and refuses it unknown_action).
      capabilities: ["onemind.task.goto.v1", "onemind.task.rtl.v1", "onemind.task.land.v1"],
      traits: ["locatable", "trackable", "taskable", "observable"],
      indicators: { simulated: true, exercise: false, emergency: false, starred: false, egress_tak: false },
      provenance: { primary: "bridge-mavlink", how: "m-s", source_t: d.t },
      sources: [{ integration: "bridge-mavlink", subject: "tel.fleet.drone.sim-x1.position", last_seen_t: d.t, components: ["location", "health"] }],
    };
  }

  start(): void {
    this.nc.subscribe("tel.fleet.drone.sim-x1.*", {
      callback: (_e, m) => {
        let g: Record<string, unknown>;
        try {
          g = JSON.parse(dec.decode(m.data));
        } catch {
          return;
        }
        const meta = (g.meta ?? {}) as Record<string, unknown>;
        const prev = this.drone;
        const d: DroneState = {
          lat: Number(g.lat), lon: Number(g.lon), hae: Number(g.hae), course: Number(g.course ?? -1), speed: Number(g.speed ?? 0),
          // "unknown" until the bridge's status says otherwise
          connection: prev?.connection ?? "unknown", battery: prev?.battery, mode: prev?.mode, t: Number(g.t) || Date.now(),
        };
        if (m.subject.endsWith(".status")) {
          d.connection = typeof meta.connection === "string" ? meta.connection : d.connection;
          d.battery = typeof meta.battery_pct === "number" ? meta.battery_pct : d.battery;
          d.mode = typeof meta.mode === "string" ? meta.mode : d.mode;
          if (prev) Object.assign(d, { lat: prev.lat, lon: prev.lon, hae: prev.hae, course: prev.course, speed: prev.speed });
        }
        if (!Number.isFinite(d.lat) || !Number.isFinite(d.lon)) return;
        this.drone = d;
      },
    });
    // 2 Hz entity updates, as an entity service would publish fused state.
    this.timers.push(setInterval(() => {
      const r = this.droneRecord();
      if (r) this.publish(r);
    }, 500));
    // A walking track near the north field (sim; FEATURES D4 "what's moving").
    const t0 = Date.now();
    this.timers.push(setInterval(() => {
      const now = Date.now();
      const ang = ((now - t0) / 1000) * (4 / 120); // 4 m/s on a 120 m circle
      const p = destination(NORTH_FIELD, (ang * 180) / Math.PI, 120);
      this.publish({
        v: 1, uid: TRACK_ID, entity_id: TRACK_ID, entity_type: "track", kind: "car", callsign: "WALKER", live: true, seq: this.next(TRACK_ID),
        t: now, stale: 10, expiry_t: now + 60_000, lat: p[1], lon: p[0], hae: 150, course: ((ang * 180) / Math.PI + 90) % 360, speed: 4,
        location: { lat: p[1], lon: p[0], hae_m: 150, course_deg: ((ang * 180) / Math.PI + 90) % 360, speed_mps: 4, measured_t: now },
        disposition: "unknown", indicators: { simulated: true }, provenance: { primary: "fixture", how: "m-s", source_t: now },
      });
    }, 1000));
    // A non-simulated drone (fixture record only): the D3 not_simulated case proposes to it.
    const nonsim = destination(CENTER, 180, 3000);
    this.timers.push(setInterval(() => {
      const now = Date.now();
      this.publish({
        v: 1, uid: NONSIM_ID, entity_id: NONSIM_ID, entity_type: "drone", kind: "multirotor", callsign: "NONSIM-X9", live: true, seq: this.next(NONSIM_ID),
        t: now, stale: 10, expiry_t: now + 60_000, lat: nonsim[1], lon: nonsim[0], hae: 200, course: -1, speed: 0,
        location: { lat: nonsim[1], lon: nonsim[0], hae_m: 200, measured_t: now }, disposition: "friendly",
        indicators: { simulated: false }, provenance: { primary: "fixture", how: "m-s", source_t: now },
      });
    }, 1000));
    this.nc.subscribe("svc.entity.get", {
      callback: (_e, m) => {
        let q: { entity_id?: string } = {};
        try {
          q = JSON.parse(dec.decode(m.data));
        } catch {
          /* empty */
        }
        if (q.entity_id === "sim-x1" && this.drone) {
          m.respond(enc.encode(JSON.stringify({ entity: this.droneRecord(this.seq.get("sim-x1") ?? 0) })));
        } else m.respond(enc.encode(JSON.stringify({ error: { code: "not_found" } })));
      },
    });
  }

  stop(): void {
    for (const t of this.timers) clearInterval(t);
  }
}

/** What the stand-in did with each `cmd.map.geofence.*` it received. */
export interface GeofenceResult {
  subject: string;
  zone_id: string;
  outcome: "created" | "updated" | "deleted" | "refused";
  code?: string;
  message?: string;
  signed: boolean;
  action_id?: string;
  at: number;
}

/** Alert rule per zone kind (omos #569 README "Alerts per zone kind"). */
const ALERT_RULE: Record<string, { when: "inside" | "outside"; level: string; reason: string }> = {
  keep_in: { when: "outside", level: "warning", reason: "left_keep_in" },
  keep_out: { when: "inside", level: "warning", reason: "entered_keep_out" },
  hazard: { when: "inside", level: "caution", reason: "entered_hazard" },
  aoi: { when: "inside", level: "advisory", reason: "entered_zone" },
  control_area: { when: "inside", level: "advisory", reason: "entered_zone" },
};

class GeofenceStandin {
  zones = new Map<string, Record<string, unknown>>();
  inside = new Map<string, boolean>();
  /** open alerts (#569 alert.v1), by alert_id */
  alerts = new Map<string, Record<string, unknown>>();
  /** every evt.so.alert.* fact the stand-in published */
  alertFacts: { subject: string; data: Record<string, unknown>; at: number }[] = [];
  private alertSeq = new Map<string, number>();
  private seeded = new Set<string>();
  results: GeofenceResult[] = [];
  seq = 0;
  constructor(
    private nc: NatsConnection,
    private kv: { put(k: string, v: Uint8Array): Promise<unknown>; delete(k: string): Promise<unknown> },
    private entity: EntityStandin,
    private policyKeys: Map<string, KeyObject>,
  ) {}

  /** Fixture zones present before the run (as if already in KV `geofences`). */
  async seed(id: string, rec: Record<string, unknown>): Promise<void> {
    this.seeded.add(id);
    await this.write(id, rec);
  }

  /** A zone already in KV that the alert rules watch (unlike `seed`): test fixture, no policy. */
  async fixture(id: string, rec: Record<string, unknown>): Promise<void> {
    await this.write(id, rec);
  }

  /** Drop a fixture zone (as a deleted KV key); its alert resolves `zone_removed` on the next tick. */
  async dropFixture(id: string): Promise<void> {
    const cur = this.zones.get(id);
    if (!cur) return;
    await this.kv.delete(id);
    this.zones.delete(id);
    this.entity.publish({ ...cur, live: false, seq: this.entity.next(id), t: Date.now() });
  }

  private async write(id: string, rec: Record<string, unknown>): Promise<void> {
    const full = { ...rec, zone_id: id, entity_id: id, uid: id, entity_type: "zone", live: true, pinned: true, expiry_t: null, t: Date.now(), seq: this.entity.next(id) };
    this.zones.set(id, full);
    await this.kv.put(id, enc.encode(JSON.stringify(full)));
    this.entity.publish(full);
  }

  /** The #569 command gate: policy signature, not expired, approved. Returns a refusal or null. */
  gate(env: Record<string, unknown>): { code: string; message: string } | null {
    const sig = env.sig as { alg?: string; kid?: string; value?: string } | undefined;
    if (!sig || typeof sig !== "object") return { code: "unsigned", message: "command carries no policy signature" };
    const key = this.policyKeys.get(sig.kid ?? "");
    if (sig.alg !== "ed25519" || !key || typeof sig.value !== "string") return { code: "bad_signature", message: `unknown key or algorithm (${sig.kid}/${sig.alg})` };
    const { sig: _drop, ...rest } = env;
    void _drop;
    let ok = false;
    try {
      ok = edVerify(null, Buffer.from(canonicalize(rest)), key, Buffer.from(sig.value, "base64"));
    } catch {
      ok = false;
    }
    if (!ok) return { code: "bad_signature", message: "signature does not verify" };
    const exp = Date.parse(String(env.expires_at ?? ""));
    if (!(exp > Date.now())) return { code: "expired", message: "expires_at is missing or past" };
    if ((env.approval as { status?: string } | undefined)?.status !== "approved") return { code: "not_approved", message: "approval.status is not approved" };
    return null;
  }

  private result(subject: string, zone: string, outcome: GeofenceResult["outcome"], env: Record<string, unknown>, refusal?: { code: string; message: string }): void {
    const action_id = typeof env.action_id === "string" ? env.action_id : undefined;
    this.results.push({ subject, zone_id: zone, outcome, signed: !!env.sig, action_id, at: Date.now(), ...refusal });
    const s = `evt.map.geofence.${outcome}`;
    const ev = {
      v: 1, id: randomUUID(), ts: new Date().toISOString(), source: "geofence-standin", subject: s,
      ...(typeof env.id === "string" ? { causation: env.id } : {}),
      data: { zone_id: zone, action_id, ...(refusal ?? {}) },
    };
    this.nc.publish(s, enc.encode(JSON.stringify(ev)));
  }

  /** Apply one command envelope as omos #569 does (zone.{verb}.v1 params in `data`). */
  async onCommand(subject: string, env: Record<string, unknown>): Promise<void> {
    const [, , , id, verb] = subject.split(".");
    if (!/^[a-z0-9_][a-z0-9_-]{0,63}$/.test(id ?? "")) return this.result(subject, id ?? "", "refused", env, { code: "schema_invalid", message: "zone id is not one lowercase token" });
    const refusal = this.gate(env);
    if (refusal) return this.result(subject, id, "refused", env, refusal);
    const data = (env.data ?? {}) as Record<string, unknown>;
    const cur = this.zones.get(id);
    if (verb === "remove" || verb === "delete") {
      if (!cur) return this.result(subject, id, "refused", env, { code: "not_found", message: "no such zone" });
      await this.kv.delete(id);
      this.zones.delete(id);
      this.entity.publish({ ...cur, live: false, seq: this.entity.next(id), t: Date.now() });
      return this.result(subject, id, "deleted", env);
    }
    if (verb !== "create" && verb !== "update") return this.result(subject, id, "refused", env, { code: "schema_invalid", message: `unknown verb ${verb}` });
    if (verb === "create" && cur) return this.result(subject, id, "refused", env, { code: "conflict", message: "zone exists" });
    if (verb === "update" && !cur) return this.result(subject, id, "refused", env, { code: "not_found", message: "no such zone" });
    const geo = (data.geo ?? cur?.geo) as { shape?: string; kind?: string; coords?: { coordinates?: unknown } } | undefined;
    if (!geo?.shape || !geo.kind || !geo.coords) return this.result(subject, id, "refused", env, { code: "schema_invalid", message: "geo is required" });
    let center: LonLat;
    if (geo.shape === "circle") center = geo.coords.coordinates as LonLat;
    else {
      const ring = ((geo.coords.coordinates as LonLat[][])[0] ?? []).slice(0, -1);
      center = [ring.reduce((t, p) => t + p[0], 0) / ring.length, ring.reduce((t, p) => t + p[1], 0) / ring.length];
    }
    const name = (data.name as string | undefined) ?? (cur?.name as string | undefined) ?? id;
    const approval = env.approval as { approver?: string } | undefined;
    await this.write(id, {
      ...(cur ?? {}), v: 1, name, callsign: name, active: data.active ?? cur?.active ?? true, geo, lat: center[1], lon: center[0],
      watch: data.watch ?? cur?.watch ?? true,
      properties: { applies_to: (data.applies_to as string[] | undefined) ?? (cur?.properties as { applies_to?: string[] } | undefined)?.applies_to ?? [] },
      updated_by: approval?.approver ?? null, action_id: env.action_id, command_id: env.id, simulated: true,
    });
    return this.result(subject, id, verb === "create" ? "created" : "updated", env);
  }

  contains(z: Record<string, unknown>, p: LonLat): boolean {
    const g = z.geo as { shape: string; coords: { coordinates: unknown }; radius_m?: number };
    if (g.shape === "circle") return distanceM(g.coords.coordinates as LonLat, p) <= (g.radius_m ?? 0);
    const ring = (g.coords.coordinates as LonLat[][])[0] ?? [];
    let inside = false;
    for (let i = 0, j = ring.length - 1; i < ring.length; j = i++) {
      const [xi, yi] = ring[i];
      const [xj, yj] = ring[j];
      if (yi > p[1] !== yj > p[1] && p[0] < ((xj - xi) * (p[1] - yi)) / (yj - yi) + xi) inside = !inside;
    }
    return inside;
  }

  /** Raise or resolve the alert of one zone and sim-x1, as #569 does (D1-P3 subjects). */
  private alertFor(id: string, z: Record<string, unknown> | undefined, violating: boolean, why?: string): void {
    const alertId = `alr-gf-${id}-sim-x1`;
    const cur = this.alerts.get(alertId);
    if (!!cur === violating) return;
    const kind = ((z?.geo as { kind?: string } | undefined)?.kind ?? (cur?.zone_kind as string | undefined)) ?? "aoi";
    const rule = ALERT_RULE[kind];
    if (!rule) return;
    const seq = (this.alertSeq.get(alertId) ?? 0) + 1;
    this.alertSeq.set(alertId, seq);
    const now = Date.now();
    const d = this.entity.drone;
    const name = (z?.name as string | undefined) ?? id;
    const data: Record<string, unknown> = violating
      ? {
          v: 1, alert_id: alertId, alert_kind: "geofence", level: rule.level, state: "open", about: { entity_type: "drone", entity_id: "sim-x1" },
          zone_id: id, zone_kind: kind, reason: rule.reason, raised_by: "geofence-standin", opened_t: now, updated_t: now, seq, simulated: true,
          message: rule.when === "outside" ? `SIM-X1 is outside keep-in zone ${name}` : `SIM-X1 is inside ${kind.replace("_", "-")} zone ${name}`,
          ...(d ? { location: { lat: d.lat, lon: d.lon } } : {}),
        }
      : { ...cur, state: "resolved", reason: why ?? "returned", updated_t: now, resolved_t: now, seq };
    if (violating) this.alerts.set(alertId, data);
    else this.alerts.delete(alertId);
    const subject = `evt.so.alert.${violating ? "raised" : "resolved"}`;
    this.alertFacts.push({ subject, data, at: now });
    this.nc.publish(subject, enc.encode(JSON.stringify({ v: 1, id: randomUUID(), ts: new Date(now).toISOString(), source: "geofence-standin", subject, data })));
  }

  start(): NodeJS.Timeout {
    // The open set (#569 svc.geofence.alerts: `{kind?, entity_id?, feed?}` -> `{v, alerts}`).
    this.nc.subscribe("svc.geofence.alerts", {
      callback: (_e, m) => {
        m.respond(enc.encode(JSON.stringify({ v: 1, alerts: [...this.alerts.values()] })));
      },
    });
    this.nc.subscribe("cmd.map.geofence.*.*", {
      callback: (_e, m) => {
        let env: Record<string, unknown> = {};
        try {
          env = JSON.parse(dec.decode(m.data));
        } catch {
          /* refused below as unsigned */
        }
        void this.onCommand(m.subject, env && typeof env === "object" ? env : {});
      },
    });
    // Pinned zones are republished (same seq) so a client that joins later sees them, as a layer
    // snapshot would give it (the core path has no history).
    let n = 0;
    return setInterval(() => {
      if (++n % 12 === 0) for (const z of this.zones.values()) this.entity.publish(z);
      const d = this.entity.drone;
      if (!d) return;
      for (const [id, z] of this.zones) {
        const kind = (z.geo as { kind?: string }).kind;
        if (!["aoi", "keep_in", "keep_out", "hazard", "control_area"].includes(kind ?? "")) continue;
        const key = `${id}|sim-x1`;
        const now = this.contains(z, [d.lon, d.lat]);
        const was = this.inside.get(key);
        if (was !== undefined && was !== now) {
          this.nc.publish(`evt.map.geofence.${id}.${now ? "enter" : "exit"}`, enc.encode(JSON.stringify({ entity_id: "sim-x1", zone_id: id, t: Date.now() })));
        }
        this.inside.set(key, now);
        // Raised also for a zone drawn around an asset that already violates it (#569); seeded
        // AOIs (Hermes' places) stay quiet so the earlier tests' alert lists do not change.
        if (this.seeded.has(id)) continue;
        const rule = ALERT_RULE[kind ?? ""];
        if (rule && z.active !== false) this.alertFor(id, z, rule.when === "inside" ? now : !now);
      }
      for (const a of [...this.alerts.values()]) {
        const zid = a.zone_id as string;
        const z = this.zones.get(zid);
        if (!z || z.active === false) this.alertFor(zid, z, false, z ? "zone_inactive" : "zone_removed");
      }
    }, 250);
  }
}

class FeedStandin {
  timers = new Map<string, NodeJS.Timeout>();
  lastFetch = new Map<string, number>();
  constructor(private nc: NatsConnection) {}
  start(source: string, staleAfterSec: number, everyMs = 1000): void {
    const tick = () => {
      const now = Date.now();
      this.lastFetch.set(source, now);
      this.nc.publish(
        `tel.feed.${source}.status`,
        enc.encode(JSON.stringify({ source, kind: "rest_poll", collector: "omos-ingest", instance: "standin-1", leader: true, fetchedAt: new Date(now).toISOString(), count: 3, published: 0, lastError: null, errorAt: null, staleAfterSec, ageSec: 0, stale: false, at: new Date(now).toISOString() })),
      );
    };
    tick();
    this.timers.set(source, setInterval(tick, everyMs));
  }
  /** Stop the collector entirely: no more heartbeats (FEATURES B20 test). */
  stop(source: string): number {
    clearInterval(this.timers.get(source));
    this.timers.delete(source);
    return this.lastFetch.get(source) ?? 0;
  }
  stopAll(): void {
    for (const s of [...this.timers.keys()]) this.stop(s);
  }
}

/** Scripted Hermes (no model API): answers from the map context, acts through cmd.map and policy. */
class HermesStandin {
  log: { question: string; reply: string; proposals: string[] }[] = [];
  constructor(private nc: NatsConnection) {}
  start(): void {
    this.nc.subscribe("agents.prompt.hermes.zeus.legacy", {
      callback: (_e, m) => void this.handle(m.data, (b) => m.respond(b)),
    });
  }
  private async handle(data: Uint8Array, respond: (b: Uint8Array) => void): Promise<void> {
    const prompt = dec.decode(data);
    const { question, context } = splitPrompt(prompt);
    const zones = context?.zones ?? [];
    const findZone = (name: string) => zones.find((z) => (z.callsign ?? z.entity_id).toLowerCase() === name.trim().toLowerCase().replace(/^the\s+/, ""));
    let reply = "";
    const proposals: string[] = [];
    let mm: RegExpExecArray | null;
    if ((mm = /moving near (.+?)\??$/i.exec(question))) {
      const z = findZone(mm[1]);
      if (!z) reply = `I do not see a zone called ${mm[1]} on your map.`;
      else {
        const moving = (context?.visible ?? []).filter((e) => (e.speed_mps ?? 0) > 0.5 && distanceM(z.center, [e.lon, e.lat]) < 1500);
        for (const e of moving) this.nc.publish(`cmd.map.highlight.${e.entity_id}`, enc.encode(JSON.stringify({ color: "#FF4FD8", ring: true, label: "MOVING" })));
        this.nc.publish(`cmd.map.annotation.hermes-${z.entity_id}.create`, enc.encode(JSON.stringify({ lat: z.center[1], lon: z.center[0], label: `${z.callsign}: ${moving.length} moving`, color: "#FF4FD8" })));
        reply = moving.length
          ? `${moving.length} moving near ${z.callsign}: ${moving.map((e) => `${e.callsign ?? e.entity_id} (${e.entity_type}, ${Math.round(e.speed_mps ?? 0)} m/s, ${Math.round(distanceM(z.center, [e.lon, e.lat]))} m from its centre)`).join("; ")}. I highlighted them.`
          : `Nothing is moving near ${z.callsign}.`;
      }
    } else if ((mm = /(?:send|fly|move)\s+(sim-[\w-]+)\s+to\s+(.+?)\??$/i.exec(question))) {
      const z = findZone(mm[2]);
      if (!z) reply = `I do not see a zone called ${mm[2]}.`;
      else {
        const sel = context?.visible.find((e) => e.entity_id === mm![1]);
        const req = {
          v: 1, request_id: crypto.randomUUID(), action_type: "asset.goto", action_version: 1,
          target: { entity_type: "drone", entity_id: mm[1] },
          params: { lat: Math.round(z.center[1] * 1e7) / 1e7, lon: Math.round(z.center[0] * 1e7) / 1e7, hae_m: Math.round((sel ? 60 : 60) * 10) / 10 },
          requested_by: { kind: "agent", id: "agent:hermes/zeus/legacy", on_behalf_of: "user:zeus" },
          reason: `Asked in the Hermes panel: ${question.slice(0, 200)}`,
          agent_session: "standin-session", model_alias: "scripted-stub",
          prompt_sha256: createHash("sha256").update(prompt).digest("hex"),
        };
        try {
          const r = JSON.parse(dec.decode((await this.nc.request("svc.policy.command.propose", enc.encode(JSON.stringify(req)), { timeout: 4000 })).data));
          if (r.denial) reply = `The policy service refused my proposal: ${r.denial.code}${r.denial.detail ? ` (${r.denial.detail})` : ""}.`;
          else {
            proposals.push(r.action_id);
            reply = `I proposed sending ${mm[1]} to ${z.callsign}. It does nothing until you confirm it (action ${r.action_id}).`;
          }
        } catch (e) {
          reply = `I could not reach the policy service: ${(e as Error).message}`;
        }
      }
    } else if (/follow/i.test(question)) {
      reply = "I will not follow or track a person; that is outside OneMind's civilian stop line.";
    } else {
      reply = "I can say what is moving near a named zone, or propose sending a sim asset to one.";
    }
    this.log.push({ question, reply, proposals });
    respond(enc.encode(JSON.stringify({ text: reply, proposals })));
  }
}

/** svc.cot.send (the projector's endpoint, not built yet) plus a scripted ATAK peer. */
class TakStandin {
  injected: string[] = [];
  timers: NodeJS.Timeout[] = [];
  constructor(private nc: NatsConnection) {}
  sa(uid: string, cs: string, group: string, at: LonLat): string {
    const t = new Date();
    const st = new Date(t.getTime() + 120_000);
    return `<event version="2.0" uid="${uid}" type="a-f-G-U-C" how="m-g" time="${t.toISOString()}" start="${t.toISOString()}" stale="${st.toISOString()}"><point lat="${at[1]}" lon="${at[0]}" hae="150" ce="10" le="10"/><detail><contact callsign="${cs}" endpoint="*:-1:stcp"/><__group name="${group}" role="Team Member"/><takv device="sim" platform="ATAK-CIV" os="sim" version="5.6.0"/></detail></event>`;
  }
  chat(from: string, cs: string, room: string, roomId: string, text: string): string {
    const t = new Date();
    const st = new Date(t.getTime() + 60_000);
    const uid = `GeoChat.${from}.${roomId}.${randomBytes(4).toString("hex")}`;
    return `<event version="2.0" uid="${xmlEscape(uid)}" type="b-t-f" how="h-g-i-g-o" time="${t.toISOString()}" start="${t.toISOString()}" stale="${st.toISOString()}"><point lat="0" lon="0" hae="0" ce="9999999" le="9999999"/><detail><__chat parent="RootContactGroup" groupOwner="false" chatroom="${xmlEscape(room)}" id="${xmlEscape(roomId)}" senderCallsign="${xmlEscape(cs)}"><chatgrp uid0="${from}" uid1="${xmlEscape(roomId)}" id="${xmlEscape(roomId)}"/></__chat><remarks source="BAO.F.ATAK.${from}" to="${xmlEscape(roomId)}" time="${t.toISOString()}">${xmlEscape(text)}</remarks></detail></event>`;
  }
  start(): void {
    const sas = () => {
      this.nc.publish("tak.cot.out", enc.encode(this.sa("ANDROID-1", "VIPER", "Cyan", destination(CENTER, 200, 600))));
      this.nc.publish("tak.cot.out", enc.encode(this.sa("ANDROID-2", "HAWK", "Red", destination(CENTER, 300, 900))));
    };
    sas();
    this.timers.push(setInterval(sas, 3000));
    this.nc.subscribe("svc.cot.send", {
      callback: (_e, m) => {
        let b: { v?: number; kind?: string; room?: { id: string; name: string }; text?: string; sender?: { uid: string; callsign: string }; message_uid?: string };
        try {
          b = JSON.parse(dec.decode(m.data));
        } catch {
          m.respond(enc.encode(JSON.stringify({ ok: false, error: { code: "schema_invalid" } })));
          return;
        }
        if (b.v !== 1 || b.kind !== "geochat" || !b.room || !b.text || !b.sender) {
          m.respond(enc.encode(JSON.stringify({ ok: false, error: { code: "schema_invalid" } })));
          return;
        }
        // The projector builds the CoT itself (no client-written XML) and is the only publisher
        // of tak.cot.inject (security-and-trust.md T4).
        const xml = this.chat(b.sender.uid, b.sender.callsign, b.room.name, b.room.id, b.text);
        this.injected.push(xml);
        this.nc.publish("tak.cot.inject", enc.encode(xml));
        m.respond(enc.encode(JSON.stringify({ ok: true, uid: b.message_uid })));
        // The scripted ATAK user answers chats sent to All Chat Rooms or to VIPER directly.
        if (b.room.id === "All Chat Rooms" || b.room.id === "ANDROID-1") {
          const roomId = b.room.id === "ANDROID-1" ? b.sender.uid : "All Chat Rooms";
          const roomName = b.room.id === "ANDROID-1" ? b.sender.callsign : "All Chat Rooms";
          setTimeout(() => this.nc.publish("tak.cot.out", enc.encode(this.chat("ANDROID-1", "VIPER", roomName, roomId, `copy: ${b.text}`))), 400);
        }
      },
    });
  }
  /** The scripted ATAK users speak (GeoChat on tak.cot.out, as TAK Server forwards it). */
  say(): void {
    this.nc.publish("tak.cot.out", enc.encode(this.chat("ANDROID-1", "VIPER", "All Chat Rooms", "All Chat Rooms", "VIPER radio check")));
    this.nc.publish("tak.cot.out", enc.encode(this.chat("ANDROID-2", "HAWK", "Red", "Red", "Red team only: holding at the gate")));
  }

  stop(): void {
    for (const t of this.timers) clearInterval(t);
  }
}

// ---- the stack ----------------------------------------------------------------------------------

export interface ActStack {
  wsUrl: string;
  url: string;
  users: Record<"zeus" | "alice" | "newbie" | "carol" | "dana" | "rookie", { user: string; pass: string }>;
  admin: NatsConnection;
  feeds: FeedStandin;
  hermes: HermesStandin;
  tak: TakStandin;
  entity: EntityStandin;
  geofence: GeofenceStandin;
  audit: { subject: string; data: Record<string, unknown>; at: number }[];
  commands: { subject: string; data: Record<string, unknown>; at: number }[];
  /** every `cmd.map.geofence.*` seen (from the policy service, or anyone else) */
  mapCommands: { subject: string; data: Record<string, unknown>; at: number }[];
  kvGet(key: string): Promise<Record<string, unknown> | null>;
  log(): string;
  stop(): Promise<void>;
}

export async function startActStack(): Promise<ActStack> {
  const av = buildReal();
  if (av.reason) throw new Error(`real stack unavailable: ${av.reason}`);
  mkdirSync(TMP_ROOT, { recursive: true });
  const dir = mkdtempSync(join(TMP_ROOT, "act-"));
  const procs: ChildProcess[] = [];
  let logs = "";
  const port = await freePort();
  const wsPort = await freePort();
  const p: Record<string, string> = Object.fromEntries(["admin", "policy", "bridge", "zeus", "alice", "newbie", "carol", "dana", "rookie", "hermes"].map((u) => [u, pw()]));
  writeFileSync(join(dir, "nats.conf"), natsConf(dir, port, wsPort, p), { mode: 0o600 });
  const spawnLogged = (name: string, cmd: string, args: string[], env: Record<string, string> = {}) => {
    const c = spawn(cmd, args, { env: { ...process.env, ...env }, stdio: ["ignore", "pipe", "pipe"], cwd: dir });
    c.stdout?.on("data", (d) => (logs += `[${name}] ${d}`));
    c.stderr?.on("data", (d) => (logs += `[${name}] ${d}`));
    procs.push(c);
    return c;
  };
  spawnLogged("nats", bin("nats-server"), ["-c", join(dir, "nats.conf")]);
  await waitTcp(port);
  await waitTcp(wsPort);
  const wsUrl = `ws://127.0.0.1:${wsPort}`;
  const url = `nats://127.0.0.1:${port}`;
  const admin = await wsconnect({ servers: wsUrl, user: "admin", pass: p.admin, name: "act-stack-admin" });
  const hermesNc = await wsconnect({ servers: wsUrl, user: "hermes", pass: p.hermes, name: "act-stack-hermes" });

  // JetStream: the streams the policy service needs (omos cmd.yaml, evt.yaml, mission.yaml) and KV geofences.
  const jsapi = async (subj: string, body: unknown) => {
    const r = JSON.parse(dec.decode((await admin.request(`$JS.API.${subj}`, enc.encode(JSON.stringify(body)), { timeout: 5000 })).data));
    if (r.error && r.error.err_code !== 10058) throw new Error(`${subj}: ${JSON.stringify(r.error)}`);
    return r;
  };
  await jsapi("STREAM.CREATE.CMD", { name: "CMD", subjects: ["cmd.>"], storage: "file", discard: "new", duplicate_window: 120e9, num_replicas: 1 });
  await jsapi("STREAM.CREATE.EVT", { name: "EVT", subjects: ["evt.so.>", "evt.drone.>", "evt.robot.>"], storage: "file", duplicate_window: 120e9, num_replicas: 1 });
  await jsapi("STREAM.CREATE.MISSION", { name: "MISSION", subjects: ["evt.mission.>", "evt.map.>"], storage: "file", num_replicas: 1 });
  await jsapi("STREAM.CREATE.KV_geofences", { name: "KV_geofences", subjects: ["$KV.geofences.>"], storage: "file", max_msgs_per_subject: 10, allow_rollup_hdrs: true, deny_delete: true, allow_direct: true, discard: "new", num_replicas: 1 });
  const kv = {
    put: (k: string, v: Uint8Array) => admin.request(`$KV.geofences.${k}`, v, { timeout: 5000 }),
    delete: (k: string) => {
      const h = headers();
      h.set("KV-Operation", "DEL");
      return admin.request(`$KV.geofences.${k}`, new Uint8Array(), { timeout: 5000, headers: h });
    },
  };

  // Audit and command taps (the D3 acceptance: proposal, approval and outcome on evt.so.action.*).
  const audit: ActStack["audit"] = [];
  const commands: ActStack["commands"] = [];
  const tap = (subj: string, into: ActStack["audit"]) =>
    admin.subscribe(subj, {
      callback: (_e, m) => {
        try {
          into.push({ subject: m.subject, data: JSON.parse(dec.decode(m.data)), at: Date.now() });
        } catch {
          /* ignore */
        }
      },
    });
  tap("evt.so.action.>", audit);
  tap("cmd.fleet.>", commands);
  const mapCommands: ActStack["commands"] = [];
  tap("cmd.map.geofence.>", mapCommands);
  // The policy signing key comes first: the geofence stand-in trusts only its public half.
  const { privateKey, publicKey } = generateKeyPairSync("ed25519");

  // Stand-ins first: the policy service reads svc.entity.get and KV geofences at start.
  const entity = new EntityStandin(admin);
  entity.start();
  const geofence = new GeofenceStandin(admin, kv, entity, new Map([["act-run", publicKey]]));
  const gTimer = geofence.start();
  const feeds = new FeedStandin(admin);
  feeds.start("nws-alerts", 6);
  feeds.start("usgs", 30);
  const hermes = new HermesStandin(hermesNc);
  hermes.start();
  const tak = new TakStandin(admin);
  tak.start();
  // Hermes' places: two AOIs (pinned zones from KV geofences).
  for (const [id, name, c] of [["zone-north-field", "NORTH FIELD", NORTH_FIELD], ["zone-home-field", "HOME FIELD", HOME_FIELD]] as const) {
    await geofence.seed(id, { v: 1, name, callsign: name, active: true, lat: c[1], lon: c[0], geo: { shape: "circle", kind: "aoi", radius_m: 250, coords: { type: "Point", coordinates: [c[0], c[1]] } }, watch: true, properties: { applies_to: [] } });
  }
  await admin.flush();

  // Real policy service (omos #554, with D1-P4), on a schema dir that adds #569's zone schemas.
  writeFileSync(join(dir, "policy-signing.pem"), privateKey.export({ type: "pkcs8", format: "pem" }), { mode: 0o600 });
  const pubRaw = (publicKey.export({ type: "spki", format: "der" }) as Buffer).subarray(-32).toString("base64");
  writeFileSync(join(dir, "tuples.json"), JSON.stringify(tuples()));
  const schemaDir = join(dir, "schemas");
  cpSync(join(POLICY_DIR, "services/schemas"), schemaDir, { recursive: true });
  for (const f of readdirSync(ZONE_SCHEMAS)) cpSync(join(ZONE_SCHEMAS, f), join(schemaDir, "actions", f));
  writeFileSync(
    join(dir, "principals.json"),
    JSON.stringify({
      own_account: "ONEMIND",
      agent_accounts: ["AGENTS"],
      principals: [
        { account: "CLIENT", user: "zeus", kind: "user", id: "user:zeus" },
        { account: "CLIENT", user: "alice", kind: "user", id: "user:alice" },
        { account: "CLIENT", user: "newbie", kind: "user", id: "user:newbie" },
        { account: "CLIENT", user: "carol", kind: "user", id: "user:carol" },
        { account: "AGENTS", user: "hermes", kind: "agent", id: "agent:hermes/zeus/legacy" },
      ],
    }),
  );
  const healthPort = await freePort();
  spawnLogged("policy", av.policy!, [], {
    NATS_URL: url, NATS_USER: "policy", NATS_PASSWORD: p.policy, POLICY_AUTHZ: "local",
    POLICY_MODEL_FILE: join(POLICY_DIR, "platform/security/openfga/onemind-actions.json"), POLICY_TUPLES_FILE: join(dir, "tuples.json"),
    POLICY_SCHEMA_DIR: schemaDir, POLICY_PRINCIPALS_FILE: join(dir, "principals.json"),
    POLICY_SIGNING_KEY_FILE: join(dir, "policy-signing.pem"), POLICY_SIGNING_KID: "act-run", HEALTH_ADDR: `127.0.0.1:${healthPort}`,
  });
  await waitTcp(healthPort);

  // Scripted drone and the real MAVLink bridge (omos #550), signatures required.
  const udp = await freePort(true);
  spawnLogged("drone", PY, ["-I", SIM, "--out", `udpout:127.0.0.1:${udp}`, "--lat", String(CENTER[1]), "--lon", String(CENTER[0]), "--radius", "120", "--alt", "60", "--speed", "30"], { MAVLINK20: "1" });
  spawnLogged("bridge", av.bridge!, [], {
    NATS_URL: url, NATS_USER: "bridge", NATS_PASSWORD: p.bridge, ASSET_ID: "sim-x1", CALLSIGN: "SIM-X1",
    MAVLINK_ENDPOINT: `udpserver:127.0.0.1:${udp}`, MAVLINK_OUT_VERSION: "2", POLICY_PUBKEYS: `act-run=${pubRaw}`, SIG_REQUIRED: "true",
    GEOFENCE_JSON: JSON.stringify({ keep_in: [{ id: "sim-range", circle: { lat: CENTER[1], lon: CENTER[0], radius_m: 5000 } }], max_hae_m: 3000 }),
  });

  const stack: ActStack = {
    wsUrl, url,
    users: { zeus: { user: "zeus", pass: p.zeus }, alice: { user: "alice", pass: p.alice }, newbie: { user: "newbie", pass: p.newbie }, carol: { user: "carol", pass: p.carol }, dana: { user: "dana", pass: p.dana }, rookie: { user: "rookie", pass: p.rookie } },
    admin, feeds, hermes, tak, entity, geofence, audit, commands, mapCommands,
    async kvGet(key) {
      const r = await admin.request(`$JS.API.DIRECT.GET.KV_geofences`, enc.encode(JSON.stringify({ last_by_subj: `$KV.geofences.${key}` })), { timeout: 3000 });
      if (r.headers?.get("Status") === "404" || !r.data.length) return null;
      return JSON.parse(dec.decode(r.data));
    },
    log: () => logs,
    async stop() {
      feeds.stopAll();
      entity.stop();
      tak.stop();
      clearInterval(gTimer);
      await hermesNc.close().catch(() => undefined);
      await admin.close().catch(() => undefined);
      for (const c of procs.reverse()) await kill(c);
      rmSync(dir, { recursive: true, force: true });
    },
  };
  // Wait until the bridge reports the drone online (its HEARTBEAT seen).
  const t0 = Date.now();
  while (entity.drone?.connection !== "online" && Date.now() - t0 < 20_000) await new Promise((r) => setTimeout(r, 200));
  if (entity.drone?.connection !== "online") {
    const l = logs;
    await stack.stop();
    throw new Error(`sim-x1 never reported through the bridge:\n${l.slice(-4000)}`);
  }
  return stack;
}

/**
 * Arm and take off through the policy service with two different humans (A4, asset-command.md §3),
 * exactly as an operator pair would: zeus proposes and confirms step 1, alice confirms step 2.
 */
export async function armAndTakeoff(s: ActStack): Promise<string[]> {
  const z = await wsconnect({ servers: s.wsUrl, user: s.users.zeus.user, pass: s.users.zeus.pass, name: "act-op-zeus" });
  const a = await wsconnect({ servers: s.wsUrl, user: s.users.alice.user, pass: s.users.alice.pass, name: "act-op-alice" });
  const req = async (nc: NatsConnection, subj: string, body: unknown) => JSON.parse(dec.decode((await nc.request(subj, enc.encode(JSON.stringify(body)), { timeout: 5000 })).data));
  const trail: string[] = [];
  try {
    for (const [type, params] of [["asset.arm", {}], ["asset.takeoff", { hae_m: 60 }]] as const) {
      const pr = await req(z, "svc.policy.command.propose", { v: 1, request_id: crypto.randomUUID(), action_type: type, action_version: 1, target: { entity_type: "drone", entity_id: "sim-x1" }, params, requested_by: { kind: "user", id: "user:zeus" }, reason: "test setup: the scripted drone takes goto only when airborne" });
      if (pr.denial) throw new Error(`${type} propose denied: ${JSON.stringify(pr.denial)}`);
      const c1 = await req(z, "svc.policy.command.confirm", { v: 1, action_id: pr.action_id, params_hash: pr.params_hash, decision: "approve" });
      const c2 = await req(a, "svc.policy.command.confirm", { v: 1, action_id: pr.action_id, params_hash: pr.params_hash, decision: "approve" });
      if (c2.denial) throw new Error(`${type} second confirm denied: ${JSON.stringify(c2.denial)}`);
      trail.push(`${type} ${pr.action_id}: ${pr.status} → ${c1.status} (zeus) → ${c2.status} (alice)`);
      const t0 = Date.now();
      while (Date.now() - t0 < 15_000) {
        const g = await req(z, "svc.policy.action.get", { v: 1, action_id: pr.action_id });
        if (["executed", "failed", "refused"].includes(g.action?.status)) {
          trail.push(`${type}: ${g.action.status}${g.action.denial ? ` ${JSON.stringify(g.action.denial)}` : ""}`);
          if (g.action.status !== "executed") throw new Error(`${type} ${g.action.status}: ${JSON.stringify(g.action.denial)}`);
          break;
        }
        await new Promise((r) => setTimeout(r, 300));
      }
    }
  } finally {
    await z.close();
    await a.close();
  }
  return trail;
}
