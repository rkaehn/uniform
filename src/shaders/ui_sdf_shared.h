#pragma once

#include "ui_shared.h"

#define ui_sdf_combine_union 0u
#define ui_sdf_combine_subtract 1u

#define ui_sdf_prim_gpu_type_circle 0u
#define ui_sdf_prim_gpu_type_path 1u

#define ui_sdf_top_scale 32
#define ui_sdf_mid_scale 8
#define ui_sdf_use_top_min_tiles (64 * 64)
#define ui_sdf_chunk_size 8
#define ui_sdf_super_size 8
#define ui_sdf_refine_list_size 512
#define ui_sdf_max_prims 8

#define ui_sdf_band_margin 5.0f
#define ui_sdf_band_f16_margin 0.5f

#define ui_sdf_bind_params 0
#define ui_sdf_bind_prims 1
#define ui_sdf_bind_seg_ae 2
#define ui_sdf_bind_seg_ext 3
#define ui_sdf_bind_chunks 4
#define ui_sdf_bind_supers 5

#define ui_sdf_bind_top_args 6
#define ui_sdf_bind_top_out 7
#define ui_sdf_bind_top_binding_count 8

#define ui_sdf_bind_mid_tiles 6
#define ui_sdf_bind_mid_args 7
#define ui_sdf_bind_mid_prim_mid 8
#define ui_sdf_bind_mid_out 9
#define ui_sdf_bind_mid_binding_count 10

#define ui_sdf_bind_refine_tiles 6
#define ui_sdf_bind_refine_prim_mid 7
#define ui_sdf_bind_refine_out 8
#define ui_sdf_bind_refine_binding_count 9

#define ui_sdf_bind_scene_params 0
#define ui_sdf_bind_scene_in_mid 1
#define ui_sdf_bind_scene_in_sdf 2
#define ui_sdf_bind_scene_content 3
#define ui_sdf_bind_scene_sampler 4
#define ui_sdf_bind_scene_analytic 5
#define ui_sdf_bind_scene_binding_count 6

#define ui_glass_pyramid_max_levels 8
#define ui_glass_capture_log2 1u
#define ui_glass_capture_texel_px (float)(1u << ui_glass_capture_log2)

#define ui_sdf_tint_none 0u
#define ui_sdf_tint_solid 1u
#define ui_sdf_tint_gradient 2u
#define ui_sdf_tint_graphic 3u

#define ui_glass_variant_count 4

cr_hdef bool ui_glass_variant_is_regular(uint32_t glass_variant) {
    return glass_variant >= 2u;
}

#define ui_glass_union_smoothness_pt 8.0f
#define ui_glass_glow_reach_pt (ui_glass_rim_diffuse_height_scale * ui_glass_rim_height_pt)
#define ui_glass_band_in_pad_px 4.0f
#define ui_glass_band_out_pad_px 2.0f
#define ui_glass_reach_in_pt 28.0f
#define ui_glass_reach_in_pad_px 8.0f
#define ui_glass_reach_out_min_pt 8.5f
#define ui_glass_reach_out_cap_pt 40.5f
#define ui_glass_refract_height_cap_pt 20.0f

cr_hdef float ui_glass_refract_height_pt(uint32_t glass_variant, float min_dim_pt) {
    float half_pt = min_dim_pt > 0.0f ? 0.5f * min_dim_pt : 1e6f;
    float h = (ui_glass_variant_is_regular(glass_variant) ? 0.5f : 0.72f) * half_pt;
    return h < ui_glass_refract_height_cap_pt ? h : ui_glass_refract_height_cap_pt;
}

cr_hdef float ui_glass_slider_lo(float slider) {
    float lo = 2.0f * slider;
    return lo < 1.0f ? lo : 1.0f;
}

cr_hdef float ui_glass_slider_hi(float slider) {
    float hi = 2.0f * slider - 1.0f;
    return hi > 0.0f ? hi : 0.0f;
}

#define ui_glass_blur_radius_rg_pt 5.0f

