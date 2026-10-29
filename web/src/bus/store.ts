// The client-side entity set: snapshot + delta merge with the contract rules.
//   - entity.md §5 "seq": a record with seq not greater than the one held is ignored.
//   - entity.md §5 "Delete" / layer-delta.md §3: `live: false` removes the entity.
//   - layer-delta.md §1 and §3: core subscribers never see expiry markers, so the client removes
//     an entity itself when now > expiry_t.
//   - layer-snapshot.md §5: dim when now - t > stale.
// The store is the client's view only; the bus stays the authority (onemind-xr CLAUDE.md rule 3).
import { isFull, toRender } from "./normalize";
import type { Entity, RemoveReason, RenderEntity } from "./types";

export interface EntityRecord {
  r: RenderEntity;
  /** The latest full record, when one arrived (core subscription or svc.entity.*). */
  full?: Entity;
  layer: string;
  receivedAt: number;
}

export interface Change {
  upserted: EntityRecord[];
  removed: { id: string; reason: RemoveReason; layer: string }[];
}

export interface StoreStats {
  applied: number;
  ignoredOlder: number;
  invalid: number;
  expired: number;
  removed: number;
}

type Listener = (c: Change) => void;

/** How long a removed id remembers its last seq, so a late older copy cannot resurrect it. */
const TOMBSTONE_MS = 10 * 60 * 1000;

export class EntityStore {
  private recs = new Map<string, EntityRecord>();
  private tombs = new Map<string, { seq: number; at: number }>();
  private listeners = new Set<Listener>();
  private pending: Change = { upserted: [], removed: [] };
  private batchDepth = 0;
  readonly stats: StoreStats = { applied: 0, ignoredOlder: 0, invalid: 0, expired: 0, removed: 0 };

  constructor(private now: () => number = () => Date.now()) {}

  get size(): number {
    return this.recs.size;
  }

  get(id: string): EntityRecord | undefined {
    return this.recs.get(id);
  }

  all(): IterableIterator<EntityRecord> {
    return this.recs.values();
  }

  idsInLayer(layer: string): string[] {
    const out: string[] = [];
    for (const [id, rec] of this.recs) if (rec.layer === layer) out.push(id);
    return out;
  }

  onChange(l: Listener): () => void {
    this.listeners.add(l);
    return () => this.listeners.delete(l);
  }

  /** Group several mutations into one change notification. */
  batch<T>(fn: () => T): T {
    this.batchDepth++;
    try {
      return fn();
    } finally {
      this.batchDepth--;
      if (this.batchDepth === 0) this.flush();
    }
  }

  /**
   * Apply one record (full or render). Returns what happened. `allowEqualSeq` lets an AOI
   * `enter` re-add an entity that left the AOI with the same revision.
   */
  apply(
    raw: unknown,
    layer: string,
    opts: { allowEqualSeq?: boolean } = {},
  ): "added" | "updated" | "removed" | "ignored" | "invalid" {
    const r = toRender(raw);
    if (!r) {
      this.stats.invalid++;
      return "invalid";
    }
    const now = this.now();
    const held = this.recs.get(r.entity_id);
    const tomb = this.tombs.get(r.entity_id);
    const floor = held ? held.r.seq : tomb && now - tomb.at < TOMBSTONE_MS ? tomb.seq : -Infinity;
    if (r.seq < floor || (r.seq === floor && !(opts.allowEqualSeq && !held))) {
      this.stats.ignoredOlder++;
      return "ignored";
    }
    if (!r.live) {
      this.removeInternal(r.entity_id, "not_live", r.seq);
      return held ? "removed" : "ignored";
    }
    if (typeof r.expiry_t === "number" && now > r.expiry_t) {
      // Already expired on arrival (for example a stale snapshot copy): never draw it.
      if (held) this.removeInternal(r.entity_id, "expired", r.seq);
      else this.tombs.set(r.entity_id, { seq: r.seq, at: now });
      this.stats.expired++;
      return held ? "removed" : "ignored";
    }
    const full = isFull(raw) ? (raw as Entity) : held?.full;
    const rec: EntityRecord = { r, full, layer: held?.layer ?? layer, receivedAt: now };
    this.recs.set(r.entity_id, rec);
    this.tombs.delete(r.entity_id);
    this.stats.applied++;
    this.emit({ upserted: [rec], removed: [] });
    return held ? "updated" : "added";
  }

  /** Remove by id (delta `remove`, `exit`, or a delete marker). */
  remove(id: string, reason: RemoveReason): boolean {
    if (!this.recs.has(id)) return false;
    // An AOI exit keeps no tombstone: the entity may re-enter with the same seq.
    this.removeInternal(id, reason, reason === "left_aoi" ? undefined : this.recs.get(id)!.r.seq);
    return true;
  }

  /** Remove every entity whose expiry_t has passed. Returns the removed ids. */
  expire(): string[] {
    const now = this.now();
    const out: string[] = [];
    this.batch(() => {
      for (const [id, rec] of this.recs) {
        const x = rec.r.expiry_t;
        if (typeof x === "number" && now > x) {
          this.removeInternal(id, "expired", rec.r.seq);
          this.stats.expired++;
          out.push(id);
        }
      }
      for (const [id, t] of this.tombs) if (now - t.at > TOMBSTONE_MS) this.tombs.delete(id);
    });
    return out;
  }

  /**
   * Replace a layer's contents with a fresh snapshot (layer-snapshot.md §3.1). Entities of that
   * layer that are not in the snapshot are removed with reason `resnapshot`.
   */
  replaceLayer(layer: string, items: unknown[]): void {
    this.batch(() => {
      const keep = new Set<string>();
      for (const it of items) {
        const id = toRender(it)?.entity_id;
        if (id) keep.add(id);
        this.apply(it, layer);
      }
      for (const id of this.idsInLayer(layer)) {
        if (!keep.has(id)) this.removeInternal(id, "resnapshot", undefined);
      }
    });
  }

  isStale(rec: EntityRecord): boolean {
    const s = rec.r.stale;
    if (typeof s !== "number" || s < 0) return false;
    return this.now() - rec.r.t > s * 1000;
  }

  /** Attach a full record fetched on selection (svc.entity.get) without changing seq order. */
  attachFull(full: Entity): void {
    const rec = this.recs.get(full.entity_id);
    if (!rec) return;
    if (typeof full.seq === "number" && full.seq < rec.r.seq) return;
    rec.full = full;
    this.emit({ upserted: [rec], removed: [] });
  }

  private removeInternal(id: string, reason: RemoveReason, seq: number | undefined): void {
    const rec = this.recs.get(id);
    if (seq !== undefined) this.tombs.set(id, { seq, at: this.now() });
    if (!rec) return;
    this.recs.delete(id);
    this.stats.removed++;
    this.emit({ upserted: [], removed: [{ id, reason, layer: rec.layer }] });
  }

  private emit(c: Change): void {
    this.pending.upserted.push(...c.upserted);
    this.pending.removed.push(...c.removed);
    if (this.batchDepth === 0) this.flush();
  }

  private flush(): void {
    if (!this.pending.upserted.length && !this.pending.removed.length) return;
    const c = this.pending;
    this.pending = { upserted: [], removed: [] };
    for (const l of this.listeners) l(c);
  }
}
