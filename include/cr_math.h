#pragma once

#if cr_shader
#define atan2f atan2
#define acosf acos
#define sinf sin
#define cosf cos
#define powf pow
#define exp2f exp2
#define logf log
#define log2f log2
#define sqrtf sqrt
#define fabsf abs
#define M_2PI_F (2.0f * M_PI_F)
#define make_float2 float2
#define make_float3 float3
#define make_float4 float4
#define make_uint2 uint2
#else
#define M_E_F ((float)M_E)
#define M_LOG2E_F ((float)M_LOG2E)
#define M_LOG10E_F ((float)M_LOG10E)
#define M_LN2_F ((float)M_LN2)
#define M_LN10_F ((float)M_LN10)
#define M_PI_F ((float)M_PI)
#define M_PI_2_F ((float)M_PI_2)
#define M_PI_4_F ((float)M_PI_4)
#define M_1_PI_F ((float)M_1_PI)
#define M_2_PI_F ((float)M_2_PI)
#define M_2_SQRTPI_F ((float)M_2_SQRTPI)
#define M_SQRT2_F ((float)M_SQRT2)
#define M_SQRT1_2_F ((float)M_SQRT1_2)
#define M_2PI_F ((float)(2.0 * M_PI))
#endif

#if !cr_shader
typedef __attribute__((__ext_vector_type__(2))) int8_t char2;
typedef __attribute__((__ext_vector_type__(3))) int8_t char3;
typedef __attribute__((__ext_vector_type__(4))) int8_t char4;

typedef __attribute__((__ext_vector_type__(2))) uint8_t uchar2;
typedef __attribute__((__ext_vector_type__(3))) uint8_t uchar3;
typedef __attribute__((__ext_vector_type__(4))) uint8_t uchar4;
typedef __attribute__((__ext_vector_type__(16),__aligned__(16))) uint8_t uchar16;

typedef __attribute__((__ext_vector_type__(2))) int16_t short2;
typedef __attribute__((__ext_vector_type__(3))) int16_t short3;
typedef __attribute__((__ext_vector_type__(4))) int16_t short4;

typedef __attribute__((__ext_vector_type__(2))) uint16_t ushort2;
typedef __attribute__((__ext_vector_type__(3))) uint16_t ushort3;
typedef __attribute__((__ext_vector_type__(4))) uint16_t ushort4;

typedef __attribute__((__ext_vector_type__(2))) int32_t int2;
typedef __attribute__((__ext_vector_type__(3))) int32_t int3;
typedef __attribute__((__ext_vector_type__(4))) int32_t int4;

typedef __attribute__((__ext_vector_type__(2))) uint32_t uint2;
typedef __attribute__((__ext_vector_type__(3))) uint32_t uint3;
typedef __attribute__((__ext_vector_type__(4))) uint32_t uint4;

typedef __attribute__((__ext_vector_type__(2))) int64_t long2;
typedef __attribute__((__ext_vector_type__(3),__aligned__(16))) int64_t long3;
typedef __attribute__((__ext_vector_type__(4),__aligned__(16))) int64_t long4;

typedef __attribute__((__ext_vector_type__(2))) uint64_t ulong2;
typedef __attribute__((__ext_vector_type__(3),__aligned__(16))) uint64_t ulong3;
typedef __attribute__((__ext_vector_type__(4),__aligned__(16))) uint64_t ulong4;

typedef __attribute__((__ext_vector_type__(2))) float float2;
typedef __attribute__((__ext_vector_type__(3))) float float3;
typedef __attribute__((__ext_vector_type__(4))) float float4;

typedef __attribute__((__ext_vector_type__(2))) double double2;
typedef __attribute__((__ext_vector_type__(3),__aligned__(16))) double double3;
typedef __attribute__((__ext_vector_type__(4),__aligned__(16))) double double4;

cr_hdef inline char2 make_char2(int8_t x, int8_t y) {
    char2 v;
    v.x = x;
    v.y = y;
    return v;
}

