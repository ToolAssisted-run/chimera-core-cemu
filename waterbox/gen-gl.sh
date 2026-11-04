#!/bin/sh
# Generates both halves of the GPU bridge (build/generated-gl) from
# miniBox's master list, for every name glad's header knows (glad, the GL
# loader the host half is built on, is in waterbox/glad). Cemu finds its
# entry points by name (chimera_gl_lookup), so the install step's
# assignments to glad's own pointers are taken out: the core links no glad.
#
#   waterbox/gen-gl.sh <miniBox checkout>
set -eu
here="$(cd "$(dirname "$0")" && pwd)"
root="$(dirname "$here")"
mb="${1:?name a miniBox checkout}"
out="$root/build/generated-gl"
mkdir -p "$out.new"
python3 "$mb/source/gl/gen-gl-bridge.py" "$here/glad/include/glad/gl.h" "$mb/source/gl/gl-entry-points.txt" "$out.new" > "$root/build/gen-gl.log" 2>&1 || {
	tail -5 "$root/build/gen-gl.log" >&2; echo "the GPU bridge did not generate: build/gen-gl.log" >&2; exit 1; }
sed -i '/^\tglad_gl[A-Za-z0-9_]* = w_gl[A-Za-z0-9_]*;$/d' "$out.new/gl-bridge-guest.cpp"
mkdir -p "$out"
for f in "$out.new"/*; do
	cmp -s "$f" "$out/$(basename "$f")" 2> /dev/null || cp "$f" "$out/"
done
rm -rf "$out.new"
