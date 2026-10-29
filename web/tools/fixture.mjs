// Fixture publisher for the L0 client. It plays the omos entity service's role on a local bus,
// publishing records shaped exactly as docs/contracts/entity.md §4.2 (sim-x1 example) on
// `ent.{entity_type}.{entity_id}.state`, with the §4.3 headers. All assets are simulated
// (`sim-*`, `indicators.simulated: true`, decision 0004 #13).
//
// Modes:
//   core       only core `ent.*.*.state` publishes (the interim path without services)
//   entity     + `svc.entity.query` / `svc.entity.get` with the reply shapes of omos PR #552
//   layer      + `svc.layer.snapshot` / `svc.layer.watch` per layer-snapshot.md §3.1 and
//              layer-delta.md §2 (the omos layer service does not exist yet; this is a stand-in)
//
// Scenario (all around the entity.md §4.2 example position, Austin TX):
//   sim-x1    drone, orbits inside the AOI, tasked, has video in media[]
//   trk-…     a moving track that crosses into the AOI (geofence enter event on evt.map.geofence)
//   zone-home AOI polygon (pinned)
//   alr-…     a warning alert about sim-x1 (non-located, lat 0 lon 0)
//   sim-v1    untasked vehicle outside the AOI (hidden when "Mission tier" is on)
//   sim-r1    robot that is deleted with a final `live: false` record after `removeAfterS`
//   sim-e1    node published once with a short expiry_t and never again (client expires it)
//   sim-m1    drone marked AMBER+STRICT for `team-b` only (hidden for a `homestead` viewer)
import { headers, wsconnect } from "@nats-io/nats-core";

const C_LAT = 30.2672;
const C_LON = -97.7431;
const M_PER_DEG_LAT = 111_320;
const mPerDegLon = (lat) => 111_320 * Math.cos((lat * Math.PI) / 180);
const enc = new TextEncoder();

export const IDS = {
  drone: "sim-x1",
  track: "trk-01jbx7k2m9q4r8s6t0v2w4y6z8",
  zone: "zone-home",
  alert: "alr-01jbx7m3n4p5q6r7s8t9v0w1x2",
  vehicle: "sim-v1",
  removed: "sim-r1",
  expiring: "sim-e1",
  marked: "sim-m1",
};

function offset(lat, lon, eastM, northM) {
  return [lat + northM / M_PER_DEG_LAT, lon + eastM / mPerDegLon(lat)];
}

function base(id, type, now, extra) {
  return {
    uid: id,
    entity_id: id,
    entity_type: type,
    v: 1,
    live: true,
    lifecycle: "active",
    created_t: now - 60_000,
    pinned: false,
    t: now,
    confidence: 0.9,
    relationships: [],
    aliases: { name: null, ids: [] },
    indicators: { simulated: true, exercise: false, emergency: false, starred: false, egress_tak: false },
    marking: { tlp: "AMBER+STRICT", releasable_to: ["homestead"] },
    confidence_parts: { source_prior: 0.9, track_quality: 0.9, corroboration: 0.9 },
    conflicts: [],
    overrides: [],
    ...extra,
  };
}

function located(lat, lon, hae, course, speed, now, ce = 5) {
  return {
    lat, lon, hae, course, speed,
    location: { lat, lon, hae_m: hae, agl_m: Math.max(0, hae - 150), course_deg: course, speed_mps: speed,
      vel_enu_mps: [speed * Math.sin((course * Math.PI) / 180), speed * Math.cos((course * Math.PI) / 180), 0],
      attitude_q: [1, 0, 0, 0], measured_t: now },
    uncertainty: { ce_m: ce, le_m: 3, pos_enu_cov: null, vel_enu_cov: null, error_ellipse: null },
  };
}

export class Fixture {
  constructor({ wsUrl, mode = "core", videoUrl = "rtsp://127.0.0.1:8554/drone/sim-x1", hz = 2, removeAfterS = 8, expireAfterS = 6, user, pass, log = () => {} }) {
    Object.assign(this, { wsUrl, mode, videoUrl, hz, removeAfterS, expireAfterS, user, pass, log });
    this.seq = new Map();
    this.latest = new Map(); // id -> full record (the fixture's ENT)
    this.entSeq = 0;
    this.watches = new Map();
    this.watchCounter = 0;
    this.timers = [];
    this.trackInside = false;
    this.t0 = Date.now();
    this.published = 0;
  }

  nextSeq(id) {
    const s = (this.seq.get(id) ?? 0) + 1;
    this.seq.set(id, s);
    return s;
  }

