#include "cr.h"
#include "ui_shared.h"

#ifndef ui_graphic_sdf
#define ui_graphic_sdf 0
#endif

[[vk::binding(ui_bind_group_0_globals, 0)]] ConstantBuffer<ui_globals> group_0_globals;
[[vk::binding(ui_bind_group_0_uniforms, 0)]] ConstantBuffer<ui_uniforms_graphic> group_0_uniforms;
[[vk::binding(ui_bind_group_0_vertices, 0)]] StructuredBuffer<ui_vertex_graphic> group_0_vertices;

#if ui_graphic_sdf
#define ui_sdf_mask_prefix ui_graphic
#include "ui_sdf_mask.h"
#define ui_v_graphic_entry ui_v_graphic_sdf
#define ui_f_graphic_entry ui_f_graphic_sdf
#else
#define ui_v_graphic_entry ui_v_graphic
#define ui_f_graphic_entry ui_f_graphic
#endif

[[vk::binding(ui_bind_group_1_texture, 1)]] cr_tex_half4 group_1_texture_0;
[[vk::binding(ui_bind_group_1_sampler, 1)]] SamplerState group_1_sampler_0;

struct ui_v_out {
    float4 pos: SV_POSITION;
    [[vk::location(0)]] float2 uv;
#if ui_graphic_sdf
    [[vk::location(1)]] float sdf_ref;
#endif
};

struct ui_f_in {
    [[vk::location(0)]] float2 uv;
#if ui_graphic_sdf
    [[vk::location(1)]] float sdf_ref;
#endif
};

[shader("vertex")]
ui_v_out ui_v_graphic_entry(uint vid: SV_VertexID) {
    ui_vertex_graphic v_in = group_0_vertices[vid];

    float2 pos_px = v_in.pos_px;
    pos_px = pos_px.x * group_0_uniforms.transform_x + pos_px.y * group_0_uniforms.transform_y + group_0_uniforms.transform_z;

    ui_v_out v_out;
    v_out.pos.x = -1.0f + group_0_globals.to_clip_from_px_offset.x + pos_px.x * group_0_globals.to_clip_from_px_scale.x;
    v_out.pos.y = 1.0f - group_0_globals.to_clip_from_px_offset.y - pos_px.y * group_0_globals.to_clip_from_px_scale.y;
    v_out.pos.z = 0.0f;
    v_out.pos.w = 1.0f;
    v_out.uv = v_in.uv;
#if ui_graphic_sdf
    v_out.sdf_ref = v_in.sdf_ref;
#endif

    return v_out;
}

[shader("pixel")]
float4 ui_f_graphic_entry(float4 frag_pos: SV_Position, ui_f_in v_out): SV_Target {
    float2 uv = v_out.uv;

    uv = ui_graphic_uv(group_0_uniforms.flags, uv, group_0_uniforms.uv_origin, group_0_uniforms.uv_size, group_0_uniforms.uv_min, group_0_uniforms.uv_max);

    half4 src = (half4)group_1_texture_0.Sample(group_1_sampler_0, uv);

    src = ui_graphic_premultiply_h(src, group_0_uniforms.flags);

    if ((group_0_uniforms.flags & ui_graphic_flag_cs_convert) != 0) {
        src = ui_cs_convert_premult_h(src, group_0_uniforms.flags, group_0_uniforms.cs_col_x, group_0_uniforms.cs_col_y, group_0_uniforms.cs_col_z);
    } else if ((group_0_uniforms.flags & ui_graphic_flag_r8_splat) != 0) {
        src.xyzw = src.x;
    }

    src *= (half4)group_0_uniforms.tint;

#if ui_graphic_sdf
    float2 local_px = frag_pos.x * group_0_uniforms.inverse_x + frag_pos.y * group_0_uniforms.inverse_y + group_0_uniforms.inverse_z;
    src *= (half)ui_sdf_mask_coverage(v_out.sdf_ref, local_px);
#endif
    return ui_dither_premult((float4)src, group_0_uniforms.flags, frag_pos.xy);
}
