#pragma once

#include "cr.h"
#include "ui.h"
#include "ui_text_input.h"

#define demo_margin 32.0
#define demo_corner 16.0

struct demo_theme {
    bool is_dark;
    struct ui_color bg;
    struct ui_color fg;
    struct ui_color fg_secondary;
    struct ui_color fg_tertiary;
    struct ui_color hairline;
    struct ui_color panel;
    struct ui_color accent;
    enum ui_glass_appearance glass;
};

struct demo_frame {
    struct ui_node root;
    double2 size;
    double keyboard_inset;
    double delta_s;
    bool appeared;
    const struct demo_theme* theme;
    struct ui_font* font_mono;
};

struct demo_page {
    const char* name;
    void* (*create)(struct cr_mem* mem);
    void (*destroy)(void* state);
    void (*frame)(void* state, const struct demo_frame* frame);
};

struct demo_field_style {
    struct ui_color text;
    struct ui_color placeholder;
    struct ui_color accent;
    float selection_alpha;
};

extern const uint8_t demo_font_data[];
extern const uint32_t demo_font_data_size;
extern const uint8_t demo_font_mono_data[];
extern const uint32_t demo_font_mono_data_size;

extern const struct ui_color demo_palette[8];

extern const struct demo_page demo_page_glass;
extern const struct demo_page demo_page_canvas;
extern const struct demo_page demo_page_collection;
extern const struct demo_page demo_page_tree;
extern const struct demo_page demo_page_circle;
extern const struct demo_page demo_page_text;
extern const struct demo_page demo_page_write;

struct demo_theme demo_theme(bool is_dark);
struct ui_str demo_str(struct ui_font* font, const char* raw, struct ui_color color, double pt_size, double weight);

double2 demo_measure(struct ui_font* font, const char* raw, double pt_size, double weight, double width);

double demo_title(const struct demo_frame* frame, const char* heading, const char* hint);
struct ui_node demo_host(struct ui_node parent, double2 pos, double2 size, const struct demo_theme* theme);

struct ui_text_input_on_update_result demo_field(struct ui_text_input* input, struct ui_node root, struct ui_node container, struct ui_text_input_layout layout, const char* placeholder, const struct demo_field_style* style, double delta_s);
