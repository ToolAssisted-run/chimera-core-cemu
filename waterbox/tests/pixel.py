#!/usr/bin/env python3
"""Colours at points of a screenshot the test runners write (a 32-bit TGA,
top row first), as RRGGBB, one a point.

    pixel.py <file.tga> <x>,<y> [<x>,<y> ...]
"""
import sys


def main():
    d = open(sys.argv[1], "rb").read()
    w = d[12] | d[13] << 8
    out = []
    for point in sys.argv[2:]:
        x, y = (int(v) for v in point.split(","))
        at = 18 + (y * w + x) * 4
        b, g, r = d[at], d[at + 1], d[at + 2]
        out.append("%02x%02x%02x" % (r, g, b))
    print(" ".join(out))


if __name__ == "__main__":
    main()
