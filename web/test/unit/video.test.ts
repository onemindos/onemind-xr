// VideoPlayer generation handling (review finding: overlapping play() calls). Runs in node with
// stand-ins for RTCPeerConnection, MediaStream, fetch, hls.js and the <video> element.
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";

const hlsInstances: FakeHls[] = [];
class FakeHls {
  static isSupported = () => true;
  static Events = { MANIFEST_PARSED: "manifestParsed", ERROR: "error" };
  handlers = new Map<string, (...a: unknown[]) => void>();
  url = "";
  el?: FakeVideo;
  destroyed = false;
  constructor() {
    hlsInstances.push(this);
  }
  on(ev: string, cb: (...a: unknown[]) => void) {
    this.handlers.set(ev, cb);
  }
  loadSource(url: string) {
    this.url = url;
    setTimeout(() => !this.destroyed && this.handlers.get("manifestParsed")?.(), 5);
  }
  attachMedia(el: FakeVideo) {
    this.el = el;
    el.attached = this;
    setTimeout(() => {
      if (this.destroyed) return;
      el.readyState = 4;
      el.videoWidth = 640;
    }, 10);
  }
  destroy() {
    this.destroyed = true;
    if (this.el?.attached === this) {
      this.el.attached = undefined;
      this.el.readyState = 0;
      this.el.videoWidth = 0;
    }
  }
}
vi.mock("hls.js", () => ({ default: FakeHls }));

class FakeVideo {
  muted = false;
  playsInline = false;
  autoplay = false;
  readyState = 0;
  videoWidth = 0;
  srcObject: unknown = null;
  src = "";
  dataset: Record<string, string> = {};
  attached?: FakeHls;
  play() {
    return Promise.resolve();
  }
  removeAttribute() {
    this.src = "";
  }
  canPlayType() {
    return "";
  }
}

const pcs: FakePc[] = [];
class FakePc {
  closed = false;
  iceGatheringState = "complete";
  localDescription?: { sdp: string };
  ontrack: unknown = null;
  constructor() {
    pcs.push(this);
  }
  addTransceiver() {}
  addEventListener() {}
  async createOffer() {
    return { type: "offer", sdp: "v=0" };
  }
  async setLocalDescription(d: { sdp: string }) {
    this.localDescription = d;
  }
  async setRemoteDescription() {
    if (this.closed) throw new Error("InvalidStateError: closed");
  }
  close() {
    this.closed = true;
  }
}

// WHEP POSTs wait until the test releases them; every WHEP answer is a 404, so play() falls
// back to HLS, the path where the original bug leaked Hls instances.
const pendingPosts: { url: string; release: () => void }[] = [];
const deletes: string[] = [];
function fakeFetch(url: string, init?: { method?: string; signal?: AbortSignal }): Promise<unknown> {
  if (init?.method === "DELETE") {
    deletes.push(url);
    return Promise.resolve({ status: 200 });
  }
  return new Promise((resolve) => {
    pendingPosts.push({
      url,
      release: () => resolve({ status: 404, headers: { get: () => null }, text: async () => "" }),
    });
  });
}

const urls = (p: string) => ({ path: p, source: `rtsp://mtx/${p}`, whep: `http://mtx/${p}/whep`, hls: `http://mtx/${p}/index.m3u8` });
const wait = (ms: number) => new Promise((r) => setTimeout(r, ms));

beforeEach(() => {
  hlsInstances.length = 0;
  pcs.length = 0;
  pendingPosts.length = 0;
  deletes.length = 0;
  vi.stubGlobal("RTCPeerConnection", FakePc);
  vi.stubGlobal("MediaStream", class { addTrack() {} });
  vi.stubGlobal("fetch", fakeFetch);
});
afterEach(() => vi.unstubAllGlobals());

describe("VideoPlayer: overlapping play() calls", () => {
  it("selecting A then B quickly plays only B, with a single live Hls instance", async () => {
    const { VideoPlayer } = await import("../../src/ui/video");
    const el = new FakeVideo();
    const states: { transport: string; message: string }[] = [];
    const player = new VideoPlayer(el as unknown as HTMLVideoElement, (s) => states.push(s));
    const a = player.play(urls("cam-a"));
    await wait(5);
    const b = player.play(urls("cam-b"));
    await wait(5);
    expect(pcs[0].closed).toBe(true); // A's peer connection closed by play(B)
    expect(pcs[1].closed).toBe(false);
    // A's WHEP answer arrives late, then B's fails: B falls back to HLS.
    for (const p of pendingPosts) p.release();
    expect(await a).toBe("none");
    expect(await b).toBe("hls");
    const live = hlsInstances.filter((h) => !h.destroyed);
    expect(live).toHaveLength(1);
    expect(live[0].url).toBe("http://mtx/cam-b/index.m3u8");
    expect(hlsInstances.some((h) => h.url.includes("cam-a"))).toBe(false);
    expect(el.attached).toBe(live[0]);
    expect(player.transport).toBe("hls");
    expect(states.every((s) => !s.message.includes("cam-a"))).toBe(true);
    expect(states.at(-1)).toMatchObject({ transport: "hls", message: "LL-HLS cam-b" });
    player.stop();
  });

  it("a newer play() during A's HLS startup destroys A's Hls and never reports A", async () => {
    const { VideoPlayer } = await import("../../src/ui/video");
    const el = new FakeVideo();
    const states: { transport: string; message: string }[] = [];
    const player = new VideoPlayer(el as unknown as HTMLVideoElement, (s) => states.push(s));
    const a = player.play(urls("cam-a"), "hls");
    await wait(1); // A's Hls exists, manifest not parsed yet
    const b = player.play(urls("cam-b"), "hls");
    expect(await a).toBe("none");
    expect(await b).toBe("hls");
    expect(hlsInstances).toHaveLength(2);
    expect(hlsInstances[0].destroyed).toBe(true);
    expect(hlsInstances[1].destroyed).toBe(false);
    expect(states.map((s) => s.message)).toEqual(["LL-HLS cam-b"]);
    player.stop();
  });

  it("stop() while a play is in flight (entity removed) plays nothing", async () => {
    const { VideoPlayer } = await import("../../src/ui/video");
    const el = new FakeVideo();
    const states: { transport: string; message: string }[] = [];
    const player = new VideoPlayer(el as unknown as HTMLVideoElement, (s) => states.push(s));
    const a = player.play(urls("cam-a"));
    await wait(5);
    player.stop();
    for (const p of pendingPosts) p.release();
    expect(await a).toBe("none");
    await wait(30);
    expect(hlsInstances).toHaveLength(0);
    expect(states).toHaveLength(0);
    expect(player.transport).toBe("none");
    expect(pcs[0].closed).toBe(true);
  });
});
