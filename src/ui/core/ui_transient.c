#include "ui_core.h"
#include "ui_transient.h"
#include "ui_render.h"
#include "ui_slot.h"
#include "ui_blur.h"
#include "ui_shelf.h"

#define ui_transient_round_px 32u
#define ui_transient_max_px 16384u
#define ui_transient_default_memory_limit (512ull << 20)
#define ui_transient_max_mips (ui_blur_pyramid_max_levels + 1)

#define ui_transient_evict_idle_frames 120

#define ui_transient_page_px 2048u
#define ui_transient_region_max_px 1024u

struct ui_transient_entry {
    struct gpu_texture* texture;
    struct gpu_texture* mip_views[ui_transient_max_mips];
    struct gpu_bind_group* bind_group;
    uint2 size;
    uint8_t format;
    uint8_t mip_count;
    bool storage;
    bool reserved;
    bool region;
    uint16_t page;
    uint2 origin;
    uint2 phys;
    uint32_t idle_frames;
};

#define cr_arr ui_arr_transient_entry
#define cr_arr_item struct ui_transient_entry
#include "cr_arr.h"

struct ui_transient_page {
    struct gpu_texture* texture;
    struct gpu_texture* mip_views[ui_transient_max_mips];
    struct gpu_bind_group* bind_group;
    struct ui_shelf shelf;
    uint8_t format;
    uint8_t mip_count;
    uint8_t channel;
    uint32_t idle_frames;
};

#define cr_arr ui_arr_transient_page
#define cr_arr_item struct ui_transient_page
#include "cr_arr.h"

struct ui_transient {
    struct cr_mem* mem;
    struct gpu_texture* target_texture;
    struct gpu_bind_group* target_bind_group;
    uint2 target_dims;
    uint64_t memory_limit;
    uint64_t memory_used;
    struct ui_arr_transient_entry* entries;
    struct ui_arr_transient_page* pages;
};

static uint32_t ui_transient_usage(enum gpu_format format, bool storage) {
    uint32_t storage_usage = storage ? gpu_texture_usage_storage_write : 0u;

    switch (format) {
    case gpu_format_bgra8: return gpu_texture_usage_sample | gpu_texture_usage_render_target;
    case gpu_format_r8: return gpu_texture_usage_sample;
    case gpu_format_rgba16f: return gpu_texture_usage_sample | gpu_texture_usage_render_target | storage_usage;
    case gpu_format_r32f: return gpu_texture_usage_sample | storage_usage;
    }
    return gpu_texture_usage_sample;
}

static uint32_t ui_transient_bytes_per_px(enum gpu_format format) {
    switch (format) {
    case gpu_format_bgra8: return 4;
    case gpu_format_r8: return 1;
    case gpu_format_rgba16f: return 8;
    case gpu_format_r32f: return 4;
    }
    return 4;
}

static uint64_t ui_transient_texture_bytes(enum gpu_format format, uint2 size, uint32_t mip_count) {
    uint64_t bytes = 0;
    for (uint32_t level = 0; level < mip_count; level++) {
        uint64_t w = max_uint32(size.x >> level, 1);
        uint64_t h = max_uint32(size.y >> level, 1);
        bytes += w * h * ui_transient_bytes_per_px(format);
    }
    return bytes;
}

static uint64_t ui_transient_entry_bytes(const struct ui_transient_entry* entry) {
    return ui_transient_texture_bytes((enum gpu_format)entry->format, entry->size, entry->mip_count);
}

static uint64_t ui_transient_page_bytes(const struct ui_transient_page* page) {
    return ui_transient_texture_bytes((enum gpu_format)page->format, make_uint2(ui_transient_page_px, ui_transient_page_px), page->mip_count);
}

struct ui_transient* ui_transient_create(struct cr_mem* mem) {
    struct ui_transient* transient = cr_malloc(mem, sizeof(*transient), 0, cr_loc);

    memset(transient, 0, sizeof(*transient));
    transient->mem = mem;
    transient->memory_limit = ui_transient_default_memory_limit;
    transient->entries = ui_arr_transient_entry_create(mem);
    transient->pages = ui_arr_transient_page_create(mem);

    return transient;
}

