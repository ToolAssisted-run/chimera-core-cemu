#!/usr/bin/env python3
"""What the mii test program (tests/homebrew/mii) must find: the hash it
keeps of the console's four Mii resource files, in the order it reads them
(FFLResHigh, FFLResHighLG, FFLResMiddle, FFLResMiddleLG), as
"hash/size/size/size/size" the way the test lines print the mailbox (after
its frame count), the hash in hexadecimal.

    mii-hash.py <folder with the four files>
"""
import os
import sys

NAMES = ("FFLResHigh.dat", "FFLResHighLG.dat", "FFLResMiddle.dat", "FFLResMiddleLG.dat")


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    h = 0x811C9DC5
    sizes = []
    for n in NAMES:
        data = open(os.path.join(sys.argv[1], n), "rb").read()
        for b in data:
            h = ((h ^ b) * 0x01000193) & 0xFFFFFFFF
        h = ((h ^ len(data)) * 0x01000193) & 0xFFFFFFFF
        sizes.append(len(data))
    print("%08x/%s" % (h, "/".join(str(s) for s in sizes)))


if __name__ == "__main__":
    main()
