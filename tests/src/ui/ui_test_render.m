#include "ui_tests.h"
#include "ui_core.h"
#include "ui_atlas.h"
#include "ui_color.h"
#include "ui_render.h"
#include <stdio.h>
#include <string.h>


static void ui_test_check(bool ok, const char* label) {
    ui_test_group_check_("render", ok, label);
}

static void ui_test_render_finish(uint8_t** px, uint32_t count) {
    for (uint32_t i = 0; i < count; i++) {
        cr_free(cr_mem_libc, px[i], cr_loc);
    }
    ui_reset_slots(ui_ctx());
}

static void ui_test_zero_area_graphic_check(void* args) {
    ui_reset_slots(ui_ctx());
}

static void ui_test_zero_area_graphic(void) {
    struct ui* ui = ui_ctx();

    uint8_t pixels[4 * 4] = { 255, 255, 255, 255, 0, 0, 0, 255, 0, 0, 0, 255, 255, 255, 255, 255 };
    struct ui_graphic* graphic = ui_graphic_create(2, 2, ui_pixel_format_bgra8, pixels, 8, true);

    struct ui_node root = ui_frame(&(struct ui_frame) {
        .size = ui_size_fix(64, 64),
    });

    ui_line(&(struct ui_line) {
        .parent = root,
        .pos = ui_pos_xy(4, 4),
        .size = ui_size_fix(40, 0),
        .fill = ui_fill_graphic(graphic, ui_address_mode_repeat, ui_white),
    });

    struct ui_path* path = ui_path_create();
    ui_path_move_to(path, make_double2(0.0, 0.0));
    ui_path_line_to(path, make_double2(20.0, 12.0));
    ui_shape(&(struct ui_shape) {
        .parent = root,
        .pos = ui_pos_xy(8, 20),
        .size = ui_size_fix(0, 0),
        .mode = ui_shape_mode_stroke,
        .thickness = 2.0,
        .path = path,
        .fill = ui_fill_graphic(graphic, ui_address_mode_fill, ui_white),
    });
    ui_path_destroy(path);

    ui_layout(root);

    struct gpu_texture* target = gpu_texture_create(ui->device, &(struct gpu_texture_desc) {
        .label = "test render target",
        .format = gpu_format_bgra8,
        .size = { 64, 64 },
        .usage = gpu_texture_usage_render_target,
    });
    struct gpu_command_buffer* command_buffer = gpu_command_buffer_create(ui->device, "test render");

    ui_render(root, command_buffer, target, (struct ui_render_desc) { 0 });

    ui_test_await_(gpu_command_buffer_commit(command_buffer));
    gpu_texture_destroy(ui->device, target);

    ui_release(root);
    ui_graphic_destroy(graphic);
    ui_test_then_(ui_test_zero_area_graphic_check, NULL);
}

static struct ui_render_desc ui_test_render_desc_ = { .compositing_format = ui_pixel_format_rgba16f };

static uint8_t* ui_test_render_backdrop_(struct ui_node root);

static uint8_t* ui_test_render_glass(struct ui_color tint) {
    struct ui_node root = ui_frame(&(struct ui_frame) {
        .size = ui_size_fix(128, 128),
        .fill = ui_fill_solid(ui_rgba(0.22f, 0.24f, 0.3f, 1.0f)),
    });

    for (uint32_t i = 0; i < 8; i++) {
        ui_frame(&(struct ui_frame) {
            .parent = root,
            .pos = ui_pos_xy((double)(i * 16 + 4), 0),
            .size = ui_size_fix(8, 128),
            .fill = ui_fill_solid(ui_rgba(0.15f + 0.1f * (float)(i % 3), 0.5f, 0.75f - 0.08f * (float)i, 1.0f)),
        });
    }

    ui_frame(&(struct ui_frame) {
        .parent = root,
        .pos = ui_pos_xy(16, 16),
        .size = ui_size_fix(96, 96),
        .corners = ui_corners(16),
        .fill.layers[0] = ui_layer_glass_ex(ui_glass_variant_clear, ui_glass_appearance_dark, tint, ui_glass_slider_default),
    });

    return ui_test_render_backdrop_(root);
}

static uint32_t ui_test_glass_max_diff(const uint8_t* a, const uint8_t* b, uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1) {
    uint32_t max_diff = 0;
    for (uint32_t y = y0; y < y1; y++) {
        for (uint32_t x = x0; x < x1; x++) {
            for (uint32_t c = 0; c < 4; c++) {
                uint32_t av = a[(y * 128 + x) * 4 + c];
                uint32_t bv = b[(y * 128 + x) * 4 + c];
                max_diff = max_uint32(max_diff, av > bv ? av - bv : bv - av);
            }
        }
    }
    return max_diff;
}

static double ui_test_glass_mean_diff(const uint8_t* a, const uint8_t* b, uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1) {
    double sum = 0.0;
    for (uint32_t y = y0; y < y1; y++) {
        for (uint32_t x = x0; x < x1; x++) {
            for (uint32_t c = 0; c < 4; c++) {
                uint32_t av = a[(y * 128 + x) * 4 + c];
                uint32_t bv = b[(y * 128 + x) * 4 + c];
                sum += (double)(av > bv ? av - bv : bv - av);
            }
        }
    }
    return sum / (double)((x1 - x0) * (y1 - y0) * 4);
}

static uint8_t* ui_test_render_backdrop_(struct ui_node root) {
    return ui_test_render_read_(root, &ui_test_render_desc_, gpu_format_bgra8, make_uint2(128, 128));
}

static int32_t ui_test_backdrop_max_channel_gap_(const uint8_t* px, uint32_t hi, uint32_t lo, uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1) {
    int32_t max_gap = INT32_MIN;
    for (uint32_t y = y0; y < y1; y++) {
        for (uint32_t x = x0; x < x1; x++) {
            const uint8_t* p = px + (y * 128 + x) * 4;
            max_gap = max_int32(max_gap, (int32_t)p[hi] - (int32_t)p[lo]);
        }
    }
    return max_gap;
}

static int32_t ui_test_backdrop_max_channel_diff_(const uint8_t* a, const uint8_t* b, uint32_t channel, uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1) {
    int32_t max_diff = INT32_MIN;
    for (uint32_t y = y0; y < y1; y++) {
        for (uint32_t x = x0; x < x1; x++) {
            uint32_t at = (y * 128 + x) * 4 + channel;
            max_diff = max_int32(max_diff, (int32_t)a[at] - (int32_t)b[at]);
        }
    }
    return max_diff;
}

static uint8_t* ui_test_render_backdrop_area_(bool grouped) {
    struct ui_node root = ui_frame(&(struct ui_frame) {
        .size = ui_size_fix(128, 128),
        .fill = ui_fill_solid(ui_rgba(1.0f, 0.0f, 0.0f, 1.0f)),
    });

    struct ui_node panel = ui_frame(&(struct ui_frame) {
        .parent = root,
        .pos = ui_pos_xy(32, 32),
        .size = ui_size_fix(64, 64),
        .fill = ui_fill_solid(ui_rgba(0.5f, 0.5f, 0.5f, 1.0f)),
        .backdrop_root = grouped,
    });

    ui_frame(&(struct ui_frame) {
        .parent = panel,
        .pos = ui_pos_xy(0, 8),
        .size = ui_size_fix(32, 48),
        .fill.layers[0] = ui_layer_background_blur(4),
    });

    return ui_test_render_backdrop_(root);
}