cr_hdef float ui_glass_blur_radius_pt(uint32_t glass_variant, float slider) {
    if (ui_glass_variant_is_regular(glass_variant)) {
        return ui_glass_blur_radius_rg_pt;
    }
    return 0.75f * ui_glass_slider_lo(slider);
}

#define ui_glass_blur_prescale 1.6f
#define ui_glass_blur_knee_chain 0.982f
#define ui_glass_blur_knot1_pt -1.0f

cr_hdef float4 ui_glass_blur_knot_o(uint32_t glass_variant, float size_ramp) {
    if (ui_glass_variant_is_regular(glass_variant)) {
        return make_float4(0.8f * size_ramp, 0.4f * size_ramp, 0.5f, 1.0f);
    }
    return make_float4(1.0f, 0.5f, 0.5f, 1.0f);
}

cr_hdef float ui_glass_blur_rh_pt(uint32_t glass_variant, float slider, float size_ramp, float min_dim_pt, float x_pt) {
    float radius = ui_glass_blur_radius_pt(glass_variant, slider);
    if (radius <= 0.0f) {
        return 0.0f;
    }
    float half_pt = min_dim_pt > 0.0f ? 0.5f * min_dim_pt : 1e6f;
    float4 o = ui_glass_blur_knot_o(glass_variant, size_ramp);
    float denom = half_pt + ui_glass_blur_knot1_pt;
    float t0 = denom > 1e-3f ? (x_pt + half_pt) / denom : 1.0f;
    t0 = t0 < 0.0f ? 0.0f : t0 > 1.0f ? 1.0f : t0;
    float t1 = x_pt - ui_glass_blur_knot1_pt;
    t1 = t1 < 0.0f ? 0.0f : t1 > 1.0f ? 1.0f : t1;
    float t2 = x_pt > 0.0f ? 1.0f : 0.0f;
    float r = radius * (o.x - (o.x - o.y) * t0 - (o.y - o.z) * t1 - (o.z - o.w) * t2);
    return r > 0.0f ? ui_glass_blur_prescale * r : 0.0f;
}

cr_hdef float ui_glass_blur_lod(float rh_pt, float scale) {
    float rt = (1.0f / ui_glass_capture_texel_px) * rh_pt * scale;
    if (rt < 2.0f) {
        float m = 0.5f * rt * ui_glass_blur_knee_chain + 1.0f;
        return m > 1.0f ? log2f(m) : 0.0f;
    }
    return log2f(rt);
}

cr_hdef float ui_glass_fill_radius_pt(uint32_t glass_variant, float slider) {
    if (ui_glass_variant_is_regular(glass_variant)) {
        return 8.0f;
    }
    return slider > 0.5f ? 8.0f : 0.0f;
}

cr_hdef float ui_glass_fill_dk(uint32_t glass_variant, float slider) {
    if (glass_variant != 2u) {
        return 0.0f;
    }
    return 0.9f * (0.75f + 0.25f * ui_glass_slider_lo(slider));
}

cr_hdef float ui_glass_fill_lk(uint32_t glass_variant, float slider) {
    if (glass_variant != 3u) {
        return 0.0f;
    }
    return 0.9f * (0.75f + 0.25f * ui_glass_slider_lo(slider));
}

cr_hdef float ui_glass_fill_wn(uint32_t glass_variant, float slider) {
    if (ui_glass_variant_is_regular(glass_variant)) {
        return slider;
    }
    return ui_glass_slider_hi(slider);
}

cr_hdef float ui_glass_face_fill_a(uint32_t glass_variant, float slider) {
    if (glass_variant == 2u) {
        return 0.5f * ui_glass_slider_hi(slider);
    }
    if (glass_variant == 3u) {
        return 0.2f + 0.3f * ui_glass_slider_hi(slider);
    }
    return 0.0f;
}

cr_hdef float ui_glass_face_fill_gray(uint32_t glass_variant, float slider) {
    if (glass_variant == 2u) {
        return 0.25f * ui_glass_face_fill_a(glass_variant, slider);
    }
    return glass_variant == 3u ? 1.0f : 0.0f;
}

