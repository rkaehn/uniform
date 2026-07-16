#include "cr.h"
#include "ui.h"
#include "ui_input.h"
#include <math.h>
#include <string.h>
#include "ui_gestures.h"

#define ui_touches_max 4
#define ui_touches_baseline_min 16.0

#define ui_tap_gap 0.3
#define ui_long_press_duration 0.3
#define ui_hold_duration_default 0.15

struct ui_touches {
    struct ui_pointer pointers[ui_touches_max];
    double began[ui_touches_max];
    uint32_t count;
    double2 translation_accum[2];
    double scale_accum[2];
    double rotation_accum[2];
    double2 centroid_last[2];
    double span_last[2];
    double angle_last[ui_touches_max][2];
    bool angle_valid[ui_touches_max][2];
};

struct ui_tap_state {
    struct ui_pointer pointers[ui_touches_max];
    double began[ui_touches_max];
    uint32_t count;
    double2 position;
    uint32_t touch_count;
    uint32_t tap_count;
    uint32_t taps_done;
    bool complete;
    double last_tap_timestamp;
    double hold;
    void (*on_change)(struct ui_tap_gesture gesture, void* user);
    void* user;
};

struct ui_long_press_state {
    struct ui_pointer pointer;
    double duration;
    double began_timestamp;
    bool direct_only;
    double hold;
    void (*on_change)(struct ui_long_press_gesture gesture, void* user);
    void* user;
};

struct ui_pan_state {
    struct ui_touches touches;
    enum ui_pan_direction direction;
    uint32_t min_touches;
    uint32_t max_touches;
    bool direct_only;
    double timestamp;
    double hold;
    void (*on_change)(struct ui_pan_gesture gesture, void* user);
    void* user;
};

struct ui_hover_state {
    double2 position;
    bool has_hover;
    void (*on_change)(struct ui_hover_gesture gesture, void* user);
    void* user;
};

struct ui_pinch_state {
    struct ui_touches touches;
    double hold;
    void (*on_change)(struct ui_pinch_gesture gesture, void* user);
    void* user;
};

struct ui_rotate_state {
    struct ui_touches touches;
    double hold;
    void (*on_change)(struct ui_rotate_gesture gesture, void* user);
    void* user;
};

static void* ui_gesture_lookup_state(uint64_t key) {
    struct ui_gesture* gesture = ui_input_get_gesture(key);
    return gesture ? ui_gesture_get_state(gesture) : NULL;
}

static enum ui_gesture_phase ui_gesture_lookup_phase(uint64_t key) {
    struct ui_gesture* gesture = ui_input_get_gesture(key);
    return gesture ? ui_gesture_get_phase(gesture) : ui_gesture_phase_possible;
}

static void ui_gesture_lookup_claim(uint64_t key) {
    struct ui_gesture* gesture = ui_input_get_gesture(key);
    if (gesture) {
        ui_gesture_claim(gesture);
    }
}

static bool ui_gesture_is_active(struct ui_gesture* gesture) {
    enum ui_gesture_phase phase = ui_gesture_get_phase(gesture);
    return phase == ui_gesture_phase_began || phase == ui_gesture_phase_changed;
}

static enum ui_gesture_decision ui_hold_decision(double hold) {
    return hold > 0.0 ? ui_gesture_decision_hold : ui_gesture_decision_accept;
}

static void ui_hold_update(struct ui_gesture* gesture, const struct ui_pointer* pointers, const double* began, uint32_t count, double hold, double timestamp) {
    for (uint32_t i = 0; hold > 0.0 && i < count; i++) {
        if (timestamp - began[i] >= hold) {
            ui_gesture_release(gesture, pointers[i]);
        }
    }
}

static bool ui_touches_simultaneous(struct ui_gesture* gesture, struct ui_gesture* other) {
    const char* name = ui_gesture_get_name(other);
    return strcmp(name, "pinch") == 0 || strcmp(name, "rotate") == 0 || strcmp(name, "pan") == 0;
}

static enum ui_gesture_decision ui_tap_accepts(struct ui_gesture* gesture, struct app_pointer_event event, struct ui_pointer pointer);
static void ui_tap_on_pointer(struct ui_gesture* gesture, struct app_pointer_event event, struct ui_pointer pointer);
static void ui_tap_on_update(struct ui_gesture* gesture, double timestamp);
static void ui_tap_cancel(struct ui_gesture* gesture);
static void ui_tap_remove_pointer(struct ui_gesture* gesture, struct ui_pointer pointer);
static void ui_tap_reset(struct ui_gesture* gesture);

static const struct ui_gesture_desc ui_tap_desc = {
    .name = "tap",
    .state_size = sizeof(struct ui_tap_state),
    .reset = ui_tap_reset,
    .accepts = ui_tap_accepts,
    .on_pointer = ui_tap_on_pointer,
    .on_update = ui_tap_on_update,
    .cancel = ui_tap_cancel,
    .remove_pointer = ui_tap_remove_pointer,
};

static enum ui_gesture_decision ui_long_press_accepts(struct ui_gesture* gesture, struct app_pointer_event event, struct ui_pointer pointer);
static void ui_long_press_on_pointer(struct ui_gesture* gesture, struct app_pointer_event event, struct ui_pointer pointer);
static void ui_long_press_on_update(struct ui_gesture* gesture, double timestamp);
static void ui_long_press_cancel(struct ui_gesture* gesture);
static void ui_long_press_remove_pointer(struct ui_gesture* gesture, struct ui_pointer pointer);

static const struct ui_gesture_desc ui_long_press_desc = {
    .name = "long press",
    .state_size = sizeof(struct ui_long_press_state),
    .accepts = ui_long_press_accepts,
    .on_pointer = ui_long_press_on_pointer,
    .on_update = ui_long_press_on_update,
    .cancel = ui_long_press_cancel,
    .remove_pointer = ui_long_press_remove_pointer,
};

static enum ui_gesture_decision ui_pan_accepts(struct ui_gesture* gesture, struct app_pointer_event event, struct ui_pointer pointer);
static void ui_pan_on_pointer(struct ui_gesture* gesture, struct app_pointer_event event, struct ui_pointer pointer);
static void ui_pan_on_update(struct ui_gesture* gesture, double timestamp);
static void ui_pan_cancel(struct ui_gesture* gesture);
static void ui_pan_remove_pointer(struct ui_gesture* gesture, struct ui_pointer pointer);

