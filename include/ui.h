#pragma once

#include "cr.h"

struct gpu_device;
struct gpu_command_buffer;
struct gpu_texture;

struct ui_font;
struct ui_graphic;
struct ui_path;

#define ui_idx_invalid uint32_max
#define ui_node_null ((struct ui_node) { 0 })
#define ui_key_auto ((struct ui_key) { 0 })
#define ui_layer_max 4
#define ui_path_corners_max 256
#define ui_str_attribute_max 4
#define ui_glass_slider_default 0.5

#define ui_rgba_cs(r, g, b, a, color_space_) ((struct ui_color) { \
.value = { (float)(r), (float)(g), (float)(b), (float)(a) }, \
.color_space = color_space_ })
#define ui_rgba(r, g, b, a) ui_rgba_cs(r, g, b, a, ui_color_space_srgb)
#define ui_hex_a(hex, alpha) ui_rgba( \
(float)(((uint32_t)(hex) >> 16) & 0xff) / 255.0f, \
(float)(((uint32_t)(hex) >> 8) & 0xff) / 255.0f, \
(float)((uint32_t)(hex) & 0xff) / 255.0f, \
alpha)
#define ui_hex(hex) ui_hex_a(hex, 1.0f)
#define ui_gray_a(x, alpha) ui_rgba(x, x, x, alpha)
#define ui_gray(x) ui_gray_a(x, 1.0f)
#define ui_white ui_rgba(1.0f, 1.0f, 1.0f, 1.0f)
#define ui_gray90 ui_rgba(0.9f, 0.9f, 0.9f, 1.0f)
#define ui_gray80 ui_rgba(0.8f, 0.8f, 0.8f, 1.0f)
#define ui_gray70 ui_rgba(0.7f, 0.7f, 0.7f, 1.0f)
#define ui_gray60 ui_rgba(0.6f, 0.6f, 0.6f, 1.0f)
#define ui_gray50 ui_rgba(0.5f, 0.5f, 0.5f, 1.0f)
#define ui_gray40 ui_rgba(0.4f, 0.4f, 0.4f, 1.0f)
#define ui_gray30 ui_rgba(0.3f, 0.3f, 0.3f, 1.0f)
#define ui_gray20 ui_rgba(0.2f, 0.2f, 0.2f, 1.0f)
#define ui_gray10 ui_rgba(0.1f, 0.1f, 0.1f, 1.0f)
#define ui_black ui_rgba(0.0f, 0.0f, 0.0f, 1.0f)
#define ui_red ui_rgba(1.0f, 0.0f, 0.0f, 1.0f)
#define ui_green ui_rgba(0.0f, 1.0f, 0.0f, 1.0f)
#define ui_blue ui_rgba(0.0f, 0.0f, 1.0f, 1.0f)
#define ui_yellow ui_rgba(1.0f, 1.0f, 0.0f, 1.0f)
#define ui_magenta ui_rgba(1.0f, 0.0f, 1.0f, 1.0f)
#define ui_cyan ui_rgba(0.0f, 1.0f, 1.0f, 1.0f)
#define ui_clear ui_rgba(0.0f, 0.0f, 0.0f, 0.0f)

#define ui_pad(x) make_double4((double)(x), (double)(x), (double)(x), (double)(x))
#define ui_pad_xy(x, y) make_double4((double)(x), (double)(x), (double)(y), (double)(y))

enum cr_closed ui_color_space {
    ui_color_space_srgb,
    ui_color_space_srgb_linear,
    ui_color_space_display_p3,
    ui_color_space_display_p3_linear,
    ui_color_space_acescg_linear,
};

enum cr_closed ui_pos_anchor {
    ui_pos_anchor_start,
    ui_pos_anchor_center,
    ui_pos_anchor_end,
};

enum cr_closed ui_size_anchor {
    ui_size_anchor_width,
    ui_size_anchor_height,
};

enum cr_closed ui_pos_mode {
    ui_pos_mode_auto,
    ui_pos_mode_rel,
};

enum cr_closed ui_size_mode {
    ui_size_mode_fit,
    ui_size_mode_fill,
    ui_size_mode_fix,
    ui_size_mode_rel,
};

