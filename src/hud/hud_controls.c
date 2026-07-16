#include "hud_internal.h"
#include "ui_gestures.h"
#include "ui_text_input.h"
#include <stdio.h>

static void hud_control_deinit(struct hud_control* control) {
    if (control->label[0] == 0) {
        return;
    }
    if (control->type == hud_control_type_text_input) {
        ui_text_input_destroy(control->text_input);
    }
    control->label[0] = 0;
}

static struct hud_control* hud_control_declare(struct hud* hud, enum hud_control_type type, const char* label, bool* out_is_init) {
    cr_check(strlen(label) > 0, "a hud control needs a label");
    cr_check(hud->controls.count < cr_count(hud->controls.order), "the hud holds at most %u controls", (uint32_t)cr_count(hud->controls.order));

    struct hud_control* control = hud->controls.order[hud->controls.count];
    bool is_init = false;

    if (control == NULL || control->type != type || strcmp(control->label, label) != 0) {
        struct hud_control* unused = NULL;

        control = NULL;
        for (uint32_t i = 0; i < cr_count(hud->controls.slots) && control == NULL; i++) {
            struct hud_control* slot = &hud->controls.slots[i];

            if (slot->label[0] == 0) {
                unused = unused ? unused : slot;
            } else if (slot->type == type && strcmp(slot->label, label) == 0) {
                control = slot;
            }
        }

        if (control == NULL) {
            cr_check(unused != NULL, "the hud holds at most %u controls", (uint32_t)cr_count(hud->controls.slots));
            control = unused;
            memset(control, 0, sizeof(*control));
            control->type = type;
            hud_label_copy(control->label, sizeof(control->label), label);
            is_init = true;
        }
    }

    cr_check(control->declared == false, "the hud control %s was declared twice in one frame", label);
    control->declared = true;
    hud->controls.order[hud->controls.count++] = control;

    if (out_is_init) {
        *out_is_init = is_init;
    }

    return control;
}

bool hud_toggle(const char* label, bool initial_value) {
    struct hud* hud = hud_ctx();

    bool is_init;
    struct hud_control* control = hud_control_declare(hud, hud_control_type_toggle, label, &is_init);

    if (is_init) {
        control->is_on = initial_value;
    }

    return control->is_on;
}

uint32_t hud_select_labels(const char* label, uint32_t option_count, uint32_t initial_option, const char* const* option_labels) {
    cr_check(option_count > 0, "a hud select needs at least one option");
    cr_check(initial_option < option_count, "the initial option %u is out of range for %u options", initial_option, option_count);

    struct hud* hud = hud_ctx();

    bool is_init;
    struct hud_control* control = hud_control_declare(hud, hud_control_type_select, label, &is_init);
    struct hud_select* select = &control->select;

    if (is_init) {
        select->selected_option = initial_option;
    }
    select->option_count = option_count;
    if (select->selected_option >= option_count) {
        select->selected_option = 0;
    }
    if (option_labels != NULL && option_labels[select->selected_option] != NULL) {
        snprintf(select->text, sizeof(select->text), "%s", option_labels[select->selected_option]);
    } else {
        snprintf(select->text, sizeof(select->text), "%u", select->selected_option + 1);
    }

    return select->selected_option;
}

uint32_t hud_select(const char* label, uint32_t option_count, uint32_t initial_option) {
    return hud_select_labels(label, option_count, initial_option, NULL);
}

bool hud_button(const char* label, const char* text) {
    struct hud* hud = hud_ctx();

    struct hud_control* control = hud_control_declare(hud, hud_control_type_button, label, NULL);

    snprintf(control->button.text, sizeof(control->button.text), "%s", text != NULL ? text : "");

    bool tapped = control->button.tapped;
    control->button.tapped = false;

    return tapped;
}

