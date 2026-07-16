#!/bin/sh
set -e
cd "$(dirname "$0")"

mkdir -p build
xcrun clang -I../../include build.c ../../lib/macos-release/libuniform.a -o build/bd

if [ "$1" = macos ]; then
    build/bd --platform=macos --run
elif [ "$1" = ios ]; then
    build/bd --platform=ios --run --device="$DEVICE"
elif [ "$1" = wasm ]; then
    build/bd --platform=wasm --run
else
    echo "usage: $0 macos|ios|wasm"
    exit 1
fi
