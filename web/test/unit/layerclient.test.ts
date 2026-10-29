import { NoRespondersError } from "@nats-io/nats-core";
import { afterEach, describe, expect, it } from "vitest";
import { DEFAULT_LAYERS } from "../../src/bus/catalog";
import { bboxSpan, insideBbox, LayerFeed, type Bus } from "../../src/bus/layerclient";
import { EntityStore } from "../../src/bus/store";

// In-memory bus with NATS wildcard matching, reply subjects and 503 no-responders statuses.
type Handler = (err: Error | null, m: FakeMsg) => void;
interface FakeMsg {
  subject: string;
  reply?: string;
  data: Uint8Array;
  headers?: { code: number; status: string; get(k: string): string };
  json(): unknown;
  respond(d: Uint8Array): boolean;
}
const enc = new TextEncoder();
const dec = new TextDecoder();
const match = (pat: string, subj: string) => {
  const p = pat.split(".");
  const s = subj.split(".");
  for (let i = 0; i < p.length; i++) {
    if (p[i] === ">") return true;
    if (i >= s.length || (p[i] !== "*" && p[i] !== s[i])) return false;
  }
  return p.length === s.length;
};

class FakeBus {
  subs: { pat: string; cb: Handler; dead: boolean }[] = [];
  published: { subject: string; body: unknown }[] = [];
  msg(subject: string, data: Uint8Array, reply?: string, headers?: FakeMsg["headers"]): FakeMsg {
    return {
      subject,
      reply,
      data,
      headers,
      json: () => JSON.parse(dec.decode(data)),
      respond: (d) => {
        if (reply) this.deliver(reply, d);
        return true;
      },
    };
  }
  deliver(subject: string, data: Uint8Array, reply?: string): number {
    const hits = this.subs.filter((s) => !s.dead && match(s.pat, subject));
    for (const s of hits) s.cb(null, this.msg(subject, data, reply));
    return hits.length;
  }
  readonly bus: Bus = {
    publish: ((subject: string, data?: Uint8Array, opts?: { reply?: string }) => {
      const d = data ?? new Uint8Array();
      try {
        this.published.push({ subject, body: JSON.parse(dec.decode(d)) });
      } catch {
        this.published.push({ subject, body: null });
      }
      const n = this.deliver(subject, d, opts?.reply);
      if (n === 0 && opts?.reply) {
        const h = { code: 503, status: "503", get: () => "" };
        for (const s of this.subs.filter((x) => !x.dead && match(x.pat, opts.reply!))) {
          s.cb(null, this.msg(opts.reply, new Uint8Array(), undefined, h));
        }
      }
    }) as Bus["publish"],
    subscribe: ((pat: string, opts?: { callback?: Handler }) => {
      const s = { pat, cb: opts!.callback!, dead: false };
      this.subs.push(s);
      return { unsubscribe: () => (s.dead = true) };
    }) as unknown as Bus["subscribe"],
    request: ((subject: string, data?: Uint8Array) =>
      new Promise((resolve, reject) => {
        const hits = this.subs.filter((s) => !s.dead && match(s.pat, subject));
        if (!hits.length) return reject(new NoRespondersError(subject));
        hits[0].cb(null, {
          ...this.msg(subject, data ?? new Uint8Array(), "_INBOX.req"),
          respond: (d: Uint8Array) => {
            resolve(this.msg("_INBOX.req", d) as never);
            return true;
          },
        });
      })) as Bus["request"],
  };
  /** Register a JSON responder. */
  serve(subject: string, fn: (q: Record<string, unknown>, m: FakeMsg) => void) {
    this.subs.push({ pat: subject, dead: false, cb: (_e, m) => fn(m.json() as Record<string, unknown>, m) });
  }
}

const fleet = DEFAULT_LAYERS.find((l) => l.id === "fleet")!;
const ent = (id: string, seq: number, extra: Record<string, unknown> = {}) => ({
  entity_id: id, entity_type: "drone", lat: 30.27, lon: -97.74, t: Date.now(), stale: 10,
  expiry_t: Date.now() + 60_000, seq, live: true, ...extra,
});
const js = (o: unknown) => enc.encode(JSON.stringify(o));
const wait = (ms: number) => new Promise((r) => setTimeout(r, ms));
const feeds: LayerFeed[] = [];
afterEach(() => feeds.splice(0).forEach((f) => f.stop()));

