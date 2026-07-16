#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef int WGpuObjectBase;
typedef WGpuObjectBase WGpuAdapter;
typedef WGpuObjectBase WGpuDevice;
typedef WGpuObjectBase WGpuQueue;
typedef WGpuObjectBase WGpuCanvasContext;
typedef WGpuObjectBase WGpuBuffer;
typedef WGpuObjectBase WGpuTexture;
typedef WGpuObjectBase WGpuTextureView;
typedef WGpuObjectBase WGpuSampler;
typedef WGpuObjectBase WGpuBindGroupLayout;
typedef WGpuObjectBase WGpuBindGroup;
typedef WGpuObjectBase WGpuPipelineLayout;
typedef WGpuObjectBase WGpuShaderModule;
typedef WGpuObjectBase WGpuRenderPipeline;
typedef WGpuObjectBase WGpuComputePipeline;
typedef WGpuObjectBase WGpuCommandEncoder;
typedef WGpuObjectBase WGpuCommandBuffer;
typedef WGpuObjectBase WGpuRenderPassEncoder;
typedef WGpuObjectBase WGpuComputePassEncoder;
typedef WGpuObjectBase WGpuQuerySet;

typedef double double_int53_t;

typedef uint32_t WGPU_FEATURES_BITFIELD;
typedef uint32_t WGPU_POWER_PREFERENCE;
typedef uint32_t WGPU_TEXTURE_FORMAT;
typedef uint32_t WGPU_CANVAS_COLOR_SPACE;
typedef uint32_t WGPU_SHADER_STAGE_FLAGS;
typedef uint32_t WGPU_BUFFER_USAGE_FLAGS;
typedef uint32_t WGPU_TEXTURE_USAGE_FLAGS;
typedef uint32_t WGPU_MAP_MODE_FLAGS;
typedef uint32_t WGPU_ERROR_TYPE;
typedef uint32_t WGPU_DEVICE_LOST_REASON;
typedef uint32_t WGPU_QUERY_TYPE;

#define WGPU_FEATURE_TIMESTAMP_QUERY 0x1u
#define WGPU_FEATURE_SHADER_F16 0x2u

#define WGPU_POWER_PREFERENCE_LOW_POWER 1u
#define WGPU_POWER_PREFERENCE_HIGH_PERFORMANCE 2u

#define WGPU_TEXTURE_FORMAT_BGRA8UNORM 1u
#define WGPU_TEXTURE_FORMAT_R8UNORM 2u
#define WGPU_TEXTURE_FORMAT_RGBA16FLOAT 3u
#define WGPU_TEXTURE_FORMAT_R32FLOAT 4u

#define WGPU_CANVAS_COLOR_SPACE_SRGB 1u
#define WGPU_CANVAS_COLOR_SPACE_DISPLAY_P3 2u

#define WGPU_SHADER_STAGE_VERTEX 0x1u
#define WGPU_SHADER_STAGE_FRAGMENT 0x2u
#define WGPU_SHADER_STAGE_COMPUTE 0x4u

#define WGPU_BUFFER_USAGE_MAP_READ 0x1u
#define WGPU_BUFFER_USAGE_MAP_WRITE 0x2u
#define WGPU_BUFFER_USAGE_COPY_SRC 0x4u
#define WGPU_BUFFER_USAGE_COPY_DST 0x8u
#define WGPU_BUFFER_USAGE_INDEX 0x10u
#define WGPU_BUFFER_USAGE_VERTEX 0x20u
#define WGPU_BUFFER_USAGE_UNIFORM 0x40u
#define WGPU_BUFFER_USAGE_STORAGE 0x80u
#define WGPU_BUFFER_USAGE_INDIRECT 0x100u
#define WGPU_BUFFER_USAGE_QUERY_RESOLVE 0x200u

#define WGPU_TEXTURE_USAGE_COPY_SRC 0x1u
#define WGPU_TEXTURE_USAGE_COPY_DST 0x2u
#define WGPU_TEXTURE_USAGE_TEXTURE_BINDING 0x4u
#define WGPU_TEXTURE_USAGE_STORAGE_BINDING 0x8u
#define WGPU_TEXTURE_USAGE_RENDER_ATTACHMENT 0x10u

