#include "ui_render_impl.h"
#include "ui_blur.h"

static uint32_t ui_elide_paint_count(const struct ui_render_command* commands, uint32_t begin, uint32_t end) {
    uint32_t count = 0;

    for (uint32_t j = begin; j < end; j++) {
        count += commands[j].type != ui_render_command_type_none && commands[j].type != ui_render_command_type_set_scissor;
    }

    return count;
}

void ui_plan_elide_layers(struct ui* ui) {
    struct ui_render* mod = ui->render;

    uint32_t command_count = ui_arr_render_command_get_count(mod->commands);
    struct ui_render_command* commands = ui_arr_render_command_get_data(mod->commands);

    for (uint32_t i = 0; i < command_count; i++) {
        struct ui_render_command* cmd = &commands[i];
        if (cmd->type != ui_render_command_type_push_layer) {
            continue;
        }
        if (cmd->push_layer.scene_only == false || cmd->push_layer.child_content) {
            continue;
        }

        struct ui_render_command* comp = &commands[cmd->push_layer.pop_cmd_index + 1];
        assert(comp->type == ui_render_command_type_composite_layer && comp->composite_layer.push_cmd_index == i);

        if (comp->composite_layer.blur_radius > 0.0f || ui_composite_has_shadow(comp)) {
            continue;
        }

        if (cmd->push_layer.opacity != 1.0f &&
            ui_elide_paint_count(commands, i + 1, cmd->push_layer.pop_cmd_index) > 1) {
            continue;
        }

        commands[cmd->push_layer.pop_cmd_index].type = ui_render_command_type_none;
        comp->type = ui_render_command_type_none;

        if (cmd->push_layer.scene_index != ui_scene_index_invalid) {
            ui_arr_render_scene_get_ptr(mod->scenes, cmd->push_layer.scene_index)->opacity = cmd->push_layer.opacity;
        }
        for (uint32_t j = i + 1; j < cmd->push_layer.pop_cmd_index; j++) {
            if (commands[j].type == ui_render_command_type_draw_background_blur) {
                commands[j].draw_background_blur.opacity *= cmd->push_layer.opacity;
            }
        }
        cmd->type = ui_render_command_type_none;
    }
}

static bool ui_glass_color_equal(struct ui_color a, struct ui_color b) {
    return a.color_space == b.color_space &&
           a.value.x == b.value.x && a.value.y == b.value.y &&
           a.value.z == b.value.z && a.value.w == b.value.w;
}

static bool ui_glass_scene_merge_compatible(const struct ui_render_scene* a, const struct ui_render_scene* b) {
    return a->has_glass && b->has_glass &&
           a->op == ui_shape_group_op_union && b->op == ui_shape_group_op_union &&
           (a->smoothing == 0.0f || a->smoothing == ui_glass_union_smoothness_pt * a->glass_scale) &&
           b->smoothing == 0.0f &&
           a->opacity == 1.0f && b->opacity == 1.0f &&
           a->glass_variant == b->glass_variant &&
           a->glass_slider == b->glass_slider &&
           a->glass_scale == b->glass_scale &&
           a->shape_count + b->shape_count <= 8 &&
           ui_glass_color_equal(a->glass_tint, b->glass_tint);
}

static uint32_t ui_glass_scene_merge(struct ui* ui, uint32_t a_index, uint32_t b_index) {
    struct ui_render* mod = ui->render;

    struct ui_render_scene a = *ui_arr_render_scene_get_ptr(mod->scenes, a_index);
    struct ui_render_scene b = *ui_arr_render_scene_get_ptr(mod->scenes, b_index);

    struct ui_scene_shape shapes[8];
    memcpy(shapes, ui_arr_scene_shape_get_ptr(mod->scene_shapes, a.shape_offset), a.shape_count * sizeof(*shapes));
    memcpy(shapes + a.shape_count, ui_arr_scene_shape_get_ptr(mod->scene_shapes, b.shape_offset), b.shape_count * sizeof(*shapes));

    struct ui_render_scene merged = a;
    merged.shape_offset = ui_arr_scene_shape_get_count(mod->scene_shapes);
    merged.shape_count = a.shape_count + b.shape_count;
    merged.fill_layers = NULL;
    merged.fill_count = 0;
    merged.outline_layers = NULL;
    merged.outline_count = 0;
    merged.outline_thickness = 0.0f;
    merged.smoothing = ui_glass_union_smoothness_pt * merged.glass_scale;
    merged.prepared_frame = 0;
    merged.node_rect = cr_rect_int32_union(a.node_rect, b.node_rect);
    merged.bounds = cr_rect_int32_union(a.bounds, b.bounds);

    merged.glass_min_dim_pt = max_float(a.glass_min_dim_pt, b.glass_min_dim_pt);
    merged.glass_min_dim_lo_pt = min_float(a.glass_min_dim_lo_pt, b.glass_min_dim_lo_pt);

    for (uint32_t i = 0; i < merged.shape_count; i++) {
        ui_arr_scene_shape_append(mod->scene_shapes, shapes[i]);
    }

    uint32_t index = ui_arr_render_scene_get_count(mod->scenes);
    ui_arr_render_scene_append(mod->scenes, merged);

    return index;
}

