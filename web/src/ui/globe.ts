// CesiumJS globe with open imagery and no ion token. Entities render through primitive
// collections (one PointPrimitive + one Label per entity, batched by Cesium), not one Entity
// object each (onemind-xr CLAUDE.md "Engineering conventions"). Zones (few) use Cesium Entities.
import {
  buildModuleUrl,
  Cartesian2,
  Cartesian3,
  Color,
  defined,
  EllipsoidTerrainProvider,
  HorizontalOrigin,
  ImageryLayer,
  LabelCollection,
  LabelStyle,
  Math as CMath,
  NearFarScalar,
  OpenStreetMapImageryProvider,
  PointPrimitiveCollection,
  PolygonHierarchy,
  Rectangle,
  SceneMode,
  ScreenSpaceEventHandler,
  ScreenSpaceEventType,
  TileMapServiceImageryProvider,
  VerticalOrigin,
  Viewer,
  type Entity as CEntity,
  type Label,
  type PointPrimitive,
} from "cesium";
import type { EntityRecord } from "../bus/store";
import type { Config } from "../config";
import { terrariumTerrain } from "./terrain";

const DISPOSITION_COLOR: Record<string, string> = {
  friendly: "#38bdf8",
  assumed_friendly: "#7dd3fc",
  neutral: "#4ade80",
  unknown: "#facc15",
  suspicious: "#fb923c",
  pending: "#c4b5fd",
};

export function colorFor(rec: EntityRecord): string {
  if (rec.r.entity_type === "alert") return "#f87171";
  return DISPOSITION_COLOR[rec.r.disposition ?? ""] ?? "#e5e7eb";
}

interface Drawn {
  point: PointPrimitive;
  label: Label;
}

export class Globe {
  readonly viewer: Viewer;
  private points: PointPrimitiveCollection;
  private labels: LabelCollection;
  private drawn = new Map<string, Drawn>();
  private zones = new Map<string, CEntity>();
  private zoneSeq = new Map<string, number>();
  private selected: string | null = null;
  private clampZones: boolean;

  constructor(container: HTMLElement, cfg: Config, onPick: (id: string | null) => void) {
    this.clampZones = cfg.terrain === "terrarium";
    const base =
      cfg.imagery === "osm"
        ? ImageryLayer.fromProviderAsync(
            // OSM tile policy: attribution required, light use only
            // (https://operations.osmfoundation.org/policies/tiles/).
            Promise.resolve(
              new OpenStreetMapImageryProvider({
                url: "https://tile.openstreetmap.org/",
                credit: "© OpenStreetMap contributors",
              }),
            ),
            {},
          )
        : ImageryLayer.fromProviderAsync(
            TileMapServiceImageryProvider.fromUrl(buildModuleUrl("Assets/Textures/NaturalEarthII")),
            {},
          );
    this.viewer = new Viewer(container, {
      baseLayer: base,
      terrainProvider: cfg.terrain === "terrarium" ? terrariumTerrain() : new EllipsoidTerrainProvider(),
      baseLayerPicker: false,
      geocoder: false, // the default geocoder uses Cesium ion
      animation: false,
      timeline: false,
      fullscreenButton: false,
      homeButton: false,
      infoBox: false,
      selectionIndicator: false,
      navigationHelpButton: false,
      sceneModePicker: false,
      requestRenderMode: true,
      maximumRenderTimeChange: Infinity,
    });
    const scene = this.viewer.scene;
    scene.globe.enableLighting = false;
    this.points = scene.primitives.add(new PointPrimitiveCollection());
    this.labels = scene.primitives.add(new LabelCollection());
    const h = new ScreenSpaceEventHandler(scene.canvas);
    h.setInputAction((e: { position: Cartesian2 }) => {
      const p = scene.pick(e.position);
      const raw: unknown = defined(p) ? (typeof p.id === "string" ? p.id : p.id?.id) : undefined;
      onPick(typeof raw === "string" ? (raw.startsWith("zone:") ? raw.slice(5) : raw) : null);
    }, ScreenSpaceEventType.LEFT_CLICK);
    if (cfg.view) this.flyTo(cfg.view.lon, cfg.view.lat, cfg.view.height, 0);
  }

  flyTo(lon: number, lat: number, height: number, duration = 1.2): void {
    this.viewer.camera.flyTo({
      destination: Cartesian3.fromDegrees(lon, lat, height),
      orientation: { heading: 0, pitch: CMath.toRadians(-90), roll: 0 },
      duration,
    });
  }

  flyToRect(west: number, south: number, east: number, north: number, duration = 0): void {
    this.viewer.camera.flyTo({ destination: Rectangle.fromDegrees(west, south, east, north), duration });
  }

  toggleMode(): "3D" | "2D" {
    const s = this.viewer.scene;
    if (s.mode === SceneMode.SCENE3D) {
      s.morphTo2D(0.6);
      return "2D";
    }
    s.morphTo3D(0.6);
    return "3D";
  }

  /** Current view as a bbox [lon0, lat0, lon1, lat1] and an approximate XYZ zoom. */
  viewBbox(): { bbox: [number, number, number, number]; zoom: number } | null {
    const r = this.viewer.camera.computeViewRectangle();
    if (!r) return null;
    const h = this.viewer.camera.positionCartographic.height;
    const zoom = Math.max(0, Math.min(20, Math.round(Math.log2(35_000_000 / Math.max(h, 1)))));
    return {
      bbox: [CMath.toDegrees(r.west), CMath.toDegrees(r.south), CMath.toDegrees(r.east), CMath.toDegrees(r.north)],
      zoom,
    };
  }

