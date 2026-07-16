#include "app_tests_internal.h"

#if cr_macos

#include "gpu.h"
#import <QuartzCore/QuartzCore.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

struct app_test_ctx {
    struct app* app;
    struct app_test_capture* capture;
    struct app_test_pattern* pattern;
    CGRect region;
    double scale;
    double frame_interval;
    struct app_test_result* results;
    uint32_t result_count;
    bool ok;
};

struct app_test_stats {
    uint32_t count;
    uint32_t shown;
    uint32_t bad;
    int32_t first_shown_frame;
    bool first_shown_valid;
    int2 first_shown_size;
    int2 last_dec_size;
    char detail[256];
};

static void app_test_report(struct app_test_ctx* ctx, const char* name, bool ok, const char* detail) {
    printf("app window %s: %s%s%s\n", name, ok ? "ok" : "FAIL", detail[0] ? " " : "", detail);
    ctx->ok = ctx->ok && ok;
}

static int2 app_test_px_size(const struct app_test_ctx* ctx, double2 size_pt) {
    double2 px = size_pt * ctx->scale;
    return to_int2_from_double2(round_double2(px));
}

static bool app_test_near(double2 pos, double2 expected, double tolerance) {
    double2 d = pos - expected;
    return all_long2((long2)(d < tolerance) & (long2)(d > -tolerance));
}

NSWindow* app_test_ns_window_(struct app_test_win* win) {
    return (NSWindow*)app_window_get_native(win->window);
}

static void app_test_on_update(struct app_window* window) {
    struct app_test_win* win = app_window_get_user(window);
    struct app_test_ctx* ctx = win->ctx;
    struct app_target_desc target_desc = app_window_get_target_desc(window);

    uint32_t frame = win->frame_count++;
    uint32_t index = frame % app_test_frame_max;
    win->event_ts[index] = win->pending_event_ts;
    win->pending_event_ts = 0.0;
    win->render_ts[index] = CACurrentMediaTime();
    win->serials[index] = 0;
    win->formats_seen |= 1u << (uint32_t)target_desc.format.pixel_format;
    if (win->format_toggle_every != 0 && frame % win->format_toggle_every == win->format_toggle_every - 1) {
        struct app_target_format format = target_desc.format;
        format.pixel_format = format.pixel_format == app_pixel_format_rgba16f ? app_pixel_format_bgra8 : app_pixel_format_rgba16f;
        app_window_set_target_format(window, format);
    }

    if (win->slow_s > 0.0) {
        usleep((useconds_t)(win->slow_s * 1e6));
    }
    if (win->skip_present) {
        return;
    }

    float gray = win->blink && (frame & 1u) ? 0.6f : 0.5f;
    float4 fill = make_float4(gray, gray, gray, win->style == app_window_style_transparent ? 0.5f : 1.0f);
    struct gpu_command_buffer* command_buffer = gpu_command_buffer_create(app_get_device(ctx->app), "app test window frame");
    struct gpu_texture* target = app_window_acquire_target(window);
    if (target) {
        app_test_pattern_encode_(ctx->pattern, command_buffer, target, target_desc.px_size, frame & 0xffffu, fill);
    }
    win->serials[index] = app_window_present(window, command_buffer, target);

    if (win->destroy_in_frame) {
        win->destroy_in_frame = false;
        app_window_destroy(window);
    }
}

static void app_test_on_close_request(struct app_window* window) {
    struct app_test_win* win = app_window_get_user(window);
    win->close_count++;
}

static struct app_test_win* app_test_win_alloc(struct app_test_ctx* ctx) {
    struct app_test_win* win = cr_malloc(cr_mem_libc, sizeof(*win), 0, cr_loc);
    memset(win, 0, sizeof(*win));
    win->ctx = ctx;
    return win;
}

static struct app_test_win* app_test_win_create(struct app_test_ctx* ctx, uint32_t track, const struct app_window_desc* desc) {
    struct app_test_win* win = app_test_win_alloc(ctx);
    win->track = track;
    win->style = desc->style;
    app_test_capture_set_track_(ctx->capture, track, desc->pos, desc->style == app_window_style_transparent);
    struct app_window_desc window_desc = *desc;
    window_desc.user = win;
    window_desc.on_update = app_test_on_update;
    window_desc.on_close_request = app_test_on_close_request;
    window_desc.title = "app test";
    win->window = app_window_create(ctx->app, &window_desc);
    return win;
}

static void app_test_win_track(struct app_test_win* win) {
    app_test_capture_set_track_(win->ctx->capture, win->track, app_window_get_pos(win->window), win->style == app_window_style_transparent);
}

static void app_test_win_destroy(struct app_test_win* win) {
    struct app_test_ctx* ctx = win->ctx;
    app_test_capture_clear_track_(ctx->capture, win->track);
    if (win->window) {
        app_window_destroy(win->window);
        app_test_run_for_(ctx->app, 0.15);
    }
    cr_free(cr_mem_libc, win, cr_loc);
}

static CGRect app_test_server_bounds(struct app_test_win* win) {
    CGRect bounds = CGRectNull;
    CFArrayRef list = CGWindowListCopyWindowInfo(kCGWindowListOptionIncludingWindow, (CGWindowID)app_test_ns_window_(win).windowNumber);
    if (list && CFArrayGetCount(list) > 0) {
        CFDictionaryRef info = CFArrayGetValueAtIndex(list, 0);
        CFDictionaryRef bounds_dict = CFDictionaryGetValue(info, kCGWindowBounds);
        if (bounds_dict) {
            CGRectMakeWithDictionaryRepresentation(bounds_dict, &bounds);
        }
    }
    if (list) {
        CFRelease(list);
    }
    return bounds;
}

static void app_test_phase_begin(struct app_test_ctx* ctx) {
    app_test_capture_clear_results_(ctx->capture);
}

static void app_test_phase_end(struct app_test_ctx* ctx, double seconds) {
    app_test_run_for_(ctx->app, seconds);
    ctx->result_count = app_test_capture_take_results_(ctx->capture, ctx->results, app_test_result_max);
}

static void app_test_phase(struct app_test_ctx* ctx, double seconds) {
    app_test_phase_begin(ctx);
    app_test_phase_end(ctx, seconds);
}

static void app_test_drag_phase(struct app_test_ctx* ctx, struct app_test_win* win, double2 from_pt, double2 to_pt, uint32_t steps) {
    app_test_phase_begin(ctx);
    app_test_drag_(win, from_pt, to_pt, steps);
    app_test_phase_end(ctx, 0.0);
}