cr_hdef inline char3 make_char3(int8_t x, int8_t y, int8_t z) {
    char3 v;
    v.x = x;
    v.y = y;
    v.z = z;
    return v;
}

cr_hdef inline char4 make_char4(int8_t x, int8_t y, int8_t z, int8_t w) {
    char4 v;
    v.x = x;
    v.y = y;
    v.z = z;
    v.w = w;
    return v;
}

cr_hdef inline uchar2 make_uchar2(uint8_t x, uint8_t y) {
    uchar2 v;
    v.x = x;
    v.y = y;
    return v;
}

cr_hdef inline uchar3 make_uchar3(uint8_t x, uint8_t y, uint8_t z) {
    uchar3 v;
    v.x = x;
    v.y = y;
    v.z = z;
    return v;
}

cr_hdef inline uchar4 make_uchar4(uint8_t x, uint8_t y, uint8_t z, uint8_t w) {
    uchar4 v;
    v.x = x;
    v.y = y;
    v.z = z;
    v.w = w;
    return v;
}

cr_hdef inline short2 make_short2(int16_t x, int16_t y) {
    short2 v;
    v.x = x;
    v.y = y;
    return v;
}

cr_hdef inline short3 make_short3(int16_t x, int16_t y, int16_t z) {
    short3 v;
    v.x = x;
    v.y = y;
    v.z = z;
    return v;
}

cr_hdef inline short4 make_short4(int16_t x, int16_t y, int16_t z, int16_t w) {
    short4 v;
    v.x = x;
    v.y = y;
    v.z = z;
    v.w = w;
    return v;
}

cr_hdef inline ushort2 make_ushort2(uint16_t x, uint16_t y) {
    ushort2 v;
    v.x = x;
    v.y = y;
    return v;
}

cr_hdef inline ushort3 make_ushort3(uint16_t x, uint16_t y, uint16_t z) {
    ushort3 v;
    v.x = x;
    v.y = y;
    v.z = z;
    return v;
}

cr_hdef inline ushort4 make_ushort4(uint16_t x, uint16_t y, uint16_t z, uint16_t w) {
    ushort4 v;
    v.x = x;
    v.y = y;
    v.z = z;
    v.w = w;
    return v;
}

cr_hdef inline int2 make_int2(int32_t x, int32_t y) {
    int2 v;
    v.x = x;
    v.y = y;
    return v;
}

cr_hdef inline int3 make_int3(int32_t x, int32_t y, int32_t z) {
    int3 v;
    v.x = x;
    v.y = y;
    v.z = z;
    return v;
}

cr_hdef inline int4 make_int4(int32_t x, int32_t y, int32_t z, int32_t w) {
    int4 v;
    v.x = x;
    v.y = y;
    v.z = z;
    v.w = w;
    return v;
}

cr_hdef inline uint2 make_uint2(uint32_t x, uint32_t y) {
    uint2 v;
    v.x = x;
    v.y = y;
    return v;
}

cr_hdef inline uint3 make_uint3(uint32_t x, uint32_t y, uint32_t z) {
    uint3 v;
    v.x = x;
    v.y = y;
    v.z = z;
    return v;
}

cr_hdef inline uint4 make_uint4(uint32_t x, uint32_t y, uint32_t z, uint32_t w) {
    uint4 v;
    v.x = x;
    v.y = y;
    v.z = z;
    v.w = w;
    return v;
}

cr_hdef inline long2 make_long2(int64_t x, int64_t y) {
    long2 v;
    v.x = x;
    v.y = y;
    return v;
}

cr_hdef inline long3 make_long3(int64_t x, int64_t y, int64_t z) {
    long3 v;
    v.x = x;
    v.y = y;
    v.z = z;
    return v;
}

cr_hdef inline long4 make_long4(int64_t x, int64_t y, int64_t z, int64_t w) {
    long4 v;
    v.x = x;
    v.y = y;
    v.z = z;
    v.w = w;
    return v;
}