static void ui_test_backdrop_root_area_check(void* args) {
    uint8_t** px = args;

    int32_t grouped_gap = ui_test_backdrop_max_channel_gap_(px[0], 2, 1, 34, 44, 46, 84);
    int32_t open_gap = ui_test_backdrop_max_channel_gap_(px[1], 2, 1, 34, 44, 46, 84);

    ui_test_check(grouped_gap <= 3, "backdrop root clamps blur to the container rect");
    ui_test_check(open_gap >= 16, "ungrouped blur pulls surrounding content past the panel");

    ui_test_render_finish(px, 2);
}

static void ui_test_backdrop_root_area(void) {
    static uint8_t* px[2];

    double saved_scale = ui_get_to_px_from_pt();
    ui_set_to_px_from_pt(1.0f);

    px[0] = ui_test_render_backdrop_area_(true);
    px[1] = ui_test_render_backdrop_area_(false);

    ui_set_to_px_from_pt(saved_scale);
    ui_test_then_(ui_test_backdrop_root_area_check, px);
}

static uint8_t* ui_test_render_backdrop_snapshot_(bool grouped, double plug_opacity) {
    struct ui_node root = ui_frame(&(struct ui_frame) {
        .size = ui_size_fix(128, 128),
        .fill = ui_fill_solid(ui_rgba(0.2f, 0.2f, 0.2f, 1.0f)),
    });

    struct ui_node container = ui_frame(&(struct ui_frame) {
        .parent = root,
        .pos = ui_pos_xy(0, 0),
        .size = ui_size_fix(128, 128),
        .backdrop_root = grouped,
    });

    ui_frame(&(struct ui_frame) {
        .parent = container,
        .pos = ui_pos_xy(8, 32),
        .size = ui_size_fix(48, 64),
        .fill.layers = { ui_layer_background_blur(8.0 / 3.0), ui_layer_solid(ui_rgba(0.0f, 1.0f, 0.0f, 1.0f)) },
    });

    ui_frame(&(struct ui_frame) {
        .parent = container,
        .pos = ui_pos_xy(60, 32),
        .size = ui_size_fix(56, 64),
        .fill.layers[0] = ui_layer_background_blur(20.0 / 3.0),
        .blend = plug_opacity < 1.0
            ? (struct ui_blend) { .mode = ui_blend_mode_normal, .opacity = plug_opacity }
            : (struct ui_blend) { 0 },
    });

    return ui_test_render_backdrop_(root);
}

static void ui_test_backdrop_root_snapshot_check(void* args) {
    uint8_t** px = args;

    int32_t grouped_gap = ui_test_backdrop_max_channel_gap_(px[0], 1, 2, 62, 40, 74, 88);
    int32_t open_gap = ui_test_backdrop_max_channel_gap_(px[1], 1, 2, 62, 40, 74, 88);

    ui_test_check(grouped_gap <= 3, "container members share the snapshot and never see each other");
    ui_test_check(open_gap >= 16, "ungrouped members blur each other's output");

    ui_test_render_finish(px, 2);
}

static void ui_test_backdrop_root_snapshot(void) {
    static uint8_t* px[2];

    double saved_scale = ui_get_to_px_from_pt();
    ui_set_to_px_from_pt(1.0f);

    px[0] = ui_test_render_backdrop_snapshot_(true, 1.0);
    px[1] = ui_test_render_backdrop_snapshot_(false, 1.0);

    ui_set_to_px_from_pt(saved_scale);
    ui_test_then_(ui_test_backdrop_root_snapshot_check, px);
}

static void ui_test_backdrop_root_snapshot_layered_check(void* args) {
    uint8_t** px = args;

    int32_t grouped_gap = ui_test_backdrop_max_channel_gap_(px[0], 1, 2, 62, 40, 74, 88);
    int32_t open_gap = ui_test_backdrop_max_channel_gap_(px[1], 1, 2, 62, 40, 74, 88);

    ui_test_check(grouped_gap <= 3, "snapshot holds for a member inside its own opacity layer");
    ui_test_check(open_gap >= 8, "ungrouped layered member blurs sibling output");

    ui_test_render_finish(px, 2);
}

static void ui_test_backdrop_root_snapshot_layered(void) {
    static uint8_t* px[2];

    double saved_scale = ui_get_to_px_from_pt();
    ui_set_to_px_from_pt(1.0f);

    px[0] = ui_test_render_backdrop_snapshot_(true, 0.5);
    px[1] = ui_test_render_backdrop_snapshot_(false, 0.5);

    ui_set_to_px_from_pt(saved_scale);
    ui_test_then_(ui_test_backdrop_root_snapshot_layered_check, px);
}

static uint8_t* ui_test_render_backdrop_nested_(bool inner_grouped) {
    struct ui_node root = ui_frame(&(struct ui_frame) {
        .size = ui_size_fix(128, 128),
        .fill = ui_fill_solid(ui_rgba(0.2f, 0.2f, 0.2f, 1.0f)),
    });

    struct ui_node outer = ui_frame(&(struct ui_frame) {
        .parent = root,
        .pos = ui_pos_xy(0, 0),
        .size = ui_size_fix(128, 128),
        .backdrop_root = true,
    });

    ui_frame(&(struct ui_frame) {
        .parent = outer,
        .pos = ui_pos_xy(0, 0),
        .size = ui_size_fix(24, 128),
        .fill = ui_fill_solid(ui_rgba(1.0f, 0.0f, 0.0f, 1.0f)),
    });

    ui_frame(&(struct ui_frame) {
        .parent = outer,
        .pos = ui_pos_xy(24, 24),
        .size = ui_size_fix(88, 88),
        .fill = ui_fill_solid(ui_rgba(0.0f, 1.0f, 0.0f, 1.0f)),
    });

    struct ui_node inner = ui_frame(&(struct ui_frame) {
        .parent = outer,
        .pos = ui_pos_xy(24, 24),
        .size = ui_size_fix(80, 80),
        .backdrop_root = inner_grouped,
    });

    ui_frame(&(struct ui_frame) {
        .parent = inner,
        .pos = ui_pos_xy(0, 8),
        .size = ui_size_fix(40, 48),
        .fill.layers[0] = ui_layer_background_blur(10.0 / 3.0),
    });

    return ui_test_render_backdrop_(root);
}

static void ui_test_backdrop_root_nested_check(void* args) {
    uint8_t** px = args;

    int32_t grouped_green = ui_test_backdrop_max_channel_gap_(px[0], 1, 2, 26, 36, 40, 76);
    int32_t open_red = ui_test_backdrop_max_channel_diff_(px[1], px[0], 2, 26, 36, 34, 76);

    ui_test_check(grouped_green >= 40, "inner container blur sees content drawn inside the outer container");
    ui_test_check(open_red >= 16, "outer-only member samples past the inner rect");

    ui_test_render_finish(px, 2);
}

static void ui_test_backdrop_root_nested(void) {
    static uint8_t* px[2];

    double saved_scale = ui_get_to_px_from_pt();
    ui_set_to_px_from_pt(1.0f);

    px[0] = ui_test_render_backdrop_nested_(true);
    px[1] = ui_test_render_backdrop_nested_(false);

    ui_set_to_px_from_pt(saved_scale);
    ui_test_then_(ui_test_backdrop_root_nested_check, px);
}

