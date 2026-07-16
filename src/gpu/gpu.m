#include "gpu.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if cr_macos || cr_ios
#import <Metal/Metal.h>
#else
#include "wgpu.h"
#endif

#define gpu_max_groups 4
#define gpu_max_bindings 16
#define gpu_max_compute_slots 32

#define gpu_timeline_size 256u
#define gpu_timeline_readbacks 4u
#define gpu_profiler_slots 16u
#define gpu_profiler_slot_samples (2u * gpu_profile_max_passes)
#define gpu_compile_label_max 96u

struct gpu_compile {
    uint64_t serial;
    bool ready;
    char label[gpu_compile_label_max];
#if cr_macos || cr_ios
    dispatch_group_t mtl_group;
    char* mtl_error;
#endif
};

struct gpu_readback {
    void* out;
    uint32_t rows;
    uint32_t src_stride;
    uint32_t dst_stride;
#if cr_macos || cr_ios
    id<MTLBuffer> mtl_buffer;
#elif cr_wasm
    WGpuBuffer wgpu_buffer;
#endif
};

struct gpu_timeline_entry {
    uint64_t serial;
    bool committed;
    bool completed;
    bool failed;
    struct gpu_compile* compile;
    uint32_t readback_count;
    struct gpu_readback readbacks[gpu_timeline_readbacks];
#if cr_macos || cr_ios
    id<MTLCommandBuffer> mtl_command_buffer;
#elif cr_wasm
    struct gpu_device* device;
    uint32_t pending;
#endif
};

struct gpu_device {
    struct cr_mem* mem;
    uint64_t next_serial;
    uint64_t completed_serial;
    uint64_t pipelines_serial;
    struct gpu_timeline_entry timeline[gpu_timeline_size];
    uint32_t profiler_slot_mask;
#if cr_macos || cr_ios
    id<MTLDevice> mtl_device;
    id<MTLCommandQueue> mtl_queue;
    id<MTLCounterSampleBuffer> mtl_profiler_samples;
#elif cr_wasm
    WGpuDevice wgpu_device;
    WGpuQueue wgpu_queue;
    WGpuQuerySet wgpu_profiler_query_set;
    void (*on_completed)(void* user);
    void* on_completed_user;
#endif
};

struct gpu_library {
#if cr_macos || cr_ios
    id<MTLLibrary> mtl_library;
#elif cr_wasm
    char* code;
    size_t size;
#endif
};

struct gpu_sampler {
#if cr_macos || cr_ios
    id<MTLSamplerState> mtl_sampler;
#elif cr_wasm
    WGpuSampler wgpu_sampler;
#endif
};

struct gpu_buffer {
    uint32_t usage;
#if cr_macos || cr_ios
    id<MTLBuffer> mtl_buffer;
#elif cr_wasm
    WGpuBuffer wgpu_buffer;
#endif
};

struct gpu_texture {
    enum gpu_format format;
    uint32_t usage;
    uint2 size;
#if cr_macos || cr_ios
    id<MTLTexture> mtl_texture;
#elif cr_wasm
    WGpuTexture wgpu_texture;
    WGpuTextureView wgpu_view;
    bool owns_texture;
    bool owns_view;
#endif
};

struct gpu_bind_layout {
    uint32_t binding_count;
    struct gpu_binding_desc bindings[gpu_max_bindings];
    uint8_t ordinals[gpu_max_bindings];
    uint32_t buffer_count;
    uint32_t texture_count;
    uint32_t sampler_count;
    uint32_t dynamic_count;
    uint8_t dynamic_indices[gpu_max_bindings];
#if cr_wasm
    WGpuBindGroupLayout wgpu_layout;
#endif
};

struct gpu_bind_group {
    struct gpu_bind_layout* layout;
#if cr_macos || cr_ios
    struct gpu_bind_desc binds[gpu_max_bindings];
#elif cr_wasm
    WGpuBindGroup wgpu_group;
#endif
};

struct gpu_slot_base {
    uint32_t buffer;
    uint32_t texture;
    uint32_t sampler;
};

struct gpu_render_pipeline {
    struct gpu_compile compile;
    struct gpu_slot_base bases[gpu_max_groups];
#if cr_macos || cr_ios
    id<MTLRenderPipelineState> mtl_pipeline;
#elif cr_wasm
    WGpuRenderPipeline wgpu_pipeline;
#endif
};

struct gpu_compute_pipeline {
    struct gpu_compile compile;
    struct gpu_slot_base bases[gpu_max_groups];
    uint2 workgroup_size;
#if cr_macos || cr_ios
    id<MTLComputePipelineState> mtl_pipeline;
#elif cr_wasm
    WGpuComputePipeline wgpu_pipeline;
#endif
};

struct gpu_render_pass {
    struct gpu_command_buffer* command_buffer;
    struct gpu_render_pipeline* pipeline;
    uint32_t profile_pair;
    struct {
        struct gpu_bind_group* group;
        struct gpu_slot_base base;
    } applied[gpu_max_groups];
#if cr_macos || cr_ios
    id<MTLRenderCommandEncoder> mtl_enc;
#elif cr_wasm
    WGpuRenderPassEncoder wgpu_enc;
#endif
};

struct gpu_compute_pass {
    struct gpu_command_buffer* command_buffer;
    struct gpu_compute_pipeline* pipeline;
    uint32_t profile_pair;
#if cr_macos || cr_ios
    id<MTLComputeCommandEncoder> mtl_enc;
    bool barrier_between_dispatches;
    bool has_dispatch;
    id<MTLBuffer> mtl_bound_buffers[gpu_max_compute_slots];
    uint32_t bound_offsets[gpu_max_compute_slots];
    id<MTLTexture> mtl_bound_textures[gpu_max_compute_slots];
    id<MTLSamplerState> mtl_bound_samplers[gpu_max_compute_slots];
#elif cr_wasm
    WGpuComputePassEncoder wgpu_enc;
#endif
};

struct gpu_profiler {
    struct gpu_device* device;
    uint32_t slot;
    uint32_t max_count;
    uint32_t count;
    uint32_t used_mask[gpu_profile_max_passes / 32];
    uint64_t serial;
    bool resolved;
    struct gpu_profile profile;
#if cr_wasm
    WGpuBuffer wgpu_resolve;
    uint64_t stamps[gpu_profiler_slot_samples];
#endif
};

struct gpu_command_buffer {
    struct gpu_device* device;
    uint64_t serial;
    bool pass_active;
    struct gpu_profiler* profile;
    struct gpu_render_pass render_pass;
    struct gpu_compute_pass compute_pass;
#if cr_macos || cr_ios
    id<MTLCommandBuffer> mtl_command_buffer;
#elif cr_wasm
    WGpuCommandEncoder wgpu_enc;
#endif
};

static struct gpu_timeline_entry* gpu_timeline_slot(struct gpu_device* device, uint64_t serial) {
    return &device->timeline[serial % gpu_timeline_size];
}

static struct gpu_timeline_entry* gpu_timeline_find(struct gpu_device* device, uint64_t serial) {
    assert(serial < device->next_serial);
    if (serial == 0) {
        return NULL;
    }
    struct gpu_timeline_entry* entry = gpu_timeline_slot(device, serial);
    return entry->serial == serial ? entry : NULL;
}

#if cr_macos || cr_ios
static void gpu_compile_finish(struct gpu_timeline_entry* entry) {
    struct gpu_compile* compile = entry->compile;
    entry->compile = NULL;
    entry->completed = true;
    entry->failed = compile->mtl_error != NULL;
    compile->ready = entry->failed == false;
    cr_check(entry->failed == false, "the pipeline %s could not be created: %s", compile->label, compile->mtl_error);
}

static bool gpu_timeline_poll(struct gpu_timeline_entry* entry) {
    if (entry->completed || entry->committed == false) {
        return entry->completed;
    }
    if (entry->compile != NULL) {
        if (dispatch_group_wait(entry->compile->mtl_group, DISPATCH_TIME_NOW) != 0) {
            return false;
        }
        gpu_compile_finish(entry);
        return true;
    }
    MTLCommandBufferStatus status = entry->mtl_command_buffer.status;
    if (status < MTLCommandBufferStatusCompleted) {
        return false;
    }
    entry->failed = status == MTLCommandBufferStatusError;
    for (uint32_t i = 0; i < entry->readback_count; i++) {
        struct gpu_readback* readback = &entry->readbacks[i];
        if (entry->failed) {
            memset(readback->out, 0, (size_t)readback->rows * readback->dst_stride);
        } else {
            const uint8_t* src = readback->mtl_buffer.contents;
            for (uint32_t row = 0; row < readback->rows; row++) {
                memcpy((uint8_t*)readback->out + (uint64_t)row * readback->dst_stride, src + (uint64_t)row * readback->src_stride, readback->dst_stride);
            }
        }
        [readback->mtl_buffer release];
    }
    [entry->mtl_command_buffer release];
    entry->mtl_command_buffer = nil;
    entry->completed = true;
    return true;
}
#elif cr_wasm
static bool gpu_timeline_poll(struct gpu_timeline_entry* entry) {
    return entry->completed;
}

static void gpu_timeline_resolve(struct gpu_timeline_entry* entry) {
    assert(entry->pending > 0);
    entry->pending--;
    if (entry->pending > 0) {
        return;
    }
    entry->completed = true;
    struct gpu_device* device = entry->device;
    if (device->on_completed != NULL) {
        device->on_completed(device->on_completed_user);
    }
}

static void gpu_timeline_on_work_done(WGpuQueue wgpu_queue, void* user) {
    gpu_timeline_resolve(user);
}

static void gpu_compile_on_created(WGpuObjectBase wgpu_pipeline, void* user, const char* message) {
    struct gpu_timeline_entry* entry = user;
    struct gpu_compile* compile = entry->compile;
    char label[gpu_compile_label_max];
    memcpy(label, compile->label, sizeof(label));
    entry->compile = NULL;
    entry->failed = message != NULL;
    compile->ready = message == NULL;
    gpu_timeline_resolve(entry);
    cr_check(message == NULL, "the pipeline %s could not be created: %s", label, message);
}

static void gpu_timeline_on_map(WGpuBuffer wgpu_buffer, void* user, WGPU_MAP_MODE_FLAGS mode, double_int53_t offset, double_int53_t size) {
    struct gpu_timeline_entry* entry = user;
    bool mapped = wgpu_buffer_get_mapped_range(wgpu_buffer, 0, size) == 0;
    entry->failed = entry->failed || mapped == false;
    for (uint32_t i = 0; i < entry->readback_count; i++) {
        struct gpu_readback* readback = &entry->readbacks[i];
        if (readback->wgpu_buffer != wgpu_buffer) {
            continue;
        }
        if (readback->out != NULL && mapped == false) {
            memset(readback->out, 0, (size_t)readback->rows * readback->dst_stride);
        } else if (readback->out != NULL) {
            for (uint32_t row = 0; row < readback->rows; row++) {
                wgpu_buffer_read_mapped_range(wgpu_buffer, 0, (double_int53_t)row * readback->src_stride, (uint8_t*)readback->out + (uint64_t)row * readback->dst_stride, readback->dst_stride);
            }
        }
        wgpu_object_destroy(wgpu_buffer);
        readback->wgpu_buffer = 0;
    }
    gpu_timeline_resolve(entry);
}

void gpu_device_set_on_completed(struct gpu_device* device, void (*func)(void* user), void* user) {
    device->on_completed = func;
    device->on_completed_user = user;
}
#endif