function layerService(fb: FakeBus, items: unknown[], entSeq = 100) {
  const watches: { inbox: string; q: Record<string, unknown> }[] = [];
  let snapshots = 0;
  fb.serve("svc.layer.snapshot", (q, m) => {
    snapshots++;
    expect(q).toMatchObject({ v: 1, layer: "fleet", fields: "full", aoi: null });
    m.respond(js({ v: 1, layer: "fleet", part: 0, last: false, mode: "entities", ent_seq: entSeq, as_of: 1, items: items.slice(0, 1) }));
    m.respond(js({ v: 1, layer: "fleet", part: 1, last: true, mode: "entities", ent_seq: entSeq, as_of: 1, items: items.slice(1) }));
  });
  fb.serve("svc.layer.watch", (q, m) => {
    watches.push({ inbox: m.reply!, q });
    m.respond(js({ v: 1, watch_id: "w-1", lease_s: q.lease_s, ent_seq: entSeq }));
  });
  return {
    watches,
    snapshots: () => snapshots,
    batch: (n: number, ops: unknown[]) => fb.deliver(watches[watches.length - 1].inbox, js({ v: 1, watch_id: "w-1", layer: "fleet", n, ops, meta: null })),
  };
}

describe("LayerFeed: svc.layer path (layer-snapshot.md §3.1, layer-delta.md §2)", () => {
  it("assembles a multi-part snapshot, watches from ent_seq + 1 and applies ops", async () => {
    const fb = new FakeBus();
    const svc = layerService(fb, [ent("sim-x1", 1), ent("sim-r1", 1)], 100);
    const store = new EntityStore();
    const f = new LayerFeed(fb.bus, store, fleet);
    feeds.push(f);
    expect(await f.start()).toBe("layer");
    expect(store.size).toBe(2);
    expect(svc.watches[0].q).toMatchObject({ from_seq: 101, lease_s: 60, watch_id: null });
    svc.batch(1, [
      { op: "upsert", e: ent("sim-x1", 2, { lat: 31 }) },
      { op: "remove", id: "sim-r1", reason: "deleted" },
      { op: "enter", e: ent("sim-v1", 1) },
    ]);
    expect(store.get("sim-x1")!.r.lat).toBe(31);
    expect(store.get("sim-r1")).toBeUndefined();
    expect(store.get("sim-v1")).toBeDefined();
    svc.batch(2, [{ op: "exit", id: "sim-v1", reason: "left_aoi" }, { op: "upsert", e: ent("sim-x1", 1, { lat: 0 }) }]);
    expect(store.get("sim-v1")).toBeUndefined();
    expect(store.get("sim-x1")!.r.lat).toBe(31); // older seq ignored
    svc.batch(3, []); // heartbeat
    expect(f.status.mode).toBe("layer");
    expect(f.status.resnapshots).toBe(0);
  });

  it("re-snapshots when a batch counter skips (gap)", async () => {
    const fb = new FakeBus();
    const svc = layerService(fb, [ent("sim-x1", 1)]);
    const f = new LayerFeed(fb.bus, new EntityStore(), fleet);
    feeds.push(f);
    await f.start();
    svc.batch(1, []);
    svc.batch(3, []);
    await wait(20);
    expect(f.status.resnapshots).toBe(1);
    expect(svc.snapshots()).toBe(2);
    expect(svc.watches).toHaveLength(2);
  });

  it("re-snapshots when no batch or heartbeat arrives within the dead-watch time", async () => {
    const fb = new FakeBus();
    const svc = layerService(fb, [ent("sim-x1", 1)]);
    const f = new LayerFeed(fb.bus, new EntityStore(), fleet, { deadWatchMs: 80 });
    feeds.push(f);
    await f.start();
    await wait(200);
    expect(f.status.resnapshots).toBeGreaterThanOrEqual(1);
    expect(svc.snapshots()).toBeGreaterThanOrEqual(2);
  });

  it("renews the lease at half of lease_s with the same watch_id", async () => {
    const fb = new FakeBus();
    const svc = layerService(fb, []);
    const f = new LayerFeed(fb.bus, new EntityStore(), fleet, { leaseS: 0.1, deadWatchMs: 10_000 });
    feeds.push(f);
    await f.start();
    await wait(130);
    const renewals = svc.watches.filter((w) => w.q.watch_id === "w-1");
    expect(renewals.length).toBeGreaterThanOrEqual(1);
  });

  it("asks large layers for an AOI and the render subset", async () => {
    const fb = new FakeBus();
    let seen: Record<string, unknown> = {};
    fb.serve("svc.layer.snapshot", (q, m) => {
      seen = q;
      m.respond(js({ v: 1, layer: "air", part: 0, last: true, mode: "entities", ent_seq: 1, as_of: 1, items: [] }));
    });
    fb.serve("svc.layer.watch", (q, m) => m.respond(js({ v: 1, watch_id: "w-a", lease_s: q.lease_s, ent_seq: 1 })));
    const air = DEFAULT_LAYERS.find((l) => l.id === "air")!;
    const f = new LayerFeed(fb.bus, new EntityStore(), air, { aoi: { bbox: [-74.3, 40.5, -73.6, 41] }, zoom: 9 });
    feeds.push(f);
    await f.start();
    expect(seen).toMatchObject({ layer: "air", fields: "render", zoom: 9, aoi: { bbox: [-74.3, 40.5, -73.6, 41] } });
  });
});

