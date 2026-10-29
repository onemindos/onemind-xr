// OneMind L0 light web client (FEATURES J1, plus the M4 Act features in ui/act.ts). A view and an
// input device: it reads the bus over NATS WebSocket and never publishes `ent.*` or `cmd.fleet.*`
// (entity.md §2; CLAUDE.md rule 3). Its only `cmd.*` publish is `cmd.map.geofence.*`
// (asset-command.md §2: clients publish `cmd.map.>`); asset commands go through policy.
import "cesium/Build/Cesium/Widgets/widgets.css";
import "./styles.css";
import { wsconnect, type NatsConnection, type Subscription } from "@nats-io/nats-core";
import { alertFromEntity, alertFromGeofence, alertsFromHealth, sortAlerts, type AlertItem } from "./bus/alerts";
import { DEFAULT_LAYERS, type LayerDef } from "./bus/catalog";
import { getEntity, LayerFeed, type FeedStatus } from "./bus/layerclient";
import { inMissionTier, isMissionZone } from "./bus/missiontier";
import { isLocatable, viewerMaySee } from "./bus/normalize";
import { EntityStore, type EntityRecord } from "./bus/store";
import type { Entity } from "./bus/types";
import { playbackUrls } from "./bus/videourl";
import { loadCredentials, readConfig, saveCredentials } from "./config";
import { Act } from "./ui/act";
import { h } from "./ui/dom";
import { Globe } from "./ui/globe";
import { renderAlerts, renderDetail, renderEntityList, renderLayers } from "./ui/panels";
import { VideoPlayer } from "./ui/video";

const cfg = readConfig();
const store = new EntityStore();
const $ = <T extends HTMLElement>(id: string) => document.getElementById(id) as T;

const enabled = new Set(cfg.layers.filter((id) => DEFAULT_LAYERS.some((l) => l.id === id)));
const feeds = new Map<string, LayerFeed>();
const feedStatus = new Map<string, FeedStatus>();
const geofenceAlerts = new Map<string, AlertItem>();
let nc: NatsConnection | null = null;
let geofenceSub: Subscription | null = null;
/** Bumped on every connect(): a superseded attempt (or its retry timer) must not touch `nc`. */
let connectGen = 0;
let retryTimer: ReturnType<typeof setTimeout> | undefined;
let missionOnly = cfg.missionOnly;
let selected: string | null = null;
let videoPath: string | null = null;
let flownToFirst = !!cfg.view;
let dirtyGlobe = true;
let dirtyPanels = true;

const globe = new Globe($("globe"), cfg, (id) => select(id, false));
const act = new Act({
  store,
  globe,
  feedStatus,
  enabledLayers: () => [...enabled],
  setLayer: (id, on) => {
    if (enabled.has(id) !== on) toggleLayer(id, on);
  },
  selected: () => selected,
  select: (id, fly) => select(id, fly),
  showTab: (name) => showTab(name),
  natsUser: () => loadCredentials().user ?? "",
  onAlertsChanged: () => {
    dirtyPanels = true;
  },
});
const player = new VideoPlayer($("video") as HTMLVideoElement, (s) => {
  $("video-state").textContent = s.message;
  $("video-panel").dataset.transport = s.transport;
});

// ---- visibility ------------------------------------------------------------------------------

function missionZones(): EntityRecord[] {
  const out: EntityRecord[] = [];
  for (const r of store.all()) if (isMissionZone(r)) out.push(r);
  return out;
}

function visibleState(zones: EntityRecord[]) {
  const out: { rec: EntityRecord; stale: boolean; mission: boolean; visible: boolean; listed: boolean }[] = [];
  for (const rec of store.all()) {
    const marked = viewerMaySee(rec.full?.marking, cfg.viewerGroups);
    const mission = inMissionTier(rec, zones);
    const visible =
      enabled.has(rec.layer) && marked && isLocatable(rec.r, rec.full) && (!missionOnly || mission);
    // Non-located alerts are listed (not drawn) when their layer is on and the marking allows.
    const listed = visible || (rec.r.entity_type === "alert" && marked && enabled.has(rec.layer));
    out.push({ rec, stale: store.isStale(rec), mission, visible, listed });
  }
  return out;
}

// ---- render loop -----------------------------------------------------------------------------

