#pragma once

#include "ui_core.h"
#include "ui_render.h"
#include "ui_shared.h"
#include "ui_slot.h"
#include "ui_path.h"
#include "ui_blur.h"
#include "ui_sdf.h"
#include "ui_transient.h"
#include <stdalign.h>
#include <string.h>
#include <math.h>

#define ui_scene_layer_invalid uint32_max

#define ui_scene_index_invalid uint32_max
#define ui_pass_stack_capacity 32
#define ui_layer_prepass_max_bytes (128ull << 20)
#define ui_transient_tex_none ((struct ui_transient_tex) { .index = ui_transient_index_target })

enum ui_pipeline {
    ui_pipeline_solid,
    ui_pipeline_solid_sdf,
    ui_pipeline_graphic,
    ui_pipeline_graphic_sdf,
    ui_pipeline_glyph,
    ui_pipeline_final,

    ui_pipeline_count,
};

enum ui_draw_type {
    ui_draw_type_none,
    ui_draw_type_solid,
    ui_draw_type_graphic,
    ui_draw_type_glyph,
};

enum ui_render_command_type {
    ui_render_command_type_none,
    ui_render_command_type_push_layer,
    ui_render_command_type_pop_layer,
    ui_render_command_type_composite_layer,
    ui_render_command_type_set_scissor,
    ui_render_command_type_draw_scene,
    ui_render_command_type_draw_text,
    ui_render_command_type_draw_custom,
    ui_render_command_type_draw_background_blur,
};

struct ui_render_scene {
    uint32_t shape_offset;
    uint32_t shape_count;
    enum ui_shape_group_op op;
    float smoothing;
    const struct ui_layer* fill_layers;
    uint32_t fill_count;
    const struct ui_layer* outline_layers;
    uint32_t outline_count;
    float outline_thickness;
    enum ui_outline_align outline_align;
    bool has_glass;
    float glass_scale;
    float glass_min_dim_pt;
    float glass_min_dim_lo_pt;
    float glass_slider;
    uint32_t glass_variant;
    struct ui_color glass_tint;
    float opacity;
    bool use_sdf;
    uint32_t prepared_frame;
    struct cr_rect_int32 node_rect;
    struct cr_rect_int32 bounds;
    float3x2 transform;
};

struct ui_scene_draw {
    const struct ui_layer* fill_layer;
    const struct ui_layer* outline_layer;
    bool band;
    bool glass;
    float bg_opacity;
};

struct ui_prepared_frame {
    struct ui_sdf_frame frame;
    uint32_t remaining;
};

struct ui_prepared_scene {
    uint32_t frame_index;
    struct ui_sdf_scene_desc desc;
    bool composited;
};

struct ui_render_command {
    enum ui_render_command_type type;
    union {
        struct {
            float opacity;
            struct cr_rect_int32 bounds;
            struct ui_transient_tex tex;
            uint32_t tex_level;
            uint32_t pop_cmd_index;
            uint32_t scene_index;
            bool scene_only;
            bool child_content;
            bool backdrop_dep;
            bool reserved;
            bool prepass;
        } push_layer;
        struct {
            uint32_t push_cmd_index;
            float blur_radius;
            int2 shadow_offset;
            float shadow_radius;
            struct ui_color shadow_color;
            struct ui_transient_tex shadow_tex;
            uint32_t shadow_tex_level;
            bool prepass;
            uint32_t clip_scene_index;
            uint32_t clip_prepared;
        } composite_layer;
        struct {
            struct cr_rect_int32 bounds;
            struct cr_rect_int32 sample;
            float radius;
            uint32_t scene_index;
            uint32_t prepared;
            uint32_t container;
            uint32_t stash;
            float opacity;
            bool band;
            bool glass;
        } draw_background_blur;
        struct {
            struct cr_rect_int32 rect;
        } set_scissor;
        struct {
            uint32_t scene_index;
            uint32_t prepared;
            uint32_t fill_layer;
            uint32_t outline_layer;
        } draw_scene;
        struct {
            struct ui_node_backing* node;
            const struct ui_layer* layer;
            int2 start_266;
            int2 size_266;
            struct cr_rect_int32 clip;
            float3x2 transform;
        } draw_text;
        struct {
            struct ui_custom_desc desc;
            float3x2 transform;
            struct cr_rect_int32 rect_px;
        } draw_custom;
    };
};

#define cr_arr ui_arr_render_command
#define cr_arr_item struct ui_render_command
#include "cr_arr.h"

#define cr_arr ui_arr_render_scene
#define cr_arr_item struct ui_render_scene
#include "cr_arr.h"

