#include "ui_render_impl.h"
#include "ui_corner.h"

#include "ui_glass_constants.h"
#include "ui_atlas.h"
#include "ui_blur.h"
#include "ui_color.h"
#include <stddef.h>

_Thread_local struct ui* ui_custom_on_render_ctx;

_Static_assert(sizeof(struct ui_custom_vertex_solid) == sizeof(struct ui_vertex_solid), "");
_Static_assert(offsetof(struct ui_custom_vertex_solid, pos_px) == offsetof(struct ui_vertex_solid, pos_px), "");
_Static_assert(sizeof(struct ui_custom_vertex_graphic) == sizeof(struct ui_vertex_graphic), "");
_Static_assert(offsetof(struct ui_custom_vertex_graphic, pos_px) == offsetof(struct ui_vertex_graphic, pos_px), "");
_Static_assert(offsetof(struct ui_custom_vertex_graphic, uv) == offsetof(struct ui_vertex_graphic, uv), "");

void ui_apply_scissor(struct ui* ui) {
    struct ui_render* mod = ui->render;

    if (mod->ctx.pass_pending) {
        return;
    }

    struct cr_rect_int32 rect = cr_rect_int32_intersect(mod->ctx.active_scissor, mod->ctx.rect);
    if (rect.size.x <= 0 || rect.size.y <= 0) {
        rect.origin = mod->ctx.rect.origin;
        rect.size = make_int2(0, 0);
    }
    rect.origin -= mod->ctx.rect.origin;

    gpu_render_pass_set_scissor(mod->ctx.slot->pass, (uint32_t)rect.origin.x, (uint32_t)rect.origin.y, (uint32_t)rect.size.x, (uint32_t)rect.size.y);
}

void ui_flush(struct ui* ui) {
    struct ui_render* mod = ui->render;
    struct ui_slot* slot = mod->ctx.slot;

    if (mod->ctx.vertex_count == 0) {
        return;
    }

    ui_require_pass(ui);

    enum ui_pipeline pipeline;
    struct gpu_bind_group* texture_group = NULL;
    uint32_t vertex_size = sizeof(struct ui_quad_glyph);
    uint32_t vertex_start = 0;

    switch (mod->ctx.last_type) {
    case ui_draw_type_none: assert(0);
    case ui_draw_type_solid: {
        struct ui_uniforms_solid* uniforms = ui_alloc_uniforms(ui, sizeof(*uniforms), &mod->ctx.uniforms_offset);
        float3x2 solid_inverse = inverse_float3x2(mod->ctx.solid.transform);
        memset(uniforms, 0, sizeof(*uniforms));
        uniforms->transform_x = mod->ctx.solid.transform.columns[0];
        uniforms->transform_y = mod->ctx.solid.transform.columns[1];
        uniforms->transform_z = mod->ctx.solid.transform.columns[2];
        uniforms->inverse_x = solid_inverse.columns[0];
        uniforms->inverse_y = solid_inverse.columns[1];
        uniforms->inverse_z = solid_inverse.columns[2];
        uniforms->flags = ui_pass_dither_flag(ui);
        uniforms->color = mod->ctx.solid.color;

        pipeline = mod->ctx.solid.sdf ? ui_pipeline_solid_sdf : ui_pipeline_solid;
        vertex_size = sizeof(struct ui_vertex_solid);
    } break;
    case ui_draw_type_graphic: {
        struct ui_uniforms_graphic* uniforms = ui_alloc_uniforms(ui, sizeof(*uniforms), &mod->ctx.uniforms_offset);
        float3x2 graphic_inverse = inverse_float3x2(mod->ctx.graphic.transform);
        memset(uniforms, 0, sizeof(*uniforms));
        uniforms->transform_x = mod->ctx.graphic.transform.columns[0];
        uniforms->transform_y = mod->ctx.graphic.transform.columns[1];
        uniforms->transform_z = mod->ctx.graphic.transform.columns[2];
        uniforms->inverse_x = graphic_inverse.columns[0];
        uniforms->inverse_y = graphic_inverse.columns[1];
        uniforms->inverse_z = graphic_inverse.columns[2];
        uniforms->tint = mod->ctx.graphic.tint;
        uniforms->uv_origin = mod->ctx.graphic.uv.origin;
        uniforms->uv_size = mod->ctx.graphic.uv.size;
        uniforms->uv_min = mod->ctx.graphic.uv.min;
        uniforms->uv_max = mod->ctx.graphic.uv.max;
        uniforms->flags = mod->ctx.graphic.flags | ui_pass_dither_flag(ui);

        struct ui_convert convert = ui_convert_make(mod->ctx.graphic.color_space, ui_color_space_srgb);
        if (convert.needed) {
            ui_convert_write(&uniforms->flags, &uniforms->cs_col_x, &uniforms->cs_col_y, &uniforms->cs_col_z, &convert);
        }

        pipeline = mod->ctx.graphic.sdf ? ui_pipeline_graphic_sdf : ui_pipeline_graphic;
        texture_group = mod->ctx.graphic.bind_group;
        vertex_size = sizeof(struct ui_vertex_graphic);
    } break;
    case ui_draw_type_glyph: {
        struct ui_uniforms_glyph* uniforms = ui_alloc_uniforms(ui, sizeof(*uniforms), &mod->ctx.uniforms_offset);
        memset(uniforms, 0, sizeof(*uniforms));
        uniforms->start_index = mod->ctx.start_vertices_offset / sizeof(struct ui_quad_glyph);
        uniforms->flags = ui_pass_dither_flag(ui);
        uniforms->tint_mode = mod->ctx.glyph.paint.tint_mode;
        uniforms->tint_color0 = mod->ctx.glyph.paint.tint_color0;
        uniforms->tint_color1 = mod->ctx.glyph.paint.tint_color1;
        uniforms->grad_dir = mod->ctx.glyph.paint.grad_dir;
        uniforms->grad_off = mod->ctx.glyph.paint.grad_off;
        uniforms->transform_x = mod->ctx.glyph.paint.transform.columns[0];
        uniforms->transform_y = mod->ctx.glyph.paint.transform.columns[1];
        uniforms->transform_z = mod->ctx.glyph.paint.transform.columns[2];

        pipeline = ui_pipeline_glyph;
        texture_group = mod->ctx.glyph.bind_group;
    } break;
    }

    assert(mod->ctx.start_vertices_offset % vertex_size == 0);
    if (mod->ctx.last_type != ui_draw_type_glyph) {
        vertex_start = mod->ctx.start_vertices_offset / vertex_size;
    }

    gpu_render_pass_set_pipeline(slot->pass, ui_pass_pipeline(ui, pipeline));
    if (texture_group != NULL) {
        gpu_render_pass_set_bind_group(slot->pass, 1, texture_group, NULL, 0);
    }
    gpu_render_pass_set_bind_group(slot->pass, 0, slot->bind_group, (uint32_t[]){ mod->ctx.globals_offset, mod->ctx.uniforms_offset }, 2);
    gpu_render_pass_draw(slot->pass, vertex_start, mod->ctx.vertex_count);

    mod->ctx.last_type = ui_draw_type_none;
    mod->ctx.vertex_count = 0;
}

