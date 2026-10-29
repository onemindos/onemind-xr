// F-ALERT (onemind-xr INT E2E-2): open alerts come from the geofence and alert service
// (svc.geofence.alerts + evt.so.alert.*), never from ent.alert.*.state.
import { describe, expect, it, vi } from "vitest";
import { AlertFeed, AlertStore, alertFromEnvelope, type ServiceAlert } from "../../src/bus/alertfeed";
import { alertFromGeofence, alertFromService, dedupeAgainstService, type AlertItem } from "../../src/bus/alerts";
import type { Bus } from "../../src/bus/layerclient";

const enc = new TextEncoder();
type Cb = (err: Error | null, m: { subject: string; data: Uint8Array }) => void;

// The alert of onemind-xr docs/evidence/INT/E2E-2/run1/results.json step 4 (real #569 output).
const E2E: ServiceAlert = {
  about: { entity_id: "sim-x1", entity_type: "drone" }, alert_id: "alr-gf-zone-e2e-pad-sim-x1", alert_kind: "geofence",
  level: "warning", location: { hae: 6.73, lat: 30.267085, lon: -97.7404688 }, message: "SIM-X1 is outside keep-in zone zone-e2e-pad",
  opened_t: 1793293303011, raised_by: "omos-geofence", reason: "left_keep_in", seq: 1, simulated: true, state: "open",
  updated_t: 1793293303011, v: 1, zone_id: "zone-e2e-pad", zone_kind: "keep_in",
} as ServiceAlert;
const env = (subject: string, data: unknown) => ({ data, id: "x", source: "omos-geofence", subject, ts: "", type: subject, v: 1 });

class Fake {
  subs: { subject: string; cb: Cb }[] = [];
  requests: string[] = [];
  reply: () => Promise<unknown> = async () => ({ v: 1, alerts: [] });
  denySub = false;
  bus: Bus = {
    publish: () => undefined,
    subscribe: ((subject: string, o: { callback: Cb }) => {
      this.subs.push({ subject, cb: o.callback });
      if (this.denySub) queueMicrotask(() => o.callback(new Error(`Permissions Violation for Subscription to "${subject}"`), { subject: "", data: new Uint8Array() }));
      return { unsubscribe: () => undefined };
    }) as unknown as Bus["subscribe"],
    request: (async (subject: string) => {
      this.requests.push(subject);
      const body = await this.reply();
      return { data: enc.encode(JSON.stringify(body)) };
    }) as unknown as Bus["request"],
  };
  emit(subject: string, body: unknown) {
    for (const s of this.subs) s.cb(null, { subject, data: enc.encode(JSON.stringify(body)) });
  }
}
const flush = () => new Promise((r) => setTimeout(r, 0));

