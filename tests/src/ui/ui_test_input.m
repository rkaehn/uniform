#include "ui_tests.h"
#include "ui_gestures.h"
#include "ui_physics.h"
#include <math.h>
#include <stdio.h>


#define ui_test_touch_emu_fingers_max 4

struct ui_test_touch_emu_finger {
    uint64_t id;
    double2 pos;
    double2 down_pos;
    double2 pending_moves[2];
    uint32_t pending_move_count;
    bool down;
    bool delivering;
    bool pending_began;
    bool pending_ended;
};

struct ui_test_touch_emu {
    struct ui_node root;
    double threshold;
    double timestamp;
    struct ui_test_touch_emu_finger fingers[ui_test_touch_emu_fingers_max];
    uint32_t finger_count;
};

cr_hdef struct ui_test_touch_emu ui_test_touch_emu(struct ui_node root, double threshold, double timestamp) {
    return (struct ui_test_touch_emu) {
        .root = root,
        .threshold = threshold,
        .timestamp = timestamp,
    };
}

cr_hdef struct ui_test_touch_emu_finger* ui_test_touch_emu_find(struct ui_test_touch_emu* emu, uint64_t id) {
    for (uint32_t i = 0; i < emu->finger_count; i++) {
        if (emu->fingers[i].id == id) {
            return &emu->fingers[i];
        }
    }

    return NULL;
}

cr_hdef void ui_test_touch_emu_down(struct ui_test_touch_emu* emu, uint64_t id, double2 pos) {
    assert(ui_test_touch_emu_find(emu, id) == NULL && "finger already down");
    assert(emu->finger_count < ui_test_touch_emu_fingers_max);

    emu->fingers[emu->finger_count++] = (struct ui_test_touch_emu_finger) {
        .id = id,
        .pos = pos,
        .down_pos = pos,
        .down = true,
        .pending_began = true,
    };
}

cr_hdef void ui_test_touch_emu_queue_move(struct ui_test_touch_emu_finger* finger, double2 pos) {
    if (finger->pending_move_count < cr_count(finger->pending_moves)) {
        finger->pending_moves[finger->pending_move_count++] = pos;
    } else {
        finger->pending_moves[finger->pending_move_count - 1] = pos;
    }
}

cr_hdef void ui_test_touch_emu_move(struct ui_test_touch_emu* emu, uint64_t id, double2 pos) {
    struct ui_test_touch_emu_finger* finger = ui_test_touch_emu_find(emu, id);
    assert(finger && finger->down && "finger must be down");

    finger->pos = pos;

    if (finger->delivering == false) {
        double2 v = finger->pos - finger->down_pos;
        if (v.x * v.x + v.y * v.y > emu->threshold * emu->threshold) {
            finger->delivering = true;

            if (emu->threshold > 0.0) {
                ui_test_touch_emu_queue_move(finger, (finger->down_pos + pos) * 0.5);
            }
        }
    }

    if (finger->delivering) {
        ui_test_touch_emu_queue_move(finger, pos);
    }
}

cr_hdef void ui_test_touch_emu_up(struct ui_test_touch_emu* emu, uint64_t id) {
    struct ui_test_touch_emu_finger* finger = ui_test_touch_emu_find(emu, id);
    assert(finger && finger->down && "finger must be down");

    finger->down = false;
    finger->pending_move_count = 0;
    finger->pending_ended = true;
}

cr_hdef void ui_test_touch_emu_flush(struct ui_test_touch_emu* emu, double dt) {
    struct app_event events[ui_test_touch_emu_fingers_max * 3];
    uint32_t event_count = 0;

    for (uint32_t i = 0; i < emu->finger_count; i++) {
        struct ui_test_touch_emu_finger* finger = &emu->fingers[i];

        struct app_event event = {
            .type = app_event_type_pointer,
            .pointer = {
                .id = finger->id,
                .timestamp = emu->timestamp,
            },
        };

        if (finger->pending_began) {
            finger->pending_began = false;
            event.pointer.type = app_pointer_event_type_touch_began;
            event.pointer.pos = finger->down_pos;
            events[event_count++] = event;
        } else if (finger->pending_move_count > 0) {
            event.pointer.type = app_pointer_event_type_touch_moved;
            event.pointer.pos = finger->pending_moves[0];
            events[event_count++] = event;

            finger->pending_move_count--;
            for (uint32_t j = 0; j < finger->pending_move_count; j++) {
                finger->pending_moves[j] = finger->pending_moves[j + 1];
            }
        } else if (finger->pending_ended) {
            finger->pending_ended = false;
            event.pointer.type = app_pointer_event_type_touch_ended;
            event.pointer.pos = finger->pos;
            events[event_count++] = event;
        } else {
            continue;
        }
    }

    ui_input_dispatch(emu->root, emu->timestamp, events, event_count, 1.0);
    emu->timestamp += dt;

    uint32_t count = 0;
    for (uint32_t i = 0; i < emu->finger_count; i++) {
        struct ui_test_touch_emu_finger* finger = &emu->fingers[i];
        if (finger->down || finger->pending_began || finger->pending_move_count > 0 || finger->pending_ended) {
            emu->fingers[count++] = emu->fingers[i];
        }
    }
    emu->finger_count = count;
}

static void ui_test_check(bool ok, const char* label) {
    ui_test_group_check_("input", ok, label);
}

static bool ui_test_simultaneous(struct ui_gesture* gesture, struct ui_gesture* other) {
    return true;
}

static bool ui_test_drag_simultaneous(struct ui_gesture* gesture, struct ui_gesture* other) {
    return strcmp(ui_gesture_get_name(other), "pan") == 0;
}

static enum ui_gesture_decision ui_test_drag_accepts(struct ui_gesture* gesture, struct app_pointer_event event, struct ui_pointer pointer) {
    return event.type == app_pointer_event_type_touch_began ? ui_gesture_decision_accept : ui_gesture_decision_reject;
}

static void ui_test_drag_on_pointer(struct ui_gesture* gesture, struct app_pointer_event event, struct ui_pointer pointer) {
    enum ui_gesture_phase phase = ui_gesture_get_phase(gesture);
    bool active = phase == ui_gesture_phase_began || phase == ui_gesture_phase_changed;

    if (event.type == app_pointer_event_type_touch_moved) {
        double2 translation = ui_input_pointer_get_translation(pointer, true);
        if (active) {
            ui_gesture_set_phase(gesture, ui_gesture_phase_changed);
        } else if (fabs(translation.x) + fabs(translation.y) > 10.0) {
            ui_gesture_set_phase(gesture, ui_gesture_phase_began);
        }
    } else if (event.type == app_pointer_event_type_touch_ended) {
        ui_gesture_set_phase(gesture, active ? ui_gesture_phase_ended : ui_gesture_phase_failed);
    }
}

static const struct ui_gesture_desc ui_test_drag_desc = {
    .name = "drag",
    .simultaneous = ui_test_drag_simultaneous,
    .accepts = ui_test_drag_accepts,
    .on_pointer = ui_test_drag_on_pointer,
};

static struct ui_gesture* ui_test_drag(struct ui_node node) {
    struct ui_gesture_desc desc = ui_test_drag_desc;
    desc.node = node;
    return ui_gesture(&desc);
}

static void ui_test_gesture_noop(struct ui_tap_gesture gesture, void* user) {
    uint32_t* count = user;
    (*count)++;
}

static void ui_test_noop_scroll(struct app_scroll_event event, void* user) {
}

static void ui_test_stale_gesture(void) {
    uint32_t tap_count = 0;

    struct ui_node root = ui_frame(&(struct ui_frame) {
        .size = ui_size_fix(400.0f, 400.0f),
    });

    struct ui_node tap_zone = ui_frame(&(struct ui_frame) {
        .parent = root,
        .pos = ui_pos_xy(0.0f, 0.0f),
        .size = ui_size_fix(400.0f, 400.0f),
    });
    ui_tap(&(struct ui_tap) {
        .node = tap_zone,
        .on_change = ui_test_gesture_noop,
        .user = &tap_count,
    });

    ui_layout(root);

    struct app_event began = {
        .type = app_event_type_pointer,
        .pointer = {
            .type = app_pointer_event_type_touch_began,
            .id = 71,
            .pos = make_double2(200.0, 200.0),
            .timestamp = 1.0,
        },
    };
    ui_input_dispatch(root, 1.0, &began, 1, 1.0);

    struct ui_node bare_root = ui_frame(&(struct ui_frame) {
        .size = ui_size_fix(400.0f, 400.0f),
    });
    ui_layout(bare_root);
    ui_release(root);

    struct app_event moved = began;
    moved.pointer.type = app_pointer_event_type_touch_moved;
    moved.pointer.pos = make_double2(240.0, 240.0);
    moved.pointer.timestamp = 1.1;
    ui_input_dispatch(bare_root, 1.1, &moved, 1, 1.0);

    struct app_event ended = moved;
    ended.pointer.type = app_pointer_event_type_touch_ended;
    ended.pointer.timestamp = 1.2;
    ui_input_dispatch(bare_root, 1.2, &ended, 1, 1.0);

    ui_release(bare_root);

    ui_test_check(tap_count == 0, "stale gesture key survives node removal");
}

static void ui_test_on_pinch_change(struct ui_pinch_gesture gesture, void* user) {
    if (ui_pinch_gesture_get_phase(gesture) == ui_gesture_phase_ended) {
        (*(uint32_t*)user)++;
    }
}

static struct app_event ui_test_touch_event(enum app_pointer_event_type type, uint64_t id, double2 pos, double timestamp) {
    return (struct app_event) {
        .type = app_event_type_pointer,
        .pointer = {
            .type = type,
            .id = id,
            .pos = pos,
            .timestamp = timestamp,
        },
    };
}

static void ui_test_send_touch(struct ui_node root, enum app_pointer_event_type type, uint64_t id, double2 pos, double timestamp) {
    struct app_event event = ui_test_touch_event(type, id, pos, timestamp);
    ui_input_dispatch(root, timestamp, &event, 1, 1.0);
}

static bool ui_test_pan_active(struct ui_pan_gesture pan) {
    enum ui_gesture_phase phase = ui_pan_gesture_get_phase(pan);
    return phase == ui_gesture_phase_began || phase == ui_gesture_phase_changed;
}

struct ui_test_custom_state {
    uint32_t pointer_events;
};

struct ui_test_custom_counts {
    uint32_t created;
    uint32_t destroyed;
};

static struct ui_test_custom_counts ui_test_custom_counts;

static void ui_test_custom_create(struct ui_gesture* gesture) {
    ui_test_custom_counts.created++;
}

static void ui_test_custom_destroy(struct ui_gesture* gesture) {
    ui_test_custom_counts.destroyed++;
}

static enum ui_gesture_decision ui_test_custom_accepts(struct ui_gesture* gesture, struct app_pointer_event event, struct ui_pointer pointer) {
    return event.type == app_pointer_event_type_touch_began ? ui_gesture_decision_accept : ui_gesture_decision_reject;
}

static uint32_t ui_test_custom_events(struct ui_gesture* gesture) {
    struct ui_test_custom_state* state = ui_gesture_get_state(gesture);
    return state->pointer_events;
}

static void ui_test_custom_on_pointer(struct ui_gesture* gesture, struct app_pointer_event event, struct ui_pointer pointer) {
    struct ui_test_custom_state* state = ui_gesture_get_state(gesture);

    state->pointer_events++;

    if (event.type == app_pointer_event_type_touch_ended) {
        ui_gesture_set_phase(gesture, ui_gesture_phase_recognized);
    }
}

static const struct ui_gesture_desc ui_test_custom_desc = {
    .name = "custom",
    .state_size = sizeof(struct ui_test_custom_state),
    .create = ui_test_custom_create,
    .destroy = ui_test_custom_destroy,
    .accepts = ui_test_custom_accepts,
    .on_pointer = ui_test_custom_on_pointer,
};

static struct ui_node ui_test_custom_zone(struct ui_node root) {
    return ui_frame(&(struct ui_frame) {
        .parent = root,
        .key = ui_key("zone"),
        .pos = ui_pos_xy(0.0f, 0.0f),
        .size = ui_size_fix(400.0f, 400.0f),
    });
}

static struct ui_gesture* ui_test_custom_declare(struct ui_node zone) {
    struct ui_gesture_desc desc = ui_test_custom_desc;
    desc.node = zone;
    return ui_gesture(&desc);
}

