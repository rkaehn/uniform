#include "cr.h"
#include "ui.h"
#include "ui_physics.h"
#include "ui_physics_internal.h"

#define ui_rubber_band_max_stretch (63.0 / 64.0)
#define ui_settle_distance 0.5
#define ui_settle_speed 10.0

struct ui_rubber_band {
    double coefficient;
    double min;
    double max;
    double range;
};

static double ui_rubber_band_offset(struct ui_rubber_band band, double linear) {
    double edge = clamp_double(linear, band.min, band.max);
    double stretch = linear - edge;

    if (stretch == 0.0 || band.range <= 0.0) {
        return edge;
    }

    return edge + band.coefficient * stretch * band.range / (band.coefficient * fabs(stretch) + band.range);
}

static double ui_rubber_band_linear(struct ui_rubber_band band, double offset) {
    double edge = clamp_double(offset, band.min, band.max);
    double displacement = offset - edge;

    if (displacement == 0.0 || band.range <= 0.0 || band.coefficient <= 0.0) {
        return edge;
    }

    double distance = min_double(fabs(displacement), band.range * ui_rubber_band_max_stretch);

    return edge + copysign(distance * band.range / (band.coefficient * (band.range - distance)), displacement);
}

static double ui_rubber_band_slope(struct ui_rubber_band band, double linear) {
    double edge = clamp_double(linear, band.min, band.max);
    double stretch = linear - edge;

    if (stretch == 0.0) {
        return 1.0;
    }
    if (band.range <= 0.0) {
        return 0.0;
    }

    double scale = band.coefficient * fabs(stretch) + band.range;

    return band.coefficient * band.range * band.range / (scale * scale);
}

static void ui_spring_step(double* displacement, double* velocity, double rate, double delta_s) {
    double fade = exp(-rate * delta_s);
    double drift = *velocity + rate * *displacement;

    *velocity = (*velocity - rate * drift * delta_s) * fade;
    *displacement = (*displacement + drift * delta_s) * fade;
}

static bool ui_spring_is_settled(double displacement, double velocity) {
    return fabs(displacement) < ui_settle_distance && fabs(velocity) < ui_settle_speed;
}

static struct ui_slide_desc ui_slide_desc_resolve(struct ui_slide_desc desc) {
    if (desc.rubber_band == 0.0 && desc.trigger_distance == 0.0 && desc.trigger_speed == 0.0 && desc.extend_speed == 0.0 && desc.spring_rate == 0.0) {
        return ui_slide_desc();
    }

    cr_check(isfinite(desc.trigger_distance) && desc.trigger_distance >= 0.0, "a slide desc needs a non-negative finite trigger distance");
    cr_check(isfinite(desc.trigger_speed) && desc.trigger_speed >= 0.0 && isfinite(desc.extend_speed) && desc.extend_speed >= 0.0, "a slide desc needs non-negative finite speeds");
    cr_check(isfinite(desc.rubber_band) && desc.rubber_band >= 0.0, "a slide desc needs a non-negative finite rubber band");
    cr_check(isfinite(desc.spring_rate) && desc.spring_rate > 0.0, "a slide desc needs a positive finite spring rate");

    return desc;
}

static struct ui_rubber_band ui_slide_band(struct ui_slide_desc desc, enum ui_slide_mode mode, double width, double extension_width) {
    if (mode == ui_slide_mode_trigger) {
        return (struct ui_rubber_band) { .coefficient = desc.rubber_band, .range = width };
    }
    if (mode == ui_slide_mode_actions) {
        return (struct ui_rubber_band) { .coefficient = desc.rubber_band, .min = -extension_width, .max = extension_width, .range = width };
    }
    return (struct ui_rubber_band) { .coefficient = desc.rubber_band, .range = width };
}

static enum ui_slide_mode ui_slide_mode_of_side(const struct ui_slide_state* state) {
    return state->is_right ? state->mode_left : state->mode_right;
}

