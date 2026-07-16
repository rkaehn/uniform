#include "ui_render_impl.h"
#include "ui_text.h"

struct ui_build_ctx {
    struct ui_fx x;
    struct ui_fx y;
    float3x2 transform;
    struct cr_rect_int32 scissor;
    struct cr_rect_int32 sample;
    uint32_t container;
    double to_px_from_pt;
    bool in_shape_group;
    struct ui_build_layer* open_layers;
};

static void ui_cmd_push(struct ui* ui, struct ui_render_command cmd) {
    ui_arr_render_command_append(ui->render->commands, cmd);
}

static void ui_cmd_push_background_blur(struct ui* ui, struct ui_render_command cmd, struct ui_build_layer* open_layers) {
    ui_cmd_push(ui, cmd);

    for (struct ui_build_layer* layer = open_layers; layer != NULL; layer = layer->parent) {
        ui_arr_render_command_get_ptr(ui->render->commands, layer->cmd_index)->push_layer.backdrop_dep = true;
    }
}

static struct cr_rect_int32 ui_node_rect_px(struct ui_node_backing* node, struct ui_fx x, struct ui_fx y) {
    int2 raw_start = make_int2(ui_fx_get_px(x), ui_fx_get_px(y));
    int2 raw_end = make_int2(ui_fx_get_px(ui_fx_add(x, node->p2.width)), ui_fx_get_px(ui_fx_add(y, node->p2.height)));

    int2 start = min_int2(raw_start, raw_end);
    int2 end = max_int2(raw_start, raw_end);

    return (struct cr_rect_int32) {
        .origin = start,
        .size = end - start,
    };
}

static struct ui_scene_shape ui_scene_shape_rect(struct cr_rect_int32 rect, const struct ui_corners* corners, double to_px_from_pt) {
    return (struct ui_scene_shape) {
        .type = ui_shape_type_rect,
        .origin = to_float2_from_int2(rect.origin),
        .size = to_float2_from_int2(rect.size),
        .radius = to_float4_from_double4(corners->radius * to_px_from_pt),
        .profile = corners->profile,
        .coverage = 1.0f,
    };
}

static struct ui_scene_shape ui_scene_shape_from_node(struct ui_node_backing* node, struct cr_rect_int32 rect, double to_px_from_pt) {
    float2 origin = to_float2_from_int2(rect.origin);
    float2 size = to_float2_from_int2(rect.size);

    switch (node->shape.shape_type) {
    case ui_shape_type_rect: {
        return ui_scene_shape_rect(rect, &node->shape.corners, to_px_from_pt);
    }
    case ui_shape_type_ellipse: {
        return (struct ui_scene_shape) {
            .type = ui_shape_type_ellipse,
            .origin = origin,
            .size = size,
            .coverage = 1.0f,
        };
    }
    case ui_shape_type_line: {
        double thickness = node->shape.thickness * to_px_from_pt;
        double coverage = 1.0;

        if (thickness < 1.0) {
            coverage = thickness;
            thickness = 1.0;
        } else {
            thickness = round(thickness);
        }

        bool horizontal = size.x >= size.y;
        double across = horizontal ? origin.y : origin.x;
        double extent = horizontal ? size.y : size.x;
        float start = (float)round(across + extent * 0.5 - thickness * 0.5);
        return (struct ui_scene_shape) {
            .type = ui_shape_type_rect,
            .origin = horizontal ? make_float2(origin.x, start) : make_float2(start, origin.y),
            .size = horizontal ? make_float2(size.x, (float)thickness) : make_float2((float)thickness, size.y),
            .coverage = (float)coverage,
        };
    }
    case ui_shape_type_path: {
        struct ui_scene_shape shape = {
            .type = ui_shape_type_path,
            .coverage = 1.0f,
            .path = &node->shape.path,
            .path_offset = origin,
            .path_scale = (float)to_px_from_pt,
        };

        if (node->shape.path_mode == ui_shape_mode_stroke) {
            shape.path_half = (float)(0.5 * node->shape.thickness * to_px_from_pt);
        }

        double2 bounds_min;
        double2 bounds_max;
        if (ui_path_get_bounds(&node->shape.path, &bounds_min, &bounds_max)) {
            shape.origin = to_float2_from_double2(to_double2_from_float2(origin) + bounds_min * to_px_from_pt - (double)shape.path_half);
            shape.size = to_float2_from_double2((bounds_max - bounds_min) * to_px_from_pt + 2.0 * (double)shape.path_half);
        }

        return shape;
    }
    }
}