static void ui_test_custom_gesture(void) {
    memset(&ui_test_custom_counts, 0, sizeof(ui_test_custom_counts));

    struct ui_node root = ui_frame(&(struct ui_frame) {
        .size = ui_size_fix(400.0f, 400.0f),
    });
    struct ui_gesture* first = ui_test_custom_declare(ui_test_custom_zone(root));
    uint64_t key = ui_gesture_get_key(first);

    ui_layout(root);

    ui_test_check(ui_test_custom_counts.created == 1 && strcmp(ui_gesture_get_name(first), "custom") == 0 && ui_input_get_gesture(key) == first, "a custom gesture is created on its first declaration");

    ui_test_send_touch(root, app_pointer_event_type_touch_began, 11, make_double2(200.0, 200.0), 1.0);

    ui_test_check(ui_test_custom_events(first) == 1, "a custom gesture receives the touch it accepts");

    struct ui_node next_root = ui_frame(&(struct ui_frame) {
        .size = ui_size_fix(400.0f, 400.0f),
    });
    struct ui_gesture* second = ui_test_custom_declare(ui_test_custom_zone(next_root));

    ui_layout(next_root);
    ui_release(root);

    ui_test_check(second == first && ui_test_custom_counts.created == 1, "a redeclared custom gesture keeps its state across a rebuild");

    ui_test_send_touch(next_root, app_pointer_event_type_touch_ended, 11, make_double2(200.0, 200.0), 1.1);

    ui_test_check(ui_test_custom_events(second) == 2 && ui_gesture_get_phase(second) == ui_gesture_phase_possible, "a custom gesture recognizes and settles");

    struct ui_node bare_root = ui_frame(&(struct ui_frame) {
        .size = ui_size_fix(400.0f, 400.0f),
    });
    ui_on_scroll(&(struct ui_on_scroll) {
        .node = ui_test_custom_zone(bare_root),
        .func = ui_test_noop_scroll,
    });

    ui_layout(bare_root);
    ui_release(next_root);
    ui_input_dispatch(bare_root, 1.2, NULL, 0, 1.0);

    ui_test_check(ui_test_custom_counts.destroyed == 1 && ui_input_get_gesture(key) == NULL, "a gesture left out of a rebuild is destroyed");

    ui_release(bare_root);
    ui_input_dispatch(ui_node_null, 1.3, NULL, 0, 1.0);
}

struct ui_test_inline_counts {
    uint32_t created;
    uint32_t destroyed;
    uint32_t resets;
    uint32_t pointer_events;
};

static struct ui_test_inline_counts ui_test_inline_counts;

static void ui_test_inline_create(struct ui_gesture* gesture) {
    ui_test_inline_counts.created++;
}

static void ui_test_inline_destroy(struct ui_gesture* gesture) {
    ui_test_inline_counts.destroyed++;
}

static void ui_test_inline_reset(struct ui_gesture* gesture) {
    ui_test_inline_counts.resets++;
}

static void ui_test_inline_on_pointer(struct ui_gesture* gesture, struct app_pointer_event event, struct ui_pointer pointer) {
    ui_test_inline_counts.pointer_events++;

    if (event.type == app_pointer_event_type_touch_ended) {
        ui_gesture_set_phase(gesture, ui_gesture_phase_recognized);
    }
}

static void ui_test_inline_gesture(void) {
    memset(&ui_test_inline_counts, 0, sizeof(ui_test_inline_counts));

    struct ui_node root = ui_frame(&(struct ui_frame) {
        .size = ui_size_fix(400.0f, 400.0f),
    });
    struct ui_gesture* gesture = ui_gesture(&(struct ui_gesture_desc) {
        .node = ui_test_custom_zone(root),
        .create = ui_test_inline_create,
        .destroy = ui_test_inline_destroy,
        .reset = ui_test_inline_reset,
        .accepts = ui_test_custom_accepts,
        .on_pointer = ui_test_inline_on_pointer,
    });

    ui_layout(root);

    ui_test_check(ui_test_inline_counts.created == 1 && ui_gesture_get_name(gesture)[0] == 0, "a gesture without a name or size runs its own functions");

    ui_test_send_touch(root, app_pointer_event_type_touch_began, 31, make_double2(200.0, 200.0), 3.0);

    ui_test_check(ui_test_inline_counts.pointer_events == 1 && ui_gesture_get_phase(gesture) == ui_gesture_phase_possible, "an accepted touch reaches on_pointer");

    ui_test_send_touch(root, app_pointer_event_type_touch_ended, 31, make_double2(200.0, 200.0), 3.1);

    ui_test_check(ui_test_inline_counts.pointer_events == 2 && ui_test_inline_counts.resets == 1 && ui_gesture_get_phase(gesture) == ui_gesture_phase_possible, "a phase set through the setter recognizes and settles");

    ui_release(root);
    ui_input_dispatch(ui_node_null, 3.2, NULL, 0, 1.0);

    ui_test_check(ui_test_inline_counts.destroyed == 1, "a gesture left out of a rebuild is destroyed through its functions");
}

static void ui_test_tap_position(struct ui_tap_gesture gesture, void* user) {
    *(double2*)user = ui_tap_gesture_get_position(gesture);
}

static void ui_test_tap_reports_position(void) {
    double2 position = 0.0;

    struct ui_node root = ui_frame(&(struct ui_frame) {
        .size = ui_size_fix(400.0f, 400.0f),
    });
    struct ui_node zone = ui_frame(&(struct ui_frame) {
        .parent = root,
        .pos = ui_pos_xy(0.0f, 0.0f),
        .size = ui_size_fix(400.0f, 400.0f),
    });

    ui_tap(&(struct ui_tap) {
        .node = zone,
        .on_change = ui_test_tap_position,
        .user = &position,
    });

    ui_layout(root);

    ui_test_send_touch(root, app_pointer_event_type_touch_began, 21, make_double2(120.0, 240.0), 2.0);
    ui_test_send_touch(root, app_pointer_event_type_touch_ended, 21, make_double2(120.0, 240.0), 2.1);

    ui_test_check(position.x == 120.0 && position.y == 240.0, "a tap reports its position when it recognizes");

    ui_release(root);
    ui_input_dispatch(ui_node_null, 2.2, NULL, 0, 1.0);
}

static bool ui_test_near(double a, double b) {
    return fabs(a - b) < 1e-6;
}

static double ui_test_settle(struct ui_node root, double timestamp, uint32_t frames) {
    for (uint32_t i = 0; i < frames; i++) {
        timestamp += 1.0 / 120.0;
        ui_input_dispatch(root, timestamp, NULL, 0, 1.0);
    }
    return timestamp;
}

static void ui_test_two_finger_gestures(void) {
    uint32_t tap_count = 0;
    uint32_t pinch_ended_count = 0;

    struct ui_node root = ui_frame(&(struct ui_frame) {
        .size = ui_size_fix(400.0f, 400.0f),
    });

    struct ui_node zone = ui_frame(&(struct ui_frame) {
        .parent = root,
        .pos = ui_pos_xy(0.0f, 0.0f),
        .size = ui_size_fix(400.0f, 400.0f),
    });

    struct ui_pinch_gesture pinch = ui_pinch(&(struct ui_pinch) {
        .node = zone,
        .key = ui_key("pinch"),
        .on_change = ui_test_on_pinch_change,
        .user = &pinch_ended_count,
    });
    struct ui_rotate_gesture rotate = ui_rotate(&(struct ui_rotate) {
        .node = zone,
        .key = ui_key("rotate"),
    });
    ui_tap(&(struct ui_tap) {
        .node = zone,
        .on_change = ui_test_gesture_noop,
        .user = &tap_count,
    });

    ui_layout(root);

    ui_test_send_touch(root, app_pointer_event_type_touch_began, 81, make_double2(100.0, 200.0), 2.0);

    ui_test_check(ui_pinch_gesture_get_phase(pinch) == ui_gesture_phase_possible, "pinch waits for second touch");

    ui_test_send_touch(root, app_pointer_event_type_touch_began, 82, make_double2(300.0, 200.0), 2.05);

    ui_test_check(ui_pinch_gesture_get_phase(pinch) == ui_gesture_phase_possible, "pinch waits for movement");
    ui_test_check(ui_pinch_gesture_get_touch_count(pinch) == 2, "pinch tracks two touches");
    ui_test_check(ui_test_near(ui_pinch_gesture_get_scale(pinch, true), 1.0), "pinch scale starts neutral");
    double2 position = ui_pinch_gesture_get_position(pinch);
    ui_test_check(ui_test_near(position.x, 200.0) && ui_test_near(position.y, 200.0), "pinch position is centroid");

    ui_test_send_touch(root, app_pointer_event_type_touch_moved, 81, make_double2(50.0, 200.0), 2.1);

    ui_test_check(ui_pinch_gesture_get_phase(pinch) == ui_gesture_phase_began, "pinch begins on first move");
    ui_test_check(ui_rotate_gesture_get_phase(rotate) == ui_gesture_phase_began, "rotate begins simultaneously");
    ui_test_check(ui_test_near(ui_pinch_gesture_get_scale(pinch, true), 1.25), "pinch scale map_direct reads from start");
    ui_test_check(ui_test_near(ui_pinch_gesture_get_scale(pinch, false), 1.0), "pinch scale smooth skips first move");

    ui_test_send_touch(root, app_pointer_event_type_touch_moved, 81, make_double2(0.0, 200.0), 2.15);

    ui_test_check(ui_pinch_gesture_get_phase(pinch) == ui_gesture_phase_changed, "pinch changes on further moves");

    double settled = ui_test_settle(root, 2.15, 120);

    ui_test_check(ui_test_near(ui_pinch_gesture_get_scale(pinch, true), 1.5), "pinch scale map_direct tracks");
    ui_test_check(ui_test_near(ui_pinch_gesture_get_scale(pinch, false), 1.25), "pinch scale smooth tracks");
    double2 translation_direct = ui_pinch_gesture_get_translation(pinch, true);
    double2 translation_smooth = ui_pinch_gesture_get_translation(pinch, false);
    ui_test_check(ui_test_near(translation_direct.x, -50.0) && ui_test_near(translation_direct.y, 0.0), "pinch translation map_direct is centroid delta");
    ui_test_check(ui_test_near(translation_smooth.x, -25.0) && ui_test_near(translation_smooth.y, 0.0), "pinch translation smooth is centroid delta");

    ui_test_send_touch(root, app_pointer_event_type_touch_moved, 82, make_double2(0.0, 400.0), settled + 0.05);

    ui_test_check(ui_test_near(ui_rotate_gesture_get_rotation(rotate, true), M_PI * 0.5), "rotation map_direct reads from start");
    ui_test_check(ui_test_near(ui_rotate_gesture_get_rotation(rotate, false), 0.0), "rotation smooth skips first move");

    ui_test_send_touch(root, app_pointer_event_type_touch_ended, 81, make_double2(0.0, 200.0), settled + 0.15);

    ui_test_check(pinch_ended_count == 1, "pinch ends when a finger lifts");
    ui_test_check(ui_pinch_gesture_has_touch(pinch), "pinch keeps remaining touch");

    ui_test_send_touch(root, app_pointer_event_type_touch_ended, 82, make_double2(0.0, 400.0), settled + 0.2);

    ui_test_check(ui_pinch_gesture_has_touch(pinch) == false, "pinch releases touches");
    ui_test_check(tap_count == 0, "tap canceled by two finger begin");

    ui_release(root);
}

static void ui_test_pinch_two_touches_only(void) {

    struct ui_node root = ui_frame(&(struct ui_frame) {
        .size = ui_size_fix(400.0f, 400.0f),
    });

    struct ui_node zone = ui_frame(&(struct ui_frame) {
        .parent = root,
        .pos = ui_pos_xy(0.0f, 0.0f),
        .size = ui_size_fix(400.0f, 400.0f),
    });

    struct ui_pinch_gesture pinch = ui_pinch(&(struct ui_pinch) {
        .node = zone,
        .key = ui_key("pinch"),
    });

    ui_layout(root);

    ui_test_send_touch(root, app_pointer_event_type_touch_began, 41, make_double2(100.0, 200.0), 5.0);
    ui_test_send_touch(root, app_pointer_event_type_touch_began, 42, make_double2(300.0, 200.0), 5.05);
    ui_test_send_touch(root, app_pointer_event_type_touch_moved, 41, make_double2(150.0, 200.0), 5.1);

    ui_test_check(ui_test_near(ui_pinch_gesture_get_scale(pinch, true), 0.75), "pinch scale before third touch");

    ui_test_send_touch(root, app_pointer_event_type_touch_began, 43, make_double2(200.0, 300.0), 5.15);

    ui_test_check(ui_pinch_gesture_get_touch_count(pinch) == 2, "pinch ignores a third touch");

    ui_test_send_touch(root, app_pointer_event_type_touch_moved, 43, make_double2(200.0, 400.0), 5.2);

    ui_test_check(ui_test_near(ui_pinch_gesture_get_scale(pinch, true), 0.75), "third touch does not affect scale");

    ui_test_send_touch(root, app_pointer_event_type_touch_ended, 41, make_double2(150.0, 200.0), 5.25);
    ui_test_send_touch(root, app_pointer_event_type_touch_ended, 42, make_double2(300.0, 200.0), 5.3);
    ui_test_send_touch(root, app_pointer_event_type_touch_ended, 43, make_double2(200.0, 400.0), 5.35);

    ui_release(root);
}

