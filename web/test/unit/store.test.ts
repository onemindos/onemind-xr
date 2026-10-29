import { describe, expect, it } from "vitest";
import simX1 from "../fixtures/entity-sim-x1.json";
import { EntityStore, type Change } from "../../src/bus/store";

function clock(start = 1_793_275_200_000) {
  let t = start;
  return { now: () => t, set: (v: number) => (t = v), add: (ms: number) => (t += ms) };
}

const rec = (id: string, seq: number, extra: Record<string, unknown> = {}) => ({
  entity_id: id,
  entity_type: "drone",
  lat: 30.27,
  lon: -97.74,
  t: 1_793_275_200_000,
  stale: 10,
  expiry_t: 1_793_275_260_000,
  seq,
  live: true,
  ...extra,
});

describe("EntityStore merge rules (entity.md §5, layer-delta.md §2.2)", () => {
  it("applies the contract example and keeps the full record", () => {
    const c = clock();
    const s = new EntityStore(c.now);
    expect(s.apply(simX1, "fleet")).toBe("added");
    const r = s.get("sim-x1")!;
    expect(r.r.seq).toBe(42);
    expect(r.full?.provenance?.primary).toBe("bridge-mavlink");
    expect(r.r.simulated).toBe(true);
  });

  it("ignores a record whose seq is not greater than the one held", () => {
    const s = new EntityStore(clock().now);
    s.apply(rec("sim-x1", 5), "fleet");
    expect(s.apply(rec("sim-x1", 4, { lat: 1 }), "fleet")).toBe("ignored");
    expect(s.apply(rec("sim-x1", 5, { lat: 1 }), "fleet")).toBe("ignored");
    expect(s.get("sim-x1")!.r.lat).toBe(30.27);
    expect(s.apply(rec("sim-x1", 6, { lat: 31 }), "fleet")).toBe("updated");
    expect(s.get("sim-x1")!.r.lat).toBe(31);
    expect(s.stats.ignoredOlder).toBe(2);
  });

  it("removes on live:false and never resurrects from an older copy", () => {
    const s = new EntityStore(clock().now);
    s.apply(rec("sim-r1", 3), "fleet");
    expect(s.apply(rec("sim-r1", 4, { live: false }), "fleet")).toBe("removed");
    expect(s.get("sim-r1")).toBeUndefined();
    // A late snapshot copy with the older seq must not bring it back.
    expect(s.apply(rec("sim-r1", 3), "fleet")).toBe("ignored");
    expect(s.get("sim-r1")).toBeUndefined();
    // A genuinely newer record (the entity came back) is accepted.
    expect(s.apply(rec("sim-r1", 5), "fleet")).toBe("added");
  });

  it("expires entities when now passes expiry_t (core subscribers see no markers)", () => {
    const c = clock();
    const s = new EntityStore(c.now);
    s.apply(rec("sim-e1", 1, { expiry_t: c.now() + 6000 }), "fleet");
    s.apply(rec("sim-x1", 1, { expiry_t: c.now() + 60000 }), "fleet");
    c.add(5000);
    expect(s.expire()).toEqual([]);
    c.add(1001);
    expect(s.expire()).toEqual(["sim-e1"]);
    expect(s.size).toBe(1);
  });

  it("drops a record that is already expired on arrival", () => {
    const c = clock();
    const s = new EntityStore(c.now);
    expect(s.apply(rec("sim-old", 1, { expiry_t: c.now() - 1 }), "fleet")).toBe("ignored");
    expect(s.size).toBe(0);
  });

  it("keeps pinned entities (expiry_t null) forever", () => {
    const c = clock();
    const s = new EntityStore(c.now);
    s.apply(rec("zone-home", 1, { entity_type: "zone", expiry_t: null }), "zones");
    c.add(365 * 24 * 3600 * 1000);
    expect(s.expire()).toEqual([]);
  });

  it("marks stale when now - t > stale", () => {
    const c = clock();
    const s = new EntityStore(c.now);
    s.apply(rec("sim-x1", 1, { t: c.now(), stale: 10 }), "fleet");
    expect(s.isStale(s.get("sim-x1")!)).toBe(false);
    c.add(10_001);
    expect(s.isStale(s.get("sim-x1")!)).toBe(true);
  });

  it("replaceLayer removes entities missing from a new snapshot", () => {
    const s = new EntityStore(clock().now);
    s.apply(rec("a", 1), "fleet");
    s.apply(rec("b", 1), "fleet");
    s.apply(rec("z", 1, { entity_type: "zone" }), "zones");
    s.replaceLayer("fleet", [rec("a", 2)]);
    expect(s.get("a")!.r.seq).toBe(2);
    expect(s.get("b")).toBeUndefined();
    expect(s.get("z")).toBeDefined();
  });

  it("an AOI exit followed by enter with the same seq re-adds the entity", () => {
    const s = new EntityStore(clock().now);
    s.apply(rec("icao-a1", 7), "air");
    s.remove("icao-a1", "left_aoi");
    expect(s.apply(rec("icao-a1", 7), "air", { allowEqualSeq: true })).toBe("added");
  });

  it("counts invalid records and never applies them", () => {
    const s = new EntityStore(clock().now);
    expect(s.apply({ entity_type: "drone", seq: 1 }, "fleet")).toBe("invalid");
    expect(s.apply(rec("bad.id", 1), "fleet")).toBe("invalid");
    expect(s.apply({ ...rec("x", 1), seq: "1" }, "fleet")).toBe("invalid");
    expect(s.apply(null, "fleet")).toBe("invalid");
    expect(s.stats.invalid).toBe(4);
    expect(s.size).toBe(0);
  });

  it("batch() emits one change for many mutations", () => {
    const s = new EntityStore(clock().now);
    const changes: Change[] = [];
    s.onChange((c) => changes.push(c));
    s.batch(() => {
      s.apply(rec("a", 1), "fleet");
      s.apply(rec("b", 1), "fleet");
      s.remove("a", "deleted");
    });
    expect(changes).toHaveLength(1);
    expect(changes[0].upserted).toHaveLength(2);
    expect(changes[0].removed).toEqual([{ id: "a", reason: "deleted", layer: "fleet" }]);
  });

  it("attachFull does not replace a newer record with an older full one", () => {
    const s = new EntityStore(clock().now);
    s.apply(rec("sim-x1", 50), "fleet");
    s.attachFull({ ...(simX1 as object), seq: 42 } as never);
    expect(s.get("sim-x1")!.full).toBeUndefined();
    s.attachFull({ ...(simX1 as object), seq: 50 } as never);
    expect(s.get("sim-x1")!.full?.provenance?.how).toBe("m-s");
  });
});

