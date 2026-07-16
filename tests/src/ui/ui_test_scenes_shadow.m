#include "ui_tests.h"
#include <math.h>

static struct ui_test_sui_item ui_test_shadow_card(double x, double y, double w, double h, double radius, float4 fill,
                                                   float4 shadow, float shadow_radius, double2 shadow_offset) {
    return (struct ui_test_sui_item) {
        .kind = ui_test_sui_kind_rect,
        .origin = make_double2(x, y),
        .size = make_double2(w, h),
        .radius = make_double4(radius, radius, radius, radius),
        .fill = fill,
        .shadow = shadow,
        .shadow_radius = shadow_radius,
        .shadow_offset = shadow_offset,
    };
}

static float4 ui_test_shadow_black(float alpha) {
    return make_float4(0.0f, 0.0f, 0.0f, alpha);
}

static void ui_test_scene_shadow_radius_fill(struct ui_test_sui_spec* spec, struct ui_test_sui_item* items) {
    uint32_t count = 0;

    static const float ladder[] = { 0.5f, 1.0f, 2.0f, 4.0f, 8.0f, 16.0f, 32.0f };
    for (uint32_t i = 0; i < cr_count(ladder); i++) {
        items[count++] = ui_test_shadow_card(48.0 + (double)i * 136.0, 80.0, 120.0, 120.0, 24.0,
                                             ui_system_background.value, ui_test_shadow_black(0.33f), ladder[i], make_double2(0.0, 4.0));
        struct ui_test_sui_item circle = ui_test_shadow_card(48.0 + (double)i * 136.0, 320.0, 120.0, 120.0, 0.0,
                                                             ui_system_background.value, ui_test_shadow_black(0.33f), ladder[i], make_double2(0.0, 12.0));
        circle.kind = ui_test_sui_kind_ellipse;
        items[count++] = circle;
        items[count++] = ui_test_shadow_card(48.0 + (double)i * 136.0, 560.0, 120.0, 120.0, 24.0,
                                             ui_test_palette_(i), ui_test_shadow_black(0.33f), ladder[i], make_double2(0.0, 4.0));
    }

    static const float soft[] = { 24.0f, 48.0f, 64.0f };
    for (uint32_t i = 0; i < cr_count(soft); i++) {
        items[count++] = ui_test_shadow_card(64.0 + (double)i * 336.0, 792.0, 200.0, 160.0, 32.0,
                                             ui_system_background.value, ui_test_shadow_black(0.4f), soft[i], make_double2(0.0, 16.0));
    }

    assert(count <= 32);
    *spec = (struct ui_test_sui_spec) { .background = ui_system_gray6.value, .items = items, .item_count = count };
}

static void ui_test_scene_shadow_offset_fill(struct ui_test_sui_spec* spec, struct ui_test_sui_item* items) {
    uint32_t count = 0;

    for (uint32_t i = 0; i < 9; i++) {
        double dx = (double)(int32_t)(i % 3) * 24.0 - 24.0;
        double dy = (double)(int32_t)(i / 3) * 24.0 - 24.0;
        items[count++] = ui_test_shadow_card(96.0 + (double)(i % 3) * 280.0, 96.0 + (double)(i / 3) * 280.0,
                                             140.0, 140.0, 28.0, ui_system_background.value,
                                             ui_test_shadow_black(0.35f), 8.0f, make_double2(dx, dy));
    }

    static const struct { double x, y; } large[] = {
        { 64.0, 0.0 }, { 0.0, 64.0 }, { 48.0, 32.0 }, { -48.0, 32.0 },
    };
    for (uint32_t i = 0; i < cr_count(large); i++) {
        items[count++] = ui_test_shadow_card(112.0 + (double)i * 232.0, 880.0, 140.0, 96.0, 20.0,
                                             ui_system_background.value, ui_test_shadow_black(0.35f), 12.0f,
                                             make_double2(large[i].x, large[i].y));
    }

    assert(count <= 16);
    *spec = (struct ui_test_sui_spec) { .background = ui_system_gray6.value, .items = items, .item_count = count };
}