static const struct ui_gesture_desc ui_pan_desc = {
    .name = "pan",
    .state_size = sizeof(struct ui_pan_state),
    .accepts = ui_pan_accepts,
    .on_pointer = ui_pan_on_pointer,
    .on_update = ui_pan_on_update,
    .cancel = ui_pan_cancel,
    .remove_pointer = ui_pan_remove_pointer,
};

static enum ui_gesture_decision ui_hover_accepts(struct ui_gesture* gesture, struct app_pointer_event event, struct ui_pointer pointer);
static void ui_hover_on_pointer(struct ui_gesture* gesture, struct app_pointer_event event, struct ui_pointer pointer);
static void ui_hover_cancel(struct ui_gesture* gesture);

static const struct ui_gesture_desc ui_hover_desc = {
    .name = "hover",
    .state_size = sizeof(struct ui_hover_state),
    .accepts = ui_hover_accepts,
    .on_pointer = ui_hover_on_pointer,
    .cancel = ui_hover_cancel,
};

static enum ui_gesture_decision ui_pinch_accepts(struct ui_gesture* gesture, struct app_pointer_event event, struct ui_pointer pointer);
static void ui_pinch_on_pointer(struct ui_gesture* gesture, struct app_pointer_event event, struct ui_pointer pointer);
static void ui_pinch_on_update(struct ui_gesture* gesture, double timestamp);
static void ui_pinch_cancel(struct ui_gesture* gesture);
static void ui_pinch_remove_pointer(struct ui_gesture* gesture, struct ui_pointer pointer);

static const struct ui_gesture_desc ui_pinch_desc = {
    .name = "pinch",
    .state_size = sizeof(struct ui_pinch_state),
    .simultaneous = ui_touches_simultaneous,
    .accepts = ui_pinch_accepts,
    .on_pointer = ui_pinch_on_pointer,
    .on_update = ui_pinch_on_update,
    .cancel = ui_pinch_cancel,
    .remove_pointer = ui_pinch_remove_pointer,
};

static enum ui_gesture_decision ui_rotate_accepts(struct ui_gesture* gesture, struct app_pointer_event event, struct ui_pointer pointer);
static void ui_rotate_on_pointer(struct ui_gesture* gesture, struct app_pointer_event event, struct ui_pointer pointer);
static void ui_rotate_on_update(struct ui_gesture* gesture, double timestamp);
static void ui_rotate_cancel(struct ui_gesture* gesture);
static void ui_rotate_remove_pointer(struct ui_gesture* gesture, struct ui_pointer pointer);

static const struct ui_gesture_desc ui_rotate_desc = {
    .name = "rotate",
    .state_size = sizeof(struct ui_rotate_state),
    .simultaneous = ui_touches_simultaneous,
    .accepts = ui_rotate_accepts,
    .on_pointer = ui_rotate_on_pointer,
    .on_update = ui_rotate_on_update,
    .cancel = ui_rotate_cancel,
    .remove_pointer = ui_rotate_remove_pointer,
};

static bool ui_is_stationary(double2 delta) {
    return delta.x * delta.x + delta.y * delta.y < 100.0;
}

static bool ui_is_direction_x(double2 delta) {
    double2 sq = delta * delta;
    return sq.x > sq.y;
}

static double ui_hold_duration(bool holds_touches, double duration) {
    if (holds_touches == false) {
        return 0.0;
    }
    return duration == 0.0 ? ui_hold_duration_default : duration;
}

static double2 ui_touches_pointer_pos(struct ui_pointer pointer, bool map_direct) {
    double2 position = ui_input_pointer_get_position(pointer);

    if (map_direct) {
        return position;
    }

    return position - ui_input_pointer_get_translation(pointer, true) + ui_input_pointer_get_translation(pointer, false);
}

static int32_t ui_touches_index(const struct ui_touches* touches, struct ui_pointer pointer) {
    for (uint32_t i = 0; i < touches->count; i++) {
        if (ui_pointer_is_equal(touches->pointers[i], pointer)) {
            return (int32_t)i;
        }
    }

    return -1;
}

static double2 ui_touches_centroid(const struct ui_touches* touches, bool map_direct) {
    double2 sum = 0.0;

    for (uint32_t i = 0; i < touches->count; i++) {
        sum += ui_touches_pointer_pos(touches->pointers[i], map_direct);
    }

    return sum / (double)touches->count;
}

static double ui_touches_span(const struct ui_touches* touches, bool map_direct) {
    double2 centroid = ui_touches_centroid(touches, map_direct);
    double sum = 0.0;

    for (uint32_t i = 0; i < touches->count; i++) {
        double2 v = ui_touches_pointer_pos(touches->pointers[i], map_direct) - centroid;
        sum += sqrt(v.x * v.x + v.y * v.y);
    }

    return sum / (double)touches->count;
}

static void ui_touches_sync(struct ui_touches* touches) {
    if (touches->count == 0) {
        return;
    }

    for (uint32_t m = 0; m < 2; m++) {
        double2 centroid = ui_touches_centroid(touches, m == 1);

        touches->centroid_last[m] = centroid;
        touches->span_last[m] = ui_touches_span(touches, m == 1);

        for (uint32_t i = 0; i < touches->count; i++) {
            double2 v = ui_touches_pointer_pos(touches->pointers[i], m == 1) - centroid;
            touches->angle_last[i][m] = atan2(v.y, v.x);
            touches->angle_valid[i][m] = v.x * v.x + v.y * v.y >= ui_touches_baseline_min * ui_touches_baseline_min;
        }
    }
}

static void ui_touches_rebase(struct ui_touches* touches) {
    for (uint32_t m = 0; m < 2; m++) {
        touches->translation_accum[m] = 0.0;
        touches->scale_accum[m] = 1.0;
        touches->rotation_accum[m] = 0.0;
    }

    ui_touches_sync(touches);
}

static bool ui_touches_add(struct ui_touches* touches, struct ui_pointer pointer, double timestamp) {
    if (touches->count == ui_touches_max) {
        return false;
    }

    touches->began[touches->count] = timestamp;
    touches->pointers[touches->count++] = pointer;
    ui_touches_sync(touches);

    return true;
}

static void ui_touches_remove(struct ui_touches* touches, uint32_t index) {
    for (uint32_t i = index + 1; i < touches->count; i++) {
        touches->pointers[i - 1] = touches->pointers[i];
        touches->began[i - 1] = touches->began[i];
    }

    touches->count--;
    ui_touches_sync(touches);
}

static void ui_touches_clear(struct ui_touches* touches) {
    touches->count = 0;
}

