#include "cr.h"
#include "ui.h"
#include "ui_list.h"
#include "ui_list_internal.h"
#include "tree.h"
#include "../core/ui_fx.h"
#include <stdlib.h>
#include "ui_gestures.h"
#include "ui_physics.h"

enum ui_list_birth {
    ui_list_birth_none,
    ui_list_birth_animated,
    ui_list_birth_immediate,
};

struct ui_list_item {
    uint32_t group_slot;
    enum ui_list_birth birth;
    bool is_removed;
    bool is_hidden;
    double2 offset;
    double2 size;
    double2 target_offset;
    double2 target_size;
    double2 group_shift;
    double opacity;
};

struct ui_list_items {
    char* base;
    size_t stride;
    size_t content_offset;
    uint32_t count;
};

struct ui_list_measure_result {
    bool sizes_changed;
    bool has_births;
};

#define cr_arr ui_arr_list_item_desc
#define cr_arr_item struct ui_list_item_desc
#include "cr_arr.h"

#define cr_arr ui_arr_list_layout_item
#define cr_arr_item struct ui_list_layout_item
#include "cr_arr.h"

struct ui_list {
    struct cr_mem* mem;
    struct cr_tree* tree;
    uint64_t content_size;
    size_t node_content_size;
    struct ui_arr_list_item_desc* item_descs;
    struct ui_arr_list_layout_item* layout_items;
    bool rebase_pending;
    bool sweep_pending;
    bool descs_dirty;
    bool in_update;
    bool drag_active;
    double2 drag_position;
    void (*on_drag)(struct ui_list_drag drag, void* user);
    void* user;
    struct ui_scroll scroll;
    struct ui_node prev_node;
    enum ui_list_axis scroll_axis;
    bool has_frame;
    double2 scroll_pos;
    double visible_low;
    double visible_high;
};

struct ui_list_op {
    uint32_t index;
    uint32_t move_id;
};

static int ui_list_op_cmp(const void* a, const void* b) {
    const struct ui_list_op* op_a = a;
    const struct ui_list_op* op_b = b;
    if (op_a->index < op_b->index) {
        return -1;
    }
    if (op_a->index > op_b->index) {
        return 1;
    }
    return 0;
}

static int ui_list_move_dst_cmp(const void* a, const void* b) {
    uint32_t dst_a = cr_tree_loc_get_index(((const struct cr_tree_loc_2*)a)->dst);
    uint32_t dst_b = cr_tree_loc_get_index(((const struct cr_tree_loc_2*)b)->dst);
    if (dst_a < dst_b) {
        return -1;
    }
    if (dst_a > dst_b) {
        return 1;
    }
    return 0;
}

static struct ui_list_items ui_list_items_get(struct ui_list* list) {
    struct ui_list_items items = {
        .stride = cr_tree_node_size(list->node_content_size),
        .count = cr_tree_get_node_count(list->tree),
    };

    if (items.count > 0) {
        struct cr_tree_node* node = cr_tree_get_node(list->tree, 0);

        items.base = (char*)node;
        items.content_offset = (size_t)((char*)cr_tree_node_get_content(node) - (char*)node);
    }

    return items;
}

static inline struct ui_list_item* ui_list_item_at(const struct ui_list_items* items, uint32_t slot) {
    assert(slot < items->count);
    return (struct ui_list_item*)(items->base + (size_t)slot * items->stride + items->content_offset);
}

static inline void* ui_list_item_content(const struct ui_list* list, struct ui_list_item* item) {
    if (list->content_size == 0) {
        return NULL;
    }
    return (char*)item + sizeof(struct ui_list_item);
}

struct ui_list* ui_list_create(struct cr_mem* mem, uint64_t content_size) {
    struct ui_list* list = cr_malloc(mem, sizeof(*list), 0, cr_loc);

    memset(list, 0, sizeof(*list));
    list->mem = mem;