static uint32_t ui_test_pan_ended_count_;

static void ui_test_on_pan_change(struct ui_pan_gesture gesture, void* user) {
    if (ui_pan_gesture_get_phase(gesture) == ui_gesture_phase_ended) {
        ui_test_pan_ended_count_++;
    }
}

static void ui_test_pan_min_touches(void) {
    ui_test_pan_ended_count_ = 0;

    struct ui_node root = ui_frame(&(struct ui_frame) {
        .size = ui_size_fix(400.0f, 400.0f),
    });

    struct ui_node zone = ui_frame(&(struct ui_frame) {
        .parent = root,
        .pos = ui_pos_xy(0.0f, 0.0f),
        .size = ui_size_fix(400.0f, 400.0f),
    });

    struct ui_pan_gesture pan = ui_pan(&(struct ui_pan) {
        .node = zone,
        .key = ui_key("pan"),
        .on_change = ui_test_on_pan_change,
        .min_touches = 2,
    });

    ui_layout(root);

    ui_test_send_touch(root, app_pointer_event_type_touch_began, 61, make_double2(100.0, 100.0), 4.0);
    ui_test_send_touch(root, app_pointer_event_type_touch_moved, 61, make_double2(200.0, 100.0), 4.05);

    ui_test_check(ui_pan_gesture_get_phase(pan) == ui_gesture_phase_possible, "pan below min touches stays possible");

    ui_test_send_touch(root, app_pointer_event_type_touch_began, 62, make_double2(100.0, 200.0), 4.1);
    ui_test_send_touch(root, app_pointer_event_type_touch_moved, 61, make_double2(250.0, 100.0), 4.15);
    ui_test_send_touch(root, app_pointer_event_type_touch_moved, 61, make_double2(260.0, 100.0), 4.18);

    ui_test_check(ui_test_pan_active(pan), "pan begins at min touches");
    double2 translation = ui_pan_gesture_get_translation(pan, true);
    ui_test_check(ui_test_near(translation.x, 30.0) && ui_test_near(translation.y, 0.0), "pan min touches translation from min");

    ui_test_send_touch(root, app_pointer_event_type_touch_ended, 62, make_double2(100.0, 200.0), 4.2);

    ui_test_check(ui_test_pan_ended_count_ == 1, "pan ends below min touches");
    ui_test_check(ui_pan_gesture_has_touch(pan), "pan keeps finger below min");

    ui_test_send_touch(root, app_pointer_event_type_touch_ended, 61, make_double2(260.0, 100.0), 4.25);

    ui_test_check(ui_pan_gesture_has_touch(pan) == false, "pan min touches releases");

    ui_release(root);
}

static void ui_test_pan_direction_lock(void) {

    struct ui_node root = ui_frame(&(struct ui_frame) {
        .size = ui_size_fix(400.0f, 400.0f),
    });

    struct ui_node zone = ui_frame(&(struct ui_frame) {
        .parent = root,
        .pos = ui_pos_xy(0.0f, 0.0f),
        .size = ui_size_fix(400.0f, 400.0f),
    });

    struct ui_pan_gesture pan = ui_pan(&(struct ui_pan) {
        .node = zone,
        .key = ui_key("pan"),
        .direction = ui_pan_direction_x,
    });

    ui_layout(root);

    ui_test_send_touch(root, app_pointer_event_type_touch_began, 21, make_double2(100.0, 100.0), 7.0);
    ui_test_send_touch(root, app_pointer_event_type_touch_moved, 21, make_double2(100.0, 200.0), 7.05);

    ui_test_check(ui_pan_gesture_get_phase(pan) == ui_gesture_phase_failed, "pan direction lock fails");

    ui_test_send_touch(root, app_pointer_event_type_touch_moved, 21, make_double2(300.0, 200.0), 7.1);

    ui_test_check(ui_pan_gesture_get_phase(pan) == ui_gesture_phase_failed, "pan failure persists while touching");

    ui_test_send_touch(root, app_pointer_event_type_touch_ended, 21, make_double2(300.0, 200.0), 7.15);

    ui_test_check(ui_pan_gesture_get_phase(pan) == ui_gesture_phase_possible, "pan failure resets after touches end");

    ui_test_send_touch(root, app_pointer_event_type_touch_began, 22, make_double2(100.0, 100.0), 7.2);
    ui_test_send_touch(root, app_pointer_event_type_touch_moved, 22, make_double2(300.0, 100.0), 7.25);
    ui_test_send_touch(root, app_pointer_event_type_touch_moved, 22, make_double2(310.0, 100.0), 7.28);

    ui_test_check(ui_test_pan_active(pan), "pan recovers on next touch");

    ui_test_send_touch(root, app_pointer_event_type_touch_ended, 22, make_double2(310.0, 100.0), 7.3);

    ui_release(root);
}

static void ui_test_scroll_on_pan(struct ui_pan_gesture gesture, void* user) {
    if (ui_scroll_on_pan(user, gesture, ui_pan_direction_y)) {
        ui_pan_gesture_claim(gesture);
    }
}

static double ui_test_scroll_frame(struct ui_node root, struct ui_scroll* scroll, double timestamp, const struct app_event* events, uint32_t event_count) {
    ui_input_dispatch(root, timestamp, events, event_count, 1.0);
    ui_scroll_on_update(scroll, scroll->offset, scroll->offset + 400.0, 0.0, 4000.0, 1.0 / 120.0);
    return scroll->offset;
}

static void ui_test_scroll_release_timing(void) {
    struct ui_node root = ui_frame(&(struct ui_frame) {
        .size = ui_size_fix(400.0f, 400.0f),
    });

    struct ui_node zone = ui_frame(&(struct ui_frame) {
        .parent = root,
        .pos = ui_pos_xy(0.0f, 0.0f),
        .size = ui_size_fix(400.0f, 400.0f),
    });

    struct ui_scroll scroll;
    ui_scroll_init(&scroll);

    ui_pan(&(struct ui_pan) {
        .node = zone,
        .key = ui_key("scroll pan"),
        .on_change = ui_test_scroll_on_pan,
        .user = &scroll,
        .direction = ui_pan_direction_y,
    });

    ui_layout(root);

    double dt = 1.0 / 120.0;
    double t0 = 20.0;
    double last_tracked = 0.0;

    for (uint32_t k = 0; k < 8; k++) {
        enum app_pointer_event_type type = k == 0 ? app_pointer_event_type_touch_began : app_pointer_event_type_touch_moved;
        struct app_event event = ui_test_touch_event(type, 73, make_double2(100.0, 300.0 - 20.0 * (double)k), t0 + (double)k * dt);
        last_tracked = ui_test_scroll_frame(root, &scroll, t0 + ((double)k + 0.5) * dt, &event, 1);
    }

    ui_test_check(ui_test_near(last_tracked, 120.0), "scroll tracks the drag from the crossing sample");

    struct app_event lift = ui_test_touch_event(app_pointer_event_type_touch_ended, 73, make_double2(100.0, 140.0), t0 + 7.5 * dt);
    double release = ui_test_scroll_frame(root, &scroll, t0 + 8.5 * dt, &lift, 1);
    double next = ui_test_scroll_frame(root, &scroll, t0 + 9.5 * dt, NULL, 0);
    double after = ui_test_scroll_frame(root, &scroll, t0 + 10.5 * dt, NULL, 0);

    ui_test_check(fabs(release - last_tracked - 19.81424) < 1.0e-3, "lift-off a frame after the last sample decelerates at once");
    ui_test_check(fabs(next - release - 19.48642) < 1.0e-3, "deceleration continues on the frame after the lift-off");
    ui_test_check(fabs(after - next - 19.16402) < 1.0e-3, "deceleration keeps decaying");

    ui_scroll_init(&scroll);
    double t1 = 30.0;

    for (uint32_t k = 0; k < 7; k++) {
        enum app_pointer_event_type type = k == 0 ? app_pointer_event_type_touch_began : app_pointer_event_type_touch_moved;
        struct app_event event = ui_test_touch_event(type, 74, make_double2(100.0, 300.0 - 20.0 * (double)k), t1 + (double)k * dt);
        ui_test_scroll_frame(root, &scroll, t1 + ((double)k + 0.5) * dt, &event, 1);
    }

    struct app_event last_and_lift[2] = {
        ui_test_touch_event(app_pointer_event_type_touch_moved, 74, make_double2(100.0, 160.0), t1 + 7.0 * dt),
        ui_test_touch_event(app_pointer_event_type_touch_ended, 74, make_double2(100.0, 150.0), t1 + 7.5 * dt),
    };
    double same_release = ui_test_scroll_frame(root, &scroll, t1 + 7.5 * dt, last_and_lift, 2);
    double same_next = ui_test_scroll_frame(root, &scroll, t1 + 8.5 * dt, NULL, 0);

    ui_test_check(ui_test_near(same_release, 120.0), "lift-off with the last sample only tracks that sample");
    ui_test_check(fabs(same_next - same_release - 19.81424) < 1.0e-3, "deceleration starts the frame after a same-frame lift-off");

    ui_scroll_init(&scroll);
    double t2 = 40.0;

    for (uint32_t k = 0; k < 8; k++) {
        enum app_pointer_event_type type = k == 0 ? app_pointer_event_type_touch_began : app_pointer_event_type_touch_moved;
        struct app_event event = ui_test_touch_event(type, 75, make_double2(100.0, 300.0 - 20.0 * (double)k), t2 + (double)k * dt);
        ui_test_scroll_frame(root, &scroll, t2 + ((double)k + 0.5) * dt, &event, 1);
    }

    double rested = ui_test_scroll_frame(root, &scroll, t2 + 8.5 * dt, NULL, 0);
    rested = ui_test_scroll_frame(root, &scroll, t2 + 9.5 * dt, NULL, 0);

    struct app_event late_lift = ui_test_touch_event(app_pointer_event_type_touch_ended, 75, make_double2(100.0, 160.0), t2 + 10.0 * dt);
    double rest_release = ui_test_scroll_frame(root, &scroll, t2 + 10.5 * dt, &late_lift, 1);
    double rest_next = ui_test_scroll_frame(root, &scroll, t2 + 11.5 * dt, NULL, 0);

    ui_test_check(rest_release == rested, "lift-off after a rest does not catch up");
    ui_test_check(fabs(rest_next - rest_release - 19.81424) < 1.0e-3, "deceleration starts the frame after a rested lift-off");

    ui_scroll_init(&scroll);
    double t3 = 50.0;

    for (uint32_t k = 0; k < 12; k++) {
        enum app_pointer_event_type type = k == 0 ? app_pointer_event_type_touch_began : app_pointer_event_type_touch_moved;
        struct app_event event = ui_test_touch_event(type, 78, make_double2(100.0, 300.0 - 2.0 * (double)k), t3 + (double)k * dt);
        ui_test_scroll_frame(root, &scroll, t3 + ((double)k + 0.5) * dt, &event, 1);
    }

    struct app_event slow_lift = ui_test_touch_event(app_pointer_event_type_touch_ended, 78, make_double2(100.0, 278.0), t3 + 11.5 * dt);
    double slow_release = ui_test_scroll_frame(root, &scroll, t3 + 12.5 * dt, &slow_lift, 1);
    double slow_next = ui_test_scroll_frame(root, &scroll, t3 + 13.5 * dt, NULL, 0);

    ui_test_check(ui_test_near(slow_release, 12.0), "a drag below the stop speed tracks to the last sample");
    ui_test_check(slow_next == slow_release, "a release below the stop speed does not fling");

    ui_release(root);
}

