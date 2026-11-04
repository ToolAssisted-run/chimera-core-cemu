#!/bin/sh
# The test programs (tests/homebrew), built with devkitPPC and wut into
# build/homebrew/*.rpx. DEVKITPRO names a devkitPro installation with
# devkitPPC and wut (the wiiu-dev group); where it has no devkitPPC,
# build/devkitpro is used, where waterbox/fetch-devkitpro.sh puts one.
set -eu
here="$(cd "$(dirname "$0")" && pwd)"
root="$(dirname "$here")"
dkp="$root/build/devkitpro"
if [ -n "${DEVKITPRO:-}" ] && [ -x "$DEVKITPRO/devkitPPC/bin/powerpc-eabi-gcc" ]; then dkp="$DEVKITPRO"; fi
[ -x "$dkp/devkitPPC/bin/powerpc-eabi-gcc" ] && [ -f "$dkp/wut/share/wut.toolchain.cmake" ] || {
	echo "no devkitPPC and wut at $dkp: set DEVKITPRO, or run waterbox/fetch-devkitpro.sh" >&2; exit 1; }
export DEVKITPRO="$dkp" DEVKITPPC="$dkp/devkitPPC"
out="$root/build/homebrew"
mkdir -p "$out"
cmake -S "$root/tests/homebrew" -B "$out/obj" -G Ninja -DCMAKE_TOOLCHAIN_FILE="$dkp/cmake/WiiU.cmake" \
	-DCMAKE_BUILD_TYPE=Release > "$out/configure.log" 2>&1 || { tail -20 "$out/configure.log" >&2; exit 1; }
ninja -C "$out/obj" > "$out/build.log" 2>&1 || { grep -E "error|FAILED" "$out/build.log" | head -20 >&2; exit 1; }
for f in "$out"/obj/*.rpx; do
	cp "$f" "$out/"
	echo "built: build/homebrew/$(basename "$f")"
done
