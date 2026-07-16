#include "ui_render_impl.h"
#include "ui_atlas.h"
#include "ui_blur.h"
#include "ui_blur_shared.h"
#include "ui_color.h"
#include "ui_text.h"

extern const unsigned char* const shaders_embed_data[];
extern const unsigned long long shaders_embed_sizes[];

static const struct {
    const char* v_name;
    const char* f_name;
} ui_pipeline_names[] = {
    { "ui_v_solid", "ui_f_solid" },
    { "ui_v_solid_sdf", "ui_f_solid_sdf" },
    { "ui_v_graphic", "ui_f_graphic" },
    { "ui_v_graphic_sdf", "ui_f_graphic_sdf" },
    { "ui_v_glyph", "ui_f_glyph" },
    { "ui_v_final", "ui_f_final" },
};

_Static_assert(cr_count(ui_pipeline_names) == ui_pipeline_count, "");

void ui_render_init(struct ui* ui) {
    struct ui_render* render = cr_malloc(ui->mem, sizeof(*render), 256, cr_loc);

    memset(render, 0, sizeof(*render));
    render->mem = ui->mem;
    render->layer_prepass_limit = ui_layer_prepass_max_bytes;
    render->commands = ui_arr_render_command_create(ui->mem);
    render->scenes = ui_arr_render_scene_create(ui->mem);
    render->scene_shapes = ui_arr_scene_shape_create(ui->mem);
    render->prepared_frames = ui_arr_prepared_frame_create(ui->mem);
    render->prepared_scenes = ui_arr_prepared_scene_create(ui->mem);
    render->prepass_passes = ui_arr_prepass_item_create(ui->mem);
    render->prepass_blurs = ui_arr_prepass_item_create(ui->mem);
    render->bg_stashes = ui_arr_bg_stash_create(ui->mem);
    ui->render = render;

    static const char* sampler_labels[2][2] = {
        { "ui repeat", "ui repeat nearest" },
        { "ui clamp", "ui clamp nearest" },
    };
    for (uint32_t clamp = 0; clamp < 2; clamp++) {
        for (uint32_t nearest = 0; nearest < 2; nearest++) {
            render->samplers[clamp][nearest] = gpu_sampler_create(ui->device, &(struct gpu_sampler_desc) {
                .label = sampler_labels[clamp][nearest],
                .repeat = clamp == 0,
                .filter = nearest ? gpu_filter_nearest : gpu_filter_linear,
            });
        }
    }

    {
        struct gpu_binding_desc bindings[ui_bind_group_0_binding_count] = {
            [ui_bind_group_0_globals] = {
                .binding = ui_bind_group_0_globals,
                .type = gpu_binding_type_uniform,
                .stages = gpu_stage_vertex | gpu_stage_fragment,
                .dynamic_offset = true,
            },
            [ui_bind_group_0_uniforms] = {
                .binding = ui_bind_group_0_uniforms,
                .type = gpu_binding_type_uniform,
                .stages = gpu_stage_vertex | gpu_stage_fragment,
                .dynamic_offset = true,
            },
            [ui_bind_group_0_vertices] = {
                .binding = ui_bind_group_0_vertices,
                .type = gpu_binding_type_storage_read,
                .stages = gpu_stage_vertex | gpu_stage_fragment,
            },
            [ui_bind_group_0_sdf] = {
                .binding = ui_bind_group_0_sdf,
                .type = gpu_binding_type_storage_read,
                .stages = gpu_stage_fragment,
            },
        };
        render->bind_layout_0 = gpu_bind_layout_create(ui->device, bindings, cr_count(bindings));
    }

    {
        struct gpu_binding_desc bindings[ui_bind_group_1_binding_count] = {
            [ui_bind_group_1_texture] = {
                .binding = ui_bind_group_1_texture,
                .type = gpu_binding_type_texture,
                .stages = gpu_stage_fragment,
            },
            [ui_bind_group_1_sampler] = {
                .binding = ui_bind_group_1_sampler,
                .type = gpu_binding_type_sampler,
                .stages = gpu_stage_fragment,
            },
        };
        render->bind_layout_1 = gpu_bind_layout_create(ui->device, bindings, cr_count(bindings));
    }

    struct gpu_library* library = gpu_library_create(ui->device, shaders_embed_data[0], shaders_embed_sizes[0], "ui");

    for (int i = 0; i < (int)ui_pipeline_count; i++) {
        enum ui_pipeline pipeline = (enum ui_pipeline)i;

        for (uint32_t bgra8 = 0; bgra8 < 2; bgra8++) {
            render->pipelines[bgra8][i] = gpu_render_pipeline_create(ui->device, &(struct gpu_render_pipeline_desc) {
                .library = library,
                .vertex_name = ui_pipeline_names[i].v_name,
                .fragment_name = ui_pipeline_names[i].f_name,
                .layouts = { render->bind_layout_0, render->bind_layout_1 },
                .layout_count = pipeline == ui_pipeline_solid || pipeline == ui_pipeline_solid_sdf ? 1u : 2u,
                .color_format = bgra8 ? gpu_format_bgra8 : gpu_format_rgba16f,
                .blend = pipeline != ui_pipeline_final,
            });
        }
    }

    render->sdf = ui_sdf_create(ui->mem, ui->device, library);
    render->blur = ui_blur_create(ui->mem, ui->device, library);
    render->pipelines_serial = gpu_device_get_pipelines_serial(ui->device);

    gpu_library_destroy(ui->device, library);
}

void ui_render_deinit(struct ui* ui) {
    struct ui_render* render = ui->render;

    ui_deinit_slots(ui);
    ui_sdf_destroy(render->sdf);
    ui_blur_destroy(render->blur);

    for (int i = 0; i < (int)ui_pipeline_count; i++) {
        gpu_render_pipeline_destroy(ui->device, render->pipelines[0][i]);
        gpu_render_pipeline_destroy(ui->device, render->pipelines[1][i]);
    }
    gpu_bind_layout_destroy(ui->device, render->bind_layout_0);
    gpu_bind_layout_destroy(ui->device, render->bind_layout_1);
    for (uint32_t i = 0; i < 4; i++) {
        gpu_sampler_destroy(ui->device, render->samplers[i >> 1][i & 1]);
    }

    ui_arr_bg_stash_destroy(render->bg_stashes);
    ui_arr_prepass_item_destroy(render->prepass_blurs);
    ui_arr_prepass_item_destroy(render->prepass_passes);
    ui_arr_prepared_scene_destroy(render->prepared_scenes);
    ui_arr_prepared_frame_destroy(render->prepared_frames);
    ui_arr_scene_shape_destroy(render->scene_shapes);
    ui_arr_render_scene_destroy(render->scenes);
    ui_arr_render_command_destroy(render->commands);
    cr_free(ui->mem, render, cr_loc);
    ui->render = NULL;
}