    list->content_size = content_size;
    list->node_content_size = sizeof(struct ui_list_item) + (size_t)content_size;
    list->tree = cr_tree_create(mem, mem, list->node_content_size);
    list->item_descs = ui_arr_list_item_desc_create(mem);
    list->layout_items = ui_arr_list_layout_item_create(mem);

    ui_scroll_init(&list->scroll);

    return list;
}

void ui_list_destroy(struct ui_list* list) {
    cr_tree_destroy(list->tree);
    ui_arr_list_item_desc_destroy(list->item_descs);
    ui_arr_list_layout_item_destroy(list->layout_items);
    cr_free(list->mem, list, cr_loc);
}

void ui_list_edit(struct ui_list* list, const struct ui_list_edit* edit) {
    cr_check(list->in_update == false, "a list cannot be edited inside its update");

    uint32_t add_count = edit->add_count;
    uint32_t delete_count = edit->delete_count;
    uint32_t move_count = edit->move_count;
    bool animated = edit->animated;

    uint32_t total_deletes = delete_count + move_count;
    uint32_t total_adds = add_count + move_count;

    if (total_deletes == 0 && total_adds == 0) {
        return;
    }

    list->descs_dirty = true;

    if (animated == false) {
        list->rebase_pending = true;
    }

    struct ui_list_op* delete_ops = total_deletes > 0 ? cr_malloc(list->mem, total_deletes * sizeof(*delete_ops), 0, cr_loc) : NULL;
    struct ui_list_op* add_ops = total_adds > 0 ? cr_malloc(list->mem, total_adds * sizeof(*add_ops), 0, cr_loc) : NULL;
    struct cr_tree_loc* tree_deletes = delete_count > 0 ? cr_malloc(list->mem, delete_count * sizeof(*tree_deletes), 0, cr_loc) : NULL;
    struct cr_tree_loc* tree_adds = add_count > 0 ? cr_malloc(list->mem, add_count * sizeof(*tree_adds), 0, cr_loc) : NULL;
    struct cr_tree_loc_2* tree_moves = move_count > 0 ? cr_malloc(list->mem, move_count * sizeof(*tree_moves), 0, cr_loc) : NULL;
    uint32_t* removed_slots = total_deletes > 0 ? cr_malloc(list->mem, total_deletes * sizeof(*removed_slots), 0, cr_loc) : NULL;

    for (uint32_t i = 0; i < delete_count; i++) {
        delete_ops[i].index = edit->deletes[i];
        delete_ops[i].move_id = uint32_max;
    }
    for (uint32_t i = 0; i < move_count; i++) {
        delete_ops[delete_count + i].index = edit->moves[i].src_index;
        delete_ops[delete_count + i].move_id = i;
    }

    if (total_deletes > 0) {
        qsort(delete_ops, total_deletes, sizeof(struct ui_list_op), ui_list_op_cmp);
    }

    for (uint32_t i = 0; i < add_count; i++) {
        add_ops[i].index = edit->adds[i];
        add_ops[i].move_id = uint32_max;
    }
    for (uint32_t i = 0; i < move_count; i++) {
        add_ops[add_count + i].index = edit->moves[i].dst_index;
        add_ops[add_count + i].move_id = i;
    }

    if (total_adds > 0) {
        qsort(add_ops, total_adds, sizeof(struct ui_list_op), ui_list_op_cmp);
    }

    struct ui_list_items items = ui_list_items_get(list);
    uint32_t tree_delete_count = 0;
    uint32_t removed_slot_count = 0;

    uint32_t slot = 0;
    uint32_t index = 0;
    uint32_t op_index = 0;

    while (op_index < total_deletes) {
        struct ui_list_item* item = ui_list_item_at(&items, slot);

        if (item->is_removed) {
            slot++;
            continue;
        }

        if (index != delete_ops[op_index].index) {
            index++;
            slot++;
            continue;
        }

        uint32_t move_id = delete_ops[op_index].move_id;

        if (move_id != uint32_max) {
            tree_moves[move_id].src = cr_tree_loc_make_index(slot);
            removed_slots[removed_slot_count++] = slot;
        } else if (animated && item->opacity != 0.0) {
            item->is_removed = true;
        } else {
            tree_deletes[tree_delete_count++] = cr_tree_loc_make_index(slot);
            removed_slots[removed_slot_count++] = slot;
        }

        slot++;
        op_index++;
        index++;
    }

    slot = 0;
    index = 0;
    op_index = 0;

    uint32_t final_slot = 0;
    uint32_t removed_cursor = 0;
    uint32_t tree_add_count = 0;

    while (op_index < total_adds) {
        if (index == add_ops[op_index].index) {
            uint32_t move_id = add_ops[op_index].move_id;

            if (move_id != uint32_max) {
                tree_moves[move_id].dst = cr_tree_loc_make_index(final_slot);
            } else {
                tree_adds[tree_add_count++] = cr_tree_loc_make_index(final_slot);
            }

            final_slot++;
            op_index++;
            index++;
        } else {
            if (removed_cursor < removed_slot_count && removed_slots[removed_cursor] == slot) {
                removed_cursor++;
                slot++;
                continue;
            }

            if (ui_list_item_at(&items, slot)->is_removed == false) {
                index++;
            }

            final_slot++;
            slot++;
        }
    }

    assert(tree_add_count == add_count);

    struct cr_tree_node* add_nodes = add_count > 0 ? cr_malloc(list->mem, add_count * cr_tree_node_size(list->node_content_size), 0, cr_loc) : NULL;

    for (uint32_t i = 0; i < add_count; i++) {
        struct cr_tree_node* node = cr_tree_node_at(add_nodes, i, cr_tree_node_size(list->node_content_size));

        cr_tree_node_init(node);

        struct ui_list_item* item = cr_tree_node_get_content(node);

        memset(item, 0, list->node_content_size);
        item->opacity = animated ? 0.0 : 1.0;
        item->birth = animated ? ui_list_birth_animated : ui_list_birth_immediate;
    }

    if (add_count > 0 || tree_delete_count > 0 || move_count > 0) {
        if (move_count > 0) {
            qsort(tree_moves, move_count, sizeof(struct cr_tree_loc_2), ui_list_move_dst_cmp);
        }
        cr_tree_move(list->tree, tree_adds, add_count, tree_deletes, tree_delete_count, tree_moves, move_count, add_nodes);
    }

    cr_free(list->mem, delete_ops, cr_loc);
    cr_free(list->mem, add_ops, cr_loc);
    cr_free(list->mem, tree_deletes, cr_loc);
    cr_free(list->mem, tree_adds, cr_loc);
    cr_free(list->mem, tree_moves, cr_loc);
    cr_free(list->mem, removed_slots, cr_loc);
    cr_free(list->mem, add_nodes, cr_loc);
}

