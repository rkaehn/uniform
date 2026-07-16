#pragma once

#include "cr.h"
#include "ui.h"
#include "ui_input.h"
#include "ui_sdf.h"
#include "ui_slot.h"

#if cr_ios
#include <CoreGraphics/CoreGraphics.h>
#include <CoreText/CoreText.h>
#endif

void ui_test_yoga_(void);
void ui_test_perf_(uint32_t iterations);
void ui_test_shelf_(void);
void ui_test_corner_(void);

#if cr_ios
#define ui_test_scene_present(fn) fn
#define ui_test_scene_pixels(fn) fn
#else
#define ui_test_scene_present(fn) NULL
#define ui_test_scene_pixels(fn) NULL
#endif

#define ui_system_red ui_hex(0xff3b30)
#define ui_system_orange ui_hex(0xff9500)
#define ui_system_yellow ui_hex(0xffcc00)
#define ui_system_green ui_hex(0x34c759)
#define ui_system_mint ui_hex(0x00c7be)
#define ui_system_teal ui_hex(0x30b0c7)
#define ui_system_cyan ui_hex(0x32ade6)
#define ui_system_blue ui_hex(0x007aff)
#define ui_system_indigo ui_hex(0x5856d6)
#define ui_system_purple ui_hex(0xaf52de)
#define ui_system_pink ui_hex(0xff2d55)
#define ui_system_brown ui_hex(0xa2845e)
#define ui_system_gray ui_hex(0x8e8e93)
#define ui_system_gray2 ui_hex(0xaeaeb2)
#define ui_system_gray3 ui_hex(0xc7c7cc)
#define ui_system_gray4 ui_hex(0xd1d1d6)
#define ui_system_gray5 ui_hex(0xe5e5ea)
#define ui_system_gray6 ui_hex(0xf2f2f7)
#define ui_label ui_hex(0x000000)
#define ui_secondary_label ui_hex_a(0x3c3c43, 0.6f)
#define ui_tertiary_label ui_hex_a(0x3c3c43, 0.3f)
#define ui_quaternary_label ui_hex_a(0x3c3c43, 0.18f)
#define ui_separator ui_hex_a(0x3c3c43, 0.29f)
#define ui_opaque_separator ui_hex(0xc6c6c8)
#define ui_system_background ui_hex(0xffffff)
#define ui_secondary_system_background ui_hex(0xf2f2f7)
#define ui_system_fill ui_hex_a(0x787880, 0.2f)
#define ui_secondary_system_fill ui_hex_a(0x787880, 0.16f)
#define ui_tertiary_system_fill ui_hex_a(0x767680, 0.12f)
#define ui_quaternary_system_fill ui_hex_a(0x747480, 0.08f)

struct ui_test_glass_pattern_rect {
    double2 origin;
    double2 size;
    float4 color;
};

struct ui_test_capture_scene {
    char name[112];
    const char* group;
    uint16_t canvas_w;
    uint16_t canvas_h;
    float slider;
    bool regular;
    bool light;
    uint8_t kind;
    uint8_t prim_count;
    struct ui_sdf_prim prims[8];
    uint32_t union_groups[8];
    bool use_union;
    float4 background;
    float4 tint;
    int16_t pattern;
    float edr_boost;
    char shape[24];
    float md;
    float radius;
    char profile[4];
    char bg_tag[20];
    char tint_tag[20];
    char union_tag[20];
};

#define ui_test_capture_kind_glass 0u
#define ui_test_capture_kind_edr_ramp 1u

#define ui_test_style_max_layers 4
#define ui_test_style_max_nodes 8

#define ui_test_style_layer_none 0u
#define ui_test_style_layer_solid 1u
#define ui_test_style_layer_gradient 2u

struct ui_test_style_layer {
    uint32_t type;
    uint32_t direction;
    float opacity;
    float4 color0;
    float4 color1;
};

struct ui_test_style_node {
    double2 origin;
    double2 size;
    double radius;
    bool continuous;
    bool circle;
    struct ui_test_style_layer fill[ui_test_style_max_layers];
    struct ui_test_style_layer outline[ui_test_style_max_layers];
    uint32_t outline_align;
    double outline_thickness;
    bool has_shadow;
    double2 shadow_offset;
    double shadow_radius;
    float4 shadow_color;
    double blur;
    double blur_radius_swiftui;
    float node_opacity;
    double2 child_origin;
    double2 child_size;
    float4 child_color;
    bool has_child;
    bool clip;
};