static struct app_test_stats app_test_track_stats(const struct app_test_ctx* ctx, uint32_t track) {
    struct app_test_stats s = { .first_shown_frame = -1 };
    for (uint32_t i = 0; i < ctx->result_count; i++) {
        const struct app_test_result* r = &ctx->results[i];
        if (r->track != track) {
            continue;
        }
        s.count++;
        if (r->absent) {
            continue;
        }
        bool consistent = r->valid && all_int2(r->dec_size == r->ext_size) && r->outside_dark;
        if (s.shown == 0) {
            s.first_shown_valid = consistent;
            s.first_shown_frame = r->valid ? r->dec_frame : -1;
            s.first_shown_size = r->dec_size;
        }
        s.shown++;
        if (consistent == false) {
            if (s.bad == 0) {
                snprintf(s.detail, sizeof(s.detail), "frame at %.3f: valid %d dec %dx%d f %d ext %dx%d outside dark %d", r->time, r->valid, r->dec_size.x, r->dec_size.y, r->dec_frame, r->ext_size.x, r->ext_size.y, r->outside_dark);
            }
            s.bad++;
        } else {
            s.last_dec_size = r->dec_size;
        }
    }
    return s;
}

static uint32_t app_test_count_events(const struct app_event* events, uint32_t count, enum app_event_type type, enum app_pointer_event_type pointer_type) {
    uint32_t n = 0;
    for (uint32_t i = 0; i < count; i++) {
        if (events[i].type == type && (type != app_event_type_pointer || events[i].pointer.type == pointer_type)) {
            n++;
        }
    }
    return n;
}

static void app_test_first_frame(struct app_test_ctx* ctx) {
    double2 pos = make_double2(40.0, 40.0);
    double2 size = make_double2(400.0, 300.0);
    app_test_phase_begin(ctx);
    struct app_test_win* win = app_test_win_create(ctx, 0, &(struct app_window_desc) { .style = app_window_style_bordered, .pos = pos, .size = size, .resizable = true });
    app_test_phase_end(ctx, 0.5);
    struct app_test_stats s = app_test_track_stats(ctx, 0);
    char detail[512];
    snprintf(detail, sizeof(detail), "%u captured, %u shown, first shown frame %d, %u bad %s", s.count, s.shown, s.first_shown_frame, s.bad, s.detail);
    app_test_report(ctx, "first frame", s.shown > 10 && s.bad == 0 && s.first_shown_valid && s.first_shown_frame >= 0 && s.first_shown_frame <= 3, detail);

    double2 got_pos = app_window_get_pos(win->window);
    double2 got_size = app_window_get_size(win->window);
    CGRect server = app_test_server_bounds(win);
    snprintf(detail, sizeof(detail), "pos %.0f,%.0f size %.0fx%.0f server %.0f,%.0f %.0fx%.0f", got_pos.x, got_pos.y, got_size.x, got_size.y, server.origin.x, server.origin.y, server.size.width, server.size.height);
    bool geometry_ok = is_equal_double2(got_pos, pos) && is_equal_double2(got_size, size)
        && is_equal_double2(make_double2(server.origin.x, server.origin.y), pos) && is_equal_double2(make_double2(server.size.width, server.size.height), size);
    app_test_report(ctx, "geometry", geometry_ok, detail);

    struct app_target_desc target_desc = app_window_get_target_desc(win->window);
    int2 px_size = app_test_px_size(ctx, size);
    bool target_ok = all_int2(to_int2_from_uint2(target_desc.px_size) == px_size)
        && target_desc.to_px_from_pt == ctx->scale && target_desc.format.pixel_format == app_pixel_format_rgba16f && target_desc.format.sampleable == false;
    snprintf(detail, sizeof(detail), "px %ux%u scale %.1f", target_desc.px_size.x, target_desc.px_size.y, target_desc.to_px_from_pt);
    app_test_report(ctx, "target desc", target_ok, detail);

    bool focused = app_window_get_focused(win->window) && app_test_ns_window_(win).isKeyWindow;
    app_test_report(ctx, "created window is focused", focused, "");

    app_test_win_destroy(win);
}

static void app_test_resize(struct app_test_ctx* ctx) {
    double2 pos = make_double2(40.0, 40.0);
    double2 size = make_double2(400.0, 300.0);
    struct app_test_win* win = app_test_win_create(ctx, 0, &(struct app_window_desc) { .style = app_window_style_bordered, .pos = pos, .size = size, .resizable = true });
    app_test_run_for_(ctx->app, 0.3);

    for (uint32_t pass = 0; pass < 2; pass++) {
        win->slow_s = pass == 0 ? 0.0 : 0.04;
        double2 corner = pos + size - 1.0;
        app_test_drag_phase(ctx, win, corner, corner + make_double2(180.0, 120.0), 60);
        struct app_test_stats s = app_test_track_stats(ctx, 0);
        double2 got_size = app_window_get_size(win->window);
        char detail[512];
        snprintf(detail, sizeof(detail), "%u captured, %u bad, size after %.0fx%.0f, last content %dx%d %s", s.count, s.bad, got_size.x, got_size.y, s.last_dec_size.x, s.last_dec_size.y, s.detail);
        bool grew = all_long2((long2)(got_size >= size + 20.0));
        bool final_matches = all_int2(s.last_dec_size == app_test_px_size(ctx, got_size));
        app_test_report(ctx, pass == 0 ? "live resize" : "live resize slow frames", s.count > 20 && s.bad == 0 && grew && final_matches, detail);

        win->slow_s = 0.0;
        app_test_phase_begin(ctx);
        app_window_set_size(win->window, size);
        app_test_phase_end(ctx, 0.3);
        s = app_test_track_stats(ctx, 0);
        got_size = app_window_get_size(win->window);
        snprintf(detail, sizeof(detail), "%u captured, %u bad, size %.0fx%.0f %s", s.count, s.bad, got_size.x, got_size.y, s.detail);
        app_test_report(ctx, "set size", s.count > 5 && s.bad == 0 && is_equal_double2(got_size, size) && s.last_dec_size.x == app_test_px_size(ctx, size).x, detail);
    }

    double2 top_left = pos + 1.0;
    app_test_drag_(win, top_left, top_left - 30.0, 40);
    double2 got_pos = app_window_get_pos(win->window);
    double2 got_size = app_window_get_size(win->window);
    app_test_win_track(win);
    app_test_phase(ctx, 0.2);
    struct app_test_stats s = app_test_track_stats(ctx, 0);
    char detail[512];
    snprintf(detail, sizeof(detail), "pos %.0f,%.0f size %.0fx%.0f, %u bad after %s", got_pos.x, got_pos.y, got_size.x, got_size.y, s.bad, s.detail);
    app_test_report(ctx, "live resize from top left", all_long2((long2)(got_pos < pos)) && all_long2((long2)(got_size > size)) && s.bad == 0 && s.shown > 3, detail);

    app_test_win_destroy(win);
}

