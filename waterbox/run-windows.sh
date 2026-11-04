#!/bin/sh
# The package on Windows, from WSL: Chimera's headless runner for Windows
# (chimera-run.exe, cross-built in a Chimera checkout) runs projects of the
# test programs on the PC's own graphics card, and the console's memory it
# leaves is held to what the same projects leave on Linux. The pictures are
# compared too, and said; a picture drawn by another card may differ in its
# edges without the machine differing.
#
#   waterbox/run-windows.sh --chimera-root <chimera checkout> --stage <folder under /mnt/c> [--exe <chimera-run.exe>] [--cpu interpreter|recompiler]
#
# Each program runs with both CPU modes (the recompiler is the default
# setting); --cpu names one.
#
# Needs what waterbox/run-frontend.sh needs, and a chimera-run.exe of the
# same Chimera (by default <chimera>/build/dll/chimera-run.exe; --exe names
# another, such as a cross build of the same commit made elsewhere; the DLLs
# beside it go with it). The stage folder is made, used and emptied; nothing
# of a game is ever put there (the test programs are this repository's own).
set -u
here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/.." && pwd)"
chimera_root="${CHIMERA_ROOT:-}"
stage=""
runexe=""
cpus="interpreter recompiler"
while [ $# -gt 0 ]; do
	case "$1" in
		--chimera-root) chimera_root="$2"; shift ;;
		--stage) stage="$2"; shift ;;
		--exe) runexe="$2"; shift ;;
		--cpu) cpus="$2"; shift ;;
		*) echo "unknown argument: $1" >&2; exit 2 ;;
	esac
	shift