describe("EntityStore: layer cap and older full records (review fixes)", () => {
  it("refuses new entities beyond a layer cap and keeps updating held ones", () => {
    const s = new EntityStore(clock().now);
    s.setLayerCap("air", 2);
    expect(s.apply(rec("a", 1), "air")).toBe("added");
    expect(s.apply(rec("b", 1), "air")).toBe("added");
    expect(s.apply(rec("c", 1), "air")).toBe("capped");
    expect(s.apply(rec("a", 2, { lat: 31 }), "air")).toBe("updated");
    expect(s.layerSize("air")).toBe(2);
    expect(s.stats.capped).toBe(1);
    s.remove("b", "left_aoi");
    expect(s.layerSize("air")).toBe(1);
    expect(s.apply(rec("c", 1), "air")).toBe("added");
    // Other layers are not capped.
    expect(s.apply(rec("d", 1), "fleet")).toBe("added");
  });

  it("marks the full record older when a render upsert with a higher seq arrives", () => {
    const s = new EntityStore(clock().now);
    s.apply(rec("sim-x1", 41), "fleet");
    s.attachFull(simX1 as never); // seq 42
    expect(s.get("sim-x1")!.fullStale).toBe(false);
    expect(s.apply(rec("sim-x1", 43, { lat: 30.3 }), "fleet")).toBe("updated");
    const r = s.get("sim-x1")!;
    expect(r.r.lat).toBe(30.3);
    expect(r.full?.seq).toBe(42);
    expect(r.fullStale).toBe(true);
    s.attachFull({ ...(simX1 as object), seq: 43 } as never);
    expect(s.get("sim-x1")!.fullStale).toBe(false);
    // A full record on the feed is never older than itself.
    expect(s.apply({ ...(simX1 as object), seq: 44 }, "fleet")).toBe("updated");
    expect(s.get("sim-x1")!.fullStale).toBe(false);
  });
});