static void ui_test_scroll_catch(void) {
    struct ui_node root = ui_frame(&(struct ui_frame) {
        .size = ui_size_fix(400.0f, 400.0f),
    });

    struct ui_node zone = ui_frame(&(struct ui_frame) {
        .parent = root,
        .pos = ui_pos_xy(0.0f, 0.0f),
        .size = ui_size_fix(400.0f, 400.0f),
    });

    struct ui_scroll scroll;
    ui_scroll_init(&scroll);

    ui_pan(&(struct ui_pan) {
        .node = zone,
        .key = ui_key("catch pan"),
        .on_change = ui_test_scroll_on_pan,
        .user = &scroll,
        .direction = ui_pan_direction_y,
    });

    ui_layout(root);

    double dt = 1.0 / 120.0;
    double t0 = 60.0;

    for (uint32_t k = 0; k < 8; k++) {
        enum app_pointer_event_type type = k == 0 ? app_pointer_event_type_touch_began : app_pointer_event_type_touch_moved;
        struct app_event event = ui_test_touch_event(type, 76, make_double2(100.0, 300.0 - 20.0 * (double)k), t0 + (double)k * dt);
        ui_test_scroll_frame(root, &scroll, t0 + ((double)k + 0.5) * dt, &event, 1);
    }

    struct app_event lift = ui_test_touch_event(app_pointer_event_type_touch_ended, 76, make_double2(100.0, 140.0), t0 + 7.5 * dt);
    ui_test_scroll_frame(root, &scroll, t0 + 8.5 * dt, &lift, 1);
    double decelerating = ui_test_scroll_frame(root, &scroll, t0 + 9.5 * dt, NULL, 0);

    struct app_event land = ui_test_touch_event(app_pointer_event_type_touch_began, 77, make_double2(100.0, 300.0), t0 + 10.0 * dt);
    double caught = ui_test_scroll_frame(root, &scroll, t0 + 10.5 * dt, &land, 1);
    double held = ui_test_scroll_frame(root, &scroll, t0 + 11.5 * dt, NULL, 0);

    ui_test_check(caught == decelerating && held == caught, "a touch landing during deceleration stops it in that frame");

    struct app_event creep = ui_test_touch_event(app_pointer_event_type_touch_moved, 77, make_double2(100.0, 295.0), t0 + 12.0 * dt);
    double crept = ui_test_scroll_frame(root, &scroll, t0 + 12.5 * dt, &creep, 1);

    ui_test_check(crept == held, "a caught scroll waits for the hysteresis again");

    struct app_event cross = ui_test_touch_event(app_pointer_event_type_touch_moved, 77, make_double2(100.0, 285.0), t0 + 13.0 * dt);
    ui_test_scroll_frame(root, &scroll, t0 + 13.5 * dt, &cross, 1);
    struct app_event resume = ui_test_touch_event(app_pointer_event_type_touch_moved, 77, make_double2(100.0, 275.0), t0 + 16.0 * dt);
    double tracked = ui_test_scroll_frame(root, &scroll, t0 + 16.5 * dt, &resume, 1);

    ui_test_check(ui_test_near(tracked - crept, 10.0), "tracking resumes anchored at the crossing sample");

    ui_release(root);
}

static void ui_test_count_pointer(struct app_pointer_event event, struct ui_pointer pointer, void* user) {
    uint32_t* count = user;
    (*count)++;
}

static void ui_test_scroll_fling(struct ui_node root, struct ui_scroll* scroll, uint64_t id, double x, double t0) {
    double dt = 1.0 / 120.0;

    for (uint32_t k = 0; k < 8; k++) {
        enum app_pointer_event_type type = k == 0 ? app_pointer_event_type_touch_began : app_pointer_event_type_touch_moved;
        struct app_event event = ui_test_touch_event(type, id, make_double2(x, 280.0 - 20.0 * (double)k), t0 + (double)k * dt);
        ui_test_scroll_frame(root, scroll, t0 + ((double)k + 0.5) * dt, &event, 1);
    }

    struct app_event lift = ui_test_touch_event(app_pointer_event_type_touch_ended, id, make_double2(x, 140.0), t0 + 7.5 * dt);
    ui_test_scroll_frame(root, scroll, t0 + 8.5 * dt, &lift, 1);
    ui_test_scroll_frame(root, scroll, t0 + 9.5 * dt, NULL, 0);
}

static void ui_test_scroll_content_touches(void) {
    uint32_t button_taps = 0;
    uint32_t button_events = 0;
    uint32_t zone_taps = 0;
    uint32_t zone_events = 0;

    struct ui_node root = ui_frame(&(struct ui_frame) {
        .size = ui_size_fix(400.0f, 400.0f),
    });

    struct ui_node zone = ui_frame(&(struct ui_frame) {
        .parent = root,
        .pos = ui_pos_xy(0.0f, 0.0f),
        .size = ui_size_fix(400.0f, 400.0f),
    });

    struct ui_node button = ui_frame(&(struct ui_frame) {
        .parent = zone,
        .pos = ui_pos_xy(100.0f, 100.0f),
        .size = ui_size_fix(200.0f, 200.0f),
    });

    struct ui_scroll scroll;
    ui_scroll_init(&scroll);

    struct ui_long_press_gesture zone_press = ui_long_press(&(struct ui_long_press) {
        .node = zone,
        .key = ui_key("zone press"),
        .duration = 10.0,
    });
    struct ui_pan_gesture pan = ui_pan(&(struct ui_pan) {
        .node = zone,
        .key = ui_key("content pan"),
        .on_change = ui_test_scroll_on_pan,
        .user = &scroll,
        .direction = ui_pan_direction_y,
        .holds_touches = true,
    });
    ui_tap(&(struct ui_tap) {
        .node = zone,
        .key = ui_key("zone tap"),
        .on_change = ui_test_gesture_noop,
        .user = &zone_taps,
    });
    ui_on_pointer(&(struct ui_on_pointer) {
        .node = zone,
        .func = ui_test_count_pointer,
        .user = &zone_events,
    });
    ui_tap(&(struct ui_tap) {
        .node = button,
        .key = ui_key("button tap"),
        .on_change = ui_test_gesture_noop,
        .user = &button_taps,
    });
    ui_on_pointer(&(struct ui_on_pointer) {
        .node = button,
        .func = ui_test_count_pointer,
        .user = &button_events,
    });

    ui_layout(root);

    double dt = 1.0 / 120.0;

    struct app_event down = ui_test_touch_event(app_pointer_event_type_touch_began, 86, make_double2(200.0, 200.0), 70.0);
    ui_test_scroll_frame(root, &scroll, 70.0 + 0.5 * dt, &down, 1);

    ui_test_check(button_events == 0, "a touch-down inside a scroll is held back from the content");
    ui_test_check(ui_long_press_gesture_has_touch(zone_press), "a gesture on the holder's own node is not held whatever its declaration order");

    struct app_event up = ui_test_touch_event(app_pointer_event_type_touch_ended, 86, make_double2(200.0, 200.0), 70.05);
    ui_test_scroll_frame(root, &scroll, 70.05 + 0.5 * dt, &up, 1);

    ui_test_check(button_taps == 1 && button_events == 2, "a quick lift delivers the held touch-down and the tap fires");

    struct app_event rest = ui_test_touch_event(app_pointer_event_type_touch_began, 87, make_double2(200.0, 200.0), 71.0);
    ui_test_scroll_frame(root, &scroll, 71.0 + 0.5 * dt, &rest, 1);
    ui_test_scroll_frame(root, &scroll, 71.1, NULL, 0);

    ui_test_check(button_events == 2, "a resting finger is still held before the hold elapses");

    ui_test_scroll_frame(root, &scroll, 71.16, NULL, 0);

    ui_test_check(button_events == 3, "the hold elapses and the content receives the touch-down");

    struct app_event rest_up = ui_test_touch_event(app_pointer_event_type_touch_ended, 87, make_double2(200.0, 200.0), 71.3);
    ui_test_scroll_frame(root, &scroll, 71.3 + 0.5 * dt, &rest_up, 1);

    ui_test_check(button_taps == 2 && button_events == 4, "lifting after the hold taps");

    ui_test_scroll_fling(root, &scroll, 88, 200.0, 72.0);

    ui_test_check(scroll.phase == ui_scroll_phase_decelerating && button_taps == 2 && button_events == 4, "a touch that scrolls never reaches the content");

    struct app_event stop = ui_test_touch_event(app_pointer_event_type_touch_began, 89, make_double2(200.0, 200.0), 72.0 + 10.0 * dt);
    ui_test_scroll_frame(root, &scroll, 72.0 + 10.5 * dt, &stop, 1);

    ui_test_check(scroll.phase == ui_scroll_phase_holding && button_events == 4, "the touch that stops a deceleration is kept from the content");

    struct app_event stop_up = ui_test_touch_event(app_pointer_event_type_touch_ended, 89, make_double2(200.0, 200.0), 72.0 + 13.0 * dt);
    ui_test_scroll_frame(root, &scroll, 72.0 + 13.5 * dt, &stop_up, 1);
    ui_test_scroll_frame(root, &scroll, 72.0 + 14.5 * dt, NULL, 0);

    ui_test_check(button_taps == 2 && button_events == 4 && scroll.phase == ui_scroll_phase_idle, "lifting the stopping touch is not a tap and leaves the list at rest");

    ui_test_scroll_fling(root, &scroll, 90, 350.0, 73.0);
    uint32_t zone_events_before = zone_events;

    struct app_event stop_zone = ui_test_touch_event(app_pointer_event_type_touch_began, 91, make_double2(350.0, 350.0), 73.0 + 10.0 * dt);
    ui_test_scroll_frame(root, &scroll, 73.0 + 10.5 * dt, &stop_zone, 1);
    struct app_event stop_zone_up = ui_test_touch_event(app_pointer_event_type_touch_ended, 91, make_double2(350.0, 350.0), 73.0 + 13.0 * dt);
    ui_test_scroll_frame(root, &scroll, 73.0 + 13.5 * dt, &stop_zone_up, 1);

    ui_test_check(zone_taps == 0 && zone_events == zone_events_before && scroll.phase == ui_scroll_phase_idle, "the stopping touch reaches nothing on the scroll's own node either");

    struct app_event zone_down = ui_test_touch_event(app_pointer_event_type_touch_began, 92, make_double2(350.0, 350.0), 74.0);
    ui_test_scroll_frame(root, &scroll, 74.0 + 0.5 * dt, &zone_down, 1);

    ui_test_check(zone_events == zone_events_before + 1, "the scroll's own handler hears a touch-down at once");

    struct app_event zone_up = ui_test_touch_event(app_pointer_event_type_touch_ended, 92, make_double2(350.0, 350.0), 74.05);
    ui_test_scroll_frame(root, &scroll, 74.05 + 0.5 * dt, &zone_up, 1);

    ui_test_check(zone_taps == 1 && zone_events == zone_events_before + 2, "a tap on the scroll's own node fires");

    struct app_event sideways = ui_test_touch_event(app_pointer_event_type_touch_began, 93, make_double2(200.0, 200.0), 75.0);
    ui_test_scroll_frame(root, &scroll, 75.0 + 0.5 * dt, &sideways, 1);
    struct app_event sideways_move = ui_test_touch_event(app_pointer_event_type_touch_moved, 93, make_double2(230.0, 200.0), 75.02);
    ui_test_scroll_frame(root, &scroll, 75.02 + 0.5 * dt, &sideways_move, 1);

    ui_test_check(ui_pan_gesture_get_phase(pan) == ui_gesture_phase_failed && button_events == 6, "a pan that fails releases the held touch-down and the travel so far");

    struct app_event sideways_up = ui_test_touch_event(app_pointer_event_type_touch_ended, 93, make_double2(230.0, 200.0), 75.05);
    ui_test_scroll_frame(root, &scroll, 75.05 + 0.5 * dt, &sideways_up, 1);

    ui_test_check(button_taps == 2 && button_events == 7, "a released touch that moved is not a tap");

    struct app_event click = ui_test_touch_event(app_pointer_event_type_touch_began, 94, make_double2(200.0, 200.0), 76.0);
    click.pointer.source = app_pointer_source_indirect;
    ui_test_scroll_frame(root, &scroll, 76.0 + 0.5 * dt, &click, 1);

    ui_test_check(button_events == 8, "an indirect press reaches the content at once");

    struct app_event click_up = ui_test_touch_event(app_pointer_event_type_touch_ended, 94, make_double2(200.0, 200.0), 76.05);
    click_up.pointer.source = app_pointer_source_indirect;
    ui_test_scroll_frame(root, &scroll, 76.05 + 0.5 * dt, &click_up, 1);

    ui_test_check(button_taps == 3 && button_events == 9, "an indirect click taps at once");

    ui_release(root);
}