#if cr_macos || cr_ios || cr_wasm

#define ui_test_scene_band_shape_none 0u
#define ui_test_scene_band_shape_circle 1u
#define ui_test_scene_band_shape_rect 2u

struct ui_test_scene_band_shape {
    uint32_t type;
    float2 origin;
    float2 size;
    float radius;
};

enum ui_test_scene_capture {
    ui_test_scene_capture_pixels = 1,
    ui_test_scene_capture_screenshot,
};

struct ui_test_scenes;
struct ui_test_scene;

struct ui_test_scene_state {
    struct ui_test_scenes* scenes;
    struct cr_mem* mem;
    void (*destroy)(struct ui_test_scene_state* state);
};

struct ui_test_scene {
    const char* label;
    const char* group;
    double2 size;
    uint32_t width;
    uint32_t height;
    float to_px_from_pt;
    float mean_threshold;
    float alarm_diff;
    uint32_t alarm_allowance;
    float tile_threshold;
    struct ui_test_scene_band_shape band_shape;
    float rim_threshold;
    float bevel_threshold;
    float interior_threshold;
    enum ui_test_scene_capture capture;
    const struct ui_test_capture_scene* cap;
    enum ui_color_space output_color_space;
    struct ui_test_scene_state* (*create)(struct ui_test_scenes* scenes, struct cr_mem* mem, const void* ctx);
    const void* ctx;
    struct ui_node (*build)(struct ui_test_scene_state* state, struct ui_node parent, double2 size, const struct ui_test_scene* scene);
    void (*pixels)(struct ui_test_scene_state* state, uint32_t width, uint32_t height, float to_px_from_pt, float* out_rgba, uint32_t bytes_per_row);
    void (*present)(struct ui_test_scene_state* state, uint32_t width, uint32_t height, float to_px_from_pt);
};

enum ui_test_sui_kind {
    ui_test_sui_kind_rect,
    ui_test_sui_kind_ellipse,
    ui_test_sui_kind_path,
};

struct ui_test_sui_item {
    enum ui_test_sui_kind kind;
    double2 origin;
    double2 size;
    double4 radius;
    bool circular;
    double2 transform_x;
    double2 transform_y;
    double2 transform_t;
    struct ui_path* path;
    float path_thickness;
    float4 fill;
    float4 stroke;
    float stroke_width;
    enum ui_outline_align stroke_align;
    float4 shadow;
    float shadow_radius;
    double2 shadow_offset;
    float opacity;
    float blur;
    uint32_t group;
    float group_opacity;
    bool group_clip;
};

struct ui_test_sui_spec {
    float4 background;
    const struct ui_test_sui_item* items;
    uint32_t item_count;
};

struct ui_test_scenes_desc {
    const uint8_t* font_data;
    uint64_t font_size;
    const uint8_t* font_ui_data;
    uint64_t font_ui_size;
    float screen_to_px_from_pt;
};

struct ui_test_scenes {
    struct cr_mem* mem;
    struct ui_test_scenes_desc desc;
    struct ui_test_scene* list;
    uint32_t count;
    struct ui_test_scene* glass_scenes;
    struct ui_test_capture_scene* glass_captures;
    uint32_t glass_count;
};

struct ui_test_scenes* ui_test_scenes_create_(struct cr_mem* mem, const struct ui_test_scenes_desc* desc);
void ui_test_scenes_destroy_(struct ui_test_scenes* scenes);

uint32_t ui_test_scene_count_(struct ui_test_scenes* scenes);
const struct ui_test_scene* ui_test_scene_get_(struct ui_test_scenes* scenes, uint32_t index);
struct ui_test_scene_state* ui_test_scene_create_(struct ui_test_scenes* scenes, struct cr_mem* mem, const struct ui_test_scene* scene);
void ui_test_scene_state_free_(struct ui_test_scene_state* state);

const uint8_t* ui_test_scenes_font_(const struct ui_test_scenes* scenes, bool ui_font, uint64_t* out_size);

void ui_test_containment_(void);
void ui_test_edr_(void);
void ui_test_field_(void);
void ui_test_text_(void);
void ui_test_text_input_(void);
void ui_test_list_(void);
void ui_test_input_(void);
void ui_test_key_(void);
void ui_test_transient_(void);
void ui_test_layout_(void);
void ui_test_render_(void);
void ui_test_atlas_(void);

float4 ui_test_palette_(uint32_t i);