cr_hdef float ui_glass_bleed_radius_pt(uint32_t glass_variant, float min_dim_pt) {
    if (ui_glass_variant_is_regular(glass_variant) == false) {
        return 0.0f;
    }
    return 0.35f * min_dim_pt;
}

#define ui_glass_blur_bleed_rt_cap_pt 160.0f
#define ui_glass_blur_margin_sigma_per_r 2.03f
#define ui_glass_bleed_lod_scale 0.5f
#define ui_glass_veil_tap_texels 0.25f

cr_hdef float ui_glass_blur_veil_rt_pt(float radius_pt) {
    if (radius_pt <= 0.0f) {
        return 0.0f;
    }
    return ui_glass_blur_prescale * radius_pt;
}

cr_hdef float ui_glass_blur_bleed_rt_pt(float radius_pt) {
    if (radius_pt <= 0.0f) {
        return 0.0f;
    }
    float capped = radius_pt < ui_glass_blur_bleed_rt_cap_pt ? radius_pt : ui_glass_blur_bleed_rt_cap_pt;
    return ui_glass_blur_prescale * ui_glass_bleed_lod_scale * capped;
}

cr_hdef float ui_glass_blur_margin_sigma_px(float radius_pt, float scale) {
    return ui_glass_blur_margin_sigma_per_r * radius_pt * scale;
}

cr_hdef float ui_glass_outer_refraction_amount_pt(uint32_t glass_variant, float min_dim_pt) {
    float half_pt = min_dim_pt > 0.0f ? 0.5f * min_dim_pt : 1e6f;
    if (ui_glass_variant_is_regular(glass_variant)) {
        return 0.5f * half_pt > 16.0f ? 0.5f * half_pt : 16.0f;
    }
    return 0.4f * half_pt;
}

cr_hdef float ui_glass_outer_refraction_height_pt(uint32_t glass_variant, float min_dim_pt) {
    float half_pt = min_dim_pt > 0.0f ? 0.5f * min_dim_pt : 1e6f;
    if (ui_glass_variant_is_regular(glass_variant)) {
        return 0.4f * half_pt > 16.0f ? 0.4f * half_pt : 16.0f;
    }
    return 0.25f * half_pt;
}

cr_hdef float ui_glass_refraction_opacity(uint32_t glass_variant) {
    return ui_glass_variant_is_regular(glass_variant) ? 0.6f : 0.0f;
}

#define ui_glass_outset_blur_tail_pt (3.0f * ui_glass_blur_prescale * ui_glass_blur_radius_rg_pt)

cr_hdef float ui_glass_outset_pt(uint32_t glass_variant, float min_dim_pt) {
    if (ui_glass_variant_is_regular(glass_variant) == false) {
        return 0.0f;
    }
    float outer_reach = ui_glass_outer_refraction_amount_pt(glass_variant, min_dim_pt > 0.0f ? min_dim_pt : 32.0f)
                      + ui_glass_outset_blur_tail_pt;
    return outer_reach > ui_glass_reach_out_cap_pt ? outer_reach : ui_glass_reach_out_cap_pt;
}

cr_hdef float ui_glass_size_t(float min_dim_pt) {
    float t = (min_dim_pt - 64.0f) * (1.0f / 96.0f);
    return t < 0.0f ? 0.0f : t > 1.0f ? 1.0f : t;
}

#define ui_glass_rim_height_pt 1.0f
#define ui_glass_rim_amount 0.5f
#define ui_glass_rim_curvature 0.75f
#define ui_glass_rim_diffuse_height_scale 8.0f
#define ui_glass_rim_diffuse_amount_scale 0.15f
#define ui_glass_rim_diffuse_spread_scale 0.65f

cr_hdef float ui_glass_rim_spread_rad(float min_dim_pt) {
    float t = (min_dim_pt - 96.0f) * (1.0f / 96.0f);
    t = t < 0.0f ? 0.0f : t > 1.0f ? 1.0f : t;
    return (80.0f + 10.0f * t) * (3.14159265358979f / 180.0f);
}