enum cr_closed ui_layout_mode {
    ui_layout_mode_manual,
    ui_layout_mode_stack_x,
    ui_layout_mode_stack_y,
};

enum cr_closed ui_dist {
    ui_dist_start,
    ui_dist_center,
    ui_dist_end,
};

enum cr_closed ui_align {
    ui_align_start,
    ui_align_center,
    ui_align_end,
};

enum cr_closed ui_blend_mode {
    ui_blend_mode_none,
    ui_blend_mode_normal,
};

enum cr_closed ui_layer_type {
    ui_layer_type_none,
    ui_layer_type_solid,
    ui_layer_type_gradient,
    ui_layer_type_graphic,
    ui_layer_type_background_blur,
    ui_layer_type_glass,
};

enum cr_closed ui_gradient_direction {
    ui_gradient_direction_right,
    ui_gradient_direction_left,
    ui_gradient_direction_down,
    ui_gradient_direction_up,
};

enum cr_closed ui_address_mode {
    ui_address_mode_fill,
    ui_address_mode_repeat,
};

enum cr_closed ui_filter {
    ui_filter_linear,
    ui_filter_nearest,
};

enum cr_closed ui_glass_variant {
    ui_glass_variant_clear,
    ui_glass_variant_regular,
};

enum cr_closed ui_glass_appearance {
    ui_glass_appearance_dark,
    ui_glass_appearance_light,
};

enum cr_closed ui_outline_align {
    ui_outline_align_inside,
    ui_outline_align_center,
    ui_outline_align_outside,
};

enum cr_closed ui_corner_profile {
    ui_corner_profile_continuous,
    ui_corner_profile_circular,
};

enum cr_closed ui_shape_group_op {
    ui_shape_group_op_none,
    ui_shape_group_op_union,
    ui_shape_group_op_subtract,
};

enum cr_closed ui_shape_mode {
    ui_shape_mode_fill,
    ui_shape_mode_stroke,
};

enum cr_closed ui_path_component_type {
    ui_path_component_type_move,
    ui_path_component_type_line,
    ui_path_component_type_cubic,
    ui_path_component_type_close,
};

enum cr_closed ui_str_attribute_type {
    ui_str_attribute_type_none,
    ui_str_attribute_type_color,
    ui_str_attribute_type_pt_size,
    ui_str_attribute_type_font,
    ui_str_attribute_type_weight,
};

enum cr_closed ui_pixel_format {
    ui_pixel_format_invalid,
    ui_pixel_format_bgra8,
    ui_pixel_format_r8,
    ui_pixel_format_rgba16f,
};

#define cr_handle ui_node
#include "cr_handle.h"

struct ui_key {
    uint64_t raw;
};

struct ui_color {
    float4 value;
    enum ui_color_space color_space;
};

struct ui_pos_axis {
    enum ui_pos_mode mode;
    enum ui_pos_anchor anchor;
    enum ui_pos_anchor parent_anchor;
    double constant;
};

struct ui_pos {
    struct ui_pos_axis x;
    struct ui_pos_axis y;
};

struct ui_size_axis {
    enum ui_size_mode mode;
    enum ui_size_anchor anchor;
    double multiplier;
    double constant;
};

struct ui_size {
    struct ui_size_axis width;
    struct ui_size_axis height;
};

struct ui_layout {
    enum ui_layout_mode mode;
    enum ui_dist dist;
    enum ui_align align;
    double spacing;
    union {
        double4 padding;
        struct {
            double padding_left;
            double padding_right;
            double padding_top;
            double padding_bottom;
        };
    };
};

struct ui_blend {
    enum ui_blend_mode mode;
    double opacity;
};

struct ui_layer {
    enum ui_layer_type type;
    struct ui_blend blend;
    union {
        struct {
            struct ui_color color;
        } solid;
        struct {
            enum ui_gradient_direction direction;
            struct ui_color color_start;
            struct ui_color color_end;
        } gradient;
        struct {
            struct ui_graphic* graphic;
            enum ui_address_mode address_mode;
            enum ui_filter filter;
            double2 scale;
            struct ui_color tint;
            enum ui_color_space color_space;
            bool premultiplied;
        } graphic;
        struct {
            double radius;
        } background_blur;
        struct {
            enum ui_glass_variant variant;
            enum ui_glass_appearance appearance;
            struct ui_color tint;
            double slider;
        } glass;
    };
};

