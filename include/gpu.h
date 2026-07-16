#pragma once

#include "cr.h"

struct gpu_device;
struct gpu_library;
struct gpu_sampler;
struct gpu_buffer;
struct gpu_texture;
struct gpu_bind_layout;
struct gpu_bind_group;
struct gpu_render_pipeline;
struct gpu_compute_pipeline;
struct gpu_command_buffer;
struct gpu_render_pass;
struct gpu_compute_pass;
struct gpu_profiler;

enum cr_closed gpu_format {
    gpu_format_bgra8,
    gpu_format_r8,
    gpu_format_rgba16f,
    gpu_format_r32f,
};

enum cr_closed_flag gpu_stage {
    gpu_stage_vertex = 1 << 0,
    gpu_stage_fragment = 1 << 1,
    gpu_stage_compute = 1 << 2,
};

enum cr_closed_flag gpu_buffer_usage {
    gpu_buffer_usage_uniform = 1 << 0,
    gpu_buffer_usage_storage = 1 << 1,
    gpu_buffer_usage_indirect = 1 << 2,
    gpu_buffer_usage_write = 1 << 3,
    gpu_buffer_usage_read = 1 << 4,
};

enum cr_closed_flag gpu_texture_usage {
    gpu_texture_usage_sample = 1 << 0,
    gpu_texture_usage_render_target = 1 << 1,
    gpu_texture_usage_storage_write = 1 << 2,
    gpu_texture_usage_upload = 1 << 3,
    gpu_texture_usage_read = 1 << 4,
};

enum cr_closed gpu_binding_type {
    gpu_binding_type_uniform,
    gpu_binding_type_storage,
    gpu_binding_type_storage_read,
    gpu_binding_type_texture,
    gpu_binding_type_storage_texture,
    gpu_binding_type_sampler,
};

struct gpu_binding_desc {
    uint32_t binding;
    enum gpu_binding_type type;
    uint32_t stages;
    bool dynamic_offset;
    bool unfilterable;
    enum gpu_format format;
};

struct gpu_bind_desc {
    uint32_t binding;
    struct gpu_buffer* buffer;
    uint32_t buffer_offset;
    uint32_t buffer_size;
    struct gpu_texture* texture;
    struct gpu_sampler* sampler;
};

enum cr_closed gpu_filter {
    gpu_filter_linear,
    gpu_filter_nearest,
};

struct gpu_sampler_desc {
    const char* label;
    bool repeat;
    enum gpu_filter filter;
    bool mip;
};

struct gpu_texture_desc {
    const char* label;
    enum gpu_format format;
    uint2 size;
    uint32_t usage;
    uint32_t mip_count;
};

struct gpu_render_pipeline_desc {
    struct gpu_library* library;
    const char* vertex_name;
    const char* fragment_name;
    struct gpu_bind_layout* layouts[4];
    uint32_t layout_count;
    enum gpu_format color_format;
    bool blend;
};

struct gpu_compute_pipeline_desc {
    struct gpu_library* library;
    const char* name;
    struct gpu_bind_layout* layouts[4];
    uint32_t layout_count;
    uint2 workgroup_size;
};

enum cr_closed gpu_load {
    gpu_load_clear,
    gpu_load_keep,
    gpu_load_dont_care,
};

#if cr_macos || cr_ios
struct gpu_device* gpu_device_create(struct cr_mem* mem);
#elif cr_wasm
struct gpu_device* gpu_device_create(struct cr_mem* mem, cr_wgpu_t wgpu_device, cr_wgpu_t wgpu_queue);
#endif
void gpu_device_destroy(struct gpu_device* device);

struct gpu_library* gpu_library_create(struct gpu_device* device, const void* data, uint64_t size, const char* label);
void gpu_library_destroy(struct gpu_device* device, struct gpu_library* library);
const char* gpu_library_get_extension(void);

struct gpu_sampler* gpu_sampler_create(struct gpu_device* device, const struct gpu_sampler_desc* desc);
void gpu_sampler_destroy(struct gpu_device* device, struct gpu_sampler* sampler);

struct gpu_buffer* gpu_buffer_create(struct gpu_device* device, uint32_t size, uint32_t usage, const char* label);
void gpu_buffer_destroy(struct gpu_device* device, struct gpu_buffer* buffer);
void gpu_buffer_write(struct gpu_device* device, struct gpu_buffer* buffer, uint32_t offset, const void* data, uint32_t size);
void* gpu_buffer_get_contents(struct gpu_buffer* buffer);

struct gpu_texture* gpu_texture_create(struct gpu_device* device, const struct gpu_texture_desc* desc);
#if cr_macos || cr_ios
struct gpu_texture* gpu_texture_create_target_from_native(struct gpu_device* device, void* mtl_texture);
#elif cr_wasm
struct gpu_texture* gpu_texture_create_target_from_native(struct gpu_device* device, cr_wgpu_t wgpu_texture_view, uint2 size);
#endif
struct gpu_texture* gpu_texture_create_view_mip(struct gpu_device* device, struct gpu_texture* texture, uint32_t level);
void gpu_texture_destroy(struct gpu_device* device, struct gpu_texture* texture);
#if cr_macos || cr_ios
void* gpu_device_get_native_device(struct gpu_device* device);
void* gpu_device_get_native_queue(struct gpu_device* device);
void* gpu_texture_get_native(struct gpu_texture* texture);
void* gpu_command_buffer_get_native(struct gpu_command_buffer* command_buffer);
#endif
void gpu_texture_write(struct gpu_device* device, struct gpu_texture* texture, uint32_t x, uint32_t y, uint32_t width, uint32_t height, const void* data, uint32_t bytes_per_row);
enum gpu_format gpu_texture_get_format(struct gpu_texture* texture);
uint2 gpu_texture_get_size(struct gpu_texture* texture);
uint32_t gpu_texture_get_usage(struct gpu_texture* texture);

