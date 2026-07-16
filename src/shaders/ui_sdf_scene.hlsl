#include "cr.h"
#include "ui_shared.h"
#include "ui_sdf_shared.h"
#include "ui_blur_shared.h"

[[vk::binding(ui_sdf_bind_scene_params, 0)]] ConstantBuffer<ui_sdf_scene_params> ui_sdf_scene_uniforms;
[[vk::binding(ui_sdf_bind_scene_in_mid, 0)]] Texture2D<float4> ui_sdf_scene_in_mid;
[[vk::binding(ui_sdf_bind_scene_in_sdf, 0)]] Texture2D<float4> ui_sdf_scene_in_sdf;
[[vk::binding(ui_sdf_bind_scene_content, 0)]] cr_tex_half4 ui_sdf_scene_content_tex;
[[vk::binding(ui_sdf_bind_scene_sampler, 0)]] SamplerState ui_sdf_scene_sampler;
[[vk::binding(ui_sdf_bind_scene_analytic, 0)]] ConstantBuffer<ui_sdf_scene_analytic_prims> ui_sdf_scene_prims;

static float3 ui_sdf_scene_analytic(float2 p) {
    float3 f = float3(0.0f, 0.0f, 0.0f);
    for (uint32_t i = 0; i < ui_sdf_scene_uniforms.analytic_count; i++) {
        float3 v = ui_sdf_analytic_prim_field(ui_sdf_scene_prims.prims[i].pos,
                                             ui_sdf_scene_prims.prims[i].cell_x,
                                             ui_sdf_scene_prims.prims[i].cell_y,
                                             ui_sdf_scene_prims.prims[i].coef, p,
                                             ui_sdf_scene_uniforms.analytic_range);
        f = ui_sdf_analytic_fold(ui_sdf_scene_uniforms.analytic_combine, ui_sdf_scene_uniforms.analytic_smoothing, i, f, v);
    }
    return f;
}

struct ui_sdf_scene_field {
    float sdf;
    float2 dir;
    bool detail;
};

static ui_sdf_scene_field ui_sdf_scene_field_load(uint2 tid) {
    ui_sdf_scene_field field;
    field.dir = float2(0.0f, 0.0f);

    if (ui_sdf_scene_uniforms.analytic_count != 0u) {
        float3 f = ui_sdf_scene_analytic(float2((float)tid.x, (float)tid.y) + 0.5f);
        field.sdf = f.x;
        field.dir = f.yz;
        field.detail = ui_sdf_near_band(f.x, ui_sdf_scene_uniforms.band_lo, ui_sdf_scene_uniforms.band_hi);
        return field;
    }

    uint2 mid_texel = tid / ui_sdf_mid_scale;
    float4 approx = ui_sdf_scene_in_mid.Load(int3((int)mid_texel.x, (int)mid_texel.y, 0));

    field.sdf = approx.x;
    field.dir = approx.yz;
    field.detail = ui_sdf_near_band(approx.x, ui_sdf_scene_uniforms.band_lo, ui_sdf_scene_uniforms.band_hi);
    if (field.detail) {
        float4 fine = ui_sdf_scene_in_sdf.Load(int3((int)tid.x, (int)tid.y, 0));
        field.sdf = fine.x;
        field.dir = fine.yz;
    }
    return field;
}

static float ui_sdf_scene_sdf(uint2 tid) {
    return ui_sdf_scene_field_load(tid).sdf;
}

static float ui_sdf_scene_outline_coverage(float sdf) {
    if (ui_sdf_scene_uniforms.outline_half <= 0.0f) {
        return 0.0f;
    }
    float band = saturate(0.5f - (abs(sdf - ui_sdf_scene_uniforms.outline_center) - ui_sdf_scene_uniforms.outline_half));
    return min(band, saturate(2.0f * ui_sdf_scene_uniforms.outline_half));
}

static half4 ui_sdf_scene_composite(half4 fill, float sdf) {
    if (ui_sdf_scene_uniforms.region_band != 0u) {
        half band_coverage = (half)ui_sdf_scene_outline_coverage(sdf);
        return fill * band_coverage * (half)ui_sdf_scene_uniforms.opacity;
    }
    half4 color = fill * (half)(ui_sdf_scene_uniforms.has_fill != 0 ? saturate(0.5f - sdf) : 0.0f);
    half outline_coverage = (half)ui_sdf_scene_outline_coverage(sdf);
    half4 outline_color = (half4)ui_sdf_scene_uniforms.outline_color;
    color = outline_color * outline_coverage + color * ((half)1.0f - outline_coverage * outline_color.w);
    return color * (half)ui_sdf_scene_uniforms.opacity;
}

