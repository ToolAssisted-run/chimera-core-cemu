#!/usr/bin/env python3
"""What the storage test program (tests/homebrew/storage) must find: the
folders it looks for all there (63), and the system's lists of languages and
countries as upstream Cemu's window writes them into a new console storage
(CemuApp::CreateDefaultMLCFiles), the countries taken from upstream's own
table in ncrypto.cpp. Printed as the test lines print the mailbox after its
frame count: "languagehash/63/languagesize/countrysize/countryhash", the
first hash in hexadecimal, the second in decimal.

    storage-lists.py <Cemu's ncrypto.cpp>
"""
import re
import sys


def fnv(data):
    h = 0x811C9DC5
    for b in data:
        h = ((h ^ b) * 0x01000193) & 0xFFFFFFFF
    return h


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    source = open(sys.argv[1], encoding="utf-8").read()
    start = source.index("g_countryTable = {")
    table = dict((int(k), v) for k, v in re.findall(r'\{\s*(\d+)\s*,\s*"([^"]*)"\s*\}', source[start:source.index("};", start)]))
    languages = "".join('"%s",\n' % code for code in ("ja", "en", "fr", "de", "it", "es", "zh", "ko", "nl", "pt", "ru", "zh"))
    # upstream's loop: an index from 0 to the table's size, "NN" for an
    # index the table does not have, and "NN" written as NULL
    countries = ""
    for i in range(len(table)):
        code = table.get(i, "NN")
        countries += "NULL,\n" if code.upper() == "NN" else '"%s",\n' % code
    lang, country = languages.encode(), countries.encode()
    print("%08x/63/%d/%d/%d" % (fnv(lang), len(lang), len(country), fnv(country)))


if __name__ == "__main__":
    main()
