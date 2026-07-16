#pragma once

#if defined(__OBJC__)
#define cr_objc 1
#else
#define cr_objc 0
#endif

#if defined(TARGET_OS_IOS)
#if TARGET_OS_IOS
#define cr_ios 1
#else
#define cr_ios 0
#endif
#else
#define cr_ios 0
#endif

#if cr_ios
#if defined(__IPHONE_18_0) && defined(__IPHONE_OS_VERSION_MIN_REQUIRED)
#if __IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_18_0
#define cr_ios_25 1
#else
#define cr_ios_25 0
#endif
#else
#define cr_ios_25 0
#endif
#else
#define cr_ios_25 0
#endif

#if defined(TARGET_OS_OSX)
#if TARGET_OS_OSX
#define cr_macos 1
#else
#define cr_macos 0
#endif
#else
#define cr_macos 0
#endif

#if cr_macos
#if defined(__MAC_15_0) && defined(__MAC_OS_X_VERSION_MIN_REQUIRED)
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= __MAC_15_0
#define cr_macos_25 1
#else
#define cr_macos_25 0
#endif
#else
#define cr_macos_25 0
#endif
#else
#define cr_macos_25 0
#endif

#if defined(__METAL_VERSION__)
#define cr_metal 1
#if __METAL_VERSION__ >= 320
#define cr_metal_25 1
#else
#define cr_metal_25 0
#endif
#else
#define cr_metal 0
#define cr_metal_25 0
#endif

#if defined(__SLANG__)
#define cr_slang 1
#else
#define cr_slang 0
#endif

#if cr_slang
#if !defined(cr_f16)
#if defined(cr_slang_wgsl)
#define cr_f16 0
#else
#define cr_f16 1
#endif
#endif
#else
#define cr_f16 0
#endif

#if cr_slang
#if cr_f16
#define cr_tex_half4 Texture2D<half4>
#else
#define cr_tex_half4 Texture2D<float4>
#define half float
#define half2 float2
#define half3 float3
#define half4 float4
#endif
#endif

#if cr_metal || cr_slang
#define cr_shader 1
#else
#define cr_shader 0
#endif

#if defined(__wasm__)
#define cr_wasm 1
#else
#define cr_wasm 0
#endif

#if cr_metal
#if cr_metal_25
#define printf os_log_default.log_debug
#else
#define printf(...)
#endif
#endif

#define cr_path_div '/'

#define char_bitcount (sizeof(char) * 8lu)
#define short_bitcount (sizeof(short) * 8lu)
#define int_bitcount (sizeof(int) * 8lu)
#if !cr_slang
#define long_bitcount (sizeof(long) * 8lu)
#define llong_bitcount (sizeof(long long) * 8lu)
#endif

#define char_min CHAR_MIN
#define short_min SHRT_MIN
#define int_min INT_MIN
#define long_min LONG_MIN
#define llong_min LLONG_MIN
#define char_max CHAR_MAX
#define short_max SHRT_MAX
#define int_max INT_MAX
#define long_max LONG_MAX
#define llong_max LLONG_MAX
#define uchar_max UCHAR_MAX
#define ushort_max USHRT_MAX
#define uint_max UINT_MAX
#define ulong_max ULONG_MAX
#define ullong_max ULLONG_MAX

#define int8_min INT8_MIN
#define int16_min INT16_MIN
#define int32_min INT32_MIN
#define int64_min INT64_MIN
#define int8_max INT8_MAX
#define int16_max INT16_MAX
#define int32_max INT32_MAX
#define int64_max INT64_MAX
#define uint8_max UINT8_MAX
#define uint16_max UINT16_MAX
#define uint32_max UINT32_MAX
#define uint64_max UINT64_MAX

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif

