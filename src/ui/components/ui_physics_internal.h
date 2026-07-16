#pragma once

#include "cr.h"
#include "ui_physics.h"

struct ui_slide_state {
    enum ui_slide_phase phase;
    enum ui_slide_mode mode_left;
    enum ui_slide_mode mode_right;
    bool is_right;
    bool is_extended;
    bool is_first_tracking;
    bool pending_ended;
    bool pending_canceled;
    void (*on_slide)(void* user);
    void* user;
    double offset;
    double velocity;
    double anchor;
    double translation;
    double start_linear_offset;
    double timestamp;
};

struct ui_scroll_state {
    enum ui_scroll_phase phase;
    uint32_t sample_age;
    bool is_first_tracking;
    bool pending_ended;
    double offset;
    double velocity;
    double release_velocity;
    double anchor;
    double drag;
    double sample_timestamp;
    double start_linear_offset;
    double pending_wheel_delta;
    double target;
    double bar_anchor;
    double bar_translation;
    double bar_offset;
    double bar_rail_length;
    double bar_min_length;
    double view_start;
    double view_end;
    double content_start;
    double content_end;
    double timestamp;
};

_Static_assert(sizeof(struct ui_slide_state) <= sizeof(struct ui_slide), "the slide storage is too small for its state");
_Static_assert(_Alignof(struct ui_slide_state) <= _Alignof(struct ui_slide), "the slide storage is not aligned for its state");
_Static_assert(sizeof(struct ui_scroll_state) <= sizeof(struct ui_scroll), "the scroll storage is too small for its state");
_Static_assert(_Alignof(struct ui_scroll_state) <= _Alignof(struct ui_scroll), "the scroll storage is not aligned for its state");

cr_hdef struct ui_slide_state* ui_slide_state(struct ui_slide* slide) {
    return (struct ui_slide_state*)slide;
}

cr_hdef const struct ui_slide_state* ui_slide_state_const(const struct ui_slide* slide) {
    return (const struct ui_slide_state*)slide;
}

cr_hdef struct ui_scroll_state* ui_scroll_state(struct ui_scroll* scroll) {
    return (struct ui_scroll_state*)scroll;
}

cr_hdef const struct ui_scroll_state* ui_scroll_state_const(const struct ui_scroll* scroll) {
    return (const struct ui_scroll_state*)scroll;
}