static void ui_test_glass_tint_modes_check(void* args) {
    uint8_t** px = args;

    ui_test_check(ui_test_glass_max_diff(px[0], px[1], 24, 24, 104, 104) > 32, "glass tint solid colors distinct");
    ui_test_check(ui_test_glass_max_diff(px[0], px[2], 0, 0, 128, 128) <= 1, "glass tint render deterministic");

    ui_test_render_finish(px, 3);
}

static void ui_test_glass_tint_modes(void) {
    static uint8_t* px[3];

    double saved_scale = ui_get_to_px_from_pt();
    ui_set_to_px_from_pt(1.0f);

    struct ui_color c0 = ui_rgba(230.0f / 255.0f, 64.0f / 255.0f, 26.0f / 255.0f, 1.0f);
    struct ui_color c1 = ui_rgba(26.0f / 255.0f, 102.0f / 255.0f, 230.0f / 255.0f, 1.0f);

    px[0] = ui_test_render_glass(c0);
    px[1] = ui_test_render_glass(c1);
    px[2] = ui_test_render_glass(c0);

    ui_set_to_px_from_pt(saved_scale);
    ui_test_then_(ui_test_glass_tint_modes_check, px);
}

static void ui_test_glass_color_spaces_check(void* args) {
    uint8_t** px = args;
    char label[160];

    {
        uint32_t diff = ui_test_glass_max_diff(px[0], px[1], 18, 18, 110, 110);
        snprintf(label, sizeof(label), "glass cs: p3 tint matches srgb twin (max %u)", diff);
        ui_test_check(diff <= 2, label);
    }

    {
        uint32_t diff = ui_test_glass_max_diff(px[0], px[2], 18, 18, 110, 110);
        snprintf(label, sizeof(label), "glass cs: linear tint matches encoded twin (max %u)", diff);
        ui_test_check(diff <= 2, label);
    }

    {
        const char* tint_names[2] = { "tinted", "clear" };
        struct ui_convert conv = ui_convert_make(ui_color_space_srgb, ui_color_space_display_p3);

        for (uint32_t v = 0; v < 2; v++) {
            const uint8_t* out_srgb = px[3 + v * 2];
            const uint8_t* out_p3 = px[4 + v * 2];

            uint32_t max_diff = 0;
            for (uint32_t y = 4; y < 124; y++) {
                for (uint32_t x = 4; x < 124; x++) {
                    const uint8_t* s = out_srgb + (y * 128 + x) * 4;
                    const uint8_t* p = out_p3 + (y * 128 + x) * 4;
                    if (s[0] == 255 || s[1] == 255 || s[2] == 255) {
                        continue;
                    }
                    float3 expected = ui_convert_apply_float3(&conv, make_float3((float)s[2] / 255.0f, (float)s[1] / 255.0f, (float)s[0] / 255.0f));
                    uint8_t eb[3] = {
                        (uint8_t)lroundf(max_float(0.0f, min_float(1.0f, expected.z)) * 255.0f),
                        (uint8_t)lroundf(max_float(0.0f, min_float(1.0f, expected.y)) * 255.0f),
                        (uint8_t)lroundf(max_float(0.0f, min_float(1.0f, expected.x)) * 255.0f),
                    };
                    for (uint32_t c = 0; c < 3; c++) {
                        uint32_t av = p[c];
                        uint32_t bv = eb[c];
                        max_diff = max_uint32(max_diff, av > bv ? av - bv : bv - av);
                    }
                }
            }

            snprintf(label, sizeof(label), "glass cs output: %s glass srgb converts to p3 output (max %u)", tint_names[v], max_diff);
            ui_test_check(max_diff <= 3, label);
        }
    }

    ui_test_render_finish(px, 7);
}

static void ui_test_glass_color_spaces(void) {
    static uint8_t* px[7];

    double saved_scale = ui_get_to_px_from_pt();
    ui_set_to_px_from_pt(1.0f);

    struct ui_color p3 = ui_rgba_cs(0.85f, 0.3f, 0.2f, 1.0f, ui_color_space_display_p3);

    struct ui_convert to_srgb = ui_convert_make(ui_color_space_display_p3, ui_color_space_srgb);
    struct ui_color srgb_twin = p3;
    srgb_twin.value.xyz = ui_convert_apply_float3(&to_srgb, p3.value.xyz);
    srgb_twin.color_space = ui_color_space_srgb;

    struct ui_convert to_linear = ui_convert_make(ui_color_space_srgb, ui_color_space_srgb_linear);
    struct ui_color linear_twin = srgb_twin;
    linear_twin.value.xyz = ui_convert_apply_float3(&to_linear, srgb_twin.value.xyz);
    linear_twin.color_space = ui_color_space_srgb_linear;

    px[0] = ui_test_render_glass(p3);
    px[1] = ui_test_render_glass(srgb_twin);
    px[2] = ui_test_render_glass(linear_twin);

    struct ui_color tints[2] = { srgb_twin, { 0 } };
    for (uint32_t v = 0; v < 2; v++) {
        ui_test_render_desc_.output_color_space = ui_color_space_srgb;
        px[3 + v * 2] = ui_test_render_glass(tints[v]);
        ui_test_render_desc_.output_color_space = ui_color_space_display_p3;
        px[4 + v * 2] = ui_test_render_glass(tints[v]);
    }

    ui_test_render_desc_ = (struct ui_render_desc) { .compositing_format = ui_pixel_format_rgba16f };
    ui_set_to_px_from_pt(saved_scale);
    ui_test_then_(ui_test_glass_color_spaces_check, px);
}

static uint8_t* ui_test_render_fill(struct ui_fill fill, struct ui_color bg, double radius) {
    struct ui_node root = ui_frame(&(struct ui_frame) {
        .size = ui_size_fix(128, 128),
        .fill = ui_fill_solid(bg),
    });

    ui_frame(&(struct ui_frame) {
        .parent = root,
        .pos = ui_pos_xy(16, 16),
        .size = ui_size_fix(96, 96),
        .corners = ui_corners(radius),
        .fill = fill,
    });

    return ui_test_render_backdrop_(root);
}

static float ui_test_gradient_w(enum ui_gradient_direction direction, uint32_t x, uint32_t y) {
    float fx = (float)x + 0.5f;
    float fy = (float)y + 0.5f;
    float w = 0.0f;
    switch (direction) {
    case ui_gradient_direction_right: w = (fx - 16.0f) / 96.0f; break;
    case ui_gradient_direction_left: w = (112.0f - fx) / 96.0f; break;
    case ui_gradient_direction_down: w = (fy - 16.0f) / 96.0f; break;
    case ui_gradient_direction_up: w = (112.0f - fy) / 96.0f; break;
    }
    return max_float(0.0f, min_float(1.0f, w));
}

