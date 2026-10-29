// Act features of the L0 client (ROADMAP M4): draw AOIs and geofences (D1), task a simulated
// asset through policy with a human confirm (D3), drawing and tactical graphics with CoT export
// (B11), intelligence gaps (B20), the Hermes panel (D4), feed and asset alerts (D6), role modes
// (J2) and GeoChat contacts and chat (C8).
//
// What it publishes, all existing grammar: requests on `svc.policy.*` (propose, confirm,
// action.get, check), `svc.cot.send` (chat) and `agents.prompt.hermes.*` (Hermes). It publishes no
// `cmd.*` subject at all: zone changes are `zone.create|update|remove` actions (ONTOLOGY §6.2,
// A2) that the policy service signs and sends on `cmd.map.geofence.{id}.{verb}` after a human
// confirm, and asset commands go the same way to `cmd.fleet.*`. It never publishes `ent.*`.
import type { NatsConnection, Subscription } from "@nats-io/nats-core";
import { alertFromStaleAsset, alertsFromGaps, type AlertItem } from "../bus/alerts";
import type { FeedStatus } from "../bus/layerclient";
import { isLocatable } from "../bus/normalize";
import type { EntityRecord, EntityStore } from "../bus/store";
import { ALL_ROOM, ChatStore, geochatRequest, type Room } from "../act/chat";
import { cotForDrawing } from "../act/cot";
import {
  buffer,
  canBeZone,
  lassoSelect,
  loosens,
  SAFETY_KINDS,
  TOOL_POINTS,
  ZONE_KINDS,
  zoneIdFor,
  zoneParams,
  type Drawing,
  type ShapeKind,
  type ZoneKind,
} from "../act/drawing";
import { FeedGaps, type Gap } from "../act/gaps";
import type { LonLat } from "../act/geo";
import { buildPrompt, DEFAULT_HERMES_SUBJECT, MAX_CONTEXT_ENTITIES, parseMapCommand, parseReply, zoomToHeight, type MapAction, type MapContext } from "../act/hermes";
import { canHash, hashDocument, paramsHash, type HashInput } from "../act/jcs";
import {
  ActionTracker,
  confirmGate,
  detailsLoaded,
  gotoRequest,
  permissionFromCheck,
  roleProbe,
  simpleRequest,
  SUBJ,
  TERMINAL,
  uuid,
  zoneRequest,
  type ActionRequest,
  type ActionView,
  type ConfirmReply,
  type Denial,
  type HashCheck,
  type Permission,
  type ProposeReply,
} from "../act/policy";
import { abilities, effectiveRole, parseRole, ROLE_TEXT, ROLES, type Role } from "../act/roles";
import { ago, h } from "./dom";
import type { Globe } from "./globe";
import { Overlay } from "./overlay";

const enc = new TextEncoder();
const dec = new TextDecoder();
const COMMANDABLE = new Set(["drone", "robot", "vehicle"]);
const TOOLS: ShapeKind[] = ["point", "label", "line", "polygon", "rectangle", "circle", "sector", "rings", "rangebearing", "lasso"];
const TOOL_TITLE: Record<ShapeKind, string> = {
  point: "Point", label: "Label", line: "Line", polygon: "Polygon", rectangle: "Rectangle", circle: "Circle",
  sector: "Sector", rings: "Range rings", rangebearing: "Range & bearing", lasso: "Lasso select", buffer: "Buffer",
};

export interface ActDeps {
  store: EntityStore;
  globe: Globe;
  feedStatus: Map<string, FeedStatus>;
  enabledLayers: () => string[];
  setLayer: (id: string, on: boolean) => void;
  selected: () => string | null;
  select: (id: string | null, fly: boolean) => void;
  showTab: (name: string) => void;
  natsUser: () => string;
  onAlertsChanged: () => void;
}

interface HermesLine {
  from: "me" | "hermes" | "map" | "error";
  text: string;
  t: number;
  proposals?: string[];
}

/** Lines kept in the Hermes log (oldest dropped). */
export const MAX_HERMES_LINES = 200;

interface ZoneOp {
  drawingId: string;
  zoneId: string;
  /** the kind after the change; null for a removal */
  kind: ZoneKind | null;
  verb: "create" | "update" | "remove";
}

const $ = <T extends HTMLElement>(id: string) => document.getElementById(id) as T;

function json(m: { data: Uint8Array }): unknown {
  try {
    return JSON.parse(dec.decode(m.data));
  } catch {
    return null;
  }
}

function lsGet(k: string): string | null {
  try {
    return localStorage.getItem(k);
  } catch {
    return null;
  }
}
function lsSet(k: string, v: string): void {
  try {
    localStorage.setItem(k, v);
  } catch {
    /* storage blocked: the mode lasts for this page */
  }
}

export class Act {
  readonly tracker = new ActionTracker();
  readonly gaps = new FeedGaps();
  readonly chat: ChatStore;
  readonly overlay: Overlay;
  readonly drawings = new Map<string, Drawing>();
  /** drawing id -> zone id and kind once a zone change executed */
  readonly shared = new Map<string, { zoneId: string; kind: ZoneKind }>();
  /** action id -> the zone change it proposes (applied to `shared` once it executes) */
  private zoneOps = new Map<string, ZoneOp>();
  /** action id -> the client's own params_hash check */
  private hashChecks = new Map<string, { key: string; state: HashCheck }>();
  private hermesSeq = 0;
  readonly hermesLog: HermesLine[] = [];
  readonly mapLog: { action: MapAction; t: number }[] = [];
  readonly busNotes = new Map<string, string>();
  lassoed: string[] = [];

  private nc: NatsConnection | null = null;
  private subs: Subscription[] = [];
  private role: Role = parseRole(lsGet("l0.role"));
  private perms = new Map<string, { permission: Permission; denial?: Denial; at: number }>();
  private tool: { kind: ShapeKind; points: LonLat[] } | null = null;
  private taskMode: { assetId: string } | null = null;
  private selectedDrawing: string | null = null;
  private cotText = "";
  private chatChannel: string | null = null;
  private chatRoom = ALL_ROOM;
  private dismissed = new Set<string>();
  private dialogAction: string | null = null;
  private hermesBusy = false;
  private drawSeq = 0;
  private dirty = true;
  private polling = new Set<string>();
  private lastFeedSeen = new Map<string, number>();
  /** goto altitude typed by the operator (kept across panel rebuilds) */
  private taskHae = "";

  constructor(private d: ActDeps) {
    this.chat = new ChatStore(this.selfUid());
    this.overlay = new Overlay(d.globe.viewer, (id) => {
      const r = d.store.get(id);
      return r && isLocatable(r.r, r.full) ? [r.r.lon, r.r.lat] : null;
    });
    this.tracker.onChange((a) => this.onAction(a));
    this.wireStatic();
    setInterval(() => this.tick(), 1000);
    setInterval(() => this.render(), 400);
  }

  // ---- identity and role ------------------------------------------------------------------

  /** The principal the policy service will resolve this connection to (principals map). */
  principal(): string {
    return `user:${this.d.natsUser() || "anonymous"}`;
  }

  selfUid(): string {
    return `OM-WEB-${(this.d.natsUser() || "guest").replace(/[^A-Za-z0-9_-]/g, "")}`;
  }