#define WGPU_MAP_MODE_READ 0x1u
#define WGPU_MAP_MODE_WRITE 0x2u

#define WGPU_ERROR_TYPE_VALIDATION 1u
#define WGPU_ERROR_TYPE_OUT_OF_MEMORY 2u
#define WGPU_ERROR_TYPE_INTERNAL 3u

#define WGPU_DEVICE_LOST_REASON_UNKNOWN 0u
#define WGPU_DEVICE_LOST_REASON_DESTROYED 1u

#define WGPU_ADDRESS_MODE_CLAMP_TO_EDGE 1u
#define WGPU_ADDRESS_MODE_REPEAT 2u
#define WGPU_ADDRESS_MODE_MIRROR_REPEAT 3u

#define WGPU_FILTER_MODE_NEAREST 1u
#define WGPU_FILTER_MODE_LINEAR 2u

#define WGPU_BIND_GROUP_LAYOUT_TYPE_BUFFER 1u
#define WGPU_BIND_GROUP_LAYOUT_TYPE_SAMPLER 2u
#define WGPU_BIND_GROUP_LAYOUT_TYPE_TEXTURE 3u
#define WGPU_BIND_GROUP_LAYOUT_TYPE_STORAGE_TEXTURE 4u

#define WGPU_BUFFER_BINDING_TYPE_UNIFORM 1u
#define WGPU_BUFFER_BINDING_TYPE_STORAGE 2u
#define WGPU_BUFFER_BINDING_TYPE_READ_ONLY_STORAGE 3u

#define WGPU_TEXTURE_SAMPLE_TYPE_FLOAT 1u
#define WGPU_TEXTURE_SAMPLE_TYPE_UNFILTERABLE_FLOAT 2u

#define WGPU_TEXTURE_VIEW_DIMENSION_2D 1u

#define WGPU_STORAGE_TEXTURE_ACCESS_WRITE_ONLY 1u

#define WGPU_SAMPLER_BINDING_TYPE_FILTERING 1u

#define WGPU_BLEND_OPERATION_DISABLED 0u
#define WGPU_BLEND_OPERATION_ADD 1u

#define WGPU_BLEND_FACTOR_ZERO 1u
#define WGPU_BLEND_FACTOR_ONE 2u
#define WGPU_BLEND_FACTOR_SRC_ALPHA 3u
#define WGPU_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA 4u

#define WGPU_LOAD_OP_LOAD 1u
#define WGPU_LOAD_OP_CLEAR 2u

#define WGPU_STORE_OP_STORE 1u
#define WGPU_STORE_OP_DISCARD 2u

#define WGPU_QUERY_TYPE_TIMESTAMP 1u

enum wgpu_object_type {
    wgpu_object_type_invalid,
    wgpu_object_type_adapter,
    wgpu_object_type_device,
    wgpu_object_type_queue,
    wgpu_object_type_canvas_context,
    wgpu_object_type_buffer,
    wgpu_object_type_texture,
    wgpu_object_type_texture_view,
    wgpu_object_type_sampler,
    wgpu_object_type_bind_group_layout,
    wgpu_object_type_bind_group,
    wgpu_object_type_pipeline_layout,
    wgpu_object_type_shader_module,
    wgpu_object_type_render_pipeline,
    wgpu_object_type_compute_pipeline,
    wgpu_object_type_command_encoder,
    wgpu_object_type_command_buffer,
    wgpu_object_type_render_pass_encoder,
    wgpu_object_type_compute_pass_encoder,
    wgpu_object_type_query_set,
};

typedef struct WGpuRequestAdapterOptions {
    WGPU_POWER_PREFERENCE powerPreference;
} WGpuRequestAdapterOptions;

#define WGPU_REQUEST_ADAPTER_OPTIONS_DEFAULT_INITIALIZER ((WGpuRequestAdapterOptions){ 0 })

