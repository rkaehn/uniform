#pragma once

#if !defined(__METAL_VERSION__) && !defined(__SLANG__)
#include <assert.h>
#ifdef NDEBUG
#error asserts must stay enabled in every build profile
#endif
#if defined(__wasm__) && !defined(__wasm_simd128__)
#error wasm code must be compiled with -msimd128 to match the library's calling convention
#endif
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#endif

#if defined(__METAL_VERSION__) || defined(__SLANG__)
#include "cr_def.h"
#include "cr_math.h"
#else
#include "cr_def.h"
#include "cr_math.h"
#include "cr_mem.h"
#include "cr_str.h"
#include "cr_util.h"
#include "cr_task.h"
#include "cr_hash.h"
#endif
