#include "ui_tests.h"

static uint32_t ui_test_blur_collage(struct ui_test_sui_item* items, uint32_t group, float blur,
                                     double x, double y, double size, uint32_t tiles) {
    uint32_t count = 0;

    struct ui_test_sui_item backing = {
        .kind = ui_test_sui_kind_rect,
        .origin = make_double2(x, y),
        .size = make_double2(size, size),
        .fill = ui_system_background.value,
        .blur = blur,
        .group = group,
    };
    items[count++] = backing;

    double margin = size * 0.0625;
    double pitch = (size - 2.0 * margin) / (double)tiles;
    for (uint32_t t = 0; t < tiles * tiles; t++) {
        struct ui_test_sui_item tile = {
            .kind = t % 5 == 4 ? ui_test_sui_kind_ellipse : ui_test_sui_kind_rect,
            .origin = make_double2(x + margin + (double)(t % tiles) * pitch, y + margin + (double)(t / tiles) * pitch),
            .size = make_double2(pitch * 0.8, pitch * 0.8),
            .radius = make_double4(pitch * 0.15, pitch * 0.15, pitch * 0.15, pitch * 0.15),
            .fill = ui_test_palette_(t),
            .group = group,
        };
        items[count++] = tile;
    }

    return count;
}

static void ui_test_scene_blur_small_r4_fill(struct ui_test_sui_spec* spec, struct ui_test_sui_item* items) {
    uint32_t count = ui_test_blur_collage(items, 1, 4.0f, 112.0, 112.0, 288.0, 5);
    *spec = (struct ui_test_sui_spec) { .background = ui_system_gray6.value, .items = items, .item_count = count };
}

static void ui_test_scene_blur_small_r42_fill(struct ui_test_sui_spec* spec, struct ui_test_sui_item* items) {
    uint32_t count = ui_test_blur_collage(items, 1, 42.0f, 112.0, 112.0, 288.0, 5);
    *spec = (struct ui_test_sui_spec) { .background = ui_system_gray6.value, .items = items, .item_count = count };
}

static void ui_test_scene_blur_screen_r12_fill(struct ui_test_sui_spec* spec, struct ui_test_sui_item* items) {
    uint32_t count = ui_test_blur_collage(items, 1, 12.0f, 0.0, 0.0, 1024.0, 12);
    *spec = (struct ui_test_sui_spec) { .background = ui_system_gray6.value, .items = items, .item_count = count };
}

static void ui_test_scene_blur_screen_r42_fill(struct ui_test_sui_spec* spec, struct ui_test_sui_item* items) {
    uint32_t count = ui_test_blur_collage(items, 1, 42.0f, 0.0, 0.0, 1024.0, 12);
    *spec = (struct ui_test_sui_spec) { .background = ui_system_gray6.value, .items = items, .item_count = count };
}

static void ui_test_scene_blur_screen_r160_fill(struct ui_test_sui_spec* spec, struct ui_test_sui_item* items) {
    uint32_t count = ui_test_blur_collage(items, 1, 160.0f, 0.0, 0.0, 1024.0, 12);
    *spec = (struct ui_test_sui_spec) { .background = ui_system_gray6.value, .items = items, .item_count = count };
}

static void ui_test_scene_blur_mixed_fill(struct ui_test_sui_spec* spec, struct ui_test_sui_item* items) {
    static const float radii[4] = { 2.0f, 8.0f, 20.0f, 42.0f };
    uint32_t count = 0;
    for (uint32_t p = 0; p < 4; p++) {
        count += ui_test_blur_collage(items + count, p + 1, radii[p],
                                      48.0 + (double)(p % 2) * 504.0, 48.0 + (double)(p / 2) * 504.0, 424.0, 5);
    }
    *spec = (struct ui_test_sui_spec) { .background = ui_system_gray6.value, .items = items, .item_count = count };
}

static const struct ui_test_scene ui_test_scenes_blur_table_[] = {
    {
        .label = "blur-small-r4", .group = "blur",
        .capture = ui_test_scene_capture_screenshot,
        .size = { 512.0, 512.0 },
        .mean_threshold = 0.006f,
        .tile_threshold = 0.05f,
        .alarm_allowance = 64,
        ui_test_sui_scene(ui_test_scene_blur_small_r4, 32),
    },
    {
        .label = "blur-small-r42", .group = "blur",
        .capture = ui_test_scene_capture_screenshot,
        .size = { 512.0, 512.0 },
        .mean_threshold = 0.025f,
        .tile_threshold = 0.12f,
        .alarm_allowance = 64,
        ui_test_sui_scene(ui_test_scene_blur_small_r42, 32),
    },
    {
        .label = "blur-screen-r12", .group = "blur",
        .capture = ui_test_scene_capture_screenshot,
        .size = { 1024.0, 1024.0 },
        .mean_threshold = 0.02f,
        .tile_threshold = 0.09f,
        .alarm_allowance = 64,
        ui_test_sui_scene(ui_test_scene_blur_screen_r12, 152),
    },
    {
        .label = "blur-screen-r42", .group = "blur",
        .capture = ui_test_scene_capture_screenshot,
        .size = { 1024.0, 1024.0 },
        .mean_threshold = 0.1f,
        .tile_threshold = 0.18f,
        .alarm_allowance = 64,
        ui_test_sui_scene(ui_test_scene_blur_screen_r42, 152),
    },
    {
        .label = "blur-mixed", .group = "blur",
        .capture = ui_test_scene_capture_screenshot,
        .size = { 1024.0, 1024.0 },
        .mean_threshold = 0.03f,
        .tile_threshold = 0.15f,
        .alarm_allowance = 64,
        ui_test_sui_scene(ui_test_scene_blur_mixed, 112),
    },
    {
        .label = "blur-screen-r160", .group = "blur",
        .capture = ui_test_scene_capture_screenshot,
        .size = { 1024.0, 1024.0 },
        .mean_threshold = 0.1f,
        .tile_threshold = 0.18f,
        .alarm_allowance = 64,
        ui_test_sui_scene(ui_test_scene_blur_screen_r160, 152),
    },
};

const struct ui_test_scene* ui_test_scenes_blur_(struct ui_test_scenes* scenes, uint32_t* out_count) {
    *out_count = cr_count(ui_test_scenes_blur_table_);
    return ui_test_scenes_blur_table_;
}
