#include "app_private.h"

#if cr_wasm

#include "../gpu/wgpu.h"
#include <stdio.h>
#include <wasi/libc-busywait.h>

cr_js_import(app_js_init) void app_js_init(void* app);
cr_js_import(app_js_canvas_create) uint32_t app_js_canvas_create(double x, double y, double width, double height, uint32_t style);
cr_js_import(app_js_canvas_destroy) void app_js_canvas_destroy(uint32_t canvas);
cr_js_import(app_js_canvas_set_rect) void app_js_canvas_set_rect(uint32_t canvas, double x, double y, double width, double height);
cr_js_import(app_js_canvas_set_hidden) void app_js_canvas_set_hidden(uint32_t canvas, bool hidden);
cr_js_import(app_js_canvas_order_front) void app_js_canvas_order_front(uint32_t canvas);
cr_js_import(app_js_canvas_order_back) void app_js_canvas_order_back(uint32_t canvas);
cr_js_import(app_js_canvas_focus) void app_js_canvas_focus(uint32_t canvas);
cr_js_import(app_js_canvas_x) double app_js_canvas_x(uint32_t canvas);
cr_js_import(app_js_canvas_y) double app_js_canvas_y(uint32_t canvas);
cr_js_import(app_js_canvas_sync_size) void app_js_canvas_sync_size(uint32_t canvas);
cr_js_import(app_js_canvas_set_size) void app_js_canvas_set_size(uint32_t canvas, uint32_t width, uint32_t height);
cr_js_import(app_js_canvas_set_cursor) void app_js_canvas_set_cursor(uint32_t canvas, uint32_t cursor);
cr_js_import(app_js_canvas_set_title) void app_js_canvas_set_title(uint32_t canvas, const char* title);
cr_js_import(app_js_screen_width) double app_js_screen_width(void);
cr_js_import(app_js_screen_height) double app_js_screen_height(void);
cr_js_import(app_js_is_dark) bool app_js_is_dark(void);
cr_js_import(app_js_text_input_set_active) void app_js_text_input_set_active(uint32_t canvas, bool active);
cr_js_import(app_js_text_input_set_caret_rect) void app_js_text_input_set_caret_rect(uint32_t canvas, double x, double y, double height);
cr_js_import(app_js_set_clipboard) void app_js_set_clipboard(const char* utf8, uint32_t size);
cr_js_import(app_js_animation_frame_loop) void app_js_animation_frame_loop(bool (*callback)(double timestamp, void* user), void* user);

struct app_window_native {
    uint32_t canvas;
    WGpuCanvasContext wgpu_context;
    bool fill;
    bool rgba16f_unsupported;
    uint64_t presented;
    double2 pt_size;
    uint2 px_size;
    double to_px_from_pt;
};

struct app_native {
    WGpuDevice wgpu_device;
    double last_timestamp;
    double frame_interval;
};

static struct app_window* app_wasm_window_for_canvas(struct app* app, uint32_t canvas) {
    for (struct app_window* window = app->windows; window; window = window->next) {
        if (window->native->canvas == canvas) {
            return window;
        }
    }
    return NULL;
}

static double2 app_wasm_canvas_pos(uint32_t canvas) {
    return make_double2(app_js_canvas_x(canvas), app_js_canvas_y(canvas));
}

void app_platform_create(struct app* app) {
    __wasilibc_enable_futex_busywait_on_current_thread();
    struct app_native* native = cr_malloc(app->mem, sizeof(*native), 16, cr_loc);
    memset(native, 0, sizeof(*native));
    native->frame_interval = 1.0 / 60.0;
    app->native = native;
    app_js_init(app);
}

void app_platform_destroy(struct app* app) {
    cr_free(app->mem, app->native, cr_loc);
}

static void app_wasm_on_error(WGpuDevice wgpu_device, WGPU_ERROR_TYPE type, const char* message, void* user) {
    printf("webgpu error %d %s\n", type, message);
}