static struct cr_rect_int32 ui_rect_transform_aabb(struct cr_rect_int32 rect, float3x2 t) {
    float2 p0 = mul_float3x2_float2(t, to_float2_from_int2(rect.origin));
    float2 p1 = mul_float3x2_float2(t, to_float2_from_int2(rect.origin + make_int2(rect.size.x, 0)));
    float2 p2 = mul_float3x2_float2(t, to_float2_from_int2(rect.origin + make_int2(0, rect.size.y)));
    float2 p3 = mul_float3x2_float2(t, to_float2_from_int2(rect.origin + rect.size));
    float2 lo = min_float2(min_float2(p0, p1), min_float2(p2, p3));
    float2 hi = max_float2(max_float2(p0, p1), max_float2(p2, p3));

    struct cr_rect_int32 result;
    result.origin = make_int2((int32_t)floorf(lo.x), (int32_t)floorf(lo.y));
    result.size = make_int2((int32_t)ceilf(hi.x), (int32_t)ceilf(hi.y)) - result.origin;
    return result;
}

static bool ui_shape_node_is_empty(struct ui_node_backing* node, struct cr_rect_int32 rect) {
    if (node->shape.shape_type == ui_shape_type_path) {
        double2 bounds_min;
        double2 bounds_max;
        return ui_path_get_bounds(&node->shape.path, &bounds_min, &bounds_max) == false;
    }
    if (node->shape.shape_type == ui_shape_type_line) {
        return rect.size.x <= 0 && rect.size.y <= 0;
    }
    return rect.size.x <= 0 || rect.size.y <= 0;
}

static void ui_scene_collect(struct ui* ui, struct ui_node_backing* node, struct ui_fx x, struct ui_fx y, double to_px_from_pt, uint32_t* inout_count) {
    struct ui_render* mod = ui->render;

    for (struct ui_node_backing* child = ui_node_get_child(ui, node, 0); child != NULL; child = ui_node_get_next(ui, child)) {
        struct ui_fx child_x = ui_fx_add(x, child->p2.x);
        struct ui_fx child_y = ui_fx_add(y, child->p2.y);

        if (child->type == ui_content_type_shape) {
            cr_check(is_zero_float3x2(child->transform), "shapes in a shape group cannot carry their own transform");
            struct cr_rect_int32 rect = ui_node_rect_px(child, child_x, child_y);
            if (ui_shape_node_is_empty(child, rect) == false) {
                ui_arr_scene_shape_append(mod->scene_shapes, ui_scene_shape_from_node(child, rect, to_px_from_pt));
                (*inout_count)++;
            }
            continue;
        }

        if (child->type == ui_content_type_frame && child->frame.shape_group.op == ui_shape_group_op_none) {
            ui_scene_collect(ui, child, child_x, child_y, to_px_from_pt, inout_count);
        }
    }
}

static const struct ui_layer* ui_layers_find_glass(const struct ui_layer* layers, uint32_t count) {
    for (uint32_t i = 0; i < count; i++) {
        if (layers[i].type == ui_layer_type_glass) {
            return &layers[i];
        }
    }
    return NULL;
}

