#include "cr.h"
#include "ui.h"
#include "ui_input.h"
#include <math.h>
#include <string.h>

#define ui_pointer_capacity 128
#define ui_input_chain_capacity 128
#define ui_attachment_gestures_max 8
#define ui_pointer_deferred_gap 0.05
#define ui_pointer_deferred_tail 0.04

#define ui_pointer_smooth_f_min 1.5
#define ui_pointer_smooth_lag 1.0
#define ui_pointer_smooth_clamp 1.5
#define ui_pointer_smooth_v_tau 0.025
#define ui_pointer_smooth_epsilon (1.0 / 256.0)

#define cr_dict ui_input_dict
#define cr_dict_key uint64_t
#define cr_dict_value uint32_t
#define cr_dict_is_equal(a, b) (a == b)
#define cr_dict_hash(key) cr_hash(&key, sizeof(key), 0)
#include "cr_dict.h"

#define ui_pointer_velocity_taps 3
#define ui_pointer_velocity_min_dt 0.0005
#define ui_pointer_stop_speed 270.0

#define ui_palm_radius_default 90.0

static const double ui_pointer_velocity_weights[ui_pointer_velocity_taps] = { 0.05, 0.35, 0.60 };

struct ui_pointer_backing {
    struct ui_pointer handle;
    uint64_t id;
    struct app_pointer_event began;
    double2 current_pos;
    double2 velocity;
    double2 translation;
    double2 first_move_pos;
    double2 smooth_pos;
    double smooth_velocity;
    double first_move_timestamp;
    uint64_t first_move_batch;
    bool has_first_move;
    bool first_move_deferred;
    enum app_pointer_source source;
    double2 sample_pos;
    double sample_timestamp;
    double2 interval_velocity[ui_pointer_velocity_taps];
    uint32_t interval_count;
    bool has_sample;
    bool is_moving;
    uint64_t gestures[128];
    uint32_t gesture_count;
    uint64_t holder;
    uint64_t on_pointer_key;
    uint64_t held_on_pointer_key;
    bool is_touching;
    bool release_requested;
};

#define cr_pool ui_pointer_pool
#define cr_pool_item struct ui_pointer_backing
#define cr_pool_capacity ui_pointer_capacity
#define cr_pool_handle ui_pointer
#include "cr_pool.h"

struct ui_gesture {
    _Alignas(16) struct ui_input* input;
    struct ui_node node;
    uint64_t key;
    uint64_t build;
    enum ui_gesture_phase phase;
    bool claim;
    char name[ui_gesture_name_max];
    struct ui_gesture_desc desc;
};

struct ui_input_attachment {
    struct ui_node node;
    uint64_t key;
    uint64_t build;
    void (*on_pointer)(struct app_pointer_event event, struct ui_pointer pointer, void* user);
    void* on_pointer_user;
    void (*on_scroll)(struct app_scroll_event event, void* user);
    void* on_scroll_user;
    void (*on_text)(const struct app_event* event, void* user);
    void* on_text_user;
    enum app_cursor cursor;
    uint32_t gesture_count;
    uint64_t gestures[ui_attachment_gestures_max];
};

#define cr_arr ui_input_arr_attachment
#define cr_arr_item struct ui_input_attachment
#include "cr_arr.h"

#define cr_arr ui_input_arr_gesture
#define cr_arr_item struct ui_gesture*
#include "cr_arr.h"

struct ui_input {
    struct cr_mem* mem;
    struct ui_input_arr_attachment* attachments;
    struct ui_input_arr_gesture* gestures;
    struct ui_input_dict* dict_attachments;
    struct ui_input_dict* dict_gestures;
    uint64_t build;
    bool declared;
    struct ui_pointer pointers[ui_pointer_capacity];
    struct ui_pointer_pool pointer_pool;
    uint64_t batch;
    double batch_timestamp;
    enum app_cursor cursor;
    uint64_t focus_key;
    bool caret_rect_valid;
    double2 caret_origin_pt;
    double2 caret_size_pt;
    char* clipboard;
    uint64_t clipboard_size;
    bool clipboard_pending;
    double palm_radius;
};

static _Thread_local struct ui_input* ui_input_current = NULL;

static struct ui_input* ui_input_ctx(void) {
    cr_check(ui_input_current != NULL, "there is no current input context; call ui_input_set first");
    return ui_input_current;
}

static struct ui_pointer_backing* ui_pointer_check(struct ui_input* input, struct ui_pointer pointer) {
    uint32_t idx = ui_pointer_get_idx(pointer);
    cr_check(idx < cr_count(input->pointer_pool.items) && input->pointer_pool.items[idx].handle.raw == pointer.raw, "the pointer is not alive");
    return &input->pointer_pool.items[idx];
}

static struct ui_gesture* ui_input_find_gesture(struct ui_input* input, uint64_t key) {
    uint32_t index;
    if (ui_input_dict_get(input->dict_gestures, key, &index) == false) {
        return NULL;
    }
    return ui_input_arr_gesture_get(input->gestures, index);
}

static struct ui_input_attachment* ui_input_find_attachment_by_key(struct ui_input* input, uint64_t key) {
    uint32_t index;
    if (ui_input_dict_get(input->dict_attachments, key, &index) == false) {
        return NULL;
    }
    return ui_input_arr_attachment_get_ptr(input->attachments, index);
}

static struct ui_input_attachment* ui_input_find_attachment(struct ui_input* input, struct ui_node node) {
    struct ui_input_attachment* attachment = ui_input_find_attachment_by_key(input, ui_get_key(node));
    if (attachment == NULL || ui_node_is_equal(attachment->node, node) == false) {
        return NULL;
    }
    return attachment;
}

static struct ui_input_attachment* ui_input_find_focused(struct ui_input* input) {
    if (input->focus_key == 0) {
        return NULL;
    }
    struct ui_input_attachment* attachment = ui_input_find_attachment_by_key(input, input->focus_key);
    if (attachment == NULL || attachment->on_text == NULL) {
        return NULL;
    }
    return attachment;
}