static void ui_test_hold_kinds(void) {
    uint32_t button_taps = 0;
    uint32_t button_events = 0;

    struct ui_node root = ui_frame(&(struct ui_frame) {
        .size = ui_size_fix(400.0f, 400.0f),
    });
    struct ui_node zone = ui_frame(&(struct ui_frame) {
        .parent = root,
        .pos = ui_pos_xy(0.0f, 0.0f),
        .size = ui_size_fix(400.0f, 400.0f),
    });
    struct ui_node button = ui_frame(&(struct ui_frame) {
        .parent = zone,
        .pos = ui_pos_xy(100.0f, 100.0f),
        .size = ui_size_fix(200.0f, 200.0f),
    });

    ui_pinch(&(struct ui_pinch) {
        .node = zone,
        .key = ui_key("zone pinch"),
        .holds_touches = true,
        .hold_duration = 0.3,
    });
    ui_tap(&(struct ui_tap) {
        .node = button,
        .key = ui_key("button tap"),
        .on_change = ui_test_gesture_noop,
        .user = &button_taps,
    });
    ui_on_pointer(&(struct ui_on_pointer) {
        .node = button,
        .func = ui_test_count_pointer,
        .user = &button_events,
    });

    ui_layout(root);

    ui_test_send_touch(root, app_pointer_event_type_touch_began, 95, make_double2(200.0, 200.0), 80.0);
    ui_input_dispatch(root, 80.2, NULL, 0, 1.0);

    ui_test_check(button_events == 0, "a pinch holds the content for its own duration");

    ui_input_dispatch(root, 80.32, NULL, 0, 1.0);

    ui_test_check(button_events == 1, "the longer hold elapses");

    ui_test_send_touch(root, app_pointer_event_type_touch_ended, 95, make_double2(200.0, 200.0), 80.4);

    ui_test_check(button_taps == 1, "the tap fires after the longer hold");

    ui_release(root);

    root = ui_frame(&(struct ui_frame) {
        .size = ui_size_fix(400.0f, 400.0f),
    });
    zone = ui_frame(&(struct ui_frame) {
        .parent = root,
        .pos = ui_pos_xy(0.0f, 0.0f),
        .size = ui_size_fix(400.0f, 400.0f),
    });
    button = ui_frame(&(struct ui_frame) {
        .parent = zone,
        .pos = ui_pos_xy(100.0f, 100.0f),
        .size = ui_size_fix(200.0f, 200.0f),
    });

    struct ui_pan_gesture pan = ui_pan(&(struct ui_pan) {
        .node = zone,
        .key = ui_key("all pan"),
        .holds_touches = true,
    });
    struct ui_gesture_desc willing = ui_test_custom_desc;
    willing.node = button;
    willing.simultaneous = ui_test_simultaneous;
    struct ui_gesture* custom = ui_gesture(&willing);

    ui_layout(root);

    double dt = 1.0 / 120.0;

    ui_test_send_touch(root, app_pointer_event_type_touch_began, 96, make_double2(200.0, 200.0), 81.0);

    ui_test_check(ui_test_custom_events(custom) == 1, "a gesture in the content hears a held touch-down at once");

    for (uint32_t k = 1; k <= 4; k++) {
        ui_test_send_touch(root, app_pointer_event_type_touch_moved, 96, make_double2(200.0, 200.0 - 10.0 * (double)k), 81.0 + (double)k * dt);
    }

    ui_test_check(ui_test_pan_active(pan) && ui_test_custom_events(custom) > 1, "a simultaneous gesture in the content keeps the touch when the holder begins");

    ui_test_send_touch(root, app_pointer_event_type_touch_ended, 96, make_double2(200.0, 160.0), 81.1);

    ui_release(root);
}

static void ui_test_claim_on_pointer(struct ui_gesture* gesture, struct app_pointer_event event, struct ui_pointer pointer) {
    if (event.type == app_pointer_event_type_touch_began) {
        ui_gesture_claim(gesture);
    }
}

static const struct ui_gesture_desc ui_test_claim_desc = {
    .name = "claimer",
    .accepts = ui_test_custom_accepts,
    .on_pointer = ui_test_claim_on_pointer,
};

static void ui_test_hold_content_claim(void) {
    uint32_t zone_events = 0;

    struct ui_node root = ui_frame(&(struct ui_frame) {
        .size = ui_size_fix(400.0f, 400.0f),
    });
    struct ui_node zone = ui_frame(&(struct ui_frame) {
        .parent = root,
        .pos = ui_pos_xy(0.0f, 0.0f),
        .size = ui_size_fix(400.0f, 400.0f),
    });
    struct ui_node button = ui_frame(&(struct ui_frame) {
        .parent = zone,
        .pos = ui_pos_xy(100.0f, 100.0f),
        .size = ui_size_fix(200.0f, 200.0f),
    });

    ui_pan(&(struct ui_pan) {
        .node = zone,
        .key = ui_key("zone pan"),
        .holds_touches = true,
    });
    ui_on_pointer(&(struct ui_on_pointer) {
        .node = zone,
        .func = ui_test_count_pointer,
        .user = &zone_events,
    });
    struct ui_gesture_desc claimer = ui_test_claim_desc;
    claimer.node = button;
    ui_gesture(&claimer);

    ui_layout(root);

    ui_test_send_touch(root, app_pointer_event_type_touch_began, 97, make_double2(200.0, 200.0), 82.0);

    ui_test_check(zone_events == 0, "a gesture in the content that claims the touch-down keeps it from the handler on the holder's node");

    ui_input_dispatch(root, 82.2, NULL, 0, 1.0);
    ui_test_send_touch(root, app_pointer_event_type_touch_ended, 97, make_double2(200.0, 200.0), 82.3);

    ui_test_check(zone_events == 0, "the handler hears nothing after the hold time or the lift");

    ui_release(root);
}

static void ui_test_hold_second_touch(void) {
    struct ui_node root = ui_frame(&(struct ui_frame) {
        .size = ui_size_fix(400.0f, 400.0f),
    });
    struct ui_node zone = ui_frame(&(struct ui_frame) {
        .parent = root,
        .pos = ui_pos_xy(0.0f, 0.0f),
        .size = ui_size_fix(400.0f, 400.0f),
    });
    struct ui_node button = ui_frame(&(struct ui_frame) {
        .parent = zone,
        .pos = ui_pos_xy(100.0f, 100.0f),
        .size = ui_size_fix(200.0f, 200.0f),
    });

    ui_pan(&(struct ui_pan) {
        .node = zone,
        .key = ui_key("zone pan"),
        .holds_touches = true,
    });
    struct ui_pan_gesture pan = ui_pan(&(struct ui_pan) {
        .node = button,
        .key = ui_key("button pan"),
    });

    ui_layout(root);

    ui_test_send_touch(root, app_pointer_event_type_touch_began, 98, make_double2(150.0, 150.0), 83.0);
    ui_test_send_touch(root, app_pointer_event_type_touch_began, 99, make_double2(250.0, 250.0), 83.05);
    ui_test_send_touch(root, app_pointer_event_type_touch_moved, 98, make_double2(150.0, 180.0), 83.1);

    ui_test_check(ui_test_pan_active(pan) && ui_pan_gesture_get_touch_count(pan) == 2, "a second touch reaches a gesture in the content without waiting for the hold");

    ui_test_send_touch(root, app_pointer_event_type_touch_ended, 98, make_double2(150.0, 180.0), 83.2);
    ui_test_send_touch(root, app_pointer_event_type_touch_ended, 99, make_double2(250.0, 250.0), 83.25);

    ui_release(root);
}

static void ui_test_hold_content_pan(void) {
    uint32_t plain_events = 0;

    struct ui_node root = ui_frame(&(struct ui_frame) {
        .size = ui_size_fix(400.0f, 400.0f),
    });
    struct ui_node zone = ui_frame(&(struct ui_frame) {
        .parent = root,
        .pos = ui_pos_xy(0.0f, 0.0f),
        .size = ui_size_fix(400.0f, 400.0f),
    });
    struct ui_node row = ui_frame(&(struct ui_frame) {
        .parent = zone,
        .pos = ui_pos_xy(0.0f, 100.0f),
        .size = ui_size_fix(400.0f, 100.0f),
    });
    struct ui_node plain = ui_frame(&(struct ui_frame) {
        .parent = zone,
        .pos = ui_pos_xy(0.0f, 250.0f),
        .size = ui_size_fix(400.0f, 100.0f),
    });

    struct ui_pan_gesture scroll = ui_pan(&(struct ui_pan) {
        .node = zone,
        .key = ui_key("scroll pan"),
        .holds_touches = true,
    });
    struct ui_pan_gesture slide = ui_pan(&(struct ui_pan) {
        .node = row,
        .key = ui_key("slide pan"),
        .direction = ui_pan_direction_x,
    });
    ui_on_pointer(&(struct ui_on_pointer) {
        .node = plain,
        .func = ui_test_count_pointer,
        .user = &plain_events,
    });

    ui_layout(root);

    double dt = 1.0 / 120.0;

    ui_test_send_touch(root, app_pointer_event_type_touch_began, 100, make_double2(100.0, 150.0), 84.0);
    for (uint32_t k = 1; k <= 3; k++) {
        ui_test_send_touch(root, app_pointer_event_type_touch_moved, 100, make_double2(100.0 + 8.0 * (double)k, 152.0), 84.0 + (double)k * dt);
    }

    ui_test_check(ui_test_pan_active(slide) && ui_test_pan_active(scroll) == false, "a quick sideways swipe goes to a sideways pan in the content of an any direction holder");
    ui_test_check(ui_pan_gesture_get_translation(slide, true).x == 24.0, "the pan in the content tracks the swipe from the touch-down");

    ui_test_send_touch(root, app_pointer_event_type_touch_ended, 100, make_double2(124.0, 152.0), 84.1);

    ui_test_send_touch(root, app_pointer_event_type_touch_began, 101, make_double2(100.0, 150.0), 85.0);
    for (uint32_t k = 1; k <= 3; k++) {
        ui_test_send_touch(root, app_pointer_event_type_touch_moved, 101, make_double2(104.0, 150.0 - 8.0 * (double)k), 85.0 + (double)k * dt);
    }

    ui_test_check(ui_test_pan_active(scroll) && ui_test_pan_active(slide) == false, "a quick vertical swipe over it goes to the holder");

    ui_test_send_touch(root, app_pointer_event_type_touch_ended, 101, make_double2(104.0, 126.0), 85.1);

    ui_test_send_touch(root, app_pointer_event_type_touch_began, 102, make_double2(100.0, 300.0), 86.0);

    ui_test_check(plain_events == 0, "the holder still holds the touch from a handler in the content");

    ui_test_send_touch(root, app_pointer_event_type_touch_moved, 102, make_double2(124.0, 302.0), 86.0 + dt);

    ui_test_check(ui_test_pan_active(scroll), "the holder pans sideways where no pan in the content competes");

    ui_test_send_touch(root, app_pointer_event_type_touch_ended, 102, make_double2(124.0, 302.0), 86.1);

    ui_release(root);
}

static void ui_test_pan_release_velocity(void) {
    struct ui_node root = ui_frame(&(struct ui_frame) {
        .size = ui_size_fix(400.0f, 400.0f),
    });

    struct ui_node zone = ui_frame(&(struct ui_frame) {
        .parent = root,
        .pos = ui_pos_xy(0.0f, 0.0f),
        .size = ui_size_fix(400.0f, 400.0f),
    });

    struct ui_pan_gesture pan = ui_pan(&(struct ui_pan) {
        .node = zone,
        .key = ui_key("pan"),
    });

    ui_layout(root);

    ui_test_send_touch(root, app_pointer_event_type_touch_began, 81, make_double2(100.0, 100.0), 11.0);
    ui_test_send_touch(root, app_pointer_event_type_touch_moved, 81, make_double2(100.0, 120.0), 11.01);
    ui_test_send_touch(root, app_pointer_event_type_touch_moved, 81, make_double2(100.0, 140.0), 11.02);
    ui_test_send_touch(root, app_pointer_event_type_touch_moved, 81, make_double2(100.0, 170.0), 11.03);
    ui_test_send_touch(root, app_pointer_event_type_touch_moved, 81, make_double2(100.0, 190.0), 11.04);
    ui_test_send_touch(root, app_pointer_event_type_touch_moved, 81, make_double2(100.0, 200.0), 11.05);

    double filtered = 0.05 * 1000.0 + 0.35 * 2000.0 + 0.60 * 3000.0;
    double2 velocity = ui_pan_gesture_get_velocity(pan);
    ui_test_check(ui_test_near(velocity.y, filtered) && ui_test_near(velocity.x, 0.0), "pan velocity filters the three newest sample velocities");

    ui_input_dispatch(root, 11.5, NULL, 0, 1.0);

    ui_test_check(ui_test_near(ui_pan_gesture_get_velocity(pan).y, filtered), "pan velocity holds while the finger rests");

    ui_test_send_touch(root, app_pointer_event_type_touch_ended, 81, make_double2(100.0, 200.0), 12.0);

    ui_test_send_touch(root, app_pointer_event_type_touch_began, 82, make_double2(100.0, 100.0), 13.0);
    ui_test_send_touch(root, app_pointer_event_type_touch_moved, 82, make_double2(100.0, 130.0), 13.01);
    ui_test_send_touch(root, app_pointer_event_type_touch_moved, 82, make_double2(100.0, 160.0), 13.02);
    ui_test_send_touch(root, app_pointer_event_type_touch_moved, 82, make_double2(100.0, 162.0), 13.03);
    ui_test_send_touch(root, app_pointer_event_type_touch_moved, 82, make_double2(100.0, 163.0), 13.04);

    ui_test_check(ui_pan_gesture_is_moving(pan) == false, "pan reports stopped after the finger stops");

    ui_test_send_touch(root, app_pointer_event_type_touch_ended, 82, make_double2(100.0, 163.0), 13.05);

    ui_test_send_touch(root, app_pointer_event_type_touch_began, 83, make_double2(100.0, 100.0), 16.0);
    ui_test_send_touch(root, app_pointer_event_type_touch_moved, 83, make_double2(100.0, 130.0), 16.01);
    ui_test_send_touch(root, app_pointer_event_type_touch_moved, 83, make_double2(100.0, 160.0), 16.02);
    ui_test_send_touch(root, app_pointer_event_type_touch_moved, 83, make_double2(100.0, 162.4), 16.03);
    ui_test_send_touch(root, app_pointer_event_type_touch_moved, 83, make_double2(100.0, 164.8), 16.04);

    ui_test_check(ui_pan_gesture_is_moving(pan) == false, "pan reports stopped at the fastest speed UIScrollView ignores");

    ui_test_send_touch(root, app_pointer_event_type_touch_ended, 83, make_double2(100.0, 164.8), 16.05);

    ui_test_send_touch(root, app_pointer_event_type_touch_began, 84, make_double2(100.0, 100.0), 17.0);
    ui_test_send_touch(root, app_pointer_event_type_touch_moved, 84, make_double2(100.0, 130.0), 17.01);
    ui_test_send_touch(root, app_pointer_event_type_touch_moved, 84, make_double2(100.0, 160.0), 17.02);
    ui_test_send_touch(root, app_pointer_event_type_touch_moved, 84, make_double2(100.0, 163.0), 17.03);
    ui_test_send_touch(root, app_pointer_event_type_touch_moved, 84, make_double2(100.0, 166.0), 17.04);

    ui_test_check(ui_pan_gesture_is_moving(pan) && ui_test_near(ui_pan_gesture_get_velocity(pan).y, 0.05 * 300.0 + 0.35 * 300.0 + 0.60 * 3000.0), "pan reports moving at the slowest speed UIScrollView flings");

    ui_test_send_touch(root, app_pointer_event_type_touch_ended, 84, make_double2(100.0, 166.0), 17.05);

    ui_test_send_touch(root, app_pointer_event_type_touch_began, 85, make_double2(100.0, 100.0), 18.0);
    ui_test_send_touch(root, app_pointer_event_type_touch_moved, 85, make_double2(100.0, 130.0), 18.01);
    ui_test_send_touch(root, app_pointer_event_type_touch_moved, 85, make_double2(100.0, 160.0), 18.02);
    ui_test_send_touch(root, app_pointer_event_type_touch_moved, 85, make_double2(100.0, 190.0), 18.03);
    ui_test_send_touch(root, app_pointer_event_type_touch_moved, 85, make_double2(100.0, 191.0), 18.04);

    ui_test_check(ui_pan_gesture_is_moving(pan) && ui_test_near(ui_pan_gesture_get_velocity(pan).y, 0.05 * 100.0 + 0.35 * 3000.0 + 0.60 * 3000.0), "a slow newest sample still counts as moving when the one before it moved");

    ui_test_send_touch(root, app_pointer_event_type_touch_ended, 85, make_double2(100.0, 191.0), 18.05);

    ui_release(root);
}

