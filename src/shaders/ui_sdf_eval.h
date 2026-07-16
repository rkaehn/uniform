#pragma once

#include "cr.h"
#include "ui_sdf_shared.h"

[[vk::binding(ui_sdf_bind_params, 0)]] ConstantBuffer<ui_sdf_params> ui_sdf_uniforms;
[[vk::binding(ui_sdf_bind_prims, 0)]] StructuredBuffer<ui_sdf_prim_gpu> ui_sdf_prims;
[[vk::binding(ui_sdf_bind_seg_ae, 0)]] StructuredBuffer<float4> ui_sdf_seg_ae;
[[vk::binding(ui_sdf_bind_seg_ext, 0)]] StructuredBuffer<float2> ui_sdf_seg_ext;
[[vk::binding(ui_sdf_bind_chunks, 0)]] StructuredBuffer<float4> ui_sdf_chunks;
[[vk::binding(ui_sdf_bind_supers, 0)]] StructuredBuffer<float4> ui_sdf_supers;

static const float ui_sdf_far = 1e30f;

static const float ui_sdf_tile_radius = 0.70710678f * (float)(ui_sdf_mid_scale - 1);

cr_hdef bool ui_sdf_parity_e(float2 p, float2 a, float2 e, float end_y) {
    float2 w = p - a;

    bool c1 = p.y >= a.y;
    bool c2 = p.y < end_y;
    bool c3 = e.x * w.y > e.y * w.x;

    return (c1 && c2 && c3) || (!c1 && !c2 && !c3);
}

cr_hdef bool ui_sdf_parity(float2 p, float2 a, float2 b) {
    return ui_sdf_parity_e(p, a, b - a, b.y);
}

cr_hdef float ui_sdf_segment_distance_sq(float2 p, float4 ae, float inv_ee) {
    float2 e = ae.zw;
    float2 w = p - ae.xy;

    float h = saturate(dot(w, e) * inv_ee);
    float2 b = w - e * h;
    return dot(b, b);
}

cr_hdef float2 ui_sdf_segment_closest_delta(float2 p, float4 ae, float inv_ee) {
    float2 e = ae.zw;
    float2 w = p - ae.xy;

    float h = saturate(dot(w, e) * inv_ee);
    return w - e * h;
}

cr_hdef void ui_sdf_eval_segments(float2 p, uint32_t i0, uint32_t i1, inout float d, inout float s) {
    for (uint32_t i = i0; i < i1; i++) {
        float4 ae = ui_sdf_seg_ae[i];
        float2 ext = ui_sdf_seg_ext[i];

        d = min(d, ui_sdf_segment_distance_sq(p, ae, ext.x));

        if (ui_sdf_parity_e(p, ae.xy, ae.zw, ext.y)) {
            s = -s;
        }
    }
}

cr_hdef void ui_sdf_eval_segments_dir(float2 p, uint32_t i0, uint32_t i1, inout float d, inout float s, inout float2 best) {
    for (uint32_t i = i0; i < i1; i++) {
        float4 ae = ui_sdf_seg_ae[i];
        float2 ext = ui_sdf_seg_ext[i];

        float2 b = ui_sdf_segment_closest_delta(p, ae, ext.x);
        float bd = dot(b, b);
        if (bd < d) {
            d = bd;
            best = b;
        }

        if (ui_sdf_parity_e(p, ae.xy, ae.zw, ext.y)) {
            s = -s;
        }
    }
}

cr_hdef float ui_sdf_path_presample(float2 p, uint32_t seg_offset, uint32_t seg_count) {
    float2 w0 = p - ui_sdf_seg_ae[seg_offset].xy;
    float d = dot(w0, w0);
    cr_unroll
    for (uint32_t k = 1; k < 8; k++) {
        float2 w = p - ui_sdf_seg_ae[seg_offset + (k * seg_count) / 8].xy;
        d = min(d, dot(w, w));
    }
    return d;
}

