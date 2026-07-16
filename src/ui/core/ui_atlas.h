#pragma once

#include "cr.h"
#include "gpu.h"
#include "ui.h"
#include "ui_persistent.h"

struct ui;
struct ui_atlas;

#define cr_handle ui_graphic_handle
#include "cr_handle.h"

struct ui_graphic {
    struct ui_graphic_handle handle;
    struct gpu_texture* texture;
    struct gpu_bind_group* bind_groups_by_nearest_clamp[2][2];
    bool in_persistent;
    struct ui_persistent_slice slice;
    uint2 size;
    enum gpu_format format;
    uint32_t rc;
};

struct ui_glyph_key {
    uint32_t font_id;
    uint32_t glyph_id;
    uint32_t size_266_px;
    uint32_t weight_10;
    uint32_t opsz_4;
    uint32_t phase_x_64;
};

struct ui_glyph {
    struct gpu_bind_group* bind_group;
    int2 offset;
    short2 size;
    short2 origin_uv;
    short2 total_uv;
};

cr_hdef __attribute__((always_inline))
uint32_t ui_glyph_size_266_snap(uint32_t size_266_px) {
    return (size_266_px + 4) / 8 * 8;
}

cr_hdef __attribute__((always_inline))
uint32_t ui_glyph_phase_count(uint32_t size_266_px) {
    return size_266_px == 0 ? 1 : min_uint32(((34u << 6) + size_266_px - 1) / size_266_px, 5);
}

cr_hdef __attribute__((always_inline))
uint32_t ui_glyph_phase_x_64(double frac, uint32_t phase_count) {
    uint32_t phase = min_uint32((uint32_t)(frac * (double)phase_count), phase_count - 1);
    return (phase * 64 + phase_count / 2) / phase_count;
}

cr_hdef struct ui_glyph_key ui_glyph_key_make(uint32_t font_id, uint32_t size_266_px, double weight, double opsz) {
    assert(isfinite(weight) && weight >= 0.0);
    assert(isfinite(opsz) && opsz >= 0.0);
    assert(size_266_px == ui_glyph_size_266_snap(size_266_px));

    return (struct ui_glyph_key) {
        .font_id = font_id,
        .size_266_px = size_266_px,
        .weight_10 = (uint32_t)lround(weight / 10.0),
        .opsz_4 = (uint32_t)lround(opsz * 4.0),
    };
}

struct ui_atlas* ui_atlas_create(struct cr_mem* mem);
void ui_atlas_destroy(struct ui* ui);
const struct ui_glyph* ui_atlas_glyph(struct ui* ui, struct ui_font* font, struct ui_glyph_key key, uint64_t serial, uint32_t* inout_hint);

void ui_atlas_retain_graphic(struct ui_graphic* graphic);
void ui_atlas_release_graphic(struct ui* ui, struct ui_graphic* graphic);
struct gpu_bind_group* ui_graphic_bind_group(struct ui* ui, struct ui_graphic* graphic, bool clamp, bool nearest);

uint32_t ui_atlas_glyph_count_(void);
uint64_t ui_atlas_glyph_bytes_(void);
uint64_t ui_atlas_glyph_misses_(void);
