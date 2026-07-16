#include "ui_tests.h"
#include "ui_core.h"

enum { ui_test_health_width_pt = 512, ui_test_health_height_pt = 128 };

struct ui_test_health_patch {
    const char* label;
    uint2 origin;
    uint2 size;
    float rgb[3];
    float expected[3];
};

static const struct ui_test_health_patch ui_test_health_patches_[] = {
    { "corner tl", { 0, 0 }, { 8, 8 }, { 1.0f, 1.0f, 1.0f }, { 0 } },
    { "corner tr", { 504, 0 }, { 8, 8 }, { 1.0f, 1.0f, 1.0f }, { 0 } },
    { "corner bl", { 0, 120 }, { 8, 8 }, { 1.0f, 1.0f, 1.0f }, { 0 } },
    { "corner br", { 504, 120 }, { 8, 8 }, { 1.0f, 1.0f, 1.0f }, { 0 } },
    { "gray 0.25", { 32, 32 }, { 48, 48 }, { 0.25f, 0.25f, 0.25f }, { 0 } },
    { "gray 0.50", { 88, 32 }, { 48, 48 }, { 0.5f, 0.5f, 0.5f }, { 0 } },
    { "gray 0.75", { 144, 32 }, { 48, 48 }, { 0.75f, 0.75f, 0.75f }, { 0 } },
    { "gray 1.00", { 200, 32 }, { 48, 48 }, { 1.0f, 1.0f, 1.0f }, { 0 } },
    { "gray 1.25", { 256, 32 }, { 48, 48 }, { 1.25f, 1.25f, 1.25f }, { 0 } },
    { "gray 1.50", { 312, 32 }, { 48, 48 }, { 1.5f, 1.5f, 1.5f }, { 0 } },
    { "gray 1.75", { 368, 32 }, { 48, 48 }, { 1.75f, 1.75f, 1.75f }, { 0 } },
    { "gray 2.00", { 424, 32 }, { 48, 48 }, { 2.0f, 2.0f, 2.0f }, { 0 } },
    { "srgb red", { 48, 92 }, { 48, 24 }, { 0.917488f, 0.200287f, 0.138561f }, { 0 } },
    { "srgb green", { 120, 92 }, { 48, 24 }, { 0.458402f, 0.985265f, 0.298295f }, { 0 } },
    { "srgb blue", { 192, 92 }, { 48, 24 }, { 0.0f, 0.0f, 0.959588f }, { 0 } },
    { "p3 red", { 264, 92 }, { 48, 24 }, { 0.95f, 0.0f, 0.0f }, { 0.9536f, 0.1527f, 0.0825f } },
    { "p3 green", { 336, 92 }, { 48, 24 }, { 0.0f, 0.95f, 0.0f }, { 0.4009f, 0.9536f, 0.2368f } },
    { "p3 blue", { 408, 92 }, { 48, 24 }, { 0.0f, 0.0f, 0.95f }, { 0 } },
};

static uint2 ui_test_health_px_size(float to_px_from_pt) {
    assert(to_px_from_pt == floorf(to_px_from_pt));
    return make_uint2(ui_test_health_width_pt, ui_test_health_height_pt) * (uint32_t)to_px_from_pt;
}

static void ui_test_health_pattern(float* rgba, float to_px_from_pt, bool expected) {
    uint2 size = ui_test_health_px_size(to_px_from_pt);
    memset(rgba, 0, (size_t)size.x * size.y * 4 * sizeof(float));
    for (uint32_t i = 0; i < size.x * size.y; i++) {
        rgba[i * 4 + 3] = 1.0f;
    }
    for (uint32_t p = 0; p < cr_count(ui_test_health_patches_); p++) {
        const struct ui_test_health_patch* patch = &ui_test_health_patches_[p];
        bool pinned = patch->expected[0] != 0.0f || patch->expected[1] != 0.0f || patch->expected[2] != 0.0f;
        const float* value = expected && pinned ? patch->expected : patch->rgb;
        uint2 min = patch->origin * (uint32_t)to_px_from_pt;
        uint2 max = (patch->origin + patch->size) * (uint32_t)to_px_from_pt;
        for (uint32_t y = min.y; y < max.y; y++) {
            for (uint32_t x = min.x; x < max.x; x++) {
                float* px = &rgba[((uint64_t)y * size.x + x) * 4];
                px[0] = value[0];
                px[1] = value[1];
                px[2] = value[2];
            }
        }
    }
}

