#include "ui_render_impl.h"
#include "ui_blur.h"

#include "ui_blur_shared.h"

#define ui_blur_direct_max_radius 12.0f
#define ui_blur_max_levels 8
#define ui_blur_max_dispatches 3
#define ui_blur_coarse_level_max 2u

_Static_assert(sizeof(struct ui_blur_fused_params) <= ui_blur_params_stride, "");
_Static_assert(sizeof(struct ui_blur_scale_params) <= ui_blur_params_stride, "");
_Static_assert(sizeof(struct ui_blur_down_params) <= ui_blur_params_stride, "");
_Static_assert(ui_blur_chain_max_dispatches == ui_blur_max_dispatches, "");

static struct ui_blur_view ui_blur_tex_view(struct ui* ui, struct ui_transient_tex tex) {
    struct gpu_texture* texture = ui_transient_mip_count(ui, tex) > 1
        ? ui_transient_mip_view(ui, tex, 0)
        : ui_transient_texture(ui, tex);
    return (struct ui_blur_view) {
        .texture = texture,
        .page_size = ui_transient_tex_size(ui, tex),
        .origin = ui_transient_tex_origin(ui, tex),
        .size = make_uint2(tex.w, tex.h),
        .phys = ui_transient_tex_phys(ui, tex),
    };
}

struct ui_blur* ui_blur_create(struct cr_mem* mem, struct gpu_device* device, struct gpu_library* library) {
    struct ui_blur* blur = cr_malloc(mem, sizeof(struct ui_blur), 16, cr_loc);
    memset(blur, 0, sizeof(*blur));

    blur->mem = mem;
    blur->device = device;
    blur->sampler = gpu_sampler_create(device, &(struct gpu_sampler_desc) { .label = "ui blur clamp" });

    {
        struct gpu_binding_desc bindings[ui_blur_bind_count] = {
            [ui_blur_bind_params] = { .binding = ui_blur_bind_params, .type = gpu_binding_type_uniform, .stages = gpu_stage_compute },
            [ui_blur_bind_src] = { .binding = ui_blur_bind_src, .type = gpu_binding_type_texture, .stages = gpu_stage_compute },
            [ui_blur_bind_sampler] = { .binding = ui_blur_bind_sampler, .type = gpu_binding_type_sampler, .stages = gpu_stage_compute },
            [ui_blur_bind_dst] = { .binding = ui_blur_bind_dst, .type = gpu_binding_type_storage_texture, .stages = gpu_stage_compute, .format = gpu_format_rgba16f },
        };
        blur->layout = gpu_bind_layout_create(device, bindings, cr_count(bindings));
    }

    {
        struct gpu_compute_pipeline_desc desc = {
            .library = library,
            .name = "ui_blur_fused",
            .layouts = { blur->layout },
            .layout_count = 1,
            .workgroup_size = { ui_blur_tile_w, ui_blur_tile_h / ui_blur_rows_per_thread },
        };
        blur->pipeline_fused = gpu_compute_pipeline_create(device, &desc);
    }

    {
        struct gpu_compute_pipeline_desc desc = {
            .library = library,
            .name = "ui_blur_scale",
            .layouts = { blur->layout },
            .layout_count = 1,
            .workgroup_size = { 8, 8 },
        };
        blur->pipeline_scale = gpu_compute_pipeline_create(device, &desc);
    }

    {
        struct gpu_compute_pipeline_desc desc = {
            .library = library,
            .name = "ui_blur_down_direct",
            .layouts = { blur->layout },
            .layout_count = 1,
            .workgroup_size = { 8, 8 },
        };
        blur->pipeline_down_direct = gpu_compute_pipeline_create(device, &desc);
        desc.name = "ui_blur_up_direct";
        desc.workgroup_size = make_uint2(256, 1);
        blur->pipeline_up_direct = gpu_compute_pipeline_create(device, &desc);
    }

    {
        struct gpu_compute_pipeline_desc desc = {
            .library = library,
            .name = "ui_blur_down",
            .layouts = { blur->layout },
            .layout_count = 1,
            .workgroup_size = { 8, 8 },
        };
        blur->pipeline_down = gpu_compute_pipeline_create(device, &desc);
    }

    {
        struct gpu_binding_desc bindings[ui_blur_pair_bind_count] = {
            [ui_blur_bind_params] = { .binding = ui_blur_bind_params, .type = gpu_binding_type_uniform, .stages = gpu_stage_compute },
            [ui_blur_bind_src] = { .binding = ui_blur_bind_src, .type = gpu_binding_type_texture, .stages = gpu_stage_compute },
            [ui_blur_bind_sampler] = { .binding = ui_blur_bind_sampler, .type = gpu_binding_type_sampler, .stages = gpu_stage_compute },
            [ui_blur_bind_dst] = { .binding = ui_blur_bind_dst, .type = gpu_binding_type_storage_texture, .stages = gpu_stage_compute, .format = gpu_format_rgba16f },
            [ui_blur_bind_dst2] = { .binding = ui_blur_bind_dst2, .type = gpu_binding_type_storage_texture, .stages = gpu_stage_compute, .format = gpu_format_rgba16f },
        };
        blur->layout_pair = gpu_bind_layout_create(device, bindings, cr_count(bindings));
    }

