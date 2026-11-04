#!/bin/sh
# The core's test script. Each test says what it compared. It must end with
# "0 failed", and a SKIP is not a pass.
#
#   waterbox/run-gate.sh
#
# Needs waterbox/build-native.sh and waterbox/build-homebrew.sh to have run;
# the sandbox's tests need waterbox/build-guest.sh. The tests run the
# homebrew programs of tests/homebrew; no game is needed. The files no
# runner has (fonts, the Mii resource, a game and the common key) are named
# by variables, below; without them firmware:fonts, firmware:mii and
# nus:game are skipped. texts:chimera needs CHIMERA_ROOT.
set -u
here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/.." && pwd)"
run="$root/build/native/driver/run-native"
wbx="$root/build/wbx"
frames_rpx="$root/build/homebrew/frames.rpx"
pad_rpx="$root/build/homebrew/pad.rpx"
fonts_rpx="$root/build/homebrew/fonts.rpx"
draw_rpx="$root/build/homebrew/draw.rpx"
readback_rpx="$root/build/homebrew/readback.rpx"
cores_rpx="$root/build/homebrew/cores.rpx"
frames_wua="$root/build/homebrew/frames.wua"
frames_wuhb="$root/build/homebrew/frames.wuhb"
saves_wua="$root/build/homebrew/saves.wua"
profiled_wua="$root/build/homebrew/profiled.wua"
# What no runner has and this repository never holds, named by the person
# who runs the tests; a test that needs one it is not given says SKIP:
# - CEMU_FONTS: a folder of the console's four shared fonts as upstream Cemu
#   ships them (CafeCn.ttf, CafeKr.ttf, CafeStd.ttf, CafeTw.ttf);
# - CEMU_CONSOLE_FONTS: a folder of the same four as a console has them
#   (its system title 0005001b-10042400);
# - CEMU_MII: a folder of the console's Mii resource (the four files of its
#   system title 0005001b-10056000);
# - CEMU_NUS_GAME: a zip of a game's NUS folder (New Super Mario Bros. U's
#   was used), and CEMU_COMMON_KEY: a file of the Wii U common key.
fonts_dir="${CEMU_FONTS:-}"
console_fonts_dir="${CEMU_CONSOLE_FONTS:-}"
nus_game="${CEMU_NUS_GAME:-}"
mii_dir="${CEMU_MII:-}"
common_key="${CEMU_COMMON_KEY:-}"
mii_wua="$root/build/homebrew/mii.wua"
storage_wua="$root/build/homebrew/storage.wua"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
ok=0; failed=0; skipped=0
report() { # <test> <PASS|FAIL|SKIP> <what>
	printf '%-22s %-6s %s\n' "$1" "$2" "$3"
	case "$2" in PASS) ok=$((ok + 1)) ;; FAIL) failed=$((failed + 1)) ;; *) skipped=$((skipped + 1)) ;; esac
}
# what a run ended with
last() { tail -1 "$1" 2> /dev/null; }
native() { # <name> <program> <options...>: a native run, its lines in $work/<name>.out
	name="$1"; prog="$2"; shift 2
	"$run" "$prog" --work "$work/$name-files" "$@" > "$work/$name.out" 2> "$work/$name.err"
}
# a run of the sandbox under a memory cap, where the user's systemd can
# make one (a CI runner has none: there it runs without)
capped() {
	if [ "$can_cap" = 1 ]; then systemd-run --user --scope --quiet -p MemoryMax=12G -p MemorySwapMax=0 "$@"; else "$@"; fi
}
can_cap=0
command -v systemd-run > /dev/null 2>&1 && systemd-run --user --scope --quiet true 2> /dev/null && can_cap=1
sandbox() { # <name> <program> <options...>
	name="$1"; prog="$2"; shift 2
	capped "$wbx/run-wbx" "$wbx/core.wbx" "$prog" --work "$work/$name-files" "$@" > "$work/$name.out" 2> "$work/$name.err"
}
# a run that went the whole way and saw the program run
whole() { [ -s "$1" ] && grep -q "^end frames=$2 .* mailbox=[1-9]" "$1"; }

frames="${CEMU_GATE_FRAMES:-300}"
have_native=0; have_sandbox=0
[ -x "$run" ] && [ -f "$frames_rpx" ] && [ -f "$pad_rpx" ] && [ -f "$fonts_rpx" ] && [ -f "$draw_rpx" ] && have_native=1
[ -x "$wbx/run-wbx" ] && [ -f "$wbx/core.wbx" ] && [ "$have_native" = 1 ] && have_sandbox=1
have_fonts=0
[ -n "$fonts_dir" ] && [ -f "$fonts_dir/CafeCn.ttf" ] && [ -f "$fonts_dir/CafeKr.ttf" ] && [ -f "$fonts_dir/CafeStd.ttf" ] && [ -f "$fonts_dir/CafeTw.ttf" ] && have_fonts=1
why_native="needs build/native/driver/run-native and build/homebrew/{frames,pad,fonts,draw}.rpx: run waterbox/build-native.sh and waterbox/build-homebrew.sh"
why_sandbox="needs build/wbx/core.wbx: run waterbox/build-guest.sh (and build/homebrew/*.rpx)"
# A field of a line: field <file> <frame> <name>, e.g. field a.out 60 mailbox
field() { grep "^frame $2 " "$1" | tr ' ' '\n' | sed -n "s/^$3=//p"; }
# the picture's size at a frame
size() { grep "^frame $2 " "$1" | tr ' ' '\n' | grep -x '[0-9]*x[0-9]*'; }
# the mailbox's Nth value (1: the program's frame, 2: its hash, 3..6: its own)
mailbox() { field "$1" "$2" mailbox | cut -d/ -f"$3"; }

# One thread at a time on the machine's clock: two runs of the program are
# one run - the same clock, the same switches between tasks, the same
# mailbox, the same console memory and the same picture at every 60th
# frame. A run that did not end, or in which the program did nothing, is
# refused: "the same" could otherwise be two runs that both did nothing.
if [ "$have_native" = 0 ]; then
	report "clock:repeats" SKIP "$why_native"
else
	native a "$frames_rpx" --frames "$frames" --every 60
	native b "$frames_rpx" --frames "$frames" --every 60
	if whole "$work/a.out" "$frames" && cmp -s "$work/a.out" "$work/b.out"; then
		report "clock:repeats" PASS "interpreter, $frames frames twice: $(last "$work/a.out")"
	else
		report "clock:repeats" FAIL "first [$(last "$work/a.out")] second [$(last "$work/b.out")] $(tail -1 "$work/a.err")"
	fi
fi

# The machine does not notice the host: a run in which the host stalls for
# 300 ms in the middle is the run without. The control: with the test knob
# CHIMERA_TEST_HOST_CLOCK the console's time also counts the host's (as
# upstream's does), and then the same stall makes another run - so the
# comparison can tell.
if [ "$have_native" = 0 ]; then
	report "clock:stall" SKIP "$why_native"
elif ! whole "$work/a.out" "$frames"; then
	report "clock:stall" SKIP "no run to hold it to (clock:repeats did not run)"
else
	half=$((frames / 2))
	native s "$frames_rpx" --frames "$frames" --every 60 --stall "$half,300"
	CHIMERA_TEST_HOST_CLOCK=1 native h1 "$frames_rpx" --frames "$frames" --every 60
	CHIMERA_TEST_HOST_CLOCK=1 native h2 "$frames_rpx" --frames "$frames" --every 60 --stall "$half,300"
	if cmp -s "$work/a.out" "$work/s.out" && whole "$work/h1.out" "$frames" && ! cmp -s "$work/h1.out" "$work/h2.out"; then
		report "clock:stall" PASS "a 300 ms stall of the host at frame $half changes nothing; on the host's clock it changes the run"
	else
		report "clock:stall" FAIL "straight [$(last "$work/a.out")] stalled [$(last "$work/s.out")]; host clock [$(last "$work/h1.out")] host clock stalled [$(last "$work/h2.out")]"
	fi