static void ui_touches_step(struct ui_touches* touches) {
    if (touches->count == 0) {
        return;
    }

    for (uint32_t m = 0; m < 2; m++) {
        double2 centroid = ui_touches_centroid(touches, m == 1);

        touches->translation_accum[m] += centroid - touches->centroid_last[m];
        touches->centroid_last[m] = centroid;

        if (touches->count >= 2) {
            double span = ui_touches_span(touches, m == 1);

            if (touches->span_last[m] >= ui_touches_baseline_min && span >= ui_touches_baseline_min) {
                touches->scale_accum[m] *= span / touches->span_last[m];
            }
            touches->span_last[m] = span;

            double sum = 0.0;
            double valid_count = 0.0;
            for (uint32_t i = 0; i < touches->count; i++) {
                double2 v = ui_touches_pointer_pos(touches->pointers[i], m == 1) - centroid;
                double angle = atan2(v.y, v.x);
                bool valid = v.x * v.x + v.y * v.y >= ui_touches_baseline_min * ui_touches_baseline_min;

                if (valid && touches->angle_valid[i][m]) {
                    sum += remainder(angle - touches->angle_last[i][m], 2.0 * M_PI);
                    valid_count += 1.0;
                }

                touches->angle_last[i][m] = angle;
                touches->angle_valid[i][m] = valid;
            }
            if (valid_count > 0.0) {
                touches->rotation_accum[m] += sum / valid_count;
            }
        }
    }
}

static double2 ui_touches_position(const struct ui_touches* touches) {
    return ui_touches_centroid(touches, true);
}

static double2 ui_touches_velocity(const struct ui_touches* touches) {
    double2 sum = 0.0;

    for (uint32_t i = 0; i < touches->count; i++) {
        sum += ui_input_pointer_get_velocity(touches->pointers[i]);
    }

    return sum / (double)touches->count;
}

static bool ui_touches_moving(const struct ui_touches* touches) {
    for (uint32_t i = 0; i < touches->count; i++) {
        if (ui_input_pointer_is_moving(touches->pointers[i])) {
            return true;
        }
    }

    return false;
}

struct ui_tap_gesture ui_tap(const struct ui_tap* tap) {
    struct ui_gesture_desc desc = ui_tap_desc;
    desc.node = tap->node;
    desc.key = tap->key;

    struct ui_gesture* gesture = ui_gesture(&desc);
    struct ui_tap_state* backing = ui_gesture_get_state(gesture);

    backing->on_change = tap->on_change;
    backing->user = tap->user;
    backing->touch_count = tap->touch_count == 0 ? 1 : min_uint32(tap->touch_count, ui_touches_max);
    backing->tap_count = tap->tap_count == 0 ? 1 : tap->tap_count;
    backing->hold = ui_hold_duration(tap->holds_touches, tap->hold_duration);

    return (struct ui_tap_gesture) { .raw = ui_gesture_get_key(gesture) };
}

static int32_t ui_tap_index(const struct ui_tap_state* backing, struct ui_pointer pointer) {
    for (uint32_t i = 0; i < backing->count; i++) {
        if (ui_pointer_is_equal(backing->pointers[i], pointer)) {
            return (int32_t)i;
        }
    }

    return -1;
}

static double2 ui_tap_centroid(struct ui_gesture* gesture) {
    struct ui_tap_state* backing = ui_gesture_get_state(gesture);
    double2 sum = 0.0;

    for (uint32_t i = 0; i < backing->count; i++) {
        sum += ui_input_pointer_get_position(backing->pointers[i]);
    }

    return sum / (double)backing->count;
}

static void ui_tap_notify(struct ui_gesture* gesture) {
    struct ui_tap_state* backing = ui_gesture_get_state(gesture);

    if (backing->on_change) {
        backing->on_change((struct ui_tap_gesture) { .raw = ui_gesture_get_key(gesture) }, backing->user);
    }
}

static void ui_tap_clear(struct ui_tap_state* backing) {
    backing->count = 0;
    backing->taps_done = 0;
    backing->complete = false;
}

static enum ui_gesture_decision ui_tap_accepts(struct ui_gesture* gesture, struct app_pointer_event event, struct ui_pointer pointer) {
    struct ui_tap_state* backing = ui_gesture_get_state(gesture);

    if (event.type != app_pointer_event_type_touch_began || ui_gesture_get_phase(gesture) == ui_gesture_phase_failed || backing->count >= backing->touch_count) {
        return ui_gesture_decision_reject;
    }

    return ui_hold_decision(backing->hold);
}

static void ui_tap_on_pointer(struct ui_gesture* gesture, struct app_pointer_event event, struct ui_pointer pointer) {
    struct ui_tap_state* backing = ui_gesture_get_state(gesture);
    int32_t index = ui_tap_index(backing, pointer);

    if (event.type == app_pointer_event_type_touch_began && index == -1 && backing->count < backing->touch_count) {
        backing->began[backing->count] = event.timestamp;
        backing->pointers[backing->count++] = pointer;
        index = (int32_t)(backing->count - 1);
    }

    if (index == -1) {
        return;
    }

    backing->position = ui_tap_centroid(gesture);

    switch (event.type) {
    case app_pointer_event_type_touch_began: {
        if (backing->count == 1 && backing->taps_done > 0 && event.timestamp - backing->last_tap_timestamp > ui_tap_gap) {
            backing->taps_done = 0;
        }
        if (backing->count == backing->touch_count) {
            backing->complete = true;
        }
        ui_gesture_set_phase(gesture, ui_gesture_phase_possible);
    } break;
    case app_pointer_event_type_touch_moved: {
        if (ui_gesture_get_phase(gesture) == ui_gesture_phase_possible) {
            if (!ui_is_stationary(ui_input_pointer_get_translation(pointer, true))) {
                ui_gesture_set_phase(gesture, ui_gesture_phase_failed);
                ui_tap_clear(backing);
            }
        }
    } break;
    case app_pointer_event_type_touch_ended: {
        for (uint32_t i = (uint32_t)index + 1; i < backing->count; i++) {
            backing->pointers[i - 1] = backing->pointers[i];
        }
        backing->count--;

        if (ui_gesture_get_phase(gesture) != ui_gesture_phase_possible || backing->count > 0) {
            break;
        }

        if (backing->complete) {
            backing->complete = false;
            backing->taps_done++;
            backing->last_tap_timestamp = event.timestamp;

            if (backing->taps_done >= backing->tap_count) {
                backing->taps_done = 0;
                ui_gesture_set_phase(gesture, ui_gesture_phase_recognized);
                ui_tap_notify(gesture);
            }
        } else {
            backing->taps_done = 0;
            ui_gesture_set_phase(gesture, ui_gesture_phase_failed);
        }
    } break;
    default:
        assert(0);
    }
}