static uint32_t ui_test_gradient_check(const uint8_t* img, enum ui_gradient_direction direction, float4 p0, float4 p1, float4 bg,
                                          uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1) {
    uint32_t max_diff = 0;
    for (uint32_t y = y0; y < y1; y++) {
        for (uint32_t x = x0; x < x1; x++) {
            float w = ui_test_gradient_w(direction, x, y);
            float4 src = p0 + (p1 - p0) * w;
            float4 v = src + bg * (1.0f - src.w);
            uint8_t expected[4] = {
                (uint8_t)lroundf(v.z * 255.0f),
                (uint8_t)lroundf(v.y * 255.0f),
                (uint8_t)lroundf(v.x * 255.0f),
                (uint8_t)lroundf(v.w * 255.0f),
            };
            for (uint32_t c = 0; c < 4; c++) {
                uint32_t av = img[(y * 128 + x) * 4 + c];
                uint32_t bv = expected[c];
                max_diff = max_uint32(max_diff, av > bv ? av - bv : bv - av);
            }
        }
    }
    return max_diff;
}

#define ui_test_gradient_c0 ui_rgba(230.0f / 255.0f, 64.0f / 255.0f, 26.0f / 255.0f, 1.0f)
#define ui_test_gradient_c1 ui_rgba(26.0f / 255.0f, 102.0f / 255.0f, 230.0f / 255.0f, 1.0f)
#define ui_test_gradient_a0 ui_rgba(0.9f, 0.25f, 0.1f, 0.6f)
#define ui_test_gradient_a1 ui_rgba(0.1f, 0.4f, 0.9f, 0.2f)
#define ui_test_gradient_bg ui_rgba(128.0f / 255.0f, 128.0f / 255.0f, 128.0f / 255.0f, 1.0f)

static const enum ui_gradient_direction ui_test_gradient_directions_[4] = {
    ui_gradient_direction_right,
    ui_gradient_direction_left,
    ui_gradient_direction_down,
    ui_gradient_direction_up,
};

static void ui_test_gradient_fill_check(void* args) {
    uint8_t** px = args;

    float4 p0 = premultiply_float4(ui_test_gradient_c0.value);
    float4 p1 = premultiply_float4(ui_test_gradient_c1.value);
    float4 zero = make_float4(0.0f, 0.0f, 0.0f, 0.0f);

    const char* labels[4] = {
        "gradient fill right matches expected",
        "gradient fill left matches expected",
        "gradient fill down matches expected",
        "gradient fill up matches expected",
    };

    for (uint32_t i = 0; i < 4; i++) {
        uint32_t diff = ui_test_gradient_check(px[i], ui_test_gradient_directions_[i], p0, p1, zero, 18, 18, 110, 110);
        char label[128];
        snprintf(label, sizeof(label), "%s (max %u)", labels[i], diff);
        ui_test_check(diff <= 2, label);
    }

    {
        const uint8_t* grad = px[4];
        const uint8_t* solid = px[5];
        uint32_t max_diff = 0;
        for (uint32_t i = 0; i < 128 * 128 * 4; i++) {
            uint32_t av = grad[i];
            uint32_t bv = solid[i];
            max_diff = max_uint32(max_diff, av > bv ? av - bv : bv - av);
        }
        char label[128];
        snprintf(label, sizeof(label), "gradient fill equal endpoints matches solid (max %u)", max_diff);
        ui_test_check(max_diff <= 1, label);
    }

    {
        uint32_t diff = ui_test_gradient_check(px[6], ui_gradient_direction_right,
                                                  premultiply_float4(ui_test_gradient_a0.value), premultiply_float4(ui_test_gradient_a1.value), premultiply_float4(ui_test_gradient_bg.value),
                                                  18, 18, 110, 110);
        char label[128];
        snprintf(label, sizeof(label), "gradient fill alpha composites over background (max %u)", diff);
        ui_test_check(diff <= 2, label);
    }

    {
        uint32_t diff = ui_test_gradient_check(px[7], ui_gradient_direction_down, p0, p1, zero, 30, 30, 98, 98);
        char label[128];
        snprintf(label, sizeof(label), "gradient fill rounded corners interior matches expected (max %u)", diff);
        ui_test_check(diff <= 2, label);
    }

    ui_test_render_finish(px, 8);
}

static void ui_test_gradient_fill(void) {
    static uint8_t* px[8];

    double saved_scale = ui_get_to_px_from_pt();
    ui_set_to_px_from_pt(1.0f);

    for (uint32_t i = 0; i < 4; i++) {
        px[i] = ui_test_render_fill(ui_fill_gradient(ui_test_gradient_directions_[i], ui_test_gradient_c0, ui_test_gradient_c1), ui_black, 0.0);
    }
    px[4] = ui_test_render_fill(ui_fill_gradient(ui_gradient_direction_right, ui_test_gradient_c0, ui_test_gradient_c0), ui_black, 0.0);
    px[5] = ui_test_render_fill(ui_fill_solid(ui_test_gradient_c0), ui_black, 0.0);
    px[6] = ui_test_render_fill(ui_fill_gradient(ui_gradient_direction_right, ui_test_gradient_a0, ui_test_gradient_a1), ui_test_gradient_bg, 0.0);
    px[7] = ui_test_render_fill(ui_fill_gradient(ui_gradient_direction_down, ui_test_gradient_c0, ui_test_gradient_c1), ui_black, 10.0);

    ui_set_to_px_from_pt(saved_scale);
    ui_test_then_(ui_test_gradient_fill_check, px);
}

static double ui_test_block_mean_diff_max(const uint8_t* a, const uint8_t* b, uint32_t x0, uint32_t y0, uint32_t block, uint32_t nx, uint32_t ny) {
    double worst = 0.0;
    for (uint32_t by = 0; by < ny; by++) {
        for (uint32_t bx = 0; bx < nx; bx++) {
            for (uint32_t c = 0; c < 4; c++) {
                double sum_a = 0.0;
                double sum_b = 0.0;
                for (uint32_t y = 0; y < block; y++) {
                    for (uint32_t x = 0; x < block; x++) {
                        uint32_t px = x0 + bx * block + x;
                        uint32_t py = y0 + by * block + y;
                        sum_a += (double)a[(py * 128 + px) * 4 + c];
                        sum_b += (double)b[(py * 128 + px) * 4 + c];
                    }
                }
                worst = max_double(worst, fabs(sum_a - sum_b) / (double)(block * block));
            }
        }
    }
    return worst;
}

static uint8_t* ui_test_render_shadowed_gradient(void) {
    struct ui_node root = ui_frame(&(struct ui_frame) {
        .size = ui_size_fix(128, 128),
        .fill = ui_fill_solid(ui_rgba(0.82f, 0.84f, 0.88f, 1.0f)),
    });

    ui_frame(&(struct ui_frame) {
        .parent = root,
        .pos = ui_pos_xy(24, 24),
        .size = ui_size_fix(80, 80),
        .corners = ui_corners(12),
        .fill = ui_fill_gradient(ui_gradient_direction_down, ui_rgba(0.35f, 0.4f, 0.55f, 1.0f), ui_rgba(0.3f, 0.34f, 0.47f, 1.0f)),
        .shadow = ui_shadow(0.0f, 6.0f, 4.0f, ui_rgba(0.0f, 0.0f, 0.0f, 0.5f)),
    });

    return ui_test_render_backdrop_(root);
}

static void ui_test_layer_blur_shadow_check(void* args) {
    uint8_t** px = args;
    const uint8_t* raw = px[0];

    uint8_t corner = raw[(8 * 128 + 8) * 4 + 1];
    uint8_t center = raw[(56 * 128 + 64) * 4 + 1];
    uint8_t below = raw[(104 * 128 + 64) * 4 + 1];

    ui_test_check(corner >= 250, "blur+shadow corner stays background");
    ui_test_check(center <= 80, "blur+shadow content stays dark");
    ui_test_check(below <= corner - 40, "blur+shadow casts shadow below content");

    ui_test_render_finish(px, 1);
}

