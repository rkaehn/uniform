#include "cr.h"
#include "ui_blur_shared.h"

[[vk::binding(ui_blur_bind_params, 0)]] ConstantBuffer<ui_blur_scale_params> blur_params;
[[vk::binding(ui_blur_bind_src, 0)]] cr_tex_half4 blur_src;
[[vk::binding(ui_blur_bind_sampler, 0)]] SamplerState blur_sampler;
[[vk::binding(ui_blur_bind_dst, 0)]] [[vk::image_format("rgba16f")]] WTexture2D<float4> blur_dst;

static half4 ui_blur_sample_clamped(float2 uv) {
    return (half4)blur_src.SampleLevel(blur_sampler, clamp(uv, blur_params.uv_min, blur_params.uv_max), 0.0f);
}

static half4 ui_blur_scale_finish(half4 sum) {
    return blur_params.colorize != 0 ? (half4)blur_params.color * sum.w : sum * (half4)blur_params.color;
}

[shader("compute")]
[numthreads(8, 8, 1)]
void ui_blur_scale(uint3 dispatch_id: SV_DispatchThreadID) {
    uint2 tid = dispatch_id.xy;

    if (tid.x >= blur_params.dst_size.x || tid.y >= blur_params.dst_size.y) {
        return;
    }

    float2 uv = blur_params.uv_origin + (float2((float)tid.x, (float)tid.y) + 0.5f) * blur_params.uv_step;

    blur_dst.Store(blur_params.dst_origin + tid, (float4)ui_blur_scale_finish(ui_blur_sample_clamped(uv)));
}

[shader("compute")]
[numthreads(8, 8, 1)]
void ui_blur_down_direct(uint3 dispatch_id: SV_DispatchThreadID) {
    uint2 tid = dispatch_id.xy;

    if (tid.x >= blur_params.dst_size.x || tid.y >= blur_params.dst_size.y) {
        return;
    }

    uint32_t taps = blur_params.taps;
    float2 base = blur_params.uv_origin + (float2((float)(tid.x * taps), (float)(tid.y * taps)) + 0.5f) * blur_params.uv_step;

    float4 sum = float4(0.0f, 0.0f, 0.0f, 0.0f);
    for (uint32_t j = 0; j < taps * taps; j++) {
        float2 uv = base + float2((float)(j % taps), (float)(j / taps)) * blur_params.uv_step;
        sum += (float4)ui_blur_sample_clamped(uv);
    }

    half4 v = (half4)(sum * (1.0f / (float)(taps * taps)));
    blur_dst.Store(blur_params.dst_origin + tid, (float4)ui_blur_scale_finish(v));
}

#define ui_blur_up_tile 32
#define ui_blur_up_span (ui_blur_up_tile / 2 + 4)

#if cr_f16
#define ui_blur_up_lds_t half4

static half4 ui_blur_up_pack(half4 v) {
    return v;
}

static half4 ui_blur_up_unpack(half4 p) {
    return p;
}
#else
#define ui_blur_up_lds_t uint2

static uint2 ui_blur_up_pack(float4 v) {
    return uint2(f32tof16(v.x) | (f32tof16(v.y) << 16), f32tof16(v.z) | (f32tof16(v.w) << 16));
}

static float4 ui_blur_up_unpack(uint2 p) {
    return float4(f16tof32(p.x & 0xffffu), f16tof32(p.x >> 16), f16tof32(p.y & 0xffffu), f16tof32(p.y >> 16));
}
#endif

groupshared ui_blur_up_lds_t up_a[ui_blur_up_span * ui_blur_up_span];
groupshared ui_blur_up_lds_t up_b[ui_blur_up_span * ui_blur_up_span];

static half4 ui_blur_up_load(uint32_t src_b, uint32_t index) {
    return ui_blur_up_unpack(src_b != 0 ? up_b[index] : up_a[index]);
}

