#include "ui_core.h"
#include "ui_atlas.h"
#include "ui_persistent.h"
#include "ui_render.h"
#include "ui_text.h"

#define ui_graphic_capacity 1024
#define ui_atlas_none uint32_max
#define ui_atlas_page_count_max 8u
#define ui_atlas_glyph_bytes_max (8u << 20)

cr_hdef __attribute__((always_inline))
bool ui_glyph_key_is_equal(struct ui_glyph_key a, struct ui_glyph_key b) {
    static_assert(sizeof(struct ui_glyph_key) == 6 * sizeof(uint32_t), "no padding");
    return memcmp(&a, &b, sizeof(a)) == 0;
}

cr_hdef __attribute__((always_inline))
uint64_t ui_glyph_key_hash(struct ui_glyph_key key) {
    return cr_hash(&key, sizeof(key), 0);
}

#define cr_dict ui_dict_glyph
#define cr_dict_key struct ui_glyph_key
#define cr_dict_value uint32_t
#define cr_dict_is_equal ui_glyph_key_is_equal
#define cr_dict_hash ui_glyph_key_hash
#include "cr_dict.h"

struct ui_atlas_slot {
    struct ui_glyph_key key;
    struct ui_glyph glyph;
    uint64_t serial;
    uint32_t bytes;
    uint32_t prev;
    uint32_t next;
    struct gpu_texture* texture;
    struct ui_persistent_slice slice;
};

#define cr_arr ui_arr_atlas_slot
#define cr_arr_item struct ui_atlas_slot
#include "cr_arr.h"

#define cr_pool ui_graphic_pool
#define cr_pool_item struct ui_graphic
#define cr_pool_capacity ui_graphic_capacity
#define cr_pool_handle ui_graphic_handle
#include "cr_pool.h"

struct ui_atlas {
    struct cr_mem* mem;
    struct ui_dict_glyph* glyphs;
    struct ui_arr_atlas_slot* slots;
    uint32_t head;
    uint32_t tail;
    uint32_t free;
    uint64_t bytes;
    uint64_t misses;
    struct ui_graphic_pool pool;
};

struct ui_atlas* ui_atlas_create(struct cr_mem* mem) {
    struct ui_atlas* atlas = cr_malloc(mem, sizeof(*atlas), 0, cr_loc);

    memset(atlas, 0, sizeof(*atlas));
    atlas->mem = mem;
    atlas->glyphs = ui_dict_glyph_create(mem);
    atlas->slots = ui_arr_atlas_slot_create(mem);
    atlas->head = ui_atlas_none;
    atlas->tail = ui_atlas_none;
    atlas->free = ui_atlas_none;
    ui_graphic_pool_init(&atlas->pool);

    return atlas;
}

static struct ui_atlas_slot* ui_atlas_slot(struct ui_atlas* atlas, uint32_t index) {
    return ui_arr_atlas_slot_get_ptr(atlas->slots, index);
}

void ui_atlas_destroy(struct ui* ui) {
    struct ui_atlas* atlas = ui->atlas;

    for (uint32_t index = atlas->head; index != ui_atlas_none; index = ui_atlas_slot(atlas, index)->next) {
        struct ui_atlas_slot* slot = ui_atlas_slot(atlas, index);

        if (slot->texture != NULL) {
            gpu_bind_group_destroy(ui->device, slot->glyph.bind_group);
            gpu_texture_destroy(ui->device, slot->texture);
        }
    }

    ui_dict_glyph_destroy(atlas->glyphs);
    ui_arr_atlas_slot_destroy(atlas->slots);
    cr_free(atlas->mem, atlas, cr_loc);
}

