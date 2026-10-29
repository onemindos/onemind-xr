// Alerts shown by L0, from three sources on existing subjects:
//   1. `ent.alert.{id}.state` entities (ONTOLOGY.md §3 alert row: `alert_kind`, `level`
//      advisory|caution|warning, `about`, `raised_by`, `state` open|acknowledged|resolved).
//      entity.md does not yet fix where these properties sit in the record, so they are read from
//      the top level first, then `properties`, then an `alert` object (unverified placement).
//   2. `health.alerts[]` of any entity (entity.md §4.1 `health`).
//   3. `evt.map.geofence.{id}.{enter|exit}` (omos docs/NATS-DOCTRINE.md, geofence watch job). Its
//      payload is not specified there, so only subject tokens plus common id fields are read.
//   4. (FEATURES D6) stale own assets: a fleet entity past its `stale` limit.
//   5. (FEATURES D6) feed outages: the B20 gaps (act/gaps.ts).
import type { EntityRecord } from "./store";

export type AlertLevel = "advisory" | "caution" | "warning";

export interface AlertItem {
  key: string;
  level: AlertLevel;
  kind: string;
  title: string;
  about?: string;
  t: number;
  source: "entity" | "health" | "geofence" | "stale" | "feed";
  state: string;
}

const LEVEL_RANK: Record<AlertLevel, number> = { warning: 3, caution: 2, advisory: 1 };

const asLevel = (v: unknown): AlertLevel =>
  v === "warning" || v === "caution" || v === "advisory" ? v : "advisory";

function pick(rec: EntityRecord, key: string): unknown {
  const f = rec.full as Record<string, unknown> | undefined;
  if (!f) return undefined;
  if (f[key] !== undefined) return f[key];
  const p = f["properties"] as Record<string, unknown> | undefined;
  if (p && p[key] !== undefined) return p[key];
  const a = f["alert"] as Record<string, unknown> | undefined;
  return a ? a[key] : undefined;
}

export function alertFromEntity(rec: EntityRecord): AlertItem | null {
  if (rec.r.entity_type !== "alert") return null;
  const state = String(pick(rec, "state") ?? "open");
  if (state === "resolved") return null;
  const kind = String(pick(rec, "alert_kind") ?? "alert");
  const about = pick(rec, "about");
  return {
    key: `ent:${rec.r.entity_id}`,
    level: asLevel(pick(rec, "level")),
    kind,
    title: rec.r.callsign && rec.r.callsign !== rec.r.entity_id ? rec.r.callsign : kind,
    about: typeof about === "string" ? about : undefined,
    t: rec.r.t,
    source: "entity",
    state,
  };
}

export function alertsFromHealth(rec: EntityRecord): AlertItem[] {
  const list = rec.full?.health?.alerts ?? [];
  return list.map((a) => ({
    key: `health:${rec.r.entity_id}:${a.code}`,
    level: asLevel(a.level),
    kind: a.code,
    title: `${rec.r.callsign ?? rec.r.entity_id}: ${a.code}`,
    about: rec.r.entity_id,
    t: a.since_t ?? rec.r.t,
    source: "health" as const,
    state: "open",
  }));
}

/** Parse `evt.map.geofence.{id}.{enter|exit}` into an alert. Returns null for other subjects. */
export function alertFromGeofence(subject: string, payload: unknown, now: number): AlertItem | null {
  const tok = subject.split(".");
  if (tok.length !== 5 || tok[0] !== "evt" || tok[1] !== "map" || tok[2] !== "geofence") return null;
  const [, , , fence, event] = tok;
  if (event !== "enter" && event !== "exit") return null;
  const p = (payload && typeof payload === "object" ? payload : {}) as Record<string, unknown>;
  const who = [p["entity_id"], p["uid"], p["callsign"]].find((v) => typeof v === "string") as
    | string
    | undefined;
  const t = typeof p["t"] === "number" ? (p["t"] as number) : now;
  return {
    key: `geofence:${fence}:${who ?? "?"}:${event}:${t}`,
    level: "caution",
    kind: `geofence ${event}`,
    title: `${who ?? "Entity"} ${event === "enter" ? "entered" : "left"} ${fence}`,
    about: who,
    t,
    source: "geofence",
    state: "open",
  };
}

export function sortAlerts(items: AlertItem[]): AlertItem[] {
  return [...items].sort((a, b) => LEVEL_RANK[b.level] - LEVEL_RANK[a.level] || b.t - a.t);
}

/** Own asset gone stale (FEATURES D6). Only commandable fleet types raise it. */
export function alertFromStaleAsset(rec: EntityRecord, stale: boolean): AlertItem | null {
  if (!stale || !["drone", "robot", "vehicle"].includes(rec.r.entity_type)) return null;
  const name = rec.r.callsign ?? rec.r.entity_id;
  return {
    key: `stale:${rec.r.entity_id}`,
    level: "caution",
    kind: "stale asset",
    title: `${name} has not reported for ${Math.round((Date.now() - rec.r.t) / 1000)} s`,
    about: rec.r.entity_id,
    t: rec.r.t,
    source: "stale",
    state: "open",
  };
}

/** Feed outages from the intelligence-gaps model (FEATURES B20, D6). */
export function alertsFromGaps(gaps: { key: string; source: string; layers: string[]; reason: string; detail: string; since: number }[]): AlertItem[] {
  return gaps.map((g) => ({
    key: `feed:${g.key}`,
    level: g.reason === "error" || g.reason === "layer_down" ? "warning" : "caution",
    kind: "feed outage",
    title: `${g.source} ${g.reason.replace("_", " ")}: ${g.layers.join(", ") || "no layer"} weakened`,
    t: g.since,
    source: "feed" as const,
    state: "open",
  }));
}
