#!/usr/bin/env python3
"""Write an MPEG-TS holding only a MISB ST 0601 KLV stream (synthetic sim drone), for MediaMTX tests.

Encoding follows paretech/klvdata (MIT) misb0601.py / common.py: UAS LS key, BER length,
1-byte tags, linear fixed-point maps, checksum = low 16 bits of the big-endian 16-bit
word sum over the packet up to (not including) the 2 checksum value bytes.
The KLV PES is private stream 1 (0xBD), stream_type 0x06, registration descriptor 'KLVA',
with PTS, so ffmpeg identifies it as codec 'klv'.
Usage: klv_ts_gen.py OUT.ts [seconds] [hz]
"""
import math, struct, sys, time, zlib

UAS_KEY = bytes.fromhex("060E2B34020B01010E01030101000000")

def lmap(v, rng, dom):
    (r0, r1), (d0, d1) = rng, dom
    return round(d0 + (v - r0) * (d1 - d0) / (r1 - r0))

def tlv(tag, val):
    return bytes([tag, len(val)]) + val

def ber(n):
    if n < 128:
        return bytes([n])
    b = n.to_bytes((n.bit_length() + 7) // 8, "big")
    return bytes([0x80 | len(b)]) + b

def checksum(data):
    s = 0
    for i, byte in enumerate(data):
        s += byte << 8 if i % 2 == 0 else byte
    return s & 0xFFFF

def st0601(t_us, lat, lon, alt, hdg, hfov, vfov, fc_lat, fc_lon):
    i32 = (-(2**31 - 1), 2**31 - 1)
    body = b"".join([
        tlv(2, struct.pack(">Q", t_us)),
        tlv(5, lmap(hdg, (0, 360), (0, 2**16 - 1)).to_bytes(2, "big")),
        tlv(13, lmap(lat, (-90, 90), i32).to_bytes(4, "big", signed=True)),
        tlv(14, lmap(lon, (-180, 180), i32).to_bytes(4, "big", signed=True)),
        tlv(15, lmap(alt, (-900, 19000), (0, 2**16 - 1)).to_bytes(2, "big")),
        tlv(16, lmap(hfov, (0, 180), (0, 2**16 - 1)).to_bytes(2, "big")),
        tlv(17, lmap(vfov, (0, 180), (0, 2**16 - 1)).to_bytes(2, "big")),
        tlv(23, lmap(fc_lat, (-90, 90), i32).to_bytes(4, "big", signed=True)),
        tlv(24, lmap(fc_lon, (-180, 180), i32).to_bytes(4, "big", signed=True)),
        tlv(65, bytes([19])),
    ])
    body += bytes([1, 2])  # checksum tag + length; value appended below
    pkt = UAS_KEY + ber(len(body) + 2) + body
    return pkt + struct.pack(">H", checksum(pkt))

def crc32_mpeg(data):
    crc = 0xFFFFFFFF
    for b in data:
        crc ^= b << 24
        for _ in range(8):
            crc = ((crc << 1) ^ 0x04C11DB7) if crc & 0x80000000 else crc << 1
            crc &= 0xFFFFFFFF
    return crc

def section(table_id, ext, payload):
    sec = bytes([table_id]) + struct.pack(">H", 0xB000 | (len(payload) + 9)) + struct.pack(">H", ext) + bytes([0xC1, 0, 0]) + payload
    return sec + struct.pack(">I", crc32_mpeg(sec))

cc = {}
def ts_packets(pid, payload, pusi_first=True):
    out = []
    first = True
    while payload or first:
        chunk = payload[:184]
        payload = payload[184:]
        c = cc.get(pid, 0); cc[pid] = (c + 1) & 15
        hdr = struct.pack(">BH", 0x47, (0x4000 if (first and pusi_first) else 0) | pid)
        if len(chunk) < 184:  # adaptation field stuffing
            stuff = 184 - len(chunk) - 1
            af = bytes([stuff]) + (bytes([0x00]) + b"\xff" * (stuff - 1) if stuff > 0 else b"")
            out.append(hdr + bytes([0x30 | c]) + af + chunk)
        else:
            out.append(hdr + bytes([0x10 | c]) + chunk)
        first = False
    return b"".join(out)

def pts_bytes(pts):
    return bytes([0x21 | ((pts >> 29) & 0x0E), (pts >> 22) & 0xFF, 0x01 | ((pts >> 14) & 0xFE), (pts >> 7) & 0xFF, 0x01 | ((pts << 1) & 0xFE)])

def main():
    out = sys.argv[1]; secs = float(sys.argv[2]) if len(sys.argv) > 2 else 30; hz = float(sys.argv[3]) if len(sys.argv) > 3 else 10
    PMT_PID, KLV_PID = 0x1000, 0x0101
    pat = b"\x00" + section(0x00, 1, struct.pack(">HH", 1, 0xE000 | PMT_PID))
    es = bytes([0x06]) + struct.pack(">H", 0xE000 | KLV_PID)
    desc = bytes([0x05, 4]) + b"KLVA"
    es += struct.pack(">H", 0xF000 | len(desc)) + desc
    pmt = b"\x00" + section(0x02, 1, struct.pack(">HH", 0xE000 | 0x1FFF, 0xF000) + es)
    t0 = int(time.time() * 1e6)
    lat0, lon0, alt = 40.0000, -75.0000, 120.0  # synthetic sim-x1 orbit, not a real site
    with open(out, "wb") as f:
        n = int(secs * hz)
        for i in range(n):
            if i % int(hz) == 0:
                f.write(ts_packets(0, pat) + ts_packets(PMT_PID, pmt))
            t = i / hz
            a = 2 * math.pi * t / 60.0
            lat = lat0 + 0.001 * math.sin(a); lon = lon0 + 0.0013 * math.cos(a)
            hdg = (math.degrees(a) + 90) % 360
            klv = st0601(t0 + int(t * 1e6), lat, lon, alt, hdg, 60.0, 34.0, lat0, lon0)
            pts = int((t + 1.0) * 90000)
            pes = b"\x00\x00\x01\xBD" + struct.pack(">H", len(klv) + 8) + bytes([0x84, 0x80, 5]) + pts_bytes(pts) + klv
            f.write(ts_packets(KLV_PID, pes))
    print(f"wrote {out}: {n} ST 0601 packets at {hz} Hz, {len(klv)} bytes each")

if __name__ == "__main__":
    main()
