#include "ui_core.h"
#include "ui_atlas.h"
#include "ui_persistent.h"
#include "ui_render.h"
#include "ui_text.h"
#include "ui_transient.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

struct ui* ui_context = NULL;

void ui_init(struct cr_mem* mem, struct gpu_device* device) {
    cr_check(ui_context == NULL, "the ui is already initialized");
    cr_check(mem != NULL, "ui_init needs a memory allocator");
    cr_check(device != NULL, "ui_init needs a gpu device");

    struct ui* ui = cr_malloc(mem, sizeof(*ui), 0, cr_loc);

    memset(ui, 0, sizeof(*ui));
    ui->mem = mem;
    ui->device = device;
    ui->to_px_from_pt = 1.0;
    ui_node_pool_init(&ui->node_pool);
    ui->mod_text = ui_mod_text_create(mem);
    ui->atlas = ui_atlas_create(mem);
    ui->persistent = ui_persistent_create(mem, 1024);
    ui->transient = ui_transient_create(mem);
    ui_render_init(ui);

    ui_context = ui;
}

void ui_deinit(void) {
    struct ui* ui = ui_context;
    if (ui == NULL) {
        return;
    }

    ui_render_deinit(ui);
    ui_mod_text_destroy(ui->mod_text);
    ui_atlas_destroy(ui);
    ui_transient_destroy(ui);
    ui_persistent_destroy(ui);
    cr_free(ui->mem, ui, cr_loc);

    ui_context = NULL;
}

void ui_set_to_px_from_pt(double to_px_from_pt) {
    struct ui* ui = ui_ctx();
    cr_check(to_px_from_pt > 0.0, "the pixel scale %g is not positive", to_px_from_pt);
    ui->to_px_from_pt = to_px_from_pt;
}

double ui_get_to_px_from_pt(void) {
    return ui_ctx()->to_px_from_pt;
}

double ui_round_px(double value) {
    return ui_round_px_ex(value, ui_get_to_px_from_pt());
}

double ui_approach_px(double current, double target, double f) {
    return ui_approach_px_ex(current, target, f, ui_get_to_px_from_pt());
}

#define ui_key_payload_mask ((1ull << 52) - 1)
#define ui_key_explicit_bit (1ull << 52)

struct ui_key ui_key(const char* fmt, ...) {
    char str[256];

    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(str, sizeof(str), fmt, args);
    va_end(args);

    cr_check(len >= 0 && (size_t)len < sizeof(str), "a key string holds at most %zu bytes", sizeof(str) - 1);

    return ui_key_u64(cr_hash(str, (size_t)len, 0));
}

struct ui_key ui_key_str(const char* str) {
    return ui_key_u64(cr_hash(str, strlen(str), 0));
}

struct ui_key ui_key_u64(uint64_t value) {
    return (struct ui_key) { .raw = (value & ui_key_payload_mask) | ui_key_explicit_bit };
}

bool ui_key_is_explicit(struct ui_key key) {
    return key.raw != 0;
}

#define ui_key_seed_child_named 1u
#define ui_key_seed_child_ordinal 2u
#define ui_key_seed_derived_named 3u
#define ui_key_seed_derived_ordinal 4u

static uint64_t ui_key_mix(uint64_t key, uint64_t sub, uint64_t seed) {
    uint64_t raw[2] = { key, sub };
    return ui_key_u64(cr_hash(raw, sizeof(raw), seed)).raw;
}

static uint64_t ui_key_child(struct ui_node_backing* parent, struct ui_key key) {
    uint64_t base = parent ? parent->key : 0;

    if (ui_key_is_explicit(key)) {
        return ui_key_mix(base, key.raw, ui_key_seed_child_named);
    }

    uint32_t ordinal = parent ? parent->auto_child_count++ : 0;

    return ui_key_mix(base, ordinal, ui_key_seed_child_ordinal);
}

