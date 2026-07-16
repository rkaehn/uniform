#include "ui_tests.h"

#if cr_ios
#import <Foundation/Foundation.h>

@interface SwiftUIShapesBridge : NSObject
+ (void)presentCompareWithWidth:(uint32_t)width height:(uint32_t)height items:(const struct ui_test_sui_item*)items itemCount:(uint32_t)itemCount background:(const float*)background toPxFromPt:(CGFloat)toPxFromPt;
+ (void)showHDRImageWithWidth:(uint32_t)width height:(uint32_t)height rgba:(const float*)rgba;
@end

void ui_test_sui_present_(const struct ui_test_sui_spec* spec, uint32_t width, uint32_t height, float to_px_from_pt) {
    [SwiftUIShapesBridge presentCompareWithWidth:width height:height
                                           items:spec->items itemCount:spec->item_count
                                      background:(const float*)&spec->background toPxFromPt:to_px_from_pt];
}

void ui_test_scene_present_raw_(uint32_t width, uint32_t height, const float* rgba) {
    [SwiftUIShapesBridge showHDRImageWithWidth:width height:height rgba:rgba];
}
#endif

static struct ui_color ui_test_sui_color(float4 value) {
    return ui_color(value, ui_color_space_srgb);
}

static struct ui_fill ui_test_sui_fill(float4 value) {
    return value.w > 0.0f ? ui_fill_solid(ui_test_sui_color(value)) : (struct ui_fill) { 0 };
}

static struct ui_outline ui_test_sui_outline(const struct ui_test_sui_item* item) {
    if (item->stroke_width <= 0.0f) {
        return (struct ui_outline) { 0 };
    }
    return ui_outline_solid(item->stroke_align, item->stroke_width, ui_test_sui_color(item->stroke));
}

static struct ui_corners ui_test_sui_corners(const struct ui_test_sui_item* item) {
    return (struct ui_corners) {
        .profile = item->circular ? ui_corner_profile_circular : ui_corner_profile_continuous,
        .radius = item->radius,
    };
}

static struct ui_blend ui_test_sui_blend(float opacity) {
    if (opacity <= 0.0f) {
        return (struct ui_blend) { 0 };
    }
    return (struct ui_blend) { .mode = ui_blend_mode_normal, .opacity = opacity };
}

static float3x2 ui_test_sui_transform(const struct ui_test_sui_item* item) {
    bool zero = item->transform_x.x == 0.0 && item->transform_x.y == 0.0 &&
                item->transform_y.x == 0.0 && item->transform_y.y == 0.0 &&
                item->transform_t.x == 0.0 && item->transform_t.y == 0.0;
    if (zero) {
        return (float3x2) { 0 };
    }
    return make_float3x2(to_float2_from_double2(item->transform_x),
                         to_float2_from_double2(item->transform_y),
                         to_float2_from_double2(item->transform_t));
}

static struct ui_shadow ui_test_sui_shadow(const struct ui_test_sui_item* item) {
    if (item->shadow.w <= 0.0f) {
        return (struct ui_shadow) { 0 };
    }
    return ui_shadow(item->shadow_offset.x, item->shadow_offset.y, item->shadow_radius,
                     ui_test_sui_color(item->shadow));
}

static struct ui_layer_blur ui_test_sui_blur(const struct ui_test_sui_item* item) {
    return item->blur > 0.0f ? ui_blur(item->blur) : (struct ui_layer_blur) { 0 };
}

static void ui_test_sui_item_node(struct ui_node parent, const struct ui_test_sui_item* item, double2 shift, double2 canvas) {
    double2 pos = item->origin - shift;
    double2 size = item->size;

    if (item->kind == ui_test_sui_kind_rect) {
        ui_frame(&(struct ui_frame) {
            .parent = parent,
            .pos = ui_pos_xy(pos.x, pos.y),
            .size = ui_size_fix(size.x, size.y),
            .transform = ui_test_sui_transform(item),
            .corners = ui_test_sui_corners(item),
            .fill = ui_test_sui_fill(item->fill),
            .outline = ui_test_sui_outline(item),
            .shadow = ui_test_sui_shadow(item),
            .blur = ui_test_sui_blur(item),
            .blend = ui_test_sui_blend(item->opacity),
        });
        return;
    }

    bool wrapped = item->shadow.w > 0.0f || item->blur > 0.0f || item->opacity > 0.0f;

    if (item->kind == ui_test_sui_kind_ellipse) {
        struct ui_node shape_parent = parent;
        double2 shape_pos = pos;
        if (wrapped) {
            shape_parent = ui_frame(&(struct ui_frame) {
                .parent = parent,
                .pos = ui_pos_xy(pos.x, pos.y),
                .size = ui_size_fix(size.x, size.y),
                .shadow = ui_test_sui_shadow(item),
                .blur = ui_test_sui_blur(item),
                .blend = ui_test_sui_blend(item->opacity),
            });
            shape_pos = make_double2(0.0, 0.0);
        }
        ui_ellipse(&(struct ui_ellipse) {
            .parent = shape_parent,
            .pos = ui_pos_xy(shape_pos.x, shape_pos.y),
            .size = ui_size_fix(size.x, size.y),
            .transform = ui_test_sui_transform(item),
            .fill = ui_test_sui_fill(item->fill),
            .outline = ui_test_sui_outline(item),
        });
        return;
    }

    struct ui_node shape_parent = parent;
    double2 path_pos = -shift;
    if (wrapped) {
        shape_parent = ui_frame(&(struct ui_frame) {
            .parent = parent,
            .pos = ui_pos_xy(path_pos.x, path_pos.y),
            .size = ui_size_fix(canvas.x, canvas.y),
            .shadow = ui_test_sui_shadow(item),
            .blur = ui_test_sui_blur(item),
            .blend = ui_test_sui_blend(item->opacity),
        });
        path_pos = make_double2(0.0, 0.0);
    }
    ui_shape(&(struct ui_shape) {
        .parent = shape_parent,
        .pos = ui_pos_xy(path_pos.x, path_pos.y),
        .size = ui_size_fix(canvas.x, canvas.y),
        .mode = item->path_thickness > 0.0f ? ui_shape_mode_stroke : ui_shape_mode_fill,
        .thickness = item->path_thickness,
        .path = item->path,
        .fill = ui_test_sui_fill(item->fill),
    });
}

