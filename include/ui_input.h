#pragma once

#include "cr.h"
#include "app_event.h"
#include "ui.h"

struct ui_input;
struct ui_gesture;

#define cr_handle ui_pointer
#include "cr_handle.h"

#define ui_pointer_null ((struct ui_pointer) { 0 })
#define ui_gesture_name_max 32

enum cr_closed ui_gesture_phase {
    ui_gesture_phase_possible,
    ui_gesture_phase_began,
    ui_gesture_phase_changed,
    ui_gesture_phase_ended,
    ui_gesture_phase_recognized,
    ui_gesture_phase_canceled,
    ui_gesture_phase_failed,
};

enum cr_closed ui_gesture_decision {
    ui_gesture_decision_reject,
    ui_gesture_decision_accept,
    ui_gesture_decision_hold,
};

struct ui_gesture_desc {
    struct ui_node node;
    struct ui_key key;
    const char* name;
    uint64_t state_size;
    void (*create)(struct ui_gesture* gesture);
    void (*destroy)(struct ui_gesture* gesture);
    void (*reset)(struct ui_gesture* gesture);
    bool (*simultaneous)(struct ui_gesture* gesture, struct ui_gesture* other);
    enum ui_gesture_decision (*accepts)(struct ui_gesture* gesture, struct app_pointer_event event, struct ui_pointer pointer);
    void (*on_pointer)(struct ui_gesture* gesture, struct app_pointer_event event, struct ui_pointer pointer);
    void (*on_update)(struct ui_gesture* gesture, double timestamp);
    void (*cancel)(struct ui_gesture* gesture);
    void (*remove_pointer)(struct ui_gesture* gesture, struct ui_pointer pointer);
};

struct ui_on_pointer {
    struct ui_node node;
    void (*func)(struct app_pointer_event event, struct ui_pointer pointer, void* user);
    void* user;
};

struct ui_on_scroll {
    struct ui_node node;
    void (*func)(struct app_scroll_event event, void* user);
    void* user;
};

struct ui_on_text {
    struct ui_node node;
    void (*func)(const struct app_event* event, void* user);
    void* user;
};

struct ui_cursor {
    struct ui_node node;
    enum app_cursor cursor;
};

void ui_input_set(struct ui_input* input);
struct ui_input* ui_input_get(void);

struct ui_input* ui_input_create(struct cr_mem* mem);
void ui_input_destroy(struct ui_input* input);

void ui_input_dispatch(struct ui_node root, double timestamp, const struct app_event* events, uint32_t event_count, double to_pt_from_app);

struct ui_gesture* ui_gesture(const struct ui_gesture_desc* desc);
struct ui_input* ui_gesture_get_input(const struct ui_gesture* gesture);
struct ui_node ui_gesture_get_node(const struct ui_gesture* gesture);
uint64_t ui_gesture_get_key(const struct ui_gesture* gesture);
const char* ui_gesture_get_name(const struct ui_gesture* gesture);
enum ui_gesture_phase ui_gesture_get_phase(const struct ui_gesture* gesture);
void ui_gesture_set_phase(struct ui_gesture* gesture, enum ui_gesture_phase phase);
void* ui_gesture_get_state(struct ui_gesture* gesture);
void ui_gesture_claim(struct ui_gesture* gesture);
void ui_gesture_release(struct ui_gesture* gesture, struct ui_pointer pointer);
void ui_on_pointer(const struct ui_on_pointer* on_pointer);
void ui_on_scroll(const struct ui_on_scroll* on_scroll);
void ui_on_text(const struct ui_on_text* on_text);
void ui_cursor(const struct ui_cursor* cursor);

struct ui_gesture* ui_input_get_gesture(uint64_t key);

double2 ui_input_pointer_get_position(struct ui_pointer pointer);
double2 ui_input_pointer_get_translation(struct ui_pointer pointer, bool map_direct);
double2 ui_input_pointer_get_velocity(struct ui_pointer pointer);
double ui_input_pointer_get_timestamp(struct ui_pointer pointer);
bool ui_input_pointer_is_moving(struct ui_pointer pointer);

enum app_cursor ui_input_get_cursor(void);

void ui_input_set_palm_radius(double radius);
double ui_input_get_palm_radius(void);

void ui_input_set_focus(uint64_t key);
uint64_t ui_input_get_focus(void);

bool ui_input_has_focusable_at(double2 pos);

void ui_input_set_caret_rect(double2 origin_pt, double2 size_pt);
bool ui_input_get_caret_rect(double2* out_origin_pt, double2* out_size_pt);

void ui_input_post_clipboard(const char* utf8, uint64_t size);
bool ui_input_take_clipboard(const char** out_utf8, uint64_t* out_size);