static void app_wasm_on_device_lost(WGpuDevice wgpu_device, WGPU_DEVICE_LOST_REASON reason, const char* message, void* user) {
    if (reason != WGPU_DEVICE_LOST_REASON_DESTROYED) {
        printf("webgpu device lost %u %s\n", reason, message);
    }
}

static double app_wasm_update_interval(struct app_native* native, double timestamp) {
    if (native->last_timestamp > 0.0) {
        double delta = timestamp - native->last_timestamp;
        if (delta > 0.001 && delta < 0.1) {
            native->frame_interval = native->frame_interval * 0.9 + delta * 0.1;
        }
    }
    native->last_timestamp = timestamp;
    return native->frame_interval;
}

static bool app_wasm_frame(double timestamp_ms, void* user) {
    struct app* app = user;
    double timestamp = timestamp_ms / 1000.0;
    double interval = app_wasm_update_interval(app->native, timestamp);
    if (app->quit == false) {
        for (struct app_window* window = app->windows; window; window = window->next) {
            app_window_frame(window, timestamp, interval, false);
        }
    }
    app_sweep(app);
    if (app->quit) {
        app_finish(app);
        return false;
    }
    return true;
}

static void app_wasm_on_device(WGpuDevice wgpu_device, void* user) {
    struct app* app = user;
    assert(wgpu_is_device(wgpu_device));
    wgpu_device_set_uncapturederror_callback(wgpu_device, app_wasm_on_error, NULL);
    wgpu_device_set_lost_callback(wgpu_device, app_wasm_on_device_lost, NULL);

    WGpuQueue wgpu_queue = wgpu_device_get_queue(wgpu_device);
    assert(wgpu_is_queue(wgpu_queue));
    app->native->wgpu_device = wgpu_device;
    app->device = gpu_device_create(app->mem, wgpu_device, wgpu_queue);

    app_ready(app);
    app_js_animation_frame_loop(app_wasm_frame, app);
}

static void app_wasm_on_adapter(WGpuAdapter wgpu_adapter, void* user) {
    assert(wgpu_is_adapter(wgpu_adapter));
    WGpuDeviceDescriptor wgpu_desc = WGPU_DEVICE_DESCRIPTOR_DEFAULT_INITIALIZER;
    wgpu_desc.requiredFeatures = wgpu_adapter_or_device_get_features(wgpu_adapter) & (WGPU_FEATURE_TIMESTAMP_QUERY | WGPU_FEATURE_SHADER_F16);
    wgpu_desc.maxComputeWorkgroupStorageSize = wgpu_adapter_or_device_get_workgroup_storage_limit(wgpu_adapter);
    wgpu_adapter_request_device_async(wgpu_adapter, &wgpu_desc, app_wasm_on_device, user);
}

void app_platform_run(struct app* app) {
    WGpuRequestAdapterOptions wgpu_options = WGPU_REQUEST_ADAPTER_OPTIONS_DEFAULT_INITIALIZER;
    wgpu_options.powerPreference = WGPU_POWER_PREFERENCE_HIGH_PERFORMANCE;
    navigator_gpu_request_adapter_async(&wgpu_options, app_wasm_on_adapter, app);
}

void app_quit(struct app* app) {
    app->quit = true;
}

void app_platform_request_sweep(struct app* app) {
}

static double2 app_wasm_screen_size(void) {
    return make_double2(app_js_screen_width(), app_js_screen_height());
}

struct app_screen_desc app_window_get_screen(struct app_window* window) {
    return (struct app_screen_desc) { .size = app_wasm_screen_size() };
}

void app_set_clipboard_text(struct app* app, const char* utf8, uint64_t size) {
    app_js_set_clipboard(utf8, (uint32_t)size);
}

