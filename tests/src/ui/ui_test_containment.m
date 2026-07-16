#include "ui_tests.h"
#include "ui_core.h"
#include <stdio.h>


#define ui_test_containment_span_bound_ 1.5
#define ui_test_containment_radius_ 14.0

static const struct {
    const char* label;
    enum ui_corner_profile profile;
} ui_test_containment_cases_[] = {
    { "continuous", ui_corner_profile_continuous },
    { "circular", ui_corner_profile_circular },
};

struct ui_test_containment_params {
    double radius;
    enum ui_corner_profile profile;
};

struct ui_test_containment_state {
    struct ui_test_scene_state base;
    const struct ui_test_containment_params* params;
};

static void ui_test_containment_exclude(uint8_t* mask, uint2 dims, int32_t cx, int32_t cy, int32_t half) {
    int32_t x0 = max_int32(cx - half, 0);
    int32_t x1 = min_int32(cx + half, (int32_t)dims.x);
    int32_t y0 = max_int32(cy - half, 0);
    int32_t y1 = min_int32(cy + half, (int32_t)dims.y);

    for (int32_t y = y0; y < y1; y++) {
        memset(mask + (uint32_t)y * dims.x + (uint32_t)x0, 1, (size_t)(x1 - x0));
    }
}

static struct ui_node ui_test_containment_build(struct ui_test_scene_state* state, struct ui_node parent, double2 size, const struct ui_test_scene* scene) {
    const struct ui_test_containment_params* params = ((struct ui_test_containment_state*)state)->params;
    double radius = params->radius;
    enum ui_corner_profile profile = params->profile;

    struct ui_node root = ui_frame(&(struct ui_frame) {
        .parent = parent,
        .size = ui_size_fix(size.x, size.y),
        .fill = ui_fill_solid(ui_system_gray6),
        .clip = true,
    });

    struct ui_color panel_colors[4] = { ui_system_blue, ui_system_indigo, ui_system_green, ui_system_brown };
    struct ui_color tile_colors[4] = { ui_system_red, ui_system_cyan, ui_system_yellow, ui_system_mint };

    double panel_margin = 16.0;
    double panel = 0.5 * (size.x - 3.0 * panel_margin);

    for (uint32_t i = 0; i < 4; i++) {
        struct ui_frame panel_desc = {
            .parent = root,
            .pos = ui_pos_xy(panel_margin + (double)(i % 2) * (panel + panel_margin),
                             panel_margin + (double)(i / 2) * (panel + panel_margin)),
            .size = ui_size_fix(panel, panel),
            .fill = ui_fill_solid(panel_colors[i]),
        };

        if (radius > 0.0) {
            panel_desc.corners = ui_corners(radius * 3.0);
            panel_desc.corners.profile = profile;
        }

        ui_frame(&panel_desc);
    }

    uint32_t grid = 16;
    double pitch = size.x / (double)grid;
    double tile = pitch - 8.0;

    for (uint32_t y = 0; y < grid; y++) {
        for (uint32_t x = 0; x < grid; x++) {
            struct ui_frame tile_desc = {
                .parent = root,
                .pos = ui_pos_xy(4.0 + pitch * (double)x, 4.0 + pitch * (double)y),
                .size = ui_size_fix(tile, tile),
                .fill = ui_fill_solid(tile_colors[(x + y) % 4]),
            };

            if (radius > 0.0) {
                tile_desc.corners = ui_corners(radius);
                tile_desc.corners.profile = profile;
            }

            ui_frame(&tile_desc);
        }
    }

    return root;
}

static struct ui_test_scene_state* ui_test_containment_create(struct ui_test_scenes* scenes, struct cr_mem* mem, const void* ctx) {
    struct ui_test_containment_state* state = cr_malloc(mem, sizeof(*state), 16, cr_loc);
    *state = (struct ui_test_containment_state) {
        .base = {
            .scenes = scenes,
            .mem = mem,
            .destroy = ui_test_scene_state_free_,
        },
        .params = ctx,
    };
    return &state->base;
}

