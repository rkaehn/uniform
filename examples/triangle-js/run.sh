#!/bin/sh
set -e
cd "$(dirname "$0")"

uniform=../../bin/macos-release/uniform

if [ "$1" = macos ]; then
    $uniform build.js --platform=macos --run
elif [ "$1" = ios ]; then
    $uniform build.js --platform=ios --run --device="$DEVICE"
elif [ "$1" = wasm ]; then
    $uniform build.js --platform=wasm --run
else
    echo "usage: $0 macos|ios|wasm"
    exit 1
fi