uint32_t ui_get_node_alloc_count(void) {
    struct ui* ui = ui_ctx();

    return ui->node_pool.stack_idx;
}

struct ui_node_style {
    struct ui_fill fill;
    struct ui_outline outline;
    struct ui_shadow shadow;
    struct ui_layer_blur blur;
    struct ui_blend blend;
};

static struct ui_node_backing* ui_node_declare(struct ui* ui, struct ui_node parent, struct ui_key key) {
    struct ui_node_backing* parent_node = ui_node_is_null(parent) ? NULL : ui_node_check(ui, parent, "the parent");

    struct ui_node_backing* node = ui_node_create(ui);
    node->key = ui_key_child(parent_node, key);

    if (parent_node) {
        ui_node_add_child(ui, parent_node, node);
    }

    return node;
}

static struct ui_node_backing* ui_node_define(struct ui* ui, struct ui_node node, struct ui_node parent, struct ui_key key, enum ui_content_type type) {
    struct ui_node_backing* backing;

    if (ui_node_is_null(node)) {
        backing = ui_node_declare(ui, parent, key);
    } else {
        cr_check(ui_node_is_null(parent), "a declared node already has its parent");
        cr_check(ui_key_is_explicit(key) == false, "a declared node already has its key");
        backing = ui_node_check(ui, node, "the declared node");
        cr_check(backing->type == ui_content_type_none, "the node already has content");
    }

    backing->type = type;

    return backing;
}

struct ui_node ui_node(struct ui_node parent, struct ui_key key) {
    struct ui* ui = ui_ctx();

    return ui_node_declare(ui, parent, key)->handle;
}

static void ui_layers_validate(const struct ui_layer* layers, bool is_outline, bool is_text, uint32_t* inout_glass_count) {
    for (uint32_t i = 0; i < ui_layer_max; i++) {
        switch (layers[i].type) {
        case ui_layer_type_none:
        case ui_layer_type_solid: break;
        case ui_layer_type_gradient: break;
        case ui_layer_type_graphic: {
            cr_check(is_text == false, "text fills support solid and gradient layers");
            cr_check(layers[i].graphic.graphic != NULL, "a graphic layer needs a graphic");
        } break;
        case ui_layer_type_background_blur: {
            cr_check(is_text == false, "text fills support solid and gradient layers");
        } break;
        case ui_layer_type_glass: {
            cr_check(is_outline == false, "glass is a fill layer");
            cr_check(is_text == false, "text fills support solid and gradient layers");
            (*inout_glass_count)++;
            cr_check(*inout_glass_count <= 1, "a node holds at most one glass layer");
        } break;
        }
    }
}

static uint32_t ui_layers_compact(struct ui_layer* dst, const struct ui_layer* src) {
    uint32_t count = 0;
    for (uint32_t i = 0; i < ui_layer_max; i++) {
        if (src[i].type != ui_layer_type_none) {
            if (dst != NULL) {
                dst[count] = src[i];
            }
            count++;
        }
    }
    return count;
}

static void ui_node_set_style(struct ui* ui, struct ui_node_backing* node, const struct ui_node_style* style) {
    uint32_t glass_count = 0;
    ui_layers_validate(style->fill.layers, false, node->type == ui_content_type_text, &glass_count);
    ui_layers_validate(style->outline.layers, true, node->type == ui_content_type_text, &glass_count);

    uint32_t fill_count = ui_layers_compact(NULL, style->fill.layers);
    uint32_t outline_count = ui_layers_compact(NULL, style->outline.layers);
    uint64_t total_size = (fill_count + outline_count) * sizeof(struct ui_layer);

    node->fill = (struct ui_fill_backing) { .layer_count = fill_count };
    node->outline = (struct ui_outline_backing) {
        .align = style->outline.align,
        .thickness = style->outline.thickness,
        .layer_count = outline_count,
    };
    node->shadow = style->shadow;
    node->blur = style->blur;
    node->blend = style->blend;

    if (total_size != 0) {
        struct ui_layer* block = cr_malloc(ui->mem, total_size, 0, cr_loc);
        ui_layers_compact(block, style->fill.layers);
        ui_layers_compact(block + fill_count, style->outline.layers);

        node->fill.layers = block;
        node->outline.layers = block + fill_count;
    }
}

