#pragma once

#include "cr.h"
#include "app_event.h"

struct gpu_device;
struct gpu_command_buffer;
struct gpu_texture;
struct app;
struct app_window;

enum cr_closed app_color_space {
    app_color_space_srgb,
    app_color_space_display_p3,
};

enum cr_closed app_pixel_format {
    app_pixel_format_default,
    app_pixel_format_rgba16f,
    app_pixel_format_bgra8,
};

enum cr_closed app_window_style {
    app_window_style_bordered,
    app_window_style_borderless,
    app_window_style_transparent,
};

struct app_target_format {
    enum app_color_space color_space;
    enum app_pixel_format pixel_format;
    bool sampleable;
};

struct app_target_desc {
    uint2 px_size;
    double2 pt_size;
    double to_px_from_pt;
    struct app_target_format format;
};

struct app_screen_desc {
    double2 pos;
    double2 size;
};

#define app_delta_max 0.1

typedef void app_on_start_t(struct app* app);
typedef void app_on_quit_request_t(struct app* app);
typedef void app_on_quit_t(struct app* app);
typedef void app_on_window_request_t(struct app* app);
typedef void app_on_update_t(struct app_window* window);
typedef void app_on_close_request_t(struct app_window* window);

struct app_desc {
    struct cr_mem* mem;
    int argc;
    char** argv;
    void* user;
    app_on_start_t* on_start;
    app_on_quit_request_t* on_quit_request;
    app_on_quit_t* on_quit;
    app_on_window_request_t* on_window_request;
};

struct app_window_desc {
    void* user;
    app_on_update_t* on_update;
    app_on_close_request_t* on_close_request;
    enum app_window_style style;
    const char* title;
    double2 pos;
    double2 size;
    bool center;
    bool resizable;
    bool hidden;
    uint32_t fps;
    uint32_t slot_count;
    struct app_target_format format;
};

void app_run(const struct app_desc* desc);
void app_restart(struct app* app, const struct app_desc* desc);
void app_quit(struct app* app);

void* app_get_user(struct app* app);
struct gpu_device* app_get_device(struct app* app);
void app_set_clipboard_text(struct app* app, const char* utf8, uint64_t size);

struct app_window* app_window_create(struct app* app, const struct app_window_desc* desc);
void app_window_destroy(struct app_window* window);

struct app* app_window_get_app(struct app_window* window);
void* app_window_get_user(struct app_window* window);
void* app_window_get_native(struct app_window* window);

void app_window_set_pos(struct app_window* window, double2 pos_pt);
double2 app_window_get_pos(struct app_window* window);
void app_window_set_size(struct app_window* window, double2 size_pt);
double2 app_window_get_size(struct app_window* window);
struct app_screen_desc app_window_get_screen(struct app_window* window);
void app_window_set_hidden(struct app_window* window, bool hidden);
bool app_window_get_hidden(struct app_window* window);
void app_window_focus(struct app_window* window);
bool app_window_get_focused(struct app_window* window);
void app_window_order_front(struct app_window* window);
void app_window_order_back(struct app_window* window);
void app_window_set_title(struct app_window* window, const char* title);
void app_window_set_fps(struct app_window* window, uint32_t fps);
void app_window_set_target_format(struct app_window* window, struct app_target_format format);

struct app_target_desc app_window_get_target_desc(struct app_window* window);
double app_window_get_timestamp(struct app_window* window);
double app_window_get_frame_interval(struct app_window* window);
double app_window_get_delta(struct app_window* window);
bool app_window_get_is_dark(struct app_window* window);
double app_window_get_keyboard_inset(struct app_window* window);

struct gpu_texture* app_window_acquire_target(struct app_window* window);
uint64_t app_window_present(struct app_window* window, struct gpu_command_buffer* command_buffer, struct gpu_texture* target);
uint64_t app_window_get_presented(struct app_window* window);

void app_window_take_events(struct app_window* window, const struct app_event** out_events, uint32_t* out_event_count);

void app_window_text_input_set_active(struct app_window* window, bool active);
bool app_window_text_input_get_active(struct app_window* window);
void app_window_text_input_set_caret_rect(struct app_window* window, double2 origin_pt, double2 size_pt);
void app_window_text_input_set_hit_test(struct app_window* window, bool (*hit_test)(double2 pos_pt, void* user), void* user);
void app_window_set_cursor(struct app_window* window, enum app_cursor cursor);
