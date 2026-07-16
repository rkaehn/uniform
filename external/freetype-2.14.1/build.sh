#!/bin/sh
set -e
here="$(cd "$(dirname "$0")" && pwd)"
. "$here/../toolchain.sh"

fetch freetype-2.14.1 https://download.savannah.gnu.org/releases/freetype/freetype-2.14.1.tar.xz
src="$sources/freetype-2.14.1"
cp "$src/docs/FTL.TXT" "$here/"
cp "$here/ftoption.h" "$src/include/freetype/config/ftoption.h"
cp "$here/ftmodule.h" "$src/include/freetype/config/ftmodule.h"

build_freetype() {
    platform="$1"
    shift
    build="$src/build-$platform"
    rm -rf "$build"
    output "$platform"
    cmake -S "$src" -B "$build" -G Ninja -Wno-dev \
        -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_FLAGS_RELEASE="-O2 -DNDEBUG" \
        -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
        -DCMAKE_AR="$llvm/llvm-ar" -DCMAKE_RANLIB="$llvm/llvm-ranlib" \
        -DCMAKE_C_FLAGS="$prefix_map" -DCMAKE_INSTALL_PREFIX="$out" \
        -DFT_DISABLE_ZLIB=ON -DFT_DISABLE_PNG=ON -DFT_DISABLE_BZIP2=ON -DFT_DISABLE_HARFBUZZ=ON -DFT_DISABLE_BROTLI=ON \
        "$@"
    cmake --build "$build"
    cmake --install "$build" >/dev/null
    sed -i '' 's|^prefix=.*|prefix=${pcfiledir}/../..|' "$out/lib/pkgconfig/freetype2.pc"
    publish
}

build_freetype macos-aarch64 -DCMAKE_C_COMPILER="$apple_cc" -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0
build_freetype ios-aarch64 -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_C_COMPILER="$apple_cc" -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=16.0
build_freetype wasm32-wasip1-threads -DCMAKE_SYSTEM_NAME=Generic -DCMAKE_SYSTEM_PROCESSOR=wasm32 \
    -DCMAKE_C_COMPILER="$llvm/clang" -DCMAKE_C_FLAGS="$wasm_cflags" \
    -DCMAKE_C_ARCHIVE_CREATE="<CMAKE_AR> --format=gnu qc <TARGET> <LINK_FLAGS> <OBJECTS>"
