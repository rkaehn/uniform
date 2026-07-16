#include "ui_sdf.h"
#include "ui_slot.h"
#include "ui_corner.h"
#include "ui_path.h"
#include <stddef.h>

_Static_assert(sizeof(struct ui_sdf_scene_params) % 16 == 0, "slang cbuffer layout requires 16-byte struct size");
_Static_assert(sizeof(struct ui_sdf_params) % 16 == 0, "slang cbuffer layout requires 16-byte struct size");
_Static_assert(sizeof(struct ui_sdf_analytic_prim) % 16 == 0, "slang cbuffer array stride requires 16-byte elements");
_Static_assert(offsetof(struct ui_sdf_scene_params, mid_uv_scale) % 8 == 0, "float2 members need 8-byte alignment");

static uint32_t ui_sdf_geometry_bindings(struct gpu_binding_desc* bindings) {
    bindings[0] = (struct gpu_binding_desc) { .binding = ui_sdf_bind_params, .type = gpu_binding_type_uniform, .stages = gpu_stage_compute };
    bindings[1] = (struct gpu_binding_desc) { .binding = ui_sdf_bind_prims, .type = gpu_binding_type_storage_read, .stages = gpu_stage_compute };
    bindings[2] = (struct gpu_binding_desc) { .binding = ui_sdf_bind_seg_ae, .type = gpu_binding_type_storage_read, .stages = gpu_stage_compute };
    bindings[3] = (struct gpu_binding_desc) { .binding = ui_sdf_bind_seg_ext, .type = gpu_binding_type_storage_read, .stages = gpu_stage_compute };
    bindings[4] = (struct gpu_binding_desc) { .binding = ui_sdf_bind_chunks, .type = gpu_binding_type_storage_read, .stages = gpu_stage_compute };
    bindings[5] = (struct gpu_binding_desc) { .binding = ui_sdf_bind_supers, .type = gpu_binding_type_storage_read, .stages = gpu_stage_compute };
    return 6;
}

struct ui_sdf* ui_sdf_create(struct cr_mem* mem, struct gpu_device* device, struct gpu_library* library) {
    struct ui_sdf* sdf = cr_malloc(mem, sizeof(struct ui_sdf), 16, cr_loc);
    memset(sdf, 0, sizeof(*sdf));

    sdf->mem = mem;
    sdf->device = device;
    sdf->sampler_mip = gpu_sampler_create(device, &(struct gpu_sampler_desc) { .label = "ui sdf mip", .repeat = true, .mip = true });

    {
        struct gpu_binding_desc bindings[ui_sdf_bind_top_binding_count];
        uint32_t count = ui_sdf_geometry_bindings(bindings);
        bindings[count++] = (struct gpu_binding_desc) { .binding = ui_sdf_bind_top_args, .type = gpu_binding_type_storage, .stages = gpu_stage_compute };
        bindings[count++] = (struct gpu_binding_desc) { .binding = ui_sdf_bind_top_out, .type = gpu_binding_type_storage, .stages = gpu_stage_compute };
        assert(count == cr_count(bindings));
        sdf->layout_top = gpu_bind_layout_create(device, bindings, count);
    }

    {
        struct gpu_binding_desc bindings[ui_sdf_bind_mid_binding_count];
        uint32_t count = ui_sdf_geometry_bindings(bindings);
        bindings[count++] = (struct gpu_binding_desc) { .binding = ui_sdf_bind_mid_tiles, .type = gpu_binding_type_storage, .stages = gpu_stage_compute };
        bindings[count++] = (struct gpu_binding_desc) { .binding = ui_sdf_bind_mid_args, .type = gpu_binding_type_storage, .stages = gpu_stage_compute };
        bindings[count++] = (struct gpu_binding_desc) { .binding = ui_sdf_bind_mid_prim_mid, .type = gpu_binding_type_storage, .stages = gpu_stage_compute };
        bindings[count++] = (struct gpu_binding_desc) { .binding = ui_sdf_bind_mid_out, .type = gpu_binding_type_storage_texture, .stages = gpu_stage_compute, .format = gpu_format_rgba16f };
        assert(count == cr_count(bindings));
        sdf->layout_mid = gpu_bind_layout_create(device, bindings, count);
    }

    {
        struct gpu_binding_desc bindings[ui_sdf_bind_refine_binding_count];
        uint32_t count = ui_sdf_geometry_bindings(bindings);
        bindings[count++] = (struct gpu_binding_desc) { .binding = ui_sdf_bind_refine_tiles, .type = gpu_binding_type_storage_read, .stages = gpu_stage_compute };
        bindings[count++] = (struct gpu_binding_desc) { .binding = ui_sdf_bind_refine_prim_mid, .type = gpu_binding_type_storage_read, .stages = gpu_stage_compute };
        bindings[count++] = (struct gpu_binding_desc) { .binding = ui_sdf_bind_refine_out, .type = gpu_binding_type_storage_texture, .stages = gpu_stage_compute, .format = gpu_format_rgba16f };
        assert(count == cr_count(bindings));
        sdf->layout_refine = gpu_bind_layout_create(device, bindings, count);
    }