struct ui_node ui_frame(const struct ui_frame* frame) {
    struct ui* ui = ui_ctx();

    struct ui_node_backing* node = ui_node_define(ui, frame->node, frame->parent, frame->key, ui_content_type_frame);
    node->pos = ui_node_pos_make(frame->pos);
    node->size = ui_node_size_make(frame->size);
    node->transform = frame->transform;
    node->frame.layout = frame->layout;
    node->frame.corners = frame->corners;
    node->frame.shape_group = frame->shape_group;
    node->frame.clip = frame->clip;
    node->frame.backdrop_root = frame->backdrop_root;

    ui_node_set_style(ui, node, &(struct ui_node_style) {
        .fill = frame->fill,
        .outline = frame->outline,
        .shadow = frame->shadow,
        .blur = frame->blur,
        .blend = frame->blend,
    });

    return node->handle;
}

static struct ui_node ui_shape_node(struct ui* ui, struct ui_node handle, struct ui_node parent, struct ui_key key, enum ui_shape_type shape_type, struct ui_pos pos, struct ui_size size, float3x2 transform, struct ui_corners corners, double thickness, const struct ui_node_style* style) {
    struct ui_node_backing* node = ui_node_define(ui, handle, parent, key, ui_content_type_shape);
    node->pos = ui_node_pos_make(pos);
    node->size = ui_node_size_make(size);
    node->transform = transform;
    node->shape.shape_type = shape_type;
    node->shape.corners = corners;
    node->shape.thickness = thickness;

    ui_node_set_style(ui, node, style);

    return node->handle;
}

struct ui_node ui_rect(const struct ui_rect* rect) {
    return ui_shape_node(ui_ctx(), rect->node, rect->parent, rect->key, ui_shape_type_rect, rect->pos, rect->size, rect->transform, rect->corners, 0.0, &(struct ui_node_style) {
        .fill = rect->fill,
        .outline = rect->outline,
        .shadow = rect->shadow,
        .blur = rect->blur,
        .blend = rect->blend,
    });
}

struct ui_node ui_ellipse(const struct ui_ellipse* ellipse) {
    return ui_shape_node(ui_ctx(), ellipse->node, ellipse->parent, ellipse->key, ui_shape_type_ellipse, ellipse->pos, ellipse->size, ellipse->transform, (struct ui_corners) { 0 }, 0.0, &(struct ui_node_style) {
        .fill = ellipse->fill,
        .outline = ellipse->outline,
        .shadow = ellipse->shadow,
        .blur = ellipse->blur,
        .blend = ellipse->blend,
    });
}

struct ui_node ui_line(const struct ui_line* line) {
    return ui_shape_node(ui_ctx(), line->node, line->parent, line->key, ui_shape_type_line, line->pos, line->size, line->transform, (struct ui_corners) { 0 }, line->thickness, &(struct ui_node_style) {
        .fill = line->fill,
        .outline = line->outline,
        .shadow = line->shadow,
        .blur = line->blur,
        .blend = line->blend,
    });
}

struct ui_node ui_shape(const struct ui_shape* shape) {
    struct ui* ui = ui_ctx();

    struct ui_node handle = ui_shape_node(ui, shape->node, shape->parent, shape->key, ui_shape_type_path, shape->pos, shape->size, shape->transform, (struct ui_corners) { 0 }, shape->thickness, &(struct ui_node_style) {
        .fill = shape->fill,
        .outline = shape->outline,
        .shadow = shape->shadow,
        .blur = shape->blur,
        .blend = shape->blend,
    });
    struct ui_node_backing* node = ui_node_pool_item(&ui->node_pool, handle);