static float4 ui_sdf_scene_out(half4 color, float2 pos) {
    float4 c = (float4)color;
    if (ui_sdf_scene_uniforms.dither != 0u) {
        c = dithered_float4(c, uint2((uint32_t)pos.x, (uint32_t)pos.y));
    }
    return c;
}

struct ui_sdf_scene_vary {
    float4 position: SV_POSITION;
};

[shader("vertex")]
ui_sdf_scene_vary ui_sdf_scene_vertex(uint vid: SV_VertexID) {
    ui_sdf_scene_vary v_out;
    v_out.position = float4(vid == 1 || vid == 4 || vid == 5 ? 1.0f : -1.0f,
                            vid == 2 || vid == 3 || vid == 5 ? 1.0f : -1.0f, 0.0f, 1.0f);
    return v_out;
}

[shader("pixel")]
float4 ui_sdf_scene_solid(float4 position: SV_Position): SV_Target {
    uint2 tid = uint2(position.xy) - ui_sdf_scene_uniforms.out_origin;
    float sdf = ui_sdf_scene_sdf(tid);

    half4 fill = (half4)ui_sdf_scene_uniforms.fill_color;
    if (ui_sdf_scene_uniforms.has_fill != 0 && ui_sdf_scene_uniforms.tint_mode == ui_sdf_tint_gradient) {
        float w = saturate(dot(position.xy, ui_sdf_scene_uniforms.tint_grad_dir) + ui_sdf_scene_uniforms.tint_grad_off);
        fill = lerp((half4)ui_sdf_scene_uniforms.tint_color0, (half4)ui_sdf_scene_uniforms.tint_color1, (half)w);
    }

    return ui_sdf_scene_out(ui_sdf_scene_composite(fill, sdf), position.xy);
}

[shader("pixel")]
float4 ui_sdf_scene_content(float4 position: SV_Position): SV_Target {
    uint2 tid = uint2(position.xy) - ui_sdf_scene_uniforms.out_origin;
    float sdf = ui_sdf_scene_sdf(tid);

    float2 uv = (position.xy - ui_sdf_scene_uniforms.content_ref) * ui_sdf_scene_uniforms.content_scale;

    uv = ui_graphic_uv(ui_sdf_scene_uniforms.content_flags, uv, ui_sdf_scene_uniforms.content_uv_origin, ui_sdf_scene_uniforms.content_uv_size, ui_sdf_scene_uniforms.content_uv_min, ui_sdf_scene_uniforms.content_uv_max);

    half4 src = (half4)ui_sdf_scene_content_tex.Sample(ui_sdf_scene_sampler, uv);

    src = ui_graphic_premultiply_h(src, ui_sdf_scene_uniforms.content_flags);

    if ((ui_sdf_scene_uniforms.content_flags & ui_graphic_flag_cs_convert) != 0) {
        src = ui_cs_convert_premult_h(src, ui_sdf_scene_uniforms.content_flags, ui_sdf_scene_uniforms.cs_col_x, ui_sdf_scene_uniforms.cs_col_y, ui_sdf_scene_uniforms.cs_col_z);
    } else if ((ui_sdf_scene_uniforms.content_flags & ui_graphic_flag_r8_splat) != 0) {
        src.xyzw = src.x;
    }

    return ui_sdf_scene_out(ui_sdf_scene_composite(src * (half4)ui_sdf_scene_uniforms.fill_color, sdf), position.xy);
}

static float4 ui_sdf_scene_mid_bilinear(uint2 tid) {
    float2 uv = (float2((float)tid.x, (float)tid.y) + 0.5f) * ui_sdf_scene_uniforms.mid_uv_scale;
    uv = clamp(uv, ui_sdf_scene_uniforms.mid_uv_min, ui_sdf_scene_uniforms.mid_uv_max);
    return ui_sdf_scene_in_mid.SampleLevel(ui_sdf_scene_sampler, uv, 0.0f);
}

