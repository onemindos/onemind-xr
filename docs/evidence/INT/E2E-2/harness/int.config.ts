// Same browser flags as playwright.config.ts (run/m4-web), testDir e2e-int.
import { defineConfig } from "@playwright/test";
import base from "./playwright.config";
const PORT = 4191;
export default defineConfig({ ...base, testDir: "e2e-int", outputDir: "/Users/danielcarku/Desktop/work/.run/tmp-e2e/pw-results",
  use: { ...base.use, baseURL: `http://127.0.0.1:${PORT}/` },
  webServer: { ...(base.webServer as object), command: `npx vite build --outDir /Users/danielcarku/Desktop/work/.run/tmp-e2e/web-dist && npx vite preview --outDir /Users/danielcarku/Desktop/work/.run/tmp-e2e/web-dist --host 127.0.0.1 --port ${PORT} --strictPort`, url: `http://127.0.0.1:${PORT}/` } });
