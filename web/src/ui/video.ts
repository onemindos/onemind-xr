// Video panel player. WHEP (WebRTC, the <= 300 ms class) first, LL-HLS second
// (docs/research/video-and-plugins.md "Latency classes"; REFERENCE-ARCHITECTURE.md §8 L0 row).
// Video comes from MediaMTX directly; it never travels on NATS.
import Hls from "hls.js";
import type { PlaybackUrls } from "../bus/videourl";

export type Transport = "whep" | "hls" | "native-hls" | "none";

export class VideoPlayer {
  private pc?: RTCPeerConnection;
  private hls?: Hls;
  private sessionUrl?: string;
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
    if (mode !== "hls") {
      try {
        await this.playWhep(urls.whep);
        await this.waitForFrames(5000);
        return this.set("whep", `WebRTC (WHEP) ${urls.path}`);
      } catch (err) {
        this.stopWhep();
        if (mode === "whep") return this.set("none", `WHEP failed: ${(err as Error).message}`);
      }
    }
    try {
      const t = await this.playHls(urls.hls);
      await this.waitForFrames(15000);
      return this.set(t, `LL-HLS ${urls.path}`);
    } catch (err) {
      this.stop();
      return this.set("none", `No video: ${(err as Error).message}`);
    }
  }

  stop(): void {
    this.stopWhep();
    this.hls?.destroy();
    this.hls = undefined;
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

  private stopWhep(): void {
    if (this.sessionUrl) {
      // WHEP session teardown (RFC 9725 §4.3, HTTP DELETE on the session URL); best effort.
      fetch(this.sessionUrl, { method: "DELETE" }).catch(() => undefined);
      this.sessionUrl = undefined;
    }
    this.pc?.close();
    this.pc = undefined;
  }

  private async playWhep(url: string): Promise<void> {
    const pc = new RTCPeerConnection();
    this.pc = pc;
    pc.addTransceiver("video", { direction: "recvonly" });
    pc.addTransceiver("audio", { direction: "recvonly" });
    const stream = new MediaStream();
    pc.ontrack = (ev) => {
      stream.addTrack(ev.track);
      if (this.el.srcObject !== stream) this.el.srcObject = stream;
      void this.el.play().catch(() => undefined);
    };
    await pc.setLocalDescription(await pc.createOffer());
    await new Promise<void>((resolve) => {
      if (pc.iceGatheringState === "complete") return resolve();
      const t = setTimeout(resolve, 1500);
      pc.addEventListener("icegatheringstatechange", () => {
        if (pc.iceGatheringState === "complete") {
          clearTimeout(t);
          resolve();
        }
      });
    });
    const res = await fetch(url, {
      method: "POST",
      headers: { "Content-Type": "application/sdp" },
      body: pc.localDescription!.sdp,
    });
    if (res.status !== 201 && res.status !== 200) throw new Error(`WHEP ${res.status}`);
    const loc = res.headers.get("Location");
    if (loc) this.sessionUrl = new URL(loc, url).toString();
    await pc.setRemoteDescription({ type: "answer", sdp: await res.text() });
  }

  private async playHls(url: string): Promise<Transport> {
    if (Hls.isSupported()) {
      const hls = new Hls({ lowLatencyMode: true, liveSyncDurationCount: 2 });
      this.hls = hls;
      await new Promise<void>((resolve, reject) => {
        hls.on(Hls.Events.MANIFEST_PARSED, () => resolve());
        hls.on(Hls.Events.ERROR, (_e, d) => {
          if (d.fatal) reject(new Error(`HLS ${d.details}`));
        });
        hls.loadSource(url);
        hls.attachMedia(this.el);
      });
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

  private waitForFrames(ms: number): Promise<void> {
    return new Promise((resolve, reject) => {
      const start = performance.now();
      const check = () => {
        if (this.el.readyState >= 2 && this.el.videoWidth > 0) return resolve();
        if (performance.now() - start > ms) return reject(new Error("no frames"));
        setTimeout(check, 100);
      };
      check();
    });
  }
}
