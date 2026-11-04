#!/bin/sh
# Builds the libraries Cemu needs and this repository does not carry, from
# release archives pinned by their SHA-256, for one of the two builds:
#
#   waterbox/build-deps.sh native       the reference, for the host
#   waterbox/build-deps.sh guest        the core, with miniBox's toolchain
#
# Each is installed under build/deps/<build>, where CMake finds it. The
# guest build needs MINIBOX_DIR (a miniBox checkout built with its C++ guest
# toolchain). Static libraries only, no assembly, no threads.
set -eu
here="$(cd "$(dirname "$0")" && pwd)"
root="$(dirname "$here")"
flavour="${1:?native|guest}"
case "$flavour" in native|guest) ;; *) echo "usage: $0 native|guest" >&2; exit 2 ;; esac
deps="$root/build/deps"
src="$deps/src"
prefix="$deps/$flavour"
obj="$deps/obj-$flavour"
jobs="${JOBS:-4}"
mkdir -p "$src" "$prefix" "$obj"

fetch() { # <url> <sha256> <file>
	if [ ! -f "$src/$3" ]; then
		curl -sfL -o "$src/$3.part" "$1" || { echo "could not download $1" >&2; exit 1; }
		mv "$src/$3.part" "$src/$3"
	fi
	echo "$2  $src/$3" | sha256sum -c - > /dev/null || { echo "$3: the SHA-256 is not the pinned one" >&2; exit 1; }
}

unpack() { # <file> <directory it makes>
	[ -d "$obj/$2" ] && return 0
	case "$1" in
		*.zip) (cd "$obj" && unzip -q "$src/$1") ;;
		*.tar.bz2) tar xjf "$src/$1" -C "$obj" ;;
		*) tar xzf "$src/$1" -C "$obj" ;;
	esac
}

if [ "$flavour" = guest ]; then
	[ -n "${MINIBOX_DIR:-}" ] || { echo "set MINIBOX_DIR to a miniBox checkout built with -Dguest_cpp=true" >&2; exit 2; }
	export MINIBOX_DIR
	# what the guest toolchain lets CMake look into, beside its sysroot
	export CHIMERA_DEPS_PREFIX="$prefix"
	cmake_flavour="-DCMAKE_TOOLCHAIN_FILE=$here/guest-toolchain.cmake"
	sr="$MINIBOX_DIR/build/meson-cpp/guest-sysroot"
	cc="gcc -specs=$sr/lib/musl-gcc.specs"
	cflags="-fvisibility=hidden -mcmodel=large -mstack-protector-guard=global -fno-stack-protector -fno-pic -fno-pie -fcf-protection=none -O2"
else
	cmake_flavour="-DCMAKE_POSITION_INDEPENDENT_CODE=ON"
	cc=gcc
	cflags="-O2 -fPIC"
fi

cmake_dep() { # <name> <source dir> [cmake options...]
	name="$1"; dir="$2"; shift 2
	[ -f "$obj/$name.done" ] && return 0
	# shellcheck disable=SC2086
	cmake -S "$dir" -B "$obj/$name-build" -G Ninja $cmake_flavour -DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_INSTALL_PREFIX="$prefix" -DCMAKE_INSTALL_LIBDIR=lib -DBUILD_SHARED_LIBS=OFF "$@" \
		> "$obj/$name-configure.log" 2>&1 || { tail -20 "$obj/$name-configure.log" >&2; exit 1; }
	ninja -C "$obj/$name-build" -j "$jobs" install > "$obj/$name-build.log" 2>&1 || { grep -E "error|FAILED" "$obj/$name-build.log" | head -20 >&2; exit 1; }
	touch "$obj/$name.done"
	echo "$flavour: $name"
}

fetch https://github.com/madler/zlib/releases/download/v1.3.1/zlib-1.3.1.tar.gz \
	9a93b2b7dfdac77ceba5a558a580e74667dd6fede4585b91eefb60f03b72df23 zlib-1.3.1.tar.gz
fetch https://github.com/facebook/zstd/releases/download/v1.5.6/zstd-1.5.6.tar.gz \
	8c29e06cf42aacc1eafc4077ae2ec6c6fcb96a626157e0593d5e82a34fd403c1 zstd-1.5.6.tar.gz
fetch https://github.com/fmtlib/fmt/releases/download/12.1.0/fmt-12.1.0.zip \
	695fd197fa5aff8fc67b5f2bbc110490a875cdf7a41686ac8512fb480fa8ada7 fmt-12.1.0.zip
fetch https://github.com/zeux/pugixml/releases/download/v1.14/pugixml-1.14.tar.gz \
	2f10e276870c64b1db6809050a75e11a897a8d7456c4be5c6b2e35a11168a015 pugixml-1.14.tar.gz
fetch https://github.com/g-truc/glm/archive/refs/tags/1.0.1.tar.gz \
	9f3174561fd26904b23f0db5e560971cbf9b3cbda0b280f04d5c379d03bf234c glm-1.0.1.tar.gz
fetch https://github.com/Tencent/rapidjson/archive/24b5e7a8b27f42fa16b96fc70aade9106cf7102f.tar.gz \
	2d2601a82d2d3b7e143a3c8d43ef616671391034bc46891a9816b79cf2d3e7a8 rapidjson-24b5e7a8.tar.gz
fetch https://archives.boost.io/release/1.86.0/source/boost_1_86_0.tar.bz2 \
	1bed88e40401b2cb7a1f76d4bab499e352fa4d0c5f31c0dbae64e24d34d7513b boost_1_86_0.tar.bz2
fetch https://github.com/openssl/openssl/releases/download/openssl-3.0.15/openssl-3.0.15.tar.gz \
	23c666d0edf20f14249b3d8f0368acaee9ab585b09e1de82107c66e1f3ec9533 openssl-3.0.15.tar.gz

