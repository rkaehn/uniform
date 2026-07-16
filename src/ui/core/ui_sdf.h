#pragma once

#include "ui_core.h"
#include "ui_transient.h"
#include "ui_sdf_shared.h"
#include "gpu.h"

struct ui_path;

enum ui_sdf_prim_type {
    ui_sdf_prim_type_circle,
    ui_sdf_prim_type_rect,
    ui_sdf_prim_type_path,
};

struct ui_sdf_prim {
    enum ui_sdf_prim_type type;
    union {
        struct {
            double2 center;
            double radius;
        } circle;
        struct {
            double2 origin;
            double width;
            double height;
            double radius;
            bool continuous;
        } rect;
        struct {
            const struct ui_path* path;
            double2 offset;
            double scale;
            double band_half;
        } path;
    };
};

struct ui_scene_shape {
    enum ui_shape_type type;
    float2 origin;
    float2 size;
    float4 radius;
    enum ui_corner_profile profile;
    float coverage;
    const struct ui_path* path;
    float2 path_offset;
    float path_scale;
    float path_half;
};

enum ui_sdf_scene_pipeline {
    ui_sdf_scene_pipeline_solid,
    ui_sdf_scene_pipeline_content,
    ui_sdf_scene_pipeline_glass,

    ui_sdf_scene_pipeline_count,
};

struct ui_sdf {
    struct cr_mem* mem;
    struct gpu_device* device;
    struct gpu_sampler* sampler_mip;
    struct gpu_bind_layout* layout_top;
    struct gpu_bind_layout* layout_mid;
    struct gpu_bind_layout* layout_refine;
    struct gpu_bind_layout* layout_scene;
    struct gpu_compute_pipeline* pipeline_top;
    struct gpu_compute_pipeline* pipeline_mid;
    struct gpu_compute_pipeline* pipeline_refine;
    struct gpu_render_pipeline* pipelines_scene[ui_sdf_scene_pipeline_count];
    struct gpu_render_pipeline* pipelines_scene_c8[ui_sdf_scene_pipeline_count];
    struct gpu_texture* dummy_content;
};

struct ui_sdf_frame {
    bool analytic;
    uint32_t width;
    uint32_t height;
    uint32_t width_top;
    uint32_t height_top;
    uint32_t width_mid;
    uint32_t height_mid;

    uint32_t sdf_params_offset;
    uint32_t prims_offset;
    uint32_t seg_ae_offset;
    uint32_t seg_ext_offset;
    uint32_t chunks_offset;
    uint32_t supers_offset;
    uint32_t geometry_size;

    uint32_t tiles_offset;
    uint32_t prim_mid_offset;
    uint32_t top_offset;
    uint32_t scratch_size;

    struct gpu_buffer* geometry;
    struct gpu_buffer* scratch;
    bool transient;
    struct ui_transient_tex mid_tex;
    struct ui_transient_tex fine_tex;
    struct gpu_texture* mid;
    struct gpu_texture* fine;
    uint2 mid_page;

    struct gpu_bind_group* group_top;
    struct gpu_bind_group* group_mid;
    struct gpu_bind_group* group_refine;

    struct ui_sdf_params sdf_params;
};

struct ui_sdf_geometry {
    const struct ui_sdf_prim* prims;
    uint32_t prim_count;
    uint32_t combine;
    float smoothing;
    float band_lo;
    float band_hi;
    double flatten_epsilon;
    double flatten_max_angle;
    double2 to_px_offset;
    bool exact_mid;
};

struct ui_sdf_scene_desc {
    const struct ui_scene_shape* shapes;
    uint32_t shape_count;
    uint32_t combine;
    float smoothing;
    double2 to_px_offset;
    struct ui_sdf_scene_params params;
    struct ui_sdf_scene_analytic_prims analytic;
    struct gpu_texture* content;
    bool draw_color;
    bool draw_glass;
    struct gpu_texture* out_texture;
    uint2 out_scissor_origin;
    uint2 out_scissor_size;
};

struct ui_sdf* ui_sdf_create(struct cr_mem* mem, struct gpu_device* device, struct gpu_library* library);
void ui_sdf_destroy(struct ui_sdf* sdf);

void ui_sdf_frame_reserve(struct ui_sdf* sdf, struct ui* ui, struct ui_sdf_frame* frame, uint32_t width, uint32_t height, bool shared_textures);
void ui_sdf_frame_build(struct ui_sdf* sdf, struct ui* ui, struct ui_sdf_frame* frame,
                        const struct ui_sdf_geometry* geometry);
void ui_sdf_frame_destroy(struct ui_sdf* sdf, struct ui* ui, struct ui_slot* slot, struct ui_sdf_frame* frame);

void ui_sdf_encode_top(struct ui_sdf* sdf, struct gpu_compute_pass* pass, const struct ui_sdf_frame* frame);
void ui_sdf_encode_mid(struct ui_sdf* sdf, struct gpu_compute_pass* pass, const struct ui_sdf_frame* frame);
void ui_sdf_encode_refine(struct ui_sdf* sdf, struct gpu_compute_pass* pass, const struct ui_sdf_frame* frame);

bool ui_sdf_scene_analytic(struct ui_sdf_scene_desc* desc);
void ui_sdf_scene_prepare(struct ui_sdf* sdf, struct ui* ui, struct ui_sdf_scene_desc* desc, struct ui_sdf_frame* frame);
void ui_sdf_scene_composite(struct ui_sdf* sdf, struct ui* ui, struct ui_slot* slot, struct gpu_render_pass* render_pass,
                            enum gpu_format target_format, const struct ui_sdf_frame* frame, const struct ui_sdf_scene_desc* desc);

void ui_sdf_encode_scene(struct ui_sdf* sdf, struct ui* ui, struct ui_slot* slot, struct gpu_command_buffer* command_buffer,
                         const struct ui_sdf_scene_desc* desc);
