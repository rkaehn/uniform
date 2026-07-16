#include "demo.h"
#include "ui_input.h"
#include "ui_gestures.h"
#include "hud.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define canvas_scale_min 0.1
#define canvas_scale_max 40.0
#define canvas_dot_spacing 40.0
#define canvas_dot_radius 2.0
#define canvas_dot_samples 4u
#define canvas_cell 48.0
#define canvas_cells 8u

struct canvas_page {
    struct cr_mem* mem;
    double2 translation;
    double scale;
    double rotation;
    double2 pan_last;
    double pinch_last;
    double rotate_last;
    bool placed;
    struct ui_graphic* checker[2];
    struct ui_graphic* dots[2];
    double dots_to_px;
};

static struct ui_graphic* canvas_checker_graphic(float light, float dark) {
    uint8_t l = (uint8_t)lroundf(light * 255.0f);
    uint8_t d = (uint8_t)lroundf(dark * 255.0f);
    uint8_t pixels[16] = {
        l, l, l, 255, d, d, d, 255,
        d, d, d, 255, l, l, l, 255,
    };

    return ui_graphic_create(2, 2, ui_pixel_format_bgra8, pixels, 8, true);
}

static struct ui_graphic* canvas_dots_graphic(struct cr_mem* mem, uint32_t step, double to_px, float gray, float alpha) {
    uint8_t* pixels = cr_malloc(mem, (uint64_t)step * step * 4, 0, cr_loc);
    double radius = canvas_dot_radius * to_px;
    double2 half = make_double2(step, step) * 0.5;
    double sample = 1.0 / (double)canvas_dot_samples;

    for (uint32_t y = 0; y < step; y++) {
        for (uint32_t x = 0; x < step; x++) {
            uint32_t inside = 0;

            for (uint32_t sy = 0; sy < canvas_dot_samples; sy++) {
                for (uint32_t sx = 0; sx < canvas_dot_samples; sx++) {
                    double2 q = make_double2(x, y) + (make_double2(sx, sy) + 0.5) * sample + half;
                    double2 p = q - floor_double2(q / (double)step) * (double)step - half;

                    inside += length_double2(p) <= radius ? 1u : 0u;
                }
            }

            float coverage = (float)inside / (float)(canvas_dot_samples * canvas_dot_samples);
            float a = coverage * alpha;
            uint8_t* pixel = &pixels[(y * step + x) * 4];
            uint8_t value = to_uint8_from_float_clamped(gray * a);

            pixel[0] = value;
            pixel[1] = value;
            pixel[2] = value;
            pixel[3] = to_uint8_from_float_clamped(a);
        }
    }

    struct ui_graphic* graphic = ui_graphic_create(step, step, ui_pixel_format_bgra8, pixels, step * 4, true);

    cr_free(mem, pixels, cr_loc);

    return graphic;
}

static void canvas_dots_sync(struct canvas_page* page, double to_px) {
    if (page->dots_to_px == to_px) {
        return;
    }

    if (page->dots_to_px != 0.0) {
        ui_graphic_destroy(page->dots[0]);
        ui_graphic_destroy(page->dots[1]);
    }

    uint32_t step = (uint32_t)lround(canvas_dot_spacing * to_px);

    page->dots_to_px = to_px;
    page->dots[0] = canvas_dots_graphic(page->mem, step, to_px, 0.0f, 0.12f);
    page->dots[1] = canvas_dots_graphic(page->mem, step, to_px, 1.0f, 0.16f);
}

static double2 canvas_rotate(double2 v, double angle) {
    double c = cos(angle);
    double s = sin(angle);

    return make_double2(v.x * c - v.y * s, v.x * s + v.y * c);
}

static bool canvas_phase_updates(enum ui_gesture_phase phase) {
    return phase == ui_gesture_phase_began || phase == ui_gesture_phase_changed || phase == ui_gesture_phase_ended;
}

static void canvas_on_pan(struct ui_pan_gesture gesture, void* user) {
    struct canvas_page* page = user;
    enum ui_gesture_phase phase = ui_pan_gesture_get_phase(gesture);

    if (phase == ui_gesture_phase_began) {
        page->pan_last = ui_pan_gesture_get_translation(gesture, false);
    }

    if (canvas_phase_updates(phase)) {
        double2 translation = ui_pan_gesture_get_translation(gesture, false);

        page->translation += translation - page->pan_last;
        page->pan_last = translation;
    }
}

static void canvas_on_pinch(struct ui_pinch_gesture gesture, void* user) {
    struct canvas_page* page = user;
    enum ui_gesture_phase phase = ui_pinch_gesture_get_phase(gesture);

    if (phase == ui_gesture_phase_began) {
        page->pinch_last = ui_pinch_gesture_get_scale(gesture, false);
    }

    if (canvas_phase_updates(phase)) {
        double scale = ui_pinch_gesture_get_scale(gesture, false);
        double target = clamp_double(page->scale * scale / page->pinch_last, canvas_scale_min, canvas_scale_max);
        double delta = target / page->scale;
        double2 anchor = ui_pinch_gesture_get_position(gesture);

        page->pinch_last = scale;
        page->translation = anchor + (page->translation - anchor) * delta;
        page->scale = target;
    }
}