static struct cr_rect_int32 ui_glass_cmd_extent(struct ui* ui, const struct ui_render_command* cmd) {
    struct ui_render* mod = ui->render;

    struct cr_rect_int32 extent = cmd->draw_background_blur.bounds;
    const struct ui_render_scene* scene = ui_arr_render_scene_get_ptr(mod->scenes, cmd->draw_background_blur.scene_index);
    if (scene->bounds.size.x > 0 && scene->bounds.size.y > 0) {
        extent = cr_rect_int32_union(extent, scene->bounds);
    }

    return extent;
}

static bool ui_glass_merge_candidate(const struct ui_render_command* cmd) {
    return cmd->type == ui_render_command_type_draw_background_blur &&
           cmd->draw_background_blur.glass &&
           cmd->draw_background_blur.opacity == 1.0f &&
           cmd->draw_background_blur.scene_index != ui_scene_index_invalid;
}

void ui_plan_merge_glass(struct ui* ui) {
    struct ui_render* mod = ui->render;

    uint32_t command_count = ui_arr_render_command_get_count(mod->commands);
    struct ui_render_command* commands = ui_arr_render_command_get_data(mod->commands);

    for (uint32_t i = 0; i < command_count; i++) {
        struct ui_render_command* a = &commands[i];
        if (ui_glass_merge_candidate(a) == false || ui_arr_render_scene_get_ptr(mod->scenes, a->draw_background_blur.scene_index)->has_glass == false) {
            continue;
        }

        for (uint32_t j = i + 1; j < command_count; j++) {
            struct ui_render_command* b = &commands[j];
            if (b->type == ui_render_command_type_none) {
                continue;
            }
            if (ui_glass_merge_candidate(b) == false || b->draw_background_blur.container != a->draw_background_blur.container) {
                break;
            }

            const struct ui_render_scene* sa = ui_arr_render_scene_get_ptr(mod->scenes, a->draw_background_blur.scene_index);
            const struct ui_render_scene* sb = ui_arr_render_scene_get_ptr(mod->scenes, b->draw_background_blur.scene_index);
            if (ui_glass_scene_merge_compatible(sa, sb) == false) {
                break;
            }

            struct cr_rect_int32 overlap = cr_rect_int32_intersect(ui_glass_cmd_extent(ui, a), ui_glass_cmd_extent(ui, b));
            if (overlap.size.x <= 0 || overlap.size.y <= 0) {
                break;
            }

            a->draw_background_blur.scene_index = ui_glass_scene_merge(ui, a->draw_background_blur.scene_index, b->draw_background_blur.scene_index);
            a->draw_background_blur.bounds = cr_rect_int32_union(a->draw_background_blur.bounds, b->draw_background_blur.bounds);
            b->type = ui_render_command_type_none;
        }
    }
}

static bool ui_scene_shapes_overlap(const struct ui_scene_shape* shapes, uint32_t count) {
    for (uint32_t i = 0; i + 1 < count; i++) {
        float2 a_min = shapes[i].origin - 1.0f;
        float2 a_max = shapes[i].origin + shapes[i].size + 1.0f;

        for (uint32_t j = i + 1; j < count; j++) {
            float2 b_min = shapes[j].origin;
            float2 b_max = shapes[j].origin + shapes[j].size;

            if (a_min.x < b_max.x && b_min.x < a_max.x && a_min.y < b_max.y && b_min.y < a_max.y) {
                return true;
            }
        }
    }

    return false;
}

void ui_plan_select_sdf(struct ui* ui) {
    struct ui_render* mod = ui->render;

    uint32_t scene_count = ui_arr_render_scene_get_count(mod->scenes);

    for (uint32_t i = 0; i < scene_count; i++) {
        struct ui_render_scene* scene = ui_arr_render_scene_get_ptr(mod->scenes, i);
        const struct ui_scene_shape* shapes = ui_arr_scene_shape_get_ptr(mod->scene_shapes, scene->shape_offset);

        bool needs_field = false;
        for (uint32_t s = 0; s < scene->shape_count; s++) {
            needs_field |= shapes[s].type == ui_shape_type_path ||
                           (shapes[s].type == ui_shape_type_ellipse && fabsf(shapes[s].size.x - shapes[s].size.y) > 1e-3f);
        }

        bool any_gradient = ui_layers_any(scene->fill_layers, scene->fill_count, ui_layer_is_gradient) ||
                            ui_layers_any(scene->outline_layers, scene->outline_count, ui_layer_is_gradient);

        scene->use_sdf = scene->has_glass ||
                         any_gradient ||
                         needs_field ||
                         (scene->shape_count > 1 && (scene->op == ui_shape_group_op_subtract ||
                                                     scene->smoothing > 0.0f ||
                                                     ui_scene_shapes_overlap(shapes, scene->shape_count)));

        assert((scene->use_sdf && is_equal_float3x2(scene->transform, float3x2_identity) == false) == false &&
               "transformed nodes support solid and graphic fills on rects, rounded rects and circles; sdf composite scenes (gradients, paths, glass, overlapping groups) are unsupported");
    }
}

