#pragma once

#include "cr.h"

#define cr_tree_node_header_size 64
#define cr_tree_node_align 16
#define cr_tree_node_size(content_size) (cr_tree_node_header_size + (((content_size) + (cr_tree_node_align - 1)) & ~(size_t)(cr_tree_node_align - 1)))
#define cr_tree_node_at(base, index, node_size) ((struct cr_tree_node*)((char*)(base) + (index) * (node_size)))

enum cr_closed cr_tree_selection_mode {
    cr_tree_selection_mode_add,
    cr_tree_selection_mode_subtract,
    cr_tree_selection_mode_replace,
};

struct cr_tree_node;
struct cr_tree;

struct cr_tree_loc {
    uint32_t raw;
};

struct cr_tree_loc_2 {
    struct cr_tree_loc src;
    struct cr_tree_loc dst;
};

struct cr_tree_insertion {
    uint32_t index;
    uint32_t level;
};

struct cr_tree_removal {
    uint32_t index;
};

struct cr_tree_cursor_info {
    bool is_active;
    bool is_bottom;
    uint32_t level;
    uint32_t row;
};

struct cr_tree_iter {
    struct cr_tree* tree;
    uint32_t index;
};

struct cr_tree_loc cr_tree_loc_make_index_level(uint32_t index, uint32_t level);
struct cr_tree_loc cr_tree_loc_make_index(uint32_t index);
uint32_t cr_tree_loc_get_index(struct cr_tree_loc);
uint32_t cr_tree_loc_get_level(struct cr_tree_loc);
struct cr_tree_loc_2 cr_tree_loc_2_make(uint32_t src_index, uint32_t src_level, uint32_t dst_index, uint32_t dst_level);

struct cr_tree_node_ref* cr_tree_node_ref_retain(struct cr_tree* tree, uint32_t index, uint32_t owner);
void cr_tree_node_ref_release(struct cr_tree_node_ref* ref, uint32_t owner);
uint32_t cr_tree_node_ref_get_index(struct cr_tree_node_ref* ref);
struct cr_tree_node* cr_tree_node_ref_ptr(struct cr_tree_node_ref* ref);

void cr_tree_node_init(struct cr_tree_node* node);

uint32_t cr_tree_node_get_first_child_index(struct cr_tree_node* node);
uint32_t cr_tree_node_get_parent_index(struct cr_tree_node* node);
uint32_t cr_tree_node_get_next_index(struct cr_tree_node* node);
uint32_t cr_tree_node_get_prev_index(struct cr_tree_node* node);

struct cr_tree_loc cr_tree_node_get_loc(struct cr_tree_node* node);
uint32_t cr_tree_node_get_index(struct cr_tree_node* node);
uint32_t cr_tree_node_get_level(struct cr_tree_node* node);
bool cr_tree_node_get_is_selected(struct cr_tree_node* node);

void* cr_tree_node_get_content(struct cr_tree_node* node);

struct cr_tree* cr_tree_create(struct cr_mem* mem, struct cr_mem* temp_mem, size_t content_size);
void cr_tree_destroy(struct cr_tree* tree);

void cr_tree_set_on_move(struct cr_tree* tree, void (*on_move)(const struct cr_tree_loc* adds, uint32_t add_count, const struct cr_tree_loc* deletes, uint32_t delete_count, const struct cr_tree_loc_2* moves, uint32_t move_count, const struct cr_tree_node* nodes, void* user), void* user);

uint32_t cr_tree_get_node_count(struct cr_tree* tree);
struct cr_tree_node* cr_tree_get_node(struct cr_tree* tree, uint32_t index);
uint32_t cr_tree_get_next_index(struct cr_tree* tree, uint32_t index);

void cr_tree_remove(struct cr_tree* tree, struct cr_tree_removal* removals, uint32_t removal_count, bool adjust_capacity);
void cr_tree_insert(struct cr_tree* tree, struct cr_tree_insertion* insertions, uint32_t insertion_count, const struct cr_tree_node* nodes);

void cr_tree_select(struct cr_tree* tree, const struct cr_tree_loc* locs, uint32_t loc_count, enum cr_tree_selection_mode mode);
void cr_tree_move(struct cr_tree* tree, const struct cr_tree_loc* adds, uint32_t add_count, const struct cr_tree_loc* deletes, uint32_t delete_count, const struct cr_tree_loc_2* moves, uint32_t move_count, const struct cr_tree_node* nodes);

void cr_tree_duplicate(struct cr_tree* tree, const uint32_t* indices, uint32_t index_count);
void cr_tree_group(struct cr_tree* tree, const uint32_t* indices, uint32_t index_count, uint32_t* out_group_index, bool should_select);
void cr_tree_delete(struct cr_tree* tree, const uint32_t* indices, uint32_t index_count);
void cr_tree_add(struct cr_tree* tree, uint32_t index, const struct cr_tree_node* nodes, uint32_t node_count, bool should_select);

bool cr_tree_update_drag(struct cr_tree* tree, const uint32_t* indices, uint32_t index_count, bool is_target_bottom, uint32_t target_index, uint32_t target_level, struct cr_tree_cursor_info* out_cursor_info, bool (*is_group)(struct cr_tree_node* node, void* user), void* is_group_user);
void cr_tree_end_drag(struct cr_tree* tree, bool apply);

void cr_tree_print(struct cr_tree* tree);

struct cr_tree_iter cr_tree_iter(struct cr_tree* tree);
bool cr_tree_iter_next(struct cr_tree_iter* inout_iter, struct cr_tree_node** out_node_ptr, uint32_t* out_index);