    {
        struct gpu_binding_desc bindings[ui_blur_tail_bind_count] = {
            [ui_blur_bind_params] = { .binding = ui_blur_bind_params, .type = gpu_binding_type_uniform, .stages = gpu_stage_compute },
            [ui_blur_bind_src] = { .binding = ui_blur_bind_src, .type = gpu_binding_type_texture, .stages = gpu_stage_compute },
            [ui_blur_bind_sampler] = { .binding = ui_blur_bind_sampler, .type = gpu_binding_type_sampler, .stages = gpu_stage_compute },
            [ui_blur_bind_dst] = { .binding = ui_blur_bind_dst, .type = gpu_binding_type_storage_texture, .stages = gpu_stage_compute, .format = gpu_format_rgba16f },
            [ui_blur_bind_dst2] = { .binding = ui_blur_bind_dst2, .type = gpu_binding_type_storage_texture, .stages = gpu_stage_compute, .format = gpu_format_rgba16f },
            [ui_blur_bind_dst3] = { .binding = ui_blur_bind_dst3, .type = gpu_binding_type_storage_texture, .stages = gpu_stage_compute, .format = gpu_format_rgba16f },
            [ui_blur_bind_dst4] = { .binding = ui_blur_bind_dst4, .type = gpu_binding_type_storage_texture, .stages = gpu_stage_compute, .format = gpu_format_rgba16f },
        };
        blur->layout_tail = gpu_bind_layout_create(device, bindings, cr_count(bindings));
    }

    {
        struct gpu_compute_pipeline_desc desc = {
            .library = library,
            .name = "ui_blur_down_pair",
            .layouts = { blur->layout_pair },
            .layout_count = 1,
            .workgroup_size = { ui_blur_pair_tile, ui_blur_pair_tile },
        };
        blur->pipeline_pair = gpu_compute_pipeline_create(device, &desc);
    }

    {
        struct gpu_compute_pipeline_desc desc = {
            .library = library,
            .name = "ui_blur_down_tail",
            .layouts = { blur->layout_tail },
            .layout_count = 1,
            .workgroup_size = { ui_blur_tail_threads, 1 },
        };
        blur->pipeline_tail = gpu_compute_pipeline_create(device, &desc);
    }

    blur->unused_dst = gpu_texture_create(device, &(struct gpu_texture_desc) {
        .label = "ui blur unused dst",
        .format = gpu_format_rgba16f,
        .size = { 1, 1 },
        .usage = gpu_texture_usage_sample | gpu_texture_usage_storage_write,
    });

    return blur;
}

void ui_blur_destroy(struct ui_blur* blur) {
    gpu_texture_destroy(blur->device, blur->unused_dst);
    gpu_compute_pipeline_destroy(blur->device, blur->pipeline_fused);
    gpu_compute_pipeline_destroy(blur->device, blur->pipeline_scale);
    gpu_compute_pipeline_destroy(blur->device, blur->pipeline_down_direct);
    gpu_compute_pipeline_destroy(blur->device, blur->pipeline_up_direct);
    gpu_compute_pipeline_destroy(blur->device, blur->pipeline_down);
    gpu_compute_pipeline_destroy(blur->device, blur->pipeline_pair);
    gpu_compute_pipeline_destroy(blur->device, blur->pipeline_tail);
    gpu_bind_layout_destroy(blur->device, blur->layout_pair);
    gpu_bind_layout_destroy(blur->device, blur->layout_tail);
    gpu_bind_layout_destroy(blur->device, blur->layout);
    gpu_sampler_destroy(blur->device, blur->sampler);
    cr_free(blur->mem, blur, cr_loc);
}

struct ui_blur_plan {
    uint32_t level;
    float sigma;
    float extent;
};

static float ui_blur_up_var(uint32_t level) {
    float var = 0.0f;
    uint32_t k = level;

    if ((k & 1) != 0) {
        float h = (float)(1u << k);
        var += (3.0f / 16.0f) * h * h;
        k -= 1;
    }
    while (k >= 2) {
        float h = (float)(1u << k);
        var += (11.0f / 64.0f) * h * h;
        k -= 2;
    }

    return var;
}

static struct ui_blur_plan ui_blur_make_plan(float radius, uint2 size) {
    struct ui_blur_plan plan = { 0 };

    uint32_t max_level = 0;
    while (max_level + 1 < ui_blur_max_levels && (min_uint32(size.x, size.y) >> (max_level + 1)) >= 4) {
        max_level++;
    }

    if (radius <= ui_blur_direct_max_radius || max_level == 0) {
        plan.sigma = max_float(radius / (float)ui_blur_extent_per_sigma, 0.001f);
        plan.extent = radius;
        return plan;
    }

    float sigma = radius / (float)ui_blur_extent_per_sigma;
    float sigma_sq = sigma * sigma;

