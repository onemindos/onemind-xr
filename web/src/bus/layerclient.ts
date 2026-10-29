// Follows one layer over NATS with the contract paths, best first:
//   1. `svc.layer.snapshot` + `svc.layer.watch` (layer-snapshot.md §3.1, layer-delta.md §2), the
//      required path for every client including the CLIENT account.
//   2. Interim (entity.md §8): core subscription to `ent.{type}.*.state`, then a
//      `svc.entity.query` snapshot (omos PR #552, services/entity: `{types, bbox, limit, cursor}`
//      -> `{entities, cursor}`). The subscription is opened first so nothing is lost between the
//      snapshot and the first live record; the seq rule removes duplicates.
//   3. Core subscription only, when neither service answers (expiry from expiry_t).
// The client never publishes `ent.*` or `cmd.*` (entity.md §2).
import {
  createInbox,
  NoRespondersError,
  RequestError,
  type Msg,
  type NatsConnection,
  type Subscription,
} from "@nats-io/nats-core";
import { entSubject, type LayerDef } from "./catalog";
import type { EntityStore } from "./store";
import type { DeltaBatch, LayerManifest, SnapshotChunk } from "./types";

export type Bus = Pick<NatsConnection, "publish" | "subscribe" | "request">;
export type FeedMode = "connecting" | "layer" | "interim" | "core" | "error" | "stopped";

export interface FeedStatus {
  layer: string;
  mode: FeedMode;
  detail?: string;
  manifest?: LayerManifest;
  lastBatchAt?: number;
  resnapshots: number;
}

export interface Aoi {
  bbox: [number, number, number, number];
}

export interface FeedOptions {
  aoi?: Aoi | null;
  zoom?: number;
  snapshotTimeoutMs?: number;
  /** layer-delta.md §2.2: no batch (or heartbeat) for 30 s means a dead watch. */
  deadWatchMs?: number;
  leaseS?: number;
  maxRateHz?: number;
  queryLimit?: number;
  onStatus?: (s: FeedStatus) => void;
  now?: () => number;
}

export class NoResponders extends Error {
  constructor(subject: string) {
    super(`no responders on ${subject}`);
  }
}

export class LayerError extends Error {
  constructor(public code: string, detail?: string) {
    super(detail ? `${code}: ${detail}` : code);
  }
}

const enc = new TextEncoder();
const dec = new TextDecoder();
const toJson = (o: unknown) => enc.encode(JSON.stringify(o));

function isNoResponders(m: Msg): boolean {
  return m.headers?.code === 503 || (m.data.length === 0 && m.headers?.status === "503");
}

function serviceError(m: Msg): string | null {
  const code = m.headers?.get("Nats-Service-Error-Code");
  if (code) return `${code} ${m.headers?.get("Nats-Service-Error") ?? ""}`.trim();
  return null;
}

function parse<T>(m: Msg): T | null {
  try {
    return JSON.parse(dec.decode(m.data)) as T;
  } catch {
    return null;
  }
}

function insideBbox(lat: number, lon: number, b: [number, number, number, number]): boolean {
  return lon >= b[0] && lon <= b[2] && lat >= b[1] && lat <= b[3];
}

export class LayerFeed {
  status: FeedStatus;
  private subs: Subscription[] = [];
  private watchSub?: Subscription;
  private watchId: string | null = null;
  private lastN = 0;
  private deadTimer?: ReturnType<typeof setTimeout>;
  private renewTimer?: ReturnType<typeof setInterval>;
  private stopped = false;
  private o: Required<Omit<FeedOptions, "aoi" | "zoom" | "onStatus">> & FeedOptions;

  constructor(
    private bus: Bus,
    private store: EntityStore,
    readonly def: LayerDef,
    opts: FeedOptions = {},
  ) {
    this.o = {
      snapshotTimeoutMs: 4000,
      deadWatchMs: 30_000,
      leaseS: 60,
      maxRateHz: 4,
      queryLimit: 200,
      now: () => Date.now(),
      ...opts,
    };
    this.status = { layer: def.id, mode: "connecting", resnapshots: 0 };
  }

