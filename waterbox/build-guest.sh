#!/bin/sh
# The guest (sandbox) build: this core's CMake under miniBox's guest
# toolchain, then core.wbx linked from its archives, and the host runner for
# it (run-wbx), which carries the GPU bridge's host half.
#
#   waterbox/build-guest.sh -m <miniBox checkout> [-j <jobs>]
#
# The miniBox checkout (or MINIBOX_DIR) must be built with the C++ guest
# toolchain and its host library:
#   meson setup <miniBox>/build/meson-cpp <miniBox> -Dguest_cpp=true && ninja -C <miniBox>/build/meson-cpp
#   meson setup <miniBox>/build/meson-linux <miniBox> && ninja -C <miniBox>/build/meson-linux
set -eu
here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/.." && pwd)"
mb="${MINIBOX_DIR:-}"
jobs=4
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
mbuild="$mb/build/meson-cpp"
sr="$mbuild/guest-sysroot"
[ -f "$sr/lib/libstdc++.a" ] || { echo "no C++ guest toolchain at $sr" >&2; exit 1; }
mbhost="$mb/build/meson-linux/source/host"
[ -f "$mbhost/libminiboxhost.so" ] || { echo "no miniBox host library at $mbhost" >&2; exit 1; }

[ -f "$root/build/deps/guest/lib/libfmt.a" ] || JOBS="$jobs" "$here/build-deps.sh" guest
"$here/apply-patches.sh"
"$here/gen-gl.sh" "$mb"

guest="$root/build/guest"
out="$root/build/wbx"
mkdir -p "$guest" "$out"
CHIMERA_DEPS_PREFIX="$root/build/deps/guest" cmake -S "$here" -B "$guest" -G Ninja \
	-DCMAKE_TOOLCHAIN_FILE="$here/guest-toolchain.cmake" -DCMAKE_BUILD_TYPE=RelWithDebInfo \
	-DCHIMERA_GUEST_BUILD=ON -DCMAKE_PREFIX_PATH="$root/build/deps/guest" -DMINIBOX_DIR="$mb" \
	> "$root/build/configure-guest.log" 2>&1 || {
	tail -20 "$root/build/configure-guest.log" >&2; echo "configure failed: build/configure-guest.log" >&2; exit 1; }
ninja -C "$guest" -j "$jobs" -k 0 > "$root/build/build-guest.log" 2>&1 || {
	grep -E "error|FAILED" "$root/build/build-guest.log" | head -40 >&2
	echo "the guest build failed: build/build-guest.log" >&2
	exit 1
}

# core.wbx: the driver's archive whole (nothing refers to its exports), then
# every other archive of the build and the libraries, in one group
deps="$root/build/deps/guest/lib"
libs="$(find "$guest" -name '*.a' ! -name 'libcemu-driver.a' | sort | tr '\n' ' ')"
# shellcheck disable=SC2086
g++ -specs "$sr/lib/musl-gcc.specs" -mcmodel=large -fno-pic -fno-pie \
	-static -no-pie -Wl,--eh-frame-hdr,-O2,--no-relax,-z,stack-size=8388608 -T "$mb/source/guest/linkscript.T" \
	-Wl,-u,pthread_once -Wl,-u,pthread_cond_wait -Wl,-u,pthread_cond_broadcast -Wl,-u,pthread_key_create \
	-Wl,-u,pthread_mutexattr_init -Wl,-u,pthread_mutexattr_settype -Wl,-u,pthread_mutexattr_destroy \
	-o "$out/core.wbx" \
	"$mbuild/source/guest/cxxglue.c.o" "$mbuild/source/guest/emulibc.c.o" \
	-Wl,--whole-archive "$guest/driver/libcemu-driver.a" -Wl,--no-whole-archive \
	-Wl,--start-group $libs "$deps/libssl.a" "$deps/libcrypto.a" "$deps/libzstd.a" "$deps/libz.a" "$deps/libpugixml.a" "$deps/libfmt.a" -Wl,--end-group \
	-L"$sr/lib" -lstdc++ -lgcc -lgcc_eh -lc > "$root/build/link-core.log" 2>&1 || {
	grep -E "undefined reference|error" "$root/build/link-core.log" | sed 's/.*undefined reference to/undefined:/' | sort | uniq -c | sort -rn | head -40 >&2
	echo "core.wbx did not link: build/link-core.log" >&2
	exit 1
}
# where the code reads its thread pointer, listed in the image: an Intel Mac
# rewrites those reads as the core loads (miniBox's wbx-tp-rewrite.py)
python3 "$mb/source/guest/wbx-tp-rewrite.py" "$out/core.wbx"
sh "$mb/source/guest/check-wbx.sh" "$out/core.wbx"
echo "built: build/wbx/core.wbx"

# the runner: miniBox's host, and the GPU bridge's host half on an EGL context
gcc -O2 -g -Wall -DCHIMERA_GL_BRIDGE -I"$mb/source/host" -I"$mb/source/gl" -I"$root/build/generated-gl" \
	-I"$here/glad/include" -I"$here" -o "$out/run-wbx" "$here/run-wbx.c" "$here/gl-host.c" "$here/glad/src/gl.c" \
	"$mbhost/libminiboxhost.so" -Wl,-rpath,"$mbhost" -lEGL -ldl
echo "built: build/wbx/run-wbx"
