#!/usr/bin/env python3
"""Decode a raw stream of MISB ST 0601 packets (as extracted by ffmpeg -map 0:d -c copy -f data),
validate each checksum, and print a summary plus the first and last decoded packets."""
import struct, sys

UAS_KEY = bytes.fromhex("060E2B34020B01010E01030101000000")

def unmap(raw, rng, dom, signed):
    v = int.from_bytes(raw, "big", signed=signed)
    (r0, r1), (d0, d1) = rng, dom
    return r0 + (v - d0) * (r1 - r0) / (d1 - d0)

I32 = (-(2**31 - 1), 2**31 - 1); U16 = (0, 2**16 - 1)
TAGS = {2: ("time_us", None), 5: ("heading", ((0, 360), U16, False)), 13: ("sensor_lat", ((-90, 90), I32, True)),
        14: ("sensor_lon", ((-180, 180), I32, True)), 15: ("sensor_alt_m", ((-900, 19000), U16, False)),
        16: ("hfov", ((0, 180), U16, False)), 17: ("vfov", ((0, 180), U16, False)),
        23: ("frame_center_lat", ((-90, 90), I32, True)), 24: ("frame_center_lon", ((-180, 180), I32, True)), 65: ("ls_version", None)}

def checksum(data):
    return sum((b << 8) if i % 2 == 0 else b for i, b in enumerate(data)) & 0xFFFF

def main():
    data = open(sys.argv[1], "rb").read()
    i = ok = bad = 0; first = last = None
    while True:
        j = data.find(UAS_KEY, i)
        if j < 0 or j + 17 > len(data):
            break
        p = j + 16; L = data[p]; p += 1
        if L & 0x80:
            n = L & 0x7F; L = int.from_bytes(data[p:p + n], "big"); p += n
        pkt = data[j:p + L]; body = data[p:p + L]
        if len(body) < L:
            break
        good = struct.unpack(">H", pkt[-2:])[0] == checksum(pkt[:-2])
        ok += good; bad += not good
        rec = {}; k = 0
        while k < len(body):
            tag, ln = body[k], body[k + 1]; v = body[k + 2:k + 2 + ln]; k += 2 + ln
            name, m = TAGS.get(tag, (f"tag{tag}", None))
            if tag == 2: rec[name] = struct.unpack(">Q", v)[0]
            elif tag == 65: rec[name] = v[0]
            elif m: rec[name] = round(unmap(v, *m), 6)
        first = first or rec; last = rec; i = p + L
    print(f"ST 0601 packets: {ok + bad}, checksum ok: {ok}, bad: {bad}")
    print("first:", first); print("last: ", last)

if __name__ == "__main__":
    main()