fi

# The recompiler is as repeatable as the interpreter (it compiles a function
# where it is first met, not on a thread of its own), and is another
# machine than the interpreter.
if [ "$have_native" = 0 ]; then
	report "clock:recompiler" SKIP "$why_native"
else
	native r1 "$frames_rpx" --frames "$frames" --every 60 --cpu recompiler
	native r2 "$frames_rpx" --frames "$frames" --every 60 --cpu recompiler
	if whole "$work/r1.out" "$frames" && cmp -s "$work/r1.out" "$work/r2.out" && ! cmp -s "$work/a.out" "$work/r1.out"; then
		report "clock:recompiler" PASS "recompiler, $frames frames twice: $(last "$work/r1.out")"
	else
		report "clock:recompiler" FAIL "first [$(last "$work/r1.out")] second [$(last "$work/r2.out")] $(tail -1 "$work/r1.err")"
	fi
fi

# The sandbox is the native run: the same lines at every 60th frame, the
# picture included, from the same sources built by the same compiler. The
# control: a sandboxed run whose calendar starts a day later is another run
# (the program keeps the console's time in its memory).
if [ "$have_sandbox" = 0 ]; then
	report "sandbox:native" SKIP "$why_sandbox"
elif ! whole "$work/a.out" "$frames"; then
	report "sandbox:native" SKIP "no native run to hold it to (clock:repeats did not run)"
else
	sandbox w "$frames_rpx" --frames "$frames" --every 60
	sandbox wday "$frames_rpx" --frames 60 --every 60 --clock-start 1495975468
	if cmp -s "$work/a.out" "$work/w.out" && whole "$work/wday.out" 60 && [ "$(sed -n 1p "$work/wday.out")" != "$(sed -n 1p "$work/w.out")" ]; then
		report "sandbox:native" PASS "$frames frames in the sandbox are the native run's: $(last "$work/w.out")"
	else
		report "sandbox:native" FAIL "native [$(last "$work/a.out")] sandbox [$(last "$work/w.out")] $(tail -1 "$work/w.err")"
	fi
fi

if [ "$have_sandbox" = 0 ]; then
	report "sandbox:recompiler" SKIP "$why_sandbox"
elif ! whole "$work/r1.out" "$frames"; then
	report "sandbox:recompiler" SKIP "no native run to hold it to (clock:recompiler did not run)"
else
	sandbox wr "$frames_rpx" --frames "$frames" --every 60 --cpu recompiler
	if cmp -s "$work/r1.out" "$work/wr.out"; then
		report "sandbox:recompiler" PASS "$frames frames with the recompiler in the sandbox are the native run's"
	else
		report "sandbox:recompiler" FAIL "native [$(last "$work/r1.out")] sandbox [$(last "$work/wr.out")] $(tail -1 "$work/wr.err")"
	fi
fi

# The GamePad: the pad program keeps what VPADRead gave it (tests/homebrew/
# pad). A script presses from frame 30 Up, Down, Left, Right, A, X, R, ZL,
# Minus and the right stick's click; from 60 B, Y, L, ZR, Plus, Home and the
# left stick's click; from 90 it moves the sticks (left half right and full
# down, right full up); from 120 it touches the middle of the GamePad's
# screen; at 150 it lets go. What the console read must be exactly what was
# pressed: the hold word in VPAD's bits (every button once, each its own
# bit; the sticks' directions 0x20000000, 0x08000000, 0x01000000), the
# sticks (50, -100, 0, 100 hundredths) and the touch point in the panel's
# own units (x 2033, y 2054: the middle across, 232 of the GamePad's 480
# lines down, as the picture shows it under the TV).
input_script="$work/pad.input"
cat > "$input_script" <<'INPUT'
# frame mask [left x, left y, right x, right y, touch x, touch y]
30 0x1265F
60 0xD9A0
90 0 16384 -32768 0 32767
120 0x20000 0 0 0 0 32768 52000
150 0
INPUT
if [ "$have_native" = 0 ]; then
	report "input:gamepad" SKIP "$why_native"
else
	native i1 "$pad_rpx" --frames 180 --every 30 --input "$input_script"
	got="$(mailbox "$work/i1.out" 30 3) $(mailbox "$work/i1.out" 60 3) $(mailbox "$work/i1.out" 90 3)/$(mailbox "$work/i1.out" 90 6) $(mailbox "$work/i1.out" 120 5) $(mailbox "$work/i1.out" 180 3)/$(mailbox "$work/i1.out" 180 5)"
	want="$((0x2AF94)) $((0x4506A)) $((0x29000000))/$((0x329C0064)) $((0x07F10806)) 0/0"
	if whole "$work/i1.out" 180 && [ "$got" = "$want" ]; then
		report "input:gamepad" PASS "buttons, sticks and touch reached the console as pressed: $got"
	else
		report "input:gamepad" FAIL "read [$got] pressed [$want] $(tail -1 "$work/i1.err")"
	fi
fi

# Which screens the picture shows: the TV alone (1280x720, and the touch
# screen cannot be touched), the GamePad alone (854x480, the same pointer is
# another point of the panel: 909 down, not 2054).
if [ "$have_native" = 0 ]; then
	report "input:screens" SKIP "$why_native"
else
	native it "$pad_rpx" --frames 120 --every 30 --input "$input_script" --set screens=tv
	native ig "$pad_rpx" --frames 120 --every 30 --input "$input_script" --set screens=gamepad
	got="$(size "$work/it.out" 120)/$(mailbox "$work/it.out" 120 5) $(size "$work/ig.out" 120)/$(mailbox "$work/ig.out" 120 5)"
	want="1280x720/0 854x480/$((0x07F1038D))"
	if [ "$got" = "$want" ]; then
		report "input:screens" PASS "TV alone and GamePad alone: $got"
	else
		report "input:screens" FAIL "got [$got] want [$want] $(tail -1 "$work/ig.err")"
	fi
fi

# The sound: what the program plays reaches the frame's sound, at the
# console's 48 kHz - 600 frames hold 600 x 48000 x 1001 / 60000 = 480480
# pairs, give or take the 576 of one of the sound library's blocks - and it
# follows the input (A raises the wave an octave: frames 31 to 60 sound
# otherwise than frames 61 to 90, which have the length of frames 151 to 180
# and sound as they do).
if [ "$have_native" = 0 ]; then
	report "sound:tv" SKIP "$why_native"
else
	native snd "$pad_rpx" --frames 600 --every 30
	pairs=0
	for f in $(seq 30 30 600); do pairs=$((pairs + $(field "$work/snd.out" "$f" audio | cut -d/ -f1))); done
	high="$(field "$work/i1.out" 60 audio)"; low="$(field "$work/i1.out" 90 audio)"; low2="$(field "$work/i1.out" 180 audio)"
	if whole "$work/snd.out" 600 && [ "$pairs" -ge $((480480 - 576)) ] && [ "$pairs" -le $((480480 + 576)) ] &&
		[ "$high" != "$low" ] && [ "$low" = "$low2" ] && [ "${low%%/*}" -gt 0 ]; then
		report "sound:tv" PASS "$pairs pairs in 600 frames (480480 expected); A changes the sound"
	else
		report "sound:tv" FAIL "$pairs pairs in 600 frames; with A [$high], without [$low] [$low2]"
	fi
fi