struct ui_test_health_state {
    struct ui_test_scene_state base;
    struct ui_graphic* graphic;
};

static struct ui_node ui_test_scene_glass_health_build(struct ui_test_scene_state* state, struct ui_node parent, double2 size, const struct ui_test_scene* scene) {
    struct ui_test_health_state* health = (struct ui_test_health_state*)state;
    return ui_frame(&(struct ui_frame) {
        .parent = parent,
        .size = ui_size_fix(size.x, size.y),
        .fill.layers[0] = ui_layer_graphic_ex(health->graphic, ui_address_mode_fill, ui_filter_linear, make_double2(1.0, 1.0), ui_white, ui_color_space_display_p3, true),
    });
}

#if cr_ios
static void ui_test_scene_glass_health_present(struct ui_test_scene_state* state, uint32_t width, uint32_t height, float to_px_from_pt) {
    struct cr_mem* mem = cr_mem_libc;
    float* rgba = cr_malloc(mem, (uint64_t)width * height * 4 * sizeof(float), 16, cr_loc);
    ui_test_health_pattern(rgba, to_px_from_pt, false);
    ui_test_scene_present_raw_(width, height, rgba);
    cr_free(mem, rgba, cr_loc);
}
#endif

static void ui_test_health_state_destroy(struct ui_test_scene_state* state) {
    struct ui_test_health_state* health = (struct ui_test_health_state*)state;
    ui_graphic_destroy(health->graphic);
    cr_free(state->mem, state, cr_loc);
}

static struct ui_test_scene_state* ui_test_health_state_create(struct ui_test_scenes* scenes, struct cr_mem* mem, const void* ctx) {
    struct ui_test_health_state* health = cr_malloc(mem, sizeof(*health), 16, cr_loc);
    *health = (struct ui_test_health_state) {
        .base = {
            .scenes = scenes,
            .mem = mem,
            .destroy = ui_test_health_state_destroy,
        },
    };

    float to_px_from_pt = scenes->desc.screen_to_px_from_pt;
    uint2 size = ui_test_health_px_size(to_px_from_pt);
    uint64_t count = (uint64_t)size.x * size.y * 4;
    float* rgba = cr_malloc(mem, count * sizeof(float), 16, cr_loc);
    ui_test_health_pattern(rgba, to_px_from_pt, true);
    __fp16* half = cr_malloc(mem, count * sizeof(__fp16), 16, cr_loc);
    for (uint64_t i = 0; i < count; i++) {
        half[i] = (__fp16)rgba[i];
    }
    health->graphic = ui_graphic_create(size.x, size.y, ui_pixel_format_rgba16f,
                                        (const uint8_t*)half, size.x * 4 * (uint32_t)sizeof(__fp16), true);
    cr_free(mem, half, cr_loc);
    cr_free(mem, rgba, cr_loc);

    return &health->base;
}

static const struct ui_test_scene ui_test_scenes_health_table_[] = {
    {
        .label = "health", .group = "health",
        .capture = ui_test_scene_capture_screenshot,
        .size = { ui_test_health_width_pt, ui_test_health_height_pt },
        .output_color_space = ui_color_space_display_p3,
        .mean_threshold = 0.002f,
        .tile_threshold = 0.006f,
        .alarm_diff = 0.05f,
        .create = ui_test_health_state_create,
        .build = ui_test_scene_glass_health_build,
        .present = ui_test_scene_present(ui_test_scene_glass_health_present),
    },
};

const struct ui_test_scene* ui_test_scenes_health_(struct ui_test_scenes* scenes, uint32_t* out_count) {
    *out_count = cr_count(ui_test_scenes_health_table_);
    return ui_test_scenes_health_table_;
}