cr_hdef inline ulong2 make_ulong2(uint64_t x, uint64_t y) {
    ulong2 v;
    v.x = x;
    v.y = y;
    return v;
}

cr_hdef inline ulong3 make_ulong3(uint64_t x, uint64_t y, uint64_t z) {
    ulong3 v;
    v.x = x;
    v.y = y;
    v.z = z;
    return v;
}

cr_hdef inline ulong4 make_ulong4(uint64_t x, uint64_t y, uint64_t z, uint64_t w) {
    ulong4 v;
    v.x = x;
    v.y = y;
    v.z = z;
    v.w = w;
    return v;
}

cr_hdef inline float2 make_float2(float x, float y) {
    float2 v;
    v.x = x;
    v.y = y;
    return v;
}

cr_hdef inline float3 make_float3(float x, float y, float z) {
    float3 v;
    v.x = x;
    v.y = y;
    v.z = z;
    return v;
}

cr_hdef inline float4 make_float4(float x, float y, float z, float w) {
    float4 v;
    v.x = x;
    v.y = y;
    v.z = z;
    v.w = w;
    return v;
}

cr_hdef inline double2 make_double2(double x, double y) {
    double2 v;
    v.x = x;
    v.y = y;
    return v;
}

cr_hdef inline double3 make_double3(double x, double y, double z) {
    double3 v;
    v.x = x;
    v.y = y;
    v.z = z;
    return v;
}

cr_hdef inline double4 make_double4(double x, double y, double z, double w) {
    double4 v;
    v.x = x;
    v.y = y;
    v.z = z;
    v.w = w;
    return v;
}

cr_hdef inline float length_float2(float2 a) {
    float x = sqrtf(a.x * a.x + a.y * a.y);
    return x;
}

cr_hdef inline double length_double2(double2 a) {
    double x = sqrt(a.x * a.x + a.y * a.y);
    return x;
}

cr_hdef inline bool all_int2(int2 v) {
    return v.x && v.y;
}

cr_hdef inline bool any_int2(int2 v) {
    return v.x || v.y;
}

cr_hdef inline bool all_int4(int4 v) {
    return v.x && v.y && v.z && v.w;
}

cr_hdef inline bool all_long2(long2 v) {
    return v.x && v.y;
}

cr_hdef inline bool all_long4(long4 v) {
    return v.x && v.y && v.z && v.w;
}

cr_hdef inline bool is_equal_double2(double2 a, double2 b) {
    return all_long2((long2)(a == b));
}

cr_hdef inline bool is_equal_float2(float2 a, float2 b) {
    return all_int2(a == b);
}

cr_hdef inline bool is_equal_float4(float4 a, float4 b) {
    return all_int4(a == b);
}

cr_hdef inline bool is_equal_double4(double4 a, double4 b) {
    return all_long4((long4)(a == b));
}

cr_hdef inline int2 min_int2(int2 a, int2 b) {
    int2 v;
    v.x = min_int32(a.x, b.x);
    v.y = min_int32(a.y, b.y);
    return v;
}

cr_hdef inline int2 max_int2(int2 a, int2 b) {
    int2 v;
    v.x = max_int32(a.x, b.x);
    v.y = max_int32(a.y, b.y);
    return v;
}

cr_hdef inline float2 min_float2(float2 a, float2 b) {
    float2 v;
    v.x = min_float(a.x, b.x);
    v.y = min_float(a.y, b.y);
    return v;
}

cr_hdef inline float2 max_float2(float2 a, float2 b) {
    float2 v;
    v.x = max_float(a.x, b.x);
    v.y = max_float(a.y, b.y);
    return v;
}

cr_hdef inline float2 aspect_fit_float2(float2 size, float2 bounds) {
    float scale = min_float(bounds.x / size.x, bounds.y / size.y);
    return size * scale;
}

cr_hdef inline float2 aspect_fill_float2(float2 size, float2 bounds) {
    float scale = max_float(bounds.x / size.x, bounds.y / size.y);
    return size * scale;
}

