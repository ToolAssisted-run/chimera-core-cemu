#!/bin/sh
# The package in Chimera's engine: projects made and run by Chimera's
# headless runner (chimera-run), held to the core's own sandbox runner
# (run-wbx). Each test says what it compared, and the script ends with a
# count; a SKIP is not a pass.
#
#   waterbox/run-frontend.sh --chimera-root <chimera checkout>
#
# Needs build/package/cemu.chimeraCore (waterbox/build-package.sh), the
# sandbox runner (waterbox/build-guest.sh), the test programs
# (waterbox/build-homebrew.sh, waterbox/build-test-titles.sh), and Chimera
# built in the checkout named (its build/dll/chimera-run, and for
# window:program its window, with CEMU_QT_LIB naming Qt's libraries). The
# files no runner has (fonts, the Mii resource, a game and the common key)
# are named by variables, below; without them those tests are skipped.
set -u
here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/.." && pwd)"
chimera_root="${CHIMERA_ROOT:-}"
while [ $# -gt 0 ]; do
	case "$1" in
		--chimera-root) chimera_root="$2"; shift ;;
		*) echo "unknown argument: $1" >&2; exit 2 ;;
	esac
	shift
done
[ -n "$chimera_root" ] && [ -d "$chimera_root" ] || { echo "say where Chimera is: --chimera-root <checkout> (or CHIMERA_ROOT)" >&2; exit 2; }
chimera_root="$(cd "$chimera_root" && pwd)"
runner="$chimera_root/build/dll/chimera-run"
package="$root/build/package/cemu.chimeraCore"
wbx="$root/build/wbx"
hb="$root/build/homebrew"
# What no runner has and this repository never holds, named by the person
# who runs the tests (as for waterbox/run-gate.sh); a test that needs one it
# is not given says SKIP: CEMU_FONTS (Cemu's copies of the four fonts),
# CEMU_CONSOLE_FONTS (the console's own), CEMU_MII (the console's Mii
# resource), CEMU_NUS_GAME with CEMU_NUS_DLC (a game's NUS zip and its
# add-on content's) and CEMU_COMMON_KEY (the Wii U common key's file).
fonts_dir="${CEMU_FONTS:-}"
console_fonts_dir="${CEMU_CONSOLE_FONTS:-}"
nus_game="${CEMU_NUS_GAME:-}"
nus_dlc="${CEMU_NUS_DLC:-}"
common_key="${CEMU_COMMON_KEY:-}"
mii_dir="${CEMU_MII:-}"
[ -x "$runner" ] || { echo "Chimera's headless runner is not built: $runner" >&2; exit 1; }
[ -f "$package" ] || { echo "no package: run waterbox/build-package.sh" >&2; exit 1; }
[ -x "$wbx/run-wbx" ] && [ -f "$wbx/core.wbx" ] || { echo "no sandbox runner: run waterbox/build-guest.sh" >&2; exit 1; }
for f in frames.rpx pad.rpx fonts.rpx readback.rpx frames.wua saves.wua mii.wua; do
	[ -f "$hb/$f" ] || { echo "no build/homebrew/$f: run waterbox/build-homebrew.sh and waterbox/build-test-titles.sh" >&2; exit 1; }
