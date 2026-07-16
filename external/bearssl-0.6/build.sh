#!/bin/sh
set -e
here="$(cd "$(dirname "$0")" && pwd)"
. "$here/../toolchain.sh"

fetch bearssl-0.6 https://bearssl.org/bearssl-0.6.tar.gz
src="$sources/bearssl-0.6"
cp "$src/LICENSE.txt" "$here/LICENSE"

build_bearssl() {
    platform="$1"
    rm -rf "$src/build-$platform"
    mkdir -p "$src/build-$platform/obj"
    make -s -C "$src" -j"$jobs" BUILD="build-$platform" CC="$2" CFLAGS="$3 -O2 -W -Wall" AR="$llvm/llvm-ar" lib
    output "$platform"
    mkdir "$out/include" "$out/lib"
    cp "$src"/inc/*.h "$out/include/"
    cp "$src/build-$platform/libbearssl.a" "$out/lib/"
    publish
}

build_bearssl macos-aarch64 "$apple_cc" "$macos_cflags"
build_bearssl linux-x86_64 "$llvm/clang" "$linux_cflags"
