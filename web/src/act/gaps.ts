// Intelligence gaps (FEATURES B20): which feeds are stale, failing or silent, and which layers
// and alerts that weakens. Two inputs, both on existing subjects:
//   1. `tel.feed.{source}.status` (BUS-DESIGN §3.1; omos #553 feed-status.v1 schema:
//      fetchedAt, count, lastError, staleAfterSec, stale, at). Read directly when the connection
//      may subscribe (local stack, services); a CLIENT account user may not (omos #562).
//   2. Layer manifests (layer-snapshot.md §2 `freshness.feeds[]` and `freshness.status`), which
//      the layer client already reads; this is the path for CLIENT users.
// The client clock judges freshness from `fetchedAt`, so a collector that stops publishing
// shows up when its last fetch passes `staleAfterSec`, i.e. within its freshness limit.
import type { LayerManifest } from "../bus/types";

/**
 * Feed source id → layers it feeds. Ids are the ones publishers use today: `nws-alerts` (omos
 * #553), `noaa`, `usgs`, `acled`, `firms` (omos `platform/omos/intel-scripts.yaml`), `celestrak`
 * and `mta` (omos #558); `opensky`, `adsb`, `ais` per layer-snapshot.md §1. A manifest's
 * `freshness.feeds[]` overrides this table, since the layer service knows its own feeds.
 */
export const FEED_LAYERS: Record<string, string[]> = {
  "nws-alerts": ["hazards", "alerts"],
  noaa: ["hazards"],
  usgs: ["hazards"],
  firms: ["hazards"],
  acled: ["hazards"],
  celestrak: ["space"],
  mta: ["transit"],
  opensky: ["air"],
  adsb: ["air"],
  ais: ["sea"],
  aprs: ["rf"],
  meshtastic: ["rf"],
};

/** Alerts a layer feeds (FEATURES D6: geofence crossings, stale assets, feed outages). */
export const LAYER_ALERTS: Record<string, string[]> = {
  hazards: ["hazard alerts (weather, quake, fire)"],
  alerts: ["alert entities"],
  fleet: ["stale-asset alerts", "geofence crossings of own assets"],
  tracks: ["geofence crossings of tracks"],
  zones: ["geofence crossings (zones themselves)"],
  air: ["geofence crossings of aircraft"],
  sea: ["geofence crossings of vessels"],
};

export function layersForSource(source: string): string[] {
  if (FEED_LAYERS[source]) return FEED_LAYERS[source];
  const prefix = Object.keys(FEED_LAYERS).find((k) => source.startsWith(`${k}-`) || source.startsWith(`${k}_`));
  return prefix ? FEED_LAYERS[prefix] : [];
}

export type GapReason = "stale" | "error" | "silent" | "never" | "layer_down" | "layer_stale";

export interface Gap {
  key: string;
  source: string;
  layers: string[];
  alerts: string[];
  reason: GapReason;
  detail: string;
  /** client time the gap began (best estimate) */
  since: number;
  limitS: number;
}

interface FeedState {
  source: string;
  fetchedAt: number | null;
  lastError: string | null;
  staleFlag: boolean;
  staleAfterSec: number;
  count?: number;
  heardAt: number;
  layers: Set<string>;
  via: "tel.feed" | "manifest";
}

/** Parse an RFC 3339 string or epoch milliseconds (both forms are in use today). */
export function toMs(v: unknown): number | null {
  if (typeof v === "number" && Number.isFinite(v)) return v > 1e12 ? v : v * 1000;
  if (typeof v === "string" && v) {
    const t = Date.parse(v);
    return Number.isFinite(t) ? t : null;
  }
  return null;
}

const DEFAULT_LIMIT_S = 60;

export class FeedGaps {
  private feeds = new Map<string, FeedState>();
  private layerDown = new Map<string, { status: string; limitS: number; at: number }>();
  constructor(private now: () => number = () => Date.now()) {}