    for (uint32_t level = 1; ; level++) {
        float scale_sq = (float)(1u << (2 * level));
        float chain_var = (scale_sq - 1.0f) / 12.0f + ui_blur_up_var(level);
        float level_sigma = sqrtf(max_float(sigma_sq - chain_var, 0.0625f) / scale_sq);

        if ((float)ui_blur_extent_per_sigma * level_sigma <= (float)ui_blur_max_extent || level == max_level) {
            plan.level = level;
            plan.sigma = level_sigma;
            plan.extent = (float)ui_blur_extent_per_sigma * level_sigma;
            return plan;
        }
    }
}

struct ui_blur_kernel {
    uint32_t tap_count;
    uint32_t radius;
    float4 taps[ui_blur_max_tap_pairs];
    float4 weights[ui_blur_max_weight_vecs];
};

static void ui_blur_write_tap(float4* taps, uint32_t index, float2 tap) {
    if ((index & 1) == 0) {
        taps[index / 2].xy = tap;
    } else {
        taps[index / 2].zw = tap;
    }
}

static struct ui_blur_kernel ui_blur_build_kernel(float sigma, float extent) {
    struct ui_blur_kernel kernel;
    memset(&kernel, 0, sizeof(kernel));

    int32_t r = min_int32((int32_t)ceilf(extent), ui_blur_max_extent);

    if (r <= 0) {
        kernel.taps[0] = make_float4(0.0f, 1.0f, 0.0f, 0.0f);
        kernel.weights[0].x = 1.0f;
        kernel.tap_count = 1;
        return kernel;
    }

    float two_sigma_sq = 2.0f * sigma * sigma;

    float weights[ui_blur_max_extent + 1];
    float total = 0.0f;
    for (int32_t i = 0; i <= r; i++) {
        weights[i] = expf(-(float)(i * i) / two_sigma_sq);
        total += i == 0 ? weights[i] : 2.0f * weights[i];
    }

    for (int32_t i = 0; i <= r; i++) {
        kernel.weights[i / 4][i % 4] = weights[i] / total;
    }
    kernel.radius = (uint32_t)r;

    ui_blur_write_tap(kernel.taps, 0, make_float2(0.0f, weights[0] / total));
    kernel.tap_count = 1;

    for (int32_t i = 1; i <= r; i += 2) {
        float w0 = weights[i];
        float w1 = i + 1 <= r ? weights[i + 1] : 0.0f;
        float w = w0 + w1;
        float tap_x = w > 0.0f ? (float)i + w1 / w : (float)i;
        ui_blur_write_tap(kernel.taps, kernel.tap_count, make_float2(tap_x, w / total));
        kernel.tap_count++;
    }

    assert(kernel.tap_count <= 2 * ui_blur_max_tap_pairs);
    return kernel;
}

static uint2 ui_blur_level_size(uint2 size, uint32_t level) {
    uint32_t round = (1u << level) - 1u;
    return make_uint2(max_uint32((size.x + round) >> level, 1), max_uint32((size.y + round) >> level, 1));
}

struct ui_blur_dispatch {
    struct gpu_compute_pipeline* pipeline;
    struct gpu_texture* src;
    struct gpu_texture* dst;
    uint2 groups;
    bool is_fused;
    struct ui_blur_fused_params fused;
    struct ui_blur_scale_params scale;
};

static float2 ui_blur_view_texel(struct ui_blur_view view) {
    return 1.0f / to_float2_from_uint2(view.page_size);
}

static float2 ui_blur_view_uv_origin(struct ui_blur_view view) {
    return to_float2_from_uint2(view.origin) * ui_blur_view_texel(view);
}

static float2 ui_blur_view_uv_min(struct ui_blur_view view) {
    return (to_float2_from_uint2(view.origin) + 0.5f) * ui_blur_view_texel(view);
}

static float2 ui_blur_view_uv_max(struct ui_blur_view view) {
    return (to_float2_from_uint2(view.origin + view.size) - 0.5f) * ui_blur_view_texel(view);
}

static uint2 ui_blur_dst_store_size(struct ui_blur_view dst) {
    assert(dst.phys.x >= dst.size.x && dst.phys.y >= dst.size.y);
    assert(all_int2(dst.origin + dst.phys <= dst.page_size));
    uint2 store = make_uint2(min_uint32(dst.size.x + 1, dst.phys.x), min_uint32(dst.size.y + 1, dst.phys.y));
    return store;
}

static void ui_blur_push_fused(struct ui_blur* blur, struct ui_blur_dispatch* items, uint32_t* count, const struct ui_blur_kernel* kernel,
                               struct ui_blur_view src, struct ui_blur_view dst, float4 color, bool colorize) {
    uint2 store = ui_blur_dst_store_size(dst);

    struct ui_blur_dispatch* item = &items[(*count)++];
    memset(item, 0, sizeof(*item));
    item->pipeline = blur->pipeline_fused;
    item->src = src.texture;
    item->dst = dst.texture;
    item->groups = make_uint2(factor_up_uint32(store.x, ui_blur_tile_w), factor_up_uint32(store.y, ui_blur_tile_h));
    item->is_fused = true;
    item->fused.color = color;
    item->fused.colorize = colorize ? 1u : 0u;
    item->fused.uv_origin = ui_blur_view_uv_origin(src);
    item->fused.uv_texel = ui_blur_view_texel(src);
    item->fused.uv_min = ui_blur_view_uv_min(src);
    item->fused.uv_max = ui_blur_view_uv_max(src);
    item->fused.dst_origin = dst.origin;
    item->fused.dst_size = store;
    item->fused.tap_count = kernel->tap_count;
    item->fused.radius = kernel->radius;
    memcpy(item->fused.taps, kernel->taps, sizeof(kernel->taps));
    memcpy(item->fused.weights, kernel->weights, sizeof(kernel->weights));
}