# Lag frames: the pad program does not read the GamePad every tenth pass of
# its loop, and a pass is a little shorter than a frame, so about one frame
# in ten (55 to 60 of 600) is a lag frame, and every thirty frames have some;
# the frames program reads it every pass, and has none.
if [ "$have_native" = 0 ]; then
	report "lag:counted" SKIP "$why_native"
else
	lags="$(for f in $(seq 30 30 600); do field "$work/snd.out" "$f" lag; done)"
	total=0; least=99
	for l in $lags; do total=$((total + l)); [ "$l" -lt "$least" ] && least=$l; done
	none="$(grep '^frame ' "$work/a.out" | tr ' ' '\n' | sed -n 's/^lag=//p' | sort -u | tr '\n' ' ')"
	if [ "$total" -ge 50 ] && [ "$total" -le 65 ] && [ "$least" -ge 1 ] && [ "$none" = "0 " ]; then
		report "lag:counted" PASS "the pad program: $total lag frames in 600, some in every 30; the frames program: none"
	else
		report "lag:counted" FAIL "pad program: $total in 600, least $least in 30 [$(echo $lags)]; frames program [$none]"
	fi
fi

# The sandbox gives the GamePad, the sound and the lag frames the native run
# gave: the pad program's lines with the script, picture included.
if [ "$have_sandbox" = 0 ]; then
	report "input:sandbox" SKIP "$why_sandbox"
elif ! whole "$work/i1.out" 180; then
	report "input:sandbox" SKIP "no native run to hold it to (input:gamepad did not run)"
else
	sandbox wi "$pad_rpx" --frames 180 --every 30 --input "$input_script"
	if cmp -s "$work/i1.out" "$work/wi.out"; then
		report "input:sandbox" PASS "180 frames of the pad program with the script in the sandbox are the native run's"
	else
		report "input:sandbox" FAIL "native [$(last "$work/i1.out")] sandbox [$(last "$work/wi.out")] $(tail -1 "$work/wi.err")"
	fi
fi

# The console's shared fonts: the fonts program hashes the four fonts the
# console gives it (OSGetSharedData). The expected hashes are computed from
# the files (waterbox/tests/font-hash.py).
#
# fonts:standin, with no file at all: with the setting on the stand-in, the
# default, each font is Cemu's own stand-in (Lato, from Cemu's source),
# natively and in the sandbox; with the setting on the console's and no
# font given, the core refuses to start and says which is missing.
if [ "$have_native" = 0 ]; then
	report "fonts:standin" SKIP "$why_native"
else
	want_standin="$(python3 "$here/tests/font-hash.py" --standin "$root/extern/cemu/src/resource/CafeDefaultFont.cpp")"
	native fs "$fonts_rpx" --frames 30 --every 30
	native fx "$fonts_rpx" --frames 30 --every 30 --set console_fonts=console
	got_standin="$(field "$work/fs.out" 30 mailbox | cut -d/ -f2-)"
	refused="$(grep -a -c 'Init failed: error.no_font.CafeCn.ttf' "$work/fx.err")"
	sandboxed="not run"
	if [ "$have_sandbox" = 1 ]; then
		sandbox wfs "$fonts_rpx" --frames 30 --every 30
		cmp -s "$work/fs.out" "$work/wfs.out" && sandboxed="the same" || sandboxed="different"
	fi
	if [ -n "$want_standin" ] && [ "$got_standin" = "$want_standin" ] && [ "$refused" = 1 ] && { [ "$have_sandbox" = 0 ] || [ "$sandboxed" = "the same" ]; }; then
		report "fonts:standin" PASS "the stand-in by default ($want_standin), a refusal when the console's are wanted and none is given; sandbox: $sandboxed"
	else
		report "fonts:standin" FAIL "stand-in [$got_standin] want [$want_standin]; refused $refused; sandbox $sandboxed"
	fi
fi

# firmware:fonts, with the files (Nintendo's, never in this repository;
# CEMU_FONTS names a folder of Cemu's copies, CafeCn.ttf, CafeKr.ttf,
# CafeStd.ttf and CafeTw.ttf, and CEMU_CONSOLE_FONTS one of the console's
# own, the four files of its system title 0005001b-10042400): with Cemu's
# copies given (Console Fonts on Cemu's copies), the console has those
# files, byte for byte, natively and in the sandbox, and so it has the
# console's own when they are given (Console Fonts on the console's); with
# the setting on the console's and Cemu's copies given (an older
# project's), the core refuses them.
if [ "$have_native" = 0 ]; then
	report "firmware:fonts" SKIP "$why_native"
elif [ "$have_fonts" = 0 ]; then
	report "firmware:fonts" SKIP "no fonts (set CEMU_FONTS to a folder with Cemu's CafeCn.ttf, CafeKr.ttf, CafeStd.ttf, CafeTw.ttf)"
else
	want_fonts="$(python3 "$here/tests/font-hash.py" "$fonts_dir")"
	native fo "$fonts_rpx" --frames 30 --every 30 --fonts "$fonts_dir"
	native fk "$fonts_rpx" --frames 30 --every 30 --fonts "$fonts_dir" --set console_fonts=console
	console_got="not given"; console_want="not given"
	if [ -n "$console_fonts_dir" ] && [ -f "$console_fonts_dir/CafeStd.ttf" ]; then
		console_want="$(python3 "$here/tests/font-hash.py" "$console_fonts_dir")"
		native fc "$fonts_rpx" --frames 30 --every 30 --fonts "$console_fonts_dir" --set console_fonts=console
		console_got="$(field "$work/fc.out" 30 mailbox | cut -d/ -f2-)"
	fi
	got_fonts="$(field "$work/fo.out" 30 mailbox | cut -d/ -f2-)"
	refused_copies="$(grep -a -c 'Init failed: error.font_is_cemu_copy.CafeCn.ttf' "$work/fk.err")"
	sandboxed="not run"
	if [ "$have_sandbox" = 1 ]; then
		sandbox wfo "$fonts_rpx" --frames 30 --every 30 --fonts "$fonts_dir"
		cmp -s "$work/fo.out" "$work/wfo.out" && sandboxed="the same" || sandboxed="different"
	fi
	if [ "$got_fonts" = "$want_fonts" ] && [ "$console_got" = "$console_want" ] && [ "$refused_copies" = 1 ] &&
		{ [ "$have_sandbox" = 0 ] || [ "$sandboxed" = "the same" ]; }; then
		report "firmware:fonts" PASS "Cemu's copies reached the console byte for byte ($want_fonts), the console's own too ($console_want), and Cemu's copies given for the console's own were refused; sandbox: $sandboxed"
	else
		report "firmware:fonts" FAIL "Cemu's copies [$got_fonts] want [$want_fonts]; the console's [$console_got] want [$console_want]; Cemu's copies as the console's refused $refused_copies; sandbox $sandboxed"
	fi
fi

# The ways a game is given: the frames program as a title in a Wii U archive
# (.wua) and in a homebrew bundle (.wuhb), both made by
# waterbox/build-test-titles.sh. Each is mounted as a title (Cemu's log says
# so, with its title id) and runs as the program on its own does: the same
# mailbox and the same picture at frame 60. The archive in the sandbox is
# the native run. A file that is no title and no program is refused, with
# the reason's key.
if [ "$have_native" = 0 ]; then
	report "formats:titles" SKIP "$why_native"
elif [ ! -f "$frames_wua" ] || [ ! -f "$frames_wuhb" ]; then
	report "formats:titles" SKIP "needs build/homebrew/frames.wua and frames.wuhb: run waterbox/build-test-titles.sh"
