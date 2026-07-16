#include "demo.h"
#include "hud.h"
#include <string.h>

#define text_panel_width_max 560.0
#define text_notes_height_min 140.0
#define text_notes_height_max 420.0
#define text_field_chrome 130.0

struct text_page {
    struct cr_mem* mem;
    struct ui_text_input* title;
    struct ui_text_input* notes;
};

static const char text_notes_initial[] = "Glass refracts the scene behind it.\nLists animate size, position and order.\nEverything in this demo is drawn by the ui library.";

static void text_reset(struct text_page* page) {
    ui_text_input_set_text(page->title, "", 0);
    ui_text_input_set_text(page->notes, text_notes_initial, sizeof(text_notes_initial) - 1);
}

static void* text_create(struct cr_mem* mem) {
    struct text_page* page = cr_malloc(mem, sizeof(*page), 0, cr_loc);

    memset(page, 0, sizeof(*page));
    page->mem = mem;
    page->title = ui_text_input_create(mem);
    page->notes = ui_text_input_create(mem);
    text_reset(page);

    return page;
}

static void text_destroy(void* state) {
    struct text_page* page = state;

    ui_text_input_destroy(page->notes);
    ui_text_input_destroy(page->title);
    cr_free(page->mem, page, cr_loc);
}

static void text_field(const struct demo_frame* frame, struct ui_node column, const char* name, struct ui_text_input* input, const char* placeholder, struct ui_text_input_layout layout) {
    const struct demo_theme* theme = frame->theme;

    struct ui_node group = ui_frame(&(struct ui_frame) {
        .parent = column,
        .layout = ui_layout_stack_y(ui_dist_start, ui_align_start, 8, 0),
    });

    ui_text(&(struct ui_text) {
        .parent = group,
        .str = ui_str_weight(name, theme->fg_secondary, 13, 510),
    });

    struct ui_node field = ui_frame(&(struct ui_frame) {
        .parent = group,
        .layout = ui_layout_stack_y(ui_dist_start, ui_align_start, 0, 0),
        .corners = ui_corners(14),
        .fill = ui_fill_solid(theme->panel),
    });

    struct demo_field_style style = {
        .text = theme->fg,
        .placeholder = theme->fg_tertiary,
        .accent = theme->accent,
        .selection_alpha = 0.35f,
    };

    demo_field(input, frame->root, field, layout, placeholder, &style, frame->delta_s);
}

static void text_frame(void* state, const struct demo_frame* frame) {
    struct text_page* page = state;

    double pt_size = hud_slider("Pt size", 12, 28, 15, hud_slider_display_mode_float);
    double weight = hud_slider("Weight", 300, 800, 400, hud_slider_display_mode_float);

    if (hud_button("Text", "Reset")) {
        text_reset(page);
    }

    double top = demo_title(frame, "Text Input", "click to focus and edit, drag to select");

    double2 size = frame->size;
    double panel_w = max_double(min_double(size.x - 2.0 * demo_margin, text_panel_width_max), 160.0);
    double notes_h = clamp_double(size.y - top - demo_margin - text_field_chrome, text_notes_height_min, text_notes_height_max);
    double4 padding = make_double4(14, 14, 12, 12);

    struct ui_node column = ui_frame(&(struct ui_frame) {
        .parent = frame->root,
        .pos = ui_pos_xy(demo_margin, top),
        .layout = ui_layout_stack_y(ui_dist_start, ui_align_start, 20, 0),
    });

    text_field(frame, column, "Title", page->title, "Type a title", (struct ui_text_input_layout) {
        .width = panel_w,
        .pt_size = pt_size,
        .weight = weight,
        .padding = padding,
    });

    text_field(frame, column, "Notes", page->notes, "Write something", (struct ui_text_input_layout) {
        .width = panel_w,
        .min_height = notes_h,
        .max_height = notes_h,
        .pt_size = pt_size,
        .weight = weight,
        .padding = padding,
        .multiline = true,
    });
}

const struct demo_page demo_page_text = {
    .name = "Text",
    .create = text_create,
    .destroy = text_destroy,
    .frame = text_frame,
};
