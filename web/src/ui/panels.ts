// Panel renderers. Each returns a fresh DOM subtree; the app swaps it in when data changes.
import type { AlertItem } from "../bus/alerts";
import type { LayerDef } from "../bus/catalog";
import type { FeedStatus } from "../bus/layerclient";
import type { EntityRecord } from "../bus/store";
import type { Entity } from "../bus/types";
import { ago, h, HOW } from "./dom";
import { colorFor } from "./globe";

const fmt = (n: number | undefined, d = 1, unit = "") =>
  typeof n === "number" && n >= 0 ? `${n.toFixed(d)}${unit}` : "unknown";

function row(k: string, v: string | Node, testid?: string): HTMLElement {
  return h("div", { class: "kv" }, h("dt", {}, k), h("dd", testid ? { "data-testid": testid } : {}, v));
}

export function renderAlerts(items: AlertItem[], onPick: (id: string) => void, onDismiss?: (key: string) => void): HTMLElement {
  if (!items.length) return h("p", { class: "empty", "data-testid": "alerts-empty" }, "No open alerts.");
  const ul = h("ul", { class: "alerts", "data-testid": "alerts-list" });
  for (const a of items) {
    const b = h(
      "button",
      { class: "link", type: "button", disabled: !a.about, "aria-label": `Show ${a.about ?? a.title}` },
      a.title,
    );
    const about = a.about;
    if (about) b.addEventListener("click", () => onPick(about));
    // Dismiss hides an alert for this viewer only (it does not acknowledge it on the bus).
    const dismiss = onDismiss && (a.source === "stale" || a.source === "feed" || a.source === "geofence")
      ? h("button", { class: "link dismiss", type: "button", "aria-label": `Dismiss ${a.title}`, "data-testid": "alert-dismiss" }, "Dismiss")
      : null;
    if (dismiss) dismiss.addEventListener("click", () => onDismiss!(a.key));
    ul.append(
      h(
        "li",
        { class: `alert lvl-${a.level}`, "data-testid": "alert-item", "data-key": a.key, "data-source": a.source },
        h("span", { class: "lvl" }, a.level.toUpperCase()),
        b,
        h("span", { class: "muted" }, `${a.kind} · ${a.source} · ${ago(a.t)}`, dismiss ? " · " : "", dismiss),
      ),
    );
  }
  return ul;
}

export function renderEntityList(
  recs: { rec: EntityRecord; stale: boolean; mission: boolean }[],
  selected: string | null,
  onPick: (id: string) => void,
): HTMLElement {
  if (!recs.length) return h("p", { class: "empty" }, "No entities in view.");
  const ul = h("ul", { class: "entities", "data-testid": "entity-list" });
  for (const { rec, stale, mission } of recs) {
    const b = h(
      "button",
      {
        type: "button",
        class: `ent${rec.r.entity_id === selected ? " sel" : ""}${stale ? " stale" : ""}`,
        "data-testid": `entity-${rec.r.entity_id}`,
        "aria-pressed": rec.r.entity_id === selected ? "true" : "false",
      },
      h("span", { class: "dot", style: `background:${colorFor(rec)}` }),
      h("span", { class: "name" }, rec.r.callsign ?? rec.r.entity_id),
      h("span", { class: "muted" }, `${rec.r.entity_type}${mission ? " · mission" : ""}${stale ? " · stale" : ""}`),
    );
    b.addEventListener("click", () => onPick(rec.r.entity_id));
    ul.append(h("li", {}, b));
  }
  return ul;
}