bool gpu_device_is_completed(struct gpu_device* device, uint64_t serial) {
    cr_check(serial < device->next_serial, "the serial %llu was never issued by this device", (unsigned long long)serial);
    bool contiguous = true;
    for (uint64_t s = device->completed_serial + 1; s <= serial; s++) {
        struct gpu_timeline_entry* entry = gpu_timeline_slot(device, s);
        assert(entry->serial == s);
        if (entry->committed == false && s == serial) {
            return false;
        }
        if (entry->committed && gpu_timeline_poll(entry) == false) {
            return false;
        }
        contiguous = contiguous && entry->committed;
        if (contiguous) {
            device->completed_serial = s;
        }
    }
    return true;
}

uint64_t gpu_device_get_oldest_incomplete_serial(struct gpu_device* device) {
    for (uint64_t s = device->completed_serial + 1; s < device->next_serial; s++) {
        struct gpu_timeline_entry* entry = gpu_timeline_slot(device, s);
        assert(entry->serial == s);
        if (entry->committed == false || gpu_timeline_poll(entry) == false) {
            return s;
        }
        device->completed_serial = s;
    }
    return device->next_serial;
}

bool gpu_device_is_failed(struct gpu_device* device, uint64_t serial) {
    cr_check(serial < device->next_serial, "the serial %llu was never issued by this device", (unsigned long long)serial);
    struct gpu_timeline_entry* entry = gpu_timeline_find(device, serial);
    return entry != NULL && entry->failed;
}

uint64_t gpu_device_get_pipelines_serial(struct gpu_device* device) {
    return device->pipelines_serial;
}

#if cr_macos || cr_ios
void gpu_device_wait(struct gpu_device* device, uint64_t serial) {
    cr_check(serial < device->next_serial, "the serial %llu was never issued by this device", (unsigned long long)serial);
    for (uint64_t s = device->completed_serial + 1; s <= serial; s++) {
        struct gpu_timeline_entry* entry = gpu_timeline_slot(device, s);
        if (entry->committed == false || entry->completed) {
            continue;
        }
        if (entry->compile != NULL) {
            dispatch_group_wait(entry->compile->mtl_group, DISPATCH_TIME_FOREVER);
        } else {
            [entry->mtl_command_buffer waitUntilCompleted];
        }
    }
    bool completed = gpu_device_is_completed(device, serial);
    assert(completed);
}
#endif

static struct gpu_timeline_entry* gpu_timeline_acquire(struct gpu_device* device) {
    uint64_t serial = device->next_serial;
    struct gpu_timeline_entry* entry = gpu_timeline_slot(device, serial);
    if (entry->serial != 0) {
        cr_check(entry->committed, "the gpu timeline is full of %u uncommitted command buffers; commit or discard them", (uint32_t)gpu_timeline_size);
#if cr_macos || cr_ios
        gpu_device_wait(device, entry->serial);
#endif
        bool completed = gpu_device_is_completed(device, entry->serial);
        assert(completed && "the timeline is full of work the gpu has not completed");
    }
    device->next_serial++;
    memset(entry, 0, sizeof(*entry));
    entry->serial = serial;
#if cr_wasm
    entry->device = device;
#endif
    return entry;
}

static struct gpu_timeline_entry* gpu_compile_begin(struct gpu_device* device, struct gpu_compile* compile) {
    struct gpu_timeline_entry* entry = gpu_timeline_acquire(device);
    entry->committed = true;
    entry->compile = compile;
    compile->serial = entry->serial;
    device->pipelines_serial = entry->serial;
#if cr_macos || cr_ios
    compile->mtl_group = dispatch_group_create();
    dispatch_group_enter(compile->mtl_group);
#elif cr_wasm
    entry->pending = 1;
#endif
    return entry;
}

static void gpu_compile_end(struct gpu_device* device, struct gpu_compile* compile) {
#if cr_macos || cr_ios
    dispatch_group_wait(compile->mtl_group, DISPATCH_TIME_FOREVER);
#endif
    struct gpu_timeline_entry* entry = gpu_timeline_find(device, compile->serial);
    if (entry != NULL && entry->compile == compile) {
        entry->compile = NULL;
        entry->completed = true;
#if cr_macos || cr_ios
        entry->failed = compile->mtl_error != NULL;
#endif
    }
#if cr_macos || cr_ios
    dispatch_release(compile->mtl_group);
    free(compile->mtl_error);
#endif
}

static void gpu_compile_require(struct gpu_device* device, struct gpu_compile* compile) {
    if (compile->ready) {
        return;
    }
    struct gpu_timeline_entry* entry = gpu_timeline_find(device, compile->serial);
    if (entry != NULL) {
        gpu_timeline_poll(entry);
    }
    cr_check(compile->ready, "the pipeline %s is still compiling; wait for its serial before using it", compile->label);
}

cr_hdef bool gpu_binding_is_buffer(enum gpu_binding_type type) {
    return type == gpu_binding_type_uniform || type == gpu_binding_type_storage || type == gpu_binding_type_storage_read;
}

cr_hdef bool gpu_binding_is_texture(enum gpu_binding_type type) {
    return type == gpu_binding_type_texture || type == gpu_binding_type_storage_texture;
}

#if cr_macos || cr_ios

static MTLPixelFormat gpu_mtl_format(enum gpu_format format) {
    switch (format) {
    case gpu_format_bgra8: return MTLPixelFormatBGRA8Unorm;
    case gpu_format_r8: return MTLPixelFormatR8Unorm;
    case gpu_format_rgba16f: return MTLPixelFormatRGBA16Float;
    case gpu_format_r32f: return MTLPixelFormatR32Float;
    }
}

#elif cr_wasm

static WGPU_TEXTURE_FORMAT gpu_wgpu_format(enum gpu_format format) {
    switch (format) {
    case gpu_format_bgra8: return WGPU_TEXTURE_FORMAT_BGRA8UNORM;
    case gpu_format_r8: return WGPU_TEXTURE_FORMAT_R8UNORM;
    case gpu_format_rgba16f: return WGPU_TEXTURE_FORMAT_RGBA16FLOAT;
    case gpu_format_r32f: return WGPU_TEXTURE_FORMAT_R32FLOAT;
    }
}

static WGPU_SHADER_STAGE_FLAGS gpu_wgpu_stages(uint32_t stages) {
    WGPU_SHADER_STAGE_FLAGS flags = 0;
    if (stages & gpu_stage_vertex) {
        flags |= WGPU_SHADER_STAGE_VERTEX;
    }
    if (stages & gpu_stage_fragment) {
        flags |= WGPU_SHADER_STAGE_FRAGMENT;
    }
    if (stages & gpu_stage_compute) {
        flags |= WGPU_SHADER_STAGE_COMPUTE;
    }
    return flags;
}

static WGpuPipelineLayout gpu_wgpu_pipeline_layout(struct gpu_device* device, struct gpu_bind_layout* const* layouts, uint32_t layout_count) {
    WGpuBindGroupLayout wgpu_layouts[gpu_max_groups];
    for (uint32_t i = 0; i < layout_count; i++) {
        wgpu_layouts[i] = layouts[i]->wgpu_layout;
    }

    WGpuPipelineLayout wgpu_layout = wgpu_device_create_pipeline_layout(device->wgpu_device, wgpu_layouts, (int)layout_count);
    cr_check(wgpu_is_pipeline_layout(wgpu_layout), "the pipeline layout could not be created");
    return wgpu_layout;
}

static const char* gpu_wgpu_module_code(struct gpu_library* library, const char* name) {
    char pattern[256];
    cr_check(snprintf(pattern, sizeof(pattern), "\nfn %s(", name) < (int)sizeof(pattern), "the entry point name %s is too long", name);

    const char* code = library->code;
    while (code < library->code + library->size && strstr(code, pattern) == NULL) {
        code += strlen(code) + 1;
    }
    cr_check(code < library->code + library->size, "the shader library has no entry point %s", name);
    return code;
}

static WGpuShaderModule gpu_wgpu_shader_module(struct gpu_device* device, const char* code, const char* name) {
    WGpuShaderModuleDescriptor wgpu_desc = { 0 };
    wgpu_desc.code = code;

    WGpuShaderModule wgpu_shader_module = wgpu_device_create_shader_module(device->wgpu_device, &wgpu_desc);
    cr_check(wgpu_is_shader_module(wgpu_shader_module), "the shader module with the entry point %s could not be created", name);
    return wgpu_shader_module;
}

#endif

static void gpu_slot_bases(struct gpu_bind_layout* const* layouts, uint32_t layout_count, struct gpu_slot_base* bases) {
    struct gpu_slot_base base = { 0 };

    for (uint32_t i = 0; i < layout_count; i++) {
        bases[i] = base;
        base.buffer += layouts[i]->buffer_count;
        base.texture += layouts[i]->texture_count;
        base.sampler += layouts[i]->sampler_count;
    }
}

#if cr_macos || cr_ios
struct gpu_device* gpu_device_create(struct cr_mem* mem)
#elif cr_wasm
struct gpu_device* gpu_device_create(struct cr_mem* mem, cr_wgpu_t wgpu_device, cr_wgpu_t wgpu_queue)
#endif
{
    struct gpu_device* device = cr_malloc(mem, sizeof(*device), 16, cr_loc);
    memset(device, 0, sizeof(*device));
    device->mem = mem;
    device->next_serial = 1;

#if cr_macos || cr_ios
    device->mtl_device = MTLCreateSystemDefaultDevice();
    device->mtl_queue = [device->mtl_device newCommandQueue];
    device->mtl_queue.label = @"gpu queue";
    assert(device->mtl_device.hasUnifiedMemory && [device->mtl_device supportsFamily:MTLGPUFamilyApple7] && "metal requires apple silicon");

    id<MTLCounterSet> mtl_counter_set = nil;
    for (id<MTLCounterSet> mtl_set in device->mtl_device.counterSets) {
        if ([mtl_set.name isEqualToString:MTLCommonCounterSetTimestamp]) {
            mtl_counter_set = mtl_set;
        }
    }

    if (mtl_counter_set != nil && [device->mtl_device supportsCounterSampling:MTLCounterSamplingPointAtStageBoundary]) {
        MTLCounterSampleBufferDescriptor* mtl_samples_desc = [[MTLCounterSampleBufferDescriptor alloc] init];
        mtl_samples_desc.label = @"gpu profiler samples";
        mtl_samples_desc.counterSet = mtl_counter_set;
        mtl_samples_desc.storageMode = MTLStorageModeShared;
        mtl_samples_desc.sampleCount = gpu_profiler_slots * gpu_profiler_slot_samples;
        device->mtl_profiler_samples = [device->mtl_device newCounterSampleBufferWithDescriptor:mtl_samples_desc error:NULL];
        assert(device->mtl_profiler_samples);
        [mtl_samples_desc release];
    }
#elif cr_wasm
    assert(wgpu_is_device(wgpu_device));
    assert(wgpu_is_queue(wgpu_queue));
    device->wgpu_device = wgpu_device;
    device->wgpu_queue = wgpu_queue;

    if (wgpu_adapter_or_device_get_features(device->wgpu_device) & WGPU_FEATURE_TIMESTAMP_QUERY) {
        WGpuQuerySetDescriptor wgpu_query_desc = { .type = WGPU_QUERY_TYPE_TIMESTAMP, .count = gpu_profiler_slots * gpu_profiler_slot_samples };
        device->wgpu_profiler_query_set = wgpu_device_create_query_set(device->wgpu_device, &wgpu_query_desc);
        assert(wgpu_is_query_set(device->wgpu_profiler_query_set));
    }
#endif

    return device;
}