static void ui_atlas_unlink(struct ui_atlas* atlas, uint32_t index) {
    struct ui_atlas_slot* slot = ui_atlas_slot(atlas, index);

    if (slot->prev != ui_atlas_none) {
        ui_atlas_slot(atlas, slot->prev)->next = slot->next;
    } else {
        atlas->head = slot->next;
    }
    if (slot->next != ui_atlas_none) {
        ui_atlas_slot(atlas, slot->next)->prev = slot->prev;
    } else {
        atlas->tail = slot->prev;
    }
}

static void ui_atlas_link_head(struct ui_atlas* atlas, uint32_t index) {
    struct ui_atlas_slot* slot = ui_atlas_slot(atlas, index);

    slot->prev = ui_atlas_none;
    slot->next = atlas->head;
    if (atlas->head != ui_atlas_none) {
        ui_atlas_slot(atlas, atlas->head)->prev = index;
    } else {
        atlas->tail = index;
    }
    atlas->head = index;
}

static uint32_t ui_atlas_take_free_slot(struct ui_atlas* atlas) {
    uint32_t index = atlas->free;

    if (index == ui_atlas_none) {
        index = ui_arr_atlas_slot_get_count(atlas->slots);
        ui_arr_atlas_slot_append_ptr(atlas->slots, 1);
    } else {
        atlas->free = ui_atlas_slot(atlas, index)->next;
    }

    return index;
}

static bool ui_atlas_evict_lru(struct ui* ui) {
    struct ui_atlas* atlas = ui->atlas;
    uint32_t index = atlas->tail;

    if (index == ui_atlas_none || ui_atlas_slot(atlas, index)->serial >= gpu_device_get_oldest_incomplete_serial(ui->device)) {
        return false;
    }

    struct ui_atlas_slot* slot = ui_atlas_slot(atlas, index);

    ui_dict_glyph_remove(atlas->glyphs, slot->key);
    ui_atlas_unlink(atlas, index);

    if (slot->texture != NULL) {
        gpu_bind_group_destroy(ui->device, slot->glyph.bind_group);
        gpu_texture_destroy(ui->device, slot->texture);
    } else if (slot->glyph.bind_group != NULL) {
        ui_persistent_free(ui, slot->slice);
    }

    atlas->bytes -= slot->bytes;
    *slot = (struct ui_atlas_slot) { .next = atlas->free };
    atlas->free = index;

    return true;
}

static void ui_atlas_write_padded(struct ui* ui, struct ui_persistent_slice slice, const uint8_t* src, uint32_t src_bytes_per_row, uint32_t bpp, bool wrap) {
    uint32_t dst_w = slice.w;
    uint32_t dst_h = slice.h;
    uint32_t src_w = dst_w - 2 * ui_persistent_border_px;
    uint32_t src_h = dst_h - 2 * ui_persistent_border_px;
    uint32_t dst_bpr = dst_w * bpp;

    uint8_t* tmp = cr_malloc(ui->mem, (uint64_t)dst_bpr * dst_h, 0, cr_loc);

    if (wrap == false) {
        memset(tmp, 0, (size_t)dst_bpr * dst_h);
    }

    for (uint32_t y = 0; y < dst_h; y++) {
        uint8_t* dst_row = tmp + y * dst_bpr;
        uint32_t src_y;

        if (y < ui_persistent_border_px) {
            if (wrap == false) continue;
            src_y = src_h - ui_persistent_border_px + y;
        } else if (y >= dst_h - ui_persistent_border_px) {
            if (wrap == false) continue;
            src_y = y - (dst_h - ui_persistent_border_px);
        } else {
            src_y = y - ui_persistent_border_px;
        }

        const uint8_t* src_row = src + src_y * src_bytes_per_row;

        memcpy(dst_row + ui_persistent_border_px * bpp, src_row, src_w * bpp);

        if (wrap) {
            memcpy(dst_row, src_row + (src_w - ui_persistent_border_px) * bpp, ui_persistent_border_px * bpp);
            memcpy(dst_row + (ui_persistent_border_px + src_w) * bpp, src_row, ui_persistent_border_px * bpp);
        }
    }

    ui_persistent_write(ui, slice, tmp, dst_bpr);

    cr_free(ui->mem, tmp, cr_loc);
}

