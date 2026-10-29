// Map overlay for the Act features: drawings (B11), the drawing preview, Hermes highlights and
// annotations from `cmd.map.>` (D4), and task destinations (D3). These are few objects, so they
// use Cesium Entities; the many-entity path stays on primitive collections in globe.ts.
import {
  CallbackProperty,
  Cartesian2,
  Cartesian3,
  Color,
  HeightReference,
  LabelStyle,
  Material,
  PolygonHierarchy,
  PolylineCollection,
  VerticalOrigin,
  type Entity as CEntity,
  type Viewer,
} from "cesium";
import { anchor, areaRings, lines, measure, type Drawing } from "../act/drawing";
import { circleRing, type LonLat } from "../act/geo";

const DRAW = Color.fromCssColorString("#a78bfa");
const PREVIEW = Color.fromCssColorString("#fde047");

const pos = ([lon, lat]: LonLat, h = 2) => Cartesian3.fromDegrees(lon, lat, h);
/** A small marker ring (12 m) around each clicked preview point. */
const circleRingSmall = (p: LonLat) => circleRing(p, 12, 12);

function fmtMeasure(d: Drawing): string {
  const m = measure(d);
  const parts: string[] = [];
  if (m.distanceM !== undefined) parts.push(m.distanceM >= 1000 ? `${(m.distanceM / 1000).toFixed(2)} km` : `${Math.round(m.distanceM)} m`);
  if (m.bearingDeg !== undefined) parts.push(`${m.bearingDeg.toFixed(1)}°T`);
  if (m.radiusM !== undefined) parts.push(`r ${Math.round(m.radiusM)} m`);
  if (m.areaKm2 !== undefined && m.distanceM === undefined) parts.push(`${m.areaKm2.toFixed(3)} km²`);
  return parts.join(" · ");
}

export class Overlay {
  private drawings = new Map<string, CEntity[]>();
  private highlights = new Map<string, CEntity>();
  private annotations = new Map<string, CEntity>();
  private targets = new Map<string, CEntity>();
  /**
   * The drawing preview is a primitive collection, not Entities: entity geometry is batched and
   * rebuilt asynchronously, and a preview rebuilt on every pointer move left a stale batch on
   * screen after the tool finished (seen in the B11 e2e). Polylines in a collection are added and
   * removed synchronously.
   */
  private preview: PolylineCollection;

  constructor(private viewer: Viewer, private positionOf: (id: string) => LonLat | null) {
    this.preview = viewer.scene.primitives.add(new PolylineCollection());
  }

  private kickTimer: ReturnType<typeof setTimeout> | undefined;

  /**
   * Request frames for a moment after a change. Entity geometry is batched and rebuilt
   * asynchronously, and in request-render mode nothing draws the finished batch until the next
   * request, so a removed shape could stay on screen for up to the app's 1 s render tick.
   */
  private kick(): void {
    this.viewer.scene.requestRender();
    if (this.kickTimer) return;
    let n = 0;
    const tick = () => {
      this.viewer.scene.requestRender();
      this.kickTimer = ++n < 8 ? setTimeout(tick, 80) : undefined;
    };
    this.kickTimer = setTimeout(tick, 80);
  }

  private add(e: Parameters<Viewer["entities"]["add"]>[0]): CEntity {
    const ent = this.viewer.entities.add(e);
    this.kick();
    return ent;
  }

  private drop(list: CEntity[]): void {
    for (const e of list) this.viewer.entities.remove(e);
    this.kick();
  }

  /** Entities for one drawing (areas, lines, a label with the measurement). */
  private build(d: Drawing, color: Color, prefix: string): CEntity[] {
    const out: CEntity[] = [];
    for (const ring of areaRings(d)) {
      out.push(
        this.add({
          id: `${prefix}${d.id}:a${out.length}`,
          polygon: { hierarchy: new PolygonHierarchy(ring.map((p) => pos(p, 0))), material: color.withAlpha(d.kind === "rings" ? 0.04 : 0.15) },
          polyline: { positions: ring.map((p) => pos(p)), width: 2, material: color },
        }),
      );
    }
    for (const l of lines(d)) out.push(this.add({ id: `${prefix}${d.id}:l${out.length}`, polyline: { positions: l.map((p) => pos(p)), width: 3, material: color } }));
    if (d.points.length) {
      const text = d.kind === "label" ? d.label : [d.label, fmtMeasure(d)].filter(Boolean).join("\n");
      out.push(
        this.add({
          id: `${prefix}${d.id}:p`,
          position: pos(anchor(d), 3),
          point: d.kind === "label" ? undefined : { pixelSize: 8, color, outlineColor: Color.BLACK, outlineWidth: 1, disableDepthTestDistance: Infinity },
          label: {
            text,
            font: d.kind === "label" ? "700 15px system-ui, sans-serif" : "600 12px system-ui, sans-serif",
            fillColor: Color.WHITE,
            outlineColor: Color.BLACK,
            outlineWidth: 3,
            style: LabelStyle.FILL_AND_OUTLINE,
            pixelOffset: new Cartesian2(0, -14),
            verticalOrigin: VerticalOrigin.BOTTOM,
            disableDepthTestDistance: Infinity,
          },
        }),
      );
    }
    return out;
  }