describe("LayerFeed: interim and core paths (entity.md §8)", () => {
  it("falls back to svc.entity.query (paged by cursor) plus core ent.* when no layer service answers", async () => {
    const fb = new FakeBus();
    const all = [ent("sim-a", 1), ent("sim-b", 1), ent("sim-c", 1)];
    const queries: Record<string, unknown>[] = [];
    fb.serve("svc.entity.query", (q, m) => {
      queries.push(q);
      const rest = all.filter((e) => !q.cursor || e.entity_id > String(q.cursor));
      const page = rest.slice(0, 1);
      m.respond(js({ entities: page, cursor: rest.length > 1 ? page[0].entity_id : null }));
    });
    const store = new EntityStore();
    const f = new LayerFeed(fb.bus, store, fleet);
    feeds.push(f);
    expect(await f.start()).toBe("interim");
    expect(store.size).toBe(3);
    expect(queries.map((q) => q.cursor)).toEqual(["", "sim-a", "sim-b"]);
    expect(queries[0].types).toEqual(["drone", "robot", "vehicle", "node"]);
    // Live core records follow; live:false removes.
    fb.deliver("ent.drone.sim-a.state", js(ent("sim-a", 2, { lat: 32 })));
    fb.deliver("ent.robot.sim-b.state", js(ent("sim-b", 2, { live: false })));
    expect(store.get("sim-a")!.r.lat).toBe(32);
    expect(store.get("sim-b")).toBeUndefined();
    // Subscriptions are per entity type, inside the locked grammar.
    expect(fb.subs.filter((s) => !s.dead).map((s) => s.pat)).toEqual(
      expect.arrayContaining(["ent.drone.*.state", "ent.robot.*.state", "ent.vehicle.*.state", "ent.node.*.state"]),
    );
  });

  it("uses core subscriptions only when no service answers at all", async () => {
    const fb = new FakeBus();
    const store = new EntityStore();
    const f = new LayerFeed(fb.bus, store, fleet);
    feeds.push(f);
    expect(await f.start()).toBe("core");
    fb.deliver("ent.drone.sim-x1.state", js(ent("sim-x1", 1)));
    fb.deliver("ent.drone.sim-x1.state", enc.encode("{not json"));
    expect(store.size).toBe(1);
    expect(store.stats.invalid).toBe(1);
  });

  it("an error reply from the layer service falls back to the interim path", async () => {
    const fb = new FakeBus();
    fb.serve("svc.layer.snapshot", (_q, m) => m.respond(js({ v: 1, error: "layer_down", detail: "ENT unreadable" })));
    const f = new LayerFeed(fb.bus, new EntityStore(), fleet);
    feeds.push(f);
    expect(await f.start()).toBe("core");
  });

  it("filters large layers to the AOI on the interim path", async () => {
    const fb = new FakeBus();
    const store = new EntityStore();
    const air = DEFAULT_LAYERS.find((l) => l.id === "air")!;
    const f = new LayerFeed(fb.bus, store, air, { aoi: { bbox: [-75, 40, -73, 41] } });
    feeds.push(f);
    await f.start();
    fb.deliver("ent.aircraft.icao-in.state", js(ent("icao-in", 1, { entity_type: "aircraft", lat: 40.5, lon: -74 })));
    fb.deliver("ent.aircraft.icao-out.state", js(ent("icao-out", 1, { entity_type: "aircraft", lat: 10, lon: 10 })));
    expect(store.get("icao-in")).toBeDefined();
    expect(store.get("icao-out")).toBeUndefined();
    f.setAoi({ bbox: [0, 0, 20, 20] });
    expect(store.get("icao-in")).toBeUndefined();
  });

  it("never publishes ent.* or cmd.* (entity.md §2)", async () => {
    const fb = new FakeBus();
    layerService(fb, [ent("sim-x1", 1)]);
    const f = new LayerFeed(fb.bus, new EntityStore(), fleet);
    feeds.push(f);
    await f.start();
    f.stop();
    expect(fb.published.every((p) => p.subject.startsWith("svc."))).toBe(true);
  });
});

