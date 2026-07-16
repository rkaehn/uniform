#pragma once

#include "app_private.h"

#if cr_macos || cr_ios

#import <QuartzCore/QuartzCore.h>
#import <Metal/Metal.h>

@class AppPresented;

struct app_apple_state {
    dispatch_block_t sweep;
};

struct app_apple_surface {
    CAMetalLayer* mtl_layer;
    CADisplayLink* link;
    id<CAMetalDrawable> mtl_drawable;
    AppPresented* presented;
};

struct app_apple_state* app_apple_app_state(struct app* app);
struct app_apple_surface* app_apple_window_surface(struct app_window* window);
uint32_t app_apple_window_max_fps(struct app_window* window);

void app_apple_state_deinit(struct app_apple_state* state);

void app_apple_surface_init(struct app_apple_surface* surface, struct app_window* window, CAMetalLayer* mtl_layer, CADisplayLink* link);
void app_apple_surface_configure(struct app_apple_surface* surface, struct app_window* window);
void app_apple_surface_deinit(struct app_apple_surface* surface);

void app_apple_window_frame(struct app_window* window, CADisplayLink* link);
void app_apple_window_frame_sync(struct app_window* window);
void app_apple_window_add_text(struct app_window* window, NSString* text);
void app_apple_window_add_marked_text(struct app_window* window, NSString* text, NSUInteger caret_utf16);
uint2 app_apple_px_size(double2 pt_size, double to_px_from_pt);

#endif
