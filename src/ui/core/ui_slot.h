#pragma once

#include "cr.h"
#include "gpu.h"

struct ui;
struct ui_graphic;

#define ui_buffer_uniforms_size (1u << 23)
#define ui_buffer_vertices_size (1u << 23)

enum ui_slot_resource_type {
    ui_slot_resource_type_buffer,
    ui_slot_resource_type_texture,
    ui_slot_resource_type_bind_group,
    ui_slot_resource_type_graphic,
};

struct ui_linear_allocator {
    uint32_t offset;
    uint32_t size;
};

struct ui_slot_resource {
    enum ui_slot_resource_type type;
    union {
        struct gpu_buffer* buffer;
        struct gpu_texture* texture;
        struct gpu_bind_group* bind_group;
        struct ui_graphic* graphic;
    };
};

#define cr_arr ui_arr_slot_resource
#define cr_arr_item struct ui_slot_resource
#include "cr_arr.h"

struct ui_slot {
    uint64_t serial;
    struct gpu_buffer* uniforms_buffer;
    struct gpu_buffer* vertices_buffer;
    struct gpu_bind_group* bind_group;
    struct gpu_command_buffer* command_buffer;
    struct gpu_texture* target;
    struct gpu_render_pass* pass;
    struct ui_arr_slot_resource* owned;
};

cr_hdef void ui_slot_own(struct ui_slot* slot, struct ui_slot_resource resource) {
    ui_arr_slot_resource_append(slot->owned, resource);
}

cr_hdef void ui_slot_destroy_buffer(struct gpu_device* device, struct ui_slot* slot, struct gpu_buffer* buffer) {
    if (slot) {
        ui_slot_own(slot, (struct ui_slot_resource) { .type = ui_slot_resource_type_buffer, .buffer = buffer });
    } else {
        gpu_buffer_destroy(device, buffer);
    }
}

cr_hdef void ui_slot_destroy_texture(struct gpu_device* device, struct ui_slot* slot, struct gpu_texture* texture) {
    if (slot) {
        ui_slot_own(slot, (struct ui_slot_resource) { .type = ui_slot_resource_type_texture, .texture = texture });
    } else {
        gpu_texture_destroy(device, texture);
    }
}

struct ui_slot* ui_slot_begin(struct ui* ui, struct gpu_command_buffer* command_buffer, struct gpu_texture* target);
void ui_slot_end(struct ui* ui, struct ui_slot* slot);
void ui_reset_slots(struct ui* ui);
void ui_deinit_slots(struct ui* ui);

void* ui_alloc_uniforms(struct ui* ui, uint32_t size, uint32_t* out_offset);
void* ui_alloc_vertices(struct ui* ui, uint32_t size, uint32_t align, uint32_t* out_offset);