struct gpu_bind_group* ui_render_texture_bind_group(struct ui* ui, struct gpu_texture* texture, bool clamp, bool nearest) {
    struct ui_render* render = ui->render;
    struct gpu_bind_desc binds[ui_bind_group_1_binding_count] = {
        [ui_bind_group_1_texture] = {
            .binding = ui_bind_group_1_texture,
            .texture = texture,
        },
        [ui_bind_group_1_sampler] = {
            .binding = ui_bind_group_1_sampler,
            .sampler = render->samplers[clamp][nearest],
        },
    };
    return gpu_bind_group_create(ui->device, render->bind_layout_1, binds, cr_count(binds));
}

struct ui_sdf* ui_render_sdf(void) {
    return ui_ctx()->render->sdf;
}

void ui_set_layer_prepass_limit(uint64_t bytes) {
    ui_ctx()->render->layer_prepass_limit = bytes;
}

bool ui_render_last_direct_(void) {
    return ui_ctx()->render->ctx.direct;
}

void ui_render_retain_graphic(struct ui* ui, struct ui_graphic* graphic) {
    ui_atlas_retain_graphic(graphic);
    ui_slot_own(ui->render->ctx.slot, (struct ui_slot_resource) { .type = ui_slot_resource_type_graphic, .graphic = graphic });
}

static uint32_t ui_tex_globals(struct ui* ui, struct ui_transient_tex tex) {
    uint32_t offset;
    struct ui_globals* globals = ui_alloc_uniforms(ui, sizeof(*globals), &offset);
    globals->to_clip_from_px_scale = 2.0f / to_float2_from_uint2(ui_transient_tex_size(ui, tex));
    globals->to_clip_from_px_offset = make_float2(0.0f, 0.0f);
    return offset;
}

static void ui_encode_pass(struct ui* ui, struct gpu_texture* target, bool clear, const char* label) {
    struct ui_slot* slot = ui->render->ctx.slot;

    ui->render->ctx.pass_format = gpu_texture_get_format(target);
    slot->pass = gpu_command_buffer_begin_render(slot->command_buffer, target, clear ? gpu_load_clear : gpu_load_keep, label);

    uint32_t offsets[2] = { 0, 0 };
    gpu_render_pass_set_bind_group(slot->pass, 0, slot->bind_group, offsets, cr_count(offsets));
}

static void ui_begin_region_pass(struct ui* ui, struct ui_transient_tex tex, bool clear) {
    struct ui_render* mod = ui->render;

    mod->ctx.pass_tex = tex;
    mod->ctx.pass_format = gpu_texture_get_format(ui_transient_texture(ui, tex));
    mod->ctx.pass_pending = clear == false;

    if (clear) {
        ui_encode_pass(ui, ui_transient_target(ui, tex), true, "ui region");
    }

    ui_apply_scissor(ui);
}

void ui_require_pass(struct ui* ui) {
    struct ui_render* mod = ui->render;

    if (mod->ctx.pass_pending == false) {
        return;
    }
    mod->ctx.pass_pending = false;

    ui_encode_pass(ui, ui_transient_target(ui, mod->ctx.pass_tex), false, "ui region");
    ui_apply_scissor(ui);
}

static void ui_end_pass(struct ui* ui) {
    struct ui_render* mod = ui->render;

    ui_flush(ui);

    if (mod->ctx.pass_pending) {
        mod->ctx.pass_pending = false;
        return;
    }

    gpu_render_pass_end(mod->ctx.slot->pass);
    mod->ctx.slot->pass = NULL;
}

static void ui_resume_pass(struct ui* ui) {
    ui_begin_region_pass(ui, ui->render->ctx.pass_tex, false);
}

struct ui_pass_state {
    struct ui_transient_tex tex;
    struct cr_rect_int32 rect;
    uint32_t globals_offset;
    float opacity;
};

static struct ui_pass_state ui_pass_state_make(struct ui* ui, struct ui_transient_tex tex, struct cr_rect_int32 rect, float opacity) {
    return (struct ui_pass_state) {
        .tex = tex,
        .rect = rect,
        .globals_offset = ui_tex_globals(ui, tex),
        .opacity = opacity,
    };
}

static void ui_enter_pass_state(struct ui* ui, const struct ui_pass_state* state) {
    ui->render->ctx.rect = state->rect;
    ui->render->ctx.globals_offset = state->globals_offset;
}

struct ui_pass_save {
    struct ui_transient_tex pass_tex;
    struct cr_rect_int32 rect;
    uint32_t globals_offset;
    struct cr_rect_int32 active_scissor;
};

static struct ui_pass_save ui_begin_scratch_pass(struct ui* ui, struct ui_transient_tex tex, struct cr_rect_int32 rect, struct cr_rect_int32 scissor) {
    struct ui_render* mod = ui->render;
    struct ui_pass_save saved = {
        .pass_tex = mod->ctx.pass_tex,
        .rect = mod->ctx.rect,
        .globals_offset = mod->ctx.globals_offset,
        .active_scissor = mod->ctx.active_scissor,
    };

    mod->ctx.rect = rect;
    mod->ctx.globals_offset = ui_tex_globals(ui, tex);
    mod->ctx.active_scissor = scissor;
    ui_begin_region_pass(ui, tex, true);
    return saved;
}

static void ui_end_scratch_pass(struct ui* ui, const struct ui_pass_save* saved) {
    struct ui_render* mod = ui->render;

    ui_end_pass(ui);
    mod->ctx.pass_tex = saved->pass_tex;
    mod->ctx.rect = saved->rect;
    mod->ctx.globals_offset = saved->globals_offset;
    mod->ctx.active_scissor = saved->active_scissor;
}

static void ui_set_active_scissor(struct ui* ui, struct cr_rect_int32 rect) {
    assert(ui->render->ctx.vertex_count == 0);
    ui->render->ctx.active_scissor = rect;
}

static struct cr_rect_int32 ui_push_scissor(struct ui* ui, struct cr_rect_int32 rect) {
    struct cr_rect_int32 saved = ui->render->ctx.active_scissor;

    ui_set_active_scissor(ui, cr_rect_int32_intersect(saved, rect));
    ui_apply_scissor(ui);
    return saved;
}

static void ui_pop_scissor(struct ui* ui, struct cr_rect_int32 saved) {
    ui_set_active_scissor(ui, saved);
    ui_apply_scissor(ui);
}

static void ui_blur_chain_run(struct ui* ui, struct ui_blur_chain* chain, struct ui_transient_tex release) {
    ui_blur_chain_encode(ui, ui->render->ctx.slot->command_buffer, chain);
    ui_blur_chain_finish(ui, chain);
    ui_transient_release(ui, release);
}

static void ui_blur_batch_encode(struct ui* ui, const char* label, struct ui_blur_chain* blur_chains, struct ui_blur_pyramid_chain* chains, uint32_t count, uint32_t max_stages) {
    struct ui_slot* slot = ui->render->ctx.slot;

    if (max_stages != 0) {
        struct gpu_compute_pass* pass = gpu_command_buffer_begin_compute_concurrent(slot->command_buffer, label);
        for (uint32_t stage = 0; stage < max_stages; stage++) {
            if (stage > 0) {
                gpu_compute_pass_barrier(pass);
            }
            for (uint32_t k = 0; k < count; k++) {
                if (blur_chains[k].count > stage) {
                    ui_blur_chain_encode_stage(ui, pass, &blur_chains[k], stage);
                }
            }
            for (uint32_t k = 0; chains != NULL && k < count; k++) {
                if (chains[k].pass_count > stage) {
                    ui_blur_pyramid_encode_pass(ui, pass, &chains[k], stage);
                }
            }
        }
        gpu_compute_pass_end(pass);
    }

    for (uint32_t k = 0; k < count; k++) {
        if (blur_chains[k].count != 0) {
            ui_blur_chain_finish(ui, &blur_chains[k]);
        }
        if (chains != NULL && chains[k].pass_count != 0) {
            ui_blur_pyramid_finish(ui, &chains[k]);
        }
    }
}

