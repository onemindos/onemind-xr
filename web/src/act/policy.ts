// Policy client for actions (FEATURES D3, D5, J2). The client never publishes `cmd.fleet.*`:
// it proposes on `svc.policy.command.propose`, the human confirms on
// `svc.policy.command.confirm`, and the policy service dispatches the signed command
// (docs/contracts/asset-command.md §4). Status comes from `evt.so.action.*` when the connection
// may subscribe to it, and always from polling `svc.policy.action.get`, because a CLIENT account
// user can subscribe only `cmd.map.>` and `_INBOX.>` (omos #562 A19 layout).

export interface Target {
  entity_type: string;
  entity_id: string;
}

export interface ActionRequest {
  v: 1;
  request_id: string;
  action_type: string;
  action_version: number;
  target: Target;
  params: Record<string, unknown>;
  requested_by: { kind: "user" | "agent" | "system"; id: string; on_behalf_of?: string };
  reason?: string;
  corr?: string;
  dry_run?: boolean;
}

export interface Denial {
  code: string;
  message?: string;
  detail?: string;
}

export interface ProposeReply {
  action_id: string;
  status: string;
  level?: string;
  params_hash?: string;
  confirm_by?: string;
  approvals_needed: number;
  denial: Denial | null;
  dry_run?: boolean;
}

export interface ConfirmReply {
  action_id: string;
  status: string;
  approvals_needed: number;
  approvals_received: number;
  command_id?: string;
  denial: Denial | null;
}

export const SUBJ = {
  propose: "svc.policy.command.propose",
  confirm: "svc.policy.command.confirm",
  get: "svc.policy.action.get",
  check: "svc.policy.check",
  audit: "evt.so.action.>",
} as const;

/** Statuses in the order an action moves through them; terminal ones share the top rank. */
const RANK: Record<string, number> = {
  proposed: 1,
  awaiting_confirmation: 2,
  approved: 3,
  dispatched: 4,
  acknowledged: 5,
  executed: 9,
  failed: 9,
  refused: 9,
  denied: 9,
  rejected: 9,
  expired: 9,
};
export const TERMINAL = new Set(["executed", "failed", "refused", "denied", "rejected", "expired"]);

/** RFC 4122 v4 uuid that also works outside a secure context (a phone on plain http over LAN). */
export function uuid(): string {
  const c = globalThis.crypto;
  if (c && typeof c.randomUUID === "function") {
    try {
      return c.randomUUID();
    } catch {
      /* insecure context */
    }
  }
  const b = new Uint8Array(16);
  c.getRandomValues(b);
  b[6] = (b[6] & 0x0f) | 0x40;
  b[8] = (b[8] & 0x3f) | 0x80;
  const h = [...b].map((x) => x.toString(16).padStart(2, "0")).join("");
  return `${h.slice(0, 8)}-${h.slice(8, 12)}-${h.slice(12, 16)}-${h.slice(16, 20)}-${h.slice(20)}`;
}

export function gotoRequest(
  target: Target,
  point: { lat: number; lon: number; hae_m: number; speed_mps?: number },
  requester: string,
  reason?: string,
): ActionRequest {
  const params: Record<string, unknown> = {
    lat: Math.round(point.lat * 1e7) / 1e7,
    lon: Math.round(point.lon * 1e7) / 1e7,
    hae_m: Math.round(point.hae_m * 10) / 10,
  };
  if (point.speed_mps) params.speed_mps = point.speed_mps;
  return {
    v: 1,
    request_id: uuid(),
    action_type: "asset.goto",
    action_version: 1,
    target,
    params,
    requested_by: { kind: "user", id: requester },
    reason,
  };
}

/**
 * A zone change as an action (ONTOLOGY.md §6.2 `zone.create|update|remove`, level A2). The policy
 * service holds it for one human confirm, signs the approved command and sends it on
 * `cmd.map.geofence.{zoneId}.{verb}`; the geofence service (omos #569) applies only signed,
 * approved, unexpired commands. `params` follow #569 `zone.{verb}.v1.schema.json`.
 */
export function zoneRequest(verb: "create" | "update" | "remove", zoneId: string, params: Record<string, unknown>, requester: string, reason?: string): ActionRequest {
  return {
    v: 1,
    request_id: uuid(),
    action_type: `zone.${verb}`,
    action_version: 1,
    target: { entity_type: "zone", entity_id: zoneId },
    params,
    requested_by: { kind: "user", id: requester },
    reason,
  };
}

export function simpleRequest(actionType: string, target: Target, requester: string, params: Record<string, unknown> = {}): ActionRequest {
  return { v: 1, request_id: uuid(), action_type: actionType, action_version: 1, target, params, requested_by: { kind: "user", id: requester } };
}