static void ui_transient_entry_free(struct ui* ui, struct ui_transient_entry* entry) {
    for (uint32_t level = 0; level < ui_transient_max_mips; level++) {
        if (entry->mip_views[level]) {
            gpu_texture_destroy(ui->device, entry->mip_views[level]);
            entry->mip_views[level] = NULL;
        }
    }
    if (entry->bind_group) {
        gpu_bind_group_destroy(ui->device, entry->bind_group);
        entry->bind_group = NULL;
    }
    gpu_texture_destroy(ui->device, entry->texture);
    entry->texture = NULL;
}

static void ui_transient_page_free(struct ui* ui, struct ui_transient_page* page) {
    struct ui_transient* transient = ui->transient;

    assert(ui_shelf_is_empty(&page->shelf));

    for (uint32_t level = 0; level < ui_transient_max_mips; level++) {
        if (page->mip_views[level]) {
            gpu_texture_destroy(ui->device, page->mip_views[level]);
            page->mip_views[level] = NULL;
        }
    }
    if (page->bind_group) {
        gpu_bind_group_destroy(ui->device, page->bind_group);
        page->bind_group = NULL;
    }

    transient->memory_used -= ui_transient_page_bytes(page);
    ui_shelf_deinit(&page->shelf);
    gpu_texture_destroy(ui->device, page->texture);
    page->texture = NULL;
}

void ui_transient_destroy(struct ui* ui) {
    struct ui_transient* transient = ui->transient;

    for (uint32_t i = 0; i < ui_arr_transient_entry_get_count(transient->entries); i++) {
        struct ui_transient_entry* entry = ui_arr_transient_entry_get_ptr(transient->entries, i);

        if (entry->texture == NULL) {
            continue;
        }
        if (entry->region) {
            struct ui_transient_page* page = ui_arr_transient_page_get_ptr(transient->pages, entry->page);

            ui_shelf_free(&page->shelf, entry->origin.x, entry->origin.y, entry->phys.x, entry->phys.y);
        } else {
            ui_transient_entry_free(ui, entry);
        }
    }
    ui_arr_transient_entry_destroy(transient->entries);

    for (uint32_t i = 0; i < ui_arr_transient_page_get_count(transient->pages); i++) {
        struct ui_transient_page* page = ui_arr_transient_page_get_ptr(transient->pages, i);

        if (page->texture) {
            ui_transient_page_free(ui, page);
        }
    }
    ui_arr_transient_page_destroy(transient->pages);

    cr_free(transient->mem, transient, cr_loc);
}

void ui_set_transient_memory_limit(uint64_t bytes) {
    struct ui* ui = ui_ctx();

    cr_check(bytes >= ui->transient->memory_used, "the transient memory limit is below the current usage");
    ui->transient->memory_limit = bytes;
}

__attribute__((always_inline))
static struct ui_transient_entry* ui_transient_get_entry(struct ui* ui, struct ui_transient_tex tex) {
    assert(tex.index != ui_transient_index_target);
    struct ui_transient_entry* entry = ui_arr_transient_entry_get_ptr(ui->transient->entries, tex.index);
    assert(entry->texture != NULL);
    return entry;
}

__attribute__((always_inline))
static struct ui_transient_page* ui_transient_entry_page(struct ui* ui, const struct ui_transient_entry* entry) {
    assert(entry->region);
    return ui_arr_transient_page_get_ptr(ui->transient->pages, entry->page);
}

struct ui_transient_tex ui_transient_wrap_target(struct ui* ui, struct gpu_texture* texture, uint2 size) {
    assert(size.x <= uint16_max && size.y <= uint16_max);

    ui->transient->target_texture = texture;
    ui->transient->target_dims = size;

    struct ui_transient_tex tex = {
        .index = ui_transient_index_target,
        .w = (uint16_t)size.x,
        .h = (uint16_t)size.y,
    };
    return tex;
}

void ui_transient_unwrap_target(struct ui* ui, struct ui_slot* slot) {
    struct ui_transient* transient = ui->transient;

    if (transient->target_bind_group) {
        ui_slot_own(slot, (struct ui_slot_resource) { .type = ui_slot_resource_type_bind_group, .bind_group = transient->target_bind_group });
        transient->target_bind_group = NULL;
    }
    transient->target_texture = NULL;
}