  get selectedRole(): Role {
    return this.role;
  }

  /** Permission for an asset from the last policy dry run ("unknown" until asked). */
  permission(assetId: string): Permission {
    return this.perms.get(assetId)?.permission ?? "unknown";
  }

  roleFor(assetId: string | null): Role {
    return effectiveRole(this.role, assetId ? this.permission(assetId) : "unknown");
  }

  /** Abilities for general UI (drawing, sharing): operator mode counts as observer for those. */
  private general() {
    return abilities(this.role === "operator" ? "operator" : this.role);
  }

  private async probe(assetId: string, entityType: string): Promise<void> {
    const cur = this.perms.get(assetId);
    if (!this.nc || (cur && Date.now() - cur.at < 30_000)) return;
    this.perms.set(assetId, { permission: cur?.permission ?? "unknown", denial: cur?.denial, at: Date.now() });
    try {
      const m = await this.nc.request(SUBJ.check, enc.encode(JSON.stringify(roleProbe({ entity_type: entityType, entity_id: assetId }, this.principal()))), { timeout: 3000 });
      const r = permissionFromCheck(json(m));
      this.perms.set(assetId, { ...r, at: Date.now() });
    } catch (e) {
      this.perms.set(assetId, { permission: "unknown", denial: { code: "policy_unavailable", message: (e as Error).message }, at: Date.now() - 20_000 });
    }
    this.dirty = true;
  }

  // ---- bus ------------------------------------------------------------------------------------

  attach(nc: NatsConnection): void {
    this.detach();
    this.nc = nc;
    this.perms.clear();
    const sub = (subject: string, note: string, fn: (subject: string, data: Uint8Array) => void) => {
      try {
        this.subs.push(
          nc.subscribe(subject, {
            callback: (err, m) => {
              if (err) {
                // A CLIENT account user may not subscribe here (omos #562); the client falls back
                // to request/reply (action.get polling, layer manifests).
                this.busNotes.set(subject, `${note}: not permitted (${err.message})`);
                this.dirty = true;
                return;
              }
              this.busNotes.set(subject, `${note}: live`);
              fn(m.subject, m.data);
            },
          }),
        );
        if (!this.busNotes.has(subject)) this.busNotes.set(subject, `${note}: subscribed`);
      } catch (e) {
        this.busNotes.set(subject, `${note}: ${(e as Error).message}`);
      }
    };
    sub("cmd.map.>", "map actions", (s, data) => this.onMapCommand(s, data));
    sub(SUBJ.audit, "action audit", (s, data) => {
      const env = json({ data }) as { data?: { action_id?: string; requested_by?: { id?: string; on_behalf_of?: string } } } | null;
      const id = env?.data?.action_id;
      if (!id) return;
      const known = !!this.tracker.get(id);
      // Track only my actions and proposals made on my behalf (Hermes): others' are not mine to confirm.
      const rb = env?.data?.requested_by;
      const mine = rb?.id === this.principal() || rb?.on_behalf_of === this.principal();
      if (!known && !mine) return;
      this.tracker.fromAudit(s, env);
      if (!known) void this.fetchAction(id); // parameters come only with action.get
    });
    sub("tel.feed.*.status", "feed status", (s, data) => {
      this.gaps.ingestStatus(s, json({ data }));
      this.lastFeedSeen.set(s, Date.now());
      this.dirty = true;
    });
    for (const s of ["tak.cot.out", "tak.cot.mesh.rx"]) {
      sub(s, "TAK CoT", (subject, data) => {
        if (this.chat.ingest(dec.decode(data), subject) !== "other") this.dirty = true;
      });
    }
    this.touch();
  }

  detach(): void {
    for (const s of this.subs) s.unsubscribe();
    this.subs = [];
    this.nc = null;
    this.busNotes.clear();
  }

  private async request<T>(subject: string, body: unknown, timeout = 5000): Promise<T> {
    if (!this.nc) throw new Error("not connected");
    const m = await this.nc.request(subject, enc.encode(JSON.stringify(body)), { timeout });
    return json(m) as T;
  }

  private async fetchAction(id: string): Promise<void> {
    if (!this.nc || id.startsWith("local-") || this.polling.has(id)) return;
    this.polling.add(id);
    try {
      this.tracker.fromGet(await this.request(SUBJ.get, { v: 1, action_id: id }, 2000));
    } catch {
      /* next tick retries */
    } finally {
      this.polling.delete(id);
    }
  }

  private tick(): void {
    for (const a of this.tracker.open()) void this.fetchAction(a.action_id);
    for (const id of this.tracker.prune(new Set(this.dialogAction ? [this.dialogAction] : []))) {
      this.hashChecks.delete(id);
      this.zoneOps.delete(id);
    }
    for (const [layer, st] of this.d.feedStatus) this.gaps.ingestManifest(layer, st.manifest);
    const sel = this.d.selected();
    const rec = sel ? this.d.store.get(sel) : undefined;
    if (rec && COMMANDABLE.has(rec.r.entity_type)) void this.probe(rec.r.entity_id, rec.r.entity_type);
    this.dirty = true;
    this.d.onAlertsChanged();
  }

  // ---- alerts (D6) ---------------------------------------------------------------------------

  /** Stale own assets and feed outages, minus what this viewer dismissed. */
  extraAlerts(listed: { rec: EntityRecord; stale: boolean }[]): AlertItem[] {
    const out: AlertItem[] = [];
    for (const v of listed) {
      const a = alertFromStaleAsset(v.rec, v.stale);
      if (a) out.push(a);
    }
    out.push(...alertsFromGaps(this.gaps.gaps()));
    return out;
  }

  isDismissed(key: string): boolean {
    return this.dismissed.has(key);
  }

  dismiss(key: string): void {
    this.dismissed.add(key);
    this.d.onAlertsChanged();
  }

  // ---- map commands (cmd.map.>, doctrine §9.7) -------------------------------------------------

  private onMapCommand(subject: string, data: Uint8Array): void {
    const a = parseMapCommand(subject, json({ data }));
    if (!a) return;
    this.mapLog.push({ action: a, t: Date.now() });
    if (this.mapLog.length > 100) this.mapLog.shift();
    switch (a.kind) {
      case "highlight":
        if (a.clear) this.overlay.unhighlight(a.uid);
        else this.overlay.highlight(a.uid, a.color, a.label);
        this.logMap(`highlighted ${a.uid}${a.label ? ` (${a.label})` : ""}`);
        break;
      case "annotation":
        this.overlay.annotate(a.id, a.verb, a);
        this.logMap(`${a.verb === "remove" ? "removed" : "annotated"} ${a.label ?? a.id}`);
        break;
      case "viewport":
        this.d.globe.flyTo(a.lon, a.lat, zoomToHeight(a.zoom));
        this.logMap(`moved the view to ${a.lat.toFixed(4)}, ${a.lon.toFixed(4)}`);
        break;
      case "filter":
        if (a.layers) {
          const want = new Set(a.layers.map((l) => l.replace(/\.\*$/, "")));
          for (const l of ["fleet", "tracks", "zones", "alerts", "tak", "hazards", "air", "sea"]) this.d.setLayer(l, want.has(l));
        }
        this.logMap(`set layers to ${a.layers?.join(", ") ?? "(unchanged)"}`);
        break;
      case "geofence":
        // Zones arrive as `ent.zone.*` records from KV `geofences`; nothing to draw here.
        break;
    }
    this.dirty = true;
  }

