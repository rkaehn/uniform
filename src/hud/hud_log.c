#include "hud_internal.h"
#include "ui_gestures.h"
#include "gpu.h"
#include "log.h"

#define hud_log_width_min 96.0
#define hud_log_width_max 1024.0
#define hud_log_height_min 64.0
#define hud_log_height_max 768.0
#define hud_log_left_padding 0.0
#define hud_log_left_hidden_padding 20.0
#define hud_log_bottom_padding 0.0
#define hud_log_toolbar_height 32.0
#define hud_log_pt_size 12.0
#define hud_log_text_color ui_rgba(0.95f, 0.5f, 0.05f, 1.0f)
#define hud_log_text_color_pressed ui_rgba(0.95f, 0.5f, 0.05f, 0.5f)

void hud_log_deinit(struct hud* hud) {
    if (hud->log.atlas.ready) {
        ui_graphic_destroy(hud->log.atlas.graphic);
    }
}

static int32_t hud_log_div_floor(int32_t a, int32_t b) {
    return (a >= 0 ? a : a - b + 1) / b;
}

static int32_t hud_log_div_ceil(int32_t a, int32_t b) {
    return (a >= 0 ? a + b - 1 : a) / b;
}

static void hud_log_atlas_update(struct hud* hud) {
    double to_px_from_pt = ui_get_to_px_from_pt();

    if (hud->log.atlas.ready && hud->log.atlas.to_px_from_pt == to_px_from_pt) {
        return;
    }

    if (hud->log.atlas.ready) {
        ui_graphic_destroy(hud->log.atlas.graphic);
    }

    memset(&hud->log.atlas, 0, sizeof(hud->log.atlas));
    hud->log.atlas.to_px_from_pt = to_px_from_pt;

    struct ui_node probe = ui_text(&(struct ui_text) {
        .str = hud_str(hud->font, "M", ui_white, hud_log_pt_size),
    });

    ui_layout(probe);

    double2 probe_size = ui_get_size(probe);

    ui_release(probe);

    uint32_t char_width = (uint32_t)ceil(probe_size.x * to_px_from_pt);
    uint32_t char_height = (uint32_t)ceil(probe_size.y * to_px_from_pt);
    assert(char_width > 0 && char_height > 0);

    uint32_t width = char_width * 32;
    uint32_t height = char_height * 8;

    struct ui_node atlas_root = ui_frame(&(struct ui_frame) {
        .size = ui_size_fix((double)width / to_px_from_pt, (double)height / to_px_from_pt),
    });

    for (uint32_t cp = 32; cp < 256; cp++) {
        if (cp == 127) {
            continue;
        }

        char str[3] = { 0 };
        if (cp < 0x80) {
            str[0] = (char)cp;
        } else {
            str[0] = (char)(0xc0 | (cp >> 6));
            str[1] = (char)(0x80 | (cp & 0x3f));
        }

        ui_text(&(struct ui_text) {
            .parent = atlas_root,
            .pos = ui_pos_xy((double)((cp & 0x1f) * char_width) / to_px_from_pt, (double)((cp >> 5) * char_height) / to_px_from_pt),
            .str = hud_str(hud->font, str, ui_white, hud_log_pt_size),
        });
    }

    ui_layout(atlas_root);

    struct gpu_texture* texture = gpu_texture_create(hud->device, &(struct gpu_texture_desc) {
        .label = "hud log atlas",
        .format = gpu_format_bgra8,
        .size = make_uint2(width, height),
        .usage = gpu_texture_usage_render_target | gpu_texture_usage_sample,
    });

    struct gpu_command_buffer* command_buffer = gpu_command_buffer_create(hud->device, "hud log atlas");
    ui_render(atlas_root, command_buffer, texture, (struct ui_render_desc) { .premultiplied = true });
    gpu_command_buffer_commit(command_buffer);

    ui_release(atlas_root);

    hud->log.atlas.graphic = ui_graphic_create_texture(texture);
    hud->log.atlas.width = width;
    hud->log.atlas.height = height;
    hud->log.atlas.char_width = char_width;
    hud->log.atlas.char_height = char_height;
    hud->log.atlas.ready = true;
}

struct hud_log_glyphs_ctx {
    const struct ui_custom_on_render* on_render;
    struct hud* hud;
    float3x2 transform;
    int32_t top_px;
};

