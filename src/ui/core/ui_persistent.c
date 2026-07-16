#include "ui_core.h"
#include "ui_render.h"
#include "ui_persistent.h"
#include "ui_shelf.h"

struct ui_persistent_page {
    struct gpu_texture* texture;
    struct gpu_bind_group* bind_groups_by_nearest[2];
    struct ui_shelf shelf;
};

#define cr_arr ui_arr_persistent_page
#define cr_arr_item struct ui_persistent_page
#include "cr_arr.h"

struct ui_persistent {
    struct cr_mem* mem;
    uint32_t page_size;
    struct ui_arr_persistent_page* formats[ui_persistent_format_count];
};

struct ui_persistent* ui_persistent_create(struct cr_mem* mem, uint32_t page_size) {
    assert(popcount_uint32(page_size) == 1);
    assert(page_size <= 1u << 15);

    struct ui_persistent* persistent = cr_malloc(mem, sizeof(*persistent), 0, cr_loc);

    memset(persistent, 0, sizeof(*persistent));
    persistent->mem = mem;
    persistent->page_size = page_size;
    for (uint32_t f = 0; f < ui_persistent_format_count; f++) {
        persistent->formats[f] = ui_arr_persistent_page_create(mem);
    }

    return persistent;
}

void ui_persistent_destroy(struct ui* ui) {
    struct ui_persistent* persistent = ui->persistent;

    for (uint32_t f = 0; f < ui_persistent_format_count; f++) {
        for (uint32_t p = 0; p < ui_arr_persistent_page_get_count(persistent->formats[f]); p++) {
            struct ui_persistent_page* page = ui_arr_persistent_page_get_ptr(persistent->formats[f], p);

            for (uint32_t nearest = 0; nearest < 2; nearest++) {
                if (page->bind_groups_by_nearest[nearest]) {
                    gpu_bind_group_destroy(ui->device, page->bind_groups_by_nearest[nearest]);
                }
            }
            gpu_texture_destroy(ui->device, page->texture);
            ui_shelf_deinit(&page->shelf);
        }
        ui_arr_persistent_page_destroy(persistent->formats[f]);
    }

    cr_free(persistent->mem, persistent, cr_loc);
}

uint32_t ui_persistent_page_size(struct ui* ui) {
    return ui->persistent->page_size;
}

__attribute__((always_inline))
static struct ui_persistent_page* ui_persistent_get_page(struct ui* ui, struct ui_persistent_slice slice) {
    assert(slice.format < ui_persistent_format_count);
    return ui_arr_persistent_page_get_ptr(ui->persistent->formats[slice.format], slice.page);
}

uint32_t ui_persistent_page_count(struct ui* ui, enum gpu_format format) {
    assert((uint32_t)format < ui_persistent_format_count);
    return ui_arr_persistent_page_get_count(ui->persistent->formats[format]);
}

bool ui_persistent_alloc(struct ui* ui, enum gpu_format format, uint32_t width, uint32_t height, bool grow, struct ui_persistent_slice* out_slice) {
    struct ui_persistent* persistent = ui->persistent;

    assert((uint32_t)format < ui_persistent_format_count);

    if (width == 0 || height == 0 || width > persistent->page_size || height > persistent->page_size) {
        return false;
    }

    uint16_t x;
    uint16_t y;
    uint32_t p = 0;

    for (; p < ui_arr_persistent_page_get_count(persistent->formats[format]); p++) {
        if (ui_shelf_alloc(&ui_arr_persistent_page_get_ptr(persistent->formats[format], p)->shelf, width, height, &x, &y)) {
            goto found;
        }
    }

    if (grow == false) {
        return false;
    }

    assert(ui_arr_persistent_page_get_count(persistent->formats[format]) < 256);

    struct ui_persistent_page* page = ui_arr_persistent_page_append_ptr(persistent->formats[format], 1);

    page->texture = gpu_texture_create(ui->device, &(struct gpu_texture_desc) {
        .label = "ui persistent page",
        .format = format,
        .size = make_uint2(persistent->page_size, persistent->page_size),
        .usage = gpu_texture_usage_sample | gpu_texture_usage_upload,
    });
    page->bind_groups_by_nearest[0] = NULL;
    page->bind_groups_by_nearest[1] = NULL;
    ui_shelf_init(&page->shelf, persistent->mem, persistent->page_size, persistent->page_size);

    bool ok = ui_shelf_alloc(&page->shelf, width, height, &x, &y);
    assert(ok);

found:
    out_slice->format = (uint8_t)format;
    out_slice->page = (uint8_t)p;
    out_slice->x = x;
    out_slice->y = y;
    out_slice->w = (uint16_t)width;
    out_slice->h = (uint16_t)height;
    return true;
}

void ui_persistent_free(struct ui* ui, struct ui_persistent_slice slice) {
    struct ui_persistent_page* page = ui_persistent_get_page(ui, slice);

    ui_shelf_free(&page->shelf, slice.x, slice.y, slice.w, slice.h);
}

struct gpu_texture* ui_persistent_texture(struct ui* ui, struct ui_persistent_slice slice) {
    return ui_persistent_get_page(ui, slice)->texture;
}

struct gpu_bind_group* ui_persistent_bind_group(struct ui* ui, struct ui_persistent_slice slice, bool nearest) {
    struct ui_persistent_page* page = ui_persistent_get_page(ui, slice);
    struct gpu_bind_group** slot = &page->bind_groups_by_nearest[nearest];

    if (*slot == NULL) {
        *slot = ui_render_texture_bind_group(ui, page->texture, false, nearest);
    }

    return *slot;
}

void ui_persistent_write(struct ui* ui, struct ui_persistent_slice slice, const void* raw, uint32_t bytes_per_row) {
    struct ui_persistent_page* page = ui_persistent_get_page(ui, slice);

    gpu_texture_write(ui->device, page->texture, slice.x, slice.y, slice.w, slice.h, raw, bytes_per_row);
}
