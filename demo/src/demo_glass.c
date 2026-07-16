#include "demo.h"
#include "ui_input.h"
#include "ui_gestures.h"
#include "hud.h"
#include <math.h>
#include <string.h>

#define glass_grip_count 8
#define glass_grip_side 11.0
#define glass_grip_touch 36.0
#define glass_plus_arm 10.0
#define glass_snap_range 12.0
#define glass_slack_range 6.0
#define glass_center_epsilon 0.5
#define glass_size_min 48.0
#define glass_corner_radius 40.0
#define glass_margin 24.0
#define glass_grid_step 32u

struct glass_page;

struct glass_grip {
    struct glass_page* page;
    double2 anchor;
};

struct glass_touches {
    struct ui_pointer pointers[8];
    uint32_t count;
};

struct glass_page {
    struct cr_mem* mem;
    double2 pos;
    double2 size;
    double2 bounds;
    bool placed;
    bool selected;
    bool moving;
    bool symmetric;
    bool slack;
    bool snapped_x;
    bool snapped_y;
    struct glass_grip* grip;
    struct glass_touches* touches;
    double2 translation;
    double2 origin_pos;
    double2 origin_size;
    double2 origin_translation;
    double2 slack_translation;
    struct glass_grip grips[glass_grip_count];
    struct ui_graphic* grid[2];
};

static const double2 glass_grip_anchors[glass_grip_count] = {
    { 0.5, 0.0 }, { 0.5, 1.0 }, { 0.0, 0.5 }, { 1.0, 0.5 },
    { 0.0, 0.0 }, { 1.0, 0.0 }, { 0.0, 1.0 }, { 1.0, 1.0 },
};

static void glass_touches_remove(struct ui_gesture* gesture, struct ui_pointer pointer) {
    struct glass_touches* touches = ui_gesture_get_state(gesture);

    for (uint32_t i = 0; i < touches->count; i++) {
        if (ui_pointer_is_equal(touches->pointers[i], pointer)) {
            touches->pointers[i] = touches->pointers[--touches->count];
            return;
        }
    }
}

static bool glass_touches_simultaneous(struct ui_gesture* gesture, struct ui_gesture* other) {
    return true;
}

static enum ui_gesture_decision glass_touches_accepts(struct ui_gesture* gesture, struct app_pointer_event event, struct ui_pointer pointer) {
    struct glass_touches* touches = ui_gesture_get_state(gesture);
    bool accepts = event.type == app_pointer_event_type_touch_began && touches->count < cr_count(touches->pointers);

    return accepts ? ui_gesture_decision_accept : ui_gesture_decision_reject;
}

static void glass_touches_on_pointer(struct ui_gesture* gesture, struct app_pointer_event event, struct ui_pointer pointer) {
    struct glass_touches* touches = ui_gesture_get_state(gesture);

    if (event.type == app_pointer_event_type_touch_began) {
        touches->pointers[touches->count++] = pointer;
    } else if (event.type == app_pointer_event_type_touch_ended) {
        glass_touches_remove(gesture, pointer);
    }
}

static void glass_touches_cancel(struct ui_gesture* gesture) {
    struct glass_touches* touches = ui_gesture_get_state(gesture);

    touches->count = 0;
}

static const struct ui_gesture_desc glass_touches_desc = {
    .name = "glass touches",
    .state_size = sizeof(struct glass_touches),
    .simultaneous = glass_touches_simultaneous,
    .accepts = glass_touches_accepts,
    .on_pointer = glass_touches_on_pointer,
    .cancel = glass_touches_cancel,
    .remove_pointer = glass_touches_remove,
};

static bool glass_accepts_tap(const struct glass_page* page) {
    return page->grip == NULL && page->moving == false && page->touches->count == 1;
}

static void glass_on_select(struct ui_tap_gesture gesture, void* user) {
    struct glass_page* page = user;

    if (glass_accepts_tap(page)) {
        page->selected = true;
    }
}

static void glass_on_deselect(struct ui_tap_gesture gesture, void* user) {
    struct glass_page* page = user;

    if (glass_accepts_tap(page)) {
        page->selected = false;
    }
}

static double glass_snap(double value, double target, bool* out_snapped) {
    *out_snapped = fabs(value - target) < glass_snap_range;
    return *out_snapped ? target : value;
}