static void app_test_position(struct app_test_ctx* ctx) {
    double2 pos = make_double2(40.0, 40.0);
    double2 size = make_double2(300.0, 200.0);
    struct app_test_win* win = app_test_win_create(ctx, 0, &(struct app_window_desc) { .style = app_window_style_bordered, .pos = pos, .size = size, .resizable = true });
    app_test_run_for_(ctx->app, 0.2);

    double2 moved = make_double2(260.0, 190.0);
    app_window_set_pos(win->window, moved);
    app_test_win_track(win);
    app_test_phase(ctx, 0.3);
    struct app_test_stats s = app_test_track_stats(ctx, 0);
    double2 got_pos = app_window_get_pos(win->window);
    CGRect server = app_test_server_bounds(win);
    char detail[512];
    snprintf(detail, sizeof(detail), "pos %.0f,%.0f server %.0f,%.0f, %u shown %u bad %s", got_pos.x, got_pos.y, server.origin.x, server.origin.y, s.shown, s.bad, s.detail);
    app_test_report(ctx, "set pos", is_equal_double2(got_pos, moved) && is_equal_double2(make_double2(server.origin.x, server.origin.y), moved) && s.shown > 5 && s.bad == 0, detail);

    struct app_screen_desc screen = app_window_get_screen(win->window);
    NSRect primary = NSScreen.screens.firstObject.frame;
    bool on_primary = is_equal_double2(screen.pos, make_double2(0.0, 0.0)) && is_equal_double2(screen.size, make_double2(primary.size.width, primary.size.height));
    bool contains = all_long2((long2)(got_pos >= screen.pos)) && all_long2((long2)(got_pos + size <= screen.pos + screen.size));
    snprintf(detail, sizeof(detail), "screen %.0f,%.0f %.0fx%.0f, window %.0f,%.0f", screen.pos.x, screen.pos.y, screen.size.x, screen.size.y, got_pos.x, got_pos.y);
    app_test_report(ctx, "screen of window", on_primary && contains, detail);

    app_test_win_destroy(win);
}

static void app_test_hidden(struct app_test_ctx* ctx) {
    double2 pos = make_double2(40.0, 40.0);
    double2 size = make_double2(300.0, 200.0);
    app_test_phase_begin(ctx);
    struct app_test_win* win = app_test_win_create(ctx, 0, &(struct app_window_desc) { .style = app_window_style_bordered, .pos = pos, .size = size, .resizable = true, .hidden = true });
    app_test_phase_end(ctx, 0.3);
    uint32_t frames_hidden = win->frame_count;
    struct app_test_stats s = app_test_track_stats(ctx, 0);
    char detail[512];
    snprintf(detail, sizeof(detail), "%u captured, %u shown, %u frames rendered", s.count, s.shown, frames_hidden);
    app_test_report(ctx, "created hidden", s.shown == 0 && frames_hidden == 0 && app_window_get_hidden(win->window), detail);

    app_test_phase_begin(ctx);
    app_window_set_hidden(win->window, false);
    app_test_phase_end(ctx, 0.4);
    s = app_test_track_stats(ctx, 0);
    snprintf(detail, sizeof(detail), "%u captured, %u shown, first shown frame %d, %u bad %s", s.count, s.shown, s.first_shown_frame, s.bad, s.detail);
    app_test_report(ctx, "shown after hidden", s.shown > 5 && s.bad == 0 && s.first_shown_valid && s.first_shown_frame <= 3 && app_window_get_hidden(win->window) == false, detail);

    uint32_t frames_before = win->frame_count;
    app_window_set_hidden(win->window, true);
    app_test_run_for_(ctx->app, 0.2);
    app_test_phase(ctx, 0.3);
    uint32_t frames_while_hidden = win->frame_count - frames_before;
    s = app_test_track_stats(ctx, 0);
    snprintf(detail, sizeof(detail), "%u captured, %u shown, %u frames rendered while hidden", s.count, s.shown, frames_while_hidden);
    app_test_report(ctx, "hidden again", s.shown == 0 && frames_while_hidden <= 2, detail);

    app_test_phase_begin(ctx);
    app_window_set_hidden(win->window, false);
    app_test_phase_end(ctx, 0.4);
    s = app_test_track_stats(ctx, 0);
    snprintf(detail, sizeof(detail), "%u shown, %u bad, first shown valid %d", s.shown, s.bad, s.first_shown_valid);
    app_test_report(ctx, "shown again", s.shown > 5 && s.bad == 0 && s.first_shown_valid, detail);

    app_test_win_destroy(win);
}