static uint32_t ui_scene_build(struct ui* ui, struct ui_node_backing* node, struct cr_rect_int32 node_rect, const struct ui_build_ctx* ctx, bool* out_has_draw) {
    struct ui_render* mod = ui->render;
    double to_px_from_pt = ctx->to_px_from_pt;

    struct ui_render_scene scene = {
        .shape_offset = ui_arr_scene_shape_get_count(mod->scene_shapes),
        .op = ui_shape_group_op_union,
        .opacity = 1.0f,
        .node_rect = node_rect,
        .transform = ctx->transform,
    };

    if (node->type == ui_content_type_frame && node->frame.shape_group.op != ui_shape_group_op_none) {
        uint32_t count = 0;
        ui_scene_collect(ui, node, ctx->x, ctx->y, to_px_from_pt, &count);

        cr_check(count <= 8, "shape groups support at most 8 shapes");

        if (count == 0) {
            *out_has_draw = false;
            return ui_scene_index_invalid;
        }

        scene.shape_count = count;
        scene.op = node->frame.shape_group.op;
        scene.smoothing = (float)(max_double(node->frame.shape_group.smoothing, 0.0) * to_px_from_pt);
    } else if (node->type == ui_content_type_shape) {
        if (ui_shape_node_is_empty(node, node_rect)) {
            *out_has_draw = false;
            return ui_scene_index_invalid;
        }
        ui_arr_scene_shape_append(mod->scene_shapes, ui_scene_shape_from_node(node, node_rect, to_px_from_pt));
        scene.shape_count = 1;
    } else {
        if (node_rect.size.x <= 0 || node_rect.size.y <= 0) {
            *out_has_draw = false;
            return ui_scene_index_invalid;
        }
        ui_arr_scene_shape_append(mod->scene_shapes, ui_scene_shape_rect(node_rect, &node->frame.corners, to_px_from_pt));
        scene.shape_count = 1;
    }

    scene.fill_layers = node->fill.layers;
    scene.fill_count = node->fill.layer_count;
    scene.outline_layers = node->outline.layers;
    scene.outline_count = node->outline.layer_count;

    scene.outline_thickness = node->outline.layer_count > 0 ? (float)(max_double(node->outline.thickness, 0.0) * to_px_from_pt) : 0.0f;
    scene.outline_align = node->outline.align;

    const struct ui_layer* glass = ui_layers_find_glass(node->fill.layers, node->fill.layer_count);
    if (glass != NULL) {
        scene.has_glass = true;
        scene.glass_scale = (float)to_px_from_pt;
        scene.glass_variant = ui_glass_variant_pack(glass);
        scene.glass_tint = glass->glass.tint;
        scene.glass_slider = ui_glass_slider_value(glass);
    }

    float2 bounds_min;
    float2 bounds_max;
    ui_scene_shapes_bounds(ui_arr_scene_shape_get_ptr(mod->scene_shapes, scene.shape_offset), scene.shape_count, &bounds_min, &bounds_max);

    if (scene.has_glass) {
        scene.glass_min_dim_pt = ui_scene_glass_min_dim_pt(bounds_min, bounds_max, scene.glass_scale);
        scene.glass_min_dim_lo_pt = scene.glass_min_dim_pt;
    }

    float outward = scene.outline_align == ui_outline_align_center ? scene.outline_thickness * 0.5f
                  : scene.outline_align == ui_outline_align_outside ? scene.outline_thickness : 0.0f;

    float margin = 2.0f + outward + scene.smoothing * 0.25f;
    if (scene.has_glass) {
        margin = max_float(margin, 5.0f + ui_glass_outside_reach_pt(scene.glass_variant, scene.glass_min_dim_pt) * (float)to_px_from_pt);
    }

    struct cr_rect_int32 bounds = {
        .origin = make_int2((int32_t)floorf(bounds_min.x - margin), (int32_t)floorf(bounds_min.y - margin)),
    };
    bounds.size = make_int2((int32_t)ceilf(bounds_max.x + margin), (int32_t)ceilf(bounds_max.y + margin)) - bounds.origin;

    if (is_equal_float3x2(ctx->transform, float3x2_identity) == false) {
        bounds = ui_rect_transform_aabb(bounds, ctx->transform);
    }

    scene.bounds = cr_rect_int32_intersect(bounds, ctx->scissor);

    bool has_color = ui_layers_any(node->fill.layers, node->fill.layer_count, ui_layer_is_color) ||
                     (scene.outline_thickness > 0.0f && ui_layers_any(node->outline.layers, node->outline.layer_count, ui_layer_is_color));

    *out_has_draw = has_color && scene.bounds.size.x > 0 && scene.bounds.size.y > 0;

    uint32_t index = ui_arr_render_scene_get_count(mod->scenes);
    ui_arr_render_scene_append(mod->scenes, scene);

    return index;
}

