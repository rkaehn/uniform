
[[vk::binding(ui_blur_bind_params, 0)]] ConstantBuffer<ui_blur_down_params> down_params;
[[vk::binding(ui_blur_bind_src, 0)]] cr_tex_half4 down_src;
[[vk::binding(ui_blur_bind_sampler, 0)]] SamplerState down_sampler;

[ForceInline]
static half4 ui_blur_down_tap(float2 pos) {
    float2 uv = down_params.uv_origin + pos * down_params.uv_texel;
    return (half4)down_src.SampleLevel(down_sampler, clamp(uv, down_params.uv_min, down_params.uv_max), 0.0f);
}

[ForceInline]
static half4 ui_blur_down_eval(float2 center) {
    const float o1 = ui_blur_down_o1;
    const float o2 = ui_blur_down_o2;

    if (down_params.mode == ui_blur_down_mode_tap) {
        return ui_blur_down_tap(center);
    }

    half4 acc = (half)ui_blur_down_w0 * ui_blur_down_tap(center);
    acc += (half)ui_blur_down_w1 * (ui_blur_down_tap(center + float2(o1, 0.0f))
                                 + ui_blur_down_tap(center - float2(o1, 0.0f))
                                 + ui_blur_down_tap(center + float2(0.0f, o1))
                                 + ui_blur_down_tap(center - float2(0.0f, o1)));
    acc += (half)ui_blur_down_w2 * (ui_blur_down_tap(center + float2(o1, o1))
                                 + ui_blur_down_tap(center + float2(-o1, o1))
                                 + ui_blur_down_tap(center + float2(o1, -o1))
                                 + ui_blur_down_tap(center - float2(o1, o1)));
    acc += (half)ui_blur_down_w3 * (ui_blur_down_tap(center + float2(o2, 0.0f))
                                 + ui_blur_down_tap(center - float2(o2, 0.0f))
                                 + ui_blur_down_tap(center + float2(0.0f, o2))
                                 + ui_blur_down_tap(center - float2(0.0f, o2)));
    return acc;
}