cr_hdef inline double2 aspect_fit_double2(double2 size, double2 bounds) {
    double scale = min_double(bounds.x / size.x, bounds.y / size.y);
    return size * scale;
}

cr_hdef inline double2 aspect_fill_double2(double2 size, double2 bounds) {
    double scale = max_double(bounds.x / size.x, bounds.y / size.y);
    return size * scale;
}

cr_hdef inline double2 min_double2(double2 a, double2 b) {
    double2 v;
    v.x = min_double(a.x, b.x);
    v.y = min_double(a.y, b.y);
    return v;
}

cr_hdef inline double2 max_double2(double2 a, double2 b) {
    double2 v;
    v.x = max_double(a.x, b.x);
    v.y = max_double(a.y, b.y);
    return v;
}

cr_hdef inline double2 floor_double2(double2 v) {
    return __builtin_elementwise_floor(v);
}

cr_hdef inline double2 round_double2(double2 v) {
    return __builtin_elementwise_round(v);
}

cr_hdef inline float2 to_float2_from_int2(int2 v) {
    return __builtin_convertvector(v, float2);
}

cr_hdef inline float2 to_float2_from_uint2(uint2 v) {
    return __builtin_convertvector(v, float2);
}

cr_hdef inline float2 to_float2_from_double2(double2 v) {
    return __builtin_convertvector(v, float2);
}

cr_hdef inline double2 to_double2_from_int2(int2 v) {
    return __builtin_convertvector(v, double2);
}

cr_hdef inline double2 to_double2_from_float2(float2 v) {
    return __builtin_convertvector(v, double2);
}

cr_hdef inline double4 to_double4_from_float4(float4 v) {
    return __builtin_convertvector(v, double4);
}

cr_hdef inline int2 to_int2_from_float2(float2 v) {
    return __builtin_convertvector(v, int2);
}

cr_hdef inline int2 to_int2_from_double2(double2 v) {
    return __builtin_convertvector(v, int2);
}

cr_hdef inline int2 to_int2_from_uint2(uint2 v) {
    return __builtin_convertvector(v, int2);
}

cr_hdef inline uint2 to_uint2_from_int2(int2 v) {
    return __builtin_convertvector(v, uint2);
}

cr_hdef inline float4 to_float4_from_double4(double4 v) {
    return __builtin_convertvector(v, float4);
}

typedef struct { float2 columns[2]; } float2x2;
typedef struct { float2 columns[3]; } float3x2;
typedef struct { float2 columns[4]; } float4x2;
typedef struct { float3 columns[2]; } float2x3;
typedef struct { float3 columns[3]; } float3x3;
typedef struct { float3 columns[4]; } float4x3;
typedef struct { float4 columns[2]; } float2x4;
typedef struct { float4 columns[3]; } float3x4;
typedef struct { float4 columns[4]; } float4x4;
typedef struct { double2 columns[2]; } double2x2;
typedef struct { double2 columns[3]; } double3x2;
typedef struct { double2 columns[4]; } double4x2;
typedef struct { double3 columns[2]; } double2x3;
typedef struct { double3 columns[3]; } double3x3;
typedef struct { double3 columns[4]; } double4x3;
typedef struct { double4 columns[2]; } double2x4;
typedef struct { double4 columns[3]; } double3x4;
typedef struct { double4 columns[4]; } double4x4;

#define float3x2_identity ((float3x2) { .columns[0] = { 1.0f, 0.0f }, .columns[1] = { 0.0f, 1.0f }, .columns[2] = { 0.0f, 0.0f } })
#define double3x2_identity ((double3x2) { .columns[0] = { 1.0, 0.0 }, .columns[1] = { 0.0, 1.0 }, .columns[2] = { 0.0, 0.0 } })

cr_hdef inline float3x2 make_float3x2(float2 x, float2 y, float2 z) {
    float3x2 m;
    m.columns[0] = x;
    m.columns[1] = y;
    m.columns[2] = z;
    return m;
}