    {
        struct gpu_binding_desc bindings[ui_sdf_bind_scene_binding_count] = {
            { .binding = ui_sdf_bind_scene_params, .type = gpu_binding_type_uniform, .stages = gpu_stage_fragment },
            { .binding = ui_sdf_bind_scene_in_mid, .type = gpu_binding_type_texture, .stages = gpu_stage_fragment },
            { .binding = ui_sdf_bind_scene_in_sdf, .type = gpu_binding_type_texture, .stages = gpu_stage_fragment, .unfilterable = true },
            { .binding = ui_sdf_bind_scene_content, .type = gpu_binding_type_texture, .stages = gpu_stage_fragment },
            { .binding = ui_sdf_bind_scene_sampler, .type = gpu_binding_type_sampler, .stages = gpu_stage_fragment },
            { .binding = ui_sdf_bind_scene_analytic, .type = gpu_binding_type_uniform, .stages = gpu_stage_fragment },
        };
        sdf->layout_scene = gpu_bind_layout_create(device, bindings, cr_count(bindings));
    }

    {
        struct gpu_compute_pipeline_desc desc = {
            .library = library,
            .name = "ui_sdf_top",
            .layouts = { sdf->layout_top },
            .layout_count = 1,
            .workgroup_size = { 8, 8 },
        };
        sdf->pipeline_top = gpu_compute_pipeline_create(device, &desc);
    }

    {
        struct gpu_compute_pipeline_desc desc = {
            .library = library,
            .name = "ui_sdf_mid",
            .layouts = { sdf->layout_mid },
            .layout_count = 1,
            .workgroup_size = { 8, 8 },
        };
        sdf->pipeline_mid = gpu_compute_pipeline_create(device, &desc);
    }

    {
        struct gpu_compute_pipeline_desc desc = {
            .library = library,
            .name = "ui_sdf_refine",
            .layouts = { sdf->layout_refine },
            .layout_count = 1,
            .workgroup_size = { ui_sdf_mid_scale, ui_sdf_mid_scale },
        };
        sdf->pipeline_refine = gpu_compute_pipeline_create(device, &desc);
    }

    {
        static const char* fragment_names[ui_sdf_scene_pipeline_count] = {
            "ui_sdf_scene_solid",
            "ui_sdf_scene_content",
            "ui_sdf_scene_glass",
        };

        for (int i = 0; i < (int)ui_sdf_scene_pipeline_count; i++) {
            struct gpu_render_pipeline_desc desc = {
                .library = library,
                .vertex_name = "ui_sdf_scene_vertex",
                .fragment_name = fragment_names[i],
                .layouts = { sdf->layout_scene },
                .layout_count = 1,
                .color_format = gpu_format_rgba16f,
                .blend = true,
            };
            sdf->pipelines_scene[i] = gpu_render_pipeline_create(device, &desc);

            desc.color_format = gpu_format_bgra8;
            sdf->pipelines_scene_c8[i] = gpu_render_pipeline_create(device, &desc);
        }
    }

    {
        struct gpu_texture_desc desc = {
            .label = "ui sdf dummy content",
            .format = gpu_format_bgra8,
            .size = { 1, 1 },
            .usage = gpu_texture_usage_sample,
        };
        sdf->dummy_content = gpu_texture_create(device, &desc);
    }

    return sdf;
}

void ui_sdf_destroy(struct ui_sdf* sdf) {
    gpu_texture_destroy(sdf->device, sdf->dummy_content);
    gpu_sampler_destroy(sdf->device, sdf->sampler_mip);
    gpu_compute_pipeline_destroy(sdf->device, sdf->pipeline_top);
    gpu_compute_pipeline_destroy(sdf->device, sdf->pipeline_mid);
    gpu_compute_pipeline_destroy(sdf->device, sdf->pipeline_refine);
    for (int i = 0; i < (int)ui_sdf_scene_pipeline_count; i++) {
        gpu_render_pipeline_destroy(sdf->device, sdf->pipelines_scene[i]);
        gpu_render_pipeline_destroy(sdf->device, sdf->pipelines_scene_c8[i]);
    }
    gpu_bind_layout_destroy(sdf->device, sdf->layout_top);
    gpu_bind_layout_destroy(sdf->device, sdf->layout_mid);
    gpu_bind_layout_destroy(sdf->device, sdf->layout_refine);
    gpu_bind_layout_destroy(sdf->device, sdf->layout_scene);
    cr_free(sdf->mem, sdf, cr_loc);
}

static void ui_sdf_rect_path(struct ui_path* path, const struct ui_sdf_prim* prim) {
    double radius = prim->rect.radius;
    ui_path_rounded_rect(path, prim->rect.origin, make_double2(prim->rect.width, prim->rect.height),
                         (struct ui_corners) {
                             .profile = prim->rect.continuous ? ui_corner_profile_continuous : ui_corner_profile_circular,
                             .radius = make_double4(radius, radius, radius, radius),
                         });
}

static bool ui_sdf_rect_is_circle(double width, double height, double4 radius, bool continuous, double* out_radius) {
    if (fabs(width - height) > 1e-3) {
        return false;
    }

    double r_min = min_double(min_double(radius.x, radius.y), min_double(radius.z, radius.w));
    double r_max = max_double(max_double(radius.x, radius.y), max_double(radius.z, radius.w));
    double half = 0.5 * min_double(width, height);

    if (r_min < half - 1e-3 || r_max - r_min > 1e-3) {
        return false;
    }

    *out_radius = half;
    return true;
}

