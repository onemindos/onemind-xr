// F-ALERT against the real omos #562 permissions (opt-in: set OMOS_IDENTITIES_DIR to a checkout
// of feat/nats-identities). Renders the A19 accounts with `scripts/nats_identities.py conf
// --stage B --test-users`, adds one CLIENT user with the J2 newcomer role's lists (#565
// onemind-roles.json plus the A19-P11 proposed additions, as permtest TestClientRoles does), and
// starts a disposable nats-server. The `geofence` user (nats CLI, STANDARD connection) serves
// svc.geofence.alerts and publishes evt.so.alert.raised; AlertFeed runs over WebSocket with the
// browser's nats.js as `zeus` and as the newcomer.
import { spawn, spawnSync, type ChildProcess } from "node:child_process";
import { existsSync, mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { createServer } from "node:net";
import { join } from "node:path";
import { wsconnect, type NatsConnection } from "@nats-io/nats-core";
import { afterAll, beforeAll, describe, expect, it } from "vitest";
import { AlertFeed } from "../../src/bus/alertfeed";

const DIR = process.env.OMOS_IDENTITIES_DIR ?? "";
const TOOLS = "/Users/danielcarku/Desktop/work/.tools";
const NATS_BIN = process.env.NATS_SERVER_BIN ?? `${TOOLS}/bin/nats-server`;
const NATS_CLI = process.env.NATS_CLI ?? `${TOOLS}/bin/nats`;
const PY = process.env.PYTHON ?? `${TOOLS}/py/bin/python`;
const TMP = process.env.F_ALERT_TMP ?? "/Users/danielcarku/Desktop/work/.run/tmp-f-alert";
const ready = !!DIR && [NATS_BIN, NATS_CLI, PY, join(DIR, "scripts/nats_identities.py")].every(existsSync);

const wait = (ms: number) => new Promise((r) => setTimeout(r, ms));
async function until(fn: () => boolean, ms: number, label: string) {
  const t0 = Date.now();
  while (!fn()) {
    if (Date.now() - t0 > ms) throw new Error(`timeout: ${label}`);
    await wait(50);
  }
}
const freePort = () =>
  new Promise<number>((res) => {
    const s = createServer();
    s.listen(0, "127.0.0.1", () => {
      const p = (s.address() as { port: number }).port;
      s.close(() => res(p));
    });
  });

// The alert of onemind-xr docs/evidence/INT/E2E-2/run1/results.json step 4 (real #569 output).
const ALERT = {
  about: { entity_id: "sim-x1", entity_type: "drone" }, alert_id: "alr-gf-zone-e2e-pad-sim-x1", alert_kind: "geofence", level: "warning",
  message: "SIM-X1 is outside keep-in zone zone-e2e-pad", opened_t: 1793293303011, raised_by: "omos-geofence", reason: "left_keep_in",
  seq: 1, simulated: true, state: "open", updated_t: 1793293303011, v: 1, zone_id: "zone-e2e-pad", zone_kind: "keep_in",
};
const NEWCOMER = {
  publish: ["svc.layer.list", "svc.layer.meta", "svc.layer.snapshot", "svc.layer.watch", "svc.layer.unwatch", "svc.entity.get", "svc.entity.search"],
  subscribe: ["_INBOX.>", "tel.feed.*.status"],
};

describe.skipIf(!ready)("F-ALERT on the omos #562 render", () => {
  const procs: ChildProcess[] = [];
  let dir = "";
  let wsUrl = "";
  let natsUrl = "";
  let pw: Record<string, string> = {};
  const conns: NatsConnection[] = [];
  let log = "";

  beforeAll(async () => {
    mkdirSync(TMP, { recursive: true });
    dir = mkdtempSync(join(TMP, "a19-"));
    const py = (args: string[]) => {
      const r = spawnSync(PY, [join(DIR, "scripts/nats_identities.py"), ...args], { cwd: DIR, encoding: "utf8" });
      if (r.status !== 0) throw new Error(`nats_identities.py ${args[0]}: ${r.stderr}`);
    };
    py(["gen-passwords", "--out", join(dir, "pw.json")]);
    py(["conf", "--passwords", join(dir, "pw.json"), "--stage", "B", "--test-users", "--out", join(dir, "accounts.json")]);
    pw = JSON.parse(readFileSync(join(dir, "pw.json"), "utf8"));
    pw["role-newcomer"] = Math.random().toString(36).slice(2) + Math.random().toString(36).slice(2);
    const acc = JSON.parse(readFileSync(join(dir, "accounts.json"), "utf8"));
    acc.accounts.CLIENT.users.push({
      user: "role-newcomer", password: pw["role-newcomer"], allowed_connection_types: ["STANDARD", "WEBSOCKET"],
      permissions: { publish: { allow: NEWCOMER.publish }, subscribe: { allow: NEWCOMER.subscribe } },
    });
    const port = await freePort();
    const ws = await freePort();
    const conf = { listen: `127.0.0.1:${port}`, websocket: { listen: `127.0.0.1:${ws}`, no_tls: true }, jetstream: { store_dir: join(dir, "js"), max_mem: 67108864, max_file: 268435456 }, ...acc };
    writeFileSync(join(dir, "nats.conf"), JSON.stringify(conf, null, 1), { mode: 0o600 });
    const srv = spawn(NATS_BIN, ["-c", join(dir, "nats.conf")], { stdio: ["ignore", "pipe", "pipe"] });
    srv.stderr?.on("data", (d) => (log += d));
    srv.stdout?.on("data", (d) => (log += d));
    procs.push(srv);
    natsUrl = `nats://127.0.0.1:${port}`;
    wsUrl = `ws://127.0.0.1:${ws}`;
    await until(() => log.includes("Server is ready"), 10_000, "nats-server ready");
    // The geofence service's RPC, as the real `geofence` user.
    const rep = spawn(NATS_CLI, ["--server", natsUrl, "--user", "geofence", "--password", pw.geofence, "reply", "svc.geofence.alerts", JSON.stringify({ v: 1, alerts: [ALERT] })], { stdio: ["ignore", "pipe", "pipe"] });
    rep.stderr?.on("data", (d) => (log += `[reply] ${d}`));
    procs.push(rep);
    await wait(500);
  }, 30_000);

  afterAll(async () => {
    for (const c of conns) await c.close().catch(() => undefined);
    for (const p of procs) p.kill();
    await wait(200);
    if (process.env.F_ALERT_LOG) writeFileSync(process.env.F_ALERT_LOG, log);
    if (dir) rmSync(dir, { recursive: true, force: true });
  });

  const client = async (user: string) => {
    const nc = await wsconnect({ servers: wsUrl, user, pass: pw[user], name: `f-alert-${user}` });
    conns.push(nc);
    return nc;
  };
  const geofencePub = (subject: string, data: unknown) => {
    const r = spawnSync(NATS_CLI, ["--server", natsUrl, "--user", "geofence", "--password", pw.geofence, "pub", subject, JSON.stringify({ v: 1, id: "f", source: "omos-geofence", subject, data })], { encoding: "utf8" });
    if (r.status !== 0) throw new Error(`nats pub: ${r.stderr}`);
  };

  it("zeus (CLIENT human): open set from svc.geofence.alerts, then live raised/resolved", async () => {
    const nc = await client("zeus");
    const feed = new AlertFeed(nc, { refreshMs: 600_000 });
    feed.start();
    await until(() => feed.status.snapshot === "ok", 10_000, "snapshot");
    expect(feed.alerts().map((a) => a.alert_id)).toEqual([ALERT.alert_id]);
    // The live subscription stands before any fact arrives (a clean flush round trip).
    await until(() => feed.status.live === "ok", 5000, "live subscription accepted");
    const b = { ...ALERT, alert_id: "alr-gf-zone-b-sim-x1", zone_id: "zone-b", seq: 1 };
    geofencePub("evt.so.alert.raised", b);
    await until(() => feed.alerts().length === 2, 10_000, "live raised");
    expect(feed.status.live).toBe("ok");
    geofencePub("evt.so.alert.resolved", { ...b, seq: 2, state: "resolved", reason: "returned" });
    await until(() => feed.alerts().length === 1, 10_000, "live resolved");
    feed.stop();
    expect(log).not.toMatch(/f-alert-zeus.*Violation/);
  });

  it("zeus may not subscribe ent.alert.*.state (why the old alerts layer stayed empty)", async () => {
    const nc = await client("zeus");
    const err = await new Promise<Error | null>((res) => {
      nc.subscribe("ent.alert.*.state", { callback: (e) => res(e) });
      setTimeout(() => res(null), 3000);
    });
    expect(String(err?.message)).toMatch(/permission/i);
  });

  it("J2 newcomer role: both alert paths refused, nothing shown, no polling storm", async () => {
    const nc = await client("role-newcomer");
    const feed = new AlertFeed(nc, { pollMs: 500, refreshMs: 500, deniedRetryMs: 600_000 });
    feed.start();
    await until(() => feed.status.snapshot !== "pending" && feed.status.live !== "pending", 10_000, "newcomer status");
    geofencePub("evt.so.alert.raised", { ...ALERT, alert_id: "alr-gf-zone-c-sim-x1", seq: 1 });
    await wait(2000);
    expect(feed.status).toMatchObject({ snapshot: "denied", live: "denied" });
    expect(feed.alerts()).toHaveLength(0);
    const pubViolations = log.split("\n").filter((l) => l.includes("f-alert-role-newcomer") && l.includes('Publish Violation - Subject "svc.geofence.alerts"'));
    expect(pubViolations.length).toBe(1);
    feed.stop();
  });
});