cr_hdef inline double3x2 make_double3x2(double2 x, double2 y, double2 z) {
    double3x2 m;
    m.columns[0] = x;
    m.columns[1] = y;
    m.columns[2] = z;
    return m;
}

cr_hdef inline float3x2 to_float3x2_from_double3x2(double3x2 m) {
    float3x2 r;
    r.columns[0] = to_float2_from_double2(m.columns[0]);
    r.columns[1] = to_float2_from_double2(m.columns[1]);
    r.columns[2] = to_float2_from_double2(m.columns[2]);
    return r;
}

cr_hdef inline float3x3 make_float3x3(float3 x, float3 y, float3 z) {
    float3x3 m;
    m.columns[0] = x;
    m.columns[1] = y;
    m.columns[2] = z;
    return m;
}

cr_hdef inline bool is_equal_float3x2(float3x2 a, float3x2 b) {
    return is_equal_float2(a.columns[0], b.columns[0]) &&
           is_equal_float2(a.columns[1], b.columns[1]) &&
           is_equal_float2(a.columns[2], b.columns[2]);
}

cr_hdef inline bool is_zero_float3x2(float3x2 a) {
    float2 zero = make_float2(0.0f, 0.0f);
    return is_equal_float2(a.columns[0], zero) &&
           is_equal_float2(a.columns[1], zero) &&
           is_equal_float2(a.columns[2], zero);
}

cr_hdef inline float2 mul_float3x2_float2(float3x2 m, float2 v) {
    return v.x * m.columns[0] + v.y * m.columns[1] + m.columns[2];
}

cr_hdef inline float3x2 mul_float3x2(float3x2 a, float3x2 b) {
    float3x2 r;
    r.columns[0] = b.columns[0].x * a.columns[0] + b.columns[0].y * a.columns[1];
    r.columns[1] = b.columns[1].x * a.columns[0] + b.columns[1].y * a.columns[1];
    r.columns[2] = b.columns[2].x * a.columns[0] + b.columns[2].y * a.columns[1] + a.columns[2];
    return r;
}

cr_hdef inline float3x2 inverse_float3x2(float3x2 m) {
    float det = m.columns[0].x * m.columns[1].y - m.columns[0].y * m.columns[1].x;
    float inv_det = 1.0f / det;
    float3x2 r;
    r.columns[0] = make_float2(m.columns[1].y, -m.columns[0].y) * inv_det;
    r.columns[1] = make_float2(-m.columns[1].x, m.columns[0].x) * inv_det;
    r.columns[2] = -(m.columns[2].x * r.columns[0] + m.columns[2].y * r.columns[1]);
    return r;
}

cr_hdef inline bool is_equal_double3x2(double3x2 a, double3x2 b) {
    return is_equal_double2(a.columns[0], b.columns[0]) &&
           is_equal_double2(a.columns[1], b.columns[1]) &&
           is_equal_double2(a.columns[2], b.columns[2]);
}

cr_hdef inline bool is_zero_double3x2(double3x2 a) {
    double2 zero = make_double2(0.0, 0.0);
    return is_equal_double2(a.columns[0], zero) &&
           is_equal_double2(a.columns[1], zero) &&
           is_equal_double2(a.columns[2], zero);
}

cr_hdef inline float3 mul_float3x3_float3(float3x3 m, float3 v) {
    float3 result = make_float3(0.0f, 0.0f, 0.0f);
    result += m.columns[0] * v.x;
    result += m.columns[1] * v.y;
    result += m.columns[2] * v.z;
    return result;
}

struct cr_range_uint {
    uint32_t start;
    uint32_t end;
};

struct cr_rect_int32 {
    int2 origin;
    int2 size;
};