static void ui_sdf_build_segments(const double2* points, uint32_t point_count, double2 to_px_offset,
                                     float4* seg_ae, float2* seg_ext, float4* chunks, float4* supers) {
    uint32_t chunk_count = factor_up_uint32(point_count, ui_sdf_chunk_size);
    uint32_t super_count = factor_up_uint32(chunk_count, ui_sdf_super_size);

    for (uint32_t i = 0; i < point_count; i++) {
        float2 a = to_float2_from_double2(points[i] + to_px_offset);
        seg_ae[i].xy = a;
    }

    for (uint32_t c = 0; c < chunk_count; c++) {
        chunks[2 * c] = make_float4(INFINITY, INFINITY, -INFINITY, -INFINITY);
    }

    for (uint32_t i = 0, j = point_count - 1; i < point_count; j = i, i++) {
        float2 a = seg_ae[i].xy;
        float2 b = seg_ae[j].xy;
        float2 e = b - a;

        float ee = e.x * e.x + e.y * e.y;

        seg_ae[i].zw = e;
        seg_ext[i] = make_float2(ee > 0.0f ? 1.0f / ee : 0.0f, b.y);

        float4* bounds = &chunks[2 * (i / ui_sdf_chunk_size)];
        bounds->xy = min_float2(bounds->xy, min_float2(a, b));
        bounds->zw = max_float2(bounds->zw, max_float2(a, b));
    }

    for (uint32_t c = 0; c < chunk_count; c++) {
        uint32_t i0 = c * ui_sdf_chunk_size;
        uint32_t i1 = min_uint32(i0 + ui_sdf_chunk_size, point_count);
        float4 a = seg_ae[i1 - 1];
        float4 b = seg_ae[(i0 + point_count - 1) % point_count];
        chunks[2 * c + 1] = make_float4(a.x, a.y, b.x, b.y);
    }

    for (uint32_t sc = 0; sc < super_count; sc++) {
        uint32_t c0 = sc * ui_sdf_super_size;
        uint32_t c1 = min_uint32(c0 + ui_sdf_super_size, chunk_count);

        float4 bounds = make_float4(INFINITY, INFINITY, -INFINITY, -INFINITY);
        for (uint32_t c = c0; c < c1; c++) {
            float4 cb = chunks[2 * c];
            bounds.xy = min_float2(bounds.xy, cb.xy);
            bounds.zw = max_float2(bounds.zw, cb.zw);
        }
        supers[2 * sc] = bounds;

        uint32_t i0 = c0 * ui_sdf_chunk_size;
        uint32_t i1 = min_uint32(c1 * ui_sdf_chunk_size, point_count);
        float4 a = seg_ae[i1 - 1];
        float4 b = seg_ae[(i0 + point_count - 1) % point_count];
        supers[2 * sc + 1] = make_float4(a.x, a.y, b.x, b.y);
    }
}

static struct gpu_texture* ui_sdf_texture(struct gpu_device* device, enum gpu_format format, uint32_t width, uint32_t height, bool shared, const char* label) {
    struct gpu_texture_desc desc = {
        .label = label,
        .format = format,
        .size = { width, height },
        .usage = gpu_texture_usage_storage_write | gpu_texture_usage_sample | (shared ? gpu_texture_usage_read : 0u),
    };
    return gpu_texture_create(device, &desc);
}

static uint32_t ui_sdf_geometry_binds(const struct ui_sdf_frame* frame, struct gpu_bind_desc* binds) {
    binds[0] = (struct gpu_bind_desc) { .binding = ui_sdf_bind_params, .buffer = frame->geometry, .buffer_offset = frame->sdf_params_offset, .buffer_size = sizeof(struct ui_sdf_params) };
    binds[1] = (struct gpu_bind_desc) { .binding = ui_sdf_bind_prims, .buffer = frame->geometry, .buffer_offset = frame->prims_offset, .buffer_size = frame->seg_ae_offset - frame->prims_offset };
    binds[2] = (struct gpu_bind_desc) { .binding = ui_sdf_bind_seg_ae, .buffer = frame->geometry, .buffer_offset = frame->seg_ae_offset, .buffer_size = frame->seg_ext_offset - frame->seg_ae_offset };
    binds[3] = (struct gpu_bind_desc) { .binding = ui_sdf_bind_seg_ext, .buffer = frame->geometry, .buffer_offset = frame->seg_ext_offset, .buffer_size = frame->chunks_offset - frame->seg_ext_offset };
    binds[4] = (struct gpu_bind_desc) { .binding = ui_sdf_bind_chunks, .buffer = frame->geometry, .buffer_offset = frame->chunks_offset, .buffer_size = frame->supers_offset - frame->chunks_offset };
    binds[5] = (struct gpu_bind_desc) { .binding = ui_sdf_bind_supers, .buffer = frame->geometry, .buffer_offset = frame->supers_offset, .buffer_size = frame->geometry_size - frame->supers_offset };
    return 6;
}