static void ui_blur_push_scale(struct ui_blur* blur, struct ui_blur_dispatch* items, uint32_t* count,
                               struct ui_blur_view src, struct ui_blur_view dst, float4 color, bool colorize) {
    uint2 store = ui_blur_dst_store_size(dst);

    struct ui_blur_dispatch* item = &items[(*count)++];
    memset(item, 0, sizeof(*item));
    item->pipeline = blur->pipeline_scale;
    item->src = src.texture;
    item->dst = dst.texture;
    item->groups = make_uint2(factor_up_uint32(store.x, 8), factor_up_uint32(store.y, 8));
    item->scale.color = color;
    item->scale.colorize = colorize ? 1u : 0u;
    item->scale.uv_origin = ui_blur_view_uv_origin(src);
    item->scale.uv_step = ui_blur_view_texel(src);
    item->scale.uv_min = ui_blur_view_uv_min(src);
    item->scale.uv_max = ui_blur_view_uv_max(src);
    item->scale.dst_origin = dst.origin;
    item->scale.dst_size = store;
}

static void ui_blur_push_down_direct(struct ui_blur* blur, struct ui_blur_dispatch* items, uint32_t* count,
                                     struct ui_blur_view src, struct ui_blur_view dst, uint32_t level) {
    assert(level >= 1);
    uint2 store = ui_blur_dst_store_size(dst);

    struct ui_blur_dispatch* item = &items[(*count)++];
    memset(item, 0, sizeof(*item));
    item->pipeline = blur->pipeline_down_direct;
    item->src = src.texture;
    item->dst = dst.texture;
    item->groups = make_uint2(factor_up_uint32(store.x, 8), factor_up_uint32(store.y, 8));
    item->scale.color = make_float4(1.0f, 1.0f, 1.0f, 1.0f);
    item->scale.uv_origin = ui_blur_view_uv_origin(src);
    item->scale.uv_step = 2.0f * ui_blur_view_texel(src);
    item->scale.uv_min = ui_blur_view_uv_min(src);
    item->scale.uv_max = ui_blur_view_uv_max(src);
    item->scale.dst_origin = dst.origin;
    item->scale.dst_size = store;
    item->scale.taps = 1u << (level - 1);
}

static void ui_blur_push_up_direct(struct ui_blur* blur, struct ui_blur_dispatch* items, uint32_t* count,
                                   struct ui_blur_view src, struct ui_blur_view dst, uint32_t levels, float4 color, bool colorize) {
    assert(levels >= 1);
    uint2 store = ui_blur_dst_store_size(dst);

    struct ui_blur_dispatch* item = &items[(*count)++];
    memset(item, 0, sizeof(*item));
    item->pipeline = blur->pipeline_up_direct;
    item->src = src.texture;
    item->dst = dst.texture;
    item->groups = make_uint2(factor_up_uint32(store.x, 32), factor_up_uint32(store.y, 32));
    item->scale.color = color;
    item->scale.colorize = colorize ? 1u : 0u;
    item->scale.uv_origin = ui_blur_view_uv_origin(src);
    item->scale.uv_step = ui_blur_view_texel(src);
    item->scale.uv_min = ui_blur_view_uv_min(src);
    item->scale.uv_max = ui_blur_view_uv_max(src);
    item->scale.dst_origin = dst.origin;
    item->scale.dst_size = store;
    item->scale.levels = levels;
}

struct ui_blur_params_span {
    struct gpu_buffer* buffer;
    uint32_t base;
    struct gpu_buffer* temp_buffer;
    char* temp_raw;
    char* raw;
};

static struct ui_blur_params_span ui_blur_params_begin(struct ui_blur* blur, struct ui* ui, struct ui_slot* slot, uint32_t size) {
    struct ui_blur_params_span span = { 0 };

    if (slot) {
        span.buffer = slot->uniforms_buffer;
        span.raw = ui_alloc_uniforms(ui, size, &span.base);
    } else {
        span.temp_buffer = gpu_buffer_create(blur->device, size, gpu_buffer_usage_uniform | gpu_buffer_usage_write, "ui blur uniforms");
        span.buffer = span.temp_buffer;
        char* contents = gpu_buffer_get_contents(span.temp_buffer);
        span.temp_raw = contents == NULL ? cr_malloc(blur->mem, size, 16, cr_loc) : NULL;
        span.raw = contents != NULL ? contents : span.temp_raw;
    }

    memset(span.raw, 0, size);
    return span;
}

static void ui_blur_params_commit(struct ui_blur* blur, struct ui_blur_params_span* span, uint32_t size) {
    if (span->temp_raw != NULL) {
        gpu_buffer_write(blur->device, span->temp_buffer, 0, span->temp_raw, size);
        cr_free(blur->mem, span->temp_raw, cr_loc);
        span->temp_raw = NULL;
    }
}