static void app_test_styles(struct app_test_ctx* ctx) {
    double2 size = make_double2(300.0, 200.0);
    double2 pos_bordered = make_double2(40.0, 40.0);
    double2 pos_borderless = make_double2(400.0, 40.0);
    double2 pos_transparent = make_double2(40.0, 320.0);
    app_test_phase_begin(ctx);
    struct app_test_win* bordered = app_test_win_create(ctx, 0, &(struct app_window_desc) { .style = app_window_style_bordered, .pos = pos_bordered, .size = size, .resizable = true });
    struct app_test_win* borderless = app_test_win_create(ctx, 1, &(struct app_window_desc) { .style = app_window_style_borderless, .pos = pos_borderless, .size = size, .resizable = true });
    struct app_test_win* transparent = app_test_win_create(ctx, 2, &(struct app_window_desc) { .style = app_window_style_transparent, .pos = pos_transparent, .size = size });
    app_test_phase_end(ctx, 0.5);
    char detail[512];
    const char* names[3] = { "bordered", "borderless", "transparent" };
    for (uint32_t t = 0; t < 3; t++) {
        struct app_test_stats s = app_test_track_stats(ctx, t);
        snprintf(detail, sizeof(detail), "%s: %u shown, %u bad, first shown frame %d %s", names[t], s.shown, s.bad, s.first_shown_frame, s.detail);
        app_test_report(ctx, "style frames", s.shown > 10 && s.bad == 0 && s.first_shown_valid && s.first_shown_frame <= 3, detail);
    }

    uchar4 px;
    bool got = app_test_capture_pixel_(ctx->capture, pos_borderless + 0.5, &px);
    snprintf(detail, sizeof(detail), "corner pixel %u %u %u %u", px.x, px.y, px.z, px.w);
    app_test_report(ctx, "borderless has square opaque corners", got && px.x > 150 && px.y < 60 && px.z > 150 && px.w == 255, detail);

    got = app_test_capture_pixel_(ctx->capture, pos_bordered + 0.5, &px);
    snprintf(detail, sizeof(detail), "corner pixel %u %u %u %u", px.x, px.y, px.z, px.w);
    app_test_report(ctx, "bordered has rounded corners", got && px.w < 200, detail);

    got = app_test_capture_pixel_(ctx->capture, pos_transparent + make_double2(150.0, 100.0), &px);
    snprintf(detail, sizeof(detail), "center pixel %u %u %u %u", px.x, px.y, px.z, px.w);
    app_test_report(ctx, "transparent content alpha", got && px.w > 100 && px.w < 160 && px.y > 40 && px.y < 90, detail);

    got = app_test_capture_pixel_(ctx->capture, pos_transparent + make_double2(150.0, size.y + 6.0), &px);
    snprintf(detail, sizeof(detail), "pixel below %u %u %u %u", px.x, px.y, px.z, px.w);
    app_test_report(ctx, "transparent has no shadow", got && px.w == 0, detail);

    got = app_test_capture_pixel_(ctx->capture, pos_bordered + make_double2(150.0, size.y + 6.0), &px);
    snprintf(detail, sizeof(detail), "pixel below %u %u %u %u", px.x, px.y, px.z, px.w);
    app_test_report(ctx, "bordered has shadow", got && px.w > 20, detail);

    double2 corner = pos_borderless + size - 1.0;
    app_test_drag_phase(ctx, borderless, corner, corner + make_double2(120.0, 80.0), 40);
    struct app_test_stats s = app_test_track_stats(ctx, 1);
    double2 got_size = app_window_get_size(borderless->window);
    snprintf(detail, sizeof(detail), "size %.0fx%.0f, %u bad %s", got_size.x, got_size.y, s.bad, s.detail);
    app_test_report(ctx, "borderless user resize", all_long2((long2)(got_size >= size + 20.0)) && s.bad == 0 && s.shown > 10, detail);

    app_window_set_pos(borderless->window, pos_bordered + make_double2(120.0, 80.0));
    app_test_win_track(borderless);
    app_test_run_for_(ctx->app, 0.2);
    double2 probe = pos_bordered + make_double2(200.0, 150.0);
    app_window_order_front(bordered->window);
    app_test_run_for_(ctx->app, 0.2);
    bool got1 = app_test_capture_pixel_(ctx->capture, probe, &px);
    bool bordered_on_top = got1 && px.w == 255 && px.z > 100 && px.y > 100;
    app_window_order_back(bordered->window);
    app_test_run_for_(ctx->app, 0.2);
    bool got2 = app_test_capture_pixel_(ctx->capture, probe, &px);
    bool borderless_on_top = got2 && px.w == 255 && px.z > 100;
    app_window_order_front(bordered->window);
    app_test_run_for_(ctx->app, 0.2);
    bool got3 = app_test_capture_pixel_(ctx->capture, probe, &px);
    NSUInteger level_bordered = [NSApp.orderedWindows indexOfObject:app_test_ns_window_(bordered)];
    NSUInteger level_borderless = [NSApp.orderedWindows indexOfObject:app_test_ns_window_(borderless)];
    snprintf(detail, sizeof(detail), "probe ok %d %d %d, ordered index bordered %lu borderless %lu", got1, got2, got3, (unsigned long)level_bordered, (unsigned long)level_borderless);
    app_test_report(ctx, "order front and back", bordered_on_top && borderless_on_top && level_bordered < level_borderless, detail);

    app_window_focus(borderless->window);
    app_test_run_for_(ctx->app, 0.1);
    bool borderless_focused = app_window_get_focused(borderless->window) && app_window_get_focused(bordered->window) == false && app_test_ns_window_(borderless).isKeyWindow;
    app_window_focus(transparent->window);
    app_test_run_for_(ctx->app, 0.1);
    bool transparent_focused = app_window_get_focused(transparent->window) && app_window_get_focused(borderless->window) == false;
    app_window_focus(bordered->window);
    app_test_run_for_(ctx->app, 0.1);
    bool bordered_focused = app_window_get_focused(bordered->window) && app_window_get_focused(transparent->window) == false;
    snprintf(detail, sizeof(detail), "borderless %d transparent %d bordered %d", borderless_focused, transparent_focused, bordered_focused);
    app_test_report(ctx, "focus", borderless_focused && transparent_focused && bordered_focused, detail);

    app_test_win_destroy(bordered);
    app_test_win_destroy(borderless);
    app_test_win_destroy(transparent);
}