static bool app_wasm_configure(struct app_window* window, struct app_target_format format) {
    bool is_16f = format.pixel_format == app_pixel_format_rgba16f;
    WGpuCanvasConfiguration wgpu_config = WGPU_CANVAS_CONFIGURATION_DEFAULT_INITIALIZER;
    wgpu_config.device = window->app->native->wgpu_device;
    wgpu_config.format = is_16f ? WGPU_TEXTURE_FORMAT_RGBA16FLOAT : WGPU_TEXTURE_FORMAT_BGRA8UNORM;
    wgpu_config.colorSpace = format.color_space == app_color_space_display_p3 ? WGPU_CANVAS_COLOR_SPACE_DISPLAY_P3 : WGPU_CANVAS_COLOR_SPACE_SRGB;
    wgpu_config.usage = WGPU_TEXTURE_USAGE_RENDER_ATTACHMENT | (format.sampleable ? WGPU_TEXTURE_USAGE_TEXTURE_BINDING : 0u);
    wgpu_config.toneMappingExtended = is_16f ? 1u : 0u;
    return wgpu_canvas_context_configure(window->native->wgpu_context, &wgpu_config);
}

void app_platform_window_clamp_format(struct app_window* window, struct app_target_format* format) {
    if (format->pixel_format == app_pixel_format_default) {
        format->pixel_format = app_pixel_format_bgra8;
    }
    if (window->native != NULL && window->native->rgba16f_unsupported && format->pixel_format == app_pixel_format_rgba16f) {
        format->pixel_format = app_pixel_format_bgra8;
    }
}

static void app_wasm_configure_supported(struct app_window* window, struct app_target_format* format) {
    app_platform_window_clamp_format(window, format);
    if (app_wasm_configure(window, *format)) {
        return;
    }
    assert(format->pixel_format == app_pixel_format_rgba16f);
    window->native->rgba16f_unsupported = true;
    format->pixel_format = app_pixel_format_bgra8;
    bool configured = app_wasm_configure(window, *format);
    assert(configured);
}

void app_platform_window_create(struct app_window* window, const struct app_window_desc* desc, struct app_window* vacated) {
    struct app* app = window->app;
    struct app_window_native* native = cr_malloc(app->mem, sizeof(*native), 16, cr_loc);
    memset(native, 0, sizeof(*native));
    window->native = native;
    window->is_dark = app_js_is_dark();

    native->fill = desc->resizable || all_long2((long2)(desc->size > 0.0)) == false;
    double2 size = native->fill ? make_double2(0.0, 0.0) : desc->size;
    double2 pos = native->fill ? make_double2(0.0, 0.0) : (desc->center ? (app_wasm_screen_size() - size) * 0.5 : desc->pos);
    native->canvas = app_js_canvas_create(pos.x, pos.y, size.x, size.y, (uint32_t)desc->style);
    if (desc->title) {
        app_js_canvas_set_title(native->canvas, desc->title);
    }
    if (window->hidden) {
        app_js_canvas_set_hidden(native->canvas, true);
    }

    char name[32];
    snprintf(name, sizeof(name), "canvas%u", native->canvas);
    native->wgpu_context = wgpu_canvas_get_webgpu_context(name);
    assert(wgpu_is_canvas_context(native->wgpu_context));
    app_js_canvas_sync_size(native->canvas);

    struct app_target_desc initial = app_window_make_target_desc(window, native->pt_size, native->px_size, native->to_px_from_pt);
    app_js_canvas_set_size(native->canvas, initial.px_size.x, initial.px_size.y);
    app_wasm_configure_supported(window, &initial.format);
    window->target_desc = initial;
}

void app_platform_window_close(struct app_window* window) {
    app_js_canvas_set_hidden(window->native->canvas, true);
}

void app_platform_window_destroy(struct app_window* window) {
    app_js_canvas_destroy(window->native->canvas);
    cr_free(window->app->mem, window->native, cr_loc);
}

void* app_window_get_native(struct app_window* window) {
    return (void*)(uintptr_t)window->native->canvas;
}

void app_window_set_pos(struct app_window* window, double2 pos_pt) {
    struct app_window_native* native = window->native;
    if (native->fill) {
        return;
    }
    app_js_canvas_set_rect(native->canvas, pos_pt.x, pos_pt.y, native->pt_size.x, native->pt_size.y);
}

double2 app_window_get_pos(struct app_window* window) {
    return app_wasm_canvas_pos(window->native->canvas);
}

