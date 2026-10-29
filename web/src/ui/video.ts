// Video panel player. WHEP (WebRTC, the <= 300 ms class) first, LL-HLS second
// (docs/research/video-and-plugins.md "Latency classes"; REFERENCE-ARCHITECTURE.md §8 L0 row).
// Video comes from MediaMTX directly; it never travels on NATS.
import Hls from "hls.js";
import type { PlaybackUrls } from "../bus/videourl";

export type Transport = "whep" | "hls" | "native-hls" | "none";

/** Thrown inside a play() that a newer play() or stop() has replaced. */
class Superseded extends Error {
  constructor() {
    super("superseded");
  }
}

/** Resolve or reject with `p`, unless `signal` aborts first (then reject with Superseded). */
function unlessAborted<T>(p: Promise<T>, signal: AbortSignal): Promise<T> {
  return new Promise<T>((resolve, reject) => {
    if (signal.aborted) return reject(new Superseded());
    const onAbort = () => reject(new Superseded());
    signal.addEventListener("abort", onAbort, { once: true });
    p.then(
      (v) => {
        signal.removeEventListener("abort", onAbort);
        resolve(v);
      },
      (e) => {
        signal.removeEventListener("abort", onAbort);
        reject(e);
      },
    );
  });
}

/** One WHEP attempt's resources, owned by the play() call that created them. */
interface WhepSession {
  pc?: RTCPeerConnection;
  sessionUrl?: string;
  closed?: boolean;
}

/**
 * Every play() gets its own generation (an AbortController). A newer play() or a stop() aborts
 * it; after each await the call returns without touching the element, the transport label or
 * the player's current resources, and on failure it closes only the resources it created.
 */
export class VideoPlayer {
  private whep?: WhepSession;
  private hls?: Hls;
  private gen?: AbortController;
  transport: Transport = "none";

  constructor(
    readonly el: HTMLVideoElement,
    private onState: (s: { transport: Transport; message: string }) => void,
  ) {
    el.muted = true;
    el.playsInline = true;
    el.autoplay = true;
  }

  async play(urls: PlaybackUrls, mode: "auto" | "whep" | "hls" = "auto"): Promise<Transport> {
    this.stop();
    const gen = new AbortController();
    this.gen = gen;
    const signal = gen.signal;
    if (mode !== "hls") {
      const w: WhepSession = {};
      this.whep = w;
      try {
        await this.playWhep(urls.whep, w, signal);
        await this.waitForFrames(5000, signal);
        return this.set("whep", `WebRTC (WHEP) ${urls.path}`);
      } catch (err) {
        this.closeWhep(w);
        if (signal.aborted) return "none";
        this.el.srcObject = null;
        if (mode === "whep") return this.set("none", `WHEP failed: ${(err as Error).message}`);
      }
    }
    const holder: { hls?: Hls } = {};
    try {
      const t = await this.playHls(urls.hls, holder, signal);
      await this.waitForFrames(15000, signal);
      return this.set(t, `LL-HLS ${urls.path}`);
    } catch (err) {
      holder.hls?.destroy();
      if (this.hls === holder.hls) this.hls = undefined;
      if (signal.aborted) return "none";
      this.resetElement();
      return this.set("none", `No video: ${(err as Error).message}`);
    }
  }

  stop(): void {
    this.gen?.abort();
    this.gen = undefined;
    if (this.whep) this.closeWhep(this.whep);
    this.hls?.destroy();
    this.hls = undefined;
    this.resetElement();
  }

  private resetElement(): void {
    this.el.removeAttribute("src");
    this.el.srcObject = null;
    this.transport = "none";
  }

  private set(t: Transport, message: string): Transport {
    this.transport = t;
    this.el.dataset.transport = t;
    this.onState({ transport: t, message });
    return t;
  }

  /** Close one WHEP attempt (idempotent). Never touches another attempt's resources. */
  private closeWhep(w: WhepSession): void {
    if (w.closed) return;
    w.closed = true;
    if (w.sessionUrl) {
      // WHEP session teardown (RFC 9725 §4.3, HTTP DELETE on the session URL); best effort.
      fetch(w.sessionUrl, { method: "DELETE" }).catch(() => undefined);
    }
    w.pc?.close();
    if (this.whep === w) this.whep = undefined;
  }

  private async playWhep(url: string, w: WhepSession, signal: AbortSignal): Promise<void> {
    const pc = new RTCPeerConnection();
    w.pc = pc;
    pc.addTransceiver("video", { direction: "recvonly" });
    pc.addTransceiver("audio", { direction: "recvonly" });
    const stream = new MediaStream();
    pc.ontrack = (ev) => {
      if (signal.aborted) return;
      stream.addTrack(ev.track);
      if (this.el.srcObject !== stream) this.el.srcObject = stream;
      void this.el.play().catch(() => undefined);
    };
    await unlessAborted(pc.createOffer().then((o) => pc.setLocalDescription(o)), signal);
    await unlessAborted(
      new Promise<void>((resolve) => {
        if (pc.iceGatheringState === "complete") return resolve();
        const t = setTimeout(resolve, 1500);
        pc.addEventListener("icegatheringstatechange", () => {
          if (pc.iceGatheringState === "complete") {
            clearTimeout(t);
            resolve();
          }
        });
      }),
      signal,
    );
    const res = await unlessAborted(
      fetch(url, {
        method: "POST",
        headers: { "Content-Type": "application/sdp" },
        body: pc.localDescription!.sdp,
        signal,
      }),
      signal,
    );
    const loc = res.headers.get("Location");
    // Record the session first, so a superseded or failed attempt still DELETEs it.
    if (loc) w.sessionUrl = new URL(loc, url).toString();
    if (res.status !== 201 && res.status !== 200) throw new Error(`WHEP ${res.status}`);
    const sdp = await unlessAborted(res.text(), signal);
    await unlessAborted(pc.setRemoteDescription({ type: "answer", sdp }), signal);
  }

  private async playHls(url: string, holder: { hls?: Hls }, signal: AbortSignal): Promise<Transport> {
    if (signal.aborted) throw new Superseded();
    if (Hls.isSupported()) {
      const hls = new Hls({ lowLatencyMode: true, liveSyncDurationCount: 2 });
      holder.hls = hls;
      this.hls = hls;
      await unlessAborted(
        new Promise<void>((resolve, reject) => {
          hls.on(Hls.Events.MANIFEST_PARSED, () => resolve());
          hls.on(Hls.Events.ERROR, (_e, d) => {
            if (d.fatal) reject(new Error(`HLS ${d.details}`));
          });
          hls.loadSource(url);
          hls.attachMedia(this.el);
        }),
        signal,
      );
      void this.el.play().catch(() => undefined);
      return "hls";
    }
    if (this.el.canPlayType("application/vnd.apple.mpegurl")) {
      // iOS Safari plays HLS natively.
      this.el.src = url;
      void this.el.play().catch(() => undefined);
      return "native-hls";
    }
    throw new Error("this browser plays neither WebRTC nor HLS");
  }

  private waitForFrames(ms: number, signal: AbortSignal): Promise<void> {
    return new Promise((resolve, reject) => {
      const start = performance.now();
      const check = () => {
        if (signal.aborted) return reject(new Superseded());
        if (this.el.readyState >= 2 && this.el.videoWidth > 0) return resolve();
        if (performance.now() - start > ms) return reject(new Error("no frames"));
        setTimeout(check, 100);
      };
      check();
    });
  }
}
