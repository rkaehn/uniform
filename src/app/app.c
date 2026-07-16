#include "app_private.h"

#define app_text_chunk_min 4096u
#define app_text_chunk_keep_max 65536u

static void app_window_list_insert(struct app* app, struct app_window* window) {
    window->prev = NULL;
    window->next = app->windows;
    if (app->windows) {
        app->windows->prev = window;
    }
    app->windows = window;
}

static void app_window_list_remove(struct app* app, struct app_window* window) {
    if (window->prev) {
        window->prev->next = window->next;
    } else {
        app->windows = window->next;
    }
    if (window->next) {
        window->next->prev = window->prev;
    }
}

void app_ready(struct app* app) {
    if (app->ready) {
        return;
    }
    app->ready = true;
    if (app->desc.on_start) {
        app->desc.on_start(app);
    }
}

void app_quit_requested(struct app* app) {
    if (app->desc.on_quit_request) {
        app->desc.on_quit_request(app);
    } else {
        app_quit(app);
    }
}

void app_window_requested(struct app* app) {
    if (app->desc.on_window_request) {
        app->desc.on_window_request(app);
    }
}

void app_window_close_requested(struct app_window* window) {
    if (window && window->closed == false && window->on_close_request) {
        window->on_close_request(window);
    }
}

bool app_target_format_equal(struct app_target_format a, struct app_target_format b) {
    return a.color_space == b.color_space && a.pixel_format == b.pixel_format && a.sampleable == b.sampleable;
}

struct app_target_desc app_window_make_target_desc(struct app_window* window, double2 pt_size, uint2 px_size, double to_px_from_pt) {
    int2 px = max_int2(to_int2_from_uint2(px_size), make_int2(1, 1));
    return (struct app_target_desc) {
        .px_size = to_uint2_from_int2(px),
        .pt_size = pt_size,
        .to_px_from_pt = to_px_from_pt,
        .format = window->format,
    };
}

void app_event_queue_init(struct app_event_queue* queue, struct cr_mem* mem) {
    memset(queue, 0, sizeof(*queue));
    queue->mem = mem;
}

static void app_event_queue_free_chunks(struct app_event_queue* queue, struct app_text_chunk* chunk) {
    while (chunk) {
        struct app_text_chunk* next = chunk->next;
        cr_free(queue->mem, chunk, cr_loc);
        chunk = next;
    }
}

void app_event_queue_deinit(struct app_event_queue* queue) {
    if (queue->events) {
        cr_free(queue->mem, queue->events, cr_loc);
    }
    app_event_queue_free_chunks(queue, queue->chunks);
    memset(queue, 0, sizeof(*queue));
}

void app_event_queue_reset(struct app_event_queue* queue) {
    queue->count = 0;
    struct app_text_chunk* head = queue->chunks;
    if (head == NULL) {
        return;
    }
    if (head->capacity > app_text_chunk_keep_max) {
        app_event_queue_free_chunks(queue, head);
        queue->chunks = NULL;
        return;
    }
    app_event_queue_free_chunks(queue, head->next);
    head->next = NULL;
    head->used = 0;
}

void app_event_queue_push(struct app_event_queue* queue, const struct app_event* event) {
    if (queue->count == queue->capacity) {
        uint32_t capacity = queue->capacity ? queue->capacity * 2 : 64;
        uint64_t size = (uint64_t)capacity * sizeof(*queue->events);
        queue->events = queue->events ? cr_realloc(queue->mem, queue->events, size, cr_loc) : cr_malloc(queue->mem, size, 16, cr_loc);
        queue->capacity = capacity;
    }
    queue->events[queue->count++] = *event;
}

const char* app_event_queue_push_text(struct app_event_queue* queue, const char* utf8, uint64_t size) {
    assert(size < (1u << 30));
    struct app_text_chunk* chunk = queue->chunks;
    if (chunk == NULL || (uint64_t)(chunk->capacity - chunk->used) < size) {
        uint32_t capacity = max_uint32(chunk ? chunk->capacity * 2 : app_text_chunk_min, (uint32_t)size);
        struct app_text_chunk* fresh = cr_malloc(queue->mem, sizeof(*fresh) + capacity, 16, cr_loc);
        fresh->next = chunk;
        fresh->capacity = capacity;
        fresh->used = 0;
        queue->chunks = fresh;
        chunk = fresh;
    }
    char* dst = (char*)(chunk + 1) + chunk->used;
    memcpy(dst, utf8, (size_t)size);
    chunk->used += (uint32_t)size;
    return dst;
}