  async start(): Promise<FeedMode> {
    this.stopped = false;
    try {
      const seq = await this.snapshot();
      if (this.stopped) return "stopped";
      await this.watch(seq + 1);
      this.setStatus({ mode: "layer", detail: "svc.layer.watch" });
      return "layer";
    } catch (err) {
      if (this.stopped) return "stopped";
      if (!(err instanceof NoResponders)) {
        this.setStatus({ mode: "error", detail: String((err as Error).message ?? err) });
        if (err instanceof LayerError && err.code === "forbidden") return "error";
      }
    }
    return this.startInterim();
  }

  stop(): void {
    this.stopped = true;
    this.clearWatch();
    for (const s of this.subs) s.unsubscribe();
    this.subs = [];
    if (this.watchId) {
      this.bus.publish("svc.layer.unwatch", toJson({ v: 1, watch_id: this.watchId }));
      this.watchId = null;
    }
    this.setStatus({ mode: "stopped" });
  }

  /** Change the AOI (large layers); on the watch path the service answers with enter/exit ops. */
  setAoi(aoi: Aoi | null, zoom?: number): void {
    this.o.aoi = aoi;
    if (zoom !== undefined) this.o.zoom = zoom;
    if (this.status.mode === "layer" && this.watchId) this.sendWatch(this.watchId, undefined);
    if (this.status.mode !== "layer" && aoi && this.def.tier === "large") {
      // Interim: client-side AOI filter.
      this.store.batch(() => {
        for (const id of this.store.idsInLayer(this.def.id)) {
          const r = this.store.get(id)!.r;
          if (!insideBbox(r.lat, r.lon, aoi.bbox)) this.store.remove(id, "left_aoi");
        }
      });
    }
  }

  // ---- path 1: layer service -------------------------------------------------------------

  /**
   * Small layers ask for full records (allowed by layer-snapshot.md §3.1): zones need `geo`,
   * alerts their alert fields and the detail panel provenance, none of which is in the render
   * subset. Medium and large layers use the render subset.
   */
  private get fields(): "full" | "render" {
    return this.def.tier === "small" ? "full" : "render";
  }

  /** layer-snapshot.md §3.1. Returns the snapshot's ent_seq. */
  async snapshot(): Promise<number> {
    const req = {
      v: 1,
      layer: this.def.id,
      aoi: this.def.tier === "large" ? (this.o.aoi ?? null) : null,
      zoom: this.o.zoom ?? 10,
      fields: this.fields,
      max: 5000,
    };
    const chunks = await this.collect("svc.layer.snapshot", req);
    let entSeq = 0;
    const items: unknown[] = [];
    let truncated = false;
    for (const c of chunks) {
      if (c.mode !== "entities") {
        throw new LayerError("unsupported_mode", `L0 draws entities only, got ${c.mode}`);
      }
      entSeq = Math.max(entSeq, c.ent_seq ?? 0);
      truncated ||= !!c.truncated;
      items.push(...(c.items ?? []));
    }
    this.store.replaceLayer(this.def.id, items);
    this.setStatus({ detail: truncated ? "snapshot truncated: zoom in" : undefined });
    return entSeq;
  }

  private collect(subject: string, req: unknown): Promise<SnapshotChunk[]> {
    return new Promise((resolve, reject) => {
      const inbox = createInbox();
      const chunks: SnapshotChunk[] = [];
      let timer: ReturnType<typeof setTimeout>;
      const done = (err?: Error) => {
        clearTimeout(timer);
        sub.unsubscribe();
        if (err) reject(err);
        else resolve(chunks.sort((a, b) => a.part - b.part));
      };
      const arm = () => {
        clearTimeout(timer);
        timer = setTimeout(() => done(new LayerError("timeout", subject)), this.o.snapshotTimeoutMs);
      };
      const sub = this.bus.subscribe(inbox, {
        callback: (_e, m) => {
          if (isNoResponders(m)) return done(new NoResponders(subject));
          const c = parse<SnapshotChunk>(m);
          if (!c) return done(new LayerError("bad_reply", subject));
          if (c.error) return done(new LayerError(c.error, c.detail));
          chunks.push(c);
          if (c.last) return done();
          arm();
        },
      });
      arm();
      this.bus.publish(subject, toJson(req), { reply: inbox });
    });
  }