static bool ui_scene_masks(struct ui* ui, uint32_t scene_index) {
    struct ui_render* mod = ui->render;

    if (scene_index == ui_scene_index_invalid) {
        return false;
    }

    const struct ui_render_scene* scene = ui_arr_render_scene_get_ptr(mod->scenes, scene_index);

    if (scene->bounds.size.x <= 0 || scene->bounds.size.y <= 0) {
        return true;
    }

    if (scene->shape_count != 1) {
        return true;
    }

    const struct ui_scene_shape* shape = ui_arr_scene_shape_get_ptr(mod->scene_shapes, scene->shape_offset);

    return ui_scene_shape_covers_rect(shape, scene->node_rect) == false;
}

static struct cr_rect_int32 ui_glass_capture_rect(const struct ui_layer* layer, struct cr_rect_int32 node_rect, struct cr_rect_int32 scissor, struct cr_rect_int32 sample, double to_px_from_pt) {
    uint32_t glass_variant = ui_glass_variant_pack(layer);
    float glass_slider = ui_glass_slider_value(layer);
    float veil_radius_pt = max_float(ui_glass_fill_radius_pt(glass_variant, glass_slider),
                                     ui_glass_blur_radius_pt(glass_variant, glass_slider));
    float veil_reach = 3.0f * ui_glass_blur_margin_sigma_px(veil_radius_pt, (float)to_px_from_pt);
    float outset_min_dim_pt = (float)min_int32(node_rect.size.x, node_rect.size.y) / (float)to_px_from_pt;
    float outset = ui_glass_outset_pt(glass_variant, outset_min_dim_pt) * (float)to_px_from_pt;
    int32_t margin = 3 + (int32_t)ceilf(max_float(veil_reach, outset));
    struct cr_rect_int32 glass_rect = { .origin = node_rect.origin - margin, .size = node_rect.size + 2 * margin };
    struct cr_rect_int32 bounds = cr_rect_int32_intersect(cr_rect_int32_intersect(scissor, glass_rect), sample);
    int2 snap = make_int2(bounds.origin.x & 1, bounds.origin.y & 1);
    bounds.origin -= snap;
    bounds.size += snap;
    return bounds;
}

struct ui_emit_ctx {
    const struct ui_build_ctx* build;
    struct cr_rect_int32 node_rect;
    struct cr_rect_int32 scissor;
    uint32_t scene_index;
    bool scene_has_draw;
};

static void ui_emit_backdrop(struct ui* ui, const struct ui_layer* layer, bool band, const struct ui_emit_ctx* ctx) {
    const struct ui_build_ctx* build = ctx->build;

    if (ctx->scene_index == ui_scene_index_invalid) {
        return;
    }

    struct ui_render_command cmd = { .type = ui_render_command_type_draw_background_blur };
    cmd.draw_background_blur.sample = build->sample;
    cmd.draw_background_blur.container = build->container;
    cmd.draw_background_blur.opacity = ui_layer_opacity_value(layer);
    cmd.draw_background_blur.scene_index = ctx->scene_index;

    if (layer->type == ui_layer_type_glass) {
        assert(band == false);
        struct cr_rect_int32 bounds = ui_glass_capture_rect(layer, ctx->node_rect, ctx->scissor, build->sample, build->to_px_from_pt);
        if (bounds.size.x <= 0 || bounds.size.y <= 0) {
            return;
        }
        cmd.draw_background_blur.bounds = bounds;
        cmd.draw_background_blur.glass = true;
    } else {
        assert(layer->type == ui_layer_type_background_blur);
        const struct ui_render_scene* scene = ui_arr_render_scene_get_ptr(ui->render->scenes, ctx->scene_index);

        struct cr_rect_int32 bounds = cr_rect_int32_intersect(ctx->scissor, band ? scene->bounds : ctx->node_rect);
        struct cr_rect_int32 overlap = cr_rect_int32_intersect(bounds, build->sample);
        if (overlap.size.x <= 0 || overlap.size.y <= 0) {
            return;
        }
        cmd.draw_background_blur.bounds = bounds;
        cmd.draw_background_blur.radius = (float)(layer->background_blur.radius * ui_blur_extent_per_sigma * build->to_px_from_pt);
        cmd.draw_background_blur.band = band;
        if (band == false && ui_scene_masks(ui, ctx->scene_index) == false) {
            cmd.draw_background_blur.scene_index = ui_scene_index_invalid;
        }
    }

    ui_cmd_push_background_blur(ui, cmd, build->open_layers);
}

