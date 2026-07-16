#!/bin/sh
set -e
here="$(cd "$(dirname "$0")" && pwd)"
. "$here/../toolchain.sh"

fetch yoga-9d159f2d https://github.com/facebook/yoga/archive/9d159f2d3d8dbc777e1f82c20b94e369c113d608.tar.gz
src="$sources/yoga-9d159f2d"
cp "$src/LICENSE" "$here/"

build_yoga() {
    platform="$1"
    shift
    build="$src/build-$platform"
    rm -rf "$build"
    output "$platform"
    cmake -S "$src/yoga" -B "$build" -G Ninja -Wno-dev \
        -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS_RELEASE="-O2 -DNDEBUG" \
        -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
        -DCMAKE_AR="$llvm/llvm-ar" -DCMAKE_RANLIB="$llvm/llvm-ranlib" \
        -DCMAKE_CXX_FLAGS="$prefix_map" \
        "$@"
    cmake --build "$build"
    mkdir -p "$out/include" "$out/lib"
    cp -R "$src/yoga" "$out/include/yoga"
    find "$out/include" -type f ! -name '*.h' -delete
    cp "$build/libyogacore.a" "$out/lib/"
    publish
}

build_yoga macos-aarch64 -DCMAKE_CXX_COMPILER="$apple_cxx" -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0
build_yoga ios-aarch64 -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_CXX_COMPILER="$apple_cxx" -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=16.0
build_yoga wasm32-wasip1-threads -DCMAKE_SYSTEM_NAME=Generic -DCMAKE_SYSTEM_PROCESSOR=wasm32 \
    -DCMAKE_CXX_COMPILER="$llvm/clang++" \
    -DCMAKE_CXX_ARCHIVE_CREATE="<CMAKE_AR> --format=gnu qc <TARGET> <LINK_FLAGS> <OBJECTS>" \
    -DCMAKE_CXX_COMPILE_OBJECT="<CMAKE_CXX_COMPILER> <DEFINES> <INCLUDES> <FLAGS> -fno-exceptions -fno-pic -o <OBJECT> -c <SOURCE>" \
    -DCMAKE_CXX_FLAGS="$wasm_cflags $wasm_cxxflags"