cr_hdef float ui_sdf_path_presample_dir(float2 p, uint32_t seg_offset, uint32_t seg_count, inout float2 best) {
    float2 w0 = p - ui_sdf_seg_ae[seg_offset].xy;
    float d = dot(w0, w0);
    best = w0;
    cr_unroll
    for (uint32_t k = 1; k < 8; k++) {
        float2 w = p - ui_sdf_seg_ae[seg_offset + (k * seg_count) / 8].xy;
        float wd = dot(w, w);
        if (wd < d) {
            d = wd;
            best = w;
        }
    }
    return d;
}

cr_hdef float2 ui_sdf_dir_normalize(float2 v) {
    float len = length(v);
    return len > 1e-6f ? v / len : float2(0.0f, 0.0f);
}

cr_hdef float ui_sdf_path_distance(float2 p, uint32_t prim_index, float d_bound) {
    uint32_t seg_offset = ui_sdf_prims[prim_index].seg_offset;
    uint32_t seg_count = ui_sdf_prims[prim_index].seg_count;
    uint32_t chunk_offset = ui_sdf_prims[prim_index].chunk_offset;
    uint32_t super_offset = ui_sdf_prims[prim_index].super_offset;

    float d = min(d_bound, ui_sdf_path_presample(p, seg_offset, seg_count));
    float s = 1.0f;

    uint32_t chunk_count = (seg_count + ui_sdf_chunk_size - 1) / ui_sdf_chunk_size;
    uint32_t super_count = (chunk_count + ui_sdf_super_size - 1) / ui_sdf_super_size;

    for (uint32_t sc = 0; sc < super_count; sc++) {
        float4 sb = ui_sdf_supers[2 * (super_offset + sc)];
        float2 sv = p - clamp(p, sb.xy, sb.zw);

        if (dot(sv, sv) >= d) {
            if (p.y >= sb.y && p.y < sb.w) {
                float4 se = ui_sdf_supers[2 * (super_offset + sc) + 1];
                if (ui_sdf_parity(p, se.xy, se.zw)) {
                    s = -s;
                }
            }
            continue;
        }

        uint32_t c_end = min((sc + 1) * ui_sdf_super_size, chunk_count);

        for (uint32_t c = sc * ui_sdf_super_size; c < c_end; c++) {
            float4 cb = ui_sdf_chunks[2 * (chunk_offset + c)];
            float2 cv = p - clamp(p, cb.xy, cb.zw);

            if (dot(cv, cv) >= d) {
                if (p.y >= cb.y && p.y < cb.w) {
                    float4 ce = ui_sdf_chunks[2 * (chunk_offset + c) + 1];
                    if (ui_sdf_parity(p, ce.xy, ce.zw)) {
                        s = -s;
                    }
                }
                continue;
            }

            uint32_t i0 = seg_offset + c * ui_sdf_chunk_size;
            uint32_t i1 = seg_offset + min((c + 1) * ui_sdf_chunk_size, seg_count);
            ui_sdf_eval_segments(p, i0, i1, d, s);
        }
    }

    return s * sqrt(d);
}

