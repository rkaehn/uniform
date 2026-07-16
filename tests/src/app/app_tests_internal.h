#pragma once

#include "app_tests.h"
#include "app.h"

#if cr_macos

#import <AppKit/AppKit.h>

#define app_test_frame_max 4096
#define app_test_track_max 8
#define app_test_result_max 8192
#define app_test_barcode_x 16
#define app_test_barcode_y 64
#define app_test_barcode_sync 0x5aa5u
#define app_test_barcode_cell 8
#define app_test_barcode_bits 16

enum app_test_barcode_row {
    app_test_barcode_row_width,
    app_test_barcode_row_height,
    app_test_barcode_row_frame,
    app_test_barcode_row_sync,
    app_test_barcode_row_count,
};

struct app_test_ctx;
struct app_test_capture;
struct app_test_pattern;

struct app_test_result {
    double time;
    double display_time;
    uint32_t track;
    int32_t dec_frame;
    int2 dec_size;
    int2 ext_size;
    bool valid;
    bool absent;
    bool outside_dark;
};

struct app_test_win {
    struct app_window* window;
    struct app_test_ctx* ctx;
    uint32_t track;
    enum app_window_style style;
    uint32_t frame_count;
    uint32_t close_count;
    double slow_s;
    double pending_event_ts;
    uint32_t format_toggle_every;
    uint32_t formats_seen;
    bool destroy_in_frame;
    bool skip_present;
    bool blink;
    double event_ts[app_test_frame_max];
    double render_ts[app_test_frame_max];
    uint64_t serials[app_test_frame_max];
};

struct app_test_capture* app_test_capture_create_(CGRect region, double scale);
void app_test_capture_destroy_(struct app_test_capture* capture);
bool app_test_capture_start_(struct app_test_capture* capture);
void app_test_capture_stop_(struct app_test_capture* capture);
void app_test_capture_set_track_(struct app_test_capture* capture, uint32_t index, double2 pos_pt, bool transparent);
void app_test_capture_clear_track_(struct app_test_capture* capture, uint32_t index);
void app_test_capture_clear_results_(struct app_test_capture* capture);
uint32_t app_test_capture_take_results_(struct app_test_capture* capture, struct app_test_result* out, uint32_t capacity);
bool app_test_capture_pixel_(struct app_test_capture* capture, double2 pos_pt, uchar4* out_rgba);

struct app_test_pattern* app_test_pattern_create_(struct gpu_device* device);
void app_test_pattern_destroy_(struct app_test_pattern* pattern);
void app_test_pattern_encode_(const struct app_test_pattern* pattern, struct gpu_command_buffer* command_buffer, struct gpu_texture* target, uint2 px_size, uint32_t frame, float4 fill);

void app_test_run_for_(struct app* app, double seconds);
NSWindow* app_test_ns_window_(struct app_test_win* win);
void app_test_post_mouse_(struct app_test_win* win, NSEventType type, double2 pos_pt, NSEventModifierFlags modifiers);
void app_test_drag_(struct app_test_win* win, double2 from_pt, double2 to_pt, uint32_t steps);
void app_test_hover_(struct app_test_win* win, double2 from_pt, double2 step_pt, uint32_t steps, double interval);
void app_test_post_key_(struct app_test_win* win, NSEventType type, const char* characters, uint16_t key_code, NSEventModifierFlags modifiers);
void app_test_scroll_(struct app_test_win* win, double2 pos_pt, int2 lines);

#endif
