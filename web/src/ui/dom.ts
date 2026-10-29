// Tiny DOM helper. Everything from the bus goes through textContent, never innerHTML, so a
// hostile callsign or remark cannot inject markup.
export function h<K extends keyof HTMLElementTagNameMap>(
  tag: K,
  attrs: Record<string, string | boolean | undefined> = {},
  ...children: (Node | string | null | undefined | false)[]
): HTMLElementTagNameMap[K] {
  const el = document.createElement(tag);
  for (const [k, v] of Object.entries(attrs)) {
    if (v === undefined || v === false) continue;
    if (k === "class") el.className = String(v);
    else el.setAttribute(k, v === true ? "" : v);
  }
  for (const c of children) {
    if (c === null || c === undefined || c === false) continue;
    el.append(typeof c === "string" ? document.createTextNode(c) : c);
  }
  return el;
}

export function ago(ms: number, now = Date.now()): string {
  const s = Math.round((now - ms) / 1000);
  if (!Number.isFinite(s)) return "unknown";
  if (Math.abs(s) < 60) return s >= 0 ? `${s}s ago` : `in ${-s}s`;
  const m = Math.round(s / 60);
  if (Math.abs(m) < 60) return m >= 0 ? `${m}m ago` : `in ${-m}m`;
  const hr = Math.round(m / 60);
  return hr >= 0 ? `${hr}h ago` : `in ${-hr}h`;
}

/** Glosses that entity.md §4.1 gives for `provenance.how`; other codes are shown raw. */
export const HOW: Record<string, string> = {
  "m-f": "fused",
  "m-s": "simulated",
};