static void ui_transient_evict(struct ui* ui, uint64_t needed_bytes) {
    struct ui_transient* transient = ui->transient;

    while (transient->memory_used + needed_bytes > transient->memory_limit) {
        struct ui_transient_entry* oldest_entry = NULL;
        struct ui_transient_page* oldest_page = NULL;
        uint32_t oldest_idle = 0;

        for (uint32_t i = 0; i < ui_arr_transient_entry_get_count(transient->entries); i++) {
            struct ui_transient_entry* entry = ui_arr_transient_entry_get_ptr(transient->entries, i);

            if (entry->texture == NULL || entry->region || entry->reserved || entry->idle_frames == 0) {
                continue;
            }
            if (entry->idle_frames > oldest_idle) {
                oldest_entry = entry;
                oldest_idle = entry->idle_frames;
            }
        }

        for (uint32_t i = 0; i < ui_arr_transient_page_get_count(transient->pages); i++) {
            struct ui_transient_page* page = ui_arr_transient_page_get_ptr(transient->pages, i);

            if (page->texture == NULL || ui_shelf_is_empty(&page->shelf) == false || page->idle_frames == 0) {
                continue;
            }
            if (page->idle_frames > oldest_idle) {
                oldest_entry = NULL;
                oldest_page = page;
                oldest_idle = page->idle_frames;
            }
        }

        if (oldest_entry) {
            transient->memory_used -= ui_transient_entry_bytes(oldest_entry);
            ui_transient_entry_free(ui, oldest_entry);
        } else if (oldest_page) {
            ui_transient_page_free(ui, oldest_page);
        } else {
            return;
        }
    }
}

static uint32_t ui_transient_entry_slot(struct ui_transient* transient) {
    uint32_t index;

    for (index = 0; index < ui_arr_transient_entry_get_count(transient->entries); index++) {
        if (ui_arr_transient_entry_get_ptr(transient->entries, index)->texture == NULL) {
            break;
        }
    }
    if (index == ui_arr_transient_entry_get_count(transient->entries)) {
        assert(index < 0xffffu);
        ui_arr_transient_entry_append_ptr(transient->entries, 1);
    }
    return index;
}

static uint32_t ui_transient_page_slot(struct ui_transient* transient) {
    uint32_t index;

    for (index = 0; index < ui_arr_transient_page_get_count(transient->pages); index++) {
        if (ui_arr_transient_page_get_ptr(transient->pages, index)->texture == NULL) {
            break;
        }
    }
    if (index == ui_arr_transient_page_get_count(transient->pages)) {
        assert(index < 0xffffu);
        ui_arr_transient_page_append_ptr(transient->pages, 1);
    }
    return index;
}

static void ui_transient_page_clear(struct ui* ui, struct ui_transient_page* page) {
    struct gpu_command_buffer* command_buffer = gpu_command_buffer_create(ui->device, "ui transient clear");

    for (uint32_t level = 0; level < page->mip_count; level++) {
        struct gpu_texture* target = page->texture;

        if (page->mip_count > 1) {
            if (page->mip_views[level] == NULL) {
                page->mip_views[level] = gpu_texture_create_view_mip(ui->device, page->texture, level);
            }
            target = page->mip_views[level];
        }

        struct gpu_render_pass* pass = gpu_command_buffer_begin_render(command_buffer, target, gpu_load_clear, "ui transient clear");
        gpu_render_pass_end(pass);
    }

    gpu_command_buffer_commit(command_buffer);
}

static uint32_t ui_transient_region_mips(uint2 phys, uint32_t mip_count) {
    mip_count = min_uint32(max_uint32(mip_count, 1), ui_transient_max_mips);
    while (mip_count > 1 && (min_uint32(phys.x, phys.y) >> (mip_count - 1)) == 0) {
        mip_count--;
    }
    return mip_count;
}