cr_hdef struct cr_rect_int32 cr_rect_int32_union(struct cr_rect_int32 a, struct cr_rect_int32 b) {
    int2 a_p2 = a.origin + a.size;
    int2 a_min = min_int2(a.origin, a_p2);
    int2 a_max = max_int2(a.origin, a_p2);

    int2 b_p2 = b.origin + b.size;
    int2 b_min = min_int2(b.origin, b_p2);
    int2 b_max = max_int2(b.origin, b_p2);

    if (a_min.x == a_max.x || a_min.y == a_max.y) {
        struct cr_rect_int32 res;
        res.origin = b_min;
        res.size = b_max - b_min;
        return res;
    }
    if (b_min.x == b_max.x || b_min.y == b_max.y) {
        struct cr_rect_int32 res;
        res.origin = a_min;
        res.size = a_max - a_min;
        return res;
    }

    int2 rect_origin = min_int2(a_min, b_min);
    int2 rect_end = max_int2(a_max, b_max);

    struct cr_rect_int32 rect;
    rect.origin = rect_origin;
    rect.size = rect_end - rect_origin;
    return rect;
}

cr_hdef struct cr_rect_int32 cr_rect_int32_intersect(struct cr_rect_int32 a, struct cr_rect_int32 b) {
    int2 a_p2 = a.origin + a.size;
    int2 a_min = min_int2(a.origin, a_p2);
    int2 a_max = max_int2(a.origin, a_p2);

    int2 b_p2 = b.origin + b.size;
    int2 b_min = min_int2(b.origin, b_p2);
    int2 b_max = max_int2(b.origin, b_p2);

    int2 rect_origin = max_int2(a_min, b_min);
    int2 rect_end = min_int2(a_max, b_max);

    struct cr_rect_int32 rect;
    rect.origin = rect_origin;
    rect.size = max_int2(rect_end - rect_origin, 0);

    return rect;
}

cr_hdef uint8_t to_uint8_from_float_clamped(float v) {
    float clamped = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
    return (uint8_t)(clamped * 255.0f + 0.5f);
}

cr_hdef float4 to_float4_from_uchar4(uchar4 v) {
    float f = 1.0f / 255.0f;
    return make_float4((float)v.x * f,
                       (float)v.y * f,
                       (float)v.z * f,
                       (float)v.w * f);
}

cr_hdef uchar4 to_uchar4_from_float4(float4 v) {
    return make_uchar4(to_uint8_from_float_clamped(v.x),
                       to_uint8_from_float_clamped(v.y),
                       to_uint8_from_float_clamped(v.z),
                       to_uint8_from_float_clamped(v.w));
}

#endif

cr_hdef float to_linear_srgb_from_srgb_float(float v) {
    if (v <= 0.04045f) {
        return v / 12.92f;
    } else {
        return powf((v + 0.055f) / 1.055f, 2.4f);
    }
}

cr_hdef float to_srgb_from_linear_srgb_float(float v) {
    if (v <= 0.0031308f) {
        return 12.92f * v;
    } else {
        return 1.055f * powf(v, 1.0f / 2.4f) - 0.055f;
    }
}

cr_hdef float3 to_linear_srgb_from_srgb_float3(float3 v) {
    return make_float3(to_linear_srgb_from_srgb_float(v.x),
                       to_linear_srgb_from_srgb_float(v.y),
                       to_linear_srgb_from_srgb_float(v.z));
}

cr_hdef float3 to_srgb_from_linear_srgb_float3(float3 v) {
    return make_float3(to_srgb_from_linear_srgb_float(v.x),
                       to_srgb_from_linear_srgb_float(v.y),
                       to_srgb_from_linear_srgb_float(v.z));
}

cr_hdef float to_linear_srgb_from_srgb_signed_float(float v) {
    float a = fabsf(v);
    float d = a <= 0.04045f ? a / 12.92f : powf((a + 0.055f) / 1.055f, 2.4f);
    return v < 0.0f ? -d : d;
}

cr_hdef float to_srgb_from_linear_srgb_signed_float(float v) {
    float a = fabsf(v);
    float e = a <= 0.0031308f ? 12.92f * a : 1.055f * powf(a, 1.0f / 2.4f) - 0.055f;
    return v < 0.0f ? -e : e;
}