static void ui_emit_layers(struct ui* ui, const struct ui_layer* layers, uint32_t count, bool outline, const struct ui_emit_ctx* ctx) {
    for (uint32_t i = 0; i < count; i++) {
        const struct ui_layer* layer = &layers[i];

        if (ui_layer_is_backdrop(layer) && (outline == false || layer->type == ui_layer_type_background_blur)) {
            ui_emit_backdrop(ui, layer, outline, ctx);
            continue;
        }

        if (ctx->scene_has_draw == false || ui_layer_is_color(layer) == false) {
            continue;
        }

        struct ui_render_command cmd = { .type = ui_render_command_type_draw_scene };
        cmd.draw_scene.scene_index = ctx->scene_index;
        cmd.draw_scene.fill_layer = outline ? ui_scene_layer_invalid : i;
        cmd.draw_scene.outline_layer = outline ? i : ui_scene_layer_invalid;
        ui_cmd_push(ui, cmd);
    }
}

static bool ui_transform_is_invertible(float3x2 t) {
    return t.columns[0].x * t.columns[1].y != t.columns[0].y * t.columns[1].x;
}

static struct cr_rect_int32 ui_text_clip(const struct ui_build_ctx* ctx, struct cr_rect_int32 node_rect) {
    if (is_equal_float3x2(ctx->transform, float3x2_identity)) {
        return ctx->scissor;
    }
    if (ui_transform_is_invertible(ctx->transform)) {
        return ui_rect_transform_aabb(ctx->scissor, inverse_float3x2(ctx->transform));
    }
    return node_rect;
}

static void ui_emit_text(struct ui* ui, struct ui_node_backing* node, struct cr_rect_int32 node_rect, const struct ui_build_ctx* ctx) {
    struct ui_render_command cmd = { .type = ui_render_command_type_draw_text };
    cmd.draw_text.node = node;
    cmd.draw_text.start_266 = make_int2(ctx->x._raw, ctx->y._raw);
    cmd.draw_text.size_266 = make_int2(ui_fx_to_text_266(node->p2.width, ctx->to_px_from_pt), ui_fx_to_text_266(node->p2.height, ctx->to_px_from_pt));
    cmd.draw_text.clip = ui_text_clip(ctx, node_rect);
    cmd.draw_text.transform = ctx->transform;

    if (node->fill.layer_count == 0) {
        ui_cmd_push(ui, cmd);
        return;
    }

    for (uint32_t i = 0; i < node->fill.layer_count; i++) {
        if (node->fill.layers[i].type == ui_layer_type_none) {
            continue;
        }
        cmd.draw_text.layer = &node->fill.layers[i];
        ui_cmd_push(ui, cmd);
    }
}

static uint32_t ui_cmd_open_layer(struct ui* ui, float opacity, struct ui_build_layer* link, struct ui_build_layer* parent) {
    uint32_t index = ui_arr_render_command_get_count(ui->render->commands);

    struct ui_render_command cmd = { .type = ui_render_command_type_push_layer };
    cmd.push_layer.opacity = opacity;
    ui_cmd_push(ui, cmd);

    *link = (struct ui_build_layer) { .parent = parent, .cmd_index = index };
    return index;
}