    node->shape.path_mode = shape->mode;
    if (shape->path) {
        ui_path_init_copy(&node->shape.path, ui->mem, shape->path);
    } else {
        ui_path_init(&node->shape.path, ui->mem);
    }

    return handle;
}

struct ui_node ui_text(const struct ui_text* text) {
    struct ui* ui = ui_ctx();

    struct ui_node_backing* node = ui_node_define(ui, text->node, text->parent, text->key, ui_content_type_text);
    node->pos = ui_node_pos_make(text->pos);
    node->size = ui_node_size_make(text->size);
    node->transform = text->transform;

    ui_node_set_style(ui, node, &(struct ui_node_style) {
        .fill = text->fill,
        .shadow = text->shadow,
        .blur = text->blur,
        .blend = text->blend,
    });

    ui_text_node_init(ui, node, &text->str);

    return node->handle;
}

struct ui_node ui_custom(const struct ui_custom* custom) {
    struct ui* ui = ui_ctx();

    struct ui_node_backing* node = ui_node_define(ui, custom->node, custom->parent, custom->key, ui_content_type_custom);
    node->pos = ui_node_pos_make(custom->pos);
    node->size = ui_node_size_make(custom->size);
    node->transform = custom->transform;
    node->custom.desc = custom->desc;

    return node->handle;
}

static void ui_node_origin_from_root(struct ui* ui, struct ui_node root, struct ui_node_backing* target_node, struct ui_fx* out_x, struct ui_fx* out_y) {
    struct ui_node_backing* root_node = ui_node_check(ui, root, "the root");
    struct ui_fx x = ui_fx_zero;
    struct ui_fx y = ui_fx_zero;

    for (struct ui_node_backing* node = target_node; node != root_node; node = ui_node_get_parent(ui, node)) {
        cr_check(node != NULL, "the node is not below the root");
        x = ui_fx_add(x, node->p2.x);
        y = ui_fx_add(y, node->p2.y);
    }

    *out_x = x;
    *out_y = y;
}

void ui_release(struct ui_node root) {
    struct ui* ui = ui_ctx();

    if (ui_node_is_null(root)) {
        return;
    }

    struct ui_node_backing* root_node = ui_node_check(ui, root, "the root");

    if (root_node->parent_idx != ui_idx_invalid) {
        ui_node_remove_from_parent(ui, root_node);
    }

    ui_node_destroy_recursive(ui, root_node);
}

bool ui_is_alive(struct ui_node node) {
    struct ui* ui = ui_ctx();

    uint32_t idx = ui_node_get_idx(node);
    if (idx >= (uint32_t)cr_count(ui->node_pool.items)) {
        return false;
    }

    return ui->node_pool.items[idx].handle.raw == node.raw;
}

uint64_t ui_get_key(struct ui_node node) {
    struct ui* ui = ui_ctx();

    if (ui_node_is_null(node)) {
        return 0;
    }

    return ui_node_check(ui, node, "the node")->key;
}

uint64_t ui_derive_key(struct ui_node node, struct ui_key key) {
    struct ui* ui = ui_ctx();

    struct ui_node_backing* backing = ui_node_check(ui, node, "the node");

    if (ui_key_is_explicit(key)) {
        return ui_key_mix(backing->key, key.raw, ui_key_seed_derived_named);
    }

    return ui_key_mix(backing->key, backing->auto_derived_count++, ui_key_seed_derived_ordinal);
}

struct ui_node ui_get_root(struct ui_node node) {
    struct ui* ui = ui_ctx();

    if (ui_node_is_null(node)) {
        return ui_node_null;
    }

    struct ui_node_backing* backing = ui_node_check(ui, node, "the node");
    struct ui_node_backing* parent;

    while ((parent = ui_node_get_parent(ui, backing))) {
        backing = parent;
    }

    return backing->handle;
}