static void ui_gesture_cancel(struct ui_gesture* gesture) {
    bool active = gesture->phase == ui_gesture_phase_began || gesture->phase == ui_gesture_phase_changed;

    gesture->phase = active ? ui_gesture_phase_canceled : ui_gesture_phase_failed;

    if (gesture->desc.cancel) {
        gesture->desc.cancel(gesture);
    }
}

static void ui_gesture_reset(struct ui_gesture* gesture) {
    gesture->phase = ui_gesture_phase_possible;

    if (gesture->desc.reset) {
        gesture->desc.reset(gesture);
    }
}

static void ui_gesture_settle(struct ui_gesture* gesture) {
    if (gesture->phase == ui_gesture_phase_recognized ||
        gesture->phase == ui_gesture_phase_canceled ||
        gesture->phase == ui_gesture_phase_ended) {
        ui_gesture_reset(gesture);
    }
}

static void ui_gesture_remove_pointer(struct ui_gesture* gesture, struct ui_pointer pointer) {
    if (gesture->desc.remove_pointer) {
        gesture->desc.remove_pointer(gesture, pointer);
    } else {
        ui_gesture_cancel(gesture);
    }
}

static bool ui_gesture_is_simultaneous_with(struct ui_gesture* gesture, struct ui_gesture* other) {
    return (gesture->desc.simultaneous && gesture->desc.simultaneous(gesture, other)) ||
           (other->desc.simultaneous && other->desc.simultaneous(other, gesture));
}

static void ui_input_gesture_destroy(struct ui_input* input, struct ui_gesture* gesture) {
    ui_gesture_cancel(gesture);

    if (gesture->desc.destroy) {
        gesture->desc.destroy(gesture);
    }

    ui_input_dict_remove(input->dict_gestures, gesture->key);
    cr_free(input->mem, gesture, cr_loc);
}

void ui_input_set(struct ui_input* input) {
    ui_input_current = input;
}

struct ui_input* ui_input_get(void) {
    return ui_input_current;
}

struct ui_input* ui_input_create(struct cr_mem* mem) {
    struct ui_input* input = cr_malloc(mem, sizeof(*input), 0, cr_loc);

    memset(input, 0, sizeof(*input));
    input->mem = mem;
    input->attachments = ui_input_arr_attachment_create(mem);
    input->gestures = ui_input_arr_gesture_create(mem);
    input->dict_attachments = ui_input_dict_create(mem);
    input->dict_gestures = ui_input_dict_create(mem);
    ui_pointer_pool_init(&input->pointer_pool);
    input->palm_radius = ui_palm_radius_default;

    return input;
}

void ui_input_destroy(struct ui_input* input) {
    uint32_t gesture_count = ui_input_arr_gesture_get_count(input->gestures);
    struct ui_gesture** gestures = ui_input_arr_gesture_get_data(input->gestures);
    struct ui_input* prev = ui_input_current;

    ui_input_current = input;

    for (uint32_t i = 0; i < gesture_count; i++) {
        ui_input_gesture_destroy(input, gestures[i]);
    }

    ui_input_current = prev == input ? NULL : prev;

    ui_input_arr_gesture_destroy(input->gestures);
    ui_input_arr_attachment_destroy(input->attachments);
    ui_input_dict_destroy(input->dict_gestures);
    ui_input_dict_destroy(input->dict_attachments);

    if (input->clipboard) {
        cr_free(input->mem, input->clipboard, cr_loc);
    }
    cr_free(input->mem, input, cr_loc);
}

static struct ui_input_attachment* ui_input_declare(struct ui_input* input, struct ui_node node) {
    cr_check(ui_node_is_null(node) == false, "an attachment needs a node");

    uint64_t key = ui_get_key(node);
    struct ui_input_attachment* attachment;
    uint32_t index;

    input->declared = true;

    if (ui_input_dict_get(input->dict_attachments, key, &index)) {
        attachment = ui_input_arr_attachment_get_ptr(input->attachments, index);

        if (attachment->build == input->build && ui_is_alive(attachment->node)) {
            cr_check(ui_node_is_equal(attachment->node, node), "two attached nodes share a key; name them or their roots distinctly");
            return attachment;
        }
    } else {
        index = ui_input_arr_attachment_get_count(input->attachments);
        attachment = ui_input_arr_attachment_append_ptr(input->attachments, 1);
        ui_input_dict_set(input->dict_attachments, key, index);
    }

    memset(attachment, 0, sizeof(*attachment));
    attachment->node = node;
    attachment->key = key;
    attachment->build = input->build;

    return attachment;
}

struct ui_gesture* ui_gesture(const struct ui_gesture_desc* desc) {
    struct ui_input* input = ui_input_ctx();
    struct ui_input_attachment* attachment = ui_input_declare(input, desc->node);

    const char* name = desc->name ? desc->name : "";
    cr_check(strlen(name) < ui_gesture_name_max, "a gesture name holds at most %u bytes", (uint32_t)(ui_gesture_name_max - 1));

    uint64_t raw[2] = { ui_derive_key(desc->node, desc->key), cr_hash(name, strlen(name), 0) };
    uint64_t key = ui_key_u64(cr_hash(raw, sizeof(raw), 0)).raw;

    struct ui_gesture* gesture = ui_input_find_gesture(input, key);
    bool is_new = gesture == NULL;

    if (is_new) {
        uint64_t total = sizeof(struct ui_gesture) + desc->state_size;
        gesture = cr_malloc(input->mem, total, 16, cr_loc);

        memset(gesture, 0, (size_t)total);
        gesture->input = input;
        gesture->key = key;
        memcpy(gesture->name, name, strlen(name) + 1);

        ui_input_dict_set(input->dict_gestures, key, ui_input_arr_gesture_get_count(input->gestures));
        ui_input_arr_gesture_append(input->gestures, gesture);
    } else {
        bool declared = gesture->build == input->build && ui_is_alive(gesture->node);
        cr_check(declared == false, "a gesture was declared twice on a node; name the gestures distinctly");
    }

    gesture->build = input->build;
    gesture->node = desc->node;
    gesture->desc = *desc;
    gesture->desc.name = gesture->name;

    if (is_new && gesture->desc.create) {
        gesture->desc.create(gesture);
    }

    cr_check(attachment->gesture_count < ui_attachment_gestures_max, "a node holds at most %u gestures", (uint32_t)ui_attachment_gestures_max);
    attachment->gestures[attachment->gesture_count++] = key;