done
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
ok=0; failed=0; skipped=0
report() { # <test> <PASS|FAIL|SKIP> <what>
	printf '%-22s %-6s %s\n' "$1" "$2" "$3"
	case "$2" in PASS) ok=$((ok + 1)) ;; FAIL) failed=$((failed + 1)) ;; *) skipped=$((skipped + 1)) ;; esac
}
# heavy runs under a memory cap (the console's address space is 4 GiB),
# where the user's systemd can make one (a CI runner has none: there they
# run without)
can_cap=0
command -v systemd-run > /dev/null 2>&1 && systemd-run --user --scope --quiet true 2> /dev/null && can_cap=1
capped() {
	if [ "$can_cap" = 1 ]; then systemd-run --user --scope --quiet -p MemoryMax=12G -p MemorySwapMax=0 "$@"; else "$@"; fi
}
# new_project <name> <game> <movie> <settings json> [chimera-run options]: a
# project in its own folder, with the game beside it and the movie as its
# input (a project runs as many frames as its movie has)
new_project() {
	name="$1"; game="$2"; movie="$3"; settings="$4"; shift 4
	mkdir -p "$work/$name"
	cp "$game" "$work/$name/"
	"$runner" --new-project "$work/$name/$name.chimeraProject" "$package" "$work/$name/$(basename "$game")" "$movie" \
		--settings "$settings" "$@" > "$work/$name.new.log" 2>&1
}
# movie <script or /dev/null> <frames> <out>
movie() { python3 "$here/tests/make-movie.py" "$here/waterbox.config" "$1" "$2" > "$3"; }
# run_project <name> <tag> [chimera-run options]: the project in the engine
run_project() {
	name="$1"; tag="$2"; shift 2
	capped "$runner" --project "$work/$name/$name.chimeraProject" "$package" --gpu --meta "$work/$tag.meta" "$@" \
		> "$work/$tag.log" 2>&1
}
meta() { sed -n "s/^$2=//p" "$work/$1.meta" 2>/dev/null | head -1; }
# the core's own runner: run-wbx <tag> <program> [options]
run_wbx() {
	tag="$1"; prog="$2"; shift 2
	capped "$wbx/run-wbx" "$wbx/core.wbx" "$prog" --work "$work/$tag-files" "$@" > "$work/$tag.out" 2> "$work/$tag.err"
}
# MEM2 (console address 0x10000000, 1 GiB) out of the runner's dump of every
# mapped range (its .index names them)
mem2_of() { # <dump> <out>
	python3 - "$1" "$2" <<'PY'
import sys
dump, out = sys.argv[1], sys.argv[2]
at = 0
for line in open(dump + ".index"):
    _, base, size = line.split()
    base, size = int(base, 16), int(size, 16)
    if base == 0x10000000:
        with open(dump, "rb") as f:
            f.seek(at)
            open(out, "wb").write(f.read(size))
        sys.exit(0)
    at += size
sys.exit("no MEM2 in " + dump)
PY
}
# the test programs' mailbox in a dump of MEM2: its values after the magic,
# found where the core finds it (on a 64-byte boundary; the program's own
# copy of the magic, in its constants, is not)
mailbox_of() { # <dump>
	python3 - "$1" <<'PY'
import struct, sys
d = open(sys.argv[1], "rb").read()
at = d.find(b"CHIMERA-MAILBOX\0")
while at >= 0 and at % 64:
    at = d.find(b"CHIMERA-MAILBOX\0", at + 1)
print("none" if at < 0 else "/".join(str(v) for v in struct.unpack(">6I", d[at + 16:at + 40])))
PY
}
same_file() { [ -s "$1" ] && cmp -s "$1" "$2"; }

# ---- the frames program, 300 frames: the engine's run is the sandbox
# runner's - the console's main memory and the picture at the end
frames=300
movie /dev/null $frames "$work/empty.movie"
new_project frames "$hb/frames.rpx" "$work/empty.movie" '{"cpu_mode": "interpreter", "console_fonts": "standin", "mii_resource": "none"}'
run_project frames f1 --frames $frames --dump "MEM2=$work/f1.mem2" --final-screenshot "$work/f1.tga"
run_wbx w1 "$hb/frames.rpx" --frames $frames --every $frames --dump-ram "$work/w1.ram" --screenshot "$frames=$work/w1.tga"
mem2_of "$work/w1.ram" "$work/w1.mem2" 2> /dev/null
pic_e="$(python3 "$here/tests/picture.py" "$work/f1.tga" 2> /dev/null)"
pic_w="$(python3 "$here/tests/picture.py" "$work/w1.tga" 2> /dev/null)"
if [ "$(meta f1 frames)" = $frames ] && same_file "$work/f1.mem2" "$work/w1.mem2" && [ -n "$pic_e" ] && [ "$pic_e" = "$pic_w" ]; then
	report "engine:program" PASS "$frames frames through a project: MEM2 and the picture ($pic_e) are the sandbox runner's; the program's record $(mailbox_of "$work/f1.mem2")"
else
	report "engine:program" FAIL "frames $(meta f1 frames) status $(meta f1 status) $(meta f1 detail); MEM2 $(cmp "$work/f1.mem2" "$work/w1.mem2" 2>&1 | head -1); picture engine [$pic_e] runner [$pic_w]"
fi

# ---- the package in Chimera's window, run headless (Linux): the frames
# program's project opens there - the window gives a core the GPU bridge only
# for a renderer whose value ends in -hw, which the package's one renderer
# does - and the machine a Lua script of the window seeks to, frame 300, is
# chimera-run's (MEM2's SHA-256). Needs the window built in the Chimera
# checkout and the lib folder of the Qt 6 it was built with (CEMU_QT_LIB).
window="$chimera_root/build/meson-linux/source/ui/chimera"
qt_lib="${CEMU_QT_LIB:-}"
if [ ! -x "$window" ]; then
	report "window:program" SKIP "Chimera's window is not built: $window"
elif [ -z "$qt_lib" ] || [ ! -f "$qt_lib/libQt6Core.so.6" ]; then
	report "window:program" SKIP "no Qt 6 libraries (set CEMU_QT_LIB to the lib folder of the Qt 6.12 Chimera was built with)"