double2 ui_get_size(struct ui_node node_handle) {
    struct ui* ui = ui_ctx();

    if (ui_node_is_null(node_handle)) {
        return make_double2(0.0, 0.0);
    }

    struct ui_node_backing* node = ui_node_check(ui, node_handle, "the node");

    return make_double2(ui_fx_to_double(node->p2.width), ui_fx_to_double(node->p2.height)) / ui->to_px_from_pt;
}

double2 ui_get_origin_from_root(struct ui_node root, struct ui_node node_handle) {
    struct ui* ui = ui_ctx();

    if (ui_node_is_null(node_handle)) {
        return make_double2(0.0, 0.0);
    }

    struct ui_node_backing* target_node = ui_node_check(ui, node_handle, "the node");

    struct ui_fx x;
    struct ui_fx y;
    ui_node_origin_from_root(ui, root, target_node, &x, &y);

    return make_double2(ui_fx_to_double(x), ui_fx_to_double(y)) / ui->to_px_from_pt;
}

double2 ui_to_local_from_root(struct ui_node root, struct ui_node node_handle, double2 position) {
    struct ui* ui = ui_ctx();

    if (ui_node_is_null(node_handle)) {
        return make_double2(0.0, 0.0);
    }

    struct ui_node_backing* target_node = ui_node_check(ui, node_handle, "the node");

    struct ui_fx x;
    struct ui_fx y;
    ui_node_origin_from_root(ui, root, target_node, &x, &y);

    position -= make_double2(ui_fx_to_double(x), ui_fx_to_double(y)) / ui->to_px_from_pt;

    return position;
}

struct ui_hit_query {
    struct ui* ui;
    bool (*is_target)(struct ui_node node, void* user);
    void* user;
    struct ui_hit* chain;
    uint32_t chain_capacity;
    uint32_t chain_count;
};

static bool ui_hit_test_node(struct ui_hit_query* query, struct ui_node_backing* node, struct ui_fx event_x, struct ui_fx event_y, struct ui_fx parent_abs_x, struct ui_fx parent_abs_y) {
    struct ui_fx abs_x = ui_fx_add(parent_abs_x, node->p2.x);
    struct ui_fx abs_y = ui_fx_add(parent_abs_y, node->p2.y);

    if (is_zero_float3x2(node->transform) == false) {
        float3x2 m = node->transform;
        m.columns[2] *= (float)query->ui->to_px_from_pt;
        float det = m.columns[0].x * m.columns[1].y - m.columns[0].y * m.columns[1].x;
        if (fabsf(det) < 1e-8f) {
            return false;
        }
        float2 anchor = make_float2((float)ui_fx_to_double(abs_x), (float)ui_fx_to_double(abs_y));
        float2 event = make_float2((float)ui_fx_to_double(event_x), (float)ui_fx_to_double(event_y));
        float2 local = anchor + mul_float3x2_float2(inverse_float3x2(m), event - anchor);
        event_x = ui_fx_make_round(local.x);
        event_y = ui_fx_make_round(local.y);
    }

    struct ui_fx local_x = ui_fx_sub(event_x, abs_x);
    struct ui_fx local_y = ui_fx_sub(event_y, abs_y);

    bool is_inside = (local_x._raw >= 0 && local_y._raw >= 0 &&
                      local_x._raw < node->p2.width._raw &&
                      local_y._raw < node->p2.height._raw);

    if (!is_inside && node->type == ui_content_type_frame && node->frame.clip) {
        return false;
    }

    bool hit = false;

    for (struct ui_node_backing* child = ui_node_get_last_child(query->ui, node); child != NULL; child = ui_node_get_prev(query->ui, child)) {
        if (ui_hit_test_node(query, child, event_x, event_y, abs_x, abs_y)) {
            hit = true;
            break;
        }
    }

    if (!hit && is_inside && query->is_target(node->handle, query->user)) {
        hit = true;
    }

    if (hit) {
        cr_check(query->chain_count < query->chain_capacity, "the hit chain holds at most %u nodes", query->chain_capacity);
        query->chain[query->chain_count++] = (struct ui_hit) {
            .node = node->handle,
            .contains = is_inside,
        };
    }

    return hit;
}

