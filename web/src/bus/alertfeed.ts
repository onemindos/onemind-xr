// Open alerts from the omos geofence and alert service (FEATURES D6; omos #569 services/geofence):
//   1. snapshot: `svc.geofence.alerts` `{}` -> `{v, alerts: [...]}`, the open set;
//   2. live: `evt.so.alert.{raised|updated|resolved}` envelopes (`data` = alert.v1, #569 D1-P3).
// Both reach a CLIENT user through #562 imports (A19-P11). The client does not read alerts from
// `ent.alert.*.state`: CLIENT imports no `ent.*` stream and nobody writes alert entities yet
// (#569 D1-P5); that was the onemind-xr INT E2E-2 bug F-ALERT.
// When the live subscription is refused (an older #562 render, or a role without it) the feed
// polls the snapshot instead. When the snapshot itself is refused (J2 newcomer role, #565) the
// feed shows nothing and retries rarely: the server decides who sees alerts, not this client.
// One alert per condition, `seq` grows by one per fact; the higher seq wins, and a resolved
// alert leaves a tombstone so an older snapshot cannot bring it back.
import type { Bus } from "./layerclient";

export interface ServiceAlert {
  v?: number;
  alert_id: string;
  alert_kind: string;
  level: string;
  state: string;
  about?: { entity_type?: string; entity_id?: string; feed?: string };
  zone_id?: string;
  zone_kind?: string;
  reason?: string;
  message?: string;
  raised_by?: string;
  opened_t?: number;
  updated_t?: number;
  resolved_t?: number;
  seq: number;
  location?: { lat?: number; lon?: number; hae?: number };
  simulated?: boolean;
}

export type AlertPath = "pending" | "ok" | "denied" | "no_responders" | "error";

export interface AlertFeedStatus {
  snapshot: AlertPath;
  live: AlertPath;
  detail?: string;
  lastSnapshotAt?: number;
}

export interface AlertFeedOptions {
  /** Snapshot refresh while the live subscription works (reconciles missed facts). */
  refreshMs?: number;
  /** Snapshot polling when the live subscription is refused. */
  pollMs?: number;
  /** Retry after the snapshot was refused or had no responders. */
  deniedRetryMs?: number;
  timeoutMs?: number;
  onChange?: () => void;
  now?: () => number;
}

export const ALERT_SNAPSHOT = "svc.geofence.alerts";
export const ALERT_LIVE = "evt.so.alert.>";

const enc = new TextEncoder();
const dec = new TextDecoder();

/** One alert.v1 record, or null if it lacks the fields the list needs. */
export function parseServiceAlert(raw: unknown): ServiceAlert | null {
  if (!raw || typeof raw !== "object") return null;
  const a = raw as Record<string, unknown>;
  if (typeof a.alert_id !== "string" || !a.alert_id || typeof a.seq !== "number") return null;
  return {
    ...(a as unknown as ServiceAlert),
    alert_kind: typeof a.alert_kind === "string" ? a.alert_kind : "alert",
    level: typeof a.level === "string" ? a.level : "advisory",
    state: typeof a.state === "string" ? a.state : "open",
  };
}

/** The alert inside an `evt.so.alert.*` envelope (or a bare record). */
export function alertFromEnvelope(body: unknown): ServiceAlert | null {
  if (!body || typeof body !== "object") return null;
  const env = body as Record<string, unknown>;
  return parseServiceAlert(env.data && typeof env.data === "object" ? env.data : env);
}

const isPermission = (e: unknown) => /permission/i.test(String((e as Error)?.message ?? e));
const isNoResponders = (e: unknown) => /no responders|503/i.test(String((e as Error)?.message ?? e));

export class AlertStore {
  private open = new Map<string, ServiceAlert>();
  /** alert_id -> seq of the resolving fact */
  private gone = new Map<string, number>();

  /** Apply one fact; returns true if the open set changed. */
  apply(a: ServiceAlert): boolean {
    const cur = this.open.get(a.alert_id);
    if (cur && cur.seq >= a.seq) return false;
    const g = this.gone.get(a.alert_id);
    if (g !== undefined && g >= a.seq) return false;
    if (a.state === "resolved") {
      this.gone.set(a.alert_id, a.seq);
      return this.open.delete(a.alert_id);
    }
    this.gone.delete(a.alert_id);
    this.open.set(a.alert_id, a);
    return true;
  }

  /**
   * Reconcile with a snapshot of the open set taken at `askedAt`. An alert missing from it was
   * resolved while we were not listening, unless a live fact newer than the request arrived.
   */
  reconcile(alerts: ServiceAlert[], askedAt: number, seenAt: Map<string, number>): boolean {
    let changed = false;
    const ids = new Set<string>();
    for (const a of alerts) {
      ids.add(a.alert_id);
      if (this.apply(a)) changed = true;
    }
    for (const id of [...this.open.keys()]) {
      if (ids.has(id) || (seenAt.get(id) ?? 0) > askedAt) continue;
      this.open.delete(id);
      changed = true;
    }
    return changed;
  }

