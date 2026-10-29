// End-to-end: the built L0 client in headless Chromium against a disposable nats-server
// WebSocket, the fixture publisher (entity.md records, a moving track, removal and expiry) and a
// disposable MediaMTX with an ffmpeg test pattern on drone/sim-x1. Screenshots go to
// L0_EVIDENCE_DIR (default test-results/screens).
import { mkdirSync } from "node:fs";
import { join } from "node:path";
import { expect, test, type Page } from "@playwright/test";
// @ts-expect-error plain ESM tool module
import { startMediaMTX, startNats } from "../tools/services.mjs";
// @ts-expect-error plain ESM tool module
import { Fixture, IDS } from "../tools/fixture.mjs";

const SHOTS = process.env.L0_EVIDENCE_DIR ?? "test-results/screens";
mkdirSync(SHOTS, { recursive: true });

type Stack = { nats: { wsUrl: string; stop(): Promise<void> }; fx: InstanceType<typeof Fixture> };
let mtx: { rtspUrl: string; whepBase: string; hlsBase: string; stop(): Promise<void>; log(): string };

async function stack(mode: "core" | "entity" | "layer", o: Record<string, number> = {}): Promise<Stack> {
  const nats = await startNats();
  const fx = await new Fixture({ wsUrl: nats.wsUrl, mode, videoUrl: mtx.rtspUrl, hz: 2, ...o }).start();
  return { nats, fx };
}

function url(s: Stack, extra = "") {
  const q = new URLSearchParams({ nats: s.nats.wsUrl, whep: mtx.whepBase, hls: mtx.hlsBase });
  return `./?${q}${extra}`;
}

function watchErrors(page: Page): string[] {
  const errors: string[] = [];
  page.on("pageerror", (e) => errors.push(e.message));
  page.on("console", (m) => {
    if (m.type() === "error") errors.push(m.text());
  });
  return errors;
}

async function videoAdvances(page: Page) {
  const t0 = await page.evaluate(() => (document.getElementById("video") as HTMLVideoElement).currentTime);
  await page.waitForTimeout(1500);
  return page.evaluate((t) => {
    const v = document.getElementById("video") as HTMLVideoElement;
    return { advanced: v.currentTime > t, width: v.videoWidth, height: v.videoHeight, readyState: v.readyState };
  }, t0);
}

test.beforeAll(async () => {
  mtx = await startMediaMTX();
});
test.afterAll(async () => {
  await mtx?.stop();
});

