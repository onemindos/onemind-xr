// Unit tests for the Act modules (FEATURES D1, D3, B11, B20, D4, D6, J2, C8).
import { describe, expect, it } from "vitest";
import { alertFromStaleAsset, alertsFromGaps } from "../../src/bus/alerts";
import type { EntityRecord } from "../../src/bus/store";
import { ALL_ROOM, ChatStore, geochatRequest } from "../../src/act/chat";
import { cotForDrawing, element, parseCot, xmlEscape } from "../../src/act/cot";
import { areaRings, buffer, canBeZone, lassoSelect, measure, zoneBody, zoneIdFor, type Drawing } from "../../src/act/drawing";
import { bearingDeg, bufferLine, circleRing, destination, distanceM, pointInRing, rectangleRing, ringAreaM2, sectorRing, type LonLat } from "../../src/act/geo";
import { FeedGaps, layersForSource, toMs } from "../../src/act/gaps";
import { buildPrompt, parseMapCommand, parseReply, splitPrompt, zoomToHeight, type MapContext } from "../../src/act/hermes";
import { ActionTracker, gotoRequest, permissionFromCheck, roleProbe, uuid } from "../../src/act/policy";
import { abilities, effectiveRole, parseRole } from "../../src/act/roles";

const C: LonLat = [-97.7431, 30.2672];
const D = (kind: Drawing["kind"], points: LonLat[], extra: Partial<Drawing> = {}): Drawing => ({ id: "d1", kind, label: "T", points, createdT: 0, ...extra });

describe("geo", () => {
  it("distance, bearing and destination agree", () => {
    const p = destination(C, 45, 1000);
    expect(distanceM(C, p)).toBeCloseTo(1000, 3);
    expect(bearingDeg(C, p)).toBeCloseTo(45, 3);
    // 1° of latitude ≈ 111.2 km on the mean sphere
    expect(distanceM([0, 0], [0, 1])).toBeCloseTo(111_195, -1);
    expect(bearingDeg([0, 0], [-1, 0])).toBeCloseTo(270, 6);
  });
  it("circle ring is closed and on the radius", () => {
    const r = circleRing(C, 500, 32);
    expect(r[0]).toEqual(r[r.length - 1]);
    for (const p of r) expect(distanceM(C, p)).toBeCloseTo(500, 3);
    expect(ringAreaM2(r) / (Math.PI * 500 * 500)).toBeGreaterThan(0.99);
  });
  it("sector starts at the centre and spans the clockwise sweep", () => {
    const s = sectorRing(C, 1000, 350, 10, 4);
    expect(s[0]).toEqual(C);
    expect(bearingDeg(C, s[1])).toBeCloseTo(350, 4);
    expect(bearingDeg(C, s[s.length - 2])).toBeCloseTo(10, 4);
    expect(pointInRing(destination(C, 0, 500), s)).toBe(true);
    expect(pointInRing(destination(C, 180, 500), s)).toBe(false);
  });
  it("rectangle from any two corners", () => {
    expect(rectangleRing([1, 2], [0, 0])).toEqual([[0, 0], [1, 0], [1, 2], [0, 2], [0, 0]]);
  });
  it("line buffer covers points within the distance and not beyond", () => {
    const a = C;
    const b = destination(C, 90, 2000);
    const ring = bufferLine([a, b], 100);
    expect(pointInRing(destination(destination(C, 90, 1000), 0, 90), ring)).toBe(true);
    expect(pointInRing(destination(destination(C, 90, 1000), 0, 120), ring)).toBe(false);
    expect(pointInRing(destination(b, 90, 80), ring)).toBe(true); // end cap
  });
});

