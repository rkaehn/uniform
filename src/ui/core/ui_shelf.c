#include "ui_shelf.h"

#define ui_shelf_none 0xffffu

__attribute__((always_inline))
static uint32_t ui_shelf_buddy_slot(const struct ui_shelf_buddy* buddy, uint32_t order, uint32_t index) {
    return (uint32_t)buddy->bases[order] + index;
}

__attribute__((always_inline))
static bool ui_shelf_buddy_is_free(const struct ui_shelf_buddy* buddy, uint32_t order, uint32_t index) {
    uint32_t slot = ui_shelf_buddy_slot(buddy, order, index);
    return (buddy->free_bits[slot >> 5] >> (slot & 31)) & 1u;
}

static void ui_shelf_buddy_push(struct ui_shelf_buddy* buddy, uint32_t order, uint32_t index) {
    uint32_t slot = ui_shelf_buddy_slot(buddy, order, index);

    assert(!ui_shelf_buddy_is_free(buddy, order, index));

    buddy->prev[slot] = ui_shelf_none;
    buddy->next[slot] = buddy->heads[order];
    if (buddy->heads[order] != ui_shelf_none) {
        buddy->prev[ui_shelf_buddy_slot(buddy, order, buddy->heads[order])] = (uint16_t)index;
    }
    buddy->heads[order] = (uint16_t)index;
    buddy->free_bits[slot >> 5] |= 1u << (slot & 31);
    buddy->free_mask |= 1u << order;
}

static void ui_shelf_buddy_remove(struct ui_shelf_buddy* buddy, uint32_t order, uint32_t index) {
    uint32_t slot = ui_shelf_buddy_slot(buddy, order, index);

    assert(ui_shelf_buddy_is_free(buddy, order, index));

    uint16_t next = buddy->next[slot];
    uint16_t prev = buddy->prev[slot];

    if (prev != ui_shelf_none) {
        buddy->next[ui_shelf_buddy_slot(buddy, order, prev)] = next;
    } else {
        buddy->heads[order] = next;
    }
    if (next != ui_shelf_none) {
        buddy->prev[ui_shelf_buddy_slot(buddy, order, next)] = prev;
    }

    buddy->free_bits[slot >> 5] &= ~(1u << (slot & 31));
    if (buddy->heads[order] == ui_shelf_none) {
        buddy->free_mask &= ~(1u << order);
    }
}

static void ui_shelf_buddy_init(struct ui_shelf_buddy* buddy, struct cr_mem* mem, uint32_t leaf_count) {
    assert(popcount_uint32(leaf_count) == 1);

    uint32_t order_count = (uint32_t)ctz_uint32(leaf_count) + 1;
    assert(order_count <= ui_shelf_order_count);

    uint32_t slot_count = 2 * leaf_count - 1;
    uint32_t word_count = (slot_count + 31) / 32;
    uint8_t* raw = cr_malloc(mem, (uint64_t)slot_count * 4 + (uint64_t)word_count * 4, 0, cr_loc);

    memset(buddy, 0, sizeof(*buddy));
    buddy->leaf_count = leaf_count;
    buddy->order_count = order_count;
    buddy->next = (uint16_t*)raw;
    buddy->prev = (uint16_t*)(raw + slot_count * 2);
    buddy->free_bits = (uint32_t*)(raw + slot_count * 4);
    memset(buddy->free_bits, 0, word_count * 4);

    uint32_t base = 0;
    for (uint32_t o = 0; o < order_count; o++) {
        buddy->bases[o] = (uint16_t)base;
        buddy->heads[o] = ui_shelf_none;
        base += leaf_count >> o;
    }

    ui_shelf_buddy_push(buddy, order_count - 1, 0);
}

static void ui_shelf_buddy_deinit(struct ui_shelf_buddy* buddy, struct cr_mem* mem) {
    cr_free(mem, buddy->next, cr_loc);
}

static uint32_t ui_shelf_buddy_alloc(struct ui_shelf_buddy* buddy, uint32_t order) {
    uint32_t candidates = buddy->free_mask >> order;
    if (candidates == 0) {
        return uint32_max;
    }

    uint32_t q = order + (uint32_t)ctz_uint32(candidates);
    uint32_t index = buddy->heads[q];
    ui_shelf_buddy_remove(buddy, q, index);

    while (q > order) {
        q--;
        index <<= 1;
        ui_shelf_buddy_push(buddy, q, index | 1);
    }

    return index << order;
}

static void ui_shelf_buddy_free(struct ui_shelf_buddy* buddy, uint32_t leaf_offset, uint32_t order) {
    uint32_t index = leaf_offset >> order;

    while (order + 1 < buddy->order_count && ui_shelf_buddy_is_free(buddy, order, index ^ 1)) {
        ui_shelf_buddy_remove(buddy, order, index ^ 1);
        index >>= 1;
        order++;
    }

    ui_shelf_buddy_push(buddy, order, index);
}