const struct ui_test_scene* ui_test_scenes_health_(struct ui_test_scenes* scenes, uint32_t* out_count);
const struct ui_test_scene* ui_test_scenes_color_(struct ui_test_scenes* scenes, uint32_t* out_count);
const struct ui_test_scene* ui_test_scenes_text_(struct ui_test_scenes* scenes, uint32_t* out_count);
const struct ui_test_scene* ui_test_scenes_rect_(struct ui_test_scenes* scenes, uint32_t* out_count);
const struct ui_test_scene* ui_test_scenes_path_(struct ui_test_scenes* scenes, uint32_t* out_count);
const struct ui_test_scene* ui_test_scenes_layer_(struct ui_test_scenes* scenes, uint32_t* out_count);
const struct ui_test_scene* ui_test_scenes_shadow_(struct ui_test_scenes* scenes, uint32_t* out_count);
const struct ui_test_scene* ui_test_scenes_blur_(struct ui_test_scenes* scenes, uint32_t* out_count);
const struct ui_test_scene* ui_test_scenes_glass_(struct ui_test_scenes* scenes, uint32_t* out_count);
void ui_test_scenes_glass_build_(struct ui_test_scenes* scenes);

struct ui_node ui_test_sui_build_(struct ui_node parent, double2 size, const struct ui_test_sui_spec* spec);

#if cr_ios
void ui_test_sui_present_(const struct ui_test_sui_spec* spec, uint32_t width, uint32_t height, float to_px_from_pt);
#endif

struct ui_test_sui_scene_desc {
    uint32_t item_max;
    void (*fill)(struct ui_test_sui_spec* spec, struct ui_test_sui_item* items);
};

struct ui_test_scene_state* ui_test_sui_scene_create_(struct ui_test_scenes* scenes, struct cr_mem* mem, const void* ctx);
struct ui_node ui_test_sui_scene_build_(struct ui_test_scene_state* state, struct ui_node parent, double2 size, const struct ui_test_scene* scene);
#if cr_ios
void ui_test_sui_scene_present_(struct ui_test_scene_state* state, uint32_t width, uint32_t height, float to_px_from_pt);
#endif

#define ui_test_sui_scene(name, items_max) \
    .create = ui_test_sui_scene_create_, \
    .ctx = &(const struct ui_test_sui_scene_desc) { .item_max = items_max, .fill = name##_fill }, \
    .build = ui_test_sui_scene_build_, \
    .present = ui_test_scene_present(ui_test_sui_scene_present_)

void ui_test_group_check_(const char* group, bool ok, const char* label);

void ui_test_run_(struct gpu_device* device, void (*const* cases)(void), uint32_t case_count, void (*done)(void* user), void* user);
void ui_test_await_(uint64_t serial);
void ui_test_await_flag_(const bool* flag);
void ui_test_then_(void (*func)(void* args), void* args);
void ui_test_wake_(void);

struct ui_test_scene_render {
    uint64_t serial;
    double build_s;
    double layout_s;
    double render_s;
    bool profiled;
};

struct ui_test_scene_render ui_test_scene_render_(struct ui_test_scene_state* state, const struct ui_test_scene* scene, struct gpu_texture* target, struct gpu_profiler* profiler, void* out);
__fp16* ui_test_scene_render_read_(struct ui_test_scenes* scenes, const struct ui_test_scene* scene);
void* ui_test_render_read_(struct ui_node root, const struct ui_render_desc* desc, enum gpu_format format, uint2 size);

const uint8_t* ui_test_font_data_(bool ui_font, uint64_t* out_size);
double ui_test_text_advance_(const char* raw);
double2 ui_test_measure_text_(const char* raw, double fix_width);
struct app_event ui_test_touch_event_(enum app_pointer_event_type type, double x, double y, double timestamp, uint32_t modifiers);
struct app_event ui_test_direct_touch_event_(enum app_pointer_event_type type, double x, double y, double timestamp);

#if cr_ios

void ui_test_scene_present_raw_(uint32_t width, uint32_t height, const float* rgba);

void ui_test_glass_scene_present_(const struct ui_test_scene* scene);

void ui_test_ref_set_fill_(CGContextRef ctx, struct ui_color color);
CGContextRef ui_test_ref_begin_(uint32_t width, uint32_t height, struct ui_color background);
void ui_test_ref_end_(CGContextRef ctx, uint32_t width, uint32_t height, float* out_rgba, uint32_t bytes_per_row);

CTFontRef ui_test_scene_text_ct_font_(CGFontRef cg_font, double pt_size, double weight, bool inter);

#endif

#endif
