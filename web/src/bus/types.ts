// Types for the records the L0 client reads. Field names follow the onemind-xr contracts
// exactly: docs/contracts/entity.md §4 (full entity), docs/contracts/layer-snapshot.md §4
// (render entity), docs/contracts/layer-delta.md §2.2 (delta batch). Consumers ignore unknown
// fields (entity.md §4.1), so every interface here is open.

export type Disposition =
  | "friendly"
  | "assumed_friendly"
  | "neutral"
  | "unknown"
  | "suspicious"
  | "pending";

/** Render entity: layer-snapshot.md §4 (the `fields: "render"` subset). */
export interface RenderEntity {
  entity_id: string;
  entity_type: string;
  lat: number;
  lon: number;
  hae?: number;
  course?: number;
  speed?: number;
  callsign?: string;
  cot_type?: string;
  disposition?: Disposition | string;
  t: number;
  stale?: number;
  expiry_t?: number | null;
  seq: number;
  live: boolean;
  origin?: string;
  simulated?: boolean;
  [k: string]: unknown;
}

export interface Marking {
  tlp?: string;
  releasable_to?: string[];
}

export interface GeoShape {
  shape: "point" | "line" | "polygon" | "circle" | "corridor";
  coords: { type: string; coordinates: unknown };
  radius_m?: number;
  kind?: string;
  alt_min_m?: number;
  alt_max_m?: number;
}

export interface MediaRef {
  kind: string;
  url?: string;
  object?: string;
  sha256?: string;
  role?: string;
}

export interface SourceRef {
  integration?: string;
  source_id?: string;
  data_type?: string;
  subject?: string;
  stream?: string;
  seq?: number;
  first_seen_t?: number;
  last_seen_t?: number;
  source_t?: number;
  components?: string[];
}

/** Full entity: entity.md §4.1 (groups A, B and C). Only the fields the client reads are typed. */
export interface Entity extends RenderEntity {
  uid?: string;
  type?: string;
  t: number;
  meta?: Record<string, unknown>;
  confidence?: number;
  properties?: Record<string, unknown>;
  v?: number;
  lifecycle?: string;
  created_t?: number;
  pinned?: boolean;
  kind?: string;
  template?: string;
  traits?: string[];
  environment?: string;
  aliases?: { name?: string | null; ids?: { type: string; id: string }[] };
  location?: {
    lat?: number;
    lon?: number;
    hae_m?: number;
    agl_m?: number;
    course_deg?: number;
    speed_mps?: number;
    measured_t?: number;
  };
  uncertainty?: { ce_m?: number; le_m?: number };
  geo?: GeoShape | null;
  status?: { activity?: string; role?: string; mode?: string };
  health?: {
    connection?: string;
    overall?: string;
    battery_pct?: number;
    link_rssi?: number;
    alerts?: { code: string; level: string; since_t?: number }[];
  };
  current_task?: { task_id: string; state: string } | null;
  route?: { destination?: unknown; eta_t?: number; mission_id?: string } | null;
  media?: MediaRef[];
  indicators?: {
    simulated?: boolean;
    exercise?: boolean;
    emergency?: boolean;
    starred?: boolean;
    egress_tak?: boolean;
  };
  marking?: Marking;
  provenance?: { primary?: string; how?: string; source_t?: number };
  sources?: SourceRef[];
  confidence_parts?: { source_prior?: number; track_quality?: number; corroboration?: number };
  conflicts?: { attribute: string; kept: unknown; rejected: unknown; sources?: string[] }[];
  relationships?: { id: string; type: string; to: string; since_t?: number; source?: string }[];
}

/** One op of a delta batch: layer-delta.md §2.2. */
export type DeltaOp =
  | { op: "upsert"; e: RenderEntity }
  | { op: "enter"; e: RenderEntity }
  | { op: "exit"; id: string; reason?: string }
  | { op: "remove"; id: string; reason?: string }
  | { op: "cells"; z: number; cells: [number, number, number, number, number][] };

export interface DeltaBatch {
  v: number;
  watch_id: string;
  layer: string;
  n: number;
  from_seq?: number;
  to_seq?: number;
  as_of?: number;
  mode?: "entities" | "cells";
  ops: DeltaOp[];
  meta?: LayerManifest | null;
}

/** Snapshot reply chunk: layer-snapshot.md §3.1. */
export interface SnapshotChunk {
  v: number;
  layer: string;
  part: number;
  last: boolean;
  mode: "entities" | "cells" | "object";
  ent_seq: number;
  as_of: number;
  truncated?: boolean;
  items: unknown[];
  error?: string;
  detail?: string;
}

/** Layer manifest: layer-snapshot.md §2 (fields the client reads). */
export interface LayerManifest {
  v: number;
  layer: string;
  title?: string;
  tier?: string;
  count?: number;
  as_of?: number;
  freshness?: { status?: "fresh" | "degraded" | "stale" | "down"; limit_s?: number };
  [k: string]: unknown;
}

export type RemoveReason =
  | "expired"
  | "deleted"
  | "not_live"
  | "released_out"
  | "left_aoi"
  | "resnapshot"
  | string;