struct ui_fill {
    struct ui_layer layers[ui_layer_max];
};

struct ui_outline {
    enum ui_outline_align align;
    double thickness;
    struct ui_layer layers[ui_layer_max];
};

struct ui_shadow {
    double2 offset;
    double radius;
    struct ui_color color;
};

struct ui_layer_blur {
    double radius;
};

struct ui_corners {
    enum ui_corner_profile profile;
    double4 radius;
};

struct ui_shape_group {
    enum ui_shape_group_op op;
    double smoothing;
};

struct ui_frame {
    struct ui_node node;
    struct ui_node parent;
    struct ui_key key;
    struct ui_pos pos;
    struct ui_size size;
    float3x2 transform;
    struct ui_layout layout;
    struct ui_fill fill;
    struct ui_outline outline;
    struct ui_corners corners;
    struct ui_shape_group shape_group;
    struct ui_shadow shadow;
    struct ui_layer_blur blur;
    bool clip;
    bool backdrop_root;
    struct ui_blend blend;
};

struct ui_rect {
    struct ui_node node;
    struct ui_node parent;
    struct ui_key key;
    struct ui_pos pos;
    struct ui_size size;
    float3x2 transform;
    struct ui_corners corners;
    struct ui_fill fill;
    struct ui_outline outline;
    struct ui_shadow shadow;
    struct ui_layer_blur blur;
    struct ui_blend blend;
};

struct ui_ellipse {
    struct ui_node node;
    struct ui_node parent;
    struct ui_key key;
    struct ui_pos pos;
    struct ui_size size;
    float3x2 transform;
    struct ui_fill fill;
    struct ui_outline outline;
    struct ui_shadow shadow;
    struct ui_layer_blur blur;
    struct ui_blend blend;
};

struct ui_line {
    struct ui_node node;
    struct ui_node parent;
    struct ui_key key;
    struct ui_pos pos;
    struct ui_size size;
    float3x2 transform;
    double thickness;
    struct ui_fill fill;
    struct ui_outline outline;
    struct ui_shadow shadow;
    struct ui_layer_blur blur;
    struct ui_blend blend;
};

struct ui_path_component {
    enum ui_path_component_type type;
    union {
        struct {
            double2 p;
        } move;
        struct {
            double2 p;
        } line;
        struct {
            double2 c1;
            double2 c2;
            double2 p;
        } cubic;
    };
};

struct ui_shape {
    struct ui_node node;
    struct ui_node parent;
    struct ui_key key;
    struct ui_pos pos;
    struct ui_size size;
    float3x2 transform;
    enum ui_shape_mode mode;
    double thickness;
    const struct ui_path* path;
    struct ui_fill fill;
    struct ui_outline outline;
    struct ui_shadow shadow;
    struct ui_layer_blur blur;
    struct ui_blend blend;
};

struct ui_str_attribute {
    uint32_t start;
    uint32_t end;
    enum ui_str_attribute_type type;
    union {
        struct ui_color color;
        double pt_size;
        struct ui_font* font;
        double weight;
    };
};

struct ui_str {
    const char* raw;
    struct ui_str_attribute attributes[ui_str_attribute_max];
    const struct ui_str_attribute* extra_attributes;
    uint32_t extra_attribute_count;
};

struct ui_text {
    struct ui_node node;
    struct ui_node parent;
    struct ui_key key;
    struct ui_pos pos;
    struct ui_size size;
    float3x2 transform;
    struct ui_str str;
    struct ui_fill fill;
    struct ui_shadow shadow;
    struct ui_layer_blur blur;
    struct ui_blend blend;
};

struct ui_custom_vertex_solid {
    float2 pos_px;
    float2 _;
};

struct ui_custom_vertex_graphic {
    float2 pos_px;
    float2 uv;
    float2 _;
};

