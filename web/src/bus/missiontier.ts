// Mission tier: the entities that matter to a mission. Rule from docs/FEATURES.md A6 and
// docs/design/REFERENCE-ARCHITECTURE.md §5 ("Mission tier to TAK"): an entity is in the mission
// tier when it is inside an AOI or geofence, or tasked. The projector applies the same rule (plus
// `egress_tak` and marking) for TAK; L0 applies it as a view filter only.
import type { EntityRecord } from "./store";
import type { Entity } from "./types";

/** Zone kinds that define the mission tier (entity.md §4.1 `geo.kind`). */
export const MISSION_ZONE_KINDS = new Set(["aoi", "keep_in", "keep_out", "hazard", "control_area"]);

const TERMINAL_TASK = new Set(["succeeded", "failed", "cancelled", "canceled", "rejected", "done"]);

const R_EARTH = 6_371_008.8;

export function haversineM(lat1: number, lon1: number, lat2: number, lon2: number): number {
  const toRad = Math.PI / 180;
  const dLat = (lat2 - lat1) * toRad;
  const dLon = (lon2 - lon1) * toRad;
  const a =
    Math.sin(dLat / 2) ** 2 + Math.cos(lat1 * toRad) * Math.cos(lat2 * toRad) * Math.sin(dLon / 2) ** 2;
  return 2 * R_EARTH * Math.asin(Math.min(1, Math.sqrt(a)));
}

/** Ray casting on a GeoJSON ring of [lon, lat] (adequate for AOI-sized polygons). */
export function pointInRing(lon: number, lat: number, ring: number[][]): boolean {
  let inside = false;
  for (let i = 0, j = ring.length - 1; i < ring.length; j = i++) {
    const [xi, yi] = ring[i];
    const [xj, yj] = ring[j];
    if (yi > lat !== yj > lat && lon < ((xj - xi) * (lat - yi)) / (yj - yi) + xi) inside = !inside;
  }
  return inside;
}

/** True when (lat, lon) is inside the zone's geometry. Lines and corridors never contain points. */
export function zoneContains(zone: Entity, lat: number, lon: number): boolean {
  const g = zone.geo;
  if (!g || !g.coords) return false;
  const c = g.coords as { type: string; coordinates: unknown };
  if (g.shape === "circle" && c.type === "Point" && typeof g.radius_m === "number") {
    const [clon, clat] = c.coordinates as number[];
    return haversineM(clat, clon, lat, lon) <= g.radius_m;
  }
  if (c.type === "Polygon") {
    const rings = c.coordinates as number[][][];
    if (!rings.length || !pointInRing(lon, lat, rings[0])) return false;
    return !rings.slice(1).some((h) => pointInRing(lon, lat, h));
  }
  if (c.type === "MultiPolygon") {
    return (c.coordinates as number[][][][]).some(
      (p) => p.length > 0 && pointInRing(lon, lat, p[0]) && !p.slice(1).some((h) => pointInRing(lon, lat, h)),
    );
  }
  return false;
}

export function isMissionZone(rec: EntityRecord): boolean {
  return rec.r.entity_type === "zone" && !!rec.full?.geo && MISSION_ZONE_KINDS.has(rec.full.geo.kind ?? "");
}

export function isTasked(full: Entity | undefined): boolean {
  if (!full) return false;
  if (full.current_task && !TERMINAL_TASK.has(String(full.current_task.state))) return true;
  if (typeof full.route?.mission_id === "string" && full.route.mission_id) return true;
  const mid = full.meta?.["mission_id"];
  return typeof mid === "string" && mid.length > 0;
}

/**
 * Mission-tier membership. Zones of a mission kind and open alerts are always part of the
 * mission view (they define it and demand attention).
 */
export function inMissionTier(rec: EntityRecord, zones: EntityRecord[]): boolean {
  const t = rec.r.entity_type;
  if (t === "alert") return true;
  if (t === "zone") return isMissionZone(rec);
  if (isTasked(rec.full)) return true;
  return zones.some((z) => z.full && zoneContains(z.full, rec.r.lat, rec.r.lon));
}
