#include "ui_sdf_eval.h"

[[vk::binding(ui_sdf_bind_refine_tiles, 0)]] StructuredBuffer<uint32_t> ui_sdf_tiles;
[[vk::binding(ui_sdf_bind_refine_prim_mid, 0)]] StructuredBuffer<float> ui_sdf_prim_mid;
[[vk::binding(ui_sdf_bind_refine_out, 0)]] [[vk::image_format("rgba16f")]] WTexture2D<float4> ui_sdf_out_sdf;

groupshared Atomic<uint32_t> ui_sdf_list_count;
groupshared uint32_t ui_sdf_list[ui_sdf_refine_list_size];
groupshared uint32_t ui_sdf_list_pos[ui_sdf_max_prims + 1];

groupshared float ui_sdf_center_d[ui_sdf_max_prims];

[shader("compute")]
[numthreads(ui_sdf_mid_scale, ui_sdf_mid_scale, 1)]
void ui_sdf_refine(uint3 group_id: SV_GroupID, uint3 group_tid: SV_GroupThreadID, uint32_t lindex: SV_GroupIndex) {
    uint32_t tile = ui_sdf_tiles[group_id.x];
    uint2 tile_xy = uint2(tile & 0xffffu, tile >> 16);
    uint2 lid = group_tid.xy;

    uint2 tid = tile_xy * ui_sdf_mid_scale + lid;
    float2 p = float2(tid.x, tid.y) + 0.5f;
    float2 center = (float2(tile_xy.x, tile_xy.y) + 0.5f) * (float)ui_sdf_mid_scale;

    if (lindex == 0) {
        ui_sdf_list_count.store(0);
        ui_sdf_list_pos[0] = 0;
    }
    if (lindex < ui_sdf_uniforms.prim_count) {
        ui_sdf_center_d[lindex] = ui_sdf_prim_mid[(tile_xy.y * ui_sdf_uniforms.mid_size.x + tile_xy.x) * ui_sdf_uniforms.prim_count + lindex];
    }
    GroupMemoryBarrierWithGroupSync();

    uint32_t i;
    uint32_t skip_mask = 0;
    for (i = 0; i < ui_sdf_uniforms.prim_count; i++) {
        float t = ui_sdf_uniforms.combine == ui_sdf_combine_subtract && i > 0 ? -ui_sdf_center_d[i] : ui_sdf_center_d[i];
        bool skip = ui_sdf_uniforms.combine == ui_sdf_combine_union
            ? t - ui_sdf_tile_radius > ui_sdf_uniforms.band_hi + ui_sdf_band_margin + ui_sdf_uniforms.smoothing
            : t + ui_sdf_tile_radius < ui_sdf_uniforms.band_lo - ui_sdf_band_margin - ui_sdf_uniforms.smoothing;
        skip_mask |= skip ? 1u << i : 0u;
    }

    for (i = 0; i < ui_sdf_uniforms.prim_count; i++) {
        if (ui_sdf_prims[i].type == ui_sdf_prim_gpu_type_path && abs(ui_sdf_center_d[i]) > ui_sdf_tile_radius && ((skip_mask >> i) & 1u) == 0u) {
            uint32_t seg_offset = ui_sdf_prims[i].seg_offset;
            uint32_t seg_count = ui_sdf_prims[i].seg_count;
            uint32_t chunk_offset = ui_sdf_prims[i].chunk_offset;

            float bound_tile = abs(ui_sdf_center_d[i]) + 2.0f * ui_sdf_tile_radius + ui_sdf_uniforms.smoothing + ui_sdf_prims[i].band_half;
            uint32_t chunk_count = (seg_count + ui_sdf_chunk_size - 1) / ui_sdf_chunk_size;

            for (uint32_t c = lindex; c < chunk_count; c += ui_sdf_mid_scale * ui_sdf_mid_scale) {
                float4 cb = ui_sdf_chunks[2 * (chunk_offset + c)];
                float2 v = center - clamp(center, cb.xy, cb.zw);

                if (dot(v, v) >= bound_tile * bound_tile) {
                    continue;
                }

                uint32_t s1 = seg_offset + min((c + 1) * ui_sdf_chunk_size, seg_count);

                for (uint32_t s = seg_offset + c * ui_sdf_chunk_size; s < s1; s++) {
                    if (ui_sdf_segment_distance_sq(center, ui_sdf_seg_ae[s], ui_sdf_seg_ext[s].x) < bound_tile * bound_tile) {
                        uint32_t idx = ui_sdf_list_count.add(1);
                        if (idx < ui_sdf_refine_list_size) {
                            ui_sdf_list[idx] = s;
                        }
                    }
                }
            }
        }
        GroupMemoryBarrierWithGroupSync();

        if (lindex == 0) {
            ui_sdf_list_pos[i + 1] = min(ui_sdf_list_count.load(), (uint32_t)ui_sdf_refine_list_size);
        }
        GroupMemoryBarrierWithGroupSync();
    }

    bool overflow = ui_sdf_list_count.load() > ui_sdf_refine_list_size;

    float3 f = float3(0.0f, 0.0f, 0.0f);
    bool first = true;

    for (i = 0; i < ui_sdf_uniforms.prim_count; i++) {
        if (((skip_mask >> i) & 1u) != 0u) {
            continue;
        }

        float v;
        float2 dir = float2(0.0f, 0.0f);
        if (ui_sdf_prims[i].type == ui_sdf_prim_gpu_type_circle) {
            v = ui_sdf_prim_band_dir(i, ui_sdf_circle_distance_dir(p, i, dir), dir);
        } else {
            float band_half = ui_sdf_prims[i].band_half;
            float bound = abs(ui_sdf_center_d[i]) + distance(p, center) + ui_sdf_uniforms.smoothing + band_half;

            if (abs(ui_sdf_center_d[i]) > ui_sdf_tile_radius && overflow == false) {
                float d = bound * bound;
                float2 best = float2(0.0f, 0.0f);

                for (uint32_t k = ui_sdf_list_pos[i]; k < ui_sdf_list_pos[i + 1]; k++) {
                    uint32_t s = ui_sdf_list[k];
                    float2 b = ui_sdf_segment_closest_delta(p, ui_sdf_seg_ae[s], ui_sdf_seg_ext[s].x);
                    float bd = dot(b, b);
                    if (bd < d) {
                        d = bd;
                        best = b;
                    }
                }

                dir = ui_sdf_dir_normalize(best);
                if (band_half > 0.0f) {
                    v = ui_sdf_prim_band_dir(i, sqrt(d), dir);
                } else {
                    float sign_d = ui_sdf_center_d[i] < 0.0f ? -1.0f : 1.0f;
                    v = sign_d * sqrt(d);
                    dir = sign_d * dir;
                }
            } else {
                v = ui_sdf_path_distance_dir(p, i, bound * bound, dir);
                v = ui_sdf_prim_band_dir(i, v, dir);
            }
        }

        if (first) {
            if (ui_sdf_uniforms.combine == ui_sdf_combine_subtract && i > 0) {
                f = float3(-v, -dir.x, -dir.y);
            } else {
                f = float3(v, dir.x, dir.y);
            }
            first = false;
        } else {
            f = ui_sdf_analytic_fold(ui_sdf_uniforms.combine, ui_sdf_uniforms.smoothing, i, f, float3(v, dir.x, dir.y));
        }
    }

    if (tid.x < ui_sdf_uniforms.fine_size.x && tid.y < ui_sdf_uniforms.fine_size.y) {
        ui_sdf_out_sdf.Store(tid, float4(f.x, f.y, f.z, 0.0f));
    }
}
