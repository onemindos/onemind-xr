// Disposable local services for tests: a nats-server with a WebSocket listener, and a MediaMTX
// with an ffmpeg test pattern on path drone/sim-x1. Every listener binds 127.0.0.1 on a free
// port, every store is a temp dir, and stop() removes it. Nothing here touches the shared
// session services on 4222/9222/8554.
import { spawn } from "node:child_process";
import { existsSync, mkdirSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { createServer } from "node:net";
import { join } from "node:path";

const WORK = process.env.ONEMIND_WORK ?? "/Users/danielcarku/Desktop/work";
const TMP_ROOT = process.env.L0_TMP ?? join(WORK, ".run", "tmp-j1");

function bin(name, envVar) {
  const fromEnv = process.env[envVar];
  if (fromEnv && existsSync(fromEnv)) return fromEnv;
  for (const p of [join(WORK, ".tools/bin", name), `/opt/homebrew/bin/${name}`, `/usr/local/bin/${name}`]) {
    if (existsSync(p)) return p;
  }
  return name; // rely on PATH
}

export function freePort() {
  return new Promise((resolve, reject) => {
    const s = createServer();
    s.once("error", reject);
    s.listen(0, "127.0.0.1", () => {
      const { port } = s.address();
      s.close(() => resolve(port));
    });
  });
}

function tmpDir(prefix) {
  mkdirSync(TMP_ROOT, { recursive: true });
  return mkdtempSync(join(TMP_ROOT, `${prefix}-`));
}

async function waitTcp(port, ms = 10000) {
  const { connect } = await import("node:net");
  const start = Date.now();
  while (Date.now() - start < ms) {
    const ok = await new Promise((r) => {
      const c = connect(port, "127.0.0.1", () => {
        c.destroy();
        r(true);
      });
      c.on("error", () => r(false));
    });
    if (ok) return;
    await new Promise((r) => setTimeout(r, 100));
  }
  throw new Error(`port ${port} did not open`);
}

function kill(proc) {
  return new Promise((resolve) => {
    if (!proc || proc.exitCode !== null) return resolve();
    proc.once("exit", () => resolve());
    proc.kill("SIGTERM");
    setTimeout(() => proc.exitCode === null && proc.kill("SIGKILL"), 3000);
  });
}

/** Start a disposable nats-server with JetStream and a no-TLS WebSocket listener. */
export async function startNats() {
  const dir = tmpDir("nats");
  const port = await freePort();
  const wsPort = await freePort();
  const conf = join(dir, "nats.conf");
  writeFileSync(
    conf,
    [
      `listen: "127.0.0.1:${port}"`,
      `jetstream { store_dir: "${join(dir, "js")}", max_memory_store: 64MB, max_file_store: 256MB }`,
      `websocket { listen: "127.0.0.1:${wsPort}", no_tls: true }`,
      "",
    ].join("\n"),
  );
  const proc = spawn(bin("nats-server", "NATS_SERVER_BIN"), ["-c", conf], { stdio: ["ignore", "ignore", "pipe"] });
  let log = "";
  proc.stderr.on("data", (d) => (log += d));
  await waitTcp(wsPort);
  return {
    url: `nats://127.0.0.1:${port}`,
    wsUrl: `ws://127.0.0.1:${wsPort}`,
    log: () => log,
    async stop() {
      await kill(proc);
      rmSync(dir, { recursive: true, force: true });
    },
  };
}

/**
 * Start a disposable MediaMTX and publish an ffmpeg test pattern (H.264, 640x360, 15 fps) on
 * `drone/sim-x1` over RTSP/TCP. Returns the RTSP URL a fixture puts in the entity's media[].
 */
export async function startMediaMTX({ path = "drone/sim-x1" } = {}) {
  const dir = tmpDir("mtx");
  const [rtsp, hls, webrtc, ice] = [await freePort(), await freePort(), await freePort(), await freePort()];
  const conf = join(dir, "mediamtx.yml");
  writeFileSync(
    conf,
    `logLevel: info
authMethod: internal
authInternalUsers:
  - user: any
    pass:
    ips: ["127.0.0.1", "::1"]
    permissions:
      - action: publish
      - action: read
api: false
metrics: false
pprof: false
playback: false
rtsp: true
rtspTransports: [tcp]
rtspAddress: 127.0.0.1:${rtsp}
rtmp: false
srt: false
moq: false
hls: true
hlsAddress: 127.0.0.1:${hls}
hlsAllowOrigins: ["*"]
hlsVariant: lowLatency
hlsSegmentCount: 7
hlsSegmentDuration: 1s
hlsPartDuration: 200ms
webrtc: true
webrtcAddress: 127.0.0.1:${webrtc}
webrtcAllowOrigins: ["*"]
webrtcLocalUDPAddress: 127.0.0.1:${ice}
webrtcLocalTCPAddress: ""
webrtcIPsFromInterfaces: false
webrtcAdditionalHosts: ["127.0.0.1"]
paths:
  all_others:
`,
  );
  const mtx = spawn(bin("mediamtx", "MEDIAMTX_BIN"), [conf], { cwd: dir, stdio: ["ignore", "pipe", "pipe"] });
  let log = "";
  mtx.stdout.on("data", (d) => (log += d));
  mtx.stderr.on("data", (d) => (log += d));
  await waitTcp(rtsp);
  await waitTcp(webrtc);
  const rtspUrl = `rtsp://127.0.0.1:${rtsp}/${path}`;
  const ff = spawn(
    bin("ffmpeg", "FFMPEG_BIN"),
    [
      "-hide_banner", "-loglevel", "error", "-re",
      "-f", "lavfi", "-i", "testsrc2=size=640x360:rate=15",
      "-c:v", "libx264", "-preset", "ultrafast", "-tune", "zerolatency", "-g", "15", "-bf", "0",
      "-pix_fmt", "yuv420p", "-profile:v", "baseline",
      "-f", "rtsp", "-rtsp_transport", "tcp", rtspUrl,
    ],
    { stdio: ["ignore", "ignore", "pipe"] },
  );
  ff.stderr.on("data", (d) => (log += `[ffmpeg] ${d}`));
  // Wait for the publisher to register (MediaMTX logs "is publishing to path").
  const start = Date.now();
  while (!/is publishing to path/.test(log)) {
    if (Date.now() - start > 15000) throw new Error(`ffmpeg did not start publishing:\n${log}`);
    await new Promise((r) => setTimeout(r, 200));
  }
  return {
    rtspUrl,
    whepBase: `http://127.0.0.1:${webrtc}`,
    hlsBase: `http://127.0.0.1:${hls}`,
    ports: { rtsp, hls, webrtc, ice },
    log: () => log,
    async stop() {
      await kill(ff);
      await kill(mtx);
      rmSync(dir, { recursive: true, force: true });
    },
  };
}