static void ui_test_layer_blur_shadow(void) {
    static uint8_t* px[1];

    double saved_scale = ui_get_to_px_from_pt();
    ui_set_to_px_from_pt(1.0f);

    struct ui_node root = ui_frame(&(struct ui_frame) {
        .size = ui_size_fix(128, 128),
        .fill = ui_fill_solid(ui_rgba(1.0f, 1.0f, 1.0f, 1.0f)),
    });

    ui_frame(&(struct ui_frame) {
        .parent = root,
        .pos = ui_pos_xy(32, 24),
        .size = ui_size_fix(64, 64),
        .fill = ui_fill_solid(ui_rgba(0.1f, 0.1f, 0.1f, 1.0f)),
        .shadow = ui_shadow(0.0f, 24.0f, 8.0f / 3.0f, ui_rgba(0.0f, 0.0f, 0.0f, 1.0f)),
        .blur = ui_blur(4.0f / 3.0f),
    });

    px[0] = ui_test_render_backdrop_(root);

    ui_set_to_px_from_pt(saved_scale);
    ui_test_then_(ui_test_layer_blur_shadow_check, px);
}

static void ui_test_compositing_format_check(void* args) {
    uint8_t** px = args;
    const uint8_t* grad16 = px[0];
    const uint8_t* glass16 = px[1];
    const uint8_t* shadow16 = px[2];
    const uint8_t* grad8 = px[3];
    const uint8_t* glass8 = px[4];
    const uint8_t* shadow8 = px[5];

    {
        uint32_t diff = ui_test_glass_max_diff(grad16, grad8, 18, 18, 110, 110);
        char label[128];
        snprintf(label, sizeof(label), "bgra8 compositing gradient within one code of rgba16f (max %u)", diff);
        ui_test_check(diff <= 1, label);
    }

    {
        double diff = ui_test_block_mean_diff_max(grad16, grad8, 24, 24, 8, 10, 10);
        char label[128];
        snprintf(label, sizeof(label), "bgra8 compositing dither preserves gradient means (max %.3f)", diff);
        ui_test_check(diff <= 0.5, label);
    }

    {
        uint32_t max_diff = ui_test_glass_max_diff(glass16, glass8, 0, 0, 128, 128);
        double mean = ui_test_glass_mean_diff(glass16, glass8, 0, 0, 128, 128);
        char label[160];
        snprintf(label, sizeof(label), "bgra8 compositing glass matches rgba16f (max %u mean %.3f)", max_diff, mean);
        ui_test_check(max_diff <= 24 && mean <= 1.0, label);
    }

    {
        uint32_t max_diff = ui_test_glass_max_diff(shadow16, shadow8, 0, 0, 128, 128);
        double mean = ui_test_glass_mean_diff(shadow16, shadow8, 0, 0, 128, 128);
        char label[160];
        snprintf(label, sizeof(label), "bgra8 compositing layer shadow matches rgba16f (max %u mean %.3f)", max_diff, mean);
        ui_test_check(max_diff <= 6 && mean <= 0.75, label);
    }

    ui_test_render_finish(px, 6);
}

static void ui_test_compositing_format(void) {
    static uint8_t* px[6];

    double saved_scale = ui_get_to_px_from_pt();
    ui_set_to_px_from_pt(1.0f);

    struct ui_color g0 = ui_rgba(56.0f / 255.0f, 61.0f / 255.0f, 77.0f / 255.0f, 1.0f);
    struct ui_color g1 = ui_rgba(66.0f / 255.0f, 71.0f / 255.0f, 87.0f / 255.0f, 1.0f);
    struct ui_fill gradient = ui_fill_gradient(ui_gradient_direction_right, g0, g1);
    struct ui_color glass_tint = ui_rgba(0.9f, 0.4f, 0.2f, 0.35f);

    ui_test_render_desc_ = (struct ui_render_desc) { .compositing_format = ui_pixel_format_rgba16f };
    px[0] = ui_test_render_fill(gradient, ui_black, 0.0);
    px[1] = ui_test_render_glass(glass_tint);
    px[2] = ui_test_render_shadowed_gradient();

    ui_test_render_desc_.compositing_format = ui_pixel_format_bgra8;
    ui_test_render_desc_.dither = true;
    px[3] = ui_test_render_fill(gradient, ui_black, 0.0);
    px[4] = ui_test_render_glass(glass_tint);
    px[5] = ui_test_render_shadowed_gradient();

    ui_test_render_desc_ = (struct ui_render_desc) { .compositing_format = ui_pixel_format_rgba16f };
    ui_set_to_px_from_pt(saved_scale);
    ui_test_then_(ui_test_compositing_format_check, px);
}

static void ui_test_direct_to_target_check(void* args) {
    uint8_t** px = args;
    const uint8_t* grad_direct = px[0];
    const uint8_t* glass_direct = px[1];
    const uint8_t* shadow_direct = px[2];
    const uint8_t* grad_final = px[3];
    const uint8_t* glass_final = px[4];
    const uint8_t* shadow_final = px[5];

    {
        uint32_t diff = ui_test_glass_max_diff(grad_direct, grad_final, 0, 0, 128, 128);
        char label[128];
        snprintf(label, sizeof(label), "direct gradient matches final pass output (max %u)", diff);
        ui_test_check(diff <= 1, label);
    }

    {
        uint32_t diff = ui_test_glass_max_diff(glass_direct, glass_final, 0, 0, 128, 128);
        char label[128];
        snprintf(label, sizeof(label), "direct glass matches final pass output (max %u)", diff);
        ui_test_check(diff <= 2, label);
    }

    {
        uint32_t diff = ui_test_glass_max_diff(shadow_direct, shadow_final, 0, 0, 128, 128);
        char label[128];
        snprintf(label, sizeof(label), "direct layer shadow matches final pass output (max %u)", diff);
        ui_test_check(diff <= 2, label);
    }

    ui_test_render_finish(px, 7);
}

static void ui_test_direct_to_target(void) {
    static uint8_t* px[7];

    double saved_scale = ui_get_to_px_from_pt();
    ui_set_to_px_from_pt(1.0f);

    struct ui_color g0 = ui_rgba(56.0f / 255.0f, 61.0f / 255.0f, 77.0f / 255.0f, 1.0f);
    struct ui_color g1 = ui_rgba(66.0f / 255.0f, 71.0f / 255.0f, 87.0f / 255.0f, 1.0f);
    struct ui_fill gradient = ui_fill_gradient(ui_gradient_direction_right, g0, g1);
    struct ui_color glass_tint = ui_rgba(0.9f, 0.4f, 0.2f, 0.35f);

    ui_test_render_desc_ = (struct ui_render_desc) {
        .compositing_format = ui_pixel_format_bgra8,
        .premultiplied = true,
    };
    px[0] = ui_test_render_fill(gradient, ui_black, 0.0);
    ui_test_check(ui_render_last_direct_(), "direct to target engages for matching bgra8 formats");
    px[1] = ui_test_render_glass(glass_tint);
    px[2] = ui_test_render_shadowed_gradient();

    ui_test_render_desc_.premultiplied = false;
    px[3] = ui_test_render_fill(gradient, ui_black, 0.0);
    ui_test_check(ui_render_last_direct_() == false, "final pass runs when output is not premultiplied");
    px[4] = ui_test_render_glass(glass_tint);
    px[5] = ui_test_render_shadowed_gradient();

    ui_test_render_desc_ = (struct ui_render_desc) { .compositing_format = ui_pixel_format_rgba16f, .premultiplied = true };
    px[6] = ui_test_render_fill(gradient, ui_black, 0.0);
    ui_test_check(ui_render_last_direct_() == false, "final pass runs when compositing and target formats differ");

    ui_test_render_desc_ = (struct ui_render_desc) { .compositing_format = ui_pixel_format_rgba16f };
    ui_set_to_px_from_pt(saved_scale);
    ui_test_then_(ui_test_direct_to_target_check, px);
}