static void ui_test_scene_shadow_color_fill(struct ui_test_sui_spec* spec, struct ui_test_sui_item* items) {
    uint32_t count = 0;

    static const uint32_t shadow_colors[6] = { 7, 0, 3, 9, 1, 5 };
    for (uint32_t i = 0; i < 6; i++) {
        float4 shadow = ui_test_palette_(shadow_colors[i]);
        shadow.w = 0.6f;
        items[count++] = ui_test_shadow_card(48.0 + (double)i * 160.0, 80.0, 120.0, 120.0, 24.0,
                                             ui_system_background.value, shadow, 16.0f, make_double2(0.0, 10.0));
    }

    static const float alphas[] = { 0.15f, 0.3f, 0.5f, 0.75f, 1.0f };
    for (uint32_t i = 0; i < cr_count(alphas); i++) {
        items[count++] = ui_test_shadow_card(48.0 + (double)i * 192.0, 320.0, 140.0, 120.0, 24.0,
                                             ui_system_background.value, ui_test_shadow_black(alphas[i]), 16.0f, make_double2(0.0, 10.0));
    }

    for (uint32_t i = 0; i < 5; i++) {
        float4 shadow = ui_test_palette_(i * 2 + 1);
        shadow.w = 0.7f;
        items[count++] = ui_test_shadow_card(48.0 + (double)i * 192.0, 560.0, 140.0, 120.0, 24.0,
                                             ui_test_palette_(i * 2), shadow, 12.0f, make_double2(6.0, 8.0));
    }

    for (uint32_t i = 0; i < 4; i++) {
        float4 glow = ui_test_palette_(shadow_colors[i]);
        glow.w = 0.8f;
        struct ui_test_sui_item circle = ui_test_shadow_card(96.0 + (double)i * 240.0, 800.0, 140.0, 140.0, 0.0,
                                                             ui_system_background.value, glow, 40.0f, make_double2(0.0, 0.0));
        circle.kind = ui_test_sui_kind_ellipse;
        items[count++] = circle;
    }

    assert(count <= 32);
    *spec = (struct ui_test_sui_spec) { .background = ui_system_background.value, .items = items, .item_count = count };
}

static void ui_test_scene_shadow_shape_fill(struct ui_test_sui_spec* spec, struct ui_test_sui_item* items) {
    uint32_t count = 0;

    float4 shadow = ui_test_shadow_black(0.35f);
    float radius = 12.0f;
    double2 offset = make_double2(0.0, 8.0);

    items[count++] = ui_test_shadow_card(64.0, 80.0, 200.0, 150.0, 32.0, ui_test_palette_(7), shadow, radius, offset);

    struct ui_test_sui_item circular = ui_test_shadow_card(360.0, 80.0, 200.0, 150.0, 32.0, ui_test_palette_(5), shadow, radius, offset);
    circular.circular = true;
    items[count++] = circular;

    struct ui_test_sui_item uneven = ui_test_shadow_card(656.0, 80.0, 200.0, 150.0, 0.0, ui_test_palette_(1), shadow, radius, offset);
    uneven.radius = make_double4(64.0, 8.0, 8.0, 64.0);
    uneven.circular = true;
    items[count++] = uneven;

    struct ui_test_sui_item circle = ui_test_shadow_card(64.0, 340.0, 170.0, 170.0, 0.0, ui_test_palette_(3), shadow, radius, offset);
    circle.kind = ui_test_sui_kind_ellipse;
    items[count++] = circle;

    struct ui_test_sui_item capsule = ui_test_shadow_card(360.0, 360.0, 240.0, 90.0, 45.0, ui_test_palette_(9), shadow, radius, offset);
    capsule.circular = true;
    items[count++] = capsule;

    struct ui_test_sui_item ellipse = ui_test_shadow_card(680.0, 340.0, 280.0, 130.0, 0.0, ui_test_palette_(0), shadow, radius, offset);
    ellipse.kind = ui_test_sui_kind_ellipse;
    items[count++] = ellipse;

    struct ui_path* star_path = ui_path_create();
    for (uint32_t i = 0; i < 10; i++) {
        double r = i % 2 == 0 ? 120.0 : 52.0;
        double angle = -0.5 * M_PI + (double)i * 0.2 * M_PI;
        double2 p = make_double2(184.0 + r * cos(angle), 740.0 + r * sin(angle));
        if (i == 0) {
            ui_path_move_to(star_path, p);
        } else {
            ui_path_line_to(star_path, p);
        }
    }
    ui_path_close(star_path);
    struct ui_test_sui_item star = {
        .kind = ui_test_sui_kind_path,
        .path = star_path,
        .fill = ui_test_palette_(2),
        .shadow = shadow,
        .shadow_radius = radius,
        .shadow_offset = offset,
    };
    items[count++] = star;

    struct ui_test_sui_item ring = ui_test_shadow_card(420.0, 640.0, 200.0, 200.0, 0.0, make_float4(0.0f, 0.0f, 0.0f, 0.0f), shadow, radius, offset);
    ring.kind = ui_test_sui_kind_ellipse;
    ring.stroke = ui_test_palette_(10);
    ring.stroke_width = 24.0f;
    ring.stroke_align = ui_outline_align_center;
    items[count++] = ring;

    struct ui_test_sui_item overlap_a = ui_test_shadow_card(700.0, 620.0, 200.0, 150.0, 32.0, ui_system_background.value, shadow, 16.0f, make_double2(0.0, 12.0));
    items[count++] = overlap_a;
    struct ui_test_sui_item overlap_b = ui_test_shadow_card(780.0, 720.0, 200.0, 150.0, 32.0, ui_system_background.value, shadow, 16.0f, make_double2(0.0, 12.0));
    items[count++] = overlap_b;

    assert(count <= 16);
    *spec = (struct ui_test_sui_spec) { .background = ui_system_gray6.value, .items = items, .item_count = count };
}

