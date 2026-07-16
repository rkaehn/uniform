#pragma once

#include "cr.h"
#include "gpu.h"

struct ui;
struct ui_persistent;

#define ui_persistent_format_count 4
#define ui_persistent_border_px 1u

struct ui_persistent_slice {
    uint8_t format;
    uint8_t page;
    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h;
};

struct ui_persistent* ui_persistent_create(struct cr_mem* mem, uint32_t page_size);
void ui_persistent_destroy(struct ui* ui);
uint32_t ui_persistent_page_size(struct ui* ui);
uint32_t ui_persistent_page_count(struct ui* ui, enum gpu_format format);
bool ui_persistent_alloc(struct ui* ui, enum gpu_format format, uint32_t width, uint32_t height, bool grow, struct ui_persistent_slice* out_slice);
void ui_persistent_free(struct ui* ui, struct ui_persistent_slice slice);
struct gpu_texture* ui_persistent_texture(struct ui* ui, struct ui_persistent_slice slice);
struct gpu_bind_group* ui_persistent_bind_group(struct ui* ui, struct ui_persistent_slice slice, bool nearest);
void ui_persistent_write(struct ui* ui, struct ui_persistent_slice slice, const void* raw, uint32_t bytes_per_row);
