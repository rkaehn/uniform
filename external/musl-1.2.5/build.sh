#!/bin/sh
set -e
here="$(cd "$(dirname "$0")" && pwd)"
. "$here/../toolchain.sh"

fetch musl-1.2.5 https://musl.libc.org/releases/musl-1.2.5.tar.gz
src="$sources/musl-1.2.5"
cp "$src/COPYRIGHT" "$here/LICENSE"

output linux-x86_64
cd "$src"
[ -f config.mak ] && make -s clean
./configure --disable-shared --disable-wrapper --prefix="$out" CC="$llvm/clang -target x86_64-linux-musl" AR="$llvm/llvm-ar" RANLIB="$llvm/llvm-ranlib" CFLAGS="-O2 $prefix_map" >/dev/null
make -s -j"$jobs"
make -s install
publish