    return gesture;
}

struct ui_input* ui_gesture_get_input(const struct ui_gesture* gesture) {
    return gesture->input;
}

struct ui_node ui_gesture_get_node(const struct ui_gesture* gesture) {
    return gesture->node;
}

uint64_t ui_gesture_get_key(const struct ui_gesture* gesture) {
    return gesture->key;
}

const char* ui_gesture_get_name(const struct ui_gesture* gesture) {
    return gesture->name;
}

void* ui_gesture_get_state(struct ui_gesture* gesture) {
    return gesture + 1;
}

void ui_gesture_release(struct ui_gesture* gesture, struct ui_pointer pointer) {
    if (ui_pointer_is_null(pointer)) {
        return;
    }

    struct ui_pointer_backing* item = ui_pointer_check(gesture->input, pointer);

    if (item->holder == gesture->key) {
        item->release_requested = true;
    }
}

void ui_gesture_claim(struct ui_gesture* gesture) {
    gesture->claim = true;
}

enum ui_gesture_phase ui_gesture_get_phase(const struct ui_gesture* gesture) {
    return gesture->phase;
}

void ui_gesture_set_phase(struct ui_gesture* gesture, enum ui_gesture_phase phase) {
    gesture->phase = phase;
}

void ui_on_pointer(const struct ui_on_pointer* on_pointer) {
    cr_check(on_pointer->func != NULL, "on_pointer needs a func");

    struct ui_input_attachment* attachment = ui_input_declare(ui_input_ctx(), on_pointer->node);
    cr_check(attachment->on_pointer == NULL, "on_pointer was declared twice on a node");

    attachment->on_pointer = on_pointer->func;
    attachment->on_pointer_user = on_pointer->user;
}

void ui_on_scroll(const struct ui_on_scroll* on_scroll) {
    cr_check(on_scroll->func != NULL, "on_scroll needs a func");

    struct ui_input_attachment* attachment = ui_input_declare(ui_input_ctx(), on_scroll->node);
    cr_check(attachment->on_scroll == NULL, "on_scroll was declared twice on a node");

    attachment->on_scroll = on_scroll->func;
    attachment->on_scroll_user = on_scroll->user;
}

void ui_on_text(const struct ui_on_text* on_text) {
    cr_check(on_text->func != NULL, "on_text needs a func");

    struct ui_input_attachment* attachment = ui_input_declare(ui_input_ctx(), on_text->node);
    cr_check(attachment->on_text == NULL, "on_text was declared twice on a node");

    attachment->on_text = on_text->func;
    attachment->on_text_user = on_text->user;
}

void ui_cursor(const struct ui_cursor* cursor) {
    struct ui_input_attachment* attachment = ui_input_declare(ui_input_ctx(), cursor->node);

    attachment->cursor = cursor->cursor;
}

static void ui_input_sweep(struct ui_input* input) {
    bool declared = input->declared;
    input->declared = false;

    struct ui_gesture** gestures = ui_input_arr_gesture_get_data(input->gestures);
    uint32_t gesture_count = ui_input_arr_gesture_get_count(input->gestures);

    for (uint32_t i = 0; i < gesture_count;) {
        struct ui_gesture* gesture = gestures[i];

        if (ui_is_alive(gesture->node) && (declared == false || gesture->build == input->build)) {
            i++;
            continue;
        }

        ui_input_gesture_destroy(input, gesture);

        gesture_count--;
        if (i != gesture_count) {
            gestures[i] = gestures[gesture_count];
            ui_input_dict_set(input->dict_gestures, gestures[i]->key, i);
        }
        ui_input_arr_gesture_remove(input->gestures, gesture_count);
    }

    struct ui_input_attachment* attachments = ui_input_arr_attachment_get_data(input->attachments);
    uint32_t attachment_count = ui_input_arr_attachment_get_count(input->attachments);

    for (uint32_t i = 0; i < attachment_count;) {
        struct ui_input_attachment* attachment = &attachments[i];

        if (ui_is_alive(attachment->node) && (declared == false || attachment->build == input->build)) {
            i++;
            continue;
        }

        ui_input_dict_remove(input->dict_attachments, attachment->key);

        attachment_count--;
        if (i != attachment_count) {
            *attachment = attachments[attachment_count];
            ui_input_dict_set(input->dict_attachments, attachment->key, i);
        }
        ui_input_arr_attachment_remove(input->attachments, attachment_count);
    }

    if (declared) {
        input->build++;
    }

    if (input->focus_key != 0 && ui_input_find_focused(input) == NULL) {
        input->focus_key = 0;
    }
}

static void ui_pointer_push_sample(struct ui_pointer_backing* pointer, double2 pos, double timestamp) {
    if (pointer->has_sample == false) {
        pointer->sample_pos = pos;
        pointer->sample_timestamp = timestamp;
        pointer->has_sample = true;
        return;
    }

    double dt = timestamp - pointer->sample_timestamp;

    if (dt >= ui_pointer_velocity_min_dt) {
        for (uint32_t i = ui_pointer_velocity_taps - 1; i > 0; i--) {
            pointer->interval_velocity[i] = pointer->interval_velocity[i - 1];
        }
        pointer->interval_velocity[0] = (pos - pointer->sample_pos) / dt;
        pointer->interval_count = min_uint32(pointer->interval_count + 1, ui_pointer_velocity_taps);
        pointer->sample_pos = pos;
        pointer->sample_timestamp = timestamp;
    }

    double2 velocity = 0.0;
    double weight = 0.0;

    pointer->is_moving = false;

    for (uint32_t i = 0; i < pointer->interval_count; i++) {
        double2 v = pointer->interval_velocity[i];

        velocity += v * ui_pointer_velocity_weights[i];
        weight += ui_pointer_velocity_weights[i];

        if (i < 2 && v.x * v.x + v.y * v.y >= ui_pointer_stop_speed * ui_pointer_stop_speed) {
            pointer->is_moving = true;
        }
    }

    pointer->velocity = weight > 0.0 ? velocity / weight : 0.0;
}