describe("LayerFeed: large layers stay AOI-bounded (layer-snapshot.md §1, FEATURES A14)", () => {
  const air = DEFAULT_LAYERS.find((l) => l.id === "air")!;
  const aircraft = (id: string, seq: number, lat: number, lon: number) =>
    ent(id, seq, { entity_type: "aircraft", lat, lon });
  const entSubs = (fb: FakeBus) => fb.subs.filter((s) => !s.dead && s.pat.startsWith("ent."));

  it("cells mode at wide zoom: zoom in, keep the watch, never subscribe to raw ent.*", async () => {
    const fb = new FakeBus();
    const watches: { inbox: string; q: Record<string, unknown> }[] = [];
    fb.serve("svc.layer.snapshot", (_q, m) =>
      m.respond(js({ v: 1, layer: "air", part: 0, last: true, mode: "cells", ent_seq: 7, as_of: 1, items: [[1, 2, 900, 40, -74]] })),
    );
    fb.serve("svc.layer.watch", (q, m) => {
      watches.push({ inbox: m.reply!, q });
      m.respond(js({ v: 1, watch_id: "w-a", lease_s: q.lease_s, ent_seq: 7 }));
    });
    const store = new EntityStore();
    const f = new LayerFeed(fb.bus, store, air, { aoi: { bbox: [-170, -80, 170, 80] }, zoom: 3 });
    feeds.push(f);
    expect(await f.start()).toBe("layer");
    expect(f.status.detail).toMatch(/zoom in/);
    expect(store.size).toBe(0);
    expect(entSubs(fb)).toHaveLength(0);
    expect(watches[0].q).toMatchObject({ from_seq: 8, zoom: 3 });
    // Zooming in re-sends the watch with the new AOI; the service answers with enter ops.
    f.setAoi({ bbox: [-74.3, 40.5, -73.6, 41] }, 10);
    expect(watches.at(-1)!.q).toMatchObject({ watch_id: "w-a", zoom: 10, aoi: { bbox: [-74.3, 40.5, -73.6, 41] } });
    fb.deliver(watches[0].inbox, js({ v: 1, watch_id: "w-a", layer: "air", n: 1, mode: "entities", ops: [{ op: "enter", e: aircraft("icao-1", 3, 40.7, -74) }] }));
    expect(store.get("icao-1")).toBeDefined();
    expect(f.status.detail).toBeUndefined();
  });

  it("aoi_required (or any service error) keeps a large layer on the layer path and retries", async () => {
    const fb = new FakeBus();
    let asks = 0;
    fb.serve("svc.layer.snapshot", (q, m) => {
      asks++;
      if (!q.aoi) return m.respond(js({ v: 1, error: "aoi_required", detail: "large layer" }));
      m.respond(js({ v: 1, layer: "air", part: 0, last: true, mode: "entities", ent_seq: 1, as_of: 1, items: [aircraft("icao-1", 1, 40.7, -74)] }));
    });
    fb.serve("svc.layer.watch", (q, m) => m.respond(js({ v: 1, watch_id: "w-a", lease_s: q.lease_s, ent_seq: 1 })));
    const store = new EntityStore();
    const f = new LayerFeed(fb.bus, store, air, { aoi: null, retryMs: 30 });
    feeds.push(f);
    expect(await f.start()).toBe("error");
    expect(f.status.detail).toMatch(/aoi_required/);
    expect(entSubs(fb)).toHaveLength(0);
    await wait(80);
    expect(asks).toBeGreaterThanOrEqual(2); // retried on the layer path
    expect(entSubs(fb)).toHaveLength(0);
    // A bounded view arrives: the feed retries at once and lands on the watch.
    f.setAoi({ bbox: [-74.3, 40.5, -73.6, 41] }, 10);
    await wait(30);
    expect(f.status.mode).toBe("layer");
    expect(store.get("icao-1")).toBeDefined();
  });

  it("with no layer responders, refuses raw ent.* until the AOI is bounded, then caps the store", async () => {
    const fb = new FakeBus();
    const store = new EntityStore();
    const f = new LayerFeed(fb.bus, store, air, { aoi: { bbox: [-180, -85, 180, 85] }, maxEntities: 2 });
    feeds.push(f);
    expect(await f.start()).toBe("core");
    expect(entSubs(fb)).toHaveLength(0);
    expect(f.status.detail).toMatch(/zoom in/);
    f.setAoi({ bbox: [-75, 40, -73, 41] }, 10);
    await wait(10);
    expect(entSubs(fb).map((s) => s.pat)).toEqual(["ent.aircraft.*.state"]);
    for (let i = 0; i < 4; i++) fb.deliver(`ent.aircraft.icao-${i}.state`, js(aircraft(`icao-${i}`, 1, 40.5, -74)));
    expect(store.layerSize("air")).toBe(2);
    expect(f.status.detail).toMatch(/cap 2/);
    // Zooming back out closes the raw subscription and clears the layer.
    f.setAoi({ bbox: [-120, 10, -60, 60] }, 4);
    await wait(10);
    expect(entSubs(fb)).toHaveLength(0);
    expect(store.layerSize("air")).toBe(0);
  });

  it("handles a view across the antimeridian (west > east)", async () => {
    expect(insideBbox(10, 179, [170, 0, -170, 20])).toBe(true);
    expect(insideBbox(10, -175, [170, 0, -170, 20])).toBe(true);
    expect(insideBbox(10, 0, [170, 0, -170, 20])).toBe(false);
    expect(bboxSpan([170, 0, -170, 20])).toEqual({ dLon: 20, dLat: 20 });
    const fb = new FakeBus();
    const store = new EntityStore();
    const f = new LayerFeed(fb.bus, store, air, { aoi: { bbox: [175, 0, -175, 8] } });
    feeds.push(f);
    expect(await f.start()).toBe("core");
    fb.deliver("ent.aircraft.icao-e.state", js(aircraft("icao-e", 1, 5, 178)));
    fb.deliver("ent.aircraft.icao-w.state", js(aircraft("icao-w", 1, 5, -178)));
    expect(store.layerSize("air")).toBe(2);
  });
});