else
	native ta "$frames_wua" --frames 60 --every 60 --log
	native tb "$frames_wuhb" --frames 60 --every 60 --log
	native tr "$frames_rpx" --frames 60 --every 60
	printf 'not a title\n' > "$work/junk.wua"
	native tj "$work/junk.wua" --frames 1 --every 1
	want="$(field "$work/tr.out" 60 mailbox) $(field "$work/tr.out" 60 video)"
	got_a="$(field "$work/ta.out" 60 mailbox) $(field "$work/ta.out" 60 video)"
	got_b="$(field "$work/tb.out" 60 mailbox) $(field "$work/tb.out" 60 video)"
	mounted="$(grep -a -c 'Mounting title 00050000f1c3e000' "$work/ta.err")/$(grep -a -c 'Mounting title 0005000f' "$work/tb.err")"
	refused="$(grep -a -c 'Init failed: error.not_a_title.junk.wua' "$work/tj.err")"
	sandboxed="not run"
	if [ "$have_sandbox" = 1 ]; then
		sandbox wta "$frames_wua" --frames 60 --every 60
		[ "$(sed -n 1p "$work/wta.out")" = "$(sed -n 1p "$work/ta.out")" ] && sandboxed="the same" || sandboxed="different"
	fi
	if whole "$work/tr.out" 60 && [ "$got_a" = "$want" ] && [ "$got_b" = "$want" ] && [ "$mounted" = "1/1" ] && [ "$refused" = 1 ] &&
		{ [ "$have_sandbox" = 0 ] || [ "$sandboxed" = "the same" ]; }; then
		report "formats:titles" PASS ".wua and .wuhb mounted as titles and run as the .rpx does; a file that is neither is refused; the archive in the sandbox: $sandboxed"
	else
		report "formats:titles" FAIL "rpx [$want] wua [$got_a] wuhb [$got_b] mounted $mounted refused $refused sandbox $sandboxed $(tail -1 "$work/ta.err")"
	fi
fi

# The console's GPU library: the draw program (tests/homebrew/draw) clears a
# colour buffer and draws a triangle, a square as GX2's QUADS from the vertex
# buffer and a square as QUADS through an index buffer, with shaders of its
# own, and copies the buffer to both screens. Two runs are one run (the
# picture every 30th frame included), and the picture of frame 60 is what was
# drawn: the clear colour in a corner (0.1, 0.2, 60/64), red in the triangle,
# green and yellow in the two squares, on the TV and on the GamePad. (A core
# profile has no GL_QUADS: the squares were missing until they were drawn
# as triangles.)
draw_points="10,10 640,380 160,126 1120,594 320,804 640,980 960,1115"
draw_want="1933ef ff0000 00ff00 ffff00 00ff00 ff0000 ffff00"
if [ "$have_native" = 0 ]; then
	report "gx2:draw" SKIP "$why_native"
else
	native d1 "$draw_rpx" --frames 120 --every 30 --screenshot "60=$work/d60.tga"
	native d2 "$draw_rpx" --frames 120 --every 30
	got="$(python3 "$here/tests/pixel.py" "$work/d60.tga" $draw_points 2> /dev/null)"
	if whole "$work/d1.out" 120 && cmp -s "$work/d1.out" "$work/d2.out" && [ "$got" = "$draw_want" ]; then
		report "gx2:draw" PASS "120 frames twice the same; frame 60 shows the clear colour, the triangle and both squares on both screens"
	else
		report "gx2:draw" FAIL "first [$(last "$work/d1.out")] second [$(last "$work/d2.out")]; frame 60 [$got] want [$draw_want] $(tail -1 "$work/d1.err")"
	fi
fi

# Occlusion queries: the draw program asks how many of the triangle's pixels
# were drawn. With the card's count (the default) the answer is the
# triangle's area, 384 by 252 pixels halved (48384), and comes at the same
# point every run (the lines of gx2:draw, which carry it, are the same twice);
# with the setting on fake it is 0x100000 for every query.
if [ "$have_native" = 0 ]; then
	report "gx2:queries" SKIP "$why_native"
elif ! whole "$work/d1.out" 120; then
	report "gx2:queries" SKIP "no run to hold it to (gx2:draw did not run)"
else
	native dq "$draw_rpx" --frames 60 --every 30 --set occlusion_queries=fake
	got="$(mailbox "$work/d1.out" 60 5)/$(mailbox "$work/dq.out" 60 5)"
	if [ "$got" = "48384/$((0x100000))" ]; then
		report "gx2:queries" PASS "the card's count: 48384 pixels, the triangle's area; fake: $((0x100000))"
	else
		report "gx2:queries" FAIL "card/fake [$got] want [48384/$((0x100000))]"
	fi
fi

# The same in the sandbox; and the renderer made again on a new GPU context
# before every frame draws what the straight run draws (shaders, buffers and
# the colour buffer made again from the console's memory). The control: with
# the renderer never told (CHIMERA_TEST_NO_REMAKE) the picture is another.
if [ "$have_native" = 0 ]; then
	report "gx2:remade" SKIP "$why_native"
elif ! whole "$work/d1.out" 120; then
	report "gx2:remade" SKIP "no run to hold it to (gx2:draw did not run)"
else
	native d3 "$draw_rpx" --frames 120 --every 30 --new-context 1
	CHIMERA_TEST_NO_REMAKE=1 native d4 "$draw_rpx" --frames 120 --every 30 --new-context 1
	if cmp -s "$work/d1.out" "$work/d3.out" && ! cmp -s "$work/d1.out" "$work/d4.out"; then
		report "gx2:remade" PASS "120 frames with a new GPU context before each are the run without; untold, the picture is another"
	else
		report "gx2:remade" FAIL "straight [$(last "$work/d1.out")] remade [$(last "$work/d3.out")] untold [$(last "$work/d4.out")] $(tail -1 "$work/d3.err")"
	fi
fi

if [ "$have_sandbox" = 0 ]; then
	report "gx2:sandbox" SKIP "$why_sandbox"
elif ! whole "$work/d1.out" 120; then
	report "gx2:sandbox" SKIP "no native run to hold it to (gx2:draw did not run)"
else
	sandbox wd "$draw_rpx" --frames 120 --every 30
	sandbox wdr "$draw_rpx" --frames 120 --every 30 --rerecord
	sandbox wds1 "$draw_rpx" --frames 60 --every 30 --save-state "$work/draw.state"
	sandbox wds2 "$draw_rpx" --frames 60 --every 30 --state "$work/draw.state"
	if cmp -s "$work/d1.out" "$work/wd.out" && cmp -s "$work/wd.out" "$work/wdr.out" && whole "$work/wds2.out" 120 &&
		[ "$(last "$work/wds2.out")" = "$(last "$work/wd.out")" ]; then
		report "gx2:sandbox" PASS "the sandbox draws the native run's pictures; a state around every frame, and one carried into another process, change nothing"
	else
		report "gx2:sandbox" FAIL "native [$(last "$work/d1.out")] sandbox [$(last "$work/wd.out")] rerecord [$(last "$work/wdr.out")] carried [$(last "$work/wds2.out")] $(tail -1 "$work/wd.err")"
	fi
fi

# What the GPU writes back into the console's memory: the readback program
# (tests/homebrew/readback) draws into a small buffer in linear layout, which
# Cemu copies back from the host's GPU, and reads it with the CPU after
# GX2DrawDone. Every frame it reads that frame's picture: the green
# rectangle inside (0x00FF00FF) and the frame's clear colour outside, none a
# frame late. Two runs are one run; the sandbox is the native run, with a
# state around every frame and with one carried into another process.
# (It found the idle wait taking "no alarm yet" for an alarm due at once:
# the program goes idle before it makes any alarm, and the clock stopped.)
if [ "$have_native" = 0 ]; then
	report "gx2:readback" SKIP "$why_native"
