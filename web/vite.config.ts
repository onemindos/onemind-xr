// Vite config for the L0 client. CesiumJS needs its Workers, Assets, ThirdParty and Widgets
// served as static files; they are copied from the pinned npm package into public/cesium
// (gitignored) so dev, build and preview all serve them.
import { cpSync, existsSync, readFileSync } from "node:fs";
import { resolve } from "node:path";
import type { Plugin } from "vite";
import { defineConfig } from "vitest/config";

const CESIUM_SRC = resolve(import.meta.dirname, "node_modules/cesium/Build/Cesium");
const CESIUM_DST = resolve(import.meta.dirname, "public/cesium");

function cesiumStatic(): Plugin {
  return {
    name: "onemind-cesium-static",
    configResolved() {
      const ver = JSON.parse(readFileSync(resolve(import.meta.dirname, "node_modules/cesium/package.json"), "utf8")).version;
      const stamp = resolve(CESIUM_DST, `.version-${ver}`);
      if (existsSync(stamp)) return;
      for (const d of ["Workers", "ThirdParty", "Assets", "Widgets"]) {
        cpSync(resolve(CESIUM_SRC, d), resolve(CESIUM_DST, d), { recursive: true });
      }
      cpSync(resolve(import.meta.dirname, "package.json"), stamp);
    },
  };
}

export default defineConfig({
  base: "./",
  plugins: [cesiumStatic()],
  server: { host: true, port: 5173 },
  preview: { host: true, port: 4173 },
  build: { chunkSizeWarningLimit: 6000, sourcemap: false },
  test: {
    include: ["test/**/*.test.ts"],
    environment: "node",
    testTimeout: 20000,
  },
});