test.describe("desktop, interim path (svc.entity.query + ent.*.*.state)", () => {
  let s: Stack;
  test.beforeAll(async () => {
    s = await stack("entity", { removeAfterS: 25, expireAfterS: 18 });
  });
  test.afterAll(async () => {
    await s.fx.stop();
    await s.nats.stop();
  });
  test.use({ viewport: { width: 1440, height: 900 } });

  test("entities, alerts, detail with provenance, WHEP video, removal, expiry, mission tier", async ({ page }) => {
    const errors = watchErrors(page);
    await page.goto(url(s));
    await expect(page.getByTestId("conn")).toHaveAttribute("data-state", "connected");

    // Entities from ent.*.*.state per entity.md, in the list and on the globe.
    await page.getByRole("tab", { name: /Entities/ }).click();
    for (const id of [IDS.drone, IDS.track, IDS.vehicle, IDS.removed, IDS.expiring]) {
      await expect(page.getByTestId(`entity-${id}`)).toBeVisible();
    }
    // Marked AMBER+STRICT for team-b only: never shown to a homestead viewer.
    await expect(page.getByTestId(`entity-${IDS.marked}`)).toHaveCount(0);
    expect(await page.evaluate(() => (window as any).__l0.store.get("sim-m1") !== undefined)).toBe(true);
    await page.getByRole("tab", { name: "Layers" }).click();
    await expect(page.getByTestId("layer-fleet")).toHaveAttribute("data-mode", "interim");

    // Alerts tab: the warning alert entity.
    await page.getByTestId("alert-chip").click();
    const alert = page.getByTestId("alert-item").filter({ hasText: "SIM-X1 battery below 85%" });
    await expect(alert).toContainText("WARNING");

    // Detail with provenance.
    await page.getByRole("tab", { name: /Entities/ }).click();
    await page.getByTestId(`entity-${IDS.drone}`).click();
    const detail = page.getByTestId("detail");
    await expect(detail).toHaveAttribute("data-entity", IDS.drone);
    await expect(page.getByTestId("detail-callsign")).toHaveText("SIM-X1");
    await expect(page.getByTestId("badge-simulated")).toBeVisible();
    await expect(page.getByTestId("prov-primary")).toHaveText("bridge-mavlink");
    await expect(page.getByTestId("prov-how")).toHaveText("m-s (simulated)");
    await expect(page.getByTestId("sources")).toContainText("tel.fleet.drone.sim-x1.position");
    const pos1 = await page.getByTestId("detail-position").textContent();
    await expect(page.getByTestId("detail-position")).not.toHaveText(pos1!, { timeout: 10_000 });

    // Video panel: WHEP from MediaMTX, frames advancing.
    await expect(page.getByTestId("video-panel")).toHaveAttribute("data-transport", "whep", { timeout: 30_000 });
    const v = await videoAdvances(page);
    expect(v).toMatchObject({ advanced: true, width: 640, height: 360 });
    await page.waitForTimeout(500);
    await page.screenshot({ path: join(SHOTS, "desktop-detail-video.png") });

    // The moving track enters the AOI: geofence event -> alert.
    await page.getByTestId("alert-chip").click();
    await expect(page.getByTestId("alert-item").filter({ hasText: `${IDS.track} entered ${IDS.zone}` })).toBeVisible({ timeout: 30_000 });

    // Removal (final live:false record) and expiry (expiry_t passes, no message).
    await page.getByRole("tab", { name: /Entities/ }).click();
    await expect(page.getByTestId(`entity-${IDS.expiring}`)).toHaveCount(0, { timeout: 30_000 });
    await expect(page.getByTestId(`entity-${IDS.removed}`)).toHaveCount(0, { timeout: 30_000 });
    const gone = await page.evaluate(() => {
      const st = (window as any).__l0.store;
      return { r1: st.get("sim-r1") === undefined, e1: st.get("sim-e1") === undefined, expired: st.stats.expired };
    });
    expect(gone.r1 && gone.e1).toBe(true);
    expect(gone.expired).toBeGreaterThanOrEqual(1);

    // Mission tier: the untasked vehicle outside the AOI disappears; the tasked drone stays.
    await page.getByTestId("mission-toggle").click();
    await expect(page.getByTestId("mission-toggle")).toHaveAttribute("aria-pressed", "true");
    await expect(page.getByTestId(`entity-${IDS.vehicle}`)).toHaveCount(0);
    await expect(page.getByTestId(`entity-${IDS.drone}`)).toBeVisible();
    await page.getByTestId("alert-chip").click();
    await page.waitForTimeout(500);
    await page.screenshot({ path: join(SHOTS, "desktop-mission-alerts.png") });
    expect(await page.evaluate(() => (window as any).__l0.drawn)).toBeGreaterThanOrEqual(2);

    expect(errors).toEqual([]);
  });
});