typedef struct WGpuDeviceDescriptor {
    WGPU_FEATURES_BITFIELD requiredFeatures;
    uint32_t maxComputeWorkgroupStorageSize;
} WGpuDeviceDescriptor;

#define WGPU_DEVICE_DESCRIPTOR_DEFAULT_INITIALIZER ((WGpuDeviceDescriptor){ 0 })

typedef struct WGpuCanvasConfiguration {
    WGpuDevice device;
    WGPU_TEXTURE_FORMAT format;
    WGPU_CANVAS_COLOR_SPACE colorSpace;
    WGPU_TEXTURE_USAGE_FLAGS usage;
    uint32_t toneMappingExtended;
} WGpuCanvasConfiguration;

#define WGPU_CANVAS_CONFIGURATION_DEFAULT_INITIALIZER ((WGpuCanvasConfiguration){ 0 })

typedef struct WGpuShaderModuleDescriptor {
    const char* code;
} WGpuShaderModuleDescriptor;

typedef struct WGpuSamplerDescriptor {
    uint32_t addressModeU;
    uint32_t addressModeV;
    uint32_t addressModeW;
    uint32_t minFilter;
    uint32_t magFilter;
    uint32_t mipmapFilter;
} WGpuSamplerDescriptor;

#define WGPU_SAMPLER_DESCRIPTOR_DEFAULT_INITIALIZER ((WGpuSamplerDescriptor){ \
    .addressModeU = WGPU_ADDRESS_MODE_CLAMP_TO_EDGE, \
    .addressModeV = WGPU_ADDRESS_MODE_CLAMP_TO_EDGE, \
    .addressModeW = WGPU_ADDRESS_MODE_CLAMP_TO_EDGE, \
    .minFilter = WGPU_FILTER_MODE_NEAREST, \
    .magFilter = WGPU_FILTER_MODE_NEAREST, \
    .mipmapFilter = WGPU_FILTER_MODE_NEAREST, \
})

typedef struct WGpuBufferDescriptor {
    uint32_t size;
    WGPU_BUFFER_USAGE_FLAGS usage;
} WGpuBufferDescriptor;

typedef struct WGpuTextureDescriptor {
    uint32_t width;
    uint32_t height;
    WGPU_TEXTURE_FORMAT format;
    WGPU_TEXTURE_USAGE_FLAGS usage;
    uint32_t mipLevelCount;
} WGpuTextureDescriptor;

#define WGPU_TEXTURE_DESCRIPTOR_DEFAULT_INITIALIZER ((WGpuTextureDescriptor){ 0 })

typedef struct WGpuTexelCopyTextureInfo {
    WGpuTexture texture;
    struct {
        int32_t x;
        int32_t y;
    } origin;
} WGpuTexelCopyTextureInfo;

#define WGPU_TEXEL_COPY_TEXTURE_INFO_DEFAULT_INITIALIZER ((WGpuTexelCopyTextureInfo){ 0 })

typedef struct WGpuBindGroupLayoutEntry {
    uint32_t binding;
    WGPU_SHADER_STAGE_FLAGS visibility;
    uint32_t type;
    union {
        struct {
            uint32_t type;
            uint32_t hasDynamicOffset;
        } buffer;
        struct {
            uint32_t sampleType;
            uint32_t viewDimension;
        } texture;
        struct {
            uint32_t access;
            uint32_t format;
            uint32_t viewDimension;
        } storageTexture;
        struct {
            uint32_t type;
        } sampler;
    } layout;
} WGpuBindGroupLayoutEntry;

#define WGPU_BUFFER_BINDING_LAYOUT_ENTRY_DEFAULT_INITIALIZER ((WGpuBindGroupLayoutEntry){ 0 })

typedef struct WGpuBindGroupEntry {
    uint32_t binding;
    WGpuObjectBase resource;
    uint32_t bufferBindOffset;
    uint32_t bufferBindSize;
} WGpuBindGroupEntry;

typedef struct WGpuBlendComponent {
    uint32_t operation;
    uint32_t srcFactor;
    uint32_t dstFactor;
} WGpuBlendComponent;

