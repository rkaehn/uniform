#include "app_apple.h"

#if cr_macos || cr_ios

#include <stdatomic.h>

@interface AppPresented: NSObject
- (void)noteSerial:(uint64_t)serial time:(double)time;
- (uint64_t)serial;
@end

@implementation AppPresented {
    _Atomic uint64_t _serial;
}

- (void)noteSerial:(uint64_t)serial time:(double)time {
    if (time <= 0.0) {
        return;
    }
    uint64_t current = atomic_load_explicit(&_serial, memory_order_relaxed);
    while (serial > current && atomic_compare_exchange_weak_explicit(&_serial, &current, serial, memory_order_release, memory_order_relaxed) == false) {
    }
}

- (uint64_t)serial {
    return atomic_load_explicit(&_serial, memory_order_acquire);
}

@end

static MTLPixelFormat app_apple_pixel_format(enum app_pixel_format pixel_format) {
    return pixel_format == app_pixel_format_rgba16f ? MTLPixelFormatRGBA16Float : MTLPixelFormatBGRA8Unorm;
}

static CFStringRef app_apple_color_space_name(enum app_color_space color_space) {
    return color_space == app_color_space_display_p3 ? kCGColorSpaceExtendedDisplayP3 : kCGColorSpaceExtendedSRGB;
}

static void app_apple_layer_set_format(CAMetalLayer* mtl_layer, struct app_target_format format) {
    CGColorSpaceRef color_space = CGColorSpaceCreateWithName(app_apple_color_space_name(format.color_space));
    mtl_layer.pixelFormat = app_apple_pixel_format(format.pixel_format);
    mtl_layer.framebufferOnly = format.sampleable == false;
    mtl_layer.wantsExtendedDynamicRangeContent = format.pixel_format == app_pixel_format_rgba16f;
    mtl_layer.colorspace = color_space;
    CGColorSpaceRelease(color_space);
}

static void app_apple_layer_set_size(CAMetalLayer* mtl_layer, struct app_target_desc desc) {
    CGSize px_size = CGSizeMake(desc.px_size.x, desc.px_size.y);
    if (mtl_layer.contentsScale != desc.to_px_from_pt) {
        mtl_layer.contentsScale = desc.to_px_from_pt;
    }
    if (CGSizeEqualToSize(mtl_layer.drawableSize, px_size) == false) {
        mtl_layer.drawableSize = px_size;
    }
}

static void app_apple_link_set_fps(CADisplayLink* link, struct app_window* window, uint32_t fps) {
    float rate = (float)(fps == 0 ? app_apple_window_max_fps(window) : fps);
    link.preferredFrameRateRange = rate == 0.0f ? CAFrameRateRangeDefault : CAFrameRateRangeMake(rate, rate, rate);
}

static double app_apple_link_interval(CADisplayLink* link) {
    double duration = link.duration;
    return duration > 0.0 ? duration : 1.0 / 60.0;
}

uint2 app_apple_px_size(double2 pt_size, double to_px_from_pt) {
    int2 px = max_int2(to_int2_from_double2(round_double2(pt_size * to_px_from_pt)), make_int2(1, 1));
    return to_uint2_from_int2(px);
}

void app_apple_surface_init(struct app_apple_surface* surface, struct app_window* window, CAMetalLayer* mtl_layer, CADisplayLink* link) {
    mtl_layer.device = (id<MTLDevice>)gpu_device_get_native_device(window->app->device);
    mtl_layer.minificationFilter = kCAFilterNearest;
    mtl_layer.magnificationFilter = kCAFilterNearest;
    mtl_layer.maximumDrawableCount = 3;
    mtl_layer.presentsWithTransaction = true;
    app_apple_layer_set_format(mtl_layer, window->format);
    [link addToRunLoop:NSRunLoop.mainRunLoop forMode:NSRunLoopCommonModes];

    surface->mtl_layer = mtl_layer;
    surface->link = [link retain];
    surface->presented = [[AppPresented alloc] init];
    app_apple_surface_configure(surface, window);
}

void app_apple_surface_configure(struct app_apple_surface* surface, struct app_window* window) {
    surface->mtl_layer.opaque = window->style != app_window_style_transparent;
    app_apple_link_set_fps(surface->link, window, window->fps);
    surface->link.paused = window->hidden;
}

void app_apple_surface_deinit(struct app_apple_surface* surface) {
    [surface->link invalidate];
    [surface->link release];
    [surface->mtl_drawable release];
    [surface->presented release];
    memset(surface, 0, sizeof(*surface));
}

void app_apple_state_deinit(struct app_apple_state* state) {
    if (state->sweep) {
        dispatch_block_cancel(state->sweep);
        Block_release(state->sweep);
        state->sweep = NULL;
    }
}

