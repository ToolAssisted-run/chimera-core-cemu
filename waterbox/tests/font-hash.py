#!/usr/bin/env python3
"""What the fonts test program (tests/homebrew/fonts) must find: the hash it
keeps of the four fonts the console gives it, in the order it asks for them
(Chinese, Korean, standard, traditional Chinese), as "hash/size/size/size/size"
the way the test lines print the mailbox (after its frame count).

    font-hash.py <folder with CafeCn.ttf CafeKr.ttf CafeStd.ttf CafeTw.ttf>
    font-hash.py --standin <Cemu's src/resource/CafeDefaultFont.cpp>

The stand-in is Cemu's own (Lato, zlib-compressed in its source): with no
fonts, every one of the four is it.
"""
import os
import re
import sys
import zlib


def fonts(argv):
    if len(argv) == 3 and argv[1] == "--standin":
        src = open(argv[2]).read()
        body = src[src.index("{") + 1:src.index("};")]
        data = zlib.decompress(bytes(int(x, 16) for x in re.findall(r"0x([0-9a-fA-F]{2})", body)))
        return [data] * 4
    if len(argv) == 2:
        return [open(os.path.join(argv[1], n), "rb").read() for n in ("CafeCn.ttf", "CafeKr.ttf", "CafeStd.ttf", "CafeTw.ttf")]
    sys.exit(__doc__)


def main():
    h = 0x811C9DC5
    sizes = []
    for data in fonts(sys.argv):
        for b in data:
            h = ((h ^ b) * 0x01000193) & 0xFFFFFFFF
        h = ((h ^ len(data)) * 0x01000193) & 0xFFFFFFFF
        sizes.append(len(data))
    print("%08x/%s" % (h, "/".join(str(s) for s in sizes)))


if __name__ == "__main__":
    main()