typedef struct WGpuColorTargetState {
    WGPU_TEXTURE_FORMAT format;
    struct {
        WGpuBlendComponent color;
        WGpuBlendComponent alpha;
    } blend;
} WGpuColorTargetState;

#define WGPU_COLOR_TARGET_STATE_DEFAULT_INITIALIZER ((WGpuColorTargetState){ 0 })

typedef struct WGpuRenderPipelineDescriptor {
    WGpuPipelineLayout layout;
    struct {
        WGpuShaderModule module;
        const char* entryPoint;
    } vertex;
    struct {
        WGpuShaderModule module;
        const char* entryPoint;
        uint32_t numTargets;
        const WGpuColorTargetState* targets;
    } fragment;
} WGpuRenderPipelineDescriptor;

#define WGPU_RENDER_PIPELINE_DESCRIPTOR_DEFAULT_INITIALIZER ((WGpuRenderPipelineDescriptor){ 0 })

typedef struct WGpuQuerySetDescriptor {
    WGPU_QUERY_TYPE type;
    uint32_t count;
} WGpuQuerySetDescriptor;

typedef struct WGpuPassTimestampWrites {
    WGpuQuerySet querySet;
    int32_t beginningOfPassWriteIndex;
    int32_t endOfPassWriteIndex;
} WGpuPassTimestampWrites;

typedef struct WGpuRenderPassColorAttachment {
    WGpuTextureView view;
    uint32_t loadOp;
    uint32_t storeOp;
    struct {
        float r;
        float g;
        float b;
        float a;
    } clearValue;
} WGpuRenderPassColorAttachment;

#define WGPU_RENDER_PASS_COLOR_ATTACHMENT_DEFAULT_INITIALIZER ((WGpuRenderPassColorAttachment){ \
    .loadOp = WGPU_LOAD_OP_LOAD, \
    .storeOp = WGPU_STORE_OP_STORE, \
})

typedef struct WGpuRenderPassDescriptor {
    uint32_t numColorAttachments;
    const WGpuRenderPassColorAttachment* colorAttachments;
    WGpuPassTimestampWrites timestampWrites;
} WGpuRenderPassDescriptor;

#define WGPU_RENDER_PASS_DESCRIPTOR_DEFAULT_INITIALIZER ((WGpuRenderPassDescriptor){ 0 })

typedef struct WGpuComputePassDescriptor {
    WGpuPassTimestampWrites timestampWrites;
} WGpuComputePassDescriptor;

#define WGPU_COMPUTE_PASS_DESCRIPTOR_DEFAULT_INITIALIZER ((WGpuComputePassDescriptor){ 0 })

typedef void (*WGpuRequestAdapterCallback)(WGpuAdapter adapter, void* user);
typedef void (*WGpuRequestDeviceCallback)(WGpuDevice device, void* user);
typedef void (*WGpuDeviceErrorCallback)(WGpuDevice device, WGPU_ERROR_TYPE type, const char* message, void* user);
typedef void (*WGpuDeviceLostCallback)(WGpuDevice device, WGPU_DEVICE_LOST_REASON reason, const char* message, void* user);
typedef void (*WGpuOnSubmittedWorkDoneCallback)(WGpuQueue queue, void* user);
typedef void (*WGpuBufferMapCallback)(WGpuBuffer buffer, void* user, WGPU_MAP_MODE_FLAGS mode, double_int53_t offset, double_int53_t size);
typedef void (*WGpuCreatePipelineCallback)(WGpuObjectBase pipeline, void* user, const char* message);

#define wgpu_js(name) __attribute__((import_module("env"), import_name(#name)))

wgpu_js(wgpu_object_type) int wgpu_object_type_(WGpuObjectBase object);
wgpu_js(wgpu_object_destroy) void wgpu_object_destroy(WGpuObjectBase object);