static enum ui_slide_mode ui_slide_mode_at(const struct ui_slide_state* state, double linear) {
    if (state->is_right != (linear > 0.0)) {
        return ui_slide_mode_none;
    }
    return ui_slide_mode_of_side(state);
}

static void ui_slide_release(struct ui_slide_state* state, struct ui_slide_desc desc, double width, double extension_width) {
    enum ui_slide_mode mode = ui_slide_mode_of_side(state);
    double direction = state->is_right ? 1.0 : -1.0;

    if (mode == ui_slide_mode_trigger) {
        bool fast = state->velocity * direction > desc.trigger_speed;
        bool far = state->offset * direction > width * desc.trigger_distance;

        if ((fast || far) && state->on_slide) {
            state->on_slide(state->user);
        }
        state->is_extended = false;
    } else if (mode == ui_slide_mode_actions) {
        if (fabs(state->velocity) > desc.extend_speed) {
            state->is_extended = state->velocity * direction > 0.0;
        } else {
            state->is_extended = state->offset * direction > extension_width * 0.5;
        }
    } else {
        state->is_extended = false;
    }
}

void ui_slide_init(struct ui_slide* slide, enum ui_slide_mode mode_left, enum ui_slide_mode mode_right) {
    struct ui_slide_state* state = ui_slide_state(slide);

    memset(slide, 0, sizeof(*slide));
    state->mode_left = mode_left;
    state->mode_right = mode_right;
}

void ui_slide_set_on_slide(struct ui_slide* slide, void (*on_slide)(void* user), void* user) {
    struct ui_slide_state* state = ui_slide_state(slide);

    state->on_slide = on_slide;
    state->user = user;
}

void ui_slide_on_drag(struct ui_slide* slide, enum ui_gesture_phase phase, double translation, double velocity) {
    struct ui_slide_state* state = ui_slide_state(slide);

    cr_check(isfinite(translation) && isfinite(velocity), "a slide drag needs a finite translation and velocity");

    switch (phase) {
    case ui_gesture_phase_possible:
        break;
    case ui_gesture_phase_began: {
        state->phase = ui_slide_phase_tracking;
        state->anchor = translation;
        state->translation = translation;
        state->velocity = velocity;
        state->pending_ended = false;
        state->pending_canceled = false;
        state->is_first_tracking = true;

        if (!state->is_extended) {
            state->is_right = translation > 0.0;
        }
    } break;
    case ui_gesture_phase_changed: {
        if (state->phase == ui_slide_phase_tracking) {
            state->translation = translation;
            state->velocity = velocity;
        }
    } break;
    case ui_gesture_phase_ended:
    case ui_gesture_phase_recognized: {
        if (state->phase == ui_slide_phase_tracking) {
            state->pending_ended = true;
            state->translation = translation;
            state->velocity = velocity;
        }
    } break;
    case ui_gesture_phase_canceled:
    case ui_gesture_phase_failed: {
        if (state->phase == ui_slide_phase_tracking) {
            state->pending_ended = true;
            state->pending_canceled = true;
            state->velocity = 0.0;
        }
    } break;
    }
}

