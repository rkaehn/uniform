#include "cr.h"
#include "ui_test_compare_shared.h"

[[vk::binding(ui_test_compare_bind_params, 0)]] ConstantBuffer<ui_test_compare_params> compare_params;
[[vk::binding(ui_test_compare_bind_gpu, 0)]] Texture2D<float4> compare_gpu;
[[vk::binding(ui_test_compare_bind_ref, 0)]] StructuredBuffer<uint32_t> compare_ref;
[[vk::binding(ui_test_compare_bind_tiles, 0)]] RWStructuredBuffer<ui_test_compare_tile> compare_tiles;

static float ui_test_compare_ref_half(uint32_t byte) {
    uint32_t v = (compare_ref[byte >> 2] >> ((byte & 2u) * 8u)) & 0xffffu;
    return f16tof32(((v & 0xffu) << 8) | (v >> 8));
}

static float3 ui_test_compare_ref_load(uint32_t x, uint32_t y) {
    uint32_t byte = (y * compare_params.size.x + x) * 6u;
    return float3(ui_test_compare_ref_half(byte), ui_test_compare_ref_half(byte + 2u), ui_test_compare_ref_half(byte + 4u));
}

static float ui_test_compare_band_sdf_pt(float x_pt, float y_pt) {
    if (compare_params.band_type == ui_test_compare_band_circle) {
        float dx = x_pt - compare_params.band_origin.x;
        float dy = y_pt - compare_params.band_origin.y;
        return sqrt(dx * dx + dy * dy) - compare_params.band_radius;
    }
    float hx = 0.5f * compare_params.band_size.x;
    float hy = 0.5f * compare_params.band_size.y;
    float r = min(compare_params.band_radius, min(hx, hy));
    float px = abs(x_pt - (compare_params.band_origin.x + hx)) - (hx - r);
    float py = abs(y_pt - (compare_params.band_origin.y + hy)) - (hy - r);
    float ox = max(px, 0.0f);
    float oy = max(py, 0.0f);
    return sqrt(ox * ox + oy * oy) + min(max(px, py), 0.0f) - r;
}

groupshared float compare_gs_sum[ui_test_compare_threads];
groupshared uint32_t compare_gs_over[ui_test_compare_threads];
groupshared uint32_t compare_gs_alarm[ui_test_compare_threads];
groupshared float compare_gs_max[ui_test_compare_threads];
groupshared uint32_t compare_gs_idx[ui_test_compare_threads];
groupshared float compare_gs_band_sum[3 * ui_test_compare_threads];
groupshared uint32_t compare_gs_band_count[3 * ui_test_compare_threads];

[shader("compute")]
[numthreads(16, 16, 1)]
void ui_test_compare(uint3 group_id: SV_GroupID, uint3 thread_id: SV_GroupThreadID, uint32_t lindex: SV_GroupIndex) {
    float sum = 0.0f;
    uint32_t over_005 = 0;
    uint32_t alarm = 0;
    float max_d = -1.0f;
    uint32_t max_idx = 0;
    float band_sum[3] = { 0.0f, 0.0f, 0.0f };
    uint32_t band_count[3] = { 0, 0, 0 };

    for (uint32_t j = 0; j < 2; j++) {
        for (uint32_t i = 0; i < 2; i++) {
            uint32_t x = group_id.x * ui_test_compare_tile_size + thread_id.x * 2 + i;
            uint32_t y = group_id.y * ui_test_compare_tile_size + thread_id.y * 2 + j;
            if (x >= compare_params.size.x || y >= compare_params.size.y) {
                continue;
            }

            float4 g = compare_gpu.Load(int3((int32_t)x, (int32_t)y, 0));
            float4 r = float4(ui_test_compare_ref_load(x, y), 1.0f);
            if ((compare_params.flags & ui_test_compare_flag_clamp) != 0) {
                r = max(r, float4(0.0f, 0.0f, 0.0f, 0.0f));
            }

            float4 ad = abs(g - r);
            float d = max(max(ad.x, ad.y), max(ad.z, ad.w));

            sum += d;
            over_005 += d > 0.05f ? 1u : 0u;
            alarm += d > compare_params.alarm_diff ? 1u : 0u;

            uint32_t idx = y * compare_params.size.x + x;
            if (d > max_d || (d == max_d && idx < max_idx)) {
                max_d = d;
                max_idx = idx;
            }

            if (compare_params.band_type != 0) {
                float sdf = ui_test_compare_band_sdf_pt(((float)x + 0.5f) * compare_params.to_pt, ((float)y + 0.5f) * compare_params.to_pt);
                if (sdf <= ui_test_compare_band_rim_pt) {
                    uint32_t band = sdf >= -ui_test_compare_band_rim_pt ? 0u : (sdf >= -ui_test_compare_band_bevel_pt ? 1u : 2u);
                    band_sum[band] += d;
                    band_count[band] += 1u;
                }
            }
        }
    }

    compare_gs_sum[lindex] = sum;
    compare_gs_over[lindex] = over_005;
    compare_gs_alarm[lindex] = alarm;
    compare_gs_max[lindex] = max_d;
    compare_gs_idx[lindex] = max_idx;
    for (uint32_t b = 0; b < 3; b++) {
        compare_gs_band_sum[b * ui_test_compare_threads + lindex] = band_sum[b];
        compare_gs_band_count[b * ui_test_compare_threads + lindex] = band_count[b];
    }

    for (uint32_t s = ui_test_compare_threads / 2; s > 0; s >>= 1) {
        GroupMemoryBarrierWithGroupSync();
        if (lindex < s) {
            compare_gs_sum[lindex] += compare_gs_sum[lindex + s];
            compare_gs_over[lindex] += compare_gs_over[lindex + s];
            compare_gs_alarm[lindex] += compare_gs_alarm[lindex + s];
            float other_max = compare_gs_max[lindex + s];
            uint32_t other_idx = compare_gs_idx[lindex + s];
            if (other_max > compare_gs_max[lindex] || (other_max == compare_gs_max[lindex] && other_idx < compare_gs_idx[lindex])) {
                compare_gs_max[lindex] = other_max;
                compare_gs_idx[lindex] = other_idx;
            }
            for (uint32_t b = 0; b < 3; b++) {
                compare_gs_band_sum[b * ui_test_compare_threads + lindex] += compare_gs_band_sum[b * ui_test_compare_threads + lindex + s];
                compare_gs_band_count[b * ui_test_compare_threads + lindex] += compare_gs_band_count[b * ui_test_compare_threads + lindex + s];
            }
        }
    }

    if (lindex == 0) {
        uint32_t ti = group_id.y * compare_params.tiles_x + group_id.x;
        compare_tiles[ti].sum = compare_gs_sum[0];
        compare_tiles[ti].over_005 = compare_gs_over[0];
        compare_tiles[ti].alarm = compare_gs_alarm[0];
        compare_tiles[ti].max_d = compare_gs_max[0];
        compare_tiles[ti].max_idx = compare_gs_idx[0];
        compare_tiles[ti].rim_sum = compare_gs_band_sum[0];
        compare_tiles[ti].bevel_sum = compare_gs_band_sum[ui_test_compare_threads];
        compare_tiles[ti].interior_sum = compare_gs_band_sum[2 * ui_test_compare_threads];
        compare_tiles[ti].rim_count = compare_gs_band_count[0];
        compare_tiles[ti].bevel_count = compare_gs_band_count[ui_test_compare_threads];
        compare_tiles[ti].interior_count = compare_gs_band_count[2 * ui_test_compare_threads];
        compare_tiles[ti].pad = 0;
    }
}
