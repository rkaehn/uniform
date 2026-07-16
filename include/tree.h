#pragma once

#include "cr.h"

#define tree_node_header_size 64
#define tree_node_align 16
#define tree_node_size(content_size) (tree_node_header_size + (((content_size) + (tree_node_align - 1)) & ~(size_t)(tree_node_align - 1)))
#define tree_node_at(base, index, node_size) ((struct tree_node*)((char*)(base) + (index) * (node_size)))

enum cr_closed tree_selection_mode {
    tree_selection_mode_add,
    tree_selection_mode_subtract,
    tree_selection_mode_replace,
};

struct tree_node;
struct tree;

struct tree_loc {
    uint32_t raw;
};

struct tree_loc_2 {
    struct tree_loc src;
    struct tree_loc dst;
};

struct tree_insertion {
    uint32_t index;
    uint32_t level;
};

struct tree_removal {
    uint32_t index;
};

struct tree_cursor_info {
    bool is_active;
    bool is_bottom;
    uint32_t level;
    uint32_t row;
};

struct tree_iter {
    struct tree* tree;
    uint32_t index;
};

struct tree_loc tree_loc_make_index_level(uint32_t index, uint32_t level);
struct tree_loc tree_loc_make_index(uint32_t index);
uint32_t tree_loc_get_index(struct tree_loc);
uint32_t tree_loc_get_level(struct tree_loc);
struct tree_loc_2 tree_loc_2_make(uint32_t src_index, uint32_t src_level, uint32_t dst_index, uint32_t dst_level);

struct tree_node_ref* tree_node_ref_retain(struct tree* tree, uint32_t index, uint32_t owner);
void tree_node_ref_release(struct tree_node_ref* ref, uint32_t owner);
uint32_t tree_node_ref_get_index(struct tree_node_ref* ref);
struct tree_node* tree_node_ref_ptr(struct tree_node_ref* ref);

void tree_node_init(struct tree_node* node);

uint32_t tree_node_get_first_child_index(struct tree_node* node);
uint32_t tree_node_get_parent_index(struct tree_node* node);
uint32_t tree_node_get_next_index(struct tree_node* node);
uint32_t tree_node_get_prev_index(struct tree_node* node);

struct tree_loc tree_node_get_loc(struct tree_node* node);
uint32_t tree_node_get_index(struct tree_node* node);
uint32_t tree_node_get_level(struct tree_node* node);
bool tree_node_get_is_selected(struct tree_node* node);

void* tree_node_get_content(struct tree_node* node);

struct tree* tree_create(size_t content_size);
void tree_destroy(struct tree* tree);

void tree_set_on_move(struct tree* tree, void (*on_move)(const struct tree_loc* adds, uint32_t add_count, const struct tree_loc* deletes, uint32_t delete_count, const struct tree_loc_2* moves, uint32_t move_count, const struct tree_node* nodes, void* user), void* user);

uint32_t tree_get_node_count(struct tree* tree);
struct tree_node* tree_get_node(struct tree* tree, uint32_t index);
uint32_t tree_get_next_index(struct tree* tree, uint32_t index);

void tree_remove(struct tree* tree, struct tree_removal* removals, uint32_t removal_count, bool adjust_capacity);
void tree_insert(struct tree* tree, struct tree_insertion* insertions, uint32_t insertion_count, const struct tree_node* nodes);

void tree_select(struct tree* tree, const struct tree_loc* locs, uint32_t loc_count, enum tree_selection_mode mode);
void tree_move(struct tree* tree, const struct tree_loc* adds, uint32_t add_count, const struct tree_loc* deletes, uint32_t delete_count, const struct tree_loc_2* moves, uint32_t move_count, const struct tree_node* nodes);

void tree_duplicate(struct tree* tree, const uint32_t* indices, uint32_t index_count);
void tree_group(struct tree* tree, const uint32_t* indices, uint32_t index_count, uint32_t* out_group_index, bool should_select);
void tree_delete(struct tree* tree, const uint32_t* indices, uint32_t index_count);
void tree_add(struct tree* tree, uint32_t index, const struct tree_node* nodes, uint32_t node_count, bool should_select);

bool tree_update_drag(struct tree* tree, const uint32_t* indices, uint32_t index_count, bool is_target_bottom, uint32_t target_index, uint32_t target_level, struct tree_cursor_info* out_cursor_info, bool (*is_group)(struct tree_node* node, void* user), void* is_group_user);
void tree_end_drag(struct tree* tree, bool apply);

void tree_print(struct tree* tree);

struct tree_iter tree_iter(struct tree* tree);
bool tree_iter_next(struct tree_iter* inout_iter, struct tree_node** out_node_ptr, uint32_t* out_index);