static ui_sdf_scene_field ui_sdf_scene_field_smooth(uint2 tid, ui_sdf_scene_field field) {
    if (field.detail || ui_sdf_scene_uniforms.analytic_count != 0u) {
        return field;
    }

    float4 m = ui_sdf_scene_mid_bilinear(tid);
    field.sdf = m.x;
    float dir_len = length(m.yz);
    field.dir = dir_len > 1e-6f ? m.yz / dir_len : float2(0.0f, 0.0f);
    return field;
}

#include "ui_glass_constants.h"

#define ui_glass_ring_only_pt 2.0f

static float ui_glass_offset_sdf_pt(uint2 tid, float s) {
    float2 p = float2((float)tid.x, (float)tid.y) + 0.5f;
    p.y -= ui_glass_shadow_offset_pt * s;
    if (ui_sdf_scene_uniforms.analytic_count != 0u) {
        return ui_sdf_scene_analytic(p).x / s;
    }
    float2 uv = clamp(p * ui_sdf_scene_uniforms.mid_uv_scale, ui_sdf_scene_uniforms.mid_uv_min, ui_sdf_scene_uniforms.mid_uv_max);
    return ui_sdf_scene_in_mid.SampleLevel(ui_sdf_scene_sampler, uv, 0.0f).x / s;
}

static half ui_glass_shadow_att(float d_pt) {
    float ao = ui_sdf_scene_uniforms.glass_shadow_alpha;
    if (ao <= 0.0f) {
        return (half)1.0f;
    }
    float alpha = ao * ui_glass_soft(d_pt / ui_sdf_scene_uniforms.glass_shadow_radius_pt);
    return (half)(1.0f - alpha);
}

static float ui_glass_ring_a(float d_pt, float cov) {
    if (ui_glass_variant_is_regular(ui_sdf_scene_uniforms.glass_variant) == false) {
        return 0.0f;
    }
    float z0 = d_pt * (1.0f / ui_glass_ring_sigma_pt);
    float z1 = (d_pt + ui_glass_ring_stroke_pt) * (1.0f / ui_glass_ring_sigma_pt);
    float band = saturate(ui_glass_soft(ui_glass_ring_z * z0) - ui_glass_soft(ui_glass_ring_z * z1));
    return ui_glass_ring_opacity * band * cov;
}

static half ui_glass_luma709(half3 c) {
    return dot(c, half3((half)0.2126f, (half)0.7152f, (half)0.0722f));
}

static half ui_glass_luma_bleed(half3 c) {
    return dot(c, half3((half)0.2125f, (half)0.7154f, (half)0.0721f));
}

static half3 ui_glass_cm(float black, float white, float sat, half luma, half3 chroma) {
    return (half)(white - black) * luma + (half)black + (half)sat * chroma;
}

static half3 ui_glass_maxluma_chroma(half k, half3 chroma) {
    return (k * ((half)(1.0f + ui_glass_maxluma_chroma_gain) - (half)ui_glass_maxluma_chroma_gain * k)) * chroma;
}

static half3 ui_glass_vibrant(half3 b) {
    return half3(dot(b, (half3)ui_sdf_scene_uniforms.glass_vibrant_x.xyz),
                 dot(b, (half3)ui_sdf_scene_uniforms.glass_vibrant_y.xyz),
                 dot(b, (half3)ui_sdf_scene_uniforms.glass_vibrant_z.xyz)) + (half)ui_sdf_scene_uniforms.glass_vibrant_x.w;
}

static half3 ui_glass_transform_face(half3 bg) {
    half luma = ui_glass_luma709(bg);
    if (ui_glass_variant_is_regular(ui_sdf_scene_uniforms.glass_variant)) {
        bool dark = ui_sdf_scene_uniforms.glass_variant == 2u;
        half k = saturate((half)1.0f - (half)ui_sdf_scene_uniforms.glass_face.x * luma);
        half kl = k * luma;
        half3 cw = ui_glass_maxluma_chroma(k, bg - luma);
        half fa = (half)ui_sdf_scene_uniforms.glass_face.y;
        half fg = (half)ui_sdf_scene_uniforms.glass_face.z;
        half3 base = dark ? ui_glass_cm(ui_glass_face_black_rgdk, ui_glass_face_white_rgdk, ui_glass_face_sat_rgdk, kl, cw)
                          : ui_glass_cm(ui_glass_face_black_rglt, ui_glass_face_white_rglt, ui_glass_face_sat_rglt, kl, cw);
        return ((half)1.0f - fa) * base + fa * fg;
    }
    return ui_glass_cm(ui_glass_face_black_cr, ui_glass_face_white_cr, ui_glass_face_sat_cr, luma, bg - luma);
}