void ui_slide_on_update(struct ui_slide* slide, struct ui_slide_desc desc, double width, double extension_width, double timestamp) {
    struct ui_slide_state* state = ui_slide_state(slide);

    cr_check(isfinite(timestamp), "a slide update needs a finite timestamp");

    double delta_s = max_double(timestamp - state->timestamp, 0.0);

    state->timestamp = timestamp;
    desc = ui_slide_desc_resolve(desc);

    if (state->phase == ui_slide_phase_tracking) {
        if (state->is_first_tracking) {
            state->is_first_tracking = false;
            state->start_linear_offset = ui_rubber_band_linear(ui_slide_band(desc, ui_slide_mode_at(state, state->offset), width, extension_width), state->offset);
        }

        double linear = state->start_linear_offset + state->translation - state->anchor;
        struct ui_rubber_band band = ui_slide_band(desc, ui_slide_mode_at(state, linear), width, extension_width);

        state->offset = ui_rubber_band_offset(band, linear);

        if (state->pending_ended) {
            state->pending_ended = false;

            if (!state->pending_canceled) {
                ui_slide_release(state, desc, width, extension_width);
            }

            state->pending_canceled = false;
            state->velocity *= ui_rubber_band_slope(band, linear);
            state->phase = ui_slide_phase_snapping;
        }
    }

    if (state->phase == ui_slide_phase_snapping) {
        double target = 0.0;

        if (state->is_extended) {
            target = state->is_right ? extension_width : -extension_width;
        }

        double displacement = state->offset - target;

        ui_spring_step(&displacement, &state->velocity, desc.spring_rate, delta_s);
        state->offset = target + displacement;

        if (ui_spring_is_settled(displacement, state->velocity)) {
            state->offset = target;
            state->velocity = 0.0;
            state->phase = ui_slide_phase_idle;
        }
    }
}

enum ui_slide_phase ui_slide_get_phase(const struct ui_slide* slide) {
    return ui_slide_state_const(slide)->phase;
}

double ui_slide_get_offset(const struct ui_slide* slide, double to_px_from_pt) {
    return ui_round_px(ui_slide_state_const(slide)->offset, to_px_from_pt);
}

bool ui_slide_is_right(const struct ui_slide* slide) {
    return ui_slide_state_const(slide)->is_right;
}

bool ui_slide_is_extended(const struct ui_slide* slide) {
    return ui_slide_state_const(slide)->is_extended;
}

static struct ui_scroll_desc ui_scroll_desc_resolve(struct ui_scroll_desc desc) {
    if (desc.deceleration_rate == 0.0 && desc.rubber_band == 0.0 && desc.spring_rate == 0.0) {
        return ui_scroll_desc();
    }

    cr_check(isfinite(desc.deceleration_rate) && desc.deceleration_rate > 0.0, "a scroll desc needs a positive finite deceleration rate");
    cr_check(isfinite(desc.rubber_band) && desc.rubber_band >= 0.0, "a scroll desc needs a non-negative finite rubber band");
    cr_check(isfinite(desc.spring_rate) && desc.spring_rate > 0.0, "a scroll desc needs a positive finite spring rate");

    return desc;
}

static double ui_scroll_min(const struct ui_scroll_state* state) {
    return state->content_start - state->view_start;
}

static double ui_scroll_max(const struct ui_scroll_state* state) {
    return max_double(ui_scroll_min(state), state->content_end - state->view_end);
}

static struct ui_rubber_band ui_scroll_band(const struct ui_scroll_state* state, struct ui_scroll_desc desc) {
    return (struct ui_rubber_band) {
        .coefficient = desc.rubber_band,
        .min = ui_scroll_min(state),
        .max = ui_scroll_max(state),
        .range = state->view_end - state->view_start,
    };
}

static void ui_scroll_decelerate(struct ui_scroll_state* state, struct ui_scroll_desc desc, struct ui_rubber_band band, double delta_s) {
    if (delta_s <= 0.0) {
        return;
    }

    double edge = clamp_double(state->offset, band.min, band.max);

    if (state->offset == edge) {
        double decay = exp(-desc.deceleration_rate * delta_s);
        double offset = state->offset + state->velocity * (1.0 - decay) / desc.deceleration_rate;

        edge = clamp_double(offset, band.min, band.max);

        if (offset == edge) {
            state->offset = offset;
            state->velocity *= decay;

            if (fabs(state->velocity) < ui_settle_speed) {
                state->velocity = 0.0;
                state->phase = ui_scroll_phase_idle;
            }
            return;
        }

        decay = 1.0 - (edge - state->offset) * desc.deceleration_rate / state->velocity;
        delta_s = max_double(delta_s + log(decay) / desc.deceleration_rate, 0.0);
        state->offset = edge;
        state->velocity *= decay;
    }

    double displacement = state->offset - edge;

    ui_spring_step(&displacement, &state->velocity, desc.spring_rate, delta_s);
    state->offset = edge + displacement;

    if (ui_spring_is_settled(displacement, state->velocity)) {
        state->offset = edge;
        state->velocity = 0.0;
        state->phase = ui_scroll_phase_idle;
    }
}

