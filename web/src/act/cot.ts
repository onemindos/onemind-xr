// CoT XML for drawings (FEATURES B11 "shareable as CoT") and a small CoT reader for GeoChat and
// contacts (FEATURES C8). Types and detail elements follow docs/contracts/cot-projection.md §5.5:
//   polygon → u-d-f (closed <link point>), circle → u-d-c-c (<shape><ellipse>), rectangle →
//   u-d-r (four <link point>), line → b-m-r (<link type="b-m-p-w" relation="c">), point and
//   label → b-m-p-s-m (spot marker). Shapes the contract has no type for are exported with the
//   types it does have: sector and buffer as a closed u-d-f, range rings as one u-d-c-c per ring,
//   range-and-bearing as a two-point open u-d-f with the range and bearing in <remarks>.
// Unverified (cot-projection.md §12): the <ellipse> attribute names and the u-d-r corner order.
import { anchor, areaRings, measure, radiusM, type Drawing } from "./drawing";
import type { LonLat } from "./geo";

export function xmlEscape(s: string): string {
  return s.replace(/[&<>"']/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&apos;" })[c]!);
}

const iso = (ms: number) => new Date(ms).toISOString();
const pt = ([lon, lat]: LonLat) => `${lat.toFixed(7)},${lon.toFixed(7)}`;

export interface CotOpts {
  now: number;
  staleMs?: number;
  /** uid prefix: drawings become `OM.draw.{id}` (cot-projection.md §2 `OM.` uid namespace) */
  uidPrefix?: string;
  /** ARGB colour as a signed 32-bit integer, as ATAK writes it */
  strokeArgb?: number;
  fillArgb?: number;
}

const CYAN = -16711681; // 0xFF00FFFF
const CYAN_FILL = 1308622847; // 0x4E00FFFF (alpha ~0.3)

function eventXml(uid: string, type: string, at: LonLat, o: CotOpts, detail: string): string {
  const stale = o.now + (o.staleMs ?? 24 * 3600_000);
  return (
    `<event version="2.0" uid="${xmlEscape(uid)}" type="${type}" how="h-e" time="${iso(o.now)}" start="${iso(o.now)}" stale="${iso(stale)}">` +
    `<point lat="${at[1].toFixed(7)}" lon="${at[0].toFixed(7)}" hae="9999999.0" ce="9999999.0" le="9999999.0"/>` +
    `<detail>${detail}</detail></event>`
  );
}

function style(o: CotOpts, filled: boolean): string {
  return (
    `<strokeColor value="${o.strokeArgb ?? CYAN}"/><strokeWeight value="3.0"/>` +
    (filled ? `<fillColor value="${o.fillArgb ?? CYAN_FILL}"/>` : "") +
    `<labels_on value="true"/>`
  );
}

const contact = (callsign: string) => `<contact callsign="${xmlEscape(callsign)}"/>`;
const remarks = (s: string) => (s ? `<remarks>${xmlEscape(s)}</remarks>` : "");

