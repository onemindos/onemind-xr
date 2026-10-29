import { describe, expect, it } from "vitest";
import simX1 from "../fixtures/entity-sim-x1.json";
import { alertFromEntity, alertFromGeofence, alertsFromHealth, sortAlerts } from "../../src/bus/alerts";
import { haversineM, inMissionTier, isTasked, pointInRing, zoneContains } from "../../src/bus/missiontier";
import { isLocatable, toRender, viewerMaySee } from "../../src/bus/normalize";
import type { EntityRecord } from "../../src/bus/store";
import type { Entity } from "../../src/bus/types";
import { playbackUrls } from "../../src/bus/videourl";

const asRec = (full: Record<string, unknown>, layer = "fleet"): EntityRecord => ({
  r: toRender(full)!,
  full: full as Entity,
  layer,
  receivedAt: 0,
});

describe("normalize (entity.md §4 to layer-snapshot.md §4)", () => {
  it("maps the contract example to the render subset", () => {
    const r = toRender(simX1)!;
    expect(r).toMatchObject({
      entity_id: "sim-x1",
      entity_type: "drone",
      lat: 30.2672,
      lon: -97.7431,
      hae: 152.4,
      course: 90,
      speed: 12.5,
      callsign: "SIM-X1",
      cot_type: "a-f-A-M-H-Q",
      disposition: "friendly",
      seq: 42,
      live: true,
      simulated: true,
      expiry_t: 1793275260000,
    });
  });

  it("passes a render entity through unchanged", () => {
    const render = { entity_id: "icao-a1b2c3", entity_type: "aircraft", lat: 40.7, lon: -73.9, t: 1, seq: 9, live: true, stale: 30, expiry_t: 5, simulated: false };
    expect(toRender(render)).toMatchObject(render);
  });

  it("uses uid when entity_id is missing and falls back to the id as callsign", () => {
    expect(toRender({ uid: "sim-x9", seq: 1, lat: 1, lon: 2, t: 0 })).toMatchObject({ entity_id: "sim-x9", callsign: "sim-x9" });
  });

  it("does not draw non-located records (lat 0, lon 0 without locatable)", () => {
    const alert = { entity_id: "alr-1", entity_type: "alert", lat: 0, lon: 0, seq: 1, t: 0, traits: [] };
    expect(isLocatable(toRender(alert)!, alert as never)).toBe(false);
    const drone = toRender(simX1)!;
    expect(isLocatable(drone, simX1 as never)).toBe(true);
    const zone = { entity_id: "zone-a", entity_type: "zone", lat: 0, lon: 0, seq: 1, t: 0, traits: [], geo: { shape: "polygon", coords: { type: "Polygon", coordinates: [] } } };
    expect(isLocatable(toRender(zone)!, zone as never)).toBe(true);
    expect(isLocatable({ ...drone, lat: 0, lon: 0 })).toBe(false);
  });

  it("applies the defence-in-depth marking check (entity.md §8)", () => {
    expect(viewerMaySee(undefined, [])).toBe(true);
    expect(viewerMaySee({ tlp: "CLEAR" }, [])).toBe(true);
    expect(viewerMaySee({ tlp: "GREEN", releasable_to: ["x"] }, [])).toBe(true);
    expect(viewerMaySee({ tlp: "AMBER+STRICT", releasable_to: ["homestead"] }, ["homestead"])).toBe(true);
    expect(viewerMaySee({ tlp: "AMBER+STRICT", releasable_to: ["team-b"] }, ["homestead"])).toBe(false);
    expect(viewerMaySee({ tlp: "AMBER", releasable_to: [] }, [])).toBe(true);
    expect(viewerMaySee({ tlp: "RED", releasable_to: [] }, ["homestead"])).toBe(false);
  });
});

describe("mission tier (FEATURES A6 rule as a view filter)", () => {
  const ring = [[-97.7509, 30.2604], [-97.7353, 30.2604], [-97.7353, 30.274], [-97.7509, 30.274], [-97.7509, 30.2604]];
  const zone = asRec({ entity_id: "zone-home", entity_type: "zone", lat: 30.2672, lon: -97.7431, seq: 1, t: 0, geo: { shape: "polygon", kind: "aoi", coords: { type: "Polygon", coordinates: [ring] } } }, "zones");

  it("point in polygon and circle", () => {
    expect(pointInRing(-97.7431, 30.2672, ring)).toBe(true);
    expect(pointInRing(-97.70, 30.2672, ring)).toBe(false);
    const circle = { geo: { shape: "circle", radius_m: 500, coords: { type: "Point", coordinates: [-97.7431, 30.2672] } } } as Entity;
    expect(zoneContains(circle, 30.2672 + 0.004, -97.7431)).toBe(true); // about 445 m north
    expect(zoneContains(circle, 30.2672 + 0.005, -97.7431)).toBe(false); // about 556 m
    expect(Math.round(haversineM(0, 0, 1, 0))).toBe(111195);
  });

  it("tasked or inside an AOI is mission tier; outside and untasked is not", () => {
    expect(isTasked(simX1 as never)).toBe(true);
    expect(isTasked({ current_task: { task_id: "t", state: "succeeded" } } as never)).toBe(false);
    const outside = asRec({ entity_id: "sim-v1", entity_type: "vehicle", lat: 30.29, lon: -97.70, seq: 1, t: 0 });
    const inside = asRec({ entity_id: "trk-1", entity_type: "track", lat: 30.265, lon: -97.745, seq: 1, t: 0 });
    expect(inMissionTier(outside, [zone])).toBe(false);
    expect(inMissionTier(inside, [zone])).toBe(true);
    expect(inMissionTier(asRec(simX1 as never), [])).toBe(true);
    expect(inMissionTier(zone, [zone])).toBe(true);
    expect(inMissionTier(asRec({ entity_id: "alr-1", entity_type: "alert", lat: 0, lon: 0, seq: 1, t: 0 }), [])).toBe(true);
  });
});

