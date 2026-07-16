#pragma once

#include "cr.h"
#include "ui.h"
#include "ui_input.h"

enum cr_closed ui_pan_direction {
    ui_pan_direction_any,
    ui_pan_direction_x,
    ui_pan_direction_y,
};

struct ui_tap_gesture {
    uint64_t raw;
};

struct ui_long_press_gesture {
    uint64_t raw;
};

struct ui_pan_gesture {
    uint64_t raw;
};

struct ui_hover_gesture {
    uint64_t raw;
};

struct ui_pinch_gesture {
    uint64_t raw;
};

struct ui_rotate_gesture {
    uint64_t raw;
};

struct ui_tap {
    struct ui_node node;
    struct ui_key key;
    void (*on_change)(struct ui_tap_gesture gesture, void* user);
    void* user;
    uint32_t touch_count;
    uint32_t tap_count;
    bool holds_touches;
    double hold_duration;
};

struct ui_long_press {
    struct ui_node node;
    struct ui_key key;
    void (*on_change)(struct ui_long_press_gesture gesture, void* user);
    void* user;
    double duration;
    bool direct_only;
    bool holds_touches;
    double hold_duration;
};

struct ui_pan {
    struct ui_node node;
    struct ui_key key;
    void (*on_change)(struct ui_pan_gesture gesture, void* user);
    void* user;
    enum ui_pan_direction direction;
    uint32_t min_touches;
    uint32_t max_touches;
    bool direct_only;
    bool holds_touches;
    double hold_duration;
};

struct ui_hover {
    struct ui_node node;
    struct ui_key key;
    void (*on_change)(struct ui_hover_gesture gesture, void* user);
    void* user;
};

struct ui_pinch {
    struct ui_node node;
    struct ui_key key;
    void (*on_change)(struct ui_pinch_gesture gesture, void* user);
    void* user;
    bool holds_touches;
    double hold_duration;
};

struct ui_rotate {
    struct ui_node node;
    struct ui_key key;
    void (*on_change)(struct ui_rotate_gesture gesture, void* user);
    void* user;
    bool holds_touches;
    double hold_duration;
};

struct ui_tap_gesture ui_tap(const struct ui_tap* tap);
enum ui_gesture_phase ui_tap_gesture_get_phase(struct ui_tap_gesture gesture);
double2 ui_tap_gesture_get_position(struct ui_tap_gesture gesture);
uint32_t ui_tap_gesture_get_touch_count(struct ui_tap_gesture gesture);
bool ui_tap_gesture_has_touch(struct ui_tap_gesture gesture);
void ui_tap_gesture_claim(struct ui_tap_gesture gesture);

struct ui_long_press_gesture ui_long_press(const struct ui_long_press* long_press);
enum ui_gesture_phase ui_long_press_gesture_get_phase(struct ui_long_press_gesture gesture);
double2 ui_long_press_gesture_get_position(struct ui_long_press_gesture gesture);
bool ui_long_press_gesture_has_touch(struct ui_long_press_gesture gesture);
void ui_long_press_gesture_claim(struct ui_long_press_gesture gesture);

struct ui_pan_gesture ui_pan(const struct ui_pan* pan);
enum ui_gesture_phase ui_pan_gesture_get_phase(struct ui_pan_gesture gesture);
double2 ui_pan_gesture_get_translation(struct ui_pan_gesture gesture, bool map_direct);
double2 ui_pan_gesture_get_position(struct ui_pan_gesture gesture);
double2 ui_pan_gesture_get_velocity(struct ui_pan_gesture gesture);
double ui_pan_gesture_get_timestamp(struct ui_pan_gesture gesture);
bool ui_pan_gesture_is_moving(struct ui_pan_gesture gesture);
uint32_t ui_pan_gesture_get_touch_count(struct ui_pan_gesture gesture);
bool ui_pan_gesture_has_touch(struct ui_pan_gesture gesture);
void ui_pan_gesture_claim(struct ui_pan_gesture gesture);

struct ui_pinch_gesture ui_pinch(const struct ui_pinch* pinch);
enum ui_gesture_phase ui_pinch_gesture_get_phase(struct ui_pinch_gesture gesture);
double2 ui_pinch_gesture_get_translation(struct ui_pinch_gesture gesture, bool map_direct);
double ui_pinch_gesture_get_scale(struct ui_pinch_gesture gesture, bool map_direct);
double2 ui_pinch_gesture_get_position(struct ui_pinch_gesture gesture);
uint32_t ui_pinch_gesture_get_touch_count(struct ui_pinch_gesture gesture);
bool ui_pinch_gesture_has_touch(struct ui_pinch_gesture gesture);
void ui_pinch_gesture_claim(struct ui_pinch_gesture gesture);

struct ui_rotate_gesture ui_rotate(const struct ui_rotate* rotate);
enum ui_gesture_phase ui_rotate_gesture_get_phase(struct ui_rotate_gesture gesture);
double2 ui_rotate_gesture_get_translation(struct ui_rotate_gesture gesture, bool map_direct);
double ui_rotate_gesture_get_rotation(struct ui_rotate_gesture gesture, bool map_direct);
double2 ui_rotate_gesture_get_position(struct ui_rotate_gesture gesture);
uint32_t ui_rotate_gesture_get_touch_count(struct ui_rotate_gesture gesture);
bool ui_rotate_gesture_has_touch(struct ui_rotate_gesture gesture);
void ui_rotate_gesture_claim(struct ui_rotate_gesture gesture);

struct ui_hover_gesture ui_hover(const struct ui_hover* hover);
enum ui_gesture_phase ui_hover_gesture_get_phase(struct ui_hover_gesture gesture);
double2 ui_hover_gesture_get_position(struct ui_hover_gesture gesture);
bool ui_hover_gesture_has_hover(struct ui_hover_gesture gesture);