void gpu_device_destroy(struct gpu_device* device) {
    cr_check(device->profiler_slot_mask == 0, "profilers are destroyed before their device");
#if cr_macos || cr_ios
    gpu_device_wait(device, device->next_serial - 1);
    [device->mtl_profiler_samples release];
    [device->mtl_device release];
    [device->mtl_queue release];
#elif cr_wasm
    for (uint32_t i = 0; i < gpu_timeline_size; i++) {
        struct gpu_timeline_entry* entry = &device->timeline[i];
        for (uint32_t r = 0; r < entry->readback_count; r++) {
            wgpu_object_destroy(entry->readbacks[r].wgpu_buffer);
        }
    }
    if (device->wgpu_profiler_query_set) {
        wgpu_object_destroy(device->wgpu_profiler_query_set);
    }
    wgpu_object_destroy(device->wgpu_queue);
    wgpu_object_destroy(device->wgpu_device);
#endif
    cr_free(device->mem, device, cr_loc);
}

struct gpu_library* gpu_library_create(struct gpu_device* device, const void* data, uint64_t size, const char* label) {
    cr_check(data != NULL && size > 0, "a library needs the data of a shader file");
    cr_check(label != NULL, "a library needs a label");

    struct gpu_library* library = cr_malloc(device->mem, sizeof(*library), 16, cr_loc);
    memset(library, 0, sizeof(*library));

#if cr_macos || cr_ios
    dispatch_data_t dispatch_data = dispatch_data_create(data, size, NULL, DISPATCH_DATA_DESTRUCTOR_DEFAULT);
    library->mtl_library = [device->mtl_device newLibraryWithData:dispatch_data error:NULL];
    dispatch_release(dispatch_data);
    cr_check(library->mtl_library != nil, "the shader library %s could not be loaded", label);
    library->mtl_library.label = [NSString stringWithFormat:@"%s", label];
#elif cr_wasm
    library->size = (size_t)size;
    library->code = cr_malloc(device->mem, library->size + 1, 0, cr_loc);
    memcpy(library->code, data, library->size);
    library->code[library->size] = 0;
#endif

    return library;
}

void gpu_library_destroy(struct gpu_device* device, struct gpu_library* library) {
#if cr_macos || cr_ios
    [library->mtl_library release];
#elif cr_wasm
    cr_free(device->mem, library->code, cr_loc);
#endif
    cr_free(device->mem, library, cr_loc);
}

const char* gpu_library_get_extension(void) {
#if cr_macos || cr_ios
    return ".metallib";
#elif cr_wasm
    return ".wgsl";
#endif
}

struct gpu_sampler* gpu_sampler_create(struct gpu_device* device, const struct gpu_sampler_desc* desc) {
    cr_check(desc->label != NULL, "a sampler needs a label");

    struct gpu_sampler* sampler = cr_malloc(device->mem, sizeof(*sampler), 16, cr_loc);
    memset(sampler, 0, sizeof(*sampler));

#if cr_macos || cr_ios
    MTLSamplerAddressMode address_mode = desc->repeat ? MTLSamplerAddressModeRepeat : MTLSamplerAddressModeClampToEdge;
    MTLSamplerMinMagFilter filter = desc->filter == gpu_filter_nearest ? MTLSamplerMinMagFilterNearest : MTLSamplerMinMagFilterLinear;
    MTLSamplerDescriptor* mtl_desc = [[MTLSamplerDescriptor alloc] init];
    mtl_desc.label = [NSString stringWithFormat:@"%s", desc->label];
    mtl_desc.rAddressMode = address_mode;
    mtl_desc.sAddressMode = address_mode;
    mtl_desc.tAddressMode = address_mode;
    mtl_desc.minFilter = filter;
    mtl_desc.magFilter = filter;
    if (desc->mip) {
        mtl_desc.mipFilter = MTLSamplerMipFilterLinear;
    }
    sampler->mtl_sampler = [device->mtl_device newSamplerStateWithDescriptor:mtl_desc];
    cr_check(sampler->mtl_sampler != nil, "the sampler %s could not be created", desc->label);
    [mtl_desc release];
#elif cr_wasm
    uint32_t address_mode = desc->repeat ? WGPU_ADDRESS_MODE_REPEAT : WGPU_ADDRESS_MODE_CLAMP_TO_EDGE;
    uint32_t filter = desc->filter == gpu_filter_nearest ? WGPU_FILTER_MODE_NEAREST : WGPU_FILTER_MODE_LINEAR;
    WGpuSamplerDescriptor wgpu_desc = WGPU_SAMPLER_DESCRIPTOR_DEFAULT_INITIALIZER;
    wgpu_desc.addressModeU = address_mode;
    wgpu_desc.addressModeV = address_mode;
    wgpu_desc.addressModeW = address_mode;
    wgpu_desc.minFilter = filter;
    wgpu_desc.magFilter = filter;
    if (desc->mip) {
        wgpu_desc.mipmapFilter = WGPU_FILTER_MODE_LINEAR;
    }
    sampler->wgpu_sampler = wgpu_device_create_sampler(device->wgpu_device, &wgpu_desc);
    cr_check(wgpu_is_sampler(sampler->wgpu_sampler), "the sampler %s could not be created", desc->label);
#endif

    return sampler;
}

void gpu_sampler_destroy(struct gpu_device* device, struct gpu_sampler* sampler) {
#if cr_macos || cr_ios
    [sampler->mtl_sampler release];
#elif cr_wasm
    wgpu_object_destroy(sampler->wgpu_sampler);
#endif
    cr_free(device->mem, sampler, cr_loc);
}

struct gpu_buffer* gpu_buffer_create(struct gpu_device* device, uint32_t size, uint32_t usage, const char* label) {
    cr_check(label != NULL, "a buffer needs a label");

    struct gpu_buffer* buffer = cr_malloc(device->mem, sizeof(*buffer), 16, cr_loc);
    memset(buffer, 0, sizeof(*buffer));
    buffer->usage = usage;

#if cr_macos || cr_ios
    MTLResourceOptions options = (usage & gpu_buffer_usage_write) ? MTLResourceStorageModeShared : MTLResourceStorageModePrivate;
    buffer->mtl_buffer = [device->mtl_device newBufferWithLength:size options:options];
    cr_check(buffer->mtl_buffer != nil, "the buffer %s with %u bytes could not be created", label, size);
    buffer->mtl_buffer.label = [NSString stringWithFormat:@"%s", label];
#elif cr_wasm
    WGpuBufferDescriptor wgpu_desc = { 0 };
    wgpu_desc.size = size;
    if (usage & gpu_buffer_usage_uniform) {
        wgpu_desc.usage |= WGPU_BUFFER_USAGE_UNIFORM;
    }
    if (usage & gpu_buffer_usage_storage) {
        wgpu_desc.usage |= WGPU_BUFFER_USAGE_STORAGE;
    }
    if (usage & gpu_buffer_usage_indirect) {
        wgpu_desc.usage |= WGPU_BUFFER_USAGE_INDIRECT;
    }
    if (usage & gpu_buffer_usage_write) {
        wgpu_desc.usage |= WGPU_BUFFER_USAGE_COPY_DST;
    }
    if (usage & gpu_buffer_usage_read) {
        wgpu_desc.usage |= WGPU_BUFFER_USAGE_COPY_SRC;
    }
    buffer->wgpu_buffer = wgpu_device_create_buffer(device->wgpu_device, &wgpu_desc);
    cr_check(wgpu_is_buffer(buffer->wgpu_buffer), "the buffer %s with %u bytes could not be created", label, size);
#endif

    return buffer;
}

void gpu_buffer_destroy(struct gpu_device* device, struct gpu_buffer* buffer) {
#if cr_macos || cr_ios
    [buffer->mtl_buffer release];
#elif cr_wasm
    wgpu_object_destroy(buffer->wgpu_buffer);
#endif
    cr_free(device->mem, buffer, cr_loc);
}

void gpu_buffer_write(struct gpu_device* device, struct gpu_buffer* buffer, uint32_t offset, const void* data, uint32_t size) {
    cr_check(buffer->usage & gpu_buffer_usage_write, "the buffer was created without write usage");

#if cr_macos || cr_ios
    memcpy((char*)[buffer->mtl_buffer contents] + offset, data, size);
#elif cr_wasm
    wgpu_queue_write_buffer(device->wgpu_queue, buffer->wgpu_buffer, offset, data, size);
#endif
}

void* gpu_buffer_get_contents(struct gpu_buffer* buffer) {
#if cr_macos || cr_ios
    cr_check(buffer->usage & gpu_buffer_usage_write, "the buffer was created without write usage");
    return [buffer->mtl_buffer contents];
#elif cr_wasm
    return NULL;
#endif
}

struct gpu_texture* gpu_texture_create(struct gpu_device* device, const struct gpu_texture_desc* desc) {
    cr_check(desc->label != NULL, "a texture needs a label");

    struct gpu_texture* texture = cr_malloc(device->mem, sizeof(*texture), 16, cr_loc);
    memset(texture, 0, sizeof(*texture));
    texture->format = desc->format;
    texture->usage = desc->usage;
    texture->size = desc->size;

#if cr_macos || cr_ios
    MTLTextureDescriptor* mtl_desc = [[MTLTextureDescriptor alloc] init];
    mtl_desc.textureType = MTLTextureType2D;
    mtl_desc.pixelFormat = gpu_mtl_format(desc->format);
    mtl_desc.width = desc->size.x;
    mtl_desc.height = desc->size.y;
    mtl_desc.usage = 0;
    if (desc->usage & gpu_texture_usage_sample) {
        mtl_desc.usage |= MTLTextureUsageShaderRead;
    }
    if (desc->usage & gpu_texture_usage_render_target) {
        mtl_desc.usage |= MTLTextureUsageRenderTarget;
    }
    if (desc->usage & gpu_texture_usage_storage_write) {
        mtl_desc.usage |= MTLTextureUsageShaderWrite;
    }
    mtl_desc.storageMode = (desc->usage & gpu_texture_usage_upload) ? MTLStorageModeShared : MTLStorageModePrivate;
    mtl_desc.mipmapLevelCount = max_uint32(desc->mip_count, 1);
    texture->mtl_texture = [device->mtl_device newTextureWithDescriptor:mtl_desc];
    cr_check(texture->mtl_texture != nil, "the texture %s with size %ux%u could not be created", desc->label, desc->size.x, desc->size.y);
    texture->mtl_texture.label = [NSString stringWithFormat:@"%s", desc->label];
    [mtl_desc release];
#elif cr_wasm
    WGpuTextureDescriptor wgpu_desc = WGPU_TEXTURE_DESCRIPTOR_DEFAULT_INITIALIZER;
    wgpu_desc.width = desc->size.x;
    wgpu_desc.height = desc->size.y;
    wgpu_desc.format = gpu_wgpu_format(desc->format);
    wgpu_desc.mipLevelCount = max_uint32(desc->mip_count, 1);
    wgpu_desc.usage = 0;
    if (desc->usage & gpu_texture_usage_sample) {
        wgpu_desc.usage |= WGPU_TEXTURE_USAGE_TEXTURE_BINDING;
    }
    if (desc->usage & gpu_texture_usage_render_target) {
        wgpu_desc.usage |= WGPU_TEXTURE_USAGE_RENDER_ATTACHMENT;
    }
    if (desc->usage & gpu_texture_usage_storage_write) {
        wgpu_desc.usage |= WGPU_TEXTURE_USAGE_STORAGE_BINDING;
    }
    if (desc->usage & gpu_texture_usage_upload) {
        wgpu_desc.usage |= WGPU_TEXTURE_USAGE_COPY_DST;
    }
    if (desc->usage & gpu_texture_usage_read) {
        wgpu_desc.usage |= WGPU_TEXTURE_USAGE_COPY_SRC;
    }
    texture->wgpu_texture = wgpu_device_create_texture(device->wgpu_device, &wgpu_desc);
    cr_check(wgpu_is_texture(texture->wgpu_texture), "the texture %s with size %ux%u could not be created", desc->label, desc->size.x, desc->size.y);
    texture->wgpu_view = wgpu_texture_create_view_simple(texture->wgpu_texture);
    texture->owns_texture = true;
    texture->owns_view = true;
#endif

