#pragma once

#include "cr.h"
#include "ui.h"
#include "ui_gestures.h"

enum cr_closed ui_slide_mode {
    ui_slide_mode_none,
    ui_slide_mode_trigger,
    ui_slide_mode_actions
};

enum cr_closed ui_slide_phase {
    ui_slide_phase_idle,
    ui_slide_phase_tracking,
    ui_slide_phase_snapping,
};

enum cr_closed ui_scroll_phase {
    ui_scroll_phase_idle,
    ui_scroll_phase_holding,
    ui_scroll_phase_tracking,
    ui_scroll_phase_decelerating,
    ui_scroll_phase_wheel,
};

struct ui_slide {
    enum ui_slide_phase phase;
    enum ui_slide_mode mode_left;
    enum ui_slide_mode mode_right;
    void (*on_slide)(void* user);
    void* user;
    double offset;
    double velocity;
    bool is_right;
    bool is_extended;
    double anchor;
    double translation;
    double start_linear_offset;
    bool is_first_tracking;
    bool pending_ended;
    bool pending_canceled;
};

struct ui_scroll {
    enum ui_scroll_phase phase;
    double offset;
    double velocity;
    double release_velocity;
    double anchor;
    double drag;
    double sample_timestamp;
    uint32_t sample_age;
    double start_linear_offset;
    bool is_first_tracking;
    bool pending_ended;
    double pending_wheel_delta;
};

void ui_slide_init(struct ui_slide* state, enum ui_slide_mode mode_left, enum ui_slide_mode mode_right);
void ui_slide_set_on_slide(struct ui_slide* state, void (*on_slide)(void* user), void* user);
void ui_slide_on_drag(struct ui_slide* state, enum ui_gesture_phase phase, double translation, double velocity);
void ui_slide_on_update(struct ui_slide* state, double width, double extension_width, double delta_s);
double ui_slide_get_offset(const struct ui_slide* state);

void ui_scroll_init(struct ui_scroll* state);
bool ui_scroll_on_drag(struct ui_scroll* state, enum ui_gesture_phase phase, double translation, double velocity, double timestamp);
void ui_scroll_on_wheel(struct ui_scroll* state, double delta);
void ui_scroll_on_update(struct ui_scroll* state, double view_start, double view_end, double content_start, double content_end, double delta_s);
double ui_scroll_get_offset(const struct ui_scroll* state);

cr_hdef void ui_slide_on_pan(struct ui_slide* state, struct ui_pan_gesture gesture) {
    double velocity = ui_pan_gesture_is_moving(gesture) ? ui_pan_gesture_get_velocity(gesture).x : 0.0;

    ui_slide_on_drag(state, ui_pan_gesture_get_phase(gesture), ui_pan_gesture_get_translation(gesture, true).x, velocity);
}

cr_hdef bool ui_scroll_on_pan(struct ui_scroll* state, struct ui_pan_gesture gesture, enum ui_pan_direction direction) {
    bool is_x = direction == ui_pan_direction_x;
    double2 translation = ui_pan_gesture_get_translation(gesture, true);
    double2 velocity = 0.0;

    if (ui_pan_gesture_is_moving(gesture)) {
        velocity = ui_pan_gesture_get_velocity(gesture);
    }

    return ui_scroll_on_drag(state, ui_pan_gesture_get_phase(gesture), is_x ? translation.x : translation.y, is_x ? velocity.x : velocity.y, ui_pan_gesture_get_timestamp(gesture));
}
