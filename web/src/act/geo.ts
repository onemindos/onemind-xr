// Spherical geodesy for drawing tools (FEATURES B11). Positions are [lon, lat] in degrees, like
// GeoJSON. A sphere of the IUGG mean radius is accurate to about 0.5% for the distances drawn on
// a tactical map; the client labels measurements as approximate.
export type LonLat = [number, number];

export const EARTH_R = 6_371_008.8;
const rad = (d: number) => (d * Math.PI) / 180;
const deg = (r: number) => (r * 180) / Math.PI;
const norm360 = (d: number) => ((d % 360) + 360) % 360;

export function distanceM(a: LonLat, b: LonLat): number {
  const dLat = rad(b[1] - a[1]);
  const dLon = rad(b[0] - a[0]);
  const s = Math.sin(dLat / 2) ** 2 + Math.cos(rad(a[1])) * Math.cos(rad(b[1])) * Math.sin(dLon / 2) ** 2;
  return 2 * EARTH_R * Math.asin(Math.min(1, Math.sqrt(s)));
}

/** Initial great-circle bearing from a to b, degrees clockwise from true north, [0, 360). */
export function bearingDeg(a: LonLat, b: LonLat): number {
  const φ1 = rad(a[1]);
  const φ2 = rad(b[1]);
  const Δλ = rad(b[0] - a[0]);
  const y = Math.sin(Δλ) * Math.cos(φ2);
  const x = Math.cos(φ1) * Math.sin(φ2) - Math.sin(φ1) * Math.cos(φ2) * Math.cos(Δλ);
  return norm360(deg(Math.atan2(y, x)));
}

export function destination(p: LonLat, bearing: number, distM: number): LonLat {
  const δ = distM / EARTH_R;
  const θ = rad(bearing);
  const φ1 = rad(p[1]);
  const λ1 = rad(p[0]);
  const φ2 = Math.asin(Math.sin(φ1) * Math.cos(δ) + Math.cos(φ1) * Math.sin(δ) * Math.cos(θ));
  const λ2 = λ1 + Math.atan2(Math.sin(θ) * Math.sin(δ) * Math.cos(φ1), Math.cos(δ) - Math.sin(φ1) * Math.sin(φ2));
  return [((deg(λ2) + 540) % 360) - 180, deg(φ2)];
}

function close(ring: LonLat[]): LonLat[] {
  if (!ring.length) return ring;
  const [f, l] = [ring[0], ring[ring.length - 1]];
  return f[0] === l[0] && f[1] === l[1] ? ring : [...ring, [f[0], f[1]]];
}

/** Closed ring approximating a circle (n segments). */
export function circleRing(center: LonLat, radiusM: number, n = 64): LonLat[] {
  const ring: LonLat[] = [];
  for (let i = 0; i < n; i++) ring.push(destination(center, (360 * i) / n, radiusM));
  return close(ring);
}

/** Closed ring of a sector (pie slice) from `startDeg` clockwise to `endDeg`. */
export function sectorRing(center: LonLat, radiusM: number, startDeg: number, endDeg: number, n = 32): LonLat[] {
  let sweep = norm360(endDeg - startDeg);
  if (sweep === 0) sweep = 360;
  const ring: LonLat[] = [center];
  for (let i = 0; i <= n; i++) ring.push(destination(center, startDeg + (sweep * i) / n, radiusM));
  return close(ring);
}

/** Axis-aligned rectangle from two opposite corners, closed, counter-clockwise from south-west. */
export function rectangleRing(a: LonLat, b: LonLat): LonLat[] {
  const [w, e] = [Math.min(a[0], b[0]), Math.max(a[0], b[0])];
  const [s, n] = [Math.min(a[1], b[1]), Math.max(a[1], b[1])];
  return [[w, s], [e, s], [e, n], [w, n], [w, s]];
}

export function pointInRing(p: LonLat, ring: LonLat[]): boolean {
  let inside = false;
  for (let i = 0, j = ring.length - 1; i < ring.length; j = i++) {
    const [xi, yi] = ring[i];
    const [xj, yj] = ring[j];
    if (yi > p[1] !== yj > p[1] && p[0] < ((xj - xi) * (p[1] - yi)) / (yj - yi) + xi) inside = !inside;
  }
  return inside;
}

