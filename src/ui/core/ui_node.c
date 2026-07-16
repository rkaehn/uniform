#include "ui_core.h"
#include "ui_text.h"

struct ui_node_backing* ui_node_create(struct ui* ui) {
    struct ui_node_pool* pool = &ui->node_pool;
    if (cr_unlikely(pool->stack_idx == cr_count(pool->items))) {
        uint32_t roots = 0;
        for (uint32_t i = 0; i < cr_count(pool->items); i++) {
            roots += pool->items[i].parent_idx == ui_idx_invalid ? 1u : 0u;
        }
        cr_check(0, "the node pool is exhausted: %u nodes are alive, %u of them roots; release roots that are no longer needed", (uint32_t)cr_count(pool->items), roots);
    }
    struct ui_node_backing* node = ui_node_pool_alloc(pool);

    node->parent_idx = ui_idx_invalid;
    node->first_child_idx = ui_idx_invalid;
    node->last_child_idx = ui_idx_invalid;
    node->next_idx = ui_idx_invalid;
    node->prev_idx = ui_idx_invalid;
    node->layout_to_px_from_pt = 0.0;

    return node;
}

void ui_node_destroy(struct ui* ui, struct ui_node_backing* node) {
    if (node->type == ui_content_type_text) {
        ui_text_node_deinit(ui, node);
    }
    cr_free(ui->mem, (void*)node->fill.layers, cr_loc);
    if (node->type == ui_content_type_shape && node->shape.shape_type == ui_shape_type_path) {
        ui_path_deinit(&node->shape.path);
    }
    ui_node_pool_dealloc(&ui->node_pool, node);
}

void ui_node_destroy_recursive(struct ui* ui, struct ui_node_backing* node) {
    struct ui_node_backing* child_node = ui_node_get_child(ui, node, 0);

    while (child_node != NULL) {
        struct ui_node_backing* next_node = ui_node_get_next(ui, child_node);
        ui_node_destroy_recursive(ui, child_node);
        child_node = next_node;
    }

    ui_node_destroy(ui, node);
}

struct ui_node_backing* ui_node_get_prev(struct ui* ui, struct ui_node_backing* node) {
    uint32_t idx = node->prev_idx;
    return idx != ui_idx_invalid ? &ui->node_pool.items[idx] : NULL;
}

struct ui_node_backing* ui_node_get_next(struct ui* ui, struct ui_node_backing* node) {
    uint32_t idx = node->next_idx;
    return idx != ui_idx_invalid ? &ui->node_pool.items[idx] : NULL;
}

struct ui_node_backing* ui_node_get_parent(struct ui* ui, struct ui_node_backing* node) {
    uint32_t idx = node->parent_idx;
    return idx != ui_idx_invalid ? &ui->node_pool.items[idx] : NULL;
}

struct ui_node_backing* ui_node_get_child(struct ui* ui, struct ui_node_backing* node, uint32_t child_index_in_parent) {
    uint32_t idx = node->first_child_idx;
    if (idx == ui_idx_invalid) {
        return NULL;
    }

    struct ui_node_backing* child_node = &ui->node_pool.items[idx];

    for (uint32_t i = 0; i < child_index_in_parent; i++) {
        idx = child_node->next_idx;
        if (idx == ui_idx_invalid) {
            return NULL;
        }
        child_node = &ui->node_pool.items[idx];
    }

    return child_node;
}

struct ui_node_backing* ui_node_get_last_child(struct ui* ui, struct ui_node_backing* node) {
    uint32_t idx = node->last_child_idx;
    if (idx == ui_idx_invalid) {
        return NULL;
    }

    return &ui->node_pool.items[idx];
}

struct ui_node_backing* ui_node_get_after(struct ui* ui, struct ui_node_backing* node, int32_t* inout_level) {
    struct ui_node_backing* next_node;

    if ((next_node = ui_node_get_child(ui, node, 0))) {
        (*inout_level)++;
        return next_node;
    }

    while (1) {
        if ((next_node = ui_node_get_next(ui, node))) {
            return next_node;
        }
        if ((node = ui_node_get_parent(ui, node)) == NULL) {
            return NULL;
        }
        (*inout_level)--;
    }
}

void ui_node_add_child(struct ui* ui, struct ui_node_backing* node, struct ui_node_backing* child_node) {
    assert(child_node->parent_idx == ui_idx_invalid);
    assert(child_node->next_idx == ui_idx_invalid);
    assert(child_node->prev_idx == ui_idx_invalid);

    child_node->parent_idx = ui_node_get_idx(node->handle);

    if (node->first_child_idx == ui_idx_invalid) {
        node->first_child_idx = ui_node_get_idx(child_node->handle);
        node->last_child_idx = ui_node_get_idx(child_node->handle);
    } else {
        uint32_t prev_idx = node->last_child_idx;
        struct ui_node_backing* prev_node = &ui->node_pool.items[prev_idx];

        prev_node->next_idx = ui_node_get_idx(child_node->handle);
        child_node->prev_idx = prev_idx;
        node->last_child_idx = ui_node_get_idx(child_node->handle);
    }
}

void ui_node_remove_from_parent(struct ui* ui, struct ui_node_backing* node) {
    assert(node->parent_idx != ui_idx_invalid);

    struct ui_node_backing* parent_node = &ui->node_pool.items[node->parent_idx];

    if (node->prev_idx == ui_idx_invalid) {
        parent_node->first_child_idx = node->next_idx;
    } else {
        struct ui_node_backing* prev_node = &ui->node_pool.items[node->prev_idx];
        prev_node->next_idx = node->next_idx;
    }
    if (node->next_idx != ui_idx_invalid) {
        struct ui_node_backing* next_node = &ui->node_pool.items[node->next_idx];
        next_node->prev_idx = node->prev_idx;
    } else {
        parent_node->last_child_idx = node->prev_idx;
    }
    node->parent_idx = ui_idx_invalid;
    node->next_idx = ui_idx_invalid;
    node->prev_idx = ui_idx_invalid;
}