#define cr_arr ui_arr_scene_shape
#define cr_arr_item struct ui_scene_shape
#include "cr_arr.h"

#define cr_arr ui_arr_prepared_frame
#define cr_arr_item struct ui_prepared_frame
#include "cr_arr.h"

#define cr_arr ui_arr_prepared_scene
#define cr_arr_item struct ui_prepared_scene
#include "cr_arr.h"

struct ui_prepass_item {
    uint32_t cmd_index;
    uint32_t level;
    struct cr_rect_int32 scissor;
};

#define cr_arr ui_arr_prepass_item
#define cr_arr_item struct ui_prepass_item
#include "cr_arr.h"

struct ui_glass_pyramid {
    bool valid;
    uint32_t level_count;
    float level_max;
};

struct ui_bg_stash {
    bool active;
    struct ui_transient_tex scratch;
    uint32_t scratch_level;
    struct ui_glass_pyramid glass;
    struct cr_rect_int32 capture;
};

#define cr_arr ui_arr_bg_stash
#define cr_arr_item struct ui_bg_stash
#include "cr_arr.h"

struct ui_tex_uv {
    float2 origin;
    float2 size;
    float2 min;
    float2 max;
};

struct ui_batch_solid {
    float3x2 transform;
    float4 color;
    bool sdf;
};

struct ui_batch_graphic {
    float3x2 transform;
    struct gpu_bind_group* bind_group;
    float4 tint;
    uint32_t flags;
    struct ui_tex_uv uv;
    enum ui_color_space color_space;
    bool sdf;
};

struct ui_text_paint {
    bool override_color;
    float4 color;
    uint32_t tint_mode;
    float4 tint_color0;
    float4 tint_color1;
    float2 grad_dir;
    float grad_off;
    float3x2 transform;
};

struct ui_batch_glyph {
    struct gpu_bind_group* bind_group;
    struct ui_text_paint paint;
};

struct ui_render {
    struct cr_mem* mem;
    struct gpu_sampler* samplers[2][2];
    struct gpu_bind_layout* bind_layout_0;
    struct gpu_bind_layout* bind_layout_1;
    struct gpu_render_pipeline* pipelines[2][ui_pipeline_count];
    uint64_t pipelines_serial;
    struct ui_sdf* sdf;
    struct ui_blur* blur;
    struct ui_slot slots[8];
    struct ui_linear_allocator alloc_uniforms;
    struct ui_linear_allocator alloc_vertices;
    uint8_t* write_uniforms;
    uint8_t* write_vertices;
#if cr_wasm
    alignas(256) uint8_t temp_uniforms[ui_buffer_uniforms_size];
    alignas(256) uint8_t temp_vertices[ui_buffer_vertices_size];
#endif
    uint64_t layer_prepass_limit;
    struct ui_arr_render_command* commands;
    struct ui_arr_render_scene* scenes;
    struct ui_arr_scene_shape* scene_shapes;
    struct ui_arr_prepared_frame* prepared_frames;
    struct ui_arr_prepared_scene* prepared_scenes;
    struct ui_arr_prepass_item* prepass_passes;
    struct ui_arr_prepass_item* prepass_blurs;
    struct ui_arr_bg_stash* bg_stashes;
    uint32_t bg_container_count;
    struct {
        struct ui_slot* slot;
        enum ui_draw_type last_type;
        union {
            struct ui_batch_solid solid;
            struct ui_batch_graphic graphic;
            struct ui_batch_glyph glyph;
        };
        struct ui_text_paint text_paint;
        float3x2 custom_transform;
        uint32_t start_vertices_offset;
        uint32_t vertex_count;
        uint32_t globals_offset;
        uint32_t uniforms_offset;
        uint32_t vertices_offset;
        struct cr_rect_int32 rect;
        struct cr_rect_int32 active_scissor;
        struct ui_transient_tex pass_tex;
        enum gpu_format pass_format;
        enum gpu_format compositing_format;
        uint32_t prepass_level_count;
        bool pass_pending;
        bool dither;
        bool direct;
    } ctx;
};

struct ui_build_layer {
    struct ui_build_layer* parent;
    uint32_t cmd_index;
};

struct cr_rect_int32 ui_bg_blur_capture_rect(struct cr_rect_int32 bounds, float radius, struct cr_rect_int32 sample);

cr_hdef float3x2 ui_transform_pass_local(float3x2 t, int2 origin) {
    float2 o = to_float2_from_int2(origin);
    t.columns[2] = mul_float3x2_float2(t, o) - o;
    return t;
}

