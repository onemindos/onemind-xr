// Drawings and tactical graphics (FEATURES B11) and their geofence form (FEATURES D1).
// A drawing is client state until the operator shares it: as a zone through
// `cmd.map.geofence.{id}.create` (omos docs/NATS-DOCTRINE.md §9.7), or as CoT XML (cot.ts).
import {
  bearingDeg,
  bufferLine,
  bufferPolygon,
  centroid,
  circleRing,
  distanceM,
  lineLengthM,
  pointInRing,
  rectangleRing,
  ringAreaM2,
  sectorRing,
  type LonLat,
} from "./geo";

export type ShapeKind =
  | "point"
  | "label"
  | "line"
  | "polygon"
  | "rectangle"
  | "circle"
  | "sector"
  | "rings"
  | "rangebearing"
  | "buffer"
  | "lasso";

/** Clicks needed per tool; `min` + finish for open-ended tools. */
export const TOOL_POINTS: Record<ShapeKind, { exact?: number; min?: number; hint: string }> = {
  point: { exact: 1, hint: "Click the point." },
  label: { exact: 1, hint: "Click where the label goes." },
  circle: { exact: 2, hint: "Click the centre, then a point on the edge." },
  rings: { exact: 2, hint: "Click the centre, then the first ring's radius (3 rings)." },
  rangebearing: { exact: 2, hint: "Click the origin, then the target." },
  rectangle: { exact: 2, hint: "Click two opposite corners." },
  sector: { exact: 3, hint: "Click the centre, the start edge, then the end edge (clockwise)." },
  line: { min: 2, hint: "Click each vertex, then Finish." },
  polygon: { min: 3, hint: "Click each vertex, then Finish." },
  lasso: { min: 3, hint: "Click around the entities to select, then Finish." },
  buffer: { exact: 0, hint: "Select a drawing, set the distance, then Buffer." },
};

export interface Drawing {
  id: string;
  kind: Exclude<ShapeKind, "lasso">;
  label: string;
  points: LonLat[];
  /** rings: number of rings (default 3) */
  rings?: number;
  /** buffer: distance and the drawing it was made from */
  bufferM?: number;
  sourceId?: string;
  createdT: number;
}

export type ZoneKind = "aoi" | "keep_in" | "keep_out" | "hazard" | "control_area";
export const ZONE_KINDS: ZoneKind[] = ["aoi", "keep_in", "keep_out", "hazard", "control_area"];
/** Kinds that constrain asset movement; the client puts these behind a confirm. */
export const SAFETY_KINDS = new Set<ZoneKind>(["keep_in", "keep_out"]);

export function radiusM(d: Drawing): number {
  return d.points.length >= 2 ? distanceM(d.points[0], d.points[1]) : 0;
}

/** Area rings (closed) the drawing covers. Empty for points, labels, lines and R&B. */
export function areaRings(d: Drawing): LonLat[][] {
  switch (d.kind) {
    case "polygon":
    case "buffer":
      return d.points.length >= 3 ? [closeRing(d.points)] : [];
    case "rectangle":
      return d.points.length === 2 ? [rectangleRing(d.points[0], d.points[1])] : [];
    case "circle":
      return d.points.length === 2 ? [circleRing(d.points[0], radiusM(d))] : [];
    case "sector":
      return d.points.length === 3
        ? [sectorRing(d.points[0], radiusM(d), bearingDeg(d.points[0], d.points[1]), bearingDeg(d.points[0], d.points[2]))]
        : [];
    case "rings": {
      if (d.points.length !== 2) return [];
      const r = radiusM(d);
      return Array.from({ length: d.rings ?? 3 }, (_, i) => circleRing(d.points[0], r * (i + 1)));
    }
    default:
      return [];
  }
}

/** Polylines (open) the drawing shows. */
export function lines(d: Drawing): LonLat[][] {
  if (d.kind === "line" || d.kind === "rangebearing") return d.points.length >= 2 ? [d.points] : [];
  return [];
}

export function closeRing(pts: LonLat[]): LonLat[] {
  const f = pts[0];
  const l = pts[pts.length - 1];
  return f[0] === l[0] && f[1] === l[1] ? pts : [...pts, [f[0], f[1]]];
}

/** True when the drawing is one area that can become a geofence zone. */
export function canBeZone(d: Drawing): boolean {
  return ["polygon", "rectangle", "circle", "sector", "buffer"].includes(d.kind) && areaRings(d).length === 1;
}