static uint8_t* ui_test_render_clip_(double radius, bool outlined) {
    struct ui_node root = ui_frame(&(struct ui_frame) {
        .size = ui_size_fix(128, 128),
        .fill = ui_fill_solid(ui_rgba(1.0f, 0.0f, 0.0f, 1.0f)),
    });

    struct ui_node panel = ui_frame(&(struct ui_frame) {
        .parent = root,
        .pos = ui_pos_xy(32, 32),
        .size = ui_size_fix(64, 64),
        .corners = ui_corners(radius),
        .outline = outlined ? ui_outline_solid(ui_outline_align_inside, 2.0f, ui_rgba(0.0f, 0.0f, 1.0f, 1.0f)) : (struct ui_outline) { 0 },
        .clip = true,
    });

    ui_frame(&(struct ui_frame) {
        .parent = panel,
        .pos = ui_pos_xy(-32, -32),
        .size = ui_size_fix(128, 128),
        .fill = ui_fill_solid(ui_rgba(0.0f, 1.0f, 0.0f, 1.0f)),
    });

    return ui_test_render_backdrop_(root);
}

static void ui_test_clip_check_(const uint8_t* px, uint32_t x, uint32_t y, bool green, const char* label) {
    const uint8_t* p = px + (y * 128 + x) * 4;
    bool ok = green ? (p[1] > 200 && p[2] < 50) : (p[2] > 200 && p[1] < 50);
    char full[128];
    snprintf(full, sizeof(full), "%s (bgra %u %u %u %u at %u,%u)", label, p[0], p[1], p[2], p[3], x, y);
    ui_test_check(ok, full);
}

static void ui_test_clip_shape_check(void* args) {
    uint8_t** px = args;
    const uint8_t* rect = px[0];
    const uint8_t* rounded = px[1];
    const uint8_t* outlined = px[2];

    ui_test_clip_check_(rect, 34, 34, true, "rect clip keeps children up to the corner");
    ui_test_clip_check_(rect, 64, 64, true, "rect clip keeps children inside");
    ui_test_clip_check_(rect, 20, 64, false, "rect clip removes children outside");

    ui_test_clip_check_(rounded, 34, 34, false, "rounded clip removes children outside the corner");
    ui_test_clip_check_(rounded, 64, 64, true, "rounded clip keeps children inside the shape");
    ui_test_clip_check_(rounded, 20, 64, false, "rounded clip removes children outside the rect");

    ui_test_clip_check_(outlined, 34, 34, false, "sdf clip removes children outside the corner");
    ui_test_clip_check_(outlined, 64, 64, true, "sdf clip keeps children inside the shape");

    ui_test_render_finish(px, 3);
}

static void ui_test_clip_shape(void) {
    static uint8_t* px[3];

    double saved_scale = ui_get_to_px_from_pt();
    ui_set_to_px_from_pt(1.0f);

    px[0] = ui_test_render_clip_(0.0, false);
    px[1] = ui_test_render_clip_(24.0, false);
    px[2] = ui_test_render_clip_(24.0, true);

    ui_set_to_px_from_pt(saved_scale);
    ui_test_then_(ui_test_clip_shape_check, px);
}

static uint32_t ui_test_image_max_diff_(const uint8_t* a, const uint8_t* b) {
    uint32_t max_diff = 0;
    for (uint32_t i = 0; i < 128 * 128 * 4; i++) {
        uint32_t av = a[i];
        uint32_t bv = b[i];
        max_diff = max_uint32(max_diff, av > bv ? av - bv : bv - av);
    }
    return max_diff;
}

static uint8_t* ui_test_render_transform_rects_(bool transformed) {
    struct ui_node root = ui_frame(&(struct ui_frame) {
        .size = ui_size_fix(128, 128),
        .fill = ui_fill_solid(ui_rgba(0.1f, 0.1f, 0.12f, 1.0f)),
    });

    float3x2 scale_translate = make_float3x2(make_float2(2.0f, 0.0f), make_float2(0.0f, 2.0f), make_float2(8.0f, 4.0f));
    float3x2 rotate90 = make_float3x2(make_float2(0.0f, 1.0f), make_float2(-1.0f, 0.0f), make_float2(0.0f, 0.0f));

    if (transformed) {
        ui_rect(&(struct ui_rect) {
            .parent = root,
            .pos = ui_pos_xy(16, 16),
            .size = ui_size_fix(32, 32),
            .transform = scale_translate,
            .fill = ui_fill_solid(ui_rgba(0.2f, 0.55f, 0.95f, 1.0f)),
        });
        ui_rect(&(struct ui_rect) {
            .parent = root,
            .pos = ui_pos_xy(120, 96),
            .size = ui_size_fix(24, 48),
            .transform = rotate90,
            .fill = ui_fill_solid(ui_rgba(0.95f, 0.4f, 0.2f, 1.0f)),
        });
    } else {
        ui_rect(&(struct ui_rect) {
            .parent = root,
            .pos = ui_pos_xy(24, 20),
            .size = ui_size_fix(64, 64),
            .fill = ui_fill_solid(ui_rgba(0.2f, 0.55f, 0.95f, 1.0f)),
        });
        ui_rect(&(struct ui_rect) {
            .parent = root,
            .pos = ui_pos_xy(72, 96),
            .size = ui_size_fix(48, 24),
            .fill = ui_fill_solid(ui_rgba(0.95f, 0.4f, 0.2f, 1.0f)),
        });
    }

    return ui_test_render_backdrop_(root);
}

static uint8_t* ui_test_render_transform_rounded_(bool transformed) {
    struct ui_node root = ui_frame(&(struct ui_frame) {
        .size = ui_size_fix(128, 128),
        .fill = ui_fill_solid(ui_rgba(0.1f, 0.1f, 0.12f, 1.0f)),
    });

    float3x2 rotate90 = make_float3x2(make_float2(0.0f, 1.0f), make_float2(-1.0f, 0.0f), make_float2(0.0f, 0.0f));

    struct ui_corners corners = { .radius = make_double4(12.0, 12.0, 12.0, 12.0), .profile = ui_corner_profile_circular };

    if (transformed) {
        ui_rect(&(struct ui_rect) {
            .parent = root,
            .pos = ui_pos_xy(88, 40),
            .size = ui_size_fix(48, 48),
            .transform = rotate90,
            .corners = corners,
            .fill = ui_fill_solid(ui_rgba(0.4f, 0.85f, 0.5f, 1.0f)),
        });
    } else {
        ui_rect(&(struct ui_rect) {
            .parent = root,
            .pos = ui_pos_xy(40, 40),
            .size = ui_size_fix(48, 48),
            .corners = corners,
            .fill = ui_fill_solid(ui_rgba(0.4f, 0.85f, 0.5f, 1.0f)),
        });
    }

    return ui_test_render_backdrop_(root);
}