    return texture;
}

struct gpu_texture* gpu_texture_create_view_mip(struct gpu_device* device, struct gpu_texture* parent, uint32_t level) {
    struct gpu_texture* texture = cr_malloc(device->mem, sizeof(*texture), 16, cr_loc);
    memset(texture, 0, sizeof(*texture));
    texture->format = parent->format;
    texture->usage = parent->usage;
    texture->size = make_uint2(max_uint32(parent->size.x >> level, 1), max_uint32(parent->size.y >> level, 1));

#if cr_macos || cr_ios
    texture->mtl_texture = [parent->mtl_texture newTextureViewWithPixelFormat:parent->mtl_texture.pixelFormat
                                                          textureType:MTLTextureType2D
                                                               levels:NSMakeRange(level, 1)
                                                               slices:NSMakeRange(0, 1)];
    cr_check(texture->mtl_texture != nil, "the view of mip level %u could not be created", level);
    texture->mtl_texture.label = [NSString stringWithFormat:@"%@ mip %u", parent->mtl_texture.label, level];
#elif cr_wasm
    texture->wgpu_texture = parent->wgpu_texture;
    texture->wgpu_view = wgpu_texture_create_view_mip(parent->wgpu_texture, level);
    texture->owns_view = true;
#endif

    return texture;
}

#if cr_macos || cr_ios

static enum gpu_format gpu_format_from_mtl(MTLPixelFormat format) {
    switch (format) {
    case MTLPixelFormatBGRA8Unorm: return gpu_format_bgra8;
    case MTLPixelFormatR8Unorm: return gpu_format_r8;
    case MTLPixelFormatRGBA16Float: return gpu_format_rgba16f;
    case MTLPixelFormatR32Float: return gpu_format_r32f;
    default: assert(0 && "unsupported native texture format"); return gpu_format_bgra8;
    }
}

struct gpu_texture* gpu_texture_create_target_from_native(struct gpu_device* device, void* mtl_texture) {
    struct gpu_texture* texture = cr_malloc(device->mem, sizeof(*texture), 16, cr_loc);
    memset(texture, 0, sizeof(*texture));
    texture->mtl_texture = [(id<MTLTexture>)mtl_texture retain];
    texture->format = gpu_format_from_mtl(texture->mtl_texture.pixelFormat);
    texture->size = make_uint2((uint32_t)texture->mtl_texture.width, (uint32_t)texture->mtl_texture.height);
    if ((texture->mtl_texture.usage & MTLTextureUsageShaderRead) != 0 && texture->mtl_texture.framebufferOnly == false) {
        texture->usage |= gpu_texture_usage_sample;
    }
    if ((texture->mtl_texture.usage & MTLTextureUsageRenderTarget) != 0) {
        texture->usage |= gpu_texture_usage_render_target;
    }
    if ((texture->mtl_texture.usage & MTLTextureUsageShaderWrite) != 0) {
        texture->usage |= gpu_texture_usage_storage_write;
    }
    return texture;
}

#elif cr_wasm

static uint32_t gpu_usage_from_wgpu(WGPU_TEXTURE_USAGE_FLAGS usage) {
    return ((usage & WGPU_TEXTURE_USAGE_TEXTURE_BINDING) != 0 ? gpu_texture_usage_sample : 0u) |
           ((usage & WGPU_TEXTURE_USAGE_RENDER_ATTACHMENT) != 0 ? gpu_texture_usage_render_target : 0u) |
           ((usage & WGPU_TEXTURE_USAGE_STORAGE_BINDING) != 0 ? gpu_texture_usage_storage_write : 0u);
}

static enum gpu_format gpu_format_from_wgpu(WGPU_TEXTURE_FORMAT format) {
    switch (format) {
    case WGPU_TEXTURE_FORMAT_BGRA8UNORM: return gpu_format_bgra8;
    case WGPU_TEXTURE_FORMAT_R8UNORM: return gpu_format_r8;
    case WGPU_TEXTURE_FORMAT_RGBA16FLOAT: return gpu_format_rgba16f;
    case WGPU_TEXTURE_FORMAT_R32FLOAT: return gpu_format_r32f;
    default: assert(0 && "unsupported native texture format"); return gpu_format_bgra8;
    }
}

struct gpu_texture* gpu_texture_create_target_from_native(struct gpu_device* device, cr_wgpu_t wgpu_texture_view, uint2 size) {
    struct gpu_texture* texture = cr_malloc(device->mem, sizeof(*texture), 16, cr_loc);
    memset(texture, 0, sizeof(*texture));
    texture->format = gpu_format_from_wgpu(wgpu_texture_view_get_format(wgpu_texture_view));
    texture->usage = gpu_usage_from_wgpu(wgpu_texture_view_get_usage(wgpu_texture_view));
    texture->size = size;
    texture->wgpu_view = wgpu_texture_view;
    return texture;
}

#endif

#if cr_macos || cr_ios

void* gpu_device_get_native_device(struct gpu_device* device) {
    return device->mtl_device;
}

void* gpu_device_get_native_queue(struct gpu_device* device) {
    return device->mtl_queue;
}

void* gpu_texture_get_native(struct gpu_texture* texture) {
    return texture->mtl_texture;
}

void* gpu_command_buffer_get_native(struct gpu_command_buffer* command_buffer) {
    return command_buffer->mtl_command_buffer;
}

#endif

enum gpu_format gpu_texture_get_format(struct gpu_texture* texture) {
    return texture->format;
}

uint2 gpu_texture_get_size(struct gpu_texture* texture) {
    return texture->size;
}

uint32_t gpu_texture_get_usage(struct gpu_texture* texture) {
    return texture->usage;
}

void gpu_texture_destroy(struct gpu_device* device, struct gpu_texture* texture) {
#if cr_macos || cr_ios
    [texture->mtl_texture release];
#elif cr_wasm
    if (texture->owns_view) {
        wgpu_object_destroy(texture->wgpu_view);
    }
    if (texture->owns_texture) {
        wgpu_object_destroy(texture->wgpu_texture);
    }
#endif
    cr_free(device->mem, texture, cr_loc);
}

void gpu_texture_write(struct gpu_device* device, struct gpu_texture* texture, uint32_t x, uint32_t y, uint32_t width, uint32_t height, const void* data, uint32_t bytes_per_row) {
#if cr_macos || cr_ios
    [texture->mtl_texture replaceRegion:MTLRegionMake2D(x, y, width, height) mipmapLevel:0 slice:0 withBytes:data bytesPerRow:bytes_per_row bytesPerImage:bytes_per_row * height];
#elif cr_wasm
    WGpuTexelCopyTextureInfo wgpu_info = WGPU_TEXEL_COPY_TEXTURE_INFO_DEFAULT_INITIALIZER;
    wgpu_info.texture = texture->wgpu_texture;
    wgpu_info.origin.x = (int)x;
    wgpu_info.origin.y = (int)y;

    wgpu_queue_write_texture(device->wgpu_queue, &wgpu_info, data, bytes_per_row, height, width, height, 1);
#endif
}

struct gpu_bind_layout* gpu_bind_layout_create(struct gpu_device* device, const struct gpu_binding_desc* bindings, uint32_t binding_count) {
    cr_check(binding_count <= gpu_max_bindings, "a bind layout holds at most %u bindings", (uint32_t)gpu_max_bindings);

    struct gpu_bind_layout* layout = cr_malloc(device->mem, sizeof(*layout), 16, cr_loc);
    memset(layout, 0, sizeof(*layout));

    layout->binding_count = binding_count;

    for (uint32_t i = 0; i < binding_count; i++) {
        layout->bindings[i] = bindings[i];

        if (gpu_binding_is_buffer(bindings[i].type)) {
            layout->ordinals[i] = (uint8_t)layout->buffer_count++;
            if (bindings[i].dynamic_offset) {
                layout->dynamic_indices[layout->dynamic_count++] = (uint8_t)i;
            }
        } else if (gpu_binding_is_texture(bindings[i].type)) {
            layout->ordinals[i] = (uint8_t)layout->texture_count++;
        } else {
            layout->ordinals[i] = (uint8_t)layout->sampler_count++;
        }
    }

#if cr_wasm
    WGpuBindGroupLayoutEntry wgpu_entries[gpu_max_bindings];

    for (uint32_t i = 0; i < binding_count; i++) {
        WGpuBindGroupLayoutEntry wgpu_entry = WGPU_BUFFER_BINDING_LAYOUT_ENTRY_DEFAULT_INITIALIZER;
        wgpu_entry.binding = bindings[i].binding;
        wgpu_entry.visibility = gpu_wgpu_stages(bindings[i].stages);

        switch (bindings[i].type) {
        case gpu_binding_type_uniform: {
            wgpu_entry.type = WGPU_BIND_GROUP_LAYOUT_TYPE_BUFFER;
            wgpu_entry.layout.buffer.type = WGPU_BUFFER_BINDING_TYPE_UNIFORM;
            wgpu_entry.layout.buffer.hasDynamicOffset = bindings[i].dynamic_offset;
        } break;
        case gpu_binding_type_storage: {
            wgpu_entry.type = WGPU_BIND_GROUP_LAYOUT_TYPE_BUFFER;
            wgpu_entry.layout.buffer.type = WGPU_BUFFER_BINDING_TYPE_STORAGE;
            wgpu_entry.layout.buffer.hasDynamicOffset = bindings[i].dynamic_offset;
        } break;
        case gpu_binding_type_storage_read: {
            wgpu_entry.type = WGPU_BIND_GROUP_LAYOUT_TYPE_BUFFER;
            wgpu_entry.layout.buffer.type = WGPU_BUFFER_BINDING_TYPE_READ_ONLY_STORAGE;
            wgpu_entry.layout.buffer.hasDynamicOffset = bindings[i].dynamic_offset;
        } break;
        case gpu_binding_type_texture: {
            wgpu_entry.type = WGPU_BIND_GROUP_LAYOUT_TYPE_TEXTURE;
            wgpu_entry.layout.texture.sampleType = bindings[i].unfilterable ? WGPU_TEXTURE_SAMPLE_TYPE_UNFILTERABLE_FLOAT : WGPU_TEXTURE_SAMPLE_TYPE_FLOAT;
            wgpu_entry.layout.texture.viewDimension = WGPU_TEXTURE_VIEW_DIMENSION_2D;
        } break;
        case gpu_binding_type_storage_texture: {
            wgpu_entry.type = WGPU_BIND_GROUP_LAYOUT_TYPE_STORAGE_TEXTURE;
            wgpu_entry.layout.storageTexture.access = WGPU_STORAGE_TEXTURE_ACCESS_WRITE_ONLY;
            wgpu_entry.layout.storageTexture.format = gpu_wgpu_format(bindings[i].format);
            wgpu_entry.layout.storageTexture.viewDimension = WGPU_TEXTURE_VIEW_DIMENSION_2D;
        } break;
        case gpu_binding_type_sampler: {
            wgpu_entry.type = WGPU_BIND_GROUP_LAYOUT_TYPE_SAMPLER;
            wgpu_entry.layout.sampler.type = WGPU_SAMPLER_BINDING_TYPE_FILTERING;
        } break;
        }

        wgpu_entries[i] = wgpu_entry;
    }