cr_hdef float ui_glass_rim_compression(float amount) {
    return 1.0f / amount - 2.0f;
}

#define ui_glass_cos_denom_min 5.2e-4f
#define ui_glass_rim_clip_lo -0.75f
#define ui_glass_rim_clip_hi 1.0859375f

cr_hdef float ui_glass_rim_lobe(float d_px, float h_px, float cos_spread, float cosine, float curvature, float k, float w) {
    float u = d_px / (h_px > 1e-4f ? h_px : 1e-4f);
    u = u < 0.0f ? 0.0f : u > 1.0f ? 1.0f : u;
    float radial = (u < 1.0f ? 1.0f : 0.0f);
    radial = radial + curvature * (1.0f - u - radial);

    float lo = d_px / w + 0.5f;
    float hi = (h_px - d_px) / w + 0.5f;
    lo = lo < 0.0f ? 0.0f : lo > 1.0f ? 1.0f : lo;
    hi = hi < 0.0f ? 0.0f : hi > 1.0f ? 1.0f : hi;

    float denom = 1.0f - cos_spread;
    float ang = (cosine - cos_spread) / (denom > ui_glass_cos_denom_min ? denom : ui_glass_cos_denom_min);
    ang = ang < 0.0f ? 0.0f : ang > 1.0f ? 1.0f : ang;

    float l = radial * lo * hi * ang;
    return l / (1.0f + k * (1.0f - l));
}

#define ui_glass_face_black_cr 0.125f
#define ui_glass_face_white_cr 1.08f
#define ui_glass_face_sat_cr 1.06f
#define ui_glass_face_black_rgdk 0.125f
#define ui_glass_face_white_rgdk 1.125f
#define ui_glass_face_sat_rgdk 1.3f
#define ui_glass_face_black_rglt 0.4f
#define ui_glass_face_white_rglt 1.03f
#define ui_glass_face_sat_rglt 1.2f
#define ui_glass_bleed_black_rgdk 0.125f
#define ui_glass_bleed_white_rgdk 0.5f
#define ui_glass_bleed_sat_rgdk 1.0f
#define ui_glass_bleed_black_rglt 0.9f
#define ui_glass_bleed_white_rglt 1.0f
#define ui_glass_bleed_sat_rglt 1.2f
#define ui_glass_maxluma_chroma_gain 0.3f

cr_hdef float ui_glass_face_max_luma(uint32_t glass_variant, float slider, float size_ramp) {
    if (glass_variant == 2u) {
        float ds = 0.5f - slider;
        ds = ds > 0.0f ? ds : 0.0f;
        float lo = 0.35f + 0.2f * ds;
        float ramp = 0.6f - 0.6f * size_ramp + 0.48f * size_ramp * ds;
        return ramp > lo ? ramp : lo;
    }
    return glass_variant == 3u ? 1.0f : 0.94f;
}

#define ui_glass_clamp_lo -0.75f
#define ui_glass_shadow_offset_pt 8.0f
#define ui_glass_shadow_fill_a 0.3f
#define ui_glass_ring_stroke_pt 4.0f
#define ui_glass_ring_sigma_pt 5.0f
#define ui_glass_ring_opacity 0.06f
#define ui_glass_ring_z 0.7071f
#define ui_glass_soft_reach 2.0f
#define ui_glass_kfh_tail_pad_px 1.0f

cr_hdef float ui_glass_shadow_radius_pt(float size_ramp) {
    return 4.0f + 20.0f * size_ramp;
}

cr_hdef float ui_glass_shadow_opacity(uint32_t glass_variant, float size_ramp) {
    if (ui_glass_variant_is_regular(glass_variant) == false) {
        return 0.0f;
    }
    return 0.04f + (glass_variant == 2u ? 0.56f : 0.36f) * size_ramp;
}