struct ui_test_containment_read {
    __fp16* flat;
    __fp16* rounded;
};

static struct ui_test_containment_read ui_test_containment_reads_[cr_count(ui_test_containment_cases_)];

static void ui_test_containment_check(void* args) {
    const struct ui_test_containment_read* read = args;
    struct cr_mem* mem = cr_mem_libc;
    uint32_t index = (uint32_t)(read - ui_test_containment_reads_);
    uint2 dims = make_uint2(1024, 1024);

    uint8_t* mask = cr_malloc(mem, (uint64_t)dims.x * dims.y, 0, cr_loc);
    memset(mask, 0, (size_t)dims.x * dims.y);

    int32_t panel_half = (int32_t)ceil(ui_test_containment_radius_ * 3.0 * ui_test_containment_span_bound_) + 4;
    int32_t tile_half = (int32_t)ceil(ui_test_containment_radius_ * ui_test_containment_span_bound_) + 4;

    for (int32_t i = 0; i < 4; i++) {
        int32_t ox = 16 + (i % 2) * 504;
        int32_t oy = 16 + (i / 2) * 504;

        for (int32_t k = 0; k < 4; k++) {
            ui_test_containment_exclude(mask, dims, ox + (k % 2) * 488, oy + (k / 2) * 488, panel_half);
        }
    }

    for (int32_t gy = 0; gy < 16; gy++) {
        for (int32_t gx = 0; gx < 16; gx++) {
            int32_t ox = 4 + gx * 64;
            int32_t oy = 4 + gy * 64;

            for (int32_t k = 0; k < 4; k++) {
                ui_test_containment_exclude(mask, dims, ox + (k % 2) * 56, oy + (k / 2) * 56, tile_half);
            }
        }
    }

    uint32_t tested_count = 0;
    uint32_t diff_count = 0;
    double max = 0.0;
    uint32_t max_x = 0;
    uint32_t max_y = 0;

    for (uint32_t i = 0; i < dims.x * dims.y; i++) {
        if (mask[i]) {
            continue;
        }

        tested_count++;

        float d = 0.0f;
        for (uint32_t ch = 0; ch < 4; ch++) {
            d = max_float(d, fabsf((float)read->rounded[i * 4 + ch] - (float)read->flat[i * 4 + ch]));
        }

        if (d > 0.0f) {
            diff_count++;
            if (d > max) {
                max = d;
                max_x = i % dims.x;
                max_y = i / dims.x;
            }
        }
    }

    cr_free(mem, mask, cr_loc);
    cr_free(mem, read->rounded, cr_loc);
    cr_free(mem, read->flat, cr_loc);

    bool ok = diff_count == 0;
    printf("containment %s %ux%u px: %u px tested, %u diff px, max %.4f at (%u,%u) px, %s\n",
           ui_test_containment_cases_[index].label, dims.x, dims.y, tested_count, diff_count, max, max_x, max_y,
           ok ? "ok" : "FAIL");
    assert(ok);
}

static void ui_test_containment_case_(uint32_t index) {
    struct ui_test_containment_params params = { .radius = 0.0, .profile = ui_corner_profile_continuous };
    struct ui_test_scene scene = {
        .label = "containment",
        .width = 1024, .height = 1024, .to_px_from_pt = 1.0f,
        .create = ui_test_containment_create,
        .ctx = &params,
        .build = ui_test_containment_build,
    };

    struct ui_test_containment_read* read = &ui_test_containment_reads_[index];
    read->flat = ui_test_scene_render_read_(NULL, &scene);

    params = (struct ui_test_containment_params) { .radius = ui_test_containment_radius_, .profile = ui_test_containment_cases_[index].profile };
    read->rounded = ui_test_scene_render_read_(NULL, &scene);

    ui_test_then_(ui_test_containment_check, read);
}

void ui_test_containment_(void) {
    for (uint32_t i = 0; i < cr_count(ui_test_containment_cases_); i++) {
        ui_test_containment_case_(i);
    }
}