static bool ui_begin_solid(struct ui* ui, const struct ui_batch_solid* batch) {
    struct ui_render* mod = ui->render;

    bool is_compatible = mod->ctx.last_type == ui_draw_type_solid &&
                         is_equal_float3x2(mod->ctx.solid.transform, batch->transform) &&
                         is_equal_float4(mod->ctx.solid.color, batch->color) &&
                         mod->ctx.solid.sdf == batch->sdf;

    if (is_compatible == false) {
        ui_flush(ui);
        mod->ctx.last_type = ui_draw_type_solid;
        mod->ctx.solid = *batch;
    }

    return is_compatible;
}

static bool ui_begin_graphic(struct ui* ui, const struct ui_batch_graphic* batch) {
    struct ui_render* mod = ui->render;

    bool is_compatible = mod->ctx.last_type == ui_draw_type_graphic &&
                         is_equal_float3x2(mod->ctx.graphic.transform, batch->transform) &&
                         mod->ctx.graphic.bind_group == batch->bind_group &&
                         mod->ctx.graphic.flags == batch->flags &&
                         mod->ctx.graphic.color_space == batch->color_space &&
                         is_equal_float4(mod->ctx.graphic.tint, batch->tint) &&
                         is_equal_float2(mod->ctx.graphic.uv.origin, batch->uv.origin) &&
                         is_equal_float2(mod->ctx.graphic.uv.size, batch->uv.size) &&
                         mod->ctx.graphic.sdf == batch->sdf;

    if (is_compatible == false) {
        ui_flush(ui);
        mod->ctx.last_type = ui_draw_type_graphic;
        mod->ctx.graphic = *batch;
    }

    return is_compatible;
}

static void ui_commit_vertices(struct ui* ui, bool is_compatible, uint32_t vertex_count) {
    struct ui_render* mod = ui->render;

    mod->ctx.vertex_count += vertex_count;
    if (is_compatible == false) {
        mod->ctx.start_vertices_offset = mod->ctx.vertices_offset;
    }
}

struct ui_sdf_quad {
    float2 s;
    float2 e;
    float ref;
};

static float ui_sdf_mask_ref(struct ui* ui, const struct ui_sdf_analytic_prim* sdf, uint32_t sdf_count) {
    uint32_t size = sdf_count * (uint32_t)sizeof(struct ui_sdf_mask_prim);
    uint32_t offset;
    struct ui_sdf_mask_prim* rec = ui_alloc_uniforms(ui, size, &offset);
    if (offset == 0) {
        rec = ui_alloc_uniforms(ui, size, &offset);
    }

    for (uint32_t i = 0; i < sdf_count; i++) {
        rec[i].meta = make_float4((float)sdf_count, 0.0f, 0.0f, 0.0f);
        rec[i].pos = sdf[i].pos;
        rec[i].cell_x = sdf[i].cell_x;
        rec[i].cell_y = sdf[i].cell_y;
        memcpy(rec[i].coef, sdf[i].coef, sizeof(rec[i].coef));
    }

    return (float)(offset / (uint32_t)sizeof(struct ui_sdf_mask_prim));
}

static uint32_t ui_sdf_mask_quads(struct ui* ui, const struct ui_sdf_analytic_prim* sdf, uint32_t sdf_count, float2 start, float2 end, struct ui_sdf_quad out[9]) {
    if (sdf_count == 0) {
        out[0] = (struct ui_sdf_quad) { .s = start, .e = end };
        return 1;
    }

    float ref = ui_sdf_mask_ref(ui, sdf, sdf_count);

    const struct ui_sdf_analytic_prim* hole = &sdf[sdf_count - 1];
    float4 cx4 = hole->cell_x;
    float4 cy4 = hole->cell_y;
    float cx = max_float(max_float(cx4.x, cx4.y), max_float(cx4.z, cx4.w));
    float cy = max_float(max_float(cy4.x, cy4.y), max_float(cy4.z, cy4.w));

    float2 hole_min = max_float2(make_float2(hole->pos.x - hole->pos.z + cx + 1.0f, hole->pos.y - hole->pos.w + cy + 1.0f), start);
    float2 hole_max = min_float2(make_float2(hole->pos.x + hole->pos.z - cx - 1.0f, hole->pos.y + hole->pos.w - cy - 1.0f), end);

    if (hole_min.x >= hole_max.x || hole_min.y >= hole_max.y) {
        out[0] = (struct ui_sdf_quad) {
            .s = start,
            .e = end,
            .ref = ref,
        };
        return 1;
    }

    float xs[4] = { start.x, hole_min.x, hole_max.x, end.x };
    float ys[4] = { start.y, hole_min.y, hole_max.y, end.y };
    uint32_t count = 0;

    for (uint32_t row = 0; row < 3; row++) {
        for (uint32_t col = 0; col < 3; col++) {
            bool center = col == 1 && row == 1;
            if (center && sdf_count == 2) {
                continue;
            }
            out[count++] = (struct ui_sdf_quad) {
                .s = make_float2(xs[col], ys[row]),
                .e = make_float2(xs[col + 1], ys[row + 1]),
                .ref = center ? 0.0f : ref,
            };
        }
    }

    return count;
}

