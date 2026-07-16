#include "cr.h"
#include "ui_blur_shared.h"
#include "ui_blur_down_shared.h"

[[vk::binding(ui_blur_bind_dst, 0)]] [[vk::image_format("rgba16f")]] WTexture2D<float4> down_dst;
[[vk::binding(ui_blur_bind_dst2, 0)]] [[vk::image_format("rgba16f")]] WTexture2D<float4> down_dst2;
[[vk::binding(ui_blur_bind_dst3, 0)]] [[vk::image_format("rgba16f")]] WTexture2D<float4> down_dst3;
[[vk::binding(ui_blur_bind_dst4, 0)]] [[vk::image_format("rgba16f")]] WTexture2D<float4> down_dst4;

#if cr_f16
#define ui_blur_lds_t half4

static half4 ui_blur_lds_pack(half4 v) {
    return v;
}

static half4 ui_blur_lds_unpack(half4 p) {
    return p;
}
#else
#define ui_blur_lds_t uint2

static uint2 ui_blur_lds_pack(float4 v) {
    return uint2(f32tof16(v.x) | (f32tof16(v.y) << 16), f32tof16(v.z) | (f32tof16(v.w) << 16));
}

static float4 ui_blur_lds_unpack(uint2 p) {
    return float4(f16tof32(p.x & 0xffffu), f16tof32(p.x >> 16), f16tof32(p.y & 0xffffu), f16tof32(p.y >> 16));
}
#endif

static void ui_blur_level_store(uint32_t level, uint2 p, half4 v) {
    if (level == 0) {
        down_dst.Store(p, (float4)v);
    } else if (level == 1) {
        down_dst2.Store(p, (float4)v);
    } else if (level == 2) {
        down_dst3.Store(p, (float4)v);
    } else {
        down_dst4.Store(p, (float4)v);
    }
}

groupshared ui_blur_lds_t pair_cache[ui_blur_pair_span][ui_blur_pair_span];

static half4 ui_blur_pair_tap(int2 lds_base, uint2 src_dims, float2 pos) {
    float2 p = clamp(pos, float2(0.5f, 0.5f), float2((float)src_dims.x, (float)src_dims.y) - 0.5f);
    float2 t = p - 0.5f;
    float2 f = floor(t);
    float2 w = t - f;

    int2 i0 = clamp(int2(f) - lds_base, int2(0, 0), int2(ui_blur_pair_span - 1, ui_blur_pair_span - 1));
    int2 i1 = clamp(int2(f) - lds_base + 1, int2(0, 0), int2(ui_blur_pair_span - 1, ui_blur_pair_span - 1));

    half4 v0 = lerp(ui_blur_lds_unpack(pair_cache[i0.y][i0.x]), ui_blur_lds_unpack(pair_cache[i0.y][i1.x]), (half)w.x);
    half4 v1 = lerp(ui_blur_lds_unpack(pair_cache[i1.y][i0.x]), ui_blur_lds_unpack(pair_cache[i1.y][i1.x]), (half)w.x);
    return lerp(v0, v1, (half)w.y);
}

static half4 ui_blur_pair_eval(int2 lds_base, uint2 src_dims, float2 center) {
    const float o1 = ui_blur_down_o1;
    const float o2 = ui_blur_down_o2;

    half4 acc = (half)ui_blur_down_w0 * ui_blur_pair_tap(lds_base, src_dims, center);
    acc += (half)ui_blur_down_w1 * (ui_blur_pair_tap(lds_base, src_dims, center + float2(o1, 0.0f))
                                 + ui_blur_pair_tap(lds_base, src_dims, center - float2(o1, 0.0f))
                                 + ui_blur_pair_tap(lds_base, src_dims, center + float2(0.0f, o1))
                                 + ui_blur_pair_tap(lds_base, src_dims, center - float2(0.0f, o1)));
    acc += (half)ui_blur_down_w2 * (ui_blur_pair_tap(lds_base, src_dims, center + float2(o1, o1))
                                 + ui_blur_pair_tap(lds_base, src_dims, center + float2(-o1, o1))
                                 + ui_blur_pair_tap(lds_base, src_dims, center + float2(o1, -o1))
                                 + ui_blur_pair_tap(lds_base, src_dims, center - float2(o1, o1)));
    acc += (half)ui_blur_down_w3 * (ui_blur_pair_tap(lds_base, src_dims, center + float2(o2, 0.0f))
                                 + ui_blur_pair_tap(lds_base, src_dims, center - float2(o2, 0.0f))
                                 + ui_blur_pair_tap(lds_base, src_dims, center + float2(0.0f, o2))
                                 + ui_blur_pair_tap(lds_base, src_dims, center - float2(0.0f, o2)));
    return acc;
}