static bool ui_atlas_alloc_slice(struct ui* ui, uint2 size, struct ui_persistent_slice* out_slice) {
    uint32_t width = size.x + 2 * ui_persistent_border_px;
    uint32_t height = size.y + 2 * ui_persistent_border_px;

    while (ui_persistent_alloc(ui, gpu_format_r8, width, height, false, out_slice) == false) {
        if (ui_persistent_page_count(ui, gpu_format_r8) < ui_atlas_page_count_max || ui_atlas_evict_lru(ui) == false) {
            return ui_persistent_alloc(ui, gpu_format_r8, width, height, true, out_slice);
        }
    }

    return true;
}

static void ui_atlas_upload(struct ui* ui, struct ui_atlas_slot* slot, struct ui_glyph_bitmap bitmap) {
    if (bitmap.size.x == 0 || bitmap.size.y == 0) {
        return;
    }

    slot->glyph.offset = bitmap.offset;
    slot->glyph.size = make_short2((int16_t)bitmap.size.x, (int16_t)bitmap.size.y);

    if (ui_atlas_alloc_slice(ui, bitmap.size, &slot->slice)) {
        int16_t page_size = (int16_t)ui_persistent_page_size(ui);

        ui_atlas_write_padded(ui, slot->slice, bitmap.raw, bitmap.bytes_per_row, 1, false);
        slot->glyph.bind_group = ui_persistent_bind_group(ui, slot->slice, false);
        slot->glyph.origin_uv = make_short2((int16_t)(slot->slice.x + ui_persistent_border_px), (int16_t)(slot->slice.y + ui_persistent_border_px));
        slot->glyph.total_uv = make_short2(page_size, page_size);
        slot->bytes = (uint32_t)slot->slice.w * slot->slice.h;
    } else {
        slot->texture = gpu_texture_create(ui->device, &(struct gpu_texture_desc) {
            .label = "ui atlas glyph",
            .format = gpu_format_r8,
            .size = bitmap.size,
            .usage = gpu_texture_usage_sample | gpu_texture_usage_upload,
        });

        gpu_texture_write(ui->device, slot->texture, 0, 0, bitmap.size.x, bitmap.size.y, bitmap.raw, bitmap.bytes_per_row);
        slot->glyph.bind_group = ui_render_texture_bind_group(ui, slot->texture, true, false);
        slot->glyph.total_uv = slot->glyph.size;
        slot->bytes = bitmap.size.x * bitmap.size.y;
    }
}

static uint32_t ui_atlas_add(struct ui* ui, struct ui_font* font, struct ui_glyph_key key, uint64_t serial) {
    struct ui_atlas* atlas = ui->atlas;
    uint32_t index = ui_atlas_take_free_slot(atlas);
    struct ui_atlas_slot* slot = ui_atlas_slot(atlas, index);

    *slot = (struct ui_atlas_slot) { .key = key, .serial = serial };
    ui_atlas_upload(ui, slot, ui_font_rasterize_glyph(font, key.glyph_id, key.size_266_px, (double)key.weight_10 * 10.0, (double)key.opsz_4 * 0.25, key.phase_x_64));

    atlas->bytes += slot->bytes;
    atlas->misses++;
    ui_atlas_link_head(atlas, index);
    ui_dict_glyph_set(atlas->glyphs, key, index);

    while (atlas->bytes > ui_atlas_glyph_bytes_max && ui_atlas_evict_lru(ui)) {
    }

    return index;
}