static struct ui_pointer_backing* ui_input_find_pointer(struct ui_input* input, uint64_t id) {
    for (int i = 0; i < (int)cr_count(input->pointers); i++) {
        if (ui_pointer_is_null(input->pointers[i])) {
            continue;
        }
        struct ui_pointer_backing* pointer = ui_pointer_pool_item(&input->pointer_pool, input->pointers[i]);
        if (pointer->id == id) {
            return pointer;
        }
    }

    return NULL;
}

static void ui_input_dealloc_pointer(struct ui_input* input, struct ui_pointer_backing* pointer) {
    struct ui_pointer handle = pointer->handle;
    ui_pointer_pool_dealloc(&input->pointer_pool, pointer);

    for (int i = 0; i < (int)cr_count(input->pointers); i++) {
        if (ui_pointer_is_equal(input->pointers[i], handle)) {
            input->pointers[i] = ui_pointer_null;
            break;
        }
    }
}

static bool ui_input_gesture_has_touching_pointer(struct ui_input* input, uint64_t key) {
    for (int i = 0; i < (int)cr_count(input->pointers); i++) {
        if (ui_pointer_is_null(input->pointers[i])) {
            continue;
        }

        struct ui_pointer_backing* pointer = ui_pointer_pool_item(&input->pointer_pool, input->pointers[i]);

        if (pointer->is_touching == false) {
            continue;
        }

        for (uint32_t j = 0; j < pointer->gesture_count; j++) {
            if (pointer->gestures[j] == key) {
                return true;
            }
        }
    }

    return false;
}

static void ui_pointer_commit_gesture(struct ui_input* input, struct ui_pointer_backing* pointer, uint64_t recognized_gesture_key, double timestamp) {
    struct ui_gesture* recognized = ui_input_find_gesture(input, recognized_gesture_key);

    if (pointer->on_pointer_key != 0) {
        struct ui_input_attachment* attachment = ui_input_find_attachment_by_key(input, pointer->on_pointer_key);
        if (attachment && attachment->on_pointer) {
            struct app_pointer_event cancel_event;
            memset(&cancel_event, 0, sizeof(cancel_event));
            cancel_event.type = app_pointer_event_type_canceled;
            cancel_event.source = pointer->source;
            cancel_event.id = pointer->id;
            cancel_event.pos = pointer->current_pos;
            cancel_event.timestamp = timestamp;
            attachment->on_pointer(cancel_event, pointer->handle, attachment->on_pointer_user);
        }
        pointer->on_pointer_key = 0;
    }

    for (uint32_t i = 0; i < pointer->gesture_count; i++) {
        uint64_t other_key = pointer->gestures[i];
        if (other_key == 0 || other_key == recognized_gesture_key) {
            continue;
        }

        struct ui_gesture* other = ui_input_find_gesture(input, other_key);
        if (other == NULL) {
            pointer->gestures[i] = 0;
            continue;
        }

        if (recognized && ui_gesture_is_simultaneous_with(recognized, other)) {
            continue;
        }

        ui_gesture_remove_pointer(other, pointer->handle);
        ui_gesture_settle(other);

        pointer->gestures[i] = 0;
    }

    pointer->held_on_pointer_key = 0;
}

static bool ui_pointer_dispatch_gesture(struct ui_input* input, struct ui_pointer_backing* pointer, uint32_t gesture_index, struct app_pointer_event event) {
    uint64_t gesture_key = pointer->gestures[gesture_index];
    if (gesture_key == 0) {
        return false;
    }

    struct ui_gesture* gesture = ui_input_find_gesture(input, gesture_key);
    if (gesture == NULL) {
        pointer->gestures[gesture_index] = 0;
        return false;
    }

    if (event.type == app_pointer_event_type_canceled) {
        ui_gesture_remove_pointer(gesture, pointer->handle);
        return false;
    }

    if (gesture->desc.on_pointer) {
        gesture->desc.on_pointer(gesture, event, pointer->handle);
    }

    bool claim = gesture->claim;
    gesture->claim = false;

    if (gesture->phase == ui_gesture_phase_recognized || gesture->phase == ui_gesture_phase_began || claim) {
        ui_pointer_commit_gesture(input, pointer, gesture_key, event.timestamp);
        ui_gesture_settle(gesture);
        return true;
    }

    return false;
}

static bool ui_pointer_deliver(struct ui_input* input, struct ui_pointer_backing* pointer, struct app_pointer_event event, uint32_t first, uint32_t last, uint64_t on_pointer_key) {
    bool committed = false;

    for (uint32_t i = first; i < last; i++) {
        if (ui_pointer_dispatch_gesture(input, pointer, i, event)) {
            committed = true;
        }
    }

    if (committed || on_pointer_key == 0) {
        return committed;
    }

    pointer->on_pointer_key = on_pointer_key;

    struct ui_input_attachment* attachment = ui_input_find_attachment_by_key(input, on_pointer_key);
    if (attachment && attachment->on_pointer) {
        attachment->on_pointer(event, pointer->handle, attachment->on_pointer_user);
    }

    return false;
}

struct ui_input_hit_query {
    struct ui_input* input;
    bool scroll;
};

static bool ui_input_hit_is_target(struct ui_node node, void* user) {
    struct ui_input_hit_query* query = user;
    struct ui_input_attachment* attachment = ui_input_find_attachment(query->input, node);

    if (attachment == NULL) {
        return false;
    }
    if (attachment->gesture_count > 0) {
        return true;
    }
    if (query->scroll) {
        return attachment->on_scroll != NULL;
    }
    return attachment->on_pointer != NULL || attachment->cursor != app_cursor_default;
}

struct ui_input_hit_result {
    uint64_t gestures[128];
    uint32_t gesture_count;
    uint64_t holder;
    uint64_t on_pointer_key;
    uint64_t held_on_pointer_key;
    enum app_cursor cursor;
};