static struct ui_transient_tex ui_blur_prepare_output(struct ui* ui, struct ui_transient_tex src, uint32_t src_level, float radius_px, float4 color, bool colorize, struct ui_blur_chain* chain, uint32_t* out_level) {
    float radius = radius_px / (float)(1u << src_level);
    uint32_t level = ui_blur_coarse_level(radius, make_uint2(src.w, src.h));
    uint2 dims = ui_blur_coarse_size(make_uint2(src.w, src.h), level);

    uint32_t channel = ui_transient_tex_channel(ui, src) == 0 ? ui_transient_channel_blur_out : 0;
    struct ui_transient_tex dst = ui_transient_reserve_storage_channel(ui, gpu_format_rgba16f, dims.x, dims.y, channel);

    ui_blur_chain_prepare(ui, ui->render->ctx.slot, src, dst, radius, color, colorize, chain);

    *out_level = src_level + level;
    return dst;
}

static struct ui_transient_tex ui_layer_blur_prepare(struct ui* ui, struct ui_render_command* cmd, struct ui_blur_chain* chain) {
    struct ui_render_command* push_cmd = ui_arr_render_command_get_ptr(ui->render->commands, cmd->composite_layer.push_cmd_index);
    struct ui_transient_tex layer = push_cmd->push_layer.tex;

    push_cmd->push_layer.tex = ui_blur_prepare_output(ui, layer, push_cmd->push_layer.tex_level, cmd->composite_layer.blur_radius,
                                                      make_float4(1.0f, 1.0f, 1.0f, 1.0f), false, chain, &push_cmd->push_layer.tex_level);
    return layer;
}

static struct ui_transient_tex ui_drop_shadow_prepare(struct ui* ui, struct ui_render_command* cmd, struct ui_blur_chain* chain) {
    struct ui_render_command* push_cmd = ui_arr_render_command_get_ptr(ui->render->commands, cmd->composite_layer.push_cmd_index);

    cmd->composite_layer.shadow_tex = ui_blur_prepare_output(ui, push_cmd->push_layer.tex, push_cmd->push_layer.tex_level, cmd->composite_layer.shadow_radius,
                                                             ui_to_compositing_from_color(cmd->composite_layer.shadow_color), true, chain, &cmd->composite_layer.shadow_tex_level);
    return ui_transient_tex_none;
}

static struct ui_transient_tex ui_composite_blur_prepare(struct ui* ui, struct ui_render_command* cmd, struct ui_blur_chain* chain) {
    return cmd->composite_layer.blur_radius > 0.0f ? ui_layer_blur_prepare(ui, cmd, chain) : ui_drop_shadow_prepare(ui, cmd, chain);
}

static void ui_draw_shadow_quad(struct ui* ui, struct ui_render_command* cmd) {
    struct ui_render_command* push_cmd = ui_arr_render_command_get_ptr(ui->render->commands, cmd->composite_layer.push_cmd_index);
    struct cr_rect_int32 rect = ui_layer_rect(push_cmd);

    int2 start = rect.origin + cmd->composite_layer.shadow_offset;
    ui_add_tex_quad(ui, start, start + rect.size, cmd->composite_layer.shadow_tex, cmd->composite_layer.shadow_tex_level, make_float4(1.0f, 1.0f, 1.0f, 1.0f));
    ui_flush(ui);

    ui_transient_release(ui, cmd->composite_layer.shadow_tex);
}

static void ui_release_prepared_frame(struct ui* ui, struct ui_prepared_frame* frame) {
    assert(frame->remaining > 0);
    frame->remaining--;
    if (frame->remaining == 0) {
        ui_sdf_frame_destroy(ui->render->sdf, ui, ui->render->ctx.slot, &frame->frame);
    }
}

static void ui_draw_scene_sdf(struct ui* ui, const struct ui_render_scene* scene, const struct ui_scene_draw* draw, uint32_t prepared_index,
                              const struct ui_transient_tex* content_tex, uint32_t content_level, const struct ui_glass_pyramid* glass, struct cr_rect_int32 content_rect) {
    struct ui_render* mod = ui->render;
    struct ui_slot* slot = mod->ctx.slot;

    struct ui_sdf_scene_desc local_desc;
    struct ui_sdf_frame analytic_frame = { .analytic = true };
    struct ui_sdf_scene_desc* desc = &local_desc;
    struct ui_sdf_frame* frame = &analytic_frame;
    struct ui_prepared_scene* prepared = NULL;
    struct ui_prepared_frame* prepared_frame = NULL;

    if (prepared_index != 0) {
        prepared = ui_arr_prepared_scene_get_ptr(mod->prepared_scenes, prepared_index - 1);
        prepared_frame = ui_arr_prepared_frame_get_ptr(mod->prepared_frames, prepared->frame_index);
        desc = &prepared->desc;
        frame = &prepared_frame->frame;
    } else if (ui_scene_sdf_make_desc(ui, scene, draw, content_tex != NULL, content_rect, mod->ctx.rect, mod->ctx.active_scissor, desc) == false) {
        return;
    }

    if (content_tex != NULL) {
        ui_scene_sdf_set_content(ui, desc, *content_tex, content_rect, content_level);
    }
    ui_scene_sdf_set_glass(ui, desc, glass);

    if (prepared == NULL && ui_sdf_scene_analytic(desc) == false) {
        ui_end_pass(ui);
        desc->out_texture = ui_transient_texture(ui, mod->ctx.pass_tex);
        ui_sdf_encode_scene(mod->sdf, ui, slot, slot->command_buffer, desc);
        ui_resume_pass(ui);
        return;
    }

    ui_flush(ui);
    ui_require_pass(ui);
    ui_sdf_scene_composite(mod->sdf, ui, slot, slot->pass, mod->ctx.pass_format, frame, desc);
    if (prepared != NULL) {
        prepared->composited = true;
        ui_release_prepared_frame(ui, prepared_frame);
    }
    ui_apply_scissor(ui);
}

static const struct ui_render_scene* ui_bg_blur_mask(struct ui* ui, const struct ui_render_command* cmd) {
    if (cmd->draw_background_blur.scene_index == ui_scene_index_invalid) {
        return NULL;
    }
    return ui_arr_render_scene_get_ptr(ui->render->scenes, cmd->draw_background_blur.scene_index);
}