elif [ ! -f "$readback_rpx" ]; then
	report "gx2:readback" SKIP "needs build/homebrew/readback.rpx: run waterbox/build-homebrew.sh"
else
	native rb1 "$readback_rpx" --frames 120 --every 30
	native rb2 "$readback_rpx" --frames 120 --every 30
	f="$(mailbox "$work/rb1.out" 120 1)"
	want="16711935 $(( (f % 200) << 24 | ((f * 7) % 256) << 16 | 0x80 << 8 | 0xFF )) 0"
	got="$(mailbox "$work/rb1.out" 120 3) $(mailbox "$work/rb1.out" 120 4) $(mailbox "$work/rb1.out" 120 6)"
	fresh="$(mailbox "$work/rb1.out" 120 5)"
	sandboxed="not run"
	if [ "$have_sandbox" = 1 ]; then
		sandbox wrb "$readback_rpx" --frames 120 --every 30
		sandbox wrbr "$readback_rpx" --frames 120 --every 30 --rerecord
		sandbox wrbs1 "$readback_rpx" --frames 60 --every 30 --save-state "$work/readback.state"
		sandbox wrbs2 "$readback_rpx" --frames 60 --every 30 --state "$work/readback.state"
		cmp -s "$work/rb1.out" "$work/wrb.out" && cmp -s "$work/wrb.out" "$work/wrbr.out" && whole "$work/wrbs2.out" 120 &&
			[ "$(last "$work/wrbs2.out")" = "$(last "$work/wrb.out")" ] && sandboxed="the same" || sandboxed="different"
	fi
	if whole "$work/rb1.out" 120 && cmp -s "$work/rb1.out" "$work/rb2.out" && [ "$got" = "$want" ] && [ "${fresh:-0}" -ge "$f" ] &&
		{ [ "$have_sandbox" = 0 ] || [ "$sandboxed" = "the same" ]; }; then
		report "gx2:readback" PASS "the CPU read each frame's own picture back from the GPU (to frame $f, none a frame late); twice the same; sandbox, with states: $sandboxed"
	else
		report "gx2:readback" FAIL "inside, outside, late [$got] want [$want], fresh $fresh of $f; sandbox $sandboxed $(tail -1 "$work/rb1.err")"
	fi
fi

# The console's three cores: the cores program (tests/homebrew/cores) pins
# a thread to each core and passes a token round them under a mutex and a
# condition, with sleeps of their own and a periodic alarm of 7 ms. Each
# thread finds itself on its own core (0x210), none elsewhere, all three
# take turns, the alarm fired 285 or 286 times in 2 s; two runs are one,
# with either CPU mode; the sandbox is the native run, with a state around
# every frame and with one carried into another process.
if [ "$have_native" = 0 ]; then
	report "cpu:cores" SKIP "$why_native"
elif [ ! -f "$cores_rpx" ]; then
	report "cpu:cores" SKIP "needs build/homebrew/cores.rpx: run waterbox/build-homebrew.sh"
else
	native co1 "$cores_rpx" --frames 120 --every 30
	native co2 "$cores_rpx" --frames 120 --every 30
	native co3 "$cores_rpx" --frames 120 --every 30 --cpu recompiler
	native co4 "$cores_rpx" --frames 120 --every 30 --cpu recompiler
	cores="$(mailbox "$work/co1.out" 120 4) $(mailbox "$work/co1.out" 120 6)"
	turns="$(mailbox "$work/co1.out" 120 3)"
	alarms="$(mailbox "$work/co1.out" 120 5)"
	sandboxed="not run"
	if [ "$have_sandbox" = 1 ]; then
		sandbox wco "$cores_rpx" --frames 120 --every 30
		sandbox wcor "$cores_rpx" --frames 120 --every 30 --rerecord
		sandbox wcos1 "$cores_rpx" --frames 60 --every 30 --save-state "$work/cores.state"
		sandbox wcos2 "$cores_rpx" --frames 60 --every 30 --state "$work/cores.state"
		cmp -s "$work/co1.out" "$work/wco.out" && cmp -s "$work/wco.out" "$work/wcor.out" && whole "$work/wcos2.out" 120 &&
			[ "$(last "$work/wcos2.out")" = "$(last "$work/wco.out")" ] && sandboxed="the same" || sandboxed="different"
	fi
	if whole "$work/co1.out" 120 && cmp -s "$work/co1.out" "$work/co2.out" && whole "$work/co3.out" 120 && cmp -s "$work/co3.out" "$work/co4.out" &&
		[ "$cores" = "528 0" ] && [ "${turns:-0}" -ge 360 ] && { [ "$alarms" = 285 ] || [ "$alarms" = 286 ]; } &&
		{ [ "$have_sandbox" = 0 ] || [ "$sandboxed" = "the same" ]; }; then
		report "cpu:cores" PASS "each thread on its own core (0x210), $turns turns, $alarms alarms in 2 s; twice the same with each CPU mode; sandbox, with states: $sandboxed"
	else
		report "cpu:cores" FAIL "cores, misplaced [$cores] want [528 0], turns $turns, alarms $alarms; interpreter [$(last "$work/co1.out")] [$(last "$work/co2.out")] recompiler [$(last "$work/co3.out")] [$(last "$work/co4.out")]; sandbox $sandboxed"
	fi
fi

# Save data: the saves program (a title, tests/homebrew/saves, in
# saves.wua) writes chimera.sav into its save folder every 30 frames, the
# frame count after "CHMS"; at start it reads it. Out, the console's saved
# games are the files of its usr/save folder (the program's file holding 119
# after 120 frames); in, a zip of them makes a second run read 119 at start;
# a file that is no zip is refused with the reason's key. The sandbox exports
# the same files and reads the same zip the same way, to the last byte of
# the console's memory.
if [ "$have_native" = 0 ]; then
	report "savedata:round" SKIP "$why_native"
elif [ ! -f "$saves_wua" ]; then
	report "savedata:round" SKIP "needs build/homebrew/saves.wua: run waterbox/build-test-titles.sh"
else
	native sv1 "$saves_wua" --frames 120 --every 60 --savedata-out "$work/sv1"
	file="$work/sv1/usr/save/00050000/f1c3e100/user/80000001/chimera.sav"
	written="$(od -An -tx1 "$file" 2> /dev/null | tr -d ' \n')"
	( cd "$work/sv1" 2> /dev/null && python3 -c "import os, zipfile; z = zipfile.ZipFile('../sv1.zip', 'w', zipfile.ZIP_DEFLATED); [z.write(os.path.join(r, n)[2:], os.path.join(r, n)[2:]) for r, d, f in sorted(os.walk('.')) for n in sorted(f)]; z.close()" )
	native sv2 "$saves_wua" --frames 60 --every 60 --savedata "$work/sv1.zip"
	printf 'not a zip' > "$work/junk-save.zip"
	native sv3 "$saves_wua" --frames 1 --every 1 --savedata "$work/junk-save.zip"
	got="$written $(mailbox "$work/sv2.out" 60 3) $(grep -a -c 'Init failed: error.save_not_zip.junk-save.zip' "$work/sv3.err")"
	want="43484d5300000077 119 1"
	sandboxed="not run"
	if [ "$have_sandbox" = 1 ]; then
		sandbox wsv1 "$saves_wua" --frames 120 --every 60 --savedata-out "$work/wsv1"
		sandbox wsv2 "$saves_wua" --frames 60 --every 60 --savedata "$work/sv1.zip"
		diff -r "$work/sv1" "$work/wsv1" > /dev/null 2>&1 && whole "$work/wsv2.out" 60 && cmp -s "$work/sv2.out" "$work/wsv2.out" \
			&& sandboxed="the same" || sandboxed="different"
	fi
	if [ "$got" = "$want" ] && { [ "$have_sandbox" = 0 ] || [ "$sandboxed" = "the same" ]; }; then
		report "savedata:round" PASS "exported (chimera.sav $written), read back at start of another run (119), a non-zip refused; sandbox: $sandboxed"
	else
		report "savedata:round" FAIL "got [$got] want [$want]; sandbox $sandboxed $(tail -1 "$work/sv2.err")"
	fi