static void ui_input_hit(struct ui_input* input, struct ui_node root, struct app_pointer_event event, struct ui_pointer handle, struct ui_input_hit_result* out) {
    memset(out, 0, sizeof(*out));

    struct ui_input_hit_query query = {
        .input = input,
    };

    struct ui_hit chain[ui_input_chain_capacity];
    uint32_t chain_count = ui_hit_test(root, event.pos, ui_input_hit_is_target, &query, chain, (uint32_t)cr_count(chain));
    bool holds = event.type == app_pointer_event_type_touch_began && event.source != app_pointer_source_indirect;
    uint32_t on_pointer_depth = chain_count;
    uint32_t holder_depth = chain_count;

    for (uint32_t i = 0; i < chain_count; i++) {
        struct ui_input_attachment* attachment = ui_input_find_attachment(input, chain[i].node);
        if (attachment == NULL) {
            continue;
        }

        for (uint32_t j = 0; j < attachment->gesture_count; j++) {
            struct ui_gesture* gesture = ui_input_find_gesture(input, attachment->gestures[j]);
            if (gesture == NULL) {
                continue;
            }
            enum ui_gesture_decision decision = gesture->desc.accepts ? gesture->desc.accepts(gesture, event, handle) : ui_gesture_decision_accept;
            if (decision == ui_gesture_decision_reject) {
                continue;
            }

            if (holds && decision == ui_gesture_decision_hold && out->holder == 0) {
                out->holder = gesture->key;
                holder_depth = i;
            }

            assert(out->gesture_count < (uint32_t)cr_count(out->gestures));
            out->gestures[out->gesture_count++] = gesture->key;
        }

        if (out->on_pointer_key == 0 && attachment->on_pointer) {
            out->on_pointer_key = attachment->key;
            on_pointer_depth = i;
        }
        if (out->cursor == app_cursor_default && attachment->cursor != app_cursor_default && chain[i].contains) {
            out->cursor = attachment->cursor;
        }
    }

    if (out->holder != 0 && on_pointer_depth < holder_depth) {
        out->held_on_pointer_key = out->on_pointer_key;
        out->on_pointer_key = 0;
    }
}

static struct ui_input_attachment* ui_input_hit_scroll(struct ui_input* input, struct ui_node root, double2 pos) {
    struct ui_input_hit_query query = {
        .input = input,
        .scroll = true,
    };

    struct ui_hit chain[ui_input_chain_capacity];
    uint32_t chain_count = ui_hit_test(root, pos, ui_input_hit_is_target, &query, chain, (uint32_t)cr_count(chain));

    for (uint32_t i = 0; i < chain_count; i++) {
        struct ui_input_attachment* attachment = ui_input_find_attachment(input, chain[i].node);
        if (attachment && attachment->on_scroll) {
            return attachment;
        }
    }

    return NULL;
}

static bool ui_pointer_holds(const struct ui_pointer_backing* pointer) {
    return pointer->held_on_pointer_key != 0;
}

static bool ui_pointer_hold_expired(struct ui_input* input, struct ui_pointer_backing* pointer) {
    struct ui_gesture* holder = ui_input_find_gesture(input, pointer->holder);
    return holder == NULL || holder->phase == ui_gesture_phase_failed;
}

static void ui_pointer_release_hold(struct ui_input* input, struct ui_pointer_backing* pointer, double timestamp) {
    uint64_t on_pointer_key = pointer->held_on_pointer_key;

    pointer->held_on_pointer_key = 0;
    pointer->release_requested = false;

    ui_pointer_deliver(input, pointer, pointer->began, 0, 0, on_pointer_key);

    if (is_equal_double2(pointer->current_pos, pointer->began.pos)) {
        return;
    }

    struct app_pointer_event moved = pointer->began;
    moved.type = app_pointer_event_type_touch_moved;
    moved.pos = pointer->current_pos;
    moved.timestamp = timestamp;

    ui_pointer_deliver(input, pointer, moved, 0, 0, on_pointer_key);
}