void ui_sdf_frame_reserve(struct ui_sdf* sdf, struct ui* ui, struct ui_sdf_frame* frame, uint32_t width, uint32_t height, bool shared_textures) {
    frame->width = width;
    frame->height = height;
    frame->width_top = factor_up_uint32(width, ui_sdf_top_scale);
    frame->height_top = factor_up_uint32(height, ui_sdf_top_scale);
    frame->width_mid = factor_up_uint32(width, ui_sdf_mid_scale);
    frame->height_mid = factor_up_uint32(height, ui_sdf_mid_scale);

    if (shared_textures) {
        frame->transient = false;
        frame->mid = ui_sdf_texture(sdf->device, gpu_format_rgba16f, frame->width_mid, frame->height_mid, true, "ui sdf mid");
        frame->fine = ui_sdf_texture(sdf->device, gpu_format_rgba16f, width, height, true, "ui sdf fine");
        frame->mid_page = make_uint2(frame->width_mid, frame->height_mid);
    } else {
        frame->transient = true;
        frame->mid_tex = ui_transient_reserve_storage_dedicated(ui, gpu_format_rgba16f, frame->width_mid, frame->height_mid);
        frame->fine_tex = ui_transient_reserve_storage_dedicated(ui, gpu_format_rgba16f, width, height);
        frame->mid = ui_transient_texture(ui, frame->mid_tex);
        frame->fine = ui_transient_texture(ui, frame->fine_tex);
        frame->mid_page = ui_transient_tex_size(ui, frame->mid_tex);
    }
}