static void ui_tap_on_update(struct ui_gesture* gesture, double timestamp) {
    struct ui_tap_state* backing = ui_gesture_get_state(gesture);

    ui_hold_update(gesture, backing->pointers, backing->began, backing->count, backing->hold, timestamp);

    if (backing->count == 0 && backing->taps_done > 0 && timestamp - backing->last_tap_timestamp > ui_tap_gap) {
        backing->taps_done = 0;
        ui_gesture_set_phase(gesture, ui_gesture_phase_failed);
    }
}

static void ui_tap_cancel(struct ui_gesture* gesture) {
    struct ui_tap_state* backing = ui_gesture_get_state(gesture);

    ui_tap_clear(backing);
}

static void ui_tap_reset(struct ui_gesture* gesture) {
    struct ui_tap_state* backing = ui_gesture_get_state(gesture);

    backing->count = 0;
    backing->complete = false;
}

static void ui_tap_remove_pointer(struct ui_gesture* gesture, struct ui_pointer pointer) {
    struct ui_tap_state* backing = ui_gesture_get_state(gesture);

    if (ui_tap_index(backing, pointer) != -1) {
        ui_gesture_set_phase(gesture, ui_gesture_phase_failed);
        ui_tap_clear(backing);
    }
}

enum ui_gesture_phase ui_tap_gesture_get_phase(struct ui_tap_gesture gesture) {
    return ui_gesture_lookup_phase(gesture.raw);
}

double2 ui_tap_gesture_get_position(struct ui_tap_gesture gesture) {
    struct ui_tap_state* backing = ui_gesture_lookup_state(gesture.raw);
    return backing ? backing->position : 0.0;
}

uint32_t ui_tap_gesture_get_touch_count(struct ui_tap_gesture gesture) {
    struct ui_tap_state* backing = ui_gesture_lookup_state(gesture.raw);
    return backing ? backing->count : 0;
}

bool ui_tap_gesture_has_touch(struct ui_tap_gesture gesture) {
    return ui_tap_gesture_get_touch_count(gesture) > 0;
}

void ui_tap_gesture_claim(struct ui_tap_gesture gesture) {
    ui_gesture_lookup_claim(gesture.raw);
}

struct ui_long_press_gesture ui_long_press(const struct ui_long_press* long_press) {
    struct ui_gesture_desc desc = ui_long_press_desc;
    desc.node = long_press->node;
    desc.key = long_press->key;

    struct ui_gesture* gesture = ui_gesture(&desc);
    struct ui_long_press_state* backing = ui_gesture_get_state(gesture);

    backing->on_change = long_press->on_change;
    backing->user = long_press->user;
    backing->duration = long_press->duration == 0.0 ? ui_long_press_duration : long_press->duration;
    backing->direct_only = long_press->direct_only;
    backing->hold = ui_hold_duration(long_press->holds_touches, long_press->hold_duration);

    return (struct ui_long_press_gesture) { .raw = ui_gesture_get_key(gesture) };
}

static void ui_long_press_notify(struct ui_gesture* gesture) {
    struct ui_long_press_state* backing = ui_gesture_get_state(gesture);

    if (backing->on_change) {
        backing->on_change((struct ui_long_press_gesture) { .raw = ui_gesture_get_key(gesture) }, backing->user);
    }
}

static enum ui_gesture_decision ui_long_press_accepts(struct ui_gesture* gesture, struct app_pointer_event event, struct ui_pointer pointer) {
    struct ui_long_press_state* backing = ui_gesture_get_state(gesture);

    if (event.type != app_pointer_event_type_touch_began || ui_gesture_get_phase(gesture) == ui_gesture_phase_failed) {
        return ui_gesture_decision_reject;
    }

    if ((backing->direct_only && event.source != app_pointer_source_direct) || ui_pointer_is_null(backing->pointer) == false) {
        return ui_gesture_decision_reject;
    }

    return ui_hold_decision(backing->hold);
}

static void ui_long_press_on_pointer(struct ui_gesture* gesture, struct app_pointer_event event, struct ui_pointer pointer) {
    struct ui_long_press_state* backing = ui_gesture_get_state(gesture);

    if (event.type == app_pointer_event_type_touch_began && ui_pointer_is_null(backing->pointer)) {
        backing->pointer = pointer;
    }

    if (!ui_pointer_is_equal(backing->pointer, pointer)) {
        return;
    }

    switch (event.type) {
    case app_pointer_event_type_touch_began: {
        ui_gesture_set_phase(gesture, ui_gesture_phase_possible);
        backing->began_timestamp = event.timestamp;
    } break;
    case app_pointer_event_type_touch_moved: {
        if (ui_gesture_get_phase(gesture) == ui_gesture_phase_possible) {
            if (!ui_is_stationary(ui_input_pointer_get_translation(pointer, true))) {
                ui_gesture_set_phase(gesture, ui_gesture_phase_failed);
                backing->pointer = ui_pointer_null;
            }
        } else if (ui_gesture_get_phase(gesture) == ui_gesture_phase_began || ui_gesture_get_phase(gesture) == ui_gesture_phase_changed) {
            ui_gesture_set_phase(gesture, ui_gesture_phase_changed);
            ui_long_press_notify(gesture);
        }
    } break;
    case app_pointer_event_type_touch_ended: {
        if (ui_gesture_get_phase(gesture) == ui_gesture_phase_began || ui_gesture_get_phase(gesture) == ui_gesture_phase_changed) {
            ui_gesture_set_phase(gesture, ui_gesture_phase_ended);
            ui_long_press_notify(gesture);
        } else {
            ui_gesture_set_phase(gesture, ui_gesture_phase_failed);
        }
        backing->pointer = ui_pointer_null;
    } break;
    default:
        assert(0);
    }
}

static void ui_long_press_on_update(struct ui_gesture* gesture, double timestamp) {
    struct ui_long_press_state* backing = ui_gesture_get_state(gesture);

    ui_hold_update(gesture, &backing->pointer, &backing->began_timestamp, ui_pointer_is_null(backing->pointer) ? 0 : 1, backing->hold, timestamp);

    if (ui_gesture_get_phase(gesture) == ui_gesture_phase_possible && !ui_pointer_is_null(backing->pointer)) {
        if (timestamp - backing->began_timestamp >= backing->duration) {
            ui_gesture_set_phase(gesture, ui_gesture_phase_began);
            ui_long_press_notify(gesture);
        }
    }
}

