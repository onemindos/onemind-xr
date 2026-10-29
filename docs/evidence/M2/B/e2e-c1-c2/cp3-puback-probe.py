"""Probe: a NATS request on a subject that a JetStream stream binds gets the stream's PubAck
as a reply. With COT binding tak.cot.> (omos cot.yaml), a request on tak.cot.mesh.tx is
answered by JetStream, racing the mesh gateway's "ok" (cot-projection.md §11 CP-3)."""
import asyncio, os, socket, subprocess, sys, time, shutil
import nats
RUN = "/Users/danielcarku/Desktop/work/.run/tmp-b-client/puback"
s = socket.socket(); s.bind(("127.0.0.1", 0)); port = s.getsockname()[1]; s.close()
p = subprocess.Popen(["/Users/danielcarku/Desktop/work/.tools/bin/nats-server", "-a", "127.0.0.1", "-p", str(port), "-js", "-sd", RUN],
                     stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
time.sleep(1)
async def main():
    nc = await nats.connect(f"nats://127.0.0.1:{port}")
    await nc.jetstream().add_stream(name="COT", subjects=["tak.cot.>"])
    print("$ stream COT subjects tak.cot.>  (no gateway subscribed)")
    r = await nc.request("tak.cot.mesh.tx", b"<event/>", timeout=2)
    print("$ request tak.cot.mesh.tx -> reply:", r.data.decode())
    got = []
    async def gw(m):
        await m.respond(b"ok")
    await nc.subscribe("tak.cot.mesh.tx", cb=gw)
    await nc.flush()
    for i in range(5):
        r = await nc.request("tak.cot.mesh.tx", b"<event/>", timeout=2)
        got.append(r.data.decode())
    print("$ with a gateway-like responder also subscribed, first replies of 5 requests:", got)
    await nc.close()
try:
    asyncio.run(main())
finally:
    p.terminate(); p.wait(); shutil.rmtree(RUN, ignore_errors=True)