  async start() {
    this.nc = await wsconnect({ servers: this.wsUrl, name: "l0-fixture", user: this.user, pass: this.pass });
    if (this.mode === "entity" || this.mode === "layer") this.serveEntity();
    if (this.mode === "layer") this.serveLayer();
    const now = Date.now();
    this.publish(this.zoneRecord(now));
    this.publish(this.vehicleRecord(now));
    this.publish(this.markedRecord(now));
    this.publish(this.robotRecord(now, true));
    this.publish(this.expiringRecord(now));
    this.publish(this.alertRecord(now));
    this.tick();
    this.timers.push(setInterval(() => this.tick(), 1000 / this.hz));
    this.timers.push(
      setTimeout(() => this.publish(this.robotRecord(Date.now(), false)), this.removeAfterS * 1000),
    );
    // Heartbeat static records well before their expiry.
    this.timers.push(
      setInterval(() => {
        const n = Date.now();
        this.publish(this.vehicleRecord(n));
        this.publish(this.markedRecord(n));
        this.publish(this.alertRecord(n));
      }, 20_000),
    );
    if (this.mode === "layer") this.timers.push(setInterval(() => this.flushWatches(), 250));
    return this;
  }

  async stop() {
    for (const t of this.timers) clearInterval(t), clearTimeout(t);
    await this.nc?.drain().catch(() => undefined);
  }

  tick() {
    const now = Date.now();
    for (const [id, e] of this.latest) if (typeof e.expiry_t === "number" && e.expiry_t < now) this.latest.delete(id);
    const el = (now - this.t0) / 1000;
    this.publish(this.droneRecord(now, el));
    const tr = this.trackRecord(now, el);
    this.publish(tr);
    const inside = Math.abs(tr.lat - C_LAT) < 0.0068 && Math.abs(tr.lon - C_LON) < 0.0078;
    if (inside !== this.trackInside) {
      this.trackInside = inside;
      // Geofence watch-job event (omos NATS-DOCTRINE: evt.map.geofence.{id}.{enter|exit}).
      this.nc.publish(
        `evt.map.geofence.${IDS.zone}.${inside ? "enter" : "exit"}`,
        enc.encode(JSON.stringify({ entity_id: IDS.track, zone_id: IDS.zone, t: now })),
      );
    }
  }

  /** Publish one full record on its subject with the entity.md §4.3 headers. */
  publish(rec) {
    const h = headers();
    h.set("Nats-Msg-Id", `${rec.entity_id}:${rec.seq}`);
    h.set("Nats-TTL", rec.pinned ? "never" : `${Math.max(1, Math.round(((rec.expiry_t ?? 0) - rec.t) / 1000))}s`);
    h.set("OneMind-Marking", rec.marking.tlp);
    h.set("OneMind-Releasable-To", rec.marking.releasable_to.join(","));
    h.set("OneMind-Schema", "entity.v1");
    this.nc.publish(`ent.${rec.entity_type}.${rec.entity_id}.state`, enc.encode(JSON.stringify(rec)), { headers: h });
    this.published++;
    this.entSeq++;
    if (rec.live) this.latest.set(rec.entity_id, rec);
    else this.latest.delete(rec.entity_id);
    for (const w of this.watches.values()) {
      if (w.types.includes(rec.entity_type)) {
        w.pending.set(rec.entity_id, rec.live ? { op: "upsert", e: w.full ? rec : renderOf(rec) } : { op: "remove", id: rec.entity_id, reason: "not_live" });
      }
    }
  }

  // ---- records ---------------------------------------------------------------------------