/** One or more CoT events for a drawing (range rings give one event per ring). */
export function cotForDrawing(d: Drawing, o: CotOpts): string[] {
  const uid = `${o.uidPrefix ?? "OM.draw."}${d.id}`;
  const m = measure(d);
  switch (d.kind) {
    case "point":
    case "label":
      return [eventXml(uid, "b-m-p-s-m", d.points[0], o, contact(d.label) + remarks(d.kind === "label" ? d.label : ""))];
    case "line": {
      const links = d.points
        .map((p, i) => `<link uid="${xmlEscape(`${uid}.wp${i}`)}" type="b-m-p-w" relation="c" point="${pt(p)}"/>`)
        .join("");
      return [eventXml(uid, "b-m-r", d.points[0], o, contact(d.label) + links + style(o, false))];
    }
    case "rangebearing": {
      const links = d.points.map((p) => `<link point="${pt(p)}"/>`).join("");
      const txt = `range ${Math.round(m.distanceM ?? 0)} m, bearing ${(m.bearingDeg ?? 0).toFixed(1)}° true`;
      return [eventXml(uid, "u-d-f", d.points[0], o, contact(d.label) + links + style(o, false) + remarks(txt))];
    }
    case "circle":
      return [circleEvent(uid, d.points[0], radiusM(d), d.label, o)];
    case "rings": {
      const r = radiusM(d);
      return Array.from({ length: d.rings ?? 3 }, (_, i) =>
        circleEvent(`${uid}.r${i + 1}`, d.points[0], r * (i + 1), `${d.label} ${Math.round(r * (i + 1))} m`, o),
      );
    }
    case "rectangle": {
      const ring = areaRings(d)[0];
      const links = ring.slice(0, 4).map((p) => `<link point="${pt(p)}"/>`).join("");
      return [eventXml(uid, "u-d-r", anchor(d), o, contact(d.label) + links + style(o, true))];
    }
    case "polygon":
    case "sector":
    case "buffer": {
      const ring = areaRings(d)[0];
      if (!ring) return [];
      const links = ring.map((p) => `<link point="${pt(p)}"/>`).join("");
      return [eventXml(uid, "u-d-f", anchor(d), o, contact(d.label) + links + style(o, true))];
    }
  }
}

function circleEvent(uid: string, c: LonLat, r: number, label: string, o: CotOpts): string {
  const rr = r.toFixed(1);
  return eventXml(uid, "u-d-c-c", c, o, contact(label) + `<shape><ellipse major="${rr}" minor="${rr}" angle="360"/></shape>` + style(o, true));
}

// ---- reading CoT (GeoChat, contacts) -----------------------------------------------------------

export interface CotEvent {
  uid: string;
  type: string;
  time?: string;
  stale?: string;
  lat?: number;
  lon?: number;
  detail: string;
}

function attrs(tag: string): Record<string, string> {
  const out: Record<string, string> = {};
  const re = /([A-Za-z_:][\w:.-]*)\s*=\s*("([^"]*)"|'([^']*)')/g;
  let m: RegExpExecArray | null;
  while ((m = re.exec(tag))) out[m[1]] = xmlUnescape(m[3] ?? m[4] ?? "");
  return out;
}

export function xmlUnescape(s: string): string {
  return s.replace(/&(lt|gt|amp|quot|apos|#\d+|#x[0-9a-fA-F]+);/g, (_, e: string) => {
    if (e === "lt") return "<";
    if (e === "gt") return ">";
    if (e === "amp") return "&";
    if (e === "quot") return '"';
    if (e === "apos") return "'";
    const code = e[1] === "x" ? parseInt(e.slice(2), 16) : parseInt(e.slice(1), 10);
    return Number.isFinite(code) ? String.fromCodePoint(code) : "";
  });
}

/** First element `name` inside `xml`: its attributes and inner text (null when absent). */
export function element(xml: string, name: string): { attrs: Record<string, string>; text: string } | null {
  const esc = name.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
  const m = new RegExp(`<${esc}(\\s[^>]*?)?(/>|>([\\s\\S]*?)</${esc}\\s*>)`).exec(xml);
  if (!m) return null;
  return { attrs: attrs(m[1] ?? ""), text: m[3] !== undefined ? xmlUnescape(m[3]) : "" };
}

/** Parse one CoT XML event (TAK Protocol XML, as tak.cot.out and tak.cot.mesh.rx carry). */
export function parseCot(xml: string): CotEvent | null {
  const ev = /<event(\s[^>]*)>/.exec(xml);
  if (!ev) return null;
  const a = attrs(ev[1]);
  if (!a.uid || !a.type) return null;
  const p = element(xml, "point");
  const d = /<detail(?:\s[^>]*)?>([\s\S]*)<\/detail>/.exec(xml);
  const lat = p ? Number(p.attrs.lat) : NaN;
  const lon = p ? Number(p.attrs.lon) : NaN;
  return {
    uid: a.uid,
    type: a.type,
    time: a.time,
    stale: a.stale,
    lat: Number.isFinite(lat) ? lat : undefined,
    lon: Number.isFinite(lon) ? lon : undefined,
    detail: d ? d[1] : "",
  };
}