static void ui_add_solid_sdf(struct ui* ui, float2 start, float2 end, float4 color, float3x2 transform, const struct ui_sdf_analytic_prim* sdf, uint32_t sdf_count) {
    struct ui_render* mod = ui->render;

    struct ui_batch_solid batch = {
        .transform = transform,
        .color = color,
        .sdf = sdf_count > 0,
    };
    bool is_compatible = ui_begin_solid(ui, &batch);

    struct ui_sdf_quad quads[9];
    uint32_t quad_count = ui_sdf_mask_quads(ui, sdf, sdf_count, start, end, quads);

    struct ui_vertex_solid* v = ui_alloc_vertices(ui, quad_count * 6 * (uint32_t)sizeof(*v), sizeof(*v), &mod->ctx.vertices_offset);
    for (uint32_t q = 0; q < quad_count; q++) {
        for (uint32_t i = 0; i < 6; i++) {
            *v++ = (struct ui_vertex_solid) { .pos_px = ui_quad_corner(quads[q].s, quads[q].e, i), .sdf_ref = quads[q].ref };
        }
    }

    ui_commit_vertices(ui, is_compatible, quad_count * 6);
}

static void ui_scene_shape_sdf(const struct ui_scene_shape* shape, float2 offset, double grow, struct ui_sdf_analytic_prim* out_sdf) {
    double2 origin = to_double2_from_float2(shape->origin) - grow;
    double2 size = max_double2(to_double2_from_float2(shape->size) + 2.0 * grow, make_double2(0.0, 0.0));
    double2 to_px_offset = to_double2_from_float2(offset);

    if (shape->type == ui_shape_type_ellipse) {
        double r = max_double(0.5 * (double)shape->size.x + grow, 0.0);
        ui_corner_analytic(origin, size, make_double4(r, r, r, r), false, to_px_offset, out_sdf);
        return;
    }

    float4 r = shape->radius;
    double4 radius = make_double4(max_double((double)r.x + grow, 0.0), max_double((double)r.y + grow, 0.0),
                                  max_double((double)r.w + grow, 0.0), max_double((double)r.z + grow, 0.0));
    ui_corner_analytic(origin, size, radius, shape->profile == ui_corner_profile_continuous, to_px_offset, out_sdf);
}

static float ui_scene_outline_center(const struct ui_render_scene* scene) {
    float half = scene->outline_thickness * 0.5f;
    return scene->outline_align == ui_outline_align_inside ? -half : scene->outline_align == ui_outline_align_outside ? half : 0.0f;
}

static uint32_t ui_scene_shape_sdf_band(const struct ui_scene_shape* shape, const struct ui_render_scene* scene, float2 offset,
                                        bool band, struct ui_sdf_analytic_prim* out_sdf, float2* out_start, float2* out_end) {
    double grow = 0.0;
    uint32_t count = 1;

    if (band) {
        double half = 0.5 * (double)scene->outline_thickness;
        double center = (double)ui_scene_outline_center(scene);

        ui_scene_shape_sdf(shape, offset, center + half, &out_sdf[0]);
        ui_scene_shape_sdf(shape, offset, center - half, &out_sdf[1]);
        grow = max_double(center + half, 0.0);
        count = 2;
    } else {
        ui_scene_shape_sdf(shape, offset, 0.0, &out_sdf[0]);
    }

    float pad = (float)grow + 1.0f;
    *out_start = shape->origin + offset - pad;
    *out_end = shape->origin + offset + shape->size + pad;
    return count;
}

static void ui_add_graphic_sdf(struct ui* ui, float2 start, float2 end, const struct ui_batch_graphic* batch, float2 uv_ref, float2 uv_scale, float3x2 uv_transform,
                               const struct ui_sdf_analytic_prim* sdf, uint32_t sdf_count) {
    struct ui_render* mod = ui->render;

    bool is_compatible = ui_begin_graphic(ui, batch);

    struct ui_sdf_quad quads[9];
    uint32_t quad_count = ui_sdf_mask_quads(ui, sdf, sdf_count, start, end, quads);

    struct ui_vertex_graphic* v = ui_alloc_vertices(ui, quad_count * 6 * (uint32_t)sizeof(*v), sizeof(*v), &mod->ctx.vertices_offset);
    for (uint32_t q = 0; q < quad_count; q++) {
        for (uint32_t i = 0; i < 6; i++) {
            float2 pos = ui_quad_corner(quads[q].s, quads[q].e, i);
            *v++ = (struct ui_vertex_graphic) { .pos_px = pos, .uv = (mul_float3x2_float2(uv_transform, pos) - uv_ref) * uv_scale, .sdf_ref = quads[q].ref };
        }
    }

    ui_commit_vertices(ui, is_compatible, quad_count * 6);
}