static float ui_glass_pyramid_level_max(const struct ui_render_scene* mask) {
    uint32_t gv = mask->glass_variant;
    float s = max_float(mask->glass_scale, 1e-3f);
    float slider = mask->glass_slider;
    float size_ramp = ui_glass_size_t(mask->glass_min_dim_pt);

    float rh_deep = ui_glass_blur_rh_pt(gv, slider, size_ramp, mask->glass_min_dim_lo_pt,
                                           mask->glass_min_dim_lo_pt > 0.0f ? -0.5f * mask->glass_min_dim_lo_pt : -1e6f);
    float rh_out = ui_glass_blur_rh_pt(gv, slider, size_ramp, mask->glass_min_dim_lo_pt, 1.0f);
    float rh_fine = max_float(rh_deep, rh_out);
    float fine_level = rh_fine > 0.0f ? ui_glass_blur_lod(rh_fine, s) : 0.0f;

    float veil_rt = ui_glass_blur_veil_rt_pt(ui_glass_fill_radius_pt(gv, slider));
    float bleed_rt = ui_glass_blur_bleed_rt_pt(ui_glass_bleed_radius_pt(gv, mask->glass_min_dim_pt));
    float veil_level = veil_rt > 0.0f ? ui_glass_blur_lod(veil_rt, s) : 0.0f;
    float bleed_level = bleed_rt > 0.0f ? ui_glass_blur_lod(bleed_rt, s) : 0.0f;

    return max_float(fine_level, max_float(veil_level, bleed_level));
}

#define ui_bg_blur_run_capacity 16

struct cr_rect_int32 ui_bg_blur_capture_rect(struct cr_rect_int32 bounds, float radius, struct cr_rect_int32 sample) {
    if (radius <= 0.0f) {
        return bounds;
    }

    int32_t margin = 3 + (int32_t)ceilf(radius);
    struct cr_rect_int32 inflated = { .origin = bounds.origin - margin, .size = bounds.size + 2 * margin };
    return cr_rect_int32_intersect(inflated, sample);
}

static struct ui_transient_tex ui_bg_blur_snapshot_layers(struct ui* ui, struct cr_rect_int32 capture, struct cr_rect_int32 scissor, const struct ui_pass_state* stack, uint32_t stack_count) {
    uint2 size = to_uint2_from_int2(max_int2(capture.size, make_int2(1, 1)));
    struct ui_transient_tex snapshot = ui_transient_reserve(ui, ui->render->ctx.compositing_format, size.x, size.y);

    struct ui_pass_save saved = ui_begin_scratch_pass(ui, snapshot, capture, scissor);

    for (uint32_t s = 0; s < stack_count; s++) {
        const struct ui_pass_state* pass = &stack[s];

        int2 pass_start = pass->rect.origin;
        int2 pass_end = pass_start + pass->rect.size;

        if (all_int2(pass_start < capture.origin + capture.size) && all_int2(pass_end > capture.origin)) {
            float4 tint = make_float4(pass->opacity, pass->opacity, pass->opacity, pass->opacity);
            ui_add_tex_quad(ui, pass_start, pass_end, pass->tex, 0, tint);
        }
    }

    ui_end_scratch_pass(ui, &saved);

    return snapshot;
}

static struct ui_transient_tex ui_bg_blur_prepare(struct ui* ui, uint32_t cmd_index, const struct ui_pass_state* stack, uint32_t stack_count,
                                                  struct ui_blur_chain* blur_chain, struct ui_blur_pyramid_chain* chain, uint32_t* inout_max_stages) {
    struct ui_render* mod = ui->render;
    struct ui_slot* slot = mod->ctx.slot;
    struct ui_render_command* cmd = ui_arr_render_command_get_ptr(mod->commands, cmd_index);
    const struct ui_render_scene* mask = ui_bg_blur_mask(ui, cmd);

    struct ui_bg_stash stash = { .active = mask == NULL || (mask->bounds.size.x > 0 && mask->bounds.size.y > 0) };
    struct ui_transient_tex release = ui_transient_tex_none;

    blur_chain->count = 0;
    chain->pass_count = 0;

    if (stash.active) {
        stash.capture = ui_bg_blur_cmd_capture_rect(cmd);
        uint2 capture_size = to_uint2_from_int2(max_int2(stash.capture.size, make_int2(1, 1)));

        struct ui_transient_tex src = stack[0].tex;
        uint2 src_offset = to_uint2_from_int2(stash.capture.origin - stack[0].rect.origin);
        bool src_owned = stack_count > 1;
        if (src_owned) {
            src = ui_bg_blur_snapshot_layers(ui, stash.capture, mod->ctx.active_scissor, stack, stack_count);
            src_offset = make_uint2(0, 0);
            release = src;
        }

        float radius = cmd->draw_background_blur.radius;

        if (cmd->draw_background_blur.glass && mask != NULL) {
            float level_max = ui_glass_pyramid_level_max(mask);
            uint32_t levels = min_uint32((uint32_t)ceilf(level_max), ui_blur_pyramid_max_levels);
            struct ui_blur_pyramid_tex pyr = ui_blur_pyramid_reserve(ui, capture_size.x, capture_size.y, levels, ui_glass_capture_log2);
            stash.glass.level_count = min_uint32(levels, pyr.mips - 1);
            stash.glass.level_max = min_float(level_max, (float)stash.glass.level_count);
            stash.glass.valid = true;
            ui_blur_pyramid_prepare(ui, slot, pyr, ui_transient_texture(ui, src), ui_transient_tex_size(ui, src), src_offset + ui_transient_tex_origin(ui, src), stash.glass.level_count, 0, chain);
            *inout_max_stages = max_uint32(*inout_max_stages, chain->pass_count);
            stash.scratch = pyr.tex;
            stash.scratch_level = 1;
        } else if (radius > 0.0f) {
            stash.scratch_level = ui_blur_coarse_level(radius, capture_size);
            uint2 dims = ui_blur_coarse_size(capture_size, stash.scratch_level);
            uint32_t channel = src_owned && ui_transient_tex_channel(ui, src) == 0 ? ui_transient_channel_blur_out : 0;
            stash.scratch = ui_transient_reserve_storage_channel(ui, gpu_format_rgba16f, dims.x, dims.y, channel);
            ui_blur_chain_prepare_region(ui, slot, src, src_offset, capture_size, stash.scratch, radius, make_float4(1.0f, 1.0f, 1.0f, 1.0f), false, blur_chain);
            *inout_max_stages = max_uint32(*inout_max_stages, blur_chain->count);
        } else if (src_owned) {
            stash.scratch = src;
            release = ui_transient_tex_none;
        } else {
            stash.scratch = ui_bg_blur_snapshot_layers(ui, stash.capture, stash.capture, stack, 1);
        }
    }

    ui_arr_bg_stash_append(mod->bg_stashes, stash);
    cmd->draw_background_blur.stash = ui_arr_bg_stash_get_count(mod->bg_stashes);
    return release;
}