static uint8_t* ui_test_render_transform_text_(bool transformed) {
    struct ui_node root = ui_frame(&(struct ui_frame) {
        .size = ui_size_fix(128, 128),
        .fill = ui_fill_solid(ui_rgba(0.1f, 0.1f, 0.12f, 1.0f)),
    });

    float3x2 translate = make_float3x2(make_float2(1.0f, 0.0f), make_float2(0.0f, 1.0f), make_float2(24.0f, 40.0f));

    ui_text(&(struct ui_text) {
        .parent = root,
        .pos = transformed ? ui_pos_xy(16, 16) : ui_pos_xy(40, 56),
        .transform = transformed ? translate : (float3x2) { 0 },
        .str = ui_str("Transform", ui_white, 13),
    });

    return ui_test_render_backdrop_(root);
}

static uint8_t* ui_test_render_transform_clip_(void) {
    struct ui_node root = ui_frame(&(struct ui_frame) {
        .size = ui_size_fix(128, 128),
        .fill = ui_fill_solid(ui_rgba(1.0f, 0.0f, 0.0f, 1.0f)),
    });

    float3x2 scale_translate = make_float3x2(make_float2(2.0f, 0.0f), make_float2(0.0f, 2.0f), make_float2(16.0f, 16.0f));

    struct ui_node panel = ui_frame(&(struct ui_frame) {
        .parent = root,
        .pos = ui_pos_xy(16, 16),
        .size = ui_size_fix(32, 32),
        .transform = scale_translate,
        .corners = ui_corners(12),
        .clip = true,
    });

    ui_frame(&(struct ui_frame) {
        .parent = panel,
        .pos = ui_pos_xy(-32, -32),
        .size = ui_size_fix(128, 128),
        .fill = ui_fill_solid(ui_rgba(0.0f, 1.0f, 0.0f, 1.0f)),
    });

    return ui_test_render_backdrop_(root);
}

static bool ui_test_transform_hit_target_(struct ui_node node, void* user) {
    return node.raw == ((struct ui_node*)user)->raw;
}

static void ui_test_transform_check(void* args) {
    uint8_t** px = args;

    for (uint32_t s = 0; s < 2; s++) {
        uint32_t diff = ui_test_image_max_diff_(px[s * 2], px[s * 2 + 1]);
        char label[128];
        snprintf(label, sizeof(label), "transform scale/translate/rotate90 rects match plain equivalents at %ux (max %u)", s + 1, diff);
        ui_test_check(diff <= 1, label);
    }

    {
        uint32_t diff = ui_test_image_max_diff_(px[4], px[5]);
        char label[128];
        snprintf(label, sizeof(label), "transform rotate90 rounded rect matches plain equivalent (max %u)", diff);
        ui_test_check(diff <= 2, label);
    }

    {
        uint32_t diff = ui_test_image_max_diff_(px[6], px[7]);
        char label[128];
        snprintf(label, sizeof(label), "transform translated text matches plain equivalent (max %u)", diff);
        ui_test_check(diff <= 1, label);
    }

    ui_test_render_finish(px, 8);
}

static void ui_test_transform(void) {
    static uint8_t* px[8];

    double saved_scale = ui_get_to_px_from_pt();

    for (uint32_t s = 0; s < 2; s++) {
        ui_set_to_px_from_pt(s == 0 ? 1.0f : 2.0f);
        px[s * 2] = ui_test_render_transform_rects_(true);
        px[s * 2 + 1] = ui_test_render_transform_rects_(false);
    }
    ui_set_to_px_from_pt(1.0f);

    px[4] = ui_test_render_transform_rounded_(true);
    px[5] = ui_test_render_transform_rounded_(false);
    px[6] = ui_test_render_transform_text_(true);
    px[7] = ui_test_render_transform_text_(false);

    {
        struct ui_node root = ui_frame(&(struct ui_frame) {
            .size = ui_size_fix(128, 128),
        });
        float3x2 scale_translate = make_float3x2(make_float2(2.0f, 0.0f), make_float2(0.0f, 2.0f), make_float2(16.0f, 16.0f));
        struct ui_node target = ui_rect(&(struct ui_rect) {
            .parent = root,
            .pos = ui_pos_xy(16, 16),
            .size = ui_size_fix(32, 32),
            .transform = scale_translate,
            .fill = ui_fill_solid(ui_white),
        });
        ui_layout(root);

        struct ui_hit chain[8];
        uint32_t inside = ui_hit_test(root, make_double2(90.0, 90.0), ui_test_transform_hit_target_, &target, chain, cr_count(chain));
        uint32_t outside = ui_hit_test(root, make_double2(20.0, 20.0), ui_test_transform_hit_target_, &target, chain, cr_count(chain));
        ui_test_check(inside > 0, "hit test lands inside the transformed rect");
        ui_test_check(outside == 0, "hit test misses the untransformed position");

        ui_release(root);
    }

    ui_set_to_px_from_pt(saved_scale);
    ui_test_then_(ui_test_transform_check, px);
}

static void ui_test_transform_clip_check(void* args) {
    uint8_t** px = args;

    ui_test_clip_check_(px[0], 36, 36, false, "transformed rounded clip removes children outside the corner");
    ui_test_clip_check_(px[0], 64, 64, true, "transformed rounded clip keeps children inside");
    ui_test_clip_check_(px[0], 20, 64, false, "transformed rounded clip removes children outside the rect");

    ui_test_render_finish(px, 1);
}

static void ui_test_transform_clip(void) {
    static uint8_t* px[1];

    px[0] = ui_test_render_transform_clip_();
    ui_test_then_(ui_test_transform_clip_check, px);
}

static uint8_t* ui_test_render_clipped_text_(const char* raw, double2 clip_size, float3x2 transform) {
    struct ui_node root = ui_frame(&(struct ui_frame) {
        .size = ui_size_fix(128, 128),
        .fill = ui_fill_solid(ui_rgba(0.1f, 0.1f, 0.12f, 1.0f)),
    });

    struct ui_node clip = ui_frame(&(struct ui_frame) {
        .parent = root,
        .pos = ui_pos_xy(8, 8),
        .size = ui_size_fix(clip_size.x, clip_size.y),
        .clip = true,
    });

    ui_text(&(struct ui_text) {
        .parent = clip,
        .pos = ui_pos_xy(0, 0),
        .transform = transform,
        .str = ui_str(raw, ui_white, 13.75),
    });

    return ui_test_render_backdrop_(root);
}

static uint32_t ui_test_band_max_diff_(const uint8_t* a, const uint8_t* b, uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1) {
    uint32_t max_diff = 0;
    for (uint32_t y = y0; y < y1; y++) {
        for (uint32_t x = x0; x < x1; x++) {
            for (uint32_t c = 0; c < 4; c++) {
                uint32_t av = a[(y * 128 + x) * 4 + c];
                uint32_t bv = b[(y * 128 + x) * 4 + c];
                max_diff = max_uint32(max_diff, av > bv ? av - bv : bv - av);
            }
        }
    }
    return max_diff;
}