static void app_test_events(struct app_test_ctx* ctx) {
    double2 size = make_double2(300.0, 200.0);
    double2 pos_a = make_double2(40.0, 40.0);
    double2 pos_b = make_double2(400.0, 40.0);
    struct app_test_win* a = app_test_win_create(ctx, 0, &(struct app_window_desc) { .style = app_window_style_bordered, .pos = pos_a, .size = size, .resizable = true });
    struct app_test_win* b = app_test_win_create(ctx, 1, &(struct app_window_desc) { .style = app_window_style_borderless, .pos = pos_b, .size = size, .resizable = true });
    a->skip_present = true;
    b->skip_present = true;
    app_test_run_for_(ctx->app, 0.2);
    const struct app_event* events;
    uint32_t count;
    app_window_take_events(a->window, &events, &count);
    app_window_take_events(b->window, &events, &count);

    double2 pa = pos_a + make_double2(100.0, 120.0);
    double2 pb = pos_b + make_double2(50.0, 60.0);
    double t0 = NSProcessInfo.processInfo.systemUptime;
    app_test_post_mouse_(a, NSEventTypeMouseMoved, pa, 0);
    app_test_post_mouse_(a, NSEventTypeLeftMouseDown, pa, NSEventModifierFlagShift);
    app_test_post_mouse_(a, NSEventTypeLeftMouseDragged, pa + make_double2(10.0, 5.0), NSEventModifierFlagShift);
    app_test_post_mouse_(a, NSEventTypeLeftMouseUp, pa + make_double2(10.0, 5.0), 0);
    app_test_post_mouse_(b, NSEventTypeMouseMoved, pb, 0);
    app_test_post_mouse_(b, NSEventTypeRightMouseDown, pb, 0);
    app_test_post_mouse_(b, NSEventTypeRightMouseUp, pb, 0);
    app_test_run_for_(ctx->app, 0.2);
    app_test_scroll_(b, make_double2(50.0, 60.0), make_int2(1, -3));
    app_test_run_for_(ctx->app, 0.1);

    app_window_take_events(a->window, &events, &count);
    char detail[512];
    bool a_ok = count == 4
        && events[0].type == app_event_type_pointer && events[0].pointer.type == app_pointer_event_type_hover_moved && app_test_near(events[0].pointer.pos, make_double2(100.0, 120.0), 0.01)
        && events[1].type == app_event_type_pointer && events[1].pointer.type == app_pointer_event_type_touch_began && (events[1].pointer.modifiers & app_modifier_shift) != 0 && events[1].pointer.source == app_pointer_source_indirect && events[1].pointer.button == app_pointer_button_primary
        && events[2].type == app_event_type_pointer && events[2].pointer.type == app_pointer_event_type_touch_moved && app_test_near(events[2].pointer.pos, make_double2(110.0, 125.0), 0.01)
        && events[3].type == app_event_type_pointer && events[3].pointer.type == app_pointer_event_type_touch_ended && events[3].pointer.timestamp >= t0 && events[3].pointer.id == 1
        && events[0].pointer.pressure == 0.0 && events[1].pointer.pressure == 1.0 && events[3].pointer.pressure == 0.0;
    snprintf(detail, sizeof(detail), "window a: %u events, first type %d", count, count ? (int)events[0].type : -1);
    app_test_report(ctx, "events routed to window a", a_ok, detail);

    app_window_take_events(b->window, &events, &count);
    uint32_t hover = app_test_count_events(events, count, app_event_type_pointer, app_pointer_event_type_hover_moved);
    uint32_t began = app_test_count_events(events, count, app_event_type_pointer, app_pointer_event_type_touch_began);
    uint32_t ended = app_test_count_events(events, count, app_event_type_pointer, app_pointer_event_type_touch_ended);
    uint32_t scrolls = app_test_count_events(events, count, app_event_type_scroll, 0);
    bool secondary = false;
    for (uint32_t i = 0; i < count; i++) {
        if (events[i].type == app_event_type_pointer && events[i].pointer.type == app_pointer_event_type_touch_began) {
            secondary = events[i].pointer.button == app_pointer_button_secondary;
        }
    }
    bool scroll_ok = false;
    double2 scroll_delta = make_double2(0.0, 0.0);
    double2 scroll_pos = make_double2(0.0, 0.0);
    for (uint32_t i = 0; i < count; i++) {
        if (events[i].type == app_event_type_scroll) {
            scroll_delta = events[i].scroll.delta;
            scroll_pos = events[i].scroll.pos;
            scroll_ok = app_test_near(scroll_delta, make_double2(-1.0, 3.0) * 13.0, 0.5) && app_test_near(scroll_pos, make_double2(50.0, 60.0), 1.0);
        }
    }
    snprintf(detail, sizeof(detail), "window b: %u events, hover %u began %u ended %u scroll %u delta %.1f,%.1f pos %.1f,%.1f secondary %d", count, hover, began, ended, scrolls, scroll_delta.x, scroll_delta.y, scroll_pos.x, scroll_pos.y, secondary);
    app_test_report(ctx, "events routed to window b", count == 4 && hover == 1 && began == 1 && ended == 1 && scrolls == 1 && scroll_ok && secondary, detail);

    app_window_focus(a->window);
    app_test_run_for_(ctx->app, 0.1);
    app_window_text_input_set_active(a->window, true);
    app_window_take_events(a->window, &events, &count);
    app_test_post_key_(a, NSEventTypeKeyDown, "a", 0, 0);
    app_test_post_key_(a, NSEventTypeKeyDown, "", 123, NSEventModifierFlagFunction | NSEventModifierFlagNumericPad);
    app_test_run_for_(ctx->app, 0.2);
    app_window_take_events(a->window, &events, &count);
    bool text_ok = count == 4
        && events[0].type == app_event_type_key && events[0].key.key == app_key_a && events[0].key.down
        && events[1].type == app_event_type_text && events[1].text.size == 1 && events[1].text.utf8[0] == 'a'
        && events[2].type == app_event_type_key && events[2].key.key == app_key_left && events[2].key.down
        && events[3].type == app_event_type_text_command && events[3].text_command.command == app_text_command_move_left && events[3].text_command.select == false;
    snprintf(detail, sizeof(detail), "%u events, active %d", count, app_window_text_input_get_active(a->window));
    app_test_report(ctx, "text input events", text_ok && app_window_text_input_get_active(a->window), detail);

    app_window_text_input_set_active(a->window, false);
    app_test_post_key_(a, NSEventTypeFlagsChanged, "", 0x38, NSEventModifierFlagShift | 0x2);
    app_test_post_key_(a, NSEventTypeKeyDown, "a", 0, NSEventModifierFlagShift);
    app_test_post_key_(a, NSEventTypeKeyUp, "a", 0, NSEventModifierFlagShift);
    app_test_post_key_(a, NSEventTypeFlagsChanged, "", 0x38, 0);
    app_test_run_for_(ctx->app, 0.1);
    app_window_take_events(a->window, &events, &count);
    bool key_ok = count == 4
        && events[0].type == app_event_type_key && events[0].key.key == app_key_shift_left && events[0].key.down && (events[0].key.modifiers & app_modifier_shift) != 0
        && events[1].type == app_event_type_key && events[1].key.key == app_key_a && events[1].key.down && (events[1].key.modifiers & app_modifier_shift) != 0
        && events[2].type == app_event_type_key && events[2].key.key == app_key_a && events[2].key.down == false
        && events[3].type == app_event_type_key && events[3].key.key == app_key_shift_left && events[3].key.down == false && events[3].key.modifiers == 0;
    snprintf(detail, sizeof(detail), "%u events", count);
    app_test_report(ctx, "key events without text input", key_ok, detail);

    uint32_t before = a->close_count;
    [app_test_ns_window_(a) performClose:nil];
    app_test_run_for_(ctx->app, 0.1);
    bool still_there = app_test_ns_window_(a).isVisible;
    snprintf(detail, sizeof(detail), "close callbacks %u, visible %d", a->close_count - before, still_there);
    app_test_report(ctx, "close request", a->close_count == before + 1 && still_there, detail);

    app_test_win_destroy(a);
    app_test_win_destroy(b);
}

