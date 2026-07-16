#pragma once

#include "cr.h"
#include "gpu.h"

struct ui;
struct ui_slot;
struct ui_transient;

#define ui_transient_format_count 4

#define ui_transient_index_target 0xffffu
#define ui_transient_channel_count 4
#define ui_transient_channel_blur_down 1
#define ui_transient_channel_blur_coarse 2
#define ui_transient_channel_blur_out 3

struct ui_transient_tex {
    uint16_t index;
    uint16_t w;
    uint16_t h;
};

struct ui_transient* ui_transient_create(struct cr_mem* mem);
void ui_transient_destroy(struct ui* ui);
struct ui_transient_tex ui_transient_wrap_target(struct ui* ui, struct gpu_texture* texture, uint2 size);
void ui_transient_unwrap_target(struct ui* ui, struct ui_slot* slot);
struct ui_transient_tex ui_transient_reserve(struct ui* ui, enum gpu_format format, uint32_t width, uint32_t height);
struct ui_transient_tex ui_transient_reserve_storage(struct ui* ui, enum gpu_format format, uint32_t width, uint32_t height);
struct ui_transient_tex ui_transient_reserve_storage_dedicated(struct ui* ui, enum gpu_format format, uint32_t width, uint32_t height);
struct ui_transient_tex ui_transient_reserve_storage_channel(struct ui* ui, enum gpu_format format, uint32_t width, uint32_t height, uint32_t channel);
struct ui_transient_tex ui_transient_reserve_mips(struct ui* ui, enum gpu_format format, uint32_t width, uint32_t height, uint32_t mip_count);
struct ui_transient_tex ui_transient_reserve_mips_storage(struct ui* ui, enum gpu_format format, uint32_t width, uint32_t height, uint32_t mip_count);
void ui_transient_release(struct ui* ui, struct ui_transient_tex tex);
struct gpu_texture* ui_transient_texture(struct ui* ui, struct ui_transient_tex tex);
struct gpu_texture* ui_transient_target(struct ui* ui, struct ui_transient_tex tex);
struct gpu_texture* ui_transient_mip_view(struct ui* ui, struct ui_transient_tex tex, uint32_t level);
uint32_t ui_transient_mip_count(struct ui* ui, struct ui_transient_tex tex);
struct gpu_bind_group* ui_transient_bind_group(struct ui* ui, struct ui_transient_tex tex);
uint2 ui_transient_tex_size(struct ui* ui, struct ui_transient_tex tex);
uint2 ui_transient_tex_origin(struct ui* ui, struct ui_transient_tex tex);
uint2 ui_transient_tex_phys(struct ui* ui, struct ui_transient_tex tex);
uint32_t ui_transient_tex_channel(struct ui* ui, struct ui_transient_tex tex);
void ui_transient_frame_end(struct ui* ui);
uint64_t ui_transient_memory_used_(struct ui* ui);