void ui_plan_merge_outlines(struct ui* ui) {
    struct ui_render* mod = ui->render;

    uint32_t command_count = ui_arr_render_command_get_count(mod->commands);
    struct ui_render_command* commands = ui_arr_render_command_get_data(mod->commands);

    for (uint32_t i = 0; i + 1 < command_count; i++) {
        struct ui_render_command* fill = &commands[i];
        struct ui_render_command* outline = &commands[i + 1];

        if (fill->type != ui_render_command_type_draw_scene ||
            outline->type != ui_render_command_type_draw_scene ||
            fill->draw_scene.fill_layer == ui_scene_layer_invalid ||
            fill->draw_scene.outline_layer != ui_scene_layer_invalid ||
            outline->draw_scene.scene_index != fill->draw_scene.scene_index ||
            outline->draw_scene.fill_layer != ui_scene_layer_invalid ||
            outline->draw_scene.outline_layer == ui_scene_layer_invalid) {
            continue;
        }

        const struct ui_render_scene* scene = ui_arr_render_scene_get_ptr(mod->scenes, fill->draw_scene.scene_index);
        if (ui_scene_outline_layer(scene, outline->draw_scene.outline_layer)->type != ui_layer_type_solid) {
            continue;
        }

        fill->draw_scene.outline_layer = outline->draw_scene.outline_layer;
        outline->type = ui_render_command_type_none;
    }
}

struct ui_plan_frame {
    struct cr_rect_int32 rect;
    bool known;
    bool prepass;
};

static uint32_t ui_plan_prepare_scene(struct ui* ui, struct ui_render_scene* scene, const struct ui_scene_draw* draw,
                                      bool has_content, struct cr_rect_int32 content_rect, struct cr_rect_int32 rect, struct cr_rect_int32 scissor) {
    struct ui_render* mod = ui->render;

    struct ui_sdf_scene_desc desc;
    if (ui_scene_sdf_make_desc(ui, scene, draw, has_content, content_rect, rect, scissor, &desc) == false) {
        return 0;
    }

    if (scene->prepared_frame == 0) {
        struct ui_prepared_frame* frame = ui_arr_prepared_frame_append_ptr(mod->prepared_frames, 1);
        frame->remaining = 0;
        ui_sdf_scene_prepare(ui->render->sdf, ui, &desc, &frame->frame);
        scene->prepared_frame = ui_arr_prepared_frame_get_count(mod->prepared_frames);
    } else if (ui_arr_prepared_frame_get_ptr(mod->prepared_frames, scene->prepared_frame - 1)->frame.analytic) {
        bool analytic = ui_sdf_scene_analytic(&desc);
        assert(analytic);
    }
    ui_arr_prepared_frame_get_ptr(mod->prepared_frames, scene->prepared_frame - 1)->remaining++;

    struct ui_prepared_scene* prepared = ui_arr_prepared_scene_append_ptr(mod->prepared_scenes, 1);
    prepared->frame_index = scene->prepared_frame - 1;
    prepared->desc = desc;
    prepared->composited = false;

    return ui_arr_prepared_scene_get_count(mod->prepared_scenes);
}