static half3 ui_glass_transform_gvt(half3 bg, half3 bb) {
    half luma = ui_glass_luma709(bg);
    if (ui_glass_variant_is_regular(ui_sdf_scene_uniforms.glass_variant)) {
        bool dark = ui_sdf_scene_uniforms.glass_variant == 2u;
        half k = saturate((half)1.0f - (half)ui_sdf_scene_uniforms.glass_face.x * luma);
        half kl = k * luma;
        half3 cw = ui_glass_maxluma_chroma(k, bg - luma);
        half fa = (half)ui_sdf_scene_uniforms.glass_face.y;
        half fg = (half)ui_sdf_scene_uniforms.glass_face.z;
        half3 base = dark ? ui_glass_cm(ui_glass_face_black_rgdk, ui_glass_face_white_rgdk, ui_glass_face_sat_rgdk, kl, cw)
                          : ui_glass_cm(ui_glass_face_black_rglt, ui_glass_face_white_rglt, ui_glass_face_sat_rglt, kl, cw);
        half3 face = ((half)1.0f - fa) * base + fa * fg;
        float op = ui_sdf_scene_uniforms.glass_face_op;
        if (op > 0.0f) {
            half luma_b = ui_glass_luma709(bb);
            half3 cw_b = bb - luma_b;
            half3 bc = dark ? ui_glass_cm(ui_glass_bleed_black_rgdk, ui_glass_bleed_white_rgdk, ui_glass_bleed_sat_rgdk, luma_b, cw_b)
                            : ui_glass_cm(ui_glass_bleed_black_rglt, ui_glass_bleed_white_rglt, ui_glass_bleed_sat_rglt, luma_b, cw_b);
            half l2 = saturate(ui_glass_luma_bleed(face));
            half b = dark ? (half)1.0f - l2 : l2;
            b = b * b;
            b = b * b;
            face = face + (bc - face) * (b * (half)op);
        }
        return face;
    }
    return ui_glass_cm(ui_glass_face_black_cr, ui_glass_face_white_cr, ui_glass_face_sat_cr, luma, bg - luma);
}

static half3 ui_glass_transform(half3 bg) {
    return ui_glass_transform_gvt(bg, bg);
}

static float ui_glass_rim_h(float sdf, float2 dir, float s, float fw) {
    float d_px = -sdf;
    float cos_sharp = ui_sdf_scene_uniforms.glass_rim_cos.x;
    float cos_diffuse = ui_sdf_scene_uniforms.glass_rim_cos.y;

    float h_px = ui_glass_rim_height_pt * s;
    float hd_px = h_px * ui_glass_rim_diffuse_height_scale;
    float k = ui_sdf_scene_uniforms.glass_rim_k.x;
    float kd = ui_sdf_scene_uniforms.glass_rim_k.y;

    return ui_glass_rim_lobe(d_px, h_px, cos_sharp, -dir.y, ui_glass_rim_curvature, k, fw)
         + ui_glass_rim_lobe(d_px, h_px, cos_sharp, dir.y, ui_glass_rim_curvature, k, fw)
         + ui_glass_rim_lobe(d_px, hd_px, cos_diffuse, -dir.y, 1.0f, kd, fw)
         + ui_glass_rim_lobe(d_px, hd_px, cos_diffuse, dir.y, 1.0f, kd, fw);
}

static float ui_glass_kfh_h(float sdf, float2 dir, float s, float cov, float fw) {
    float a = (1.0f - cov) * saturate(-(sdf + ui_glass_kfh_offset_pt * s) / fw + 0.5f);
    if (a <= 0.0f) {
        return 0.0f;
    }
    float cos_spread = ui_sdf_scene_uniforms.glass_kfh.x;
    float denom = ui_sdf_scene_uniforms.glass_kfh.y;
    float lp = a * saturate((dir.x - cos_spread) / denom);
    float lm = a * saturate((-dir.x - cos_spread) / denom);
    return lp / (1.0f + ui_glass_kfh_amount * (1.0f - lp))
         + lm / (1.0f + ui_glass_kfh_amount * (1.0f - lm));
}

