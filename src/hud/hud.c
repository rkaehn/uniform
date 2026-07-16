#include "hud_internal.h"
#include "ui_gestures.h"
#include "log.h"

static _Thread_local struct hud* hud_current = NULL;

struct hud* hud_ctx(void) {
    cr_check(hud_current != NULL, "there is no current hud; call hud_set first");
    return hud_current;
}

void hud_set(struct hud* hud) {
    hud_current = hud;
}

struct hud* hud_get(void) {
    return hud_current;
}

struct hud* hud_create(struct cr_mem* mem, struct gpu_device* device, struct ui_font* font) {
    cr_check(font != NULL, "the hud needs a monospace font");

    struct hud* hud = cr_malloc(mem, sizeof(*hud), 0, cr_loc);

    memset(hud, 0, sizeof(*hud));
    hud->mem = mem;
    hud->device = device;
    hud->font = font;
    hud_frame_times_init(hud);

    return hud;
}

void hud_destroy(struct hud* hud) {
    if (hud_current == hud) {
        hud_current = NULL;
    }

    hud_controls_deinit(hud);
    hud_frame_times_deinit(hud);
    hud_log_deinit(hud);
    cr_free(hud->mem, hud, cr_loc);
}

void hud_set_enabled(bool enabled) {
    hud_ctx()->enabled = enabled;
}

void hud_label_copy(char* dst, size_t dst_size, const char* label) {
    cr_check(strlen(label) < dst_size, "a hud label holds at most %zu bytes", dst_size - 1);
    strncpy(dst, label, dst_size - 1);
    dst[dst_size - 1] = 0;
}

static void hud_enabled_on_tap(struct ui_tap_gesture gesture, void* user) {
    struct hud* hud = user;

    hud->enabled = !hud->enabled;
}

void hud_on_update(struct ui_node root, bool is_dark, double delta_s) {
    struct hud* hud = hud_ctx();

    struct ui_node main = ui_frame(&(struct ui_frame) {
        .parent = root,
        .size = ui_size(ui_size_axis_width(), ui_size_axis_height()),
        .blend = { .mode = ui_blend_mode_normal, .opacity = 0.5f },
    });

    struct hud_theme theme = hud_theme(is_dark);
    theme.font = hud->font;

    struct ui_node column = ui_frame(&(struct ui_frame) {
        .parent = main,
        .pos.x = ui_pos_axis_end(),
        .pos.y = ui_pos_axis_start(),
        .layout = ui_layout_stack_y(ui_dist_start, ui_align_end, 0, 0),
    });

    if (hud->enabled) {
        struct ui_node stack = ui_frame(&(struct ui_frame) {
            .parent = column,
            .layout = ui_layout_stack_y(ui_dist_start, ui_align_start, 0, 0),
            .fill = ui_fill_solid(theme.background_primary),
        });

        hud_controls_on_update(hud, root, stack, 240, &theme, delta_s);

        if (hud->frame_times.active) {
            hud_frame_times_on_update(hud, main, &theme);
        }
    }

    struct ui_node button = ui_frame(&(struct ui_frame) {
        .parent = main,
        .pos.x = ui_pos_axis_end(),
        .pos.y = ui_pos_axis_end(),
        .layout = ui_layout_stack_y(ui_dist_center, ui_align_center, 0, ui_pad_xy(8, 4)),
        .fill = ui_fill_solid(hud->enabled ? theme.blue : theme.background_secondary),
    });

    ui_tap(&(struct ui_tap) {
        .node = button,
        .on_change = hud_enabled_on_tap,
        .user = hud,
    });

    ui_text(&(struct ui_text) {
        .parent = button,
        .str = hud_str(theme.font, "HUD", hud->enabled ? ui_white : theme.label_primary, 12),
    });

    hud->frame_times.active = false;

    if (log_is_active()) {
        hud_log_on_update(hud, main, &theme, delta_s);
    }

    hud_controls_sweep(hud);
}
