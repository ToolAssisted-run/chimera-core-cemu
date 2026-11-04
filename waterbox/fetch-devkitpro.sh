#!/bin/sh
# Puts devkitPPC and wut (the free toolchain and libraries Wii U homebrew
# is made with) in build/devkitpro, for waterbox/build-homebrew.sh. They are
# taken from the devkitpro/devkitppc container image on Docker Hub, the
# layer that holds /opt/devkitpro, pinned by its digest (the image's own
# check: the file must have that SHA-256). No container tool is needed, and
# nothing is installed outside build/.
set -eu
here="$(cd "$(dirname "$0")" && pwd)"
root="$(dirname "$here")"
out="$root/build/devkitpro"
layer=fb5f9ba398dcc0db5f31c67dc2ffe7941de66d4e26179cccb04b8725d003bba7
if [ -x "$out/devkitPPC/bin/powerpc-eabi-gcc" ] && [ -f "$out/wut/share/wut.toolchain.cmake" ]; then
	echo "devkitPPC and wut: build/devkitpro"
	exit 0
fi
dl="$root/build/dl"
mkdir -p "$dl"
file="$dl/layer-$layer.tar.gz"
if [ ! -f "$file" ]; then
	token="$(curl -sf "https://auth.docker.io/token?service=registry.docker.io&scope=repository:devkitpro/devkitppc:pull" |
		python3 -c 'import sys, json; print(json.load(sys.stdin)["token"])')"
	curl -sfL -H "Authorization: Bearer $token" -o "$file.part" \
		"https://registry-1.docker.io/v2/devkitpro/devkitppc/blobs/sha256:$layer"
	mv "$file.part" "$file"
fi
echo "$layer  $file" | sha256sum -c - > /dev/null || { echo "the layer's SHA-256 is not the pinned one" >&2; exit 1; }
rm -rf "$out"
mkdir -p "$out"
tar xzf "$file" -C "$out" --strip-components=2 opt/devkitpro
echo "devkitPPC and wut: build/devkitpro"