export interface ActionView {
  action_id: string;
  action_type?: string;
  action_version?: number;
  target?: Target;
  params?: Record<string, unknown>;
  params_hash?: string;
  status: string;
  level?: string;
  confirm_by?: string;
  approvals_needed?: number;
  requested_by?: { kind?: string; id?: string; on_behalf_of?: string };
  denial?: Denial | null;
  command_id?: string;
  /** every status seen, in order (audit trail as the client saw it) */
  history: { status: string; at: number; via: "reply" | "audit" | "get" }[];
  updatedAt: number;
}

type Listener = (a: ActionView) => void;

/**
 * True once the client holds everything the hash binds: action type and version, target and
 * params. An action first seen on `evt.so.action.proposed` has no params (the audit event carries
 * only the hash), so it is not ready until `svc.policy.action.get` answers.
 */
export function detailsLoaded(a: ActionView): boolean {
  return (
    typeof a.action_type === "string" &&
    typeof a.action_version === "number" &&
    !!a.target &&
    typeof a.target.entity_id === "string" &&
    typeof a.target.entity_type === "string" &&
    !!a.params &&
    typeof a.params === "object" &&
    !Array.isArray(a.params) &&
    typeof a.params_hash === "string"
  );
}

/** The client's own check of `params_hash` against the parameters it shows. */
export type HashCheck = "checking" | "verified" | "mismatch" | "unavailable";

export interface ConfirmGate {
  approve: boolean;
  reject: boolean;
  /** why Approve is off, for the dialog ("" when it is on) */
  why: string;
}

/**
 * Whether the confirm dialog may offer Approve and Reject (asset-command.md §4 step 4: the human
 * sees the exact parameters the hash binds). Approve needs the details loaded and a hash that did
 * not fail the client's check. Reject needs only the hash: refusing an action is always safe.
 */
export function confirmGate(a: ActionView, canConfirm: boolean, hash: HashCheck): ConfirmGate {
  const pending = a.status === "awaiting_confirmation";
  const reject = pending && canConfirm && typeof a.params_hash === "string";
  if (!pending) return { approve: false, reject: false, why: "" };
  if (!canConfirm) return { approve: false, reject: false, why: "not allowed" };
  if (!detailsLoaded(a)) return { approve: false, reject, why: "loading parameters" };
  if (hash === "checking") return { approve: false, reject, why: "checking the hash" };
  if (hash === "mismatch") return { approve: false, reject, why: "hash mismatch" };
  return { approve: true, reject, why: "" };
}

/** How long terminal actions stay listed, and how many at most (older ones are dropped). */
export const TERMINAL_RETENTION_MS = 30 * 60_000;
export const MAX_TERMINAL = 200;

/** Merges what the client learns about actions from replies, audit events and polling. */
export class ActionTracker {
  private actions = new Map<string, ActionView>();
  private listeners = new Set<Listener>();
  constructor(private now: () => number = () => Date.now()) {}

  get size(): number {
    return this.actions.size;
  }

  /**
   * Drop terminal actions older than the retention time, and the oldest beyond `maxTerminal`.
   * Open actions are never dropped; ids in `keep` (the one in the dialog) neither.
   */
  prune(keep: Set<string> = new Set(), retentionMs = TERMINAL_RETENTION_MS, maxTerminal = MAX_TERMINAL): string[] {
    const now = this.now();
    const terminal = [...this.actions.values()].filter((a) => TERMINAL.has(a.status) && !keep.has(a.action_id)).sort((a, b) => b.updatedAt - a.updatedAt);
    const drop = terminal.filter((a, i) => i >= maxTerminal || now - a.updatedAt > retentionMs).map((a) => a.action_id);
    for (const id of drop) this.actions.delete(id);
    return drop;
  }

  onChange(fn: Listener): () => void {
    this.listeners.add(fn);
    return () => this.listeners.delete(fn);
  }

  get(id: string): ActionView | undefined {
    return this.actions.get(id);
  }

  list(): ActionView[] {
    return [...this.actions.values()].sort((a, b) => b.updatedAt - a.updatedAt);
  }

  pending(): ActionView[] {
    return this.list().filter((a) => a.status === "awaiting_confirmation");
  }

  open(): ActionView[] {
    return this.list().filter((a) => !TERMINAL.has(a.status));
  }

