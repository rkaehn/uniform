#pragma once

#include "cr.h"
#include "ui.h"

struct ui_list;

enum cr_closed ui_list_axis {
    ui_list_axis_x,
    ui_list_axis_y,
};

enum cr_closed ui_list_drag_phase {
    ui_list_drag_phase_began,
    ui_list_drag_phase_changed,
    ui_list_drag_phase_ended,
    ui_list_drag_phase_canceled,
};

struct ui_list_item_move {
    uint32_t src_index;
    uint32_t dst_index;
};

struct ui_list_edit {
    const uint32_t* adds;
    uint32_t add_count;
    const uint32_t* deletes;
    uint32_t delete_count;
    const struct ui_list_item_move* moves;
    uint32_t move_count;
    bool animated;
};

struct ui_list_on_item_update_result {
    double2 size;
    uint32_t collapse_end_index;
};

struct ui_list_layout_item {
    uint32_t index;
    void* content;
    double2 size;
    double2 prev_size;
    double2 offset;
};

struct ui_list_layout {
    double2 (*place)(struct ui_list_layout_item* items, uint32_t count, bool rebase, void* user);
    double2 (*item_size)(double2 size, double2 available, void* user);
    void* user;
    enum ui_list_axis scroll_axis;
    double2 size;
    double2 item_available;
};

struct ui_list_drag {
    enum ui_list_drag_phase phase;
    double2 position;
};

struct ui_list_update {
    struct ui_node node;
    struct ui_list_layout layout;
    bool snap;
    double delta_s;
    void (*on_item_update)(uint32_t index, void* content, double2 available_size, struct ui_list_on_item_update_result* out_result, void* user);
    void (*on_drag)(struct ui_list_drag drag, void* user);
    void* user;
};

struct ui_list_item_desc {
    uint32_t index;
    bool is_removed;
    double opacity;
    double2 pos;
    double2 size;
    void* content;
};

struct ui_list_iter {
    struct ui_list* list;
    uint32_t index;
};

struct ui_list_linear {
    enum ui_list_axis axis;
    bool wrap;
    double2 size;
    double4 padding;
    double2 spacing;
    enum ui_dist dist;
    enum ui_align align;
};

struct ui_list_linear_insert {
    uint32_t index;
    double2 cursor_pos;
    double2 cursor_size;
};

struct ui_list* ui_list_create(struct cr_mem* mem, uint64_t content_size);
void ui_list_destroy(struct ui_list* list);

void ui_list_edit(struct ui_list* list, const struct ui_list_edit* edit);
void ui_list_update(struct ui_list* list, const struct ui_list_update* update);

double ui_list_get_scroll_offset(struct ui_list* list);

bool ui_list_get_item_at_position(struct ui_list* list, double2 position, struct ui_list_item_desc* out_desc);
bool ui_list_get_item_near_position(struct ui_list* list, double2 position, struct ui_list_item_desc* out_desc);

struct ui_list_iter ui_list_iter(struct ui_list* list);
bool ui_list_iter_next(struct ui_list_iter* inout_iter, struct ui_list_item_desc* out_desc);

struct ui_list_layout ui_list_linear_layout(const struct ui_list_linear* linear);
struct ui_list_linear_insert ui_list_linear_insert(const struct ui_list_linear* linear, const struct ui_list_item_desc* desc, double2 position);