describe("LayerFeed: stop() is honoured mid-flight (review fixes)", () => {
  it("a stop during a re-snapshot opens no new watch and renews nothing", async () => {
    const fb = new FakeBus();
    let hold: (() => void) | null = null;
    let snaps = 0;
    const watchReqs: Record<string, unknown>[] = [];
    fb.serve("svc.layer.snapshot", (_q, m) => {
      snaps++;
      const reply = () => m.respond(js({ v: 1, layer: "fleet", part: 0, last: true, mode: "entities", ent_seq: 5, as_of: 1, items: [ent("sim-a", snaps)] }));
      if (snaps === 1) reply();
      else hold = reply;
    });
    fb.serve("svc.layer.watch", (q, m) => {
      watchReqs.push(q);
      m.respond(js({ v: 1, watch_id: `w-${watchReqs.length}`, lease_s: 0.05, ent_seq: 5 }));
    });
    const store = new EntityStore();
    const f = new LayerFeed(fb.bus, store, fleet, { leaseS: 0.05 });
    expect(await f.start()).toBe("layer");
    const inbox = fb.subs.filter((s) => !s.dead && s.pat.startsWith("_INBOX")).at(-1)!.pat;
    fb.deliver(inbox, js({ v: 1, watch_id: "w-1", layer: "fleet", n: 1, ops: [] }));
    fb.deliver(inbox, js({ v: 1, watch_id: "w-1", layer: "fleet", n: 3, ops: [] })); // gap -> re-snapshot
    await wait(5);
    // The old watch was released before the new snapshot.
    expect(fb.published.some((p) => p.subject === "svc.layer.unwatch" && (p.body as { watch_id: string }).watch_id === "w-1")).toBe(true);
    f.stop();
    store.remove("sim-a", "left_aoi");
    hold!();
    await wait(120);
    const before = watchReqs.length;
    await wait(120);
    expect(watchReqs.length).toBe(before);
    expect(watchReqs.filter((q) => q.watch_id === null)).toHaveLength(1); // no second watch
    expect(store.get("sim-a")).toBeUndefined(); // the late snapshot was not applied
    expect(f.status.mode).toBe("stopped");
  });

  it("a stop during the interim query loop applies no further pages", async () => {
    const fb = new FakeBus();
    let page = 0;
    let f!: LayerFeed;
    fb.serve("svc.entity.query", (_q, m) => {
      page++;
      if (page === 1) f.stop();
      m.respond(js({ entities: [ent(`sim-${page}`, 1)], cursor: page < 5 ? `c${page}` : null }));
    });
    const store = new EntityStore();
    f = new LayerFeed(fb.bus, store, fleet);
    expect(await f.start()).toBe("stopped");
    expect(page).toBe(1);
    expect(store.size).toBe(0);
  });
});