static void ui_test_pan_survives_tap_commit(void) {
    uint32_t tap_count = 0;

    struct ui_node root = ui_frame(&(struct ui_frame) {
        .size = ui_size_fix(400.0f, 400.0f),
    });

    struct ui_node zone = ui_frame(&(struct ui_frame) {
        .parent = root,
        .pos = ui_pos_xy(0.0f, 0.0f),
        .size = ui_size_fix(400.0f, 400.0f),
    });

    struct ui_node button = ui_frame(&(struct ui_frame) {
        .parent = zone,
        .pos = ui_pos_xy(300.0f, 300.0f),
        .size = ui_size_fix(50.0f, 50.0f),
    });

    struct ui_pan_gesture pan = ui_pan(&(struct ui_pan) {
        .node = zone,
        .key = ui_key("pan"),
    });
    ui_tap(&(struct ui_tap) {
        .node = button,
        .on_change = ui_test_gesture_noop,
        .user = &tap_count,
    });

    ui_layout(root);

    ui_test_send_touch(root, app_pointer_event_type_touch_began, 51, make_double2(100.0, 100.0), 6.0);
    ui_test_send_touch(root, app_pointer_event_type_touch_moved, 51, make_double2(150.0, 100.0), 6.05);
    ui_test_send_touch(root, app_pointer_event_type_touch_moved, 51, make_double2(160.0, 100.0), 6.08);

    ui_test_check(ui_test_pan_active(pan), "pan begins before tap touch");

    ui_test_send_touch(root, app_pointer_event_type_touch_began, 52, make_double2(320.0, 320.0), 6.1);

    ui_test_check(ui_pan_gesture_get_touch_count(pan) == 2, "pan tracks tap finger");

    ui_test_send_touch(root, app_pointer_event_type_touch_ended, 52, make_double2(320.0, 320.0), 6.15);

    ui_test_check(tap_count == 1, "tap recognizes on second finger");
    ui_test_check(ui_pan_gesture_get_touch_count(pan) == 1, "pan loses only the tapped finger");

    ui_test_send_touch(root, app_pointer_event_type_touch_moved, 51, make_double2(200.0, 100.0), 6.2);

    double2 translation = ui_pan_gesture_get_translation(pan, true);
    ui_test_check(ui_test_near(translation.x, 100.0) && ui_test_near(translation.y, 0.0), "pan continues after tap commit");

    ui_test_send_touch(root, app_pointer_event_type_touch_ended, 51, make_double2(200.0, 100.0), 6.25);

    ui_release(root);
}

static void ui_test_simultaneous_either_side(void) {

    struct ui_node root = ui_frame(&(struct ui_frame) {
        .size = ui_size_fix(400.0f, 400.0f),
    });

    struct ui_node zone = ui_frame(&(struct ui_frame) {
        .parent = root,
        .pos = ui_pos_xy(0.0f, 0.0f),
        .size = ui_size_fix(400.0f, 400.0f),
    });

    struct ui_gesture* willing = ui_test_drag(zone);
    struct ui_pan_gesture silent = ui_pan(&(struct ui_pan) {
        .node = zone,
        .key = ui_key("silent"),
    });

    ui_layout(root);

    ui_test_send_touch(root, app_pointer_event_type_touch_began, 31, make_double2(100.0, 100.0), 8.0);
    ui_test_send_touch(root, app_pointer_event_type_touch_moved, 31, make_double2(200.0, 100.0), 8.05);
    ui_test_send_touch(root, app_pointer_event_type_touch_moved, 31, make_double2(210.0, 100.0), 8.08);

    ui_test_check(ui_gesture_get_phase(willing) == ui_gesture_phase_changed && ui_test_pan_active(silent), "one side opting in is enough for simultaneous recognition");

    ui_test_send_touch(root, app_pointer_event_type_touch_ended, 31, make_double2(210.0, 100.0), 8.1);

    ui_release(root);
}

static void ui_test_pan_multi_finger(void) {

    struct ui_node root = ui_frame(&(struct ui_frame) {
        .size = ui_size_fix(400.0f, 400.0f),
    });

    struct ui_node zone = ui_frame(&(struct ui_frame) {
        .parent = root,
        .pos = ui_pos_xy(0.0f, 0.0f),
        .size = ui_size_fix(400.0f, 400.0f),
    });

    struct ui_pan_gesture pan = ui_pan(&(struct ui_pan) {
        .node = zone,
        .key = ui_key("pan"),
    });

    ui_layout(root);

    ui_test_send_touch(root, app_pointer_event_type_touch_began, 91, make_double2(100.0, 100.0), 3.0);
    ui_test_send_touch(root, app_pointer_event_type_touch_moved, 91, make_double2(105.0, 100.0), 3.02);

    ui_test_check(ui_pan_gesture_get_phase(pan) == ui_gesture_phase_possible, "pan waits inside the threshold");

    ui_test_send_touch(root, app_pointer_event_type_touch_moved, 91, make_double2(150.0, 100.0), 3.05);

    ui_test_check(ui_test_pan_active(pan), "pan begins on the sample that crosses the threshold");

    ui_test_send_touch(root, app_pointer_event_type_touch_moved, 91, make_double2(145.0, 100.0), 3.06);

    ui_test_send_touch(root, app_pointer_event_type_touch_moved, 91, make_double2(150.0, 100.0), 3.08);

    double2 translation = ui_pan_gesture_get_translation(pan, true);
    ui_test_check(ui_test_near(translation.x, 50.0) && ui_test_near(translation.y, 0.0), "pan translation single finger");

    ui_test_send_touch(root, app_pointer_event_type_touch_began, 92, make_double2(300.0, 300.0), 3.1);

    translation = ui_pan_gesture_get_translation(pan, true);
    ui_test_check(ui_test_near(translation.x, 50.0) && ui_test_near(translation.y, 0.0), "second finger keeps translation");

    ui_test_send_touch(root, app_pointer_event_type_touch_moved, 91, make_double2(250.0, 100.0), 3.15);

    translation = ui_pan_gesture_get_translation(pan, true);
    ui_test_check(ui_test_near(translation.x, 100.0) && ui_test_near(translation.y, 0.0), "two finger pan uses centroid");

    ui_test_send_touch(root, app_pointer_event_type_touch_moved, 92, make_double2(300.0, 200.0), 3.2);

    translation = ui_pan_gesture_get_translation(pan, true);
    ui_test_check(ui_test_near(translation.x, 100.0) && ui_test_near(translation.y, -50.0), "two finger pan tracks both fingers");

    ui_test_send_touch(root, app_pointer_event_type_touch_ended, 91, make_double2(250.0, 100.0), 3.25);

    ui_test_check(ui_pan_gesture_has_touch(pan), "pan keeps remaining finger");

    ui_test_send_touch(root, app_pointer_event_type_touch_moved, 92, make_double2(350.0, 200.0), 3.3);

    translation = ui_pan_gesture_get_translation(pan, true);
    ui_test_check(ui_test_near(translation.x, 150.0) && ui_test_near(translation.y, -50.0), "pan continues after finger lift");

    ui_test_send_touch(root, app_pointer_event_type_touch_ended, 92, make_double2(350.0, 200.0), 3.35);

    ui_test_check(ui_pan_gesture_has_touch(pan) == false, "pan releases fingers");

    ui_release(root);
}

struct ui_test_canvas {
    double2 translation;
    double scale;
    double rotation;
    double2 pan_last;
    double pinch_last;
    double rotate_last;
};

static double2 ui_test_canvas_rotate(double2 v, double angle) {
    double c = cos(angle);
    double s = sin(angle);
    return make_double2(v.x * c - v.y * s, v.x * s + v.y * c);
}

static bool ui_test_canvas_phase_updates(enum ui_gesture_phase phase) {
    return phase == ui_gesture_phase_began || phase == ui_gesture_phase_changed || phase == ui_gesture_phase_ended;
}

static void ui_test_canvas_on_pan(struct ui_pan_gesture gesture, void* user) {
    struct ui_test_canvas* canvas = user;
    enum ui_gesture_phase phase = ui_pan_gesture_get_phase(gesture);

    if (phase == ui_gesture_phase_began) {
        canvas->pan_last = ui_pan_gesture_get_translation(gesture, false);
    }

    if (ui_test_canvas_phase_updates(phase)) {
        double2 translation = ui_pan_gesture_get_translation(gesture, false);
        canvas->translation += translation - canvas->pan_last;
        canvas->pan_last = translation;
    }
}

static void ui_test_canvas_on_pinch(struct ui_pinch_gesture gesture, void* user) {
    struct ui_test_canvas* canvas = user;
    enum ui_gesture_phase phase = ui_pinch_gesture_get_phase(gesture);

    if (phase == ui_gesture_phase_began) {
        canvas->pinch_last = ui_pinch_gesture_get_scale(gesture, false);
    }

    if (ui_test_canvas_phase_updates(phase)) {
        double scale = ui_pinch_gesture_get_scale(gesture, false);
        double delta = scale / canvas->pinch_last;
        canvas->pinch_last = scale;

        double2 anchor = ui_pinch_gesture_get_position(gesture);
        canvas->translation = anchor + (canvas->translation - anchor) * delta;
        canvas->scale *= delta;
    }
}

static void ui_test_canvas_on_rotate(struct ui_rotate_gesture gesture, void* user) {
    struct ui_test_canvas* canvas = user;
    enum ui_gesture_phase phase = ui_rotate_gesture_get_phase(gesture);

    if (phase == ui_gesture_phase_began) {
        canvas->rotate_last = ui_rotate_gesture_get_rotation(gesture, false);
    }

    if (ui_test_canvas_phase_updates(phase)) {
        double rotation = ui_rotate_gesture_get_rotation(gesture, false);
        double delta = rotation - canvas->rotate_last;
        canvas->rotate_last = rotation;

        double2 anchor = ui_rotate_gesture_get_position(gesture);
        canvas->translation = anchor + ui_test_canvas_rotate(canvas->translation - anchor, delta);
        canvas->rotation += delta;
    }
}