cr_hdef float ui_glass_soft(float h) {
    float x = h * (1.0f / (2.0f * ui_glass_soft_reach)) + 0.5f;
    x = x < 0.0f ? 0.0f : x > 1.0f ? 1.0f : x;
    x = x * 4.0f - 2.0f;
    float y = x * x;
    float p = ((0.0029544830f * y - 0.034454346f) * y + 0.168212891f) * y - 0.560546875f;
    return p * x + 0.5f;
}

cr_hdef float ui_glass_outside_reach_pt(uint32_t glass_variant, float min_dim_pt) {
    if (ui_glass_variant_is_regular(glass_variant) == false) {
        return 0.0f;
    }
    float t = ui_glass_size_t(min_dim_pt);
    float shadow_pt = ui_glass_shadow_offset_pt
                    + ui_glass_soft_reach * ui_glass_shadow_radius_pt(t);
    float reach_pt = shadow_pt > ui_glass_reach_out_min_pt ? shadow_pt : ui_glass_reach_out_min_pt;
    return reach_pt < ui_glass_reach_out_cap_pt ? reach_pt : ui_glass_reach_out_cap_pt;
}

#define ui_sdf_analytic_coef_count 8

struct ui_sdf_analytic_prim {
    float4 pos;
    float4 cell_x;
    float4 cell_y;
    float4 coef[2 * 4];
};

cr_hdef float3 ui_sdf_analytic_corner_eval(float2 q, float2 cell_origin, float2 cell, float4 c0, float4 c1) {
    float2 u = (q - cell_origin) / cell;
    float ux = u.x > 0.0f ? u.x : 0.0f;
    float rr = sqrtf(ux * ux + u.y * u.y);
    float radial = rr > 1e-6f ? rr : 1e-6f;
    float2 dir = make_float2(ux, u.y) * (1.0f / radial);
    float x = dir.x * dir.x - dir.y * dir.y;
    float y = 2.0f * dir.x * dir.y;
    y = y > 0.0f ? y : 0.0f;

    float ck = 1.0f;
    float sk = 0.0f;
    float acc = c0.x;
    float dacc = 0.0f;
    cr_unroll
    for (uint32_t k = 1; k < ui_sdf_analytic_coef_count; k++) {
        float ck_next = ck * x - sk * y;
        sk = sk * x + ck * y;
        ck = ck_next;
        float coef = k < 4 ? c0[k] : c1[k - 4];
        acc += coef * ck;
        dacc += coef * (-2.0f * (float)k) * sk;
    }
    float w = 0.25f * y * y;
    float rho = 1.0f + w * acc;
    float fade = (radial - 0.35f) * 4.0f;
    fade = fade < 0.0f ? 0.0f : fade > 1.0f ? 1.0f : fade;
    float drho = (x * y * acc + w * dacc) * fade;

    float rc = radial > 0.35f ? radial : 0.35f;
    float2 g = make_float2((dir.x + drho * dir.y / rc) / cell.x,
                           (dir.y - drho * dir.x / rc) / cell.y);
    float gl = sqrtf(g.x * g.x + g.y * g.y);
    gl = gl > 1e-9f ? gl : 1e-9f;

    float d = (radial - rho) / gl;
    g = g * (1.0f / gl);
    return make_float3(d, g.x, g.y);
}

