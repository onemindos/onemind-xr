// The real bus path: a disposable nats-server (own port, temp store), the fixture publisher in
// each mode, and LayerFeed connected over NATS WebSocket with the same nats.js used in the
// browser (Node 22 has a global WebSocket). Skips when no nats-server binary is found.
import { existsSync } from "node:fs";
import { wsconnect, type NatsConnection } from "@nats-io/nats-core";
import { afterAll, beforeAll, describe, expect, it } from "vitest";
import { DEFAULT_LAYERS } from "../../src/bus/catalog";
import { LayerFeed, getEntity } from "../../src/bus/layerclient";
import { EntityStore } from "../../src/bus/store";
// @ts-expect-error plain ESM tool module
import { startNats } from "../../tools/services.mjs";
// @ts-expect-error plain ESM tool module
import { Fixture, IDS } from "../../tools/fixture.mjs";

const NATS_BIN = process.env.NATS_SERVER_BIN ?? "/Users/danielcarku/Desktop/work/.tools/bin/nats-server";
const haveNats = existsSync(NATS_BIN);
const wait = (ms: number) => new Promise((r) => setTimeout(r, ms));

async function until(fn: () => boolean, ms: number, label: string) {
  const start = Date.now();
  while (!fn()) {
    if (Date.now() - start > ms) throw new Error(`timeout: ${label}`);
    await wait(50);
  }
}

for (const mode of ["core", "entity", "layer"] as const) {
  describe.skipIf(!haveNats)(`bus integration, fixture mode "${mode}"`, () => {
    let nats: { wsUrl: string; stop(): Promise<void> };
    let fx: InstanceType<typeof Fixture>;
    let nc: NatsConnection;
    const store = new EntityStore();
    const feeds: LayerFeed[] = [];

    beforeAll(async () => {
      nats = await startNats();
      nc = await wsconnect({ servers: nats.wsUrl, name: `l0-it-${mode}` });
      // Start the client feeds first in core mode, so it sees everything live; services modes
      // start the fixture first so the snapshot has content.
      if (mode !== "core") {
        fx = await new Fixture({ wsUrl: nats.wsUrl, mode, hz: 4, removeAfterS: 4, expireAfterS: 3 }).start();
        await wait(300);
      }
      for (const id of ["fleet", "tracks", "zones", "alerts"]) {
        const f = new LayerFeed(nc, store, DEFAULT_LAYERS.find((l) => l.id === id)!, { snapshotTimeoutMs: 1500 });
        feeds.push(f);
        await f.start();
      }
      if (mode === "core") {
        fx = await new Fixture({ wsUrl: nats.wsUrl, mode, hz: 4, removeAfterS: 4, expireAfterS: 3 }).start();
      }
    }, 30_000);

    afterAll(async () => {
      for (const f of feeds) f.stop();
      await fx?.stop();
      await nc?.close();
      await nats?.stop();
    });

    it("chooses the expected contract path", () => {
      const expected = { core: "core", entity: "interim", layer: "layer" }[mode];
      expect(feeds.map((f) => f.status.mode)).toEqual([expected, expected, expected, expected]);
    });

    it("receives sim-x1 per entity.md and follows its seq upward", async () => {
      await until(() => !!store.get(IDS.drone), 5000, "sim-x1 present");
      const first = store.get(IDS.drone)!.r.seq;
      await until(() => store.get(IDS.drone)!.r.seq >= first + 3, 5000, "sim-x1 seq advances");
      const r = store.get(IDS.drone)!;
      expect(r.r.simulated).toBe(true);
      expect(r.r.entity_type).toBe("drone");
      expect(r.full?.provenance?.primary).toBe("bridge-mavlink");
      expect(r.full?.media?.[0]?.kind).toBe("video");
    });

    it("tracks the moving track and has the AOI zone and the alert", async () => {
      await until(() => !!store.get(IDS.track), 5000, "track present");
      const lon0 = store.get(IDS.track)!.r.lon;
      await wait(1200);
      expect(store.get(IDS.track)!.r.lon).toBeGreaterThan(lon0);
      expect(store.get(IDS.zone)?.full?.geo?.kind).toBe("aoi");
      expect((store.get(IDS.alert)?.full as Record<string, unknown> | undefined)?.level).toBe("warning");
    });

    it("removes sim-r1 on its final live:false record", async () => {
      await until(() => fx.seq.get(IDS.removed) === 2, 8000, "fixture deleted sim-r1");
      await until(() => !store.get(IDS.removed), 3000, "client removed sim-r1");
    });

    it("expires sim-e1 from expiry_t without any further message", async () => {
      await until(() => Date.now() > (store.get(IDS.expiring)?.r.expiry_t ?? 0) + 10, 8000, "past expiry");
      store.expire();
      expect(store.get(IDS.expiring)).toBeUndefined();
    });

    it.skipIf(mode === "core")("serves the full record through svc.entity.get", async () => {
      const full = (await getEntity(nc, IDS.drone)) as Record<string, unknown> | null;
      expect(full?.entity_id).toBe(IDS.drone);
      expect((full?.sources as unknown[])?.length).toBe(1);
      expect(await getEntity(nc, "sim-nope")).toBeNull();
    });
  });
}
