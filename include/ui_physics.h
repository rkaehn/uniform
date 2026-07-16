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
    ui_scroll_phase_animating,
    ui_scroll_phase_bar,
};

struct ui_slide_desc {
    double trigger_distance;
    double trigger_speed;
    double extend_speed;
    double rubber_band;
    double spring_rate;
};

struct ui_scroll_desc {
    double deceleration_rate;
    double rubber_band;
    double spring_rate;
};

struct ui_slide {
    _Alignas(8) uint8_t _[128];
};

struct ui_scroll {
    _Alignas(8) uint8_t _[192];
};

struct ui_scroll_bar {
    double start;
    double end;
};

cr_hdef struct ui_slide_desc ui_slide_desc(void) {
    return (struct ui_slide_desc) {
        .trigger_distance = 0.25,
        .trigger_speed = 1000.0,
        .extend_speed = 200.0,
        .rubber_band = 0.55,
        .spring_rate = 11.0,
    };
}

cr_hdef struct ui_scroll_desc ui_scroll_desc(void) {
    return (struct ui_scroll_desc) {
        .deceleration_rate = 2.0,
        .rubber_band = 0.55,
        .spring_rate = 11.0,
    };
}

void ui_slide_init(struct ui_slide* slide, enum ui_slide_mode mode_left, enum ui_slide_mode mode_right);
void ui_slide_set_on_slide(struct ui_slide* slide, void (*on_slide)(void* user), void* user);
void ui_slide_on_drag(struct ui_slide* slide, enum ui_gesture_phase phase, double translation, double velocity);
void ui_slide_on_update(struct ui_slide* slide, struct ui_slide_desc desc, double width, double extension_width, double timestamp);
enum ui_slide_phase ui_slide_get_phase(const struct ui_slide* slide);
double ui_slide_get_offset(const struct ui_slide* slide, double to_px_from_pt);
bool ui_slide_is_right(const struct ui_slide* slide);
bool ui_slide_is_extended(const struct ui_slide* slide);

void ui_scroll_init(struct ui_scroll* scroll);
bool ui_scroll_on_drag(struct ui_scroll* scroll, enum ui_gesture_phase phase, double translation, double velocity, double timestamp);
bool ui_scroll_on_bar_drag(struct ui_scroll* scroll, enum ui_gesture_phase phase, double translation, double rail_length, double min_length);
void ui_scroll_on_wheel(struct ui_scroll* scroll, double delta);
void ui_scroll_on_update(struct ui_scroll* scroll, struct ui_scroll_desc desc, double view_start, double view_end, double content_start, double content_end, double timestamp);
enum ui_scroll_phase ui_scroll_get_phase(const struct ui_scroll* scroll);
double ui_scroll_get_offset(const struct ui_scroll* scroll, double to_px_from_pt);
void ui_scroll_set_offset(struct ui_scroll* scroll, double offset, bool animated);
struct ui_scroll_bar ui_scroll_get_bar(const struct ui_scroll* scroll, double rail_length, double min_length, double to_px_from_pt);

cr_hdef void ui_slide_on_pan(struct ui_slide* slide, struct ui_pan_gesture gesture) {
    double velocity = ui_pan_gesture_is_moving(gesture) ? ui_pan_gesture_get_velocity(gesture).x : 0.0;

    ui_slide_on_drag(slide, ui_pan_gesture_get_phase(gesture), ui_pan_gesture_get_translation(gesture, true).x, velocity);
}

cr_hdef bool ui_scroll_on_pan(struct ui_scroll* scroll, struct ui_pan_gesture gesture, enum ui_pan_direction direction) {
    bool is_x = direction == ui_pan_direction_x;
    double2 translation = ui_pan_gesture_get_translation(gesture, true);
    double2 velocity = 0.0;

    if (ui_pan_gesture_is_moving(gesture)) {
        velocity = ui_pan_gesture_get_velocity(gesture);
    }

    return ui_scroll_on_drag(scroll, ui_pan_gesture_get_phase(gesture), is_x ? translation.x : translation.y, is_x ? velocity.x : velocity.y, ui_pan_gesture_get_timestamp(gesture));
}

cr_hdef bool ui_scroll_on_bar_pan(struct ui_scroll* scroll, struct ui_pan_gesture gesture, enum ui_pan_direction direction, double rail_length, double min_length) {
    bool is_x = direction == ui_pan_direction_x;
    double2 translation = ui_pan_gesture_get_translation(gesture, true);

    return ui_scroll_on_bar_drag(scroll, ui_pan_gesture_get_phase(gesture), is_x ? translation.x : translation.y, rail_length, min_length);
}
