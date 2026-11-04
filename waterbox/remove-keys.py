#!/usr/bin/env python3
"""Takes Nintendo's keys out of Cemu's sources (owner's decisions of
2026-11-03): the Wii U common key, which the user gives instead as
firmware, and the SpotPass, amiibo, NFC and icon database keys, whose
features this core does not offer (SpotPass and the icon database need a
network, which it has none of). Each array is named here, never its bytes: its initializer
is rewritten to zeros of the same size. A patch that removed those lines
would carry the keys in them, so apply-patches.sh runs this on the tree
(and on its scratch copy) before the patches, and make-patches.sh writes
the patches against a copy that has been through it.

    remove-keys.py <cemu tree> [--check]
    remove-keys.py --list

--check says, and fails, if a named array still has bytes or is not found.
--list prints the files it rewrites.
"""
import os
import re
import sys

# file: [(array name, what follows the zeroed array, or "")]
KEYS = {
    "src/Cemu/ncrypto/ncrypto.cpp": [
        ("commonKey", " // chimera: the user's, given as firmware, not in this program\n\t\tchimera_common_key(commonKey);"),
    ],
    "src/Cafe/IOSU/nn/boss/boss_service.cpp": [
        ("bossAesKey", " // chimera: SpotPass's key is not in this program (the core has no network)"),
    ],
    "src/Cafe/OS/libs/nn_nfp/AmiiboCrypto.h": [
        (name, " // chimera: an amiibo key is not in this program") for name in
        ("dataKey_hmacKey", "dataKey_magicBytes", "dataKey_xorPad", "tagKey_hmacKey", "tagKey_magicBytes", "tagKey_xorPad")
    ],
    "src/Cafe/OS/libs/nn_idbe/nn_idbe.cpp": [
        (name, " // chimera: the icon database's key is not in this program (the core has no network)") for name in
        ("idbeAesKeys", "idbeAesIv")
    ],
    "src/Cemu/napi/napi_idbe.cpp": [
        (name, " // chimera: the icon database's key is not in this program (the core has no network)") for name in
        ("idbeAesKeys", "idbeAesIv")
    ],
    "src/Cafe/IOSU/ccr_nfc/iosu_ccr_nfc.cpp": [
        (name, " // chimera: an NFC key is not in this program") for name in
        ("sNfcKey", "sNfcKeyIV", "sLockedSecretHmacKey", "sUnfixedInfosHmacKey", "sUnfixedInfosMagicBytes", "sLockedSecretMagicBytes")
    ],
}

BYTE = re.compile(r"0x[0-9A-Fa-f]{1,2}|\d+")


def rewrite(text, name, after):
    """The array's initializer as zeros; (text, 'done' | 'already' | 'missing')."""
    decl = re.compile(r"((?:static\s+)?(?:constexpr\s+)?(?:const\s+)?uint8(?:_t)?\s+" + re.escape(name) +
                      r")\s*\[([^\]]*)\]\s*=\s*\{([^}]*)\}\s*;", re.S)
    m = decl.search(text)
    if not m:
        zeroed = re.compile(r"uint8(?:_t)?\s+" + re.escape(name) + r"\s*\[[^\]]*\]\s*\{\s*\}\s*;")
        return text, ("already" if zeroed.search(text) else "missing")
    size = m.group(2).strip() or str(len(BYTE.findall(m.group(3))))
    new = "%s[%s]{};%s" % (m.group(1), size, after)
    return text[:m.start()] + new + text[m.end():], "done"


def main():
    if sys.argv[1:] == ["--list"]:
        print("\n".join(KEYS))
        return 0
    tree = sys.argv[1]
    check = "--check" in sys.argv[2:]
    bad = []
    for rel, arrays in KEYS.items():
        path = os.path.join(tree, rel)
        text = open(path, encoding="utf-8").read()
        for name, after in arrays:
            text, what = rewrite(text, name, after)
            if what == "missing":
                bad.append("%s: %s not found" % (rel, name))
            elif check and what == "done":
                bad.append("%s: %s still has its bytes" % (rel, name))
        if not check:
            open(path, "w", encoding="utf-8").write(text)
    for line in bad:
        print("remove-keys: " + line, file=sys.stderr)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