static struct app_event_queue* app_window_active_queue(struct app_window* window) {
    return &window->queues[window->queue_index];
}

void app_window_add_event(struct app_window* window, const struct app_event* event) {
    if (window == NULL || window->closed) {
        return;
    }
    app_event_queue_push(app_window_active_queue(window), event);
}

void app_window_cancel_events(struct app_window* window) {
    if (window == NULL || window->closed) {
        return;
    }
    app_event_queue_reset(app_window_active_queue(window));
    struct app_event event = {
        .type = app_event_type_cancel_all,
    };
    app_window_add_event(window, &event);
}

void app_window_add_text_event(struct app_window* window, const char* utf8, uint64_t size) {
    if (window == NULL || window->closed || size == 0) {
        return;
    }
    struct app_event event = {
        .type = app_event_type_text,
        .text.utf8 = app_event_queue_push_text(app_window_active_queue(window), utf8, size),
        .text.size = (uint32_t)size,
    };
    app_window_add_event(window, &event);
}

void app_window_add_text_command_event(struct app_window* window, enum app_text_command command, bool select) {
    struct app_event event = {
        .type = app_event_type_text_command,
        .text_command.command = command,
        .text_command.select = select,
    };
    app_window_add_event(window, &event);
}

void app_window_add_marked_text_event(struct app_window* window, const char* utf8, uint64_t size, uint64_t caret) {
    if (window == NULL || window->closed) {
        return;
    }
    struct app_event event = {
        .type = app_event_type_marked_text,
        .marked_text.utf8 = size > 0 ? app_event_queue_push_text(app_window_active_queue(window), utf8, size) : "",
        .marked_text.size = (uint32_t)size,
        .marked_text.caret = (uint32_t)min_uint64(caret, size),
    };
    app_window_add_event(window, &event);
}

void app_window_add_floating_caret_event(struct app_window* window, enum app_floating_caret_phase phase, double2 pos) {
    struct app_event event = {
        .type = app_event_type_floating_caret,
        .floating_caret.phase = phase,
        .floating_caret.pos = pos,
    };
    app_window_add_event(window, &event);
}

void app_window_add_key_event(struct app_window* window, enum app_key key, uint32_t modifiers, bool down, bool repeat) {
    struct app_event event = {
        .type = app_event_type_key,
        .key.key = key,
        .key.modifiers = modifiers,
        .key.down = down,
        .key.repeat = repeat,
    };
    app_window_add_event(window, &event);
}

void app_window_add_text_input_dismissed_event(struct app_window* window) {
    struct app_event event = {
        .type = app_event_type_text_input_dismissed,
    };
    app_window_add_event(window, &event);
}

void app_window_take_events(struct app_window* window, const struct app_event** out_events, uint32_t* out_event_count) {
    struct app_event_queue* taken = app_window_active_queue(window);
    window->queue_index ^= 1;
    app_event_queue_reset(app_window_active_queue(window));
    *out_events = taken->events;
    *out_event_count = taken->count;
}

static bool app_window_take_slot(struct app_window* window, bool sync) {
    struct gpu_device* device = window->app->device;
    uint64_t serial = window->slot_serials[window->slot_next];
#if cr_macos || cr_ios
    if (sync) {
        gpu_device_wait(device, serial);
    }
#endif
    return gpu_device_is_completed(device, serial);
}

static void app_window_free(struct app_window* window) {
    struct app* app = window->app;
    assert(window->closed && window->in_frame == false);
    app_platform_window_destroy(window);
    app_window_list_remove(app, window);
    app_event_queue_deinit(&window->queues[0]);
    app_event_queue_deinit(&window->queues[1]);
    cr_free(app->mem, window, cr_loc);
}

void app_sweep(struct app* app) {
    for (struct app_window* window = app->windows; window;) {
        struct app_window* next = window->next;
        if (window->closed) {
            app_window_free(window);
        }
        window = next;
    }
}

static void app_close_windows(struct app* app, bool vacate) {
    for (struct app_window* window = app->windows; window; window = window->next) {
        cr_check(window->in_frame == false, "windows cannot close inside their updates");
        if (window->closed == false) {
            window->closed = true;
            window->vacated = vacate;
            app_platform_window_close(window);
        }
    }
}

