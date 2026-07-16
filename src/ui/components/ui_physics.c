#include "cr.h"
#include "ui.h"
#include "ui_physics.h"

#define ui_rubber_band_coefficient 0.55
#define ui_rubber_band_max_stretch (63.0 / 64.0)

#define ui_spring_rate 11.0
#define ui_settle_distance 0.5
#define ui_settle_speed 10.0

#define ui_scroll_deceleration_ln_rate (-2.0020026706730793)
#define ui_scroll_deceleration_projection 0.499

#define ui_slide_f_none 0.15
#define ui_slide_f_trigger 0.55
#define ui_slide_trigger_speed 1000.0
#define ui_slide_extend_speed 200.0
#define ui_slide_snap_speed_max 2500.0

struct ui_rubber_band {
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

    return edge + ui_rubber_band_coefficient * stretch * band.range / (ui_rubber_band_coefficient * fabs(stretch) + band.range);
}

static double ui_rubber_band_linear(struct ui_rubber_band band, double offset) {
    double edge = clamp_double(offset, band.min, band.max);
    double displacement = offset - edge;

    if (displacement == 0.0 || band.range <= 0.0) {
        return edge;
    }

    double distance = min_double(fabs(displacement), band.range * ui_rubber_band_max_stretch);

    return edge + copysign(distance * band.range / (ui_rubber_band_coefficient * (band.range - distance)), displacement);
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

    double scale = ui_rubber_band_coefficient * fabs(stretch) + band.range;

    return ui_rubber_band_coefficient * band.range * band.range / (scale * scale);
}

static void ui_spring_step(double* displacement, double* velocity, double delta_s) {
    double fade = exp(-ui_spring_rate * delta_s);
    double drift = *velocity + ui_spring_rate * *displacement;

    *velocity = (*velocity - ui_spring_rate * drift * delta_s) * fade;
    *displacement = (*displacement + drift * delta_s) * fade;
}

static bool ui_spring_is_settled(double displacement, double velocity) {
    return fabs(displacement) < ui_settle_distance && fabs(velocity) < ui_settle_speed;
}

static struct ui_rubber_band ui_slide_band(enum ui_slide_mode mode, double width, double extension_width) {
    if (mode == ui_slide_mode_trigger) {
        return (struct ui_rubber_band) { .range = width * ui_slide_f_trigger };
    }
    if (mode == ui_slide_mode_actions) {
        return (struct ui_rubber_band) { .min = -extension_width, .max = extension_width, .range = width * ui_slide_f_none };
    }
    return (struct ui_rubber_band) { .range = width * ui_slide_f_none };
}

static enum ui_slide_mode ui_slide_mode_of_side(const struct ui_slide* state) {
    return state->is_right ? state->mode_left : state->mode_right;
}

static enum ui_slide_mode ui_slide_mode_at(const struct ui_slide* state, double linear) {
    if (state->is_right != (linear > 0.0)) {
        return ui_slide_mode_none;
    }
    return ui_slide_mode_of_side(state);
}

static void ui_slide_release(struct ui_slide* state, double linear, double width, double extension_width) {
    enum ui_slide_mode mode = ui_slide_mode_of_side(state);
    double direction = state->is_right ? 1.0 : -1.0;

    if (mode == ui_slide_mode_trigger) {
        bool fast = state->velocity * direction > ui_slide_trigger_speed;
        bool far = fabs(linear) > width * ui_slide_f_trigger && state->offset * direction > 0.0;

        if ((fast || far) && state->on_slide) {
            state->on_slide(state->user);
        }
        state->is_extended = false;
    } else if (mode == ui_slide_mode_actions) {
        if (fabs(state->velocity) > ui_slide_extend_speed) {
            state->is_extended = state->velocity * direction > 0.0;
        } else {
            state->is_extended = state->offset * direction > extension_width * 0.5;
        }
    } else {
        state->is_extended = false;
    }
}

void ui_slide_init(struct ui_slide* state, enum ui_slide_mode mode_left, enum ui_slide_mode mode_right) {
    memset(state, 0, sizeof(*state));
    state->mode_left = mode_left;
    state->mode_right = mode_right;
}

void ui_slide_set_on_slide(struct ui_slide* state, void (*on_slide)(void* user), void* user) {
    state->on_slide = on_slide;
    state->user = user;
}