  droneRecord(now, el) {
    const ang = (el / 60) * 2 * Math.PI; // one orbit per minute, radius 400 m
    const [lat, lon] = offset(C_LAT, C_LON, 400 * Math.sin(ang), 400 * Math.cos(ang));
    const course = ((ang * 180) / Math.PI + 90) % 360;
    const seq = this.nextSeq(IDS.drone);
    return base(IDS.drone, "drone", now, {
      type: "fleet.drone", callsign: "SIM-X1", stale: 10, seq,
      ...located(lat, lon, 152.4, course, 41.9, now),
      meta: { battery_pct: 82, mode: "AUTO", link_rssi: -67, mission_id: "msn-01jbx7a0000000000000000000", node_id: "build-mac" },
      properties: { callsign: "SIM-X1", altitude: 152.4, speed: 41.9, course, ce: 5.0, le: 3.0 },
      confidence: 0.97,
      expiry_t: now + 60_000,
      kind: "multirotor", template: "asset",
      traits: ["locatable", "trackable", "taskable", "observable"],
      disposition: "friendly", environment: "air", cot_type: "a-f-A-M-H-Q",
      aliases: { name: "SIM-X1", ids: [{ type: "mavlink_sysid", id: "1" }, { type: "cot_uid", id: "sim-x1" }] },
      geo: null,
      status: { activity: "orbit", role: "overwatch", mode: "AUTO" },
      health: { connection: "online", overall: "healthy", battery_pct: 82, link_rssi: -67, components: [{ id: "gps", health: "healthy" }], alerts: [] },
      sensors: [{ id: "cam0", type: "camera", state: "operational", fov: { h_deg: 82, v_deg: 52, az_deg: 90, el_deg: -30, range_m: 300 } }],
      capabilities: ["onemind.task.goto.v1", "onemind.task.orbit.v1", "onemind.task.rtl.v1", "onemind.task.land.v1", "onemind.task.observe.v1"],
      current_task: { task_id: "tsk-01jbx7k2m9q4r8s6t0v2w4y6z8", state: "executing" },
      tracked: null, correlation: null, route: null,
      relationships: [{ id: "rel-1", type: "member_of", to: "team.team-cyan", since_t: now - 600_000, source: "registry" }],
      media: [{ kind: "video", url: this.videoUrl, role: "primary" }],
      indicators: { simulated: true, exercise: false, emergency: false, starred: false, egress_tak: true },
      cot_security: null,
      provenance: { primary: "bridge-mavlink", how: "m-s", source_t: now },
      sources: [{ integration: "bridge-mavlink", source_id: "sysid-1", data_type: "mavlink", subject: "tel.fleet.drone.sim-x1.position", stream: "TEL", seq: 118220 + seq, first_seen_t: now - 60_000, last_seen_t: now, source_t: now, components: ["location", "health", "status"] }],
      confidence_parts: { source_prior: 0.95, track_quality: 1.0, corroboration: 0.95 },
    });
  }

  trackRecord(now, el) {
    // Moves east at 15 m/s from 900 m west of the centre (AOI edge at 750 m), crosses the AOI,
    // and wraps every 120 s.
    const east = -900 + ((el * 15) % 1800);
    const [lat, lon] = offset(C_LAT, C_LON, east, -200);
    const seq = this.nextSeq(IDS.track);
    return base(IDS.track, "track", now, {
      type: "onemind.track", callsign: "TRACK 7", stale: 15, seq,
      ...located(lat, lon, 160, 90, 15, now, 25),
      properties: { callsign: "TRACK 7", altitude: 160, speed: 15, course: 90, ce: 25, le: 10 },
      confidence: 0.71, expiry_t: now + 30_000,
      kind: "vehicle", template: "track", traits: ["locatable", "trackable"],
      disposition: "unknown", environment: "land", cot_type: "a-u-G",
      tracked: { quality: 9, hits: Math.round(el * this.hz), last_meas_t: now, lob: null },
      provenance: { primary: "fixture-radar", how: "m-f", source_t: now },
      sources: [
        { integration: "fixture-radar", source_id: "radar-1", data_type: "det", subject: "det.land.radar.trk7", stream: "DET", seq: 5000 + seq, first_seen_t: this.t0, last_seen_t: now, source_t: now, components: ["location"] },
        { integration: "fixture-camera", source_id: "cam-gate", data_type: "det", subject: "det.land.visual.trk7", stream: "DET", seq: 9000 + seq, first_seen_t: this.t0, last_seen_t: now - 2000, source_t: now - 2000, components: ["location", "kind"] },
      ],
      confidence_parts: { source_prior: 0.8, track_quality: 0.6, corroboration: 0.75 },
      conflicts: [{ attribute: "kind", kept: "vehicle", rejected: "unknown", sources: ["fixture-camera", "fixture-radar"] }],
    });
  }

  zoneRecord(now) {
    const ring = [[-97.7509, 30.2604], [-97.7353, 30.2604], [-97.7353, 30.2740], [-97.7509, 30.2740], [-97.7509, 30.2604]];
    return base(IDS.zone, "zone", now, {
      type: "onemind.zone", callsign: "HOME AOI", seq: this.nextSeq(IDS.zone), lat: C_LAT, lon: C_LON,
      pinned: true, expiry_t: null, template: "geo", traits: [], kind: "aoi",
      geo: { shape: "polygon", coords: { type: "Polygon", coordinates: [ring] }, kind: "aoi" },
      provenance: { primary: "policy-service", how: "h-e", source_t: now },
      sources: [{ integration: "kv-geofences", subject: "geofences/zone-home", components: ["geo"], last_seen_t: now }],
      marking: { tlp: "GREEN", releasable_to: [] },
    });
  }

