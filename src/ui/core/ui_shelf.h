#pragma once

#include "cr.h"

#define ui_shelf_leaf_px 8u
#define ui_shelf_order_count 12

struct ui_shelf_buddy {
    uint32_t leaf_count;
    uint32_t order_count;
    uint32_t free_mask;
    uint16_t heads[ui_shelf_order_count];
    uint16_t bases[ui_shelf_order_count];
    uint16_t* next;
    uint16_t* prev;
    uint32_t* free_bits;
};

struct ui_shelf_row {
    uint16_t prev;
    uint16_t next;
    uint16_t h_order;
    uint16_t span_count;
    struct ui_shelf_buddy widths;
};

struct ui_shelf {
    struct cr_mem* mem;
    uint32_t width_px;
    uint32_t height_px;
    uint32_t width_leaves;
    uint32_t height_leaves;
    uint32_t span_count;
    struct ui_shelf_buddy heights;
    struct ui_shelf_row* rows;
    uint16_t row_heads[ui_shelf_order_count];
};

cr_hdef __attribute__((always_inline))
bool ui_shelf_is_empty(const struct ui_shelf* shelf) {
    return shelf->span_count == 0;
}

cr_hdef __attribute__((always_inline))
uint32_t ui_shelf_order_for_px(uint32_t px) {
    uint32_t leaves = next_po2_uint32(max_uint32(px, ui_shelf_leaf_px)) / ui_shelf_leaf_px;
    return (uint32_t)ctz_uint32(leaves);
}

void ui_shelf_init(struct ui_shelf* shelf, struct cr_mem* mem, uint32_t width_px, uint32_t height_px);
void ui_shelf_deinit(struct ui_shelf* shelf);
bool ui_shelf_alloc(struct ui_shelf* shelf, uint32_t width, uint32_t height, uint16_t* out_x, uint16_t* out_y);
void ui_shelf_free(struct ui_shelf* shelf, uint32_t x, uint32_t y, uint32_t width, uint32_t height);

