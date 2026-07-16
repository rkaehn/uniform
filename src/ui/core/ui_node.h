#pragma once

#include "ui.h"
#include "ui_fx.h"
#include "ui_path.h"

struct ui;
struct ui_shaped;
struct ui_text_span;

#define ui_text_small_bytes 128

enum ui_content_type {
    ui_content_type_none,
    ui_content_type_frame,
    ui_content_type_shape,
    ui_content_type_text,
    ui_content_type_custom,
};

enum ui_shape_type {
    ui_shape_type_rect,
    ui_shape_type_ellipse,
    ui_shape_type_line,
    ui_shape_type_path,
};

struct ui_fill_backing {
    const struct ui_layer* layers;
    uint32_t layer_count;
};

struct ui_outline_backing {
    enum ui_outline_align align;
    double thickness;
    const struct ui_layer* layers;
    uint32_t layer_count;
};

struct ui_node_pos_axis {
    enum ui_pos_mode mode : 8;
    enum ui_pos_anchor anchor : 8;
    enum ui_pos_anchor parent_anchor : 8;
    double constant;
};

struct ui_node_size_axis {
    enum ui_size_mode mode : 8;
    enum ui_size_anchor anchor : 8;
    float multiplier;
    double constant;
};

struct ui_node_pos {
    struct ui_node_pos_axis x;
    struct ui_node_pos_axis y;
};

struct ui_node_size {
    struct ui_node_size_axis width;
    struct ui_node_size_axis height;
};

struct ui_layout_pos_axis {
    enum ui_pos_mode mode : 8;
    enum ui_pos_anchor anchor : 8;
    enum ui_pos_anchor parent_anchor : 8;
    struct ui_fx constant;
};

struct ui_layout_size_axis {
    enum ui_size_mode mode : 8;
    enum ui_size_anchor anchor : 8;
    float multiplier;
    struct ui_fx constant;
};

struct ui_layout_p0 {
    struct ui_layout_pos_axis x;
    struct ui_layout_pos_axis y;
    struct ui_layout_size_axis width;
    struct ui_layout_size_axis height;
    enum ui_layout_mode mode : 8;
    enum ui_dist dist : 8;
    enum ui_align align : 8;
    struct ui_fx spacing;
    struct ui_fx padding_top;
    struct ui_fx padding_bottom;
    struct ui_fx padding_left;
    struct ui_fx padding_right;
};

struct ui_layout_p1 {
    uint32_t fill_count;
    struct ui_fx min_width;
    struct ui_fx min_height;
    bool cons_valid;
    bool cons_is_x;
    int32_t cons_size_cross_raw;
    struct ui_fx cons_min_content_main;
};

struct ui_layout_p2 {
    struct ui_fx x;
    struct ui_fx y;
    struct ui_fx width;
    struct ui_fx height;
};

struct ui_node_backing {
    struct ui_node handle;
    uint64_t key;
    uint32_t auto_child_count;
    uint32_t auto_derived_count;
    uint32_t parent_idx;
    uint32_t first_child_idx;
    uint32_t last_child_idx;
    uint32_t next_idx;
    uint32_t prev_idx;
    enum ui_content_type type;
    struct ui_node_pos pos;
    struct ui_node_size size;
    struct ui_layout_p0 p0;
    struct ui_layout_p1 p1;
    struct ui_layout_p2 p2;
    double layout_to_px_from_pt;
    float3x2 transform;
    union {
        struct {
            struct ui_layout layout;
            struct ui_corners corners;
            struct ui_shape_group shape_group;
            bool clip;
            bool backdrop_root;
        } frame;
        struct {
            enum ui_shape_type shape_type;
            struct ui_corners corners;
            double thickness;
            enum ui_shape_mode path_mode;
            struct ui_path path;
        } shape;
        struct {
            const char* raw;
            const struct ui_text_span* spans;
            uint32_t raw_size;
            uint32_t span_count;
            struct ui_shaped* shaped;
            void* block;
            _Alignas(16) uint8_t small[ui_text_small_bytes];
        } text;
        struct {
            struct ui_custom_desc desc;
        } custom;
    };
    struct ui_fill_backing fill;
    struct ui_outline_backing outline;
    struct ui_shadow shadow;
    struct ui_layer_blur blur;
    struct ui_blend blend;
};

#define cr_pool ui_node_pool
#define cr_pool_item struct ui_node_backing
#define cr_pool_capacity 4096
#define cr_pool_handle ui_node
#include "cr_pool.h"

cr_hdef __attribute__((always_inline))
struct ui_node_pos_axis ui_node_pos_axis_make(struct ui_pos_axis axis) {
    return (struct ui_node_pos_axis) {
        .mode = axis.mode,
        .anchor = axis.anchor,
        .parent_anchor = axis.parent_anchor,
        .constant = axis.constant,
    };
}

cr_hdef __attribute__((always_inline))
struct ui_node_size_axis ui_node_size_axis_make(struct ui_size_axis axis) {
    return (struct ui_node_size_axis) {
        .mode = axis.mode,
        .anchor = axis.anchor,
        .multiplier = (float)axis.multiplier,
        .constant = axis.constant,
    };
}

cr_hdef __attribute__((always_inline))
struct ui_node_pos ui_node_pos_make(struct ui_pos pos) {
    return (struct ui_node_pos) {
        .x = ui_node_pos_axis_make(pos.x),
        .y = ui_node_pos_axis_make(pos.y),
    };
}

cr_hdef __attribute__((always_inline))
struct ui_node_size ui_node_size_make(struct ui_size size) {
    return (struct ui_node_size) {
        .width = ui_node_size_axis_make(size.width),
        .height = ui_node_size_axis_make(size.height),
    };
}

struct ui_node_backing* ui_node_create(struct ui* ui);
void ui_node_destroy(struct ui* ui, struct ui_node_backing* node);
void ui_node_destroy_recursive(struct ui* ui, struct ui_node_backing* node);
struct ui_node_backing* ui_node_get_prev(struct ui* ui, struct ui_node_backing* node);
struct ui_node_backing* ui_node_get_next(struct ui* ui, struct ui_node_backing* node);
struct ui_node_backing* ui_node_get_parent(struct ui* ui, struct ui_node_backing* node);
struct ui_node_backing* ui_node_get_child(struct ui* ui, struct ui_node_backing* node, uint32_t idx);
struct ui_node_backing* ui_node_get_last_child(struct ui* ui, struct ui_node_backing* node);
struct ui_node_backing* ui_node_get_after(struct ui* ui, struct ui_node_backing* node, int32_t* inout_level);
void ui_node_add_child(struct ui* ui, struct ui_node_backing* node, struct ui_node_backing* child_node);
void ui_node_remove_from_parent(struct ui* ui, struct ui_node_backing* node);