static void canvas_on_rotate(struct ui_rotate_gesture gesture, void* user) {
    struct canvas_page* page = user;
    enum ui_gesture_phase phase = ui_rotate_gesture_get_phase(gesture);

    if (phase == ui_gesture_phase_began) {
        page->rotate_last = ui_rotate_gesture_get_rotation(gesture, false);
    }

    if (canvas_phase_updates(phase)) {
        double rotation = ui_rotate_gesture_get_rotation(gesture, false);
        double delta = rotation - page->rotate_last;
        double2 anchor = ui_rotate_gesture_get_position(gesture);

        page->rotate_last = rotation;
        page->translation = anchor + canvas_rotate(page->translation - anchor, delta);
        page->rotation += delta;
    }
}

static float3x2 canvas_transform(const struct canvas_page* page) {
    float c = (float)(cos(page->rotation) * page->scale);
    float s = (float)(sin(page->rotation) * page->scale);

    return make_float3x2(make_float2(c, s), make_float2(-s, c), to_float2_from_double2(page->translation));
}

static void* canvas_create(struct cr_mem* mem) {
    struct canvas_page* page = cr_malloc(mem, sizeof(*page), 0, cr_loc);

    memset(page, 0, sizeof(*page));
    page->mem = mem;
    page->checker[0] = canvas_checker_graphic(0.95f, 0.4f);
    page->checker[1] = canvas_checker_graphic(0.85f, 0.5f);

    return page;
}

static void canvas_destroy(void* state) {
    struct canvas_page* page = state;

    ui_graphic_destroy(page->checker[0]);
    ui_graphic_destroy(page->checker[1]);
    ui_graphic_destroy(page->dots[0]);
    ui_graphic_destroy(page->dots[1]);
    cr_free(page->mem, page, cr_loc);
}

static void canvas_frame(void* state, const struct demo_frame* frame) {
    struct canvas_page* page = state;
    const struct demo_theme* theme = frame->theme;
    double2 size = frame->size;
    double to_px = ui_get_to_px_from_pt();

    bool reset = hud_button("Canvas", "Reset");

    if (page->placed == false || reset) {
        page->translation = size * 0.5;
        page->scale = 1.0;
        page->rotation = 0.0;
        page->placed = true;
    }

    canvas_dots_sync(page, to_px);

    struct ui_node container = ui_frame(&(struct ui_frame) {
        .parent = frame->root,
        .size = ui_size_fix(size.x, size.y),
        .clip = true,
    });

    ui_frame(&(struct ui_frame) {
        .parent = container,
        .size = ui_size_fix(size.x, size.y),
        .fill.layers[0] = ui_layer_graphic_ex(page->dots[theme->is_dark], ui_address_mode_repeat, ui_filter_nearest, make_double2(1, 1), ui_white, ui_color_space_srgb, true),
    });

    double extent = canvas_cell * (double)canvas_cells;
    double2 tile = make_double2(canvas_cell, canvas_cell) * to_px;

    struct ui_node board = ui_frame(&(struct ui_frame) {
        .parent = container,
        .transform = canvas_transform(page),
    });

    ui_frame(&(struct ui_frame) {
        .parent = board,
        .pos = ui_pos_xy(-extent * 0.5, -extent * 0.5),
        .size = ui_size_fix(extent, extent),
        .fill.layers[0] = ui_layer_graphic_ex(page->checker[theme->is_dark], ui_address_mode_repeat, ui_filter_nearest, tile, ui_white, ui_color_space_srgb, true),
    });

    ui_pinch(&(struct ui_pinch) {
        .node = container,
        .on_change = canvas_on_pinch,
        .user = page,
    });
    ui_rotate(&(struct ui_rotate) {
        .node = container,
        .on_change = canvas_on_rotate,
        .user = page,
    });
    ui_pan(&(struct ui_pan) {
        .node = container,
        .on_change = canvas_on_pan,
        .user = page,
    });

    demo_title(frame, "Canvas", "pan with one or two fingers, pinch to zoom, twist to rotate");

    char readout[64];
    snprintf(readout, sizeof(readout), "%.0f%%  %.1f°", page->scale * 100.0, page->rotation * 180.0 / M_PI);

    ui_text(&(struct ui_text) {
        .parent = frame->root,
        .pos = ui_pos_xy(demo_margin, size.y - demo_margin - 16.0),
        .str = demo_str(frame->font_mono, readout, theme->fg_secondary, 13, 400),
    });
}

const struct demo_page demo_page_canvas = {
    .name = "Canvas",
    .create = canvas_create,
    .destroy = canvas_destroy,
    .frame = canvas_frame,
};