uint32_t ui_blur_coarse_level(float radius, uint2 size) {
    struct ui_blur_plan plan = ui_blur_make_plan(radius, size);
    return min_uint32(plan.level, ui_blur_coarse_level_max);
}

uint2 ui_blur_coarse_size(uint2 size, uint32_t level) {
    return ui_blur_level_size(size, level);
}

static uint32_t ui_blur_build_items(struct ui_blur* blur, struct ui* ui, struct ui_blur_view src, struct ui_blur_view dst,
                                    float radius, float4 tint, bool colorize,
                                    struct ui_blur_dispatch* items, struct ui_transient_tex* reserved, uint32_t* out_reserved_count) {
    uint2 size = src.size;
    assert(src.texture != dst.texture);

    struct ui_blur_plan plan = ui_blur_make_plan(radius, size);
    struct ui_blur_kernel kernel = ui_blur_build_kernel(plan.sigma, plan.extent);

    uint32_t dst_level = all_int2(dst.size == size) ? 0 : min_uint32(plan.level, ui_blur_coarse_level_max);
    assert(all_int2(dst.size == ui_blur_level_size(size, dst_level)));

    uint32_t count = 0;
    uint32_t reserved_count = 0;

    if (radius <= 0.0f) {
        ui_blur_push_scale(blur, items, &count, src, dst, tint, colorize);
    } else if (plan.level == 0) {
        ui_blur_push_fused(blur, items, &count, &kernel, src, dst, tint, colorize);
    } else {
        uint2 coarse_dims = ui_blur_level_size(size, plan.level);

        struct ui_transient_tex down_tex = ui_transient_reserve_storage_channel(ui, gpu_format_rgba16f, coarse_dims.x, coarse_dims.y, ui_transient_channel_blur_down);
        reserved[reserved_count++] = down_tex;
        struct ui_blur_view down_view = ui_blur_tex_view(ui, down_tex);

        ui_blur_push_down_direct(blur, items, &count, src, down_view, plan.level);

        bool fused_last = dst_level == plan.level;

        struct ui_blur_view coarse;
        if (fused_last) {
            coarse = dst;
        } else {
            struct ui_transient_tex tex = ui_transient_reserve_storage_channel(ui, gpu_format_rgba16f, coarse_dims.x, coarse_dims.y, ui_transient_channel_blur_coarse);
            reserved[reserved_count++] = tex;
            coarse = ui_blur_tex_view(ui, tex);
        }

        ui_blur_push_fused(blur, items, &count, &kernel, down_view, coarse,
                           fused_last ? tint : make_float4(1.0f, 1.0f, 1.0f, 1.0f), fused_last ? colorize : false);

        if (fused_last == false) {
            ui_blur_push_up_direct(blur, items, &count, coarse, dst, plan.level - dst_level, tint, colorize);
        }
    }

    assert(count <= ui_blur_max_dispatches);

    *out_reserved_count = reserved_count;
    return count;
}

static struct ui_blur_params_span ui_blur_write_params(struct ui_blur* blur, struct ui* ui, struct ui_slot* slot,
                                                       const struct ui_blur_dispatch* items, uint32_t count) {
    uint32_t params_size = count * ui_blur_params_stride;
    struct ui_blur_params_span span = ui_blur_params_begin(blur, ui, slot, params_size);

    for (uint32_t i = 0; i < count; i++) {
        if (items[i].is_fused) {
            memcpy(span.raw + i * ui_blur_params_stride, &items[i].fused, sizeof(items[i].fused));
        } else {
            memcpy(span.raw + i * ui_blur_params_stride, &items[i].scale, sizeof(items[i].scale));
        }
    }

    ui_blur_params_commit(blur, &span, params_size);
    return span;
}

static struct gpu_bind_group* ui_blur_item_bind_group(struct ui_blur* blur, const struct ui_blur_params_span* span, const struct ui_blur_dispatch* item, uint32_t index) {
    struct gpu_bind_desc binds[ui_blur_bind_count] = {
        [ui_blur_bind_params] = { .binding = ui_blur_bind_params, .buffer = span->buffer, .buffer_offset = span->base + index * ui_blur_params_stride, .buffer_size = ui_blur_params_stride },
        [ui_blur_bind_src] = { .binding = ui_blur_bind_src, .texture = item->src },
        [ui_blur_bind_sampler] = { .binding = ui_blur_bind_sampler, .sampler = blur->sampler },
        [ui_blur_bind_dst] = { .binding = ui_blur_bind_dst, .texture = item->dst },
    };
    return gpu_bind_group_create(blur->device, blur->layout, binds, ui_blur_bind_count);
}

void ui_blur_chain_prepare_gpu(struct ui* ui, struct ui_slot* slot, struct ui_blur_view src, struct ui_blur_view dst,
                               float radius, float4 tint, bool colorize, struct ui_blur_chain* out) {
    struct ui_blur* blur = ui->render->blur;

    struct ui_blur_dispatch items[ui_blur_max_dispatches];

    memset(out, 0, sizeof(*out));
    out->count = ui_blur_build_items(blur, ui, src, dst, radius, tint, colorize, items, out->reserved, &out->reserved_count);

    struct ui_blur_params_span span = ui_blur_write_params(blur, ui, slot, items, out->count);
    out->temp_params = span.temp_buffer;

    for (uint32_t i = 0; i < out->count; i++) {
        out->pipelines[i] = items[i].pipeline;
        out->grids[i] = items[i].groups;
        out->groups[i] = ui_blur_item_bind_group(blur, &span, &items[i], i);
    }
}