  alertRecord(now) {
    return base(IDS.alert, "alert", now, {
      type: "onemind.alert", callsign: "SIM-X1 battery below 85%", seq: this.nextSeq(IDS.alert), lat: 0, lon: 0,
      stale: 300, expiry_t: now + 600_000, template: "spi", traits: [], kind: "low_battery",
      alert_kind: "low_battery", level: "warning", about: IDS.drone, raised_by: "watch-battery", state: "open",
      provenance: { primary: "watch-battery", how: "m-f", source_t: now },
      sources: [{ integration: "watch-battery", subject: "ent.drone.sim-x1.state", components: ["health"], last_seen_t: now }],
    });
  }

  vehicleRecord(now) {
    const [lat, lon] = offset(C_LAT, C_LON, 4200, 2600);
    return base(IDS.vehicle, "vehicle", now, {
      type: "fleet.vehicle", callsign: "SIM-V1", stale: 60, seq: this.nextSeq(IDS.vehicle),
      ...located(lat, lon, 160, 0, 0, now), expiry_t: now + 120_000,
      template: "asset", traits: ["locatable", "trackable"], disposition: "friendly", environment: "land", cot_type: "a-f-G-E-V",
      provenance: { primary: "bridge-mavlink", how: "m-s", source_t: now },
      sources: [{ integration: "bridge-mavlink", subject: "tel.fleet.vehicle.sim-v1.position", stream: "TEL", components: ["location"], last_seen_t: now }],
    });
  }

  markedRecord(now) {
    const [lat, lon] = offset(C_LAT, C_LON, -300, 300);
    return base(IDS.marked, "drone", now, {
      type: "fleet.drone", callsign: "SIM-M1", stale: 60, seq: this.nextSeq(IDS.marked),
      ...located(lat, lon, 150, 0, 0, now), expiry_t: now + 120_000,
      template: "asset", traits: ["locatable"], disposition: "friendly",
      marking: { tlp: "AMBER+STRICT", releasable_to: ["team-b"] },
      provenance: { primary: "bridge-mavlink", how: "m-s", source_t: now },
      sources: [],
    });
  }

  robotRecord(now, live) {
    const [lat, lon] = offset(C_LAT, C_LON, 200, -400);
    return base(IDS.removed, "robot", now, {
      type: "fleet.robot", callsign: "SIM-R1", stale: 30, seq: this.nextSeq(IDS.removed), live,
      lifecycle: live ? "active" : "expired",
      ...located(lat, lon, 150, 0, 0, now), expiry_t: now + 60_000,
      template: "asset", traits: ["locatable", "trackable", "taskable"], disposition: "friendly", cot_type: "a-f-G-E-V",
      provenance: { primary: "bridge-ros2", how: "m-s", source_t: now },
      sources: [],
    });
  }

  expiringRecord(now) {
    const [lat, lon] = offset(C_LAT, C_LON, -200, -100);
    return base(IDS.expiring, "node", now, {
      type: "fleet.node", callsign: "SIM-E1", stale: 3, seq: this.nextSeq(IDS.expiring),
      ...located(lat, lon, 150, 0, 0, now), expiry_t: now + this.expireAfterS * 1000,
      template: "asset", traits: ["locatable"], disposition: "friendly",
      provenance: { primary: "fixture", how: "m-s", source_t: now },
      sources: [],
    });
  }

  // ---- services ---------------------------------------------------------------------------