static void ui_add_textured_quad(struct ui* ui, int2 start, int2 end, const struct ui_batch_graphic* batch, float2 uv_end) {
    struct ui_render* mod = ui->render;

    float2 s = to_float2_from_int2(start - mod->ctx.rect.origin);
    float2 e = to_float2_from_int2(end - mod->ctx.rect.origin);

    bool is_compatible = ui_begin_graphic(ui, batch);

    struct ui_vertex_graphic* vertices = ui_alloc_vertices(ui, 6 * sizeof(vertices[0]), sizeof(vertices[0]), &mod->ctx.vertices_offset);
    for (uint32_t i = 0; i < 6; i++) {
        vertices[i] = (struct ui_vertex_graphic) { .pos_px = ui_quad_corner(s, e, i), .uv = ui_quad_corner(make_float2(0.0f, 0.0f), uv_end, i) };
    }

    ui_commit_vertices(ui, is_compatible, 6);
}

struct ui_scene_graphic {
    struct gpu_texture* texture;
    struct gpu_bind_group* bind_group;
    uint32_t flags;
    struct ui_tex_uv uv;
    float2 uv_scale;
    enum ui_color_space color_space;
};

static struct ui_scene_graphic ui_scene_graphic_resolve(struct ui* ui, const struct ui_layer* layer, float2 region_size) {
    struct ui_scene_graphic result = { 0 };

    double2 scale = layer->graphic.scale;
    float2 tile_scale = make_float2(scale.x == 0.0 ? 1.0f : (float)scale.x, scale.y == 0.0 ? 1.0f : (float)scale.y);
    bool nearest = layer->graphic.filter == ui_filter_nearest;
    enum ui_address_mode address_mode = layer->graphic.address_mode;

    struct ui_graphic* graphic = layer->graphic.graphic;
    ui_render_retain_graphic(ui, graphic);

    result.uv_scale = 1.0f / region_size;
    result.color_space = layer->graphic.color_space;

    if (graphic->format == gpu_format_r8) {
        result.flags |= ui_graphic_flag_r8_splat;
        result.color_space = ui_color_space_srgb;
    } else if (layer->graphic.premultiplied == false) {
        result.flags |= ui_graphic_flag_straight;
    }

    if (graphic->in_persistent) {
        result.texture = ui_persistent_texture(ui, graphic->slice);
        result.bind_group = ui_persistent_bind_group(ui, graphic->slice, nearest);

        float to_uv = 1.0f / (float)ui_persistent_page_size(ui);
        float2 content_origin = make_float2((float)(graphic->slice.x + ui_persistent_border_px), (float)(graphic->slice.y + ui_persistent_border_px));
        float2 content_size = to_float2_from_uint2(graphic->size);

        result.uv.origin = content_origin * to_uv;
        result.uv.size = content_size * to_uv;
        result.uv.min = result.uv.origin + 0.5f * to_uv;
        result.uv.max = result.uv.origin + result.uv.size - 0.5f * to_uv;

        if (address_mode == ui_address_mode_fill) {
            result.flags |= ui_graphic_flag_uv_region;
        } else {
            result.flags |= ui_graphic_flag_uv_wrap;
            result.uv_scale = 1.0f / (content_size * tile_scale);
        }
    } else {
        result.texture = graphic->texture;
        result.bind_group = ui_graphic_bind_group(ui, graphic, address_mode != ui_address_mode_repeat, nearest);

        if (address_mode == ui_address_mode_repeat) {
            result.uv_scale = 1.0f / (to_float2_from_uint2(graphic->size) * tile_scale);
        }
    }

    return result;
}

static struct ui_batch_graphic ui_scene_graphic_batch(const struct ui_scene_graphic* graphic, float4 tint, float3x2 transform, bool sdf) {
    return (struct ui_batch_graphic) {
        .transform = transform,
        .bind_group = graphic->bind_group,
        .tint = tint,
        .flags = graphic->flags,
        .uv = graphic->uv,
        .color_space = graphic->color_space,
        .sdf = sdf,
    };
}

static struct ui_tex_uv ui_transient_tex_uv(struct ui* ui, struct ui_transient_tex tex, int2 represented_size, uint32_t tex_level) {
    float2 page = to_float2_from_uint2(ui_transient_tex_size(ui, tex));
    float2 origin = to_float2_from_uint2(ui_transient_tex_origin(ui, tex));

    return (struct ui_tex_uv) {
        .origin = origin / page,
        .size = (to_float2_from_int2(represented_size) / (float)(1u << tex_level)) / page,
        .min = (origin + 0.5f) / page,
        .max = (origin + make_float2((float)tex.w, (float)tex.h) - 0.5f) / page,
    };
}

static struct ui_batch_graphic ui_transient_tex_batch(struct ui* ui, struct ui_transient_tex tex, int2 represented_size, uint32_t tex_level, float4 tint, float3x2 transform, bool sdf) {
    return (struct ui_batch_graphic) {
        .transform = transform,
        .bind_group = ui_transient_bind_group(ui, tex),
        .tint = tint,
        .flags = ui_graphic_flag_uv_region,
        .uv = ui_transient_tex_uv(ui, tex, represented_size, tex_level),
        .color_space = ui_color_space_srgb,
        .sdf = sdf,
    };
}

void ui_add_tex_quad(struct ui* ui, int2 start, int2 end, struct ui_transient_tex tex, uint32_t tex_level, float4 tint) {
    struct ui_batch_graphic batch = ui_transient_tex_batch(ui, tex, end - start, tex_level, tint, float3x2_identity, false);
    ui_add_textured_quad(ui, start, end, &batch, make_float2(1.0f, 1.0f));
}