static void ui_list_sweep(struct ui_list* list) {
    if (list->sweep_pending == false) {
        return;
    }
    list->sweep_pending = false;

    struct ui_list_items items = ui_list_items_get(list);
    struct cr_tree_removal* removals = cr_malloc(list->mem, items.count * sizeof(*removals), 0, cr_loc);
    uint32_t removal_count = 0;

    for (uint32_t slot = 0; slot < items.count; slot++) {
        struct ui_list_item* item = ui_list_item_at(&items, slot);

        if (item->is_removed && item->opacity == 0.0) {
            removals[removal_count++].index = slot;
        }
    }

    cr_tree_remove(list->tree, removals, removal_count, true);
    cr_free(list->mem, removals, cr_loc);
}

static void ui_list_emit_desc(struct ui_list* list, struct ui_list_item* item, uint32_t index) {
    double item_low = ui_list_component(item->offset, list->scroll_axis);
    double item_high = item_low + ui_list_component(item->size, list->scroll_axis);

    if (item->birth != ui_list_birth_none || item->opacity <= 0.0 || item_low >= list->visible_high || item_high <= list->visible_low) {
        return;
    }

    struct ui_list_item_desc* desc = ui_arr_list_item_desc_append_ptr(list->item_descs, 1);

    desc->index = item->is_removed ? uint32_max : index;
    desc->is_removed = item->is_removed;
    desc->opacity = item->opacity;
    desc->pos = item->offset + list->scroll_pos;
    desc->size = item->size;
    desc->content = ui_list_item_content(list, item);
}

