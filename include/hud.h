#pragma once

#include "cr.h"
#include "ui.h"

struct hud;
struct gpu_command_buffer;
struct gpu_device;

struct hud_theme {
    struct ui_color label_primary;
    struct ui_color background_primary;
    struct ui_color background_secondary;
    struct ui_color highlight;
    struct ui_color blue;
    struct ui_font* font;
};

cr_hdef struct hud_theme hud_theme(bool is_dark) {
    struct hud_theme theme;

    theme.font = NULL;

    if (is_dark) {
        theme.label_primary = ui_gray90;
        theme.background_primary = ui_gray(0.08);
        theme.background_secondary = ui_gray(0.16);
        theme.highlight = ui_gray60;
        theme.blue = ui_hex(0x0076ff);
    } else {
        theme.label_primary = ui_gray30;
        theme.background_primary = ui_gray(0.96);
        theme.background_secondary = ui_gray(0.88);
        theme.highlight = ui_white;
        theme.blue = ui_hex(0x0a8cff);
    }

    return theme;
}

enum cr_closed hud_slider_display_mode {
    hud_slider_display_mode_percent,
    hud_slider_display_mode_float,
};

void hud_set(struct hud* hud);
struct hud* hud_get(void);

struct hud* hud_create(struct cr_mem* mem, struct gpu_device* device, struct ui_font* font);
void hud_destroy(struct hud* hud);
void hud_set_enabled(bool enabled);

bool hud_toggle(const char* label, bool initial_value);
uint32_t hud_select(const char* label, uint32_t option_count, uint32_t initial_option);
uint32_t hud_select_labels(const char* label, uint32_t option_count, uint32_t initial_option, const char* const* option_labels);
bool hud_button(const char* label, const char* text);
uint32_t hud_stepper(const char* label, uint32_t count, uint32_t value);
void hud_label(const char* label, const char* text);
double hud_slider(const char* label, double min_value, double max_value, double initial_value, enum hud_slider_display_mode display_mode);
const char* hud_text_input(const char* label, const char* initial_text);
void hud_frame_begin(double timestamp);
void hud_frame_profile(struct gpu_command_buffer* command_buffer);
void hud_frame_label(const char* text);
void hud_frame_end(void);

void hud_on_update(struct ui_node root, bool is_dark, double delta_s);