void ui_draw_scene_cheap(struct ui* ui, const struct ui_render_scene* scene, const struct ui_layer* layer, bool outline) {
    struct ui_render* mod = ui->render;
    const struct ui_scene_shape* shapes = ui_arr_scene_shape_get_ptr(mod->scene_shapes, scene->shape_offset);

    float2 local_offset = make_float2((float)-mod->ctx.rect.origin.x, (float)-mod->ctx.rect.origin.y);
    float3x2 transform = ui_transform_pass_local(scene->transform, mod->ctx.rect.origin);

    float opacity = scene->opacity * ui_layer_opacity_value(layer);
    bool is_graphic = layer->type == ui_layer_type_graphic;
    assert(is_graphic || layer->type == ui_layer_type_solid);

    struct ui_scene_graphic graphic = { 0 };
    float4 color;
    float2 uv_ref = to_float2_from_int2(scene->node_rect.origin) + local_offset;

    if (is_graphic) {
        struct ui_color tint_color = layer->graphic.tint.value.w == 0.0f ? ui_white : layer->graphic.tint;
        color = ui_to_compositing_from_color(tint_color) * opacity;

        float2 node_size = max_float2(to_float2_from_int2(scene->node_rect.size), make_float2(1.0f, 1.0f));
        graphic = ui_scene_graphic_resolve(ui, layer, node_size);
    } else {
        color = ui_to_compositing_from_color(layer->solid.color) * opacity;
    }

    for (uint32_t i = 0; i < scene->shape_count; i++) {
        const struct ui_scene_shape* shape = &shapes[i];
        float4 shape_color = color * shape->coverage;

        if (outline == false && (is_graphic ? ui_scene_shape_covers_rect(shape, scene->node_rect) : ui_scene_shape_is_plain_rect(shape))) {
            if (is_graphic) {
                struct ui_batch_graphic batch = ui_scene_graphic_batch(&graphic, shape_color, transform, false);
                float2 uv_end = layer->graphic.address_mode == ui_address_mode_fill
                    ? make_float2(1.0f, 1.0f)
                    : to_float2_from_int2(scene->node_rect.size) * graphic.uv_scale;
                ui_add_textured_quad(ui, scene->node_rect.origin, scene->node_rect.origin + scene->node_rect.size, &batch, uv_end);
            } else {
                int2 start = to_int2_from_float2(shape->origin) - mod->ctx.rect.origin;
                int2 end = start + to_int2_from_float2(shape->size);
                ui_add_solid_sdf(ui, to_float2_from_int2(start), to_float2_from_int2(end), shape_color, transform, NULL, 0);
            }
            continue;
        }

        struct ui_sdf_analytic_prim sdf[2];
        float2 start;
        float2 end;
        uint32_t sdf_count = ui_scene_shape_sdf_band(shape, scene, local_offset, outline, sdf, &start, &end);

        if (is_graphic) {
            struct ui_batch_graphic batch = ui_scene_graphic_batch(&graphic, shape_color, transform, true);
            ui_add_graphic_sdf(ui, start, end, &batch, uv_ref, graphic.uv_scale, float3x2_identity, sdf, sdf_count);
        } else {
            ui_add_solid_sdf(ui, start, end, shape_color, transform, sdf, sdf_count);
        }
    }
}

void ui_draw_scene_tex_mask(struct ui* ui, const struct ui_render_scene* scene, float layer_opacity, bool band, struct ui_transient_tex tex, uint32_t tex_level, struct cr_rect_int32 tex_rect) {
    struct ui_render* mod = ui->render;
    const struct ui_scene_shape* shapes = ui_arr_scene_shape_get_ptr(mod->scene_shapes, scene->shape_offset);

    float2 local_offset = make_float2((float)-mod->ctx.rect.origin.x, (float)-mod->ctx.rect.origin.y);
    float2 uv_ref = to_float2_from_int2(tex_rect.origin) + local_offset;
    float2 uv_scale = 1.0f / to_float2_from_int2(tex_rect.size);
    float3x2 transform = ui_transform_pass_local(scene->transform, mod->ctx.rect.origin);

    for (uint32_t i = 0; i < scene->shape_count; i++) {
        const struct ui_scene_shape* shape = &shapes[i];

        struct ui_sdf_analytic_prim sdf[2];
        float2 start;
        float2 end;
        uint32_t sdf_count = ui_scene_shape_sdf_band(shape, scene, local_offset, band, sdf, &start, &end);

        float tint = shape->coverage * layer_opacity;
        struct ui_batch_graphic batch = ui_transient_tex_batch(ui, tex, tex_rect.size, tex_level, make_float4(tint, tint, tint, tint), transform, true);
        ui_add_graphic_sdf(ui, start, end, &batch, uv_ref, uv_scale, transform, sdf, sdf_count);
    }
}

static void ui_scene_band(const struct ui_render_scene* scene, struct ui_sdf_scene_params* p) {
    float center = scene->outline_thickness > 0.0f ? ui_scene_outline_center(scene) : 0.0f;
    float half = scene->outline_thickness * 0.5f;

    float band_lo = min_float(-2.0f, center - half - 2.0f);
    float band_hi = max_float(2.0f, center + half + 2.0f);
    float reach_lo = band_lo;
    float reach_hi = band_hi;

    if (scene->has_glass) {
        float s = scene->glass_scale;
        float height_pt = ui_glass_refract_height_pt(scene->glass_variant, scene->glass_min_dim_lo_pt);
        float inner_pt = max_float(height_pt, ui_glass_glow_reach_pt);

        band_lo = min_float(band_lo, -(inner_pt * s + ui_glass_band_in_pad_px));
        reach_lo = min_float(band_lo, -(ui_glass_reach_in_pt * s + ui_glass_reach_in_pad_px));

        float outside_hi = max_float(4.0f, ui_glass_outside_reach_pt(scene->glass_variant, scene->glass_min_dim_pt) * s + ui_glass_band_out_pad_px);
        if (ui_glass_variant_is_regular(scene->glass_variant)) {
            band_hi = max_float(band_hi, min_float(ui_glass_glow_reach_pt * s + ui_glass_band_out_pad_px, outside_hi));
            reach_hi = max_float(band_hi, outside_hi);
        } else {
            band_hi = max_float(band_hi, outside_hi);
            reach_hi = band_hi;
        }
    }

    p->outline_center = center;
    p->band_lo = band_lo;
    p->band_hi = band_hi;
    p->reach_lo = reach_lo;
    p->reach_hi = reach_hi;
}