store.onChange((c) => {
  for (const r of c.removed) {
    globe.remove(r.id);
    if (r.id === selected) {
      player.stop();
      videoPath = null;
      $("video-state").textContent = `Entity removed (${r.reason}).`;
    }
  }
  dirtyGlobe = true;
  dirtyPanels = true;
  if (cfg.select && !selected && store.get(cfg.select)) select(cfg.select, true);
  if (!flownToFirst && c.upserted.some((r) => isLocatable(r.r, r.full))) {
    // Fit the first view to what is visible once the initial snapshots have landed.
    flownToFirst = true;
    setTimeout(fitToVisible, 1200);
  }
});

function fitToVisible(): void {
  let w = 180, so = 90, e = -180, n = -90, k = 0;
  for (const v of visibleState(missionZones())) {
    if (!v.visible) continue;
    const pts: number[][] = [[v.rec.r.lon, v.rec.r.lat]];
    const g = v.rec.full?.geo?.coords as { type: string; coordinates: unknown } | undefined;
    if (g?.type === "Polygon") pts.push(...((g.coordinates as number[][][])[0] ?? []));
    for (const [lon, lat] of pts) {
      w = Math.min(w, lon); e = Math.max(e, lon); so = Math.min(so, lat); n = Math.max(n, lat); k++;
    }
  }
  if (k === 0) return;
  const pad = Math.max(0.004, (e - w) * 0.15, (n - so) * 0.15);
  globe.flyToRect(w - pad, so - pad, e + pad, n + pad);
}

function frame(): void {
  if (dirtyGlobe) {
    dirtyGlobe = false;
    const zones = missionZones();
    for (const v of visibleState(zones)) globe.upsert(v.rec, v.visible, v.stale);
  }
  requestAnimationFrame(frame);
}
requestAnimationFrame(frame);

let lastListSig = "";
let lastDetailSig = "";
function renderPanels(): void {
  if (!dirtyPanels) return;
  dirtyPanels = false;
  const zones = missionZones();
  const vis = visibleState(zones).filter((v) => v.listed);

  // Alerts: open alert entities, health alerts of visible entities, recent geofence crossings.
  const alerts: AlertItem[] = [...geofenceAlerts.values()];
  for (const v of vis) {
    const a = alertFromEntity(v.rec);
    if (a) alerts.push(a);
    alerts.push(...alertsFromHealth(v.rec));
  }
  // FEATURES D6: stale own assets and feed outages (B20) join the alert stream.
  alerts.push(...act.extraAlerts(vis.map((v) => ({ rec: v.rec, stale: v.stale }))));
  const sorted = sortAlerts(alerts.filter((x) => !act.isDismissed(x.key)));
  $("tab-alerts").replaceChildren(renderAlerts(sorted, (id) => select(id, true), (key) => act.dismiss(key)));
  $("alert-count").textContent = String(sorted.length);
  $("alert-chip").dataset.level = sorted[0]?.level ?? "none";

  const list = vis
    .filter((v) => v.rec.r.entity_type !== "alert")
    .sort((a, b) => Number(b.mission) - Number(a.mission) || (a.rec.r.callsign ?? "").localeCompare(b.rec.r.callsign ?? ""));
  const sig = list.map((v) => `${v.rec.r.entity_id}:${v.stale}:${v.mission}`).join("|") + `#${selected}`;
  if (sig !== lastListSig) {
    lastListSig = sig;
    $("tab-entities").replaceChildren(renderEntityList(list, selected, (id) => select(id, true)));
  }
  $("entity-count").textContent = String(list.length);

  const rec = selected ? store.get(selected) : undefined;
  if (rec?.fullStale) refreshFull(rec.r.entity_id);
  const dsig = rec ? `${rec.r.entity_id}:${rec.r.seq}:${store.isStale(rec)}:${!!rec.full}:${!!rec.fullStale}:${Math.floor(Date.now() / 5000)}` : "none";
  if (dsig !== lastDetailSig) {
    lastDetailSig = dsig;
    $("detail-body").replaceChildren(
      renderDetail(rec, rec ? store.isStale(rec) : false, rec ? inMissionTier(rec, zones) : false),
    );
    maybePlayVideo(rec?.full);
  }

  const counts = new Map<string, number>();
  for (const v of vis) counts.set(v.rec.layer, (counts.get(v.rec.layer) ?? 0) + 1);
  $("tab-layers").replaceChildren(renderLayers(DEFAULT_LAYERS, enabled, feedStatus, counts, toggleLayer));
}
setInterval(renderPanels, 400);