cr_hdef void ui_scene_shapes_bounds(const struct ui_scene_shape* shapes, uint32_t count, float2* out_min, float2* out_max) {
    float2 bounds_min = shapes[0].origin;
    float2 bounds_max = shapes[0].origin + shapes[0].size;

    for (uint32_t i = 1; i < count; i++) {
        bounds_min = min_float2(bounds_min, shapes[i].origin);
        bounds_max = max_float2(bounds_max, shapes[i].origin + shapes[i].size);
    }

    *out_min = bounds_min;
    *out_max = bounds_max;
}

cr_hdef float ui_scene_glass_min_dim_pt(float2 bounds_min, float2 bounds_max, float glass_scale) {
    float2 bounds_size = bounds_max - bounds_min;
    return min_float(bounds_size.x, bounds_size.y) / max_float(glass_scale, 1e-3f);
}

cr_hdef bool ui_scene_shape_is_plain_rect(const struct ui_scene_shape* shape) {
    if (shape->type != ui_shape_type_rect) {
        return false;
    }
    if (shape->radius.x >= 0.25f || shape->radius.y >= 0.25f || shape->radius.z >= 0.25f || shape->radius.w >= 0.25f) {
        return false;
    }
    return shape->origin.x == floorf(shape->origin.x) && shape->origin.y == floorf(shape->origin.y) &&
           shape->size.x == floorf(shape->size.x) && shape->size.y == floorf(shape->size.y);
}

cr_hdef bool ui_scene_shape_covers_rect(const struct ui_scene_shape* shape, struct cr_rect_int32 rect) {
    return ui_scene_shape_is_plain_rect(shape) &&
           all_int2(to_int2_from_float2(shape->origin) == rect.origin) &&
           all_int2(to_int2_from_float2(shape->size) == rect.size);
}

cr_hdef struct cr_rect_int32 ui_layer_rect(const struct ui_render_command* push_cmd) {
    return (struct cr_rect_int32) {
        .origin = push_cmd->push_layer.bounds.origin,
        .size = max_int2(push_cmd->push_layer.bounds.size, make_int2(1, 1)),
    };
}

cr_hdef struct ui_scene_draw ui_bg_blur_scene_draw(const struct ui_render_command* cmd) {
    return (struct ui_scene_draw) {
        .band = cmd->draw_background_blur.band,
        .glass = cmd->draw_background_blur.glass,
        .bg_opacity = cmd->draw_background_blur.opacity,
    };
}

cr_hdef struct cr_rect_int32 ui_bg_blur_cmd_capture_rect(const struct ui_render_command* cmd) {
    return ui_bg_blur_capture_rect(cmd->draw_background_blur.bounds, cmd->draw_background_blur.radius, cmd->draw_background_blur.sample);
}

cr_hdef bool ui_composite_has_shadow(const struct ui_render_command* cmd) {
    return cmd->composite_layer.shadow_color.value.w > 0.0f;
}

cr_hdef bool ui_composite_bake(const struct ui_render_command* cmd, const struct ui_render_command* push_cmd) {
    return ui_composite_has_shadow(cmd) &&
           (cmd->composite_layer.blur_radius > 0.0f || push_cmd->push_layer.opacity < 1.0f);
}

cr_hdef struct gpu_render_pipeline* ui_format_pipeline(struct ui* ui, enum gpu_format target_format, enum ui_pipeline pipeline) {
    return ui->render->pipelines[target_format == gpu_format_bgra8][pipeline];
}

cr_hdef float2 ui_quad_corner(float2 s, float2 e, uint32_t i) {
    static const uint8_t corner[6] = { 0, 1, 2, 3, 2, 1 };
    return make_float2(corner[i] & 1 ? e.x : s.x, corner[i] & 2 ? e.y : s.y);
}

cr_hdef uint32_t ui_format_dither_flag(struct ui* ui, enum gpu_format target_format) {
    return ui->render->ctx.dither && target_format == gpu_format_bgra8 ? ui_graphic_flag_dither : 0u;
}

cr_hdef struct gpu_render_pipeline* ui_pass_pipeline(struct ui* ui, enum ui_pipeline pipeline) {
    return ui_format_pipeline(ui, ui->render->ctx.pass_format, pipeline);
}

cr_hdef uint32_t ui_pass_dither_flag(struct ui* ui) {
    return ui_format_dither_flag(ui, ui->render->ctx.pass_format);
}

cr_hdef uint32_t ui_glass_variant_pack(const struct ui_layer* layer) {
    return ((uint32_t)layer->glass.variant << 1) | (uint32_t)layer->glass.appearance;
}