static struct app_window* app_window_vacated(struct app* app) {
    struct app_window* oldest = NULL;
    for (struct app_window* window = app->windows; window; window = window->next) {
        if (window->vacated) {
            oldest = window;
        }
    }
    return oldest;
}

static uint32_t app_window_vacated_count(struct app* app) {
    uint32_t count = 0;
    for (struct app_window* window = app->windows; window; window = window->next) {
        count += window->vacated ? 1u : 0u;
    }
    return count;
}

void app_run(const struct app_desc* desc) {
    struct app* app = cr_malloc(desc->mem, sizeof(*app), 16, cr_loc);
    memset(app, 0, sizeof(*app));
    app->mem = desc->mem;
    app->desc = *desc;
    app_platform_create(app);
    app_platform_run(app);
}

void app_restart(struct app* app, const struct app_desc* desc) {
    if (app->desc.on_quit) {
        app->desc.on_quit(app);
    }
    app_close_windows(app, true);
    app->desc.user = desc->user;
    app->desc.on_start = desc->on_start;
    app->desc.on_quit_request = desc->on_quit_request;
    app->desc.on_quit = desc->on_quit;
    app->desc.on_window_request = desc->on_window_request;
    if (app->ready && app->desc.on_start) {
        app->desc.on_start(app);
    }
    for (uint32_t requests = app_window_vacated_count(app); requests > 0; requests--) {
        app_window_requested(app);
    }
    app_sweep(app);
}

void app_finish(struct app* app) {
    if (app->desc.on_quit) {
        app->desc.on_quit(app);
    }
    app_close_windows(app, false);
    app_sweep(app);
    assert(app->windows == NULL);
    app_platform_destroy(app);
    if (app->device) {
        gpu_device_destroy(app->device);
    }
    cr_free(app->mem, app, cr_loc);
}

void* app_get_user(struct app* app) {
    return app->desc.user;
}

struct gpu_device* app_get_device(struct app* app) {
    return app->device;
}

struct app_window* app_window_create(struct app* app, const struct app_window_desc* desc) {
    assert(app->device != NULL);
    cr_check(desc->on_update != NULL, "a window needs an on_update callback");
    struct app_window* window = cr_malloc(app->mem, sizeof(*window), 16, cr_loc);
    memset(window, 0, sizeof(*window));
    window->app = app;
    window->user = desc->user;
    window->on_update = desc->on_update;
    window->on_close_request = desc->on_close_request;
    window->style = desc->style;
    window->fps = desc->fps;
    window->format = desc->format;
    app_platform_window_clamp_format(window, &window->format);
    window->hidden = desc->hidden;
    window->slot_count = desc->slot_count == 0 ? 3 : min_uint32(desc->slot_count, app_slot_count_max);
    window->sync_timestamp = -INFINITY;
    app_event_queue_init(&window->queues[0], app->mem);
    app_event_queue_init(&window->queues[1], app->mem);
    window->target_desc.format = window->format;
    app_window_list_insert(app, window);
    struct app_window_desc placed = *desc;
    struct app_window* vacated = app_window_vacated(app);
    if (vacated) {
        vacated->vacated = false;
        placed.pos = app_window_get_pos(vacated);
        placed.size = app_window_get_size(vacated);
        placed.center = false;
    }
    app_platform_window_create(window, &placed, vacated);
    app_platform_window_clamp_format(window, &window->format);
    return window->closed ? NULL : window;
}

void app_window_destroy(struct app_window* window) {
    if (window->closed) {
        return;
    }
    window->closed = true;
    app_platform_window_close(window);
    app_platform_request_sweep(window->app);
}

struct app* app_window_get_app(struct app_window* window) {
    return window->app;
}

void* app_window_get_user(struct app_window* window) {
    return window->user;
}

struct app_target_desc app_window_get_target_desc(struct app_window* window) {
    return window->target_desc;
}

double app_window_get_timestamp(struct app_window* window) {
    return window->timestamp;
}

double app_window_get_frame_interval(struct app_window* window) {
    return window->frame_interval;
}

double app_window_get_delta(struct app_window* window) {
    return window->delta;
}

bool app_window_get_is_dark(struct app_window* window) {
    return window->is_dark;
}