struct gpu_bind_layout* gpu_bind_layout_create(struct gpu_device* device, const struct gpu_binding_desc* bindings, uint32_t binding_count);
void gpu_bind_layout_destroy(struct gpu_device* device, struct gpu_bind_layout* layout);

struct gpu_bind_group* gpu_bind_group_create(struct gpu_device* device, struct gpu_bind_layout* layout, const struct gpu_bind_desc* binds, uint32_t bind_count);
void gpu_bind_group_destroy(struct gpu_device* device, struct gpu_bind_group* group);

struct gpu_render_pipeline* gpu_render_pipeline_create(struct gpu_device* device, const struct gpu_render_pipeline_desc* desc);
void gpu_render_pipeline_destroy(struct gpu_device* device, struct gpu_render_pipeline* pipeline);

struct gpu_compute_pipeline* gpu_compute_pipeline_create(struct gpu_device* device, const struct gpu_compute_pipeline_desc* desc);
void gpu_compute_pipeline_destroy(struct gpu_device* device, struct gpu_compute_pipeline* pipeline);

struct gpu_command_buffer* gpu_command_buffer_create(struct gpu_device* device, const char* label);
uint64_t gpu_command_buffer_get_serial(struct gpu_command_buffer* command_buffer);
void gpu_command_buffer_read_texture(struct gpu_command_buffer* command_buffer, struct gpu_texture* texture, void* out, uint32_t bytes_per_row);
void gpu_command_buffer_read_buffer(struct gpu_command_buffer* command_buffer, struct gpu_buffer* buffer, uint32_t offset, void* out, uint32_t size);
uint64_t gpu_command_buffer_commit(struct gpu_command_buffer* command_buffer);
void gpu_command_buffer_discard(struct gpu_command_buffer* command_buffer);

bool gpu_device_is_completed(struct gpu_device* device, uint64_t serial);
uint64_t gpu_device_get_oldest_incomplete_serial(struct gpu_device* device);
uint64_t gpu_device_get_pipelines_serial(struct gpu_device* device);
bool gpu_device_is_failed(struct gpu_device* device, uint64_t serial);
#if cr_macos || cr_ios
void gpu_device_wait(struct gpu_device* device, uint64_t serial);
#elif cr_wasm
void gpu_device_set_on_completed(struct gpu_device* device, void (*func)(void* user), void* user);
#endif

#define gpu_profile_max_passes 128u
#define gpu_pass_name_max 32u

struct gpu_profile {
    uint32_t pass_count;
    double span;
    double passes[gpu_profile_max_passes];
    char pass_names[gpu_profile_max_passes][gpu_pass_name_max];
};

struct gpu_profiler* gpu_profiler_create(struct gpu_device* device, uint32_t max_passes);
void gpu_profiler_destroy(struct gpu_device* device, struct gpu_profiler* profiler);
bool gpu_command_buffer_profile(struct gpu_command_buffer* command_buffer, struct gpu_profiler* profiler);
bool gpu_profiler_poll(struct gpu_profiler* profiler, struct gpu_profile* out);

struct gpu_render_pass* gpu_command_buffer_begin_render(struct gpu_command_buffer* command_buffer, struct gpu_texture* target, enum gpu_load load, const char* label);
void gpu_render_pass_end(struct gpu_render_pass* pass);
void gpu_render_pass_set_pipeline(struct gpu_render_pass* pass, struct gpu_render_pipeline* pipeline);
void gpu_render_pass_set_bind_group(struct gpu_render_pass* pass, uint32_t index, struct gpu_bind_group* group, const uint32_t* dynamic_offsets, uint32_t dynamic_offset_count);
void gpu_render_pass_set_scissor(struct gpu_render_pass* pass, uint32_t x, uint32_t y, uint32_t width, uint32_t height);
void gpu_render_pass_draw(struct gpu_render_pass* pass, uint32_t vertex_start, uint32_t vertex_count);

struct gpu_compute_pass* gpu_command_buffer_begin_compute(struct gpu_command_buffer* command_buffer, const char* label);
struct gpu_compute_pass* gpu_command_buffer_begin_compute_concurrent(struct gpu_command_buffer* command_buffer, const char* label);
void gpu_compute_pass_barrier(struct gpu_compute_pass* pass);
void gpu_compute_pass_end(struct gpu_compute_pass* pass);
void gpu_compute_pass_set_pipeline(struct gpu_compute_pass* pass, struct gpu_compute_pipeline* pipeline);
void gpu_compute_pass_set_bind_group(struct gpu_compute_pass* pass, uint32_t index, struct gpu_bind_group* group, const uint32_t* dynamic_offsets, uint32_t dynamic_offset_count);
void gpu_compute_pass_dispatch(struct gpu_compute_pass* pass, uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z);
void gpu_compute_pass_dispatch_indirect(struct gpu_compute_pass* pass, struct gpu_buffer* buffer, uint32_t offset);