static void glass_on_move(struct ui_pan_gesture gesture, void* user) {
    struct glass_page* page = user;
    enum ui_gesture_phase phase = ui_pan_gesture_get_phase(gesture);
    double2 translation = ui_pan_gesture_get_translation(gesture, false);

    switch (phase) {
    case ui_gesture_phase_began: {
        if (page->selected == false || page->grip != NULL) {
            return;
        }
        page->moving = true;
        page->origin_pos = page->pos;
        page->origin_translation = translation;
    } break;
    case ui_gesture_phase_changed: {
        if (page->moving == false) {
            return;
        }
        double2 center = page->origin_pos + (translation - page->origin_translation) + page->size * 0.5;
        double2 target = page->bounds * 0.5;

        center.x = glass_snap(center.x, target.x, &page->snapped_x);
        center.y = glass_snap(center.y, target.y, &page->snapped_y);

        page->pos = center - page->size * 0.5;
    } break;
    case ui_gesture_phase_ended:
    case ui_gesture_phase_canceled:
    case ui_gesture_phase_failed: {
        page->moving = false;
        page->snapped_x = false;
        page->snapped_y = false;
    } break;
    default:
        break;
    }
}

static double glass_snap_px(double value, double to_px) {
    return round(value * to_px) / to_px;
}

static double2 glass_resize_axis(double lo, double hi, double anchor, double delta, double span_max, bool symmetric, double to_px) {
    if (anchor == 0.5) {
        return make_double2(lo, hi);
    }

    if (symmetric) {
        double grow = anchor == 0.0 ? -delta : delta;
        double span = clamp_double(hi - lo + grow * 2.0, glass_size_min, span_max);
        double center = glass_snap_px((lo + hi) * 0.5, to_px);
        double reach = glass_snap_px(span * 0.5, to_px);

        return make_double2(center - reach, center + reach);
    }

    if (anchor == 0.0) {
        lo = clamp_double(lo + delta, hi - span_max, hi - glass_size_min);
    } else {
        hi = clamp_double(hi + delta, lo + glass_size_min, lo + span_max);
    }

    return make_double2(glass_snap_px(lo, to_px), glass_snap_px(hi, to_px));
}

static void glass_resize_rebase(struct glass_page* page) {
    page->origin_pos = page->pos;
    page->origin_size = page->size;
    page->origin_translation = page->translation;
}

static void glass_resize_apply(struct glass_page* page) {
    double2 delta = page->translation - page->origin_translation;
    double2 lo = page->origin_pos;
    double2 hi = page->origin_pos + page->origin_size;
    double2 span_max = max_double2(page->bounds, make_double2(glass_size_min, glass_size_min));
    double2 anchor = page->grip->anchor;
    double to_px = ui_get_to_px_from_pt();

    double2 x = glass_resize_axis(lo.x, hi.x, anchor.x, delta.x, span_max.x, page->symmetric, to_px);
    double2 y = glass_resize_axis(lo.y, hi.y, anchor.y, delta.y, span_max.y, page->symmetric, to_px);

    page->pos = make_double2(x.x, y.x);
    page->size = make_double2(x.y - x.x, y.y - y.x);
}

static void glass_on_resize(struct ui_pan_gesture gesture, void* user) {
    struct glass_grip* grip = user;
    struct glass_page* page = grip->page;
    enum ui_gesture_phase phase = ui_pan_gesture_get_phase(gesture);

    if (phase == ui_gesture_phase_began) {
        if (page->grip != NULL || page->moving) {
            return;
        }
        page->grip = grip;
        page->translation = ui_pan_gesture_get_translation(gesture, false);
        page->symmetric = page->touches->count > 1;
        page->slack = false;
        glass_resize_rebase(page);
        return;
    }

    if (page->grip != grip) {
        return;
    }

    if (phase != ui_gesture_phase_changed) {
        page->grip = NULL;
        page->slack = false;
        return;
    }

    page->translation = ui_pan_gesture_get_translation(gesture, false);

    bool spare = page->touches->count > 1;

    if (page->slack) {
        if (spare) {
            page->slack = false;
            page->symmetric = true;
            glass_resize_rebase(page);
        } else if (length_double2(page->translation - page->slack_translation) < glass_slack_range) {
            return;
        } else {
            page->slack = false;
            page->symmetric = false;
            glass_resize_rebase(page);
        }
    } else if (spare != page->symmetric) {
        glass_resize_apply(page);

        if (spare) {
            page->symmetric = true;
            glass_resize_rebase(page);
        } else {
            page->slack = true;
            page->slack_translation = page->translation;
        }
        return;
    }

    glass_resize_apply(page);
}

static double glass_grip_origin(double lo, double hi, double anchor, double reach) {
    if (anchor == 0.0) {
        return lo - 0.5 - reach;
    }

    if (anchor == 1.0) {
        return hi + 0.5 - reach;
    }

    return round((lo + hi) * 0.5 - reach);
}