[shader("compute")]
[numthreads(ui_blur_pair_tile, ui_blur_pair_tile, 1)]
void ui_blur_down_pair(uint3 group_id: SV_GroupID, uint3 group_tid: SV_GroupThreadID, uint32_t lindex: SV_GroupIndex) {
    uint2 tile1 = uint2(group_id.x * ui_blur_pair_tile, group_id.y * ui_blur_pair_tile);
    int2 tile0 = int2((int32_t)(2u * tile1.x), (int32_t)(2u * tile1.y));
    int2 lds_base = tile0 - 4;

    bool last_x = tile1.x + ui_blur_pair_tile >= down_params.dst1_size.x;
    bool last_y = tile1.y + ui_blur_pair_tile >= down_params.dst1_size.y;

    if (down_params.mode == ui_blur_down_mode_tap) {
        uint32_t n = down_params.box_levels;
        uint32_t block = 1u << n;
        int2 base0 = int2(tile0.x << n, tile0.y << n);
        uint2 write_end0 = uint2(
            last_x ? down_params.dst0_size.x : (uint32_t)(base0.x + (int32_t)(2u * ui_blur_pair_tile << n)),
            last_y ? down_params.dst0_size.y : (uint32_t)(base0.y + (int32_t)(2u * ui_blur_pair_tile << n)));

        for (uint32_t i = lindex; i < ui_blur_pair_span * ui_blur_pair_span; i += ui_blur_pair_tile * ui_blur_pair_tile) {
            int2 slot = int2((int32_t)(i % ui_blur_pair_span), (int32_t)(i / ui_blur_pair_span));
            int2 t = lds_base + slot;
            int2 u0 = int2(t.x << n, t.y << n);

            float4 sum = float4(0.0f, 0.0f, 0.0f, 0.0f);
            for (uint32_t j = 0; j < block * block; j++) {
                int2 u = u0 + int2((int32_t)(j % block), (int32_t)(j / block));
                half4 s = ui_blur_down_eval(float2((float)(2 * u.x + 1), (float)(2 * u.y + 1)));
                sum += (float4)s;

                if (u.x >= base0.x && (uint32_t)u.x < write_end0.x && u.y >= base0.y && (uint32_t)u.y < write_end0.y) {
                    down_dst.Store(down_params.dst_origin + uint2((uint32_t)u.x, (uint32_t)u.y), (float4)s);
                }
            }

            pair_cache[slot.y][slot.x] = ui_blur_lds_pack((half4)(sum * (1.0f / (float)(block * block))));
        }
    } else {
        uint2 write_end = uint2(
            last_x ? down_params.dst_size.x : (uint32_t)tile0.x + 2u * ui_blur_pair_tile,
            last_y ? down_params.dst_size.y : (uint32_t)tile0.y + 2u * ui_blur_pair_tile);

        for (uint32_t i = lindex; i < ui_blur_pair_span * ui_blur_pair_span; i += ui_blur_pair_tile * ui_blur_pair_tile) {
            int2 slot = int2((int32_t)(i % ui_blur_pair_span), (int32_t)(i / ui_blur_pair_span));
            int2 t = lds_base + slot;
            half4 v = ui_blur_down_eval(float2((float)(2 * t.x + 1), (float)(2 * t.y + 1)));
            pair_cache[slot.y][slot.x] = ui_blur_lds_pack(v);

            if (t.x >= tile0.x && (uint32_t)t.x < write_end.x && t.y >= tile0.y && (uint32_t)t.y < write_end.y) {
                down_dst.Store(down_params.dst_origin + uint2((uint32_t)t.x, (uint32_t)t.y), (float4)v);
            }
        }
    }

    GroupMemoryBarrierWithGroupSync();

    uint2 p = tile1 + group_tid.xy;
    if (p.x < down_params.dst1_size.x && p.y < down_params.dst1_size.y) {
        float2 center = float2((float)(2u * p.x + 1u), (float)(2u * p.y + 1u));
        half4 v = ui_blur_pair_eval(lds_base, down_params.dst_size, center);
        down_dst2.Store(down_params.dst1_origin + p, (float4)v);
    }
}

groupshared ui_blur_lds_t tail_a[ui_blur_tail_max_px];
groupshared ui_blur_lds_t tail_b[ui_blur_tail_max_px / 4];