/** Spherical polygon area in m² (closed or open ring), from the l'Huilier-free shoelace on a sphere. */
export function ringAreaM2(ring: LonLat[]): number {
  const r = close(ring);
  let sum = 0;
  for (let i = 0; i < r.length - 1; i++) {
    const [λ1, φ1] = [rad(r[i][0]), rad(r[i][1])];
    const [λ2, φ2] = [rad(r[i + 1][0]), rad(r[i + 1][1])];
    sum += (λ2 - λ1) * (2 + Math.sin(φ1) + Math.sin(φ2));
  }
  return Math.abs((sum * EARTH_R * EARTH_R) / 2);
}

export function lineLengthM(points: LonLat[]): number {
  let d = 0;
  for (let i = 1; i < points.length; i++) d += distanceM(points[i - 1], points[i]);
  return d;
}

/** Convex hull (Andrew's monotone chain) in lon/lat, closed. Fine at tactical scales. */
export function convexHull(pts: LonLat[]): LonLat[] {
  const p = [...pts].sort((a, b) => a[0] - b[0] || a[1] - b[1]);
  if (p.length < 3) return close(p);
  const cross = (o: LonLat, a: LonLat, b: LonLat) => (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0]);
  const lower: LonLat[] = [];
  for (const q of p) {
    while (lower.length >= 2 && cross(lower[lower.length - 2], lower[lower.length - 1], q) <= 0) lower.pop();
    lower.push(q);
  }
  const upper: LonLat[] = [];
  for (const q of p.reverse()) {
    while (upper.length >= 2 && cross(upper[upper.length - 2], upper[upper.length - 1], q) <= 0) upper.pop();
    upper.push(q);
  }
  return close([...lower.slice(0, -1), ...upper.slice(0, -1)]);
}

/**
 * Buffer a line by `d` metres: left offsets out, a round cap, right offsets back, a round cap.
 * Exact for straight segments; at sharp bends the inner side can self-overlap (still covers the
 * true buffer, never less).
 */
export function bufferLine(points: LonLat[], d: number, capN = 12): LonLat[] {
  if (points.length === 1) return circleRing(points[0], d);
  const left: LonLat[] = [];
  const right: LonLat[] = [];
  for (let i = 0; i < points.length; i++) {
    const bIn = i > 0 ? bearingDeg(points[i - 1], points[i]) : bearingDeg(points[0], points[1]);
    const bOut = i < points.length - 1 ? bearingDeg(points[i], points[i + 1]) : bIn;
    const delta = norm360(bOut - bIn);
    const turn = delta > 180 ? delta - 360 : delta; // signed turn, (-180, 180]
    const mid = bIn + turn / 2; // bisector heading
    const k = 1 / Math.max(0.25, Math.cos(rad(turn / 2))); // miter length, capped at 4×
    left.push(destination(points[i], mid - 90, d * k));
    right.push(destination(points[i], mid + 90, d * k));
  }
  const endB = bearingDeg(points[points.length - 2], points[points.length - 1]);
  const startB = bearingDeg(points[0], points[1]);
  const capEnd: LonLat[] = [];
  for (let i = 1; i < capN; i++) capEnd.push(destination(points[points.length - 1], endB - 90 + (180 * i) / capN, d));
  const capStart: LonLat[] = [];
  for (let i = 1; i < capN; i++) capStart.push(destination(points[0], startB + 90 + (180 * i) / capN, d));
  return close([...left, ...capEnd, ...right.reverse(), ...capStart]);
}

/**
 * Buffer a polygon by `d` metres as the convex hull of circles around its vertices. Exact for
 * convex polygons; for a concave polygon it over-covers the notches (never under-covers), which is
 * the safe side for a keep-out buffer. Stated in the UI as "convex buffer".
 */
export function bufferPolygon(ring: LonLat[], d: number): LonLat[] {
  const pts: LonLat[] = [];
  for (const v of ring) pts.push(...circleRing(v, d, 24));
  return convexHull(pts);
}

export function centroid(points: LonLat[]): LonLat {
  const open = points.length > 1 && points[0][0] === points[points.length - 1][0] && points[0][1] === points[points.length - 1][1]
    ? points.slice(0, -1)
    : points;
  const n = open.length || 1;
  return [open.reduce((s, p) => s + p[0], 0) / n, open.reduce((s, p) => s + p[1], 0) / n];
}