export function renderDetail(rec: EntityRecord | undefined, stale: boolean, mission: boolean): HTMLElement {
  if (!rec) return h("p", { class: "empty", "data-testid": "detail-empty" }, "Select an entity on the globe or in the list.");
  const r = rec.r;
  const f: Entity | undefined = rec.full;
  const box = h("div", { class: "detail", "data-testid": "detail", "data-entity": r.entity_id });
  box.append(
    h(
      "header",
      {},
      h("span", { class: "dot big", style: `background:${colorFor(rec)}` }),
      h("div", {}, h("h2", { "data-testid": "detail-callsign" }, r.callsign ?? r.entity_id), h("code", {}, `ent.${r.entity_type}.${r.entity_id}.state`)),
    ),
    h(
      "div",
      { class: "badges" },
      r.simulated ? h("span", { class: "badge sim", "data-testid": "badge-simulated" }, "SIMULATED") : null,
      stale ? h("span", { class: "badge stale" }, "STALE") : null,
      mission ? h("span", { class: "badge mission" }, "MISSION TIER") : null,
      rec.fullStale
        ? h(
            "span",
            { class: "badge stale", "data-testid": "badge-full-stale", title: "Position is current; the other details are from an older record and are being refetched." },
            `DETAILS FROM SEQ ${f?.seq ?? "?"}`,
          )
        : null,
      f?.marking?.tlp ? h("span", { class: "badge tlp" }, `TLP:${f.marking.tlp}`) : null,
      f?.lifecycle ? h("span", { class: "badge" }, f.lifecycle) : null,
    ),
  );
  const dl = h("dl", {});
  dl.append(
    row("Type", `${r.entity_type}${f?.kind ? ` / ${f.kind}` : ""}${r.cot_type ? ` (${r.cot_type})` : ""}`),
    row("Disposition", r.disposition ?? "unknown"),
    row("Position", `${r.lat.toFixed(5)}, ${r.lon.toFixed(5)}`, "detail-position"),
    row("Altitude (HAE)", fmt(r.hae, 1, " m")),
    row("Course / speed", `${fmt(r.course, 0, "°")} / ${fmt(r.speed, 1, " m/s")}`),
    row("Observed", `${ago(r.t)} (seq ${r.seq})`, "detail-seq"),
    row("Expires", typeof r.expiry_t === "number" ? ago(r.expiry_t) : "pinned"),
  );
  if (f?.health) {
    dl.append(
      row(
        "Health",
        `${f.health.overall ?? "?"}, link ${f.health.connection ?? "?"}${
          typeof f.health.battery_pct === "number" ? `, battery ${f.health.battery_pct}%` : ""
        }`,
      ),
    );
  }
  if (f?.status?.activity || f?.status?.mode) dl.append(row("Status", [f.status.activity, f.status.mode].filter(Boolean).join(" · ")));
  if (f?.current_task) dl.append(row("Task", `${f.current_task.task_id} (${f.current_task.state})`));
  if (typeof f?.confidence === "number") {
    const p = f.confidence_parts ?? {};
    dl.append(
      row(
        "Confidence",
        `${f.confidence.toFixed(2)} (prior ${fmt(p.source_prior, 2)}, quality ${fmt(p.track_quality, 2)}, corroboration ${fmt(p.corroboration, 2)})`,
      ),
    );
  }
  box.append(dl);

  // Provenance (entity.md §4.1 `provenance` and `sources[]`): where every component came from.
  const prov = h("section", { class: "prov", "data-testid": "provenance" }, h("h3", {}, "Provenance"));
  if (f?.provenance) {
    const how = f.provenance.how ?? "";
    prov.append(
      h(
        "dl",
        {},
        row("Primary", f.provenance.primary ?? "unknown", "prov-primary"),
        row("How", how ? `${how}${HOW[how] ? ` (${HOW[how]})` : ""}` : "unknown", "prov-how"),
        row("Source time", typeof f.provenance.source_t === "number" ? new Date(f.provenance.source_t).toISOString() : "unknown"),
      ),
    );
  } else {
    prov.append(h("p", { class: "muted" }, "Render-only record; the full record comes from svc.entity.get."));
  }
  if (f?.sources?.length) {
    const tbl = h("table", { class: "sources", "data-testid": "sources" });
    tbl.append(h("tr", {}, h("th", {}, "Integration"), h("th", {}, "Subject / stream"), h("th", {}, "Last seen"), h("th", {}, "Components")));
    for (const s of f.sources) {
      tbl.append(
        h(
          "tr",
          {},
          h("td", {}, s.integration ?? "?"),
          h("td", {}, h("code", {}, s.subject ?? "?"), ` ${s.stream ?? ""}${typeof s.seq === "number" ? ` #${s.seq}` : ""}`),
          h("td", {}, typeof s.last_seen_t === "number" ? ago(s.last_seen_t) : "?"),
          h("td", {}, (s.components ?? []).join(", ")),
        ),
      );
    }
    prov.append(tbl);
  }
  if (f?.conflicts?.length) {
    prov.append(
      h("h4", {}, "Conflicts resolved"),
      h("ul", {}, ...f.conflicts.map((c) => h("li", {}, `${c.attribute}: kept ${JSON.stringify(c.kept)}, rejected ${JSON.stringify(c.rejected)}`))),
    );
  }
  box.append(prov);
  return box;
}

export function renderLayers(
  defs: LayerDef[],
  enabled: Set<string>,
  status: Map<string, FeedStatus>,
  counts: Map<string, number>,
  onToggle: (id: string, on: boolean) => void,
): HTMLElement {
  const ul = h("ul", { class: "layers", "data-testid": "layers" });
  for (const d of defs) {
    const st = status.get(d.id);
    const cb = h("input", { type: "checkbox", id: `layer-${d.id}`, checked: enabled.has(d.id) }) as HTMLInputElement;
    cb.addEventListener("change", () => onToggle(d.id, cb.checked));
    const fresh = st?.manifest?.freshness?.status;
    ul.append(
      h(
        "li",
        { "data-testid": `layer-${d.id}`, "data-mode": st?.mode ?? "off" },
        cb,
        h("label", { for: `layer-${d.id}` }, d.title, h("span", { class: "muted" }, ` ${counts.get(d.id) ?? 0}`)),
        h(
          "span",
          { class: `muted mode mode-${st?.mode ?? "off"}` },
          enabled.has(d.id) ? `${st?.mode ?? "connecting"}${fresh && fresh !== "fresh" ? ` · ${fresh}` : ""}` : "off",
        ),
        st?.detail && enabled.has(d.id) ? h("small", { class: "muted detail-line" }, st.detail) : null,
      ),
    );
  }
  return ul;
}