static void app_test_marked_text(struct app_test_ctx* ctx) {
    double2 size = make_double2(300.0, 200.0);
    struct app_test_win* win = app_test_win_create(ctx, 0, &(struct app_window_desc) { .style = app_window_style_bordered, .pos = make_double2(40.0, 40.0), .size = size, .resizable = true });
    win->skip_present = true;
    app_test_run_for_(ctx->app, 0.2);
    app_window_focus(win->window);
    app_window_text_input_set_active(win->window, true);
    const struct app_event* events;
    uint32_t count;
    app_window_take_events(win->window, &events, &count);

    id<NSTextInputClient> client = (id<NSTextInputClient>)app_test_ns_window_(win).contentView;
    NSString* marked = @"\u3042\u3044";
    [client setMarkedText:marked selectedRange:NSMakeRange(1, 0) replacementRange:NSMakeRange(NSNotFound, 0)];
    bool had_marked = [client hasMarkedText];
    [client insertText:marked replacementRange:NSMakeRange(NSNotFound, 0)];
    bool has_marked = [client hasMarkedText];
    app_window_take_events(win->window, &events, &count);
    const char* utf8 = marked.UTF8String;
    bool marked_ok = count == 2
        && events[0].type == app_event_type_marked_text && events[0].marked_text.size == 6 && events[0].marked_text.caret == 3 && memcmp(events[0].marked_text.utf8, utf8, 6) == 0
        && events[1].type == app_event_type_text && events[1].text.size == 6 && memcmp(events[1].text.utf8, utf8, 6) == 0;
    char detail[512];
    snprintf(detail, sizeof(detail), "%u events, marked before %d after %d", count, had_marked, has_marked);
    app_test_report(ctx, "marked text", marked_ok && had_marked && has_marked == false, detail);

    uint32_t big_size = 20000;
    char* big = cr_malloc(cr_mem_libc, big_size + 1, 0, cr_loc);
    for (uint32_t i = 0; i < big_size; i++) {
        big[i] = (char)('a' + i % 26);
    }
    big[big_size] = 0;
    [client insertText:[NSString stringWithUTF8String:big] replacementRange:NSMakeRange(NSNotFound, 0)];
    app_window_take_events(win->window, &events, &count);
    bool big_ok = count == 1 && events[0].type == app_event_type_text && events[0].text.size == big_size && memcmp(events[0].text.utf8, big, big_size) == 0;
    snprintf(detail, sizeof(detail), "%u events, size %u", count, count ? events[0].text.size : 0);
    app_test_report(ctx, "large text event", big_ok, detail);
    cr_free(cr_mem_libc, big, cr_loc);

    app_window_text_input_set_active(win->window, false);
    app_test_win_destroy(win);
}

static void app_test_format(struct app_test_ctx* ctx) {
    double2 size = make_double2(300.0, 200.0);
    struct app_test_win* win = app_test_win_create(ctx, 0, &(struct app_window_desc) { .style = app_window_style_bordered, .pos = make_double2(40.0, 40.0), .size = size, .resizable = true });
    app_test_run_for_(ctx->app, 0.3);
    win->formats_seen = 0;
    win->format_toggle_every = 7;
    app_test_phase(ctx, 1.0);
    win->format_toggle_every = 0;
    struct app_test_stats s = app_test_track_stats(ctx, 0);
    uint32_t both = (1u << (uint32_t)app_pixel_format_rgba16f) | (1u << (uint32_t)app_pixel_format_bgra8);
    char detail[512];
    snprintf(detail, sizeof(detail), "%u shown, %u bad, formats %x %s", s.shown, s.bad, win->formats_seen, s.detail);
    app_test_report(ctx, "format toggles", s.shown > 20 && s.bad == 0 && win->formats_seen == both, detail);

    app_window_set_target_format(win->window, (struct app_target_format) { .pixel_format = app_pixel_format_bgra8, .sampleable = true });
    app_test_phase(ctx, 0.3);
    s = app_test_track_stats(ctx, 0);
    struct app_target_desc target_desc = app_window_get_target_desc(win->window);
    snprintf(detail, sizeof(detail), "pixel format %d sampleable %d, %u shown %u bad", target_desc.format.pixel_format, target_desc.format.sampleable, s.shown, s.bad);
    app_test_report(ctx, "set target format", target_desc.format.pixel_format == app_pixel_format_bgra8 && target_desc.format.sampleable && s.shown > 5 && s.bad == 0, detail);

    app_test_win_destroy(win);
}

static void app_test_fps(struct app_test_ctx* ctx) {
    double2 size = make_double2(300.0, 200.0);
    struct app_test_win* win = app_test_win_create(ctx, 0, &(struct app_window_desc) { .style = app_window_style_bordered, .pos = make_double2(40.0, 40.0), .size = size, .resizable = true });
    app_test_run_for_(ctx->app, 0.3);
    app_window_set_fps(win->window, 30);
    app_test_run_for_(ctx->app, 0.3);
    uint32_t before = win->frame_count;
    app_test_run_for_(ctx->app, 1.0);
    uint32_t at_30 = win->frame_count - before;
    app_window_set_fps(win->window, 0);
    app_test_run_for_(ctx->app, 0.3);
    before = win->frame_count;
    app_test_run_for_(ctx->app, 1.0);
    uint32_t at_max = win->frame_count - before;
    double interval = app_window_get_frame_interval(win->window);
    char detail[512];
    snprintf(detail, sizeof(detail), "%u frames at 30 fps, %u at default, interval %.2f ms", at_30, at_max, interval * 1000.0);
    app_test_report(ctx, "fps", at_30 >= 15 && at_30 <= 45 && at_max >= 50 && at_max > at_30 && interval > 0.0 && interval < 0.05, detail);
    app_test_win_destroy(win);
}

static void app_test_hidden_resize(struct app_test_ctx* ctx) {
    double2 pos = make_double2(40.0, 40.0);
    double2 size = make_double2(300.0, 200.0);
    double2 grown = make_double2(360.0, 260.0);
    struct app_test_win* win = app_test_win_create(ctx, 0, &(struct app_window_desc) { .style = app_window_style_bordered, .pos = pos, .size = size, .resizable = true });
    app_test_run_for_(ctx->app, 0.3);
    app_window_set_hidden(win->window, true);
    app_test_run_for_(ctx->app, 0.1);
    app_window_set_size(win->window, grown);
    app_test_phase_begin(ctx);
    app_window_set_hidden(win->window, false);
    app_test_phase_end(ctx, 0.4);
    struct app_test_stats s = app_test_track_stats(ctx, 0);
    int2 expected = app_test_px_size(ctx, grown);
    char detail[512];
    snprintf(detail, sizeof(detail), "%u shown, %u bad, first %dx%d expected %dx%d %s", s.shown, s.bad, s.first_shown_size.x, s.first_shown_size.y, expected.x, expected.y, s.detail);
    app_test_report(ctx, "resize while hidden", s.shown > 5 && s.bad == 0 && s.first_shown_valid && all_int2(s.first_shown_size == expected), detail);
    app_test_win_destroy(win);
}