void ui_plan_resources(struct ui* ui, struct cr_rect_int32 initial_scissor) {
    struct ui_render* mod = ui->render;

    uint32_t command_count = ui_arr_render_command_get_count(mod->commands);
    struct ui_render_command* commands = ui_arr_render_command_get_data(mod->commands);

    struct ui_plan_frame stack[ui_pass_stack_capacity + 1];
    uint32_t stack_count = 0;

    stack[stack_count++] = (struct ui_plan_frame) { .rect = initial_scissor, .known = true, .prepass = true };

    struct cr_rect_int32 scissor = initial_scissor;
    uint64_t budget = ui->render->layer_prepass_limit;
    uint32_t level = 0;
    uint32_t level_count = 0;

    for (uint32_t i = 0; i < command_count; i++) {
        struct ui_render_command* cmd = &commands[i];

        switch (cmd->type) {
        case ui_render_command_type_set_scissor: scissor = cmd->set_scissor.rect; break;

        case ui_render_command_type_push_layer: {
            struct ui_plan_frame* parent = &stack[stack_count - 1];
            int2 size = max_int2(cmd->push_layer.bounds.size, make_int2(1, 1));
            uint64_t bytes = (uint64_t)size.x * (uint64_t)size.y * (ui->render->ctx.compositing_format == gpu_format_bgra8 ? 4 : 8);

            cmd->push_layer.reserved = parent->known && bytes <= budget;
            cmd->push_layer.prepass = cmd->push_layer.reserved && parent->prepass && cmd->push_layer.backdrop_dep == false;

            struct ui_plan_frame frame = {
                .rect = { .origin = cmd->push_layer.bounds.origin, .size = size },
                .known = cmd->push_layer.reserved,
                .prepass = cmd->push_layer.prepass,
            };

            if (cmd->push_layer.reserved) {
                budget -= bytes;
                cmd->push_layer.tex = ui_transient_reserve(ui, ui->render->ctx.compositing_format, (uint32_t)size.x, (uint32_t)size.y);
            }

            if (cmd->push_layer.prepass) {
                ui_arr_prepass_item_append(mod->prepass_passes, (struct ui_prepass_item) {
                    .cmd_index = i,
                    .level = level,
                    .scissor = scissor,
                });
                level++;
                level_count = max_uint32(level_count, level);
            }

            assert(stack_count < cr_count(stack));
            stack[stack_count++] = frame;
        } break;

        case ui_render_command_type_pop_layer: {
            stack_count--;
            if (stack[stack_count].prepass) {
                level--;
            }
        } break;

        case ui_render_command_type_draw_scene: {
            const struct ui_plan_frame* top = &stack[stack_count - 1];
            struct ui_render_scene* scene = ui_arr_render_scene_get_ptr(mod->scenes, cmd->draw_scene.scene_index);
            if (scene->use_sdf == false) {
                break;
            }
            struct ui_scene_draw draw = ui_scene_draw_resolve(scene, cmd->draw_scene.fill_layer, cmd->draw_scene.outline_layer);
            cmd->draw_scene.prepared = ui_plan_prepare_scene(ui, scene, &draw, false, (struct cr_rect_int32) { 0 }, top->rect, scissor);
        } break;

        case ui_render_command_type_draw_background_blur: {
            const struct ui_plan_frame* top = &stack[stack_count - 1];
            if (cmd->draw_background_blur.scene_index == ui_scene_index_invalid) {
                break;
            }
            struct ui_render_scene* mask = ui_arr_render_scene_get_ptr(mod->scenes, cmd->draw_background_blur.scene_index);
            if (mask->use_sdf == false) {
                break;
            }
            struct ui_scene_draw draw = ui_bg_blur_scene_draw(cmd);
            struct cr_rect_int32 mask_scissor = cr_rect_int32_intersect(scissor, cmd->draw_background_blur.bounds);
            cmd->draw_background_blur.prepared = ui_plan_prepare_scene(ui, mask, &draw, true, ui_bg_blur_cmd_capture_rect(cmd), top->rect, mask_scissor);
        } break;

        case ui_render_command_type_composite_layer: {
            const struct ui_plan_frame* top = &stack[stack_count - 1];
            const struct ui_render_command* push_cmd = &commands[cmd->composite_layer.push_cmd_index];

            if (cmd->composite_layer.clip_scene_index != ui_scene_index_invalid) {
                struct ui_render_scene* mask = ui_arr_render_scene_get_ptr(mod->scenes, cmd->composite_layer.clip_scene_index);
                if (mask->use_sdf) {
                    struct cr_rect_int32 content_rect = ui_layer_rect(push_cmd);
                    struct ui_scene_draw draw = { .bg_opacity = push_cmd->push_layer.opacity };
                    struct cr_rect_int32 clip_scissor = cr_rect_int32_intersect(scissor, content_rect);
                    cmd->composite_layer.clip_prepared = ui_plan_prepare_scene(ui, mask, &draw, true, content_rect, top->rect, clip_scissor);
                }
            }

            bool effect = ui_composite_has_shadow(cmd) || cmd->composite_layer.blur_radius > 0.0f;
            if (effect == false || ui_composite_bake(cmd, push_cmd)) {
                break;
            }
            if (push_cmd->push_layer.prepass == false || top->known == false) {
                break;
            }

            cmd->composite_layer.prepass = true;

            ui_arr_prepass_item_append(mod->prepass_blurs, (struct ui_prepass_item) {
                .cmd_index = i,
                .level = level,
            });
        } break;

        default: break;
        }
    }

    mod->ctx.prepass_level_count = level_count;
}