static double ui_scroll_bar_offset(const struct ui_scroll_state* state) {
    double view = state->view_end - state->view_start;
    double content = state->content_end - state->content_start;
    double offset = state->bar_offset;

    if (content > view) {
        double thumb = min_double(max_double(view / content * state->bar_rail_length, state->bar_min_length), state->bar_rail_length);
        double travel = state->bar_rail_length - thumb;

        if (travel > 0.0) {
            offset += (state->bar_translation - state->bar_anchor) * (content - view) / travel;
        }
    }

    return clamp_double(offset, ui_scroll_min(state), ui_scroll_max(state));
}

void ui_scroll_init(struct ui_scroll* scroll) {
    memset(scroll, 0, sizeof(*scroll));
}

bool ui_scroll_on_drag(struct ui_scroll* scroll, enum ui_gesture_phase phase, double translation, double velocity, double timestamp) {
    struct ui_scroll_state* state = ui_scroll_state(scroll);

    cr_check(isfinite(translation) && isfinite(velocity), "a scroll drag needs a finite translation and velocity");

    switch (phase) {
    case ui_gesture_phase_possible: {
        if (state->phase == ui_scroll_phase_decelerating || state->phase == ui_scroll_phase_animating) {
            state->velocity = 0.0;
            state->phase = ui_scroll_phase_holding;
        }
        return state->phase == ui_scroll_phase_holding;
    }
    case ui_gesture_phase_began: {
        state->phase = ui_scroll_phase_tracking;
        state->anchor = translation;
        state->drag = 0.0;
        state->velocity = 0.0;
        state->release_velocity = 0.0;
        state->sample_timestamp = timestamp;
        state->sample_age = 0;
        state->pending_ended = false;
        state->pending_wheel_delta = 0.0;
        state->is_first_tracking = true;
    } break;
    case ui_gesture_phase_changed: {
        if (state->phase == ui_scroll_phase_tracking) {
            state->drag = translation - state->anchor;

            if (timestamp != state->sample_timestamp) {
                state->sample_timestamp = timestamp;
                state->sample_age = 0;
            }
        }
    } break;
    case ui_gesture_phase_ended:
    case ui_gesture_phase_recognized:
    case ui_gesture_phase_canceled:
    case ui_gesture_phase_failed: {
        if (state->phase == ui_scroll_phase_tracking) {
            bool released = phase == ui_gesture_phase_ended || phase == ui_gesture_phase_recognized;
            state->pending_ended = true;
            state->release_velocity = released ? -velocity : 0.0;
        } else if (state->phase == ui_scroll_phase_holding) {
            state->velocity = 0.0;
            state->phase = ui_scroll_phase_decelerating;
        }
    } break;
    }

    return false;
}