static void app_test_occlusion(struct app_test_ctx* ctx) {
    double2 size = make_double2(300.0, 200.0);
    struct app_test_win* under = app_test_win_create(ctx, 0, &(struct app_window_desc) { .style = app_window_style_bordered, .pos = make_double2(40.0, 40.0), .size = size, .resizable = true });
    app_test_run_for_(ctx->app, 0.3);
    struct app_test_win* over = app_test_win_create(ctx, 1, &(struct app_window_desc) { .style = app_window_style_bordered, .pos = make_double2(20.0, 20.0), .size = make_double2(400.0, 300.0), .resizable = true });
    app_test_run_for_(ctx->app, 0.6);
    uint32_t before = under->frame_count;
    app_test_run_for_(ctx->app, 0.5);
    uint32_t while_covered = under->frame_count - before;
    app_test_win_destroy(over);
    app_test_run_for_(ctx->app, 0.5);
    before = under->frame_count;
    app_test_run_for_(ctx->app, 0.5);
    uint32_t after = under->frame_count - before;
    char detail[512];
    snprintf(detail, sizeof(detail), "%u frames while covered, %u after uncovering", while_covered, after);
    app_test_report(ctx, "occluded window pauses", while_covered <= 2 && after >= 20, detail);
    app_test_win_destroy(under);
}

static void app_test_latency(struct app_test_ctx* ctx) {
    double2 pos = make_double2(40.0, 40.0);
    double2 size = make_double2(400.0, 300.0);
    struct app_test_win* win = app_test_win_create(ctx, 0, &(struct app_window_desc) { .style = app_window_style_bordered, .pos = pos, .size = size, .resizable = true });
    app_test_run_for_(ctx->app, 0.4);
    app_test_phase_begin(ctx);
    app_test_hover_(win, pos + make_double2(100.0, 150.0), make_double2(1.0, 0.0), 40, 0.037);
    app_test_phase_end(ctx, 1.8);

    bool seen[app_test_frame_max] = { 0 };
    uint32_t n_event = 0;
    double event_sum = 0.0;
    double event_max = 0.0;
    uint32_t n_render = 0;
    double render_sum = 0.0;
    double render_max = 0.0;
    for (uint32_t i = 0; i < ctx->result_count; i++) {
        const struct app_test_result* r = &ctx->results[i];
        if (r->valid == false || r->display_time <= 0.0) {
            continue;
        }
        uint32_t f = (uint32_t)r->dec_frame;
        if (f >= app_test_frame_max || seen[f]) {
            continue;
        }
        seen[f] = true;
        double render_latency = r->display_time - win->render_ts[f];
        if (render_latency > 0.0 && render_latency < 0.5) {
            n_render++;
            render_sum += render_latency;
            render_max = max_double(render_max, render_latency);
        }
        double event_ts = win->event_ts[f];
        if (event_ts > 0.0) {
            double latency = r->display_time - event_ts;
            n_event++;
            event_sum += latency;
            event_max = max_double(event_max, latency);
        }
    }
    double interval = ctx->frame_interval;
    double event_avg = n_event ? event_sum / n_event : 0.0;
    double render_avg = n_render ? render_sum / n_render : 0.0;
    char detail[512];
    snprintf(detail, sizeof(detail), "event to display n=%u avg %.1f ms max %.1f ms, render to display n=%u avg %.1f ms max %.1f ms, frame %.1f ms",
             n_event, event_avg * 1000.0, event_max * 1000.0, n_render, render_avg * 1000.0, render_max * 1000.0, interval * 1000.0);
    bool ok = n_event >= 25 && n_render >= 40
        && event_avg <= interval * 2.8 && event_max <= interval * 4.0
        && render_avg <= interval * 2.5 && render_max <= interval * 3.5;
    app_test_report(ctx, "latency", ok, detail);

    uint64_t presented = app_window_get_presented(win->window);
    uint32_t last = (win->frame_count - 1) % app_test_frame_max;
    uint64_t last_serial = win->serials[last];
    uint64_t lag_serial = win->frame_count >= 4 ? win->serials[(win->frame_count - 4) % app_test_frame_max] : 0;
    app_test_run_for_(ctx->app, 0.1);
    uint64_t presented_after = app_window_get_presented(win->window);
    snprintf(detail, sizeof(detail), "presented %llu, last serial %llu, three frames back %llu, after wait %llu", (unsigned long long)presented, (unsigned long long)last_serial, (unsigned long long)lag_serial, (unsigned long long)presented_after);
    app_test_report(ctx, "presented serial", presented > 0 && presented <= last_serial && presented_after >= last_serial && presented >= lag_serial, detail);

    app_test_win_destroy(win);
}

static void app_test_multi(struct app_test_ctx* ctx) {
    double2 size = make_double2(240.0, 200.0);
    double2 positions[3] = { make_double2(40.0, 40.0), make_double2(380.0, 40.0), make_double2(720.0, 40.0) };
    enum app_window_style styles[3] = { app_window_style_bordered, app_window_style_borderless, app_window_style_transparent };
    struct app_test_win* wins[3];
    app_test_phase_begin(ctx);
    for (uint32_t i = 0; i < 3; i++) {
        wins[i] = app_test_win_create(ctx, i, &(struct app_window_desc) { .style = styles[i], .pos = positions[i], .size = size, .resizable = true });
    }
    wins[1]->slow_s = 0.02;
    app_test_phase_end(ctx, 1.0);
    double cycle_s = max_double(ctx->frame_interval, wins[1]->slow_s);
    uint32_t expected = (uint32_t)(0.9 / cycle_s);
    bool ok = true;
    char failing[256] = "";
    for (uint32_t i = 0; i < 3; i++) {
        struct app_test_stats s = app_test_track_stats(ctx, i);
        bool window_ok = s.shown > 20 && s.bad == 0 && s.first_shown_valid && wins[i]->frame_count >= expected / 2;
        if (window_ok == false && failing[0] == 0) {
            snprintf(failing, sizeof(failing), ", window %u: %u shown, %u bad %s", i, s.shown, s.bad, s.detail);
        }
        ok = ok && window_ok;
    }
    char detail[512];
    snprintf(detail, sizeof(detail), "frames rendered %u %u %u, expected about %u each%s", wins[0]->frame_count, wins[1]->frame_count, wins[2]->frame_count, expected, failing);
    app_test_report(ctx, "three windows", ok, detail);

    double2 corner = positions[0] + size - 1.0;
    app_test_drag_phase(ctx, wins[0], corner, corner + make_double2(40.0, 60.0), 40);
    bool all_ok = true;
    detail[0] = 0;
    for (uint32_t i = 0; i < 3; i++) {
        struct app_test_stats s = app_test_track_stats(ctx, i);
        if (s.bad != 0 || s.shown < 5) {
            all_ok = false;
            snprintf(detail, sizeof(detail), "window %u: %u shown %u bad %s", i, s.shown, s.bad, s.detail);
        }
    }
    app_test_report(ctx, "resize one of three", all_ok, detail);

    for (uint32_t i = 0; i < 3; i++) {
        app_test_win_destroy(wins[i]);
    }
}