static void hud_log_on_glyph(int32_t row, uint32_t col, uint32_t cp, void* user) {
    struct hud_log_glyphs_ctx* ctx = user;
    struct hud* hud = ctx->hud;

    if (cp >= 256) {
        return;
    }

    int32_t char_width = (int32_t)hud->log.atlas.char_width;
    int32_t char_height = (int32_t)hud->log.atlas.char_height;

    float x0 = (float)((int32_t)col * char_width);
    float y0 = (float)(row * char_height - ctx->top_px);
    float x1 = x0 + (float)char_width;
    float y1 = y0 + (float)char_height;

    float scale_u = 1.0f / (float)hud->log.atlas.width;
    float scale_v = 1.0f / (float)hud->log.atlas.height;
    float u0 = (float)((cp & 0x1f) * (uint32_t)char_width) * scale_u;
    float v0 = (float)((cp >> 5) * (uint32_t)char_height) * scale_v;
    float u1 = u0 + (float)char_width * scale_u;
    float v1 = v0 + (float)char_height * scale_v;

    struct ui_custom_vertex_graphic* vertex;
    ctx->on_render->draw_graphic(ctx->transform, hud->log.atlas.graphic, hud_log_text_color, 6, &vertex);

    vertex[0].pos_px = make_float2(x0, y0);
    vertex[0].uv = make_float2(u0, v0);
    vertex[1].pos_px = make_float2(x1, y0);
    vertex[1].uv = make_float2(u1, v0);
    vertex[2].pos_px = make_float2(x0, y1);
    vertex[2].uv = make_float2(u0, v1);
    vertex[3].pos_px = make_float2(x1, y1);
    vertex[3].uv = make_float2(u1, v1);
    vertex[4].pos_px = make_float2(x0, y1);
    vertex[4].uv = make_float2(u0, v1);
    vertex[5].pos_px = make_float2(x1, y0);
    vertex[5].uv = make_float2(u1, v0);
}

static void hud_log_on_render(const struct ui_custom_on_render* on_render) {
    struct hud_log_on_render_args* args = on_render->user;
    struct hud* hud = args->hud;

    if (hud->log.atlas.ready == false) {
        return;
    }

    if (hud->log.pos_x_current + (double)hud->log.size_current.x <= 0.0) {
        return;
    }

    int32_t char_width = (int32_t)hud->log.atlas.char_width;
    int32_t char_height = (int32_t)hud->log.atlas.char_height;
    int32_t view_width = on_render->size_px.x;
    int32_t view_height = on_render->size_px.y;

    if (char_width <= 0 || char_height <= 0 || view_width <= 0 || view_height <= 0) {
        return;
    }

    uint32_t view_width_chars = (uint32_t)(view_width / char_width);
    if (view_width_chars == 0) {
        return;
    }

    float3x2 transform = float3x2_identity;
    transform.columns[2] = to_float2_from_int2(on_render->origin_px);

    int32_t top_px = args->offset_px - view_height;

    struct hud_log_glyphs_ctx ctx = {
        .on_render = on_render,
        .hud = hud,
        .transform = transform,
        .top_px = top_px,
    };

    log_view_glyphs(view_width_chars, hud_log_div_floor(top_px, char_height), hud_log_div_ceil(args->offset_px, char_height), hud_log_on_glyph, &ctx);
}

static void hud_log_on_resize_pan(struct ui_pan_gesture gesture, void* user) {
    struct hud* hud = user;
    enum ui_gesture_phase phase = ui_pan_gesture_get_phase(gesture);

    if (phase == ui_gesture_phase_began) {
        hud->log.size_start = hud->log.size_current;
        hud->log.size_start_translation = ui_pan_gesture_get_translation(gesture, false);
    } else if (phase == ui_gesture_phase_changed) {
        double2 translation = ui_pan_gesture_get_translation(gesture, false) - hud->log.size_start_translation;
        hud->log.size_current.x = clamp_double(hud->log.size_start.x + translation.x, hud_log_width_min, hud_log_width_max);
        hud->log.size_current.y = clamp_double(hud->log.size_start.y - translation.y, hud_log_height_min, hud_log_height_max);
    } else if (phase == ui_gesture_phase_ended || phase == ui_gesture_phase_canceled) {
        log_set_pref(log_pref_width, (uint16_t)hud->log.size_current.x);
        log_set_pref(log_pref_height, (uint16_t)hud->log.size_current.y);
    }
}