wgpu_js(navigator_gpu_request_adapter_async) void navigator_gpu_request_adapter_async(const WGpuRequestAdapterOptions* options, WGpuRequestAdapterCallback callback, void* user);
wgpu_js(wgpu_adapter_or_device_get_features) WGPU_FEATURES_BITFIELD wgpu_adapter_or_device_get_features(WGpuObjectBase adapter_or_device);
wgpu_js(wgpu_adapter_or_device_get_workgroup_storage_limit) uint32_t wgpu_adapter_or_device_get_workgroup_storage_limit(WGpuObjectBase adapter_or_device);
wgpu_js(wgpu_adapter_request_device_async) void wgpu_adapter_request_device_async(WGpuAdapter adapter, const WGpuDeviceDescriptor* descriptor, WGpuRequestDeviceCallback callback, void* user);

wgpu_js(wgpu_device_get_queue) WGpuQueue wgpu_device_get_queue(WGpuDevice device);
wgpu_js(wgpu_device_set_uncapturederror_callback) void wgpu_device_set_uncapturederror_callback(WGpuDevice device, WGpuDeviceErrorCallback callback, void* user);
wgpu_js(wgpu_device_set_lost_callback) void wgpu_device_set_lost_callback(WGpuDevice device, WGpuDeviceLostCallback callback, void* user);

wgpu_js(wgpu_canvas_get_webgpu_context) WGpuCanvasContext wgpu_canvas_get_webgpu_context(const char* canvas_id);
wgpu_js(wgpu_canvas_context_configure) bool wgpu_canvas_context_configure(WGpuCanvasContext context, const WGpuCanvasConfiguration* config);
wgpu_js(wgpu_canvas_context_get_current_texture_view) WGpuTextureView wgpu_canvas_context_get_current_texture_view(WGpuCanvasContext context);

wgpu_js(wgpu_device_create_shader_module) WGpuShaderModule wgpu_device_create_shader_module(WGpuDevice device, const WGpuShaderModuleDescriptor* descriptor);
wgpu_js(wgpu_device_create_sampler) WGpuSampler wgpu_device_create_sampler(WGpuDevice device, const WGpuSamplerDescriptor* descriptor);
wgpu_js(wgpu_device_create_buffer) WGpuBuffer wgpu_device_create_buffer(WGpuDevice device, const WGpuBufferDescriptor* descriptor);
wgpu_js(wgpu_device_create_texture) WGpuTexture wgpu_device_create_texture(WGpuDevice device, const WGpuTextureDescriptor* descriptor);
wgpu_js(wgpu_device_create_bind_group_layout) WGpuBindGroupLayout wgpu_device_create_bind_group_layout(WGpuDevice device, const WGpuBindGroupLayoutEntry* entries, int entry_count);
wgpu_js(wgpu_device_create_bind_group) WGpuBindGroup wgpu_device_create_bind_group(WGpuDevice device, WGpuBindGroupLayout layout, const WGpuBindGroupEntry* entries, int entry_count);
wgpu_js(wgpu_device_create_pipeline_layout) WGpuPipelineLayout wgpu_device_create_pipeline_layout(WGpuDevice device, const WGpuBindGroupLayout* layouts, int layout_count);
wgpu_js(wgpu_device_create_render_pipeline_async) WGpuRenderPipeline wgpu_device_create_render_pipeline_async(WGpuDevice device, const WGpuRenderPipelineDescriptor* descriptor, WGpuCreatePipelineCallback callback, void* user);
wgpu_js(wgpu_device_create_compute_pipeline_async) WGpuComputePipeline wgpu_device_create_compute_pipeline_async(WGpuDevice device, WGpuShaderModule module, const char* entry_point, WGpuPipelineLayout layout, WGpuCreatePipelineCallback callback, void* user);
wgpu_js(wgpu_device_create_query_set) WGpuQuerySet wgpu_device_create_query_set(WGpuDevice device, const WGpuQuerySetDescriptor* descriptor);
wgpu_js(wgpu_device_create_command_encoder_simple) WGpuCommandEncoder wgpu_device_create_command_encoder_simple(WGpuDevice device);

