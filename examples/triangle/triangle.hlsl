#include "cr.h"

struct triangle_v_out {
    float4 pos: SV_POSITION;
    [[vk::location(0)]] float3 color;
};

struct triangle_f_in {
    [[vk::location(0)]] float3 color;
};

[shader("vertex")]
triangle_v_out triangle_v(uint vid: SV_VertexID) {
    float2 positions[3] = { float2(0.0f, 0.7f), float2(-0.7f, -0.5f), float2(0.7f, -0.5f) };
    float3 colors[3] = { float3(1.0f, 0.0f, 0.0f), float3(0.0f, 1.0f, 0.0f), float3(0.0f, 0.0f, 1.0f) };

    triangle_v_out v_out;
    v_out.pos = float4(positions[vid], 0.0f, 1.0f);
    v_out.color = colors[vid];

    return v_out;
}

[shader("pixel")]
float4 triangle_f(float4 frag_pos: SV_Position, triangle_f_in v_out): SV_Target {
    return float4(v_out.color, 1.0f);
}