void ui_slide_on_drag(struct ui_slide* state, enum ui_gesture_phase phase, double translation, double velocity) {
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

void ui_slide_on_update(struct ui_slide* state, double width, double extension_width, double delta_s) {
    cr_check(delta_s >= 0.0, "a slide update needs a non-negative time step");

    if (state->phase == ui_slide_phase_tracking) {
        if (state->is_first_tracking) {
            state->is_first_tracking = false;
            state->start_linear_offset = ui_rubber_band_linear(ui_slide_band(ui_slide_mode_at(state, state->offset), width, extension_width), state->offset);
        }

        double linear = state->start_linear_offset + state->translation - state->anchor;
        struct ui_rubber_band band = ui_slide_band(ui_slide_mode_at(state, linear), width, extension_width);

        state->offset = ui_rubber_band_offset(band, linear);

        if (state->pending_ended) {
            state->pending_ended = false;

            if (!state->pending_canceled) {
                ui_slide_release(state, linear, width, extension_width);
            }

            state->pending_canceled = false;
            state->velocity = clamp_double(state->velocity * ui_rubber_band_slope(band, linear), -ui_slide_snap_speed_max, ui_slide_snap_speed_max);
            state->phase = ui_slide_phase_snapping;
        }
    }

    if (state->phase == ui_slide_phase_snapping) {
        double target = 0.0;

        if (state->is_extended) {
            target = state->is_right ? extension_width : -extension_width;
        }

        double displacement = state->offset - target;

        ui_spring_step(&displacement, &state->velocity, delta_s);
        state->offset = target + displacement;

        if (ui_spring_is_settled(displacement, state->velocity)) {
            state->offset = target;
            state->velocity = 0.0;
            state->phase = ui_slide_phase_idle;
        }
    }
}

double ui_slide_get_offset(const struct ui_slide* state) {
    return ui_round_px(state->offset);
}

static struct ui_rubber_band ui_scroll_band(const struct ui_scroll* state, double view_start, double view_end, double content_start, double content_end) {
    double min = state->offset - (view_start - content_start);
    double max = state->offset - (view_end - content_end);

    return (struct ui_rubber_band) { .min = min, .max = max_double(min, max), .range = view_end - view_start };
}

static void ui_scroll_decelerate(struct ui_scroll* state, struct ui_rubber_band band, double delta_s) {
    if (delta_s <= 0.0) {
        return;
    }

    double edge = clamp_double(state->offset, band.min, band.max);

    if (state->offset == edge) {
        double decay = exp(ui_scroll_deceleration_ln_rate * delta_s);
        double offset = state->offset + state->velocity * ui_scroll_deceleration_projection * (1.0 - decay);

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

        decay = 1.0 - (edge - state->offset) / (state->velocity * ui_scroll_deceleration_projection);
        delta_s = max_double(delta_s - log(decay) / ui_scroll_deceleration_ln_rate, 0.0);
        state->offset = edge;
        state->velocity *= decay;
    }

    double displacement = state->offset - edge;

    ui_spring_step(&displacement, &state->velocity, delta_s);
    state->offset = edge + displacement;

    if (ui_spring_is_settled(displacement, state->velocity)) {
        state->offset = edge;
        state->velocity = 0.0;
        state->phase = ui_scroll_phase_idle;
    }
}

void ui_scroll_init(struct ui_scroll* state) {
    memset(state, 0, sizeof(*state));
}

bool ui_scroll_on_drag(struct ui_scroll* state, enum ui_gesture_phase phase, double translation, double velocity, double timestamp) {
    cr_check(isfinite(translation) && isfinite(velocity), "a scroll drag needs a finite translation and velocity");

    switch (phase) {
    case ui_gesture_phase_possible: {
        if (state->phase == ui_scroll_phase_decelerating) {
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

void ui_scroll_on_wheel(struct ui_scroll* state, double delta) {
    cr_check(isfinite(delta), "a scroll wheel delta needs to be finite");

    if (state->phase == ui_scroll_phase_tracking || state->phase == ui_scroll_phase_holding) {
        return;
    }
    state->phase = ui_scroll_phase_wheel;
    state->velocity = 0.0;
    state->pending_wheel_delta += delta;
}

double ui_scroll_get_offset(const struct ui_scroll* state) {
    return ui_round_px(state->offset);
}

void ui_scroll_on_update(struct ui_scroll* state, double view_start, double view_end, double content_start, double content_end, double delta_s) {
    cr_check(delta_s >= 0.0, "a scroll update needs a non-negative time step");

    struct ui_rubber_band band = ui_scroll_band(state, view_start, view_end, content_start, content_end);

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
                ui_scroll_decelerate(state, band, state->sample_age == 1 ? delta_s : 0.0);
            }
        }

        state->sample_age++;
    } break;
    case ui_scroll_phase_decelerating: {
        ui_scroll_decelerate(state, band, delta_s);
    } break;
    }
}