cr_hdef float3 ui_sdf_analytic_corner_curve(float2 q, float2 cell_origin, float2 cell, float4 c0, float4 c1, float range) {
    if (c0.x == 0.0f && cell.x == cell.y) {
        float2 u = q - cell_origin;
        float radial = sqrtf(u.x * u.x + u.y * u.y);
        float2 g = radial > 1e-6f ? u * (1.0f / radial) : make_float2(0.0f, 1.0f);
        return make_float3(radial - cell.x, g.x, g.y);
    }

    float2 un = (q - cell_origin) / cell;
    float unx = un.x > 0.0f ? un.x : 0.0f;
    float radial = sqrtf(unx * unx + un.y * un.y);
    float tol = 0.25f * ((c0.x < 0.0f ? -c0.x : c0.x) + (c0.y < 0.0f ? -c0.y : c0.y) +
                         (c0.z < 0.0f ? -c0.z : c0.z) + (c0.w < 0.0f ? -c0.w : c0.w) +
                         (c1.x < 0.0f ? -c1.x : c1.x) + (c1.y < 0.0f ? -c1.y : c1.y) +
                         (c1.z < 0.0f ? -c1.z : c1.z) + (c1.w < 0.0f ? -c1.w : c1.w));
    float cell_min = cell.x < cell.y ? cell.x : cell.y;
    float gap = radial - 1.0f;
    if (((gap < 0.0f ? -gap : gap) - tol) * cell_min > range + 0.5f) {
        float inv = radial > 1e-6f ? 1.0f / radial : 0.0f;
        float2 g = radial > 1e-6f ? make_float2(unx * inv, un.y * inv) : make_float2(0.0f, 1.0f);
        return make_float3(gap * cell_min, g.x, g.y);
    }

    float3 e1 = ui_sdf_analytic_corner_eval(q, cell_origin, cell, c0, c1);
    float d_curve = e1.x;
    float gx = e1.y;
    float gy = e1.z;

    if ((e1.x < 0.0f ? -e1.x : e1.x) < range) {
        float2 q1 = make_float2(q.x - e1.x * e1.y, q.y - e1.x * e1.z);
        float3 e2 = ui_sdf_analytic_corner_eval(q1, cell_origin, cell, c0, c1);
        float2 q2 = make_float2(q1.x - e2.x * e2.y, q1.y - e2.x * e2.z);

        float2 dv = q - q2;
        d_curve = sqrtf(dv.x * dv.x + dv.y * dv.y);
        d_curve = e1.x < 0.0f ? -d_curve : d_curve;
        gx = e2.y;
        gy = e2.z;
    }

    return make_float3(d_curve, gx, gy);
}

cr_hdef float3 ui_sdf_analytic_prim_sharp(float4 pos, float2 p) {
    float2 rel = p - pos.xy;
    float2 half_size = pos.zw;

    float sx = rel.x < 0.0f ? -1.0f : 1.0f;
    float sy = rel.y < 0.0f ? -1.0f : 1.0f;
    float2 e = make_float2(rel.x * sx, rel.y * sy) - half_size;
    float ax = e.x > 0.0f ? e.x : 0.0f;
    float ay = e.y > 0.0f ? e.y : 0.0f;
    float outside = sqrtf(ax * ax + ay * ay);
    float inner = e.x > e.y ? e.x : e.y;
    float d_sharp = outside + (inner < 0.0f ? inner : 0.0f);
    float2 g_sharp;
    if (outside > 1e-9f) {
        g_sharp = make_float2(ax / outside * sx, ay / outside * sy);
    } else {
        g_sharp = e.x > e.y ? make_float2(sx, 0.0f) : make_float2(0.0f, sy);
    }

    return make_float3(d_sharp, g_sharp.x, g_sharp.y);
}

cr_hdef float3 ui_sdf_analytic_prim_field(float4 pos, float4 cell_x, float4 cell_y, float4 coef[2 * 4], float2 p, float range) {
    float2 rel = p - pos.xy;
    float2 half_size = pos.zw;

    float3 best = ui_sdf_analytic_prim_sharp(pos, p);
    cr_unroll
    for (uint32_t i = 0; i < 4; i++) {
        float mx = (i == 0 || i == 3) ? -1.0f : 1.0f;
        float my = i < 2 ? -1.0f : 1.0f;
        float2 cell = make_float2(cell_x[i], cell_y[i]);
        if (cell.x <= 0.0f || cell.y <= 0.0f) {
            continue;
        }
        float2 q = make_float2(rel.x * mx, rel.y * my);
        float2 cell_origin = half_size - cell;
        if (q.x > cell_origin.x && q.y > cell_origin.y) {
            float3 f = ui_sdf_analytic_corner_curve(q, cell_origin, cell, coef[2 * i], coef[2 * i + 1], range);
            if (f.x > best.x) {
                best = make_float3(f.x, f.y * mx, f.z * my);
            }
        }
    }

    return best;
}