static void ui_bg_blur_composite_stash(struct ui* ui, struct ui_render_command* cmd) {
    struct ui_bg_stash* stash = ui_arr_bg_stash_get_ptr(ui->render->bg_stashes, cmd->draw_background_blur.stash - 1);

    if (stash->active == false) {
        return;
    }

    ui_flush(ui);

    const struct ui_render_scene* mask = ui_bg_blur_mask(ui, cmd);
    const struct ui_glass_pyramid* glass = stash->glass.valid ? &stash->glass : NULL;
    float opacity = cmd->draw_background_blur.opacity;

    struct cr_rect_int32 saved_scissor = ui_push_scissor(ui, cmd->draw_background_blur.bounds);

    if (mask != NULL && mask->use_sdf) {
        struct ui_scene_draw draw = ui_bg_blur_scene_draw(cmd);
        ui_draw_scene_sdf(ui, mask, &draw, cmd->draw_background_blur.prepared, &stash->scratch, stash->scratch_level, glass, stash->capture);
    } else if (mask != NULL) {
        ui_draw_scene_tex_mask(ui, mask, opacity, cmd->draw_background_blur.band, stash->scratch, stash->scratch_level, stash->capture);
        ui_flush(ui);
    } else {
        int2 start = stash->capture.origin;
        ui_add_tex_quad(ui, start, start + max_int2(stash->capture.size, make_int2(1, 1)), stash->scratch, stash->scratch_level, make_float4(opacity, opacity, opacity, opacity));
        ui_flush(ui);
    }

    ui_pop_scissor(ui, saved_scissor);

    ui_transient_release(ui, stash->scratch);
}

static struct cr_rect_int32 ui_bg_blur_extent(struct ui* ui, const struct ui_render_command* cmd) {
    struct cr_rect_int32 extent = ui_bg_blur_cmd_capture_rect(cmd);

    const struct ui_render_scene* mask = ui_bg_blur_mask(ui, cmd);
    if (mask != NULL && mask->bounds.size.x > 0 && mask->bounds.size.y > 0) {
        extent = cr_rect_int32_union(extent, mask->bounds);
    }

    return extent;
}

static uint32_t ui_collect_bg_blur_run(struct ui* ui, struct ui_render_command* commands, uint32_t start, uint32_t end, uint32_t* out_run) {
    uint32_t count = 0;

    struct cr_rect_int32 extents[ui_bg_blur_run_capacity];

    for (uint32_t j = start; j < end && count < ui_bg_blur_run_capacity; j++) {
        struct ui_render_command* cmd = &commands[j];

        if (cmd->type == ui_render_command_type_none || cmd->type == ui_render_command_type_set_scissor) {
            continue;
        }
        if (cmd->type != ui_render_command_type_draw_background_blur || cmd->draw_background_blur.container != 0) {
            break;
        }

        struct cr_rect_int32 extent = ui_bg_blur_extent(ui, cmd);

        for (uint32_t k = 0; k < count; k++) {
            struct cr_rect_int32 x = cr_rect_int32_intersect(extents[k], extent);
            if (x.size.x > 0 && x.size.y > 0) {
                return count;
            }
        }

        extents[count] = extent;
        out_run[count++] = j;
    }

    return count;
}

static uint32_t ui_collect_bg_container(const struct ui_render_command* commands, uint32_t command_count, uint32_t container, uint32_t first_index, uint32_t* out_run) {
    uint32_t count = 0;

    for (uint32_t j = first_index; j < command_count; j++) {
        if (commands[j].type != ui_render_command_type_draw_background_blur || commands[j].draw_background_blur.container != container) {
            continue;
        }
        assert(count < ui_bg_blur_run_capacity);
        out_run[count++] = j;
    }

    return count;
}

static void ui_bg_blur_execute_run(struct ui* ui, const uint32_t* run, uint32_t run_count, const struct ui_pass_state* stack, uint32_t stack_count) {
    ui_end_pass(ui);

    struct ui_blur_pyramid_chain chains[ui_bg_blur_run_capacity];
    struct ui_blur_chain blur_chains[ui_bg_blur_run_capacity];
    struct ui_transient_tex releases[ui_bg_blur_run_capacity];
    uint32_t max_stages = 0;

    for (uint32_t k = 0; k < run_count; k++) {
        releases[k] = ui_bg_blur_prepare(ui, run[k], stack, stack_count, &blur_chains[k], &chains[k], &max_stages);
    }

    ui_blur_batch_encode(ui, "ui bg blur", blur_chains, chains, run_count, max_stages);

    for (uint32_t k = 0; k < run_count; k++) {
        ui_transient_release(ui, releases[k]);
    }

    ui_resume_pass(ui);
}

static void ui_encode_prepared_scenes(struct ui* ui) {
    struct ui_render* mod = ui->render;

    uint32_t frame_count = ui_arr_prepared_frame_get_count(mod->prepared_frames);
    struct ui_prepared_frame* frames = ui_arr_prepared_frame_get_data(mod->prepared_frames);

    uint32_t encode_count = 0;
    for (uint32_t i = 0; i < frame_count; i++) {
        encode_count += frames[i].frame.analytic == false;
    }
    if (encode_count == 0) {
        return;
    }

    struct gpu_command_buffer* command_buffer = mod->ctx.slot->command_buffer;
    bool concurrent = encode_count > 1;

    struct gpu_compute_pass* pass = concurrent ? gpu_command_buffer_begin_compute_concurrent(command_buffer, "ui sdf top mid") : gpu_command_buffer_begin_compute(command_buffer, "ui sdf top mid");
    for (uint32_t i = 0; i < frame_count; i++) {
        ui_sdf_encode_top(mod->sdf, pass, &frames[i].frame);
    }
    if (concurrent) {
        gpu_compute_pass_barrier(pass);
    }
    for (uint32_t i = 0; i < frame_count; i++) {
        ui_sdf_encode_mid(mod->sdf, pass, &frames[i].frame);
    }
    gpu_compute_pass_end(pass);

    pass = concurrent ? gpu_command_buffer_begin_compute_concurrent(command_buffer, "ui sdf refine") : gpu_command_buffer_begin_compute(command_buffer, "ui sdf refine");
    for (uint32_t i = 0; i < frame_count; i++) {
        ui_sdf_encode_refine(mod->sdf, pass, &frames[i].frame);
    }
    gpu_compute_pass_end(pass);
}

static void ui_composite_bake_execute(struct ui* ui, struct ui_render_command* cmd, struct ui_render_command* push_cmd) {
    struct ui_render* mod = ui->render;

    ui_end_pass(ui);

    struct ui_blur_chain shadow_chain;
    struct ui_transient_tex shadow_release = ui_drop_shadow_prepare(ui, cmd, &shadow_chain);
    ui_blur_chain_run(ui, &shadow_chain, shadow_release);

    struct ui_transient_tex layer = push_cmd->push_layer.tex;
    struct cr_rect_int32 rect = ui_layer_rect(push_cmd);
    struct ui_transient_tex combined = ui_transient_reserve(ui, mod->ctx.compositing_format, (uint32_t)rect.size.x, (uint32_t)rect.size.y);

    struct ui_pass_save saved = ui_begin_scratch_pass(ui, combined, rect, rect);

    int2 shadow_start = rect.origin + cmd->composite_layer.shadow_offset;
    ui_add_tex_quad(ui, shadow_start, shadow_start + rect.size, cmd->composite_layer.shadow_tex, cmd->composite_layer.shadow_tex_level, make_float4(1.0f, 1.0f, 1.0f, 1.0f));
    ui_add_tex_quad(ui, rect.origin, rect.origin + rect.size, layer, push_cmd->push_layer.tex_level, make_float4(1.0f, 1.0f, 1.0f, 1.0f));

    ui_end_scratch_pass(ui, &saved);

    ui_transient_release(ui, cmd->composite_layer.shadow_tex);
    ui_transient_release(ui, layer);

    push_cmd->push_layer.tex = combined;
    push_cmd->push_layer.tex_level = 0;

    if (cmd->composite_layer.blur_radius > 0.0f) {
        struct ui_blur_chain blur_chain;
        struct ui_transient_tex blur_release = ui_layer_blur_prepare(ui, cmd, &blur_chain);
        ui_blur_chain_run(ui, &blur_chain, blur_release);
    }

    ui_resume_pass(ui);
}