static struct ui_pan_gesture ui_test_canvas_gestures(struct ui_node zone, struct ui_test_canvas* canvas, struct ui_key pan_key) {
    ui_pinch(&(struct ui_pinch) {
        .node = zone,
        .on_change = ui_test_canvas_on_pinch,
        .user = canvas,
    });
    ui_rotate(&(struct ui_rotate) {
        .node = zone,
        .on_change = ui_test_canvas_on_rotate,
        .user = canvas,
    });
    return ui_pan(&(struct ui_pan) {
        .node = zone,
        .key = pan_key,
        .on_change = ui_test_canvas_on_pan,
        .user = canvas,
    });
}

struct ui_test_canvas_limits {
    double translation;
    double scale_lo;
    double scale_hi;
    double rotation;
};

struct ui_test_canvas_scenario {
    const char* label;
    double threshold;
    double2 a0;
    double2 b0;
    double2 a_step;
    double2 b_step;
    uint32_t stagger_frames;
    uint32_t frames;
    struct ui_test_canvas_limits limits;
};

static void ui_test_canvas_smooth_scenario(const struct ui_test_canvas_scenario* scenario) {
    char label[128];

    struct ui_node root = ui_frame(&(struct ui_frame) {
        .size = ui_size_fix(400.0f, 400.0f),
    });

    struct ui_node zone = ui_frame(&(struct ui_frame) {
        .parent = root,
        .pos = ui_pos_xy(0.0f, 0.0f),
        .size = ui_size_fix(400.0f, 400.0f),
    });

    struct ui_test_canvas canvas = {
        .translation = make_double2(320.0, 240.0),
        .scale = 1.0,
    };

    ui_test_canvas_gestures(zone, &canvas, ui_key_auto);

    ui_layout(root);

    struct ui_test_touch_emu emu = ui_test_touch_emu(root, scenario->threshold, 10.0);
    double dt = 1.0 / 120.0;
    uint64_t id_a = 0x7001;
    uint64_t id_b = 0x7002;

    double2 a = scenario->a0;
    double2 b = scenario->b0;

    ui_test_touch_emu_down(&emu, id_a, a);
    if (scenario->stagger_frames == 0) {
        ui_test_touch_emu_down(&emu, id_b, b);
    }
    ui_test_touch_emu_flush(&emu, dt);

    for (uint32_t i = 0; i < scenario->stagger_frames; i++) {
        a += scenario->a_step;
        ui_test_touch_emu_move(&emu, id_a, a);
        ui_test_touch_emu_flush(&emu, dt);
    }

    if (scenario->stagger_frames != 0) {
        ui_test_touch_emu_down(&emu, id_b, b);
        ui_test_touch_emu_flush(&emu, dt);
    }

    double translation_max = 0.0;
    double scale_ratio_lo = 1.0;
    double scale_ratio_hi = 1.0;
    double rotation_max = 0.0;

    for (uint32_t i = 0; i < scenario->frames; i++) {
        struct ui_test_canvas prev = canvas;

        a += scenario->a_step;
        b += scenario->b_step;
        ui_test_touch_emu_move(&emu, id_a, a);
        ui_test_touch_emu_move(&emu, id_b, b);
        ui_test_touch_emu_flush(&emu, dt);

        double2 dtr = canvas.translation - prev.translation;
        translation_max = max_double(translation_max, sqrt(dtr.x * dtr.x + dtr.y * dtr.y));

        double ratio = canvas.scale / prev.scale;
        scale_ratio_lo = min_double(scale_ratio_lo, ratio);
        scale_ratio_hi = max_double(scale_ratio_hi, ratio);

        rotation_max = max_double(rotation_max, fabs(canvas.rotation - prev.rotation));
    }

    ui_test_touch_emu_up(&emu, id_a);
    ui_test_touch_emu_up(&emu, id_b);
    ui_test_touch_emu_flush(&emu, dt);

    snprintf(label, sizeof(label), "%s: translation steps (max %.2f, limit %.2f)", scenario->label, translation_max, scenario->limits.translation);
    ui_test_check(translation_max <= scenario->limits.translation, label);

    snprintf(label, sizeof(label), "%s: scale steps (%.3f..%.3f, limits %.3f..%.3f)", scenario->label, scale_ratio_lo, scale_ratio_hi, scenario->limits.scale_lo, scenario->limits.scale_hi);
    ui_test_check(scale_ratio_lo >= scenario->limits.scale_lo && scale_ratio_hi <= scenario->limits.scale_hi, label);

    snprintf(label, sizeof(label), "%s: rotation steps (max %.3f, limit %.3f)", scenario->label, rotation_max, scenario->limits.rotation);
    ui_test_check(rotation_max <= scenario->limits.rotation, label);

    ui_release(root);
}

static void ui_test_canvas_gestures_smooth(void) {
    double thresholds[] = { 0.0, 8.0, 24.0 };
    struct ui_test_canvas_limits pinch_limits = { .translation = 8.0, .scale_lo = 0.95, .scale_hi = 1.05, .rotation = 0.05 };
    struct ui_test_canvas_limits pan_limits = { .translation = 8.0, .scale_lo = 0.95, .scale_hi = 1.05, .rotation = 0.05 };
    struct ui_test_canvas_limits coincident_limits = { .translation = 10.0, .scale_lo = 0.9, .scale_hi = 1.1, .rotation = 0.15 };

    for (uint32_t i = 0; i < cr_count(thresholds); i++) {
        char label[64];

        snprintf(label, sizeof(label), "simultaneous pinch t=%.0f", thresholds[i]);
        ui_test_canvas_smooth_scenario(&(struct ui_test_canvas_scenario) {
            .label = label,
            .threshold = thresholds[i],
            .a0 = make_double2(120.0, 200.0),
            .b0 = make_double2(280.0, 200.0),
            .a_step = make_double2(-1.0, 0.0),
            .b_step = make_double2(1.0, 0.0),
            .frames = 60,
            .limits = pinch_limits,
        });

        snprintf(label, sizeof(label), "simultaneous pan t=%.0f", thresholds[i]);
        ui_test_canvas_smooth_scenario(&(struct ui_test_canvas_scenario) {
            .label = label,
            .threshold = thresholds[i],
            .a0 = make_double2(150.0, 150.0),
            .b0 = make_double2(250.0, 250.0),
            .a_step = make_double2(1.0, 1.0),
            .b_step = make_double2(1.0, 1.0),
            .frames = 60,
            .limits = pan_limits,
        });

        snprintf(label, sizeof(label), "coincident pinch t=%.0f", thresholds[i]);
        ui_test_canvas_smooth_scenario(&(struct ui_test_canvas_scenario) {
            .label = label,
            .threshold = thresholds[i],
            .a0 = make_double2(200.0, 200.0),
            .b0 = make_double2(200.0, 200.0),
            .a_step = make_double2(-1.0, 0.0),
            .b_step = make_double2(1.0, 0.0),
            .frames = 60,
            .limits = coincident_limits,
        });

        snprintf(label, sizeof(label), "close 45deg pan+zoom t=%.0f", thresholds[i]);
        ui_test_canvas_smooth_scenario(&(struct ui_test_canvas_scenario) {
            .label = label,
            .threshold = thresholds[i],
            .a0 = make_double2(200.0, 200.0),
            .b0 = make_double2(217.0, 217.0),
            .a_step = make_double2(0.5, 0.5),
            .b_step = make_double2(0.9, 0.9),
            .frames = 90,
            .limits = coincident_limits,
        });
    }

    ui_test_canvas_smooth_scenario(&(struct ui_test_canvas_scenario) {
        .label = "deferred burst drift t=16",
        .threshold = 16.0,
        .a0 = make_double2(320.0, 300.0),
        .b0 = make_double2(240.0, 240.0),
        .a_step = make_double2(0.3, -1.2),
        .b_step = make_double2(0.0, 0.0),
        .frames = 60,
        .limits = { .translation = 6.0, .scale_lo = 0.95, .scale_hi = 1.05, .rotation = 0.05 },
    });

    ui_test_canvas_smooth_scenario(&(struct ui_test_canvas_scenario) {
        .label = "staggered pinch t=24",
        .threshold = 24.0,
        .a0 = make_double2(120.0, 200.0),
        .b0 = make_double2(280.0, 200.0),
        .a_step = make_double2(-1.0, 0.0),
        .b_step = make_double2(1.0, 0.0),
        .stagger_frames = 6,
        .frames = 60,
        .limits = pinch_limits,
    });
}

struct ui_test_canvas_fixture {
    struct ui_node root;
    struct ui_node zone;
};

static struct ui_test_canvas_fixture ui_test_canvas_smoothing_fixture(void) {
    struct ui_test_canvas_fixture fixture;

    fixture.root = ui_frame(&(struct ui_frame) {
        .size = ui_size_fix(400.0f, 400.0f),
    });

    fixture.zone = ui_frame(&(struct ui_frame) {
        .parent = fixture.root,
        .key = ui_key("zone"),
        .pos = ui_pos_xy(0.0f, 0.0f),
        .size = ui_size_fix(400.0f, 400.0f),
    });

    return fixture;
}

static void ui_test_canvas_quantized_creep(void) {
    struct ui_test_canvas canvas = {
        .translation = make_double2(320.0, 240.0),
        .scale = 1.0,
    };

    struct ui_test_canvas_fixture fixture = ui_test_canvas_smoothing_fixture();
    struct ui_node root = fixture.root;

    ui_test_canvas_gestures(fixture.zone, &canvas, ui_key_auto);
    ui_layout(fixture.root);

    struct ui_test_touch_emu emu = ui_test_touch_emu(root, 0.0, 30.0);
    double dt = 1.0 / 120.0;

    double2 a = make_double2(120.0, 200.0);
    ui_test_touch_emu_down(&emu, 0x9001, a);
    ui_test_touch_emu_down(&emu, 0x9002, make_double2(280.0, 200.0));
    ui_test_touch_emu_flush(&emu, dt);

    double translation_max = 0.0;
    double scale_ratio_lo = 1.0;
    double scale_ratio_hi = 1.0;

    for (uint32_t i = 0; i < 360; i++) {
        struct ui_test_canvas prev = canvas;

        if (i % 12 == 0) {
            a.x -= 0.5;
            ui_test_touch_emu_move(&emu, 0x9001, a);
        }
        ui_test_touch_emu_flush(&emu, dt);

        if (i < 30) {
            continue;
        }

        double2 dtr = canvas.translation - prev.translation;
        translation_max = max_double(translation_max, sqrt(dtr.x * dtr.x + dtr.y * dtr.y));
        double ratio = canvas.scale / prev.scale;
        scale_ratio_lo = min_double(scale_ratio_lo, ratio);
        scale_ratio_hi = max_double(scale_ratio_hi, ratio);
    }

    char label[128];
    snprintf(label, sizeof(label), "quantized creep: translation steps glide (max %.3f)", translation_max);
    ui_test_check(translation_max <= 0.15, label);

    snprintf(label, sizeof(label), "quantized creep: scale steps glide (%.4f..%.4f)", scale_ratio_lo, scale_ratio_hi);
    ui_test_check(scale_ratio_lo >= 0.9985 && scale_ratio_hi <= 1.0015, label);

    snprintf(label, sizeof(label), "quantized creep: motion arrives (scale %.4f)", canvas.scale);
    ui_test_check(canvas.scale > 1.06 && canvas.scale < 1.11, label);

    ui_test_touch_emu_up(&emu, 0x9001);
    ui_test_touch_emu_up(&emu, 0x9002);
    ui_test_touch_emu_flush(&emu, dt);

    ui_release(root);
}

static void ui_test_canvas_smoothing_latency(void) {
    struct ui_test_canvas canvas = {
        .translation = make_double2(320.0, 240.0),
        .scale = 1.0,
    };

    struct ui_test_canvas_fixture fixture = ui_test_canvas_smoothing_fixture();
    struct ui_node root = fixture.root;

    struct ui_pan_gesture pan = ui_test_canvas_gestures(fixture.zone, &canvas, ui_key("pan"));
    ui_layout(fixture.root);

    struct ui_test_touch_emu emu = ui_test_touch_emu(root, 0.0, 40.0);
    double dt = 1.0 / 120.0;

    double2 a = make_double2(100.0, 150.0);
    double2 b = make_double2(100.0, 250.0);
    ui_test_touch_emu_down(&emu, 0xa001, a);
    ui_test_touch_emu_down(&emu, 0xa002, b);
    ui_test_touch_emu_flush(&emu, dt);

    double2 step = make_double2(4.0, 0.0);
    uint32_t frames = 50;
    double lag_max = 0.0;
    double2 first_move = a + step;

    for (uint32_t i = 0; i < frames; i++) {
        a += step;
        b += step;
        ui_test_touch_emu_move(&emu, 0xa001, a);
        ui_test_touch_emu_move(&emu, 0xa002, b);
        ui_test_touch_emu_flush(&emu, dt);

        if (i == 0) {
            continue;
        }

        double raw_mapped = a.x - first_move.x;
        double lag = raw_mapped - ui_pan_gesture_get_translation(pan, false).x;
        lag_max = max_double(lag_max, lag);
    }

    char label[128];
    snprintf(label, sizeof(label), "smoothing latency: fast pan lag bounded by clamp (max %.2f)", lag_max);
    ui_test_check(lag_max <= 1.6, label);

    ui_test_touch_emu_up(&emu, 0xa001);
    ui_test_touch_emu_up(&emu, 0xa002);
    ui_test_touch_emu_flush(&emu, dt);

    ui_release(root);
}