fi

# Cemu's per-game profiles ship in the core and are written into the
# machine's data folder, where Cemu reads the one of the title it runs:
# profiled.wua (the frames program under the title id of Super Mario 3D
# World (USA), whose profile turns accurateShaderMul off) runs with it, and
# frames.wua, a title with no profile, without; the sandbox the same.
if [ "$have_native" = 0 ]; then
	report "profiles:shipped" SKIP "$why_native"
elif [ ! -f "$profiled_wua" ] || [ ! -f "$frames_wua" ]; then
	report "profiles:shipped" SKIP "needs build/homebrew/profiled.wua and frames.wua: run waterbox/build-test-titles.sh"
else
	profile_lines() { # <stderr of a run with --log>: what Cemu said of the profile
		echo "$(grep -a -c 'gameprofile path: gameProfiles/default/0005000010145c00.ini' "$1") $(grep -a -c 'Strict shader mul: false' "$1")"
	}
	native pr1 "$profiled_wua" --frames 2 --every 2 --log
	native pr2 "$frames_wua" --frames 2 --every 2 --log
	got="$(profile_lines "$work/pr1.err") / $(grep -a -c 'gameprofile path:  (not present)' "$work/pr2.err") $(grep -a -c 'Strict shader mul: true' "$work/pr2.err")"
	want="1 1 / 1 1"
	sandboxed="not run"
	if [ "$have_sandbox" = 1 ]; then
		sandbox wpr1 "$profiled_wua" --frames 2 --every 2 --log
		[ "$(profile_lines "$work/wpr1.err")" = "1 1" ] && sandboxed="the same" || sandboxed="different"
	fi
	if [ "$got" = "$want" ] && { [ "$have_sandbox" = 0 ] || [ "$sandboxed" = "the same" ]; }; then
		report "profiles:shipped" PASS "the profiled title read gameProfiles/default/0005000010145c00.ini (strict shader mul off), a title with none kept the defaults; sandbox: $sandboxed"
	else
		report "profiles:shipped" FAIL "got [$got] want [$want]; sandbox $sandboxed"
	fi
fi

# The console's Mii resource as firmware: the mii program (tests/homebrew
# /mii, in mii.wua) reads the four files of the Mii resource title where a
# console keeps them and keeps their sizes and a hash of their bytes. Given
# (CEMU_MII; skipped without it), they reach it byte for byte; with Mii
# Resource set to none it finds none; with the console's and a file
# missing, the project is refused with the file's name. The sandbox finds
# what the native run finds.
if [ "$have_native" = 0 ]; then
	report "firmware:mii" SKIP "$why_native"
elif [ ! -f "$mii_wua" ]; then
	report "firmware:mii" SKIP "needs build/homebrew/mii.wua: run waterbox/build-test-titles.sh"
elif [ -z "$mii_dir" ] || [ ! -f "$mii_dir/FFLResHigh.dat" ] || [ ! -f "$mii_dir/FFLResMiddleLG.dat" ]; then
	report "firmware:mii" SKIP "no Mii resource files (set CEMU_MII to a folder of the four; never in this repository)"
else
	want="$(python3 "$here/tests/mii-hash.py" "$mii_dir")"
	mkdir -p "$work/mii3"
	for f in FFLResHigh.dat FFLResHighLG.dat FFLResMiddle.dat; do ln -s "$mii_dir/$f" "$work/mii3/$f"; done
	native mi1 "$mii_wua" --frames 30 --every 30 --mii "$mii_dir"
	native mi2 "$mii_wua" --frames 30 --every 30
	native mi3 "$mii_wua" --frames 1 --every 1 --mii "$work/mii3"
	got="$(mailbox "$work/mi1.out" 30 2-6)"
	none="$(mailbox "$work/mi2.out" 30 3-6)"
	refused="$(grep -a -c 'Init failed: error.no_mii_resource.FFLResMiddleLG.dat' "$work/mi3.err")"
	sandboxed="not run"
	if [ "$have_sandbox" = 1 ]; then
		sandbox wmi1 "$mii_wua" --frames 30 --every 30 --mii "$mii_dir"
		cmp -s "$work/mi1.out" "$work/wmi1.out" && sandboxed="the same" || sandboxed="different"
	fi
	if [ "$got" = "$want" ] && [ "$none" = "0/0/0/0" ] && [ "$refused" = 1 ] && { [ "$have_sandbox" = 0 ] || [ "$sandboxed" = "the same" ]; }; then
		report "firmware:mii" PASS "the four Mii resource files reached the console byte for byte ($want); none with none; a missing file refused; sandbox: $sandboxed"
	else
		report "firmware:mii" FAIL "got [$got] want [$want]; with none [$none]; refused $refused; sandbox $sandboxed"
	fi
fi

# The console's storage as a new one is set up: what upstream Cemu's window
# makes in it (CemuApp::CreateDefaultMLCFiles) and this core makes at boot.
# The storage program (tests/homebrew/storage, in storage.wua) can make a
# file in the Mii Maker's database folder of each region (where a game's
# Mii library keeps FFL_HDB.dat; without it New Super Mario Bros. U stops
# after its title screen), finds the folders titles are installed in, and
# reads the system's lists of languages and countries as upstream writes
# them (waterbox/tests/storage-lists.py, from upstream's own country
# table). The sandbox finds the same.
if [ "$have_native" = 0 ]; then
	report "storage:defaults" SKIP "$why_native"
elif [ ! -f "$storage_wua" ]; then
	report "storage:defaults" SKIP "needs build/homebrew/storage.wua: run waterbox/build-test-titles.sh"
else
	want="$(python3 "$here/tests/storage-lists.py" "$root/extern/cemu/src/Cemu/ncrypto/ncrypto.cpp")"
	native sto "$storage_wua" --frames 30 --every 30
	got="$(mailbox "$work/sto.out" 30 2-6)"
	sandboxed="not run"
	if [ "$have_sandbox" = 1 ]; then
		sandbox wsto "$storage_wua" --frames 30 --every 30
		cmp -s "$work/sto.out" "$work/wsto.out" && sandboxed="the same" || sandboxed="different"
	fi
	if [ -n "$want" ] && [ "$got" = "$want" ] && { [ "$have_sandbox" = 0 ] || [ "$sandboxed" = "the same" ]; }; then
		report "storage:defaults" PASS "the Mii Maker's three database folders take a file, the title folders are there, the language and country lists are upstream's ($want); sandbox: $sandboxed"
	else
		report "storage:defaults" FAIL "got [$got] want [$want]; sandbox $sandboxed"
	fi
fi

# Games in the NUS form, refused plainly when they cannot be read, with no
# game needed: each a zip made here, with a stand-in key of 16 zero bytes
# where one is needed (only the size of a key is looked at before a title
# is opened). A zip with no title.tmd; a file that is no zip; a NUS zip and
# no key; a key that is not 16 bytes; a large compressed entry; add-on
# content beside a game that is not in the NUS form. Each refused with its
# reason's key, natively and in the sandbox.
if [ "$have_native" = 0 ]; then
	report "nus:refused" SKIP "$why_native"