describe("AlertFeed", () => {
  it("subscribes evt.so.alert.> and asks svc.geofence.alerts, never ent.*", async () => {
    const f = new Fake();
    f.reply = async () => ({ v: 1, alerts: [E2E] });
    const feed = new AlertFeed(f.bus);
    feed.start();
    await flush();
    expect(f.subs.map((s) => s.subject)).toEqual(["evt.so.alert.>"]);
    expect(f.requests).toEqual(["svc.geofence.alerts"]);
    expect(feed.alerts()).toHaveLength(1);
    expect(feed.status).toMatchObject({ snapshot: "ok" });
    feed.stop();
  });

  it("applies live raised/updated/resolved by seq; a stale snapshot does not resurrect a resolved alert", async () => {
    const f = new Fake();
    let release!: (v: unknown) => void;
    f.reply = () => new Promise((r) => (release = r));
    const feed = new AlertFeed(f.bus);
    feed.start();
    f.emit("evt.so.alert.raised", env("evt.so.alert.raised", E2E));
    expect(feed.alerts()[0].seq).toBe(1);
    f.emit("evt.so.alert.updated", env("evt.so.alert.updated", { ...E2E, seq: 2, level: "caution" }));
    f.emit("evt.so.alert.raised", env("evt.so.alert.raised", E2E)); // redelivered older fact
    expect(feed.alerts()[0]).toMatchObject({ seq: 2, level: "caution" });
    f.emit("evt.so.alert.resolved", env("evt.so.alert.resolved", { ...E2E, seq: 3, state: "resolved", reason: "returned" }));
    expect(feed.alerts()).toHaveLength(0);
    release({ v: 1, alerts: [{ ...E2E, seq: 2 }] }); // snapshot taken before the resolve
    await flush();
    expect(feed.alerts()).toHaveLength(0);
    expect(feed.status.live).toBe("ok");
    feed.stop();
  });

  it("drops an alert the snapshot no longer lists (resolved while away)", () => {
    const st = new AlertStore();
    st.apply(E2E);
    expect(st.reconcile([], 10, new Map())).toBe(true);
    expect(st.list()).toHaveLength(0);
    // ...unless a live fact newer than the request arrived meanwhile.
    st.apply({ ...E2E, alert_id: "alr-gf-b", seq: 1 });
    expect(st.reconcile([], 10, new Map([["alr-gf-b", 11]]))).toBe(false);
    expect(st.list()).toHaveLength(1);
  });

  it("polls when the live subscription is refused (older #562 render)", async () => {
    vi.useFakeTimers();
    try {
      const f = new Fake();
      f.denySub = true;
      const feed = new AlertFeed(f.bus, { pollMs: 1000, refreshMs: 60_000 });
      feed.start();
      await vi.advanceTimersByTimeAsync(10);
      expect(feed.status.live).toBe("denied");
      await vi.advanceTimersByTimeAsync(3100);
      expect(f.requests.length).toBeGreaterThanOrEqual(3);
      feed.stop();
    } finally {
      vi.useRealTimers();
    }
  });

  it("shows nothing and backs off when the server refuses the snapshot (J2 newcomer role)", async () => {
    vi.useFakeTimers();
    try {
      const f = new Fake();
      f.denySub = true;
      f.reply = async () => {
        throw new Error('Permissions Violation for Publish to "svc.geofence.alerts"');
      };
      const feed = new AlertFeed(f.bus, { pollMs: 1000, deniedRetryMs: 300_000 });
      feed.start();
      // A forged fact cannot arrive (the subscription is refused); the store stays empty.
      await vi.advanceTimersByTimeAsync(60_000);
      expect(feed.status).toMatchObject({ snapshot: "denied", live: "denied" });
      expect(feed.alerts()).toHaveLength(0);
      expect(f.requests).toHaveLength(1);
      feed.stop();
    } finally {
      vi.useRealTimers();
    }
  });

  it("reads the envelope's data and ignores records without alert_id/seq", () => {
    expect(alertFromEnvelope(env("evt.so.alert.raised", E2E))?.alert_id).toBe(E2E.alert_id);
    expect(alertFromEnvelope(E2E)?.alert_id).toBe(E2E.alert_id);
    expect(alertFromEnvelope({ data: { alert_id: "x" } })).toBeNull();
    expect(alertFromEnvelope(null)).toBeNull();
  });
});

describe("service alerts in the list", () => {
  it("maps alert.v1 to a list item", () => {
    expect(alertFromService(E2E)).toMatchObject({
      key: "alert:alr-gf-zone-e2e-pad-sim-x1", level: "warning", kind: "geofence · left keep in", title: "SIM-X1 is outside keep-in zone zone-e2e-pad",
      about: "sim-x1", source: "service", zone: "zone-e2e-pad", t: 1793293303011,
    });
    expect(alertFromService({ ...E2E, state: "resolved" })).toBeNull();
  });

  it("drops client-derived duplicates: the same crossing, stale asset or feed", () => {
    const svc = [
      alertFromService(E2E)!,
      alertFromService({ ...E2E, alert_id: "alr-stale-drone-sim-x2", alert_kind: "stale_asset", reason: "stale", about: { entity_id: "sim-x2" }, zone_id: undefined })!,
      alertFromService({ ...E2E, alert_id: "alr-feed-nws-alerts", alert_kind: "feed_outage", reason: "silent", about: { feed: "nws-alerts" }, zone_id: undefined })!,
    ];
    // #569 crossings come in an envelope.
    const crossing = alertFromGeofence("evt.map.geofence.zone-e2e-pad.exit", { data: { entity_id: "sim-x1", zone_id: "zone-e2e-pad" } }, 5)!;
    expect(crossing).toMatchObject({ about: "sim-x1", zone: "zone-e2e-pad" });
    const other = alertFromGeofence("evt.map.geofence.zone-b.enter", { entity_id: "sim-x1" }, 5)!;
    const derived: AlertItem[] = [
      crossing, other,
      { key: "stale:sim-x2", level: "caution", kind: "stale asset", title: "", about: "sim-x2", t: 1, source: "stale", state: "open" },
      { key: "stale:sim-x3", level: "caution", kind: "stale asset", title: "", about: "sim-x3", t: 1, source: "stale", state: "open" },
      { key: "feed:nws", level: "caution", kind: "feed outage", title: "", t: 1, source: "feed", state: "open", feed: "nws-alerts" },
      { key: "health:x", level: "caution", kind: "gps", title: "", about: "sim-x1", t: 1, source: "health", state: "open" },
    ];
    expect(dedupeAgainstService(derived, svc).map((a) => a.key)).toEqual([other.key, "stale:sim-x3", "health:x"]);
  });
});