static void glass_line(struct ui_node parent, double2 lo, double2 hi, double to_pt, struct ui_color color) {
    double2 pos = lo * to_pt;
    double2 size = (hi - lo) * to_pt;

    ui_frame(&(struct ui_frame) {
        .parent = parent,
        .pos = ui_pos_xy(pos.x, pos.y),
        .size = ui_size_fix(size.x, size.y),
        .fill = ui_fill_solid(color),
    });
}

static void glass_controls(struct ui_node canvas, struct glass_page* page, struct ui_color color) {
    double to_px = ui_get_to_px_from_pt();
    double to_pt = 1.0 / to_px;
    double line = 1.0;
    double side = glass_grip_side;
    double reach = side * 0.5;
    double touch_reach = max_double(reach, round(glass_grip_touch * to_px * 0.5));

    double2 lo = round_double2(page->pos * to_px);
    double2 hi = lo + round_double2(page->size * to_px);

    bool pinned = page->grip != NULL && page->symmetric;
    double2 offset = page->pos + page->size * 0.5 - page->bounds * 0.5;
    bool plus_x = page->snapped_x || (pinned && fabs(offset.x) < glass_center_epsilon);
    bool plus_y = page->snapped_y || (pinned && fabs(offset.y) < glass_center_epsilon);

    if (plus_x || plus_y) {
        double2 c = round_double2(page->bounds * 0.5 * to_px);
        double arm = glass_plus_arm;

        if (plus_x) {
            glass_line(canvas, make_double2(c.x, c.y - arm), make_double2(c.x + line, c.y + line + arm), to_pt, color);
        }

        if (plus_y) {
            glass_line(canvas, make_double2(c.x - arm, c.y), make_double2(c.x + line + arm, c.y + line), to_pt, color);
        }
    }

    double rows[2] = { lo.y - line, hi.y };
    double cols[2] = { lo.x - line, hi.x };
    double stops_x[3] = {
        glass_grip_origin(lo.x, hi.x, 0.0, reach),
        glass_grip_origin(lo.x, hi.x, 0.5, reach),
        glass_grip_origin(lo.x, hi.x, 1.0, reach),
    };
    double stops_y[3] = {
        glass_grip_origin(lo.y, hi.y, 0.0, reach),
        glass_grip_origin(lo.y, hi.y, 0.5, reach),
        glass_grip_origin(lo.y, hi.y, 1.0, reach),
    };

    for (uint32_t i = 0; i < 2; i++) {
        for (uint32_t j = 0; j < 2; j++) {
            glass_line(canvas, make_double2(stops_x[j] + side, rows[i]), make_double2(stops_x[j + 1], rows[i] + line), to_pt, color);
            glass_line(canvas, make_double2(cols[i], stops_y[j] + side), make_double2(cols[i] + line, stops_y[j + 1]), to_pt, color);
        }
    }

    for (uint32_t i = 0; i < glass_grip_count; i++) {
        struct glass_grip* grip = &page->grips[i];
        double2 origin = make_double2(
            glass_grip_origin(lo.x, hi.x, grip->anchor.x, reach),
            glass_grip_origin(lo.y, hi.y, grip->anchor.y, reach));
        double2 origin_pt = origin * to_pt;

        ui_frame(&(struct ui_frame) {
            .parent = canvas,
            .pos = ui_pos_xy(origin_pt.x, origin_pt.y),
            .size = ui_size_fix(side * to_pt, side * to_pt),
            .outline = ui_outline_solid(ui_outline_align_inside, line * to_pt, color),
        });

        double2 touch = round_double2(origin + reach - touch_reach) * to_pt;

        struct ui_node node = ui_frame(&(struct ui_frame) {
            .parent = canvas,
            .key = ui_key_u64(i),
            .pos = ui_pos_xy(touch.x, touch.y),
            .size = ui_size_fix(touch_reach * 2.0 * to_pt, touch_reach * 2.0 * to_pt),
        });

        ui_tap(&(struct ui_tap) {
            .node = node,
            .on_change = glass_on_select,
            .user = page,
        });
        ui_pan(&(struct ui_pan) {
            .node = node,
            .on_change = glass_on_resize,
            .user = grip,
            .min_touches = 1,
            .max_touches = 1,
        });
    }
}

static struct ui_graphic* glass_grid_graphic(float gray) {
    uint8_t value = (uint8_t)lroundf(gray * 255.0f);
    uint8_t pixels[glass_grid_step * glass_grid_step * 4] = { 0 };

    for (uint32_t y = 0; y < glass_grid_step; y++) {
        for (uint32_t x = 0; x < glass_grid_step; x++) {
            bool on_line = x == glass_grid_step - 1 || x == 0 || y == glass_grid_step - 1 || y == 0;

            if (on_line) {
                uint8_t* pixel = &pixels[(y * glass_grid_step + x) * 4];

                pixel[0] = value;
                pixel[1] = value;
                pixel[2] = value;
                pixel[3] = 255;
            }
        }
    }