cr_hdef float ui_glass_slider_value(const struct ui_layer* layer) {
    return (float)clamp_double(layer->glass.slider, 0.0, 1.0);
}

cr_hdef float ui_layer_opacity_value(const struct ui_layer* layer) {
    if (layer == NULL || layer->blend.mode == ui_blend_mode_none) {
        return 1.0f;
    }
    return (float)clamp_double(layer->blend.opacity, 0.0, 1.0);
}

cr_hdef bool ui_layer_is_color(const struct ui_layer* layer) {
    return layer->type == ui_layer_type_solid ||
           layer->type == ui_layer_type_gradient ||
           layer->type == ui_layer_type_graphic;
}

cr_hdef bool ui_layer_is_backdrop(const struct ui_layer* layer) {
    return layer->type == ui_layer_type_background_blur ||
           layer->type == ui_layer_type_glass;
}

cr_hdef bool ui_layer_is_gradient(const struct ui_layer* layer) {
    return layer->type == ui_layer_type_gradient;
}

cr_hdef const struct ui_layer* ui_scene_fill_layer(const struct ui_render_scene* scene, uint32_t index) {
    return index != ui_scene_layer_invalid ? &scene->fill_layers[index] : NULL;
}

cr_hdef const struct ui_layer* ui_scene_outline_layer(const struct ui_render_scene* scene, uint32_t index) {
    return index != ui_scene_layer_invalid ? &scene->outline_layers[index] : NULL;
}

cr_hdef struct ui_scene_draw ui_scene_draw_resolve(const struct ui_render_scene* scene, uint32_t fill_layer, uint32_t outline_layer) {
    return (struct ui_scene_draw) {
        .fill_layer = ui_scene_fill_layer(scene, fill_layer),
        .outline_layer = ui_scene_outline_layer(scene, outline_layer),
        .bg_opacity = 1.0f,
    };
}

cr_hdef bool ui_layers_any(const struct ui_layer* layers, uint32_t count, bool (*predicate)(const struct ui_layer*)) {
    for (uint32_t i = 0; i < count; i++) {
        if (predicate(&layers[i])) {
            return true;
        }
    }
    return false;
}

void ui_build_root(struct ui* ui, struct ui_node_backing* root, struct cr_rect_int32 scissor, double to_px_from_pt);
void ui_plan_elide_layers(struct ui* ui);
void ui_plan_merge_glass(struct ui* ui);
void ui_plan_select_sdf(struct ui* ui);
void ui_plan_merge_outlines(struct ui* ui);
void ui_plan_resources(struct ui* ui, struct cr_rect_int32 initial_scissor);

void ui_flush(struct ui* ui);
void ui_set_text_paint(struct ui* ui, const struct ui_text_paint* paint);
void ui_require_pass(struct ui* ui);
void ui_apply_scissor(struct ui* ui);
void ui_add_tex_quad(struct ui* ui, int2 start, int2 end, struct ui_transient_tex tex, uint32_t tex_level, float4 tint);
void ui_draw_scene_cheap(struct ui* ui, const struct ui_render_scene* scene, const struct ui_layer* layer, bool outline);
void ui_draw_scene_tex_mask(struct ui* ui, const struct ui_render_scene* scene, float layer_opacity, bool band, struct ui_transient_tex tex, uint32_t tex_level, struct cr_rect_int32 tex_rect);

bool ui_scene_sdf_make_desc(struct ui* ui, const struct ui_render_scene* scene, const struct ui_scene_draw* draw,
                            bool has_content, struct cr_rect_int32 content_rect, struct cr_rect_int32 rect, struct cr_rect_int32 active_scissor,
                            struct ui_sdf_scene_desc* out_desc);
void ui_scene_gradient_dir_(enum ui_gradient_direction direction, float2 node_origin, float2 node_size, float2* out_dir, float* out_off);
void ui_scene_sdf_set_content(struct ui* ui, struct ui_sdf_scene_desc* desc, struct ui_transient_tex content_tex, struct cr_rect_int32 content_rect, uint32_t tex_level);
void ui_scene_sdf_set_glass(struct ui* ui, struct ui_sdf_scene_desc* desc, const struct ui_glass_pyramid* glass);

void ui_custom_draw_solid(float3x2 transform, struct ui_color color, uint32_t vertex_count, struct ui_custom_vertex_solid** out_vertices);
void ui_custom_draw_graphic(float3x2 transform, struct ui_graphic* graphic, struct ui_color tint, uint32_t vertex_count, struct ui_custom_vertex_graphic** out_vertices);