void ui_blur_chain_prepare(struct ui* ui, struct ui_slot* slot, struct ui_transient_tex src, struct ui_transient_tex dst,
                           float radius, float4 tint, bool colorize, struct ui_blur_chain* out) {
    ui_blur_chain_prepare_gpu(ui, slot, ui_blur_tex_view(ui, src), ui_blur_tex_view(ui, dst), radius, tint, colorize, out);
}

void ui_blur_chain_prepare_region(struct ui* ui, struct ui_slot* slot, struct ui_transient_tex src, uint2 src_offset, uint2 src_size, struct ui_transient_tex dst,
                                  float radius, float4 tint, bool colorize, struct ui_blur_chain* out) {
    struct ui_blur_view src_view = ui_blur_tex_view(ui, src);
    src_view.origin = src_view.origin + src_offset;
    src_view.size = src_size;

    ui_blur_chain_prepare_gpu(ui, slot, src_view, ui_blur_tex_view(ui, dst), radius, tint, colorize, out);
}

void ui_blur_chain_encode(struct ui* ui, struct gpu_command_buffer* command_buffer, const struct ui_blur_chain* chain) {
    struct gpu_compute_pass* pass = gpu_command_buffer_begin_compute(command_buffer, "ui blur chain");
    for (uint32_t stage = 0; stage < chain->count; stage++) {
        ui_blur_chain_encode_stage(ui, pass, chain, stage);
    }
    gpu_compute_pass_end(pass);
}

void ui_blur_chain_encode_stage(struct ui* ui, struct gpu_compute_pass* pass, const struct ui_blur_chain* chain, uint32_t stage) {
    assert(stage < chain->count);
    gpu_compute_pass_set_pipeline(pass, chain->pipelines[stage]);
    gpu_compute_pass_set_bind_group(pass, 0, chain->groups[stage], NULL, 0);
    gpu_compute_pass_dispatch(pass, chain->grids[stage].x, chain->grids[stage].y, 1);
}

void ui_blur_chain_finish(struct ui* ui, struct ui_blur_chain* chain) {
    for (uint32_t i = 0; i < chain->count; i++) {
        gpu_bind_group_destroy(ui->render->blur->device, chain->groups[i]);
    }
    for (uint32_t i = 0; i < chain->reserved_count; i++) {
        ui_transient_release(ui, chain->reserved[i]);
    }
    if (chain->temp_params) {
        gpu_buffer_destroy(ui->render->blur->device, chain->temp_params);
        chain->temp_params = NULL;
    }
    chain->count = 0;
    chain->reserved_count = 0;
}

struct ui_blur_pyramid_tex ui_blur_pyramid_reserve(struct ui* ui, uint32_t width, uint32_t height, uint32_t level_count, uint32_t capture_log2) {
    struct ui_blur_pyramid_tex pyr;

    uint2 capture = ui_blur_level_size(make_uint2(width, height), capture_log2);
    pyr.tex = ui_transient_reserve_mips_storage(ui, gpu_format_rgba16f, capture.x, capture.y, min_uint32(level_count + 1, ui_blur_pyramid_max_levels + 1));
    pyr.mips = ui_transient_mip_count(ui, pyr.tex);
    pyr.source_size = make_uint2(width, height);
    pyr.capture_log2 = capture_log2;
    return pyr;
}

static uint2 ui_blur_mip_dims(uint2 page, uint32_t level) {
    return make_uint2(max_uint32(page.x >> level, 1), max_uint32(page.y >> level, 1));
}

static uint2 ui_blur_mip_origin(uint2 origin, uint32_t level) {
    assert((origin.x & ((1u << level) - 1u)) == 0 && (origin.y & ((1u << level) - 1u)) == 0);
    return make_uint2(origin.x >> level, origin.y >> level);
}

enum ui_blur_pyramid_stage_kind {
    ui_blur_pyramid_stage_head,
    ui_blur_pyramid_stage_single,
    ui_blur_pyramid_stage_pair,
    ui_blur_pyramid_stage_tail,
};

struct ui_blur_pyramid_stage {
    enum ui_blur_pyramid_stage_kind kind;
    uint32_t index;
    uint32_t levels;
};

struct ui_blur_pyramid_index_dims {
    uint2 src_page;
    uint2 src_origin;
    uint2 src_valid;
    uint2 dst_origin;
    uint2 dst_dims;
};