    return ui_graphic_create(glass_grid_step, glass_grid_step, ui_pixel_format_bgra8, pixels, glass_grid_step * 4, true);
}

static void* glass_create(struct cr_mem* mem) {
    struct glass_page* page = cr_malloc(mem, sizeof(*page), 0, cr_loc);

    memset(page, 0, sizeof(*page));
    page->mem = mem;

    for (uint32_t i = 0; i < glass_grip_count; i++) {
        page->grips[i] = (struct glass_grip) { .page = page, .anchor = glass_grip_anchors[i] };
    }

    page->grid[0] = glass_grid_graphic(0.95f);
    page->grid[1] = glass_grid_graphic(0.25f);

    return page;
}

static void glass_destroy(void* state) {
    struct glass_page* page = state;

    ui_graphic_destroy(page->grid[0]);
    ui_graphic_destroy(page->grid[1]);
    cr_free(page->mem, page, cr_loc);
}

static void glass_frame(void* state, const struct demo_frame* frame) {
    struct glass_page* page = state;
    const struct demo_theme* theme = frame->theme;

    static const char* const shape_labels[] = { "Rounded", "Circle" };
    static const char* const variant_labels[] = { "Clear", "Regular" };

    uint32_t shape = hud_select_labels("Shape", cr_count(shape_labels), 0, shape_labels);
    uint32_t variant = hud_select_labels("Variant", cr_count(variant_labels), 1, variant_labels);
    bool tinted = hud_toggle("Tint", false);
    bool reset = hud_button("Transform", "Reset");

    double2 size = frame->size;

    ui_frame(&(struct ui_frame) {
        .parent = frame->root,
        .size = ui_size_fix(size.x, size.y),
        .fill.layers[0] = ui_layer_graphic_ex(page->grid[theme->is_dark], ui_address_mode_repeat, ui_filter_nearest, make_double2(1, 1), ui_white, ui_color_space_srgb, true),
    });

    demo_title(frame, "Glass", "tap the shape to select it, drag to move, pull a handle to resize");

    page->bounds = size;

    if (page->placed == false || reset) {
        page->size = shape == 1 ? make_double2(220, 220) : make_double2(308, 220);
        page->pos = (size - page->size) * 0.5;
        page->placed = true;
    }

    double2 size_min = make_double2(glass_size_min, glass_size_min);
    double2 margin = make_double2(glass_margin, glass_margin);

    page->size = min_double2(max_double2(page->size, size_min), max_double2(size, size_min));
    page->pos = max_double2(margin - page->size * 0.5, min_double2(page->pos, size - page->size * 0.5 - margin));

    double corner = min_double(page->size.x, page->size.y) * 0.5;
    struct ui_corners corners = shape == 1 ? ui_corners_circular(corner) : ui_corners(min_double(glass_corner_radius, corner));

    struct ui_layer glass = ui_layer_glass((enum ui_glass_variant)variant, theme->glass);

    if (tinted) {
        glass.glass.tint = theme->accent;
        glass.glass.tint.value.w = 0.45f;
    }

    struct ui_node canvas = ui_frame(&(struct ui_frame) {
        .parent = frame->root,
        .size = ui_size_fix(size.x, size.y),
    });

    ui_tap(&(struct ui_tap) {
        .node = canvas,
        .on_change = glass_on_deselect,
        .user = page,
    });
    ui_pan(&(struct ui_pan) {
        .node = canvas,
        .on_change = glass_on_move,
        .user = page,
        .min_touches = 1,
        .max_touches = 1,
    });
    struct ui_gesture_desc touches = glass_touches_desc;
    touches.node = canvas;
    page->touches = ui_gesture_get_state(ui_gesture(&touches));

    struct ui_node shape_node = ui_frame(&(struct ui_frame) {
        .parent = canvas,
        .pos = ui_pos_xy(page->pos.x, page->pos.y),
        .size = ui_size_fix(page->size.x, page->size.y),
        .corners = corners,
        .fill.layers[0] = glass,
    });

    ui_tap(&(struct ui_tap) {
        .node = shape_node,
        .on_change = glass_on_select,
        .user = page,
    });

    if (page->selected) {
        glass_controls(canvas, page, theme->is_dark ? ui_gray70 : ui_gray30);
    }
}

const struct demo_page demo_page_glass = {
    .name = "Glass",
    .create = glass_create,
    .destroy = glass_destroy,
    .frame = glass_frame,
};