uint32_t ui_hit_test(struct ui_node root, double2 position, bool (*is_target)(struct ui_node node, void* user), void* user, struct ui_hit* out_chain, uint32_t chain_capacity) {
    struct ui* ui = ui_ctx();

    if (ui_node_is_null(root)) {
        return 0;
    }

    struct ui_hit_query query = {
        .ui = ui,
        .is_target = is_target,
        .user = user,
        .chain = out_chain,
        .chain_capacity = chain_capacity,
    };

    struct ui_node_backing* root_node = ui_node_check(ui, root, "the root");

    ui_hit_test_node(&query, root_node,
                     ui_fx_make_round(position.x * ui->to_px_from_pt),
                     ui_fx_make_round(position.y * ui->to_px_from_pt),
                     ui_fx_zero, ui_fx_zero);

    return query.chain_count;
}

static const char* ui_size_mode_get_label(enum ui_size_mode mode) {
    switch (mode) {
    case ui_size_mode_fit: return "fit"; break;
    case ui_size_mode_fill: return "fill"; break;
    case ui_size_mode_fix: return "fix"; break;
    case ui_size_mode_rel: return "rel"; break;
    }
}

static const char* ui_size_anchor_get_label(enum ui_size_anchor anchor) {
    switch (anchor) {
    case ui_size_anchor_width: return "width"; break;
    case ui_size_anchor_height: return "height"; break;
    }
}

static const char* ui_pos_mode_get_label(enum ui_pos_mode mode) {
    switch (mode) {
    case ui_pos_mode_auto: return "auto"; break;
    case ui_pos_mode_rel: return "rel"; break;
    }
}

static const char* ui_pos_anchor_get_label(enum ui_pos_anchor anchor) {
    switch (anchor) {
    case ui_pos_anchor_start: return "start"; break;
    case ui_pos_anchor_center: return "center"; break;
    case ui_pos_anchor_end: return "end"; break;
    }
}

static const char* ui_layout_mode_get_label(enum ui_layout_mode mode) {
    switch (mode) {
    case ui_layout_mode_manual: return "manual"; break;
    case ui_layout_mode_stack_x: return "stack_x"; break;
    case ui_layout_mode_stack_y: return "stack_y"; break;
    }
}

static const char* ui_dist_get_label(enum ui_dist mode) {
    switch (mode) {
    case ui_dist_start: return "start"; break;
    case ui_dist_center: return "center"; break;
    case ui_dist_end: return "end"; break;
    }
}

static const char* ui_align_get_label(enum ui_align mode) {
    switch (mode) {
    case ui_align_start: return "start"; break;
    case ui_align_center: return "center"; break;
    case ui_align_end: return "end"; break;
    }
}