struct ui_custom_on_render {
    void (*draw_solid)(float3x2 transform, struct ui_color color, uint32_t vertex_count, struct ui_custom_vertex_solid** out_vertices);
    void (*draw_graphic)(float3x2 transform, struct ui_graphic* graphic, struct ui_color tint, uint32_t vertex_count, struct ui_custom_vertex_graphic** out_vertices);
    void* user;
    int2 origin_px;
    int2 size_px;
};

struct ui_custom_desc {
    void (*on_render)(const struct ui_custom_on_render* on_render);
    void* user;
};

struct ui_custom {
    struct ui_node node;
    struct ui_node parent;
    struct ui_key key;
    struct ui_pos pos;
    struct ui_size size;
    float3x2 transform;
    struct ui_custom_desc desc;
};

struct ui_render_desc {
    enum ui_color_space output_color_space;
    enum ui_pixel_format compositing_format;
    bool premultiplied;
    bool dither;
};

struct ui_hit {
    struct ui_node node;
    bool contains;
};

struct ui_text_line {
    uint32_t start;
    uint32_t end;
    uint32_t visual_end;
    double y;
    double height;
    double width;
};

cr_hdef __attribute__((always_inline))
struct ui_color ui_color(float4 value, enum ui_color_space color_space) {
    return (struct ui_color) {
        .value = value,
        .color_space = color_space,
    };
}

cr_hdef __attribute__((always_inline))
struct ui_pos_axis ui_pos_axis_auto(void) {
    return (struct ui_pos_axis) { 0 };
}

cr_hdef __attribute__((always_inline))
struct ui_pos_axis ui_pos_axis_rel(enum ui_pos_anchor anchor, enum ui_pos_anchor parent_anchor, double constant) {
    return (struct ui_pos_axis) {
        .mode = ui_pos_mode_rel,
        .anchor = anchor,
        .parent_anchor = parent_anchor,
        .constant = constant,
    };
}

cr_hdef __attribute__((always_inline))
struct ui_pos_axis ui_pos_axis_start(void) {
    return ui_pos_axis_rel(ui_pos_anchor_start, ui_pos_anchor_start, 0.0);
}

cr_hdef __attribute__((always_inline))
struct ui_pos_axis ui_pos_axis_center(void) {
    return ui_pos_axis_rel(ui_pos_anchor_center, ui_pos_anchor_center, 0.0);
}

cr_hdef __attribute__((always_inline))
struct ui_pos_axis ui_pos_axis_end(void) {
    return ui_pos_axis_rel(ui_pos_anchor_end, ui_pos_anchor_end, 0.0);
}

cr_hdef __attribute__((always_inline))
struct ui_pos ui_pos(struct ui_pos_axis x, struct ui_pos_axis y) {
    return (struct ui_pos) {
        .x = x,
        .y = y,
    };
}

cr_hdef __attribute__((always_inline))
struct ui_pos ui_pos_xy(double x, double y) {
    return ui_pos(ui_pos_axis_rel(ui_pos_anchor_start, ui_pos_anchor_start, x), ui_pos_axis_rel(ui_pos_anchor_start, ui_pos_anchor_start, y));
}

cr_hdef __attribute__((always_inline))
struct ui_pos ui_pos_center(void) {
    return ui_pos(ui_pos_axis_center(), ui_pos_axis_center());
}

cr_hdef __attribute__((always_inline))
struct ui_size_axis ui_size_axis_fit(void) {
    return (struct ui_size_axis) { .mode = ui_size_mode_fit };
}

cr_hdef __attribute__((always_inline))
struct ui_size_axis ui_size_axis_fill(void) {
    return (struct ui_size_axis) { .mode = ui_size_mode_fill };
}

cr_hdef __attribute__((always_inline))
struct ui_size_axis ui_size_axis_fix(double constant) {
    return (struct ui_size_axis) {
        .mode = ui_size_mode_fix,
        .constant = constant,
    };
}

cr_hdef __attribute__((always_inline))
struct ui_size_axis ui_size_axis_rel(enum ui_size_anchor anchor, double multiplier, double constant) {
    return (struct ui_size_axis) {
        .mode = ui_size_mode_rel,
        .anchor = anchor,
        .multiplier = multiplier,
        .constant = constant,
    };
}