void ui_scene_gradient_dir_(enum ui_gradient_direction direction, float2 node_origin, float2 node_size, float2* out_dir, float* out_off) {
    float2 dir = make_float2(0.0f, 0.0f);
    float off = 0.0f;
    switch (direction) {
    case ui_gradient_direction_right:
        dir = make_float2(1.0f / node_size.x, 0.0f);
        off = -node_origin.x / node_size.x;
        break;
    case ui_gradient_direction_left:
        dir = make_float2(-1.0f / node_size.x, 0.0f);
        off = (node_origin.x + node_size.x) / node_size.x;
        break;
    case ui_gradient_direction_down:
        dir = make_float2(0.0f, 1.0f / node_size.y);
        off = -node_origin.y / node_size.y;
        break;
    case ui_gradient_direction_up:
        dir = make_float2(0.0f, -1.0f / node_size.y);
        off = (node_origin.y + node_size.y) / node_size.y;
        break;
    }
    *out_dir = dir;
    *out_off = off;
}

static void ui_scene_glass_uniforms(struct ui_sdf_scene_params* p) {
    uint32_t gv = p->glass_variant;
    float slider = p->glass_slider;
    float size_ramp = p->glass_size_t;
    float min_dim_pt = p->glass_min_dim_pt;
    float min_dim_lo_pt = p->glass_min_dim_lo_pt;
    float half_lo_pt = min_dim_lo_pt > 0.0f ? 0.5f * min_dim_lo_pt : 1e6f;

    float4 o = ui_glass_blur_knot_o(gv, size_ramp);
    p->glass_rh_o = make_float4(o.x, o.x - o.y, o.y - o.z, o.z - o.w);
    p->glass_rh_span = make_float2(half_lo_pt, half_lo_pt + ui_glass_blur_knot1_pt);
    p->glass_rh_radius = ui_glass_blur_radius_pt(gv, slider);

    float spread = ui_glass_rim_spread_rad(min_dim_pt);
    p->glass_rim_cos = make_float2(cosf(spread), cosf(spread * ui_glass_rim_diffuse_spread_scale));
    p->glass_rim_k = make_float2(ui_glass_rim_compression(ui_glass_rim_amount),
                                 ui_glass_rim_compression(ui_glass_rim_amount * ui_glass_rim_diffuse_amount_scale));

    float refract_amount = ui_glass_variant_is_regular(gv) ? min_float(half_lo_pt, 60.0f) : min_float(1.3f * half_lo_pt, 60.0f);
    p->glass_refract = make_float2(refract_amount, ui_glass_refract_height_pt(gv, min_dim_lo_pt));
    p->glass_outer_ah = make_float2(ui_glass_outer_refraction_amount_pt(gv, min_dim_pt),
                                    ui_glass_outer_refraction_height_pt(gv, min_dim_pt));
    p->glass_outer_opacity = ui_glass_refraction_opacity(gv);

    float dk = ui_glass_fill_dk(gv, slider);
    float lk = ui_glass_fill_lk(gv, slider);
    float wn = ui_glass_fill_wn(gv, slider);
    p->glass_fill_dklk = make_float2(dk, lk);
    p->glass_fill_wn = wn;
    p->glass_fill_active = ui_glass_fill_radius_pt(gv, slider) > 0.0f && dk + lk + wn > 0.0f ? 1u : 0u;

    p->glass_shadow_alpha = ui_glass_shadow_fill_a * ui_glass_shadow_opacity(gv, size_ramp);
    p->glass_shadow_radius_pt = ui_glass_shadow_radius_pt(size_ramp);
    p->glass_bleed_radius_pt = ui_glass_bleed_radius_pt(gv, min_dim_pt);

    p->glass_face = make_float4(1.0f - ui_glass_face_max_luma(gv, slider, size_ramp),
                                ui_glass_face_fill_a(gv, slider),
                                ui_glass_face_fill_gray(gv, slider),
                                ui_glass_pick(ui_glass_cap, gv));
    p->glass_face_op = (gv == 2u ? 0.8f : 0.5f) * size_ramp;

    float kfh_cos = ui_glass_pick(ui_glass_kfh_cos_spread, gv);
    p->glass_kfh = make_float2(kfh_cos, max_float(1.0f - kfh_cos, ui_glass_cos_denom_min));

    if (ui_glass_variant_is_regular(gv)) {
        p->glass_vibrant_x = make_float4(1.1202f, -0.1894f, -0.0190f, 0.1471f);
        p->glass_vibrant_y = make_float4(-0.0563f, 0.9871f, -0.0191f, 0.0f);
        p->glass_vibrant_z = make_float4(-0.0563f, -0.1893f, 1.1574f, 0.0f);
    } else {
        p->glass_vibrant_x = make_float4(1.3192f, -0.0478f, -0.0047f, 0.2f);
        p->glass_vibrant_y = make_float4(-0.0142f, 1.2856f, -0.0048f, 0.0f);
        p->glass_vibrant_z = make_float4(-0.0142f, -0.0476f, 1.3285f, 0.0f);
    }

    p->glass_tint_hoist = 0;
    if (p->tint_mode == ui_sdf_tint_solid && p->tint_color0.w > 0.0f) {
        struct ui_glass_tint_coef coef = ui_glass_tint_coeffs(p->glass_variant, p->tint_color0);
        p->glass_tint_slope = coef.slope;
        p->glass_tint_bias = coef.bias;
        p->glass_tint_hoist = coef.mode;
    }
}