#define cr_assume_nonnull_begin _Pragma("clang assume_nonnull begin")
#define cr_assume_nonnull_end _Pragma("clang assume_nonnull end")
#define cr_value_as_error(v) int _(void)[v];
#define cr_type_as_error(t) __typeof__(t) _(void)[0];
#define cr_warning_push _Pragma("clang diagnostic push")
#define cr_warning_pop _Pragma("clang diagnostic pop")
#define cr_warning_ignore_pedantic _Pragma("clang diagnostic ignored \"-Wpedantic\"")
#define cr_warning_ignore_everything _Pragma("clang diagnostic ignored \"-Weverything\"")
#define cr_closed __attribute__((enum_extensibility(closed)))
#define cr_closed_flag __attribute__((enum_extensibility(closed),flag_enum))
#define cr_open __attribute__((enum_extensibility(open)))
#define cr_open_flag __attribute__((enum_extensibility(open),flag_enum))
#if cr_slang
#define cr_hdef [ForceInline]
#define cr_struct
#define cr_unroll [ForceUnroll]
#else
#define cr_hdef static __attribute__((unused))
#define cr_struct struct
#define cr_unroll
#endif
#define cr_likely(cond) __builtin_expect(!!(cond), 1)
#define cr_unlikely(cond) __builtin_expect(!!(cond), 0)
#if (cr_macos || cr_ios) && defined(__OBJC__)
#define cr_autoreleasepool @autoreleasepool
#else
#define cr_autoreleasepool
#endif
#define cr_count(arr) (sizeof(arr) / sizeof(*arr))
#define cr_export(ret, name, args) ret name args; __attribute__((export_name(#name))) ret name args
#define cr_js_import(name) __attribute__((import_module("env"), import_name(#name)))

#define cr_concat(_1, _2) cr_concat_helper(_1, _2)
#define cr_concat_helper(_1, _2) _1##_2
#define cr_concat3(_1, _2, _3) cr_concat(_1, cr_concat(_2, _3))
#define cr_concat4(_1, _2, _3, _4) cr_concat(cr_concat(_1, _2), cr_concat(_3, _4))
#define cr_as_string(_1) #_1
#define cr_as_string_2(_1) cr_as_string(_1)

#define cr_loc __FILE__ ":" cr_as_string_2(__LINE__)

#if !cr_shader
typedef void cr_check_handler_t(const char* message);
extern _Thread_local cr_check_handler_t* cr_check_handler;
__attribute__((noreturn, format(printf, 4, 5)))
void cr_check_fail(const char* func, const char* file, int line, const char* fmt, ...);
#define cr_check(cond, ...) ((void)(cr_likely(cond) ? 0 : (cr_check_fail(__func__, __FILE__, __LINE__, __VA_ARGS__), 0)))
#endif

#if cr_wasm
typedef int cr_wgpu_t;
#elif !cr_metal
typedef void* cr_wgpu_t;
#endif

cr_hdef int32_t min_int32(int32_t a, int32_t b) {
    return a < b ? a : b;
}

cr_hdef int32_t max_int32(int32_t a, int32_t b) {
    return a > b ? a : b;
}

#if !cr_slang
cr_hdef int64_t min_int64(int64_t a, int64_t b) {
    return a < b ? a : b;
}

cr_hdef int64_t max_int64(int64_t a, int64_t b) {
    return a > b ? a : b;
}
#endif

cr_hdef uint32_t min_uint32(uint32_t a, uint32_t b) {
    return a < b ? a : b;
}

cr_hdef uint32_t max_uint32(uint32_t a, uint32_t b) {
    return a > b ? a : b;
}

#if !cr_slang
cr_hdef uint64_t min_uint64(uint64_t a, uint64_t b) {
    return a < b ? a : b;
}

cr_hdef uint64_t max_uint64(uint64_t a, uint64_t b) {
    return a > b ? a : b;
}
#endif

cr_hdef float min_float(float a, float b) {
    return a < b ? a : b;
}

cr_hdef float max_float(float a, float b) {
    return a > b ? a : b;
}

#if !cr_shader
cr_hdef double min_double(double a, double b) {
    return a < b ? a : b;
}

cr_hdef double max_double(double a, double b) {
    return a > b ? a : b;
}
#endif

cr_hdef int32_t round_down_int32(int32_t n, int32_t multiple) {
    int32_t rem = n % multiple;
    if (rem < 0) {
        rem += multiple;
    }
    return n - rem;
}

cr_hdef int32_t round_up_int32(int32_t n, int32_t multiple) {
    int32_t rem = n % multiple;
    if (rem == 0) {
        return n;
    }
    if (rem < 0) {
        rem += multiple;
    }
    return n + multiple - rem;
}

cr_hdef int32_t round_down_po2_int32(int32_t n, int32_t po2) {
    int32_t mask = (1 << po2) - 1;
    return n & ~mask;
}