double app_window_get_keyboard_inset(struct app_window* window) {
    return window->keyboard_inset;
}

bool app_window_get_hidden(struct app_window* window) {
    return window->hidden;
}

void app_window_set_hidden(struct app_window* window, bool hidden) {
    if (window->hidden == hidden) {
        return;
    }
    window->hidden = hidden;
    app_platform_window_set_hidden(window, hidden);
}

void app_window_set_size(struct app_window* window, double2 size_pt) {
    app_platform_window_set_size(window, size_pt);
}

void app_window_set_title(struct app_window* window, const char* title) {
    app_platform_window_set_title(window, title ? title : "");
}

void app_window_set_fps(struct app_window* window, uint32_t fps) {
    window->fps = fps;
    app_platform_window_set_fps(window, fps);
}

void app_window_set_target_format(struct app_window* window, struct app_target_format format) {
    window->format = format;
    app_platform_window_clamp_format(window, &window->format);
}

void app_window_set_cursor(struct app_window* window, enum app_cursor cursor) {
    if (window->cursor == cursor) {
        return;
    }
    window->cursor = cursor;
    app_platform_window_set_cursor(window, cursor);
}

void app_window_text_input_set_active(struct app_window* window, bool active) {
    if (window->text_input_active == active) {
        return;
    }
    window->text_input_active = active;
    app_platform_window_set_text_input_active(window, active);
}

bool app_window_text_input_get_active(struct app_window* window) {
    return window->text_input_active;
}

void app_window_text_input_set_caret_rect(struct app_window* window, double2 origin_pt, double2 size_pt) {
    window->caret_origin_pt = origin_pt;
    window->caret_size_pt = size_pt;
    app_platform_window_set_caret_rect(window, origin_pt, size_pt);
}

void app_window_text_input_set_hit_test(struct app_window* window, bool (*hit_test)(double2 pos_pt, void* user), void* user) {
    window->text_input_hit_test = hit_test;
    window->text_input_hit_test_user = user;
}

void app_window_frame(struct app_window* window, double timestamp, double interval, bool sync) {
    if (window == NULL || window->closed || window->in_frame) {
        return;
    }
    struct app* app = window->app;
    if (window->hidden || app->is_in_background) {
        return;
    }
    if (interval > 0.0) {
        window->frame_interval = interval;
    }
    if (sync == false && timestamp <= window->sync_timestamp + window->frame_interval * 0.25) {
        return;
    }

    double2 pt_size;
    uint2 px_size;
    double to_px_from_pt;
    if (app_platform_window_get_target_size(window, &pt_size, &px_size, &to_px_from_pt) == false) {
        return;
    }
    if (all_long2((long2)(pt_size > 0.0)) == false || any_int2(px_size == 0) || to_px_from_pt <= 0.0) {
        return;
    }
    struct app_target_desc desired = app_window_make_target_desc(window, pt_size, px_size, to_px_from_pt);
    app_platform_window_apply_target(window, &desired);
    if (gpu_device_is_completed(app->device, gpu_device_get_pipelines_serial(app->device)) == false) {
        return;
    }
    if (app_window_take_slot(window, sync) == false) {
        return;
    }

    window->target_desc = desired;
    window->delta = window->has_timestamp ? clamp_double(timestamp - window->timestamp, 0.0, app_delta_max) : 0.0;
    window->has_timestamp = true;
    window->timestamp = timestamp;
    window->in_frame = true;
    window->presented = false;
    if (sync) {
        window->sync_timestamp = timestamp;
    }

    window->on_update(window);

    window->in_frame = false;
    app_platform_window_discard_target(window);
}

struct gpu_texture* app_window_acquire_target(struct app_window* window) {
    cr_check(window->in_frame, "a window target is acquired inside on_update");
    return app_platform_window_acquire_target(window);
}

uint64_t app_window_present(struct app_window* window, struct gpu_command_buffer* command_buffer, struct gpu_texture* target) {
    cr_check(window->in_frame, "a window presents inside on_update");
    cr_check(window->presented == false, "a window presents once per update");
    window->presented = true;

    uint64_t serial = app_platform_window_present(window, command_buffer, target);
    window->slot_serials[window->slot_next] = serial;
    window->slot_next = (window->slot_next + 1) % window->slot_count;
    return serial;
}