cr_hdef __attribute__((always_inline))
struct ui_size_axis ui_size_axis_width(void) {
    return ui_size_axis_rel(ui_size_anchor_width, 1.0, 0.0);
}

cr_hdef __attribute__((always_inline))
struct ui_size_axis ui_size_axis_height(void) {
    return ui_size_axis_rel(ui_size_anchor_height, 1.0, 0.0);
}

cr_hdef __attribute__((always_inline))
struct ui_size ui_size(struct ui_size_axis width, struct ui_size_axis height) {
    return (struct ui_size) {
        .width = width,
        .height = height,
    };
}

cr_hdef __attribute__((always_inline))
struct ui_size ui_size_fix(double width, double height) {
    return ui_size(ui_size_axis_fix(width), ui_size_axis_fix(height));
}

cr_hdef __attribute__((always_inline))
struct ui_size ui_size_fit(void) {
    return ui_size(ui_size_axis_fit(), ui_size_axis_fit());
}

cr_hdef __attribute__((always_inline))
struct ui_size ui_size_fill(void) {
    return ui_size(ui_size_axis_fill(), ui_size_axis_fill());
}

cr_hdef __attribute__((always_inline))
struct ui_layout ui_layout_stack_x(enum ui_dist dist, enum ui_align align, double spacing, double4 padding) {
    return (struct ui_layout) {
        .mode = ui_layout_mode_stack_x,
        .dist = dist,
        .align = align,
        .spacing = spacing,
        .padding = padding,
    };
}

cr_hdef __attribute__((always_inline))
struct ui_layout ui_layout_stack_y(enum ui_dist dist, enum ui_align align, double spacing, double4 padding) {
    return (struct ui_layout) {
        .mode = ui_layout_mode_stack_y,
        .dist = dist,
        .align = align,
        .spacing = spacing,
        .padding = padding,
    };
}

cr_hdef __attribute__((always_inline))
struct ui_layer ui_layer_solid(struct ui_color color) {
    return (struct ui_layer) {
        .type = ui_layer_type_solid,
        .solid.color = color,
    };
}

cr_hdef __attribute__((always_inline))
struct ui_layer ui_layer_gradient(enum ui_gradient_direction direction, struct ui_color color_start, struct ui_color color_end) {
    return (struct ui_layer) {
        .type = ui_layer_type_gradient,
        .gradient.direction = direction,
        .gradient.color_start = color_start,
        .gradient.color_end = color_end,
    };
}

cr_hdef __attribute__((always_inline))
struct ui_layer ui_layer_graphic(struct ui_graphic* graphic, enum ui_address_mode address_mode, struct ui_color tint) {
    return (struct ui_layer) {
        .type = ui_layer_type_graphic,
        .graphic.graphic = graphic,
        .graphic.address_mode = address_mode,
        .graphic.tint = tint,
        .graphic.premultiplied = true,
    };
}

cr_hdef __attribute__((always_inline))
struct ui_layer ui_layer_graphic_ex(struct ui_graphic* graphic, enum ui_address_mode address_mode, enum ui_filter filter, double2 scale, struct ui_color tint, enum ui_color_space color_space, bool premultiplied) {
    return (struct ui_layer) {
        .type = ui_layer_type_graphic,
        .graphic.graphic = graphic,
        .graphic.address_mode = address_mode,
        .graphic.filter = filter,
        .graphic.scale = scale,
        .graphic.tint = tint,
        .graphic.color_space = color_space,
        .graphic.premultiplied = premultiplied,
    };
}

cr_hdef __attribute__((always_inline))
struct ui_layer ui_layer_background_blur(double radius) {
    return (struct ui_layer) {
        .type = ui_layer_type_background_blur,
        .background_blur.radius = radius,
    };
}

cr_hdef __attribute__((always_inline))
struct ui_layer ui_layer_glass(enum ui_glass_variant variant, enum ui_glass_appearance appearance) {
    return (struct ui_layer) {
        .type = ui_layer_type_glass,
        .glass.variant = variant,
        .glass.appearance = appearance,
        .glass.slider = ui_glass_slider_default,
    };
}

