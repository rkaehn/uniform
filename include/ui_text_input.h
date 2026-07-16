#pragma once

#include "cr.h"
#include "ui.h"

struct ui_text_input;

struct ui_text_edit_rect {
    double x;
    double y;
    double width;
    double height;
};

struct ui_text_input_layout {
    double width;
    double min_height;
    double max_height;
    double pt_size;
    double weight;
    struct ui_font* font;
    double4 padding;
    bool multiline;
};

struct ui_text_input_on_update_result {
    struct ui_node main;
    struct ui_node content;
    const char* text;
    uint32_t text_size;
    bool is_focused;
    bool caret_visible;
    bool handles_visible;
    struct ui_text_edit_rect caret;
    struct ui_text_edit_rect handle_start;
    struct ui_text_edit_rect handle_end;
    const struct ui_text_edit_rect* selection_rects;
    uint32_t selection_rect_count;
    const struct ui_text_edit_rect* marked_rects;
    uint32_t marked_rect_count;
    double height;
    double content_width;
    double content_height;
    double scroll_offset;
    double scroll_offset_x;
};

struct ui_text_input* ui_text_input_create(struct cr_mem* mem);
void ui_text_input_destroy(struct ui_text_input* input);

void ui_text_input_set_text(struct ui_text_input* input, const char* utf8, uint64_t size);
void ui_text_input_get_text(struct ui_text_input* input, const char** out_utf8, uint64_t* out_size);

bool ui_text_input_is_focused(struct ui_text_input* input);
void ui_text_input_set_focus(struct ui_text_input* input, bool focused);

struct ui_text_input_on_update_result ui_text_input_on_update(struct ui_text_input* input, struct ui_node root, struct ui_node container, struct ui_text_input_layout layout, double delta_s);