static bool ui_test_outside_band_is_background_(const uint8_t* px, uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1) {
    for (uint32_t y = 0; y < 128; y++) {
        for (uint32_t x = 0; x < 128; x++) {
            if (x >= x0 && x < x1 && y >= y0 && y < y1) {
                continue;
            }
            if (memcmp(px + (y * 128 + x) * 4, px, 4) != 0) {
                return false;
            }
        }
    }
    return true;
}

static void ui_test_text_cull_check(void* args) {
    uint8_t** px = args;

    ui_test_check(ui_test_band_max_diff_(px[0], px[1], 8, 8, 120, 32) == 0, "clipped text lines match the unclipped render inside the clip");
    ui_test_check(ui_test_outside_band_is_background_(px[1], 8, 8, 120, 32), "clipped text draws nothing outside the clip");
    ui_test_check(ui_test_band_max_diff_(px[2], px[3], 8, 8, 32, 120) == 0, "rotated clipped text lines match the unclipped render inside the clip");

    ui_test_render_finish(px, 4);
}

static void ui_test_text_cull(void) {
    static uint8_t* px[4];
    const char* lines = "aaa\nbbb\nccc\nddd\neee\nfff\nggg\nhhh";
    float3x2 rotate90 = make_float3x2(make_float2(0.0f, 1.0f), make_float2(-1.0f, 0.0f), make_float2(112.0f, 0.0f));

    px[0] = ui_test_render_clipped_text_(lines, make_double2(112.0, 112.0), (float3x2) { 0 });
    px[1] = ui_test_render_clipped_text_(lines, make_double2(112.0, 24.0), (float3x2) { 0 });
    px[2] = ui_test_render_clipped_text_(lines, make_double2(112.0, 112.0), rotate90);
    px[3] = ui_test_render_clipped_text_(lines, make_double2(24.0, 112.0), rotate90);

    cr_free(cr_mem_libc, ui_test_render_clipped_text_("iii\njjj\nkkk\nlll\nmmm\nnnn\nooo\nppp", make_double2(112.0, 24.0), (float3x2) { 0 }), cr_loc);
    uint64_t misses = ui_atlas_glyph_misses_();
    cr_free(cr_mem_libc, ui_test_render_clipped_text_("ppp", make_double2(112.0, 112.0), (float3x2) { 0 }), cr_loc);
    ui_test_check(ui_atlas_glyph_misses_() > misses, "text lines outside the clip are not rasterized");

    ui_test_then_(ui_test_text_cull_check, px);
}

static bool ui_test_cells_equal_(const uint8_t* px, uint32_t width, uint32_t x0, uint32_t x1, uint32_t cell_width, uint32_t y0, uint32_t y1) {
    for (uint32_t y = y0; y < y1; y++) {
        if (memcmp(px + (y * width + x0) * 4, px + (y * width + x1) * 4, cell_width * 4) != 0) {
            return false;
        }
    }
    return true;
}

static void ui_test_text_grid_check(void* args) {
    uint8_t* px = args;

    ui_test_check(ui_test_cells_equal_(px, 160, 59, 8, 10, 8, 32) && ui_test_cells_equal_(px, 160, 110, 8, 10, 8, 32), "glyphs on a grid line keep their cell along the whole line");
    ui_test_check(ui_test_cells_equal_(px, 160, 99, 48, 10, 8, 32), "glyphs between grid lines take the lower cell");
    cr_free(cr_mem_libc, px, cr_loc);
    ui_reset_slots(ui_ctx());
}

static void ui_test_text_grid(void) {
    struct ui_node root = ui_frame(&(struct ui_frame) {
        .size = ui_size_fix(160, 40),
        .fill = ui_fill_solid(ui_rgba(0.1f, 0.1f, 0.12f, 1.0f)),
    });

    ui_text(&(struct ui_text) {
        .parent = root,
        .pos = ui_pos_xy(8, 8),
        .str = ui_str("oooooooooooo", ui_white, 17),
    });

    ui_test_then_(ui_test_text_grid_check, ui_test_render_read_(root, &ui_test_render_desc_, gpu_format_bgra8, make_uint2(160, 40)));
}

static bool ui_test_region_lit_(const uint8_t* px, uint32_t width, uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1) {
    for (uint32_t y = y0; y < y1; y++) {
        for (uint32_t x = x0; x < x1; x++) {
            if (px[(y * width + x) * 4 + 1] > 100) {
                return true;
            }
        }
    }
    return false;
}

static void ui_test_text_ink_check(void* args) {
    uint8_t** px = args;

    ui_test_check(ui_test_region_lit_(px[0], 128, 34, 8, 40, 40), "glyph ink left of the text origin is drawn");
    ui_test_check(ui_test_region_lit_(px[1], 128, 40, 7, 80, 10), "glyph ink above the first line is drawn");
    ui_test_render_finish(px, 2);
}

static uint8_t* ui_test_render_ink_(struct ui_font* font, const char* raw) {
    struct ui_node root = ui_frame(&(struct ui_frame) {
        .size = ui_size_fix(128, 128),
        .fill = ui_fill_solid(ui_rgba(0.1f, 0.1f, 0.12f, 1.0f)),
    });

    struct ui_str str = ui_str(raw, ui_white, 40);
    str.attributes[2] = (struct ui_str_attribute) { .start = 0, .end = uint32_max, .type = ui_str_attribute_type_font, .font = font };

    ui_text(&(struct ui_text) {
        .parent = root,
        .pos = ui_pos_xy(40, 10),
        .str = str,
    });

    return ui_test_render_backdrop_(root);
}

static void ui_test_text_ink(void) {
    static uint8_t* px[2];
    uint64_t size;
    const uint8_t* data = ui_test_font_data_(true, &size);
    struct ui_font* inter = ui_font_create(data, size);

    px[0] = ui_test_render_ink_(inter, "\xc3\xae");
    px[1] = ui_test_render_ink_(inter, "\xc3\x85");

    ui_font_destroy(inter);
    ui_test_then_(ui_test_text_ink_check, px);
}

static void (*const ui_test_render_cases_[])(void) = {
    ui_test_zero_area_graphic,
    ui_test_transform,
    ui_test_transform_clip,
    ui_test_text_cull,
    ui_test_text_grid,
    ui_test_text_ink,
    ui_test_clip_shape,
    ui_test_backdrop_root_area,
    ui_test_backdrop_root_snapshot,
    ui_test_backdrop_root_snapshot_layered,
    ui_test_backdrop_root_nested,
    ui_test_glass_tint_modes,
    ui_test_glass_color_spaces,
    ui_test_gradient_fill,
    ui_test_layer_blur_shadow,
    ui_test_compositing_format,
    ui_test_direct_to_target,
};

static void ui_test_render_case(void* args) {
    uint32_t i = (uint32_t)(uintptr_t)args;

    ui_test_render_cases_[i]();
    if (i + 1 < cr_count(ui_test_render_cases_)) {
        ui_test_then_(ui_test_render_case, (void*)(uintptr_t)(i + 1));
    }
}

void ui_test_render_(void) {
    ui_test_render_case((void*)(uintptr_t)0);
}

