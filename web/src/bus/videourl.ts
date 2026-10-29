// Derive browser playback URLs from an entity's `media[]` (entity.md §4.1: video URLs point at
// MediaMTX, never at NATS). MediaMTX serves the same path over WHEP at
// http://host:8889/{path}/whep and LL-HLS at http://host:8888/{path}/index.m3u8
// (docs/research/video-and-plugins.md W1: MediaMTX docs `5-read/03-webrtc.md`, `5-read/06-hls.md`).
import type { MediaRef } from "./types";

export interface PlaybackUrls {
  path: string;
  whep: string;
  hls: string;
  source: string;
}

export interface VideoPorts {
  whep: number;
  hls: number;
}

const LOOPBACK = new Set(["127.0.0.1", "localhost", "::1", "[::1]"]);

/**
 * `pageHost` replaces a loopback host so a phone on the LAN reaches the MediaMTX on the machine
 * that served the page. `override` bases (the `?whep=` and `?hls=` page parameters) win.
 */
export function playbackUrls(
  media: MediaRef[] | undefined,
  pageHost: string,
  ports: VideoPorts = { whep: 8889, hls: 8888 },
  override: { whep?: string; hls?: string } = {},
): PlaybackUrls | null {
  const v = (media ?? []).filter((m) => m.kind === "video" && typeof m.url === "string");
  const ref = v.find((m) => m.role === "primary") ?? v[0];
  if (!ref?.url) return null;
  let u: URL;
  try {
    u = new URL(ref.url);
  } catch {
    return null;
  }
  let path = decodeURIComponent(u.pathname).replace(/^\/+/, "").replace(/\/+$/, "");
  path = path.replace(/\/(whep|index\.m3u8)$/, "");
  if (!path || path.includes("..")) return null;
  const host = LOOPBACK.has(u.hostname) && pageHost ? pageHost : u.hostname;
  const h = host.includes(":") && !host.startsWith("[") ? `[${host}]` : host;
  const wb = override.whep?.replace(/\/+$/, "") || `http://${h}:${ports.whep}`;
  const hb = override.hls?.replace(/\/+$/, "") || `http://${h}:${ports.hls}`;
  return {
    path,
    whep: `${wb}/${path}/whep`,
    hls: `${hb}/${path}/index.m3u8`,
    source: ref.url,
  };
}
