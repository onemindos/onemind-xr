// End-to-end tests for the L0 client: chromium only (Playwright's Chrome for Testing headless
// shell, installed under work/.tools/ms-playwright via PLAYWRIGHT_BROWSERS_PATH).
import { defineConfig } from "@playwright/test";

const PORT = Number(process.env.L0_PREVIEW_PORT ?? 4179);

export default defineConfig({
  testDir: "e2e",
  timeout: 120_000,
  expect: { timeout: 20_000 },
  workers: 1,
  reporter: [["list"]],
  outputDir: "test-results",
  use: {
    baseURL: `http://127.0.0.1:${PORT}/`,
    browserName: "chromium",
    launchOptions: {
      // WebGL through SwiftShader; autoplay for the muted video panel. The separate GPU process
      // hangs on this build machine (WebGL calls never return); an in-process GPU works
      // (docs/evidence/M3/J1/webgl-probe.txt).
      args: [
        "--in-process-gpu",
        "--use-angle=swiftshader",
        "--enable-unsafe-swiftshader",
        "--autoplay-policy=no-user-gesture-required",
      ],
    },
  },
  webServer: {
    command: `npx vite build && npx vite preview --host 127.0.0.1 --port ${PORT} --strictPort`,
    url: `http://127.0.0.1:${PORT}/`,
    reuseExistingServer: false,
    timeout: 240_000,
  },
});