describe("alerts", () => {
  it("reads alert entities (top level, then properties) and hides resolved ones", () => {
    const top = asRec({ entity_id: "alr-1", entity_type: "alert", lat: 0, lon: 0, seq: 1, t: 5, alert_kind: "low_battery", level: "warning", about: "sim-x1", state: "open" });
    expect(alertFromEntity(top)).toMatchObject({ level: "warning", kind: "low_battery", about: "sim-x1", source: "entity" });
    const props = asRec({ entity_id: "alr-2", entity_type: "alert", lat: 0, lon: 0, seq: 1, t: 5, properties: { alert_kind: "stale_feed", level: "caution" } });
    expect(alertFromEntity(props)).toMatchObject({ level: "caution", kind: "stale_feed" });
    expect(alertFromEntity(asRec({ entity_id: "alr-3", entity_type: "alert", lat: 0, lon: 0, seq: 1, t: 5, state: "resolved" }))).toBeNull();
    expect(alertFromEntity(asRec(simX1 as never))).toBeNull();
  });

  it("lifts health.alerts[] and geofence events; sorts warning first", () => {
    const sick = asRec({ ...simX1, health: { alerts: [{ code: "gps_degraded", level: "caution", since_t: 10 }] } });
    const h = alertsFromHealth(sick);
    expect(h).toHaveLength(1);
    expect(h[0]).toMatchObject({ kind: "gps_degraded", about: "sim-x1", level: "caution" });
    const g = alertFromGeofence("evt.map.geofence.zone-home.enter", { entity_id: "trk-1", t: 20 }, 99)!;
    expect(g).toMatchObject({ title: "trk-1 entered zone-home", level: "caution", about: "trk-1", t: 20 });
    expect(alertFromGeofence("evt.map.geofence.zone-home.dwell", {}, 1)).toBeNull();
    expect(alertFromGeofence("evt.mission.x.completed", {}, 1)).toBeNull();
    expect(alertFromGeofence("evt.map.geofence.zone-home.exit", null, 7)).toMatchObject({ about: undefined, t: 7 });
    const w = { ...g, key: "w", level: "warning" as const, t: 1 };
    expect(sortAlerts([g, w])[0].key).toBe("w");
  });
});

describe("video URLs from media[] (MediaMTX, never NATS)", () => {
  const media = [{ kind: "video", url: "rtsp://127.0.0.1:8554/drone/sim-x1", role: "primary" }];
  it("derives WHEP and LL-HLS and rewrites loopback to the page host", () => {
    expect(playbackUrls(media, "192.168.1.20")).toEqual({
      path: "drone/sim-x1",
      whep: "http://192.168.1.20:8889/drone/sim-x1/whep",
      hls: "http://192.168.1.20:8888/drone/sim-x1/index.m3u8",
      source: "rtsp://127.0.0.1:8554/drone/sim-x1",
    });
  });
  it("keeps a non-loopback host and honours override bases", () => {
    const m = [{ kind: "video", url: "rtsp://media.example:8554/cam/gate" }];
    expect(playbackUrls(m, "10.0.0.2")!.whep).toBe("http://media.example:8889/cam/gate/whep");
    expect(playbackUrls(media, "h", undefined, { whep: "http://127.0.0.1:9001/", hls: "http://127.0.0.1:9002" })).toMatchObject({
      whep: "http://127.0.0.1:9001/drone/sim-x1/whep",
      hls: "http://127.0.0.1:9002/drone/sim-x1/index.m3u8",
    });
  });
  it("returns null without a usable video entry", () => {
    expect(playbackUrls(undefined, "h")).toBeNull();
    expect(playbackUrls([{ kind: "image", url: "http://x/y.png" }], "h")).toBeNull();
    expect(playbackUrls([{ kind: "video", url: "not a url" }], "h")).toBeNull();
    expect(playbackUrls([{ kind: "video", url: "rtsp://127.0.0.1:8554/" }], "h")).toBeNull();
  });
  it("brackets an IPv6 page host", () => {
    expect(playbackUrls(media, "fe80::1")!.hls).toBe("http://[fe80::1]:8888/drone/sim-x1/index.m3u8");
  });
});
