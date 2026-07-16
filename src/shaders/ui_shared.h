#pragma once

#include "cr.h"

#define ui_bind_group_0_globals 0
#define ui_bind_group_0_uniforms 1
#define ui_bind_group_0_vertices 2
#define ui_bind_group_0_sdf 3
#define ui_bind_group_0_binding_count 4

#define ui_bind_group_1_texture 0
#define ui_bind_group_1_sampler 1
#define ui_bind_group_1_binding_count 2

#define ui_graphic_flag_r8_splat 1u
#define ui_graphic_flag_uv_wrap 2u
#define ui_graphic_flag_uv_region 4u
#define ui_graphic_flag_cs_decode 8u
#define ui_graphic_flag_cs_encode 16u
#define ui_graphic_flag_cs_convert 32u
#define ui_graphic_flag_dither 64u
#define ui_graphic_flag_premultiplied 128u
#define ui_graphic_flag_straight 256u

#define ui_sdf_mask_range 0.0f

struct ui_sdf_mask_prim {
    float4 meta;
    float4 pos;
    float4 cell_x;
    float4 cell_y;
    float4 coef[8];
    float4 _[4];
};

struct ui_globals {
    float2 to_clip_from_px_scale;
    float2 to_clip_from_px_offset;
};

struct ui_uniforms_solid {
    float2 transform_x;
    float2 transform_y;
    float2 transform_z;
    float2 inverse_x;
    float2 inverse_y;
    float2 inverse_z;
    uint32_t flags;
    uint32_t _;
    uint32_t _0;
    uint32_t _1;
    float4 color;
};

struct ui_uniforms_graphic {
    float2 transform_x;
    float2 transform_y;
    float2 transform_z;
    float2 inverse_x;
    float2 inverse_y;
    float2 inverse_z;
    float4 tint;
    float2 uv_origin;
    float2 uv_size;
    float2 uv_min;
    float2 uv_max;
    uint32_t flags;
    uint32_t _0;
    uint32_t _1;
    uint32_t _2;
    float4 cs_col_x;
    float4 cs_col_y;
    float4 cs_col_z;
};

struct ui_uniforms_glyph {
    uint32_t start_index;
    uint32_t flags;
    uint32_t tint_mode;
    uint32_t _0;
    float4 tint_color0;
    float4 tint_color1;
    float2 grad_dir;
    float grad_off;
    float _1;
    float2 transform_x;
    float2 transform_y;
    float2 transform_z;
    float2 _2;
};

struct ui_uniforms_final {
    uint32_t flags;
    uint32_t _0;
    uint32_t _1;
    uint32_t _2;
    float4 cs_col_x;
    float4 cs_col_y;
    float4 cs_col_z;
};

struct ui_vertex_solid {
    float2 pos_px;
    float sdf_ref;
    float _0;
};

struct ui_vertex_graphic {
    float2 pos_px;
    float2 uv;
    float sdf_ref;
    float _0;
};

struct ui_vertex_final {
    int2 pos_px;
    float2 uv;
};

struct ui_quad_glyph {
    float2 origin_px;
    uint32_t size_px;
    uint32_t origin_uv_px;
    uint32_t total_uv_px;
    uint32_t _0;
    uint32_t _1;
    uint32_t _2;
    float4 color;
};

#if cr_shader
cr_hdef float2 ui_graphic_uv(uint32_t flags, float2 uv, float2 uv_origin, float2 uv_size, float2 uv_min, float2 uv_max) {
    if ((flags & ui_graphic_flag_uv_wrap) != 0) {
        uv = uv_origin + frac(uv) * uv_size;
    } else if ((flags & ui_graphic_flag_uv_region) != 0) {
        uv = uv_origin + clamp(uv, 0.0f, 1.0f) * uv_size;
        uv = clamp(uv, uv_min, uv_max);
    }
    return uv;
}
#endif

cr_hdef float4 ui_graphic_premultiply(float4 src, uint32_t flags) {
    if ((flags & ui_graphic_flag_straight) != 0) {
        src.xyz = src.xyz * src.w;
    }
    return src;
}

cr_hdef float4 ui_dither_premult(float4 c, uint32_t flags, float2 frag_pos) {
    if ((flags & ui_graphic_flag_dither) != 0) {
        c = dithered_float4(c, make_uint2((uint32_t)frag_pos.x, (uint32_t)frag_pos.y));
    }
    return c;
}

cr_hdef float4 ui_cs_convert_premult(float4 src, uint32_t flags, float4 cs_col_x, float4 cs_col_y, float4 cs_col_z) {
    float a = src.w;
    float3 v = a > 0.0f ? src.xyz / a : make_float3(0.0f, 0.0f, 0.0f);
    if ((flags & ui_graphic_flag_cs_decode) != 0) {
        v = to_linear_srgb_from_srgb_signed_float3(v);
    }
    v = cs_col_x.xyz * v.x + cs_col_y.xyz * v.y + cs_col_z.xyz * v.z;
    if ((flags & ui_graphic_flag_cs_encode) != 0) {
        v = to_srgb_from_linear_srgb_signed_float3(v);
    }
    return make_float4(v.x * a, v.y * a, v.z * a, a);
}

#if cr_slang
cr_hdef half4 ui_graphic_premultiply_h(half4 src, uint32_t flags) {
    if ((flags & ui_graphic_flag_straight) != 0) {
        src.xyz = src.xyz * src.w;
    }
    return src;
}

cr_hdef half4 ui_cs_convert_premult_h(half4 src, uint32_t flags, float4 cs_col_x, float4 cs_col_y, float4 cs_col_z) {
    half a = src.w;
    half3 v = a > (half)0.0f ? src.xyz / a : half3((half)0.0f, (half)0.0f, (half)0.0f);
    if ((flags & ui_graphic_flag_cs_decode) != 0) {
        v = to_linear_srgb_from_srgb_signed_half3(v);
    }
    v = (half3)cs_col_x.xyz * v.x + (half3)cs_col_y.xyz * v.y + (half3)cs_col_z.xyz * v.z;
    if ((flags & ui_graphic_flag_cs_encode) != 0) {
        v = to_srgb_from_linear_srgb_signed_half3(v);
    }
    return half4(v * a, a);
}
#endif

