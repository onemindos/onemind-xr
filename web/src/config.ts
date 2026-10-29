// Page configuration from URL parameters. No credential is ever read from the URL or stored in
// Git (onemind-xr CLAUDE.md rule 4): user and password are typed into the Connection tab and kept
// only in sessionStorage for the tab's lifetime.
import { DEFAULT_LAYERS } from "./bus/catalog";

export interface Config {
  natsUrl: string;
  viewerGroups: string[];
  missionOnly: boolean;
  imagery: "natural" | "osm";
  terrain: "none" | "terrarium";
  layers: string[];
  whepBase?: string;
  hlsBase?: string;
  videoMode: "auto" | "whep" | "hls";
  select?: string;
  view?: { lon: number; lat: number; height: number };
}

export function readConfig(loc: Location = window.location): Config {
  const q = new URLSearchParams(loc.search);
  const host = loc.hostname || "127.0.0.1";
  const scheme = loc.protocol === "https:" ? "wss" : "ws";
  const list = (k: string, d: string[]) =>
    q.get(k) !== null ? q.get(k)!.split(",").map((s) => s.trim()).filter(Boolean) : d;
  const lon = Number(q.get("lon"));
  const lat = Number(q.get("lat"));
  const height = Number(q.get("height") ?? 4000);
  const vm = q.get("video");
  return {
    natsUrl: q.get("nats") ?? `${scheme}://${host}:9222`,
    // Default viewer group: the owner's site (decision 0004; entity.md §4.2 example marking).
    viewerGroups: list("groups", ["homestead"]),
    missionOnly: q.get("mission") === "1",
    imagery: q.get("imagery") === "osm" ? "osm" : "natural",
    terrain: q.get("terrain") === "terrarium" ? "terrarium" : "none",
    layers: list("layers", DEFAULT_LAYERS.filter((l) => l.defaultOn).map((l) => l.id)),
    whepBase: q.get("whep") ?? undefined,
    hlsBase: q.get("hls") ?? undefined,
    videoMode: vm === "whep" || vm === "hls" ? vm : "auto",
    select: q.get("select") ?? undefined,
    view:
      Number.isFinite(lon) && Number.isFinite(lat) && q.get("lon") && q.get("lat")
        ? { lon, lat, height: Number.isFinite(height) ? height : 4000 }
        : undefined,
  };
}

export interface Credentials {
  user?: string;
  pass?: string;
}

export function loadCredentials(): Credentials {
  try {
    const raw = sessionStorage.getItem("l0.creds");
    return raw ? (JSON.parse(raw) as Credentials) : {};
  } catch {
    return {};
  }
}

export function saveCredentials(c: Credentials): void {
  try {
    sessionStorage.setItem("l0.creds", JSON.stringify(c));
  } catch {
    /* private mode: credentials last for this page only */
  }
}