void ui_sdf_frame_build(struct ui_sdf* sdf, struct ui* ui, struct ui_sdf_frame* frame,
                                  const struct ui_sdf_geometry* geometry) {
    uint32_t prim_count = geometry->prim_count;
    assert(prim_count >= 1 && prim_count <= ui_sdf_max_prims);

    struct ui_arr_double2* rings[ui_sdf_max_prims] = { 0 };
    struct ui_arr_uint32* contours[ui_sdf_max_prims] = { 0 };
    struct ui_sdf_prim_gpu gpu_prims[ui_sdf_max_prims];

    uint32_t seg_count = 0;
    uint32_t chunk_count = 0;
    uint32_t super_count = 0;

    for (uint32_t i = 0; i < prim_count; i++) {
        const struct ui_sdf_prim* prim = &geometry->prims[i];
        struct ui_sdf_prim circle_prim;

        if (prim->type == ui_sdf_prim_type_rect) {
            double circle_radius;
            if (ui_sdf_rect_is_circle(prim->rect.width, prim->rect.height,
                                         make_double4(prim->rect.radius, prim->rect.radius, prim->rect.radius, prim->rect.radius),
                                         prim->rect.continuous, &circle_radius)) {
                circle_prim = (struct ui_sdf_prim) {
                    .type = ui_sdf_prim_type_circle,
                    .circle = {
                        .center = prim->rect.origin + make_double2(0.5 * prim->rect.width, 0.5 * prim->rect.height),
                        .radius = circle_radius,
                    },
                };
                prim = &circle_prim;
            }
        }

        if (prim->type == ui_sdf_prim_type_circle) {
            gpu_prims[i] = (struct ui_sdf_prim_gpu) {
                .type = ui_sdf_prim_gpu_type_circle,
                .circle_x = (float)(prim->circle.center.x + geometry->to_px_offset.x),
                .circle_y = (float)(prim->circle.center.y + geometry->to_px_offset.y),
                .circle_radius = (float)prim->circle.radius,
            };
            continue;
        }

        struct ui_path rect_path;
        const struct ui_path* path = &rect_path;
        double2 offset = make_double2(0.0, 0.0);
        double scale = 1.0;
        bool stroke = false;
        if (prim->type == ui_sdf_prim_type_rect) {
            ui_path_init(&rect_path, sdf->mem);
            ui_sdf_rect_path(&rect_path, prim);
        } else {
            path = prim->path.path;
            offset = prim->path.offset;
            scale = prim->path.scale;
            stroke = prim->path.band_half > 0.0;
        }

        struct ui_arr_double2* points = ui_arr_double2_create(sdf->mem);
        struct ui_arr_uint32* starts = ui_arr_uint32_create(sdf->mem);
        ui_path_to_points(path, offset, scale, points, starts, geometry->flatten_epsilon, (double)geometry->flatten_max_angle);

        if (prim->type == ui_sdf_prim_type_rect) {
            ui_path_deinit(&rect_path);
        }

        rings[i] = ui_arr_double2_create(sdf->mem);
        contours[i] = ui_arr_uint32_create(sdf->mem);

        uint32_t contour_count = ui_arr_uint32_get_count(starts);
        uint32_t flat_count = ui_arr_double2_get_count(points);

        for (uint32_t c = 0; c < contour_count; c++) {
            uint32_t src_start = ui_arr_uint32_get(starts, c);
            uint32_t src_end = c + 1 < contour_count ? ui_arr_uint32_get(starts, c + 1) : flat_count;
            uint32_t src_count = src_end - src_start;
            const double2* src = ui_arr_double2_get_ptr(points, src_start);

            bool closed = src_count >= 2 && is_equal_double2(src[0], src[src_count - 1]);
            if (closed) {
                src_count--;
            }

            uint32_t pair_count = ui_arr_uint32_get_count(contours[i]);
            if (pair_count != 0) {
                uint32_t prev_start = ui_arr_uint32_get(contours[i], pair_count - 2);
                uint32_t ring_count = ui_arr_double2_get_count(rings[i]);
                uint32_t aligned = align_up_uint32(ring_count, ui_sdf_chunk_size * ui_sdf_super_size);
                double2 pad = ui_arr_double2_get(rings[i], prev_start);
                for (uint32_t n = ring_count; n < aligned; n++) {
                    ui_arr_double2_append(rings[i], pad);
                }
                ui_arr_uint32_set(contours[i], pair_count - 1, aligned - prev_start);
            }

            uint32_t ring_start = ui_arr_double2_get_count(rings[i]);
            for (uint32_t n = 0; n < src_count; n++) {
                ui_arr_double2_append(rings[i], src[n]);
            }
            if (stroke && closed == false) {
                for (uint32_t n = src_count - 1; n-- > 1;) {
                    ui_arr_double2_append(rings[i], src[n]);
                }
            }

            ui_arr_uint32_append(contours[i], ring_start);
            ui_arr_uint32_append(contours[i], ui_arr_double2_get_count(rings[i]) - ring_start);
        }

        ui_arr_double2_destroy(points);
        ui_arr_uint32_destroy(starts);

        uint32_t count = ui_arr_double2_get_count(rings[i]);
        assert(count >= 1);

        gpu_prims[i] = (struct ui_sdf_prim_gpu) {
            .type = ui_sdf_prim_gpu_type_path,
            .seg_offset = seg_count,
            .seg_count = count,
            .chunk_offset = chunk_count,
            .super_offset = super_count,
            .band_half = prim->type == ui_sdf_prim_type_path ? (float)prim->path.band_half : 0.0f,
        };

        seg_count += count;
        chunk_count += factor_up_uint32(count, ui_sdf_chunk_size);
        super_count += factor_up_uint32(factor_up_uint32(count, ui_sdf_chunk_size), ui_sdf_super_size);
    }

    uint32_t tile_capacity = frame->width_mid * frame->height_mid;

    frame->sdf_params = (struct ui_sdf_params) {
        .top_size = make_uint2(frame->width_top, frame->height_top),
        .mid_size = make_uint2(frame->width_mid, frame->height_mid),
        .fine_size = make_uint2(frame->width, frame->height),
        .prim_count = prim_count,
        .combine = geometry->combine,
        .use_top = tile_capacity > ui_sdf_use_top_min_tiles ? 1u : 0u,
        .smoothing = geometry->smoothing,
        .band_lo = geometry->band_lo,
        .band_hi = geometry->band_hi,
        .exact_mid = geometry->exact_mid ? 1u : 0u,
        .top_base = align_up_po2_uint32(tile_capacity * prim_count, 6),
    };

    frame->sdf_params_offset = 0;
    frame->prims_offset = align_up_po2_uint32((uint32_t)sizeof(struct ui_sdf_params), 8);
    frame->seg_ae_offset = frame->prims_offset + align_up_po2_uint32((uint32_t)(prim_count * sizeof(struct ui_sdf_prim_gpu)), 8);
    frame->seg_ext_offset = frame->seg_ae_offset + align_up_po2_uint32(max_uint32(seg_count, 1) * (uint32_t)sizeof(float4), 8);
    frame->chunks_offset = frame->seg_ext_offset + align_up_po2_uint32(max_uint32(seg_count, 1) * (uint32_t)sizeof(float2), 8);
    frame->supers_offset = frame->chunks_offset + align_up_po2_uint32(max_uint32(2 * chunk_count, 1) * (uint32_t)sizeof(float4), 8);
    frame->geometry_size = frame->supers_offset + align_up_po2_uint32(max_uint32(2 * super_count, 1) * (uint32_t)sizeof(float4), 8);

    frame->geometry = gpu_buffer_create(sdf->device, frame->geometry_size, gpu_buffer_usage_uniform | gpu_buffer_usage_storage | gpu_buffer_usage_write, "ui sdf geometry");

    char* contents = gpu_buffer_get_contents(frame->geometry);
    char* raw = contents ? contents : cr_malloc(sdf->mem, frame->geometry_size, 16, cr_loc);

    memcpy(raw + frame->sdf_params_offset, &frame->sdf_params, sizeof(frame->sdf_params));
    memcpy(raw + frame->prims_offset, gpu_prims, prim_count * sizeof(struct ui_sdf_prim_gpu));

    for (uint32_t i = 0; i < prim_count; i++) {
        if (rings[i] == NULL) {
            continue;
        }
        const double2* ring = ui_arr_double2_get_data(rings[i]);

        for (uint32_t c = 0, c_end = ui_arr_uint32_get_count(contours[i]); c < c_end; c += 2) {
            uint32_t start = ui_arr_uint32_get(contours[i], c);
            uint32_t count = ui_arr_uint32_get(contours[i], c + 1);

            ui_sdf_build_segments(ring + start, count, geometry->to_px_offset,
                                     (float4*)(raw + frame->seg_ae_offset) + gpu_prims[i].seg_offset + start,
                                     (float2*)(raw + frame->seg_ext_offset) + gpu_prims[i].seg_offset + start,
                                     (float4*)(raw + frame->chunks_offset) + 2 * (gpu_prims[i].chunk_offset + start / ui_sdf_chunk_size),
                                     (float4*)(raw + frame->supers_offset) + 2 * (gpu_prims[i].super_offset + start / (ui_sdf_chunk_size * ui_sdf_super_size)));
        }

        ui_arr_double2_destroy(rings[i]);
        ui_arr_uint32_destroy(contours[i]);
    }

    if (contents == NULL) {
        gpu_buffer_write(sdf->device, frame->geometry, 0, raw, frame->geometry_size);
        cr_free(sdf->mem, raw, cr_loc);
    }

    frame->tiles_offset = 256;
    frame->prim_mid_offset = frame->tiles_offset + align_up_po2_uint32(tile_capacity * (uint32_t)sizeof(uint32_t), 8);
    frame->top_offset = frame->prim_mid_offset + frame->sdf_params.top_base * (uint32_t)sizeof(float);
    frame->scratch_size = frame->top_offset + frame->width_top * frame->height_top * (uint32_t)sizeof(float);

    frame->scratch = gpu_buffer_create(sdf->device, frame->scratch_size, gpu_buffer_usage_storage | gpu_buffer_usage_indirect, "ui sdf scratch");

    {
        struct gpu_bind_desc binds[ui_sdf_bind_top_binding_count];
        uint32_t count = ui_sdf_geometry_binds(frame, binds);
        binds[count++] = (struct gpu_bind_desc) { .binding = ui_sdf_bind_top_args, .buffer = frame->scratch, .buffer_size = 16 };
        binds[count++] = (struct gpu_bind_desc) { .binding = ui_sdf_bind_top_out, .buffer = frame->scratch, .buffer_offset = frame->top_offset, .buffer_size = frame->scratch_size - frame->top_offset };
        frame->group_top = gpu_bind_group_create(sdf->device, sdf->layout_top, binds, count);
    }

    {
        struct gpu_bind_desc binds[ui_sdf_bind_mid_binding_count];
        uint32_t count = ui_sdf_geometry_binds(frame, binds);
        binds[count++] = (struct gpu_bind_desc) { .binding = ui_sdf_bind_mid_tiles, .buffer = frame->scratch, .buffer_offset = frame->tiles_offset, .buffer_size = frame->prim_mid_offset - frame->tiles_offset };
        binds[count++] = (struct gpu_bind_desc) { .binding = ui_sdf_bind_mid_args, .buffer = frame->scratch, .buffer_size = 16 };
        binds[count++] = (struct gpu_bind_desc) { .binding = ui_sdf_bind_mid_prim_mid, .buffer = frame->scratch, .buffer_offset = frame->prim_mid_offset, .buffer_size = frame->scratch_size - frame->prim_mid_offset };
        binds[count++] = (struct gpu_bind_desc) { .binding = ui_sdf_bind_mid_out, .texture = frame->mid };
        frame->group_mid = gpu_bind_group_create(sdf->device, sdf->layout_mid, binds, count);
    }

    {
        struct gpu_bind_desc binds[ui_sdf_bind_refine_binding_count];
        uint32_t count = ui_sdf_geometry_binds(frame, binds);
        binds[count++] = (struct gpu_bind_desc) { .binding = ui_sdf_bind_refine_tiles, .buffer = frame->scratch, .buffer_offset = frame->tiles_offset, .buffer_size = frame->prim_mid_offset - frame->tiles_offset };
        binds[count++] = (struct gpu_bind_desc) { .binding = ui_sdf_bind_refine_prim_mid, .buffer = frame->scratch, .buffer_offset = frame->prim_mid_offset, .buffer_size = frame->top_offset - frame->prim_mid_offset };
        binds[count++] = (struct gpu_bind_desc) { .binding = ui_sdf_bind_refine_out, .texture = frame->fine };
        frame->group_refine = gpu_bind_group_create(sdf->device, sdf->layout_refine, binds, count);
    }
}