static void ui_input_handle_pointer(struct ui_input* input, struct ui_node root, struct app_pointer_event event) {
    cr_check(event.id != 0, "pointer events need an id other than 0");

    struct ui_pointer_backing* pointer = ui_input_find_pointer(input, event.id);

    if (event.type == app_pointer_event_type_touch_began && pointer && pointer->is_touching) {
        struct app_pointer_event cancel_event = event;
        cancel_event.type = app_pointer_event_type_canceled;
        cancel_event.pos = pointer->current_pos;
        ui_input_handle_pointer(input, root, cancel_event);
        pointer = ui_input_find_pointer(input, event.id);
    }

    bool is_touching = pointer ? pointer->is_touching : false;
    bool is_cancel = event.type == app_pointer_event_type_canceled;

    bool is_hover = (event.type == app_pointer_event_type_hover_began ||
                     event.type == app_pointer_event_type_hover_moved ||
                     event.type == app_pointer_event_type_hover_ended ||
                     (is_cancel && !is_touching));

    bool is_touch = (event.type == app_pointer_event_type_touch_began ||
                     event.type == app_pointer_event_type_touch_moved ||
                     event.type == app_pointer_event_type_touch_ended ||
                     (is_cancel && is_touching));

    if (event.type == app_pointer_event_type_touch_began) {
        if (pointer && !pointer->is_touching) {
            struct app_pointer_event hover_end_event = event;
            hover_end_event.type = app_pointer_event_type_hover_ended;
            hover_end_event.timestamp = event.timestamp;

            for (uint32_t i = 0; i < pointer->gesture_count; i++) {
                ui_pointer_dispatch_gesture(input, pointer, i, hover_end_event);
            }
            pointer->gesture_count = 0;
        }
    }

    bool wants_pointer = event.type == app_pointer_event_type_touch_began ||
                         ((event.type == app_pointer_event_type_hover_began ||
                           event.type == app_pointer_event_type_hover_moved) &&
                          event.source != app_pointer_source_direct);

    if (pointer == NULL && wants_pointer) {
        for (int i = 0; i < (int)cr_count(input->pointers); i++) {
            if (ui_pointer_is_null(input->pointers[i])) {
                pointer = ui_pointer_pool_alloc(&input->pointer_pool);
                input->pointers[i] = pointer->handle;
                break;
            }
        }
    }

    if (pointer == NULL) {
        return;
    }

    bool pos_changed = !is_equal_double2(pointer->current_pos, event.pos);

    pointer->id = event.id;
    pointer->source = event.source;
    pointer->current_pos = event.pos;

    if (is_hover) {
        if (pointer->is_touching || event.source == app_pointer_source_direct) {
            return;
        }

        struct ui_input_hit_result hit;
        memset(&hit, 0, sizeof(hit));

        if (event.type != app_pointer_event_type_hover_ended && event.type != app_pointer_event_type_canceled) {
            ui_input_hit(input, root, event, pointer->handle, &hit);
        }

        input->cursor = hit.cursor;

        uint64_t old_gestures[cr_count(pointer->gestures)];
        uint32_t old_gesture_count = pointer->gesture_count;
        for (uint32_t i = 0; i < old_gesture_count; i++) {
            old_gestures[i] = pointer->gestures[i];
        }

        for (uint32_t i = 0; i < old_gesture_count; i++) {
            uint64_t old_gesture = old_gestures[i];
            if (old_gesture == 0) {
                continue;
            }

            bool found = false;
            for (uint32_t j = 0; j < hit.gesture_count; j++) {
                if (hit.gestures[j] == old_gesture) {
                    found = true;
                    break;
                }
            }

            if (!found) {
                struct ui_gesture* gesture = ui_input_find_gesture(input, old_gesture);
                if (gesture && gesture->desc.on_pointer) {
                    struct app_pointer_event end_event = event;
                    end_event.type = app_pointer_event_type_hover_ended;
                    gesture->desc.on_pointer(gesture, end_event, pointer->handle);
                }
            }
        }

        pointer->gesture_count = hit.gesture_count;
        for (uint32_t i = 0; i < hit.gesture_count; i++) {
            pointer->gestures[i] = hit.gestures[i];
        }

        for (uint32_t i = 0; i < pointer->gesture_count; i++) {
            uint64_t new_gesture = pointer->gestures[i];
            if (new_gesture == 0) {
                continue;
            }

            bool found = false;
            for (uint32_t j = 0; j < old_gesture_count; j++) {
                if (old_gestures[j] == new_gesture) {
                    found = true;
                    break;
                }
            }

            struct app_pointer_event dispatch_event = event;
            bool should_dispatch = true;

            if (!found) {
                dispatch_event.type = app_pointer_event_type_hover_began;
            } else if (event.type == app_pointer_event_type_hover_moved) {
                if (pos_changed) {
                    dispatch_event.type = app_pointer_event_type_hover_moved;
                } else {
                    should_dispatch = false;
                }
            }

            if (should_dispatch) {
                ui_pointer_dispatch_gesture(input, pointer, i, dispatch_event);
            }
        }

        if (event.type == app_pointer_event_type_hover_ended || event.type == app_pointer_event_type_canceled) {
            ui_input_dealloc_pointer(input, pointer);
        }
    } else if (is_touch) {
        uint64_t on_pointer_key;

        if (event.type == app_pointer_event_type_touch_began) {
            pointer->is_touching = true;
            pointer->began = event;
            pointer->translation = 0.0;
            pointer->has_first_move = false;
            pointer->smooth_pos = event.pos;
            pointer->smooth_velocity = 0.0;
            pointer->sample_timestamp = event.timestamp;
            pointer->interval_count = 0;
            pointer->velocity = 0.0;
            pointer->has_sample = false;
            pointer->is_moving = false;

            struct ui_input_hit_result hit;
            ui_input_hit(input, root, event, pointer->handle, &hit);

            pointer->gesture_count = hit.gesture_count;
            for (uint32_t i = 0; i < hit.gesture_count; i++) {
                pointer->gestures[i] = hit.gestures[i];
            }

            pointer->holder = hit.holder;
            pointer->held_on_pointer_key = hit.held_on_pointer_key;
            on_pointer_key = hit.on_pointer_key;
        } else {
            if (!pointer->is_touching || (event.type == app_pointer_event_type_touch_moved && !pos_changed)) {
                return;
            }

            pointer->translation = pointer->current_pos - pointer->began.pos;

            if (event.type == app_pointer_event_type_touch_moved) {
                ui_pointer_push_sample(pointer, event.pos, event.timestamp);

                bool swallow = false;

                if (!pointer->has_first_move) {
                    pointer->has_first_move = true;
                    pointer->first_move_batch = input->batch;
                    pointer->first_move_timestamp = event.timestamp;
                    pointer->first_move_deferred = event.timestamp - pointer->began.timestamp > ui_pointer_deferred_gap;
                    swallow = true;
                } else if (pointer->first_move_batch == input->batch ||
                           (pointer->first_move_deferred && event.timestamp - pointer->first_move_timestamp <= ui_pointer_deferred_tail)) {
                    swallow = true;
                }

                if (swallow) {
                    pointer->first_move_pos = pointer->current_pos;
                    pointer->smooth_pos = pointer->current_pos;
                    pointer->smooth_velocity = 0.0;
                }
            }

            if (event.type == app_pointer_event_type_touch_ended && ui_pointer_holds(pointer)) {
                ui_pointer_release_hold(input, pointer, event.timestamp);
            }

            on_pointer_key = pointer->on_pointer_key;
        }

        ui_pointer_deliver(input, pointer, event, 0, pointer->gesture_count, on_pointer_key);

        if (event.type == app_pointer_event_type_canceled) {
            ui_input_dealloc_pointer(input, pointer);
        } else if (event.type == app_pointer_event_type_touch_ended) {
            if (pointer->source == app_pointer_source_direct) {
                ui_input_dealloc_pointer(input, pointer);
            } else {
                pointer->is_touching = false;
                pointer->on_pointer_key = 0;
                pointer->gesture_count = 0;
            }
        }
    }
}

static void ui_input_handle_scroll(struct ui_input* input, struct ui_node root, struct app_scroll_event event) {
    struct ui_input_attachment* target = ui_input_hit_scroll(input, root, event.pos);

    if (target) {
        target->on_scroll(event, target->on_scroll_user);
    }
}

static void ui_input_dispatch_focused(struct ui_input* input, const struct app_event* event) {
    struct ui_input_attachment* focused = ui_input_find_focused(input);
    if (focused) {
        focused->on_text(event, focused->on_text_user);
    }
}