  private logMap(text: string): void {
    const last = this.hermesLog[this.hermesLog.length - 1];
    if (last?.from === "map" && Date.now() - last.t < 2000) {
      last.text += `; ${text}`;
      this.hermesSeq++;
    } else this.pushHermes({ from: "map", text: `Map: ${text}`, t: Date.now() });
  }

  /** Append to the Hermes log, keeping the newest MAX_HERMES_LINES. */
  private pushHermes(line: HermesLine): void {
    this.hermesLog.push(line);
    if (this.hermesLog.length > MAX_HERMES_LINES) this.hermesLog.splice(0, this.hermesLog.length - MAX_HERMES_LINES);
    this.hermesSeq++;
  }

  // ---- drawing (B11) and geofences (D1) -------------------------------------------------------

  startTool(kind: ShapeKind): void {
    if (!this.general().draw) return;
    this.cancelModes();
    this.tool = { kind, points: [] };
    this.d.globe.setTap(
      (ll) => this.toolClick(ll),
      (ll) => this.toolPreview(ll),
    );
    this.d.showTab("draw");
    this.touch();
  }

  private toolClick(ll: LonLat): void {
    if (!this.tool) return;
    this.tool.points.push(ll);
    const spec = TOOL_POINTS[this.tool.kind];
    if (spec.exact && this.tool.points.length >= spec.exact) this.finishTool();
    else this.toolPreview(null);
    this.touch();
  }

  private toolPreview(ll: LonLat | null): void {
    if (!this.tool) return;
    const pts = ll ? [...this.tool.points, ll] : this.tool.points;
    const kind = this.tool.kind === "lasso" ? "polygon" : this.tool.kind;
    this.overlay.setPreview({ id: "preview", kind: kind as Drawing["kind"], label: "", points: pts, createdT: 0 });
  }

  finishTool(): void {
    const t = this.tool;
    if (!t) return;
    const spec = TOOL_POINTS[t.kind];
    const need = spec.exact ?? spec.min ?? 1;
    if (t.points.length < need) return;
    this.cancelModes();
    if (t.kind === "lasso") {
      const items: { id: string; lon: number; lat: number }[] = [];
      const on = new Set(this.d.enabledLayers());
      for (const r of this.d.store.all()) if (on.has(r.layer) && isLocatable(r.r, r.full) && r.r.entity_type !== "zone") items.push({ id: r.r.entity_id, lon: r.r.lon, lat: r.r.lat });
      this.lassoed = lassoSelect(t.points, items);
      if (this.lassoed.length === 1) this.d.select(this.lassoed[0], false);
      this.touch();
      return;
    }
    const labelInput = $("draw-label") as HTMLInputElement;
    const n = ++this.drawSeq;
    const id = `d${Date.now().toString(36)}${n}`;
    const dr: Drawing = {
      id,
      kind: t.kind as Drawing["kind"],
      label: labelInput.value.trim() || `${TOOL_TITLE[t.kind]} ${n}`,
      points: t.points,
      rings: t.kind === "rings" ? 3 : undefined,
      createdT: Date.now(),
    };
    labelInput.value = "";
    this.addDrawing(dr);
  }

  addDrawing(dr: Drawing): void {
    this.drawings.set(dr.id, dr);
    this.overlay.setDrawing(dr);
    this.selectedDrawing = dr.id;
    this.touch();
  }

  removeDrawing(id: string): void {
    this.drawings.delete(id);
    this.overlay.removeDrawing(id);
    if (this.selectedDrawing === id) this.selectedDrawing = null;
    this.touch();
  }

  cancelModes(): void {
    this.tool = null;
    this.taskMode = null;
    this.overlay.setPreview(null);
    this.d.globe.setTap(null);
    this.touch();
  }

  /**
   * Propose a drawing as a zone (D1): `zone.create`, or `zone.update` once shared, through the
   * policy service (ONTOLOGY §6.2, A2). The confirm dialog opens with the exact parameters; the
   * zone exists only after a human approves and the geofence service applies the signed command.
   */
  async shareZone(id: string, kind: ZoneKind): Promise<ActionView | null> {
    const dr = this.drawings.get(id);
    if (!dr || !this.nc || !this.general().geofence || !canBeZone(dr) || this.zonePending(id)) return null;
    const params = zoneParams(dr, kind);
    if (!params) return null;
    const cur = this.shared.get(id);
    const zoneId = cur?.zoneId ?? zoneIdFor(dr);
    const verb = cur ? "update" : "create";
    return this.propose(zoneRequest(verb, zoneId, params, this.principal(), `${verb === "create" ? "Drawn" : "Edited"} in the L0 web client as ${kind}`), { drawingId: id, zoneId, kind, verb });
  }

  /** Propose removing a shared zone (`zone.remove`, A2): the same confirm as creating one. */
  async unshareZone(id: string): Promise<ActionView | null> {
    const s = this.shared.get(id);
    if (!s || !this.nc || !this.general().geofence || this.zonePending(id)) return null;
    return this.propose(zoneRequest("remove", s.zoneId, {}, this.principal(), "Removed in the L0 web client"), { drawingId: id, zoneId: s.zoneId, kind: null, verb: "remove" });
  }

  /** A zone change for this drawing is still open (proposed, awaiting confirm or dispatched). */
  zonePending(drawingId: string): ActionView | undefined {
    for (const [aid, op] of this.zoneOps) {
      const a = this.tracker.get(aid);
      if (op.drawingId === drawingId && a && !TERMINAL.has(a.status)) return a;
    }
    return undefined;
  }

  /** The kind a zone has now: from the shared drawing, else from its `ent.zone` record. */
  private zoneKindNow(zoneId: string): ZoneKind | undefined {
    for (const v of this.shared.values()) if (v.zoneId === zoneId) return v.kind;
    const k = this.d.store.get(zoneId)?.full?.geo?.kind;
    return typeof k === "string" ? (k as ZoneKind) : undefined;
  }

  cotFor(id: string): string {
    const dr = this.drawings.get(id);
    if (!dr) return "";
    return cotForDrawing(dr, { now: Date.now() }).join("\n");
  }

  // ---- tasking (D3) -----------------------------------------------------------------------------

  startGoto(assetId: string): void {
    if (!abilities(this.roleFor(assetId)).assetCommands) return;
    this.cancelModes();
    this.taskMode = { assetId };
    this.d.globe.setTap((ll) => {
      this.cancelModes();
      void this.proposeGoto(assetId, ll);
    });
    this.touch();
  }

  async proposeGoto(assetId: string, ll: LonLat): Promise<ActionView | null> {
    const rec = this.d.store.get(assetId);
    if (!rec) return null;
    // The operator's altitude when given (detail panel), else the asset's current HAE.
    const typed = this.taskHae.trim() === "" ? NaN : Number(this.taskHae);
    const hae = Number.isFinite(typed) ? typed : typeof rec.r.hae === "number" && rec.r.hae > -500 ? rec.r.hae : 100;
    const req = gotoRequest({ entity_type: rec.r.entity_type, entity_id: assetId }, { lat: ll[1], lon: ll[0], hae_m: hae }, this.principal(), "Clicked on the map in the L0 web client");
    return this.propose(req);
  }