bool ui_scene_sdf_make_desc(struct ui* ui, const struct ui_render_scene* scene, const struct ui_scene_draw* draw,
                            bool has_content, struct cr_rect_int32 content_rect, struct cr_rect_int32 rect, struct cr_rect_int32 active_scissor,
                            struct ui_sdf_scene_desc* out_desc) {
    struct ui_render* mod = ui->render;

    struct cr_rect_int32 scissor = cr_rect_int32_intersect(cr_rect_int32_intersect(active_scissor, rect), scene->bounds);
    if (scissor.size.x <= 0 || scissor.size.y <= 0) {
        return false;
    }

    int2 page_offset = make_int2(0, 0) - rect.origin;

    struct ui_sdf_scene_desc desc = {
        .shapes = ui_arr_scene_shape_get_ptr(mod->scene_shapes, scene->shape_offset),
        .shape_count = scene->shape_count,
        .combine = scene->op == ui_shape_group_op_subtract ? ui_sdf_combine_subtract : ui_sdf_combine_union,
        .smoothing = scene->smoothing,
        .to_px_offset = make_double2((double)-scene->bounds.origin.x, (double)-scene->bounds.origin.y),
        .out_scissor_origin = to_uint2_from_int2(scissor.origin + page_offset),
        .out_scissor_size = to_uint2_from_int2(scissor.size),
    };

    struct ui_sdf_scene_params* p = &desc.params;
    *p = (struct ui_sdf_scene_params) {
        .out_origin = to_uint2_from_int2(scene->bounds.origin + page_offset),
        .fine_size = to_uint2_from_int2(scene->bounds.size),
        .glass_scale = scene->glass_scale,
        .glass_variant = scene->glass_variant,
        .glass_size_t = ui_glass_size_t(scene->glass_min_dim_pt),
        .glass_min_dim_pt = scene->glass_min_dim_pt,
        .glass_min_dim_lo_pt = scene->glass_min_dim_lo_pt,
        .glass_slider = scene->glass_slider,
        .opacity = scene->opacity,
        .dither = mod->ctx.dither && mod->ctx.compositing_format == gpu_format_bgra8 ? 1u : 0u,
    };
    ui_scene_band(scene, p);

    const struct ui_layer* paint = draw->fill_layer;
    bool band_region = draw->band;

    if (draw->outline_layer != NULL) {
        if (draw->outline_layer->type == ui_layer_type_solid) {
            p->outline_half = scene->outline_thickness * 0.5f;
            p->outline_color = ui_to_compositing_from_color(draw->outline_layer->solid.color) * ui_layer_opacity_value(draw->outline_layer);
        } else {
            assert(paint == NULL);
            paint = draw->outline_layer;
            band_region = true;
        }
    }

    if (band_region) {
        p->outline_half = scene->outline_thickness * 0.5f;
        p->region_band = 1u;
    }

    float2 node_origin = to_float2_from_int2(scene->node_rect.origin + page_offset);
    float2 node_size = max_float2(to_float2_from_int2(scene->node_rect.size), make_float2(1.0f, 1.0f));

    if (has_content) {
        p->opacity = draw->bg_opacity;
        p->content_flags = ui_graphic_flag_uv_region;
        p->content_ref = to_float2_from_int2(content_rect.origin + page_offset);
        p->content_scale = 1.0f / to_float2_from_int2(content_rect.size);
        desc.draw_glass = draw->glass;

        if (draw->glass) {
            if (scene->glass_tint.value.w > 0.0f) {
                p->tint_mode = ui_sdf_tint_solid;
                p->tint_color0 = ui_to_compositing_from_color(scene->glass_tint);
            }
            ui_scene_glass_uniforms(p);
        } else {
            p->has_fill = 1;
            p->fill_color = make_float4(1.0f, 1.0f, 1.0f, 1.0f);
        }
    } else if (paint != NULL) {
        float layer_opacity = ui_layer_opacity_value(paint);
        p->has_fill = 1;

        if (paint->type == ui_layer_type_solid) {
            p->fill_color = ui_to_compositing_from_color(paint->solid.color) * layer_opacity;
        } else if (paint->type == ui_layer_type_gradient) {
            p->tint_mode = ui_sdf_tint_gradient;
            p->tint_color0 = ui_to_compositing_from_color(paint->gradient.color_start) * layer_opacity;
            p->tint_color1 = ui_to_compositing_from_color(paint->gradient.color_end) * layer_opacity;
            ui_scene_gradient_dir_(paint->gradient.direction, node_origin, node_size, &p->tint_grad_dir, &p->tint_grad_off);
        } else {
            assert(paint->type == ui_layer_type_graphic);
            struct ui_scene_graphic graphic = ui_scene_graphic_resolve(ui, paint, node_size);
            struct ui_color tint = paint->graphic.tint.value.w == 0.0f ? ui_white : paint->graphic.tint;
            struct ui_convert convert = ui_convert_make(graphic.color_space, ui_color_space_srgb);

            desc.content = graphic.texture;
            p->fill_color = ui_to_compositing_from_color(tint) * layer_opacity;
            p->content_flags = graphic.flags;
            p->content_uv_origin = graphic.uv.origin;
            p->content_uv_size = graphic.uv.size;
            p->content_uv_min = graphic.uv.min;
            p->content_uv_max = graphic.uv.max;
            p->content_ref = node_origin;
            p->content_scale = graphic.uv_scale;

            if (convert.needed) {
                ui_convert_write(&p->content_flags, &p->cs_col_x, &p->cs_col_y, &p->cs_col_z, &convert);
            }
        }
    }

    desc.draw_color = p->has_fill != 0 || p->outline_half > 0.0f;

    if (desc.draw_color == false && desc.draw_glass == false) {
        return false;
    }

    *out_desc = desc;
    return true;
}

void ui_scene_sdf_set_content(struct ui* ui, struct ui_sdf_scene_desc* desc, struct ui_transient_tex content_tex, struct cr_rect_int32 content_rect, uint32_t tex_level) {
    struct ui_tex_uv uv = ui_transient_tex_uv(ui, content_tex, max_int2(content_rect.size, make_int2(1, 1)), tex_level);

    desc->content = ui_transient_texture(ui, content_tex);
    desc->params.content_uv_origin = uv.origin;
    desc->params.content_uv_size = uv.size;
    desc->params.content_uv_min = uv.min;
    desc->params.content_uv_max = uv.max;
}