static void hud_log_on_slide_pan(struct ui_pan_gesture gesture, void* user) {
    struct hud* hud = user;
    enum ui_gesture_phase phase = ui_pan_gesture_get_phase(gesture);

    if (phase == ui_gesture_phase_began) {
        hud->log.pos_x_start = hud->log.pos_x_current;
        hud->log.pos_x_start_translation = ui_pan_gesture_get_translation(gesture, false).x;
        hud->log.is_sliding = true;
    } else if (phase == ui_gesture_phase_changed) {
        double translation_x = ui_pan_gesture_get_translation(gesture, false).x - hud->log.pos_x_start_translation;

        hud->log.pos_x_current = clamp_double(hud->log.pos_x_start + translation_x, -hud->log.size_current.x - hud_log_left_hidden_padding, hud_log_left_padding);
    } else if (phase == ui_gesture_phase_ended || phase == ui_gesture_phase_canceled) {
        hud->log.is_sliding = false;

        double2 velocity = ui_pan_gesture_get_velocity(gesture);

        if (velocity.x > 1500.0) {
            hud->log.is_visible = true;
        } else if (velocity.x < -1500.0) {
            hud->log.is_visible = false;
        } else {
            hud->log.is_visible = (hud->log.pos_x_current + hud->log.size_current.x * 0.5) > 0.0;
        }

        log_set_pref(log_pref_is_visible, hud->log.is_visible);
    }
}

static void hud_log_on_scroll_pan(struct ui_pan_gesture gesture, void* user) {
    struct hud* hud = user;

    if (ui_scroll_on_pan(&hud->log.scroll, gesture, ui_pan_direction_y)) {
        ui_pan_gesture_claim(gesture);
    }
}

static void hud_log_on_wheel(struct app_scroll_event event, void* user) {
    struct hud* hud = user;

    ui_scroll_on_wheel(&hud->log.scroll, event.delta.y);
}

static void hud_log_on_mode_tap(struct ui_tap_gesture gesture, void* user) {
    enum log_mode mode = (enum log_mode)log_get_pref(log_pref_mode);

    switch (mode) {
    case log_mode_tool: mode = log_mode_both; break;
    case log_mode_both: mode = log_mode_term; break;
    case log_mode_term: mode = log_mode_tool; break;
    }

    log_set_mode(mode);
}

static void hud_log_on_clear_tap(struct ui_tap_gesture gesture, void* user) {
    struct hud* hud = user;

    log_resume();
    log_clear();
    ui_scroll_init(&hud->log.scroll);
}

static void hud_log_on_pause_tap(struct ui_tap_gesture gesture, void* user) {
    struct hud* hud = user;

    if (log_is_paused()) {
        log_resume();
        ui_scroll_init(&hud->log.scroll);
    } else {
        log_pause();
    }
}

static void hud_log_toolbar_button(struct hud* hud, struct ui_node parent, const char* label, void (*on_tap)(struct ui_tap_gesture gesture, void* user)) {
    double char_width_pt = (double)hud->log.atlas.char_width / hud->log.atlas.to_px_from_pt;

    struct ui_node button = ui_frame(&(struct ui_frame) {
        .parent = parent,
        .size.width = ui_size_axis_fit(),
        .size.height = ui_size_axis_fill(),
        .layout = ui_layout_stack_y(ui_dist_center, ui_align_center, 0, ui_pad_xy(char_width_pt * 2.0, 0)),
    });

    struct ui_tap_gesture tap = ui_tap(&(struct ui_tap) {
        .node = button,
        .on_change = on_tap,
        .user = hud,
    });

    struct ui_color color = ui_tap_gesture_has_touch(tap) ? hud_log_text_color_pressed : hud_log_text_color;

    ui_text(&(struct ui_text) {
        .parent = button,
        .str = hud_str(hud->font, label, color, hud_log_pt_size),
    });
}

