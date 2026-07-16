#include "cr.h"
#include "ui_shared.h"

[[vk::binding(ui_bind_group_0_globals, 0)]] ConstantBuffer<ui_globals> group_0_globals;
[[vk::binding(ui_bind_group_0_uniforms, 0)]] ConstantBuffer<ui_uniforms_final> group_0_uniforms;
[[vk::binding(ui_bind_group_0_vertices, 0)]] StructuredBuffer<ui_vertex_final> group_0_vertices;

[[vk::binding(ui_bind_group_1_texture, 1)]] cr_tex_half4 group_1_texture_0;
[[vk::binding(ui_bind_group_1_sampler, 1)]] SamplerState group_1_sampler_0;

struct ui_v_out {
    float4 pos: SV_POSITION;
    [[vk::location(0)]] float2 uv;
};

struct ui_f_in {
    [[vk::location(0)]] float2 uv;
};

[shader("vertex")]
ui_v_out ui_v_final(uint vid: SV_VertexID) {
    ui_vertex_final v_in = group_0_vertices[vid];

    float2 pos_px = (float2)v_in.pos_px;

    ui_v_out v_out;
    v_out.pos.x = -1.0f + group_0_globals.to_clip_from_px_offset.x + pos_px.x * group_0_globals.to_clip_from_px_scale.x;
    v_out.pos.y = 1.0f - group_0_globals.to_clip_from_px_offset.y - pos_px.y * group_0_globals.to_clip_from_px_scale.y;
    v_out.pos.z = 0.0f;
    v_out.pos.w = 1.0f;
    v_out.uv = v_in.uv;

    return v_out;
}

half3 ui_final_convert(half3 c) {
    if ((group_0_uniforms.flags & ui_graphic_flag_cs_convert) != 0) {
        if ((group_0_uniforms.flags & ui_graphic_flag_cs_decode) != 0) {
            c = to_linear_srgb_from_srgb_signed_half3(c);
        }
        c = (half3)group_0_uniforms.cs_col_x.xyz * c.x + (half3)group_0_uniforms.cs_col_y.xyz * c.y + (half3)group_0_uniforms.cs_col_z.xyz * c.z;
        if ((group_0_uniforms.flags & ui_graphic_flag_cs_encode) != 0) {
            c = to_srgb_from_linear_srgb_signed_half3(c);
        }
    }
    return c;
}

[shader("pixel")]
float4 ui_f_final(float4 frag_pos: SV_Position, ui_f_in v_out): SV_Target {
    half4 src = (half4)group_1_texture_0.Sample(group_1_sampler_0, v_out.uv);
    half a = src.w;
    half3 c = a > (half)0.0f ? src.xyz / a : half3((half)0.0f, (half)0.0f, (half)0.0f);
    c = ui_final_convert(c);
    if ((group_0_uniforms.flags & ui_graphic_flag_premultiplied) != 0) {
        c *= a;
    }
    float4 c_out = float4((float3)c, (float)a);
    if ((group_0_uniforms.flags & ui_graphic_flag_dither) != 0) {
        c_out = dithered_float4(c_out, uint2((uint32_t)frag_pos.x, (uint32_t)frag_pos.y));
    }
    return c_out;
}