    layout->wgpu_layout = wgpu_device_create_bind_group_layout(device->wgpu_device, wgpu_entries, (int)binding_count);
    cr_check(wgpu_is_bind_group_layout(layout->wgpu_layout), "the bind layout could not be created");
#endif

    return layout;
}

void gpu_bind_layout_destroy(struct gpu_device* device, struct gpu_bind_layout* layout) {
#if cr_wasm
    wgpu_object_destroy(layout->wgpu_layout);
#endif
    cr_free(device->mem, layout, cr_loc);
}

struct gpu_bind_group* gpu_bind_group_create(struct gpu_device* device, struct gpu_bind_layout* layout, const struct gpu_bind_desc* binds, uint32_t bind_count) {
    cr_check(bind_count == layout->binding_count, "a bind group needs %u binds for its layout", layout->binding_count);

    struct gpu_bind_group* group = cr_malloc(device->mem, sizeof(*group), 16, cr_loc);
    memset(group, 0, sizeof(*group));
    group->layout = layout;

    for (uint32_t i = 0; i < bind_count; i++) {
        cr_check(binds[i].binding == layout->bindings[i].binding, "bind %u targets binding %u but the layout has binding %u there", i, binds[i].binding, layout->bindings[i].binding);
    }

#if cr_macos || cr_ios
    memcpy(group->binds, binds, bind_count * sizeof(*binds));
#elif cr_wasm
    WGpuBindGroupEntry wgpu_entries[gpu_max_bindings];

    for (uint32_t i = 0; i < bind_count; i++) {
        WGpuBindGroupEntry wgpu_entry = { 0 };
        wgpu_entry.binding = binds[i].binding;

        if (gpu_binding_is_buffer(layout->bindings[i].type)) {
            wgpu_entry.resource = binds[i].buffer->wgpu_buffer;
            wgpu_entry.bufferBindOffset = binds[i].buffer_offset;
            wgpu_entry.bufferBindSize = binds[i].buffer_size;
        } else if (gpu_binding_is_texture(layout->bindings[i].type)) {
            wgpu_entry.resource = binds[i].texture->wgpu_view;
        } else {
            wgpu_entry.resource = binds[i].sampler->wgpu_sampler;
        }

        wgpu_entries[i] = wgpu_entry;
    }

    group->wgpu_group = wgpu_device_create_bind_group(device->wgpu_device, layout->wgpu_layout, wgpu_entries, (int)bind_count);
    cr_check(wgpu_is_bind_group(group->wgpu_group), "the bind group could not be created");
#endif

    return group;
}

void gpu_bind_group_destroy(struct gpu_device* device, struct gpu_bind_group* group) {
#if cr_wasm
    wgpu_object_destroy(group->wgpu_group);
#endif
    cr_free(device->mem, group, cr_loc);
}

struct gpu_render_pipeline* gpu_render_pipeline_create(struct gpu_device* device, const struct gpu_render_pipeline_desc* desc) {
    cr_check(desc->library != NULL && desc->vertex_name != NULL && desc->fragment_name != NULL, "a render pipeline needs a library, a vertex name and a fragment name");
    cr_check(desc->layout_count <= gpu_max_groups, "a pipeline holds at most %u bind layouts", (uint32_t)gpu_max_groups);

    struct gpu_render_pipeline* pipeline = cr_malloc(device->mem, sizeof(*pipeline), 16, cr_loc);
    memset(pipeline, 0, sizeof(*pipeline));

    gpu_slot_bases(desc->layouts, desc->layout_count, pipeline->bases);
    snprintf(pipeline->compile.label, sizeof(pipeline->compile.label), "%s %s", desc->vertex_name, desc->fragment_name);
    struct gpu_compile* compile = &pipeline->compile;

#if cr_macos || cr_ios
    MTLRenderPipelineDescriptor* mtl_desc = [[MTLRenderPipelineDescriptor alloc] init];
    mtl_desc.label = [NSString stringWithFormat:@"%s %s", desc->vertex_name, desc->fragment_name];
    mtl_desc.colorAttachments[0].pixelFormat = gpu_mtl_format(desc->color_format);

    if (desc->blend) {
        mtl_desc.colorAttachments[0].blendingEnabled = true;
        mtl_desc.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
        mtl_desc.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
        mtl_desc.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorOne;
        mtl_desc.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
        mtl_desc.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorOne;
        mtl_desc.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    }

    uint32_t buffer_count = 0;
    for (uint32_t i = 0; i < desc->layout_count; i++) {
        buffer_count += desc->layouts[i]->buffer_count;
    }
    for (uint32_t i = 0; i < buffer_count; i++) {
        mtl_desc.vertexBuffers[i].mutability = MTLMutabilityImmutable;
        mtl_desc.fragmentBuffers[i].mutability = MTLMutabilityImmutable;
    }

    id<MTLFunction> mtl_vertex = [desc->library->mtl_library newFunctionWithName:[NSString stringWithFormat:@"%s", desc->vertex_name]];
    cr_check(mtl_vertex != nil, "the vertex function %s is not in the library", desc->vertex_name);
    mtl_desc.vertexFunction = mtl_vertex;
    [mtl_vertex release];

    id<MTLFunction> mtl_fragment = [desc->library->mtl_library newFunctionWithName:[NSString stringWithFormat:@"%s", desc->fragment_name]];
    cr_check(mtl_fragment != nil, "the fragment function %s is not in the library", desc->fragment_name);
    mtl_desc.fragmentFunction = mtl_fragment;
    [mtl_fragment release];

    gpu_compile_begin(device, compile);
    [device->mtl_device newRenderPipelineStateWithDescriptor:mtl_desc completionHandler:^(id<MTLRenderPipelineState> mtl_pipeline, NSError* error) {
        pipeline->mtl_pipeline = [mtl_pipeline retain];
        compile->mtl_error = error != nil ? strdup(error.localizedDescription.UTF8String) : NULL;
        dispatch_group_leave(compile->mtl_group);
    }];
    [mtl_desc release];
#elif cr_wasm
    WGpuColorTargetState wgpu_color_target = WGPU_COLOR_TARGET_STATE_DEFAULT_INITIALIZER;
    wgpu_color_target.format = gpu_wgpu_format(desc->color_format);

    if (desc->blend) {
        wgpu_color_target.blend.color.operation = WGPU_BLEND_OPERATION_ADD;
        wgpu_color_target.blend.color.srcFactor = WGPU_BLEND_FACTOR_ONE;
        wgpu_color_target.blend.color.dstFactor = WGPU_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        wgpu_color_target.blend.alpha.operation = WGPU_BLEND_OPERATION_ADD;
        wgpu_color_target.blend.alpha.srcFactor = WGPU_BLEND_FACTOR_ONE;
        wgpu_color_target.blend.alpha.dstFactor = WGPU_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    }

    WGpuPipelineLayout wgpu_pipeline_layout = gpu_wgpu_pipeline_layout(device, desc->layouts, desc->layout_count);
    const char* vertex_code = gpu_wgpu_module_code(desc->library, desc->vertex_name);
    const char* fragment_code = gpu_wgpu_module_code(desc->library, desc->fragment_name);
    WGpuShaderModule wgpu_vertex_module = gpu_wgpu_shader_module(device, vertex_code, desc->vertex_name);
    WGpuShaderModule wgpu_fragment_module = fragment_code == vertex_code ? wgpu_vertex_module : gpu_wgpu_shader_module(device, fragment_code, desc->fragment_name);

    WGpuRenderPipelineDescriptor wgpu_pipeline_desc = WGPU_RENDER_PIPELINE_DESCRIPTOR_DEFAULT_INITIALIZER;
    wgpu_pipeline_desc.layout = wgpu_pipeline_layout;
    wgpu_pipeline_desc.vertex.module = wgpu_vertex_module;
    wgpu_pipeline_desc.vertex.entryPoint = desc->vertex_name;
    wgpu_pipeline_desc.fragment.module = wgpu_fragment_module;
    wgpu_pipeline_desc.fragment.entryPoint = desc->fragment_name;
    wgpu_pipeline_desc.fragment.numTargets = 1;
    wgpu_pipeline_desc.fragment.targets = &wgpu_color_target;

    struct gpu_timeline_entry* entry = gpu_compile_begin(device, compile);
    pipeline->wgpu_pipeline = wgpu_device_create_render_pipeline_async(device->wgpu_device, &wgpu_pipeline_desc, gpu_compile_on_created, entry);
    assert(wgpu_is_render_pipeline(pipeline->wgpu_pipeline));

    if (wgpu_fragment_module != wgpu_vertex_module) {
        wgpu_object_destroy(wgpu_fragment_module);
    }
    wgpu_object_destroy(wgpu_vertex_module);
    wgpu_object_destroy(wgpu_pipeline_layout);
#endif

    return pipeline;
}

void gpu_render_pipeline_destroy(struct gpu_device* device, struct gpu_render_pipeline* pipeline) {
    gpu_compile_end(device, &pipeline->compile);
#if cr_macos || cr_ios
    [pipeline->mtl_pipeline release];
#elif cr_wasm
    wgpu_object_destroy(pipeline->wgpu_pipeline);
#endif
    cr_free(device->mem, pipeline, cr_loc);
}

struct gpu_compute_pipeline* gpu_compute_pipeline_create(struct gpu_device* device, const struct gpu_compute_pipeline_desc* desc) {
    cr_check(desc->library != NULL && desc->name != NULL, "a compute pipeline needs a library and a function name");
    cr_check(desc->layout_count <= gpu_max_groups, "a pipeline holds at most %u bind layouts", (uint32_t)gpu_max_groups);

    struct gpu_compute_pipeline* pipeline = cr_malloc(device->mem, sizeof(*pipeline), 16, cr_loc);
    memset(pipeline, 0, sizeof(*pipeline));

    gpu_slot_bases(desc->layouts, desc->layout_count, pipeline->bases);
    pipeline->workgroup_size = desc->workgroup_size;
    snprintf(pipeline->compile.label, sizeof(pipeline->compile.label), "%s", desc->name);
    struct gpu_compile* compile = &pipeline->compile;

#if cr_macos || cr_ios
    id<MTLFunction> mtl_function = [desc->library->mtl_library newFunctionWithName:[NSString stringWithFormat:@"%s", desc->name]];
    cr_check(mtl_function != nil, "the compute function %s is not in the library", desc->name);
    MTLComputePipelineDescriptor* mtl_desc = [[MTLComputePipelineDescriptor alloc] init];
    mtl_desc.label = [NSString stringWithFormat:@"%s", desc->name];
    mtl_desc.computeFunction = mtl_function;
    gpu_compile_begin(device, compile);
    [device->mtl_device newComputePipelineStateWithDescriptor:mtl_desc options:MTLPipelineOptionNone completionHandler:^(id<MTLComputePipelineState> mtl_pipeline, MTLComputePipelineReflection* reflection, NSError* error) {
        pipeline->mtl_pipeline = [mtl_pipeline retain];
        compile->mtl_error = error != nil ? strdup(error.localizedDescription.UTF8String) : NULL;
        dispatch_group_leave(compile->mtl_group);
    }];
    [mtl_desc release];
    [mtl_function release];
#elif cr_wasm
    WGpuPipelineLayout wgpu_pipeline_layout = gpu_wgpu_pipeline_layout(device, desc->layouts, desc->layout_count);
    WGpuShaderModule wgpu_shader_module = gpu_wgpu_shader_module(device, gpu_wgpu_module_code(desc->library, desc->name), desc->name);