  /** layer-delta.md §2.1. Resolves after the first reply (the watch id). */
  private watch(fromSeq: number): Promise<void> {
    this.clearWatch();
    this.lastN = 0;
    const inbox = createInbox();
    return new Promise((resolve, reject) => {
      let first = true;
      const firstTimer = setTimeout(() => {
        if (first) {
          first = false;
          reject(new LayerError("timeout", "svc.layer.watch"));
        }
      }, this.o.snapshotTimeoutMs);
      this.watchSub = this.bus.subscribe(inbox, {
        callback: (_e, m) => {
          if (isNoResponders(m)) {
            if (first) {
              first = false;
              clearTimeout(firstTimer);
              reject(new NoResponders("svc.layer.watch"));
            }
            return;
          }
          const body = parse<Partial<DeltaBatch> & { error?: string; lease_s?: number }>(m);
          if (!body) return;
          if (body.error) {
            if (first) {
              first = false;
              clearTimeout(firstTimer);
              if (body.error === "resnapshot") {
                void this.resnapshot("resnapshot requested");
                resolve();
              } else reject(new LayerError(body.error));
            } else if (body.error === "resnapshot") void this.resnapshot("resnapshot requested");
            return;
          }
          this.armDead();
          if (!Array.isArray(body.ops)) {
            // First reply or a renewal acknowledgement.
            if (body.watch_id) this.watchId = body.watch_id;
            if (first) {
              first = false;
              clearTimeout(firstTimer);
              this.startRenew(body.lease_s ?? this.o.leaseS);
              resolve();
            }
            return;
          }
          this.onBatch(body as DeltaBatch);
        },
      });
      this.watchInbox = inbox;
      this.sendWatch(null, fromSeq);
      this.armDead();
    });
  }

  private watchInbox = "";

  private sendWatch(watchId: string | null, fromSeq: number | undefined): void {
    const req = {
      v: 1,
      layer: this.def.id,
      aoi: this.def.tier === "large" ? (this.o.aoi ?? null) : null,
      zoom: this.o.zoom ?? 10,
      fields: this.fields,
      from_seq: fromSeq,
      lease_s: this.o.leaseS,
      max_rate_hz: this.o.maxRateHz,
      watch_id: watchId,
    };
    this.bus.publish("svc.layer.watch", toJson(req), { reply: this.watchInbox });
  }

  /** Apply one delta batch (layer-delta.md §2.2 rules: order, gaps, coalesced ops). */
  onBatch(b: DeltaBatch): void {
    if (this.lastN !== 0 && b.n !== this.lastN + 1) {
      void this.resnapshot(`gap: batch ${this.lastN} then ${b.n}`);
      return;
    }
    this.lastN = b.n;
    if (b.watch_id) this.watchId = b.watch_id;
    this.store.batch(() => {
      for (const op of b.ops) {
        switch (op.op) {
          case "upsert":
            this.store.apply(op.e, this.def.id);
            break;
          case "enter":
            this.store.apply(op.e, this.def.id, { allowEqualSeq: true });
            break;
          case "exit":
            this.store.remove(op.id, op.reason ?? "left_aoi");
            break;
          case "remove":
            this.store.remove(op.id, op.reason ?? "deleted");
            break;
          case "cells":
            break; // density cells are not drawn by L0 (zoom in for entities)
        }
      }
    });
    this.setStatus({ lastBatchAt: this.o.now(), ...(b.meta ? { manifest: b.meta } : {}) });
  }

  private resnapshotting = false;

  private async resnapshot(why: string): Promise<void> {
    if (this.stopped || this.resnapshotting) return;
    this.resnapshotting = true;
    try {
      await this.doResnapshot(why);
    } finally {
      this.resnapshotting = false;
    }
  }

  private async doResnapshot(why: string): Promise<void> {
    this.status.resnapshots++;
    this.setStatus({ detail: `re-snapshot (${why})` });
    this.clearWatch();
    try {
      const seq = await this.snapshot();
      await this.watch(seq + 1);
      this.setStatus({ mode: "layer", detail: undefined });
    } catch (err) {
      if (this.stopped) return;
      if (err instanceof NoResponders) {
        await this.startInterim();
        return;
      }
      this.setStatus({ mode: "error", detail: String((err as Error).message) });
      setTimeout(() => void this.resnapshot("retry"), 5000);
    }
  }