static void ui_test_gesture_drift(void) {

    struct ui_node root = ui_frame(&(struct ui_frame) {
        .size = ui_size_fix(400.0f, 400.0f),
    });

    struct ui_node zone = ui_frame(&(struct ui_frame) {
        .parent = root,
        .pos = ui_pos_xy(0.0f, 0.0f),
        .size = ui_size_fix(400.0f, 400.0f),
    });

    struct ui_pinch_gesture pinch = ui_pinch(&(struct ui_pinch) {
        .node = zone,
        .key = ui_key("pinch"),
    });
    struct ui_rotate_gesture rotate = ui_rotate(&(struct ui_rotate) {
        .node = zone,
        .key = ui_key("rotate"),
    });
    struct ui_pan_gesture pan = ui_pan(&(struct ui_pan) {
        .node = zone,
        .key = ui_key("pan"),
    });

    ui_layout(root);

    struct ui_test_touch_emu emu = ui_test_touch_emu(root, 0.0, 20.0);
    uint32_t frames = 720;
    double mid_scale = 0.0;

    ui_test_touch_emu_down(&emu, 0x8001, make_double2(260.0, 200.0));
    ui_test_touch_emu_down(&emu, 0x8002, make_double2(140.0, 200.0));
    ui_test_touch_emu_flush(&emu, 1.0 / 120.0);

    for (uint32_t k = 1; k <= frames; k++) {
        double s = (double)k / (double)frames;
        double2 center = make_double2(200.0 + 30.0 * (cos(2.0 * M_PI * s) - 1.0), 200.0 + 30.0 * sin(2.0 * M_PI * s));
        double radius = 60.0 + 20.0 * sin(2.0 * M_PI * s);
        double angle = 4.0 * M_PI * s;
        double2 arm = make_double2(radius * cos(angle), radius * sin(angle));

        ui_test_touch_emu_move(&emu, 0x8001, center + arm);
        ui_test_touch_emu_move(&emu, 0x8002, center - arm);
        ui_test_touch_emu_flush(&emu, 1.0 / 120.0);

        if (k == frames / 4) {
            mid_scale = ui_pinch_gesture_get_scale(pinch, true);
        }
    }

    ui_test_check(fabs(mid_scale - 80.0 / 60.0) < 1e-6, "drift loop scale tracks mid-loop");

    double scale = ui_pinch_gesture_get_scale(pinch, true);
    double rotation = ui_rotate_gesture_get_rotation(rotate, true);
    double2 translation = ui_pan_gesture_get_translation(pan, true);

    ui_test_check(fabs(scale - 1.0) < 1e-6, "drift loop scale returns to identity");
    ui_test_check(fabs(rotation - 4.0 * M_PI) < 1e-6, "drift loop rotation is two turns");
    ui_test_check(fabs(translation.x) < 1e-6 && fabs(translation.y) < 1e-6, "drift loop translation returns to zero");

    ui_test_touch_emu_up(&emu, 0x8001);
    ui_test_touch_emu_up(&emu, 0x8002);
    ui_test_touch_emu_flush(&emu, 1.0 / 120.0);

    ui_release(root);
}

static uint32_t ui_test_palm_taps;
static uint32_t ui_test_palm_pointer_events;

static void ui_test_palm_on_tap(struct ui_tap_gesture gesture, void* user) {
    if (ui_tap_gesture_get_phase(gesture) == ui_gesture_phase_recognized) {
        ui_test_palm_taps++;
    }
}

static void ui_test_palm_on_pointer(struct app_pointer_event event, struct ui_pointer pointer, void* user) {
    ui_test_palm_pointer_events++;
}

static void ui_test_palm_on_text(const struct app_event* event, void* user) {
}

static void ui_test_send_touch_ex(struct ui_node root, enum app_pointer_event_type type, uint64_t id, enum app_pointer_source source, double radius, double2 pos, double timestamp) {
    struct app_event event = ui_test_touch_event(type, id, pos, timestamp);
    event.pointer.source = source;
    event.pointer.radius = radius;
    ui_input_dispatch(root, timestamp, &event, 1, 1.0);
}

static void ui_test_palm(void) {
    ui_test_palm_taps = 0;
    ui_test_palm_pointer_events = 0;

    struct ui_node root = ui_frame(&(struct ui_frame) {
        .size = ui_size_fix(400.0f, 400.0f),
    });
    struct ui_node zone = ui_test_custom_zone(root);
    struct ui_tap_gesture tap = ui_tap(&(struct ui_tap) {
        .node = zone,
        .on_change = ui_test_palm_on_tap,
    });
    struct ui_pan_gesture pan = ui_pan(&(struct ui_pan) {
        .node = zone,
    });
    ui_on_pointer(&(struct ui_on_pointer) {
        .node = zone,
        .func = ui_test_palm_on_pointer,
    });
    struct ui_node field = ui_frame(&(struct ui_frame) {
        .parent = root,
        .key = ui_key("field"),
        .pos = ui_pos_xy(0.0f, 0.0f),
        .size = ui_size_fix(50.0f, 50.0f),
    });
    ui_on_text(&(struct ui_on_text) {
        .node = field,
        .func = ui_test_palm_on_text,
    });
    ui_layout(root);

    uint64_t field_key = ui_get_key(field);
    double palm_radius = ui_input_get_palm_radius();
    ui_test_check(palm_radius > 0.0, "palm rejection is on by default");
    ui_input_set_focus(field_key);
    ui_input_set_palm_radius(20.0);

    double2 pos = make_double2(200.0, 200.0);
    double t = 20.0;

    ui_test_send_touch_ex(root, app_pointer_event_type_touch_began, 91, app_pointer_source_direct, 30.0, pos, t);
    ui_test_send_touch_ex(root, app_pointer_event_type_touch_moved, 91, app_pointer_source_direct, 30.0, make_double2(260.0, 200.0), t + 0.05);
    ui_test_send_touch_ex(root, app_pointer_event_type_touch_moved, 91, app_pointer_source_direct, 30.0, make_double2(320.0, 200.0), t + 0.1);
    ui_test_check(ui_tap_gesture_has_touch(tap) == false && ui_pan_gesture_has_touch(pan) == false && ui_test_palm_pointer_events == 0, "a palm reaches no gesture and no pointer attachment");
    ui_test_check(ui_input_get_focus() == field_key, "a palm leaves focus alone");
    ui_test_send_touch_ex(root, app_pointer_event_type_touch_ended, 91, app_pointer_source_direct, 30.0, make_double2(320.0, 200.0), t + 0.15);
    ui_test_check(ui_test_palm_taps == 0, "a palm lifts without a tap");

    ui_test_send_touch_ex(root, app_pointer_event_type_touch_began, 92, app_pointer_source_direct, 10.0, pos, t + 1.0);
    ui_test_send_touch_ex(root, app_pointer_event_type_touch_ended, 92, app_pointer_source_direct, 10.0, pos, t + 1.1);
    ui_test_check(ui_test_palm_taps == 1 && ui_test_palm_pointer_events == 2 && ui_input_get_focus() == 0, "a fingertip passes and takes focus away");

    ui_test_send_touch_ex(root, app_pointer_event_type_touch_began, 93, app_pointer_source_pencil, 30.0, pos, t + 2.0);
    ui_test_send_touch_ex(root, app_pointer_event_type_touch_ended, 93, app_pointer_source_pencil, 30.0, pos, t + 2.1);
    ui_test_send_touch_ex(root, app_pointer_event_type_touch_began, 94, app_pointer_source_indirect, 30.0, pos, t + 3.0);
    ui_test_send_touch_ex(root, app_pointer_event_type_touch_ended, 94, app_pointer_source_indirect, 30.0, pos, t + 3.1);
    ui_test_check(ui_test_palm_taps == 3, "only direct touches can be palms");

    ui_input_set_palm_radius(0.0);
    ui_test_send_touch_ex(root, app_pointer_event_type_touch_began, 95, app_pointer_source_direct, 30.0, pos, t + 4.0);
    ui_test_send_touch_ex(root, app_pointer_event_type_touch_ended, 95, app_pointer_source_direct, 30.0, pos, t + 4.1);
    ui_test_check(ui_test_palm_taps == 4, "no palm radius means no palms");

    ui_input_set_palm_radius(20.0);
    for (uint32_t i = 0; i < 300; i++) {
        double ts = t + 5.0 + (double)i * 0.01;
        ui_test_send_touch_ex(root, app_pointer_event_type_touch_began, 1000 + i, app_pointer_source_direct, 30.0, pos, ts);
        ui_test_send_touch_ex(root, app_pointer_event_type_touch_ended, 1000 + i, app_pointer_source_direct, 30.0, pos, ts + 0.005);
    }
    ui_test_send_touch_ex(root, app_pointer_event_type_touch_began, 96, app_pointer_source_direct, 10.0, pos, t + 9.0);
    ui_test_send_touch_ex(root, app_pointer_event_type_touch_ended, 96, app_pointer_source_direct, 10.0, pos, t + 9.1);
    ui_test_check(ui_test_palm_taps == 5, "palms hold no pointer slots");

    ui_test_send_touch_ex(root, app_pointer_event_type_touch_began, 97, app_pointer_source_direct, 30.0, pos, t + 10.0);
    ui_test_send_touch_ex(root, app_pointer_event_type_touch_began, 97, app_pointer_source_direct, 10.0, pos, t + 10.1);
    ui_test_send_touch_ex(root, app_pointer_event_type_touch_ended, 97, app_pointer_source_direct, 10.0, pos, t + 10.2);
    ui_test_check(ui_test_palm_taps == 6, "a palm id can begin again as a fingertip");

    ui_test_send_touch_ex(root, app_pointer_event_type_touch_began, 98, app_pointer_source_direct, 30.0, pos, t + 11.0);
    struct app_event cancel_all = { .type = app_event_type_cancel_all };
    ui_input_dispatch(root, t + 11.1, &cancel_all, 1, 1.0);
    ui_test_send_touch_ex(root, app_pointer_event_type_touch_began, 98, app_pointer_source_direct, 10.0, pos, t + 11.2);
    ui_test_send_touch_ex(root, app_pointer_event_type_touch_ended, 98, app_pointer_source_direct, 10.0, pos, t + 11.3);
    ui_test_check(ui_test_palm_taps == 7 && ui_test_palm_pointer_events == 14, "cancel all has nothing to do for a palm");

    ui_input_set_palm_radius(palm_radius);
    ui_release(root);
    ui_input_dispatch(ui_node_null, t + 12.0, NULL, 0, 1.0);
}

static void ui_test_release_detaches(void) {
    uint32_t before = ui_get_node_alloc_count();

    struct ui_node root = ui_frame(&(struct ui_frame) {
        .size = ui_size_fix(100.0f, 100.0f),
    });

    struct ui_node child = ui_frame(&(struct ui_frame) {
        .parent = root,
        .size = ui_size_fix(50.0f, 50.0f),
    });

    ui_frame(&(struct ui_frame) {
        .parent = root,
        .size = ui_size_fix(25.0f, 25.0f),
    });

    ui_release(child);
    ui_test_check(ui_get_node_alloc_count() == before + 2, "release detaches from parent");

    ui_layout(root);
    ui_release(root);
    ui_test_check(ui_get_node_alloc_count() == before, "release detached tree balances");
}

void ui_test_input_(void) {

    ui_test_stale_gesture();
    ui_test_custom_gesture();
    ui_test_inline_gesture();
    ui_test_tap_reports_position();
    ui_test_two_finger_gestures();
    ui_test_pinch_two_touches_only();
    ui_test_pan_multi_finger();
    ui_test_pan_min_touches();
    ui_test_pan_direction_lock();
    ui_test_pan_survives_tap_commit();
    ui_test_pan_release_velocity();
    ui_test_scroll_release_timing();
    ui_test_scroll_catch();
    ui_test_scroll_content_touches();
    ui_test_hold_kinds();
    ui_test_hold_content_claim();
    ui_test_hold_second_touch();
    ui_test_hold_content_pan();
    ui_test_simultaneous_either_side();
    ui_test_canvas_gestures_smooth();
    ui_test_canvas_quantized_creep();
    ui_test_canvas_smoothing_latency();
    ui_test_gesture_drift();
    ui_test_palm();
    ui_test_release_detaches();

}

