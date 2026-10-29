// Turn whatever arrives (a full entity from `ent.*.*.state` / `svc.entity.*`, or a render
// entity from `svc.layer.*`) into one render record, and decide what the viewer may see.
import type { Entity, Marking, RenderEntity } from "./types";

const ID_RE = /^[A-Za-z0-9_-]{1,64}$/; // layer-snapshot.md §4 (entity.md §3 is the lowercase subset)

const num = (v: unknown): number | undefined =>
  typeof v === "number" && Number.isFinite(v) ? v : undefined;

/**
 * Normalize a record to a render entity. Returns null when the record cannot be applied
 * (missing id or seq): such records are counted by the caller, never guessed.
 */
export function toRender(raw: unknown): RenderEntity | null {
  if (!raw || typeof raw !== "object") return null;
  const e = raw as Entity;
  const id = typeof e.entity_id === "string" ? e.entity_id : typeof e.uid === "string" ? e.uid : "";
  if (!ID_RE.test(id)) return null;
  const seq = num(e.seq);
  if (seq === undefined) return null;
  const loc = e.location ?? {};
  const lat = num(loc.lat) ?? num(e.lat);
  const lon = num(loc.lon) ?? num(e.lon);
  const props = e.properties ?? {};
  const callsign =
    (typeof e.callsign === "string" && e.callsign) ||
    (typeof e.aliases?.name === "string" && e.aliases.name) ||
    (typeof props["callsign"] === "string" && (props["callsign"] as string)) ||
    id;
  return {
    entity_id: id,
    entity_type: typeof e.entity_type === "string" ? e.entity_type : "unknown",
    lat: lat ?? 0,
    lon: lon ?? 0,
    hae: num(loc.hae_m) ?? num(e.hae),
    course: num(loc.course_deg) ?? num(e.course),
    speed: num(loc.speed_mps) ?? num(e.speed),
    callsign,
    cot_type: typeof e.cot_type === "string" ? e.cot_type : undefined,
    disposition: typeof e.disposition === "string" ? e.disposition : undefined,
    t: num(e.t) ?? 0,
    stale: num(e.stale),
    expiry_t: e.expiry_t === null ? null : num(e.expiry_t),
    seq,
    live: e.live !== false,
    origin: typeof e.origin === "string" ? e.origin : undefined,
    simulated:
      typeof e.simulated === "boolean" ? e.simulated : e.indicators?.simulated === true,
  };
}

/** True when the record carries more than the render subset (entity.md §4.1 group C). */
export function isFull(raw: unknown): raw is Entity {
  return !!raw && typeof raw === "object" && "v" in raw && ("provenance" in raw || "sources" in raw);
}

/**
 * Locatable: entity.md §4.1 says non-located types use the reserved `lat: 0, lon: 0` without the
 * `locatable` trait, and clients must not draw them. A render entity has no traits, so 0,0 is
 * treated as "not located".
 */
export function isLocatable(r: RenderEntity, full?: Entity): boolean {
  if (full?.traits && Array.isArray(full.traits)) {
    if (!full.traits.includes("locatable") && full.geo == null) return false;
  }
  if (full?.geo) return true;
  if (r.lat === 0 && r.lon === 0) return false;
  return Math.abs(r.lat) <= 90 && Math.abs(r.lon) <= 180;
}

/**
 * Defence-in-depth marking check (entity.md §8: "clients drop records whose marking the viewer
 * may not see"; the release service and the account boundary are the real control).
 * CLEAR and GREEN are visible to everyone; other TLP levels need a shared group when
 * `releasable_to` is set.
 */
export function viewerMaySee(marking: Marking | undefined, viewerGroups: string[]): boolean {
  if (!marking) return true;
  const tlp = (marking.tlp ?? "CLEAR").toUpperCase();
  if (tlp === "CLEAR" || tlp === "GREEN") return true;
  const rel = marking.releasable_to ?? [];
  if (rel.length === 0) return tlp !== "RED";
  return rel.some((g) => viewerGroups.includes(g));
}