  /** One `tel.feed.{source}.status` message. */
  ingestStatus(subject: string, body: unknown): void {
    const tok = subject.split(".");
    if (tok.length !== 4 || tok[0] !== "tel" || tok[1] !== "feed" || tok[3] !== "status") return;
    const b = (body && typeof body === "object" ? body : {}) as Record<string, unknown>;
    const source = typeof b.source === "string" && b.source ? b.source : tok[2];
    const st = this.state(source, "tel.feed");
    st.fetchedAt = toMs(b.fetchedAt) ?? st.fetchedAt;
    st.lastError = typeof b.lastError === "string" && b.lastError ? b.lastError : null;
    st.staleFlag = b.stale === true;
    if (typeof b.staleAfterSec === "number" && b.staleAfterSec > 0) st.staleAfterSec = b.staleAfterSec;
    if (typeof b.count === "number") st.count = b.count;
    st.heardAt = this.now();
  }

  /** A layer manifest (layer-snapshot.md §2). */
  ingestManifest(layer: string, m: LayerManifest | undefined): void {
    const f = m?.freshness as (LayerManifest["freshness"] & { feeds?: Record<string, unknown>[] }) | undefined;
    if (!f) return;
    const limitS = typeof f.limit_s === "number" && f.limit_s > 0 ? f.limit_s : DEFAULT_LIMIT_S;
    if (f.status === "down" || f.status === "stale") this.layerDown.set(layer, { status: f.status, limitS, at: this.now() });
    else this.layerDown.delete(layer);
    for (const feed of f.feeds ?? []) {
      if (typeof feed.source !== "string") continue;
      const st = this.state(feed.source, "manifest");
      st.layers.add(layer);
      const fa = toMs(feed.fetchedAt);
      if (fa && (!st.fetchedAt || fa > st.fetchedAt)) st.fetchedAt = fa;
      if (st.via === "manifest") {
        st.lastError = typeof feed.lastError === "string" && feed.lastError ? feed.lastError : null;
        st.staleFlag = feed.stale === true;
        st.staleAfterSec = limitS;
        st.heardAt = this.now();
      }
    }
  }

  private state(source: string, via: FeedState["via"]): FeedState {
    let st = this.feeds.get(source);
    if (!st) {
      st = { source, fetchedAt: null, lastError: null, staleFlag: false, staleAfterSec: DEFAULT_LIMIT_S, heardAt: this.now(), layers: new Set(layersForSource(source)), via };
      this.feeds.set(source, st);
    }
    if (via === "tel.feed") st.via = "tel.feed";
    return st;
  }

  sources(): string[] {
    return [...this.feeds.keys()].sort();
  }

  gaps(): Gap[] {
    const now = this.now();
    const out: Gap[] = [];
    for (const st of this.feeds.values()) {
      const layers = [...st.layers].sort();
      const alerts = [...new Set(layers.flatMap((l) => LAYER_ALERTS[l] ?? []))];
      const limitMs = st.staleAfterSec * 1000;
      const base = { source: st.source, layers, alerts, limitS: st.staleAfterSec };
      const age = st.fetchedAt ? now - st.fetchedAt : null;
      if (st.lastError) {
        out.push({ ...base, key: `feed:${st.source}`, reason: "error", detail: `last error: ${st.lastError}`, since: st.heardAt });
      } else if (age === null) {
        if (now - st.heardAt > limitMs)
          out.push({ ...base, key: `feed:${st.source}`, reason: "never", detail: "no successful fetch reported", since: st.heardAt });
      } else if (st.staleFlag || age > limitMs) {
        out.push({
          ...base,
          key: `feed:${st.source}`,
          reason: "stale",
          detail: `no fresh data for ${Math.round(age / 1000)} s (limit ${st.staleAfterSec} s)`,
          since: st.fetchedAt! + limitMs,
        });
      } else if (st.via === "tel.feed" && now - st.heardAt > limitMs) {
        out.push({ ...base, key: `feed:${st.source}`, reason: "silent", detail: `no status heartbeat for ${Math.round((now - st.heardAt) / 1000)} s`, since: st.heardAt + limitMs });
      }
    }
    for (const [layer, d] of this.layerDown) {
      out.push({
        key: `layer:${layer}`,
        source: `layer service`,
        layers: [layer],
        alerts: LAYER_ALERTS[layer] ?? [],
        reason: d.status === "down" ? "layer_down" : "layer_stale",
        detail: d.status === "down" ? "the layer service cannot read ENT" : "no feed of the layer is within its limit",
        since: d.at,
        limitS: d.limitS,
      });
    }
    return out.sort((a, b) => a.source.localeCompare(b.source));
  }
}
