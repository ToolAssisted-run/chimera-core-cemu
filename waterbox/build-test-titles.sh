#!/bin/sh
# The test programs as the console's other file formats, for the tests of
# the ways a game is given to the core: a Wii U archive (.wua: a title's
# code, content and meta folders, compressed into one file, as Cemu makes
# them) and a homebrew bundle (.wuhb, as wut's wuhbtool makes them). Made
# from build/homebrew/frames.rpx (waterbox/build-homebrew.sh first), into
# build/homebrew (and saves.rpx as saves.wua, mii.rpx as mii.wua). The archive tool is
# ZArchive's own, built here.
#
#   waterbox/build-test-titles.sh
set -eu
here="$(cd "$(dirname "$0")" && pwd)"
root="$(dirname "$here")"
out="$root/build/homebrew"
dkp="$root/build/devkitpro"
if [ -n "${DEVKITPRO:-}" ] && [ -x "$DEVKITPRO/tools/bin/wuhbtool" ]; then dkp="$DEVKITPRO"; fi
[ -f "$out/frames.rpx" ] || { echo "no build/homebrew/frames.rpx: run waterbox/build-homebrew.sh" >&2; exit 1; }
[ -x "$dkp/tools/bin/wuhbtool" ] || { echo "no wuhbtool at $dkp/tools/bin: run waterbox/fetch-devkitpro.sh" >&2; exit 1; }
deps="$root/build/deps/native"
[ -f "$deps/lib/libzstd.a" ] || { echo "no zstd at $deps: run waterbox/build-native.sh" >&2; exit 1; }

# the archive tool
za="$root/extern/cemu/dependencies/ZArchive"
tool="$out/zarchive"
if [ ! -x "$tool" ]; then
	gcc -O2 -c -o "$out/sha_256.o" "$za/src/sha_256.c"
	g++ -std=c++20 -O2 -I"$za/include" -I"$deps/include" -o "$tool" \
		"$za/src/main.cpp" "$za/src/zarchivereader.cpp" "$za/src/zarchivewriter.cpp" "$out/sha_256.o" \
		"$deps/lib/libzstd.a"
fi

# Titles of this repository's own: title ids no published title has (the
# Wii U's application titles are 00050000-1xxxxxxx), each program a title's
# code, a file of content, and the three files of metadata a console's
# title has. frames.wua holds the frames program; saves.wua the saves
# program (a game's save folder is its title's).
make_title() { # <program> <title id> <product code> [<name of the archive>]
	prog="$1"; tid="$2"; code="$3"; name="${4:-$1}"
	title="$out/title-$name/${tid}_v0"
	rm -rf "$out/title-$name"
	mkdir -p "$title/code" "$title/content" "$title/meta"
	cp "$out/$prog.rpx" "$title/code/$prog.rpx"
	printf 'content of the %s test title\n' "$prog" > "$title/content/readme.txt"
	cat > "$title/code/app.xml" <<XML
<?xml version="1.0" encoding="utf-8"?>
<app type="complex" access="777">
  <version type="unsignedInt" length="4">16</version>
  <os_version type="hexBinary" length="8">000500101000400A</os_version>
  <title_id type="hexBinary" length="8">$tid</title_id>
  <title_version type="hexBinary" length="2">0000</title_version>
  <sdk_version type="unsignedInt" length="4">21204</sdk_version>
  <app_type type="hexBinary" length="4">80000000</app_type>
  <group_id type="hexBinary" length="4">0000F1C3</group_id>
</app>
XML
	cat > "$title/code/cos.xml" <<XML
<?xml version="1.0" encoding="utf-8"?>
<app type="complex" access="777">
  <version type="unsignedInt" length="4">1</version>
  <argstr type="string" length="4096">$prog.rpx</argstr>
</app>
XML
	cat > "$title/meta/meta.xml" <<XML
<?xml version="1.0" encoding="utf-8"?>
<menu type="complex" access="777">
  <version type="unsignedInt" length="4">33</version>
  <product_code type="string" length="32">$code</product_code>
  <content_platform type="string" length="32">WUP</content_platform>
  <company_code type="string" length="8">0000</company_code>
  <title_id type="hexBinary" length="8">$tid</title_id>
  <title_version type="unsignedInt" length="4">0</title_version>
  <region type="hexBinary" length="4">00000007</region>
  <longname_en type="string" length="512">Chimera $prog test</longname_en>
  <shortname_en type="string" length="256">$prog</shortname_en>
  <publisher_en type="string" length="256">chimera-core-cemu</publisher_en>
</menu>
XML
	rm -f "$out/$name.wua"
	"$tool" "$out/title-$name" "$out/$name.wua" > "$out/zarchive.log" 2>&1 || { cat "$out/zarchive.log" >&2; exit 1; }
	echo "built: build/homebrew/$name.wua"
}
make_title frames 00050000F1C3E000 WUP-N-CHMA
[ -f "$out/saves.rpx" ] && make_title saves 00050000F1C3E100 WUP-N-CHMB
# the mii program reads the console's storage, which a program on its own
# is not given
[ -f "$out/mii.rpx" ] && make_title mii 00050000F1C3E200 WUP-N-CHMD
# so does the storage program
[ -f "$out/storage.rpx" ] && make_title storage 00050000F1C3E300 WUP-N-CHME
# One exception: profiled.wua is the frames program under a published
# title's id, Super Mario 3D World (USA), 00050000-10145C00, so that Cemu
# applies the profile it ships for that title (accurateShaderMul = false).
# It holds nothing of that game.
make_title frames 0005000010145C00 WUP-N-CHMC profiled
title="$out/title-frames/00050000F1C3E000_v0"

rm -f "$out/frames.wuhb"
"$dkp/tools/bin/wuhbtool" "$out/frames.rpx" "$out/frames.wuhb" --name="Chimera frames test" --short-name=frames \
	--author=chimera-core-cemu --content="$title/content" > "$out/wuhbtool.log" 2>&1 || { cat "$out/wuhbtool.log" >&2; exit 1; }
echo "built: build/homebrew/frames.wuhb"