static struct app_event ui_input_event_scaled(struct app_event event, double to_pt_from_app) {
    switch (event.type) {
    case app_event_type_pointer: {
        event.pointer.pos *= to_pt_from_app;
        event.pointer.radius *= to_pt_from_app;
    } break;
    case app_event_type_scroll: {
        event.scroll.delta *= to_pt_from_app;
        event.scroll.pos *= to_pt_from_app;
    } break;
    case app_event_type_floating_caret: {
        event.floating_caret.pos *= to_pt_from_app;
    } break;
    default:
        break;
    }
    return event;
}

static bool ui_input_is_palm(struct ui_input* input, struct app_pointer_event event) {
    return event.source == app_pointer_source_direct && input->palm_radius > 0.0 && event.radius >= input->palm_radius;
}

void ui_input_dispatch(struct ui_node root, double timestamp, const struct app_event* events, uint32_t event_count, double to_pt_from_app) {
    struct ui_input* input = ui_input_ctx();

    ui_input_sweep(input);

    input->batch++;

    bool has_root = ui_node_is_null(root) == false;

    for (uint32_t i = 0; i < event_count; i++) {
        struct app_event event = to_pt_from_app == 1.0 ? events[i] : ui_input_event_scaled(events[i], to_pt_from_app);

        switch (event.type) {
        case app_event_type_pointer: {
            if (event.pointer.type == app_pointer_event_type_touch_began) {
                if (ui_input_is_palm(input, event.pointer)) {
                    break;
                }
                if (input->focus_key != 0 && ui_input_has_focusable_at(event.pointer.pos) == false) {
                    input->focus_key = 0;
                }
            }
            if (has_root) {
                ui_input_handle_pointer(input, root, event.pointer);
            }
        } break;
        case app_event_type_scroll: {
            if (has_root) {
                ui_input_handle_scroll(input, root, event.scroll);
            }
        } break;
        case app_event_type_cancel_all: {
            if (has_root == false) {
                break;
            }
            for (int j = 0; j < (int)cr_count(input->pointers); j++) {
                if (ui_pointer_is_null(input->pointers[j])) {
                    continue;
                }

                struct ui_pointer_backing* pointer = ui_pointer_pool_item(&input->pointer_pool, input->pointers[j]);

                struct app_pointer_event cancel_event;
                memset(&cancel_event, 0, sizeof(cancel_event));
                cancel_event.type = app_pointer_event_type_canceled;
                cancel_event.id = pointer->id;
                cancel_event.source = pointer->source;
                cancel_event.pos = pointer->current_pos;
                cancel_event.timestamp = timestamp;

                ui_input_handle_pointer(input, root, cancel_event);
            }
        } break;
        case app_event_type_text:
        case app_event_type_marked_text:
        case app_event_type_text_command:
        case app_event_type_floating_caret: {
            ui_input_dispatch_focused(input, &event);
        } break;
        case app_event_type_text_input_dismissed: {
            input->focus_key = 0;
        } break;
        default:
            break;
        }
    }

    if (has_root == false) {
        return;
    }

    for (int i = 0; i < (int)cr_count(input->pointers); i++) {
        if (ui_pointer_is_null(input->pointers[i])) {
            continue;
        }

        struct ui_pointer_backing* pointer = ui_pointer_pool_item(&input->pointer_pool, input->pointers[i]);

        if (!pointer->is_touching) {
            struct app_pointer_event synth_event;
            memset(&synth_event, 0, sizeof(synth_event));
            synth_event.type = app_pointer_event_type_hover_moved;
            synth_event.id = pointer->id;
            synth_event.source = pointer->source;
            synth_event.pos = pointer->current_pos;
            synth_event.timestamp = timestamp;

            ui_input_handle_pointer(input, root, synth_event);
        }
    }

    double smooth_dt = clamp_double(timestamp - input->batch_timestamp, 0.001, 1.0 / 30.0);
    input->batch_timestamp = timestamp;

    for (int i = 0; i < (int)cr_count(input->pointers); i++) {
        if (ui_pointer_is_null(input->pointers[i])) {
            continue;
        }

        struct ui_pointer_backing* pointer = ui_pointer_pool_item(&input->pointer_pool, input->pointers[i]);

        if (ui_pointer_holds(pointer) && ui_pointer_hold_expired(input, pointer)) {
            ui_pointer_release_hold(input, pointer, timestamp);
        }
        if (!pointer->is_touching || !pointer->has_first_move) {
            continue;
        }

        double2 target = pointer->current_pos;
        double2 before = pointer->smooth_pos;
        double2 gap = target - before;
        double2 smooth;

        if (fabs(gap.x) < ui_pointer_smooth_epsilon && fabs(gap.y) < ui_pointer_smooth_epsilon) {
            smooth = target;
        } else {
            double cutoff = max_double(ui_pointer_smooth_f_min, pointer->smooth_velocity / (2.0 * M_PI * ui_pointer_smooth_lag));
            double alpha = 1.0 / (1.0 + 1.0 / (2.0 * M_PI * cutoff * smooth_dt));
            smooth = before + gap * alpha;

            double2 excess = target - smooth;
            double excess_len = sqrt(excess.x * excess.x + excess.y * excess.y);
            if (excess_len > ui_pointer_smooth_clamp) {
                smooth += excess * ((excess_len - ui_pointer_smooth_clamp) / excess_len);
            }
        }

        double2 moved = smooth - before;
        double moved_len = sqrt(moved.x * moved.x + moved.y * moved.y);
        double alpha_v = 1.0 / (1.0 + ui_pointer_smooth_v_tau / smooth_dt);
        pointer->smooth_velocity += (moved_len / smooth_dt - pointer->smooth_velocity) * alpha_v;
        pointer->smooth_pos = smooth;

        if (moved.x == 0.0 && moved.y == 0.0) {
            continue;
        }

        struct app_pointer_event smooth_event;
        memset(&smooth_event, 0, sizeof(smooth_event));
        smooth_event.type = app_pointer_event_type_touch_moved;
        smooth_event.id = pointer->id;
        smooth_event.source = pointer->source;
        smooth_event.pos = target;
        smooth_event.timestamp = timestamp;

        for (uint32_t j = 0; j < pointer->gesture_count; j++) {
            ui_pointer_dispatch_gesture(input, pointer, j, smooth_event);
        }
    }

    struct ui_gesture** gestures = ui_input_arr_gesture_get_data(input->gestures);
    uint32_t gesture_count = ui_input_arr_gesture_get_count(input->gestures);

    for (uint32_t g = 0; g < gesture_count; g++) {
        struct ui_gesture* gesture = gestures[g];

        if (gesture->desc.on_update) {
            gesture->desc.on_update(gesture, timestamp);
        }

        bool claim = gesture->claim;
        gesture->claim = false;

        if (gesture->phase == ui_gesture_phase_recognized || gesture->phase == ui_gesture_phase_began || claim) {
            for (int i = 0; i < (int)cr_count(input->pointers); i++) {
                if (ui_pointer_is_null(input->pointers[i])) {
                    continue;
                }
                struct ui_pointer_backing* pointer = ui_pointer_pool_item(&input->pointer_pool, input->pointers[i]);

                bool pointer_has_gesture = false;
                for (uint32_t j = 0; j < pointer->gesture_count; j++) {
                    if (pointer->gestures[j] == gesture->key) {
                        pointer_has_gesture = true;
                        break;
                    }
                }

                if (pointer_has_gesture) {
                    ui_pointer_commit_gesture(input, pointer, gesture->key, timestamp);
                }
            }
        }

        if (gesture->phase == ui_gesture_phase_failed) {
            if (ui_input_gesture_has_touching_pointer(input, gesture->key) == false) {
                ui_gesture_reset(gesture);
            }
        } else {
            ui_gesture_settle(gesture);
        }
    }

    for (int i = 0; i < (int)cr_count(input->pointers); i++) {
        if (ui_pointer_is_null(input->pointers[i])) {
            continue;
        }

        struct ui_pointer_backing* pointer = ui_pointer_pool_item(&input->pointer_pool, input->pointers[i]);

        if (pointer->release_requested && ui_pointer_holds(pointer)) {
            ui_pointer_release_hold(input, pointer, timestamp);
        }
        pointer->release_requested = false;
    }
}

