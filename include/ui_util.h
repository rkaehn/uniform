#pragma once

#include "cr.h"
#include "app.h"
#include "ui.h"
#include "ui_input.h"

cr_hdef enum ui_color_space ui_color_space_from_app(enum app_color_space color_space) {
    switch (color_space) {
    case app_color_space_srgb: return ui_color_space_srgb;
    case app_color_space_display_p3: return ui_color_space_display_p3;
    }
    return ui_color_space_srgb;
}

cr_hdef enum ui_pixel_format ui_pixel_format_from_app(enum app_pixel_format pixel_format) {
    return pixel_format == app_pixel_format_rgba16f ? ui_pixel_format_rgba16f : ui_pixel_format_bgra8;
}

cr_hdef struct ui_render_desc ui_util_window_render_desc(struct app_window* window) {
    struct app_target_format format = app_window_get_target_desc(window).format;
    return (struct ui_render_desc) {
        .output_color_space = ui_color_space_from_app(format.color_space),
        .premultiplied = true,
        .dither = format.pixel_format != app_pixel_format_rgba16f,
    };
}

cr_hdef void ui_util_window_sync(struct app_window* window, struct ui_node prev_root, double to_app_from_pt) {
    struct app_target_desc target_desc = app_window_get_target_desc(window);
    ui_set_to_px_from_pt(target_desc.to_px_from_pt * to_app_from_pt);

    const struct app_event* events;
    uint32_t event_count;
    app_window_take_events(window, &events, &event_count);
    ui_input_dispatch(prev_root, app_window_get_timestamp(window), events, event_count, 1.0 / to_app_from_pt);

    app_window_set_cursor(window, ui_input_get_cursor());

    bool active = ui_input_get_focus() != 0;
    app_window_text_input_set_active(window, active);

    if (active) {
        double2 caret_origin_pt;
        double2 caret_size_pt;
        if (ui_input_get_caret_rect(&caret_origin_pt, &caret_size_pt)) {
            app_window_text_input_set_caret_rect(window, caret_origin_pt * to_app_from_pt, caret_size_pt * to_app_from_pt);
        }
    }

    const char* clipboard;
    uint64_t clipboard_size;
    if (ui_input_take_clipboard(&clipboard, &clipboard_size)) {
        app_set_clipboard_text(app_window_get_app(window), clipboard, clipboard_size);
    }
}