wgpu_js(wgpu_texture_create_view_simple) WGpuTextureView wgpu_texture_create_view_simple(WGpuTexture texture);
wgpu_js(wgpu_texture_view_get_format) WGPU_TEXTURE_FORMAT wgpu_texture_view_get_format(WGpuTextureView view);
wgpu_js(wgpu_texture_view_get_usage) WGPU_TEXTURE_USAGE_FLAGS wgpu_texture_view_get_usage(WGpuTextureView view);
wgpu_js(wgpu_texture_get_usage) WGPU_TEXTURE_USAGE_FLAGS wgpu_texture_get_usage(WGpuTexture texture);
wgpu_js(wgpu_texture_create_view_mip) WGpuTextureView wgpu_texture_create_view_mip(WGpuTexture texture, uint32_t level);

wgpu_js(wgpu_queue_write_buffer) void wgpu_queue_write_buffer(WGpuQueue queue, WGpuBuffer buffer, double_int53_t buffer_offset, const void* data, double_int53_t size);
wgpu_js(wgpu_queue_write_texture) void wgpu_queue_write_texture(WGpuQueue queue, const WGpuTexelCopyTextureInfo* destination, const void* data, uint32_t bytes_per_block_row, uint32_t block_rows_per_image, uint32_t write_width, uint32_t write_height, uint32_t write_depth);
wgpu_js(wgpu_queue_set_on_submitted_work_done_callback) void wgpu_queue_set_on_submitted_work_done_callback(WGpuQueue queue, WGpuOnSubmittedWorkDoneCallback callback, void* user);
wgpu_js(wgpu_queue_submit_one_and_destroy) void wgpu_queue_submit_one_and_destroy(WGpuQueue queue, WGpuCommandBuffer command_buffer);

wgpu_js(wgpu_buffer_map_async) void wgpu_buffer_map_async(WGpuBuffer buffer, WGpuBufferMapCallback callback, void* user, WGPU_MAP_MODE_FLAGS mode, double_int53_t offset, double_int53_t size);
wgpu_js(wgpu_command_encoder_copy_texture_to_buffer) void wgpu_command_encoder_copy_texture_to_buffer(WGpuCommandEncoder encoder, WGpuTexture source, WGpuBuffer destination, uint32_t bytes_per_row, uint32_t width, uint32_t height);
wgpu_js(wgpu_buffer_get_mapped_range) int wgpu_buffer_get_mapped_range(WGpuBuffer buffer, double_int53_t offset, double_int53_t size);
wgpu_js(wgpu_buffer_read_mapped_range) void wgpu_buffer_read_mapped_range(WGpuBuffer buffer, double_int53_t start_offset, double_int53_t sub_offset, void* dst, double_int53_t size);
wgpu_js(wgpu_buffer_unmap) void wgpu_buffer_unmap(WGpuBuffer buffer);

wgpu_js(wgpu_command_encoder_begin_render_pass) WGpuRenderPassEncoder wgpu_command_encoder_begin_render_pass(WGpuCommandEncoder encoder, const WGpuRenderPassDescriptor* descriptor);
wgpu_js(wgpu_command_encoder_begin_compute_pass) WGpuComputePassEncoder wgpu_command_encoder_begin_compute_pass(WGpuCommandEncoder encoder, const WGpuComputePassDescriptor* descriptor);
wgpu_js(wgpu_command_encoder_copy_buffer_to_buffer) void wgpu_command_encoder_copy_buffer_to_buffer(WGpuCommandEncoder encoder, WGpuBuffer source, double_int53_t source_offset, WGpuBuffer destination, double_int53_t destination_offset, double_int53_t size);
wgpu_js(wgpu_command_encoder_resolve_query_set) void wgpu_command_encoder_resolve_query_set(WGpuCommandEncoder encoder, WGpuQuerySet query_set, uint32_t first_query, uint32_t query_count, WGpuBuffer destination, double_int53_t destination_offset);
wgpu_js(wgpu_command_encoder_finish) WGpuCommandBuffer wgpu_command_encoder_finish(WGpuCommandEncoder encoder);