void app_platform_window_set_size(struct app_window* window, double2 size_pt) {
    struct app_window_native* native = window->native;
    native->fill = false;
    double2 pos_pt = app_wasm_canvas_pos(native->canvas);
    app_js_canvas_set_rect(native->canvas, pos_pt.x, pos_pt.y, size_pt.x, size_pt.y);
}

double2 app_window_get_size(struct app_window* window) {
    return window->native->pt_size;
}

void app_platform_window_set_hidden(struct app_window* window, bool hidden) {
    app_js_canvas_set_hidden(window->native->canvas, hidden);
}

void app_window_focus(struct app_window* window) {
    app_js_canvas_focus(window->native->canvas);
    for (struct app_window* other = window->app->windows; other; other = other->next) {
        other->focused = other == window;
    }
}

bool app_window_get_focused(struct app_window* window) {
    return window->focused;
}

void app_window_order_front(struct app_window* window) {
    app_js_canvas_order_front(window->native->canvas);
}

void app_window_order_back(struct app_window* window) {
    app_js_canvas_order_back(window->native->canvas);
}

void app_platform_window_set_title(struct app_window* window, const char* title) {
    app_js_canvas_set_title(window->native->canvas, title);
}

uint64_t app_window_get_presented(struct app_window* window) {
    return window->native->presented;
}

bool app_platform_window_get_target_size(struct app_window* window, double2* out_pt_size, uint2* out_px_size, double* out_to_px_from_pt) {
    struct app_window_native* native = window->native;
    *out_pt_size = native->pt_size;
    *out_px_size = native->px_size;
    *out_to_px_from_pt = native->to_px_from_pt;
    return true;
}

void app_platform_window_apply_target(struct app_window* window, struct app_target_desc* desired) {
    struct app_window_native* native = window->native;
    if (any_int2(desired->px_size != window->target_desc.px_size)) {
        app_js_canvas_set_size(native->canvas, desired->px_size.x, desired->px_size.y);
    }
    if (app_target_format_equal(desired->format, window->target_desc.format) == false) {
        app_wasm_configure_supported(window, &desired->format);
    }
}

struct gpu_texture* app_platform_window_acquire_target(struct app_window* window) {
    return gpu_texture_create_target_from_native(window->app->device, wgpu_canvas_context_get_current_texture_view(window->native->wgpu_context), window->target_desc.px_size);
}

uint64_t app_platform_window_present(struct app_window* window, struct gpu_command_buffer* command_buffer, struct gpu_texture* target) {
    uint64_t serial = gpu_command_buffer_commit(command_buffer);
    gpu_texture_destroy(window->app->device, target);
    window->native->presented = serial;
    return serial;
}

void app_platform_window_discard_target(struct app_window* window) {
}

void app_platform_window_set_fps(struct app_window* window, uint32_t fps) {
}

void app_platform_window_set_cursor(struct app_window* window, enum app_cursor cursor) {
    app_js_canvas_set_cursor(window->native->canvas, (uint32_t)cursor);
}

void app_platform_window_set_text_input_active(struct app_window* window, bool active) {
    app_js_text_input_set_active(window->native->canvas, active);
}

void app_platform_window_set_caret_rect(struct app_window* window, double2 origin_pt, double2 size_pt) {
    app_js_text_input_set_caret_rect(window->native->canvas, origin_pt.x, origin_pt.y, size_pt.y);
}

cr_export(void, app_wasm_handle_size, (void* app_ptr, uint32_t canvas, double pt_width, double pt_height, uint32_t px_width, uint32_t px_height, double to_px_from_pt)) {
    struct app_window* window = app_wasm_window_for_canvas(app_ptr, canvas);
    if (window == NULL) {
        return;
    }
    window->native->pt_size = make_double2(pt_width, pt_height);
    window->native->px_size = make_uint2(px_width, px_height);
    window->native->to_px_from_pt = to_px_from_pt;
}