else
	mkdir -p "$work/nus"
	head -c 16 /dev/zero > "$work/nus/zero.key"
	head -c 15 /dev/zero > "$work/nus/short.key"
	python3 - "$work/nus" <<'PY'
import os, sys, zipfile
d = sys.argv[1]
with zipfile.ZipFile(os.path.join(d, 'notmd.zip'), 'w', zipfile.ZIP_STORED) as z:
    z.writestr('00000000.app', b'x' * 64)
with zipfile.ZipFile(os.path.join(d, 'nus.zip'), 'w', zipfile.ZIP_STORED) as z:
    z.writestr('title.tmd', b'\0' * 64)
    z.writestr('title.tik', b'\0' * 64)
with zipfile.ZipFile(os.path.join(d, 'packed.zip'), 'w', zipfile.ZIP_DEFLATED) as z:
    z.writestr('title.tmd', b'\0' * 64)
    z.writestr('00000001.app', b'\0' * (17 << 20))
open(os.path.join(d, 'junk.zip'), 'wb').write(b'not a zip at all')
PY
	native nr1 "$work/nus/notmd.zip" --frames 1 --every 1 --key "$work/nus/zero.key"
	native nr2 "$work/nus/junk.zip" --frames 1 --every 1 --key "$work/nus/zero.key"
	native nr3 "$work/nus/nus.zip" --frames 1 --every 1
	native nr4 "$work/nus/nus.zip" --frames 1 --every 1 --key "$work/nus/short.key"
	native nr5 "$work/nus/packed.zip" --frames 1 --every 1 --key "$work/nus/zero.key"
	native nr6 "$frames_wua" --frames 1 --every 1 --dlc "$work/nus/nus.zip" --key "$work/nus/zero.key"
	refused() { grep -a -c "Init failed: $2" "$work/$1.err"; }
	got="$(refused nr1 'error.nus_no_tmd.notmd.zip') $(refused nr2 'error.nus_not_zip.junk.zip') $(refused nr3 'error.nus_no_key.nus.zip') $(refused nr4 'error.key_not_a_key.wiiu-common-key.bin') $(refused nr5 'error.nus_entry_compressed.packed.zip.00000001.app') $(refused nr6 'error.dlc_without_nus.nus.zip')"
	want="1 1 1 1 1 1"
	sandboxed="not run"
	if [ "$have_sandbox" = 1 ]; then
		sandbox wnr3 "$work/nus/nus.zip" --frames 1 --every 1
		sandbox wnr5 "$work/nus/packed.zip" --frames 1 --every 1 --key "$work/nus/zero.key"
		[ "$(refused wnr3 'error.nus_no_key.nus.zip') $(refused wnr5 'error.nus_entry_compressed.packed.zip.00000001.app')" = "1 1" ] && sandboxed="the same" || sandboxed="different"
	fi
	if [ "$got" = "$want" ] && { [ "$have_sandbox" = 0 ] || [ "$sandboxed" = "the same" ]; }; then
		report "nus:refused" PASS "no title.tmd, no zip, no key, a key not 16 bytes, a large compressed entry, add-on content beside a .wua: each refused with its reason; sandbox: $sandboxed"
	else
		report "nus:refused" FAIL "got [$got] want [$want]; sandbox $sandboxed; $(tail -1 "$work/nr1.err" | cut -c1-120)"
	fi
fi

# A game in the NUS form (CEMU_NUS_GAME, with CEMU_COMMON_KEY; skipped
# without them): it is read from its zip and runs, two runs are one, the
# sandbox is the native run, and a wrong key (16 zero bytes) is refused
# with its reason (the title's files cannot be read with it).
if [ "$have_native" = 0 ]; then
	report "nus:game" SKIP "$why_native"
elif [ -z "$nus_game" ] || [ -z "$common_key" ] || [ ! -f "$nus_game" ] || [ ! -f "$common_key" ]; then
	report "nus:game" SKIP "no NUS game or no key (set CEMU_NUS_GAME and CEMU_COMMON_KEY; never in this repository)"
else
	mkdir -p "$work/nus"
	head -c 16 /dev/zero > "$work/nus/zero.key"
	game_name="$(basename "$nus_game")"
	native ng1 "$nus_game" --frames 300 --every 100 --set console_fonts=standin --key "$common_key"
	native ng2 "$nus_game" --frames 300 --every 100 --set console_fonts=standin --key "$common_key"
	native ng3 "$nus_game" --frames 1 --every 1 --set console_fonts=standin --key "$work/nus/zero.key"
	sandboxed="not run"
	if [ "$have_sandbox" = 1 ]; then
		sandbox wng "$nus_game" --frames 300 --every 100 --set console_fonts=standin --key "$common_key"
		cmp -s "$work/ng1.out" "$work/wng.out" && sandboxed="the same" || sandboxed="different"
	fi
	pictures="$(sed -n 's/.* video=\([0-9a-f]*\) .*/\1/p' "$work/ng1.out" | sort -u | wc -l)"
	if [ -s "$work/ng1.out" ] && grep -q '^end frames=300 ' "$work/ng1.out" && cmp -s "$work/ng1.out" "$work/ng2.out" && [ "$pictures" -ge 3 ] &&
		[ "$(grep -a -c "Init failed: error.nus_cannot_decrypt.$game_name" "$work/ng3.err")" = 1 ] &&
		{ [ "$have_sandbox" = 0 ] || [ "$sandboxed" = "the same" ]; }; then
		report "nus:game" PASS "$game_name read from its zip: 300 frames twice the same, $pictures pictures; a wrong key refused; sandbox: $sandboxed"
	else
		report "nus:game" FAIL "first [$(last "$work/ng1.out")] second [$(last "$work/ng2.out")] pictures $pictures; wrong key: $(tail -1 "$work/ng3.err" | cut -c1-100); sandbox $sandboxed"
	fi
fi

# Nintendo's keys are not in the core, the package or this repository:
# upstream builds the Wii U common key and the SpotPass, amiibo, NFC and icon
# database keys into the program, and apply-patches.sh takes them out
# (remove-keys.py). waterbox/tests/find-keys.py reads them from upstream's
# own sources and looks for each, as bytes, as hex text and as a C list of
# bytes, in core.wbx, the native runner, every file of the package and
# every file of this repository; this script holds none of them.
if [ ! -d "$root/extern/cemu/.git" ] && [ ! -f "$root/extern/cemu/.git" ]; then
	report "keys:absent" SKIP "no Cemu checkout to read the keys from"
else
	files="$(cd "$root" && git ls-files | grep -v '^extern/' | sed "s|^|$root/|")"
	built=""
	for f in "$wbx/core.wbx" "$run" "$root/build/package/cemu.chimeraCore"; do [ -f "$f" ] && built="$built $f"; done
	# shellcheck disable=SC2086
	found="$(python3 "$here/tests/find-keys.py" $built $files 2>&1)"
	if [ -n "$built" ] && echo "$found" | grep -q ': none$'; then
		report "keys:absent" PASS "$(echo "$found" | sed 's/: none$//'), none holds one (core.wbx, the runner, the package, the repository)"
	else
		report "keys:absent" FAIL "$found"
	fi
fi

# The host's random numbers: std::random_device asks the processor
# (RDRAND), which no sandbox can stand in front of, so a machine that used it
# would be another every run (the console's account got a new id each time).
# No object the sandboxed core is linked from may refer to it. (Its code is
# in core.wbx all the same: the C++ library keeps it in the object that
# also holds std::mt19937, which the core uses with a fixed seed.)
if [ "$have_sandbox" = 0 ]; then
	report "host:random" SKIP "$why_sandbox"
