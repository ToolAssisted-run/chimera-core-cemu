#!/usr/bin/env python3
"""Compares two memory dumps of run-native/run-wbx (--dump-ram): names the
4 KiB pages that differ, by the console's address, and shows the first
differing words of each.

usage: diff-ram.py <dump a> <dump b> [max pages]
"""
import sys


def ranges(path):
    out = []
    off = 0
    for line in open(path + ".index"):
        name, base, size = line.split()
        out.append((name, int(base, 16), int(size, 16), off))
        off += int(size, 16)
    return out


def main():
    a, b = sys.argv[1], sys.argv[2]
    limit = int(sys.argv[3]) if len(sys.argv) > 3 else 20
    ra, rb = ranges(a), ranges(b)
    if ra != rb:
        print("the two dumps have different ranges")
        return 1
    da, db = open(a, "rb").read(), open(b, "rb").read()
    shown = 0
    total = 0
    for name, base, size, off in ra:
        for p in range(0, size, 4096):
            x = da[off + p:off + p + 4096]
            y = db[off + p:off + p + 4096]
            if x == y:
                continue
            total += 1
            if shown < limit:
                shown += 1
                words = [i for i in range(0, len(x), 4) if x[i:i + 4] != y[i:i + 4]]
                first = words[:4]
                print(f"{name} page {base + p:08x}: {len(words)} words differ, e.g. " +
                      ", ".join(f"{base + p + i:08x} {x[i:i + 4].hex()}/{y[i:i + 4].hex()}" for i in first))
    print(f"{total} pages differ")
    return 1 if total else 0


if __name__ == "__main__":
    sys.exit(main())