cr_hdef float ui_sdf_path_distance_dir(float2 p, uint32_t prim_index, float d_bound, inout float2 out_dir) {
    uint32_t seg_offset = ui_sdf_prims[prim_index].seg_offset;
    uint32_t seg_count = ui_sdf_prims[prim_index].seg_count;
    uint32_t chunk_offset = ui_sdf_prims[prim_index].chunk_offset;
    uint32_t super_offset = ui_sdf_prims[prim_index].super_offset;

    float2 best = float2(0.0f, 0.0f);
    float d_pre = ui_sdf_path_presample_dir(p, seg_offset, seg_count, best);
    float d = min(d_bound, d_pre);
    float s = 1.0f;

    uint32_t chunk_count = (seg_count + ui_sdf_chunk_size - 1) / ui_sdf_chunk_size;
    uint32_t super_count = (chunk_count + ui_sdf_super_size - 1) / ui_sdf_super_size;

    for (uint32_t sc = 0; sc < super_count; sc++) {
        float4 sb = ui_sdf_supers[2 * (super_offset + sc)];
        float2 sv = p - clamp(p, sb.xy, sb.zw);

        if (dot(sv, sv) >= d) {
            if (p.y >= sb.y && p.y < sb.w) {
                float4 se = ui_sdf_supers[2 * (super_offset + sc) + 1];
                if (ui_sdf_parity(p, se.xy, se.zw)) {
                    s = -s;
                }
            }
            continue;
        }

        uint32_t c_end = min((sc + 1) * ui_sdf_super_size, chunk_count);

        for (uint32_t c = sc * ui_sdf_super_size; c < c_end; c++) {
            float4 cb = ui_sdf_chunks[2 * (chunk_offset + c)];
            float2 cv = p - clamp(p, cb.xy, cb.zw);

            if (dot(cv, cv) >= d) {
                if (p.y >= cb.y && p.y < cb.w) {
                    float4 ce = ui_sdf_chunks[2 * (chunk_offset + c) + 1];
                    if (ui_sdf_parity(p, ce.xy, ce.zw)) {
                        s = -s;
                    }
                }
                continue;
            }

            uint32_t i0 = seg_offset + c * ui_sdf_chunk_size;
            uint32_t i1 = seg_offset + min((c + 1) * ui_sdf_chunk_size, seg_count);
            ui_sdf_eval_segments_dir(p, i0, i1, d, s, best);
        }
    }

    out_dir = s * ui_sdf_dir_normalize(best);
    return s * sqrt(d);
}

cr_hdef float ui_sdf_circle_distance(float2 p, uint32_t prim_index) {
    float2 center = float2(ui_sdf_prims[prim_index].circle_x, ui_sdf_prims[prim_index].circle_y);
    return distance(p, center) - ui_sdf_prims[prim_index].circle_radius;
}

cr_hdef float ui_sdf_circle_distance_dir(float2 p, uint32_t prim_index, inout float2 out_dir) {
    float2 center = float2(ui_sdf_prims[prim_index].circle_x, ui_sdf_prims[prim_index].circle_y);
    out_dir = ui_sdf_dir_normalize(p - center);
    return distance(p, center) - ui_sdf_prims[prim_index].circle_radius;
}

cr_hdef float ui_sdf_prim_band(uint32_t prim_index, float v) {
    float band_half = ui_sdf_prims[prim_index].band_half;
    if (band_half > 0.0f) {
        v = max(abs(v) - band_half, 0.5f - 2.0f * band_half);
    }
    return v;
}

cr_hdef float ui_sdf_prim_band_dir(uint32_t prim_index, float v, inout float2 dir) {
    float band_half = ui_sdf_prims[prim_index].band_half;
    if (band_half > 0.0f) {
        if (v < 0.0f) {
            dir = -dir;
        }
        v = max(abs(v) - band_half, 0.5f - 2.0f * band_half);
    }
    return v;
}

cr_hdef float ui_sdf_combine_fold(uint32_t combine, float smoothing, uint32_t i, float f, float v) {
    if (i == 0) {
        return v;
    }

    if (combine == ui_sdf_combine_union) {
        if (smoothing > 0.0f) {
            float h = max(smoothing - abs(f - v), 0.0f) / smoothing;
            return min(f, v) - h * h * smoothing * 0.25f;
        }
        return min(f, v);
    }

    if (smoothing > 0.0f) {
        float h = max(smoothing - abs(f + v), 0.0f) / smoothing;
        return max(f, -v) + h * h * smoothing * 0.25f;
    }
    return max(f, -v);
}

cr_hdef float ui_sdf_distance(float2 p, float d_bound) {
    float f = 0.0f;

    for (uint32_t i = 0; i < ui_sdf_uniforms.prim_count; i++) {
        float v;
        if (ui_sdf_prims[i].type == ui_sdf_prim_gpu_type_circle) {
            v = ui_sdf_circle_distance(p, i);
        } else {
            v = ui_sdf_path_distance(p, i, d_bound);
        }

        v = ui_sdf_prim_band(i, v);

        f = ui_sdf_combine_fold(ui_sdf_uniforms.combine, ui_sdf_uniforms.smoothing, i, f, v);
    }

    return f;
}
