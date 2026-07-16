#include "cr.h"
#include "ui_shared.h"

[[vk::binding(ui_bind_group_0_globals, 0)]] ConstantBuffer<ui_globals> group_0_globals;
[[vk::binding(ui_bind_group_0_uniforms, 0)]] ConstantBuffer<ui_uniforms_glyph> group_0_uniforms;
[[vk::binding(ui_bind_group_0_vertices, 0)]] StructuredBuffer<ui_quad_glyph> group_0_vertices;

[[vk::binding(ui_bind_group_1_texture, 1)]] cr_tex_half4 group_1_texture_0;
[[vk::binding(ui_bind_group_1_sampler, 1)]] SamplerState group_1_sampler_0;

struct ui_v_out {
    float4 pos: SV_POSITION;
    [[vk::location(0)]] float2 uv;
    [[vk::location(1)]] float4 color;
};

struct ui_f_in {
    [[vk::location(0)]] float2 uv;
    [[vk::location(1)]] float4 color;
};

int2 to_int2_from_uint32(uint32_t x) {
    int2 v;
    v.x = (int)(x << 16) >> 16;
    v.y = (int)x >> 16;
    return v;
}

[shader("vertex")]
ui_v_out ui_v_glyph(uint vid: SV_VertexID) {
    uint quad_index = vid / 6;

    ui_quad_glyph v_in = group_0_vertices[group_0_uniforms.start_index + quad_index];

    float2 origin_px = v_in.origin_px;
    int2 size_px = to_int2_from_uint32(v_in.size_px);

    int2 origin_uv_px = to_int2_from_uint32(v_in.origin_uv_px);
    int2 total_uv_px = to_int2_from_uint32(v_in.total_uv_px);

    uint index_in_quad = vid - quad_index * 6;
    int2 corner = int2(0, 0);

    if (index_in_quad == 1) {
        corner.x = size_px.x;
        corner.y = 0;
    }
    if (index_in_quad == 2) {
        corner.x = 0;
        corner.y = size_px.y;
    }
    if (index_in_quad == 3) {
        corner = size_px;
    }
    if (index_in_quad == 4) {
        corner.x = 0;
        corner.y = size_px.y;
    }
    if (index_in_quad == 5) {
        corner.x = size_px.x;
        corner.y = 0;
    }

    float2 px = origin_px + (float2)corner;
    int2 uv_px = origin_uv_px + corner;

    float2 px_t = px.x * group_0_uniforms.transform_x + px.y * group_0_uniforms.transform_y + group_0_uniforms.transform_z;
    float2 px_f = group_0_globals.to_clip_from_px_offset + px_t * group_0_globals.to_clip_from_px_scale;

    ui_v_out v_out;
    v_out.pos.x = -1.0f + px_f.x;
    v_out.pos.y = 1.0f - px_f.y;
    v_out.pos.z = 0.0f;
    v_out.pos.w = 1.0f;
    v_out.uv = (float2)uv_px / (float2)total_uv_px;
    v_out.color = v_in.color;

    return v_out;
}

[shader("pixel")]
float4 ui_f_glyph(float4 frag_pos: SV_Position, ui_f_in v_out): SV_Target {
    half4 paint = (half4)v_out.color;
    if (group_0_uniforms.tint_mode != 0u) {
        float w = saturate(dot(frag_pos.xy, group_0_uniforms.grad_dir) + group_0_uniforms.grad_off);
        paint = lerp((half4)group_0_uniforms.tint_color0, (half4)group_0_uniforms.tint_color1, (half)w);
    }
    float4 c = (float4)((half)group_1_texture_0.Sample(group_1_sampler_0, v_out.uv).x * paint);
    return ui_dither_premult(c, group_0_uniforms.flags, frag_pos.xy);
}
