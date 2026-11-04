#!/usr/bin/env python3
"""The picture in a screenshot, as "<width>x<height> <hash>": the hash is of
its red, green and blue values, top row first, so that a TGA the test
runners write and a PNG Chimera writes can be compared.

    picture.py <file.tga | file.png>

Reads uncompressed 32-bit TGAs (as gate-harness.h writes them) and 8-bit
RGB or RGBA PNGs (as chimera-run writes them).
"""
import hashlib
import struct
import sys
import zlib


def tga(d):
    w, h = d[12] | d[13] << 8, d[14] | d[15] << 8
    top_first = d[17] & 0x20
    px = d[18:]
    rows = []
    for y in range(h):
        line = px[y * w * 4:(y + 1) * w * 4]
        rgb = bytearray()
        for x in range(w):
            b, g, r = line[x * 4], line[x * 4 + 1], line[x * 4 + 2]
            rgb += bytes((r, g, b))
        rows.append(bytes(rgb))
    if not top_first:
        rows.reverse()
    return w, h, rows


def png(d):
    assert d[:8] == b"\x89PNG\r\n\x1a\n", "not a PNG"
    at, idat, w = 8, b"", 0
    while at < len(d):
        n, t = struct.unpack(">I4s", d[at:at + 8])
        body = d[at + 8:at + 8 + n]
        if t == b"IHDR":
            w, h, depth, ctype = struct.unpack(">IIBB", body[:10])
            assert depth == 8 and ctype in (2, 6), "only 8-bit RGB or RGBA"
            bpp = 3 if ctype == 2 else 4
        elif t == b"IDAT":
            idat += body
        at += 12 + n
    raw = zlib.decompress(idat)
    stride = w * bpp
    rows, prev = [], bytearray(stride)
    for y in range(h):
        f = raw[y * (stride + 1)]
        line = bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        for i in range(stride):
            a = line[i - bpp] if i >= bpp else 0
            b = prev[i]
            c = prev[i - bpp] if i >= bpp else 0
            if f == 1:
                line[i] = (line[i] + a) & 255
            elif f == 2:
                line[i] = (line[i] + b) & 255
            elif f == 3:
                line[i] = (line[i] + (a + b) // 2) & 255
            elif f == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[i] = (line[i] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        prev = line
        rows.append(bytes(line) if bpp == 3 else bytes(v for i, v in enumerate(line) if i % 4 != 3))
    return w, h, rows


def main():
    d = open(sys.argv[1], "rb").read()
    w, h, rows = png(d) if d[:4] == b"\x89PNG" else tga(d)
    print("%dx%d %s" % (w, h, hashlib.sha1(b"".join(rows)).hexdigest()[:16]))


if __name__ == "__main__":
    main()