cr_hdef float3 ui_sdf_analytic_fold(uint32_t combine, float smoothing, uint32_t i, float3 f, float3 v) {
    if (i == 0) {
        return v;
    }

    if (combine != ui_sdf_combine_union) {
        v = make_float3(-v.x, -v.y, -v.z);
    }

    bool f_wins = combine == ui_sdf_combine_union ? f.x < v.x : f.x > v.x;

    float d;
    float2 g;
    if (smoothing > 0.0f) {
        float k = smoothing * (0.5f - 0.5f * (f.y * v.y + f.z * v.z));
        float diff = f.x - v.x;
        float h = k - (diff < 0.0f ? -diff : diff);
        h = h > 0.0f ? h / k : 0.0f;
        float bump = h * h * k * 0.25f;
        d = (f_wins ? f.x : v.x) + (combine == ui_sdf_combine_union ? -bump : bump);
        float wv = f_wins ? 0.5f * h : 1.0f - 0.5f * h;
        g = make_float2(f.y + (v.y - f.y) * wv, f.z + (v.z - f.z) * wv);
        if (g.x * g.x + g.y * g.y < 1e-12f) {
            g = make_float2(f.y, f.z);
        }
    } else {
        d = f_wins ? f.x : v.x;
        g = f_wins ? make_float2(f.y, f.z) : make_float2(v.y, v.z);
    }
    return make_float3(d, g.x, g.y);
}

struct ui_sdf_scene_params {
    uint2 out_origin;
    uint2 fine_size;
    float4 fill_color;
    float4 outline_color;
    float outline_center;
    float outline_half;
    float band_lo;
    float band_hi;
    float2 content_uv_origin;
    float2 content_uv_size;
    float2 content_scale;
    float2 content_ref;
    float2 content_uv_min;
    float2 content_uv_max;
    uint32_t has_fill;
    uint32_t content_flags;
    float opacity;
    float glass_scale;
    float4 cs_col_x;
    float4 cs_col_y;
    float4 cs_col_z;
    float4 tint_color0;
    float4 tint_color1;
    float2 tint_grad_dir;
    float tint_grad_off;
    uint32_t tint_mode;
    uint32_t region_band;
    uint32_t glass_variant;
    float glass_size_t;
    float glass_min_dim_pt;
    float glass_slider;
    float glass_min_dim_lo_pt;
    float glass_pyr_veil;
    float glass_pyr_bleed;
    uint32_t glass_pyr_count;
    uint32_t analytic_count;
    uint32_t analytic_combine;
    float analytic_smoothing;
    float analytic_range;
    float reach_lo;
    float reach_hi;
    uint32_t dither;
    float2 mid_uv_scale;
    float2 mid_uv_min;
    float2 mid_uv_max;
    float2 _pad_b1;
    float4 glass_rh_o;
    float4 glass_tint_slope;
    float4 glass_tint_bias;
    float2 glass_rh_span;
    float2 glass_rim_cos;
    float2 glass_rim_k;
    float2 glass_refract;
    float2 glass_outer_ah;
    float2 glass_fill_dklk;
    float glass_rh_radius;
    float glass_outer_opacity;
    float glass_fill_wn;
    float glass_shadow_alpha;
    float glass_shadow_radius_pt;
    float glass_bleed_radius_pt;
    uint32_t glass_fill_active;
    uint32_t glass_tint_hoist;
    float4 glass_face;
    float4 glass_vibrant_x;
    float4 glass_vibrant_y;
    float4 glass_vibrant_z;
    float2 glass_kfh;
    float glass_face_op;
    float _pad_b2;
};

struct ui_glass_tint_coef {
    float4 slope;
    float4 bias;
    uint32_t mode;
};