static void ui_list_sync_descs(struct ui_list* list) {
    if (list->descs_dirty == false) {
        return;
    }
    list->descs_dirty = false;

    ui_arr_list_item_desc_remove_all(list->item_descs);

    if (list->has_frame == false) {
        return;
    }

    struct ui_list_items items = ui_list_items_get(list);
    uint32_t index = 0;

    for (uint32_t slot = 0; slot < items.count; slot++) {
        struct ui_list_item* item = ui_list_item_at(&items, slot);

        ui_list_emit_desc(list, item, index);

        if (item->is_removed == false) {
            index++;
        }
    }
}

double ui_list_get_scroll_offset(struct ui_list* list) {
    return ui_scroll_get_offset(&list->scroll);
}

bool ui_list_get_item_at_position(struct ui_list* list, double2 position, struct ui_list_item_desc* out_desc) {
    ui_list_sync_descs(list);

    uint32_t count = ui_arr_list_item_desc_get_count(list->item_descs);
    const struct ui_list_item_desc* descs = ui_arr_list_item_desc_get_data(list->item_descs);

    for (uint32_t i = 0; i < count; i++) {
        const struct ui_list_item_desc* desc = &descs[i];

        if (desc->is_removed) {
            continue;
        }

        if (all_long2((long2)(position >= desc->pos)) && all_long2((long2)(position < desc->pos + desc->size))) {
            *out_desc = *desc;
            return true;
        }
    }

    return false;
}

bool ui_list_get_item_near_position(struct ui_list* list, double2 position, struct ui_list_item_desc* out_desc) {
    ui_list_sync_descs(list);

    uint32_t count = ui_arr_list_item_desc_get_count(list->item_descs);
    const struct ui_list_item_desc* descs = ui_arr_list_item_desc_get_data(list->item_descs);
    const struct ui_list_item_desc* nearest = NULL;
    double nearest_distance = 0.0;

    for (uint32_t i = 0; i < count; i++) {
        const struct ui_list_item_desc* desc = &descs[i];

        if (desc->is_removed) {
            continue;
        }

        double distance = length_double2(position - (desc->pos + desc->size * 0.5));

        if (nearest == NULL || distance < nearest_distance) {
            nearest = desc;
            nearest_distance = distance;
        }
    }

    if (nearest == NULL) {
        return false;
    }

    *out_desc = *nearest;
    return true;
}

static void ui_list_on_pan(struct ui_pan_gesture pan, void* user) {
    struct ui_list* list = user;
    enum ui_pan_direction direction = list->scroll_axis == ui_list_axis_x ? ui_pan_direction_x : ui_pan_direction_y;

    if (ui_scroll_on_pan(&list->scroll, pan, direction)) {
        ui_pan_gesture_claim(pan);
    }
}

static void ui_list_on_long_press(struct ui_long_press_gesture gesture, void* user) {
    struct ui_list* list = user;
    struct ui_list_drag drag = { .position = list->drag_position };

    switch (ui_long_press_gesture_get_phase(gesture)) {
    case ui_gesture_phase_began:
        drag.phase = ui_list_drag_phase_began;
        break;
    case ui_gesture_phase_changed:
        drag.phase = ui_list_drag_phase_changed;
        break;
    case ui_gesture_phase_ended:
        drag.phase = ui_list_drag_phase_ended;
        break;
    case ui_gesture_phase_canceled:
        drag.phase = ui_list_drag_phase_canceled;
        break;
    default:
        return;
    }

    if (drag.phase != ui_list_drag_phase_canceled) {
        struct ui_node root = ui_get_root(list->prev_node);

        drag.position = ui_to_local_from_root(root, list->prev_node, ui_long_press_gesture_get_position(gesture));
    }

    list->drag_active = drag.phase == ui_list_drag_phase_began || drag.phase == ui_list_drag_phase_changed;
    list->drag_position = drag.position;

    if (list->on_drag) {
        list->on_drag(drag, list->user);
    }
}

