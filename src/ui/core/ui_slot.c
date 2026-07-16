#include "ui_render_impl.h"
#include "ui_atlas.h"

static void ui_linear_allocator_reset(struct ui_linear_allocator* alloc, uint32_t size) {
    memset(alloc, 0, sizeof(*alloc));
    alloc->size = size;
}

static uint32_t ui_linear_allocator_alloc(struct ui_linear_allocator* alloc, uint32_t size, uint32_t align) {
    uint32_t offset = align_up_uint32(alloc->offset, align);
    if (size != 0) {
        assert(offset + size <= alloc->size);
        alloc->offset = offset + size;
    }
    return offset;
}

static void ui_init_slot(struct ui* ui, struct ui_slot* slot) {
    assert(slot->owned == NULL);

    memset(slot, 0, sizeof(*slot));
    slot->owned = ui_arr_slot_resource_create(ui->mem);
    slot->uniforms_buffer = gpu_buffer_create(ui->device, ui_buffer_uniforms_size, gpu_buffer_usage_uniform | gpu_buffer_usage_storage | gpu_buffer_usage_write, "ui slot uniforms");
    slot->vertices_buffer = gpu_buffer_create(ui->device, ui_buffer_vertices_size, gpu_buffer_usage_storage | gpu_buffer_usage_write, "ui slot vertices");

    struct gpu_bind_desc binds[ui_bind_group_0_binding_count] = {
        [ui_bind_group_0_globals] = {
            .binding = ui_bind_group_0_globals,
            .buffer = slot->uniforms_buffer,
            .buffer_size = 256,
        },
        [ui_bind_group_0_uniforms] = {
            .binding = ui_bind_group_0_uniforms,
            .buffer = slot->uniforms_buffer,
            .buffer_size = 256,
        },
        [ui_bind_group_0_vertices] = {
            .binding = ui_bind_group_0_vertices,
            .buffer = slot->vertices_buffer,
            .buffer_size = ui_buffer_vertices_size,
        },
        [ui_bind_group_0_sdf] = {
            .binding = ui_bind_group_0_sdf,
            .buffer = slot->uniforms_buffer,
            .buffer_size = ui_buffer_uniforms_size,
        },
    };
    slot->bind_group = gpu_bind_group_create(ui->device, ui->render->bind_layout_0, binds, cr_count(binds));
}

static void ui_deinit_slot(struct ui* ui, struct ui_slot* slot) {
    assert(slot->owned != NULL);
    assert(ui_arr_slot_resource_get_count(slot->owned) == 0);

    ui_arr_slot_resource_destroy(slot->owned);
    gpu_bind_group_destroy(ui->device, slot->bind_group);
    gpu_buffer_destroy(ui->device, slot->uniforms_buffer);
    gpu_buffer_destroy(ui->device, slot->vertices_buffer);
    memset(slot, 0, sizeof(*slot));
}

static void ui_reset_slot(struct ui* ui, struct ui_slot* slot) {
    uint32_t owned_count = ui_arr_slot_resource_get_count(slot->owned);
    struct ui_slot_resource* owned = ui_arr_slot_resource_get_data(slot->owned);

    for (uint32_t i = 0; i < owned_count; i++) {
        struct ui_slot_resource* resource = &owned[i];

        switch (resource->type) {
        case ui_slot_resource_type_buffer: gpu_buffer_destroy(ui->device, resource->buffer); break;
        case ui_slot_resource_type_texture: gpu_texture_destroy(ui->device, resource->texture); break;
        case ui_slot_resource_type_bind_group: gpu_bind_group_destroy(ui->device, resource->bind_group); break;
        case ui_slot_resource_type_graphic: ui_atlas_release_graphic(ui, resource->graphic); break;
        }
    }
    ui_arr_slot_resource_remove_all(slot->owned);
    slot->serial = 0;
}

