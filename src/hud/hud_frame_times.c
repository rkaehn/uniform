#include "hud_internal.h"
#include "gpu.h"
#include <stdio.h>

void hud_frame_times_init(struct hud* hud) {
    for (uint32_t i = 0; i < cr_count(hud->frame_times.slots); i++) {
        hud->frame_times.slots[i].profiler = gpu_profiler_create(hud->device, gpu_profile_max_passes);
    }
}

void hud_frame_times_deinit(struct hud* hud) {
    for (uint32_t i = 0; i < cr_count(hud->frame_times.slots); i++) {
        gpu_profiler_destroy(hud->device, hud->frame_times.slots[i].profiler);
    }
}

static void hud_frame_poll(struct hud* hud) {
    for (uint32_t i = 0; i < cr_count(hud->frame_times.slots); i++) {
        struct hud_frame_slot* slot = &hud->frame_times.slots[i];
        if (slot->seq == 0 || slot->ended == false) {
            continue;
        }
        struct gpu_profile profile = { 0 };
        if (slot->armed && gpu_profiler_poll(slot->profiler, &profile) == false) {
            continue;
        }
        if (slot->seq > hud->frame_times.shown_seq) {
            hud->frame_times.shown_seq = slot->seq;
            hud->frame_times.frame_s = slot->frame_s;
            hud->frame_times.cpu_s = slot->cpu_s;
            hud->frame_times.gpu_s = profile.span;
            hud->frame_times.pass_count = profile.pass_count;
            memcpy(hud->frame_times.passes, profile.passes, profile.pass_count * sizeof(profile.passes[0]));
            memcpy(hud->frame_times.pass_names, profile.pass_names, profile.pass_count * sizeof(profile.pass_names[0]));
        }
        slot->seq = 0;
        slot->armed = false;
        slot->ended = false;
    }
}

void hud_frame_begin(double timestamp) {
    struct hud* hud = hud_ctx();

    struct hud_frame_slot* stale = hud->frame_times.slot;
    if (stale != NULL) {
        stale->seq = 0;
        stale->armed = false;
        stale->ended = false;
    }

    hud_frame_poll(hud);

    struct hud_frame_slot* slot = NULL;
    for (uint32_t i = 0; i < cr_count(hud->frame_times.slots); i++) {
        if (hud->frame_times.slots[i].seq == 0) {
            slot = &hud->frame_times.slots[i];
            break;
        }
    }

    if (slot != NULL) {
        slot->seq = ++hud->frame_times.next_seq;
        slot->armed = false;
        slot->ended = false;
        slot->frame_s = hud->frame_times.prev_timestamp > 0.0 ? timestamp - hud->frame_times.prev_timestamp : 0.0;
        slot->cpu_s = 0.0;
    }

    hud->frame_times.active = true;
    hud->frame_times.slot = slot;
    hud->frame_times.prev_timestamp = timestamp;
    hud->frame_times.cpu_begin_s = cr_get_current_time();
    hud->frame_times.label[0] = 0;
}

void hud_frame_profile(struct gpu_command_buffer* command_buffer) {
    struct hud* hud = hud_ctx();

    struct hud_frame_slot* slot = hud->frame_times.slot;
    if (slot == NULL) {
        return;
    }
    cr_check(slot->armed == false, "hud_frame_profile runs once per frame");
    slot->armed = gpu_command_buffer_profile(command_buffer, slot->profiler);
}

void hud_frame_end(void) {
    struct hud* hud = hud_ctx();

    struct hud_frame_slot* slot = hud->frame_times.slot;
    hud->frame_times.slot = NULL;
    if (slot == NULL) {
        return;
    }
    slot->cpu_s = cr_get_current_time() - hud->frame_times.cpu_begin_s;
    slot->ended = true;
}

void hud_frame_label(const char* text) {
    struct hud* hud = hud_ctx();

    hud_label_copy(hud->frame_times.label, sizeof(hud->frame_times.label), text);
}

void hud_frame_times_on_update(struct hud* hud, struct ui_node parent, const struct hud_theme* theme) {
    struct ui_node box = ui_frame(&(struct ui_frame) {
        .parent = parent,
        .key = ui_key("frame times"),
        .pos = ui_pos_xy(0, 0),
        .layout = ui_layout_stack_y(ui_dist_start, ui_align_start, 2, ui_pad_xy(8, 4)),
        .fill = ui_fill_solid(theme->background_primary),
    });

    char text[64];

    snprintf(text, sizeof(text), "Frame %.2f ms", hud->frame_times.frame_s * 1000.0);
    ui_text(&(struct ui_text) {
        .parent = box,
        .str = hud_str(theme->font, text, theme->label_primary, 12),
    });

    snprintf(text, sizeof(text), "CPU %.2f ms", hud->frame_times.cpu_s * 1000.0);
    ui_text(&(struct ui_text) {
        .parent = box,
        .str = hud_str(theme->font, text, theme->label_primary, 12),
    });

    snprintf(text, sizeof(text), "GPU %.2f ms", hud->frame_times.gpu_s * 1000.0);
    ui_text(&(struct ui_text) {
        .parent = box,
        .str = hud_str(theme->font, text, theme->label_primary, 12),
    });

    for (uint32_t i = 0; i < hud->frame_times.pass_count; i++) {
        snprintf(text, sizeof(text), "%s %.2f ms", hud->frame_times.pass_names[i], hud->frame_times.passes[i] * 1000.0);
        ui_text(&(struct ui_text) {
            .parent = box,
            .str = hud_str(theme->font, text, theme->label_primary, 12),
        });
    }

    if (hud->frame_times.label[0] != 0) {
        ui_text(&(struct ui_text) {
            .parent = box,
            .str = hud_str(theme->font, hud->frame_times.label, theme->label_primary, 12),
        });
    }
}