describe("drawings", () => {
  it("area rings per kind", () => {
    expect(areaRings(D("circle", [C, destination(C, 0, 300)]))).toHaveLength(1);
    expect(areaRings(D("rings", [C, destination(C, 0, 300)]))).toHaveLength(3);
    expect(areaRings(D("line", [C, destination(C, 0, 300)]))).toHaveLength(0);
  });
  it("range and bearing measure", () => {
    const m = measure(D("rangebearing", [C, destination(C, 120, 2500)]));
    expect(m.distanceM).toBeCloseTo(2500, 2);
    expect(m.bearingDeg).toBeCloseTo(120, 3);
  });
  it("zone body uses only shapes the policy service can evaluate", () => {
    const rect = D("rectangle", [C, [C[0] + 0.01, C[1] + 0.01]]);
    const z = zoneBody(rect, "keep_out", { now: 1 })!;
    const zone = z.zone as { entity_id: string; geo: { shape: string; kind: string; coords: { type: string; coordinates: number[][][] } } };
    expect(zone.geo.shape).toBe("polygon");
    expect(zone.geo.coords.type).toBe("Polygon");
    expect(zone.geo.coords.coordinates[0]).toHaveLength(5);
    expect(zone.geo.kind).toBe("keep_out");
    expect(zone.entity_id).toBe(zoneIdFor(rect));
    expect(z.polygon).toHaveLength(5); // doctrine §9.7 field
    const circ = zoneBody(D("circle", [C, destination(C, 0, 250)]), "aoi", { now: 1 })!;
    const cz = circ.zone as { geo: { shape: string; radius_m: number; coords: { type: string } } };
    expect(cz.geo).toMatchObject({ shape: "circle", coords: { type: "Point" } });
    expect(cz.geo.radius_m).toBeCloseTo(250, 0);
    expect(zoneBody(D("line", [C, destination(C, 0, 250)]), "aoi", { now: 1 })).toBeNull();
    expect(canBeZone(D("rings", [C, destination(C, 0, 250)]))).toBe(false);
  });
  it("zone ids are safe subject tokens", () => {
    expect(zoneIdFor({ ...D("polygon", []), id: "a.b>c*d e" })).toBe("zone-abcde");
  });
  it("buffer of a point, a line and a polygon", () => {
    const pt = buffer(D("point", [C]), 200, "b1", 0)!;
    expect(pt.kind).toBe("buffer");
    expect(pointInRing(destination(C, 33, 190), pt.points)).toBe(true);
    const poly = D("polygon", rectangleRing(C, destination(destination(C, 0, 500), 90, 500)));
    const pb = buffer(poly, 100, "b2", 0)!;
    expect(pointInRing(destination(C, 225, 90), pb.points)).toBe(true);
    expect(pointInRing(destination(C, 225, 160), pb.points)).toBe(false);
    expect(buffer(poly, 0, "b3", 0)).toBeNull();
  });
  it("lasso selects what is inside", () => {
    const ring = rectangleRing([C[0] - 0.01, C[1] - 0.01], [C[0] + 0.01, C[1] + 0.01]);
    expect(lassoSelect(ring, [{ id: "in", lon: C[0], lat: C[1] }, { id: "out", lon: C[0] + 1, lat: C[1] }])).toEqual(["in"]);
  });
});