  private armDead(): void {
    clearTimeout(this.deadTimer);
    this.deadTimer = setTimeout(() => void this.resnapshot("dead watch"), this.o.deadWatchMs);
  }

  private startRenew(leaseS: number): void {
    clearInterval(this.renewTimer);
    this.renewTimer = setInterval(() => {
      if (this.watchId) this.sendWatch(this.watchId, undefined);
    }, (leaseS * 1000) / 2);
  }

  private clearWatch(): void {
    clearTimeout(this.deadTimer);
    clearInterval(this.renewTimer);
    this.watchSub?.unsubscribe();
    this.watchSub = undefined;
  }

  // ---- paths 2 and 3: interim ------------------------------------------------------------

  private async startInterim(): Promise<FeedMode> {
    this.clearWatch();
    for (const s of this.subs) s.unsubscribe();
    this.subs = [];
    for (const t of this.def.entityTypes) {
      this.subs.push(
        this.bus.subscribe(entSubject(t), {
          callback: (_e, m) => {
            const body = parse<Record<string, unknown>>(m);
            if (!body) {
              this.store.stats.invalid++;
              return;
            }
            const aoi = this.def.tier === "large" ? this.o.aoi : null;
            if (aoi && typeof body.lat === "number" && typeof body.lon === "number") {
              if (!insideBbox(body.lat, body.lon, aoi.bbox) && body.live !== false) {
                const id = String(body.entity_id ?? "");
                if (this.store.get(id)) this.store.remove(id, "left_aoi");
                return;
              }
            }
            this.store.apply(body, this.def.id);
            this.status.lastBatchAt = this.o.now();
          },
        }),
      );
    }
    try {
      const n = await this.querySnapshot();
      this.setStatus({ mode: "interim", detail: `svc.entity.query (${n}) + ent.*.*.state` });
      return "interim";
    } catch (err) {
      const detail =
        err instanceof NoResponders
          ? "ent.*.*.state only (no svc.layer / svc.entity responders)"
          : `ent.*.*.state only (${(err as Error).message})`;
      this.setStatus({ mode: "core", detail });
      return "core";
    }
  }

  /** Page through svc.entity.query (omos PR #552). Returns the number of records applied. */
  private async querySnapshot(): Promise<number> {
    let cursor: string | null = null;
    let n = 0;
    const bbox = this.def.tier === "large" && this.o.aoi ? this.o.aoi.bbox : undefined;
    for (let page = 0; page < 500; page++) {
      let m: Msg;
      try {
        m = await this.bus.request(
          "svc.entity.query",
          toJson({ types: this.def.entityTypes, bbox, limit: this.o.queryLimit, cursor: cursor ?? "" }),
          { timeout: this.o.snapshotTimeoutMs },
        );
      } catch (err) {
        if (
          err instanceof NoRespondersError ||
          (err instanceof RequestError && err.isNoResponders()) ||
          /no responders/i.test(String((err as Error).message))
        ) {
          throw new NoResponders("svc.entity.query");
        }
        throw err;
      }
      const se = serviceError(m);
      if (se) throw new LayerError("service_error", se);
      const body = parse<{ entities?: unknown[]; cursor?: string | null }>(m);
      if (!body) throw new LayerError("bad_reply", "svc.entity.query");
      this.store.batch(() => {
        for (const e of body.entities ?? []) {
          this.store.apply(e, this.def.id);
          n++;
        }
      });
      cursor = body.cursor ?? null;
      if (!cursor) break;
    }
    return n;
  }

  private setStatus(p: Partial<FeedStatus>): void {
    this.status = { ...this.status, ...p };
    this.o.onStatus?.(this.status);
  }
}

/** Fetch one full entity for the detail panel (`svc.entity.get`, entity.md §8). */
export async function getEntity(bus: Bus, id: string, timeoutMs = 2000): Promise<unknown | null> {
  try {
    const m = await bus.request("svc.entity.get", toJson({ entity_id: id }), { timeout: timeoutMs });
    if (serviceError(m)) return null;
    return parse(m);
  } catch {
    return null;
  }
}