static void ui_execute_composite_layer(struct ui* ui, struct ui_render_command* cmd) {
    struct ui_render* mod = ui->render;
    struct ui_render_command* push_cmd = ui_arr_render_command_get_ptr(mod->commands, cmd->composite_layer.push_cmd_index);

    bool shadow = ui_composite_has_shadow(cmd);
    bool bake = ui_composite_bake(cmd, push_cmd);

    if (bake) {
        ui_composite_bake_execute(ui, cmd, push_cmd);
    } else if ((shadow || cmd->composite_layer.blur_radius > 0.0f) && cmd->composite_layer.prepass == false) {
        ui_end_pass(ui);

        struct ui_blur_chain chain;
        struct ui_transient_tex release = ui_composite_blur_prepare(ui, cmd, &chain);
        ui_blur_chain_run(ui, &chain, release);

        ui_resume_pass(ui);
    }

    if (shadow && bake == false) {
        ui_draw_shadow_quad(ui, cmd);
    }

    struct ui_transient_tex layer = push_cmd->push_layer.tex;
    struct cr_rect_int32 rect = ui_layer_rect(push_cmd);
    float opacity = push_cmd->push_layer.opacity;

    ui_flush(ui);

    struct cr_rect_int32 saved_scissor = ui_push_scissor(ui, push_cmd->push_layer.bounds);

    if (cmd->composite_layer.clip_scene_index != ui_scene_index_invalid) {
        const struct ui_render_scene* mask = ui_arr_render_scene_get_ptr(mod->scenes, cmd->composite_layer.clip_scene_index);

        if (mask->use_sdf) {
            struct ui_scene_draw draw = { .bg_opacity = opacity };
            ui_draw_scene_sdf(ui, mask, &draw, cmd->composite_layer.clip_prepared, &layer, push_cmd->push_layer.tex_level, NULL, rect);
        } else {
            ui_draw_scene_tex_mask(ui, mask, opacity, false, layer, push_cmd->push_layer.tex_level, rect);
            ui_flush(ui);
        }
    } else {
        ui_add_tex_quad(ui, rect.origin, rect.origin + rect.size, layer, push_cmd->push_layer.tex_level, make_float4(opacity, opacity, opacity, opacity));
        ui_flush(ui);
    }

    ui_pop_scissor(ui, saved_scissor);

    ui_transient_release(ui, layer);
}

static void ui_execute_draw_text(struct ui* ui, struct ui_render_command* cmd, double to_px_from_pt) {
    struct ui_render* mod = ui->render;
    int2 local_start_266 = cmd->draw_text.start_266 - make_int2(mod->ctx.rect.origin.x << 6, mod->ctx.rect.origin.y << 6);

    const struct ui_layer* layer = cmd->draw_text.layer;
    struct ui_text_paint paint = {
        .transform = ui_transform_pass_local(cmd->draw_text.transform, mod->ctx.rect.origin),
    };

    if (layer != NULL) {
        float layer_opacity = ui_layer_opacity_value(layer);
        paint.override_color = true;

        if (layer->type == ui_layer_type_solid) {
            paint.color = ui_to_compositing_from_color(layer->solid.color) * layer_opacity;
        } else {
            assert(layer->type == ui_layer_type_gradient);
            float2 box_origin = to_float2_from_int2(local_start_266) * (1.0f / 64.0f);
            float2 box_size = max_float2(to_float2_from_int2(cmd->draw_text.size_266) * (1.0f / 64.0f), make_float2(1.0f, 1.0f));

            paint.color = make_float4(1.0f, 1.0f, 1.0f, 1.0f);
            paint.tint_mode = 1u;
            paint.tint_color0 = ui_to_compositing_from_color(layer->gradient.color_start) * layer_opacity;
            paint.tint_color1 = ui_to_compositing_from_color(layer->gradient.color_end) * layer_opacity;
            ui_scene_gradient_dir_(layer->gradient.direction, box_origin, box_size, &paint.grad_dir, &paint.grad_off);

            if (is_equal_float3x2(paint.transform, float3x2_identity) == false) {
                float3x2 inverse = inverse_float3x2(paint.transform);
                float2 dir = paint.grad_dir;
                paint.grad_dir = make_float2(inverse.columns[0].x * dir.x + inverse.columns[0].y * dir.y,
                                             inverse.columns[1].x * dir.x + inverse.columns[1].y * dir.y);
                paint.grad_off += inverse.columns[2].x * dir.x + inverse.columns[2].y * dir.y;
            }
        }
    }

    struct cr_rect_int32 clip = cmd->draw_text.clip;
    clip.origin -= mod->ctx.rect.origin;

    ui_set_text_paint(ui, &paint);
    ui_text_draw(ui, cmd->draw_text.node, to_px_from_pt, local_start_266, cmd->draw_text.size_266.x, clip, mod->ctx.slot->serial);
}