  async proposeSimple(assetId: string, actionType: string): Promise<ActionView | null> {
    const rec = this.d.store.get(assetId);
    if (!rec) return null;
    return this.propose(simpleRequest(actionType, { entity_type: rec.r.entity_type, entity_id: assetId }, this.principal()));
  }

  private async propose(req: ActionRequest, zoneOp?: ZoneOp): Promise<ActionView | null> {
    let reply: ProposeReply;
    try {
      reply = await this.request<ProposeReply>(SUBJ.propose, req);
    } catch (e) {
      reply = { action_id: "", status: "denied", approvals_needed: 0, denial: { code: "policy_unavailable", message: (e as Error).message } };
    }
    if (zoneOp && reply.action_id) this.zoneOps.set(reply.action_id, zoneOp);
    const a = this.tracker.fromPropose(req, reply);
    this.d.showTab("tasks");
    if (a.status === "awaiting_confirmation") this.openConfirm(a.action_id);
    return a;
  }

  private onAction(a: ActionView): void {
    const op = this.zoneOps.get(a.action_id);
    if (op && TERMINAL.has(a.status)) {
      if (a.status === "executed") {
        if (op.verb === "remove" || !op.kind) this.shared.delete(op.drawingId);
        else this.shared.set(op.drawingId, { zoneId: op.zoneId, kind: op.kind });
      }
      this.zoneOps.delete(a.action_id);
    }
    if (a.action_type === "asset.goto" && a.params && typeof a.params.lat === "number" && typeof a.params.lon === "number") {
      const terminal = TERMINAL.has(a.status);
      const ok = a.status === "executed";
      this.overlay.target(a.action_id, terminal && !ok ? null : [a.params.lon as number, a.params.lat as number], `${a.target?.entity_id ?? ""} goto · ${a.status}`, ok);
    }
    if (this.dialogAction === a.action_id) this.fillConfirm(a);
    this.dirty = true;
  }

  // ---- confirm dialog (asset-command.md §4 step 4) ----------------------------------------------

  openConfirm(actionId: string): void {
    const a = this.tracker.get(actionId);
    if (!a) return;
    this.dialogAction = actionId;
    // Parameters come with the propose reply or action.get; ask now rather than wait for the tick.
    if (!detailsLoaded(a)) void this.fetchAction(actionId);
    this.fillConfirm(a);
    const dlg = $("confirm-dialog") as HTMLDialogElement;
    if (!dlg.open) dlg.showModal();
  }

  /** Who may confirm: for an asset, the effective role there; for a zone, anyone who may share zones. */
  private canConfirm(a: ActionView): boolean {
    if (!a.target) return false;
    if (a.target.entity_type === "zone") return this.general().geofence;
    return abilities(this.roleFor(a.target.entity_id)).confirm;
  }

  /**
   * The client's own check that `params_hash` covers the parameters shown (JCS + SHA-256, the
   * policy service's definition). Starts the digest on first call and re-renders when it lands.
   */
  private hashState(a: ActionView): HashCheck {
    if (!detailsLoaded(a)) return "checking";
    if (!canHash()) return "unavailable";
    const input = a as unknown as HashInput;
    let key: string;
    try {
      key = `${hashDocument(input)}|${a.params_hash}`;
    } catch {
      return "mismatch"; // parameters that cannot be canonicalised cannot be what was hashed
    }
    const cur = this.hashChecks.get(a.action_id);
    if (cur?.key === key) return cur.state;
    const entry = { key, state: "checking" as HashCheck };
    this.hashChecks.set(a.action_id, entry);
    paramsHash(input)
      .then((h) => {
        entry.state = h === null ? "unavailable" : h === a.params_hash ? "verified" : "mismatch";
      })
      .catch(() => {
        entry.state = "unavailable";
      })
      .finally(() => {
        const now = this.tracker.get(a.action_id);
        if (now && this.dialogAction === a.action_id) this.fillConfirm(now);
        this.dirty = true;
      });
    return "checking";
  }

  /** A warning when a zone change creates a movement constraint or frees assets from one. */
  private zoneSafety(a: ActionView): string {
    if (a.target?.entity_type !== "zone" || !a.params) return "";
    const before = this.zoneKindNow(a.target.entity_id);
    const geo = a.params.geo as { kind?: string } | undefined;
    const after = a.action_type === "zone.remove" ? null : ((geo?.kind ?? (a.params.kind as string | undefined) ?? before) as ZoneKind | undefined) ?? null;
    if (loosens(before, after)) {
      const b = before!.replace("_", "-");
      return after === null
        ? `This removes a ${b} zone. Assets will no longer be held by it.`
        : `This changes a ${b} zone to ${after.replace("_", "-")}. Assets will no longer be held by it.`;
    }
    if (after && SAFETY_KINDS.has(after)) return `This ${after.replace("_", "-")} zone will constrain where simulated assets may move. The policy service and each bridge check every movement command against it.`;
    return "";
  }

  private fillConfirm(a: ActionView): void {
    const can = this.canConfirm(a);
    const agent = a.requested_by?.kind === "agent";
    const loaded = detailsLoaded(a);
    const hash = this.hashState(a);
    const gate = confirmGate(a, can, hash);
    const HASH_TEXT: Record<HashCheck, string> = {
      checking: loaded ? "checking…" : "waiting for the parameters",
      verified: "hash verified: it covers exactly these parameters",
      mismatch: "HASH MISMATCH: these parameters are not what the policy service hashed. Do not approve",
      unavailable: "not checked here (needs https or localhost)",
    };
    const rows: [string, string][] = [
      ["Action", `${a.action_type ?? "?"} (level ${a.level ?? "?"})`],
      ["Target", `${a.target?.entity_type ?? "?"} ${a.target?.entity_id ?? "?"}`],
      ["Parameters", loaded ? JSON.stringify(a.params, null, 0) : "Loading parameters from the policy service…"],
      ["Requested by", `${a.requested_by?.id ?? "?"}${a.requested_by?.on_behalf_of ? ` for ${a.requested_by.on_behalf_of}` : ""}`],
      ["Params hash", a.params_hash ?? "?"],
      ["Hash check", HASH_TEXT[hash]],
      ["Confirm by", a.confirm_by ? `${new Date(a.confirm_by).toLocaleTimeString()} (${ago(Date.parse(a.confirm_by))})` : "?"],
      ["Status", a.status],
    ];
    const safety = loaded ? this.zoneSafety(a) : "";
    $("confirm-body").replaceChildren(
      agent ? h("p", { class: "badge agent", "data-testid": "confirm-agent" }, "PROPOSED BY AN AGENT: check every value") : "",
      safety ? h("p", { class: "deny", "data-testid": "confirm-safety" }, safety) : "",
      h("dl", {}, ...rows.map(([k, v]) => h("div", { class: "kv" }, h("dt", {}, k), h("dd", { "data-testid": `confirm-${k.toLowerCase().replace(/ /g, "-")}`, ...(k === "Hash check" ? { "data-state": hash } : {}) }, v)))),
      a.denial ? h("p", { class: "deny", "data-testid": "confirm-denial" }, `${a.denial.code}${a.denial.detail ? ` (${a.denial.detail})` : ""}: ${a.denial.message ?? ""}`) : "",
      gate.why === "loading parameters" ? h("p", { class: "muted", "data-testid": "confirm-loading" }, "Confirm stays off until the policy service returns the action type, target and parameters.") : "",
      can ? "" : h("p", { class: "muted", "data-testid": "confirm-not-allowed" }, "Your mode or the policy service does not let you confirm this action."),
    );
    ($("confirm-approve") as HTMLButtonElement).disabled = !gate.approve;
    ($("confirm-reject") as HTMLButtonElement).disabled = !gate.reject;
  }