done
[ -n "$chimera_root" ] && [ -d "$chimera_root" ] || { echo "say where Chimera is: --chimera-root <checkout>" >&2; exit 2; }
case "$stage" in /mnt/*) ;; *) echo "say where to stage on Windows: --stage /mnt/c/..." >&2; exit 2 ;; esac
runner="$chimera_root/build/dll/chimera-run"
[ -n "$runexe" ] || runexe="$chimera_root/build/dll/chimera-run.exe"
package="$root/build/package/cemu.chimeraCore"
hb="$root/build/homebrew"
[ -x "$runner" ] && [ -f "$runexe" ] && [ -f "$package" ] || { echo "needs chimera-run, chimera-run.exe and the package" >&2; exit 1; }
work="$(mktemp -d)"
mkdir -p "$stage"
trap 'rm -rf "$work"; rm -rf "${stage:?}"' EXIT
ok=0; failed=0
report() {
	printf '%-22s %-6s %s\n' "$1" "$2" "$3"
	case "$2" in PASS) ok=$((ok + 1)) ;; *) failed=$((failed + 1)) ;; esac
}
# the runner, the libraries beside it (miniBox's host, its crash notes) and the package
cp "$runexe" "$package" "$stage/"
for dll in "$(dirname "$runexe")"/*.dll; do [ -f "$dll" ] && cp "$dll" "$stage/"; done
# and libzstd, which the engine loads from beside the runner, as the bundle
# has it: without it a history is written uncompressed (measured: the pad
# program's 180 frames, 218 MB instead of 4.8 MB)
if [ ! -f "$stage/libzstd.dll" ]; then
	zstd_dll=""
	for d in "$(dirname "$runexe")" "$chimera_root/build/meson-windows" "$chimera_root/build/dll"; do
		[ -f "$d/libzstd.dll" ] && { zstd_dll="$d/libzstd.dll"; break; }
	done
	[ -n "$zstd_dll" ] || { echo "no libzstd.dll beside the runner or in $chimera_root/build: a history would be written uncompressed" >&2; exit 1; }
	cp "$zstd_dll" "$stage/"
fi
winstage="$(wslpath -w "$stage")"

# one project a program, a movie of N frames (the pad program's presses
# every control), made on Linux and run on both
cat > "$work/pad.input" <<'INPUT'
30 0x1265F
60 0xD9A0
90 0 16384 -32768 0 32767
120 0x20000 0 0 0 0 32768 52000
150 0
INPUT
# Savestates and the state history on Windows, with the pad program's
# project (windows:states, windows:history below). A state saved and
# loaded around every frame (--rerecord), and the state after frame 90 (F=89,
# counted from 0) saved by one Windows process and loaded by another into a
# project whose movie is the rest, end with Linux's straight run; so does
# the Windows state loaded on Linux.
states_check() {
	tail -n +91 "$work/pad.movie" > "$work/rest.movie"
	mkdir -p "$work/rest" "$stage/rest"
	cp "$hb/pad.rpx" "$work/rest/"
	"$runner" --new-project "$work/rest/rest.chimeraProject" "$package" "$work/rest/pad.rpx" "$work/rest.movie" \
		--settings "{\"cpu_mode\": \"$cpu\", \"console_fonts\": \"standin\", \"mii_resource\": \"none\"}" > "$work/rest.new.log" 2>&1
	cp "$work/rest/rest.chimeraProject" "$hb/pad.rpx" "$stage/rest/"
	( cd "$stage" && ./chimera-run.exe --project "$winstage\\pad\\pad.chimeraProject" "$winstage\\cemu.chimeraCore" \
		--gpu --frames 180 --rerecord --dump "MEM2=$winstage\\rr.mem2" > "$work/rr.windows.log" 2>&1 )
	( cd "$stage" && ./chimera-run.exe --project "$winstage\\pad\\pad.chimeraProject" "$winstage\\cemu.chimeraCore" \
		--gpu --frames 90 --save-state "89=$winstage\\pad.state" > "$work/save.windows.log" 2>&1 )
	( cd "$stage" && ./chimera-run.exe --project "$winstage\\rest\\rest.chimeraProject" "$winstage\\cemu.chimeraCore" \
		--gpu --state "$winstage\\pad.state" --frames 90 --dump "MEM2=$winstage\\rest.mem2" > "$work/rest.windows.log" 2>&1 )
	systemd-run --user --scope --quiet -p MemoryMax=12G -p MemorySwapMax=0 "$runner" --project "$work/rest/rest.chimeraProject" "$package" \
		--gpu --state "$stage/pad.state" --frames 90 --dump "MEM2=$work/rest.linux.mem2" > "$work/rest.linux.log" 2>&1
	rr=differs; carried=differs; across=differs
	[ -s "$stage/rr.mem2" ] && cmp -s "$work/pad.linux.mem2" "$stage/rr.mem2" && rr="the same"
	[ -s "$stage/rest.mem2" ] && cmp -s "$work/pad.linux.mem2" "$stage/rest.mem2" && carried="the same"
	[ -s "$work/rest.linux.mem2" ] && cmp -s "$work/pad.linux.mem2" "$work/rest.linux.mem2" && across="the same"
	if [ "$rr" = "the same" ] && [ "$carried" = "the same" ] && [ "$across" = "the same" ]; then
		report "windows:states:$cpu" PASS "180 frames of the pad program: a state around every frame, a state of frame 90 carried into another process, and that state loaded on Linux all end with Linux's straight MEM2"
	else
		report "windows:states:$cpu" FAIL "around every frame: $rr; carried: $carried ($(tail -1 "$work/rest.windows.log" | cut -c1-100)); on Linux: $across"
	fi
	rm -f "$stage/rr.mem2" "$stage/rest.mem2" "$stage/pad.state" "$work/rest.linux.mem2"

	# the state history (greenzone) kept by one Windows process and sought
	# into by another (frame 150, as reopening a project does), and the same
	# Windows history sought into on Linux: the machine and the picture at
	# frame 150 are Linux's straight run's
	systemd-run --user --scope --quiet -p MemoryMax=12G -p MemorySwapMax=0 "$runner" --project "$work/pad/pad.chimeraProject" "$package" \
		--gpu --frames 150 --dump "MEM2=$work/s150.mem2" --final-screenshot "$work/s150.tga" > "$work/s150.log" 2>&1
	( cd "$stage" && ./chimera-run.exe --project "$winstage\\pad\\pad.chimeraProject" "$winstage\\cemu.chimeraCore" \
		--gpu --frames 180 --history-out "$winstage\\pad.hist" > "$work/hout.windows.log" 2>&1 )
	( cd "$stage" && ./chimera-run.exe --project "$winstage\\pad\\pad.chimeraProject" "$winstage\\cemu.chimeraCore" \
		--gpu --frames 180 --history-in "$winstage\\pad.hist" --seek 150 --stop-at-seek \
		--dump "MEM2=$winstage\\h150.mem2" --final-screenshot "$winstage\\h150.tga" > "$work/hin.windows.log" 2>&1 )
	systemd-run --user --scope --quiet -p MemoryMax=12G -p MemorySwapMax=0 "$runner" --project "$work/pad/pad.chimeraProject" "$package" \
		--gpu --frames 180 --history-in "$stage/pad.hist" --seek 150 --stop-at-seek --dump "MEM2=$work/h150.linux.mem2" > "$work/hin.linux.log" 2>&1
	pic_s="$(python3 "$here/tests/picture.py" "$work/s150.tga" 2> /dev/null)"
	pic_h="$(python3 "$here/tests/picture.py" "$stage/h150.tga" 2> /dev/null)"
	sought=differs; across=differs
	[ -s "$stage/h150.mem2" ] && cmp -s "$work/s150.mem2" "$stage/h150.mem2" && sought="the same"
	[ -s "$work/h150.linux.mem2" ] && cmp -s "$work/s150.mem2" "$work/h150.linux.mem2" && across="the same"
	if [ -s "$stage/pad.hist" ] && [ "$sought" = "the same" ] && [ -n "$pic_h" ] && [ "$pic_h" = "$pic_s" ] && [ "$across" = "the same" ]; then
		report "windows:history:$cpu" PASS "a history of $(wc -c < "$stage/pad.hist") bytes kept by one Windows process: sought into by another, frame 150 is Linux's straight run, MEM2 and picture; sought into on Linux, the same"
	else
		report "windows:history:$cpu" FAIL "sought on Windows: MEM2 $sought, picture [$pic_h] want [$pic_s] ($(tail -1 "$work/hin.windows.log" | cut -c1-100)); on Linux: $across"
	fi
	rm -f "$stage/pad.hist" "$stage/h150.mem2" "$stage/h150.tga" "$work/s150.mem2" "$work/h150.linux.mem2"
}

for cpu in $cpus; do
for spec in frames:300:/dev/null pad:180:$work/pad.input draw:120:/dev/null readback:120:/dev/null cores:120:/dev/null; do
	name="${spec%%:*}"; rest="${spec#*:}"; n="${rest%%:*}"; script="${rest#*:}"
	mkdir -p "$work/$name" "$stage/$name"
	cp "$hb/$name.rpx" "$work/$name/"
	python3 "$here/tests/make-movie.py" "$here/waterbox.config" "$script" "$n" > "$work/$name.movie"
	"$runner" --new-project "$work/$name/$name.chimeraProject" "$package" "$work/$name/$name.rpx" "$work/$name.movie" \
		--settings "{\"cpu_mode\": \"$cpu\", \"console_fonts\": \"standin\", \"mii_resource\": \"none\"}" > "$work/$name.new.log" 2>&1
	cp "$work/$name/$name.chimeraProject" "$hb/$name.rpx" "$stage/$name/"
	systemd-run --user --scope --quiet -p MemoryMax=12G -p MemorySwapMax=0 "$runner" --project "$work/$name/$name.chimeraProject" "$package" \
		--gpu --frames "$n" --dump "MEM2=$work/$name.linux.mem2" --final-screenshot "$work/$name.linux.tga" > "$work/$name.linux.log" 2>&1
	( cd "$stage" && ./chimera-run.exe --project "$winstage\\$name\\$name.chimeraProject" "$winstage\\cemu.chimeraCore" \
		--gpu --frames "$n" --dump "MEM2=$winstage\\$name.mem2" --final-screenshot "$winstage\\$name.tga" \
		--meta "$winstage\\$name.meta" > "$work/$name.windows.log" 2>&1 )
	win_rc=$?
	card="$(grep -a -m1 'chimera gl:' "$work/$name.windows.log" | tr '\037' ' ' | cut -c1-120)"
	pic_l="$(python3 "$here/tests/picture.py" "$work/$name.linux.tga" 2> /dev/null)"
	pic_w="$(python3 "$here/tests/picture.py" "$stage/$name.tga" 2> /dev/null)"
	if [ -s "$stage/$name.mem2" ] && cmp -s "$work/$name.linux.mem2" "$stage/$name.mem2"; then
		report "windows:$name:$cpu" PASS "$n frames: MEM2 is Linux's; picture $( [ "$pic_l" = "$pic_w" ] && echo "the same" || echo "differs ($pic_w, Linux $pic_l)"); $card"
	else
		report "windows:$name:$cpu" FAIL "exit $win_rc; MEM2 $(cmp "$work/$name.linux.mem2" "$stage/$name.mem2" 2>&1 | head -1); $(sed -n 's/^detail=//p' "$stage/$name.meta" 2>/dev/null) $(tail -2 "$work/$name.windows.log" | tr '\n' ' ' | cut -c1-200)"
	fi
	[ "$name" = pad ] && states_check
	rm -f "$stage/$name.mem2" "$stage/$name.tga" "$stage/$name.meta" "$work/$name.linux.mem2"
done
done
echo "$ok passed, $failed failed"
[ "$failed" -eq 0 ]