uint32_t hud_stepper(const char* label, uint32_t count, uint32_t value) {
    cr_check(count > 0, "a hud stepper needs a count above 0");

    struct hud* hud = hud_ctx();

    struct hud_control* control = hud_control_declare(hud, hud_control_type_stepper, label, NULL);
    struct hud_stepper* stepper = &control->stepper;

    value = min_uint32(value, count - 1);
    if (stepper->delta != 0) {
        int64_t next = (int64_t)value + stepper->delta;
        next = ((next % count) + count) % count;
        value = (uint32_t)next;
        stepper->delta = 0;
    }

    stepper->value = value;
    stepper->count = count;

    return value;
}

void hud_label(const char* label, const char* text) {
    struct hud* hud = hud_ctx();

    struct hud_control* control = hud_control_declare(hud, hud_control_type_label, label, NULL);

    snprintf(control->text, sizeof(control->text), "%s", text != NULL ? text : "");
}

const char* hud_text_input(const char* label, const char* initial_text) {
    struct hud* hud = hud_ctx();

    bool is_init;
    struct hud_control* control = hud_control_declare(hud, hud_control_type_text_input, label, &is_init);

    if (is_init) {
        control->text_input = ui_text_input_create(hud->mem);
        if (initial_text) {
            ui_text_input_set_text(control->text_input, initial_text, strlen(initial_text));
        }
    }

    const char* text;
    uint64_t size;
    ui_text_input_get_text(control->text_input, &text, &size);

    return text;
}

double hud_slider(const char* label, double min_value, double max_value, double initial_value, enum hud_slider_display_mode display_mode) {
    cr_check(max_value > min_value, "a hud slider needs a maximum above its minimum");

    struct hud* hud = hud_ctx();

    bool is_init;
    struct hud_control* control = hud_control_declare(hud, hud_control_type_slider, label, &is_init);
    struct hud_slider* slider = &control->slider;

    if (is_init) {
        slider->t = clamp_double((initial_value - min_value) / (max_value - min_value), 0, 1);
    }
    slider->min_value = min_value;
    slider->max_value = max_value;
    slider->display_mode = display_mode;

    return slider->min_value * (1.0 - slider->t) + slider->max_value * slider->t;
}

static void hud_slider_on_pan(struct ui_pan_gesture gesture, void* user) {
    struct hud_control* control = user;
    struct hud_slider* slider = &control->slider;

    double to_px_from_pt = ui_get_to_px_from_pt();

    double pos_x = (double)ui_pan_gesture_get_position(gesture).x * to_px_from_pt;

    if (ui_pan_gesture_get_phase(gesture) == ui_gesture_phase_began) {
        slider->pan.pos_px_start = pos_x;
        slider->pan.rail_width_px_start = slider->rail_width * to_px_from_pt;
        slider->pan.t_start = slider->t;
    }

    if (ui_pan_gesture_get_phase(gesture) == ui_gesture_phase_began || ui_pan_gesture_get_phase(gesture) == ui_gesture_phase_changed) {
        double t_delta = (pos_x - slider->pan.pos_px_start) / slider->pan.rail_width_px_start;

        slider->t = clamp_double(slider->pan.t_start + t_delta, 0, 1);
    }
}

struct hud_frame_result {
    struct ui_node frame;
    struct ui_node content_frame;
    struct ui_node label_frame;
    double content_width;
};

static struct hud_frame_result hud_frame(struct ui_node parent, double width, const char* left_text, const struct hud_theme* theme) {
    double pad = 8;
    double content_width = width - 2 * pad;

    struct ui_node frame = ui_frame(&(struct ui_frame) {
        .parent = parent,
        .key = ui_key_str(left_text),
        .size.width = ui_size_axis_fix(width),
        .layout = ui_layout_stack_y(ui_dist_start, ui_align_start, 0, 0),
    });

    struct ui_node content_frame = ui_frame(&(struct ui_frame) {
        .parent = frame,
        .size.width = ui_size_axis_fix(width),
        .layout = ui_layout_stack_y(ui_dist_start, ui_align_center, 0, ui_pad_xy(0, 4)),
    });

    struct ui_node label_frame = ui_frame(&(struct ui_frame) {
        .parent = content_frame,
        .size.width = ui_size_axis_fix(content_width),
        .layout.mode = ui_layout_mode_stack_x,
    });