void ui_reset_slots(struct ui* ui) {
    for (uint32_t i = 0; i < cr_count(ui->render->slots); i++) {
        struct ui_slot* slot = &ui->render->slots[i];

        if (slot->serial != 0 && gpu_device_is_completed(ui->device, slot->serial)) {
            ui_reset_slot(ui, slot);
        }
    }
}

void ui_deinit_slots(struct ui* ui) {
    for (uint32_t i = 0; i < cr_count(ui->render->slots); i++) {
        struct ui_slot* slot = &ui->render->slots[i];

        if (slot->owned != NULL) {
#if cr_macos || cr_ios
            gpu_device_wait(ui->device, slot->serial);
#endif
            ui_reset_slot(ui, slot);
            ui_deinit_slot(ui, slot);
        }
    }
}

static struct ui_slot* ui_take_slot(struct ui* ui) {
    ui_reset_slots(ui);

    uint32_t i = 0;
    while (i < cr_count(ui->render->slots) && ui->render->slots[i].serial != 0) {
        i++;
    }
    cr_check(i < cr_count(ui->render->slots), "all %u render slots are in use by command buffers the gpu has not completed; commit or discard them", (uint32_t)cr_count(ui->render->slots));

    return &ui->render->slots[i];
}

struct ui_slot* ui_slot_begin(struct ui* ui, struct gpu_command_buffer* command_buffer, struct gpu_texture* target) {
    struct ui_slot* slot = ui_take_slot(ui);

    if (slot->owned == NULL) {
        ui_init_slot(ui, slot);
    }

    slot->serial = gpu_command_buffer_get_serial(command_buffer);
    slot->command_buffer = command_buffer;
    slot->target = target;
    slot->pass = NULL;

    ui_linear_allocator_reset(&ui->render->alloc_uniforms, ui_buffer_uniforms_size);
    ui_linear_allocator_reset(&ui->render->alloc_vertices, ui_buffer_vertices_size);

    uint8_t* uniforms_contents = gpu_buffer_get_contents(slot->uniforms_buffer);
    uint8_t* vertices_contents = gpu_buffer_get_contents(slot->vertices_buffer);
#if cr_wasm
    ui->render->write_uniforms = uniforms_contents != NULL ? uniforms_contents : ui->render->temp_uniforms;
    ui->render->write_vertices = vertices_contents != NULL ? vertices_contents : ui->render->temp_vertices;
#else
    assert(uniforms_contents != NULL && vertices_contents != NULL);
    ui->render->write_uniforms = uniforms_contents;
    ui->render->write_vertices = vertices_contents;
#endif

    return slot;
}

void* ui_alloc_uniforms(struct ui* ui, uint32_t size, uint32_t* out_offset) {
    *out_offset = ui_linear_allocator_alloc(&ui->render->alloc_uniforms, size, 256);
    return ui->render->write_uniforms + *out_offset;
}

void* ui_alloc_vertices(struct ui* ui, uint32_t size, uint32_t align, uint32_t* out_offset) {
    *out_offset = ui_linear_allocator_alloc(&ui->render->alloc_vertices, size, align);
    return ui->render->write_vertices + *out_offset;
}

void ui_slot_end(struct ui* ui, struct ui_slot* slot) {
#if cr_wasm
    if (ui->render->write_uniforms == ui->render->temp_uniforms && ui->render->alloc_uniforms.offset != 0) {
        gpu_buffer_write(ui->device, slot->uniforms_buffer, 0, ui->render->temp_uniforms, ui->render->alloc_uniforms.offset);
    }
    if (ui->render->write_vertices == ui->render->temp_vertices && ui->render->alloc_vertices.offset != 0) {
        gpu_buffer_write(ui->device, slot->vertices_buffer, 0, ui->render->temp_vertices, ui->render->alloc_vertices.offset);
    }
#endif
    slot->command_buffer = NULL;
    slot->target = NULL;
    slot->pass = NULL;
}