else
	wq="$work/window"
	mkdir -p "$wq"
	printf '{}\n' > "$wq/settings.json"
	printf 'to=%s\nmeta=%s\n' "$frames" "$wq/run.meta" > "$wq/job"
	# its settings, cache, recovery files and data all in the test's folder
	( cd "$wq" && capped env -u DISPLAY LD_LIBRARY_PATH="$qt_lib" QT_PLUGIN_PATH="$qt_lib/../plugins" QML_IMPORT_PATH="$qt_lib/../qml" \
		ALSOFT_DRIVERS=null CHIMERA_JOB="$wq/job" CHIMERA_QT_CACHE="$wq/qt-cache" CHIMERA_QT_RECOVERY="$wq/qt-recovery" \
		CHIMERA_QT_HASHES="$wq/qt-file-hashes.txt" CHIMERA_DATA_HOME="$wq/data" XDG_CACHE_HOME="$wq/cache-home" \
		XDG_DATA_HOME="$wq/data-home" XDG_CONFIG_HOME="$wq/config-home" \
		timeout 600 "$window" --headless "--config=$wq/settings.json" "--core=$package" \
		"--lua=$here/tests/window-seek.lua" "--project=$work/frames/frames.chimeraProject" ) > "$work/window.log" 2>&1
	got="$(sed -n 's/^mem2=//p' "$wq/run.meta" 2> /dev/null)"
	want="$(sha256sum "$work/f1.mem2" 2> /dev/null | cut -d' ' -f1 | tr 'a-f' 'A-F')"
	gl="$(grep -a -m1 'chimera gl:' "$work/window.log" | cut -c1-80)"
	if grep -q '^status=OK' "$wq/run.meta" 2> /dev/null && [ -n "$want" ] && [ "$got" = "$want" ]; then
		report "window:program" PASS "the frames program's project opened in the window ($gl); MEM2 at frame $frames is chimera-run's"
	else
		report "window:program" FAIL "$(sed -n 's/^detail=//p' "$wq/run.meta" 2> /dev/null) $(grep -a 'headless\]' "$work/window.log" | tail -2 | tr '\n' ' ' | cut -c1-200); MEM2 [$got] want [$want]"
	fi
fi

# ---- the pad program with a movie: the package's controls, in its order,
# reach the console as the core's runner gives them (every button, the
# sticks, the touch screen); the lag frames the engine counts are the
# runner's
cat > "$work/pad.input" <<'INPUT'
30 0x1265F
60 0xD9A0
90 0 16384 -32768 0 32767
120 0x20000 0 0 0 0 32768 52000
150 0
INPUT
pf=180
movie "$work/pad.input" $pf "$work/pad.movie"
new_project pad "$hb/pad.rpx" "$work/pad.movie" '{"cpu_mode": "interpreter", "console_fonts": "standin", "mii_resource": "none"}'
run_project pad p1 --frames $pf --dump "MEM2=$work/p1.mem2" --final-screenshot "$work/p1.tga" --count-input-polls
run_wbx pw "$hb/pad.rpx" --frames $pf --every 30 --input "$work/pad.input" --dump-ram "$work/pw.ram" --screenshot "$pf=$work/pw.tga"
mem2_of "$work/pw.ram" "$work/pw.mem2" 2> /dev/null
lag_w=0
for l in $(sed -n 's/.* lag=\([0-9]*\)$/\1/p' "$work/pw.out" | head -6); do lag_w=$((lag_w + l)); done
pic_e="$(python3 "$here/tests/picture.py" "$work/p1.tga" 2> /dev/null)"
pic_w="$(python3 "$here/tests/picture.py" "$work/pw.tga" 2> /dev/null)"
if [ "$(meta p1 frames)" = $pf ] && same_file "$work/p1.mem2" "$work/pw.mem2" && [ "$pic_e" = "$pic_w" ] && [ "$(meta p1 lagFrames)" = "$lag_w" ]; then
	report "engine:input" PASS "$pf frames with a movie pressing every control: MEM2, the picture and the lag frames ($lag_w) are the runner's with the same script; the program's record $(mailbox_of "$work/p1.mem2")"
else
	report "engine:input" FAIL "frames $(meta p1 frames) $(meta p1 detail); MEM2 $(cmp "$work/p1.mem2" "$work/pw.mem2" 2>&1 | head -1); picture [$pic_e] [$pic_w]; lag engine $(meta p1 lagFrames) runner $lag_w; record engine $(mailbox_of "$work/p1.mem2") runner $(mailbox_of "$work/pw.mem2")"
fi

