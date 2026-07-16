#include "demo_items.h"
#include "ui_input.h"
#include "ui_gestures.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

struct demo_item {
    uint32_t id;
    double2 size;
};

void demo_items_init(struct demo_items* items, struct cr_mem* mem, uint32_t count) {
    memset(items, 0, sizeof(*items));
    items->view = ui_list_create(mem, sizeof(struct demo_item));
    items->next_id = 1;
    items->expanded_id = uint32_max;
    items->drag_src = uint32_max;

    for (uint32_t i = 0; i < count; i++) {
        ui_list_edit(items->view, &(struct ui_list_edit) {
            .adds = &i,
            .add_count = 1,
        });
    }

    items->count = count;
}

void demo_items_deinit(struct demo_items* items) {
    ui_list_destroy(items->view);
}

void demo_items_add(struct demo_items* items, uint32_t index) {
    ui_list_edit(items->view, &(struct ui_list_edit) {
        .adds = &index,
        .add_count = 1,
        .animated = true,
    });
    items->count++;
}

void demo_items_remove(struct demo_items* items, uint32_t index) {
    ui_list_edit(items->view, &(struct ui_list_edit) {
        .deletes = &index,
        .delete_count = 1,
        .animated = true,
    });
    items->count--;
}

bool demo_items_is_dragging(const struct demo_items* items) {
    return items->drag_src != uint32_max;
}

static void demo_items_on_item_update(uint32_t index, void* content, double2 available, struct ui_list_on_item_update_result* out_result, void* user) {
    struct demo_items* items = user;
    struct demo_item* item = content;
    bool is_new = item->id == 0;

    if (is_new) {
        item->id = items->next_id++;
    }

    double2 target = items->desc.size(item->id, item->id == items->expanded_id, items->desc.user);

    if (is_new) {
        item->size = target;
    } else {
        double f = 1.0 - exp(-8.0 * items->desc.delta_s);

        item->size.x = ui_approach_px(item->size.x, target.x, f);
        item->size.y = ui_approach_px(item->size.y, target.y, f);
    }

    out_result->size = item->size;
}

static void demo_items_on_drag(struct ui_list_drag drag, void* user) {
    struct demo_items* items = user;
    struct ui_list_item_desc target;

    items->drag_position = drag.position;

    switch (drag.phase) {
    case ui_list_drag_phase_began: {
        items->drag_src = ui_list_get_item_at_position(items->view, drag.position, &target) ? target.index : uint32_max;
    } break;
    case ui_list_drag_phase_changed: break;
    case ui_list_drag_phase_ended: {
        if (items->drag_src != uint32_max && ui_list_get_item_near_position(items->view, drag.position, &target)) {
            struct ui_list_linear_insert insert = items->desc.insert(&target, drag.position, items->desc.user);
            uint32_t src = items->drag_src;
            uint32_t dst = insert.index > src ? insert.index - 1 : insert.index;

            if (dst != src) {
                struct ui_list_item_move move = { .src_index = src, .dst_index = dst };

                ui_list_edit(items->view, &(struct ui_list_edit) {
                    .moves = &move,
                    .move_count = 1,
                    .animated = true,
                });
            }
        }
        items->drag_src = uint32_max;
    } break;
    case ui_list_drag_phase_canceled: {
        items->drag_src = uint32_max;
    } break;
    }
}

static void demo_items_on_tap(struct ui_tap_gesture gesture, void* user) {
    struct demo_items* items = user;

    if (items->drag_src != uint32_max) {
        return;
    }

    struct ui_node host = items->desc.host;
    struct ui_node root = ui_get_root(host);
    double2 position = ui_to_local_from_root(root, host, ui_tap_gesture_get_position(gesture));
    struct ui_list_item_desc desc;

    if (ui_list_get_item_at_position(items->view, position, &desc)) {
        struct demo_item* item = desc.content;

        items->expanded_id = items->expanded_id == item->id ? uint32_max : item->id;
    }
}

void demo_items_frame(struct demo_items* items, const struct demo_items_desc* desc) {
    items->desc = *desc;

    ui_list_update(items->view, &(struct ui_list_update) {
        .node = desc->host,
        .layout = desc->layout,
        .snap = desc->snap,
        .delta_s = desc->delta_s,
        .on_item_update = demo_items_on_item_update,
        .on_drag = demo_items_on_drag,
        .user = items,
    });

    ui_tap(&(struct ui_tap) {
        .node = desc->host,
        .on_change = demo_items_on_tap,
        .user = items,
    });

    bool dragging = items->drag_src != uint32_max;
    struct ui_list_linear_insert insert = { .index = uint32_max };
    struct ui_list_item_desc target;

    if (dragging && ui_list_get_item_near_position(items->view, items->drag_position, &target)) {
        insert = desc->insert(&target, items->drag_position, desc->user);
    }

    struct ui_list_iter iter = ui_list_iter(items->view);
    struct ui_list_item_desc item_desc;

    while (ui_list_iter_next(&iter, &item_desc)) {
        struct demo_item* item = item_desc.content;
        double opacity = item_desc.opacity;

        if (dragging && item_desc.index == items->drag_src) {
            opacity *= 0.35;
        }

        struct ui_node card = ui_frame(&(struct ui_frame) {
            .parent = desc->host,
            .key = ui_key_u64(item->id),
            .pos = ui_pos_xy(item_desc.pos.x, item_desc.pos.y),
            .size = ui_size_fix(item_desc.size.x, item_desc.size.y),
            .corners = ui_corners(12),
            .fill = ui_fill_solid(demo_palette[item->id % (uint32_t)cr_count(demo_palette)]),
            .blend = { .mode = ui_blend_mode_normal, .opacity = opacity },
            .layout = ui_layout_stack_y(ui_dist_center, ui_align_center, 0, 0),
        });

        char label[16];
        snprintf(label, sizeof(label), "%u", item->id);

        ui_text(&(struct ui_text) {
            .parent = card,
            .str = ui_str_weight(label, ui_gray_a(0.0f, 0.8f), 13, 590),
        });
    }

    if (insert.index != uint32_max) {
        double thickness = 3.0;
        double2 cursor_size = max_double2(insert.cursor_size, make_double2(thickness, thickness));
        double2 cursor_pos = insert.cursor_pos - cursor_size * 0.5;

        ui_frame(&(struct ui_frame) {
            .parent = desc->host,
            .pos = ui_pos_xy(cursor_pos.x, cursor_pos.y),
            .size = ui_size_fix(cursor_size.x, cursor_size.y),
            .corners = ui_corners(1.5),
            .fill = ui_fill_solid(desc->theme->fg),
        });
    }
}