cr_hdef int32_t round_up_po2_int32(int32_t n, int32_t po2) {
    int32_t mask = (1 << po2) - 1;
    return (n + mask) & ~mask;
}

cr_hdef uint32_t align_up_uint32(uint32_t n, uint32_t align) {
    return (n + align - 1u) / align * align;
}

#if !cr_slang
cr_hdef uint64_t align_up_uint64(uint64_t n, uint64_t align) {
    return (n + align - 1llu) / align * align;
}
#endif

cr_hdef uint32_t align_up_po2_uint32(uint32_t n, int32_t po2) {
    uint32_t mask = (1u << po2) - 1u;
    return (n + mask) & ~mask;
}

#if !cr_slang
cr_hdef uint64_t align_up_po2_uint64(uint64_t n, int32_t po2) {
    uint64_t mask = (1llu << po2) - 1llu;
    return (n + mask) & ~mask;
}
#endif

cr_hdef uint32_t factor_up_uint32(uint32_t n, uint32_t align) {
    return (n + align - 1u) / align;
}

#if !cr_slang
cr_hdef uint64_t factor_up_uint64(uint64_t n, uint64_t align) {
    return (n + align - 1llu) / align;
}
#endif

cr_hdef int32_t clamp_int32(int32_t x, int32_t x_min, int32_t x_max) {
    if (x < x_min) return x_min;
    if (x > x_max) return x_max;
    return x;
}

cr_hdef float clamp_float(float x, float x_min, float x_max) {
    if (x < x_min) return x_min;
    if (x > x_max) return x_max;
    return x;
}

#if !cr_metal
cr_hdef double clamp_double(double x, double x_min, double x_max) {
    if (x < x_min) return x_min;
    if (x > x_max) return x_max;
    return x;
}
#endif

#if !cr_shader
cr_hdef int32_t popcount_uint32(uint32_t x) {
    static_assert(int_bitcount == 32, "");
    return __builtin_popcount(x);
}

cr_hdef int32_t popcount_uint64(uint64_t x) {
    static_assert(llong_bitcount == 64, "");
    return __builtin_popcountll(x);
}

cr_hdef int32_t ctz_uint32(uint32_t x) {
    static_assert(int_bitcount == 32, "");
    return x ? __builtin_ctz(x) : 32;
}

cr_hdef int32_t ctz_uint64(uint64_t x) {
    static_assert(llong_bitcount == 64, "");
    return x ? __builtin_ctzll(x) : 64;
}

cr_hdef int32_t clz_uint32(uint32_t x) {
    static_assert(int_bitcount == 32, "");
    return x ? __builtin_clz(x) : 32;
}

cr_hdef int32_t clz_uint64(uint64_t x) {
    static_assert(llong_bitcount == 64, "");
    return x ? __builtin_clzll(x) : 64;
}

cr_hdef uint32_t next_po2_uint32(uint32_t x) {
    if (x == 0) return 0;
    if (popcount_uint32(x) == 1) return x;
    assert(x <= (1u << 31));
    return 1u << (32 - clz_uint32(x));
}

cr_hdef uint64_t next_po2_uint64(uint64_t x) {
    if (x == 0) return 0;
    if (popcount_uint64(x) == 1) return x;
    assert(x <= (1llu << 63));
    return 1llu << (64 - clz_uint64(x));
}

cr_hdef uint32_t extract_bits_uint32(uint32_t x, int32_t offset, int32_t bit_count) {
    return (x >> offset) & ((1u << bit_count) - 1u);
}

cr_hdef uint64_t extract_bits_uint64(uint64_t x, int32_t offset, int32_t bit_count) {
    return (x >> offset) & ((1llu << bit_count) - 1llu);
}

cr_hdef uint32_t insert_bits_uint32(uint32_t x, uint32_t insert, int32_t offset, int32_t bit_count) {
    uint32_t mask = ((1u << bit_count) - 1u) << offset;
    x &= ~mask;
    x |= (insert << offset) & mask;
    return x;
}

cr_hdef uint64_t insert_bits_uint64(uint64_t x, uint64_t insert, int32_t offset, int32_t bit_count) {
    uint64_t mask = ((1llu << bit_count) - 1llu) << offset;
    x &= ~mask;
    x |= (insert << offset) & mask;
    return x;
}
#endif