# ---- savestates in the engine: a state saved and loaded around every frame
# changes nothing; the state after frame 90, saved by one process and
# loaded by another into a project whose movie is the rest of the first's,
# ends where the straight run ends (MEM2 and the picture). (chimera-run's
# --save-state F is taken after frame F counted from 0, so F=89 is the
# machine after 90 frames; a loaded state's movie starts at its first line.)
# The control: the state after frame 90 of a run with nothing pressed,
# loaded the same way, ends elsewhere - the state, not the movie, is what the
# second process started from.
run_project pad p2 --frames $pf --rerecord --dump "MEM2=$work/p2.mem2" --final-screenshot "$work/p2.tga"
run_project pad p3 --frames 90 --save-state "89=$work/p3.state"
tail -n +91 "$work/pad.movie" > "$work/rest.movie"
new_project rest "$hb/pad.rpx" "$work/rest.movie" '{"cpu_mode": "interpreter", "console_fonts": "standin", "mii_resource": "none"}'
run_project rest p4 --state "$work/p3.state" --frames $((pf - 90)) --dump "MEM2=$work/p4.mem2" --final-screenshot "$work/p4.tga"
movie /dev/null $pf "$work/none.movie"
new_project none "$hb/pad.rpx" "$work/none.movie" '{"cpu_mode": "interpreter", "console_fonts": "standin", "mii_resource": "none"}'
run_project none p5 --frames 90 --save-state "89=$work/p5.state"
run_project rest p6 --state "$work/p5.state" --frames $((pf - 90)) --dump "MEM2=$work/p6.mem2"
pic_s1="$(python3 "$here/tests/picture.py" "$work/p1.tga" 2> /dev/null)"
pic_r="$(python3 "$here/tests/picture.py" "$work/p2.tga" 2> /dev/null)"
pic_s="$(python3 "$here/tests/picture.py" "$work/p4.tga" 2> /dev/null)"
if same_file "$work/p1.mem2" "$work/p2.mem2" && [ -n "$pic_r" ] && [ "$pic_r" = "$pic_s1" ] &&
	same_file "$work/p1.mem2" "$work/p4.mem2" && [ "$pic_s" = "$pic_s1" ] &&
	[ -s "$work/p6.mem2" ] && ! cmp -s "$work/p1.mem2" "$work/p6.mem2"; then
	report "engine:states" PASS "a state around every frame, and the state after frame 90 carried into another process, end with the straight run's MEM2 and picture; another run's state ends elsewhere"
else
	report "engine:states" FAIL "rerecord MEM2 $(cmp "$work/p1.mem2" "$work/p2.mem2" 2>&1 | head -1) picture [$pic_r] want [$pic_s1]; carried $(meta p4 status) $(meta p4 detail) MEM2 $(cmp "$work/p1.mem2" "$work/p4.mem2" 2>&1 | head -1) picture [$pic_s]; control $(meta p6 status) $(cmp -s "$work/p1.mem2" "$work/p6.mem2" && echo the-same || echo differs)"
fi

# ---- what the GPU writes back into the console's memory, through the
# engine (Chimera's own GL host, which keeps a mapped buffer as a copy of
# its own): the readback program's 120 frames, default settings, leave the
# MEM2 of the core's sandbox runner, where the program found every frame's
# own picture (the rectangle's green inside, the frame's clear colour
# outside, none a frame late)
movie /dev/null 120 "$work/rb.movie"
new_project readback "$hb/readback.rpx" "$work/rb.movie" '{"console_fonts": "standin", "mii_resource": "none"}'
run_project readback rb1 --frames 120 --dump "MEM2=$work/rb1.mem2"
run_wbx wrb "$hb/readback.rpx" --frames 120 --every 120 --dump-ram "$work/wrb.ram"
mem2_of "$work/wrb.ram" "$work/wrb.mem2" 2> /dev/null
rec="$(mailbox_of "$work/rb1.mem2")"
rb_frame="$(echo "$rec" | cut -d/ -f1)"
rb_want="16711935/$(( (rb_frame % 200) << 24 | ((rb_frame * 7) % 256) << 16 | 0x80 << 8 | 0xFF ))"
if [ "$(meta rb1 frames)" = 120 ] && same_file "$work/rb1.mem2" "$work/wrb.mem2" && [ "$(echo "$rec" | cut -d/ -f3-4)" = "$rb_want" ] &&
	[ "$(echo "$rec" | cut -d/ -f6)" = 0 ]; then
	report "engine:readback" PASS "120 frames through a project: MEM2 is the sandbox runner's, and the program read every frame's own picture back ($rec)"
else
	report "engine:readback" FAIL "$(meta rb1 status) $(meta rb1 detail); MEM2 $(cmp "$work/rb1.mem2" "$work/wrb.mem2" 2>&1 | head -1); record [$rec], inside/outside want [$rb_want]"
fi

# ---- the state history (greenzone) kept by one process and sought into by
# another, as reopening a project does (chimera-run --history-out, then
# --history-in --seek 150 --stop-at-seek), with the default settings (the
# recompiler): the machine at frame 150 and its picture are the straight
# run's. Controls: the straight run's frame 100 is another picture, and a
# history kept from a run with nothing pressed, sought into the same way,
# gives another machine - the history's states are what the seek used.
movie "$work/pad.input" $pf "$work/hist.movie"
new_project hist "$hb/pad.rpx" "$work/hist.movie" '{"console_fonts": "standin", "mii_resource": "none"}'
run_project hist h1 --frames $pf --history-out "$work/h1.hist"
run_project hist h2 --frames $pf --history-in "$work/h1.hist" --seek 150 --stop-at-seek \
	--dump "MEM2=$work/h2.mem2" --final-screenshot "$work/h2.tga"
