#!/usr/bin/env node
// CLI for the L0 fixture publisher.
//   node tools/fixture-publisher.mjs [--nats ws://127.0.0.1:9222] [--mode core|entity|layer]
//        [--video rtsp://127.0.0.1:8554/drone/sim-x1] [--hz 2] [--remove-after 8] [--expire-after 6]
//        [--user NAME]   (the password is read from the NATS_PASSWORD environment variable only)
// Publishes only simulated (sim-*) records; never publishes cmd.*.
import { Fixture } from "./fixture.mjs";

const args = process.argv.slice(2);
const opt = (k, d) => {
  const i = args.indexOf(`--${k}`);
  return i >= 0 && args[i + 1] ? args[i + 1] : d;
};

const f = new Fixture({
  wsUrl: opt("nats", "ws://127.0.0.1:9222"),
  mode: opt("mode", "entity"),
  videoUrl: opt("video", "rtsp://127.0.0.1:8554/drone/sim-x1"),
  hz: Number(opt("hz", "2")),
  removeAfterS: Number(opt("remove-after", "8")),
  expireAfterS: Number(opt("expire-after", "6")),
  user: opt("user", undefined),
  pass: process.env.NATS_PASSWORD || undefined,
});
await f.start();
console.log(`fixture: mode=${f.mode} publishing to ${f.wsUrl} (Ctrl-C to stop)`);
const stat = setInterval(() => console.log(`fixture: ${f.published} records, ${f.latest.size} live`), 10_000);
process.on("SIGINT", async () => {
  clearInterval(stat);
  await f.stop();
  process.exit(0);
});