static float ui_glass_refract_px(float sdf, float s) {
    float d_pt = max(-sdf, 0.0f) / s;
    float u = saturate(d_pt / max(ui_sdf_scene_uniforms.glass_refract.y, 1e-3f));
    return ui_sdf_scene_uniforms.glass_refract.x * (1.0f - sqrt(saturate((2.0f - u) * u))) * s;
}

static half3 ui_glass_tint_transform(half3 tf, half4 tc) {
    if (tc.w <= (half)0.0f) {
        return tf;
    }
    float4 slope = ui_sdf_scene_uniforms.glass_tint_slope;
    float4 bias = ui_sdf_scene_uniforms.glass_tint_bias;
    uint32_t mode = ui_sdf_scene_uniforms.glass_tint_hoist;
    half lt = ui_glass_luma709(tf);
    half3 c = (half)slope.w * tf + (half3)slope.xyz * lt + (half3)bias.xyz;
    if (mode == 1u) {
        c = min(c, (half)ui_glass_tint_cap);
    } else if (mode == 2u) {
        c = max(c, (half)0.0f);
    }
    return tf * ((half)1.0f - tc.w) + c * tc.w;
}

static float2 ui_glass_content_uv(float2 pos_px) {
    float2 uv = (pos_px - ui_sdf_scene_uniforms.content_ref) * ui_sdf_scene_uniforms.content_scale;
    uv = ui_sdf_scene_uniforms.content_uv_origin + clamp(uv, 0.0f, 1.0f) * ui_sdf_scene_uniforms.content_uv_size;
    return clamp(uv, ui_sdf_scene_uniforms.content_uv_min, ui_sdf_scene_uniforms.content_uv_max);
}

static half3 ui_glass_bg(float2 pos_px) {
    return (half3)ui_sdf_scene_content_tex.SampleLevel(ui_sdf_scene_sampler, ui_glass_content_uv(pos_px), 0.0f).xyz;
}

static half3 ui_glass_pyr_sample(float level_f, float2 pos_px) {
    float lod = clamp(level_f, 0.0f, (float)ui_sdf_scene_uniforms.glass_pyr_count);
    float2 pad = (ui_sdf_scene_uniforms.content_uv_min - ui_sdf_scene_uniforms.content_uv_origin) * (exp2(ceil(lod)) - 1.0f);
    float2 lo = ui_sdf_scene_uniforms.content_uv_min + pad;
    float2 hi = ui_sdf_scene_uniforms.content_uv_max - pad;
    float2 mid = 0.5f * (lo + hi);
    float2 uv = clamp(ui_glass_content_uv(pos_px), min(lo, mid), max(hi, mid));
    return (half3)ui_sdf_scene_content_tex.SampleLevel(ui_sdf_scene_sampler, uv, lod).xyz;
}

static float ui_glass_rh_hoisted(float x_pt) {
    if (ui_sdf_scene_uniforms.glass_rh_radius <= 0.0f) {
        return 0.0f;
    }
    float denom = ui_sdf_scene_uniforms.glass_rh_span.y;
    float t0 = denom > 1e-3f ? saturate((x_pt + ui_sdf_scene_uniforms.glass_rh_span.x) / denom) : 1.0f;
    float t1 = saturate(x_pt - ui_glass_blur_knot1_pt);
    float t2 = x_pt > 0.0f ? 1.0f : 0.0f;
    float4 o = ui_sdf_scene_uniforms.glass_rh_o;
    float r = ui_sdf_scene_uniforms.glass_rh_radius * (o.x - o.y * t0 - o.z * t1 - o.w * t2);
    return r > 0.0f ? ui_glass_blur_prescale * r : 0.0f;
}

static half3 ui_glass_backdrop_x(float2 pos_px, float x_pt, float s) {
    return ui_glass_pyr_sample(ui_glass_blur_lod(ui_glass_rh_hoisted(x_pt), s), pos_px);
}

static half3 ui_glass_fill(half3 fine, float2 pos_px) {
    if (ui_sdf_scene_uniforms.glass_fill_active == 0u) {
        return fine;
    }
    float veil_lod = ui_sdf_scene_uniforms.glass_pyr_veil;
    float veil_off_px = ui_glass_veil_tap_texels * ui_glass_capture_texel_px * veil_lod;
    float2 veil_off = float2(veil_off_px, veil_off_px);
    half3 veil = (ui_glass_pyr_sample(veil_lod, pos_px + veil_off)
                + ui_glass_pyr_sample(veil_lod, pos_px - veil_off)) * (half)0.5f;
    half3 c = fine + (half)ui_sdf_scene_uniforms.glass_fill_dklk.x * (min(fine, veil) - fine)
                   + (half)ui_sdf_scene_uniforms.glass_fill_dklk.y * (max(fine, veil) - fine);
    return c + (veil - c) * (half)ui_sdf_scene_uniforms.glass_fill_wn;
}

