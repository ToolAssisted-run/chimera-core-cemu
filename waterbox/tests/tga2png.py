#!/usr/bin/env python3
"""Converts a TGA the test runners write (32-bit, top row first) to a PNG.

usage: tga2png.py <in.tga> <out.png>
"""
import struct
import sys
import zlib


def main():
    d = open(sys.argv[1], "rb").read()
    w = d[12] | d[13] << 8
    h = d[14] | d[15] << 8
    px = d[18:]
    rows = []
    for y in range(h):
        row = bytearray(b"\x00")
        line = px[y * w * 4:(y + 1) * w * 4]
        for x in range(w):
            b, g, r = line[x * 4], line[x * 4 + 1], line[x * 4 + 2]
            row += bytes((r, g, b))
        rows.append(bytes(row))

    def chunk(t, b):
        return struct.pack(">I", len(b)) + t + b + struct.pack(">I", zlib.crc32(t + b) & 0xFFFFFFFF)

    open(sys.argv[2], "wb").write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)) +
                                  chunk(b"IDAT", zlib.compress(b"".join(rows))) + chunk(b"IEND", b""))


if __name__ == "__main__":
    main()
