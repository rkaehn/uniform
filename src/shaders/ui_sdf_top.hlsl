#include "ui_sdf_eval.h"

[[vk::binding(ui_sdf_bind_top_args, 0)]] RWStructuredBuffer<uint32_t> ui_sdf_args;
[[vk::binding(ui_sdf_bind_top_out, 0)]] RWStructuredBuffer<float> ui_sdf_out_top;

[shader("compute")]
[numthreads(8, 8, 1)]
void ui_sdf_top(uint3 dispatch_id: SV_DispatchThreadID) {
    uint2 tid = dispatch_id.xy;

    if (tid.x == 0 && tid.y == 0) {
        ui_sdf_args[0] = 0;
        ui_sdf_args[1] = 1;
        ui_sdf_args[2] = 1;
    }

    if (ui_sdf_uniforms.use_top == 0 || tid.x >= ui_sdf_uniforms.top_size.x || tid.y >= ui_sdf_uniforms.top_size.y) {
        return;
    }

    float2 p = (float2(tid.x, tid.y) + 0.5f) * (float)ui_sdf_top_scale;
    float f = ui_sdf_distance(p, ui_sdf_far);

    ui_sdf_out_top[tid.y * ui_sdf_uniforms.top_size.x + tid.x] = f;
}