void ui_sdf_frame_destroy(struct ui_sdf* sdf, struct ui* ui, struct ui_slot* slot, struct ui_sdf_frame* frame) {
    if (frame->analytic) {
        memset(frame, 0, sizeof(*frame));
        return;
    }
    gpu_bind_group_destroy(sdf->device, frame->group_top);
    gpu_bind_group_destroy(sdf->device, frame->group_mid);
    gpu_bind_group_destroy(sdf->device, frame->group_refine);
    if (frame->transient) {
        ui_transient_release(ui, frame->mid_tex);
        ui_transient_release(ui, frame->fine_tex);
    } else {
        ui_slot_destroy_texture(sdf->device, slot, frame->mid);
        ui_slot_destroy_texture(sdf->device, slot, frame->fine);
    }
    ui_slot_destroy_buffer(sdf->device, slot, frame->geometry);
    ui_slot_destroy_buffer(sdf->device, slot, frame->scratch);
    memset(frame, 0, sizeof(*frame));
}

void ui_sdf_encode_top(struct ui_sdf* sdf, struct gpu_compute_pass* pass, const struct ui_sdf_frame* frame) {
    if (frame->analytic) {
        return;
    }
    gpu_compute_pass_set_pipeline(pass, sdf->pipeline_top);
    gpu_compute_pass_set_bind_group(pass, 0, frame->group_top, NULL, 0);

    if (frame->sdf_params.use_top != 0) {
        gpu_compute_pass_dispatch(pass, factor_up_uint32(frame->width_top, 8), factor_up_uint32(frame->height_top, 8), 1);
    } else {
        gpu_compute_pass_dispatch(pass, 1, 1, 1);
    }
}

void ui_sdf_encode_mid(struct ui_sdf* sdf, struct gpu_compute_pass* pass, const struct ui_sdf_frame* frame) {
    if (frame->analytic) {
        return;
    }
    gpu_compute_pass_set_pipeline(pass, sdf->pipeline_mid);
    gpu_compute_pass_set_bind_group(pass, 0, frame->group_mid, NULL, 0);
    gpu_compute_pass_dispatch(pass, factor_up_uint32(frame->width_mid, 8), factor_up_uint32(frame->height_mid, 8), 1);
}