static half3 ui_glass_face_sample(float2 pos_px, float sdf, float2 dir, float s) {
    float sdf_pt = sdf / s;
    float refract_px = ui_glass_refract_px(sdf, s);
    half3 c = ui_glass_backdrop_x(pos_px - dir * refract_px, sdf_pt - refract_px / s, s);
    c = ui_glass_fill(c, pos_px);
    float w_out = ui_sdf_scene_uniforms.glass_outer_opacity * saturate(sdf_pt + 1.0f);
    if (w_out > 0.0f) {
        float u = saturate(max(-sdf_pt, 0.0f) / max(ui_sdf_scene_uniforms.glass_outer_ah.y, 1e-3f));
        float off_out = ui_sdf_scene_uniforms.glass_outer_ah.x * (1.0f - sqrt(saturate((2.0f - u) * u)));
        half3 c_out = ui_glass_backdrop_x(pos_px + dir * (off_out * s), sdf_pt + off_out, s);
        c = lerp(c, c_out, (half)w_out);
    }
    return c;
}

static half3 ui_glass_transform_fill(half3 fill, float2 pos_px, float sdf, float2 dir, float s) {
    float bleed_pt = ui_sdf_scene_uniforms.glass_bleed_radius_pt;
    float u = saturate(max(-sdf, 0.0f) / max(bleed_pt * s, 1e-3f));
    float off_px = bleed_pt * (1.0f - sqrt(saturate((2.0f - u) * u))) * s;
    half3 bb = ui_glass_pyr_sample(ui_sdf_scene_uniforms.glass_pyr_bleed, pos_px + dir * off_px);
    return ui_glass_transform_gvt(fill, bb);
}