  setDrawing(d: Drawing): void {
    this.removeDrawing(d.id);
    this.drawings.set(d.id, this.build(d, DRAW, "draw:"));
  }

  removeDrawing(id: string): void {
    const old = this.drawings.get(id);
    if (old) this.drop(old);
    this.drawings.delete(id);
  }

  setPreview(d: Drawing | null): void {
    this.preview.removeAll();
    if (d && d.points.length) {
      // One material per polyline: a Polyline destroys its material when it is removed.
      const mat = () => Material.fromType("Color", { color: PREVIEW });
      const paths = [...areaRings(d), ...lines(d)];
      // Too few points for a shape yet: join the clicked points so the operator sees progress.
      if (!paths.length && d.points.length >= 2) paths.push(d.points);
      for (const path of paths) this.preview.add({ positions: path.map((p) => pos(p, 4)), width: 2, material: mat() });
      for (const p of d.points) {
        const c = circleRingSmall(p);
        this.preview.add({ positions: c.map((q) => pos(q, 4)), width: 2, material: mat() });
      }
    }
    this.kick();
  }

  /** `cmd.map.highlight.{uid}`: a ring that follows the entity. */
  highlight(uid: string, color: string, label?: string): void {
    this.unhighlight(uid);
    const c = Color.fromCssColorString(color);
    this.highlights.set(
      uid,
      this.add({
        id: `hl:${uid}`,
        position: new CallbackProperty(() => {
          const p = this.positionOf(uid);
          return p ? pos(p, 4) : undefined;
        }, false) as never,
        point: { pixelSize: 30, color: Color.TRANSPARENT, outlineColor: c, outlineWidth: 4, disableDepthTestDistance: Infinity },
        label: label
          ? { text: label, font: "700 12px system-ui", fillColor: c, outlineColor: Color.BLACK, outlineWidth: 3, style: LabelStyle.FILL_AND_OUTLINE, pixelOffset: new Cartesian2(0, 26), disableDepthTestDistance: Infinity }
          : undefined,
      }),
    );
  }

  unhighlight(uid: string): void {
    const e = this.highlights.get(uid);
    if (e) this.drop([e]);
    this.highlights.delete(uid);
  }

  clearHighlights(): void {
    for (const uid of [...this.highlights.keys()]) this.unhighlight(uid);
  }

  get highlighted(): string[] {
    return [...this.highlights.keys()];
  }

  /** `cmd.map.annotation.{id}.{create|update|remove}`. */
  annotate(id: string, verb: string, a: { lat?: number; lon?: number; label?: string; color?: string; note?: string }): void {
    const old = this.annotations.get(id);
    if (old) this.drop([old]);
    this.annotations.delete(id);
    if (verb === "remove" || a.lat === undefined || a.lon === undefined) return;
    const c = Color.fromCssColorString(a.color ?? "#FF4FD8");
    this.annotations.set(
      id,
      this.add({
        id: `ann:${id}`,
        position: pos([a.lon, a.lat], 3),
        point: { pixelSize: 10, color: c, outlineColor: Color.BLACK, outlineWidth: 1, heightReference: HeightReference.NONE, disableDepthTestDistance: Infinity },
        label: { text: a.label ?? "", font: "600 12px system-ui", fillColor: Color.WHITE, outlineColor: Color.BLACK, outlineWidth: 3, style: LabelStyle.FILL_AND_OUTLINE, pixelOffset: new Cartesian2(0, -16), disableDepthTestDistance: Infinity },
      }),
    );
  }

  get previewCount(): number {
    return this.preview.length;
  }

  get annotationIds(): string[] {
    return [...this.annotations.keys()];
  }

  /** Destination of a goto (pending, dispatched or done). */
  target(actionId: string, at: LonLat | null, text: string, done = false): void {
    const old = this.targets.get(actionId);
    if (old) this.drop([old]);
    this.targets.delete(actionId);
    if (!at) return;
    const c = Color.fromCssColorString(done ? "#86efac" : "#f472b6");
    this.targets.set(
      actionId,
      this.add({
        id: `tgt:${actionId}`,
        position: pos(at, 3),
        point: { pixelSize: 14, color: c.withAlpha(0.35), outlineColor: c, outlineWidth: 3, disableDepthTestDistance: Infinity },
        label: { text, font: "600 12px system-ui", fillColor: c, outlineColor: Color.BLACK, outlineWidth: 3, style: LabelStyle.FILL_AND_OUTLINE, pixelOffset: new Cartesian2(0, 18), disableDepthTestDistance: Infinity },
      }),
    );
  }
}