static bool ui_transient_reserve_region(struct ui* ui, enum gpu_format format, uint32_t width, uint32_t height, uint32_t mip_count, uint32_t channel, uint32_t* out_index) {
    struct ui_transient* transient = ui->transient;

    uint2 phys = make_uint2(next_po2_uint32(max_uint32(width + 1, ui_shelf_leaf_px)), next_po2_uint32(max_uint32(height + 1, ui_shelf_leaf_px)));

    if (phys.x > ui_transient_region_max_px || phys.y > ui_transient_region_max_px) {
        return false;
    }

    mip_count = ui_transient_region_mips(phys, mip_count);
    uint32_t page_mips = mip_count > 1 ? ui_transient_max_mips : 1;

    uint32_t page_index = uint32_max;
    uint16_t x = 0;
    uint16_t y = 0;

    for (uint32_t i = 0; i < ui_arr_transient_page_get_count(transient->pages); i++) {
        struct ui_transient_page* page = ui_arr_transient_page_get_ptr(transient->pages, i);

        if (page->texture == NULL || page->format != (uint8_t)format || page->mip_count != (uint8_t)page_mips || page->channel != (uint8_t)channel) {
            continue;
        }
        if (ui_shelf_alloc(&page->shelf, phys.x, phys.y, &x, &y)) {
            page_index = i;
            break;
        }
    }

    if (page_index == uint32_max) {
        struct ui_transient_page created = {
            .format = (uint8_t)format,
            .mip_count = (uint8_t)page_mips,
            .channel = (uint8_t)channel,
        };

        uint64_t bytes = ui_transient_page_bytes(&created);

        ui_transient_evict(ui, bytes);
        if (transient->memory_used + bytes > transient->memory_limit) {
            return false;
        }

        page_index = ui_transient_page_slot(transient);

        struct ui_transient_page* page = ui_arr_transient_page_get_ptr(transient->pages, page_index);

        *page = created;
        page->texture = gpu_texture_create(ui->device, &(struct gpu_texture_desc) {
            .label = "ui transient page",
            .format = format,
            .size = make_uint2(ui_transient_page_px, ui_transient_page_px),
            .usage = ui_transient_usage(format, true) | gpu_texture_usage_render_target,
            .mip_count = page_mips,
        });
        ui_shelf_init(&page->shelf, transient->mem, ui_transient_page_px, ui_transient_page_px);
        ui_transient_page_clear(ui, page);

        transient->memory_used += bytes;

        bool ok = ui_shelf_alloc(&page->shelf, phys.x, phys.y, &x, &y);
        assert(ok);
        (void)ok;
    }

    struct ui_transient_page* page = ui_arr_transient_page_get_ptr(transient->pages, page_index);
    page->idle_frames = 0;

    uint32_t index = ui_transient_entry_slot(transient);
    struct ui_transient_entry* entry = ui_arr_transient_entry_get_ptr(transient->entries, index);

    memset(entry, 0, sizeof(*entry));
    entry->texture = page->texture;
    entry->size = make_uint2(ui_transient_page_px, ui_transient_page_px);
    entry->format = (uint8_t)format;
    entry->mip_count = (uint8_t)mip_count;
    entry->storage = true;
    entry->region = true;
    entry->page = (uint16_t)page_index;
    entry->origin = make_uint2(x, y);
    entry->phys = phys;

    *out_index = index;
    return true;
}

static struct ui_transient_tex ui_transient_reserve_impl(struct ui* ui, enum gpu_format format, uint32_t width, uint32_t height, uint32_t mip_count, bool storage, bool dedicated, uint32_t channel) {
    struct ui_transient* transient = ui->transient;

    assert((uint32_t)format < ui_transient_format_count);
    assert(channel < ui_transient_channel_count);

    width = max_uint32(width, 1);
    height = max_uint32(height, 1);
    assert(width <= ui_transient_max_px && height <= ui_transient_max_px);

    uint32_t index = 0;

    if (storage && dedicated == false && ui_transient_reserve_region(ui, format, width, height, mip_count, channel, &index)) {
        struct ui_transient_entry* entry = ui_arr_transient_entry_get_ptr(transient->entries, index);

        entry->reserved = true;

        struct ui_transient_tex tex = {
            .index = (uint16_t)index,
            .w = (uint16_t)width,
            .h = (uint16_t)height,
        };
        return tex;
    }

    uint2 size = make_uint2(align_up_uint32(width, ui_transient_round_px), align_up_uint32(height, ui_transient_round_px));

    mip_count = min_uint32(max_uint32(mip_count, 1), ui_transient_max_mips);
    while (mip_count > 1 && (min_uint32(size.x, size.y) >> (mip_count - 1)) == 0) {
        mip_count--;
    }

    struct ui_transient_entry* entry = NULL;

    for (uint32_t i = 0; i < ui_arr_transient_entry_get_count(transient->entries); i++) {
        struct ui_transient_entry* candidate = ui_arr_transient_entry_get_ptr(transient->entries, i);

        if (candidate->texture == NULL || candidate->region || candidate->reserved) {
            continue;
        }
        if (candidate->format != (uint8_t)format || candidate->mip_count != (uint8_t)mip_count || candidate->storage != storage) {
            continue;
        }
        if (!all_int2(candidate->size == size)) {
            continue;
        }
        index = i;
        entry = candidate;
        break;
    }