  private async decide(decision: "approve" | "reject"): Promise<void> {
    const id = this.dialogAction;
    const a = id ? this.tracker.get(id) : undefined;
    if (!a || !a.params_hash) return;
    const gate = confirmGate(a, this.canConfirm(a), this.hashState(a));
    if (decision === "approve" ? !gate.approve : !gate.reject) return;
    const reason = ($("confirm-reason") as HTMLInputElement).value.trim();
    if (decision === "reject" && !reason) {
      $("confirm-reason").focus();
      ($("confirm-reason") as HTMLInputElement).placeholder = "A reason is required to reject";
      return;
    }
    ($("confirm-approve") as HTMLButtonElement).disabled = true;
    ($("confirm-reject") as HTMLButtonElement).disabled = true;
    try {
      const r = await this.request<ConfirmReply>(SUBJ.confirm, { v: 1, action_id: a.action_id, params_hash: a.params_hash, decision, ...(reason ? { reason } : {}) });
      // A human reject comes back as status `rejected` with denial `rejected_by_human`: that is
      // the outcome asked for, not an error, so it is applied and the dialog closes.
      const rejected = decision === "reject" && r.status === "rejected";
      this.tracker.apply(a.action_id, { status: r.denial && !rejected ? undefined : r.status, command_id: r.command_id, denial: r.denial ?? undefined }, "reply");
      if (!r.denial || rejected) ($("confirm-dialog") as HTMLDialogElement).close();
      else this.fillConfirm(this.tracker.get(a.action_id)!);
    } catch (e) {
      this.tracker.apply(a.action_id, { denial: { code: "policy_unavailable", message: (e as Error).message } }, "reply");
      this.fillConfirm(this.tracker.get(a.action_id)!);
    }
    ($("confirm-reason") as HTMLInputElement).value = "";
  }

  // ---- Hermes (D4) ----------------------------------------------------------------------------------

  mapContext(): MapContext {
    const sel = this.d.selected();
    const toCtx = (r: EntityRecord) => ({
      entity_id: r.r.entity_id,
      entity_type: r.r.entity_type,
      callsign: r.r.callsign,
      lat: Math.round(r.r.lat * 1e6) / 1e6,
      lon: Math.round(r.r.lon * 1e6) / 1e6,
      speed_mps: typeof r.r.speed === "number" && r.r.speed >= 0 ? r.r.speed : undefined,
      course_deg: typeof r.r.course === "number" && r.r.course >= 0 ? r.r.course : undefined,
      stale: this.d.store.isStale(r),
    });
    const view = this.d.globe.viewBbox();
    const on = new Set(this.d.enabledLayers());
    const visible: MapContext["visible"] = [];
    const zones: MapContext["zones"] = [];
    for (const r of this.d.store.all()) {
      if (!on.has(r.layer) || !isLocatable(r.r, r.full)) continue;
      if (r.r.entity_type === "zone") {
        zones.push({ entity_id: r.r.entity_id, callsign: r.r.callsign, kind: r.full?.geo?.kind, center: [r.r.lon, r.r.lat] });
        continue;
      }
      if (view) {
        const [w, s, e, n] = view.bbox;
        if (r.r.lat < s || r.r.lat > n || (w <= e ? r.r.lon < w || r.r.lon > e : r.r.lon < w && r.r.lon > e)) continue;
      }
      if (visible.length < MAX_CONTEXT_ENTITIES) visible.push(toCtx(r));
    }
    const srec = sel ? this.d.store.get(sel) : undefined;
    return {
      selection: srec ? toCtx(srec) : null,
      viewport: view,
      layers: [...on],
      visible,
      zones,
      gaps: this.gaps.gaps().map((g) => `${g.source}: ${g.reason} (${g.layers.join(", ")})`),
    };
  }

  hermesSubject(): string {
    const q = new URLSearchParams(location.search).get("hermes");
    return q && /^agents\.prompt\.hermes\.[A-Za-z0-9_-]+\.[A-Za-z0-9_-]+$/.test(q) ? q : DEFAULT_HERMES_SUBJECT;
  }

  async askHermes(question: string): Promise<void> {
    if (!question.trim() || this.hermesBusy) return;
    this.pushHermes({ from: "me", text: question.trim(), t: Date.now() });
    this.touch();
    if (!this.nc) {
      this.pushHermes({ from: "error", text: "Not connected to the bus.", t: Date.now() });
      return;
    }
    this.hermesBusy = true;
    const chunks: string[] = [];
    try {
      const it = await this.nc.requestMany(this.hermesSubject(), enc.encode(buildPrompt(question, this.mapContext())), {
        strategy: "stall",
        stall: 4000,
        maxWait: 90_000,
        maxMessages: 1000,
      });
      for await (const m of it) {
        if (m.data.length === 0) break; // end-of-stream sentinel
        chunks.push(dec.decode(m.data));
      }
      if (!chunks.length) throw new Error("Hermes did not answer (no responder or timeout)");
      const r = parseReply(chunks);
      this.pushHermes({ from: "hermes", text: r.text || "(no text)", t: Date.now(), proposals: r.proposals });
      for (const id of r.proposals) void this.fetchAction(id);
    } catch (e) {
      this.pushHermes({ from: "error", text: (e as Error).message, t: Date.now() });
    } finally {
      this.hermesBusy = false;
      this.touch();
    }
  }

  // ---- chat (C8) --------------------------------------------------------------------------------------

  async sendChat(text: string): Promise<void> {
    const t = text.trim();
    if (!t) return;
    const room = this.chat.rooms(this.chatChannel).find((r) => r.id === this.chatRoom) ?? this.chat.rooms(null)[0];
    const callsign = ($("chat-callsign") as HTMLInputElement).value.trim() || (this.d.natsUser() || "WEB").toUpperCase();
    const uid = `GeoChat.${this.selfUid()}.${room.id}.${uuid()}`;
    this.chat.addOutgoing(room, t, uid, callsign);
    this.touch();
    try {
      const r = await this.request<{ ok?: boolean; error?: { code: string; message?: string } }>("svc.cot.send", geochatRequest(room, t, { uid: this.selfUid(), callsign }, uid), 4000);
      if (r?.ok) this.chat.setState(uid, "sent");
      else this.chat.setState(uid, "failed", r?.error?.code ?? "refused");
    } catch (e) {
      this.chat.setState(uid, "failed", (e as Error).message);
    }
    this.touch();
  }

  // ---- static DOM wiring -------------------------------------------------------------------------------