describe("CoT export (cot-projection.md §5.5)", () => {
  const o = { now: Date.UTC(2026, 9, 29, 12), staleMs: 3600_000 };
  const types = (d: Drawing) => cotForDrawing(d, o).map((x) => parseCot(x)!.type);
  it("maps each drawing to a contract type", () => {
    const e = destination(C, 0, 300);
    expect(types(D("point", [C]))).toEqual(["b-m-p-s-m"]);
    expect(types(D("label", [C]))).toEqual(["b-m-p-s-m"]);
    expect(types(D("line", [C, e, destination(C, 90, 300)]))).toEqual(["b-m-r"]);
    expect(types(D("polygon", [C, e, destination(C, 90, 300)]))).toEqual(["u-d-f"]);
    expect(types(D("rectangle", [C, [C[0] + 0.01, C[1] + 0.01]]))).toEqual(["u-d-r"]);
    expect(types(D("circle", [C, e]))).toEqual(["u-d-c-c"]);
    expect(types(D("sector", [C, e, destination(C, 60, 300)]))).toEqual(["u-d-f"]);
    expect(types(D("rings", [C, e]))).toEqual(["u-d-c-c", "u-d-c-c", "u-d-c-c"]);
    expect(types(D("rangebearing", [C, e]))).toEqual(["u-d-f"]);
  });
  it("keeps geometry: polygon links, circle radius, route waypoints, stale", () => {
    const poly = cotForDrawing(D("polygon", [C, destination(C, 0, 300), destination(C, 90, 300)]), o)[0];
    expect(poly.match(/<link point=/g)).toHaveLength(4); // closed
    const circle = cotForDrawing(D("circle", [C, destination(C, 0, 300)]), o)[0];
    expect(Number(element(circle, "ellipse")!.attrs.major)).toBeCloseTo(300, 0);
    const ev = parseCot(circle)!;
    expect(ev.lat).toBeCloseTo(C[1], 6);
    expect(Date.parse(ev.stale!) - Date.parse(ev.time!)).toBe(3600_000);
    const route = cotForDrawing(D("line", [C, destination(C, 0, 300)]), o)[0];
    expect(route.match(/type="b-m-p-w" relation="c"/g)).toHaveLength(2);
    const rb = cotForDrawing(D("rangebearing", [C, destination(C, 90, 1000)]), o)[0];
    expect(element(rb, "remarks")!.text).toMatch(/range 1000 m, bearing 90.0/);
  });
  it("escapes labels", () => {
    const x = cotForDrawing({ ...D("point", [C]), label: `<a href="x">&` }, o)[0];
    expect(x).toContain("&lt;a href=&quot;x&quot;&gt;&amp;");
    expect(element(x, "contact")!.attrs.callsign).toBe(`<a href="x">&`);
    expect(xmlEscape("'")).toBe("&apos;");
  });
});

describe("policy tracker and role probe", () => {
  it("status never moves backwards and terminal states stick", () => {
    let t = 0;
    const tr = new ActionTracker(() => ++t);
    const req = gotoRequest({ entity_type: "drone", entity_id: "sim-x1" }, { lat: 30.27, lon: -97.74, hae_m: 200 }, "user:zeus");
    tr.fromPropose(req, { action_id: "act-1", status: "awaiting_confirmation", params_hash: "sha256:ab", approvals_needed: 1, denial: null });
    tr.fromAudit("evt.so.action.dispatched", { data: { action_id: "act-1" } });
    tr.fromGet({ action: { action_id: "act-1", status: "approved" } }); // late poll
    expect(tr.get("act-1")!.status).toBe("dispatched");
    tr.fromAudit("evt.so.action.executed", { data: { action_id: "act-1" } });
    tr.fromAudit("evt.so.action.failed", { data: { action_id: "act-1", code: "no_ack" } });
    const a = tr.get("act-1")!;
    expect(a.status).toBe("executed");
    expect(a.history.map((h) => h.status)).toEqual(["awaiting_confirmation", "dispatched", "executed"]);
    expect(a.params).toEqual({ lat: 30.27, lon: -97.74, hae_m: 200 });
  });
  it("proposed audit event means awaiting confirmation; denial reply is denied", () => {
    const tr = new ActionTracker();
    expect(tr.fromAudit("evt.so.action.proposed", { data: { action_id: "act-2", requested_by: { kind: "agent", id: "agent:hermes/zeus/legacy" } } })!.status).toBe("awaiting_confirmation");
    expect(tr.pending().map((x) => x.action_id)).toEqual(["act-2"]);
    const req = gotoRequest({ entity_type: "drone", entity_id: "x1" }, { lat: 0, lon: 0, hae_m: 0 }, "user:zeus");
    expect(tr.fromPropose(req, { action_id: "", status: "denied", approvals_needed: 0, denial: { code: "not_simulated" } }).status).toBe("denied");
    expect(tr.fromAudit("evt.so.action.bogus", { data: { action_id: "x" } })).toBeNull();
  });
  it("role probe is an estop dry run and only not_authorized means denied", () => {
    const p = roleProbe({ entity_type: "drone", entity_id: "sim-x1" }, "user:zeus");
    expect(p.action).toMatchObject({ action_type: "asset.estop", dry_run: true, params: {} });
    expect(permissionFromCheck({ allowed: true }).permission).toBe("operator");
    expect(permissionFromCheck({ allowed: false, decision: { denial: { code: "not_authorized" } } }).permission).toBe("denied");
    expect(permissionFromCheck({ allowed: false, decision: { denial: { code: "policy_unavailable" } } }).permission).toBe("unknown");
    expect(permissionFromCheck(null).permission).toBe("unknown");
  });
  it("uuid is v4 shaped", () => {
    expect(uuid()).toMatch(/^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/);
  });
  it("roles narrow; operator needs the policy grant", () => {
    expect(effectiveRole("operator", "operator")).toBe("operator");
    expect(effectiveRole("operator", "denied")).toBe("observer");
    expect(effectiveRole("operator", "unknown")).toBe("observer");
    expect(effectiveRole("newcomer", "operator")).toBe("newcomer");
    expect(abilities("newcomer").assetCommands).toBe(false);
    expect(abilities("newcomer").draw).toBe(false);
    expect(abilities("observer").assetCommands).toBe(false);
    expect(abilities("operator").confirm).toBe(true);
    expect(parseRole("admin")).toBe("newcomer");
  });
});