cr_hdef __attribute__((always_inline))
struct ui_layer ui_layer_glass_ex(enum ui_glass_variant variant, enum ui_glass_appearance appearance, struct ui_color tint, double slider) {
    return (struct ui_layer) {
        .type = ui_layer_type_glass,
        .glass.variant = variant,
        .glass.appearance = appearance,
        .glass.tint = tint,
        .glass.slider = slider,
    };
}

cr_hdef __attribute__((always_inline))
struct ui_layer ui_layer_opacity(struct ui_layer layer, double opacity) {
    layer.blend = (struct ui_blend) {
        .mode = ui_blend_mode_normal,
        .opacity = opacity,
    };
    return layer;
}

cr_hdef __attribute__((always_inline))
struct ui_fill ui_fill_solid(struct ui_color color) {
    return (struct ui_fill) { .layers[0] = ui_layer_solid(color) };
}

cr_hdef __attribute__((always_inline))
struct ui_fill ui_fill_gradient(enum ui_gradient_direction direction, struct ui_color color_start, struct ui_color color_end) {
    return (struct ui_fill) { .layers[0] = ui_layer_gradient(direction, color_start, color_end) };
}

cr_hdef __attribute__((always_inline))
struct ui_fill ui_fill_graphic(struct ui_graphic* graphic, enum ui_address_mode address_mode, struct ui_color tint) {
    return (struct ui_fill) { .layers[0] = ui_layer_graphic(graphic, address_mode, tint) };
}

cr_hdef __attribute__((always_inline))
struct ui_outline ui_outline_solid(enum ui_outline_align align, double thickness, struct ui_color color) {
    return (struct ui_outline) {
        .align = align,
        .thickness = thickness,
        .layers[0] = ui_layer_solid(color),
    };
}

cr_hdef __attribute__((always_inline))
struct ui_shadow ui_shadow(double x, double y, double radius, struct ui_color color) {
    return (struct ui_shadow) {
        .offset = make_double2(x, y),
        .radius = radius,
        .color = color,
    };
}

cr_hdef __attribute__((always_inline))
struct ui_layer_blur ui_blur(double radius) {
    return (struct ui_layer_blur) {
        .radius = radius,
    };
}

cr_hdef __attribute__((always_inline))
struct ui_corners ui_corners(double radius) {
    return (struct ui_corners) {
        .radius = make_double4(radius, radius, radius, radius),
    };
}

cr_hdef __attribute__((always_inline))
struct ui_corners ui_corners_circular(double radius) {
    return (struct ui_corners) {
        .profile = ui_corner_profile_circular,
        .radius = make_double4(radius, radius, radius, radius),
    };
}

cr_hdef __attribute__((always_inline))
struct ui_str ui_str(const char* raw, struct ui_color color, double pt_size) {
    return (struct ui_str) {
        .raw = raw,
        .attributes = {
            { .start = 0, .end = uint32_max, .type = ui_str_attribute_type_color, .color = color },
            { .start = 0, .end = uint32_max, .type = ui_str_attribute_type_pt_size, .pt_size = pt_size },
        },
    };
}

cr_hdef __attribute__((always_inline))
struct ui_str ui_str_weight(const char* raw, struct ui_color color, double pt_size, double weight) {
    return (struct ui_str) {
        .raw = raw,
        .attributes = {
            { .start = 0, .end = uint32_max, .type = ui_str_attribute_type_color, .color = color },
            { .start = 0, .end = uint32_max, .type = ui_str_attribute_type_pt_size, .pt_size = pt_size },
            { .start = 0, .end = uint32_max, .type = ui_str_attribute_type_weight, .weight = weight },
        },
    };
}

void ui_init(struct cr_mem* mem, struct gpu_device* device);
void ui_deinit(void);

void ui_set_transient_memory_limit(uint64_t bytes);
void ui_set_layer_prepass_limit(uint64_t bytes);
void ui_set_to_px_from_pt(double to_px_from_pt);
double ui_get_to_px_from_pt(void);
double ui_round_px(double value);
double ui_approach_px(double current, double target, double f);