    struct gpu_timeline_entry* entry = gpu_compile_begin(device, compile);
    pipeline->wgpu_pipeline = wgpu_device_create_compute_pipeline_async(device->wgpu_device, wgpu_shader_module, desc->name, wgpu_pipeline_layout, gpu_compile_on_created, entry);
    assert(wgpu_is_compute_pipeline(pipeline->wgpu_pipeline));

    wgpu_object_destroy(wgpu_shader_module);
    wgpu_object_destroy(wgpu_pipeline_layout);
#endif

    return pipeline;
}

void gpu_compute_pipeline_destroy(struct gpu_device* device, struct gpu_compute_pipeline* pipeline) {
    gpu_compile_end(device, &pipeline->compile);
#if cr_macos || cr_ios
    [pipeline->mtl_pipeline release];
#elif cr_wasm
    wgpu_object_destroy(pipeline->wgpu_pipeline);
#endif
    cr_free(device->mem, pipeline, cr_loc);
}

struct gpu_command_buffer* gpu_command_buffer_create(struct gpu_device* device, const char* label) {
    cr_check(label != NULL, "a command buffer needs a label");

    struct gpu_timeline_entry* entry = gpu_timeline_acquire(device);

    struct gpu_command_buffer* command_buffer = cr_malloc(device->mem, sizeof(*command_buffer), 16, cr_loc);
    memset(command_buffer, 0, sizeof(*command_buffer));
    command_buffer->device = device;
    command_buffer->serial = entry->serial;

#if cr_macos || cr_ios
    command_buffer->mtl_command_buffer = [[device->mtl_queue commandBuffer] retain];
    command_buffer->mtl_command_buffer.label = [NSString stringWithFormat:@"%s", label];
#elif cr_wasm
    command_buffer->wgpu_enc = wgpu_device_create_command_encoder_simple(device->wgpu_device);
#endif

    return command_buffer;
}

uint64_t gpu_command_buffer_get_serial(struct gpu_command_buffer* command_buffer) {
    return command_buffer->serial;
}

static struct gpu_readback* gpu_command_buffer_add_readback(struct gpu_command_buffer* command_buffer, void* out, uint32_t rows, uint32_t src_stride, uint32_t dst_stride) {
    cr_check(command_buffer->pass_active == false, "a readback cannot be added while a pass is active");
    struct gpu_timeline_entry* entry = gpu_timeline_slot(command_buffer->device, command_buffer->serial);
    cr_check(entry->readback_count < gpu_timeline_readbacks, "a command buffer holds at most %u readbacks", (uint32_t)gpu_timeline_readbacks);

    struct gpu_readback* readback = &entry->readbacks[entry->readback_count++];
    *readback = (struct gpu_readback) { .out = out, .rows = rows, .src_stride = src_stride, .dst_stride = dst_stride };
#if cr_macos || cr_ios
    readback->mtl_buffer = [command_buffer->device->mtl_device newBufferWithLength:(NSUInteger)src_stride * rows options:MTLResourceStorageModeShared];
    cr_check(readback->mtl_buffer != nil, "the readback buffer with %u bytes could not be created", src_stride * rows);
#elif cr_wasm
    WGpuBufferDescriptor wgpu_desc = { .size = (double_int53_t)src_stride * rows, .usage = WGPU_BUFFER_USAGE_COPY_DST | WGPU_BUFFER_USAGE_MAP_READ };
    readback->wgpu_buffer = wgpu_device_create_buffer(command_buffer->device->wgpu_device, &wgpu_desc);
    cr_check(wgpu_is_buffer(readback->wgpu_buffer), "the readback buffer with %u bytes could not be created", src_stride * rows);
#endif
    return readback;
}

void gpu_command_buffer_read_texture(struct gpu_command_buffer* command_buffer, struct gpu_texture* texture, void* out, uint32_t bytes_per_row) {
    cr_check(texture->usage & gpu_texture_usage_read, "the texture was created without read usage");
    uint32_t stride = align_up_uint32(bytes_per_row, 256);
    struct gpu_readback* readback = gpu_command_buffer_add_readback(command_buffer, out, texture->size.y, stride, bytes_per_row);

#if cr_macos || cr_ios
    id<MTLBlitCommandEncoder> mtl_blit = [command_buffer->mtl_command_buffer blitCommandEncoder];
    [mtl_blit copyFromTexture:texture->mtl_texture
                 sourceSlice:0
                 sourceLevel:0
                sourceOrigin:MTLOriginMake(0, 0, 0)
                  sourceSize:MTLSizeMake(texture->size.x, texture->size.y, 1)
                    toBuffer:readback->mtl_buffer
           destinationOffset:0
      destinationBytesPerRow:stride
    destinationBytesPerImage:(NSUInteger)stride * texture->size.y];
    [mtl_blit endEncoding];
#elif cr_wasm
    wgpu_command_encoder_copy_texture_to_buffer(command_buffer->wgpu_enc, texture->wgpu_texture, readback->wgpu_buffer, stride, texture->size.x, texture->size.y);
#endif
}

void gpu_command_buffer_read_buffer(struct gpu_command_buffer* command_buffer, struct gpu_buffer* buffer, uint32_t offset, void* out, uint32_t size) {
    cr_check(buffer->usage & gpu_buffer_usage_read, "the buffer was created without read usage");
    struct gpu_readback* readback = gpu_command_buffer_add_readback(command_buffer, out, 1, size, size);

#if cr_macos || cr_ios
    id<MTLBlitCommandEncoder> mtl_blit = [command_buffer->mtl_command_buffer blitCommandEncoder];
    [mtl_blit copyFromBuffer:buffer->mtl_buffer sourceOffset:offset toBuffer:readback->mtl_buffer destinationOffset:0 size:size];
    [mtl_blit endEncoding];
#elif cr_wasm
    wgpu_command_encoder_copy_buffer_to_buffer(command_buffer->wgpu_enc, buffer->wgpu_buffer, offset, readback->wgpu_buffer, 0, size);
#endif
}

uint64_t gpu_command_buffer_commit(struct gpu_command_buffer* command_buffer) {
    cr_check(command_buffer->pass_active == false, "a command buffer cannot commit while a pass is active");

    struct gpu_device* device = command_buffer->device;
    uint64_t serial = command_buffer->serial;
    struct gpu_timeline_entry* entry = gpu_timeline_find(device, serial);
    assert(entry != NULL && entry->committed == false);
    entry->committed = true;

#if cr_macos || cr_ios
    entry->mtl_command_buffer = command_buffer->mtl_command_buffer;
    [entry->mtl_command_buffer commit];
#elif cr_wasm
    struct gpu_profiler* profiler = command_buffer->profile;
    if (profiler != NULL && profiler->count > 0) {
        uint32_t size = profiler->count * (uint32_t)sizeof(uint64_t);
        wgpu_command_encoder_resolve_query_set(command_buffer->wgpu_enc, device->wgpu_profiler_query_set, profiler->slot * gpu_profiler_slot_samples, profiler->count, profiler->wgpu_resolve, 0);
        struct gpu_readback* readback = gpu_command_buffer_add_readback(command_buffer, profiler->stamps, 1, size, size);
        wgpu_command_encoder_copy_buffer_to_buffer(command_buffer->wgpu_enc, profiler->wgpu_resolve, 0, readback->wgpu_buffer, 0, size);
    }

    wgpu_queue_submit_one_and_destroy(device->wgpu_queue, wgpu_command_encoder_finish(command_buffer->wgpu_enc));
    entry->pending = 1 + entry->readback_count;
    wgpu_queue_set_on_submitted_work_done_callback(device->wgpu_queue, gpu_timeline_on_work_done, entry);
    for (uint32_t i = 0; i < entry->readback_count; i++) {
        struct gpu_readback* readback = &entry->readbacks[i];
        wgpu_buffer_map_async(readback->wgpu_buffer, gpu_timeline_on_map, entry, WGPU_MAP_MODE_READ, 0, (double_int53_t)readback->src_stride * readback->rows);
    }
#endif

    cr_free(device->mem, command_buffer, cr_loc);
    return serial;
}

void gpu_command_buffer_discard(struct gpu_command_buffer* command_buffer) {
    struct gpu_device* device = command_buffer->device;
    struct gpu_timeline_entry* entry = gpu_timeline_find(device, command_buffer->serial);
    assert(entry != NULL && entry->committed == false);

    if (command_buffer->pass_active) {
#if cr_macos || cr_ios
        [command_buffer->render_pass.mtl_enc endEncoding];
        [command_buffer->compute_pass.mtl_enc endEncoding];
#elif cr_wasm
        if (command_buffer->render_pass.wgpu_enc != 0) {
            wgpu_render_pass_encoder_end(command_buffer->render_pass.wgpu_enc);
        }
        if (command_buffer->compute_pass.wgpu_enc != 0) {
            wgpu_compute_pass_encoder_end(command_buffer->compute_pass.wgpu_enc);
        }
#endif
        command_buffer->pass_active = false;
    }

    for (uint32_t i = 0; i < entry->readback_count; i++) {
#if cr_macos || cr_ios
        [entry->readbacks[i].mtl_buffer release];
#elif cr_wasm
        wgpu_object_destroy(entry->readbacks[i].wgpu_buffer);
#endif
    }
    entry->readback_count = 0;
    entry->committed = true;
    entry->completed = true;
    entry->failed = true;

#if cr_macos || cr_ios
    [command_buffer->mtl_command_buffer release];
#elif cr_wasm
    wgpu_object_destroy(command_buffer->wgpu_enc);
#endif
    cr_free(device->mem, command_buffer, cr_loc);
}

struct gpu_profiler* gpu_profiler_create(struct gpu_device* device, uint32_t max_passes) {
    cr_check(max_passes > 0 && max_passes <= gpu_profile_max_passes, "a profiler holds between 1 and %u passes", (uint32_t)gpu_profile_max_passes);

#if cr_macos || cr_ios
    bool supported = device->mtl_profiler_samples != nil;
#elif cr_wasm
    bool supported = device->wgpu_profiler_query_set != 0;
#endif
    uint32_t free_mask = ~device->profiler_slot_mask & ((1u << gpu_profiler_slots) - 1);
    if (supported == false || free_mask == 0) {
        return NULL;
    }

    uint32_t slot = (uint32_t)ctz_uint32(free_mask);
    device->profiler_slot_mask |= 1u << slot;

    struct gpu_profiler* profiler = cr_malloc(device->mem, sizeof(*profiler), 16, cr_loc);
    memset(profiler, 0, sizeof(*profiler));
    profiler->device = device;
    profiler->slot = slot;
    profiler->max_count = 2 * max_passes;

#if cr_wasm
    WGpuBufferDescriptor wgpu_resolve_desc = { .size = profiler->max_count * (uint32_t)sizeof(uint64_t), .usage = WGPU_BUFFER_USAGE_QUERY_RESOLVE | WGPU_BUFFER_USAGE_COPY_SRC };
    profiler->wgpu_resolve = wgpu_device_create_buffer(device->wgpu_device, &wgpu_resolve_desc);
    assert(wgpu_is_buffer(profiler->wgpu_resolve));
#endif

    return profiler;
}

void gpu_profiler_destroy(struct gpu_device* device, struct gpu_profiler* profiler) {
    if (profiler == NULL) {
        return;
    }
    struct gpu_timeline_entry* entry = gpu_timeline_find(device, profiler->serial);
    cr_check(entry == NULL || entry->committed, "the profiler is attached to an uncommitted command buffer");
    device->profiler_slot_mask &= ~(1u << profiler->slot);

#if cr_wasm
    for (uint32_t i = 0; entry != NULL && i < entry->readback_count; i++) {
        if (entry->readbacks[i].out == profiler->stamps) {
            entry->readbacks[i].out = NULL;
        }
    }
    wgpu_object_destroy(profiler->wgpu_resolve);
#endif

    cr_free(device->mem, profiler, cr_loc);
}