describe("intelligence gaps", () => {
  it("a feed that stops goes stale at its limit and names its layers", () => {
    let now = Date.UTC(2026, 9, 29, 12);
    const g = new FeedGaps(() => now);
    g.ingestStatus("tel.feed.nws-alerts.status", { source: "nws-alerts", fetchedAt: new Date(now).toISOString(), lastError: null, staleAfterSec: 6, stale: false, count: 3 });
    expect(g.gaps()).toEqual([]);
    now += 5000;
    expect(g.gaps()).toEqual([]);
    now += 1500;
    const [gap] = g.gaps();
    expect(gap).toMatchObject({ source: "nws-alerts", reason: "stale", layers: ["alerts", "hazards"] });
    expect(gap.alerts.length).toBeGreaterThan(0);
  });
  it("errors, never-fetched and layer down", () => {
    let now = 1_000_000;
    const g = new FeedGaps(() => now);
    g.ingestStatus("tel.feed.usgs.status", { fetchedAt: now, lastError: "HTTP 503", staleAfterSec: 60, stale: false });
    g.ingestStatus("tel.feed.mta.status", { fetchedAt: null, lastError: null, staleAfterSec: 10 });
    g.ingestManifest("air", { v: 1, layer: "air", freshness: { status: "down", limit_s: 120 } });
    now += 11_000;
    const by = Object.fromEntries(g.gaps().map((x) => [x.source === "layer service" ? x.key : x.source, x.reason]));
    expect(by).toEqual({ usgs: "error", mta: "never", "layer:air": "layer_down" });
    g.ingestManifest("air", { v: 1, layer: "air", freshness: { status: "fresh", limit_s: 120 } });
    expect(g.gaps().some((x) => x.key === "layer:air")).toBe(false);
  });
  it("manifest feeds map sources to the layer that reports them", () => {
    const now = 5_000_000;
    const g = new FeedGaps(() => now);
    g.ingestManifest("hazards", { v: 1, layer: "hazards", freshness: { status: "degraded", limit_s: 30, feeds: [{ source: "custom-quake", fetchedAt: now - 60_000, stale: true }] } as never });
    expect(g.gaps()[0]).toMatchObject({ source: "custom-quake", layers: ["hazards"], reason: "stale" });
  });
  it("source lookup and time parsing", () => {
    expect(layersForSource("nws-alerts")).toEqual(["hazards", "alerts"]);
    expect(layersForSource("ais-aishub")).toEqual(["sea"]);
    expect(layersForSource("nope")).toEqual([]);
    expect(toMs("2026-10-29T12:00:00Z")).toBe(Date.UTC(2026, 9, 29, 12));
    expect(toMs(1_793_275_200)).toBe(1_793_275_200_000);
  });
  it("gaps and stale assets become alerts (D6)", () => {
    const rec = { r: { entity_id: "sim-x1", entity_type: "drone", callsign: "SIM X1", t: Date.now() - 30_000 } } as unknown as EntityRecord;
    expect(alertFromStaleAsset(rec, true)).toMatchObject({ kind: "stale asset", about: "sim-x1", source: "stale" });
    expect(alertFromStaleAsset(rec, false)).toBeNull();
    const [a] = alertsFromGaps([{ key: "feed:usgs", source: "usgs", layers: ["hazards"], reason: "error", detail: "", since: 1 }]);
    expect(a).toMatchObject({ level: "warning", kind: "feed outage", source: "feed" });
  });
});