static void ui_list_on_scroll(struct app_scroll_event event, void* user) {
    struct ui_list* list = user;
    bool is_x = list->scroll_axis == ui_list_axis_x;
    ui_scroll_on_wheel(&list->scroll, is_x ? event.delta.x : event.delta.y);
}

static struct ui_list_measure_result ui_list_measure(struct ui_list* list, const struct ui_list_update* update, const struct ui_list_items* items, double to_px_from_pt) {
    const struct ui_list_layout* layout = &update->layout;
    struct ui_list_measure_result result = { 0 };

    struct {
        uint32_t end_index;
        uint32_t header_slot;
    } collapse[64];
    uint32_t collapse_depth = 0;
    uint32_t index = 0;

    ui_arr_list_layout_item_remove_all(list->layout_items);

    for (uint32_t slot = 0; slot < items->count; slot++) {
        struct ui_list_item* item = ui_list_item_at(items, slot);

        while (collapse_depth > 0 && index >= collapse[collapse_depth - 1].end_index) {
            collapse_depth--;
        }

        if (item->is_removed) {
            continue;
        }

        struct ui_list_on_item_update_result update_result = { 0 };
        void* content = ui_list_item_content(list, item);

        update->on_item_update(index, content, layout->item_available, &update_result, update->user);

        double2 size = update_result.size;

        if (layout->item_size) {
            size = layout->item_size(size, layout->item_available, layout->user);
        }

        size.x = ui_round_px_size_ex(size.x, to_px_from_pt);
        size.y = ui_round_px_size_ex(size.y, to_px_from_pt);

        if (item->birth != ui_list_birth_none) {
            result.sizes_changed = result.sizes_changed || item->birth == ui_list_birth_immediate;
            result.has_births = true;
            item->size = size;
        } else if (is_equal_double2(size, item->target_size) == false) {
            result.sizes_changed = true;
        }

        item->target_size = size;
        item->is_hidden = collapse_depth > 0;
        item->group_slot = item->is_hidden ? collapse[collapse_depth - 1].header_slot : uint32_max;

        if (item->is_hidden == false) {
            ui_arr_list_layout_item_append(list->layout_items, (struct ui_list_layout_item) {
                .index = index,
                .content = content,
                .size = size,
                .prev_size = item->size,
            });
        }

        if (update_result.collapse_end_index > index) {
            assert(collapse_depth < cr_count(collapse));

            collapse[collapse_depth].end_index = update_result.collapse_end_index;
            collapse[collapse_depth].header_slot = slot;
            collapse_depth++;
        }

        index++;
    }

    return result;
}

static void ui_list_rebase(struct ui_list* list, const struct ui_list_layout* layout, const struct ui_list_items* items) {
    struct ui_list_layout_item* layout_items = ui_arr_list_layout_item_get_data(list->layout_items);
    uint32_t visible_index = 0;

    layout->place(layout_items, ui_arr_list_layout_item_get_count(list->layout_items), true, layout->user);

    for (uint32_t slot = 0; slot < items->count; slot++) {
        struct ui_list_item* item = ui_list_item_at(items, slot);

        if (item->is_removed) {
            continue;
        }

        double2 shift = make_double2(0.0, 0.0);

        if (item->birth == ui_list_birth_none) {
            if (item->is_hidden) {
                shift = ui_list_item_at(items, item->group_slot)->group_shift;
            } else {
                shift = layout_items[visible_index].offset - item->target_offset;
            }

            item->offset += shift;
        }

        item->group_shift = shift;

        if (item->is_hidden == false) {
            visible_index++;
        }
    }
}