bool gpu_command_buffer_profile(struct gpu_command_buffer* command_buffer, struct gpu_profiler* profiler) {
    cr_check(command_buffer->pass_active == false, "a profiler cannot attach while a pass is active");
    cr_check(command_buffer->profile == NULL, "the command buffer already has a profiler");

    if (profiler == NULL) {
        return false;
    }
    cr_check(profiler->device == command_buffer->device, "the profiler belongs to another device");
    if (gpu_device_is_completed(profiler->device, profiler->serial) == false) {
        return false;
    }

    profiler->count = 0;
    profiler->serial = command_buffer->serial;
    profiler->resolved = false;
    memset(profiler->used_mask, 0, sizeof(profiler->used_mask));
    memset(&profiler->profile, 0, sizeof(profiler->profile));
    command_buffer->profile = profiler;
    return true;
}

#if cr_macos || cr_ios
static void gpu_profiler_read_stamps(struct gpu_profiler* profiler, uint64_t* stamps) {
    memset(stamps, 0, profiler->count * sizeof(stamps[0]));
    if (profiler->count == 0) {
        return;
    }
    @autoreleasepool {
        NSData* data = [profiler->device->mtl_profiler_samples resolveCounterRange:NSMakeRange(profiler->slot * gpu_profiler_slot_samples, profiler->count)];
        if (data == nil || data.length < profiler->count * sizeof(MTLCounterResultTimestamp)) {
            return;
        }
        const MTLCounterResultTimestamp* raw = data.bytes;
        for (uint32_t i = 0; i < profiler->count; i++) {
            stamps[i] = raw[i].timestamp != MTLCounterErrorValue ? raw[i].timestamp : 0;
        }
    }
}
#endif

static void gpu_profiler_resolve(struct gpu_profiler* profiler) {
#if cr_macos || cr_ios
    uint64_t stamps[gpu_profiler_slot_samples];
    gpu_profiler_read_stamps(profiler, stamps);
#elif cr_wasm
    const uint64_t* stamps = profiler->stamps;
#endif

    uint32_t pair_count = profiler->count / 2;
    uint64_t t_min = uint64_max;
    uint64_t t_max = 0;
    for (uint32_t i = 0; i < pair_count; i++) {
        uint64_t begin = stamps[i * 2];
        uint64_t end = stamps[i * 2 + 1];
        bool used = (profiler->used_mask[i / 32] >> (i % 32)) & 1u;
        if (used == false || begin == 0 || end == 0 || end < begin) {
            profiler->profile.passes[i] = 0.0;
            continue;
        }
        profiler->profile.passes[i] = (double)(end - begin) * 1e-9;
        t_min = min_uint64(t_min, begin);
        t_max = max_uint64(t_max, end);
    }
    profiler->profile.pass_count = pair_count;
    profiler->profile.span = t_max > t_min ? (double)(t_max - t_min) * 1e-9 : 0.0;
    profiler->resolved = true;
}

bool gpu_profiler_poll(struct gpu_profiler* profiler, struct gpu_profile* out) {
    if (profiler == NULL || profiler->serial == 0) {
        return false;
    }
    if (profiler->resolved == false) {
        if (gpu_device_is_completed(profiler->device, profiler->serial) == false) {
            return false;
        }
        gpu_profiler_resolve(profiler);
    }
    *out = profiler->profile;
    return true;
}

static uint32_t gpu_command_buffer_profile_alloc_pair(struct gpu_command_buffer* command_buffer) {
    if (command_buffer->profile == NULL || command_buffer->profile->count + 2 > command_buffer->profile->max_count) {
        return uint32_max;
    }
    uint32_t pair = command_buffer->profile->count / 2;
    command_buffer->profile->count += 2;
    return pair;
}

static void gpu_command_buffer_profile_mark_used(struct gpu_command_buffer* command_buffer, uint32_t pair) {
    if (pair != uint32_max) {
        command_buffer->profile->used_mask[pair / 32] |= 1u << (pair % 32);
    }
}

static void gpu_command_buffer_profile_name_pair(struct gpu_command_buffer* command_buffer, uint32_t pair, const char* label) {
    if (pair != uint32_max) {
        snprintf(command_buffer->profile->profile.pass_names[pair], gpu_pass_name_max, "%s", label);
    }
}

struct gpu_render_pass* gpu_command_buffer_begin_render(struct gpu_command_buffer* command_buffer, struct gpu_texture* target, enum gpu_load load, const char* label) {
    cr_check(label != NULL, "a render pass needs a label");
    cr_check(command_buffer->pass_active == false, "a pass cannot begin while another is active");
    command_buffer->pass_active = true;

    struct gpu_render_pass* pass = &command_buffer->render_pass;
    memset(pass, 0, sizeof(*pass));
    pass->command_buffer = command_buffer;
    pass->profile_pair = gpu_command_buffer_profile_alloc_pair(command_buffer);
    gpu_command_buffer_profile_name_pair(command_buffer, pass->profile_pair, label);

#if cr_macos || cr_ios
    MTLRenderPassDescriptor* mtl_desc = [[MTLRenderPassDescriptor alloc] init];
    mtl_desc.colorAttachments[0].texture = target->mtl_texture;
    switch (load) {
    case gpu_load_clear: mtl_desc.colorAttachments[0].loadAction = MTLLoadActionClear; break;
    case gpu_load_keep: mtl_desc.colorAttachments[0].loadAction = MTLLoadActionLoad; break;
    case gpu_load_dont_care: mtl_desc.colorAttachments[0].loadAction = MTLLoadActionDontCare; break;
    }
    mtl_desc.colorAttachments[0].clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 0.0);
    mtl_desc.colorAttachments[0].storeAction = MTLStoreActionStore;

    if (pass->profile_pair != uint32_max) {
        MTLRenderPassSampleBufferAttachmentDescriptor* mtl_samples = mtl_desc.sampleBufferAttachments[0];
        mtl_samples.sampleBuffer = command_buffer->device->mtl_profiler_samples;
        mtl_samples.startOfVertexSampleIndex = command_buffer->profile->slot * gpu_profiler_slot_samples + pass->profile_pair * 2;
        mtl_samples.endOfFragmentSampleIndex = command_buffer->profile->slot * gpu_profiler_slot_samples + pass->profile_pair * 2 + 1;
    }

    pass->mtl_enc = [command_buffer->mtl_command_buffer renderCommandEncoderWithDescriptor:mtl_desc];
    pass->mtl_enc.label = [NSString stringWithFormat:@"%s", label];
    [mtl_desc release];
#elif cr_wasm
    WGpuRenderPassColorAttachment wgpu_color_attachment = WGPU_RENDER_PASS_COLOR_ATTACHMENT_DEFAULT_INITIALIZER;
    wgpu_color_attachment.view = target->wgpu_view;
    wgpu_color_attachment.loadOp = load == gpu_load_keep ? WGPU_LOAD_OP_LOAD : WGPU_LOAD_OP_CLEAR;
    memset(&wgpu_color_attachment.clearValue, 0, sizeof(wgpu_color_attachment.clearValue));
    wgpu_color_attachment.storeOp = WGPU_STORE_OP_STORE;

    WGpuRenderPassDescriptor wgpu_desc = WGPU_RENDER_PASS_DESCRIPTOR_DEFAULT_INITIALIZER;
    wgpu_desc.numColorAttachments = 1;
    wgpu_desc.colorAttachments = &wgpu_color_attachment;
    if (pass->profile_pair != uint32_max) {
        wgpu_desc.timestampWrites.querySet = command_buffer->device->wgpu_profiler_query_set;
        wgpu_desc.timestampWrites.beginningOfPassWriteIndex = (int32_t)(command_buffer->profile->slot * gpu_profiler_slot_samples + pass->profile_pair * 2);
        wgpu_desc.timestampWrites.endOfPassWriteIndex = (int32_t)(command_buffer->profile->slot * gpu_profiler_slot_samples + pass->profile_pair * 2 + 1);
    }

    pass->wgpu_enc = wgpu_command_encoder_begin_render_pass(command_buffer->wgpu_enc, &wgpu_desc);
#endif

    return pass;
}

void gpu_render_pass_end(struct gpu_render_pass* pass) {
    pass->command_buffer->pass_active = false;

#if cr_macos || cr_ios
    [pass->mtl_enc endEncoding];
    pass->mtl_enc = nil;
#elif cr_wasm
    wgpu_render_pass_encoder_end(pass->wgpu_enc);
    pass->wgpu_enc = 0;
#endif
}

void gpu_render_pass_set_pipeline(struct gpu_render_pass* pass, struct gpu_render_pipeline* pipeline) {
    if (pass->pipeline == pipeline) {
        return;
    }
    gpu_compile_require(pass->command_buffer->device, &pipeline->compile);
    pass->pipeline = pipeline;

#if cr_macos || cr_ios
    [pass->mtl_enc setRenderPipelineState:pipeline->mtl_pipeline];
#elif cr_wasm
    wgpu_render_pass_encoder_set_pipeline(pass->wgpu_enc, pipeline->wgpu_pipeline);
#endif
}

void gpu_render_pass_set_bind_group(struct gpu_render_pass* pass, uint32_t index, struct gpu_bind_group* group, const uint32_t* dynamic_offsets, uint32_t dynamic_offset_count) {
    cr_check(index < gpu_max_groups, "bind group indices are below %u", (uint32_t)gpu_max_groups);
    cr_check(dynamic_offset_count == group->layout->dynamic_count || dynamic_offset_count == 0, "the bind group takes %u dynamic offsets or none", group->layout->dynamic_count);

#if cr_macos || cr_ios
    struct gpu_bind_layout* layout = group->layout;
    struct gpu_slot_base base = { 0 };
    if (pass->pipeline) {
        base = pass->pipeline->bases[index];
    } else {
        cr_check(index == 0, "groups above 0 need the pipeline set first");
    }

    if (pass->applied[index].group == group &&
        pass->applied[index].base.buffer == base.buffer &&
        pass->applied[index].base.texture == base.texture &&
        pass->applied[index].base.sampler == base.sampler &&
        dynamic_offset_count != 0) {
        for (uint32_t k = 0; k < dynamic_offset_count; k++) {
            uint32_t i = layout->dynamic_indices[k];
            uint32_t slot = base.buffer + layout->ordinals[i];
            uint32_t offset = group->binds[i].buffer_offset + dynamic_offsets[k];

            if (layout->bindings[i].stages & gpu_stage_vertex) {
                [pass->mtl_enc setVertexBufferOffset:offset atIndex:slot];
            }
            if (layout->bindings[i].stages & gpu_stage_fragment) {
                [pass->mtl_enc setFragmentBufferOffset:offset atIndex:slot];
            }
        }
        return;
    }

    uint32_t dynamic_index = 0;

    for (uint32_t i = 0; i < layout->binding_count; i++) {
        const struct gpu_binding_desc* binding = &layout->bindings[i];
        const struct gpu_bind_desc* bind = &group->binds[i];
        uint32_t stages = binding->stages;

        if (gpu_binding_is_buffer(binding->type)) {
            uint32_t slot = base.buffer + layout->ordinals[i];
            uint32_t offset = bind->buffer_offset;
            if (binding->dynamic_offset && dynamic_offset_count != 0) {
                offset += dynamic_offsets[dynamic_index++];
            }
            if (stages & gpu_stage_vertex) {
                [pass->mtl_enc setVertexBuffer:bind->buffer->mtl_buffer offset:offset atIndex:slot];
            }
            if (stages & gpu_stage_fragment) {
                [pass->mtl_enc setFragmentBuffer:bind->buffer->mtl_buffer offset:offset atIndex:slot];
            }
        } else if (gpu_binding_is_texture(binding->type)) {
            uint32_t slot = base.texture + layout->ordinals[i];
            if (stages & gpu_stage_vertex) {
                [pass->mtl_enc setVertexTexture:bind->texture->mtl_texture atIndex:slot];
            }
            if (stages & gpu_stage_fragment) {
                [pass->mtl_enc setFragmentTexture:bind->texture->mtl_texture atIndex:slot];
            }
        } else {
            uint32_t slot = base.sampler + layout->ordinals[i];
            if (stages & gpu_stage_vertex) {
                [pass->mtl_enc setVertexSamplerState:bind->sampler->mtl_sampler atIndex:slot];
            }
            if (stages & gpu_stage_fragment) {
                [pass->mtl_enc setFragmentSamplerState:bind->sampler->mtl_sampler atIndex:slot];
            }
        }
    }

    pass->applied[index].group = group;
    pass->applied[index].base = base;
#elif cr_wasm
    wgpu_render_pass_encoder_set_bind_group(pass->wgpu_enc, index, group->wgpu_group, (uint32_t*)dynamic_offsets, dynamic_offset_count);
#endif
}