describe("Hermes", () => {
  const ctx: MapContext = { selection: null, viewport: { bbox: [-98, 30, -97, 31], zoom: 12 }, layers: ["fleet"], visible: [{ entity_id: "sim-x1", entity_type: "drone", lat: 30.27, lon: -97.74, speed_mps: 12 }], zones: [], gaps: [] };
  it("prompt round trip", () => {
    const p = buildPrompt("  What's moving near the north field? ", ctx);
    const s = splitPrompt(p);
    expect(s.question).toBe("What's moving near the north field?");
    expect(s.context).toEqual(ctx);
    expect(splitPrompt("plain").context).toBeNull();
  });
  it("reply parsing: JSON, plain chunks, empty terminator", () => {
    expect(parseReply(['{"text":"Two things.","proposals":["act-1"]}'])).toEqual({ text: "Two things.", proposals: ["act-1"] });
    expect(parseReply(["Hello ", "world", ""])).toEqual({ text: "Hello world", proposals: [] });
    expect(parseReply(["{not json"]).text).toBe("{not json");
  });
  it("cmd.map.* parsing (doctrine §9.7)", () => {
    expect(parseMapCommand("cmd.map.highlight.sim-x1", { color: "#00FF00", ring: true, label: "FOCUS" })).toEqual({ kind: "highlight", uid: "sim-x1", color: "#00FF00", ring: true, label: "FOCUS", clear: false });
    expect(parseMapCommand("cmd.map.highlight.sim-x1", { color: "red;" })).toMatchObject({ color: "#FF4FD8" });
    expect(parseMapCommand("cmd.map.annotation.a1.create", { lat: 30, lon: -97, label: "here" })).toMatchObject({ kind: "annotation", id: "a1", verb: "create" });
    expect(parseMapCommand("cmd.map.annotation.a1.create", { lat: 99, lon: 0 })).toBeNull();
    expect(parseMapCommand("cmd.map.annotation.a1.remove", {})).toMatchObject({ verb: "remove" });
    expect(parseMapCommand("cmd.map.viewport.set", { lat: 26.5, lon: 56.5, zoom: 7 })).toEqual({ kind: "viewport", lat: 26.5, lon: 56.5, zoom: 7 });
    expect(parseMapCommand("cmd.map.filter.set", { layers: ["air.*"], bbox: [0, 0, 1, 1] })).toMatchObject({ kind: "filter", layers: ["air.*"] });
    expect(parseMapCommand("cmd.map.geofence.zone-a.create", { polygon: [] })).toMatchObject({ kind: "geofence", id: "zone-a", verb: "create" });
    expect(parseMapCommand("cmd.fleet.drone.sim-x1.goto", {})).toBeNull();
    expect(parseMapCommand("cmd.map.selfdestruct", {})).toBeNull();
    expect(zoomToHeight(12)).toBeCloseTo(8544.9, 0);
  });
});

