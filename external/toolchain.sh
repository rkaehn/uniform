llvm=/opt/homebrew/opt/llvm/bin
sources="$HOME/Developer"
external="$(cd "$(dirname "$0")/.." && pwd)"
jobs="$(sysctl -n hw.ncpu)"
prefix_map="-ffile-prefix-map=$sources/="

apple_cc="$(xcrun -f clang)"
apple_cxx="$(xcrun -f clang++)"
macos_sdk="$(xcrun --sdk macosx --show-sdk-path)"
ios_sdk="$(xcrun --sdk iphoneos --show-sdk-path)"
macos_cflags="-arch arm64 -isysroot $macos_sdk -mmacosx-version-min=15.0 $prefix_map"
ios_cflags="-arch arm64 -isysroot $ios_sdk -miphoneos-version-min=16.0 $prefix_map"

linux_cflags="-target x86_64-linux-musl --sysroot=$external/musl-1.2.5/linux-x86_64 $prefix_map"

wasm_sysroot="$external/wasi-libc-27.0/wasm32-wasip1-threads"
wasm_features="-msimd128 -mllvm -wasm-enable-sjlj"
wasm_cflags="--target=wasm32-wasip1-threads -pthread $wasm_features --no-default-config --sysroot=$wasm_sysroot $prefix_map"
wasm_cxxflags="-nostdinc++ -isystem $external/libcxx-22.1.8/wasm32-wasip1-threads/include/c++/v1"
wasm_ldflags="-nostdlib -Wl,$wasm_sysroot/lib/crt1-command.o -L$wasm_sysroot/lib -L$external/compiler-rt-22.1.8/wasm32-wasip1-threads/lib -lc -lpthread -lclang_rt.builtins"

fetch() {
    [ -d "$sources/$1" ] && return
    staging="$sources/$1.fetch"
    rm -rf "$staging"
    mkdir -p "$staging"
    curl -sSfL "$2" -o "$staging/archive"
    tar xf "$staging/archive" -C "$staging" --strip-components 1
    rm "$staging/archive"
    mv "$staging" "$sources/$1"
}

fetch_llvm() {
    [ -d "$sources/llvm-project-22.1.8" ] && return
    staging="$sources/llvm-project-22.1.8.fetch"
    rm -rf "$staging"
    git clone --filter=blob:none --sparse --depth 1 --branch llvmorg-22.1.8 https://github.com/llvm/llvm-project.git "$staging"
    git -C "$staging" sparse-checkout set compiler-rt cmake runtimes libcxx libcxxabi llvm/cmake libc/shared libc/src/__support libc/hdr libc/include
    mv "$staging" "$sources/llvm-project-22.1.8"
}

output() {
    out="$here/$1.new"
    rm -rf "$out"
    mkdir -p "$out"
}

publish() {
    rm -rf "$out/lib/cmake" "$out/share"
    rm -rf "${out%.new}"
    mv "$out" "${out%.new}"
}

meson_array() {
    [ -n "$1" ] || { printf '[]'; return; }
    printf "['%s']" "$(printf '%s' "$1" | sed "s/ /', '/g")"
}