static void ui_long_press_cancel(struct ui_gesture* gesture) {
    struct ui_long_press_state* backing = ui_gesture_get_state(gesture);

    backing->pointer = ui_pointer_null;

    if (ui_gesture_get_phase(gesture) == ui_gesture_phase_canceled) {
        ui_long_press_notify(gesture);
    }
}

static void ui_long_press_remove_pointer(struct ui_gesture* gesture, struct ui_pointer pointer) {
    struct ui_long_press_state* backing = ui_gesture_get_state(gesture);

    if (!ui_pointer_is_equal(backing->pointer, pointer)) {
        return;
    }

    bool active = ui_gesture_get_phase(gesture) == ui_gesture_phase_began || ui_gesture_get_phase(gesture) == ui_gesture_phase_changed;

    ui_gesture_set_phase(gesture, active ? ui_gesture_phase_canceled : ui_gesture_phase_failed);
    backing->pointer = ui_pointer_null;

    if (active) {
        ui_long_press_notify(gesture);
    }
}

enum ui_gesture_phase ui_long_press_gesture_get_phase(struct ui_long_press_gesture gesture) {
    return ui_gesture_lookup_phase(gesture.raw);
}

double2 ui_long_press_gesture_get_position(struct ui_long_press_gesture gesture) {
    struct ui_long_press_state* backing = ui_gesture_lookup_state(gesture.raw);

    if (backing == NULL || ui_pointer_is_null(backing->pointer)) {
        return 0.0;
    }

    return ui_input_pointer_get_position(backing->pointer);
}

bool ui_long_press_gesture_has_touch(struct ui_long_press_gesture gesture) {
    struct ui_long_press_state* backing = ui_gesture_lookup_state(gesture.raw);
    return backing != NULL && !ui_pointer_is_null(backing->pointer);
}

void ui_long_press_gesture_claim(struct ui_long_press_gesture gesture) {
    ui_gesture_lookup_claim(gesture.raw);
}

struct ui_pan_gesture ui_pan(const struct ui_pan* pan) {
    struct ui_gesture_desc desc = ui_pan_desc;
    desc.node = pan->node;
    desc.key = pan->key;

    struct ui_gesture* gesture = ui_gesture(&desc);
    struct ui_pan_state* backing = ui_gesture_get_state(gesture);

    backing->hold = ui_hold_duration(pan->holds_touches, pan->hold_duration);
    backing->on_change = pan->on_change;
    backing->user = pan->user;
    backing->direction = pan->direction;
    backing->min_touches = pan->min_touches == 0 ? 1 : min_uint32(pan->min_touches, ui_touches_max);
    backing->max_touches = pan->max_touches == 0 ? ui_touches_max : min_uint32(pan->max_touches, ui_touches_max);
    backing->max_touches = max_uint32(backing->max_touches, backing->min_touches);
    backing->direct_only = pan->direct_only;

    return (struct ui_pan_gesture) { .raw = ui_gesture_get_key(gesture) };
}

static void ui_pan_notify(struct ui_gesture* gesture) {
    struct ui_pan_state* backing = ui_gesture_get_state(gesture);

    if (backing->on_change) {
        backing->on_change((struct ui_pan_gesture) { .raw = ui_gesture_get_key(gesture) }, backing->user);
    }
}

static enum ui_gesture_decision ui_pan_accepts(struct ui_gesture* gesture, struct app_pointer_event event, struct ui_pointer pointer) {
    struct ui_pan_state* backing = ui_gesture_get_state(gesture);

    if (event.type != app_pointer_event_type_touch_began || ui_gesture_get_phase(gesture) == ui_gesture_phase_failed) {
        return ui_gesture_decision_reject;
    }

    if ((backing->direct_only && event.source != app_pointer_source_direct) || backing->touches.count >= backing->max_touches) {
        return ui_gesture_decision_reject;
    }

    return ui_hold_decision(backing->hold);
}

static void ui_pan_on_update(struct ui_gesture* gesture, double timestamp) {
    struct ui_pan_state* backing = ui_gesture_get_state(gesture);

    ui_hold_update(gesture, backing->touches.pointers, backing->touches.began, backing->touches.count, backing->hold, timestamp);
}

static void ui_pan_on_pointer(struct ui_gesture* gesture, struct app_pointer_event event, struct ui_pointer pointer) {
    struct ui_pan_state* backing = ui_gesture_get_state(gesture);
    struct ui_touches* touches = &backing->touches;
    int32_t index = ui_touches_index(touches, pointer);

    if (event.type == app_pointer_event_type_touch_began && index == -1) {
        bool added = ui_touches_add(touches, pointer, event.timestamp);
        assert(added && "accepted touch must fit");
        index = (int32_t)(touches->count - 1);
    }

    if (index == -1) {
        return;
    }

    switch (event.type) {
    case app_pointer_event_type_touch_began: {
        if (ui_gesture_get_phase(gesture) == ui_gesture_phase_failed) {
            break;
        }

        if (touches->count == 1 || touches->count == backing->min_touches) {
            ui_touches_rebase(touches);
            ui_gesture_set_phase(gesture, ui_gesture_phase_possible);
            ui_pan_notify(gesture);
        }
    } break;
    case app_pointer_event_type_touch_moved: {
        backing->timestamp = ui_input_pointer_get_timestamp(pointer);

        ui_touches_step(touches);

        if (ui_gesture_get_phase(gesture) == ui_gesture_phase_possible) {
            if (touches->count < backing->min_touches) {
                return;
            }

            double2 delta = touches->translation_accum[1];

            if (!ui_is_stationary(delta)) {
                bool lock_failed = false;
                if (backing->direction == ui_pan_direction_x) {
                    if (!ui_is_direction_x(delta)) {
                        lock_failed = true;
                    }
                } else if (backing->direction == ui_pan_direction_y) {
                    if (ui_is_direction_x(delta)) {
                        lock_failed = true;
                    }
                }

                if (lock_failed) {
                    ui_gesture_set_phase(gesture, ui_gesture_phase_failed);
                } else {
                    ui_gesture_set_phase(gesture, ui_gesture_phase_began);
                    ui_pan_notify(gesture);
                }
            }
        } else if (ui_gesture_get_phase(gesture) == ui_gesture_phase_began || ui_gesture_get_phase(gesture) == ui_gesture_phase_changed) {
            ui_gesture_set_phase(gesture, ui_gesture_phase_changed);
            ui_pan_notify(gesture);
        }
    } break;
    case app_pointer_event_type_touch_ended: {
        bool was_active = ui_gesture_is_active(gesture);

        if (was_active && touches->count == backing->min_touches) {
            ui_gesture_set_phase(gesture, ui_gesture_phase_ended);
            ui_pan_notify(gesture);
        }

        if (touches->count > 1) {
            ui_touches_remove(touches, (uint32_t)index);
            break;
        }

        bool notify_release = ui_gesture_get_phase(gesture) != ui_gesture_phase_ended;

        if (notify_release) {
            ui_gesture_set_phase(gesture, ui_gesture_phase_failed);
        }

        ui_touches_clear(touches);

        if (notify_release) {
            ui_pan_notify(gesture);
        }
    } break;
    default:
        assert(0);
    }
}