static void ui_list_place_births(struct ui_list* list, const struct ui_list_items* items) {
    const struct ui_list_layout_item* layout_items = ui_arr_list_layout_item_get_data(list->layout_items);
    struct ui_list_item* before = NULL;
    uint32_t run_first_slot = uint32_max;
    uint32_t run_first_visible = 0;
    uint32_t visible_index = 0;

    for (uint32_t slot = 0; slot <= items->count; slot++) {
        struct ui_list_item* item = slot < items->count ? ui_list_item_at(items, slot) : NULL;

        if (item && (item->is_removed || item->is_hidden)) {
            continue;
        }

        bool in_run = item && item->birth != ui_list_birth_none;

        if (in_run) {
            if (run_first_slot == uint32_max) {
                run_first_slot = slot;
                run_first_visible = visible_index;
            }
        } else if (run_first_slot != uint32_max) {
            struct ui_list_item* neighbor = item ? item : before;
            double2 lag = neighbor ? neighbor->offset - neighbor->target_offset : make_double2(0.0, 0.0);
            uint32_t run_visible = run_first_visible;

            for (uint32_t run_slot = run_first_slot; run_slot < slot; run_slot++) {
                struct ui_list_item* new_item = ui_list_item_at(items, run_slot);

                if (new_item->is_removed || new_item->is_hidden) {
                    continue;
                }

                if (new_item->birth == ui_list_birth_animated) {
                    new_item->offset = layout_items[run_visible].offset + lag;
                }

                run_visible++;
            }

            run_first_slot = uint32_max;
        }

        if (item) {
            if (in_run == false) {
                before = item;
            }
            visible_index++;
        }
    }
}

static void ui_list_animate(struct ui_list* list, const struct ui_list_items* items, double approach, bool snap_frame, double to_px_from_pt) {
    const struct ui_list_layout_item* layout_items = ui_arr_list_layout_item_get_data(list->layout_items);
    uint32_t place_index = 0;
    uint32_t index = 0;

    ui_arr_list_item_desc_remove_all(list->item_descs);

    for (uint32_t slot = 0; slot < items->count; slot++) {
        struct ui_list_item* item = ui_list_item_at(items, slot);
        bool is_new = item->birth != ui_list_birth_none;

        if (item->is_removed == false) {
            double2 prev_target = item->target_offset;

            if (item->is_hidden == false) {
                item->target_offset = layout_items[place_index].offset;
                place_index++;
            } else if (is_new) {
                item->target_offset = ui_list_item_at(items, item->group_slot)->target_offset;
            } else {
                item->target_offset += ui_list_item_at(items, item->group_slot)->group_shift;
            }

            item->group_shift = is_new ? make_double2(0.0, 0.0) : item->target_offset - prev_target;
        }

        double target_opacity = (item->is_removed || item->is_hidden) ? 0.0 : 1.0;
        double2 target_offset = item->is_removed ? item->offset : item->target_offset;

        item->opacity += (target_opacity - item->opacity) * approach;

        if (snap_frame || item->birth == ui_list_birth_immediate || (is_new && target_opacity == 0.0)) {
            if (snap_frame || target_opacity == 0.0) {
                item->opacity = target_opacity;
            }
            item->offset = target_offset;
        } else {
            item->offset.x = ui_approach_px_ex(item->offset.x, target_offset.x, approach, to_px_from_pt);
            item->offset.y = ui_approach_px_ex(item->offset.y, target_offset.y, approach, to_px_from_pt);
        }

        item->birth = ui_list_birth_none;

        if (item->is_removed == false) {
            item->size = item->target_size;
        }

        if (fabs(target_opacity - item->opacity) < 0.001) {
            item->opacity = target_opacity;
        }

        if (item->is_removed && item->opacity == 0.0) {
            list->sweep_pending = true;
        }

        ui_list_emit_desc(list, item, index);

        if (item->is_removed == false) {
            index++;
        }
    }
}

