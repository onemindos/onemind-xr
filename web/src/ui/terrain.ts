// Open terrain without a Cesium ion token: the AWS Open Data "Terrain Tiles" (Mapzen terrarium
// PNG encoding, https://registry.opendata.aws/terrain-tiles/), decoded in the browser into
// CesiumJS heightmaps with CustomHeightmapTerrainProvider. Terrarium decoding:
// height_m = (R * 256 + G + B / 256) - 32768
// (https://github.com/tilezen/joerd/blob/master/docs/formats.md). Free, no key, no paid API.
import { CustomHeightmapTerrainProvider, WebMercatorTilingScheme } from "cesium";

export const TERRARIUM_URL = "https://s3.amazonaws.com/elevation-tiles-prod/terrarium/{z}/{x}/{y}.png";
const SIZE = 65; // heightmap samples per side
const MAX_LEVEL = 14;

export function decodeTerrarium(rgba: Uint8ClampedArray, w: number, h: number, out = SIZE): Float32Array {
  const heights = new Float32Array(out * out);
  for (let j = 0; j < out; j++) {
    const y = Math.min(h - 1, Math.round((j / (out - 1)) * (h - 1)));
    for (let i = 0; i < out; i++) {
      const x = Math.min(w - 1, Math.round((i / (out - 1)) * (w - 1)));
      const k = (y * w + x) * 4;
      heights[j * out + i] = rgba[k] * 256 + rgba[k + 1] + rgba[k + 2] / 256 - 32768;
    }
  }
  return heights;
}

async function fetchTile(x: number, y: number, z: number): Promise<Float32Array> {
  const url = TERRARIUM_URL.replace("{z}", String(z)).replace("{x}", String(x)).replace("{y}", String(y));
  const res = await fetch(url, { mode: "cors" });
  if (!res.ok) throw new Error(`terrain ${res.status}`);
  const bmp = await createImageBitmap(await res.blob());
  const canvas = new OffscreenCanvas(bmp.width, bmp.height);
  const ctx = canvas.getContext("2d", { willReadFrequently: true })!;
  ctx.drawImage(bmp, 0, 0);
  const data = ctx.getImageData(0, 0, bmp.width, bmp.height).data;
  return decodeTerrarium(data, bmp.width, bmp.height);
}

export function terrariumTerrain(): CustomHeightmapTerrainProvider {
  return new CustomHeightmapTerrainProvider({
    width: SIZE,
    height: SIZE,
    tilingScheme: new WebMercatorTilingScheme(),
    callback: (x, y, level) => (level > MAX_LEVEL ? undefined : fetchTile(x, y, level)),
    credit: "Terrain: AWS Terrain Tiles (Mapzen terrarium; USGS, SRTM and others)",
  });
}