run_project hist h3 --frames 150 --dump "MEM2=$work/h3.mem2" --final-screenshot "$work/h3.tga"
run_project hist h4 --frames 100 --final-screenshot "$work/h4.tga"
movie /dev/null $pf "$work/hnone.movie"
new_project hnone "$hb/pad.rpx" "$work/hnone.movie" '{"console_fonts": "standin", "mii_resource": "none"}'
run_project hnone h5 --frames $pf --history-out "$work/h5.hist"
run_project hist h6 --frames $pf --history-in "$work/h5.hist" --seek 150 --stop-at-seek --dump "MEM2=$work/h6.mem2"
pic_h2="$(python3 "$here/tests/picture.py" "$work/h2.tga" 2> /dev/null)"
pic_h3="$(python3 "$here/tests/picture.py" "$work/h3.tga" 2> /dev/null)"
pic_h4="$(python3 "$here/tests/picture.py" "$work/h4.tga" 2> /dev/null)"
if [ -s "$work/h1.hist" ] && [ "$(meta h2 status)" = OK ] && same_file "$work/h2.mem2" "$work/h3.mem2" &&
	[ -n "$pic_h2" ] && [ "$pic_h2" = "$pic_h3" ] && [ "$pic_h4" != "$pic_h3" ] &&
	[ -s "$work/h6.mem2" ] && ! cmp -s "$work/h6.mem2" "$work/h3.mem2"; then
	report "engine:history" PASS "a history of $(wc -c < "$work/h1.hist") bytes kept by one process, sought into by another: frame 150 is the straight run's, MEM2 and picture; frame 100 is another picture; another run's history gives another machine"
else
	report "engine:history" FAIL "$(meta h2 status) $(meta h2 detail); MEM2 $(cmp "$work/h2.mem2" "$work/h3.mem2" 2>&1 | head -1); picture [$pic_h2] want [$pic_h3], frame 100 [$pic_h4]; other history $(cmp -s "$work/h6.mem2" "$work/h3.mem2" && echo the-same || echo differs)"
fi

# ---- a game in the NUS form through the engine (CEMU_NUS_GAME with its
# add-on content CEMU_NUS_DLC and CEMU_COMMON_KEY; skipped without them): a
# project of the game's zip, the add-on content's in its own slot and the
# key as firmware, default settings, leaves the sandbox runner's MEM2 after
# 600 frames; its state history kept by one process and sought into by
# another (frame 650, after the press at 620 that the other movie lacks) is
# the straight run's, and a history kept from a run with nothing pressed
# gives another machine; the same project given no key, or a wrong one, is
# refused with the reason.
if [ ! -f "$nus_game" ] || [ ! -f "$nus_dlc" ] || [ ! -f "$common_key" ]; then
	report "engine:nus" SKIP "no NUS game, add-on content or key (CEMU_NUS_GAME, CEMU_NUS_DLC, CEMU_COMMON_KEY; never in this repository)"
