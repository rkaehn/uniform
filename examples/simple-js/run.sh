#!/bin/sh
set -e
cd "$(dirname "$0")"

uniform=../../bin/macos-release/uniform

if [ "$1" = macos ]; then
    $uniform main.js
elif [ "$1" = ios ]; then
    $uniform --device="$DEVICE" main.js
elif [ "$1" = wasm ]; then
    $uniform --serve main.js
else
    echo "usage: $0 macos|ios|wasm"
    exit 1
fi