static void ui_pan_remove_pointer(struct ui_gesture* gesture, struct ui_pointer pointer) {
    struct ui_pan_state* backing = ui_gesture_get_state(gesture);
    int32_t index = ui_touches_index(&backing->touches, pointer);

    if (index == -1) {
        return;
    }

    bool was_active = ui_gesture_is_active(gesture);

    if (was_active && backing->touches.count == backing->min_touches) {
        ui_gesture_set_phase(gesture, ui_gesture_phase_canceled);
        ui_pan_notify(gesture);
        ui_touches_remove(&backing->touches, (uint32_t)index);
        return;
    }

    ui_touches_remove(&backing->touches, (uint32_t)index);

    if (backing->touches.count == 0) {
        ui_gesture_set_phase(gesture, ui_gesture_phase_failed);
        ui_pan_notify(gesture);
    }
}

static void ui_pan_cancel(struct ui_gesture* gesture) {
    struct ui_pan_state* backing = ui_gesture_get_state(gesture);
    bool had_touch = backing->touches.count > 0;

    ui_touches_clear(&backing->touches);

    if (ui_gesture_get_phase(gesture) == ui_gesture_phase_canceled || had_touch) {
        ui_pan_notify(gesture);
    }
}

enum ui_gesture_phase ui_pan_gesture_get_phase(struct ui_pan_gesture gesture) {
    return ui_gesture_lookup_phase(gesture.raw);
}

double2 ui_pan_gesture_get_position(struct ui_pan_gesture gesture) {
    struct ui_pan_state* backing = ui_gesture_lookup_state(gesture.raw);

    if (backing == NULL || backing->touches.count == 0) {
        return 0.0;
    }

    return ui_touches_position(&backing->touches);
}

double2 ui_pan_gesture_get_translation(struct ui_pan_gesture gesture, bool map_direct) {
    struct ui_pan_state* backing = ui_gesture_lookup_state(gesture.raw);

    if (backing == NULL || backing->touches.count == 0) {
        return 0.0;
    }

    return backing->touches.translation_accum[map_direct];
}

double2 ui_pan_gesture_get_velocity(struct ui_pan_gesture gesture) {
    struct ui_pan_state* backing = ui_gesture_lookup_state(gesture.raw);

    if (backing == NULL || backing->touches.count == 0) {
        return 0.0;
    }

    return ui_touches_velocity(&backing->touches);
}

double ui_pan_gesture_get_timestamp(struct ui_pan_gesture gesture) {
    struct ui_pan_state* backing = ui_gesture_lookup_state(gesture.raw);
    return backing ? backing->timestamp : 0.0;
}

bool ui_pan_gesture_is_moving(struct ui_pan_gesture gesture) {
    struct ui_pan_state* backing = ui_gesture_lookup_state(gesture.raw);

    if (backing == NULL || backing->touches.count == 0) {
        return false;
    }

    return ui_touches_moving(&backing->touches);
}

uint32_t ui_pan_gesture_get_touch_count(struct ui_pan_gesture gesture) {
    struct ui_pan_state* backing = ui_gesture_lookup_state(gesture.raw);
    return backing ? backing->touches.count : 0;
}

bool ui_pan_gesture_has_touch(struct ui_pan_gesture gesture) {
    return ui_pan_gesture_get_touch_count(gesture) > 0;
}

void ui_pan_gesture_claim(struct ui_pan_gesture gesture) {
    ui_gesture_lookup_claim(gesture.raw);
}

struct ui_hover_gesture ui_hover(const struct ui_hover* hover) {
    struct ui_gesture_desc desc = ui_hover_desc;
    desc.node = hover->node;
    desc.key = hover->key;

    struct ui_gesture* gesture = ui_gesture(&desc);
    struct ui_hover_state* backing = ui_gesture_get_state(gesture);

    backing->on_change = hover->on_change;
    backing->user = hover->user;

    return (struct ui_hover_gesture) { .raw = ui_gesture_get_key(gesture) };
}

static void ui_hover_notify(struct ui_gesture* gesture) {
    struct ui_hover_state* backing = ui_gesture_get_state(gesture);

    if (backing->on_change) {
        backing->on_change((struct ui_hover_gesture) { .raw = ui_gesture_get_key(gesture) }, backing->user);
    }
}

static enum ui_gesture_decision ui_hover_accepts(struct ui_gesture* gesture, struct app_pointer_event event, struct ui_pointer pointer) {
    bool hovers = event.type == app_pointer_event_type_hover_began || event.type == app_pointer_event_type_hover_moved;
    return hovers ? ui_gesture_decision_accept : ui_gesture_decision_reject;
}

static void ui_hover_on_pointer(struct ui_gesture* gesture, struct app_pointer_event event, struct ui_pointer pointer) {
    struct ui_hover_state* backing = ui_gesture_get_state(gesture);

    switch (event.type) {
    case app_pointer_event_type_hover_began: {
        ui_gesture_set_phase(gesture, ui_gesture_phase_began);
        backing->position = event.pos;
        backing->has_hover = true;
        ui_hover_notify(gesture);
    } break;
    case app_pointer_event_type_hover_moved: {
        if (ui_gesture_get_phase(gesture) == ui_gesture_phase_began || ui_gesture_get_phase(gesture) == ui_gesture_phase_changed) {
            ui_gesture_set_phase(gesture, ui_gesture_phase_changed);
            backing->position = event.pos;
            ui_hover_notify(gesture);
        }
    } break;
    case app_pointer_event_type_hover_ended: {
        if (ui_gesture_get_phase(gesture) == ui_gesture_phase_began || ui_gesture_get_phase(gesture) == ui_gesture_phase_changed) {
            ui_gesture_set_phase(gesture, ui_gesture_phase_ended);
            backing->position = event.pos;
            backing->has_hover = false;
            ui_hover_notify(gesture);
        } else {
            ui_gesture_set_phase(gesture, ui_gesture_phase_failed);
        }
    } break;
    default:
        assert(0);
    }
}

