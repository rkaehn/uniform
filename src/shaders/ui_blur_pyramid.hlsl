#include "cr.h"
#include "ui_blur_shared.h"
#include "ui_blur_down_shared.h"

[[vk::binding(ui_blur_bind_dst, 0)]] [[vk::image_format("rgba16f")]] WTexture2D<float4> down_dst;

[shader("compute")]
[numthreads(8, 8, 1)]
void ui_blur_down(uint3 dispatch_id: SV_DispatchThreadID) {
    uint2 tid = dispatch_id.xy;

    if (tid.x >= down_params.dst_size.x || tid.y >= down_params.dst_size.y) {
        return;
    }

    float2 center = 2.0f * float2(tid.x, tid.y) + 1.0f;
    down_dst.Store(down_params.dst_origin + tid, (float4)ui_blur_down_eval(center));
}