  /** Apply a partial view. Status never moves backwards (a late poll cannot undo an audit event). */
  apply(id: string, patch: Partial<Omit<ActionView, "history">>, via: "reply" | "audit" | "get"): ActionView {
    const cur = this.actions.get(id) ?? { action_id: id, status: "proposed", history: [], updatedAt: 0 };
    const next: ActionView = { ...cur };
    for (const [k, v] of Object.entries(patch)) {
      if (v !== undefined && k !== "status") (next as unknown as Record<string, unknown>)[k] = v;
    }
    const st = patch.status;
    if (st && (RANK[st] ?? 0) >= (RANK[cur.status] ?? 0) && !(TERMINAL.has(cur.status) && st !== cur.status)) {
      if (st !== cur.status || cur.history.length === 0) next.history = [...cur.history, { status: st, at: this.now(), via }];
      next.status = st;
    }
    next.updatedAt = this.now();
    this.actions.set(id, next);
    for (const l of this.listeners) l(next);
    return next;
  }

  fromPropose(req: ActionRequest, r: ProposeReply): ActionView {
    return this.apply(
      r.action_id || `local-${req.request_id}`,
      {
        action_type: req.action_type,
        action_version: req.action_version,
        target: req.target,
        params: req.params,
        params_hash: r.params_hash,
        level: r.level,
        confirm_by: r.confirm_by,
        approvals_needed: r.approvals_needed,
        requested_by: req.requested_by,
        denial: r.denial,
        status: r.denial ? "denied" : r.status,
      },
      "reply",
    );
  }

  /** `svc.policy.action.get` reply body. */
  fromGet(reply: unknown): ActionView | null {
    const a = (reply as { action?: Record<string, unknown> } | null)?.action;
    if (!a || typeof a.action_id !== "string") return null;
    return this.apply(
      a.action_id,
      {
        action_type: a.action_type as string,
        action_version: typeof a.action_version === "number" ? a.action_version : undefined,
        target: a.target as Target,
        params: a.params && typeof a.params === "object" && !Array.isArray(a.params) ? (a.params as Record<string, unknown>) : undefined,
        params_hash: a.params_hash as string,
        level: a.level as string,
        confirm_by: a.confirm_by as string,
        approvals_needed: a.approvals_needed as number,
        requested_by: a.requested_by as ActionView["requested_by"],
        denial: (a.denial as Denial) ?? null,
        command_id: a.command_id as string,
        status: a.status as string,
      },
      "get",
    );
  }

  /** An `evt.so.action.{event}` envelope (asset-command.md §5). */
  fromAudit(subject: string, env: unknown): ActionView | null {
    const event = subject.split(".")[3];
    const d = (env as { data?: Record<string, unknown> } | null)?.data;
    if (!event || !d || typeof d.action_id !== "string") return null;
    const status = event === "proposed" ? "awaiting_confirmation" : event;
    if (!(status in RANK)) return null;
    return this.apply(
      d.action_id,
      {
        action_type: d.action_type as string,
        target: d.target as Target,
        params_hash: d.params_hash as string,
        level: d.level as string,
        requested_by: d.requested_by as ActionView["requested_by"],
        command_id: d.command_id as string,
        denial: d.code ? { code: d.code as string, message: d.message as string, detail: d.detail as string } : undefined,
        status,
      },
      "audit",
    );
  }
}

/** What a policy dry run says about this viewer's right to command an asset (FEATURES J2). */
export type Permission = "operator" | "denied" | "unknown";

/**
 * The role probe: a dry run of `asset.estop` on the asset. The policy service (omos #554
 * `propose`) checks asset preconditions (online, geofence) before authorization, so a dry run
 * of `goto` on an offline asset would never reach the OpenFGA check. `estop` is unblockable and
 * skips those preconditions, so its dry run answers only "may this person command this asset"
 * (proposer on the action type and operator of the asset). Nothing is stored or sent.
 */
export function roleProbe(target: Target, requester: string): { v: 1; action: ActionRequest } {
  return { v: 1, action: { ...simpleRequest("asset.estop", target, requester), dry_run: true } };
}

export function permissionFromCheck(reply: unknown): { permission: Permission; denial?: Denial } {
  const r = reply as { allowed?: boolean; decision?: ProposeReply; denial?: Denial } | null;
  if (!r) return { permission: "unknown" };
  if (r.allowed === true) return { permission: "operator" };
  const d = r.decision?.denial ?? r.denial ?? undefined;
  // Only an authorization answer says who the viewer is. Anything else (policy unavailable, a
  // stop-line or simulation refusal) fails closed: commands stay hidden.
  if (d?.code === "not_authorized" || d?.code === "agent_cannot_confirm") return { permission: "denied", denial: d };
  return { permission: "unknown", denial: d };
}