const struct ui_glyph* ui_atlas_glyph(struct ui* ui, struct ui_font* font, struct ui_glyph_key key, uint64_t serial, uint32_t* inout_hint) {
    struct ui_atlas* atlas = ui->atlas;
    uint32_t index = *inout_hint;

    assert(key.phase_x_64 < 64);

    if (index >= ui_arr_atlas_slot_get_count(atlas->slots) || ui_glyph_key_is_equal(ui_atlas_slot(atlas, index)->key, key) == false) {
        if (ui_dict_glyph_get(atlas->glyphs, key, &index) == false) {
            index = ui_atlas_add(ui, font, key, serial);
        }
        *inout_hint = index;
    }

    struct ui_atlas_slot* slot = ui_atlas_slot(atlas, index);

    if (slot->serial < serial) {
        slot->serial = serial;
        ui_atlas_unlink(atlas, index);
        ui_atlas_link_head(atlas, index);
    }

    return &slot->glyph;
}

void ui_atlas_retain_graphic(struct ui_graphic* graphic) {
    graphic->rc++;
}

void ui_atlas_release_graphic(struct ui* ui, struct ui_graphic* graphic) {
    cr_check(graphic->rc > 0, "the graphic was already destroyed");
    graphic->rc--;

    if (graphic->rc == 0) {
        if (graphic->in_persistent) {
            ui_persistent_free(ui, graphic->slice);
        }
        for (uint32_t nearest = 0; nearest < 2; nearest++) {
            for (uint32_t clamp = 0; clamp < 2; clamp++) {
                if (graphic->bind_groups_by_nearest_clamp[nearest][clamp]) {
                    gpu_bind_group_destroy(ui->device, graphic->bind_groups_by_nearest_clamp[nearest][clamp]);
                }
            }
        }
        if (graphic->texture) {
            gpu_texture_destroy(ui->device, graphic->texture);
        }

        ui_graphic_pool_dealloc(&ui->atlas->pool, graphic);
    }
}

struct gpu_bind_group* ui_graphic_bind_group(struct ui* ui, struct ui_graphic* graphic, bool clamp, bool nearest) {
    struct gpu_bind_group** slot = &graphic->bind_groups_by_nearest_clamp[nearest][clamp];

    if (*slot == NULL) {
        *slot = ui_render_texture_bind_group(ui, graphic->texture, clamp, nearest);
    }

    return *slot;
}

static uint8_t* ui_graphic_premultiply_raw(struct ui* ui, uint32_t width, uint32_t height, enum ui_pixel_format pixel_format, const uint8_t* src_raw, uint32_t src_bytes_per_row, uint32_t bpp) {
    uint8_t* raw = cr_malloc(ui->mem, (uint64_t)width * height * bpp, 16, cr_loc);

    for (uint32_t y = 0; y < height; y++) {
        const uint8_t* src_row = src_raw + y * src_bytes_per_row;
        uint8_t* dst_row = raw + y * width * bpp;

        if (pixel_format == ui_pixel_format_bgra8) {
            for (uint32_t x = 0; x < width; x++) {
                uint32_t a = src_row[x * 4 + 3];
                dst_row[x * 4 + 0] = (uint8_t)((src_row[x * 4 + 0] * a + 127) / 255);
                dst_row[x * 4 + 1] = (uint8_t)((src_row[x * 4 + 1] * a + 127) / 255);
                dst_row[x * 4 + 2] = (uint8_t)((src_row[x * 4 + 2] * a + 127) / 255);
                dst_row[x * 4 + 3] = (uint8_t)a;
            }
        } else {
            const __fp16* src_px = (const __fp16*)(const void*)src_row;
            __fp16* dst_px = (__fp16*)(void*)dst_row;
            for (uint32_t x = 0; x < width; x++) {
                float a = (float)src_px[x * 4 + 3];
                dst_px[x * 4 + 0] = (__fp16)((float)src_px[x * 4 + 0] * a);
                dst_px[x * 4 + 1] = (__fp16)((float)src_px[x * 4 + 1] * a);
                dst_px[x * 4 + 2] = (__fp16)((float)src_px[x * 4 + 2] * a);
                dst_px[x * 4 + 3] = src_px[x * 4 + 3];
            }
        }
    }

    return raw;
}

