#pragma once

#include "cr.h"
#include "gpu.h"
#include "ui_transient.h"

struct ui;
struct ui_slot;

#define ui_blur_params_stride 512
#define ui_blur_chain_max_dispatches 3
#define ui_blur_pyramid_max_levels 8

#define ui_blur_extent_per_sigma 3.0

struct ui_blur {
    struct cr_mem* mem;
    struct gpu_device* device;
    struct gpu_sampler* sampler;
    struct gpu_bind_layout* layout;
    struct gpu_bind_layout* layout_pair;
    struct gpu_bind_layout* layout_tail;
    struct gpu_compute_pipeline* pipeline_fused;
    struct gpu_compute_pipeline* pipeline_scale;
    struct gpu_compute_pipeline* pipeline_down_direct;
    struct gpu_compute_pipeline* pipeline_up_direct;
    struct gpu_compute_pipeline* pipeline_down;
    struct gpu_compute_pipeline* pipeline_pair;
    struct gpu_compute_pipeline* pipeline_tail;
    struct gpu_texture* unused_dst;
};

struct ui_blur_view {
    struct gpu_texture* texture;
    uint2 page_size;
    uint2 origin;
    uint2 size;
    uint2 phys;
};

struct ui_blur_chain {
    uint32_t count;
    uint32_t reserved_count;
    struct gpu_buffer* temp_params;
    struct gpu_compute_pipeline* pipelines[ui_blur_chain_max_dispatches];
    struct gpu_bind_group* groups[ui_blur_chain_max_dispatches];
    uint2 grids[ui_blur_chain_max_dispatches];
    struct ui_transient_tex reserved[ui_blur_chain_max_dispatches];
};

struct ui_blur_pyramid_tex {
    struct ui_transient_tex tex;
    uint32_t mips;
    uint2 source_size;
    uint32_t capture_log2;
};

struct ui_blur_pyramid_chain {
    uint32_t pass_count;
    struct gpu_buffer* temp_params;
    uint2 grids[ui_blur_pyramid_max_levels + 1];
    struct gpu_bind_group* groups[ui_blur_pyramid_max_levels + 1];
    struct gpu_compute_pipeline* pipelines[ui_blur_pyramid_max_levels + 1];
};

struct ui_blur* ui_blur_create(struct cr_mem* mem, struct gpu_device* device, struct gpu_library* library);
void ui_blur_destroy(struct ui_blur* blur);

uint32_t ui_blur_coarse_level(float radius, uint2 size);
uint2 ui_blur_coarse_size(uint2 size, uint32_t level);
void ui_blur_chain_prepare(struct ui* ui, struct ui_slot* slot, struct ui_transient_tex src, struct ui_transient_tex dst, float radius, float4 tint, bool colorize, struct ui_blur_chain* out);
void ui_blur_chain_prepare_region(struct ui* ui, struct ui_slot* slot, struct ui_transient_tex src, uint2 src_offset, uint2 src_size, struct ui_transient_tex dst, float radius, float4 tint, bool colorize, struct ui_blur_chain* out);
void ui_blur_chain_prepare_gpu(struct ui* ui, struct ui_slot* slot, struct ui_blur_view src, struct ui_blur_view dst,
                               float radius, float4 tint, bool colorize, struct ui_blur_chain* out);
void ui_blur_chain_encode(struct ui* ui, struct gpu_command_buffer* command_buffer, const struct ui_blur_chain* chain);
void ui_blur_chain_encode_stage(struct ui* ui, struct gpu_compute_pass* pass, const struct ui_blur_chain* chain, uint32_t stage);
void ui_blur_chain_finish(struct ui* ui, struct ui_blur_chain* chain);

struct ui_blur_pyramid_tex ui_blur_pyramid_reserve(struct ui* ui, uint32_t width, uint32_t height, uint32_t level_count, uint32_t capture_log2);
void ui_blur_pyramid_prepare(struct ui* ui, struct ui_slot* slot, struct ui_blur_pyramid_tex pyr, struct gpu_texture* base_texture, uint2 base_page, uint2 base_offset, uint32_t level_count, uint32_t box_levels, struct ui_blur_pyramid_chain* out);
void ui_blur_pyramid_encode_pass(struct ui* ui, struct gpu_compute_pass* pass, const struct ui_blur_pyramid_chain* chain, uint32_t index);
void ui_blur_pyramid_finish(struct ui* ui, struct ui_blur_pyramid_chain* chain);
