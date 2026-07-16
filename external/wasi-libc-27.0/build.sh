#!/bin/sh
set -e
here="$(cd "$(dirname "$0")" && pwd)"
. "$here/../toolchain.sh"

fetch wasi-libc-27.0 https://github.com/WebAssembly/wasi-libc/archive/3f7eb4c7d6ed.tar.gz
src="$sources/wasi-libc-27.0"
cp "$src/LICENSE" "$src/LICENSE-APACHE" "$src/LICENSE-APACHE-LLVM" "$src/LICENSE-MIT" "$here/"

rm -rf "$src/build" "$src/sysroot"
make -s -C "$src" -j"$jobs" TARGET_TRIPLE=wasm32-wasip1-threads THREAD_MODEL=posix CC="$llvm/clang" AR="$llvm/llvm-ar" NM="$llvm/llvm-nm" EXTRA_CFLAGS="-O2 -DNDEBUG $wasm_features --no-default-config $prefix_map" no-check-symbols

output wasm32-wasip1-threads
cp -R "$src/sysroot/include/wasm32-wasip1-threads" "$out/include"
cp -R "$src/sysroot/lib/wasm32-wasip1-threads" "$out/lib"
publish
