#!/bin/sh
set -e
here="$(cd "$(dirname "$0")" && pwd)"

for dep in musl-1.2.5 wasi-libc-27.0 compiler-rt-22.1.8 libcxx-22.1.8 freetype-2.14.1 harfbuzz-11.5.1 bearssl-0.6 yoga-9d159f2d; do
    sh "$here/$dep/build.sh"
done
