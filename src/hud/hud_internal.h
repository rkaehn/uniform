#pragma once

#include "cr.h"
#include "ui.h"
#include "ui_physics.h"
#include "hud.h"
#include "gpu.h"

#define hud_control_max 256

enum hud_control_type {
    hud_control_type_slider,
    hud_control_type_toggle,
    hud_control_type_select,
    hud_control_type_button,
    hud_control_type_stepper,
    hud_control_type_label,
    hud_control_type_text_input,
};

struct hud_slider {
    double t;
    double min_value;
    double max_value;
    double rail_width;
    struct {
        double pos_px_start;
        double rail_width_px_start;
        double t_start;
    } pan;
    enum hud_slider_display_mode display_mode;
};

struct hud_select {
    char text[64];
    uint32_t option_count;
    uint32_t selected_option;
};

struct hud_button {
    char text[64];
    bool tapped;
};

struct hud_stepper {
    int32_t delta;
    uint32_t value;
    uint32_t count;
};

struct hud_control {
    enum hud_control_type type;
    bool declared;
    char label[32];
    union {
        struct hud_slider slider;
        struct hud_select select;
        struct hud_button button;
        struct hud_stepper stepper;
        struct ui_text_input* text_input;
        char text[64];
        bool is_on;
    };
};

struct hud_frame_slot {
    struct gpu_profiler* profiler;
    uint64_t seq;
    bool armed;
    bool ended;
    double frame_s;
    double cpu_s;
};

struct hud_log_on_render_args {
    struct hud* hud;
    int32_t offset_px;
};

cr_hdef __attribute__((always_inline))
struct ui_str hud_str(struct ui_font* font, const char* raw, struct ui_color color, double pt_size) {
    return (struct ui_str) {
        .raw = raw,
        .attributes = {
            { .start = 0, .end = uint32_max, .type = ui_str_attribute_type_color, .color = color },
            { .start = 0, .end = uint32_max, .type = ui_str_attribute_type_pt_size, .pt_size = pt_size },
            { .start = 0, .end = uint32_max, .type = ui_str_attribute_type_font, .font = font },
        },
    };
}

struct hud {
    struct cr_mem* mem;
    struct gpu_device* device;
    struct ui_font* font;
    bool enabled;
    struct {
        struct hud_control slots[hud_control_max];
        struct hud_control* order[hud_control_max];
        uint32_t count;
    } controls;
    struct {
        bool active;
        uint64_t next_seq;
        uint64_t shown_seq;
        double prev_timestamp;
        double cpu_begin_s;
        struct hud_frame_slot slots[4];
        struct hud_frame_slot* slot;
        double frame_s;
        double cpu_s;
        double gpu_s;
        uint32_t pass_count;
        double passes[gpu_profile_max_passes];
        char pass_names[gpu_profile_max_passes][gpu_pass_name_max];
        char label[40];
    } frame_times;
    struct {
        struct {
            struct ui_graphic* graphic;
            uint32_t width;
            uint32_t height;
            uint32_t char_width;
            uint32_t char_height;
            double to_px_from_pt;
            bool ready;
        } atlas;
        struct ui_scroll scroll;
        struct hud_log_on_render_args on_render_args;
        double2 size_start;
        double2 size_current;
        double2 size_start_translation;
        double pos_x_start;
        double pos_x_current;
        double pos_x_start_translation;
        bool is_visible;
        bool is_sliding;
        bool is_init;
    } log;
};

struct hud* hud_ctx(void);
void hud_label_copy(char* dst, size_t dst_size, const char* label);

void hud_controls_deinit(struct hud* hud);
void hud_controls_on_update(struct hud* hud, struct ui_node root, struct ui_node stack, double width, const struct hud_theme* theme, double delta_s);
void hud_controls_sweep(struct hud* hud);

void hud_frame_times_init(struct hud* hud);
void hud_frame_times_deinit(struct hud* hud);
void hud_frame_times_on_update(struct hud* hud, struct ui_node parent, const struct hud_theme* theme);

void hud_log_deinit(struct hud* hud);
void hud_log_on_update(struct hud* hud, struct ui_node main, const struct hud_theme* theme, double delta_s);