else
	key="--firmware wiiu-common-key.bin=$common_key"
	printf '620 0x10\n626 0\n' > "$work/nus.input"
	movie "$work/nus.input" 700 "$work/nus.movie"
	movie /dev/null 700 "$work/nus-none.movie"
	mkdir -p "$work/nus"
	cp "$nus_game" "$nus_dlc" "$work/nus/"
	g="$work/nus/$(basename "$nus_game")"; d="$work/nus/$(basename "$nus_dlc")"
	"$runner" --new-project "$work/nus/nus.chimeraProject" "$package" "$g" "$work/nus.movie" --file "dlc=$d" $key \
		--settings '{"console_fonts": "standin", "mii_resource": "none"}' > "$work/nus.new.log" 2>&1
	"$runner" --new-project "$work/nus/none.chimeraProject" "$package" "$g" "$work/nus-none.movie" --file "dlc=$d" $key \
		--settings '{"console_fonts": "standin", "mii_resource": "none"}' > "$work/nus-none.new.log" 2>&1
	nusrun() { tag="$1"; proj="$2"; shift 2; capped "$runner" --project "$work/nus/$proj.chimeraProject" "$package" --gpu --meta "$work/$tag.meta" "$@" > "$work/$tag.log" 2>&1; }
	nusrun n1 nus --frames 600 $key --dump "MEM2=$work/n1.mem2"
	run_wbx wn "$g" --frames 600 --every 600 --cpu recompiler --set console_fonts=standin --key "$common_key" --dlc "$d" \
		--input "$work/nus.input" --dump-ram "$work/wn.ram"
	mem2_of "$work/wn.ram" "$work/wn.mem2" 2> /dev/null
	rm -f "$work/wn.ram"
	nusrun n2 nus --frames 700 --history-out "$work/n2.hist" $key
	nusrun n3 nus --frames 700 --history-in "$work/n2.hist" --seek 650 --stop-at-seek $key --dump "MEM2=$work/n3.mem2" --final-screenshot "$work/n3.tga"
	nusrun n4 nus --frames 650 $key --dump "MEM2=$work/n4.mem2" --final-screenshot "$work/n4.tga"
	nusrun n5 none --frames 700 --history-out "$work/n5.hist" $key
	nusrun n6 nus --frames 700 --history-in "$work/n5.hist" --seek 650 --stop-at-seek $key --dump "MEM2=$work/n6.mem2"
	nusrun n7 nus --frames 10
	head -c 16 /dev/zero > "$work/zero.key"
	nusrun n8 nus --frames 10 --firmware "wiiu-common-key.bin=$work/zero.key"
	pic_n3="$(python3 "$here/tests/picture.py" "$work/n3.tga" 2> /dev/null)"
	pic_n4="$(python3 "$here/tests/picture.py" "$work/n4.tga" 2> /dev/null)"
	if [ "$(meta n1 status)" = OK ] && same_file "$work/n1.mem2" "$work/wn.mem2" && same_file "$work/n3.mem2" "$work/n4.mem2" &&
		[ -n "$pic_n3" ] && [ "$pic_n3" = "$pic_n4" ] && [ -s "$work/n6.mem2" ] && ! cmp -s "$work/n6.mem2" "$work/n4.mem2" &&
		meta n7 detail | grep -q 'common key' && meta n8 detail | grep -q 'could not be read with the common key'; then
		report "engine:nus" PASS "$(basename "$nus_game") with its add-on content: 600 frames leave the sandbox runner's MEM2; a history of $(wc -c < "$work/n2.hist") bytes sought into by another process (frame 650) is the straight run, MEM2 and picture, another run's history another machine; no key and a wrong key refused"
	else
		report "engine:nus" FAIL "run $(meta n1 status) $(meta n1 detail | cut -c1-80); MEM2 $(cmp "$work/n1.mem2" "$work/wn.mem2" 2>&1 | head -1); seek $(cmp "$work/n3.mem2" "$work/n4.mem2" 2>&1 | head -1) picture [$pic_n3] [$pic_n4]; other history $(cmp -s "$work/n6.mem2" "$work/n4.mem2" && echo the-same || echo differs); no key [$(meta n7 detail | cut -c1-60)] wrong key [$(meta n8 detail | cut -c1-60)]"
	fi
	rm -rf "$work/nus" "$work"/n*.mem2 "$work"/wn.mem2 "$work"/n*.hist
fi

# ---- the console's fonts as a project's firmware: with Console Fonts on
# Cemu's copies, and on the console's own (CEMU_CONSOLE_FONTS), the program
# finds the four files byte for byte; a project that wants them and has
# none is refused; a project that names no value gets the stand-in, with no
# file at all; an older project whose "console" meant Cemu's copies is
# refused, with the sentence that says to choose Cemu's copies
fonts_project() { # <name> <value> <folder>: a project of the fonts program with the folder's fonts as firmware, run 30 frames
	fw=""
	for f in CafeCn.ttf CafeKr.ttf CafeStd.ttf CafeTw.ttf; do fw="$fw --firmware $f=$3/$f"; done
	# shellcheck disable=SC2086
	new_project "$1" "$hb/fonts.rpx" "$work/fonts.movie" "{\"cpu_mode\": \"interpreter\", \"mii_resource\": \"none\", \"console_fonts\": \"$2\"}" $fw
	# shellcheck disable=SC2086
	run_project "$1" "$1" --frames 30 --dump "MEM2=$work/$1.mem2" $fw
	got="$(mailbox_of "$work/$1.mem2" | cut -d/ -f2-)"
	printf '%08x/%s\n' "$(echo "$got" | cut -d/ -f1)" "$(echo "$got" | cut -d/ -f2-)" 2> /dev/null
}
movie /dev/null 30 "$work/fonts.movie"
# engine:standin, with no file at all: a project that names no value gets
# the stand-in, and one that wants the console's fonts and has none is
# refused with the reason
new_project fx "$hb/fonts.rpx" "$work/fonts.movie" '{"cpu_mode": "interpreter", "mii_resource": "none", "console_fonts": "console"}'
run_project fx fx --frames 30
new_project fstand "$hb/fonts.rpx" "$work/fonts.movie" '{"cpu_mode": "interpreter", "mii_resource": "none"}'
run_project fstand fstand --frames 30 --dump "MEM2=$work/fstand.mem2"
standin_want="$(python3 "$here/tests/font-hash.py" --standin "$root/extern/cemu/src/resource/CafeDefaultFont.cpp")"
got="$(mailbox_of "$work/fstand.mem2" | cut -d/ -f2-)"
standin_got="$(printf '%08x' "$(echo "$got" | cut -d/ -f1)" 2> /dev/null)/$(echo "$got" | cut -d/ -f2-)"
if [ -n "$standin_want" ] && [ "$standin_got" = "$standin_want" ] && [ "$(meta fx status)" != OK ] && meta fx detail | grep -q 'CafeCn.ttf'; then
	report "engine:standin" PASS "a project naming no value gets the stand-in ($standin_want); wanted and missing, refused: $(meta fx detail | cut -c1-100)"