cr_export(void, app_wasm_handle_pointer, (void* app_ptr, uint32_t canvas, int32_t event_type, int32_t pointer_id, int32_t pointer_type, int32_t button, uint32_t modifiers, double pressure, double radius, double x, double y, double timestamp)) {
    struct app_window* window = app_wasm_window_for_canvas(app_ptr, canvas);
    if (window == NULL) {
        return;
    }
    assert(pointer_id != 0);
    struct app_event event = {
        .type = app_event_type_pointer,
        .pointer.type = (enum app_pointer_event_type)event_type,
        .pointer.source = (enum app_pointer_source)pointer_type,
        .pointer.button = (enum app_pointer_button)button,
        .pointer.id = (uint64_t)pointer_id,
        .pointer.modifiers = modifiers,
        .pointer.pressure = pressure,
        .pointer.radius = radius,
        .pointer.pos = make_double2(x, y),
        .pointer.timestamp = timestamp,
    };
    app_window_add_event(window, &event);
}

cr_export(void, app_wasm_handle_scroll, (void* app_ptr, uint32_t canvas, double delta_x, double delta_y, double x, double y, double timestamp)) {
    struct app_window* window = app_wasm_window_for_canvas(app_ptr, canvas);
    if (window == NULL) {
        return;
    }
    struct app_event event = {
        .type = app_event_type_scroll,
        .scroll.delta = make_double2(delta_x, delta_y),
        .scroll.pos = make_double2(x, y),
        .scroll.timestamp = timestamp,
    };
    app_window_add_event(window, &event);
}

cr_export(void, app_wasm_handle_key, (void* app_ptr, uint32_t canvas, uint32_t key, uint32_t modifiers, bool down, bool repeat)) {
    struct app_window* window = app_wasm_window_for_canvas(app_ptr, canvas);
    if (window) {
        app_window_add_key_event(window, (enum app_key)key, modifiers, down, repeat);
    }
}

cr_export(void, app_wasm_handle_is_dark, (void* app_ptr, bool is_dark)) {
    struct app* app = app_ptr;
    for (struct app_window* window = app->windows; window; window = window->next) {
        window->is_dark = is_dark;
    }
}

cr_export(void, app_wasm_handle_keyboard_inset, (void* app_ptr, double inset)) {
    struct app* app = app_ptr;
    for (struct app_window* window = app->windows; window; window = window->next) {
        window->keyboard_inset = max_double(inset, 0.0);
    }
}

cr_export(void, app_wasm_handle_focus, (void* app_ptr, uint32_t canvas, bool focused)) {
    struct app_window* window = app_wasm_window_for_canvas(app_ptr, canvas);
    if (window) {
        window->focused = focused;
    }
}

cr_export(void, app_wasm_handle_text, (void* app_ptr, uint32_t canvas, const char* utf8, uint32_t size)) {
    struct app_window* window = app_wasm_window_for_canvas(app_ptr, canvas);
    if (window) {
        app_window_add_text_event(window, utf8, size);
    }
}

cr_export(void, app_wasm_handle_marked_text, (void* app_ptr, uint32_t canvas, const char* utf8, uint32_t size, uint32_t caret)) {
    struct app_window* window = app_wasm_window_for_canvas(app_ptr, canvas);
    if (window) {
        app_window_add_marked_text_event(window, utf8, size, caret);
    }
}

cr_export(void, app_wasm_handle_text_command, (void* app_ptr, uint32_t canvas, uint32_t command, bool select)) {
    struct app_window* window = app_wasm_window_for_canvas(app_ptr, canvas);
    if (window) {
        app_window_add_text_command_event(window, (enum app_text_command)command, select);
    }
}

cr_export(void, app_wasm_handle_text_input_dismissed, (void* app_ptr, uint32_t canvas)) {
    struct app_window* window = app_wasm_window_for_canvas(app_ptr, canvas);
    if (window) {
        app_window_add_text_input_dismissed_event(window);
    }
}

cr_export(bool, app_wasm_text_input_hit_test, (void* app_ptr, uint32_t canvas, double x, double y)) {
    struct app_window* window = app_wasm_window_for_canvas(app_ptr, canvas);
    if (window == NULL || window->text_input_hit_test == NULL) {
        return false;
    }
    return window->text_input_hit_test(make_double2(x, y), window->text_input_hit_test_user);
}

#endif