describe("GeoChat and contacts", () => {
  const sa = (uid: string, cs: string, group: string) =>
    `<event version="2.0" uid="${uid}" type="a-f-G-U-C" how="h-e" time="2026-10-29T12:00:00Z" start="2026-10-29T12:00:00Z" stale="2026-10-29T12:05:00Z"><point lat="30.27" lon="-97.74" hae="150" ce="5" le="5"/><detail><contact callsign="${cs}" endpoint="*:-1:stcp"/><__group name="${group}" role="Team Member"/></detail></event>`;
  // The GeoChat event of omos #551 gateway_test.go chatEvent()
  const chat = (uid: string, room: string, id: string, from: string, cs: string, text: string) =>
    `<event version="2.0" uid="${uid}" type="b-t-f" how="h-g-i-g-o" time="2026-10-29T12:00:01Z" start="2026-10-29T12:00:01Z" stale="2026-10-29T12:01:01Z"><point lat="0" lon="0" hae="0" ce="9999999" le="9999999"/><detail><__chat parent="RootContactGroup" groupOwner="false" chatroom="${room}" id="${id}" senderCallsign="${cs}"><chatgrp uid0="${from}" uid1="${id}" id="${id}"/></__chat><remarks source="BAO.F.ATAK.${from}" to="${id}" time="2026-10-29T12:00:01.123Z">${text}</remarks></detail></event>`;
  it("contacts, channels, rooms and messages", () => {
    const s = new ChatStore("OM-web-1");
    expect(s.ingest(sa("ANDROID-1", "VIPER", "Cyan"), "tak.cot.out")).toBe("contact");
    expect(s.ingest(sa("ANDROID-2", "HAWK", "Red"), "tak.cot.mesh.rx")).toBe("contact");
    expect(s.ingest(chat("GeoChat.ANDROID-1.All Chat Rooms.1", ALL_ROOM, ALL_ROOM, "ANDROID-1", "VIPER", "hi &amp; hello"), "tak.cot.out")).toBe("chat");
    expect(s.ingest(chat("GeoChat.ANDROID-1.All Chat Rooms.1", ALL_ROOM, ALL_ROOM, "ANDROID-1", "VIPER", "dup"), "tak.cot.mesh.rx")).toBe("chat");
    expect(s.ingest(chat("GeoChat.ANDROID-2.OM-web-1.2", "OM-web-1", "OM-web-1", "ANDROID-2", "HAWK", "direct to you"), "tak.cot.out")).toBe("chat");
    expect(s.messages(ALL_ROOM).map((m) => m.text)).toEqual(["hi & hello"]);
    expect(s.messages("ANDROID-2").map((m) => m.text)).toEqual(["direct to you"]);
    expect(s.channels()).toEqual(["Cyan", "Red"]);
    expect(s.contactList("Cyan").map((c) => c.callsign)).toEqual(["VIPER"]);
    const roomsCyan = s.rooms("Cyan").map((r) => r.id);
    expect(roomsCyan).toContain("ANDROID-1");
    expect(roomsCyan).not.toContain("ANDROID-2");
    expect(roomsCyan).toContain("Cyan");
    expect(roomsCyan).not.toContain("Red");
    expect(s.rooms(null).find((r) => r.id === ALL_ROOM)!.unread).toBe(1);
    s.markRead(ALL_ROOM);
    expect(s.rooms(null).find((r) => r.id === ALL_ROOM)!.unread).toBe(0);
    expect(s.ingest("<event/>", "x")).toBe("other");
  });
  it("outgoing message state and request body", () => {
    const s = new ChatStore("OM-web-1");
    const room = s.rooms(null)[0];
    s.addOutgoing(room, "hello", "m1", "WEB");
    s.setState("m1", "sent");
    expect(s.messages(ALL_ROOM)[0]).toMatchObject({ mine: true, state: "sent" });
    expect(geochatRequest(room, "x".repeat(2000), { uid: "OM-web-1", callsign: "WEB" }, "m2")).toMatchObject({ v: 1, kind: "geochat", room: { id: ALL_ROOM } });
    expect(geochatRequest(room, "x".repeat(2000), { uid: "u", callsign: "c" }, "m2").text).toHaveLength(1000);
  });
});