cr_hdef cr_struct ui_glass_tint_coef ui_glass_tint_coeffs(uint32_t glass_variant, float4 tint) {
    cr_struct ui_glass_tint_coef coef;
    coef.slope = make_float4(0.0f, 0.0f, 0.0f, 0.0f);
    coef.bias = make_float4(0.0f, 0.0f, 0.0f, 0.0f);
    coef.mode = 0u;
    if (tint.w <= 0.0f) {
        return coef;
    }
    float3 t = make_float3(tint.x / tint.w, tint.y / tint.w, tint.z / tint.w);
    float mx = max_float(t.x, max_float(t.y, t.z));
    float mn = min_float(t.x, min_float(t.y, t.z));
    if (mx - mn < 1e-4f) {
        float v = 0.5f * (mx + mn);
        float k = 6.25f / ((4.0f + v) * (5.0f - v));
        float bias = k * (3.8f * v - 0.8f);
        coef.slope = make_float4(0.0f, 0.0f, 0.0f, k);
        coef.bias = make_float4(bias, bias, bias, 0.0f);
        coef.mode = 1u;
        return coef;
    }
    float sum = mx + mn;
    float3 slope;
    if (glass_variant == 2u) {
        float w = (19.0f / 60.0f) * max_float(0.0f, min_float(1.0f / max_float(2.0f - sum, 1e-3f), 11.0f - 10.0f / max_float(sum, 1e-3f)));
        float off = sum * (11.0f / 240.0f + 0.5f * w);
        slope.x = min_float((0.075f - w) * t.x + off, (1.0f - t.x) * (1.0f / 0.6f));
        slope.y = min_float((0.075f - w) * t.y + off, (1.0f - t.y) * (1.0f / 0.6f));
        slope.z = min_float((0.075f - w) * t.z + off, (1.0f - t.z) * (1.0f / 0.6f));
        float3 bias = t - 0.15f * slope;
        coef.slope = make_float4(slope.x, slope.y, slope.z, 0.0f);
        coef.bias = make_float4(bias.x, bias.y, bias.z, 0.0f);
        coef.mode = 2u;
        return coef;
    }
    float k = max_float(1.0f, min_float(sum, 10.0f / 3.0f - sum) / max_float(2.0f - sum, 1e-3f));
    slope.x = (t.x - clamp_float(0.3f * sum + k * (0.72f * t.x - 0.36f * sum), 0.0f, 1.0f)) * (1.0f / 0.85f);
    slope.y = (t.y - clamp_float(0.3f * sum + k * (0.72f * t.y - 0.36f * sum), 0.0f, 1.0f)) * (1.0f / 0.85f);
    slope.z = (t.z - clamp_float(0.3f * sum + k * (0.72f * t.z - 0.36f * sum), 0.0f, 1.0f)) * (1.0f / 0.85f);
    float3 bias = t - slope;
    coef.slope = make_float4(slope.x, slope.y, slope.z, 0.0f);
    coef.bias = make_float4(bias.x, bias.y, bias.z, 0.0f);
    coef.mode = 3u;
    return coef;
}

struct ui_sdf_scene_analytic_prims {
    cr_struct ui_sdf_analytic_prim prims[ui_sdf_max_prims];
};

struct ui_sdf_prim_gpu {
    uint32_t type;
    uint32_t seg_offset;
    uint32_t seg_count;
    uint32_t chunk_offset;
    uint32_t super_offset;
    float circle_x;
    float circle_y;
    float circle_radius;
    float band_half;
    uint32_t _pad0;
    uint32_t _pad1;
    uint32_t _pad2;
};

struct ui_sdf_params {
    uint2 top_size;
    uint2 mid_size;
    uint2 fine_size;
    uint32_t prim_count;
    uint32_t combine;
    uint32_t use_top;
    float smoothing;
    float band_lo;
    float band_hi;
    uint32_t exact_mid;
    uint32_t top_base;
    uint32_t _pad3;
    uint32_t _pad4;
};

cr_hdef bool ui_sdf_near_band(float sdf, float band_lo, float band_hi) {
    return !(sdf < band_lo - ui_sdf_band_margin || sdf > band_hi + ui_sdf_band_margin);
}