    ui_text(&(struct ui_text) {
        .parent = label_frame,
        .str = hud_str(theme->font, left_text, theme->label_primary, 12),
    });

    ui_frame(&(struct ui_frame) {
        .parent = label_frame,
        .size.width = ui_size_axis_fill(),
    });

    struct hud_frame_result result = { 0 };
    result.frame = frame;
    result.content_frame = content_frame;
    result.label_frame = label_frame;
    result.content_width = content_width;

    return result;
}

static void hud_slider_on_update(struct ui_node parent, double width, struct hud_control* control, const struct hud_theme* theme) {
    struct hud_slider* slider = &control->slider;
    struct hud_frame_result frame = hud_frame(parent, width, control->label, theme);

    char text[32];
    if (slider->display_mode == hud_slider_display_mode_percent) {
        snprintf(text, sizeof(text), "%d%%", (int)round(slider->t * 100.0));
    } else {
        double value = slider->min_value * (1.0 - slider->t) + slider->max_value * slider->t;
        snprintf(text, sizeof(text), "%.2f", value);
    }

    ui_text(&(struct ui_text) {
        .parent = frame.label_frame,
        .str = hud_str(theme->font, text, theme->label_primary, 12),
    });

    double rail_height = 2;
    double rail_pad_y = 8;

    struct ui_node slider_frame = ui_frame(&(struct ui_frame) {
        .parent = frame.content_frame,
        .size = ui_size_fix(frame.content_width, 16),
    });

    ui_frame(&(struct ui_frame) {
        .parent = slider_frame,
        .pos = ui_pos_xy(0, rail_pad_y),
        .size = ui_size_fix(frame.content_width, rail_height),
        .fill = ui_fill_solid(theme->background_secondary),
    });

    ui_frame(&(struct ui_frame) {
        .parent = slider_frame,
        .pos = ui_pos_xy(0, rail_pad_y),
        .size = ui_size_fix(slider->t * frame.content_width, rail_height),
        .fill = ui_fill_solid(theme->blue),
    });

    struct ui_node knob = ui_frame(&(struct ui_frame) {
        .parent = slider_frame,
        .pos.x = ui_pos_axis_rel(ui_pos_anchor_center, ui_pos_anchor_start, slider->t * frame.content_width),
        .pos.y = ui_pos_axis_rel(ui_pos_anchor_center, ui_pos_anchor_start, rail_pad_y + 0.5 * rail_height),
        .size = ui_size_fix(16, 8),
        .fill = ui_fill_solid(theme->highlight),
    });

    struct ui_node knob_grab = ui_frame(&(struct ui_frame) {
        .parent = knob,
        .pos = ui_pos_center(),
        .size = ui_size_fix(40, 40),
    });

    ui_pan(&(struct ui_pan) {
        .node = knob_grab,
        .on_change = hud_slider_on_pan,
        .user = control,
        .direction = ui_pan_direction_x,
    });

    slider->rail_width = frame.content_width;
}

static void hud_button_frame(struct ui_node parent, double width, const char* label, const struct hud_theme* theme,
                             void (*on_tap)(struct ui_tap_gesture gesture, void* user), void* user, struct ui_color fill, const char* text, struct ui_color text_color) {
    struct hud_frame_result frame = hud_frame(parent, width, label, theme);

    ui_layout(frame.frame);

    double side = ui_get_size(frame.frame).y;
    double pad = 8;

    struct ui_node button_frame = ui_frame(&(struct ui_frame) {
        .parent = frame.frame,
        .size.width = ui_size_axis_fit(),
        .size.height = ui_size_axis_fix(side),
        .pos.x = ui_pos_axis_end(),
        .fill = ui_fill_solid(fill),
        .layout = ui_layout_stack_y(ui_dist_center, ui_align_center, 0, ui_pad_xy(pad, 0)),
    });

    ui_tap(&(struct ui_tap) {
        .node = button_frame,
        .on_change = on_tap,
        .user = user,
    });

    ui_text(&(struct ui_text) {
        .parent = button_frame,
        .str = hud_str(theme->font, text, text_color, 12),
    });

    ui_frame(&(struct ui_frame) {
        .parent = button_frame,
        .size = ui_size_fix(max_double(side - 2 * pad, 0.0), 0),
    });
}