else
	report "engine:standin" FAIL "stand-in [$standin_got] want [$standin_want] ($(meta fstand status) $(meta fstand detail)); without files: $(meta fx status) $(meta fx detail)"
fi
# engine:fonts, with the files
if [ -z "$fonts_dir" ] || [ ! -f "$fonts_dir/CafeStd.ttf" ]; then
	report "engine:fonts" SKIP "no fonts (set CEMU_FONTS to a folder of Cemu's copies of the four)"
else
	want="$(python3 "$here/tests/font-hash.py" "$fonts_dir")"
	got_cemu="$(fonts_project fcemu cemu "$fonts_dir")"
	console_want="not given"; console_got="not given"
	if [ -n "$console_fonts_dir" ] && [ -f "$console_fonts_dir/CafeStd.ttf" ]; then
		console_want="$(python3 "$here/tests/font-hash.py" "$console_fonts_dir")"
		console_got="$(fonts_project fcons console "$console_fonts_dir")"
	fi
	# a project of the package before 2026-11-04, when "console" meant Cemu's
	# copies: the value now means the console's own files. The engine holds
	# a project to the files it pinned, so it would open with Cemu's copies
	# under the console's own; the core refuses them (owner, 2026-11-04)
	fw=""
	for f in CafeCn.ttf CafeKr.ttf CafeStd.ttf CafeTw.ttf; do fw="$fw --firmware $f=$fonts_dir/$f"; done
	# shellcheck disable=SC2086
	new_project fold "$hb/fonts.rpx" "$work/fonts.movie" '{"cpu_mode": "interpreter", "mii_resource": "none", "console_fonts": "cemu"}' $fw
	python3 - "$work/fold/fold.chimeraProject" "$work/fold.rewritten" <<'PY'
import json, sys, zipfile
path, mark = sys.argv[1], sys.argv[2]
with zipfile.ZipFile(path) as z:
    parts = {name: z.read(name) for name in z.namelist()}
project = json.loads(parts["project.json"])
done = []
def walk(node):
    if isinstance(node, dict):
        for key, value in node.items():
            if key == "console_fonts" and value == "cemu":
                node[key] = "console"
                done.append(key)
            else:
                walk(value)
    elif isinstance(node, list):
        for value in node:
            walk(value)
walk(project)
parts["project.json"] = json.dumps(project, indent=1).encode()
with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as z:
    for name, data in parts.items():
        z.writestr(name, data)
open(mark, "w").write("%d\n" % len(done))
PY
	# shellcheck disable=SC2086
	run_project fold fold --frames 30 $fw
	if [ "$got_cemu" = "$want" ] && [ "$console_got" = "$console_want" ] &&
		[ "$(cat "$work/fold.rewritten" 2> /dev/null)" = 1 ] && [ "$(meta fold status)" != OK ] && meta fold detail | grep -q "CafeCn.ttf is Cemu's copy"; then
		report "engine:fonts" PASS "Cemu's copies ($want) and the console's own ($console_want) as the project's firmware reached the console; an older project's console value with Cemu's copies, refused: $(meta fold detail | cut -c1-100)"
	else
		report "engine:fonts" FAIL "Cemu's copies [$got_cemu] want [$want]; the console's [$console_got] want [$console_want]; older project: $(meta fold status) $(meta fold detail)"
	fi
fi

# ---- the console's Mii resource as a project's firmware (the default,
# the console's): the mii program finds the four files byte for byte; a
# project that wants them and is given none is refused with the reason
if [ -z "$mii_dir" ] || [ ! -f "$mii_dir/FFLResHigh.dat" ] || [ ! -f "$mii_dir/FFLResMiddleLG.dat" ]; then
	report "engine:mii" SKIP "no Mii resource files (set CEMU_MII to a folder of the four; never in this repository)"