    if (entry == NULL) {
        struct ui_transient_entry created = {
            .size = size,
            .format = (uint8_t)format,
            .mip_count = (uint8_t)mip_count,
            .storage = storage,
        };

        ui_transient_evict(ui, ui_transient_entry_bytes(&created));

        index = ui_transient_entry_slot(transient);
        entry = ui_arr_transient_entry_get_ptr(transient->entries, index);

        *entry = created;
        entry->texture = gpu_texture_create(ui->device, &(struct gpu_texture_desc) {
            .label = "ui transient entry",
            .format = format,
            .size = size,
            .usage = ui_transient_usage(format, storage),
            .mip_count = mip_count,
        });

        transient->memory_used += ui_transient_entry_bytes(entry);
        assert(transient->memory_used <= transient->memory_limit);
    }

    entry->reserved = true;
    entry->idle_frames = 0;

    struct ui_transient_tex tex = {
        .index = (uint16_t)index,
        .w = (uint16_t)width,
        .h = (uint16_t)height,
    };
    return tex;
}

struct ui_transient_tex ui_transient_reserve_mips(struct ui* ui, enum gpu_format format, uint32_t width, uint32_t height, uint32_t mip_count) {
    return ui_transient_reserve_impl(ui, format, width, height, mip_count, false, true, 0);
}

struct ui_transient_tex ui_transient_reserve_mips_storage(struct ui* ui, enum gpu_format format, uint32_t width, uint32_t height, uint32_t mip_count) {
    return ui_transient_reserve_impl(ui, format, width, height, mip_count, true, false, 0);
}

struct ui_transient_tex ui_transient_reserve(struct ui* ui, enum gpu_format format, uint32_t width, uint32_t height) {
    return ui_transient_reserve_impl(ui, format, width, height, 1, false, true, 0);
}

struct ui_transient_tex ui_transient_reserve_storage(struct ui* ui, enum gpu_format format, uint32_t width, uint32_t height) {
    return ui_transient_reserve_impl(ui, format, width, height, 1, true, false, 0);
}

struct ui_transient_tex ui_transient_reserve_storage_dedicated(struct ui* ui, enum gpu_format format, uint32_t width, uint32_t height) {
    return ui_transient_reserve_impl(ui, format, width, height, 1, true, true, 0);
}

struct ui_transient_tex ui_transient_reserve_storage_channel(struct ui* ui, enum gpu_format format, uint32_t width, uint32_t height, uint32_t channel) {
    return ui_transient_reserve_impl(ui, format, width, height, 1, true, false, channel);
}

void ui_transient_release(struct ui* ui, struct ui_transient_tex tex) {
    if (tex.index == ui_transient_index_target) {
        return;
    }

    struct ui_transient_entry* entry = ui_transient_get_entry(ui, tex);

    assert(entry->reserved);

    if (entry->region) {
        struct ui_transient_page* page = ui_transient_entry_page(ui, entry);

        ui_shelf_free(&page->shelf, entry->origin.x, entry->origin.y, entry->phys.x, entry->phys.y);
        memset(entry, 0, sizeof(*entry));
        return;
    }

    entry->reserved = false;
}

struct gpu_texture* ui_transient_texture(struct ui* ui, struct ui_transient_tex tex) {
    if (tex.index == ui_transient_index_target) {
        return ui->transient->target_texture;
    }
    return ui_transient_get_entry(ui, tex)->texture;
}

struct gpu_texture* ui_transient_target(struct ui* ui, struct ui_transient_tex tex) {
    if (tex.index == ui_transient_index_target) {
        return ui->transient->target_texture;
    }

    struct ui_transient_entry* entry = ui_transient_get_entry(ui, tex);

    assert(entry->region == false);

    if (entry->mip_count > 1) {
        return ui_transient_mip_view(ui, tex, 0);
    }
    return entry->texture;
}

struct gpu_texture* ui_transient_mip_view(struct ui* ui, struct ui_transient_tex tex, uint32_t level) {
    if (tex.index == ui_transient_index_target) {
        assert(level == 0);
        return ui->transient->target_texture;
    }

    struct ui_transient_entry* entry = ui_transient_get_entry(ui, tex);

    assert(level < entry->mip_count);

    if (entry->region) {
        struct ui_transient_page* page = ui_transient_entry_page(ui, entry);

        if (page->mip_views[level] == NULL) {
            page->mip_views[level] = gpu_texture_create_view_mip(ui->device, page->texture, level);
        }
        return page->mip_views[level];
    }

