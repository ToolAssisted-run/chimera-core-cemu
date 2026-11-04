#!/usr/bin/env python3
"""Looks for Nintendo's keys - every array remove-keys.py takes out of
Cemu - in files, packages and this repository's history, and names where
one is found. The keys are read from upstream's own sources (the extern/cemu
submodule's HEAD), never written here: this file holds none of them.

    find-keys.py [--history] <file or .chimeraCore>...

A key is looked for as its bytes, as hex text in both cases, and as a C list
of bytes ("0x.., 0x..", any spacing and case); an array of several keys is
also looked for 16 bytes at a time. --history looks through every commit of
this repository (git log -p --all). Prints "none" or the places, by name.
"""
import importlib.util
import os
import re
import subprocess
import sys
import zipfile

here = os.path.dirname(os.path.abspath(__file__))
root = os.path.dirname(os.path.dirname(here))
spec = importlib.util.spec_from_file_location("rk", os.path.join(root, "waterbox", "remove-keys.py"))
rk = importlib.util.module_from_spec(spec)
spec.loader.exec_module(rk)


def keys():
    out = []
    for rel, arrays in rk.KEYS.items():
        src = subprocess.run(["git", "-C", os.path.join(root, "extern", "cemu"), "show", "HEAD:" + rel],
                             capture_output=True, text=True, check=True).stdout
        for name, _ in arrays:
            m = re.search(r"uint8(?:_t)?\s+" + re.escape(name) + r"\s*\[[^\]]*\]\s*=\s*\{([^}]*)\}", src, re.S)
            if not m:
                sys.exit("find-keys: %s not found in upstream's %s" % (name, rel))
            b = bytes(int(x, 16) for x in re.findall(r"0x([0-9A-Fa-f]{1,2})", m.group(1)))
            pieces = [b] + ([b[i:i + 16] for i in range(0, len(b), 16)] if len(b) > 16 else [])
            out.append((name, pieces))
    return out


def has(data, pieces, squeezed=None):
    for p in pieces:
        if p in data or p.hex().encode() in data or p.hex().upper().encode() in data:
            return True
        # as source text: only where there is a "0x" to begin such a list
        if squeezed is not None and b",".join(b"0x%02x" % x for x in p) in squeezed:
            return True
    return False


def squeeze(data):
    """The text with spacing taken away and lower case, for C byte lists; None
    for a file that holds no such list (a binary's bytes need no squeezing)."""
    if b"\0" in data[:65536] or (b"0x" not in data and b"0X" not in data):
        return None  # a binary, or text with no hex in it
    return re.sub(rb"\s+", b"", data).lower()


def main():
    args = sys.argv[1:]
    history = "--history" in args
    args = [a for a in args if a != "--history"]
    found, looked = [], 0
    ks = keys()
    for path in args:
        blobs = []
        if path.endswith(".chimeraCore"):
            with zipfile.ZipFile(path) as z:
                blobs = [(os.path.basename(path) + ":" + n, z.read(n)) for n in z.namelist()]
        elif os.path.isfile(path):
            blobs = [(os.path.relpath(path, root) if path.startswith(root) else path, open(path, "rb").read())]
        for where, data in blobs:
            looked += 1
            sq = squeeze(data)
            for name, pieces in ks:
                if has(data, pieces, sq):
                    found.append("%s (%s)" % (where, name))
    if history:
        log = subprocess.run(["git", "-C", root, "log", "--all", "-p", "--format=@@commit %h"],
                             capture_output=True, check=True).stdout
        for chunk in log.split(b"@@commit ")[1:]:
            commit = chunk.split(b"\n", 1)[0].decode()
            looked += 1
            sq = squeeze(chunk)
            for name, pieces in ks:
                if has(chunk, pieces, sq):
                    found.append("commit %s (%s)" % (commit, name))
    print("%d keys, %d places looked in: %s" % (len(ks), looked, "; ".join(found) if found else "none"))
    return 1 if found else 0


if __name__ == "__main__":
    sys.exit(main())
