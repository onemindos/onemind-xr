// E2E integration step 8: the run/m4-web client against the REAL omos stack (all services from
// their branch tips, A19 accounts) held up by the Go harness. No stand-ins: entity, layer,
// policy, bridge, sim drone are the real ones. Credentials come from the harness run dir.
import { readFileSync, mkdirSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { expect, test } from "@playwright/test";
import { destination, distanceM, type LonLat } from "../src/act/geo";

const stack = JSON.parse(readFileSync(process.env.E2E_STACK!, "utf8"));
const SHOTS = process.env.E2E_SHOTS!;
mkdirSync(SHOTS, { recursive: true });
const HOME: LonLat = [stack.home[1], stack.home[0]];
const notes: Record<string, unknown> = {};

test("web client on the real stack: drone moving, pending confirm, human confirm, it flies", async ({ browser }) => {
  test.setTimeout(240_000);
  const page = await browser.newPage({ viewport: { width: 1440, height: 900 } });
  const errors: string[] = [];
  const consoleLines: string[] = [];
  page.on("pageerror", (e) => errors.push(e.message));
  page.on("console", (m) => consoleLines.push(`${m.type()}: ${m.text()}`));
  await page.addInitScript((c) => sessionStorage.setItem("l0.creds", JSON.stringify(c)), stack.users.zeus);
  const q = new URLSearchParams({ nats: stack.ws, lon: String(HOME[0]), lat: String(HOME[1]), height: "7000" });
  await page.goto(`./?${q}`);
  await expect(page.getByTestId("conn")).toHaveAttribute("data-state", "connected");
  const pos = async (): Promise<LonLat | null> =>
    page.evaluate(() => { const e = (window as any).__l0.store.get("sim-x1"); return e ? [e.r.lon, e.r.lat] : null; });
  await expect.poll(pos, { timeout: 30_000 }).not.toBeNull();
  notes.path = await page.evaluate(() => (window as any).__l0?.path ?? (document.querySelector('[data-testid="conn"]') as HTMLElement)?.dataset);
  // Moving: positions change over ~6 s.
  const samples: LonLat[] = [];
  for (let i = 0; i < 6; i++) { samples.push((await pos())!); await page.waitForTimeout(1000); }
  const moved = Math.max(...samples.map((p) => distanceM(p, samples[0])));
  notes.samples = samples; notes.moved_m = moved;
  // E2E-3: before the browser goto the service has no open alert (the reflight took sim-x1 back
  // into zone-e2e-pad and the service resolved step 4's alert, reason "returned").
  notes.alerts_before = { service_items: await page.locator('[data-testid="alert-item"][data-source="service"]').count(), feed_status: await page.evaluate(() => (window as any).__l0.alertFeed?.status) };
  await page.getByRole("tab", { name: /Entities/ }).click();
  await expect(page.getByTestId("entity-sim-x1")).toBeVisible({ timeout: 20_000 });
  await page.screenshot({ path: join(SHOTS, "w1-drone-on-globe.png") });

  await page.getByTestId("role").selectOption("operator");
  await page.getByTestId("entity-sim-x1").click();
  const goto = page.getByTestId("task-goto");
  await expect(goto).toBeVisible({ timeout: 15_000 });
  await page.getByTestId("task-hae").fill("60");
  await goto.click();
  const target = destination(HOME, 270, 900);
  const sp = await page.evaluate(([lon, lat]) => (window as any).__l0.toScreen(lon, lat), target);
  expect(sp).not.toBeNull();
  await page.mouse.click(sp.x, sp.y);
  const dlg = page.getByTestId("confirm-dialog");
  await expect(dlg).toBeVisible();
  await expect(page.getByTestId("confirm-status")).toHaveText("awaiting_confirmation");
  await expect(page.getByTestId("confirm-hash-check")).toHaveAttribute("data-state", "verified");
  notes.confirm = {
    action: await page.getByTestId("confirm-action").textContent(),
    target: await page.getByTestId("confirm-target").textContent(),
    params: await page.getByTestId("confirm-parameters").textContent(),
    hash: await page.getByTestId("confirm-params-hash").textContent(),
  };
  await page.screenshot({ path: join(SHOTS, "w2-confirm-dialog.png") });
  await page.getByTestId("confirm-close").click();
  await expect(page.getByTestId("tasks").locator('[data-status="awaiting_confirmation"]')).toHaveCount(1);
  await page.getByTestId("tab-tasks").click().catch(() => {});
  await page.screenshot({ path: join(SHOTS, "w3-pending-confirm.png") });
  const d0 = distanceM((await pos())!, target);
  await page.getByRole("button", { name: "Review and confirm" }).click();
  await page.getByTestId("confirm-approve").click();
  await expect(dlg).toBeHidden();
  await page.getByTestId("tab-detail").click();
  await expect(page.getByTestId("task-status")).toHaveAttribute("data-status", "executed", { timeout: 30_000 });
  const flight: { t: number; p: LonLat; d: number }[] = [];
  await expect.poll(async () => { const p = (await pos())!; const d = distanceM(p, target); flight.push({ t: Date.now(), p, d: Math.round(d) }); if (flight.length === 4) await page.screenshot({ path: join(SHOTS, "w4a-drone-moving.png") }); return d; }, { timeout: 90_000, intervals: [1000] }).toBeLessThan(80);
  notes.flight = flight;
  const distinct = new Set(flight.map((f) => f.p.join(","))).size;
  expect(distinct, "the client showed sim-x1 at several positions while it flew").toBeGreaterThan(3);
  notes.d0_m = Math.round(d0); notes.d1_m = Math.round(distanceM((await pos())!, target));
  await page.screenshot({ path: join(SHOTS, "w4-executed-arrived.png") });
  // E2E-3 (F-ALERT): the geofence service's alert raised live when the browser-driven goto
  // takes sim-x1 out of keep-in zone-e2e-pad is listed for the CLIENT user zeus, from svc.geofence.alerts + evt.so.alert.*.
  await page.getByTestId("alert-chip").click();
  const alertItem = page.locator('[data-testid="alert-item"][data-source="service"]').filter({ hasText: /zone-e2e-pad/i });
  await expect(alertItem.first()).toBeVisible({ timeout: 20_000 });
  await expect(page.locator("#tab-alerts")).toHaveAttribute("data-alert-service", "ok/ok", { timeout: 20_000 });
  notes.alert = {
    text: await alertItem.first().textContent(),
    key: await alertItem.first().getAttribute("data-key"),
    feed_status: await page.evaluate(() => (window as any).__l0.alertFeed?.status),
    service_items: await page.locator('[data-testid="alert-item"][data-source="service"]').count(),
    chip_level: await page.getByTestId("alert-chip").getAttribute("data-level"),
  };
  await page.screenshot({ path: join(SHOTS, "w0-open-geofence-alert.png") });
  notes.page_errors = errors;
  notes.console_tail = consoleLines.slice(-40);
  writeFileSync(join(SHOTS, "..", "web-notes.json"), JSON.stringify(notes, null, 2));
});

test.afterEach(async () => { writeFileSync(join(SHOTS, "..", "web-notes.json"), JSON.stringify(notes, null, 2)); });