  private wireStatic(): void {
    const roleSel = $("role") as HTMLSelectElement;
    roleSel.replaceChildren(...ROLES.map((r) => h("option", { value: r, selected: r === this.role }, r[0].toUpperCase() + r.slice(1))));
    roleSel.addEventListener("change", () => {
      this.role = parseRole(roleSel.value);
      lsSet("l0.role", this.role);
      if (!abilities(this.role).draw) this.cancelModes();
      this.touch();
    });
    $("confirm-approve").addEventListener("click", () => void this.decide("approve"));
    $("confirm-reject").addEventListener("click", () => void this.decide("reject"));
    $("confirm-close").addEventListener("click", () => ($("confirm-dialog") as HTMLDialogElement).close());
    ($("confirm-dialog") as HTMLDialogElement).addEventListener("close", () => (this.dialogAction = null));
    const hermesForm = $("hermes-form") as HTMLFormElement;
    hermesForm.addEventListener("submit", (e) => {
      e.preventDefault();
      const inp = $("hermes-input") as HTMLTextAreaElement;
      const q = inp.value;
      inp.value = "";
      void this.askHermes(q);
    });
    ($("hermes-input") as HTMLTextAreaElement).addEventListener("keydown", (e) => {
      if (e.key === "Enter" && !e.shiftKey) {
        e.preventDefault();
        hermesForm.requestSubmit();
      }
    });
    $("hermes-clear").addEventListener("click", () => {
      this.overlay.clearHighlights();
      for (const id of this.overlay.annotationIds) this.overlay.annotate(id, "remove", {});
      this.touch();
    });
    $("chat-form").addEventListener("submit", (e) => {
      e.preventDefault();
      const inp = $("chat-input") as HTMLInputElement;
      const t = inp.value;
      inp.value = "";
      void this.sendChat(t);
    });
    ($("chat-callsign") as HTMLInputElement).value = "";
    document.addEventListener("keydown", (e) => {
      if ((e.target as HTMLElement)?.closest?.("input, textarea, select, dialog")) return;
      if (e.key === "Escape" && (this.tool || this.taskMode)) this.cancelModes();
      if (e.key === "Enter" && this.tool) this.finishTool();
    });
  }

  // ---- rendering ---------------------------------------------------------------------------------------------

  private render(): void {
    if (!this.dirty) return;
    this.dirty = false;
    this.renderRole();
    this.renderDetailActions();
    this.renderDraw();
    this.renderTasks();
    this.renderGaps();
    this.renderHermes();
    this.renderChat();
  }

  private renderRole(): void {
    const sel = this.d.selected();
    const asset = sel && this.d.store.get(sel) && COMMANDABLE.has(this.d.store.get(sel)!.r.entity_type) ? sel : null;
    const eff = this.roleFor(asset);
    const chip = $("role-chip");
    chip.dataset.role = eff;
    chip.textContent = this.role === "operator" && eff !== "operator" ? (asset ? `observer: not an operator of ${asset}` : "operator: select an asset") : eff;
    chip.title = ROLE_TEXT[eff];
    for (const b of document.querySelectorAll<HTMLElement>("[data-needs]")) {
      const need = b.dataset.needs as keyof ReturnType<typeof abilities>;
      b.hidden = !this.general()[need];
    }
  }

  private renderDetailActions(): void {
    const box = $("detail-actions");
    const sel = this.d.selected();
    const rec = sel ? this.d.store.get(sel) : undefined;
    if (!rec || !COMMANDABLE.has(rec.r.entity_type)) {
      this.swap(box, "none", () => []);
      return;
    }
    const id = rec.r.entity_id;
    const ab = abilities(this.roleFor(id));
    const perm = this.perms.get(id);
    const latest = this.tracker.list().find((a) => a.target?.entity_id === id);
    const sig = [id, this.role, perm?.permission, perm?.denial?.code, this.taskMode?.assetId, latest?.action_id, latest?.status, latest?.denial?.code].join("|");
    if (box.dataset.sig === sig) return;
    box.dataset.sig = sig;
    const status = latest
      ? h(
          "p",
          { class: `task-status st-${latest.status}`, "data-testid": "task-status", "data-status": latest.status },
          `${latest.action_type}: ${latest.status}${latest.denial ? ` (${latest.denial.code}${latest.denial.detail ? `/${latest.denial.detail}` : ""})` : ""}`,
        )
      : null;
    if (!ab.assetCommands) {
      const why =
        this.role === "newcomer"
          ? "Asset commands are hidden in Newcomer mode."
          : this.role === "observer"
            ? "Asset commands are hidden in Observer mode."
            : perm?.permission === "denied"
              ? `The policy service says you do not operate ${id} (${perm.denial?.code ?? "not_authorized"}).`
              : "Asking the policy service whether you operate this asset…";
      box.replaceChildren(h("div", { class: "actions", "data-testid": "detail-actions" }, h("p", { class: "muted", "data-testid": "commands-hidden" }, why), status ?? ""));
      return;
    }
    const goto = h("button", { class: "btn primary", type: "button", "data-testid": "task-goto" }, this.taskMode?.assetId === id ? "Click the destination… (Esc cancels)" : "Go to point…");
    goto.addEventListener("click", () => (this.taskMode ? this.cancelModes() : this.startGoto(id)));
    const rtl = h("button", { class: "btn", type: "button", "data-testid": "task-rtl" }, "Return to launch");
    rtl.addEventListener("click", () => void this.proposeSimple(id, "asset.rtl"));
    const hold = h("button", { class: "btn", type: "button", "data-testid": "task-hold" }, "Hold");
    const hae = h("input", { id: "task-hae", class: "num", type: "number", step: "1", min: "-500", max: "20000", placeholder: typeof rec.r.hae === "number" ? rec.r.hae.toFixed(0) : "HAE m", "aria-label": "Goto altitude, metres HAE (blank: current)", "data-testid": "task-hae", value: this.taskHae }) as HTMLInputElement;
    hae.addEventListener("input", () => (this.taskHae = hae.value));
    hold.addEventListener("click", () => void this.proposeSimple(id, "asset.hold"));
    box.replaceChildren(
      h(
        "div",
        { class: "actions", "data-testid": "detail-actions" },
        h("p", { class: "muted" }, "Commands go to the policy service; nothing moves until a human confirms."),
        h("div", { class: "row" }, goto, h("label", { class: "inline small" }, "alt ", hae, " m HAE"), rtl, hold),
        status ?? "",
      ),
    );
  }