struct ui_test_sui_scene_state {
    struct ui_test_scene_state base;
    struct ui_test_sui_spec spec;
};

struct ui_node ui_test_sui_scene_build_(struct ui_test_scene_state* state, struct ui_node parent, double2 size, const struct ui_test_scene* scene) {
    struct ui_test_sui_scene_state* sui = (struct ui_test_sui_scene_state*)state;
    return ui_test_sui_build_(parent, size, &sui->spec);
}

#if cr_ios
void ui_test_sui_scene_present_(struct ui_test_scene_state* state, uint32_t width, uint32_t height, float to_px_from_pt) {
    struct ui_test_sui_scene_state* sui = (struct ui_test_sui_scene_state*)state;
    ui_test_sui_present_(&sui->spec, width, height, to_px_from_pt);
}
#endif

static void ui_test_sui_scene_destroy_(struct ui_test_scene_state* state) {
    struct ui_test_sui_scene_state* sui = (struct ui_test_sui_scene_state*)state;

    for (uint32_t i = 0; i < sui->spec.item_count; i++) {
        if (sui->spec.items[i].kind == ui_test_sui_kind_path) {
            ui_path_destroy(sui->spec.items[i].path);
        }
    }

    ui_test_scene_state_free_(state);
}

struct ui_test_scene_state* ui_test_sui_scene_create_(struct ui_test_scenes* scenes, struct cr_mem* mem, const void* ctx) {
    const struct ui_test_sui_scene_desc* desc = ctx;
    uint64_t state_size = (sizeof(struct ui_test_sui_scene_state) + 15ull) & ~15ull;
    uint64_t items_size = (uint64_t)desc->item_max * sizeof(struct ui_test_sui_item);

    struct ui_test_sui_scene_state* sui = cr_malloc(mem, state_size + items_size, 16, cr_loc);
    memset(sui, 0, (size_t)(state_size + items_size));
    sui->base = (struct ui_test_scene_state) {
        .scenes = scenes,
        .mem = mem,
        .destroy = ui_test_sui_scene_destroy_,
    };

    struct ui_test_sui_item* items = (struct ui_test_sui_item*)((uint8_t*)sui + state_size);
    desc->fill(&sui->spec, items);
    assert(sui->spec.item_count <= desc->item_max);

    return &sui->base;
}

struct ui_node ui_test_sui_build_(struct ui_node parent, double2 size, const struct ui_test_sui_spec* spec) {
    struct ui_node root = ui_frame(&(struct ui_frame) {
        .parent = parent,
        .size = ui_size_fix(size.x, size.y),
        .fill = ui_test_sui_fill(spec->background),
        .clip = true,
    });

    uint32_t i = 0;
    while (i < spec->item_count) {
        const struct ui_test_sui_item* item = &spec->items[i];
        if (item->group == 0) {
            ui_test_sui_item_node(root, item, make_double2(0.0, 0.0), size);
            i++;
            continue;
        }

        uint32_t first = i;
        while (i < spec->item_count && spec->items[i].group == item->group) {
            i++;
        }

        if (item->group_clip) {
            struct ui_node container = ui_frame(&(struct ui_frame) {
                .parent = root,
                .pos = ui_pos_xy(item->origin.x, item->origin.y),
                .size = ui_size_fix(item->size.x, item->size.y),
                .transform = ui_test_sui_transform(item),
                .corners = ui_test_sui_corners(item),
                .fill = ui_test_sui_fill(item->fill),
                .clip = true,
                .shadow = ui_test_sui_shadow(item),
                .blur = ui_test_sui_blur(item),
                .blend = ui_test_sui_blend(item->group_opacity),
            });
            for (uint32_t m = first + 1; m < i; m++) {
                ui_test_sui_item_node(container, &spec->items[m], item->origin, size);
            }
        } else {
            struct ui_node group = ui_frame(&(struct ui_frame) {
                .parent = root,
                .size = ui_size_fix(size.x, size.y),
                .transform = ui_test_sui_transform(item),
                .shadow = ui_test_sui_shadow(item),
                .blur = ui_test_sui_blur(item),
                .blend = ui_test_sui_blend(item->group_opacity),
            });
            struct ui_test_sui_item head = *item;
            head.shadow.w = 0.0f;
            head.blur = 0.0f;
            head.opacity = 0.0f;
            head.transform_x = make_double2(0.0, 0.0);
            head.transform_y = make_double2(0.0, 0.0);
            head.transform_t = make_double2(0.0, 0.0);
            ui_test_sui_item_node(group, &head, make_double2(0.0, 0.0), size);
            for (uint32_t m = first + 1; m < i; m++) {
                ui_test_sui_item_node(group, &spec->items[m], make_double2(0.0, 0.0), size);
            }
        }
    }

    return root;
}