static struct ui_graphic* ui_graphic_alloc(struct ui* ui) {
    struct ui_graphic_pool* pool = &ui->atlas->pool;
    cr_check(ui_graphic_pool_get_count(pool) < cr_count(pool->items), "the graphic pool is exhausted: %u graphics are alive; destroy graphics that are no longer needed", (uint32_t)cr_count(pool->items));
    return ui_graphic_pool_alloc(pool);
}

struct ui_graphic* ui_graphic_create(uint32_t width, uint32_t height, enum ui_pixel_format pixel_format, const uint8_t* src_raw, uint32_t src_bytes_per_row, bool premultiplied) {
    cr_check(pixel_format != ui_pixel_format_invalid, "a graphic needs a pixel format");
    struct ui* ui = ui_ctx();

    cr_check(width > 0 && height > 0, "a graphic needs a size above 0");
    cr_check(src_raw != NULL, "a graphic needs pixel data");

    enum gpu_format format = pixel_format == ui_pixel_format_r8 ? gpu_format_r8 :
                             pixel_format == ui_pixel_format_rgba16f ? gpu_format_rgba16f : gpu_format_bgra8;
    uint32_t bpp = pixel_format == ui_pixel_format_r8 ? 1u :
                   pixel_format == ui_pixel_format_rgba16f ? 8u : 4u;

    uint8_t* converted = NULL;
    if (premultiplied == false && pixel_format != ui_pixel_format_r8) {
        converted = ui_graphic_premultiply_raw(ui, width, height, pixel_format, src_raw, src_bytes_per_row, bpp);
        src_raw = converted;
        src_bytes_per_row = width * bpp;
    }

    struct ui_persistent_slice slice;

    if (ui_persistent_alloc(ui, format, width + 2 * ui_persistent_border_px, height + 2 * ui_persistent_border_px, true, &slice)) {
        ui_atlas_write_padded(ui, slice, src_raw, src_bytes_per_row, bpp, true);

        if (converted != NULL) {
            cr_free(ui->mem, converted, cr_loc);
        }

        struct ui_graphic* graphic = ui_graphic_alloc(ui);

        graphic->in_persistent = true;
        graphic->slice = slice;
        graphic->size = make_uint2(width, height);
        graphic->format = format;

        ui_atlas_retain_graphic(graphic);

        return graphic;
    }

    struct gpu_texture* texture = gpu_texture_create(ui->device, &(struct gpu_texture_desc) {
        .label = "ui atlas graphic",
        .format = format,
        .size = make_uint2(width, height),
        .usage = gpu_texture_usage_sample | gpu_texture_usage_upload,
    });

    gpu_texture_write(ui->device, texture, 0, 0, width, height, src_raw, src_bytes_per_row);

    if (converted != NULL) {
        cr_free(ui->mem, converted, cr_loc);
    }

    return ui_graphic_create_texture(texture);
}

struct ui_graphic* ui_graphic_create_texture(struct gpu_texture* texture) {
    struct ui* ui = ui_ctx();

    struct ui_graphic* graphic = ui_graphic_alloc(ui);

    graphic->texture = texture;
    graphic->size = gpu_texture_get_size(texture);
    graphic->format = gpu_texture_get_format(texture);

    ui_atlas_retain_graphic(graphic);

    return graphic;
}

void ui_graphic_destroy(struct ui_graphic* graphic) {
    struct ui* ui = ui_ctx();

    ui_atlas_release_graphic(ui, graphic);
}

uint32_t ui_atlas_glyph_count_(void) {
    return ui_dict_glyph_get_count(ui_ctx()->atlas->glyphs);
}

uint64_t ui_atlas_glyph_bytes_(void) {
    return ui_ctx()->atlas->bytes;
}

uint64_t ui_atlas_glyph_misses_(void) {
    return ui_ctx()->atlas->misses;
}
