#pragma once

#include "app.h"
#include "gpu.h"
#include <string.h>

#define app_slot_count_max 8

struct app_native;
struct app_window_native;

struct app_text_chunk {
    struct app_text_chunk* next;
    uint32_t capacity;
    uint32_t used;
};

struct app_event_queue {
    struct cr_mem* mem;
    struct app_event* events;
    uint32_t count;
    uint32_t capacity;
    struct app_text_chunk* chunks;
};

struct app_window {
    struct app* app;
    struct app_window* prev;
    struct app_window* next;
    struct app_window_native* native;
    void* user;
    app_on_update_t* on_update;
    app_on_close_request_t* on_close_request;
    enum app_window_style style;
    uint32_t fps;
    struct app_target_format format;
    bool hidden;
    bool focused;
    bool is_dark;
    double keyboard_inset;
    enum app_cursor cursor;
    bool text_input_active;
    bool (*text_input_hit_test)(double2 pos_pt, void* user);
    void* text_input_hit_test_user;
    double2 caret_origin_pt;
    double2 caret_size_pt;
    struct app_event_queue queues[2];
    uint32_t queue_index;
    uint32_t slot_count;
    uint32_t slot_next;
    uint64_t slot_serials[app_slot_count_max];
    struct app_target_desc target_desc;
    double timestamp;
    double delta;
    bool has_timestamp;
    double frame_interval;
    double sync_timestamp;
    bool in_frame;
    bool presented;
    bool closed;
    bool vacated;
};

struct app {
    struct cr_mem* mem;
    struct app_desc desc;
    struct gpu_device* device;
    struct app_native* native;
    struct app_window* windows;
    bool is_in_background;
    bool ready;
    bool quit;
};

void app_ready(struct app* app);
void app_quit_requested(struct app* app);
void app_window_requested(struct app* app);
void app_window_close_requested(struct app_window* window);
void app_sweep(struct app* app);
void app_finish(struct app* app);

struct app_target_desc app_window_make_target_desc(struct app_window* window, double2 pt_size, uint2 px_size, double to_px_from_pt);
bool app_target_format_equal(struct app_target_format a, struct app_target_format b);

void app_event_queue_init(struct app_event_queue* queue, struct cr_mem* mem);
void app_event_queue_deinit(struct app_event_queue* queue);
void app_event_queue_reset(struct app_event_queue* queue);
void app_event_queue_push(struct app_event_queue* queue, const struct app_event* event);
const char* app_event_queue_push_text(struct app_event_queue* queue, const char* utf8, uint64_t size);

void app_window_add_event(struct app_window* window, const struct app_event* event);
void app_window_cancel_events(struct app_window* window);
void app_window_add_text_event(struct app_window* window, const char* utf8, uint64_t size);
void app_window_add_text_command_event(struct app_window* window, enum app_text_command command, bool select);
void app_window_add_marked_text_event(struct app_window* window, const char* utf8, uint64_t size, uint64_t caret);
void app_window_add_floating_caret_event(struct app_window* window, enum app_floating_caret_phase phase, double2 pos);
void app_window_add_key_event(struct app_window* window, enum app_key key, uint32_t modifiers, bool down, bool repeat);
void app_window_add_text_input_dismissed_event(struct app_window* window);

void app_window_frame(struct app_window* window, double timestamp, double interval, bool sync);

void app_platform_create(struct app* app);
void app_platform_run(struct app* app);
void app_platform_destroy(struct app* app);
void app_platform_request_sweep(struct app* app);
void app_platform_window_create(struct app_window* window, const struct app_window_desc* desc, struct app_window* vacated);
void app_platform_window_close(struct app_window* window);
void app_platform_window_destroy(struct app_window* window);
void app_platform_window_clamp_format(struct app_window* window, struct app_target_format* format);
void app_platform_window_set_hidden(struct app_window* window, bool hidden);
void app_platform_window_set_size(struct app_window* window, double2 size_pt);
void app_platform_window_set_title(struct app_window* window, const char* title);
void app_platform_window_set_fps(struct app_window* window, uint32_t fps);
void app_platform_window_set_cursor(struct app_window* window, enum app_cursor cursor);
void app_platform_window_set_text_input_active(struct app_window* window, bool active);
void app_platform_window_set_caret_rect(struct app_window* window, double2 origin_pt, double2 size_pt);
bool app_platform_window_get_target_size(struct app_window* window, double2* out_pt_size, uint2* out_px_size, double* out_to_px_from_pt);
void app_platform_window_apply_target(struct app_window* window, struct app_target_desc* desired);
struct gpu_texture* app_platform_window_acquire_target(struct app_window* window);
uint64_t app_platform_window_present(struct app_window* window, struct gpu_command_buffer* command_buffer, struct gpu_texture* target);
void app_platform_window_discard_target(struct app_window* window);
