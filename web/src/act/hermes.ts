// Hermes panel logic (FEATURES D4). The client prompts Hermes on its existing agent subject
// `agents.prompt.hermes.{owner}.{session}` (omos platform/hermes/agent/FABRIC-REFERENCE.md;
// CLIENT import in omos #562), with the map as context. Hermes acts on the map only through
// reversible `cmd.map.>` (doctrine §9.7) and on assets only by proposing through policy; the
// client renders the first and shows the second as pending confirms.
//
// The gateway (synadia-ai/hermes-nats-gateway) takes a text prompt (`nats req … "<task>"` in
// FABRIC-REFERENCE.md). Its reply framing is not verified here, so the client accepts either one
// reply or several chunks ending in an empty message or a pause, and either plain text or a JSON
// object `{text, proposals?: [action_id]}`.
import type { LonLat } from "./geo";

export const DEFAULT_HERMES_SUBJECT = "agents.prompt.hermes.zeus.legacy";

export interface ContextEntity {
  entity_id: string;
  entity_type: string;
  callsign?: string;
  lat: number;
  lon: number;
  speed_mps?: number;
  course_deg?: number;
  stale?: boolean;
}

export interface MapContext {
  selection: ContextEntity | null;
  viewport: { bbox: [number, number, number, number]; zoom: number } | null;
  layers: string[];
  /** visible entities (capped) so Hermes can answer "what's moving" without a firehose */
  visible: ContextEntity[];
  zones: { entity_id: string; callsign?: string; kind?: string; center: LonLat }[];
  gaps: string[];
}

export const MAX_CONTEXT_ENTITIES = 50;

/** The prompt text: the question, then a fenced JSON block with the map context. */
export function buildPrompt(question: string, ctx: MapContext): string {
  const c = { ...ctx, visible: ctx.visible.slice(0, MAX_CONTEXT_ENTITIES) };
  return `${question.trim()}\n\n[map-context v1]\n${JSON.stringify(c)}\n[/map-context]`;
}

/** Split a prompt built by buildPrompt (used by the scripted Hermes stand-in and tests). */
export function splitPrompt(text: string): { question: string; context: MapContext | null } {
  const m = /\n\n\[map-context v1\]\n([\s\S]*)\n\[\/map-context\]\s*$/.exec(text);
  if (!m) return { question: text.trim(), context: null };
  try {
    return { question: text.slice(0, m.index).trim(), context: JSON.parse(m[1]) as MapContext };
  } catch {
    return { question: text.slice(0, m.index).trim(), context: null };
  }
}

export interface HermesReply {
  text: string;
  proposals: string[];
}

export function parseReply(chunks: string[]): HermesReply {
  const proposals: string[] = [];
  const texts: string[] = [];
  for (const raw of chunks) {
    const s = raw.trim();
    if (!s) continue;
    if (s.startsWith("{")) {
      try {
        const o = JSON.parse(s) as { text?: unknown; proposals?: unknown };
        if (typeof o.text === "string") texts.push(o.text);
        if (Array.isArray(o.proposals)) for (const p of o.proposals) if (typeof p === "string") proposals.push(p);
        continue;
      } catch {
        /* plain text that starts with a brace */
      }
    }
    texts.push(raw);
  }
  return { text: texts.join(""), proposals };
}

// ---- cmd.map.> (doctrine §9.7) ------------------------------------------------------------------

export type MapAction =
  | { kind: "highlight"; uid: string; color: string; ring: boolean; label?: string; clear?: boolean }
  | { kind: "annotation"; id: string; verb: string; lat?: number; lon?: number; label?: string; color?: string; note?: string }
  | { kind: "viewport"; lat: number; lon: number; zoom?: number }
  | { kind: "filter"; layers?: string[]; bbox?: [number, number, number, number] }
  | { kind: "geofence"; id: string; verb: string; body: Record<string, unknown> };

const TOKEN = /^[A-Za-z0-9_-]{1,128}$/;
const num = (v: unknown) => (typeof v === "number" && Number.isFinite(v) ? v : undefined);
const str = (v: unknown, max = 200) => (typeof v === "string" ? v.slice(0, max) : undefined);
const COLOR = /^#[0-9a-fA-F]{6}$/;

/** Parse one `cmd.map.*` message into a view action, or null when malformed or unknown. */
export function parseMapCommand(subject: string, body: unknown): MapAction | null {
  const t = subject.split(".");
  if (t[0] !== "cmd" || t[1] !== "map") return null;
  const b = (body && typeof body === "object" ? body : {}) as Record<string, unknown>;
  if (t[2] === "highlight" && t.length === 4 && TOKEN.test(t[3])) {
    const color = str(b.color, 7);
    return { kind: "highlight", uid: t[3], color: color && COLOR.test(color) ? color : "#FF4FD8", ring: b.ring !== false, label: str(b.label, 40), clear: b.clear === true };
  }
  if (t[2] === "annotation" && t.length === 5 && TOKEN.test(t[3]) && ["create", "update", "remove"].includes(t[4])) {
    const lat = num(b.lat);
    const lon = num(b.lon);
    if (t[4] !== "remove" && (lat === undefined || lon === undefined || Math.abs(lat) > 90 || Math.abs(lon) > 180)) return null;
    const color = str(b.color, 7);
    return { kind: "annotation", id: t[3], verb: t[4], lat, lon, label: str(b.label, 80), color: color && COLOR.test(color) ? color : undefined, note: str(b.note, 500) };
  }
  if (t[2] === "viewport" && t[3] === "set" && t.length === 4) {
    const lat = num(b.lat);
    const lon = num(b.lon);
    if (lat === undefined || lon === undefined || Math.abs(lat) > 90 || Math.abs(lon) > 180) return null;
    return { kind: "viewport", lat, lon, zoom: num(b.zoom) };
  }
  if (t[2] === "filter" && t[3] === "set" && t.length === 4) {
    const layers = Array.isArray(b.layers) ? b.layers.filter((x): x is string => typeof x === "string").slice(0, 50) : undefined;
    const bbox = Array.isArray(b.bbox) && b.bbox.length === 4 && b.bbox.every((x) => typeof x === "number") ? (b.bbox as [number, number, number, number]) : undefined;
    return { kind: "filter", layers, bbox };
  }
  if (t[2] === "geofence" && t.length === 5 && TOKEN.test(t[3]) && ["create", "update", "remove"].includes(t[4])) {
    return { kind: "geofence", id: t[3], verb: t[4], body: b };
  }
  return null;
}

/** Height in metres for a doctrine `zoom` (web-map zoom level), for flying the camera. */
export function zoomToHeight(zoom: number | undefined): number {
  const z = zoom === undefined ? 12 : Math.max(0, Math.min(20, zoom));
  return 35_000_000 / 2 ** z;
}
