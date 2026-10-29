---
id: "notion-3c4d0cf70a7e814b9d20fc6e2daac365"
title: "OneMind OS — Recon 02: Map / Globe Frontend Overlap"
aliases: ["OneMind OS — Recon 02: Map / Globe Frontend Overlap"]
type: "reference"
status: "active"
created: "2026-10-16"
updated: "2026-10-16"
owner: "zeus"
tags: ["type/reference", "domain/so", "collection/so", "kind/knowledge"]
collection: "so"
kind: "knowledge"
notion_database: "Sovereign Operations "
notion_status: "Not started"
notion_area_2: "🌐 Fabric"
notion_category: "Geo"
notion_id: "3c4d0cf70a7e814b9d20fc6e2daac365"
notion_url: "https://app.notion.com/p/OneMind-OS-Recon-02-Map-Globe-Frontend-Overlap-3c4d0cf70a7e814b9d20fc6e2daac365"
notion_created: "2026-08-22"
notion_last_edited: "2026-09-14T03:37:00.000Z"
imported: "2026-10-16"
---

# OneMind OS — Recon 02: Map / Globe Frontend Overlap

# OneMind OS — Recon 02: Map / Globe Frontend Overlap
*Generated 2026-07-06 by Legacy (direct). Read-only analysis.*
## Comparison table
| Frontend | Engine | 2D/3D | Real-time bus | WS client | Plugin arch | AI/agent control | Self-host | License |
| **WorldWideView** | CesiumJS (+ Google 3D Tiles) | 3D globe | ✅ **DataBus** (typed event bus) | ✅ WsClient.ts | ✅ PluginManager + manifests | ✅ **Agent Bus** (MCP SSE) | Docker/Next.js | Elastic 2.0 |
| WorldMonitor | globe.gl + deck.gl | 2D+3D | ❌ polling only | ❌ | ⚠️ variant configs | ⚠️ agent-bus-applier | Tauri/web | AGPL v3 |
| Osiris | MapLibre GL | 2D flat | ❌ polling | ❌ | ❌ | ❌ | Next.js | MIT |
| Crucix | (globe + TAK room) | 3D | ⚠️ partial | ⚠️ | ❌ | ⚠️ briefing API | web | unknown |
| constellation-overwatch | MapLibre web components | 2D | ✅ KV watcher | ✅ | ⚠️ templ components | ❌ | Go binary | Community Copyleft |
| om-unity COP | built-in (Rust-served) | 2D map | ✅ native (in-binary) | ✅ | ❌ | ORP-QL query | single binary | Apache 2.0 |
## NATS integration readiness
- **WorldWideView — BEST.** Its `DataBus` (src/core/data/DataBus.ts) is a typed pub/sub event bus, architecturally identical to NATS. `WsClient.ts` already exists for WebSocket transport. Wiring = NATS → WebSocket bridge → `dataBus.emit(subject, payload)` → plugin renders. The `PluginManager` already routes plugin snapshots into DataBus + global store. This is a near-drop-in fit.
- **constellation-overwatch / om-unity** — both have native real-time (KV watcher / in-binary) but their UIs are tightly coupled to their own backends. Hard to repurpose as a general OneMind map.
- **WorldMonitor / Osiris** — polling-based, no bus. Would need real-time layer bolted on. Not worth it.
## Best pick: **WorldWideView** — evidence
1. **DataBus = NATS in the browser.** Typed event bus, singleton, `on/emit/off`. NATS subject → DataBus event is 1:1.
1. **WsClient already built.** No need to write WebSocket handling.
1. **PluginManager + manifests.** Each NATS subject family becomes a plugin (ships, flights, fires…). Composable, no monolith. Built-in plugins already exist: earthquakes, iss, weather, geojson.
1. **Agent Bus (docs/agent-bus.md).** HTTP+SSE write path. MCP server `szski/wwv-mcp` exposes 15 tools (globe_fly_to, layer_toggle, engine_query…). Legacy can drive the globe. Nothing else has this.
1. **CesiumJS 3D** — real globe with photorealistic tiles, horizon culling, timeline scrubber.
## What to salvage from the ones we DON'T pick
- **WorldMonitor:** the 56-layer `map-layer-definitions.ts` catalog (layer styling/config), the CII country-stress scoring algorithm, the 500+ feed list. Extract as config/docs.
- **Osiris:** the MapLibre layer rendering approach for dense point clouds (CCTV, flights), and the OSINT recon UI panels. Extract as reference.
- **constellation-overwatch:** the entity_id primitive schema + KV state aggregation pattern (already bridged). Keep entity model as docs.
- **om-unity COP:** keep as-is — it's the ORP built-in operator view, useful as a fallback/debug COP at :9090. Don't delete.
- **Crucix:** the TAK ops-room boot sequence UI and briefing generation API → reference for Legacy briefing.
## Verdict
Deploy **WorldWideView** to DOKS as the OneMind global map. Build a **NATS → WebSocket bridge** that maps subjects to DataBus events. Each data domain = one WWV plugin. Wire **Agent Bus → Legacy** so Legacy can fly the globe and toggle layers. Keep om-unity COP as debug view. Extract layer catalog + CII from WorldMonitor before deleting it.
