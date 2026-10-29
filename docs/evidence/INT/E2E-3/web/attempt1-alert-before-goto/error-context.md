# Instructions

- Following Playwright test failed.
- Explain why, be concise, respect Playwright best practices.
- Provide a snippet of code with the fix, if possible.

# Test info

- Name: int.spec.ts >> web client on the real stack: drone moving, pending confirm, human confirm, it flies
- Location: e2e-int/int.spec.ts:15:1

# Error details

```
Error: expect(locator).toBeVisible() failed

Locator: locator('[data-testid="alert-item"][data-source="service"]').filter({ hasText: /zone-e2e-pad/i }).first()
Expected: visible
Timeout: 20000ms
Error: element(s) not found

Call log:
  - Expect "toBeVisible" locator('[data-testid="alert-item"][data-source="service"]').filter({ hasText: /zone-e2e-pad/i }).first() with timeout 20000ms
  - waiting for locator('[data-testid="alert-item"][data-source="service"]').filter({ hasText: /zone-e2e-pad/i }).first()

```

```yaml
- banner:
  - strong: OneMind L0
  - text: bus ws://127.0.0.1:65047
  - button "Alerts 0"
  - text: Mode
  - combobox "Role mode":
    - option "Newcomer" [selected]
    - option "Observer"
    - option "Operator"
  - text: newcomer
  - 'button "Mission tier: off"'
  - button "Switch 2D or 3D": 2D
- main:
  - link "Cesium ion":
    - /url: https://cesium.com/
    - img "Cesium ion"
  - complementary:
    - tablist "Panels":
      - tab "Alerts" [selected]
      - tab "Entities 2"
      - tab "Detail"
      - tab "Tasks 0"
      - tab "Gaps 0"
      - tab "Hermes"
      - tab "Chat"
      - tab "Layers"
      - tab "Connection"
    - tabpanel "Alerts":
      - paragraph: No open alerts.
```

# Test source

