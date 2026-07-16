#include "app_tests.h"

#if cr_macos || cr_wasm

#include "app_private.h"
#include <stdio.h>
#include <string.h>

static bool app_test_core_report(const char* name, bool ok, const char* detail) {
    printf("app core %s: %s%s%s\n", name, ok ? "ok" : "FAIL", detail[0] ? " " : "", detail);
    return ok;
}

static void app_test_core_on_update(struct app_window* window) {
}

static bool app_test_core_queue(struct cr_mem* mem) {
    struct app_event_queue queue;
    app_event_queue_init(&queue, mem);
    for (uint32_t i = 0; i < 3000; i++) {
        struct app_event event = { .type = app_event_type_pointer, .pointer.id = i };
        app_event_queue_push(&queue, &event);
    }
    bool grow_ok = queue.count == 3000 && queue.events[2999].pointer.id == 2999 && queue.events[17].pointer.id == 17;
    app_event_queue_reset(&queue);
    grow_ok = grow_ok && queue.count == 0 && queue.capacity >= 3000;
    char detail[128];
    snprintf(detail, sizeof(detail), "capacity %u", queue.capacity);
    bool ok = app_test_core_report("event queue grows", grow_ok, detail);

    uint32_t big_size = 100000;
    char* big = cr_malloc(mem, big_size, 0, cr_loc);
    for (uint32_t i = 0; i < big_size; i++) {
        big[i] = (char)('a' + i % 26);
    }
    const char* a = app_event_queue_push_text(&queue, "hello", 5);
    const char* b = app_event_queue_push_text(&queue, big, big_size);
    const char* c = app_event_queue_push_text(&queue, "tail", 4);
    bool arena_ok = memcmp(a, "hello", 5) == 0 && memcmp(b, big, big_size) == 0 && memcmp(c, "tail", 4) == 0
        && queue.chunks != NULL && queue.chunks->capacity >= big_size && queue.chunks->next != NULL;
    app_event_queue_reset(&queue);
    arena_ok = arena_ok && queue.chunks == NULL;
    const char* d = app_event_queue_push_text(&queue, "x", 1);
    arena_ok = arena_ok && d[0] == 'x' && queue.chunks != NULL && queue.chunks->capacity == 4096 && queue.chunks->next == NULL;
    app_event_queue_reset(&queue);
    arena_ok = arena_ok && queue.chunks != NULL && queue.chunks->used == 0;
    ok = app_test_core_report("text arena", arena_ok, "") && ok;
    cr_free(mem, big, cr_loc);
    app_event_queue_deinit(&queue);
    return ok;
}

static bool app_test_core_target_desc(struct app* app) {
    struct app_window window;
    memset(&window, 0, sizeof(window));
    window.app = app;
    window.format = (struct app_target_format) { .pixel_format = app_pixel_format_bgra8, .sampleable = true };
    struct app_target_desc desc = app_window_make_target_desc(&window, make_double2(100.5, 50.25), make_uint2(0, 101), 2.0);
    bool ok = all_int2(to_int2_from_uint2(desc.px_size) == make_int2(1, 101))
        && is_equal_double2(desc.pt_size, make_double2(100.5, 50.25))
        && desc.to_px_from_pt == 2.0
        && app_target_format_equal(desc.format, window.format);
    char detail[128];
    snprintf(detail, sizeof(detail), "px %ux%u", desc.px_size.x, desc.px_size.y);
    return app_test_core_report("target desc", ok, detail);
}

#if cr_macos
static bool app_test_core_hidden_window(struct app* app) {
    struct app_window* window = app_window_create(app, &(struct app_window_desc) {
        .on_update = app_test_core_on_update,
        .pos = make_double2(40.0, 40.0),
        .size = make_double2(200.0, 150.0),
        .hidden = true,
    });
    app_window_add_text_event(window, "abc", 3);
    app_window_add_key_event(window, app_key_a, app_modifier_shift, true, false);
    const struct app_event* events;
    uint32_t count;
    app_window_take_events(window, &events, &count);
    bool ok = count == 2
        && events[0].type == app_event_type_text && events[0].text.size == 3 && memcmp(events[0].text.utf8, "abc", 3) == 0
        && events[1].type == app_event_type_key && events[1].key.key == app_key_a && events[1].key.modifiers == app_modifier_shift && events[1].key.down;
    const struct app_event* first = events;
    app_window_add_text_event(window, "d", 1);
    app_window_take_events(window, &events, &count);
    ok = ok && count == 1 && events != first && events[0].text.utf8[0] == 'd' && first[0].text.utf8[0] == 'a';
    app_window_take_events(window, &events, &count);
    ok = ok && count == 0;
    ok = ok && app_window_get_hidden(window) && app_window_get_frame_interval(window) == 0.0;
    app_window_destroy(window);
    char detail[64];
    snprintf(detail, sizeof(detail), "%u events", count);
    return app_test_core_report("hidden window events", ok, detail);
}
#endif

bool app_test_core_(struct app* app) {
    bool ok = app_test_core_queue(cr_mem_libc);
    ok = app_test_core_target_desc(app) && ok;
#if cr_macos
    ok = app_test_core_hidden_window(app) && ok;
#endif
    return ok;
}

#endif