  /** Draw or update one entity. `visible` folds in layer, marking and mission-tier filters. */
  upsert(rec: EntityRecord, visible: boolean, stale: boolean): void {
    const id = rec.r.entity_id;
    if (rec.r.entity_type === "zone" && rec.full?.geo) {
      this.upsertZone(rec, visible);
      return;
    }
    const pos = Cartesian3.fromDegrees(rec.r.lon, rec.r.lat, Math.max(0, rec.r.hae ?? 0));
    const color = Color.fromCssColorString(colorFor(rec)).withAlpha(stale ? 0.35 : 1);
    let d = this.drawn.get(id);
    if (!d) {
      d = {
        point: this.points.add({ id, position: pos, pixelSize: 12, disableDepthTestDistance: Infinity }),
        label: this.labels.add({
          id,
          position: pos,
          font: "600 13px system-ui, sans-serif",
          style: LabelStyle.FILL_AND_OUTLINE,
          outlineWidth: 3,
          outlineColor: Color.BLACK,
          pixelOffset: new Cartesian2(10, -10),
          horizontalOrigin: HorizontalOrigin.LEFT,
          verticalOrigin: VerticalOrigin.BOTTOM,
          disableDepthTestDistance: Infinity,
          translucencyByDistance: new NearFarScalar(2e5, 1, 5e6, 0),
        }),
      };
      this.drawn.set(id, d);
    }
    d.point.position = pos;
    d.point.color = color;
    d.point.outlineColor = rec.r.simulated ? Color.WHITE : Color.BLACK;
    d.point.outlineWidth = id === this.selected ? 4 : 2;
    d.point.pixelSize = id === this.selected ? 16 : 12;
    d.point.show = visible;
    d.label.position = pos;
    d.label.text = `${rec.r.callsign ?? id}${stale ? " (stale)" : ""}`;
    d.label.fillColor = Color.WHITE.withAlpha(stale ? 0.5 : 1);
    d.label.show = visible;
    this.viewer.scene.requestRender();
  }

  remove(id: string): void {
    const d = this.drawn.get(id);
    if (d) {
      this.points.remove(d.point);
      this.labels.remove(d.label);
      this.drawn.delete(id);
    }
    const z = this.zones.get(id);
    if (z) {
      this.viewer.entities.remove(z);
      this.zones.delete(id);
      this.zoneSeq.delete(id);
    }
    this.viewer.scene.requestRender();
  }

  select(id: string | null): void {
    const prev = this.selected;
    this.selected = id;
    for (const k of [prev, id]) {
      const d = k ? this.drawn.get(k) : undefined;
      if (d) {
        d.point.outlineWidth = k === id ? 4 : 2;
        d.point.pixelSize = k === id ? 16 : 12;
      }
    }
    this.viewer.scene.requestRender();
  }

  get drawnCount(): number {
    let n = 0;
    for (const d of this.drawn.values()) if (d.point.show) n++;
    for (const z of this.zones.values()) if (z.show) n++;
    return n;
  }

  private upsertZone(rec: EntityRecord, visible: boolean): void {
    const id = rec.r.entity_id;
    const g = rec.full!.geo!;
    const c = g.coords as { type: string; coordinates: unknown };
    const color = Color.fromCssColorString(g.kind === "keep_out" || g.kind === "hazard" ? "#f97316" : "#22d3ee");
    let ent = this.zones.get(id);
    // Rebuild only when the zone changed: Cesium builds the geometry asynchronously, and
    // re-creating it on every reconcile would keep the border from ever appearing.
    if (ent && this.zoneSeq.get(id) === rec.r.seq) {
      if (ent.show !== visible) {
        ent.show = visible;
        this.viewer.scene.requestRender();
      }
      return;
    }
    if (ent) this.viewer.entities.remove(ent);
    this.zoneSeq.set(id, rec.r.seq);
    const common = { id: `zone:${id}`, name: rec.r.callsign ?? id, show: visible };
    if (c.type === "Polygon") {
      const ring = (c.coordinates as number[][][])[0] ?? [];
      ent = this.viewer.entities.add({
        ...common,
        polygon: {
          hierarchy: new PolygonHierarchy(ring.map(([lon, lat]) => Cartesian3.fromDegrees(lon, lat))),
          material: color.withAlpha(0.12),
        },
        // The border is a polyline (polygon outlines are unsupported on terrain); it is clamped to
        // the ground only when terrain is on, since the ellipsoid needs no clamping.
        polyline: {
          // Unclamped, the line sits 5 m up so it does not depth-fight the globe surface.
          positions: ring.map(([lon, lat]) => Cartesian3.fromDegrees(lon, lat, this.clampZones ? 0 : 5)),
          width: 2,
          material: color,
          clampToGround: this.clampZones,
        },
      });
    } else if (c.type === "Point" && typeof g.radius_m === "number") {
      const [lon, lat] = c.coordinates as number[];
      ent = this.viewer.entities.add({
        ...common,
        position: Cartesian3.fromDegrees(lon, lat),
        ellipse: {
          semiMajorAxis: g.radius_m,
          semiMinorAxis: g.radius_m,
          material: color.withAlpha(0.12),
          height: 0,
          outline: true,
          outlineColor: color,
        },
      });
    } else {
      return;
    }
    this.zones.set(id, ent);
    this.viewer.scene.requestRender();
  }
}