```ts
  1  | // E2E integration step 8: the run/m4-web client against the REAL omos stack (all services from
  2  | // their branch tips, A19 accounts) held up by the Go harness. No stand-ins: entity, layer,
  3  | // policy, bridge, sim drone are the real ones. Credentials come from the harness run dir.
  4  | import { readFileSync, mkdirSync, writeFileSync } from "node:fs";
  5  | import { join } from "node:path";
  6  | import { expect, test } from "@playwright/test";
  7  | import { destination, distanceM, type LonLat } from "../src/act/geo";
  8  | 
  9  | const stack = JSON.parse(readFileSync(process.env.E2E_STACK!, "utf8"));
  10 | const SHOTS = process.env.E2E_SHOTS!;
  11 | mkdirSync(SHOTS, { recursive: true });
  12 | const HOME: LonLat = [stack.home[1], stack.home[0]];
  13 | const notes: Record<string, unknown> = {};
  14 | 
  15 | test("web client on the real stack: drone moving, pending confirm, human confirm, it flies", async ({ browser }) => {
  16 |   test.setTimeout(240_000);
  17 |   const page = await browser.newPage({ viewport: { width: 1440, height: 900 } });
  18 |   const errors: string[] = [];
  19 |   const consoleLines: string[] = [];
  20 |   page.on("pageerror", (e) => errors.push(e.message));
  21 |   page.on("console", (m) => consoleLines.push(`${m.type()}: ${m.text()}`));
  22 |   await page.addInitScript((c) => sessionStorage.setItem("l0.creds", JSON.stringify(c)), stack.users.zeus);
  23 |   const q = new URLSearchParams({ nats: stack.ws, lon: String(HOME[0]), lat: String(HOME[1]), height: "7000" });
  24 |   await page.goto(`./?${q}`);
  25 |   await expect(page.getByTestId("conn")).toHaveAttribute("data-state", "connected");
  26 |   const pos = async (): Promise<LonLat | null> =>
  27 |     page.evaluate(() => { const e = (window as any).__l0.store.get("sim-x1"); return e ? [e.r.lon, e.r.lat] : null; });
  28 |   await expect.poll(pos, { timeout: 30_000 }).not.toBeNull();
  29 |   notes.path = await page.evaluate(() => (window as any).__l0?.path ?? (document.querySelector('[data-testid="conn"]') as HTMLElement)?.dataset);
  30 |   // Moving: positions change over ~6 s.
  31 |   const samples: LonLat[] = [];
  32 |   for (let i = 0; i < 6; i++) { samples.push((await pos())!); await page.waitForTimeout(1000); }
  33 |   const moved = Math.max(...samples.map((p) => distanceM(p, samples[0])));
  34 |   notes.samples = samples; notes.moved_m = moved;
  35 |   // E2E-3 (F-ALERT): the geofence service's open alert from step 4 (sim-x1 left keep-in
  36 |   // zone-e2e-pad) is listed for the CLIENT user zeus, from svc.geofence.alerts + evt.so.alert.*.
  37 |   await page.getByTestId("alert-chip").click();
  38 |   const alertItem = page.locator('[data-testid="alert-item"][data-source="service"]').filter({ hasText: /zone-e2e-pad/i });
> 39 |   await expect(alertItem.first()).toBeVisible({ timeout: 20_000 });
     |                                   ^ Error: expect(locator).toBeVisible() failed
  40 |   await expect(page.locator("#tab-alerts")).toHaveAttribute("data-alert-service", "ok/ok", { timeout: 20_000 });
  41 |   notes.alert = {
  42 |     text: await alertItem.first().textContent(),
  43 |     key: await alertItem.first().getAttribute("data-key"),
  44 |     feed_status: await page.evaluate(() => (window as any).__l0.alertFeed?.status),
  45 |     service_items: await page.locator('[data-testid="alert-item"][data-source="service"]').count(),
  46 |     chip_level: await page.getByTestId("alert-chip").getAttribute("data-level"),
  47 |   };
  48 |   await page.screenshot({ path: join(SHOTS, "w0-open-geofence-alert.png") });
  49 |   await page.getByRole("tab", { name: /Entities/ }).click();
  50 |   await expect(page.getByTestId("entity-sim-x1")).toBeVisible({ timeout: 20_000 });
  51 |   await page.screenshot({ path: join(SHOTS, "w1-drone-on-globe.png") });
  52 | 
  53 |   await page.getByTestId("role").selectOption("operator");
  54 |   await page.getByTestId("entity-sim-x1").click();
  55 |   const goto = page.getByTestId("task-goto");
  56 |   await expect(goto).toBeVisible({ timeout: 15_000 });
  57 |   await page.getByTestId("task-hae").fill("60");
  58 |   await goto.click();
  59 |   const target = destination(HOME, 270, 900);
  60 |   const sp = await page.evaluate(([lon, lat]) => (window as any).__l0.toScreen(lon, lat), target);
  61 |   expect(sp).not.toBeNull();
  62 |   await page.mouse.click(sp.x, sp.y);
  63 |   const dlg = page.getByTestId("confirm-dialog");
  64 |   await expect(dlg).toBeVisible();
  65 |   await expect(page.getByTestId("confirm-status")).toHaveText("awaiting_confirmation");
  66 |   await expect(page.getByTestId("confirm-hash-check")).toHaveAttribute("data-state", "verified");
  67 |   notes.confirm = {
  68 |     action: await page.getByTestId("confirm-action").textContent(),
  69 |     target: await page.getByTestId("confirm-target").textContent(),
  70 |     params: await page.getByTestId("confirm-parameters").textContent(),
  71 |     hash: await page.getByTestId("confirm-params-hash").textContent(),
  72 |   };
  73 |   await page.screenshot({ path: join(SHOTS, "w2-confirm-dialog.png") });
  74 |   await page.getByTestId("confirm-close").click();
  75 |   await expect(page.getByTestId("tasks").locator('[data-status="awaiting_confirmation"]')).toHaveCount(1);
  76 |   await page.getByTestId("tab-tasks").click().catch(() => {});
  77 |   await page.screenshot({ path: join(SHOTS, "w3-pending-confirm.png") });
  78 |   const d0 = distanceM((await pos())!, target);
  79 |   await page.getByRole("button", { name: "Review and confirm" }).click();
  80 |   await page.getByTestId("confirm-approve").click();
  81 |   await expect(dlg).toBeHidden();
  82 |   await page.getByTestId("tab-detail").click();
  83 |   await expect(page.getByTestId("task-status")).toHaveAttribute("data-status", "executed", { timeout: 30_000 });
  84 |   const flight: { t: number; p: LonLat; d: number }[] = [];
  85 |   await expect.poll(async () => { const p = (await pos())!; const d = distanceM(p, target); flight.push({ t: Date.now(), p, d: Math.round(d) }); if (flight.length === 4) await page.screenshot({ path: join(SHOTS, "w4a-drone-moving.png") }); return d; }, { timeout: 90_000, intervals: [1000] }).toBeLessThan(80);
  86 |   notes.flight = flight;
  87 |   const distinct = new Set(flight.map((f) => f.p.join(","))).size;
  88 |   expect(distinct, "the client showed sim-x1 at several positions while it flew").toBeGreaterThan(3);
  89 |   notes.d0_m = Math.round(d0); notes.d1_m = Math.round(distanceM((await pos())!, target));
  90 |   await page.screenshot({ path: join(SHOTS, "w4-executed-arrived.png") });
  91 |   notes.page_errors = errors;
  92 |   notes.console_tail = consoleLines.slice(-40);
  93 |   writeFileSync(join(SHOTS, "..", "web-notes.json"), JSON.stringify(notes, null, 2));
  94 | });
  95 | 
  96 | test.afterEach(async () => { writeFileSync(join(SHOTS, "..", "web-notes.json"), JSON.stringify(notes, null, 2)); });
  97 | 
```