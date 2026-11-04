#!/bin/sh
# Writes patches/ from the patched extern/cemu tree: the reverse of
# apply-patches.sh. A patch is the tree's difference in a list of paths; what
# no list names goes to the patch marked "*". Edit the tree, run this, and
# apply-patches.sh must then say "already applied". A file is in ONE patch,
# whole: where two patches' subjects meet in a file, it is in the earlier one.
set -eu
# a reader that stops reading (| head) must not stop this half way: it has
# deleted the patches by then and written only some of them back
trap '' PIPE
here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/.." && pwd)"
cemu="$root/extern/cemu"

# name | paths (space separated), in the order the patches apply
series='
0001-chimera-a-build-without-vulkan|src/Cafe/HW/Latte/Core/LatteBufferData.cpp src/Cafe/HW/Latte/Core/LatteShader.cpp
0002-chimera-opengl-through-the-driver|src/Cafe/HW/Latte/Renderer/OpenGL/OpenGLRenderer.h src/Cafe/HW/Latte/Renderer/OpenGL/OpenGLRenderer.cpp src/Cafe/HW/Latte/Renderer/OpenGL/OpenGLSurfaceCopy.cpp
0003-chimera-the-machine-the-driver-chooses|src/config/ActiveSettings.cpp src/Cafe/GraphicPack/GraphicPack2.cpp
0004-chimera-one-thread-at-a-time-on-one-clock|*
0005-chimera-no-host-addresses-in-the-consoles-memory|src/Cafe/OS/libs/coreinit/coreinit_FS.cpp src/Cafe/IOSU/legacy/iosu_ioctl.h
0006-chimera-the-picture-is-the-consoles|src/Cafe/HW/Latte/Core/LatteRenderTarget.cpp
0007-chimera-the-renderer-made-again-on-a-new-context|src/Cafe/HW/Latte/Renderer/OpenGL/OpenGLRendererCore.cpp
0008-chimera-the-gamepad-is-the-frames-input|src/Cafe/OS/libs/vpad/vpad.cpp src/Cafe/OS/libs/mic/mic.cpp
'

# new files are part of the difference
git -C "$cemu" add -N . > /dev/null 2>&1
# The files Nintendo's keys are taken out of (remove-keys.py) are written
# against a copy of HEAD's that has been through it, never against HEAD: a
# patch against HEAD would carry the keys in the lines it removes, and
# showing them as context would carry them too.
keyed="$(python3 "$here/remove-keys.py" --list | tr '\n' ' ')"
base="$(mktemp -d)"
trap 'rm -rf "$base"; git -C "$cemu" reset -q' EXIT
for f in $keyed; do
	mkdir -p "$base/$(dirname "$f")"
	git -C "$cemu" show "HEAD:$f" > "$base/$f"
done
python3 "$here/remove-keys.py" "$base"
keyed_diff() { # <file>: its difference from the keys-out copy, as git apply reads it
	if ! cmp -s "$base/$1" "$cemu/$1"; then
		echo "diff --git a/$1 b/$1"
		diff -u --label "a/$1" --label "b/$1" "$base/$1" "$cemu/$1" || true
	fi
}
named=""
for line in $(printf '%s\n' "$series" | grep '|' | grep -v '|\*$' | cut -d'|' -f2 | tr ' ' '\n'); do
	named="$named $line"
done
mkdir -p "$root/patches"
rm -f "$root"/patches/*.patch
printf '%s\n' "$series" | grep '|' | while IFS='|' read -r name paths; do
	out="$root/patches/$name.patch"
	if [ "$paths" = "*" ]; then
		excludes=""
		for p in $named $keyed; do excludes="$excludes :!$p"; done
		# shellcheck disable=SC2086
		git -C "$cemu" diff -- . $excludes > "$out"
		# the keyed files no patch names fall to this one
		for f in $keyed; do
			case " $named " in *" $f "*) ;; *) keyed_diff "$f" >> "$out" ;; esac
		done
	else
		plain=""
		for p in $paths; do
			case " $keyed " in *" $p "*) ;; *) plain="$plain $p" ;; esac
		done
		: > "$out"
		# shellcheck disable=SC2086
		[ -n "$plain" ] && git -C "$cemu" diff -- $plain >> "$out"
		for f in $paths; do
			case " $keyed " in *" $f "*) keyed_diff "$f" >> "$out" ;; esac
		done
	fi
	echo "$name.patch: $(grep -c '^diff --git' "$out") files" 2> /dev/null || true
done
