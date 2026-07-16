#include "demo.h"
#include <math.h>

cr_warning_push
cr_warning_ignore_everything
const uint8_t demo_font_data[] = {
#embed "../assets/Inter.ttf"
};
const uint8_t demo_font_mono_data[] = {
#embed "../assets/JetBrainsMono.ttf"
};
cr_warning_pop

const uint32_t demo_font_data_size = sizeof(demo_font_data);
const uint32_t demo_font_mono_data_size = sizeof(demo_font_mono_data);

const struct ui_color demo_palette[8] = {
    ui_hex(0xff453a),
    ui_hex(0xff9f0a),
    ui_hex(0xffd60a),
    ui_hex(0x32d74b),
    ui_hex(0x64d2ff),
    ui_hex(0x0a84ff),
    ui_hex(0xbf5af2),
    ui_hex(0xff375f),
};

struct demo_theme demo_theme(bool is_dark) {
    float gray = is_dark ? 1.0f : 0.0f;

    return (struct demo_theme) {
        .is_dark = is_dark,
        .bg = is_dark ? ui_gray20 : ui_white,
        .fg = is_dark ? ui_white : ui_black,
        .fg_secondary = ui_gray_a(gray, 0.55f),
        .fg_tertiary = ui_gray_a(gray, 0.35f),
        .hairline = ui_gray_a(gray, 0.15f),
        .panel = ui_gray_a(gray, 0.06f),
        .accent = demo_palette[5],
        .glass = is_dark ? ui_glass_appearance_dark : ui_glass_appearance_light,
    };
}

struct ui_str demo_str(struct ui_font* font, const char* raw, struct ui_color color, double pt_size, double weight) {
    struct ui_str str = ui_str_weight(raw, color, pt_size, weight);

    if (font) {
        str.attributes[3] = (struct ui_str_attribute) { .start = 0, .end = uint32_max, .type = ui_str_attribute_type_font, .font = font };
    }

    return str;
}

double2 demo_measure(struct ui_font* font, const char* raw, double pt_size, double weight, double width) {
    struct ui_node node = ui_text(&(struct ui_text) {
        .size.width = width > 0.0 ? ui_size_axis_fix(width) : ui_size_axis_fit(),
        .str = demo_str(font, raw, ui_black, pt_size, weight),
    });

    ui_layout(node);
    double2 size = ui_get_size(node);
    ui_release(node);

    return size;
}

double demo_title(const struct demo_frame* frame, const char* heading, const char* hint) {
    const struct demo_theme* theme = frame->theme;
    double top = 24.0;
    double width = max_double(frame->size.x - 2.0 * demo_margin, 120.0);

    struct ui_node column = ui_frame(&(struct ui_frame) {
        .parent = frame->root,
        .pos = ui_pos_xy(demo_margin, top),
        .size.width = ui_size_axis_fix(width),
        .layout = ui_layout_stack_y(ui_dist_start, ui_align_start, 4, 0),
    });

    ui_text(&(struct ui_text) {
        .parent = column,
        .size.width = ui_size_axis_fix(width),
        .str = ui_str_weight(heading, theme->fg, 34, 700),
    });

    ui_text(&(struct ui_text) {
        .parent = column,
        .size.width = ui_size_axis_fix(width),
        .str = ui_str_weight(hint, theme->fg_secondary, 15, 400),
    });

    double height = demo_measure(NULL, heading, 34, 700, width).y + 4.0 + demo_measure(NULL, hint, 15, 400, width).y;

    return top + ceil(height) + 20.0;
}

struct ui_node demo_host(struct ui_node parent, double2 pos, double2 size, const struct demo_theme* theme) {
    return ui_frame(&(struct ui_frame) {
        .parent = parent,
        .pos = ui_pos_xy(pos.x, pos.y),
        .size = ui_size_fix(size.x, size.y),
        .corners = ui_corners(demo_corner),
        .fill = ui_fill_solid(theme->panel),
        .clip = true,
    });
}

static void demo_field_handle(struct ui_node content, const struct ui_text_edit_rect* rect, bool is_start, struct ui_color accent) {
    double knob = 5.0;
    double knob_y = is_start ? rect->y - knob : rect->y + rect->height + knob;

    ui_frame(&(struct ui_frame) {
        .parent = content,
        .pos = ui_pos_xy(rect->x - 1.0, is_start ? knob_y : rect->y),
        .size = ui_size_fix(2.0, rect->height + knob),
        .fill = ui_fill_solid(accent),
    });

    ui_ellipse(&(struct ui_ellipse) {
        .parent = content,
        .pos = ui_pos_xy(rect->x - knob, knob_y - knob),
        .size = ui_size_fix(knob * 2.0, knob * 2.0),
        .fill = ui_fill_solid(accent),
    });
}

struct ui_text_input_on_update_result demo_field(struct ui_text_input* input, struct ui_node root, struct ui_node container, struct ui_text_input_layout layout, const char* placeholder, const struct demo_field_style* style, double delta_s) {
    struct ui_text_input_on_update_result result = ui_text_input_on_update(input, root, container, layout, delta_s);

    struct ui_color selection = style->accent;
    selection.value.w = style->selection_alpha;

    for (uint32_t i = 0; i < result.selection_rect_count; i++) {
        const struct ui_text_edit_rect* rect = &result.selection_rects[i];

        ui_frame(&(struct ui_frame) {
            .parent = result.content,
            .pos = ui_pos_xy(rect->x, rect->y),
            .size = ui_size_fix(rect->width, rect->height),
            .fill = ui_fill_solid(selection),
        });
    }

    for (uint32_t i = 0; i < result.marked_rect_count; i++) {
        const struct ui_text_edit_rect* rect = &result.marked_rects[i];

        ui_frame(&(struct ui_frame) {
            .parent = result.content,
            .pos = ui_pos_xy(rect->x, rect->y + rect->height - 3.0),
            .size = ui_size_fix(rect->width, 2.0),
            .corners = ui_corners(1.0),
            .fill = ui_fill_solid(style->accent),
        });
    }

    bool has_text = result.text_size > 0;

    ui_text(&(struct ui_text) {
        .parent = result.content,
        .size.width = ui_size_axis_fix(result.content_width),
        .str = demo_str(layout.font, has_text ? result.text : placeholder, has_text ? style->text : style->placeholder, layout.pt_size, layout.weight),
    });

    if (result.caret_visible) {
        ui_frame(&(struct ui_frame) {
            .parent = result.content,
            .pos = ui_pos_xy(result.caret.x - 1.0, result.caret.y + 1.0),
            .size = ui_size_fix(2.0, result.caret.height - 2.0),
            .corners = ui_corners(1.0),
            .fill = ui_fill_solid(style->accent),
        });
    }

    if (result.handles_visible) {
        demo_field_handle(result.content, &result.handle_start, true, style->accent);
        demo_field_handle(result.content, &result.handle_end, false, style->accent);
    }

    return result;
}
