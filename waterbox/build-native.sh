#!/bin/sh
# The native reference: Cemu and this core's driver, built for the host
# with the same compiler as the guest (GCC), into build/native.
#
#   waterbox/build-native.sh -m <miniBox checkout> [-j <jobs>]
#
# The miniBox checkout (or MINIBOX_DIR) gives the GPU bridge both runners
# draw through.
set -eu
here="$(cd "$(dirname "$0")" && pwd)"
root="$(dirname "$here")"
jobs=4
mb="${MINIBOX_DIR:-}"
while getopts m:j: opt; do
	case "$opt" in
		m) mb="$OPTARG" ;;
		j) jobs="$OPTARG" ;;
		*) echo "usage: $0 -m <miniBox checkout> [-j <jobs>]" >&2; exit 2 ;;
	esac
done
[ -n "$mb" ] || { echo "name a miniBox checkout: -m <dir> or MINIBOX_DIR" >&2; exit 2; }
mb="$(cd "$mb" && pwd)"
export MINIBOX_DIR="$mb"
[ -f "$root/build/deps/native/lib/libfmt.a" ] || JOBS="$jobs" "$here/build-deps.sh" native
"$here/apply-patches.sh"
"$here/gen-gl.sh" "$mb"
out="$root/build/native"
mkdir -p "$out"
cmake -S "$here" -B "$out" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
	-DCMAKE_C_COMPILER="$(command -v gcc)" -DCMAKE_CXX_COMPILER="$(command -v g++)" \
	-DCMAKE_PREFIX_PATH="$root/build/deps/native" -DMINIBOX_DIR="$mb" > "$root/build/configure-native.log" 2>&1 || {
	tail -20 "$root/build/configure-native.log" >&2; echo "configure failed: build/configure-native.log" >&2; exit 1; }
ninja -C "$out" -j "$jobs" -k 0 > "$root/build/build-native.log" 2>&1 || {
	grep -E "error|FAILED" "$root/build/build-native.log" | head -40 >&2
	echo "the native build failed: build/build-native.log" >&2
	exit 1
}
echo "built: build/native"