export interface Measure {
  distanceM?: number;
  bearingDeg?: number;
  areaKm2?: number;
  radiusM?: number;
}

export function measure(d: Drawing): Measure {
  if (d.kind === "rangebearing" && d.points.length === 2)
    return { distanceM: distanceM(d.points[0], d.points[1]), bearingDeg: bearingDeg(d.points[0], d.points[1]) };
  if (d.kind === "line") return { distanceM: lineLengthM(d.points) };
  if (d.kind === "circle" || d.kind === "rings" || d.kind === "sector") {
    const rings = areaRings(d);
    return { radiusM: radiusM(d), areaKm2: rings.length ? ringAreaM2(rings[rings.length - 1]) / 1e6 : undefined };
  }
  const rings = areaRings(d);
  return rings.length ? { areaKm2: ringAreaM2(rings[0]) / 1e6 } : {};
}

/** Buffer a drawing into a new polygon drawing. */
export function buffer(d: Drawing, distM: number, id: string, now: number): Drawing | null {
  if (!(distM > 0)) return null;
  let ring: LonLat[];
  if (d.kind === "point" || d.kind === "label") ring = circleRing(d.points[0], distM);
  else if (d.kind === "line" || d.kind === "rangebearing") ring = bufferLine(d.points, distM);
  else {
    const rings = areaRings(d);
    if (!rings.length) return null;
    ring = bufferPolygon(rings[rings.length - 1], distM);
  }
  return { id, kind: "buffer", label: `${d.label} +${Math.round(distM)} m`, points: ring, bufferM: distM, sourceId: d.id, createdT: now };
}

/** Ids of the located items inside a lasso ring. */
export function lassoSelect(ring: LonLat[], items: Iterable<{ id: string; lon: number; lat: number }>): string[] {
  const closed = closeRing(ring);
  const out: string[] = [];
  for (const it of items) if (pointInRing([it.lon, it.lat], closed)) out.push(it.id);
  return out;
}

export function anchor(d: Drawing): LonLat {
  if (d.kind === "circle" || d.kind === "rings" || d.kind === "sector" || d.kind === "point" || d.kind === "label") return d.points[0];
  const r = areaRings(d);
  return centroid(r.length ? r[0] : d.points);
}

/** A zone id from a drawing id: KV keys and subject tokens allow [A-Za-z0-9_-] only. */
export function zoneIdFor(d: Drawing): string {
  return `zone-${d.id.replace(/[^A-Za-z0-9_-]/g, "")}`.slice(0, 64);
}

/**
 * Body of `cmd.map.geofence.{id}.create|update` (owner proposal WEB-P1). It carries the doctrine
 * §9.7 fields (`polygon`, `watch`, `alert`) and the zone record that KV `geofences` stores, which
 * is the shape the policy service (omos #554 `ParseZone`) and the projector (omos #563 A6-P2)
 * read: `geo.shape` is `polygon` or `circle` only, because the policy treats any other keep-in or
 * keep-out shape as unevaluable and denies movement.
 */
export function zoneBody(
  d: Drawing,
  kind: ZoneKind,
  opts: { now: number; appliesTo?: string[]; watch?: boolean; createdBy?: string },
): Record<string, unknown> | null {
  if (!canBeZone(d)) return null;
  const id = zoneIdFor(d);
  const isCircle = d.kind === "circle";
  const ring = areaRings(d)[0];
  const c = isCircle ? d.points[0] : centroid(ring);
  const geo = isCircle
    ? { shape: "circle", kind, coords: { type: "Point", coordinates: [c[0], c[1]] }, radius_m: Math.round(radiusM(d) * 10) / 10 }
    : { shape: "polygon", kind, coords: { type: "Polygon", coordinates: [ring.map(([lon, lat]) => [round6(lon), round6(lat)])] } };
  return {
    v: 1,
    polygon: ring.map(([lon, lat]) => [round6(lon), round6(lat)]),
    watch: opts.watch ?? true,
    alert: "evt.map.geofence",
    zone: {
      entity_id: id,
      entity_type: "zone",
      v: 1,
      live: true,
      callsign: d.label,
      t: opts.now,
      lat: round6(c[1]),
      lon: round6(c[0]),
      geo,
      properties: { applies_to: opts.appliesTo ?? [], drawn_as: d.kind, created_by: opts.createdBy ?? null },
    },
  };
}

const round6 = (n: number) => Math.round(n * 1e6) / 1e6;