// Expiry: core subscribers never see delete markers (layer-delta.md §1), so expire from expiry_t.
setInterval(() => {
  store.expire();
  // requestRenderMode: ground-clamped zone geometry finishes asynchronously and requests no
  // frame by itself, so render at least once a second.
  globe.viewer.scene.requestRender();
  dirtyGlobe = true;
  dirtyPanels = true;
  const cutoff = Date.now() - 10 * 60 * 1000;
  for (const [k, a] of geofenceAlerts) if (a.t < cutoff) geofenceAlerts.delete(k);
}, 1000);

// ---- selection and video ---------------------------------------------------------------------

function select(id: string | null, fly: boolean): void {
  selected = id;
  globe.select(id);
  dirtyPanels = true;
  lastListSig = "";
  if (!id) return;
  showTab("detail");
  const rec = store.get(id);
  if (rec && fly && isLocatable(rec.r, rec.full)) globe.flyTo(rec.r.lon, rec.r.lat, 3000);
  fullFetchAt = 0;
  refreshFull(id);
}

// The full record (svc.entity.get) for the selected entity; refetched, at most every 2 s, while
// later render-only records make the held one older (store `fullStale`).
let fullFetchAt = 0;
function refreshFull(id: string): void {
  const now = Date.now();
  if (!nc || now - fullFetchAt < 2000) return;
  fullFetchAt = now;
  void getEntity(nc, id).then((full) => {
    if (full && typeof full === "object") store.attachFull(full as Entity);
  });
}

function maybePlayVideo(full: Entity | undefined): void {
  const urls = playbackUrls(full?.media, location.hostname, undefined, { whep: cfg.whepBase, hls: cfg.hlsBase });
  if (!urls) {
    if (videoPath) player.stop();
    videoPath = null;
    $("video-state").textContent = full ? "This entity has no video stream." : "Select an entity with video.";
    $("video-panel").dataset.transport = "none";
    return;
  }
  if (urls.path === videoPath) return;
  videoPath = urls.path;
  $("video-state").textContent = `Connecting to ${urls.path}…`;
  void player.play(urls, cfg.videoMode);
}

// ---- layers and bus --------------------------------------------------------------------------

function toggleLayer(id: string, on: boolean): void {
  if (on) enabled.add(id);
  else enabled.delete(id);
  const def = DEFAULT_LAYERS.find((l) => l.id === id);
  if (on && def && nc && !feeds.has(id)) startFeed(def);
  if (!on) {
    feeds.get(id)?.stop();
    feeds.delete(id);
    store.batch(() => store.idsInLayer(id).forEach((e) => store.remove(e, "left_aoi")));
  }
  dirtyGlobe = dirtyPanels = true;
}

function startFeed(def: LayerDef): void {
  const view = def.tier === "large" ? globe.viewBbox() : null;
  const feed = new LayerFeed(nc!, store, def, {
    aoi: view ? { bbox: view.bbox } : null,
    zoom: view?.zoom,
    onStatus: (s) => {
      feedStatus.set(def.id, s);
      dirtyPanels = true;
    },
  });
  feeds.set(def.id, feed);
  void feed.start();
}

globe.viewer.camera.moveEnd.addEventListener(() => {
  const v = globe.viewBbox();
  if (!v) return;
  for (const f of feeds.values()) if (f.def.tier === "large") f.setAoi({ bbox: v.bbox }, v.zoom);
});

function setConn(state: string, text: string): void {
  const el = $("conn");
  el.dataset.state = state;
  el.textContent = text;
}