static struct ui_render_command* ui_cmd_close_layer(struct ui* ui, uint32_t push_index, struct cr_rect_int32 bounds, struct cr_rect_int32 children_bounds, uint32_t scene_index, uint32_t clip_scene_index) {
    struct ui_render_command* push_cmd = ui_arr_render_command_get_ptr(ui->render->commands, push_index);
    push_cmd->push_layer.bounds = bounds;
    push_cmd->push_layer.scene_index = scene_index;
    push_cmd->push_layer.child_content = children_bounds.size.x > 0 && children_bounds.size.y > 0;
    push_cmd->push_layer.pop_cmd_index = ui_arr_render_command_get_count(ui->render->commands);

    struct ui_render_command pop_cmd = { .type = ui_render_command_type_pop_layer };
    ui_cmd_push(ui, pop_cmd);

    struct ui_render_command comp_cmd = { .type = ui_render_command_type_composite_layer };
    comp_cmd.composite_layer.push_cmd_index = push_index;
    comp_cmd.composite_layer.clip_scene_index = clip_scene_index;
    ui_cmd_push(ui, comp_cmd);

    return ui_arr_render_command_get_ptr(ui->render->commands, push_cmd->push_layer.pop_cmd_index + 1);
}

cr_hdef bool ui_node_has_shadow(const struct ui_node_backing* node) {
    return node->shadow.color.value.w > 0.0f;
}

static int2 ui_node_shadow_offset_px(const struct ui_node_backing* node, double to_px_from_pt) {
    return make_int2((int32_t)lround(node->shadow.offset.x * to_px_from_pt),
                     (int32_t)lround(node->shadow.offset.y * to_px_from_pt));
}

static struct cr_rect_int32 ui_node_effect_bounds(struct ui_node_backing* node, struct cr_rect_int32 node_rect, double to_px_from_pt) {
    double max_shadow_blur_radius = node->blur.radius > 0.0 ? node->blur.radius * ui_blur_extent_per_sigma * to_px_from_pt : 0.0;
    int2 shadow_offset_min = make_int2(0, 0);
    int2 shadow_offset_max = make_int2(0, 0);

    if (ui_node_has_shadow(node)) {
        max_shadow_blur_radius = max_double(max_shadow_blur_radius, node->shadow.radius * ui_blur_extent_per_sigma * to_px_from_pt);
        int2 offset_px = ui_node_shadow_offset_px(node, to_px_from_pt);
        shadow_offset_min = min_int2(shadow_offset_min, offset_px);
        shadow_offset_max = max_int2(shadow_offset_max, offset_px);
    }

    int inflation = max_shadow_blur_radius > 0.0 ? (int)ceil(max_shadow_blur_radius * 1.25) + 4 : 0;

    return (struct cr_rect_int32) {
        .origin = node_rect.origin - inflation + shadow_offset_min,
        .size = node_rect.size + inflation * 2 + (shadow_offset_max - shadow_offset_min),
    };
}