void ui_sdf_encode_refine(struct ui_sdf* sdf, struct gpu_compute_pass* pass, const struct ui_sdf_frame* frame) {
    if (frame->analytic) {
        return;
    }
    gpu_compute_pass_set_pipeline(pass, sdf->pipeline_refine);
    gpu_compute_pass_set_bind_group(pass, 0, frame->group_refine, NULL, 0);
    gpu_compute_pass_dispatch_indirect(pass, frame->scratch, 0);
}

bool ui_sdf_scene_analytic(struct ui_sdf_scene_desc* desc) {
    for (uint32_t i = 0; i < desc->shape_count; i++) {
        const struct ui_scene_shape* shape = &desc->shapes[i];
        if (shape->type == ui_shape_type_path) {
            return false;
        }
        if (shape->type == ui_shape_type_ellipse && fabsf(shape->size.x - shape->size.y) > 1e-3f) {
            return false;
        }
    }

    for (uint32_t i = 0; i < desc->shape_count; i++) {
        const struct ui_scene_shape* shape = &desc->shapes[i];
        double2 origin = to_double2_from_float2(shape->origin);
        double2 size = to_double2_from_float2(shape->size);

        if (shape->type == ui_shape_type_ellipse) {
            double r = 0.5 * (double)shape->size.x;
            ui_corner_analytic(origin, size, make_double4(r, r, r, r), false,
                                     desc->to_px_offset, &desc->analytic.prims[i]);
        } else {
            float4 r = shape->radius;
            ui_corner_analytic(origin, size, make_double4((double)r.x, (double)r.y, (double)r.w, (double)r.z),
                                     shape->profile == ui_corner_profile_continuous,
                                     desc->to_px_offset, &desc->analytic.prims[i]);
        }
    }

    desc->params.analytic_count = desc->shape_count;
    desc->params.analytic_combine = desc->combine;
    desc->params.analytic_smoothing = desc->smoothing;
    float reach = max_float(max_float(-desc->params.reach_lo, desc->params.reach_hi),
                            max_float(-desc->params.band_lo, desc->params.band_hi));
    desc->params.analytic_range = reach + desc->smoothing + 16.0f;
    return true;
}

void ui_sdf_scene_prepare(struct ui_sdf* sdf, struct ui* ui, struct ui_sdf_scene_desc* desc, struct ui_sdf_frame* frame) {
    assert(desc->shape_count >= 1 && desc->shape_count <= ui_sdf_max_prims);
    assert(desc->draw_color || desc->draw_glass);

    if (ui_sdf_scene_analytic(desc)) {
        memset(frame, 0, sizeof(*frame));
        frame->analytic = true;
        return;
    }

    struct ui_sdf_prim prims[ui_sdf_max_prims];
    struct ui_path paths[ui_sdf_max_prims];

    for (uint32_t i = 0; i < desc->shape_count; i++) {
        const struct ui_scene_shape* shape = &desc->shapes[i];
        ui_path_init(&paths[i], sdf->mem);

        if (shape->type == ui_shape_type_path) {
            prims[i] = (struct ui_sdf_prim) {
                .type = ui_sdf_prim_type_path,
                .path = {
                    .path = shape->path,
                    .offset = to_double2_from_float2(shape->path_offset),
                    .scale = (double)shape->path_scale,
                    .band_half = (double)shape->path_half,
                },
            };
            continue;
        }

        if (shape->type == ui_shape_type_ellipse && fabsf(shape->size.x - shape->size.y) < 1e-3f) {
            prims[i] = (struct ui_sdf_prim) {
                .type = ui_sdf_prim_type_circle,
                .circle = {
                    .center = to_double2_from_float2(shape->origin + shape->size * 0.5f),
                    .radius = (double)shape->size.x * 0.5,
                },
            };
            continue;
        }

        if (shape->type == ui_shape_type_rect) {
            double circle_radius;
            if (ui_sdf_rect_is_circle((double)shape->size.x, (double)shape->size.y,
                                         to_double4_from_float4(shape->radius),
                                         shape->profile == ui_corner_profile_continuous, &circle_radius)) {
                prims[i] = (struct ui_sdf_prim) {
                    .type = ui_sdf_prim_type_circle,
                    .circle = {
                        .center = to_double2_from_float2(shape->origin + shape->size * 0.5f),
                        .radius = circle_radius,
                    },
                };
                continue;
            }
        }

        struct ui_path* path = &paths[i];

        if (shape->type == ui_shape_type_ellipse) {
            ui_path_ellipse(path, to_double2_from_float2(shape->origin), to_double2_from_float2(shape->size));
        } else {
            ui_path_rounded_rect(path, to_double2_from_float2(shape->origin), to_double2_from_float2(shape->size),
                                 (struct ui_corners) { .profile = shape->profile, .radius = to_double4_from_float4(shape->radius) });
        }

        prims[i] = (struct ui_sdf_prim) { .type = ui_sdf_prim_type_path, .path = { .path = path, .scale = 1.0 } };
    }

    double flatten_epsilon = 0.1;
    double flatten_max_angle = M_PI;

    if (desc->draw_glass) {
        double quant = 2.0 / 255.0;
        flatten_max_angle = quant;
        flatten_epsilon = 0.05;
    }

    struct ui_sdf_geometry geometry = {
        .prims = prims,
        .prim_count = desc->shape_count,
        .combine = desc->combine,
        .smoothing = desc->smoothing,
        .band_lo = desc->params.band_lo,
        .band_hi = desc->params.band_hi,
        .flatten_epsilon = flatten_epsilon,
        .flatten_max_angle = flatten_max_angle,
        .to_px_offset = desc->to_px_offset,
        .exact_mid = desc->draw_glass,
    };

    memset(frame, 0, sizeof(*frame));
    ui_sdf_frame_reserve(sdf, ui, frame, desc->params.fine_size.x, desc->params.fine_size.y, false);

    ui_sdf_frame_build(sdf, ui, frame, &geometry);

    for (uint32_t i = 0; i < desc->shape_count; i++) {
        ui_path_deinit(&paths[i]);
    }
}

