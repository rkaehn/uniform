#!/bin/sh
set -e
cd "$(dirname "$0")"

uniform=../..
runtime=$uniform/bin/macos-release/uniform
wasm_clang=/opt/homebrew/opt/llvm/bin/clang
wasm_sysroot=$uniform/external/wasi-libc-27.0/wasm32-wasip1-threads
wasm_builtins=$uniform/external/compiler-rt-22.1.8/wasm32-wasip1-threads/lib

mkdir -p build/macos build/ios build/wasm

if [ "$1" = macos ]; then
    xcrun clang -O2 -I$uniform/include main.c $uniform/lib/macos-release/libuniform.a -framework AppKit -framework Metal -framework QuartzCore -o build/macos/simple
    build/macos/simple
elif [ "$1" = ios ]; then
    xcrun -sdk iphoneos clang -target arm64-apple-ios16.0 -O2 -I$uniform/include main.c $uniform/lib/ios-release/libuniform.a -framework Foundation -framework CoreGraphics -framework UIKit -framework Metal -framework QuartzCore -o build/ios/simple
    $runtime build.js --platform=ios --run --device="$DEVICE"
elif [ "$1" = wasm ]; then
    $wasm_clang --target=wasm32-wasip1-threads -pthread -msimd128 -O2 --sysroot=$wasm_sysroot -I$uniform/include -nostdlib main.c $uniform/lib/wasm-release/libuniform.a \
        -Wl,$wasm_sysroot/lib/crt1-reactor.o -Wl,--entry=_initialize -L$wasm_sysroot/lib -L$wasm_builtins -lc -lpthread -lsetjmp -lclang_rt.builtins \
        -Wl,--import-memory -Wl,--max-memory=2147483648 -Wl,--export-table -Wl,--export-if-defined=__main_argc_argv -Wl,--export=malloc -Wl,--export=free \
        -o build/wasm/index.wasm
    $runtime build.js --platform=wasm --run
else
    echo "usage: $0 macos|ios|wasm"
    exit 1
fi