cr_hdef float3 to_linear_srgb_from_srgb_signed_float3(float3 v) {
    return make_float3(to_linear_srgb_from_srgb_signed_float(v.x),
                       to_linear_srgb_from_srgb_signed_float(v.y),
                       to_linear_srgb_from_srgb_signed_float(v.z));
}

cr_hdef float3 to_srgb_from_linear_srgb_signed_float3(float3 v) {
    return make_float3(to_srgb_from_linear_srgb_signed_float(v.x),
                       to_srgb_from_linear_srgb_signed_float(v.y),
                       to_srgb_from_linear_srgb_signed_float(v.z));
}

#if cr_slang
cr_hdef half to_linear_srgb_from_srgb_signed_half(half v) {
    half a = abs(v);
    half d = a <= (half)0.04045f ? a / (half)12.92f : pow((a + (half)0.055f) / (half)1.055f, (half)2.4f);
    return v < (half)0.0f ? -d : d;
}

cr_hdef half to_srgb_from_linear_srgb_signed_half(half v) {
    half a = abs(v);
    half e = a <= (half)0.0031308f ? (half)12.92f * a : (half)1.055f * pow(a, (half)(1.0f / 2.4f)) - (half)0.055f;
    return v < (half)0.0f ? -e : e;
}

cr_hdef half3 to_linear_srgb_from_srgb_signed_half3(half3 v) {
    return half3(to_linear_srgb_from_srgb_signed_half(v.x),
                 to_linear_srgb_from_srgb_signed_half(v.y),
                 to_linear_srgb_from_srgb_signed_half(v.z));
}

cr_hdef half3 to_srgb_from_linear_srgb_signed_half3(half3 v) {
    return half3(to_srgb_from_linear_srgb_signed_half(v.x),
                 to_srgb_from_linear_srgb_signed_half(v.y),
                 to_srgb_from_linear_srgb_signed_half(v.z));
}
#endif

cr_hdef float4 premultiply_float4(float4 v) {
    return make_float4(v.x * v.w, v.y * v.w, v.z * v.w, v.w);
}

cr_hdef float4 unpremultiply_float4(float4 v) {
    float f = 1.0f / v.w;
    return v.w == 0.0f ? make_float4(0.0f, 0.0f, 0.0f, 0.0f) : make_float4(v.x * f, v.y * f, v.z * f, v.w);
}

cr_hdef float4 to_premultiplied_linear_srgb_from_srgb_float4(float4 v) {
    v.xyz = to_linear_srgb_from_srgb_float3(v.xyz);
    v = premultiply_float4(v);
    return v;
}

cr_hdef float4 to_srgb_from_premultiplied_linear_srgb_float4(float4 v) {
    v = unpremultiply_float4(v);
    v.xyz = to_srgb_from_linear_srgb_float3(v.xyz);
    return v;
}

cr_hdef float4 blend_normal_float4(float4 src, float4 dst) {
    return src + dst * (1.0f - src.a);
}

#define bayer_exp 4u

cr_hdef uint32_t bayer_uint32(uint32_t x, uint32_t y) {
    uint32_t res = 0;
    cr_unroll
    for (uint32_t i = 0; i < bayer_exp; i++) {
        uint32_t a = (x & 1) ^ (y & 1);
        uint32_t b = y & 1;
        res = ((res << 1 | a) << 1) | b;
        x >>= 1;
        y >>= 1;
    }
    return res;
}

cr_hdef float dither_offset(uint2 position) {
    float m = 1.0f / (float)(1u << (bayer_exp << 1));
    uint2 pos_mod = position & ((1u << bayer_exp) - 1);
    return (((float)bayer_uint32(pos_mod.x, pos_mod.y) + 0.5f) * m - 0.5f) / 255.0f;
}

cr_hdef float3 dithered(float3 c, uint2 position) {
    return c + dither_offset(position);
}

cr_hdef float4 dithered_float4(float4 c, uint2 position) {
    return c + dither_offset(position);
}