static void ui_list_attach(struct ui_list* list, struct ui_node node, bool scrollable, bool has_items) {
    if (scrollable) {
        ui_on_scroll(&(struct ui_on_scroll) {
            .node = node,
            .func = ui_list_on_scroll,
            .user = list,
        });
        ui_pan(&(struct ui_pan) {
            .node = node,
            .on_change = ui_list_on_pan,
            .user = list,
            .direction = ui_pan_direction_any,
            .holds_touches = true,
        });
    }

    if (list->on_drag && (list->drag_active || has_items)) {
        ui_long_press(&(struct ui_long_press) {
            .node = node,
            .on_change = ui_list_on_long_press,
            .user = list,
        });
    }
}

void ui_list_update(struct ui_list* list, const struct ui_list_update* update) {
    const struct ui_list_layout* layout = &update->layout;

    cr_check(layout->place != NULL, "a list layout needs a place function");
    cr_check(update->on_item_update != NULL, "a list update needs an on_item_update callback");
    cr_check(ui_node_is_null(update->node) == false, "a list update needs a node");
    cr_check(list->in_update == false, "a list cannot be updated inside its update");

    list->in_update = true;

    ui_list_sweep(list);

    list->scroll_axis = layout->scroll_axis;

    bool snap_frame = list->has_frame == false || update->snap;
    double to_px_from_pt = ui_get_to_px_from_pt();
    struct ui_list_items items = ui_list_items_get(list);

    bool needs_rebase = list->rebase_pending;
    list->rebase_pending = false;

    struct ui_list_measure_result measured = ui_list_measure(list, update, &items, to_px_from_pt);

    if (snap_frame == false && (needs_rebase || measured.sizes_changed)) {
        ui_list_rebase(list, layout, &items);
    }

    uint32_t visible_count = ui_arr_list_layout_item_get_count(list->layout_items);
    double2 content_size = layout->place(ui_arr_list_layout_item_get_data(list->layout_items), visible_count, false, layout->user);

    if (snap_frame == false && measured.has_births) {
        ui_list_place_births(list, &items);
    }

    double scroll_view = ui_list_component(layout->size, layout->scroll_axis);
    double scroll_content = ui_list_component(content_size, layout->scroll_axis);

    ui_scroll_on_update(&list->scroll, list->scroll.offset, list->scroll.offset + scroll_view, 0.0, scroll_content, update->delta_s);

    list->visible_low = ui_scroll_get_offset(&list->scroll);
    list->visible_high = list->visible_low + scroll_view;
    list->scroll_pos = ui_list_from_components(layout->scroll_axis, -list->visible_low, 0.0);

    double approach = 1.0 - exp(-8.0 * update->delta_s);

    ui_list_animate(list, &items, approach, snap_frame, to_px_from_pt);

    list->has_frame = true;
    list->descs_dirty = false;
    list->on_drag = update->on_drag;
    list->user = update->user;
    list->prev_node = update->node;
    list->in_update = false;

    ui_list_attach(list, update->node, scroll_content > scroll_view, visible_count > 0);
}

struct ui_list_iter ui_list_iter(struct ui_list* list) {
    ui_list_sync_descs(list);

    return (struct ui_list_iter) {
        .list = list,
    };
}

bool ui_list_iter_next(struct ui_list_iter* inout_iter, struct ui_list_item_desc* out_desc) {
    struct ui_list* list = inout_iter->list;

    cr_check(list->descs_dirty == false, "a list cannot be edited during iteration");

    if (inout_iter->index >= ui_arr_list_item_desc_get_count(list->item_descs)) {
        return false;
    }

    *out_desc = ui_arr_list_item_desc_get_data(list->item_descs)[inout_iter->index++];
    return true;
}