void ui_print(struct ui_node root) {
    struct ui* ui = ui_ctx();

    if (ui_node_is_null(root)) {
        return;
    }

    int32_t level = 0;

    for (struct ui_node_backing* node = ui_node_check(ui, root, "the root"); node; node = ui_node_get_after(ui, node, &level)) {
        const char* type;

        switch (node->type) {
        case ui_content_type_none: {
            type = "none";
        } break;
        case ui_content_type_frame: {
            type = "frame";
        } break;
        case ui_content_type_shape: {
            type = node->shape.shape_type == ui_shape_type_rect ? "rect" :
                   node->shape.shape_type == ui_shape_type_ellipse ? "ellipse" :
                   node->shape.shape_type == ui_shape_type_line ? "line" : "path";
        } break;
        case ui_content_type_text: {
            type = "text";
        } break;
        case ui_content_type_custom: {
            type = "custom";
        } break;
        }

        char content[1u << 10];
        snprintf(content,
                 sizeof(content),
                 "(x: %s %s %s %.1f, y: %s %s %s %.1f, width: %s %s %.2f %.1f, height: %s %s %.2f %.1f)",
                 ui_pos_mode_get_label(node->pos.x.mode),
                 ui_pos_anchor_get_label(node->pos.x.anchor),
                 ui_pos_anchor_get_label(node->pos.x.parent_anchor),
                 node->pos.x.constant,
                 ui_pos_mode_get_label(node->pos.y.mode),
                 ui_pos_anchor_get_label(node->pos.y.anchor),
                 ui_pos_anchor_get_label(node->pos.y.parent_anchor),
                 node->pos.y.constant,
                 ui_size_mode_get_label(node->size.width.mode),
                 ui_size_anchor_get_label(node->size.width.anchor),
                 node->size.width.multiplier,
                 node->size.width.constant,
                 ui_size_mode_get_label(node->size.height.mode),
                 ui_size_anchor_get_label(node->size.height.anchor),
                 node->size.height.multiplier,
                 node->size.height.constant);

        char p0[1u << 10];
        snprintf(p0,
                 sizeof(p0),
                 "(x: %s %s %s %f, y: %s %s %s %f, width: %s %s %.2f %f, height: %s %s %.2f %f, mode: %s, dist: %s, align: %s, spacing: %f, padding: (%f, %f, %f, %f))",
                 ui_pos_mode_get_label(node->p0.x.mode),
                 ui_pos_anchor_get_label(node->p0.x.anchor),
                 ui_pos_anchor_get_label(node->p0.x.parent_anchor),
                 ui_fx_to_double(node->p0.x.constant),
                 ui_pos_mode_get_label(node->p0.y.mode),
                 ui_pos_anchor_get_label(node->p0.y.anchor),
                 ui_pos_anchor_get_label(node->p0.y.parent_anchor),
                 ui_fx_to_double(node->p0.y.constant),
                 ui_size_mode_get_label(node->p0.width.mode),
                 ui_size_anchor_get_label(node->p0.width.anchor),
                 node->p0.width.multiplier,
                 ui_fx_to_double(node->p0.width.constant),
                 ui_size_mode_get_label(node->p0.height.mode),
                 ui_size_anchor_get_label(node->p0.height.anchor),
                 node->p0.height.multiplier,
                 ui_fx_to_double(node->p0.height.constant),
                 ui_layout_mode_get_label(node->p0.mode),
                 ui_dist_get_label(node->p0.dist),
                 ui_align_get_label(node->p0.align),
                 ui_fx_to_double(node->p0.spacing),
                 ui_fx_to_double(node->p0.padding_top),
                 ui_fx_to_double(node->p0.padding_bottom),
                 ui_fx_to_double(node->p0.padding_left),
                 ui_fx_to_double(node->p0.padding_right));

        char p1[1u << 10];
        snprintf(p1,
                 sizeof(p1),
                 "(fill_count: %u, min_width: %f, min_height: %f)",
                 node->p1.fill_count,
                 ui_fx_to_double(node->p1.min_width),
                 ui_fx_to_double(node->p1.min_height));

        char p2[1u << 10];
        snprintf(p2,
                 sizeof(p2),
                 "(x: %f, y: %f, width: %f, height: %f)",
                 ui_fx_to_double(node->p2.x),
                 ui_fx_to_double(node->p2.y),
                 ui_fx_to_double(node->p2.width),
                 ui_fx_to_double(node->p2.height));

        char temp[1u << 10];
        uint32_t offset = 0;
        for (int32_t i = 0; i < level && offset + 2 < sizeof(temp); i++) {
            offset += (uint32_t)snprintf(temp + offset, sizeof(temp) - offset, "--");
        }
        if (offset < sizeof(temp)) {
            snprintf(temp + offset, sizeof(temp) - offset, "type: %s, key: %016llx, content: %s, p0: %s, p1: %s, p2: %s\n", type, (unsigned long long)node->key, content, p0, p1, p2);
        }
        printf("%s", temp);
    }
}