fetch https://download.sourceforge.net/libpng/libpng-1.6.44.tar.gz \
	8c25a7792099a0089fa1cc76c94260d0bb3f1ec52b93671b572f8bb61577b732 libpng-1.6.44.tar.gz
fetch https://github.com/curl/curl/releases/download/curl-8_11_1/curl-8.11.1.tar.gz \
	a889ac9dbba3644271bd9d1302b5c22a088893719b72be3487bc3d401e5c4e80 curl-8.11.1.tar.gz

# curl: its headers only. Cemu's network code is compiled against them and
# linked to waterbox/driver/cemu/curl_offline.cpp, which answers every
# transfer with "could not connect": a core has no network.
if [ ! -f "$obj/curl-headers.done" ]; then
	mkdir -p "$prefix/include"
	tar xzf "$src/curl-8.11.1.tar.gz" -C "$prefix/include" --strip-components=2 curl-8.11.1/include/curl
	# and its licence, which the package carries (package-licenses.json)
	tar xzf "$src/curl-8.11.1.tar.gz" -C "$obj" curl-8.11.1/COPYING
	touch "$obj/curl-headers.done"
	echo "$flavour: curl (headers)"
fi

unpack zlib-1.3.1.tar.gz zlib-1.3.1
cmake_dep zlib "$obj/zlib-1.3.1" -DZLIB_BUILD_EXAMPLES=OFF
# the shared library is built whatever is asked; only the archive is wanted
rm -f "$prefix"/lib/libz.so*

unpack libpng-1.6.44.tar.gz libpng-1.6.44
cmake_dep libpng "$obj/libpng-1.6.44" -DPNG_SHARED=OFF -DPNG_STATIC=ON -DPNG_TESTS=OFF -DPNG_TOOLS=OFF \
	-DPNG_HARDWARE_OPTIMIZATIONS=OFF -DCMAKE_PREFIX_PATH="$prefix" -DZLIB_ROOT="$prefix"

unpack zstd-1.5.6.tar.gz zstd-1.5.6
# (CFLAGS, not CMAKE_C_FLAGS: the latter would replace the guest toolchain's
# own flags.) No assembly, no trace hooks (weak symbols a static guest would
# resolve to address 0), and no BMI2 bodies chosen at run time (their target
# attributes reset -mno-red-zone, and the choice would be the host's).
CFLAGS="-DZSTD_DISABLE_ASM=1 -DZSTD_TRACE=0 -DDYNAMIC_BMI2=0" \
cmake_dep zstd "$obj/zstd-1.5.6/build/cmake" -DZSTD_BUILD_PROGRAMS=OFF -DZSTD_BUILD_SHARED=OFF \
	-DZSTD_BUILD_TESTS=OFF -DZSTD_MULTITHREAD_SUPPORT=OFF -DZSTD_LEGACY_SUPPORT=OFF

unpack fmt-12.1.0.zip fmt-12.1.0
cmake_dep fmt "$obj/fmt-12.1.0" -DFMT_TEST=OFF -DFMT_DOC=OFF

unpack pugixml-1.14.tar.gz pugixml-1.14
cmake_dep pugixml "$obj/pugixml-1.14" -DPUGIXML_BUILD_TESTS=OFF

unpack glm-1.0.1.tar.gz glm-1.0.1
cmake_dep glm "$obj/glm-1.0.1" -DGLM_BUILD_TESTS=OFF -DGLM_BUILD_LIBRARY=OFF -DGLM_BUILD_INSTALL=ON

unpack rapidjson-24b5e7a8.tar.gz rapidjson-24b5e7a8b27f42fa16b96fc70aade9106cf7102f
cmake_dep rapidjson "$obj/rapidjson-24b5e7a8b27f42fa16b96fc70aade9106cf7102f" \
	-DRAPIDJSON_BUILD_DOC=OFF -DRAPIDJSON_BUILD_EXAMPLES=OFF -DRAPIDJSON_BUILD_TESTS=OFF

# Boost: its headers, nothing compiled
if [ ! -f "$obj/boost.done" ]; then
	unpack boost_1_86_0.tar.bz2 boost_1_86_0
	rm -rf "$prefix/include/boost"
	mkdir -p "$prefix/include"
	cp -r "$obj/boost_1_86_0/boost" "$prefix/include/boost"
	touch "$obj/boost.done"
	echo "$flavour: boost (headers)"
fi

# OpenSSL's libcrypto (the console's cryptography; nothing reaches a network)
if [ ! -f "$obj/openssl.done" ]; then
	rm -rf "$obj/openssl-build"
	mkdir -p "$obj/openssl-build"
	tar xzf "$src/openssl-3.0.15.tar.gz" -C "$obj/openssl-build" --strip-components=1
	# shellcheck disable=SC2086
	(cd "$obj/openssl-build" && CC="$cc" ./Configure linux-x86_64 --prefix="$prefix" --libdir=lib \
		no-shared no-asm no-threads no-dso no-engine no-async no-tests no-sock \
		no-module no-legacy no-autoload-config no-secure-memory no-afalgeng no-devcryptoeng \
		$cflags) > "$obj/openssl-configure.log" 2>&1 || { tail -20 "$obj/openssl-configure.log" >&2; exit 1; }
	make -C "$obj/openssl-build" -j"$jobs" build_libs > "$obj/openssl-build.log" 2>&1 || { tail -20 "$obj/openssl-build.log" >&2; exit 1; }
	make -C "$obj/openssl-build" install_dev > "$obj/openssl-install.log" 2>&1
	touch "$obj/openssl.done"
	echo "$flavour: openssl"
fi
echo "$flavour: dependencies in build/deps/$flavour"