static void hud_toggle_on_tap(struct ui_tap_gesture gesture, void* user) {
    struct hud_control* control = user;

    control->is_on = !control->is_on;
}

static void hud_toggle_on_update(struct ui_node parent, double width, struct hud_control* control, const struct hud_theme* theme) {
    hud_button_frame(parent, width, control->label, theme, hud_toggle_on_tap, control,
                     control->is_on ? theme->blue : theme->background_secondary,
                     control->is_on ? "On" : "Off",
                     control->is_on ? ui_white : theme->label_primary);
}

static void hud_select_on_tap(struct ui_tap_gesture gesture, void* user) {
    struct hud_control* control = user;
    struct hud_select* select = &control->select;

    select->selected_option = (select->selected_option + 1) % select->option_count;
}

static void hud_select_on_update(struct ui_node parent, double width, struct hud_control* control, const struct hud_theme* theme) {
    hud_button_frame(parent, width, control->label, theme, hud_select_on_tap, control, theme->background_secondary, control->select.text, theme->label_primary);
}

static void hud_button_on_tap(struct ui_tap_gesture gesture, void* user) {
    struct hud_control* control = user;

    control->button.tapped = true;
}

static void hud_button_on_update(struct ui_node parent, double width, struct hud_control* control, const struct hud_theme* theme) {
    hud_button_frame(parent, width, control->label, theme, hud_button_on_tap, control, theme->background_secondary, control->button.text, theme->label_primary);
}

static void hud_stepper_on_prev_tap(struct ui_tap_gesture gesture, void* user) {
    struct hud_control* control = user;

    control->stepper.delta -= 1;
}

static void hud_stepper_on_next_tap(struct ui_tap_gesture gesture, void* user) {
    struct hud_control* control = user;

    control->stepper.delta += 1;
}

static void hud_stepper_side_button(struct ui_node parent, double side, const char* text, void (*on_tap)(struct ui_tap_gesture gesture, void* user), void* user, const struct hud_theme* theme) {
    struct ui_node button = ui_frame(&(struct ui_frame) {
        .parent = parent,
        .size = ui_size_fix(side, side),
        .fill = ui_fill_solid(theme->background_secondary),
        .layout = ui_layout_stack_y(ui_dist_center, ui_align_center, 0, 0),
    });

    ui_tap(&(struct ui_tap) {
        .node = button,
        .on_change = on_tap,
        .user = user,
    });

    ui_text(&(struct ui_text) {
        .parent = button,
        .str = hud_str(theme->font, text, theme->label_primary, 12),
    });
}

static void hud_stepper_on_update(struct ui_node parent, double width, struct hud_control* control, const struct hud_theme* theme) {
    struct hud_frame_result frame = hud_frame(parent, width, control->label, theme);

    ui_layout(frame.frame);

    double side = ui_get_size(frame.frame).y;

    struct ui_node row = ui_frame(&(struct ui_frame) {
        .parent = frame.frame,
        .size.width = ui_size_axis_fit(),
        .size.height = ui_size_axis_fix(side),
        .pos.x = ui_pos_axis_end(),
        .layout = ui_layout_stack_x(ui_dist_start, ui_align_center, 4, 0),
    });

    hud_stepper_side_button(row, side, "<", hud_stepper_on_prev_tap, control, theme);

    char text[32];
    snprintf(text, sizeof(text), "%u/%u", control->stepper.value + 1, control->stepper.count);

    struct ui_node value_frame = ui_frame(&(struct ui_frame) {
        .parent = row,
        .size.width = ui_size_axis_fit(),
        .size.height = ui_size_axis_fix(side),
        .layout = ui_layout_stack_y(ui_dist_center, ui_align_center, 0, 0),
    });

    ui_text(&(struct ui_text) {
        .parent = value_frame,
        .str = hud_str(theme->font, text, theme->label_primary, 12),
    });

    hud_stepper_side_button(row, side, ">", hud_stepper_on_next_tap, control, theme);
}