static void ui_build_node(struct ui* ui, struct ui_node_backing* node, const struct ui_build_ctx* parent, struct cr_rect_int32* out_bounds) {
    struct ui_build_ctx ctx = *parent;
    ctx.x = ui_fx_add(parent->x, node->p2.x);
    ctx.y = ui_fx_add(parent->y, node->p2.y);
    double to_px_from_pt = ctx.to_px_from_pt;

    memset(out_bounds, 0, sizeof(*out_bounds));

    if (node->type == ui_content_type_shape && ctx.in_shape_group) {
        return;
    }

    if (is_zero_float3x2(node->transform) == false) {
        float3x2 local = node->transform;
        float2 anchor = make_float2((float)ui_fx_to_double(ctx.x), (float)ui_fx_to_double(ctx.y));
        local.columns[2] = local.columns[2] * (float)to_px_from_pt + anchor - (anchor.x * local.columns[0] + anchor.y * local.columns[1]);
        ctx.transform = is_equal_float3x2(ctx.transform, float3x2_identity)
            ? local
            : mul_float3x2(ctx.transform, local);
    }

    bool transformed = is_equal_float3x2(ctx.transform, float3x2_identity) == false;
    bool has_post_effects = ui_node_has_shadow(node) || node->blur.radius > 0.0;
    bool is_shape_node = node->type == ui_content_type_frame || node->type == ui_content_type_shape;
    bool is_group = node->type == ui_content_type_frame && node->frame.shape_group.op != ui_shape_group_op_none;

    struct cr_rect_int32 node_rect = ui_node_rect_px(node, ctx.x, ctx.y);

    if (node->type == ui_content_type_text) {
        node_rect = ui_text_ink_rect_px(node, node_rect, to_px_from_pt);
    }

    struct cr_rect_int32 node_rect_transformed = transformed ? ui_rect_transform_aabb(node_rect, ctx.transform) : node_rect;

    cr_check(transformed == false || has_post_effects == false, "a shadow or blur cannot be used below a transform");
    cr_check(transformed == false || node->type != ui_content_type_frame || node->frame.backdrop_root == false, "a backdrop root cannot be used below a transform");

    bool clip = node->type == ui_content_type_text || (node->type == ui_content_type_frame && node->frame.clip);
    bool clip_shape_candidate = false;
    if (node->type == ui_content_type_frame && clip) {
        double4 radius = node->frame.corners.radius * to_px_from_pt;
        clip_shape_candidate = is_group || radius.x >= 0.25 || radius.y >= 0.25 || radius.z >= 0.25 || radius.w >= 0.25;
    }

    struct cr_rect_int32 prev_scissor = ctx.scissor;
    if (clip) {
        ctx.scissor = cr_rect_int32_intersect(prev_scissor, node_rect_transformed);
    }

    if (ctx.scissor.size.x <= 0 || ctx.scissor.size.y <= 0) {
        return;
    }

    uint32_t scene_index = ui_scene_index_invalid;
    bool scene_has_draw = false;
    bool outline_active = node->outline.layer_count > 0 && node->outline.thickness > 0.0;

    if (is_shape_node) {
        bool has_backdrop = ui_layers_any(node->fill.layers, node->fill.layer_count, ui_layer_is_backdrop) ||
                            (outline_active && ui_layers_any(node->outline.layers, node->outline.layer_count, ui_layer_is_backdrop));

        cr_check(transformed == false || has_backdrop == false, "a backdrop layer cannot be used below a transform");

        bool needs_scene = is_group || has_backdrop || clip_shape_candidate ||
                           ui_layers_any(node->fill.layers, node->fill.layer_count, ui_layer_is_color) ||
                           (outline_active && ui_layers_any(node->outline.layers, node->outline.layer_count, ui_layer_is_color));

        if (needs_scene) {
            scene_index = ui_scene_build(ui, node, node_rect, &ctx, &scene_has_draw);
        }
    }

    bool clip_shape = clip_shape_candidate && ui_scene_masks(ui, scene_index);
    bool clip_children_hidden = clip_shape_candidate && scene_index == ui_scene_index_invalid;

    *out_bounds = cr_rect_int32_intersect(prev_scissor, transformed ? node_rect_transformed : ui_node_effect_bounds(node, node_rect, to_px_from_pt));

    if (scene_index != ui_scene_index_invalid) {
        struct cr_rect_int32 scene_bounds = ui_arr_render_scene_get_ptr(ui->render->scenes, scene_index)->bounds;
        if (scene_bounds.size.x > 0 && scene_bounds.size.y > 0) {
            *out_bounds = cr_rect_int32_union(*out_bounds, cr_rect_int32_intersect(prev_scissor, scene_bounds));
        }
    }

    float opacity = node->blend.mode == ui_blend_mode_normal ? (float)node->blend.opacity : 1.0f;
    bool requires_layer = opacity < 1.0f || has_post_effects;
    uint32_t layer_cmd_index = 0;
    struct ui_build_layer layer_link;

    if (requires_layer) {
        layer_cmd_index = ui_cmd_open_layer(ui, opacity, &layer_link, ctx.open_layers);
        ctx.open_layers = &layer_link;
    }

    if (clip) {
        struct ui_render_command cmd = { .type = ui_render_command_type_set_scissor };
        cmd.set_scissor.rect = ctx.scissor;
        ui_cmd_push(ui, cmd);
    }

    struct ui_emit_ctx emit_ctx = {
        .build = &ctx,
        .node_rect = node_rect,
        .scissor = prev_scissor,
        .scene_index = scene_index,
        .scene_has_draw = scene_has_draw,
    };

    ui_emit_layers(ui, node->fill.layers, node->fill.layer_count, false, &emit_ctx);

    struct cr_rect_int32 children_bounds = { 0 };

    switch (node->type) {
    case ui_content_type_none: assert(0); break;
    case ui_content_type_frame: {
        struct ui_build_ctx child = ctx;
        child.in_shape_group = ctx.in_shape_group || is_group;

        if (node->frame.backdrop_root) {
            child.sample = cr_rect_int32_intersect(ctx.sample, node_rect);
            child.container = ++ui->render->bg_container_count;
        }

        uint32_t clip_cmd_index = 0;
        struct ui_build_layer clip_link;

        if (clip_shape) {
            clip_cmd_index = ui_cmd_open_layer(ui, 1.0f, &clip_link, ctx.open_layers);
            child.open_layers = &clip_link;
        }

        if (clip_children_hidden == false) {
            for (struct ui_node_backing* child_node = ui_node_get_child(ui, node, 0); child_node != NULL; child_node = ui_node_get_next(ui, child_node)) {
                struct cr_rect_int32 child_bounds;
                ui_build_node(ui, child_node, &child, &child_bounds);

                *out_bounds = cr_rect_int32_union(*out_bounds, child_bounds);
                children_bounds = cr_rect_int32_union(children_bounds, child_bounds);
            }
        }

        if (clip_shape) {
            if (children_bounds.size.x <= 0 || children_bounds.size.y <= 0) {
                ui_arr_render_command_get_ptr(ui->render->commands, clip_cmd_index)->type = ui_render_command_type_none;
            } else {
                ui_cmd_close_layer(ui, clip_cmd_index, children_bounds, children_bounds, ui_scene_index_invalid, scene_index);
            }
        }
    } break;
    case ui_content_type_shape: break;
    case ui_content_type_text: {
        ui_emit_text(ui, node, node_rect, &ctx);
    } break;
    case ui_content_type_custom: {
        struct ui_render_command cmd = { .type = ui_render_command_type_draw_custom };
        cmd.draw_custom.desc = node->custom.desc;
        cmd.draw_custom.transform = ctx.transform;
        cmd.draw_custom.rect_px = node_rect;
        ui_cmd_push(ui, cmd);
    } break;
    }

    if (outline_active) {
        ui_emit_layers(ui, node->outline.layers, node->outline.layer_count, true, &emit_ctx);
    }

    if (clip) {
        struct ui_render_command cmd = { .type = ui_render_command_type_set_scissor };
        cmd.set_scissor.rect = prev_scissor;
        ui_cmd_push(ui, cmd);
    }

    if (requires_layer) {
        struct ui_render_command* comp_cmd = ui_cmd_close_layer(ui, layer_cmd_index, *out_bounds, children_bounds, scene_index, ui_scene_index_invalid);
        ui_arr_render_command_get_ptr(ui->render->commands, layer_cmd_index)->push_layer.scene_only = is_shape_node;
        comp_cmd->composite_layer.blur_radius = node->blur.radius > 0.0 ? (float)(node->blur.radius * ui_blur_extent_per_sigma * to_px_from_pt) : 0.0f;
        if (ui_node_has_shadow(node)) {
            comp_cmd->composite_layer.shadow_offset = ui_node_shadow_offset_px(node, to_px_from_pt);
            comp_cmd->composite_layer.shadow_radius = (float)(node->shadow.radius * ui_blur_extent_per_sigma * to_px_from_pt);
            comp_cmd->composite_layer.shadow_color = node->shadow.color;
        }
    }
}

void ui_build_root(struct ui* ui, struct ui_node_backing* root, struct cr_rect_int32 scissor, double to_px_from_pt) {
    struct ui_build_ctx ctx = {
        .x = ui_fx_zero,
        .y = ui_fx_zero,
        .transform = float3x2_identity,
        .scissor = scissor,
        .sample = scissor,
        .to_px_from_pt = to_px_from_pt,
    };
    struct cr_rect_int32 bounds;
    ui_build_node(ui, root, &ctx, &bounds);
}