async function connect(url: string): Promise<void> {
  const gen = ++connectGen;
  clearTimeout(retryTimer);
  retryTimer = undefined;
  for (const f of feeds.values()) f.stop();
  feeds.clear();
  geofenceSub?.unsubscribe();
  geofenceSub = null;
  act.detach();
  const old = nc;
  nc = null;
  if (old) await old.close().catch(() => undefined);
  if (gen !== connectGen) return;
  const creds = loadCredentials();
  setConn("connecting", `connecting ${url}`);
  let conn: NatsConnection;
  try {
    conn = await wsconnect({
      servers: url,
      name: "onemind-l0-web",
      user: creds.user || undefined,
      pass: creds.pass || undefined,
      maxReconnectAttempts: -1,
      reconnectTimeWait: 2000,
    });
  } catch (err) {
    if (gen !== connectGen) return;
    setConn("error", `cannot reach ${url}: ${(err as Error).message}`);
    retryTimer = setTimeout(() => void connect(url), 3000);
    return;
  }
  if (gen !== connectGen) {
    // A newer connect() started while this one was dialling.
    await conn.close().catch(() => undefined);
    return;
  }
  nc = conn;
  setConn("connected", `bus ${url}`);
  void (async () => {
    for await (const s of conn.status()) {
      if (gen !== connectGen) break;
      if (s.type === "disconnect") setConn("reconnecting", "bus disconnected, retrying");
      if (s.type === "reconnect") setConn("connected", `bus ${url}`);
    }
  })();
  for (const def of DEFAULT_LAYERS) if (enabled.has(def.id)) startFeed(def);
  act.attach(conn);
  // Geofence crossings (omos docs/NATS-DOCTRINE.md: evt.map.geofence.{id}.{enter|exit}).
  geofenceSub = conn.subscribe("evt.map.geofence.*.*", {
    callback: (_e, m) => {
      // A refused subscription arrives here with an empty message (omos #562 CLIENT users may
      // not subscribe evt.map.>); touching it would throw in nats-core's read loop and stop the
      // whole connection.
      if (_e) return;
      let body: unknown = null;
      try {
        body = m.json();
      } catch {
        /* payload is optional */
      }
      const a = alertFromGeofence(m.subject, body, Date.now());
      if (a) {
        geofenceAlerts.set(a.key, a);
        dirtyPanels = true;
      }
    },
  });
}

// ---- chrome ----------------------------------------------------------------------------------

function showTab(name: string): void {
  for (const b of document.querySelectorAll<HTMLButtonElement>("[role=tab]")) {
    const on = b.dataset.tab === name;
    b.setAttribute("aria-selected", String(on));
    $(`tab-${b.dataset.tab}`).hidden = !on;
  }
  if (name === "detail" && selected) dirtyPanels = true;
}

for (const b of document.querySelectorAll<HTMLButtonElement>("[role=tab]")) {
  b.addEventListener("click", () => showTab(b.dataset.tab!));
}
$("alert-chip").addEventListener("click", () => showTab("alerts"));

const missionBtn = $("mission-toggle");
const syncMission = () => {
  missionBtn.setAttribute("aria-pressed", String(missionOnly));
  missionBtn.textContent = missionOnly ? "Mission tier: on" : "Mission tier: off";
};
missionBtn.addEventListener("click", () => {
  missionOnly = !missionOnly;
  syncMission();
  dirtyGlobe = dirtyPanels = true;
  lastListSig = "";
});
syncMission();

$("mode-toggle").addEventListener("click", () => {
  const m = globe.toggleMode();
  $("mode-toggle").textContent = m === "3D" ? "2D" : "3D";
});

const urlInput = $("nats-url") as HTMLInputElement;
urlInput.value = cfg.natsUrl;
$("conn-form").addEventListener("submit", (ev) => {
  ev.preventDefault();
  saveCredentials({
    user: ($("nats-user") as HTMLInputElement).value,
    pass: ($("nats-pass") as HTMLInputElement).value,
  });
  void connect(urlInput.value.trim());
});
$("viewer-groups").textContent = cfg.viewerGroups.join(", ") || "(none)";

showTab("alerts");
void connect(cfg.natsUrl);

// Read-only handle for the end-to-end test and for debugging in the browser console.
Object.assign(window, {
  __l0: {
    store,
    feedStatus,
    get drawn() {
      return globe.drawnCount;
    },
    get transport() {
      return player.transport;
    },
    get connected() {
      return !!nc && !nc.isClosed();
    },
    act,
    toScreen: (lon: number, lat: number) => globe.toScreen(lon, lat),
    flyTo: (lon: number, lat: number, height: number) => globe.flyTo(lon, lat, height, 0),
    h,
  },
});