wgpu_js(wgpu_render_pass_encoder_set_pipeline) void wgpu_render_pass_encoder_set_pipeline(WGpuRenderPassEncoder pass, WGpuRenderPipeline pipeline);
wgpu_js(wgpu_render_pass_encoder_set_bind_group) void wgpu_render_pass_encoder_set_bind_group(WGpuRenderPassEncoder pass, uint32_t index, WGpuBindGroup group, const uint32_t* dynamic_offsets, uint32_t dynamic_offset_count);
wgpu_js(wgpu_render_pass_encoder_set_scissor_rect) void wgpu_render_pass_encoder_set_scissor_rect(WGpuRenderPassEncoder pass, uint32_t x, uint32_t y, uint32_t width, uint32_t height);
wgpu_js(wgpu_render_pass_encoder_draw) void wgpu_render_pass_encoder_draw(WGpuRenderPassEncoder pass, uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance);
wgpu_js(wgpu_render_pass_encoder_end) void wgpu_render_pass_encoder_end(WGpuRenderPassEncoder pass);

wgpu_js(wgpu_compute_pass_encoder_set_pipeline) void wgpu_compute_pass_encoder_set_pipeline(WGpuComputePassEncoder pass, WGpuComputePipeline pipeline);
wgpu_js(wgpu_compute_pass_encoder_set_bind_group) void wgpu_compute_pass_encoder_set_bind_group(WGpuComputePassEncoder pass, uint32_t index, WGpuBindGroup group, const uint32_t* dynamic_offsets, uint32_t dynamic_offset_count);
wgpu_js(wgpu_compute_pass_encoder_dispatch_workgroups) void wgpu_compute_pass_encoder_dispatch_workgroups(WGpuComputePassEncoder pass, uint32_t x, uint32_t y, uint32_t z);
wgpu_js(wgpu_compute_pass_encoder_dispatch_workgroups_indirect) void wgpu_compute_pass_encoder_dispatch_workgroups_indirect(WGpuComputePassEncoder pass, WGpuBuffer indirect_buffer, double_int53_t indirect_offset);
wgpu_js(wgpu_compute_pass_encoder_end) void wgpu_compute_pass_encoder_end(WGpuComputePassEncoder pass);

#define wgpu_is_valid_type(object, type_) (wgpu_object_type_(object) == (type_))
#define wgpu_is_adapter(object) wgpu_is_valid_type(object, wgpu_object_type_adapter)
#define wgpu_is_device(object) wgpu_is_valid_type(object, wgpu_object_type_device)
#define wgpu_is_queue(object) wgpu_is_valid_type(object, wgpu_object_type_queue)
#define wgpu_is_canvas_context(object) wgpu_is_valid_type(object, wgpu_object_type_canvas_context)
#define wgpu_is_buffer(object) wgpu_is_valid_type(object, wgpu_object_type_buffer)
#define wgpu_is_texture(object) wgpu_is_valid_type(object, wgpu_object_type_texture)
#define wgpu_is_texture_view(object) wgpu_is_valid_type(object, wgpu_object_type_texture_view)
#define wgpu_is_sampler(object) wgpu_is_valid_type(object, wgpu_object_type_sampler)
#define wgpu_is_bind_group_layout(object) wgpu_is_valid_type(object, wgpu_object_type_bind_group_layout)
#define wgpu_is_bind_group(object) wgpu_is_valid_type(object, wgpu_object_type_bind_group)
#define wgpu_is_pipeline_layout(object) wgpu_is_valid_type(object, wgpu_object_type_pipeline_layout)
#define wgpu_is_shader_module(object) wgpu_is_valid_type(object, wgpu_object_type_shader_module)
#define wgpu_is_render_pipeline(object) wgpu_is_valid_type(object, wgpu_object_type_render_pipeline)
#define wgpu_is_compute_pipeline(object) wgpu_is_valid_type(object, wgpu_object_type_compute_pipeline)
#define wgpu_is_command_encoder(object) wgpu_is_valid_type(object, wgpu_object_type_command_encoder)
#define wgpu_is_query_set(object) wgpu_is_valid_type(object, wgpu_object_type_query_set)