test.describe("phone, svc.layer path (snapshot + watch), LL-HLS video", () => {
  let s: Stack;
  test.beforeAll(async () => {
    s = await stack("layer", { removeAfterS: 25, expireAfterS: 18 });
  });
  test.afterAll(async () => {
    await s.fx.stop();
    await s.nats.stop();
  });
  test.use({ viewport: { width: 390, height: 844 }, isMobile: true, hasTouch: true, deviceScaleFactor: 2 });

  test("mission tier and alerts on a phone-sized screen; detail and HLS video", async ({ page }) => {
    const errors = watchErrors(page);
    await page.goto(url(s, "&video=hls&mission=1"));
    await expect(page.getByTestId("conn")).toHaveAttribute("data-state", "connected");
    await expect(page.getByTestId("mission-toggle")).toHaveAttribute("aria-pressed", "true");

    // FEATURES J1 acceptance: the phone shows mission tier and alerts from the NATS WebSocket.
    const alert = page.getByTestId("alert-item").filter({ hasText: "SIM-X1 battery below 85%" });
    await expect(alert).toContainText("WARNING");
    await expect(page.getByTestId("alert-chip")).toHaveAttribute("data-level", "warning");
    await page.getByRole("tab", { name: /Entities/ }).click();
    await expect(page.getByTestId(`entity-${IDS.drone}`)).toContainText("mission");
    await expect(page.getByTestId(`entity-${IDS.vehicle}`)).toHaveCount(0); // outside the AOI, untasked
    await page.getByRole("tab", { name: "Layers" }).click();
    await expect(page.getByTestId("layer-fleet")).toHaveAttribute("data-mode", "layer");
    await expect(page.getByTestId("layer-zones")).toHaveAttribute("data-mode", "layer");
    await page.getByRole("tab", { name: "Alerts" }).click();
    await page.waitForTimeout(800);
    await page.screenshot({ path: join(SHOTS, "phone-mission-alerts.png") });

    // No horizontal page scroll at phone width.
    const overflow = await page.evaluate(() => document.documentElement.scrollWidth - window.innerWidth);
    expect(overflow).toBeLessThanOrEqual(0);

    // Tap the alert: selects the entity it is about, shows detail and HLS video.
    await alert.getByRole("button").tap();
    await expect(page.getByTestId("detail")).toHaveAttribute("data-entity", IDS.drone);
    await expect(page.getByTestId("prov-primary")).toHaveText("bridge-mavlink");
    await page.waitForTimeout(500);
    await page.screenshot({ path: join(SHOTS, "phone-detail.png") });
    await expect(page.getByTestId("video-panel")).toHaveAttribute("data-transport", "hls", { timeout: 40_000 });
    const v = await videoAdvances(page);
    expect(v.advanced).toBe(true);
    expect(v.width).toBe(640);
    await page.getByTestId("video-panel").scrollIntoViewIfNeeded();
    await page.waitForTimeout(500);
    await page.screenshot({ path: join(SHOTS, "phone-detail-video.png") });

    // Removal through a watch `remove` op.
    await page.getByRole("tab", { name: /Entities/ }).click();
    await page.getByTestId("mission-toggle").tap();
    await expect(page.getByTestId(`entity-${IDS.removed}`)).toBeVisible();
    await expect(page.getByTestId(`entity-${IDS.removed}`)).toHaveCount(0, { timeout: 35_000 });
    await page.waitForTimeout(500);
    await page.screenshot({ path: join(SHOTS, "phone-entities.png") });
    expect(errors).toEqual([]);
  });
});

test.describe("core-only path (no services answer)", () => {
  let s: Stack;
  test.beforeAll(async () => {
    s = await stack("core");
  });
  test.afterAll(async () => {
    await s.fx.stop();
    await s.nats.stop();
  });

  test("renders from core ent.*.*.state alone", async ({ page }) => {
    const errors = watchErrors(page);
    await page.goto(url(s));
    await page.getByRole("tab", { name: "Layers" }).click();
    await expect(page.getByTestId("layer-fleet")).toHaveAttribute("data-mode", "core");
    await page.getByRole("tab", { name: /Entities/ }).click();
    await expect(page.getByTestId(`entity-${IDS.drone}`)).toBeVisible();
    await expect(page.getByTestId(`entity-${IDS.track}`)).toBeVisible();
    expect(errors).toEqual([]);
  });
});

// Opt-in (L0_ONLINE=1): open imagery and terrain over the internet, no token, no paid API.
// A handful of OSM tiles (attribution shown, light use per the OSM tile policy) and AWS Terrain
// Tiles (terrarium). Not part of the default offline run.
test.describe("open imagery and terrain (online, opt-in)", () => {
  test.skip(process.env.L0_ONLINE !== "1", "set L0_ONLINE=1 to fetch OSM and terrarium tiles");
  let s: Stack;
  test.beforeAll(async () => {
    s = await stack("entity");
  });
  test.afterAll(async () => {
    await s.fx.stop();
    await s.nats.stop();
  });
  test.use({ viewport: { width: 1440, height: 900 } });

  test("OSM imagery with terrarium terrain loads without a Cesium ion token", async ({ page }) => {
    const hosts = new Set<string>();
    page.on("request", (r) => hosts.add(new URL(r.url()).host));
    const terrain: number[] = [];
    page.on("response", (r) => {
      if (r.url().includes("elevation-tiles-prod/terrarium")) terrain.push(r.status());
    });
    await page.goto(url(s, "&imagery=osm&terrain=terrarium"));
    await expect(page.getByTestId("conn")).toHaveAttribute("data-state", "connected");
    await expect.poll(() => terrain.filter((c) => c === 200).length, { timeout: 30_000 }).toBeGreaterThan(0);
    await page.waitForTimeout(6000);
    await page.screenshot({ path: join(SHOTS, "desktop-osm-terrarium.png") });
    expect([...hosts].some((h) => h.includes("cesium.com"))).toBe(false); // never Cesium ion
    expect([...hosts].some((h) => h === "tile.openstreetmap.org")).toBe(true);
  });
});
