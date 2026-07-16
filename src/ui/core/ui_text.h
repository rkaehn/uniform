#pragma once

#include "cr.h"
#include "ui.h"

struct ui;
struct ui_node_backing;
struct ui_mod_text;

struct ui_glyph_bitmap {
    int2 offset;
    uint2 size;
    uint32_t bytes_per_row;
    const uint8_t* raw;
};

struct ui_glyph_bitmap ui_font_rasterize_glyph(struct ui_font* font, uint32_t glyph_id, uint32_t size_266_px, double weight, double opsz, uint32_t phase_x_64);

void ui_text_node_init(struct ui* ui, struct ui_node_backing* node, const struct ui_str* str);
void ui_text_node_deinit(struct ui* ui, struct ui_node_backing* node);
struct cr_rect_int32 ui_text_ink_rect_px(const struct ui_node_backing* node, struct cr_rect_int32 rect_px, double to_px_from_pt);
int2 ui_text_measure(struct ui* ui, struct ui_node_backing* node, int32_t width_266);
void ui_text_draw(struct ui* ui, struct ui_node_backing* node, double to_px_from_pt, int2 origin_266_px, int32_t width_266, struct cr_rect_int32 clip_px, uint64_t serial);

struct ui_mod_text* ui_mod_text_create(struct cr_mem* mem);
void ui_mod_text_destroy(struct ui_mod_text* mod);

uint64_t ui_text_unused_bytes_(void);