void ui_sdf_scene_composite(struct ui_sdf* sdf, struct ui* ui, struct ui_slot* slot, struct gpu_render_pass* render_pass,
                               enum gpu_format target_format, const struct ui_sdf_frame* frame, const struct ui_sdf_scene_desc* desc) {
    uint32_t params_offset;
    struct ui_sdf_scene_params* params = ui_alloc_uniforms(ui, sizeof(*params), &params_offset);
    *params = desc->params;

    if (frame->analytic == false) {
        float2 page = to_float2_from_uint2(frame->mid_page);
        params->mid_uv_scale = 1.0f / ((float)ui_sdf_mid_scale * page);
        params->mid_uv_min = 0.5f / page;
        params->mid_uv_max = (make_float2((float)frame->width_mid, (float)frame->height_mid) - 0.5f) / page;
    }

    uint32_t analytic_offset = 0;
    if (desc->params.analytic_count != 0) {
        struct ui_sdf_scene_analytic_prims* analytic = ui_alloc_uniforms(ui, sizeof(*analytic), &analytic_offset);
        *analytic = desc->analytic;
    }

    struct gpu_bind_desc binds[ui_sdf_bind_scene_binding_count] = {
        { .binding = ui_sdf_bind_scene_params, .buffer = slot->uniforms_buffer, .buffer_offset = params_offset, .buffer_size = sizeof(struct ui_sdf_scene_params) },
        { .binding = ui_sdf_bind_scene_in_mid, .texture = frame->analytic ? sdf->dummy_content : frame->mid },
        { .binding = ui_sdf_bind_scene_in_sdf, .texture = frame->analytic ? sdf->dummy_content : frame->fine },
        { .binding = ui_sdf_bind_scene_content, .texture = desc->content ? desc->content : sdf->dummy_content },
        { .binding = ui_sdf_bind_scene_sampler, .sampler = sdf->sampler_mip },
        { .binding = ui_sdf_bind_scene_analytic, .buffer = slot->uniforms_buffer, .buffer_offset = analytic_offset, .buffer_size = sizeof(struct ui_sdf_scene_analytic_prims) },
    };
    struct gpu_bind_group* group = gpu_bind_group_create(sdf->device, sdf->layout_scene, binds, cr_count(binds));

    gpu_render_pass_set_scissor(render_pass, desc->out_scissor_origin.x, desc->out_scissor_origin.y, desc->out_scissor_size.x, desc->out_scissor_size.y);

    struct gpu_render_pipeline* const* pipelines_scene = target_format == gpu_format_bgra8
        ? sdf->pipelines_scene_c8
        : sdf->pipelines_scene;

    if (desc->draw_color) {
        enum ui_sdf_scene_pipeline pipeline = ui_sdf_scene_pipeline_solid;
        if (desc->content) {
            pipeline = ui_sdf_scene_pipeline_content;
        }
        gpu_render_pass_set_pipeline(render_pass, pipelines_scene[pipeline]);
        gpu_render_pass_set_bind_group(render_pass, 0, group, NULL, 0);
        gpu_render_pass_draw(render_pass, 0, 6);
    }

    if (desc->draw_glass) {
        assert(desc->content);
        gpu_render_pass_set_pipeline(render_pass, pipelines_scene[ui_sdf_scene_pipeline_glass]);
        gpu_render_pass_set_bind_group(render_pass, 0, group, NULL, 0);
        gpu_render_pass_draw(render_pass, 0, 6);
    }

    gpu_bind_group_destroy(sdf->device, group);
}

void ui_sdf_encode_scene(struct ui_sdf* sdf, struct ui* ui, struct ui_slot* slot, struct gpu_command_buffer* command_buffer,
                            const struct ui_sdf_scene_desc* desc) {
    struct ui_sdf_scene_desc local_desc = *desc;
    struct ui_sdf_frame frame;
    ui_sdf_scene_prepare(sdf, ui, &local_desc, &frame);

    if (frame.analytic == false) {
        struct gpu_compute_pass* pass = gpu_command_buffer_begin_compute(command_buffer, "ui sdf top mid");
        ui_sdf_encode_top(sdf, pass, &frame);
        ui_sdf_encode_mid(sdf, pass, &frame);
        gpu_compute_pass_end(pass);

        pass = gpu_command_buffer_begin_compute(command_buffer, "ui sdf refine");
        ui_sdf_encode_refine(sdf, pass, &frame);
        gpu_compute_pass_end(pass);
    }

    {
        struct gpu_render_pass* render_pass = gpu_command_buffer_begin_render(command_buffer, local_desc.out_texture, gpu_load_keep, "ui sdf scene");
        ui_sdf_scene_composite(sdf, ui, slot, render_pass, gpu_texture_get_format(local_desc.out_texture), &frame, &local_desc);
        gpu_render_pass_end(render_pass);
    }

    ui_sdf_frame_destroy(sdf, ui, slot, &frame);
}