bool ui_scroll_on_bar_drag(struct ui_scroll* scroll, enum ui_gesture_phase phase, double translation, double rail_length, double min_length) {
    struct ui_scroll_state* state = ui_scroll_state(scroll);

    cr_check(isfinite(translation) && isfinite(rail_length) && isfinite(min_length) && rail_length >= 0.0 && min_length >= 0.0, "a scroll bar drag needs a finite translation and non-negative lengths");

    switch (phase) {
    case ui_gesture_phase_possible: {
        if (state->phase == ui_scroll_phase_decelerating || state->phase == ui_scroll_phase_animating) {
            state->velocity = 0.0;
            state->phase = ui_scroll_phase_holding;
        }
        return state->phase == ui_scroll_phase_holding;
    }
    case ui_gesture_phase_began: {
        state->phase = ui_scroll_phase_bar;
        state->velocity = 0.0;
        state->bar_anchor = translation;
        state->bar_translation = translation;
        state->bar_offset = state->offset;
        state->bar_rail_length = rail_length;
        state->bar_min_length = min_length;
    } break;
    case ui_gesture_phase_changed: {
        if (state->phase == ui_scroll_phase_bar) {
            state->bar_translation = translation;
            state->bar_rail_length = rail_length;
            state->bar_min_length = min_length;
            state->offset = ui_scroll_bar_offset(state);
        }
    } break;
    case ui_gesture_phase_ended:
    case ui_gesture_phase_recognized:
    case ui_gesture_phase_canceled:
    case ui_gesture_phase_failed: {
        if (state->phase == ui_scroll_phase_bar) {
            if (phase == ui_gesture_phase_ended || phase == ui_gesture_phase_recognized) {
                state->bar_translation = translation;
                state->bar_rail_length = rail_length;
                state->bar_min_length = min_length;
                state->offset = ui_scroll_bar_offset(state);
            }
            state->phase = ui_scroll_phase_idle;
        } else if (state->phase == ui_scroll_phase_holding) {
            state->velocity = 0.0;
            state->phase = ui_scroll_phase_decelerating;
        }
    } break;
    }

    return false;
}

void ui_scroll_on_wheel(struct ui_scroll* scroll, double delta) {
    struct ui_scroll_state* state = ui_scroll_state(scroll);

    cr_check(isfinite(delta), "a scroll wheel delta needs to be finite");

    if (state->phase == ui_scroll_phase_tracking || state->phase == ui_scroll_phase_holding || state->phase == ui_scroll_phase_bar) {
        return;
    }
    state->phase = ui_scroll_phase_wheel;
    state->velocity = 0.0;
    state->pending_wheel_delta += delta;
}

void ui_scroll_on_update(struct ui_scroll* scroll, struct ui_scroll_desc desc, double view_start, double view_end, double content_start, double content_end, double timestamp) {
    struct ui_scroll_state* state = ui_scroll_state(scroll);

    cr_check(isfinite(timestamp), "a scroll update needs a finite timestamp");

    double delta_s = max_double(timestamp - state->timestamp, 0.0);

    state->timestamp = timestamp;
    desc = ui_scroll_desc_resolve(desc);
    cr_check(view_start <= view_end && content_start <= content_end, "a scroll update needs a view and content whose end is not before their start");

    state->view_start = view_start;
    state->view_end = view_end;
    state->content_start = content_start;
    state->content_end = content_end;

    struct ui_rubber_band band = ui_scroll_band(state, desc);

    if (state->phase == ui_scroll_phase_idle && state->offset != clamp_double(state->offset, band.min, band.max)) {
        state->velocity = 0.0;
        state->phase = ui_scroll_phase_decelerating;
    }

    switch (state->phase) {
    case ui_scroll_phase_idle:
    case ui_scroll_phase_holding:
        break;
    case ui_scroll_phase_wheel: {
        double offset = state->offset + state->pending_wheel_delta;

        state->pending_wheel_delta = 0.0;
        state->offset = clamp_double(offset, min_double(state->offset, band.min), max_double(state->offset, band.max));
        state->phase = ui_scroll_phase_idle;
    } break;
    case ui_scroll_phase_tracking: {
        if (state->is_first_tracking) {
            state->is_first_tracking = false;
            state->start_linear_offset = ui_rubber_band_linear(band, state->offset);
        }

        double linear = state->start_linear_offset - state->drag;

        state->offset = ui_rubber_band_offset(band, linear);

        if (state->pending_ended) {
            state->pending_ended = false;
            state->velocity = state->release_velocity * ui_rubber_band_slope(band, linear);

            if (state->velocity == 0.0 && state->offset == clamp_double(state->offset, band.min, band.max)) {
                state->phase = ui_scroll_phase_idle;
            } else {
                state->phase = ui_scroll_phase_decelerating;
                ui_scroll_decelerate(state, desc, band, state->sample_age == 1 ? delta_s : 0.0);
            }
        }

        state->sample_age++;
    } break;
    case ui_scroll_phase_decelerating: {
        ui_scroll_decelerate(state, desc, band, delta_s);
    } break;
    case ui_scroll_phase_animating: {
        double target = clamp_double(state->target, band.min, band.max);
        double displacement = state->offset - target;

        ui_spring_step(&displacement, &state->velocity, desc.spring_rate, delta_s);
        state->offset = target + displacement;

        if (ui_spring_is_settled(displacement, state->velocity)) {
            state->offset = target;
            state->velocity = 0.0;
            state->phase = ui_scroll_phase_idle;
        }
    } break;
    case ui_scroll_phase_bar: {
        state->offset = ui_scroll_bar_offset(state);
    } break;
    }
}