[shader("pixel")]
float4 ui_sdf_scene_glass(float4 position: SV_Position): SV_Target {
    uint2 tid = uint2(position.xy) - ui_sdf_scene_uniforms.out_origin;

    ui_sdf_scene_field field = ui_sdf_scene_field_load(tid);
    float sdf = field.sdf;
    float2 dir = field.dir;
    bool in_band = field.detail;

    float s = max(ui_sdf_scene_uniforms.glass_scale, 1e-3f);
    const uint gv = ui_sdf_scene_uniforms.glass_variant;

    if (in_band == false) {
        field = ui_sdf_scene_field_smooth(tid, field);
        sdf = field.sdf;
        dir = field.dir;

        if (sdf > ui_sdf_scene_uniforms.reach_hi) {
            return float4(0.0f, 0.0f, 0.0f, 0.0f);
        }

        float ring_reach_pt = ui_glass_shadow_offset_pt + ui_glass_ring_stroke_pt
                            + ui_glass_soft_reach * ui_glass_ring_sigma_pt * (1.0f / ui_glass_ring_z);
        bool offset_active = ui_glass_variant_is_regular(gv) && sdf > -ring_reach_pt * s;
        float sdf2_smooth_pt = offset_active ? ui_glass_offset_sdf_pt(tid, s) : -ring_reach_pt;

        if (sdf > 0.0f) {
            half att_out = ui_glass_shadow_att(sdf2_smooth_pt);
            return ui_sdf_scene_out(half4((half)0.0f, (half)0.0f, (half)0.0f, (half)1.0f - att_out) * (half)ui_sdf_scene_uniforms.opacity, position.xy);
        }

        half3 face_in = ui_glass_face_sample(position.xy, sdf, dir, s);
        half3 tf_in;
        if (ui_glass_variant_is_regular(gv)) {
            tf_in = ui_glass_transform_fill(face_in, position.xy, sdf, dir, s);
        } else {
            tf_in = ui_glass_transform(face_in);
        }
        tf_in = tf_in * (half)(1.0f - ui_glass_ring_a(sdf2_smooth_pt, 1.0f));
        tf_in = clamp(tf_in, (half)ui_glass_clamp_lo, (half)ui_sdf_scene_uniforms.glass_face.w);
        if (ui_sdf_scene_uniforms.tint_mode != ui_sdf_tint_none) {
            tf_in = clamp(ui_glass_tint_transform(tf_in, (half4)ui_sdf_scene_uniforms.tint_color0), (half)0.0f, (half)ui_glass_tint_cap);
        }

        return ui_sdf_scene_out(half4(max(tf_in, (half)0.0f), (half)1.0f) * (half)ui_sdf_scene_uniforms.opacity, position.xy);
    }

    float sdf2_pt = ui_glass_variant_is_regular(gv) ? ui_glass_offset_sdf_pt(tid, s) : 0.0f;
    half att = ui_glass_shadow_att(sdf2_pt);

    float kfh_tail_px = -ui_glass_kfh_offset_pt * s + ui_glass_kfh_tail_pad_px;
    if (ui_glass_variant_is_regular(gv) && sdf > max(ui_glass_ring_only_pt * s, kfh_tail_px)) {
        return ui_sdf_scene_out(half4((half)0.0f, (half)0.0f, (half)0.0f, (half)1.0f - att) * (half)ui_sdf_scene_uniforms.opacity, position.xy);
    }

    half3 bg_raw = ui_glass_bg(position.xy);

    half3 face_bd = ui_glass_face_sample(position.xy, sdf, dir, s);
    half3 tf;
    if (ui_glass_variant_is_regular(gv)) {
        tf = ui_glass_transform_fill(face_bd, position.xy, sdf, dir, s);
    } else {
        tf = ui_glass_transform(face_bd);
    }

    float fw = max(abs(dir.x) + abs(dir.y), 1e-4f);
    float coverage = saturate(0.5f - sdf / fw);
    half cov = (half)coverage;

    float h_kfh = ui_glass_kfh_h(sdf, dir, s, coverage, fw);
    half3 under = bg_raw;
    if (h_kfh > 0.0f) {
        half3 tf_sharp = min(ui_glass_transform_face(bg_raw), bg_raw);
        under = bg_raw + (half)h_kfh * (tf_sharp - bg_raw);
    }

    float ring_a = ui_glass_ring_a(sdf2_pt, coverage);
    half3 outc = (tf * cov + under * att * ((half)1.0f - cov)) * (half)(1.0f - ring_a);
    if (h_kfh > 0.0f) {
        half kb = (half)(ui_glass_kfh_color_bias * h_kfh);
        outc = outc * ((half)1.0f + kb * ((half)3.0f - (half)2.0f * outc));
    }

    float a_k = (coverage + (1.0f - (float)att) * (1.0f - coverage)) * (1.0f - ring_a) + ring_a;
    a_k = a_k + (1.0f - a_k) * h_kfh;
    float3 through = (float3)bg_raw * (1.0f - a_k);
    float3 kr = (float3)outc - through;
    kr = clamp(kr / max(a_k, 1e-6f), ui_glass_clamp_lo, ui_sdf_scene_uniforms.glass_face.w) * a_k;
    outc = (half3)(kr + through);

    float a_f = a_k;
    if (ui_sdf_scene_uniforms.tint_mode != ui_sdf_tint_none) {
        half4 tint_color = (half4)ui_sdf_scene_uniforms.tint_color0;
        float cov_t = coverage;
        if (gv == 1u || gv == 3u) {
            cov_t = saturate(0.5f - (sdf + ui_glass_kfh_offset_pt * s) / fw);
        }
        tint_color.w = tint_color.w * (half)cov_t;
        outc = clamp(ui_glass_tint_transform(outc, tint_color), (half)0.0f, (half)ui_glass_tint_cap);
        a_f = a_f + (1.0f - a_f) * (float)tint_color.w;
    }

    float h_rim = ui_glass_rim_h(sdf, dir, s, fw);
    half3 vib = clamp(ui_glass_vibrant(outc), (half)ui_glass_rim_clip_lo, (half)ui_glass_rim_clip_hi);
    outc = outc + (half)h_rim * (vib - outc);
    a_f = a_f + (1.0f - a_f) * saturate(h_rim);

    half3 premult = max(outc, (half)0.0f) - bg_raw * (half)(1.0f - a_f);
    return ui_sdf_scene_out(half4(premult, (half)a_f) * (half)ui_sdf_scene_uniforms.opacity, position.xy);
}