void ui_shelf_init(struct ui_shelf* shelf, struct cr_mem* mem, uint32_t width_px, uint32_t height_px) {
    assert(popcount_uint32(width_px) == 1);
    assert(popcount_uint32(height_px) == 1);
    assert(width_px >= ui_shelf_leaf_px && height_px >= ui_shelf_leaf_px);
    assert(width_px / ui_shelf_leaf_px <= 1u << (ui_shelf_order_count - 1));
    assert(height_px / ui_shelf_leaf_px <= 1u << (ui_shelf_order_count - 1));

    memset(shelf, 0, sizeof(*shelf));
    shelf->mem = mem;
    shelf->width_px = width_px;
    shelf->height_px = height_px;
    shelf->width_leaves = width_px / ui_shelf_leaf_px;
    shelf->height_leaves = height_px / ui_shelf_leaf_px;

    ui_shelf_buddy_init(&shelf->heights, mem, shelf->height_leaves);
    shelf->rows = cr_malloc(mem, (uint64_t)shelf->height_leaves * sizeof(*shelf->rows), 0, cr_loc);

    for (uint32_t o = 0; o < ui_shelf_order_count; o++) {
        shelf->row_heads[o] = ui_shelf_none;
    }
}

void ui_shelf_deinit(struct ui_shelf* shelf) {
    for (uint32_t o = 0; o < ui_shelf_order_count; o++) {
        for (uint16_t r = shelf->row_heads[o]; r != ui_shelf_none; r = shelf->rows[r].next) {
            ui_shelf_buddy_deinit(&shelf->rows[r].widths, shelf->mem);
        }
    }
    ui_shelf_buddy_deinit(&shelf->heights, shelf->mem);
    cr_free(shelf->mem, shelf->rows, cr_loc);
}

bool ui_shelf_alloc(struct ui_shelf* shelf, uint32_t width, uint32_t height, uint16_t* out_x, uint16_t* out_y) {
    if (width == 0 || height == 0 || width > shelf->width_px || height > shelf->height_px) {
        return false;
    }

    uint32_t w_order = ui_shelf_order_for_px(width);
    uint32_t h_order = ui_shelf_order_for_px(height);

    for (uint16_t r = shelf->row_heads[h_order]; r != ui_shelf_none; r = shelf->rows[r].next) {
        struct ui_shelf_row* row = &shelf->rows[r];

        if ((row->widths.free_mask >> w_order) != 0) {
            uint32_t x_leaf = ui_shelf_buddy_alloc(&row->widths, w_order);
            assert(x_leaf != uint32_max);
            row->span_count++;
            shelf->span_count++;
            *out_x = (uint16_t)(x_leaf * ui_shelf_leaf_px);
            *out_y = (uint16_t)(r * ui_shelf_leaf_px);
            return true;
        }
    }

    uint32_t y_leaf = ui_shelf_buddy_alloc(&shelf->heights, h_order);
    if (y_leaf == uint32_max) {
        return false;
    }

    struct ui_shelf_row* row = &shelf->rows[y_leaf];
    row->h_order = (uint16_t)h_order;
    row->span_count = 1;
    row->prev = ui_shelf_none;
    row->next = shelf->row_heads[h_order];
    ui_shelf_buddy_init(&row->widths, shelf->mem, shelf->width_leaves);

    if (row->next != ui_shelf_none) {
        shelf->rows[row->next].prev = (uint16_t)y_leaf;
    }
    shelf->row_heads[h_order] = (uint16_t)y_leaf;

    uint32_t x_leaf = ui_shelf_buddy_alloc(&row->widths, w_order);
    assert(x_leaf != uint32_max);

    shelf->span_count++;
    *out_x = (uint16_t)(x_leaf * ui_shelf_leaf_px);
    *out_y = (uint16_t)(y_leaf * ui_shelf_leaf_px);
    return true;
}

void ui_shelf_free(struct ui_shelf* shelf, uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    assert(x < shelf->width_px && y < shelf->height_px);
    assert(width > 0 && width <= shelf->width_px && height > 0 && height <= shelf->height_px);

    uint32_t w_order = ui_shelf_order_for_px(width);
    uint32_t h_order = ui_shelf_order_for_px(height);
    uint32_t y_leaf = y / ui_shelf_leaf_px;

    struct ui_shelf_row* row = &shelf->rows[y_leaf];
    assert(row->h_order == h_order);
    assert(row->span_count > 0);
    assert(shelf->span_count > 0);

    ui_shelf_buddy_free(&row->widths, x / ui_shelf_leaf_px, w_order);
    row->span_count--;
    shelf->span_count--;

    if (row->span_count == 0) {
        if (row->prev != ui_shelf_none) {
            shelf->rows[row->prev].next = row->next;
        } else {
            shelf->row_heads[h_order] = row->next;
        }
        if (row->next != ui_shelf_none) {
            shelf->rows[row->next].prev = row->prev;
        }

        ui_shelf_buddy_deinit(&row->widths, shelf->mem);
        ui_shelf_buddy_free(&shelf->heights, y_leaf, h_order);
    }
}