  /** svc.entity.get / svc.entity.query with the reply shapes of omos PR #552 (services/entity). */
  serveEntity() {
    this.nc.subscribe("svc.entity.get", {
      callback: (_e, m) => {
        let q = {};
        try { q = m.json(); } catch { /* empty */ }
        const e = this.latest.get(q.entity_id);
        if (!e) {
          const h = headers();
          h.set("Nats-Service-Error-Code", "404");
          h.set("Nats-Service-Error", "entity not found");
          m.respond(new Uint8Array(), { headers: h });
          return;
        }
        m.respond(enc.encode(JSON.stringify(e)));
      },
    });
    this.nc.subscribe("svc.entity.query", {
      callback: (_e, m) => {
        let q = {};
        try { q = m.json(); } catch { /* empty */ }
        const limit = Math.min(q.limit || 200, 200);
        const all = [...this.latest.values()]
          .filter((e) => !q.types?.length || q.types.includes(e.entity_type))
          .filter((e) => !q.bbox || (e.lon >= q.bbox[0] && e.lon <= q.bbox[2] && e.lat >= q.bbox[1] && e.lat <= q.bbox[3]))
          .sort((a, b) => a.entity_id.localeCompare(b.entity_id))
          .filter((e) => !q.cursor || e.entity_id > q.cursor);
        const page = all.slice(0, limit);
        const cursor = all.length > limit ? page[page.length - 1].entity_id : null;
        m.respond(enc.encode(JSON.stringify({ entities: page, cursor })));
      },
    });
  }

  /** Stand-in for svc.layer.snapshot / svc.layer.watch (layer-snapshot.md §3.1, layer-delta.md §2). */
  serveLayer() {
    const LAYERS = {
      fleet: ["drone", "robot", "vehicle", "node"], tracks: ["track"], zones: ["zone"], alerts: ["alert"],
      tak: ["person", "place"], hazards: ["hazard", "event"], air: ["aircraft"], sea: ["vessel"],
    };
    this.nc.subscribe("svc.layer.snapshot", {
      callback: (_e, m) => {
        const q = m.json();
        const types = LAYERS[q.layer];
        if (!types) return m.respond(enc.encode(JSON.stringify({ v: 1, error: "unknown_layer", detail: q.layer })));
        const full = q.fields === "full";
        const items = [...this.latest.values()].filter((e) => types.includes(e.entity_type)).map((e) => (full ? e : renderOf(e)));
        const parts = [];
        for (let i = 0; i < items.length || i === 0; i += 2) parts.push(items.slice(i, i + 2)); // small chunks to exercise multi-part
        parts.forEach((p, i) =>
          m.respond(enc.encode(JSON.stringify({ v: 1, layer: q.layer, part: i, last: i === parts.length - 1, mode: "entities", ent_seq: this.entSeq, as_of: Date.now(), truncated: false, items: p }))),
        );
      },
    });
    this.nc.subscribe("svc.layer.watch", {
      callback: (_e, m) => {
        const q = m.json();
        const types = LAYERS[q.layer];
        if (!types) return m.respond(enc.encode(JSON.stringify({ v: 1, error: "unknown_layer" })));
        if (q.watch_id && this.watches.has(q.watch_id)) {
          const w = this.watches.get(q.watch_id);
          w.renewals++;
          return m.respond(enc.encode(JSON.stringify({ v: 1, watch_id: w.id, lease_s: q.lease_s, ent_seq: this.entSeq })));
        }
        const id = `w-${++this.watchCounter}-${Date.now().toString(36)}`;
        this.watches.set(id, { id, layer: q.layer, types, inbox: m.reply, n: 0, pending: new Map(), lastSent: Date.now(), renewals: 0, full: q.fields === "full" });
        m.respond(enc.encode(JSON.stringify({ v: 1, watch_id: id, lease_s: q.lease_s, ent_seq: this.entSeq })));
      },
    });
    this.nc.subscribe("svc.layer.unwatch", { callback: (_e, m) => this.watches.delete(m.json().watch_id) });
  }

  flushWatches() {
    const now = Date.now();
    for (const w of this.watches.values()) {
      if (!w.pending.size && now - w.lastSent < 10_000) continue; // heartbeat every 10 s
      const ops = [...w.pending.values()];
      w.pending.clear();
      w.n++;
      w.lastSent = now;
      this.nc.publish(w.inbox, enc.encode(JSON.stringify({ v: 1, watch_id: w.id, layer: w.layer, n: w.n, from_seq: this.entSeq - ops.length + 1, to_seq: this.entSeq, as_of: now, mode: "entities", ops, meta: null })));
    }
  }
}

/** The layer-snapshot.md §4 render subset. */
export function renderOf(e) {
  return {
    entity_id: e.entity_id, entity_type: e.entity_type, lat: e.lat, lon: e.lon, hae: e.hae,
    course: e.course ?? -1, speed: e.speed ?? -1, callsign: e.callsign, cot_type: e.cot_type,
    disposition: e.disposition, t: e.t, stale: e.stale, expiry_t: e.expiry_t, seq: e.seq, live: e.live,
    origin: "hub", simulated: e.indicators?.simulated === true,
  };
}