  private renderDraw(): void {
    const box = $("draw-dynamic");
    const ab = this.general();
    $("draw-static").hidden = !ab.draw;
    const t0 = this.tool;
    const sig = [
      ab.draw, t0?.kind, t0?.points.length, this.selectedDrawing, this.cotText.length, this.lassoed.join(","),
      [...this.drawings.keys()].join(","), [...this.shared.entries()].map(([k, v]) => `${k}:${v.kind}`).join(","),
      [...this.zoneOps.keys()].map((aid) => `${aid}:${this.tracker.get(aid)?.status ?? ""}`).join(","),
    ].join("|");
    if (box.dataset.sig === sig) return;
    box.dataset.sig = sig;
    if (!ab.draw) {
      box.replaceChildren(h("p", { class: "empty", "data-testid": "draw-hidden" }, "Drawing is hidden in Newcomer mode."));
      return;
    }
    const tools = h("div", { class: "tools" });
    for (const k of TOOLS) {
      const b = h("button", { type: "button", class: "btn tool", "data-testid": `tool-${k}`, "aria-pressed": this.tool?.kind === k ? "true" : "false" }, TOOL_TITLE[k]);
      b.addEventListener("click", () => (this.tool?.kind === k ? this.cancelModes() : this.startTool(k)));
      tools.append(b);
    }
    const t = this.tool;
    const hint = t
      ? h(
          "div",
          { class: "hint", "data-testid": "draw-hint" },
          `${TOOL_TITLE[t.kind]}: ${TOOL_POINTS[t.kind].hint} (${t.points.length} point${t.points.length === 1 ? "" : "s"})`,
          TOOL_POINTS[t.kind].min ? this.btn("Finish", "draw-finish", () => this.finishTool()) : "",
          this.btn("Cancel", "draw-cancel", () => this.cancelModes()),
        )
      : h("p", { class: "muted" }, "Pick a tool, then click on the map.");
    const list = h("ul", { class: "drawings", "data-testid": "drawings" });
    for (const dr of [...this.drawings.values()].reverse()) {
      const sel = dr.id === this.selectedDrawing;
      const sh = this.shared.get(dr.id);
      const li = h(
        "li",
        { class: `drawing${sel ? " sel" : ""}`, "data-testid": `drawing-${dr.id}`, "data-kind": dr.kind },
        this.btn(`${dr.label}`, "drawing-select", () => {
          this.selectedDrawing = sel ? null : dr.id;
          this.cotText = "";
          this.touch();
        }, "link"),
        h("span", { class: "muted" }, `${TOOL_TITLE[dr.kind]}${sh ? ` · shared as ${sh.kind} ${sh.zoneId}` : ""}`),
      );
      if (sel) li.append(this.drawingActions(dr));
      list.append(li);
    }
    const lasso = this.lassoed.length
      ? h(
          "div",
          { class: "lasso", "data-testid": "lasso-result" },
          h("strong", {}, `Lasso: ${this.lassoed.length} selected`),
          h("ul", {}, ...this.lassoed.map((id) => h("li", {}, this.btn(id, `lasso-${id}`, () => this.d.select(id, true), "link")))),
        )
      : "";
    box.replaceChildren(
      tools,
      hint,
      lasso,
      this.drawings.size ? list : h("p", { class: "muted" }, "No drawings yet."),
    );
  }

  private drawingActions(dr: Drawing): HTMLElement {
    const box = h("div", { class: "drawing-actions" });
    if (canBeZone(dr) && this.general().geofence) {
      const pending = this.zonePending(dr.id);
      if (pending) {
        box.append(
          h(
            "div",
            { class: "row" },
            h("span", { class: "muted", "data-testid": "zone-pending" }, `${pending.action_type} ${pending.status.replace("_", " ")}`),
            pending.status === "awaiting_confirmation" ? this.btn("Review", "zone-review", () => this.openConfirm(pending.action_id)) : "",
          ),
        );
      } else {
        const kind = h("select", { "data-testid": "zone-kind", "aria-label": "Zone kind" }, ...ZONE_KINDS.map((k) => h("option", { value: k, selected: k === (this.shared.get(dr.id)?.kind ?? "aoi") }, k))) as HTMLSelectElement;
        box.append(
          h(
            "div",
            { class: "row" },
            kind,
            this.btn(this.shared.has(dr.id) ? "Update zone" : "Share as zone", "share-zone", () => void this.shareZone(dr.id, kind.value as ZoneKind)),
            this.shared.has(dr.id) ? this.btn("Remove zone", "unshare-zone", () => void this.unshareZone(dr.id)) : "",
          ),
        );
      }
    }
    const dist = h("input", { type: "number", min: "1", step: "1", value: "100", "aria-label": "Buffer distance in metres", "data-testid": "buffer-m", class: "num" }) as HTMLInputElement;
    box.append(
      h(
        "div",
        { class: "row" },
        dist,
        h("span", { class: "muted" }, "m"),
        this.btn("Buffer", "buffer", () => {
          const b = buffer(dr, Number(dist.value), `${dr.id}b${++this.drawSeq}`, Date.now());
          if (b) this.addDrawing(b);
        }),
        this.btn("CoT", "cot-export", () => {
          this.cotText = this.cotFor(dr.id);
          this.touch();
        }),
        this.btn("Delete", "drawing-delete", () => this.removeDrawing(dr.id)),
      ),
    );
    if (this.cotText) {
      const dl = h("a", { class: "btn", "data-testid": "cot-download", download: `${dr.id}.cot.xml`, href: `data:application/xml;charset=utf-8,${encodeURIComponent(this.cotText)}` }, "Download .xml");
      box.append(h("textarea", { class: "cot", readonly: true, rows: "6", "data-testid": "cot-xml", "aria-label": "CoT XML" }, this.cotText), dl);
    }
    return box;
  }

  /** A user action: render now, so the next click or keystroke lands on the updated panel. */
  private touch(): void {
    this.dirty = true;
    this.render();
  }

  /** Rebuild a panel only when what it shows changed, so clicks and typing are never lost. */
  private swap(el: HTMLElement, sig: string, build: () => (Node | string)[]): void {
    if (el.dataset.sig === sig) return;
    el.dataset.sig = sig;
    el.replaceChildren(...build());
  }

  private btn(text: string, testid: string, fn: () => void, cls = "btn"): HTMLButtonElement {
    const b = h("button", { type: "button", class: cls, "data-testid": testid }, text);
    b.addEventListener("click", fn);
    return b;
  }

  private renderTasks(): void {
    const box = $("tab-tasks");
    const pend = this.tracker.pending();
    $("task-count").textContent = String(pend.length);
    const items = this.tracker.list();
    const sig = items.map((a) => `${a.action_id}:${a.status}:${a.denial?.code ?? ""}:${detailsLoaded(a) ? 1 : 0}`).join(",") + `#${[...this.busNotes.values()].join(",")}`;
    if (box.dataset.sig === sig) return;
    box.dataset.sig = sig;
    if (!items.length) {
      box.replaceChildren(h("p", { class: "empty", "data-testid": "tasks-empty" }, "No actions yet. Select an asset and use Go to point, or ask Hermes."), this.busLine());
      return;
    }
    const ul = h("ul", { class: "tasks", "data-testid": "tasks" });
    for (const a of items) {
      // No confirm before the parameters the hash binds are here (asset-command.md §4 step 4).
      const review =
        a.status !== "awaiting_confirmation"
          ? ""
          : detailsLoaded(a)
            ? this.btn("Review and confirm", `review-${a.action_id}`, () => this.openConfirm(a.action_id), "btn primary")
            : h("span", { class: "muted", "data-testid": `review-loading-${a.action_id}` }, "loading parameters…");
      ul.append(
        h(
          "li",
          { class: `task st-${a.status}`, "data-testid": `action-${a.action_id}`, "data-status": a.status },
          h("div", { class: "row" }, h("strong", {}, `${a.action_type ?? "action"} → ${a.target?.entity_id ?? "?"}`), h("span", { class: "pill" }, a.status)),
          h("span", { class: "muted" }, `${a.requested_by?.kind === "agent" ? `agent ${a.requested_by.id}` : (a.requested_by?.id ?? "")} · ${a.action_id}`),
          a.params ? h("code", {}, JSON.stringify(a.params)) : "",
          a.denial ? h("p", { class: "deny" }, `${a.denial.code}${a.denial.detail ? ` (${a.denial.detail})` : ""}: ${a.denial.message ?? ""}`) : "",
          h("small", { class: "muted" }, a.history.map((x) => x.status).join(" → ")),
          review,
        ),
      );
    }
    box.replaceChildren(ul, this.busLine());
  }

