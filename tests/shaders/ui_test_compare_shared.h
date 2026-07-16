
#define ui_test_compare_bind_params 0
#define ui_test_compare_bind_gpu 1
#define ui_test_compare_bind_ref 2
#define ui_test_compare_bind_tiles 3
#define ui_test_compare_bind_count 4

#define ui_test_compare_tile_size 32
#define ui_test_compare_threads 256

#define ui_test_compare_flag_clamp 0x1u

#define ui_test_compare_band_circle 1u

#define ui_test_compare_band_rim_pt 3.0f
#define ui_test_compare_band_bevel_pt 24.0f

struct ui_test_compare_params {
    uint2 size;
    uint32_t tiles_x;
    uint32_t flags;
    float alarm_diff;
    float to_pt;
    uint32_t band_type;
    float band_radius;
    float2 band_origin;
    float2 band_size;
};

struct ui_test_compare_tile {
    float sum;
    uint32_t over_005;
    uint32_t alarm;
    float max_d;
    uint32_t max_idx;
    float rim_sum;
    float bevel_sum;
    float interior_sum;
    uint32_t rim_count;
    uint32_t bevel_count;
    uint32_t interior_count;
    uint32_t pad;
};