#define ui_test_shadow_many_rows 16
#define ui_test_shadow_many_cols 40

static void ui_test_scene_shadow_many_fill(struct ui_test_sui_spec* spec, struct ui_test_sui_item* items) {
    uint32_t count = 0;
    for (uint32_t row = 0; row < ui_test_shadow_many_rows; row++) {
        for (uint32_t col = 0; col < ui_test_shadow_many_cols; col++) {
            struct ui_test_sui_item item = ui_test_shadow_card(4.0 + (double)col * 16.0, 8.0 + (double)row * 34.0, 14.0, 24.0, 3.0 + (double)(col % 4),
                                                              ui_gray(0.7f).value, ui_test_shadow_black(0.5f), (6.0f + (float)(row % 5)) / 3.0f,
                                                              make_double2(1.0 + (double)(col % 3), 2.0));
            item.stroke = ui_gray(0.3f).value;
            item.stroke_width = 1.0f;
            item.stroke_align = ui_outline_align_center;
            items[count++] = item;
        }
    }
    assert(count <= ui_test_shadow_many_rows * ui_test_shadow_many_cols);
    *spec = (struct ui_test_sui_spec) { .background = ui_gray(0.1f).value, .items = items, .item_count = count };
}

static const struct ui_test_scene ui_test_scenes_shadow_table_[] = {
    {
        .label = "shadow-radius", .group = "shadow",
        .capture = ui_test_scene_capture_screenshot,
        .size = { 1024.0, 1024.0 },
        .mean_threshold = 0.003f,
        .tile_threshold = 0.02f,
        .alarm_allowance = 32,
        ui_test_sui_scene(ui_test_scene_shadow_radius, 32),
    },
    {
        .label = "shadow-offset", .group = "shadow",
        .capture = ui_test_scene_capture_screenshot,
        .size = { 1024.0, 1024.0 },
        .mean_threshold = 0.003f,
        .tile_threshold = 0.02f,
        .alarm_allowance = 32,
        ui_test_sui_scene(ui_test_scene_shadow_offset, 16),
    },
    {
        .label = "shadow-color", .group = "shadow",
        .capture = ui_test_scene_capture_screenshot,
        .size = { 1024.0, 1024.0 },
        .mean_threshold = 0.003f,
        .tile_threshold = 0.02f,
        .alarm_allowance = 32,
        ui_test_sui_scene(ui_test_scene_shadow_color, 32),
    },
    {
        .label = "shadow-shape", .group = "shadow",
        .capture = ui_test_scene_capture_screenshot,
        .size = { 1024.0, 1024.0 },
        .mean_threshold = 0.003f,
        .tile_threshold = 0.02f,
        .alarm_allowance = 32,
        ui_test_sui_scene(ui_test_scene_shadow_shape, 16),
    },
    {
        .label = "shadow-many", .group = "shadow",
        .capture = ui_test_scene_capture_screenshot,
        .size = { 1024.0, 1024.0 },
        .mean_threshold = 0.008f,
        .tile_threshold = 0.04f,
        .alarm_allowance = 512,
        ui_test_sui_scene(ui_test_scene_shadow_many, ui_test_shadow_many_rows * ui_test_shadow_many_cols),
    },
};

const struct ui_test_scene* ui_test_scenes_shadow_(struct ui_test_scenes* scenes, uint32_t* out_count) {
    *out_count = cr_count(ui_test_scenes_shadow_table_);
    return ui_test_scenes_shadow_table_;
}