else
	want="$(python3 "$here/tests/mii-hash.py" "$mii_dir")"
	mii_fw=""
	for f in FFLResHigh.dat FFLResHighLG.dat FFLResMiddle.dat FFLResMiddleLG.dat; do mii_fw="$mii_fw --firmware $f=$mii_dir/$f"; done
	movie /dev/null 30 "$work/mii.movie"
	# shellcheck disable=SC2086
	new_project mii "$hb/mii.wua" "$work/mii.movie" '{"cpu_mode": "interpreter", "console_fonts": "standin"}' $mii_fw
	# shellcheck disable=SC2086
	run_project mii mi --frames 30 --dump "MEM2=$work/mi.mem2" $mii_fw
	run_project mii mx --frames 30
	got="$(mailbox_of "$work/mi.mem2" | cut -d/ -f2-)"
	got_hex="$(printf '%08x' "$(echo "$got" | cut -d/ -f1)" 2> /dev/null)/$(echo "$got" | cut -d/ -f2-)"
	if [ "$got_hex" = "$want" ] && [ "$(meta mx status)" != OK ] && meta mx detail | grep -q 'Mii'; then
		report "engine:mii" PASS "the four Mii resource files as the project's firmware reached the console ($want); without them the project is refused: $(meta mx detail | cut -c1-100)"
	else
		report "engine:mii" FAIL "files [$got_hex] want [$want] ($(meta mi status) $(meta mi detail)); without them: $(meta mx status) $(meta mx detail)"
	fi
fi

# ---- a Wii U archive as a project's game: mounted as a title, it runs as
# the program on its own does (the program's record at frame 60)
movie /dev/null 60 "$work/wua.movie"
new_project wua "$hb/frames.wua" "$work/wua.movie" '{"cpu_mode": "interpreter", "console_fonts": "standin", "mii_resource": "none"}'
run_project wua a1 --frames 60 --dump "MEM2=$work/a1.mem2"
run_wbx wa "$hb/frames.rpx" --frames 60 --every 60 --dump-ram "$work/wa.ram"
mem2_of "$work/wa.ram" "$work/wa.mem2" 2> /dev/null
if [ "$(meta a1 frames)" = 60 ] && [ "$(mailbox_of "$work/a1.mem2")" != none ] && [ "$(mailbox_of "$work/a1.mem2")" = "$(mailbox_of "$work/wa.mem2")" ]; then
	report "engine:archive" PASS "frames.wua as a project's game: the program's record at frame 60 ($(mailbox_of "$work/a1.mem2")) is the .rpx's"
else
	report "engine:archive" FAIL "$(meta a1 status) $(meta a1 detail): record [$(mailbox_of "$work/a1.mem2")] want [$(mailbox_of "$work/wa.mem2")]"
fi

# ---- save data through the engine: Export Save Data's files (chimera-run
# --export-savedata) after 120 frames of the saves program, and a project
# whose save data slot holds a zip of them reads them at start
movie /dev/null 120 "$work/saves.movie"
new_project saves "$hb/saves.wua" "$work/saves.movie" '{"cpu_mode": "interpreter", "console_fonts": "standin", "mii_resource": "none"}'
run_project saves e1 --frames 120 --export-savedata "$work/e1-saves"
( cd "$work/e1-saves" 2> /dev/null && python3 -c "import os, zipfile; z = zipfile.ZipFile('../e1-saves.zip', 'w', zipfile.ZIP_DEFLATED); [z.write(os.path.join(r, n)[2:], os.path.join(r, n)[2:]) for r, d, f in sorted(os.walk('.')) for n in sorted(f)]; z.close()" )
mkdir -p "$work/saves2"; cp "$hb/saves.wua" "$work/e1-saves.zip" "$work/saves2/"
movie /dev/null 60 "$work/saves2.movie"
"$runner" --new-project "$work/saves2/saves2.chimeraProject" "$package" "$work/saves2/saves.wua" "$work/saves2.movie" \
	--file "savedata=$work/saves2/e1-saves.zip" --settings '{"cpu_mode": "interpreter", "console_fonts": "standin", "mii_resource": "none"}' > "$work/saves2.new.log" 2>&1
run_project saves2 e2 --frames 60 --dump "MEM2=$work/e2.mem2"
written="$(od -An -tx1 "$work/e1-saves/usr/save/00050000/f1c3e100/user/80000001/chimera.sav" 2> /dev/null | tr -d ' \n')"
read_back="$(mailbox_of "$work/e2.mem2" | cut -d/ -f3)"
if [ "$written" = "43484d5300000077" ] && [ "$read_back" = 119 ]; then
	report "engine:savedata" PASS "Export Save Data gave chimera.sav ($written); a project with the zip in its save data slot read 119 at start"
else
	report "engine:savedata" FAIL "exported [$written] ($(meta e1 status) $(meta e1 detail)); read back [$read_back] ($(meta e2 status) $(meta e2 detail) $(tail -1 "$work/saves2.new.log"))"
fi

echo "$ok passed, $failed failed, $skipped skipped"
[ "$failed" -eq 0 ]