  list(): ServiceAlert[] {
    return [...this.open.values()];
  }

  clear(): void {
    this.open.clear();
    this.gone.clear();
  }
}

export class AlertFeed {
  readonly store = new AlertStore();
  status: AlertFeedStatus = { snapshot: "pending", live: "pending" };
  private sub?: { unsubscribe(): void };
  private timer?: ReturnType<typeof setTimeout>;
  private stopped = false;
  private inflight = false;
  private seenAt = new Map<string, number>();
  private o: Required<Omit<AlertFeedOptions, "onChange">> & Pick<AlertFeedOptions, "onChange">;

  constructor(private bus: Bus & { flush?: () => Promise<void> }, opts: AlertFeedOptions = {}) {
    this.o = {
      refreshMs: 60_000,
      pollMs: 10_000,
      deniedRetryMs: 300_000,
      timeoutMs: 5000,
      now: () => Date.now(),
      ...opts,
    };
  }

  start(): void {
    this.stopped = false;
    try {
      this.sub = this.bus.subscribe(ALERT_LIVE, {
        callback: (err, m) => {
          if (this.stopped) return;
          if (err) {
            // Refused subscription (nats-core hands it here with an empty message): poll instead.
            this.status = { ...this.status, live: isPermission(err) ? "denied" : "error", detail: err.message };
            this.o.onChange?.();
            // Poll instead, unless the server refuses the snapshot too (then keep the long retry).
            if (this.status.snapshot !== "denied") this.schedule(this.o.pollMs);
            return;
          }
          let body: unknown = null;
          try {
            body = JSON.parse(dec.decode(m.data));
          } catch {
            return;
          }
          const a = alertFromEnvelope(body);
          if (!a) return;
          this.seenAt.set(a.alert_id, this.o.now());
          if (this.status.live !== "ok") this.status = { ...this.status, live: "ok" };
          if (this.store.apply(a)) this.o.onChange?.();
        },
      });
      // The server answers a refused SUB with -ERR before the PONG of this flush, so a clean
      // round trip means the subscription stands.
      void this.bus.flush?.().then(
        () => {
          if (this.stopped || this.status.live !== "pending") return;
          this.status = { ...this.status, live: "ok" };
          this.o.onChange?.();
        },
        () => undefined,
      );
    } catch (e) {
      this.status = { ...this.status, live: "error", detail: (e as Error).message };
    }
    void this.snapshot();
  }

  stop(): void {
    this.stopped = true;
    clearTimeout(this.timer);
    this.sub?.unsubscribe();
    this.sub = undefined;
  }

  alerts(): ServiceAlert[] {
    return this.store.list();
  }

  private schedule(ms: number): void {
    if (this.stopped) return;
    clearTimeout(this.timer);
    this.timer = setTimeout(() => void this.snapshot(), ms);
  }

  /** Ask for the open set once; reschedules itself. */
  async snapshot(): Promise<void> {
    if (this.stopped || this.inflight) return;
    this.inflight = true;
    const askedAt = this.o.now();
    let next: number | null = null;
    try {
      const m = await this.bus.request(ALERT_SNAPSHOT, enc.encode("{}"), { timeout: this.o.timeoutMs });
      if (this.stopped) return;
      const body = JSON.parse(dec.decode(m.data)) as { alerts?: unknown[]; error?: unknown };
      if (!Array.isArray(body.alerts)) throw new Error(`bad reply${body.error ? `: ${JSON.stringify(body.error)}` : ""}`);
      const list = body.alerts.map(parseServiceAlert).filter((a): a is ServiceAlert => !!a);
      this.store.reconcile(list, askedAt, this.seenAt);
      this.status = { ...this.status, snapshot: "ok", detail: undefined, lastSnapshotAt: this.o.now() };
      this.o.onChange?.(); // also on no change: the status line moves
    } catch (e) {
      if (this.stopped) return;
      const denied = isPermission(e);
      const none = !denied && isNoResponders(e);
      this.status = { ...this.status, snapshot: denied ? "denied" : none ? "no_responders" : "error", detail: (e as Error).message };
      if (denied) {
        // The server withholds alerts from this user (J2): show none, do not keep knocking.
        this.store.clear();
        next = this.o.deniedRetryMs;
      } else if (none) next = this.o.pollMs * 3;
      this.o.onChange?.();
    } finally {
      this.inflight = false;
    }
    // Decided after the reply: the live subscription may have been refused meanwhile.
    this.schedule(next ?? (this.status.live === "ok" || this.status.live === "pending" ? this.o.refreshMs : this.o.pollMs));
  }
}
