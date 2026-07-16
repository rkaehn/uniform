#include "ui_sdf_eval.h"

[[vk::binding(ui_sdf_bind_mid_tiles, 0)]] RWStructuredBuffer<uint32_t> ui_sdf_tiles;
[[vk::binding(ui_sdf_bind_mid_args, 0)]] RWStructuredBuffer<Atomic<uint32_t>> ui_sdf_args;
[[vk::binding(ui_sdf_bind_mid_prim_mid, 0)]] RWStructuredBuffer<float> ui_sdf_prim_mid;
[[vk::binding(ui_sdf_bind_mid_out, 0)]] [[vk::image_format("rgba16f")]] WTexture2D<float4> ui_sdf_out_sdf;

[shader("compute")]
[numthreads(8, 8, 1)]
void ui_sdf_mid(uint3 dispatch_id: SV_DispatchThreadID) {
    uint2 tid = dispatch_id.xy;

    if (tid.x >= ui_sdf_uniforms.mid_size.x || tid.y >= ui_sdf_uniforms.mid_size.y) {
        return;
    }

    float2 p = (float2(tid.x, tid.y) + 0.5f) * (float)ui_sdf_mid_scale;

    float bound = 0.0f;
    bool has_bound = false;

    if (ui_sdf_uniforms.use_top != 0) {
        uint2 top_tid = tid / (ui_sdf_top_scale / ui_sdf_mid_scale);
        float approx = ui_sdf_prim_mid[ui_sdf_uniforms.top_base + top_tid.y * ui_sdf_uniforms.top_size.x + top_tid.x];
        float2 p_top = (float2(top_tid.x, top_tid.y) + 0.5f) * (float)ui_sdf_top_scale;

        float dist_top = distance(p, p_top);

        if (ui_sdf_uniforms.exact_mid == 0 && !ui_sdf_near_band(approx, ui_sdf_uniforms.band_lo - dist_top, ui_sdf_uniforms.band_hi + dist_top)) {
            ui_sdf_out_sdf.Store(tid, float4(approx, 0.0f, 0.0f, 0.0f));
            return;
        }

        bound = abs(approx) + dist_top + ui_sdf_uniforms.smoothing;
        has_bound = true;
    }

    float3 f = float3(0.0f, 0.0f, 0.0f);
    uint32_t entry = (tid.y * ui_sdf_uniforms.mid_size.x + tid.x) * ui_sdf_uniforms.prim_count;

    for (uint32_t i = 0; i < ui_sdf_uniforms.prim_count; i++) {
        float v;
        float2 dir = float2(0.0f, 0.0f);
        if (ui_sdf_prims[i].type == ui_sdf_prim_gpu_type_circle) {
            v = ui_sdf_circle_distance_dir(p, i, dir);
        } else {
            float d_bound = ui_sdf_far;
            if (has_bound) {
                float prim_bound = bound + ui_sdf_prims[i].band_half;
                d_bound = prim_bound * prim_bound;
            }
            v = ui_sdf_path_distance_dir(p, i, d_bound, dir);
        }

        v = ui_sdf_prim_band_dir(i, v, dir);

        ui_sdf_prim_mid[entry + i] = v;
        f = ui_sdf_analytic_fold(ui_sdf_uniforms.combine, ui_sdf_uniforms.smoothing, i, f, float3(v, dir.x, dir.y));
    }

    ui_sdf_out_sdf.Store(tid, float4(f.x, f.y, f.z, 0.0f));

    if (ui_sdf_near_band(f.x, ui_sdf_uniforms.band_lo - ui_sdf_band_f16_margin, ui_sdf_uniforms.band_hi + ui_sdf_band_f16_margin)) {
        uint32_t idx = ui_sdf_args[0].add(1);
        ui_sdf_tiles[idx] = tid.x | (tid.y << 16);
    }
}