    if (entry->mip_views[level] == NULL) {
        entry->mip_views[level] = gpu_texture_create_view_mip(ui->device, entry->texture, level);
    }
    return entry->mip_views[level];
}

uint32_t ui_transient_mip_count(struct ui* ui, struct ui_transient_tex tex) {
    if (tex.index == ui_transient_index_target) {
        return 1;
    }
    return ui_transient_get_entry(ui, tex)->mip_count;
}

uint2 ui_transient_tex_size(struct ui* ui, struct ui_transient_tex tex) {
    if (tex.index == ui_transient_index_target) {
        return ui->transient->target_dims;
    }
    return ui_transient_get_entry(ui, tex)->size;
}

uint2 ui_transient_tex_origin(struct ui* ui, struct ui_transient_tex tex) {
    if (tex.index == ui_transient_index_target) {
        return make_uint2(0, 0);
    }

    struct ui_transient_entry* entry = ui_transient_get_entry(ui, tex);

    return entry->region ? entry->origin : make_uint2(0, 0);
}

uint2 ui_transient_tex_phys(struct ui* ui, struct ui_transient_tex tex) {
    if (tex.index == ui_transient_index_target) {
        return ui->transient->target_dims;
    }

    struct ui_transient_entry* entry = ui_transient_get_entry(ui, tex);

    return entry->region ? entry->phys : entry->size;
}

uint32_t ui_transient_tex_channel(struct ui* ui, struct ui_transient_tex tex) {
    if (tex.index == ui_transient_index_target) {
        return uint32_max;
    }

    struct ui_transient_entry* entry = ui_transient_get_entry(ui, tex);

    return entry->region ? ui_transient_entry_page(ui, entry)->channel : uint32_max;
}

struct gpu_bind_group* ui_transient_bind_group(struct ui* ui, struct ui_transient_tex tex) {
    if (tex.index == ui_transient_index_target) {
        if (ui->transient->target_bind_group == NULL) {
            ui->transient->target_bind_group = ui_render_texture_bind_group(ui, ui->transient->target_texture, false, false);
        }
        return ui->transient->target_bind_group;
    }

    struct ui_transient_entry* entry = ui_transient_get_entry(ui, tex);

    if (entry->region) {
        struct ui_transient_page* page = ui_transient_entry_page(ui, entry);

        if (page->bind_group == NULL) {
            page->bind_group = ui_render_texture_bind_group(ui, page->mip_count > 1 ? ui_transient_mip_view(ui, tex, 0) : page->texture, false, false);
        }
        return page->bind_group;
    }

    if (entry->bind_group == NULL) {
        entry->bind_group = ui_render_texture_bind_group(ui, entry->mip_count > 1 ? ui_transient_mip_view(ui, tex, 0) : entry->texture, false, false);
    }

    return entry->bind_group;
}

uint64_t ui_transient_memory_used_(struct ui* ui) {
    return ui->transient->memory_used;
}

void ui_transient_frame_end(struct ui* ui) {
    struct ui_transient* transient = ui->transient;

    uint32_t live_count = 0;

    for (uint32_t i = 0; i < ui_arr_transient_entry_get_count(transient->entries); i++) {
        struct ui_transient_entry* entry = ui_arr_transient_entry_get_ptr(transient->entries, i);

        if (entry->texture == NULL) {
            continue;
        }

        assert(entry->region == false);
        assert(entry->reserved == false);

        entry->idle_frames++;

        if (entry->idle_frames >= ui_transient_evict_idle_frames) {
            transient->memory_used -= ui_transient_entry_bytes(entry);
            ui_transient_entry_free(ui, entry);
            continue;
        }

        live_count = i + 1;
    }

    while (ui_arr_transient_entry_get_count(transient->entries) > live_count) {
        ui_arr_transient_entry_remove(transient->entries, ui_arr_transient_entry_get_count(transient->entries) - 1);
    }

    for (uint32_t i = 0; i < ui_arr_transient_page_get_count(transient->pages); i++) {
        struct ui_transient_page* page = ui_arr_transient_page_get_ptr(transient->pages, i);

        if (page->texture == NULL) {
            continue;
        }

        if (ui_shelf_is_empty(&page->shelf) == false) {
            page->idle_frames = 0;
            continue;
        }

        page->idle_frames++;

        if (page->idle_frames >= ui_transient_evict_idle_frames) {
            ui_transient_page_free(ui, page);
        }
    }

    assert(transient->memory_used <= transient->memory_limit);
}