static void app_test_lifetime(struct app_test_ctx* ctx) {
    double2 size = make_double2(200.0, 150.0);

    struct app_test_win* win = app_test_win_create(ctx, 0, &(struct app_window_desc) { .style = app_window_style_bordered, .pos = make_double2(40.0, 40.0), .size = size, .resizable = true });
    app_test_run_for_(ctx->app, 0.2);
    win->destroy_in_frame = true;
    app_test_run_for_(ctx->app, 0.2);
    app_test_capture_clear_track_(ctx->capture, 0);
    app_test_run_for_(ctx->app, 0.2);
    bool destroyed = win->destroy_in_frame == false;
    win->window = NULL;
    app_test_win_destroy(win);
    app_test_report(ctx, "destroy inside frame", destroyed, "");

    struct app_test_win* doomed = app_test_win_alloc(ctx);
    doomed->destroy_in_frame = true;
    doomed->window = app_window_create(ctx->app, &(struct app_window_desc) {
        .user = doomed,
        .on_update = app_test_on_update,
        .on_close_request = app_test_on_close_request,
        .pos = make_double2(40.0, 40.0),
        .size = size,
        .resizable = true,
    });
    bool doomed_ok = doomed->window == NULL && doomed->frame_count == 1 && doomed->destroy_in_frame == false;
    char doomed_detail[128];
    snprintf(doomed_detail, sizeof(doomed_detail), "create returned %s after %u frames", doomed->window ? "a window" : "NULL", doomed->frame_count);
    app_test_run_for_(ctx->app, 0.2);
    cr_free(cr_mem_libc, doomed, cr_loc);
    app_test_report(ctx, "destroy inside first frame", doomed_ok, doomed_detail);

    struct app_test_win* resized = app_test_win_create(ctx, 0, &(struct app_window_desc) { .style = app_window_style_bordered, .pos = make_double2(40.0, 40.0), .size = size, .resizable = true });
    app_test_run_for_(ctx->app, 0.2);
    resized->destroy_in_frame = true;
    app_window_set_size(resized->window, size + 20.0);
    app_test_run_for_(ctx->app, 0.2);
    bool resize_destroyed = resized->destroy_in_frame == false;
    resized->window = NULL;
    app_test_win_destroy(resized);
    app_test_report(ctx, "destroy inside resize frame", resize_destroyed, "");

    double t0 = CACurrentMediaTime();
    for (uint32_t i = 0; i < 20; i++) {
        struct app_test_win* stress = app_test_win_create(ctx, 0, &(struct app_window_desc) {
            .style = (enum app_window_style)(i % 3),
            .pos = make_double2(40.0 + (i % 5) * 20.0, 40.0 + (i % 3) * 20.0),
            .size = size,
            .resizable = true,
        });
        app_test_run_for_(ctx->app, 0.02);
        app_test_win_destroy(stress);
    }
    char detail[512];
    snprintf(detail, sizeof(detail), "20 windows in %.0f ms", (CACurrentMediaTime() - t0) * 1000.0);
    app_test_report(ctx, "create and destroy stress", true, detail);

    app_test_phase(ctx, 0.3);
    uint32_t shown = 0;
    for (uint32_t i = 0; i < ctx->result_count; i++) {
        if (ctx->results[i].absent == false) {
            shown++;
        }
    }
    app_test_report(ctx, "nothing left on screen", shown == 0, "");
}

bool app_test_window_(struct app* app) {
    NSScreen* screen = NSScreen.screens.firstObject;
    double2 screen_size = make_double2(screen.frame.size.width, screen.frame.size.height);
    double2 region_size = make_double2(1000.0, 700.0);
    if (screen_size.x < region_size.x || screen_size.y < region_size.y) {
        printf("app window: screen %.0fx%.0f too small for the window tests, skipping\n", screen_size.x, screen_size.y);
        return true;
    }

    struct app_test_ctx ctx = {
        .app = app,
        .region = CGRectMake(0.0, 0.0, region_size.x, region_size.y),
        .scale = screen.backingScaleFactor,
        .frame_interval = 1.0 / (double)max_int32((int32_t)screen.maximumFramesPerSecond, 30),
        .ok = true,
    };

    cr_autoreleasepool {
        ctx.pattern = app_test_pattern_create_(app_get_device(app));
        ctx.results = cr_malloc(cr_mem_libc, app_test_result_max * sizeof(*ctx.results), 0, cr_loc);
        ctx.capture = app_test_capture_create_(ctx.region, ctx.scale);

        struct app_test_win* anchor = app_test_win_create(&ctx, app_test_track_max - 1, &(struct app_window_desc) {
            .style = app_window_style_transparent,
            .pos = make_double2(ctx.region.size.width - 8.0, ctx.region.size.height - 8.0),
            .size = make_double2(8.0, 8.0),
        });
        anchor->blink = true;
        app_test_capture_clear_track_(ctx.capture, app_test_track_max - 1);
        app_test_run_for_(app, 0.2);

        if (app_test_capture_start_(ctx.capture)) {
            printf("app window: screen %.0fx%.0f scale %.1f refresh %.1f Hz\n", screen_size.x, screen_size.y, ctx.scale, 1.0 / ctx.frame_interval);
            app_test_run_for_(app, 0.3);

            app_test_first_frame(&ctx);
            app_test_resize(&ctx);
            app_test_position(&ctx);
            app_test_hidden(&ctx);
            app_test_styles(&ctx);
            app_test_events(&ctx);
            app_test_marked_text(&ctx);
            app_test_format(&ctx);
            app_test_fps(&ctx);
            app_test_hidden_resize(&ctx);
            app_test_occlusion(&ctx);
            app_test_latency(&ctx);
            app_test_multi(&ctx);
            app_test_lifetime(&ctx);

            app_test_capture_stop_(ctx.capture);
        } else {
            printf("app window: screen capture unavailable, skipping\n");
        }

        app_test_win_destroy(anchor);
        app_test_capture_destroy_(ctx.capture);
        app_test_pattern_destroy_(ctx.pattern);
        cr_free(cr_mem_libc, ctx.results, cr_loc);
    }
    return ctx.ok;
}

#endif
