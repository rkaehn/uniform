#pragma once

#include "demo.h"
#include "ui_list.h"

struct demo_items_desc {
    struct ui_node host;
    struct ui_list_layout layout;
    double2 (*size)(uint32_t id, bool expanded, void* user);
    struct ui_list_linear_insert (*insert)(const struct ui_list_item_desc* target, double2 position, void* user);
    void* user;
    bool snap;
    double delta_s;
    const struct demo_theme* theme;
};

struct demo_items {
    struct ui_list* view;
    uint32_t count;
    uint32_t next_id;
    uint32_t expanded_id;
    uint32_t drag_src;
    double2 drag_position;
    struct demo_items_desc desc;
};

void demo_items_init(struct demo_items* items, struct cr_mem* mem, uint32_t count);
void demo_items_deinit(struct demo_items* items);

void demo_items_add(struct demo_items* items, uint32_t index);
void demo_items_remove(struct demo_items* items, uint32_t index);
bool demo_items_is_dragging(const struct demo_items* items);

void demo_items_frame(struct demo_items* items, const struct demo_items_desc* desc);
