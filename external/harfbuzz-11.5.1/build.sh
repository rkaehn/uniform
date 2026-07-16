#!/bin/sh
set -e
here="$(cd "$(dirname "$0")" && pwd)"
. "$here/../toolchain.sh"

fetch harfbuzz-11.5.1 https://github.com/harfbuzz/harfbuzz/releases/download/11.5.1/harfbuzz-11.5.1.tar.xz
src="$sources/harfbuzz-11.5.1"
cp "$src/COPYING" "$here/LICENSE"
grep -q HB_NO_OT_SHAPER_COMPLEX "$src/src/hb-ot-shaper.hh" || perl -0pi -e 's/(\n  switch \(\(hb_tag_t\) script\)\n)/\n#ifdef HB_NO_OT_SHAPER_COMPLEX\n  return &_hb_ot_shaper_default;\n#endif$1/' "$src/src/hb-ot-shaper.hh"
grep -q HB_NO_OT_SHAPER_COMPLEX "$src/src/hb-ot-shaper.hh"
lean="-DHAVE_CONFIG_OVERRIDE_H -I$here"

options="--buildtype=plain -Db_ndebug=true -Db_staticpic=false -Ddefault_library=static -Dfreetype=disabled \
    -Dglib=disabled -Dgobject=disabled -Dcairo=disabled -Dchafa=disabled -Dicu=disabled -Dgraphite=disabled -Dgraphite2=disabled \
    -Dfontations=disabled -Dgdi=disabled -Ddirectwrite=disabled -Dcoretext=disabled -Dharfrust=disabled -Dkbts=disabled -Dwasm=disabled \
    -Dtests=disabled -Ddocs=disabled -Dutilities=disabled -Dbenchmark=disabled -Dintrospection=disabled"

build_harfbuzz() {
    platform="$1"
    shift
    build="$src/build-$platform"
    rm -rf "$build"
    output "$platform"
    meson setup "$build" "$src" --prefix "$out" $options "$@"
    meson compile -C "$build"
    meson install -C "$build" --quiet
    rm -rf "$out/lib/libharfbuzz-subset.a" "$out/lib/pkgconfig"
    publish
}

cross_file() {
    cat > "$1" <<CROSS
[binaries]
c = $(meson_array "$2")
cpp = $(meson_array "$3")
ar = '$4'
ranlib = '$5'
pkg-config = '/opt/homebrew/bin/pkg-config'

[host_machine]
system = '$6'
cpu_family = '$7'
cpu = '$7'
endian = 'little'

[properties]
needs_exe_wrapper = true

[built-in options]
c_args = ['-O2']
cpp_args = $(meson_array "-O2 $8")
c_link_args = $(meson_array "$9")
cpp_link_args = $(meson_array "$9")
CROSS
}

CC="$apple_cc" CXX="$apple_cxx" AR="$llvm/llvm-ar" RANLIB="$llvm/llvm-ranlib" CFLAGS="$macos_cflags -O2" CXXFLAGS="$macos_cflags -O2 $lean" LDFLAGS="$macos_cflags" \
    build_harfbuzz macos-aarch64

cross_file "$src/ios-aarch64.txt" "$apple_cc $ios_cflags" "$apple_cxx $ios_cflags" "$llvm/llvm-ar" "$llvm/llvm-ranlib" darwin aarch64 "$lean" ""
build_harfbuzz ios-aarch64 --cross-file "$src/ios-aarch64.txt"

cross_file "$src/wasm32-wasip1-threads.txt" "$llvm/clang $wasm_cflags" "$llvm/clang++ $wasm_cflags" "$llvm/llvm-ar" "$llvm/llvm-ranlib" wasi wasm32 "$wasm_cxxflags -D_WASI_EMULATED_MMAN $lean" "$wasm_ldflags -lwasi-emulated-mman"
build_harfbuzz wasm32-wasip1-threads --cross-file "$src/wasm32-wasip1-threads.txt"
