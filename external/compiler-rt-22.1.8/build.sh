#!/bin/sh
set -e
here="$(cd "$(dirname "$0")" && pwd)"
. "$here/../toolchain.sh"

fetch_llvm
src="$sources/llvm-project-22.1.8"
cp "$src/compiler-rt/LICENSE.TXT" "$here/LICENSE"
build="$src/build-compiler-rt-wasm32-wasip1-threads"

rm -rf "$build"
mkdir -p "$build/Platform"
echo 'set(WASI 1)' > "$build/Platform/WASI.cmake"
cmake -S "$src/compiler-rt" -B "$build" -G Ninja -Wno-dev \
    -DCMAKE_SYSTEM_NAME=WASI -DCMAKE_SYSTEM_VERSION=1 -DCMAKE_SYSTEM_PROCESSOR=wasm32 -DCMAKE_MODULE_PATH="$build" \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_FLAGS_RELEASE="-O2 -DNDEBUG" -DCMAKE_ASM_FLAGS_RELEASE="-O2 -DNDEBUG" \
    -DCMAKE_C_COMPILER="$llvm/clang" -DCMAKE_CXX_COMPILER="$llvm/clang++" -DCMAKE_ASM_COMPILER="$llvm/clang" \
    -DCMAKE_AR="$llvm/llvm-ar" -DCMAKE_RANLIB="$llvm/llvm-ranlib" -DCMAKE_NM="$llvm/llvm-nm" \
    -DCMAKE_C_ARCHIVE_CREATE="<CMAKE_AR> --format=gnu qc <TARGET> <LINK_FLAGS> <OBJECTS>" -DCMAKE_C_ARCHIVE_FINISH="<CMAKE_RANLIB> <TARGET>" \
    -DCMAKE_C_COMPILER_WORKS=ON -DCMAKE_CXX_COMPILER_WORKS=ON -DCMAKE_ASM_COMPILER_WORKS=ON \
    -DCMAKE_C_LINKER_DEPFILE_SUPPORTED=OFF -DCMAKE_CXX_LINKER_DEPFILE_SUPPORTED=OFF \
    -DCMAKE_SYSROOT="$wasm_sysroot" \
    -DCMAKE_C_COMPILER_TARGET=wasm32-wasip1-threads -DCMAKE_ASM_COMPILER_TARGET=wasm32-wasip1-threads \
    -DCMAKE_C_FLAGS="$wasm_cflags" -DCMAKE_ASM_FLAGS="-pthread -msimd128 -mexception-handling" \
    -DCMAKE_INSTALL_PREFIX="$build/install" \
    -DLLVM_CMAKE_DIR=/opt/homebrew/opt/llvm/lib/cmake/llvm -DLLVM_ENABLE_PER_TARGET_RUNTIME_DIR=ON \
    -DCOMPILER_RT_BAREMETAL_BUILD=ON -DCOMPILER_RT_DEFAULT_TARGET_ONLY=ON -DCOMPILER_RT_INCLUDE_TESTS=OFF -DCOMPILER_RT_HAS_FPIC_FLAG=OFF \
    -DCOMPILER_RT_BUILD_SANITIZERS=OFF -DCOMPILER_RT_BUILD_XRAY=OFF -DCOMPILER_RT_BUILD_LIBFUZZER=OFF -DCOMPILER_RT_BUILD_PROFILE=OFF \
    -DCOMPILER_RT_BUILD_CTX_PROFILE=OFF -DCOMPILER_RT_BUILD_MEMPROF=OFF -DCOMPILER_RT_BUILD_ORC=OFF -DCOMPILER_RT_BUILD_GWP_ASAN=OFF
cmake --build "$build"
cmake --install "$build" >/dev/null

output wasm32-wasip1-threads
mkdir "$out/lib"
cp "$build/install/lib/wasm32-unknown-wasip1-threads/libclang_rt.builtins.a" "$out/lib/"
publish