static void ui_execute_range(struct ui* ui, uint32_t start_index, uint32_t end_index, struct ui_pass_state* stack, uint32_t stack_count, double to_px_from_pt) {
    struct ui_render* mod = ui->render;
    struct ui_render_command* commands = ui_arr_render_command_get_data(mod->commands);

    for (uint32_t i = start_index; i < end_index; i++) {
        struct ui_render_command* cmd = &commands[i];

        switch (cmd->type) {
        case ui_render_command_type_none: break;

        case ui_render_command_type_push_layer: {
            if (cmd->push_layer.prepass) {
                i = cmd->push_layer.pop_cmd_index;
                break;
            }

            ui_end_pass(ui);

            struct cr_rect_int32 rect = ui_layer_rect(cmd);

            if (cmd->push_layer.reserved == false) {
                cmd->push_layer.tex = ui_transient_reserve(ui, mod->ctx.compositing_format, (uint32_t)rect.size.x, (uint32_t)rect.size.y);
            }

            assert(stack_count < ui_pass_stack_capacity);
            stack[stack_count++] = ui_pass_state_make(ui, cmd->push_layer.tex, rect, cmd->push_layer.opacity);
            ui_enter_pass_state(ui, &stack[stack_count - 1]);

            ui_begin_region_pass(ui, cmd->push_layer.tex, true);
        } break;

        case ui_render_command_type_pop_layer: {
            ui_end_pass(ui);

            stack_count--;
            ui_enter_pass_state(ui, &stack[stack_count - 1]);

            ui_begin_region_pass(ui, stack[stack_count - 1].tex, false);
        } break;

        case ui_render_command_type_draw_background_blur: {
            if (cmd->draw_background_blur.stash == 0) {
                uint32_t run[ui_bg_blur_run_capacity];
                uint32_t run_count;

                if (cmd->draw_background_blur.container != 0) {
                    run_count = ui_collect_bg_container(commands, ui_arr_render_command_get_count(mod->commands), cmd->draw_background_blur.container, i, run);
                } else if (stack_count == 1) {
                    run_count = ui_collect_bg_blur_run(ui, commands, i, end_index, run);
                } else {
                    run[0] = i;
                    run_count = 1;
                }

                ui_bg_blur_execute_run(ui, run, run_count, stack, stack_count);
            }

            assert(cmd->draw_background_blur.stash != 0);
            ui_bg_blur_composite_stash(ui, cmd);
        } break;

        case ui_render_command_type_composite_layer: {
            ui_execute_composite_layer(ui, cmd);
        } break;

        case ui_render_command_type_draw_scene: {
            struct ui_render_scene* scene = ui_arr_render_scene_get_ptr(mod->scenes, cmd->draw_scene.scene_index);

            if (scene->use_sdf) {
                struct ui_scene_draw draw = ui_scene_draw_resolve(scene, cmd->draw_scene.fill_layer, cmd->draw_scene.outline_layer);
                ui_draw_scene_sdf(ui, scene, &draw, cmd->draw_scene.prepared, NULL, 0, NULL, (struct cr_rect_int32) { 0 });
            } else {
                if (cmd->draw_scene.fill_layer != ui_scene_layer_invalid) {
                    ui_draw_scene_cheap(ui, scene, ui_scene_fill_layer(scene, cmd->draw_scene.fill_layer), false);
                }
                if (cmd->draw_scene.outline_layer != ui_scene_layer_invalid && scene->outline_thickness > 0.0f) {
                    ui_draw_scene_cheap(ui, scene, ui_scene_outline_layer(scene, cmd->draw_scene.outline_layer), true);
                }
            }
        } break;

        case ui_render_command_type_set_scissor: {
            struct cr_rect_int32 rect = cmd->set_scissor.rect;

            if (any_int2(rect.origin != mod->ctx.active_scissor.origin) ||
                any_int2(rect.size != mod->ctx.active_scissor.size)) {
                ui_flush(ui);
                ui_set_active_scissor(ui, rect);
                ui_apply_scissor(ui);
            }
        } break;

        case ui_render_command_type_draw_text: {
            ui_execute_draw_text(ui, cmd, to_px_from_pt);
        } break;

        case ui_render_command_type_draw_custom: {
            ui_custom_on_render_ctx = ui;
            mod->ctx.custom_transform = cmd->draw_custom.transform;

            struct ui_custom_on_render on_render = {
                .draw_solid = ui_custom_draw_solid,
                .draw_graphic = ui_custom_draw_graphic,
                .user = cmd->draw_custom.desc.user,
                .origin_px = cmd->draw_custom.rect_px.origin,
                .size_px = cmd->draw_custom.rect_px.size,
            };
            cmd->draw_custom.desc.on_render(&on_render);
        } break;
        }
    }
}

static void ui_execute_pass(struct ui* ui, struct ui_pass_state base, uint32_t start_index, uint32_t end_index, double to_px_from_pt) {
    struct ui_pass_state stack[ui_pass_stack_capacity];

    stack[0] = base;
    ui_enter_pass_state(ui, &stack[0]);

    ui_begin_region_pass(ui, base.tex, true);

    ui_execute_range(ui, start_index, end_index, stack, 1, to_px_from_pt);

    ui_end_pass(ui);
}

#define ui_prepass_blur_batch_capacity 16

static void ui_execute_prepass_layers(struct ui* ui, double to_px_from_pt) {
    struct ui_render* mod = ui->render;

    uint32_t level_count = mod->ctx.prepass_level_count;

    uint32_t pass_count = ui_arr_prepass_item_get_count(mod->prepass_passes);
    uint32_t blur_count = ui_arr_prepass_item_get_count(mod->prepass_blurs);
    struct ui_prepass_item* passes = ui_arr_prepass_item_get_data(mod->prepass_passes);
    struct ui_prepass_item* blurs = ui_arr_prepass_item_get_data(mod->prepass_blurs);

    for (uint32_t d = level_count; d-- > 0;) {
        for (uint32_t p = 0; p < pass_count; p++) {
            if (passes[p].level == d) {
                struct ui_render_command* cmd = ui_arr_render_command_get_ptr(mod->commands, passes[p].cmd_index);
                struct ui_pass_state base = ui_pass_state_make(ui, cmd->push_layer.tex, ui_layer_rect(cmd), cmd->push_layer.opacity);

                ui_set_active_scissor(ui, passes[p].scissor);
                ui_execute_pass(ui, base, passes[p].cmd_index + 1, cmd->push_layer.pop_cmd_index, to_px_from_pt);
            }
        }

        uint32_t b = 0;
        while (b < blur_count) {
            struct ui_blur_chain chains[ui_prepass_blur_batch_capacity];
            struct ui_transient_tex released[ui_prepass_blur_batch_capacity];
            uint32_t batch_count = 0;
            uint32_t max_stages = 0;

            for (; b < blur_count && batch_count < ui_prepass_blur_batch_capacity; b++) {
                if (blurs[b].level != d) {
                    continue;
                }

                struct ui_render_command* cmd = ui_arr_render_command_get_ptr(mod->commands, blurs[b].cmd_index);
                assert(cmd->type == ui_render_command_type_composite_layer && cmd->composite_layer.prepass);

                released[batch_count] = ui_composite_blur_prepare(ui, cmd, &chains[batch_count]);
                max_stages = max_uint32(max_stages, chains[batch_count].count);
                batch_count++;
            }

            ui_blur_batch_encode(ui, "ui layer blur", chains, NULL, batch_count, max_stages);

            for (uint32_t r = 0; r < batch_count; r++) {
                ui_transient_release(ui, released[r]);
            }
        }
    }
}

static enum gpu_format ui_compositing_format_resolve(const struct ui_render_desc* desc, enum gpu_format target_format) {
    cr_check(desc->compositing_format != ui_pixel_format_r8, "r8 is not a compositing format");
    enum gpu_format requested_format = desc->compositing_format == ui_pixel_format_invalid
        ? target_format
        : desc->compositing_format == ui_pixel_format_rgba16f ? gpu_format_rgba16f : gpu_format_bgra8;
    return requested_format == gpu_format_bgra8 ? gpu_format_bgra8 : gpu_format_rgba16f;
}

static bool ui_direct_eligible(const struct ui_render_desc* desc, enum gpu_format target_format) {
    struct ui_convert output_convert = ui_convert_make(ui_color_space_srgb, desc->output_color_space);
    return desc->premultiplied
        && ui_compositing_format_resolve(desc, target_format) == target_format
        && output_convert.needed == false;
}