[shader("compute")]
[numthreads(256, 1, 1)]
void ui_blur_up_direct(uint3 group_id: SV_GroupID, uint32_t lindex: SV_GroupIndex) {
    uint32_t k = blur_params.levels;

    uint2 dims[8];
    int2 origins[8];
    dims[0] = blur_params.dst_size;
    origins[0] = int2((int32_t)(group_id.x * ui_blur_up_tile), (int32_t)(group_id.y * ui_blur_up_tile));
    uint2 extents[8];
    extents[0] = uint2(ui_blur_up_tile, ui_blur_up_tile);
    for (uint32_t l = 1; l <= k; l++) {
        dims[l] = uint2((dims[l - 1].x + 1u) >> 1, (dims[l - 1].y + 1u) >> 1);
        int2 end = origins[l - 1] + int2((int32_t)extents[l - 1].x, (int32_t)extents[l - 1].y);
        origins[l] = int2((origins[l - 1].x - 1) >> 1, (origins[l - 1].y - 1) >> 1);
        extents[l] = uint2((uint32_t)((end.x >> 1) + 1 - origins[l].x) + 1u, (uint32_t)((end.y >> 1) + 1 - origins[l].y) + 1u);
    }

    uint32_t src_b = 1;
    uint32_t cur = k;
    uint32_t stage = 0;

    while (cur > 0) {
        uint32_t target = cur == k && (k & 1u) != 0 ? cur - 1u : cur >= 2u ? cur - 2u : 0u;
        float inv = cur - target == 1u ? 0.5f : 0.25f;

        if (stage > 0) {
            GroupMemoryBarrierWithGroupSync();
        }

        uint2 span = target == 0 ? uint2(ui_blur_up_tile, ui_blur_up_tile) : extents[target];
        int2 base = target == 0 ? origins[0] : origins[target];

        for (uint32_t j = lindex; j < span.x * span.y; j += 256u) {
            int2 q = base + int2((int32_t)(j % span.x), (int32_t)(j / span.x));

            half4 qv;
            if (stage == 0) {
                float2 uv = blur_params.uv_origin + inv * (float2((float)q.x, (float)q.y) + 0.5f) * blur_params.uv_step;
                qv = ui_blur_sample_clamped(uv);
            } else {
                float2 qpos = clamp(inv * (float2((float)q.x, (float)q.y) + 0.5f) - 0.5f,
                                    float2(0.0f, 0.0f), float2((float)(dims[cur].x - 1u), (float)(dims[cur].y - 1u)));
                float2 qf = floor(qpos);
                float2 qw = qpos - qf;
                int2 q0 = clamp(int2(qf) - origins[cur], int2(0, 0), int2((int32_t)extents[cur].x - 1, (int32_t)extents[cur].y - 1));
                int2 q1 = clamp(int2(qf) + 1 - origins[cur], int2(0, 0), int2((int32_t)extents[cur].x - 1, (int32_t)extents[cur].y - 1));

                uint32_t qstride = extents[cur].x;
                half4 qv0 = lerp(ui_blur_up_load(src_b, (uint32_t)q0.y * qstride + (uint32_t)q0.x), ui_blur_up_load(src_b, (uint32_t)q0.y * qstride + (uint32_t)q1.x), (half)qw.x);
                half4 qv1 = lerp(ui_blur_up_load(src_b, (uint32_t)q1.y * qstride + (uint32_t)q0.x), ui_blur_up_load(src_b, (uint32_t)q1.y * qstride + (uint32_t)q1.x), (half)qw.x);
                qv = lerp(qv0, qv1, (half)qw.y);
            }

            if (target == 0) {
                if ((uint32_t)q.x < blur_params.dst_size.x && (uint32_t)q.y < blur_params.dst_size.y) {
                    blur_dst.Store(blur_params.dst_origin + uint2((uint32_t)q.x, (uint32_t)q.y), (float4)ui_blur_scale_finish(qv));
                }
            } else if (src_b != 0) {
                up_a[j] = ui_blur_up_pack(qv);
            } else {
                up_b[j] = ui_blur_up_pack(qv);
            }
        }

        src_b ^= 1u;
        cur = target;
        stage += 1;
    }
}