static void ui_hover_cancel(struct ui_gesture* gesture) {
    struct ui_hover_state* backing = ui_gesture_get_state(gesture);

    backing->has_hover = false;

    if (ui_gesture_get_phase(gesture) == ui_gesture_phase_canceled) {
        ui_hover_notify(gesture);
    }
}

enum ui_gesture_phase ui_hover_gesture_get_phase(struct ui_hover_gesture gesture) {
    return ui_gesture_lookup_phase(gesture.raw);
}

double2 ui_hover_gesture_get_position(struct ui_hover_gesture gesture) {
    struct ui_hover_state* backing = ui_gesture_lookup_state(gesture.raw);
    return backing ? backing->position : 0.0;
}

bool ui_hover_gesture_has_hover(struct ui_hover_gesture gesture) {
    struct ui_hover_state* backing = ui_gesture_lookup_state(gesture.raw);
    return backing != NULL && backing->has_hover;
}

struct ui_pinch_gesture ui_pinch(const struct ui_pinch* pinch) {
    struct ui_gesture_desc desc = ui_pinch_desc;
    desc.node = pinch->node;
    desc.key = pinch->key;

    struct ui_gesture* gesture = ui_gesture(&desc);
    struct ui_pinch_state* backing = ui_gesture_get_state(gesture);

    backing->on_change = pinch->on_change;
    backing->user = pinch->user;
    backing->hold = ui_hold_duration(pinch->holds_touches, pinch->hold_duration);

    return (struct ui_pinch_gesture) { .raw = ui_gesture_get_key(gesture) };
}

struct ui_rotate_gesture ui_rotate(const struct ui_rotate* rotate) {
    struct ui_gesture_desc desc = ui_rotate_desc;
    desc.node = rotate->node;
    desc.key = rotate->key;

    struct ui_gesture* gesture = ui_gesture(&desc);
    struct ui_rotate_state* backing = ui_gesture_get_state(gesture);

    backing->on_change = rotate->on_change;
    backing->user = rotate->user;
    backing->hold = ui_hold_duration(rotate->holds_touches, rotate->hold_duration);

    return (struct ui_rotate_gesture) { .raw = ui_gesture_get_key(gesture) };
}

static void ui_pinch_notify(struct ui_gesture* gesture) {
    struct ui_pinch_state* backing = ui_gesture_get_state(gesture);

    if (backing->on_change) {
        backing->on_change((struct ui_pinch_gesture) { .raw = ui_gesture_get_key(gesture) }, backing->user);
    }
}

static void ui_rotate_notify(struct ui_gesture* gesture) {
    struct ui_rotate_state* backing = ui_gesture_get_state(gesture);

    if (backing->on_change) {
        backing->on_change((struct ui_rotate_gesture) { .raw = ui_gesture_get_key(gesture) }, backing->user);
    }
}

static void ui_touches_gesture_cancel(struct ui_gesture* gesture, struct ui_touches* touches, void (*notify)(struct ui_gesture* gesture)) {
    bool had_touch = touches->count > 0;

    ui_touches_clear(touches);

    if (ui_gesture_get_phase(gesture) == ui_gesture_phase_canceled || had_touch) {
        notify(gesture);
    }
}

static void ui_touches_gesture_remove_pointer(struct ui_gesture* gesture, struct ui_touches* touches, struct ui_pointer pointer, void (*notify)(struct ui_gesture* gesture)) {
    int32_t index = ui_touches_index(touches, pointer);

    if (index == -1) {
        return;
    }

    if (ui_gesture_get_phase(gesture) == ui_gesture_phase_began || ui_gesture_get_phase(gesture) == ui_gesture_phase_changed) {
        ui_gesture_set_phase(gesture, ui_gesture_phase_canceled);
        notify(gesture);
    }

    ui_touches_remove(touches, (uint32_t)index);
}

static void ui_touches_gesture_on_pointer(struct ui_gesture* gesture, struct ui_touches* touches, struct app_pointer_event event, struct ui_pointer pointer, void (*notify)(struct ui_gesture* gesture)) {
    int32_t index = ui_touches_index(touches, pointer);

    if (event.type == app_pointer_event_type_touch_began && index == -1) {
        bool added = ui_touches_add(touches, pointer, event.timestamp);
        assert(added && "accepted touch must fit");
        index = (int32_t)(touches->count - 1);
    }

    if (index == -1) {
        return;
    }

    switch (event.type) {
    case app_pointer_event_type_touch_began: {
        if (touches->count == 2) {
            ui_touches_rebase(touches);
        }
    } break;
    case app_pointer_event_type_touch_moved: {
        ui_touches_step(touches);

        if (touches->count == 2) {
            if (ui_gesture_get_phase(gesture) == ui_gesture_phase_possible) {
                ui_gesture_set_phase(gesture, ui_gesture_phase_began);
                notify(gesture);
            } else if (ui_gesture_get_phase(gesture) == ui_gesture_phase_began || ui_gesture_get_phase(gesture) == ui_gesture_phase_changed) {
                ui_gesture_set_phase(gesture, ui_gesture_phase_changed);
                notify(gesture);
            }
        }
    } break;
    case app_pointer_event_type_touch_ended: {
        if (touches->count == 2 &&
            (ui_gesture_get_phase(gesture) == ui_gesture_phase_began || ui_gesture_get_phase(gesture) == ui_gesture_phase_changed)) {
            ui_gesture_set_phase(gesture, ui_gesture_phase_ended);
            notify(gesture);
        }

        ui_touches_remove(touches, (uint32_t)index);

        if (touches->count == 0 && ui_gesture_get_phase(gesture) != ui_gesture_phase_ended) {
            ui_gesture_set_phase(gesture, ui_gesture_phase_failed);
        }
    } break;
    default:
        assert(0);
    }
}

static enum ui_gesture_decision ui_pinch_accepts(struct ui_gesture* gesture, struct app_pointer_event event, struct ui_pointer pointer) {
    struct ui_pinch_state* backing = ui_gesture_get_state(gesture);
    bool accepts = event.type == app_pointer_event_type_touch_began && ui_gesture_get_phase(gesture) != ui_gesture_phase_failed && backing->touches.count < 2;
    return accepts ? ui_hold_decision(backing->hold) : ui_gesture_decision_reject;
}