void gpu_render_pass_set_scissor(struct gpu_render_pass* pass, uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
#if cr_macos || cr_ios
    [pass->mtl_enc setScissorRect:(MTLScissorRect){ x, y, width, height }];
#elif cr_wasm
    wgpu_render_pass_encoder_set_scissor_rect(pass->wgpu_enc, x, y, width, height);
#endif
}

void gpu_render_pass_draw(struct gpu_render_pass* pass, uint32_t vertex_start, uint32_t vertex_count) {
    gpu_command_buffer_profile_mark_used(pass->command_buffer, pass->profile_pair);
#if cr_macos || cr_ios
    [pass->mtl_enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:vertex_start vertexCount:vertex_count];
#elif cr_wasm
    wgpu_render_pass_encoder_draw(pass->wgpu_enc, vertex_count, 1, vertex_start, 0);
#endif
}

static struct gpu_compute_pass* gpu_command_buffer_begin_compute_dispatch(struct gpu_command_buffer* command_buffer, bool concurrent, const char* label) {
    cr_check(label != NULL, "a compute pass needs a label");
    cr_check(command_buffer->pass_active == false, "a pass cannot begin while another is active");
    command_buffer->pass_active = true;

    struct gpu_compute_pass* pass = &command_buffer->compute_pass;
    memset(pass, 0, sizeof(*pass));
    pass->command_buffer = command_buffer;
    pass->profile_pair = gpu_command_buffer_profile_alloc_pair(command_buffer);
    gpu_command_buffer_profile_name_pair(command_buffer, pass->profile_pair, label);

#if cr_macos || cr_ios
    MTLComputePassDescriptor* mtl_desc = [MTLComputePassDescriptor computePassDescriptor];
    mtl_desc.dispatchType = MTLDispatchTypeConcurrent;
    pass->barrier_between_dispatches = concurrent == false;

    if (pass->profile_pair != uint32_max) {
        MTLComputePassSampleBufferAttachmentDescriptor* mtl_samples = mtl_desc.sampleBufferAttachments[0];
        mtl_samples.sampleBuffer = command_buffer->device->mtl_profiler_samples;
        mtl_samples.startOfEncoderSampleIndex = command_buffer->profile->slot * gpu_profiler_slot_samples + pass->profile_pair * 2;
        mtl_samples.endOfEncoderSampleIndex = command_buffer->profile->slot * gpu_profiler_slot_samples + pass->profile_pair * 2 + 1;
    }

    pass->mtl_enc = [command_buffer->mtl_command_buffer computeCommandEncoderWithDescriptor:mtl_desc];
    pass->mtl_enc.label = [NSString stringWithFormat:@"%s", label];
#elif cr_wasm
    (void)concurrent;
    WGpuComputePassDescriptor wgpu_desc = WGPU_COMPUTE_PASS_DESCRIPTOR_DEFAULT_INITIALIZER;
    if (pass->profile_pair != uint32_max) {
        wgpu_desc.timestampWrites.querySet = command_buffer->device->wgpu_profiler_query_set;
        wgpu_desc.timestampWrites.beginningOfPassWriteIndex = (int32_t)(command_buffer->profile->slot * gpu_profiler_slot_samples + pass->profile_pair * 2);
        wgpu_desc.timestampWrites.endOfPassWriteIndex = (int32_t)(command_buffer->profile->slot * gpu_profiler_slot_samples + pass->profile_pair * 2 + 1);
    }
    pass->wgpu_enc = wgpu_command_encoder_begin_compute_pass(command_buffer->wgpu_enc, &wgpu_desc);
#endif

    return pass;
}

struct gpu_compute_pass* gpu_command_buffer_begin_compute(struct gpu_command_buffer* command_buffer, const char* label) {
    return gpu_command_buffer_begin_compute_dispatch(command_buffer, false, label);
}

struct gpu_compute_pass* gpu_command_buffer_begin_compute_concurrent(struct gpu_command_buffer* command_buffer, const char* label) {
    return gpu_command_buffer_begin_compute_dispatch(command_buffer, true, label);
}

void gpu_compute_pass_barrier(struct gpu_compute_pass* pass) {
#if cr_macos || cr_ios
    [pass->mtl_enc memoryBarrierWithScope:MTLBarrierScopeBuffers | MTLBarrierScopeTextures];
#endif
}

void gpu_compute_pass_end(struct gpu_compute_pass* pass) {
    pass->command_buffer->pass_active = false;

#if cr_macos || cr_ios
    [pass->mtl_enc endEncoding];
    pass->mtl_enc = nil;
#elif cr_wasm
    wgpu_compute_pass_encoder_end(pass->wgpu_enc);
    pass->wgpu_enc = 0;
#endif
}

void gpu_compute_pass_set_pipeline(struct gpu_compute_pass* pass, struct gpu_compute_pipeline* pipeline) {
    if (pass->pipeline == pipeline) {
        return;
    }
    gpu_compile_require(pass->command_buffer->device, &pipeline->compile);
    pass->pipeline = pipeline;

#if cr_macos || cr_ios
    [pass->mtl_enc setComputePipelineState:pipeline->mtl_pipeline];
#elif cr_wasm
    wgpu_compute_pass_encoder_set_pipeline(pass->wgpu_enc, pipeline->wgpu_pipeline);
#endif
}

void gpu_compute_pass_set_bind_group(struct gpu_compute_pass* pass, uint32_t index, struct gpu_bind_group* group, const uint32_t* dynamic_offsets, uint32_t dynamic_offset_count) {
    cr_check(index < gpu_max_groups, "bind group indices are below %u", (uint32_t)gpu_max_groups);
    cr_check(dynamic_offset_count == group->layout->dynamic_count || dynamic_offset_count == 0, "the bind group takes %u dynamic offsets or none", group->layout->dynamic_count);

#if cr_macos || cr_ios
    struct gpu_bind_layout* layout = group->layout;
    struct gpu_slot_base base = { 0 };
    if (pass->pipeline) {
        base = pass->pipeline->bases[index];
    } else {
        cr_check(index == 0, "groups above 0 need the pipeline set first");
    }

    uint32_t dynamic_index = 0;

    for (uint32_t i = 0; i < layout->binding_count; i++) {
        const struct gpu_binding_desc* binding = &layout->bindings[i];
        const struct gpu_bind_desc* bind = &group->binds[i];

        if (gpu_binding_is_buffer(binding->type)) {
            uint32_t slot = base.buffer + layout->ordinals[i];
            uint32_t offset = bind->buffer_offset;
            if (binding->dynamic_offset && dynamic_offset_count != 0) {
                offset += dynamic_offsets[dynamic_index++];
            }
            assert(slot < gpu_max_compute_slots);
            if (pass->mtl_bound_buffers[slot] != bind->buffer->mtl_buffer) {
                [pass->mtl_enc setBuffer:bind->buffer->mtl_buffer offset:offset atIndex:slot];
                pass->mtl_bound_buffers[slot] = bind->buffer->mtl_buffer;
                pass->bound_offsets[slot] = offset;
            } else if (pass->bound_offsets[slot] != offset) {
                [pass->mtl_enc setBufferOffset:offset atIndex:slot];
                pass->bound_offsets[slot] = offset;
            }
        } else if (gpu_binding_is_texture(binding->type)) {
            uint32_t slot = base.texture + layout->ordinals[i];
            assert(slot < gpu_max_compute_slots);
            if (pass->mtl_bound_textures[slot] != bind->texture->mtl_texture) {
                [pass->mtl_enc setTexture:bind->texture->mtl_texture atIndex:slot];
                pass->mtl_bound_textures[slot] = bind->texture->mtl_texture;
            }
        } else {
            uint32_t slot = base.sampler + layout->ordinals[i];
            assert(slot < gpu_max_compute_slots);
            if (pass->mtl_bound_samplers[slot] != bind->sampler->mtl_sampler) {
                [pass->mtl_enc setSamplerState:bind->sampler->mtl_sampler atIndex:slot];
                pass->mtl_bound_samplers[slot] = bind->sampler->mtl_sampler;
            }
        }
    }
#elif cr_wasm
    wgpu_compute_pass_encoder_set_bind_group(pass->wgpu_enc, index, group->wgpu_group, (uint32_t*)dynamic_offsets, dynamic_offset_count);
#endif
}

#if cr_macos || cr_ios
static void gpu_compute_pass_order_dispatch(struct gpu_compute_pass* pass) {
    if (pass->barrier_between_dispatches && pass->has_dispatch) {
        [pass->mtl_enc memoryBarrierWithScope:MTLBarrierScopeBuffers | MTLBarrierScopeTextures];
    }
    pass->has_dispatch = true;
}
#endif

void gpu_compute_pass_dispatch(struct gpu_compute_pass* pass, uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z) {
    gpu_command_buffer_profile_mark_used(pass->command_buffer, pass->profile_pair);
#if cr_macos || cr_ios
    gpu_compute_pass_order_dispatch(pass);
    uint2 wg = pass->pipeline->workgroup_size;
    [pass->mtl_enc dispatchThreadgroups:MTLSizeMake(group_count_x, group_count_y, group_count_z)
              threadsPerThreadgroup:MTLSizeMake(wg.x, wg.y, 1)];
#elif cr_wasm
    wgpu_compute_pass_encoder_dispatch_workgroups(pass->wgpu_enc, group_count_x, group_count_y, group_count_z);
#endif
}

void gpu_compute_pass_dispatch_indirect(struct gpu_compute_pass* pass, struct gpu_buffer* buffer, uint32_t offset) {
    gpu_command_buffer_profile_mark_used(pass->command_buffer, pass->profile_pair);
#if cr_macos || cr_ios
    gpu_compute_pass_order_dispatch(pass);
    uint2 wg = pass->pipeline->workgroup_size;
    [pass->mtl_enc dispatchThreadgroupsWithIndirectBuffer:buffer->mtl_buffer indirectBufferOffset:offset
                                threadsPerThreadgroup:MTLSizeMake(wg.x, wg.y, 1)];
#elif cr_wasm
    wgpu_compute_pass_encoder_dispatch_workgroups_indirect(pass->wgpu_enc, buffer->wgpu_buffer, offset);
#endif
}
