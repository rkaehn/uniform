#pragma once

#include "cr.h"

enum js_layout {
    js_layout_args_size = 256,
    js_layout_scratch_size = 8 << 20,
    js_layout_bump_size = 4 << 20,
    js_layout_dispatch_args = 16,
    js_layout_dispatch_result = 24,
    js_layout_root_slot = 31,
};

void js_arena_init(void);
double js_arena_base(void);
double js_malloc(uint32_t size);
void js_free(double ptr);
void js_memcpy(double dst, double src, uint32_t size);
uint32_t js_strlen(double ptr);
double js_mem_libc(void);
double js_cb_addr(uint32_t kind);

void js_ui_custom_draw_solid(double draw, double transform, double color, uint32_t vertex_count, double vertices);
void js_ui_custom_draw_graphic(double draw, double transform, double graphic, double tint, uint32_t vertex_count, double vertices);