static void hud_label_on_update(struct ui_node parent, double width, const struct hud_control* control, const struct hud_theme* theme) {
    struct hud_frame_result frame = hud_frame(parent, width, control->label, theme);

    ui_text(&(struct ui_text) {
        .parent = frame.label_frame,
        .str = hud_str(theme->font, control->text, theme->label_primary, 12),
    });
}

static void hud_text_input_on_update(struct ui_node root, struct ui_node parent, double width, struct hud_control* control, const struct hud_theme* theme, double delta_s) {
    struct hud_frame_result frame = hud_frame(parent, width, control->label, theme);

    double field_width = frame.content_width;

    struct ui_node field = ui_frame(&(struct ui_frame) {
        .parent = frame.content_frame,
        .layout = ui_layout_stack_y(ui_dist_start, ui_align_start, 0, 0),
        .fill = ui_fill_solid(theme->background_secondary),
    });

    struct ui_text_input_layout layout = {
        .width = field_width,
        .min_height = 24,
        .max_height = 24,
        .pt_size = 12,
        .weight = 400,
        .padding = make_double4(8, 8, 4, 4),
        .multiline = false,
    };

    struct ui_text_input_on_update_result result = ui_text_input_on_update(control->text_input, root, field, layout, delta_s);

    struct ui_color selection_color = theme->blue;
    selection_color.value.w = 0.35f;

    for (uint32_t i = 0; i < result.selection_rect_count; i++) {
        const struct ui_text_edit_rect* rect = &result.selection_rects[i];
        ui_frame(&(struct ui_frame) {
            .parent = result.content,
            .pos = ui_pos_xy((double)rect->x, (double)rect->y),
            .size = ui_size_fix((double)rect->width, (double)rect->height),
            .fill = ui_fill_solid(selection_color),
        });
    }

    if (result.text_size > 0) {
        ui_text(&(struct ui_text) {
            .parent = result.content,
            .pos = ui_pos_xy(0, 0),
            .str = hud_str(theme->font, result.text, theme->label_primary, 12),
        });
    }

    if (result.caret_visible) {
        ui_frame(&(struct ui_frame) {
            .parent = result.content,
            .pos = ui_pos_xy((double)(result.caret.x - 1.0), (double)result.caret.y),
            .size = ui_size_fix(2.0, (double)result.caret.height),
            .fill = ui_fill_solid(theme->blue),
        });
    }
}

void hud_controls_on_update(struct hud* hud, struct ui_node root, struct ui_node stack, double width, const struct hud_theme* theme, double delta_s) {
    for (uint32_t i = 0; i < hud->controls.count; i++) {
        struct hud_control* control = hud->controls.order[i];

        switch (control->type) {
        case hud_control_type_slider: {
            hud_slider_on_update(stack, width, control, theme);
        } break;
        case hud_control_type_toggle: {
            hud_toggle_on_update(stack, width, control, theme);
        } break;
        case hud_control_type_select: {
            hud_select_on_update(stack, width, control, theme);
        } break;
        case hud_control_type_button: {
            hud_button_on_update(stack, width, control, theme);
        } break;
        case hud_control_type_stepper: {
            hud_stepper_on_update(stack, width, control, theme);
        } break;
        case hud_control_type_label: {
            hud_label_on_update(stack, width, control, theme);
        } break;
        case hud_control_type_text_input: {
            hud_text_input_on_update(root, stack, width, control, theme, delta_s);
        } break;
        }
    }
}

void hud_controls_sweep(struct hud* hud) {
    hud->controls.count = 0;

    for (uint32_t i = 0; i < cr_count(hud->controls.slots); i++) {
        struct hud_control* control = &hud->controls.slots[i];

        if (control->declared) {
            control->declared = false;
        } else {
            hud_control_deinit(control);
        }
    }
}

void hud_controls_deinit(struct hud* hud) {
    for (uint32_t i = 0; i < cr_count(hud->controls.slots); i++) {
        hud_control_deinit(&hud->controls.slots[i]);
    }
}