double2 ui_input_pointer_get_position(struct ui_pointer pointer) {
    struct ui_input* input = ui_input_ctx();
    return ui_pointer_check(input, pointer)->current_pos;
}

double2 ui_input_pointer_get_translation(struct ui_pointer pointer, bool map_direct) {
    struct ui_input* input = ui_input_ctx();
    struct ui_pointer_backing* item = ui_pointer_check(input, pointer);

    if (map_direct) {
        return item->translation;
    }

    if (item->has_first_move == false) {
        return 0.0;
    }

    return item->smooth_pos - item->first_move_pos;
}

double2 ui_input_pointer_get_velocity(struct ui_pointer pointer) {
    struct ui_input* input = ui_input_ctx();
    return ui_pointer_check(input, pointer)->velocity;
}

double ui_input_pointer_get_timestamp(struct ui_pointer pointer) {
    struct ui_input* input = ui_input_ctx();
    return ui_pointer_check(input, pointer)->sample_timestamp;
}

bool ui_input_pointer_is_moving(struct ui_pointer pointer) {
    struct ui_input* input = ui_input_ctx();
    return ui_pointer_check(input, pointer)->is_moving;
}

struct ui_gesture* ui_input_get_gesture(uint64_t key) {
    struct ui_input* input = ui_input_ctx();
    return ui_input_find_gesture(input, key);
}

enum app_cursor ui_input_get_cursor(void) {
    struct ui_input* input = ui_input_ctx();
    return input->cursor;
}

void ui_input_set_palm_radius(double radius) {
    struct ui_input* input = ui_input_ctx();
    input->palm_radius = radius;
}

double ui_input_get_palm_radius(void) {
    struct ui_input* input = ui_input_ctx();
    return input->palm_radius;
}

void ui_input_set_focus(uint64_t key) {
    struct ui_input* input = ui_input_ctx();
    input->focus_key = key;
}

uint64_t ui_input_get_focus(void) {
    struct ui_input* input = ui_input_ctx();
    return input->focus_key;
}

bool ui_input_has_focusable_at(double2 pos) {
    struct ui_input* input = ui_input_ctx();

    struct ui_input_attachment* attachments = ui_input_arr_attachment_get_data(input->attachments);
    uint32_t attachment_count = ui_input_arr_attachment_get_count(input->attachments);

    for (uint32_t i = 0; i < attachment_count; i++) {
        struct ui_input_attachment* attachment = &attachments[i];
        if (attachment->on_text == NULL || ui_is_alive(attachment->node) == false) {
            continue;
        }
        double2 origin = ui_get_origin_from_root(ui_get_root(attachment->node), attachment->node);
        double2 size = ui_get_size(attachment->node);
        if (pos.x >= origin.x && pos.x < origin.x + size.x && pos.y >= origin.y && pos.y < origin.y + size.y) {
            return true;
        }
    }
    return false;
}

void ui_input_set_caret_rect(double2 origin_pt, double2 size_pt) {
    struct ui_input* input = ui_input_ctx();
    input->caret_origin_pt = origin_pt;
    input->caret_size_pt = size_pt;
    input->caret_rect_valid = true;
}

bool ui_input_get_caret_rect(double2* out_origin_pt, double2* out_size_pt) {
    struct ui_input* input = ui_input_ctx();
    if (input->caret_rect_valid == false) {
        return false;
    }
    *out_origin_pt = input->caret_origin_pt;
    *out_size_pt = input->caret_size_pt;
    return true;
}

void ui_input_post_clipboard(const char* utf8, uint64_t size) {
    struct ui_input* input = ui_input_ctx();
    if (input->clipboard) {
        cr_free(input->mem, input->clipboard, cr_loc);
    }
    input->clipboard = cr_malloc(input->mem, size + 1, 0, cr_loc);
    memcpy(input->clipboard, utf8, (unsigned long)size);
    input->clipboard[size] = 0;
    input->clipboard_size = size;
    input->clipboard_pending = true;
}

bool ui_input_take_clipboard(const char** out_utf8, uint64_t* out_size) {
    struct ui_input* input = ui_input_ctx();
    if (input->clipboard_pending == false) {
        return false;
    }
    input->clipboard_pending = false;
    *out_utf8 = input->clipboard;
    *out_size = input->clipboard_size;
    return true;
}