static half4 ui_blur_tail_load(uint32_t src_b, uint2 dims, int2 i) {
    uint32_t index = (uint32_t)i.y * dims.x + (uint32_t)i.x;
    return ui_blur_lds_unpack(src_b != 0 ? tail_b[index] : tail_a[index]);
}

static half4 ui_blur_tail_tap(uint32_t src_b, uint2 dims, float2 pos) {
    float2 p = clamp(pos, float2(0.5f, 0.5f), float2((float)dims.x, (float)dims.y) - 0.5f);
    float2 t = p - 0.5f;
    float2 f = floor(t);
    float2 w = t - f;

    int2 i0 = clamp(int2(f), int2(0, 0), int2((int32_t)dims.x - 1, (int32_t)dims.y - 1));
    int2 i1 = clamp(int2(f) + 1, int2(0, 0), int2((int32_t)dims.x - 1, (int32_t)dims.y - 1));

    half4 v0 = lerp(ui_blur_tail_load(src_b, dims, int2(i0.x, i0.y)), ui_blur_tail_load(src_b, dims, int2(i1.x, i0.y)), (half)w.x);
    half4 v1 = lerp(ui_blur_tail_load(src_b, dims, int2(i0.x, i1.y)), ui_blur_tail_load(src_b, dims, int2(i1.x, i1.y)), (half)w.x);
    return lerp(v0, v1, (half)w.y);
}

static half4 ui_blur_tail_eval(uint32_t src_b, uint2 dims, float2 center) {
    const float o1 = ui_blur_down_o1;
    const float o2 = ui_blur_down_o2;

    half4 acc = (half)ui_blur_down_w0 * ui_blur_tail_tap(src_b, dims, center);
    acc += (half)ui_blur_down_w1 * (ui_blur_tail_tap(src_b, dims, center + float2(o1, 0.0f))
                                 + ui_blur_tail_tap(src_b, dims, center - float2(o1, 0.0f))
                                 + ui_blur_tail_tap(src_b, dims, center + float2(0.0f, o1))
                                 + ui_blur_tail_tap(src_b, dims, center - float2(0.0f, o1)));
    acc += (half)ui_blur_down_w2 * (ui_blur_tail_tap(src_b, dims, center + float2(o1, o1))
                                 + ui_blur_tail_tap(src_b, dims, center + float2(-o1, o1))
                                 + ui_blur_tail_tap(src_b, dims, center + float2(o1, -o1))
                                 + ui_blur_tail_tap(src_b, dims, center - float2(o1, o1)));
    acc += (half)ui_blur_down_w3 * (ui_blur_tail_tap(src_b, dims, center + float2(o2, 0.0f))
                                 + ui_blur_tail_tap(src_b, dims, center - float2(o2, 0.0f))
                                 + ui_blur_tail_tap(src_b, dims, center + float2(0.0f, o2))
                                 + ui_blur_tail_tap(src_b, dims, center - float2(0.0f, o2)));
    return acc;
}

[shader("compute")]
[numthreads(ui_blur_tail_threads, 1, 1)]
void ui_blur_down_tail(uint32_t lindex: SV_GroupIndex) {
    uint2 dims0 = down_params.dst_size;

    for (uint32_t i = lindex; i < dims0.x * dims0.y; i += ui_blur_tail_threads) {
        uint2 t = uint2(i % dims0.x, i / dims0.x);
        half4 v = ui_blur_down_eval(float2((float)(2u * t.x + 1u), (float)(2u * t.y + 1u)));
        tail_a[i] = ui_blur_lds_pack(v);
        down_dst.Store(down_params.dst_origin + t, (float4)v);
    }

    uint2 src_dims = dims0;
    uint32_t src_b = 0;

    for (uint32_t level = 1; level < down_params.level_count; level++) {
        uint2 dims = uint2(max((src_dims.x + 1u) >> 1, 1u), max((src_dims.y + 1u) >> 1, 1u));

        GroupMemoryBarrierWithGroupSync();

        for (uint32_t i = lindex; i < dims.x * dims.y; i += ui_blur_tail_threads) {
            uint2 p = uint2(i % dims.x, i / dims.x);
            float2 center = float2((float)(2u * p.x + 1u), (float)(2u * p.y + 1u));
            half4 v = ui_blur_tail_eval(src_b, src_dims, center);

            if (src_b != 0) {
                tail_a[i] = ui_blur_lds_pack(v);
            } else {
                tail_b[i] = ui_blur_lds_pack(v);
            }

            uint2 level_origin = uint2(down_params.dst_origin.x >> level, down_params.dst_origin.y >> level);
            ui_blur_level_store(level, level_origin + p, v);
        }

        src_dims = dims;
        src_b ^= 1u;
    }
}
