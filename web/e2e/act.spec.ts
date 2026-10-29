// End-to-end for the M4 Act features (D1, D3, D6, B20, D4, C8, B11, J2) in headless Chromium,
// against the real policy service (omos #554) and MAVLink bridge (omos #550) driving the
// scripted drone sim-x1, plus test-fixture stand-ins (test/standins/stack.ts says which and why).
// Map clicks are real mouse clicks at the screen position of a lon/lat.
import { mkdirSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { expect, test, type Browser, type Page } from "@playwright/test";
import { wsconnect } from "@nats-io/nats-core";
import { parseCot } from "../src/act/cot";
import { destination, distanceM, type LonLat } from "../src/act/geo";
import { armAndTakeoff, buildReal, CENTER, NONSIM_ID, NORTH_FIELD, startActStack, TRACK_ID, type ActStack } from "../test/standins/stack";

const SHOTS = process.env.L0_EVIDENCE_DIR ?? "test-results/screens";
const TRANSCRIPT = process.env.ACT_TRANSCRIPT ?? join(dirname(SHOTS), "act-transcript.json");
mkdirSync(SHOTS, { recursive: true });
const shot = (page: Page, name: string) => page.screenshot({ path: join(SHOTS, name) });

type L0 = {
  act: {
    shared: Map<string, { zoneId: string; kind: string }>;
    drawings: Map<string, { id: string; kind: string; label: string }>;
    overlay: { highlighted: string[]; annotationIds: string[] };
    cotFor(id: string): string;
    lassoed: string[];
    proposeSimple(assetId: string, actionType: string): Promise<unknown>;
  };
  store: { get(id: string): { r: { lat: number; lon: number } } | undefined; apply(raw: unknown, layer: string): string };
  toScreen(lon: number, lat: number): { x: number; y: number } | null;
  flyTo(lon: number, lat: number, height: number): void;
};

async function open(browser: Browser, s: ActStack, user: "zeus" | "alice" | "newbie" | "carol", viewport = { width: 1440, height: 900 }) {
  const page = await browser.newPage({ viewport });
  const errors: string[] = [];
  page.on("pageerror", (e) => errors.push(e.message));
  // Credentials go only into this tab's sessionStorage, as the Connection form would put them.
  await page.addInitScript((c) => sessionStorage.setItem("l0.creds", JSON.stringify(c)), s.users[user]);
  const q = new URLSearchParams({ nats: s.wsUrl, lon: String(CENTER[0] + 0.004), lat: String(CENTER[1] + 0.002), height: "6500" });
  await page.goto(`./?${q}`);
  await expect(page.getByTestId("conn")).toHaveAttribute("data-state", "connected");
  return { page, errors };
}

async function clickMap(page: Page, p: LonLat) {
  const pos = await page.evaluate(([lon, lat]) => (window as unknown as { __l0: L0 }).__l0.toScreen(lon, lat), p);
  expect(pos, `screen position of ${p}`).not.toBeNull();
  await page.mouse.click(pos!.x, pos!.y);
  await page.waitForTimeout(120);
}

const l0 = <T>(page: Page, fn: (l: L0, arg: unknown) => T, arg?: unknown) =>
  page.evaluate(([f, a]) => new Function("l", "a", `return (${f})(l, a)`)((window as unknown as { __l0: L0 }).__l0, a), [fn.toString(), arg] as const) as Promise<Awaited<T>>;

const dronePos = async (page: Page): Promise<LonLat> => {
  const r = await l0(page, (l) => { const e = l.store.get("sim-x1"); return e ? [e.r.lon, e.r.lat] : null; });
  return r as LonLat;
};

const av = buildReal();

test.describe.serial("Act features on the real policy service and MAVLink bridge", () => {
  test.skip(!!av.reason, `real stack not available here (${av.reason})`);
  let s: ActStack;
  let page: Page;
  let errors: string[];
  const notes: Record<string, unknown> = {};

  test.beforeAll(async ({ browser }) => {
    test.setTimeout(240_000);
    s = await startActStack();
    notes.setup = await armAndTakeoff(s);
    ({ page, errors } = await open(browser, s, "zeus"));
    await page.getByTestId("role").selectOption("operator");
  });

  test.afterAll(async () => {
    const summary = {
      ...notes,
      audit: s?.audit.map((a) => ({ subject: a.subject, action_id: (a.data.data as Record<string, unknown>)?.action_id, action_type: (a.data.data as Record<string, unknown>)?.action_type, requested_by: (a.data.data as Record<string, unknown>)?.requested_by, approvals: (a.data.data as Record<string, unknown>)?.approvals, code: (a.data.data as Record<string, unknown>)?.code, detail: (a.data.data as Record<string, unknown>)?.detail, at: new Date(a.at).toISOString() })),
      commands: s?.commands.map((c) => ({ subject: c.subject, action_id: c.data.action_id, approver: (c.data.approval as Record<string, unknown>)?.approver, signed: !!c.data.sig, simulated: c.data.simulated, data: c.data.data, at: new Date(c.at).toISOString() })),
      map_commands: s?.mapCommands.map((c) => ({ subject: c.subject, action_id: c.data.action_id, approval: (c.data.approval as Record<string, unknown>)?.status, approver: (c.data.approval as Record<string, unknown>)?.approver, signed: !!c.data.sig, data: c.data.data, at: new Date(c.at).toISOString() })),
      geofence_results: s?.geofence.results.map((r) => ({ ...r, at: new Date(r.at).toISOString() })),
      hermes: s?.hermes.log,
      tak_injected: s?.tak.injected,
      page_errors: errors,
    };
    mkdirSync(dirname(TRANSCRIPT), { recursive: true });
    writeFileSync(TRANSCRIPT, JSON.stringify(summary, null, 2));
    await page?.close();
    await s?.stop();
  });

  test("D1: an AOI goes through policy and a human confirm, then the geofence writer applies the signed command", async () => {
    await page.getByTestId("tab-draw").click();
    await page.getByTestId("draw-label").fill("PATROL AOI");
    await page.getByTestId("tool-rectangle").click();
    await clickMap(page, destination(destination(CENTER, 180, 450), 270, 450));
    await clickMap(page, destination(destination(CENTER, 0, 450), 90, 450));
    const drawing = page.locator('[data-testid^="drawing-d"][data-kind="rectangle"]').first();
    await expect(drawing).toBeVisible();
    await page.getByTestId("zone-kind").selectOption("aoi");
    await page.getByTestId("share-zone").click();
    // zone.create is A2: the policy service holds it and the confirm dialog shows what it binds.
    const dlg = page.getByTestId("confirm-dialog");
    await expect(dlg).toBeVisible();
    await expect(page.getByTestId("confirm-action")).toContainText("zone.create (level A2)");
    await expect(page.getByTestId("confirm-parameters")).toContainText('"kind":"aoi"');
    await expect(page.getByTestId("confirm-hash-check")).toHaveAttribute("data-state", "verified");
    await expect(page.getByTestId("confirm-safety")).toHaveCount(0); // an AOI constrains nothing
    const zidPending = (await page.getByTestId("confirm-target").textContent())!.replace("zone ", "");
    expect(zidPending).toMatch(/^zone-d[a-z0-9]+$/);
    // Nothing is written before the human confirms.
    expect(await s.kvGet(zidPending)).toBeNull();
    expect(s.mapCommands).toHaveLength(0);
    await shot(page, "d1-zone-confirm.png");
    await page.getByTestId("confirm-approve").click();
    await expect(dlg).toBeHidden();
    await expect.poll(() => l0(page, (l) => [...l.act.shared.values()][0]?.zoneId), { timeout: 10_000 }).toBe(zidPending);
    const zid = zidPending;
    await expect.poll(async () => ((await s.kvGet(zid))?.geo as { kind?: string } | undefined)?.kind, { timeout: 10_000 }).toBe("aoi");
    const kv = await s.kvGet(zid);
    expect((kv!.geo as { shape: string }).shape).toBe("polygon");
    // The write came from the policy service: signed, approved by zeus, and the action executed.
    const cmd = s.mapCommands.find((c) => c.subject === `cmd.map.geofence.${zid}.create`)!;
    expect(cmd.data.sig).toBeTruthy();
    expect(cmd.data.approval).toMatchObject({ status: "approved", approver: "user:zeus", level: "A2" });
    const actionId = cmd.data.action_id as string;
    await expect.poll(() => s.audit.filter((a) => (a.data.data as Record<string, unknown>).action_id === actionId).map((a) => a.subject.split(".")[3]), { timeout: 10_000 }).toContain("executed");
    expect(s.geofence.results.find((r) => r.zone_id === zid)).toMatchObject({ outcome: "created", signed: true, action_id: actionId });

    // Anyone who may publish cmd.map.> (here a CLIENT user) cannot write a zone without policy.
    const rogue = await wsconnect({ servers: s.wsUrl, user: s.users.zeus.user, pass: s.users.zeus.pass, name: "act-rogue" });
    rogue.publish("cmd.map.geofence.zone-rogue.create", new TextEncoder().encode(JSON.stringify({ v: 1, data: { name: "ROGUE", geo: { shape: "circle", kind: "aoi", coords: { type: "Point", coordinates: CENTER }, radius_m: 9000 } } })));
    await rogue.flush();
    await rogue.close();
    await expect.poll(() => s.geofence.results.find((r) => r.zone_id === "zone-rogue")?.code, { timeout: 5000 }).toBe("unsigned");
    expect(await s.kvGet("zone-rogue")).toBeNull();
    notes.d1 = { zone_id: zid, action_id: actionId, kv_value: kv, rogue: s.geofence.results.find((r) => r.zone_id === "zone-rogue") };
    await page.getByRole("tab", { name: /Entities/ }).click();
    await expect(page.getByTestId(`entity-${zid}`)).toBeVisible();
    await shot(page, "d1-aoi-shared.png");
  });

  test("D3 + D6: click to task sim-x1, pending, human confirm, it moves; leaving the AOI raises an alert", async () => {
    await page.getByTestId("entity-sim-x1").click();
    const goto = page.getByTestId("task-goto");
    await expect(goto).toBeVisible({ timeout: 15_000 });
    await page.getByTestId("task-hae").fill("60");
    await goto.click();
    const target = destination(CENTER, 90, 1300);
    await clickMap(page, target);
    const dlg = page.getByTestId("confirm-dialog");
    await expect(dlg).toBeVisible();
    await expect(page.getByTestId("confirm-status")).toHaveText("awaiting_confirmation");
    await expect(page.getByTestId("confirm-parameters")).toContainText('"hae_m":60');
    const hash = await page.getByTestId("confirm-params-hash").textContent();
    expect(hash).toMatch(/^sha256:[0-9a-f]{64}$/);
    // The client's own JCS + SHA-256 of what it shows equals the policy service's hash.
    await expect(page.getByTestId("confirm-hash-check")).toHaveAttribute("data-state", "verified");
    // Nothing is commanded before the human confirms.
    expect(s.commands.filter((c) => c.subject.endsWith(".goto"))).toHaveLength(0);
    await shot(page, "d3-confirm-dialog.png");
    await page.getByTestId("confirm-close").click();
    await expect(page.getByTestId("tasks").locator('[data-status="awaiting_confirmation"]')).toHaveCount(1);
    await shot(page, "d3-pending.png");
    const d0 = distanceM(await dronePos(page), target);
    await page.getByRole("button", { name: "Review and confirm" }).click();
    await page.getByTestId("confirm-approve").click();
    await expect(dlg).toBeHidden();
    await page.getByTestId("tab-detail").click();
    await expect(page.getByTestId("task-status")).toHaveAttribute("data-status", "executed", { timeout: 30_000 });
    await expect.poll(async () => distanceM(await dronePos(page), target), { timeout: 60_000 }).toBeLessThan(Math.min(d0 - 300, 200));
    await shot(page, "d3-executed-arrived.png");

    const id = s.audit.find((a) => a.subject === "evt.so.action.proposed" && (a.data.data as Record<string, unknown>).action_type === "asset.goto")!;
    const actionId = (id.data.data as Record<string, unknown>).action_id;
    const events = s.audit.filter((a) => (a.data.data as Record<string, unknown>).action_id === actionId).map((a) => a.subject.split(".")[3]);
    notes.d3 = { action_id: actionId, events, d0_m: Math.round(d0) };
    for (const e of ["proposed", "approved", "dispatched", "acknowledged", "executed"]) expect(events).toContain(e);
    const cmd = s.commands.find((c) => c.data.action_id === actionId)!;
    expect(cmd.subject).toBe("cmd.fleet.drone.sim-x1.goto");
    expect(cmd.data.sig).toBeTruthy();
    expect((cmd.data.approval as { approver: string }).approver).toBe("user:zeus");
    // Every fleet command in the run has a human approval before it.
    for (const c of s.commands) {
      expect(s.audit.some((a) => a.subject === "evt.so.action.approved" && (a.data.data as Record<string, unknown>).action_id === c.data.action_id && a.at <= c.at)).toBe(true);
    }

    // D6: sim-x1 left the AOI drawn in the D1 test: the geofence exit is in the alert list.
    await page.getByRole("tab", { name: "Alerts" }).click();
    const exit = page.getByTestId("alert-item").filter({ hasText: /sim-x1 left zone-d/ });
    await expect(exit).toBeVisible({ timeout: 20_000 });
    await shot(page, "d6-geofence-exit-alert.png");
  });

  test("D1 + D3: a keep-out drawn in the client is enforced by the policy service", async () => {
    await page.getByTestId("tab-draw").click();
    await page.getByTestId("draw-label").fill("NO FLY");
    await page.getByTestId("tool-circle").click();
    const c = destination(CENTER, 0, 900);
    await clickMap(page, c);
    await clickMap(page, destination(c, 90, 200));
    await page.getByTestId("zone-kind").selectOption("keep_out");
    await page.getByTestId("share-zone").click();
    await expect(page.getByTestId("confirm-dialog")).toBeVisible();
    await expect(page.getByTestId("confirm-safety")).toContainText("keep-out zone will constrain");
    await expect(page.getByTestId("confirm-hash-check")).toHaveAttribute("data-state", "verified");
    await page.getByTestId("confirm-approve").click();
    await expect(page.getByTestId("confirm-dialog")).toBeHidden();
    let zid: string | undefined;
    await expect.poll(async () => (zid = await l0(page, (l) => [...l.act.shared.values()].find((z) => z.kind === "keep_out")?.zoneId))).toMatch(/^zone-d/);
    await expect.poll(async () => ((await s.kvGet(zid!))?.geo as { kind?: string } | undefined)?.kind, { timeout: 10_000 }).toBe("keep_out");
    await page.waitForTimeout(800); // the policy service's KV watch
    await page.getByRole("tab", { name: /Entities/ }).click();
    await page.getByTestId("entity-sim-x1").click();
    await page.getByTestId("task-goto").click();
    await clickMap(page, c);
    await page.getByTestId("tab-tasks").click();
    const denied = page.getByTestId("tasks").locator('[data-status="denied"]').first();
    await expect(denied).toContainText("precondition_failed (keep_out)");
    await shot(page, "d1-keep-out-denied.png");
  });

  test("D1: loosening or removing a keep-out needs the same policy confirm; a rejected downgrade changes nothing", async () => {
    await page.getByTestId("tab-draw").click();
    if (!(await page.getByTestId("zone-kind").isVisible())) await page.getByRole("button", { name: "NO FLY", exact: true }).click();
    const zid = (await l0(page, (l) => [...l.act.shared.values()].find((z) => z.kind === "keep_out")?.zoneId))!;
    // Re-share as an AOI: an update that frees assets from the keep-out.
    await page.getByTestId("zone-kind").selectOption("aoi");
    await page.getByTestId("share-zone").click();
    await expect(page.getByTestId("confirm-dialog")).toBeVisible();
    await expect(page.getByTestId("confirm-action")).toContainText("zone.update");
    await expect(page.getByTestId("confirm-safety")).toContainText("changes a keep-out zone to aoi");
    await shot(page, "d1-downgrade-confirm.png");
    await page.getByTestId("confirm-reason").fill("keep the no-fly area");
    await page.getByTestId("confirm-reject").click();
    await expect(page.getByTestId("confirm-dialog")).toBeHidden();
    await page.waitForTimeout(500);
    expect(((await s.kvGet(zid))?.geo as { kind: string }).kind).toBe("keep_out");
    expect(s.mapCommands.some((c) => c.subject === `cmd.map.geofence.${zid}.update`)).toBe(false);
    expect(await l0(page, (l, z) => [...l.act.shared.values()].find((v) => v.zoneId === z)?.kind, zid)).toBe("keep_out");
    // Remove: the same confirm, with the warning.
    await page.getByTestId("tab-draw").click();
    if (!(await page.getByTestId("unshare-zone").isVisible())) await page.getByRole("button", { name: "NO FLY", exact: true }).click();
    await page.getByTestId("unshare-zone").click();
    await expect(page.getByTestId("confirm-dialog")).toBeVisible();
    await expect(page.getByTestId("confirm-action")).toContainText("zone.remove");
    await expect(page.getByTestId("confirm-safety")).toContainText("removes a keep-out zone");
    expect(await s.kvGet(zid)).not.toBeNull(); // nothing removed before the confirm
    await shot(page, "d1-remove-confirm.png");
    await page.getByTestId("confirm-approve").click();
    await expect.poll(() => s.kvGet(zid), { timeout: 10_000 }).toBeNull();
    const rm = s.mapCommands.find((c) => c.subject === `cmd.map.geofence.${zid}.remove`)!;
    expect(rm.data.sig).toBeTruthy();
    expect(rm.data.approval).toMatchObject({ status: "approved", approver: "user:zeus" });
    await expect(page.getByTestId("unshare-zone")).toHaveCount(0, { timeout: 10_000 });
    notes.d1_keep_out_lifecycle = { zone_id: zid, rejected_update: true, removed_by: rm.data.action_id };
  });

  test("D3: a proposal for a non-sim asset is denied not_simulated and shows in the tasks list", async () => {
    await page.getByRole("tab", { name: /Entities/ }).click();
    await expect(page.getByTestId(`entity-${NONSIM_ID}`)).toBeVisible({ timeout: 10_000 });
    // The detail panel never offers commands for it (the role probe is refused before authz),
    // so propose through the client's own propose path, as a click would.
    await l0(page, (l, id) => l.act.proposeSimple(id as string, "asset.hold"), NONSIM_ID);
    await page.getByTestId("tab-tasks").click();
    const row = page.getByTestId("tasks").locator('[data-status="denied"]').filter({ hasText: `asset.hold → ${NONSIM_ID}` });
    await expect(row).toContainText("not_simulated");
    expect(s.commands.some((c) => c.subject.includes(NONSIM_ID))).toBe(false);
    await shot(page, "d3-not-simulated.png");
  });

  test("B20 + D6: stopping a feed shows in the gaps panel within its freshness limit, naming the layer", async () => {
    await page.getByTestId("tab-gaps").click();
    await expect(page.getByTestId("gaps-empty")).toBeVisible();
    const last = s.feeds.stop("nws-alerts");
    const gap = page.locator('[data-testid="gap-item"][data-source="nws-alerts"]');
    await expect(gap).toBeVisible({ timeout: 15_000 });
    const shownAfter = (Date.now() - last) / 1000;
    notes.b20 = { source: "nws-alerts", limit_s: 6, shown_after_last_fetch_s: Math.round(shownAfter * 10) / 10 };
    // Limit 6 s; the panel ticks once a second and renders within 0.4 s, so at most 6 + 1.4 s.
    expect(shownAfter).toBeLessThanOrEqual(7.5);
    await expect(gap).toHaveAttribute("data-layers", /hazards/);
    await expect(gap).toContainText("Weakens layers: alerts, hazards");
    await shot(page, "b20-gaps-panel.png");
    await page.getByRole("tab", { name: "Alerts" }).click();
    await expect(page.getByTestId("alert-item").filter({ hasText: "nws-alerts stale" })).toBeVisible();
    await shot(page, "d6-feed-outage-alert.png");
  });

  test("D4: Hermes answers from map context, highlights on the map, and its proposal waits for a human", async () => {
    await page.getByTestId("tab-hermes").click();
    await page.getByTestId("hermes-input").fill("What's moving near the north field?");
    await page.getByTestId("hermes-send").click();
    const reply = page.getByTestId("hermes-hermes").last();
    await expect(reply).toContainText("WALKER", { timeout: 15_000 });
    await expect.poll(() => l0(page, (l) => l.act.overlay.highlighted)).toContain(TRACK_ID);
    await expect(page.getByTestId("hermes-map").first()).toContainText(`highlighted ${TRACK_ID}`);
    await shot(page, "d4-hermes-moving-highlight.png");

    await page.getByTestId("hermes-input").fill("Send sim-x1 to the home field");
    await page.getByTestId("hermes-send").click();
    const prop = page.locator('[data-testid^="hermes-proposal-"]').last();
    await expect(prop).toBeVisible({ timeout: 15_000 });
    await prop.click();
    await expect(page.getByTestId("confirm-agent")).toBeVisible();
    await expect(page.getByTestId("confirm-requested-by")).toContainText("agent:hermes/zeus/legacy for user:zeus");
    await expect(page.getByTestId("confirm-status")).toHaveText("awaiting_confirmation");
    const actionId = (await prop.getAttribute("data-testid"))!.replace("hermes-proposal-", "");
    expect(s.commands.some((c) => c.data.action_id === actionId)).toBe(false);
    await shot(page, "d4-hermes-proposal-confirm.png");
    await page.getByTestId("confirm-approve").click();
    await expect(page.getByTestId(`action-${actionId}`)).toHaveAttribute("data-status", "executed", { timeout: 30_000 });
    const approved = s.audit.find((a) => a.subject === "evt.so.action.approved" && (a.data.data as Record<string, unknown>).action_id === actionId)!;
    expect((approved.data.data as { requested_by: { kind: string } }).requested_by.kind).toBe("agent");
    notes.d4 = { hermes_action: actionId };
  });

  test("C8: contacts, channels and GeoChat both ways", async () => {
    await page.getByTestId("tab-chat").click();
    s.tak.say();
    await expect(page.getByTestId("contact-ANDROID-1")).toBeVisible();
    await expect(page.getByTestId("contact-ANDROID-2")).toBeVisible();
    await expect(page.getByTestId("chat-messages")).toContainText("VIPER radio check");
    await page.getByTestId("chat-callsign").fill("ZEUS-WEB");
    await page.getByTestId("chat-input").fill("hello from the web");
    await page.getByTestId("chat-send").click();
    await expect(page.locator('[data-testid="chat-msg"][data-state="sent"]')).toContainText("hello from the web");
    await expect(page.locator('[data-testid="chat-msg"][data-from="ANDROID-1"]').last()).toContainText("copy: hello from the web", { timeout: 10_000 });
    const inj = s.tak.injected.map((x) => parseCot(x)!);
    expect(inj.some((e) => e.type === "b-t-f" && e.detail.includes("hello from the web"))).toBe(true);
    await shot(page, "c8-chat-all-rooms.png");
    // Switching channel changes what is visible.
    await page.getByTestId("chat-channel").selectOption("Cyan");
    await expect(page.getByTestId("contact-ANDROID-2")).toHaveCount(0);
    await expect(page.getByTestId("room-Red")).toHaveCount(0);
    await expect(page.getByTestId("contact-ANDROID-1")).toBeVisible();
    await page.getByTestId("chat-channel").selectOption("Red");
    await page.getByTestId("room-Red").click();
    await expect(page.getByTestId("chat-messages")).toContainText("Red team only");
    await expect(page.getByTestId("contact-ANDROID-1")).toHaveCount(0);
    await shot(page, "c8-chat-channel-red.png");
  });

  test("B11: every drawing tool, buffer, lasso and CoT export", async () => {
    await page.getByTestId("tab-draw").click();
    const o = destination(CENTER, 270, 700);
    const at = (b: number, d: number) => destination(o, b, d);
    await l0(page, (l, a) => l.flyTo((a as number[])[0], (a as number[])[1], 3500), o);
    await page.waitForTimeout(600);
    const draw = async (tool: string, pts: LonLat[], finish = false, label = "") => {
      if (label) await page.getByTestId("draw-label").fill(label);
      await page.getByTestId(`tool-${tool}`).click();
      for (const p of pts) await clickMap(page, p);
      if (finish) await page.getByTestId("draw-finish").click();
    };
    await draw("point", [at(0, 300)], false, "OP-1");
    await draw("label", [at(180, 350)], false, "RALLY POINT");
    await draw("line", [at(300, 400), at(330, 250), at(0, 400)], true, "ROUTE");
    await draw("polygon", [at(200, 200), at(240, 300), at(280, 200), at(240, 100)], true, "FIELD");
    await draw("circle", [at(90, 300), at(90, 400)], false, "TANK");
    await draw("sector", [at(135, 100), at(135, 350), at(170, 350)], false, "ARC");
    await draw("rings", [o, at(45, 120)], false, "RINGS");
    await draw("rangebearing", [at(0, 50), at(60, 450)], false, "R&B");
    await draw("rectangle", [at(225, 420), at(200, 500)], false, "BOX");
    const kinds = await l0(page, (l) => [...l.act.drawings.values()].map((d) => d.kind));
    for (const k of ["point", "label", "line", "polygon", "circle", "sector", "rings", "rangebearing", "rectangle"]) expect(kinds).toContain(k);
    // Buffer the route by 80 m.
    await page.getByRole("button", { name: "ROUTE", exact: true }).click();
    await page.getByTestId("buffer-m").fill("80");
    await page.getByTestId("buffer").click();
    await expect(page.locator('[data-testid^="drawing-d"][data-kind="buffer"]')).toHaveCount(1);
    // CoT export of the circle.
    await page.getByRole("button", { name: "TANK", exact: true }).click();
    await page.getByTestId("cot-export").click();
    await expect(page.getByTestId("cot-xml")).toHaveValue(/type="u-d-c-c"/);
    const cot = await l0(page, (l) => [...l.act.drawings.values()].map((d) => ({ kind: d.kind, label: d.label, cot: l.act.cotFor(d.id) })));
    const types = Object.fromEntries((cot as { kind: string; cot: string }[]).map((c) => [c.kind, c.cot.split("\n").map((x) => parseCot(x)!.type)]));
    notes.b11 = { cot_types: types };
    writeFileSync(join(dirname(TRANSCRIPT), "b11-cot-export.xml"), (cot as { kind: string; label: string; cot: string }[]).map((c) => `<!-- ${c.kind}: ${c.label} -->\n${c.cot}`).join("\n\n") + "\n");
    expect(types).toMatchObject({ point: ["b-m-p-s-m"], line: ["b-m-r"], polygon: ["u-d-f"], circle: ["u-d-c-c"], rectangle: ["u-d-r"], rings: ["u-d-c-c", "u-d-c-c", "u-d-c-c"], buffer: ["u-d-f"] });
    // Lasso around the drone selects it.
    const dp = await dronePos(page);
    await l0(page, (l, a) => l.flyTo((a as number[])[0], (a as number[])[1], 3500), dp);
    await page.waitForTimeout(600);
    await page.getByTestId("tool-lasso").click();
    for (const b of [0, 120, 240]) await clickMap(page, destination(dp, b, 250));
    await page.getByTestId("draw-finish").click();
    await expect(page.getByTestId("lasso-result")).toContainText("sim-x1");
    expect(await l0(page, (l) => (l.act.overlay as unknown as { previewCount: number }).previewCount)).toBe(0);
    notes.lasso_entities_after_finish = await l0(page, (l) => (l.act.overlay as unknown as { viewer: { entities: { values: { id: string }[] } } }).viewer.entities.values.map((e) => e.id).filter((id) => !id.startsWith("draw:")));
    await page.getByTestId("tab-draw").click();
    await shot(page, "b11-graphics.png");
  });

  test("J2: a newcomer cannot see asset commands; operator mode needs the policy grant", async ({ browser }) => {
    await page.getByTestId("role").selectOption("newcomer");
    await page.getByRole("tab", { name: /Entities/ }).click();
    await page.getByTestId("entity-sim-x1").click();
    await expect(page.getByTestId("commands-hidden")).toContainText("Newcomer");
    await expect(page.getByTestId("task-goto")).toHaveCount(0);
    await expect(page.getByTestId("tab-draw")).toBeHidden();
    await shot(page, "j2-newcomer-no-task-button.png");

    // A user the policy service does not know as an operator, in operator mode.
    const nb = await open(browser, s, "newbie");
    await nb.page.getByTestId("role").selectOption("operator");
    await nb.page.getByRole("tab", { name: /Entities/ }).click();
    await nb.page.getByTestId("entity-sim-x1").click();
    await expect(nb.page.getByTestId("commands-hidden")).toContainText("do not operate sim-x1", { timeout: 15_000 });
    await expect(nb.page.getByTestId("task-goto")).toHaveCount(0);
    await expect(nb.page.getByTestId("role-chip")).toContainText("not an operator");
    await shot(nb.page, "j2-unauthorized-operator-mode.png");
    await nb.page.close();
    expect(errors).toEqual([]);
    // Cesium reports render failures in its own panel, not as page errors.
    await expect(page.locator(".cesium-widget-errorPanel")).toHaveCount(0);
  });

  test("WEB-P2: with the omos #562 CLIENT permissions, status comes from action.get polling alone", async ({ browser }) => {
    const c = await open(browser, s, "carol");
    // #562 lets a CLIENT user subscribe only cmd.map.> and _INBOX.>: the audit stream is refused.
    await c.page.getByTestId("tab-tasks").click();
    await expect(c.page.getByTestId("bus-notes")).toContainText("evt.so.action.> — action audit: not permitted", { timeout: 10_000 });
    // No ent.> either (entities come from svc.layer.*, not run here): hand the page sim-x1's record.
    await l0(c.page, (l, rec) => l.store.apply(rec, "fleet"), s.entity.droneRecord(s.entity.seq.get("sim-x1")));
    await c.page.getByTestId("role").selectOption("operator");
    await c.page.getByRole("tab", { name: /Entities/ }).click();
    await c.page.getByTestId("entity-sim-x1").click();
    await expect(c.page.getByTestId("task-rtl")).toBeVisible({ timeout: 15_000 });
    await c.page.getByTestId("task-rtl").click();
    await expect(c.page.getByTestId("confirm-dialog")).toBeVisible();
    await expect(c.page.getByTestId("confirm-hash-check")).toHaveAttribute("data-state", "verified");
    await c.page.getByTestId("confirm-approve").click();
    await c.page.getByTestId("tab-tasks").click();
    const row = c.page.getByTestId("tasks").locator("li").filter({ hasText: "asset.rtl → sim-x1" }).first();
    await expect(row).toHaveAttribute("data-status", "executed", { timeout: 30_000 });
    // Every status after the confirm reply came from polling (no audit events reach this page).
    const vias = await l0(c.page, (l) => {
      const t = (l.act as unknown as { tracker: { list(): { action_type?: string; history: { status: string; via: string }[] }[] } }).tracker;
      return t.list().find((a) => a.action_type === "asset.rtl")!.history;
    });
    expect((vias as { via: string }[]).some((h) => h.via === "audit")).toBe(false);
    expect((vias as { status: string; via: string }[]).some((h) => h.status === "executed" && h.via === "get")).toBe(true);
    notes.web_p2_polling = vias;
    await shot(c.page, "web-p2-polling-only.png");
    expect(c.errors).toEqual([]);
    await c.page.close();
  });

  test("phone layout: tasks and detail at 390×844", async ({ browser }) => {
    const ph = await open(browser, s, "zeus", { width: 390, height: 844 });
    await ph.page.getByTestId("role").selectOption("operator");
    await ph.page.getByRole("tab", { name: /Entities/ }).click();
    await ph.page.getByTestId("entity-sim-x1").click();
    await expect(ph.page.getByTestId("task-goto")).toBeVisible({ timeout: 15_000 });
    const overflow = await ph.page.evaluate(() => document.documentElement.scrollWidth - document.documentElement.clientWidth);
    expect(overflow).toBeLessThanOrEqual(0);
    await shot(ph.page, "phone-detail-actions.png");
    await ph.page.close();
  });
});

// Keep NORTH_FIELD referenced for readers: Hermes' "north field" is a fixture AOI there.
void NORTH_FIELD;