bool ui_render_direct_to_target(struct ui_render_desc desc, enum ui_pixel_format target_format, bool target_sampleable) {
    cr_check(target_format == ui_pixel_format_bgra8 || target_format == ui_pixel_format_rgba16f, "render targets are bgra8 or rgba16f");
    return target_sampleable && ui_direct_eligible(&desc, target_format == ui_pixel_format_rgba16f ? gpu_format_rgba16f : gpu_format_bgra8);
}

static void ui_encode_final_pass(struct ui* ui, const struct ui_render_desc* desc, uint2 target_size, enum gpu_format target_format, struct ui_transient_tex intermediate) {
    struct ui_render* mod = ui->render;
    struct ui_slot* slot = mod->ctx.slot;
    struct ui_convert output_convert = ui_convert_make(ui_color_space_srgb, desc->output_color_space);

    uint32_t globals_offset;
    struct ui_globals* globals = ui_alloc_uniforms(ui, sizeof(*globals), &globals_offset);
    globals->to_clip_from_px_scale = 2.0f / to_float2_from_uint2(target_size);
    globals->to_clip_from_px_offset = make_float2(0.0f, 0.0f);

    ui_encode_pass(ui, slot->target, true, "ui target");
    gpu_render_pass_set_scissor(slot->pass, 0, 0, target_size.x, target_size.y);

    uint32_t vertices_offset;
    struct ui_vertex_final* vertices = ui_alloc_vertices(ui, 6 * sizeof(*vertices), sizeof(*vertices), &vertices_offset);

    float2 size = to_float2_from_uint2(target_size);
    float2 uv_end = make_float2((float)intermediate.w, (float)intermediate.h) / to_float2_from_uint2(ui_transient_tex_size(ui, intermediate));

    for (uint32_t i = 0; i < 6; i++) {
        vertices[i].pos_px = to_int2_from_float2(ui_quad_corner(make_float2(0.0f, 0.0f), size, i));
        vertices[i].uv = ui_quad_corner(make_float2(0.0f, 0.0f), uv_end, i);
    }

    uint32_t uniforms_offset;
    struct ui_uniforms_final* uniforms = ui_alloc_uniforms(ui, sizeof(*uniforms), &uniforms_offset);
    memset(uniforms, 0, sizeof(*uniforms));

    if (output_convert.needed) {
        ui_convert_write(&uniforms->flags, &uniforms->cs_col_x, &uniforms->cs_col_y, &uniforms->cs_col_z, &output_convert);
    }
    if (mod->ctx.compositing_format != gpu_format_bgra8 || output_convert.needed) {
        uniforms->flags |= ui_format_dither_flag(ui, target_format);
    }
    if (desc->premultiplied) {
        uniforms->flags |= ui_graphic_flag_premultiplied;
    }

    gpu_render_pass_set_pipeline(slot->pass, ui_format_pipeline(ui, target_format, ui_pipeline_final));
    gpu_render_pass_set_bind_group(slot->pass, 1, ui_transient_bind_group(ui, intermediate), NULL, 0);
    gpu_render_pass_set_bind_group(slot->pass, 0, slot->bind_group, (uint32_t[]){ globals_offset, uniforms_offset }, 2);
    gpu_render_pass_draw(slot->pass, vertices_offset / sizeof(*vertices), 6);

    gpu_render_pass_end(slot->pass);
}

void ui_render(struct ui_node root, struct gpu_command_buffer* command_buffer, struct gpu_texture* target, struct ui_render_desc desc) {
    struct ui* ui = ui_ctx();
    cr_check(command_buffer != NULL, "ui_render needs a command buffer");
    struct ui_node_backing* root_node = ui_node_check(ui, root, "the root");

    if (target == NULL) {
        return;
    }

    uint2 target_size = gpu_texture_get_size(target);

    double to_px_from_pt = ui->to_px_from_pt;
    cr_check(to_px_from_pt == root_node->layout_to_px_from_pt, "ui_render runs at a different pixel scale than the layout of this root");

    if (target_size.x == 0 || target_size.y == 0) {
        return;
    }

    enum gpu_format target_format = gpu_texture_get_format(target);

    struct ui_render* mod = ui->render;
#if cr_macos || cr_ios
    gpu_device_wait(ui->device, mod->pipelines_serial);
#endif
    cr_check(gpu_device_is_completed(ui->device, mod->pipelines_serial), "the ui pipelines are still compiling; render once the serial of gpu_device_get_pipelines_serial has completed");
    memset(&mod->ctx, 0, sizeof(mod->ctx));
    mod->ctx.compositing_format = ui_compositing_format_resolve(&desc, target_format);
    mod->ctx.dither = desc.dither;
    ui_arr_render_command_remove_all(mod->commands);
    ui_arr_render_scene_remove_all(mod->scenes);
    ui_arr_scene_shape_remove_all(mod->scene_shapes);
    ui_arr_prepared_frame_remove_all(mod->prepared_frames);
    ui_arr_prepared_scene_remove_all(mod->prepared_scenes);
    ui_arr_prepass_item_remove_all(mod->prepass_passes);
    ui_arr_prepass_item_remove_all(mod->prepass_blurs);
    ui_arr_bg_stash_remove_all(mod->bg_stashes);
    mod->bg_container_count = 0;

    struct ui_slot* slot = ui_slot_begin(ui, command_buffer, target);
    mod->ctx.slot = slot;

    struct cr_rect_int32 initial_scissor = { .size = to_int2_from_uint2(target_size) };

    ui_build_root(ui, root_node, initial_scissor, to_px_from_pt);

    bool direct = ui_direct_eligible(&desc, target_format) && (gpu_texture_get_usage(target) & gpu_texture_usage_sample) != 0;
    mod->ctx.direct = direct;

    struct ui_transient_tex intermediate = direct
        ? ui_transient_wrap_target(ui, target, target_size)
        : ui_transient_reserve(ui, mod->ctx.compositing_format, target_size.x, target_size.y);

    ui_plan_merge_outlines(ui);
    ui_plan_elide_layers(ui);
    ui_plan_merge_glass(ui);
    ui_plan_select_sdf(ui);
    ui_plan_resources(ui, initial_scissor);
    ui_encode_prepared_scenes(ui);
    ui_execute_prepass_layers(ui, to_px_from_pt);

    struct ui_pass_state base = ui_pass_state_make(ui, intermediate, initial_scissor, 1.0f);

    mod->ctx.active_scissor = initial_scissor;
    ui_execute_pass(ui, base, 0, ui_arr_render_command_get_count(mod->commands), to_px_from_pt);

    uint32_t prepared_count = ui_arr_prepared_scene_get_count(mod->prepared_scenes);
    struct ui_prepared_scene* prepared = ui_arr_prepared_scene_get_data(mod->prepared_scenes);
    for (uint32_t i = 0; i < prepared_count; i++) {
        if (prepared[i].composited == false) {
            ui_release_prepared_frame(ui, ui_arr_prepared_frame_get_ptr(mod->prepared_frames, prepared[i].frame_index));
        }
    }

    if (direct == false) {
        ui_encode_final_pass(ui, &desc, target_size, target_format, intermediate);
    }

    ui_transient_release(ui, intermediate);
    ui_transient_frame_end(ui);

    ui_transient_unwrap_target(ui, slot);
    ui_slot_end(ui, slot);
}
