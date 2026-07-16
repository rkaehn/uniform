#include "cr.h"
#include "ui_blur_shared.h"

[[vk::binding(ui_blur_bind_params, 0)]] ConstantBuffer<ui_blur_fused_params> blur_params;
[[vk::binding(ui_blur_bind_src, 0)]] cr_tex_half4 blur_src;
[[vk::binding(ui_blur_bind_sampler, 0)]] SamplerState blur_sampler;
[[vk::binding(ui_blur_bind_dst, 0)]] [[vk::image_format("rgba16f")]] WTexture2D<float4> blur_dst;

#define ui_blur_cache_rows (ui_blur_tile_h + 2 * ui_blur_max_extent)
#define ui_blur_thread_count (ui_blur_tile_w * ui_blur_tile_h / ui_blur_rows_per_thread)

#if cr_f16
groupshared half4 blur_cache[ui_blur_cache_rows][ui_blur_tile_w];

static half4 ui_blur_cache_pack(half4 v) {
    return v;
}

static half4 ui_blur_cache_unpack(half4 p) {
    return p;
}
#else
groupshared uint2 blur_cache[ui_blur_cache_rows][ui_blur_tile_w];

static uint2 ui_blur_cache_pack(float4 v) {
    return uint2(f32tof16(v.x) | (f32tof16(v.y) << 16), f32tof16(v.z) | (f32tof16(v.w) << 16));
}

static float4 ui_blur_cache_unpack(uint2 p) {
    return float4(f16tof32(p.x & 0xffffu), f16tof32(p.x >> 16), f16tof32(p.y & 0xffffu), f16tof32(p.y >> 16));
}
#endif

groupshared half blur_weights[4 * ui_blur_max_weight_vecs];

static half4 ui_blur_fused_fetch(float2 uv) {
    return (half4)blur_src.SampleLevel(blur_sampler, clamp(uv, blur_params.uv_min, blur_params.uv_max), 0.0f);
}

static half4 ui_blur_sample_h(float2 uv) {
    half4 sum = ui_blur_fused_fetch(uv) * (half)blur_params.taps[0].y;

    for (uint32_t k = 1; k < blur_params.tap_count; k++) {
        float4 pair = blur_params.taps[k / 2];
        float2 tap = (k & 1) == 0 ? pair.xy : pair.zw;

        float2 offset = float2(blur_params.uv_texel.x * tap.x, 0.0f);
        sum += (ui_blur_fused_fetch(uv + offset) + ui_blur_fused_fetch(uv - offset)) * (half)tap.y;
    }

    return sum;
}

[shader("compute")]
[numthreads(ui_blur_tile_w, ui_blur_tile_h / ui_blur_rows_per_thread, 1)]
void ui_blur_fused(uint3 group_id: SV_GroupID, uint3 group_tid: SV_GroupThreadID, uint32_t lindex: SV_GroupIndex) {
    if (lindex < ui_blur_max_weight_vecs) {
        float4 w = blur_params.weights[lindex];
        blur_weights[4 * lindex + 0] = (half)w.x;
        blur_weights[4 * lindex + 1] = (half)w.y;
        blur_weights[4 * lindex + 2] = (half)w.z;
        blur_weights[4 * lindex + 3] = (half)w.w;
    }

    int32_t radius = (int32_t)blur_params.radius;
    int2 tile_origin = int2((int32_t)(group_id.x * ui_blur_tile_w), (int32_t)(group_id.y * ui_blur_tile_h));

    uint32_t rows = ui_blur_tile_h + 2u * blur_params.radius;
    uint32_t fill_col = lindex & (ui_blur_tile_w - 1u);
    uint32_t fill_row = lindex / ui_blur_tile_w;

    for (uint32_t row = fill_row; row < rows; row += ui_blur_thread_count / ui_blur_tile_w) {
        int2 pos = tile_origin + int2((int32_t)fill_col, (int32_t)row - radius);
        float2 uv = blur_params.uv_origin + (float2(pos.x, pos.y) + 0.5f) * blur_params.uv_texel;
        blur_cache[row][fill_col] = ui_blur_cache_pack(ui_blur_sample_h(uv));
    }

    GroupMemoryBarrierWithGroupSync();

    uint32_t col = group_tid.x;
    int32_t y0 = (int32_t)(ui_blur_rows_per_thread * group_tid.y);

    int32_t m;

    half4 sums[ui_blur_rows_per_thread];
    cr_unroll
    for (m = 0; m < ui_blur_rows_per_thread; m++) {
        sums[m] = half4((half)0.0f, (half)0.0f, (half)0.0f, (half)0.0f);
    }

    for (int32_t k = 0; k < 2 * radius + ui_blur_rows_per_thread; k++) {
        half4 v = ui_blur_cache_unpack(blur_cache[y0 + k][col]);
        cr_unroll
        for (m = 0; m < ui_blur_rows_per_thread; m++) {
            sums[m] += v * blur_weights[abs(k - radius - m)];
        }
    }

    cr_unroll
    for (m = 0; m < ui_blur_rows_per_thread; m++) {
        half4 result = blur_params.colorize != 0 ? (half4)blur_params.color * sums[m].w : sums[m] * (half4)blur_params.color;
        uint2 px = uint2((uint32_t)(tile_origin.x + (int32_t)col), (uint32_t)(tile_origin.y + y0 + m));

        if (px.x < blur_params.dst_size.x && px.y < blur_params.dst_size.y) {
            blur_dst.Store(blur_params.dst_origin + px, (float4)result);
        }
    }
}