void hud_log_on_update(struct hud* hud, struct ui_node main, const struct hud_theme* theme, double delta_s) {
    log_dump();

    if (hud->log.is_init == false) {
        hud->log.is_init = true;
        ui_scroll_init(&hud->log.scroll);

        uint16_t pref_width = log_get_pref(log_pref_width);
        uint16_t pref_height = log_get_pref(log_pref_height);

        hud->log.size_current = make_double2(pref_width ? pref_width : 720.0, pref_height ? pref_height : 480.0);
        hud->log.is_visible = log_get_pref(log_pref_is_visible) != 0;
        hud->log.pos_x_current = hud->log.is_visible ? hud_log_left_padding : -hud->log.size_current.x - hud_log_left_hidden_padding;
    }

    hud_log_atlas_update(hud);

    if (hud->log.is_sliding == false) {
        double target_x = hud->log.is_visible ? hud_log_left_padding : -hud->log.size_current.x - hud_log_left_hidden_padding;
        double f = 1.0 - exp(-8.0 * delta_s);

        hud->log.pos_x_current = ui_approach_px(hud->log.pos_x_current, target_x, f);
    }

    struct ui_node window = ui_frame(&(struct ui_frame) {
        .parent = main,
        .pos.x = ui_pos_axis_rel(ui_pos_anchor_start, ui_pos_anchor_start, hud->log.pos_x_current),
        .pos.y = ui_pos_axis_rel(ui_pos_anchor_end, ui_pos_anchor_end, -hud_log_bottom_padding),
        .size = ui_size_fix(hud->log.size_current.x, hud->log.size_current.y),
        .layout = ui_layout_stack_y(ui_dist_start, ui_align_start, 0, 0),
        .fill = ui_fill_solid(ui_black),
    });

    ui_on_scroll(&(struct ui_on_scroll) {
        .node = window,
        .func = hud_log_on_wheel,
        .user = hud,
    });

    struct ui_node toolbar = ui_frame(&(struct ui_frame) {
        .parent = window,
        .size.width = ui_size_axis_fill(),
        .size.height = ui_size_axis_fix(hud_log_toolbar_height),
        .layout = ui_layout_stack_x(ui_dist_start, ui_align_center, 0, 0),
    });

    const char* mode_label;
    switch ((enum log_mode)log_get_pref(log_pref_mode)) {
    case log_mode_term: mode_label = "TERM"; break;
    case log_mode_both: mode_label = "BOTH"; break;
    default: mode_label = "TOOL"; break;
    }

    hud_log_toolbar_button(hud, toolbar, mode_label, hud_log_on_mode_tap);
    hud_log_toolbar_button(hud, toolbar, "CLEAR", hud_log_on_clear_tap);
    hud_log_toolbar_button(hud, toolbar, log_is_paused() ? "CONTINUE" : "PAUSE", hud_log_on_pause_tap);

    struct ui_node output = ui_frame(&(struct ui_frame) {
        .parent = window,
        .size = ui_size_fill(),
        .clip = true,
    });

    ui_custom(&(struct ui_custom) {
        .parent = output,
        .size = ui_size(ui_size_axis_width(), ui_size_axis_height()),
        .desc.on_render = hud_log_on_render,
        .desc.user = &hud->log.on_render_args,
    });

    ui_pan(&(struct ui_pan) {
        .node = output,
        .on_change = hud_log_on_scroll_pan,
        .user = hud,
        .direction = ui_pan_direction_y,
        .holds_touches = true,
    });

    struct ui_node slide_left_zone = ui_frame(&(struct ui_frame) {
        .parent = main,
        .pos.y = ui_pos_axis_rel(ui_pos_anchor_end, ui_pos_anchor_end, -hud_log_bottom_padding),
        .size = ui_size_fix(24.0f, hud->log.size_current.y),
    });

    ui_pan(&(struct ui_pan) {
        .node = slide_left_zone,
        .on_change = hud_log_on_slide_pan,
        .user = hud,
        .direction = ui_pan_direction_x,
    });

    struct ui_node slide_zone = ui_frame(&(struct ui_frame) {
        .parent = window,
        .pos.x = ui_pos_axis_rel(ui_pos_anchor_center, ui_pos_anchor_end, 0),
        .pos.y = ui_pos_axis_start(),
        .size = ui_size_fix(64, hud->log.size_current.y),
    });

    ui_pan(&(struct ui_pan) {
        .node = slide_zone,
        .on_change = hud_log_on_slide_pan,
        .user = hud,
        .direction = ui_pan_direction_x,
    });

    struct ui_node resize_zone = ui_frame(&(struct ui_frame) {
        .parent = window,
        .pos.x = ui_pos_axis_rel(ui_pos_anchor_center, ui_pos_anchor_end, 0),
        .pos.y = ui_pos_axis_rel(ui_pos_anchor_center, ui_pos_anchor_start, 0),
        .size = ui_size_fix(64, 64),
    });

    ui_pan(&(struct ui_pan) {
        .node = resize_zone,
        .on_change = hud_log_on_resize_pan,
        .user = hud,
    });

    bool shown = hud->log.pos_x_current + hud->log.size_current.x > 0.0;

    if (shown) {
        double to_px_from_pt = hud->log.atlas.to_px_from_pt;
        double char_height_pt = (double)hud->log.atlas.char_height / to_px_from_pt;
        double view_height_pt = max_double(hud->log.size_current.y - hud_log_toolbar_height, 0.0);
        uint32_t view_width_chars = (uint32_t)max_double(floor(hud->log.size_current.x * to_px_from_pt / (double)hud->log.atlas.char_width), 0.0);

        uint32_t row_count = view_width_chars > 0 ? log_view_row_count(view_width_chars) : 0;
        double content_start = min_double(-(double)row_count * char_height_pt, -view_height_pt);

        double offset = hud->log.scroll.offset;
        ui_scroll_on_update(&hud->log.scroll, offset - view_height_pt, offset, content_start, 0.0, delta_s);
    }

    hud->log.on_render_args = (struct hud_log_on_render_args) {
        .hud = hud,
        .offset_px = (int32_t)lround(hud->log.scroll.offset * hud->log.atlas.to_px_from_pt),
    };
}
