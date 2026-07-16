
#define ui_blur_bind_params 0
#define ui_blur_bind_src 1
#define ui_blur_bind_sampler 2
#define ui_blur_bind_dst 3
#define ui_blur_bind_count 4
#define ui_blur_bind_dst2 4
#define ui_blur_bind_dst3 5
#define ui_blur_bind_dst4 6
#define ui_blur_pair_bind_count 5
#define ui_blur_tail_bind_count 7

#define ui_blur_down_sigma_sq 12.5
#define ui_blur_down_o1 1.960085153579712f
#define ui_blur_down_o2 3.920675754547119f
#define ui_blur_down_w0 0.10549704730510712f
#define ui_blur_down_w1 0.09018648415803909f
#define ui_blur_down_w2 0.07709790766239166f
#define ui_blur_down_w3 0.056341350078582764f

#define ui_blur_pair_tile 16
#define ui_blur_pair_span (2 * ui_blur_pair_tile + 8)
#define ui_blur_box_max_levels 3
#define ui_blur_tail_threads 256
#define ui_blur_tail_max_px 2048
#define ui_blur_tail_max_levels 4

#define ui_blur_max_extent 24
#define ui_blur_max_tap_pairs 7
#define ui_blur_max_weight_vecs 7

#define ui_blur_tile_w 16
#define ui_blur_tile_h 64
#define ui_blur_rows_per_thread 4

struct ui_blur_fused_params {
    float4 color;
    float2 uv_origin;
    float2 uv_texel;
    float2 uv_min;
    float2 uv_max;
    uint2 dst_origin;
    uint2 dst_size;
    uint32_t tap_count;
    uint32_t radius;
    uint32_t colorize;
    uint32_t _pad0;
    float4 taps[ui_blur_max_tap_pairs];
    float4 weights[ui_blur_max_weight_vecs];
};

struct ui_blur_scale_params {
    float4 color;
    float2 uv_origin;
    float2 uv_step;
    float2 uv_min;
    float2 uv_max;
    uint2 dst_origin;
    uint2 dst_size;
    uint32_t colorize;
    uint32_t taps;
    uint32_t levels;
    uint32_t _pad0;
};

#define ui_blur_down_mode_filter 0
#define ui_blur_down_mode_tap 1

struct ui_blur_down_params {
    float2 uv_origin;
    float2 uv_texel;
    float2 uv_min;
    float2 uv_max;
    uint2 dst_origin;
    uint2 dst_size;
    uint32_t mode;
    uint32_t _pad0;
    uint2 dst1_size;
    uint32_t level_count;
    uint32_t box_levels;
    uint2 dst0_size;
    uint2 dst1_origin;
};