static void ui_pinch_on_update(struct ui_gesture* gesture, double timestamp) {
    struct ui_pinch_state* backing = ui_gesture_get_state(gesture);
    ui_hold_update(gesture, backing->touches.pointers, backing->touches.began, backing->touches.count, backing->hold, timestamp);
}

static void ui_pinch_on_pointer(struct ui_gesture* gesture, struct app_pointer_event event, struct ui_pointer pointer) {
    struct ui_pinch_state* backing = ui_gesture_get_state(gesture);
    ui_touches_gesture_on_pointer(gesture, &backing->touches, event, pointer, ui_pinch_notify);
}

static void ui_pinch_cancel(struct ui_gesture* gesture) {
    struct ui_pinch_state* backing = ui_gesture_get_state(gesture);
    ui_touches_gesture_cancel(gesture, &backing->touches, ui_pinch_notify);
}

static void ui_pinch_remove_pointer(struct ui_gesture* gesture, struct ui_pointer pointer) {
    struct ui_pinch_state* backing = ui_gesture_get_state(gesture);
    ui_touches_gesture_remove_pointer(gesture, &backing->touches, pointer, ui_pinch_notify);
}

static enum ui_gesture_decision ui_rotate_accepts(struct ui_gesture* gesture, struct app_pointer_event event, struct ui_pointer pointer) {
    struct ui_rotate_state* backing = ui_gesture_get_state(gesture);
    bool accepts = event.type == app_pointer_event_type_touch_began && ui_gesture_get_phase(gesture) != ui_gesture_phase_failed && backing->touches.count < 2;
    return accepts ? ui_hold_decision(backing->hold) : ui_gesture_decision_reject;
}

static void ui_rotate_on_update(struct ui_gesture* gesture, double timestamp) {
    struct ui_rotate_state* backing = ui_gesture_get_state(gesture);
    ui_hold_update(gesture, backing->touches.pointers, backing->touches.began, backing->touches.count, backing->hold, timestamp);
}

static void ui_rotate_on_pointer(struct ui_gesture* gesture, struct app_pointer_event event, struct ui_pointer pointer) {
    struct ui_rotate_state* backing = ui_gesture_get_state(gesture);
    ui_touches_gesture_on_pointer(gesture, &backing->touches, event, pointer, ui_rotate_notify);
}

static void ui_rotate_cancel(struct ui_gesture* gesture) {
    struct ui_rotate_state* backing = ui_gesture_get_state(gesture);
    ui_touches_gesture_cancel(gesture, &backing->touches, ui_rotate_notify);
}

static void ui_rotate_remove_pointer(struct ui_gesture* gesture, struct ui_pointer pointer) {
    struct ui_rotate_state* backing = ui_gesture_get_state(gesture);
    ui_touches_gesture_remove_pointer(gesture, &backing->touches, pointer, ui_rotate_notify);
}

enum ui_gesture_phase ui_pinch_gesture_get_phase(struct ui_pinch_gesture gesture) {
    return ui_gesture_lookup_phase(gesture.raw);
}

double2 ui_pinch_gesture_get_translation(struct ui_pinch_gesture gesture, bool map_direct) {
    struct ui_pinch_state* backing = ui_gesture_lookup_state(gesture.raw);

    if (backing == NULL || backing->touches.count < 2) {
        return 0.0;
    }

    return backing->touches.translation_accum[map_direct];
}

double ui_pinch_gesture_get_scale(struct ui_pinch_gesture gesture, bool map_direct) {
    struct ui_pinch_state* backing = ui_gesture_lookup_state(gesture.raw);

    if (backing == NULL || backing->touches.count < 2) {
        return 1.0;
    }

    return backing->touches.scale_accum[map_direct];
}

double2 ui_pinch_gesture_get_position(struct ui_pinch_gesture gesture) {
    struct ui_pinch_state* backing = ui_gesture_lookup_state(gesture.raw);

    if (backing == NULL || backing->touches.count == 0) {
        return 0.0;
    }

    return ui_touches_position(&backing->touches);
}

uint32_t ui_pinch_gesture_get_touch_count(struct ui_pinch_gesture gesture) {
    struct ui_pinch_state* backing = ui_gesture_lookup_state(gesture.raw);
    return backing ? backing->touches.count : 0;
}

bool ui_pinch_gesture_has_touch(struct ui_pinch_gesture gesture) {
    return ui_pinch_gesture_get_touch_count(gesture) > 0;
}

void ui_pinch_gesture_claim(struct ui_pinch_gesture gesture) {
    ui_gesture_lookup_claim(gesture.raw);
}

enum ui_gesture_phase ui_rotate_gesture_get_phase(struct ui_rotate_gesture gesture) {
    return ui_gesture_lookup_phase(gesture.raw);
}

double2 ui_rotate_gesture_get_translation(struct ui_rotate_gesture gesture, bool map_direct) {
    struct ui_rotate_state* backing = ui_gesture_lookup_state(gesture.raw);

    if (backing == NULL || backing->touches.count < 2) {
        return 0.0;
    }

    return backing->touches.translation_accum[map_direct];
}

double ui_rotate_gesture_get_rotation(struct ui_rotate_gesture gesture, bool map_direct) {
    struct ui_rotate_state* backing = ui_gesture_lookup_state(gesture.raw);

    if (backing == NULL || backing->touches.count < 2) {
        return 0.0;
    }

    return backing->touches.rotation_accum[map_direct];
}

double2 ui_rotate_gesture_get_position(struct ui_rotate_gesture gesture) {
    struct ui_rotate_state* backing = ui_gesture_lookup_state(gesture.raw);

    if (backing == NULL || backing->touches.count == 0) {
        return 0.0;
    }

    return ui_touches_position(&backing->touches);
}

uint32_t ui_rotate_gesture_get_touch_count(struct ui_rotate_gesture gesture) {
    struct ui_rotate_state* backing = ui_gesture_lookup_state(gesture.raw);
    return backing ? backing->touches.count : 0;
}

bool ui_rotate_gesture_has_touch(struct ui_rotate_gesture gesture) {
    return ui_rotate_gesture_get_touch_count(gesture) > 0;
}

void ui_rotate_gesture_claim(struct ui_rotate_gesture gesture) {
    ui_gesture_lookup_claim(gesture.raw);
}