  private busLine(): HTMLElement {
    return h("p", { class: "muted small", "data-testid": "bus-notes" }, [...this.busNotes.entries()].map(([k, v]) => `${k} — ${v}`).join(" · "));
  }

  private renderGaps(): void {
    const box = $("tab-gaps");
    const gaps: Gap[] = this.gaps.gaps();
    $("gap-count").textContent = String(gaps.length);
    const sources = this.gaps.sources();
    const head = h("p", { class: "muted" }, `${sources.length} feed${sources.length === 1 ? "" : "s"} reporting${sources.length ? `: ${sources.join(", ")}` : ""}. A feed shows here when its last fetch is older than its freshness limit, it reports an error, or it falls silent.`);
    if (!gaps.length) {
      box.replaceChildren(head, h("p", { class: "empty", "data-testid": "gaps-empty" }, "No intelligence gaps."));
      return;
    }
    const ul = h("ul", { class: "gaps", "data-testid": "gaps" });
    for (const g of gaps) {
      ul.append(
        h(
          "li",
          { class: `gap r-${g.reason}`, "data-testid": "gap-item", "data-source": g.source, "data-reason": g.reason, "data-layers": g.layers.join(",") },
          h("div", { class: "row" }, h("strong", {}, g.source), h("span", { class: "pill" }, g.reason.replace("_", " "))),
          h("span", {}, `Weakens layers: ${g.layers.join(", ") || "none mapped"}`),
          g.alerts.length ? h("span", { class: "muted" }, `and alerts: ${g.alerts.join("; ")}`) : "",
          h("small", { class: "muted" }, `${g.detail} · since ${ago(g.since)}`),
        ),
      );
    }
    box.replaceChildren(head, ul);
  }

  private renderHermes(): void {
    const log = $("hermes-log");
    if (log.dataset.seq !== String(this.hermesSeq) || this.hermesBusy !== (log.dataset.busy === "1")) {
      log.replaceChildren(
        ...this.hermesLog.map((l) =>
          h(
            "li",
            { class: `hl from-${l.from}`, "data-testid": `hermes-${l.from}` },
            h("span", { class: "who" }, l.from === "me" ? "You" : l.from === "hermes" ? "Hermes" : l.from === "map" ? "Map" : "Error"),
            h("span", { class: "txt" }, l.text),
            ...(l.proposals ?? []).map((id) => this.btn(`Proposal ${id}: review`, `hermes-proposal-${id}`, () => this.openConfirm(id), "btn primary")),
          ),
        ),
      );
      log.dataset.busy = this.hermesBusy ? "1" : "0";
      log.dataset.seq = String(this.hermesSeq);
      log.scrollTop = log.scrollHeight;
    }
    if ($("tab-hermes").hidden) return; // the context summary is only worth computing when shown
    const ctx = this.mapContext();
    $("hermes-context").textContent = `Context sent with each question: ${ctx.selection ? `selected ${ctx.selection.callsign ?? ctx.selection.entity_id}, ` : ""}${ctx.visible.length} visible entities, ${ctx.zones.length} zones, layers ${ctx.layers.join(", ")}${ctx.gaps.length ? `, ${ctx.gaps.length} gaps` : ""}. Highlights: ${this.overlay.highlighted.join(", ") || "none"}.`;
    ($("hermes-send") as HTMLButtonElement).disabled = this.hermesBusy;
  }

  private renderChat(): void {
    const chSel = $("chat-channel") as HTMLSelectElement;
    const channels = this.chat.channels();
    const want = ["", ...channels].join("|");
    if (chSel.dataset.opts !== want) {
      chSel.replaceChildren(h("option", { value: "" }, "All channels"), ...channels.map((c) => h("option", { value: c, selected: c === this.chatChannel }, c)));
      chSel.dataset.opts = want;
      chSel.onchange = () => {
        this.chatChannel = chSel.value || null;
        if (!this.chat.rooms(this.chatChannel).some((r) => r.id === this.chatRoom)) this.chatRoom = ALL_ROOM;
        this.touch();
      };
    }
    const rooms = this.chat.rooms(this.chatChannel);
    this.swap($("chat-rooms"), rooms.map((r) => `${r.id}:${r.unread}`).join(",") + `#${this.chatRoom}`, () =>
      rooms.map((r: Room) => {
        const b = h("button", { type: "button", class: `room${r.id === this.chatRoom ? " sel" : ""}`, "data-testid": `room-${r.id}`, "aria-pressed": r.id === this.chatRoom ? "true" : "false" }, `${r.kind === "direct" ? "@" : r.kind === "group" ? "#" : ""}${r.name}${r.unread ? ` (${r.unread})` : ""}`);
        b.addEventListener("click", () => {
          this.chatRoom = r.id;
          this.touch();
        });
        return b;
      }),
    );
    this.chat.markRead(this.chatRoom);
    const msgs = this.chat.messages(this.chatRoom);
    this.swap($("chat-messages"), `${this.chatRoom}#` + msgs.map((m) => `${m.uid}:${m.state ?? ""}`).join(","), () =>
      (msgs.length
        ? msgs.map((m) =>
            h(
              "li",
              { class: `msg${m.mine ? " mine" : ""}`, "data-testid": "chat-msg", "data-from": m.fromUid, "data-state": m.state ?? "" },
              h("span", { class: "who" }, `${m.fromCallsign}`),
              h("span", { class: "txt" }, m.text),
              h("small", { class: "muted" }, `${new Date(m.t).toLocaleTimeString()}${m.state ? ` · ${m.state}${m.error ? ` (${m.error})` : ""}` : ""}`),
            ),
          )
        : [h("li", { class: "muted" }, "No messages in this room.")]),
    );
    const contacts = this.chat.contactList(this.chatChannel);
    this.swap($("chat-contacts"), contacts.map((c) => `${c.uid}:${c.group}:${Math.floor(c.lastT / 10000)}`).join(","), () =>
      (contacts.length
        ? contacts.map((c) => h("li", { "data-testid": `contact-${c.uid}` }, h("strong", {}, c.callsign), h("span", { class: "muted" }, ` ${c.group ?? ""}${c.role ? ` · ${c.role}` : ""} · ${c.via} · ${ago(c.lastT)}`)))
        : [h("li", { class: "muted" }, "No TAK contacts heard yet (tak.cot.out, tak.cot.mesh.rx).")]),
    );
    $("chat-room-name").textContent = rooms.find((r) => r.id === this.chatRoom)?.name ?? ALL_ROOM;
  }
}