void ui_scene_sdf_set_glass(struct ui* ui, struct ui_sdf_scene_desc* desc, const struct ui_glass_pyramid* glass) {
    if (glass == NULL || glass->valid == false) {
        return;
    }

    float s = max_float(desc->params.glass_scale, 1e-3f);
    float veil_rt = ui_glass_blur_veil_rt_pt(ui_glass_fill_radius_pt(desc->params.glass_variant, desc->params.glass_slider));
    float bleed_rt = ui_glass_blur_bleed_rt_pt(ui_glass_bleed_radius_pt(desc->params.glass_variant, desc->params.glass_min_dim_pt));

    desc->params.glass_pyr_count = glass->level_count;
    desc->params.glass_pyr_veil = min_float(veil_rt > 0.0f ? ui_glass_blur_lod(veil_rt, s) : 0.0f, glass->level_max);
    desc->params.glass_pyr_bleed = min_float(bleed_rt > 0.0f ? ui_glass_blur_lod(bleed_rt, s) : 0.0f, glass->level_max);
}

static bool ui_text_paint_batch_equal(const struct ui_text_paint* a, const struct ui_text_paint* b) {
    return a->tint_mode == b->tint_mode &&
           is_equal_float4(a->tint_color0, b->tint_color0) &&
           is_equal_float4(a->tint_color1, b->tint_color1) &&
           is_equal_float2(a->grad_dir, b->grad_dir) &&
           a->grad_off == b->grad_off &&
           is_equal_float3x2(a->transform, b->transform);
}

void ui_set_text_paint(struct ui* ui, const struct ui_text_paint* paint) {
    struct ui_render* mod = ui->render;

    if (mod->ctx.last_type == ui_draw_type_glyph && ui_text_paint_batch_equal(&mod->ctx.glyph.paint, paint) == false) {
        ui_flush(ui);
    }

    mod->ctx.text_paint = *paint;
}

void ui_render_add_glyph(struct ui* ui, const struct ui_glyph* glyph, int2 pen_px, float4 color) {
    struct ui_render* mod = ui->render;

    bool is_compatible = mod->ctx.last_type == ui_draw_type_glyph && mod->ctx.glyph.bind_group == glyph->bind_group;

    if (is_compatible == false) {
        ui_flush(ui);
        mod->ctx.last_type = ui_draw_type_glyph;
        mod->ctx.glyph.bind_group = glyph->bind_group;
        mod->ctx.glyph.paint = mod->ctx.text_paint;
    }

    struct ui_quad_glyph* quad = ui_alloc_vertices(ui, sizeof(*quad), sizeof(*quad), &mod->ctx.vertices_offset);
    memset(quad, 0, sizeof(*quad));
    quad->origin_px = to_float2_from_int2(pen_px + glyph->offset);
    memcpy(&quad->size_px, &glyph->size, sizeof(quad->size_px));
    memcpy(&quad->origin_uv_px, &glyph->origin_uv, sizeof(quad->origin_uv_px));
    memcpy(&quad->total_uv_px, &glyph->total_uv, sizeof(quad->total_uv_px));
    quad->color = mod->ctx.text_paint.override_color ? mod->ctx.text_paint.color : color;

    ui_commit_vertices(ui, is_compatible, 6);
}

static float3x2 ui_custom_transform(struct ui* ui, float3x2 transform) {
    transform = mul_float3x2(ui->render->ctx.custom_transform, transform);
    transform.columns[2] -= to_float2_from_int2(ui->render->ctx.rect.origin);
    return transform;
}

static void* ui_custom_alloc_vertices(struct ui* ui, bool is_compatible, uint32_t vertex_count, uint32_t vertex_size) {
    struct ui_render* mod = ui->render;

    uint8_t* vertices = ui_alloc_vertices(ui, vertex_count * vertex_size, vertex_size, &mod->ctx.vertices_offset);
    memset(vertices, 0, vertex_count * vertex_size);
    ui_commit_vertices(ui, is_compatible, vertex_count);
    return vertices;
}

void ui_custom_draw_solid(float3x2 transform, struct ui_color color, uint32_t vertex_count, struct ui_custom_vertex_solid** out_vertices) {
    struct ui* ui = ui_custom_on_render_ctx;

    struct ui_batch_solid batch = {
        .transform = ui_custom_transform(ui, transform),
        .color = ui_to_compositing_from_color(color),
    };
    bool is_compatible = ui_begin_solid(ui, &batch);
    *out_vertices = ui_custom_alloc_vertices(ui, is_compatible, vertex_count, sizeof(struct ui_custom_vertex_solid));
}

void ui_custom_draw_graphic(float3x2 transform, struct ui_graphic* graphic, struct ui_color tint, uint32_t vertex_count, struct ui_custom_vertex_graphic** out_vertices) {
    struct ui* ui = ui_custom_on_render_ctx;

    struct ui_color tint_color = tint.value.w == 0.0f ? ui_white : tint;
    struct ui_layer layer = {
        .type = ui_layer_type_graphic,
        .graphic = {
            .graphic = graphic,
            .address_mode = ui_address_mode_repeat,
            .premultiplied = true,
        },
    };
    struct ui_scene_graphic resolved = ui_scene_graphic_resolve(ui, &layer, make_float2(1.0f, 1.0f));

    struct ui_batch_graphic batch = ui_scene_graphic_batch(&resolved, ui_to_compositing_from_color(tint_color), ui_custom_transform(ui, transform), false);

    bool is_compatible = ui_begin_graphic(ui, &batch);
    *out_vertices = ui_custom_alloc_vertices(ui, is_compatible, vertex_count, sizeof(struct ui_custom_vertex_graphic));
}