static struct ui_blur_pyramid_index_dims ui_blur_pyramid_index_dims(struct ui_blur_pyramid_tex pyr, uint2 base_page, uint2 base_offset, uint2 page, uint2 logical, uint2 origin, uint32_t index) {
    struct ui_blur_pyramid_index_dims d;
    if (index == 0) {
        d.src_page = base_page;
        d.src_origin = base_offset;
        d.src_valid = pyr.source_size;
    } else {
        d.src_page = ui_blur_mip_dims(page, index - 1);
        d.src_origin = ui_blur_mip_origin(origin, index - 1);
        d.src_valid = ui_blur_level_size(logical, index - 1);
    }
    d.dst_origin = ui_blur_mip_origin(origin, index);
    d.dst_dims = ui_blur_level_size(logical, index);
    return d;
}

void ui_blur_pyramid_prepare(struct ui* ui, struct ui_slot* slot, struct ui_blur_pyramid_tex pyr, struct gpu_texture* base_texture, uint2 base_page, uint2 base_offset, uint32_t level_count, uint32_t box_levels, struct ui_blur_pyramid_chain* out) {
    struct ui_blur* blur = ui->render->blur;

    uint2 page = ui_transient_tex_size(ui, pyr.tex);
    uint2 logical = make_uint2(pyr.tex.w, pyr.tex.h);
    uint2 origin = ui_transient_tex_origin(ui, pyr.tex);
    uint32_t pass_count = level_count + 1;

    assert(base_texture != NULL);
    assert(pass_count <= pyr.mips);

    uint32_t head_count = pass_count >= 2
        ? min_uint32(box_levels, min_uint32(ui_blur_box_max_levels, pass_count - 2)) + 2
        : 0;

    struct ui_blur_pyramid_stage stages[ui_blur_pyramid_max_levels + 1];
    uint32_t stage_count = 0;

    if (head_count != 0) {
        stages[stage_count++] = (struct ui_blur_pyramid_stage) { .kind = ui_blur_pyramid_stage_head, .index = 0, .levels = head_count };
    }

    for (uint32_t index = head_count; index < pass_count; ) {
        uint32_t remaining = pass_count - index;
        struct ui_blur_pyramid_index_dims d = ui_blur_pyramid_index_dims(pyr, base_page, base_offset, page, logical, origin, index);
        uint32_t px = d.dst_dims.x * d.dst_dims.y;

        uint2 l1 = make_uint2(max_uint32((d.dst_dims.x + 1) >> 1, 1), max_uint32((d.dst_dims.y + 1) >> 1, 1));

        if (remaining >= 3 && remaining <= ui_blur_tail_max_levels && px <= ui_blur_tail_max_px && l1.x * l1.y <= ui_blur_tail_max_px / 4) {
            stages[stage_count++] = (struct ui_blur_pyramid_stage) { .kind = ui_blur_pyramid_stage_tail, .index = index, .levels = remaining };
            index = pass_count;
        } else if (remaining >= 2) {
            stages[stage_count++] = (struct ui_blur_pyramid_stage) { .kind = ui_blur_pyramid_stage_pair, .index = index, .levels = 2 };
            index += 2;
        } else {
            stages[stage_count++] = (struct ui_blur_pyramid_stage) { .kind = ui_blur_pyramid_stage_single, .index = index, .levels = 1 };
            index += 1;
        }
    }

    memset(out, 0, sizeof(*out));
    out->pass_count = stage_count;

    uint32_t params_size = stage_count * ui_blur_params_stride;
    struct ui_blur_params_span span = ui_blur_params_begin(blur, ui, slot, params_size);

    for (uint32_t s = 0; s < stage_count; s++) {
        struct ui_blur_pyramid_stage stage = stages[s];
        uint32_t index = stage.index;
        struct ui_blur_pyramid_index_dims d = ui_blur_pyramid_index_dims(pyr, base_page, base_offset, page, logical, origin, index);

        float2 texel = 1.0f / to_float2_from_uint2(d.src_page);
        float2 tap_texel = index == 0 && pyr.capture_log2 == 0 ? 0.5f * texel : texel;
        float2 uv_origin = to_float2_from_uint2(d.src_origin) * texel;

        struct ui_blur_down_params params = {
            .uv_origin = uv_origin,
            .uv_texel = tap_texel,
            .uv_min = uv_origin + 0.5f * texel,
            .uv_max = uv_origin + (to_float2_from_uint2(d.src_valid) - 0.5f) * texel,
            .dst_origin = d.dst_origin,
            .dst_size = d.dst_dims,
            .mode = index == 0 ? ui_blur_down_mode_tap : ui_blur_down_mode_filter,
            .level_count = stage.levels,
        };

        switch (stage.kind) {
        case ui_blur_pyramid_stage_head: {
            uint32_t n = stage.levels - 2;
            uint2 dst1_dims = ui_blur_level_size(logical, n + 1);
            params.box_levels = n;
            params.dst0_size = d.dst_dims;
            params.dst_size = ui_blur_level_size(logical, n);
            params.dst1_size = dst1_dims;
            params.dst1_origin = ui_blur_mip_origin(origin, n + 1);
            out->grids[s] = make_uint2(factor_up_uint32(dst1_dims.x, ui_blur_pair_tile), factor_up_uint32(dst1_dims.y, ui_blur_pair_tile));
            break;
        }
        case ui_blur_pyramid_stage_single: {
            out->grids[s] = make_uint2(factor_up_uint32(d.dst_dims.x, 8), factor_up_uint32(d.dst_dims.y, 8));
            break;
        }
        case ui_blur_pyramid_stage_pair: {
            struct ui_blur_pyramid_index_dims d1 = ui_blur_pyramid_index_dims(pyr, base_page, base_offset, page, logical, origin, index + 1);
            params.dst1_size = d1.dst_dims;
            params.dst1_origin = d1.dst_origin;
            out->grids[s] = make_uint2(factor_up_uint32(d1.dst_dims.x, ui_blur_pair_tile), factor_up_uint32(d1.dst_dims.y, ui_blur_pair_tile));
            break;
        }
        case ui_blur_pyramid_stage_tail: {
            out->grids[s] = make_uint2(1, 1);
            break;
        }
        }

        memcpy(span.raw + s * ui_blur_params_stride, &params, sizeof(params));
    }

    ui_blur_params_commit(blur, &span, params_size);
    out->temp_params = span.temp_buffer;

    for (uint32_t s = 0; s < stage_count; s++) {
        struct ui_blur_pyramid_stage stage = stages[s];
        uint32_t index = stage.index;

        struct gpu_texture* src = index == 0 ? base_texture : ui_transient_mip_view(ui, pyr.tex, index - 1);
        struct gpu_bind_desc params_bind = { .binding = ui_blur_bind_params, .buffer = span.buffer, .buffer_offset = span.base + s * ui_blur_params_stride, .buffer_size = ui_blur_params_stride };
        struct gpu_bind_desc src_bind = { .binding = ui_blur_bind_src, .texture = src };
        struct gpu_bind_desc sampler_bind = { .binding = ui_blur_bind_sampler, .sampler = blur->sampler };

        switch (stage.kind) {
        case ui_blur_pyramid_stage_single: {
            struct gpu_bind_desc binds[ui_blur_bind_count] = {
                [ui_blur_bind_params] = params_bind,
                [ui_blur_bind_src] = src_bind,
                [ui_blur_bind_sampler] = sampler_bind,
                [ui_blur_bind_dst] = { .binding = ui_blur_bind_dst, .texture = ui_transient_mip_view(ui, pyr.tex, index) },
            };
            out->groups[s] = gpu_bind_group_create(blur->device, blur->layout, binds, ui_blur_bind_count);
            out->pipelines[s] = blur->pipeline_down;
            break;
        }
        case ui_blur_pyramid_stage_head:
        case ui_blur_pyramid_stage_pair: {
            uint32_t dst2_index = stage.kind == ui_blur_pyramid_stage_head ? stage.levels - 1 : index + 1;
            struct gpu_bind_desc binds[ui_blur_pair_bind_count] = {
                [ui_blur_bind_params] = params_bind,
                [ui_blur_bind_src] = src_bind,
                [ui_blur_bind_sampler] = sampler_bind,
                [ui_blur_bind_dst] = { .binding = ui_blur_bind_dst, .texture = ui_transient_mip_view(ui, pyr.tex, index) },
                [ui_blur_bind_dst2] = { .binding = ui_blur_bind_dst2, .texture = ui_transient_mip_view(ui, pyr.tex, dst2_index) },
            };
            out->groups[s] = gpu_bind_group_create(blur->device, blur->layout_pair, binds, ui_blur_pair_bind_count);
            out->pipelines[s] = blur->pipeline_pair;
            break;
        }
        case ui_blur_pyramid_stage_tail: {
            struct gpu_bind_desc binds[ui_blur_tail_bind_count] = {
                [ui_blur_bind_params] = params_bind,
                [ui_blur_bind_src] = src_bind,
                [ui_blur_bind_sampler] = sampler_bind,
            };
            for (uint32_t level = 0; level < ui_blur_tail_max_levels; level++) {
                struct gpu_texture* dst = level < stage.levels ? ui_transient_mip_view(ui, pyr.tex, index + level) : blur->unused_dst;
                binds[ui_blur_bind_dst + level] = (struct gpu_bind_desc) { .binding = ui_blur_bind_dst + level, .texture = dst };
            }
            out->groups[s] = gpu_bind_group_create(blur->device, blur->layout_tail, binds, ui_blur_tail_bind_count);
            out->pipelines[s] = blur->pipeline_tail;
            break;
        }
        }
    }
}

void ui_blur_pyramid_encode_pass(struct ui* ui, struct gpu_compute_pass* pass, const struct ui_blur_pyramid_chain* chain, uint32_t index) {
    assert(index < chain->pass_count);

    gpu_compute_pass_set_pipeline(pass, chain->pipelines[index]);
    gpu_compute_pass_set_bind_group(pass, 0, chain->groups[index], NULL, 0);
    gpu_compute_pass_dispatch(pass, chain->grids[index].x, chain->grids[index].y, 1);
}

void ui_blur_pyramid_finish(struct ui* ui, struct ui_blur_pyramid_chain* chain) {
    for (uint32_t index = 0; index < chain->pass_count; index++) {
        gpu_bind_group_destroy(ui->render->blur->device, chain->groups[index]);
    }
    if (chain->temp_params) {
        gpu_buffer_destroy(ui->render->blur->device, chain->temp_params);
        chain->temp_params = NULL;
    }
    chain->pass_count = 0;
}