else
	refs="$(for a in "$root"/build/guest/*.a "$root"/build/guest/driver/*.a "$root"/build/deps/guest/lib/*.a; do
		nm -A "$a" 2> /dev/null | grep ' U _ZNSt13random_device'; done)"
	if [ -z "$refs" ]; then
		report "host:random" PASS "no object of the sandboxed core refers to std::random_device"
	else
		report "host:random" FAIL "$(echo "$refs" | head -2 | tr '\n' ' ')"
	fi
fi

# The renderer is made again on a new GPU context, as the frontend gives
# after every load: natively, a new context before every frame, and the run
# is the run without (the picture included). The control: with the test knob
# CHIMERA_TEST_NO_REMAKE the renderer is never told, and the picture is
# another.
if [ "$have_native" = 0 ]; then
	report "picture:remade" SKIP "$why_native"
else
	pf=$((frames < 120 ? frames : 120))
	native p0 "$frames_rpx" --frames "$pf" --every 30
	native p1 "$frames_rpx" --frames "$pf" --every 30 --new-context 1
	CHIMERA_TEST_NO_REMAKE=1 native p2 "$frames_rpx" --frames "$pf" --every 30 --new-context 1
	if whole "$work/p0.out" "$pf" && cmp -s "$work/p0.out" "$work/p1.out" && ! cmp -s "$work/p0.out" "$work/p2.out"; then
		report "picture:remade" PASS "$pf frames with a new GPU context before each are the run without; untold, the picture is another"
	else
		report "picture:remade" FAIL "straight [$(last "$work/p0.out")] remade [$(last "$work/p1.out")] untold [$(last "$work/p2.out")] $(tail -1 "$work/p1.err")"
	fi
fi

# A state saved and loaded before every frame changes nothing: the machine
# and the picture. The control: HARNESS_BREAK_STALE_STATE loads, half way,
# the state of the frame before, and then the run is another.
rr=$((frames < 120 ? frames : 120))
if [ "$have_sandbox" = 0 ]; then
	report "states:rerecord" SKIP "$why_sandbox"
else
	sandbox s0 "$frames_rpx" --frames "$rr" --every 30
	sandbox rr "$frames_rpx" --frames "$rr" --every 30 --rerecord
	HARNESS_BREAK_STALE_STATE=1 sandbox rrx "$frames_rpx" --frames "$rr" --every 30 --rerecord
	if whole "$work/s0.out" "$rr" && cmp -s "$work/s0.out" "$work/rr.out" && grep -q '^end ' "$work/rrx.out" && ! cmp -s "$work/s0.out" "$work/rrx.out"; then
		report "states:rerecord" PASS "$rr frames with a state saved and loaded before each are the run without; a stale state is caught"
	else
		report "states:rerecord" FAIL "straight [$(last "$work/s0.out")] rerecord [$(last "$work/rr.out")] stale [$(last "$work/rrx.out")] $(tail -1 "$work/rr.err")"
	fi
fi

# The same with the GamePad and the sound: the pad program with the script,
# a state saved and loaded before every frame, is the run without (what the
# console last read of the GamePad, the touch point it keeps, the sound
# library's place are all in the state). The control: a stale state.
if [ "$have_sandbox" = 0 ]; then
	report "states:input" SKIP "$why_sandbox"
elif ! whole "$work/wi.out" 180; then
	report "states:input" SKIP "no straight run to hold it to (input:sandbox did not run)"
else
	sandbox wir "$pad_rpx" --frames 180 --every 30 --input "$input_script" --rerecord
	HARNESS_BREAK_STALE_STATE=1 sandbox wirx "$pad_rpx" --frames 180 --every 30 --input "$input_script" --rerecord
	if cmp -s "$work/wi.out" "$work/wir.out" && grep -q '^end ' "$work/wirx.out" && ! cmp -s "$work/wi.out" "$work/wirx.out"; then
		report "states:input" PASS "180 frames of the pad program with the script and a state before each are the run without"
	else
		report "states:input" FAIL "straight [$(last "$work/wi.out")] rerecord [$(last "$work/wir.out")] stale [$(last "$work/wirx.out")] $(tail -1 "$work/wir.err")"
	fi
fi

# A state saved by one process and loaded by another ends where the run
# that was never stopped ends: the machine (clock, switches, mailbox, the
# console's memory) and the picture, drawn by a renderer made again on the
# new process's GPU context. The control: a state of the frame before
# (HARNESS_BREAK_STALE_STATE).
if [ "$have_sandbox" = 0 ]; then
	report "states:session" SKIP "$why_sandbox"
elif ! whole "$work/s0.out" "$rr"; then
	report "states:session" SKIP "no straight run to hold it to (states:rerecord did not run)"
else
	half=$((rr / 2))
	sandbox ss1 "$frames_rpx" --frames "$half" --every 30 --save-state "$work/half.state"
	sandbox ss2 "$frames_rpx" --frames $((rr - half)) --every 30 --state "$work/half.state"
	HARNESS_BREAK_STALE_STATE=1 sandbox ssx1 "$frames_rpx" --frames "$half" --every 30 --save-state "$work/stale.state"
	sandbox ssx2 "$frames_rpx" --frames $((rr - half)) --every 30 --state "$work/stale.state"
	if whole "$work/ss2.out" "$rr" && [ "$(last "$work/ss2.out")" = "$(last "$work/s0.out")" ] &&
		[ "$(last "$work/ssx2.out")" != "$(last "$work/s0.out")" ]; then
		report "states:session" PASS "a state of frame $half carried into another process ends with the straight run, picture included"
	else
		report "states:session" FAIL "straight [$(last "$work/s0.out")] carried [$(last "$work/ss2.out")] stale [$(last "$work/ssx2.out")] $(tail -1 "$work/ss2.err")"
	fi
fi

# The texts. Names are never translated or bent (Chimera's owner,
# 2026-10-31): Cemu, Wii U, GamePad and the rest stay as written in all 26
# language tables (waterbox/tests/check-names.py).
if nm_out="$(python3 -I "$here/tests/check-names.py" "$here/languages" 2>&1)"; then
	report "texts:names" PASS "$(printf '%s' "$nm_out" | tail -1)"
else
	report "texts:names" FAIL "$(printf '%s' "$nm_out" | tail -3 | tr '\n' ' ' | cut -c1-300)"
fi
# Chimera's own check of a core's texts, when a Chimera checkout is given
# (CHIMERA_ROOT): every key named and every name a key. It reads every
# source under the folder it is given, and CI checks Chimera out inside this
# repository (chimera-checkout/), so it is given a copy of this
# repository's own files only (the ones git tracks, upstream left out).
ct="${CHIMERA_ROOT:-}/tools/check-core-texts.py"
if [ -n "${CHIMERA_ROOT:-}" ] && [ -f "$ct" ] && [ -d "$root/.git" -o -f "$root/.git" ]; then
	rm -rf "${work:?}/own-files" && mkdir -p "$work/own-files"
	(cd "$root" && git ls-files -z | grep -z -v '^extern/' | xargs -0 cp --parents -t "$work/own-files")
	if ct_out="$(python3 -I "$ct" "$work/own-files" 2>&1)"; then
		report "texts:chimera" PASS "$(printf '%s' "$ct_out" | tail -1)"
	else
		report "texts:chimera" FAIL "$(printf '%s' "$ct_out" | tail -5 | tr '\n' ' ' | cut -c1-300)"
	fi
else
	report "texts:chimera" SKIP "needs CHIMERA_ROOT, a Chimera checkout with tools/check-core-texts.py"
fi

echo "$ok passed, $failed failed, $skipped skipped"
[ "$failed" -eq 0 ]
