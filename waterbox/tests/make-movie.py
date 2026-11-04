#!/usr/bin/env python3
"""A movie's input lines for the Cemu core's package, from a test script of
the runners (gate-harness.h, --input): lines "F MASK [A0 ...]", from frame F
(the first frame is 1) on these buttons held and these axes.

    make-movie.py <waterbox.config> <script> <frames> > movie.txt

A line a frame, as Chimera writes them: the console's group (none), then
the GamePad's axes (five places each) and a letter or a dot for each button,
in the package's order.
"""
import json
import sys


def main():
    cfg = json.load(open(sys.argv[1]))
    frames = int(sys.argv[3])
    inp = cfg["input"]
    buttons = inp["buttons"]
    letters = []
    for name in buttons:
        bare = name.split(" ", 1)[1] if name.startswith("P1 ") else name
        letters.append(inp["mnemonics"].get(name) or inp["mnemonics"][bare])
    neutral = [a["neutral"] for a in inp["axes"]]
    script = []
    for line in open(sys.argv[2]):
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split()
        script.append((int(parts[0]), int(parts[1], 0), [int(v, 0) for v in parts[2:]]))
    for frame in range(1, frames + 1):
        mask, axes = 0, list(neutral)
        for start, m, values in script:
            if start <= frame:
                mask = m
                for i, v in enumerate(values):
                    axes[i] = v
        cells = "".join(letters[i] if mask >> i & 1 else "." for i in range(len(buttons)))
        print("||" + "".join("%5d," % v for v in axes) + cells + "|")


if __name__ == "__main__":
    main()
