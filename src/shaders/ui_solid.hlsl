#include "cr.h"
#include "ui_shared.h"

#ifndef ui_solid_sdf
#define ui_solid_sdf 0
#endif

[[vk::binding(ui_bind_group_0_globals, 0)]] ConstantBuffer<ui_globals> group_0_globals;
[[vk::binding(ui_bind_group_0_uniforms, 0)]] ConstantBuffer<ui_uniforms_solid> group_0_uniforms;
[[vk::binding(ui_bind_group_0_vertices, 0)]] StructuredBuffer<ui_vertex_solid> group_0_vertices;

#if ui_solid_sdf
#define ui_sdf_mask_prefix ui_solid
#include "ui_sdf_mask.h"
#define ui_v_solid_entry ui_v_solid_sdf
#define ui_f_solid_entry ui_f_solid_sdf
#else
#define ui_v_solid_entry ui_v_solid
#define ui_f_solid_entry ui_f_solid
#endif

struct ui_v_out {
    float4 pos: SV_POSITION;
#if ui_solid_sdf
    [[vk::location(0)]] float sdf_ref;
#endif
};

[shader("vertex")]
ui_v_out ui_v_solid_entry(uint vid: SV_VertexID) {
    ui_vertex_solid v_in = group_0_vertices[vid];

    float2 pos_px = v_in.pos_px;
    pos_px = pos_px.x * group_0_uniforms.transform_x + pos_px.y * group_0_uniforms.transform_y + group_0_uniforms.transform_z;

    ui_v_out v_out;
    v_out.pos.x = -1.0f + group_0_globals.to_clip_from_px_offset.x + pos_px.x * group_0_globals.to_clip_from_px_scale.x;
    v_out.pos.y = 1.0f - group_0_globals.to_clip_from_px_offset.y - pos_px.y * group_0_globals.to_clip_from_px_scale.y;
    v_out.pos.z = 0.0f;
    v_out.pos.w = 1.0f;
#if ui_solid_sdf
    v_out.sdf_ref = v_in.sdf_ref;
#endif

    return v_out;
}

#if ui_solid_sdf
struct ui_f_in {
    [[vk::location(0)]] float sdf_ref;
};

[shader("pixel")]
float4 ui_f_solid_entry(float4 frag_pos: SV_Position, ui_f_in v_out): SV_Target {
    float2 local_px = frag_pos.x * group_0_uniforms.inverse_x + frag_pos.y * group_0_uniforms.inverse_y + group_0_uniforms.inverse_z;
    float4 color = group_0_uniforms.color * ui_sdf_mask_coverage(v_out.sdf_ref, local_px);
    return ui_dither_premult(color, group_0_uniforms.flags, frag_pos.xy);
}
#else
[shader("pixel")]
float4 ui_f_solid_entry(float4 frag_pos: SV_Position): SV_Target {
    return ui_dither_premult(group_0_uniforms.color, group_0_uniforms.flags, frag_pos.xy);
}
#endif