void app_apple_window_frame(struct app_window* window, CADisplayLink* link) {
    if (window == NULL || window->closed) {
        return;
    }
    cr_autoreleasepool {
        app_window_frame(window, link.targetTimestamp, app_apple_link_interval(link), false);
    }
}

void app_apple_window_frame_sync(struct app_window* window) {
    if (window == NULL || window->closed) {
        return;
    }
    cr_autoreleasepool {
        CADisplayLink* link = app_apple_window_surface(window)->link;
        double interval = app_apple_link_interval(link);
        double now = CACurrentMediaTime();
        double target = link.targetTimestamp;
        if (target <= 0.0) {
            target = now + interval;
        } else if (target <= now) {
            target += (floor((now - target) / interval) + 1.0) * interval;
        }
        app_window_frame(window, target, interval, true);
    }
}

void app_apple_window_add_text(struct app_window* window, NSString* text) {
    const char* utf8 = text.UTF8String;
    app_window_add_text_event(window, utf8, strlen(utf8));
}

void app_apple_window_add_marked_text(struct app_window* window, NSString* text, NSUInteger caret_utf16) {
    const char* utf8 = text.UTF8String;
    NSString* prefix = [text substringToIndex:min_uint64(caret_utf16, text.length)];
    app_window_add_marked_text_event(window, utf8, strlen(utf8), strlen(prefix.UTF8String));
}

uint64_t app_window_get_presented(struct app_window* window) {
    return [app_apple_window_surface(window)->presented serial];
}

void app_platform_request_sweep(struct app* app) {
    struct app_apple_state* state = app_apple_app_state(app);
    if (state->sweep) {
        return;
    }
    state->sweep = dispatch_block_create(0, ^{
        dispatch_block_t block = state->sweep;
        state->sweep = NULL;
        Block_release(block);
        app_sweep(app);
    });
    dispatch_async(dispatch_get_main_queue(), state->sweep);
}

void app_platform_window_clamp_format(struct app_window* window, struct app_target_format* format) {
    if (format->pixel_format == app_pixel_format_default) {
        format->pixel_format = app_pixel_format_rgba16f;
    }
}

void app_platform_window_apply_target(struct app_window* window, struct app_target_desc* desired) {
    CAMetalLayer* mtl_layer = app_apple_window_surface(window)->mtl_layer;
    if (app_target_format_equal(desired->format, window->target_desc.format) == false) {
        app_apple_layer_set_format(mtl_layer, desired->format);
    }
    app_apple_layer_set_size(mtl_layer, *desired);
}

struct gpu_texture* app_platform_window_acquire_target(struct app_window* window) {
    struct app_apple_surface* surface = app_apple_window_surface(window);
    cr_check(surface->mtl_drawable == NULL, "the window target was already acquired this frame");
    surface->mtl_drawable = [[surface->mtl_layer nextDrawable] retain];
    if (surface->mtl_drawable == NULL) {
        return NULL;
    }
    return gpu_texture_create_target_from_native(window->app->device, surface->mtl_drawable.texture);
}

uint64_t app_platform_window_present(struct app_window* window, struct gpu_command_buffer* command_buffer, struct gpu_texture* target) {
    struct app_apple_surface* surface = app_apple_window_surface(window);
    id<CAMetalDrawable> mtl_drawable = surface->mtl_drawable;
    if (target == NULL || mtl_drawable == NULL) {
        return gpu_command_buffer_commit(command_buffer);
    }

    AppPresented* presented = surface->presented;
    uint64_t serial = gpu_command_buffer_get_serial(command_buffer);
    [mtl_drawable addPresentedHandler:^(id<MTLDrawable> mtl_shown) {
        [presented noteSerial:serial time:mtl_shown.presentedTime];
    }];
    id<MTLCommandBuffer> mtl_command_buffer = [(id<MTLCommandBuffer>)gpu_command_buffer_get_native(command_buffer) retain];
    gpu_command_buffer_commit(command_buffer);
    [mtl_command_buffer waitUntilScheduled];
    [mtl_command_buffer release];
    [mtl_drawable present];
    [mtl_drawable release];
    surface->mtl_drawable = NULL;
    gpu_texture_destroy(window->app->device, target);
    return serial;
}

void app_platform_window_discard_target(struct app_window* window) {
    struct app_apple_surface* surface = app_apple_window_surface(window);
    [surface->mtl_drawable release];
    surface->mtl_drawable = NULL;
}

void app_platform_window_set_fps(struct app_window* window, uint32_t fps) {
    app_apple_link_set_fps(app_apple_window_surface(window)->link, window, fps);
}

#endif
