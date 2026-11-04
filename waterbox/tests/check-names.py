#!/usr/bin/env python3
"""Names are never translated or bent (Chimera's owner, 2026-10-31): a
program's, game's, character's, machine's or company's name is written as
it is, in every language. A language whose grammar puts endings on names
says the sentence with a common noun beside the name instead ("programu
Cemu", "Cemu-ohjelman"). This checks this core's 26 language tables
(waterbox/languages):

- every name an English text holds is in that text in every language,
  written as it is (a name in another script, such as a transliteration,
  is not there);
- no Latin letter is glued to either side of a name, and no short ending
  is joined to one by a hyphen, colon or apostrophe ("Cemun", "Cemu:n",
  "GamePad'in", "GamePads"). A particle in another script (Korean's,
  Japanese's) is that language's grammar and leaves the name whole; a
  hyphen and a common noun ("Wii U-Archiv", "GamePad-ohjaimen") is a
  compound, not an ending.

(Copied from the rsdkv5u core's, with this core's names.) Chimera's tools/check-proper-nouns.py does the same for Chimera's own
tables and names; this one knows this core's names too.

  check-names.py [<languages folder>]

Prints each problem as language key: what, and exits 1 when there is any."""
import json
import os
import re
import sys

here = os.path.dirname(os.path.abspath(__file__))
folder = sys.argv[1] if len(sys.argv) > 1 else os.path.join(here, "..", "languages")

# Chimera's own (tools/check-proper-nouns.py), then this core's: the
# emulator, the console, its controller, the stand-in font
NAMES = ["Chimera", "TAStudio", "GitHub", "Lua", "Windows", "Linux", "macOS", "OpenGL", "Vulkan", "Direct3D", "SQLite",
         "Sublime Text", "miniBox", "ffmpeg", "Qt", "Wayland",
         "Cemu", "Wii U", "GamePad", "Lato", "Nintendo"]
LATIN = "A-Za-z\u00c0-\u024f"
# an ending joined by a hyphen, colon or apostrophe: at most four letters and
# the word ends there ("-ben", ":ssa", "-n", "'in"), or an apostrophe alone (a
# genitive "Origins'"). A longer word after a hyphen is a common noun making
# a compound ("Plus-Inhalte", "-datapaketti").
ENDING = "[-:'\u2019][" + LATIN + "]{1,4}(?![" + LATIN + "])|['\u2019](?![" + LATIN + "])"
# a file's name is not a name in a sentence (CafeStd.ttf, resources/sharedFonts)
FILE = re.compile(r"[\w/]+\.(ttf|wua|wuhb|rpx)\b|resources/sharedFonts")


def whole(name):
    return re.compile("(?<![" + LATIN + "])" + re.escape(name) + "(?![" + LATIN + "])")


def bent(name):
    return re.compile("[" + LATIN + "]" + re.escape(name) + "|" + re.escape(name) + "([" + LATIN + "]|" + ENDING + ")")


def texts(path):
    """a table's texts, a key's plural forms as one text"""
    with open(path, encoding="utf-8") as f:
        table = json.load(f)["texts"]
    return {k: " | ".join(v.values()) if isinstance(v, dict) else v for k, v in table.items()}


def main():
    english = texts(os.path.join(folder, "en.json"))
    names_in = {k: [n for n in NAMES if whole(n).search(v)] for k, v in english.items()}
    bad = 0
    tables = sorted(n for n in os.listdir(folder) if n.endswith(".json"))
    for name in tables:
        code = name[:-5]
        for key, text in sorted(texts(os.path.join(folder, name)).items()):
            if code != "en":
                for n in names_in.get(key, []):
                    if not whole(n).search(text):
                        print("%s %s: %s is not there as itself: %s" % (code, key, n, text))
                        bad += 1
            if code == "en":
                continue  # English's own grammar ("Sonic Origins' older games") is the source
            plain = FILE.sub("", text)
            seen = set()
            for n in sorted(NAMES, key=len, reverse=True):
                for m in bent(n).finditer(plain):
                    # one report a place: "Sonic Originsin" is not also "Origins" bent
                    if m.end() in seen:
                        continue
                    seen.add(m.end())
                    print("%s %s: %s is bent (%r): %s" % (code, key, n, m.group(0), text))
                    bad += 1
    print("%d tables, %d names bent or missing" % (len(tables), bad), file=sys.stderr)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