__attribute__((format(printf, 1, 2)))
struct ui_key ui_key(const char* fmt, ...);
struct ui_key ui_key_str(const char* str);
struct ui_key ui_key_u64(uint64_t value);
bool ui_key_is_explicit(struct ui_key key);

struct ui_font* ui_font_create(const uint8_t* raw_data, uint64_t data_size);
void ui_font_destroy(struct ui_font* font);
void ui_set_default_font(struct ui_font* font);

struct ui_graphic* ui_graphic_create(uint32_t width, uint32_t height, enum ui_pixel_format pixel_format, const uint8_t* src_raw, uint32_t src_bytes_per_row, bool premultiplied);
struct ui_graphic* ui_graphic_create_texture(struct gpu_texture* texture);
void ui_graphic_destroy(struct ui_graphic* graphic);

struct ui_path* ui_path_create(void);
void ui_path_destroy(struct ui_path* path);
uint32_t ui_path_get_count(const struct ui_path* path);
const struct ui_path_component* ui_path_get_components(const struct ui_path* path);
void ui_path_append(struct ui_path* path, const struct ui_path_component* components, uint32_t count);
void ui_path_move_to(struct ui_path* path, double2 p);
void ui_path_line_to(struct ui_path* path, double2 p);
void ui_path_cubic_to(struct ui_path* path, double2 c1, double2 c2, double2 p);
void ui_path_quad_to(struct ui_path* path, double2 c, double2 p);
void ui_path_close(struct ui_path* path);
void ui_path_arc(struct ui_path* path, double2 center, double radius, double start_angle, double end_angle, bool clockwise);
void ui_path_rounded_rect(struct ui_path* path, double2 origin, double2 size, struct ui_corners corners);
void ui_path_ellipse(struct ui_path* path, double2 origin, double2 size);
void ui_path_corners(struct ui_path* path, const double2* points, const double* radii, uint32_t count, bool continuous);

struct ui_node ui_node(struct ui_node parent, struct ui_key key);
struct ui_node ui_frame(const struct ui_frame* frame);
struct ui_node ui_rect(const struct ui_rect* rect);
struct ui_node ui_ellipse(const struct ui_ellipse* ellipse);
struct ui_node ui_line(const struct ui_line* line);
struct ui_node ui_shape(const struct ui_shape* shape);
struct ui_node ui_text(const struct ui_text* text);
struct ui_node ui_custom(const struct ui_custom* custom);

void ui_layout(struct ui_node root);

bool ui_render_direct_to_target(struct ui_render_desc desc, enum ui_pixel_format target_format, bool target_sampleable);
void ui_render(struct ui_node root, struct gpu_command_buffer* command_buffer, struct gpu_texture* target, struct ui_render_desc desc);

void ui_print(struct ui_node root);
void ui_release(struct ui_node root);
bool ui_is_alive(struct ui_node node);
uint32_t ui_get_node_alloc_count(void);
uint64_t ui_get_key(struct ui_node node);
uint64_t ui_derive_key(struct ui_node node, struct ui_key key);
struct ui_node ui_get_root(struct ui_node node);

double2 ui_get_size(struct ui_node node_handle);
double2 ui_get_origin_from_root(struct ui_node root, struct ui_node node_handle);
double2 ui_to_local_from_root(struct ui_node root, struct ui_node node_handle, double2 position);
uint32_t ui_hit_test(struct ui_node root, double2 position, bool (*is_target)(struct ui_node node, void* user), void* user, struct ui_hit* out_chain, uint32_t chain_capacity);

uint32_t ui_text_get_line_count(struct ui_node text);
struct ui_text_line ui_text_get_line(struct ui_node text, uint32_t line_index);
double ui_text_get_x(struct ui_node text, uint32_t line_index, uint32_t offset);
uint32_t ui_text_get_offset(struct ui_node text, uint32_t line_index, double x_pt);
uint32_t ui_text_prev_cluster(struct ui_node text, uint32_t offset);
uint32_t ui_text_next_cluster(struct ui_node text, uint32_t offset);