enum ui_scroll_phase ui_scroll_get_phase(const struct ui_scroll* scroll) {
    return ui_scroll_state_const(scroll)->phase;
}

double ui_scroll_get_offset(const struct ui_scroll* scroll, double to_px_from_pt) {
    return ui_round_px(ui_scroll_state_const(scroll)->offset, to_px_from_pt);
}

void ui_scroll_set_offset(struct ui_scroll* scroll, double offset, bool animated) {
    struct ui_scroll_state* state = ui_scroll_state(scroll);

    cr_check(isfinite(offset), "a scroll offset needs to be finite");

    switch (state->phase) {
    case ui_scroll_phase_tracking: {
        state->offset = offset;
        state->velocity = 0.0;
        state->anchor += state->drag;
        state->drag = 0.0;
        state->is_first_tracking = true;
    } break;
    case ui_scroll_phase_bar: {
        state->offset = offset;
        state->bar_offset = offset;
        state->bar_anchor = state->bar_translation;
    } break;
    case ui_scroll_phase_holding: {
        state->offset = offset;
        state->velocity = 0.0;
    } break;
    case ui_scroll_phase_idle:
    case ui_scroll_phase_wheel:
    case ui_scroll_phase_decelerating:
    case ui_scroll_phase_animating: {
        if (animated) {
            state->target = offset;
            state->pending_wheel_delta = 0.0;
            state->phase = ui_scroll_phase_animating;
        } else {
            state->offset = offset;
            state->velocity = 0.0;

            if (state->phase != ui_scroll_phase_wheel) {
                state->phase = ui_scroll_phase_idle;
            }
        }
    } break;
    }
}

struct ui_scroll_bar ui_scroll_get_bar(const struct ui_scroll* scroll, double rail_length, double min_length, double to_px_from_pt) {
    const struct ui_scroll_state* state = ui_scroll_state_const(scroll);

    cr_check(isfinite(rail_length) && isfinite(min_length) && rail_length >= 0.0 && min_length >= 0.0, "a scroll bar needs non-negative finite lengths");

    double view = state->view_end - state->view_start;
    double content = state->content_end - state->content_start;
    double rail_end = ui_round_px(rail_length, to_px_from_pt);

    if (content <= 0.0 || view >= content) {
        return (struct ui_scroll_bar) { .start = 0.0, .end = rail_end };
    }

    double view_start = state->offset + state->view_start;
    double view_end = state->offset + state->view_end;
    double thumb = min_double(max_double(view / content * rail_length, min_length), rail_length);
    double travel = clamp_double((view_start - state->content_start) / (content - view), 0.0, 1.0);
    double over_start = max_double(state->content_start - view_start, 0.0);
    double over_end = max_double(view_end - state->content_end, 0.0);
    double squeeze = view > 0.0 ? thumb * min_double((over_start + over_end) / view, 1.0) : 0.0;
    double length = ui_round_px(thumb - squeeze, to_px_from_pt);

    if (over_end > 0.0 || travel >= 1.0) {
        return (struct ui_scroll_bar) { .start = rail_end - length, .end = rail_end };
    }

    double start = ui_round_px(travel * (rail_length - thumb), to_px_from_pt);

    return (struct ui_scroll_bar) { .start = start, .end = start + length };
}
