#include "ui_tests.h"
#include <math.h>

static float4 ui_test_layer_palette_a(uint32_t i, float alpha) {
    float4 color = ui_test_palette_(i);
    color.w = alpha;
    return color;
}

static struct ui_test_sui_item ui_test_layer_rect(double x, double y, double w, double h, double radius, float4 fill) {
    return (struct ui_test_sui_item) {
        .kind = ui_test_sui_kind_rect,
        .origin = make_double2(x, y),
        .size = make_double2(w, h),
        .radius = make_double4(radius, radius, radius, radius),
        .fill = fill,
    };
}

static struct ui_test_sui_item ui_test_layer_ellipse(double x, double y, double w, double h, float4 fill) {
    return (struct ui_test_sui_item) {
        .kind = ui_test_sui_kind_ellipse,
        .origin = make_double2(x, y),
        .size = make_double2(w, h),
        .fill = fill,
    };
}

static void ui_test_scene_layer_opacity_fill(struct ui_test_sui_spec* spec, struct ui_test_sui_item* items) {
    uint32_t count = 0;

    items[count++] = ui_test_layer_rect(32.0, 32.0, 960.0, 160.0, 24.0, ui_system_background.value);
    for (uint32_t i = 0; i < 10; i++) {
        struct ui_test_sui_item item = ui_test_layer_rect(64.0 + (double)i * 92.0, 76.0, 72.0, 72.0, 16.0, ui_system_blue.value);
        item.opacity = 0.1f * (float)(i + 1);
        items[count++] = item;
    }

    items[count++] = ui_test_layer_ellipse(64.0, 280.0, 180.0, 180.0, ui_test_layer_palette_a(7, 0.6f));
    items[count++] = ui_test_layer_ellipse(188.0, 280.0, 180.0, 180.0, ui_test_layer_palette_a(0, 0.6f));

    struct ui_test_sui_item grouped_a = ui_test_layer_ellipse(480.0, 280.0, 180.0, 180.0, ui_test_palette_(7));
    grouped_a.group = 1;
    grouped_a.group_opacity = 0.6f;
    items[count++] = grouped_a;
    struct ui_test_sui_item grouped_b = ui_test_layer_ellipse(604.0, 280.0, 180.0, 180.0, ui_test_palette_(0));
    grouped_b.group = 1;
    items[count++] = grouped_b;

    struct ui_test_sui_item pair_rect = ui_test_layer_rect(64.0, 540.0, 220.0, 150.0, 28.0, ui_test_layer_palette_a(3, 0.5f));
    items[count++] = pair_rect;
    items[count++] = ui_test_layer_ellipse(200.0, 570.0, 150.0, 150.0, ui_test_layer_palette_a(9, 0.5f));

    struct ui_test_sui_item grouped_rect = ui_test_layer_rect(480.0, 540.0, 220.0, 150.0, 28.0, ui_test_palette_(3));
    grouped_rect.group = 2;
    grouped_rect.group_opacity = 0.5f;
    items[count++] = grouped_rect;
    struct ui_test_sui_item grouped_circle = ui_test_layer_ellipse(616.0, 570.0, 150.0, 150.0, ui_test_palette_(9));
    grouped_circle.group = 2;
    items[count++] = grouped_circle;

    for (uint32_t i = 0; i < 6; i++) {
        struct ui_test_sui_item card = ui_test_layer_rect(64.0 + (double)i * 120.0, 760.0 + (double)(i % 2) * 24.0,
                                                         220.0, 200.0, 32.0, ui_test_layer_palette_a(i * 2, 0.5f));
        items[count++] = card;
    }

    assert(count <= 48);
    *spec = (struct ui_test_sui_spec) { .background = ui_secondary_system_background.value, .items = items, .item_count = count };
}

static void ui_test_scene_layer_clip_fill(struct ui_test_sui_spec* spec, struct ui_test_sui_item* items) {
    uint32_t count = 0;

    struct ui_test_sui_item container_a = ui_test_layer_rect(48.0, 48.0, 420.0, 420.0, 0.0, ui_system_background.value);
    container_a.group = 1;
    container_a.group_clip = true;
    items[count++] = container_a;
    struct ui_test_sui_item star = { .kind = ui_test_sui_kind_path, .path = ui_path_create() };
    for (uint32_t i = 0; i < 10; i++) {
        double radius = i % 2 == 0 ? 120.0 : 52.0;
        double angle = -0.5 * M_PI + (double)i * 0.2 * M_PI;
        double2 p = make_double2(120.0 + radius * cos(angle), 120.0 + radius * sin(angle));
        if (i == 0) {
            ui_path_move_to(star.path, p);
        } else {
            ui_path_line_to(star.path, p);
        }
    }
    ui_path_close(star.path);
    star.fill = ui_test_palette_(9);
    star.group = 1;
    items[count++] = star;
    struct ui_test_sui_item circle_a = ui_test_layer_ellipse(300.0, 120.0, 300.0, 300.0, ui_test_palette_(1));
    circle_a.group = 1;
    items[count++] = circle_a;
    struct ui_test_sui_item rect_a = ui_test_layer_rect(120.0, 380.0, 200.0, 160.0, 24.0, ui_test_palette_(7));
    rect_a.group = 1;
    items[count++] = rect_a;

    struct ui_test_sui_item container_b = ui_test_layer_rect(552.0, 48.0, 420.0, 420.0, 0.0, ui_system_background.value);
    container_b.group = 2;
    container_b.group_clip = true;
    items[count++] = container_b;
    for (uint32_t i = 0; i < 10; i++) {
        struct ui_test_sui_item stripe = ui_test_layer_rect(540.0, 60.0 + (double)i * 42.0, 444.0, 30.0, 0.0, ui_test_palette_(i));
        stripe.group = 2;
        items[count++] = stripe;
    }

    struct ui_test_sui_item container_c = ui_test_layer_rect(48.0, 552.0, 420.0, 420.0, 0.0, ui_system_background.value);
    container_c.group = 3;
    container_c.group_clip = true;
    container_c.group_opacity = 0.5f;
    items[count++] = container_c;
    struct ui_test_sui_item ellipse_c = ui_test_layer_ellipse(-40.0, 600.0, 360.0, 220.0, ui_test_palette_(4));
    ellipse_c.group = 3;
    items[count++] = ellipse_c;
    struct ui_test_sui_item ellipse_c2 = ui_test_layer_ellipse(240.0, 760.0, 320.0, 320.0, ui_test_palette_(10));
    ellipse_c2.group = 3;
    items[count++] = ellipse_c2;
    struct ui_test_sui_item rect_c = ui_test_layer_rect(120.0, 500.0, 200.0, 200.0, 32.0, ui_test_palette_(6));
    rect_c.group = 3;
    items[count++] = rect_c;

    struct ui_test_sui_item container_d = ui_test_layer_rect(552.0, 552.0, 420.0, 420.0, 0.0, ui_system_background.value);
    container_d.group = 4;
    container_d.group_clip = true;
    items[count++] = container_d;
    for (uint32_t i = 0; i < 64; i++) {
        struct ui_test_sui_item chip = ui_test_layer_rect(528.0 + (double)(i % 8) * 60.0, 528.0 + (double)(i / 8) * 60.0,
                                                         44.0, 44.0, 10.0, ui_test_palette_(i % 12));
        chip.group = 4;
        items[count++] = chip;
    }

    assert(count <= 96);
    *spec = (struct ui_test_sui_spec) { .background = ui_secondary_system_background.value, .items = items, .item_count = count };
}

static void ui_test_scene_layer_blend_fill(struct ui_test_sui_spec* spec, struct ui_test_sui_item* items) {
    uint32_t count = 0;

    for (uint32_t i = 0; i < 16; i++) {
        items[count++] = ui_test_layer_rect((double)i * 64.0, 0.0, 64.0, 1024.0, 0.0, ui_test_layer_palette_a(i, 0.35f));
    }

    static const struct { double x, y; float alpha; } cards[8] = {
        { 96.0, 96.0, 0.4f }, { 288.0, 160.0, 0.55f }, { 480.0, 96.0, 0.7f }, { 576.0, 224.0, 0.4f },
        { 160.0, 320.0, 0.7f }, { 384.0, 384.0, 0.55f }, { 96.0, 480.0, 0.4f }, { 512.0, 480.0, 0.7f },
    };
    for (uint32_t i = 0; i < 8; i++) {
        items[count++] = ui_test_layer_rect(cards[i].x, cards[i].y, 360.0, 280.0, 32.0,
                                            ui_test_layer_palette_a(i, cards[i].alpha));
    }

    for (uint32_t i = 0; i < 10; i++) {
        items[count++] = ui_test_layer_ellipse(32.0 + (double)i * 92.0, 812.0, 140.0, 140.0,
                                               ui_test_layer_palette_a(i, 0.5f));
    }

    items[count++] = ui_test_layer_rect(640.0, 128.0, 320.0, 560.0, 24.0, make_float4(0.0f, 0.0f, 0.0f, 0.25f));
    items[count++] = ui_test_layer_rect(64.0, 640.0, 480.0, 120.0, 24.0, make_float4(1.0f, 1.0f, 1.0f, 0.5f));

    assert(count <= 40);
    *spec = (struct ui_test_sui_spec) { .background = ui_system_background.value, .items = items, .item_count = count };
}

#if cr_ios
#import <Foundation/Foundation.h>
#import <CoreGraphics/CoreGraphics.h>
#import <CoreText/CoreText.h>
#endif

#if cr_ios
@interface SwiftUIShapesBridge : NSObject
+ (void)presentStyledWithWidth:(uint32_t)width height:(uint32_t)height nodes:(const struct ui_test_style_node*)nodes nodeCount:(uint32_t)nodeCount background:(const float*)background toPxFromPt:(CGFloat)toPxFromPt;
@end
#endif

#if cr_ios
static CGPathRef ui_test_layer_cg_path(const struct ui_path* src) {
    CGMutablePathRef path = CGPathCreateMutable();
    const struct ui_path_component* components = ui_path_get_components(src);

    for (uint32_t i = 0, count = ui_path_get_count(src); i < count; i++) {
        const struct ui_path_component* component = &components[i];

        switch (component->type) {
        case ui_path_component_type_move: {
            CGPathMoveToPoint(path, NULL, component->move.p.x, component->move.p.y);
        } break;
        case ui_path_component_type_line: {
            CGPathAddLineToPoint(path, NULL, component->line.p.x, component->line.p.y);
        } break;
        case ui_path_component_type_cubic: {
            CGPathAddCurveToPoint(path, NULL, component->cubic.c1.x, component->cubic.c1.y, component->cubic.c2.x, component->cubic.c2.y, component->cubic.p.x, component->cubic.p.y);
        } break;
        case ui_path_component_type_close: {
            CGPathCloseSubpath(path);
        } break;
        }
    }

    return path;
}

static void ui_test_layer_fill_rounded_rect(CGContextRef ctx, float x, float y, float w, float h, float radius, bool continuous, struct ui_color color) {
    ui_test_ref_set_fill_(ctx, color);

    if (radius <= 0.0f) {
        CGContextFillRect(ctx, CGRectMake(x, y, w, h));
        return;
    }

    struct ui_path* path = ui_path_create();
    ui_path_rounded_rect(path, make_double2(x, y), make_double2(w, h),
                         (struct ui_corners) {
                             .profile = continuous ? ui_corner_profile_continuous : ui_corner_profile_circular,
                             .radius = make_double4(radius, radius, radius, radius),
                         });
    CGPathRef cg_path = ui_test_layer_cg_path(path);
    ui_path_destroy(path);

    CGContextAddPath(ctx, cg_path);
    CGContextFillPath(ctx);
    CGPathRelease(cg_path);
}
#endif


#define ui_test_layer_bg { 0.114f, 0.153f, 0.2f, 1.0f }

static const struct ui_test_style_node ui_test_layer_fill_stack_nodes_[] = {
    {
        .origin = { 64.0, 64.0 }, .size = { 384.0, 288.0 }, .radius = 32.0, .continuous = true,
        .fill = {
            { .type = ui_test_style_layer_solid, .color0 = { 0.294f, 0.624f, 0.851f, 1.0f } },
            { .type = ui_test_style_layer_solid, .opacity = 0.5f, .color0 = { 0.851f, 0.416f, 0.294f, 1.0f } },
            { .type = ui_test_style_layer_solid, .opacity = 0.25f, .color0 = { 0.851f, 0.788f, 0.294f, 1.0f } },
        },
    },
    {
        .origin = { 96.0, 384.0 }, .size = { 96.0, 96.0 }, .circle = true,
        .fill = {
            { .type = ui_test_style_layer_solid, .color0 = { 0.9f, 0.9f, 0.9f, 1.0f } },
            { .type = ui_test_style_layer_solid, .opacity = 0.6f, .color0 = { 0.2f, 0.5f, 0.3f, 1.0f } },
        },
    },
    {
        .origin = { 256.0, 384.0 }, .size = { 192.0, 96.0 },
        .fill = {
            { .type = ui_test_style_layer_solid, .opacity = 0.35f, .color0 = { 1.0f, 1.0f, 1.0f, 1.0f } },
            { .type = ui_test_style_layer_solid, .opacity = 0.35f, .color0 = { 1.0f, 1.0f, 1.0f, 1.0f } },
        },
    },
};

static const struct ui_test_style_node ui_test_layer_gradient_mix_nodes_[] = {
    {
        .origin = { 48.0, 48.0 }, .size = { 416.0, 200.0 }, .radius = 24.0, .continuous = true,
        .fill = {
            { .type = ui_test_style_layer_gradient, .direction = ui_gradient_direction_right, .color0 = { 0.294f, 0.624f, 0.851f, 1.0f }, .color1 = { 0.161f, 0.239f, 0.4f, 1.0f } },
            { .type = ui_test_style_layer_solid, .opacity = 0.3f, .color0 = { 1.0f, 1.0f, 1.0f, 1.0f } },
        },
        .outline = {
            { .type = ui_test_style_layer_solid, .color0 = { 0.08f, 0.1f, 0.14f, 1.0f } },
        },
        .outline_align = ui_outline_align_inside, .outline_thickness = 4.0,
    },
    {
        .origin = { 48.0, 288.0 }, .size = { 416.0, 176.0 }, .radius = 24.0, .continuous = true,
        .fill = {
            { .type = ui_test_style_layer_gradient, .direction = ui_gradient_direction_down, .color0 = { 0.851f, 0.788f, 0.294f, 1.0f }, .color1 = { 0.851f, 0.788f, 0.294f, 0.0f } },
            { .type = ui_test_style_layer_gradient, .direction = ui_gradient_direction_up, .opacity = 0.5f, .color0 = { 0.294f, 0.624f, 0.851f, 1.0f }, .color1 = { 0.294f, 0.624f, 0.851f, 0.0f } },
        },
    },
};

static const struct ui_test_style_node ui_test_layer_outline_stack_nodes_[] = {
    {
        .origin = { 96.0, 64.0 }, .size = { 320.0, 168.0 }, .radius = 40.0, .continuous = true,
        .fill = {
            { .type = ui_test_style_layer_solid, .opacity = 0.4f, .color0 = { 0.294f, 0.624f, 0.851f, 1.0f } },
        },
        .outline = {
            { .type = ui_test_style_layer_solid, .color0 = { 0.851f, 0.788f, 0.294f, 1.0f } },
            { .type = ui_test_style_layer_gradient, .direction = ui_gradient_direction_down, .opacity = 0.6f, .color0 = { 0.05f, 0.05f, 0.08f, 1.0f }, .color1 = { 0.05f, 0.05f, 0.08f, 0.0f } },
        },
        .outline_align = ui_outline_align_center, .outline_thickness = 20.0,
    },
    {
        .origin = { 176.0, 296.0 }, .size = { 160.0, 160.0 }, .circle = true,
        .outline = {
            { .type = ui_test_style_layer_gradient, .direction = ui_gradient_direction_right, .color0 = { 0.851f, 0.416f, 0.294f, 1.0f }, .color1 = { 0.294f, 0.851f, 0.624f, 1.0f } },
        },
        .outline_align = ui_outline_align_inside, .outline_thickness = 28.0,
    },
};

static const struct ui_test_style_node ui_test_layer_outline_align_nodes_[] = {
    {
        .origin = { 64.0, 64.0 }, .size = { 128.0, 128.0 }, .radius = 28.0, .continuous = true,
        .fill = { { .type = ui_test_style_layer_solid, .color0 = { 0.4f, 0.44f, 0.5f, 1.0f } } },
        .outline = { { .type = ui_test_style_layer_solid, .opacity = 0.6f, .color0 = { 0.95f, 0.95f, 0.95f, 1.0f } } },
        .outline_align = ui_outline_align_inside, .outline_thickness = 12.0,
    },
    {
        .origin = { 192.0, 288.0 }, .size = { 128.0, 128.0 }, .radius = 28.0, .continuous = true,
        .fill = { { .type = ui_test_style_layer_solid, .color0 = { 0.4f, 0.44f, 0.5f, 1.0f } } },
        .outline = { { .type = ui_test_style_layer_solid, .opacity = 0.6f, .color0 = { 0.95f, 0.95f, 0.95f, 1.0f } } },
        .outline_align = ui_outline_align_center, .outline_thickness = 12.0,
    },
    {
        .origin = { 320.0, 64.0 }, .size = { 128.0, 128.0 }, .radius = 28.0, .continuous = true,
        .fill = { { .type = ui_test_style_layer_solid, .color0 = { 0.4f, 0.44f, 0.5f, 1.0f } } },
        .outline = { { .type = ui_test_style_layer_solid, .opacity = 0.6f, .color0 = { 0.95f, 0.95f, 0.95f, 1.0f } } },
        .outline_align = ui_outline_align_outside, .outline_thickness = 12.0,
    },
};

static const struct ui_test_style_node ui_test_layer_order_nodes_[] = {
    {
        .origin = { 96.0, 96.0 }, .size = { 320.0, 320.0 }, .radius = 48.0, .continuous = true,
        .fill = { { .type = ui_test_style_layer_solid, .color0 = { 0.176f, 0.29f, 0.227f, 1.0f } } },
        .outline = { { .type = ui_test_style_layer_solid, .opacity = 0.85f, .color0 = { 0.851f, 0.788f, 0.294f, 1.0f } } },
        .outline_align = ui_outline_align_inside, .outline_thickness = 24.0,
        .has_child = true,
        .child_origin = { 8.0, 120.0 }, .child_size = { 304.0, 80.0 }, .child_color = { 0.294f, 0.624f, 0.851f, 1.0f },
    },
};

static const struct ui_test_style_node ui_test_layer_shadow_nodes_[] = {
    {
        .origin = { 80.0, 80.0 }, .size = { 152.0, 152.0 }, .radius = 28.0, .continuous = true,
        .fill = { { .type = ui_test_style_layer_solid, .color0 = { 0.933f, 0.933f, 0.933f, 1.0f } } },
        .has_shadow = true,
        .shadow_offset = { 0.0, 10.0 }, .shadow_radius = 6.0,
        .shadow_color = { 0.0f, 0.0f, 0.0f, 0.5f },
    },
    {
        .origin = { 296.0, 80.0 }, .size = { 152.0, 152.0 }, .radius = 76.0,
        .fill = { { .type = ui_test_style_layer_solid, .color0 = { 0.294f, 0.624f, 0.851f, 1.0f } } },
        .has_shadow = true,
        .shadow_offset = { 12.0, 6.0 }, .shadow_radius = 4.0,
        .shadow_color = { 0.063f, 0.125f, 0.251f, 0.6f },
    },
    {
        .origin = { 80.0, 300.0 }, .size = { 368.0, 132.0 }, .radius = 24.0, .continuous = true,
        .fill = { { .type = ui_test_style_layer_solid, .color0 = { 0.933f, 0.933f, 0.933f, 1.0f } } },
        .has_shadow = true,
        .shadow_offset = { 0.0, 8.0 }, .shadow_radius = 8.0,
        .shadow_color = { 0.0f, 0.0f, 0.0f, 0.45f },
        .node_opacity = 0.55f,
    },
};

static const struct ui_test_style_node ui_test_layer_blur_nodes_[] = {
    {
        .origin = { 96.0, 72.0 }, .size = { 320.0, 160.0 }, .radius = 32.0, .continuous = true,
        .fill = { { .type = ui_test_style_layer_solid, .color0 = { 0.294f, 0.624f, 0.851f, 1.0f } } },
        .outline = { { .type = ui_test_style_layer_solid, .color0 = { 0.851f, 0.788f, 0.294f, 1.0f } } },
        .outline_align = ui_outline_align_inside, .outline_thickness = 12.0,
        .blur = 3.0, .blur_radius_swiftui = 3.5,
    },
    {
        .origin = { 96.0, 292.0 }, .size = { 320.0, 160.0 }, .radius = 32.0, .continuous = true,
        .fill = { { .type = ui_test_style_layer_solid, .color0 = { 0.933f, 0.933f, 0.933f, 1.0f } } },
        .has_shadow = true,
        .shadow_offset = { 0.0, 10.0 }, .shadow_radius = 6.0,
        .shadow_color = { 0.0f, 0.0f, 0.0f, 0.55f },
        .blur = 3.0, .blur_radius_swiftui = 3.5,
    },
};

static struct ui_layer ui_test_style_to_layer(const struct ui_test_style_layer* l) {
    struct ui_layer layer;

    if (l->type == ui_test_style_layer_gradient) {
        layer = ui_layer_gradient((enum ui_gradient_direction)l->direction,
                                  ui_color(l->color0, ui_color_space_srgb),
                                  ui_color(l->color1, ui_color_space_srgb));
    } else {
        layer = ui_layer_solid(ui_color(l->color0, ui_color_space_srgb));
    }

    if (l->opacity > 0.0f && l->opacity < 1.0f) {
        layer = ui_layer_opacity(layer, l->opacity);
    }

    return layer;
}

static struct ui_node ui_test_scene_style_build(struct ui_node parent, double2 size,
                                                  const struct ui_test_style_node* nodes, uint32_t node_count, float4 background) {
    struct ui_node root = ui_frame(&(struct ui_frame) {
        .parent = parent,
        .size = ui_size_fix(size.x, size.y),
        .fill = ui_fill_solid(ui_color(background, ui_color_space_srgb)),
        .clip = true,
    });

    for (uint32_t i = 0; i < node_count; i++) {
        const struct ui_test_style_node* n = &nodes[i];

        struct ui_fill fill = { 0 };
        struct ui_outline outline = {
            .align = (enum ui_outline_align)n->outline_align,
            .thickness = n->outline_thickness,
        };
        for (uint32_t k = 0; k < ui_test_style_max_layers; k++) {
            if (n->fill[k].type != ui_test_style_layer_none) {
                fill.layers[k] = ui_test_style_to_layer(&n->fill[k]);
            }
            if (n->outline[k].type != ui_test_style_layer_none) {
                outline.layers[k] = ui_test_style_to_layer(&n->outline[k]);
            }
        }

        struct ui_shadow shadow = { 0 };
        if (n->has_shadow) {
            shadow = ui_shadow(n->shadow_offset.x, n->shadow_offset.y, n->shadow_radius,
                               ui_color(n->shadow_color, ui_color_space_srgb));
        }

        struct ui_frame desc = {
            .parent = root,
            .pos = ui_pos_xy(n->origin.x, n->origin.y),
            .size = ui_size_fix(n->size.x, n->size.y),
            .fill = fill,
            .outline = outline,
            .shadow = shadow,
            .blur = ui_blur(n->blur),
            .clip = n->clip,
        };

        if (n->circle) {
            desc.corners = ui_corners_circular(n->size.x * 0.5);
        } else if (n->radius > 0.0) {
            desc.corners = ui_corners(n->radius);
            desc.corners.profile = n->continuous ? ui_corner_profile_continuous : ui_corner_profile_circular;
        }

        if (n->node_opacity > 0.0f && n->node_opacity < 1.0f) {
            desc.blend = (struct ui_blend) { .mode = ui_blend_mode_normal, .opacity = n->node_opacity };
        }

        struct ui_node node = ui_frame(&desc);

        if (n->has_child) {
            ui_rect(&(struct ui_rect) {
                .parent = node,
                .pos = ui_pos_xy(n->child_origin.x, n->child_origin.y),
                .size = ui_size_fix(n->child_size.x, n->child_size.y),
                .fill = ui_fill_solid(ui_color(n->child_color, ui_color_space_srgb)),
            });
        }
    }

    return root;
}

#if cr_ios
static void ui_test_scene_style_present(const struct ui_test_style_node* nodes, uint32_t node_count, float4 background,
                                        uint32_t width, uint32_t height, float to_px_from_pt) {
    float bg[4] = { background.x, background.y, background.z, background.w };
    [SwiftUIShapesBridge presentStyledWithWidth:width height:height nodes:nodes nodeCount:node_count background:bg toPxFromPt:to_px_from_pt];
}
#endif

#define ui_test_layer_scene(name) \
static struct ui_node ui_test_layer_##name##_build(struct ui_test_scene_state* state, struct ui_node parent, double2 size, const struct ui_test_scene* scene) { \
    return ui_test_scene_style_build(parent, size, ui_test_layer_##name##_nodes_, cr_count(ui_test_layer_##name##_nodes_), (float4)ui_test_layer_bg); \
}

#if cr_ios
#define ui_test_layer_style_scene(name) \
ui_test_layer_scene(name) \
static void ui_test_layer_##name##_present(struct ui_test_scene_state* state, uint32_t width, uint32_t height, float to_px_from_pt) { \
    ui_test_scene_style_present(ui_test_layer_##name##_nodes_, cr_count(ui_test_layer_##name##_nodes_), (float4)ui_test_layer_bg, width, height, to_px_from_pt); \
}
#else
#define ui_test_layer_style_scene(name) ui_test_layer_scene(name)
#endif

static const struct ui_test_style_node ui_test_layer_clip_nodes_[] = {
    {
        .origin = { 64.0, 64.0 }, .size = { 240.0, 240.0 }, .radius = 64.0, .continuous = true,
        .fill = { { .type = ui_test_style_layer_solid, .color0 = { 0.294f, 0.624f, 0.851f, 1.0f } } },
        .clip = true,
        .has_child = true,
        .child_origin = { -48.0, -48.0 }, .child_size = { 336.0, 168.0 }, .child_color = { 0.851f, 0.788f, 0.294f, 1.0f },
    },
    {
        .origin = { 208.0, 336.0 }, .size = { 144.0, 144.0 }, .circle = true,
        .fill = { { .type = ui_test_style_layer_solid, .color0 = { 0.176f, 0.29f, 0.227f, 1.0f } } },
        .clip = true,
        .has_child = true,
        .child_origin = { 72.0, -24.0 }, .child_size = { 120.0, 192.0 }, .child_color = { 0.851f, 0.416f, 0.294f, 1.0f },
    },
    {
        .origin = { 352.0, 96.0 }, .size = { 128.0, 176.0 }, .radius = 32.0, .continuous = true,
        .fill = { { .type = ui_test_style_layer_solid, .opacity = 0.5f, .color0 = { 0.9f, 0.9f, 0.9f, 1.0f } } },
        .outline = { { .type = ui_test_style_layer_solid, .color0 = { 0.851f, 0.788f, 0.294f, 1.0f } } },
        .outline_align = ui_outline_align_inside, .outline_thickness = 8.0,
        .clip = true,
        .has_child = true,
        .child_origin = { -40.0, 96.0 }, .child_size = { 208.0, 120.0 }, .child_color = { 0.294f, 0.851f, 0.624f, 1.0f },
    },
};

static const struct ui_test_style_node ui_test_layer_node_opacity_nodes_[] = {
    {
        .origin = { 64.0, 64.0 }, .size = { 184.0, 144.0 },
        .fill = {
            { .type = ui_test_style_layer_solid, .color0 = { 0.294f, 0.624f, 0.851f, 1.0f } },
            { .type = ui_test_style_layer_solid, .color0 = { 0.851f, 0.416f, 0.294f, 1.0f } },
        },
        .node_opacity = 0.5f,
    },
    {
        .origin = { 264.0, 64.0 }, .size = { 184.0, 144.0 }, .radius = 28.0, .continuous = true,
        .fill = {
            { .type = ui_test_style_layer_solid, .color0 = { 0.294f, 0.624f, 0.851f, 1.0f } },
            { .type = ui_test_style_layer_solid, .color0 = { 0.851f, 0.416f, 0.294f, 1.0f } },
        },
        .node_opacity = 0.5f,
    },
    {
        .origin = { 64.0, 248.0 }, .size = { 384.0, 104.0 }, .radius = 24.0, .continuous = true,
        .fill = { { .type = ui_test_style_layer_solid, .color0 = { 0.9f, 0.9f, 0.9f, 1.0f } } },
        .outline = {
            { .type = ui_test_style_layer_gradient, .direction = ui_gradient_direction_down, .color0 = { 0.851f, 0.788f, 0.294f, 1.0f }, .color1 = { 0.851f, 0.788f, 0.294f, 0.0f } },
        },
        .outline_align = ui_outline_align_inside, .outline_thickness = 16.0,
        .node_opacity = 0.6f,
    },
    {
        .origin = { 64.0, 392.0 }, .size = { 384.0, 88.0 }, .radius = 20.0, .continuous = true,
        .fill = { { .type = ui_test_style_layer_solid, .color0 = { 0.176f, 0.29f, 0.227f, 1.0f } } },
        .outline = { { .type = ui_test_style_layer_solid, .color0 = { 0.851f, 0.788f, 0.294f, 1.0f } } },
        .outline_align = ui_outline_align_inside, .outline_thickness = 8.0,
        .node_opacity = 0.5f,
    },
};

ui_test_layer_style_scene(fill_stack)
ui_test_layer_style_scene(gradient_mix)
ui_test_layer_style_scene(outline_stack)
ui_test_layer_style_scene(outline_align)
ui_test_layer_style_scene(order)
ui_test_layer_style_scene(shadow)
ui_test_layer_style_scene(blur)
ui_test_layer_style_scene(clip)
ui_test_layer_style_scene(node_opacity)

static const struct ui_test_style_node ui_test_layer_shadow_translucent_nodes_[] = {
    {
        .origin = { 96.0, 96.0 }, .size = { 200.0, 200.0 }, .radius = 36.0, .continuous = true,
        .fill = { { .type = ui_test_style_layer_solid, .opacity = 0.5f, .color0 = { 0.933f, 0.933f, 0.933f, 1.0f } } },
        .has_shadow = true,
        .shadow_offset = { 0.0, 12.0 }, .shadow_radius = 6.0,
        .shadow_color = { 0.0f, 0.0f, 0.0f, 0.5f },
    },
    {
        .origin = { 256.0, 320.0 }, .size = { 160.0, 120.0 }, .radius = 24.0, .continuous = true,
        .fill = {
            { .type = ui_test_style_layer_solid, .opacity = 0.35f, .color0 = { 0.294f, 0.624f, 0.851f, 1.0f } },
            { .type = ui_test_style_layer_solid, .opacity = 0.35f, .color0 = { 0.851f, 0.416f, 0.294f, 1.0f } },
        },
        .has_shadow = true,
        .shadow_offset = { 8.0, 8.0 }, .shadow_radius = 4.0,
        .shadow_color = { 0.063f, 0.125f, 0.251f, 0.6f },
    },
};

ui_test_layer_scene(shadow_translucent)

#if cr_ios
static void ui_test_layer_shadow_translucent_pixels(struct ui_test_scene_state* state, uint32_t width, uint32_t height, float to_px_from_pt, float* out_rgba, uint32_t bytes_per_row) {
    float s = to_px_from_pt;

    CGContextRef ctx = ui_test_ref_begin_(width, height, ui_color((float4)ui_test_layer_bg, ui_color_space_srgb));

    for (uint32_t i = 0; i < cr_count(ui_test_layer_shadow_translucent_nodes_); i++) {
        const struct ui_test_style_node* n = &ui_test_layer_shadow_translucent_nodes_[i];

        CGContextSaveGState(ctx);

        CGColorSpaceRef space = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
        CGFloat shadow_components[4] = { n->shadow_color.x, n->shadow_color.y, n->shadow_color.z, n->shadow_color.w };
        CGColorRef shadow_color = CGColorCreate(space, shadow_components);
        CGColorSpaceRelease(space);
        CGContextSetShadowWithColor(ctx, CGSizeMake(n->shadow_offset.x * s, -n->shadow_offset.y * s), 2.0f * (float)n->shadow_radius * s, shadow_color);
        CGColorRelease(shadow_color);

        CGContextBeginTransparencyLayer(ctx, NULL);
        for (uint32_t k = 0; k < ui_test_style_max_layers; k++) {
            const struct ui_test_style_layer* l = &n->fill[k];
            if (l->type != ui_test_style_layer_solid) {
                continue;
            }
            struct ui_color color = ui_color(l->color0, ui_color_space_srgb);
            color.value.w *= l->opacity > 0.0f ? l->opacity : 1.0f;
            ui_test_layer_fill_rounded_rect(ctx, (float)n->origin.x * s, (float)n->origin.y * s, (float)n->size.x * s, (float)n->size.y * s, (float)n->radius * s, n->continuous, color);
        }
        CGContextEndTransparencyLayer(ctx);

        CGContextRestoreGState(ctx);
    }

    ui_test_ref_end_(ctx, width, height, out_rgba, bytes_per_row);
}
#endif

static const char* ui_test_layer_text_raw_ = "layered gradient";
#define ui_test_layer_text_x 48.0
#define ui_test_layer_text_y 200.0
#define ui_test_layer_text_w 420.0
#define ui_test_layer_text_h 112.0
#define ui_test_layer_text_pt 40.0

static struct ui_node ui_test_layer_text_build(struct ui_test_scene_state* state, struct ui_node parent, double2 size, const struct ui_test_scene* scene) {
    struct ui_node root = ui_frame(&(struct ui_frame) {
        .parent = parent,
        .size = ui_size_fix(size.x, size.y),
        .fill = ui_fill_solid(ui_hex(0x1d2733)),
        .clip = true,
    });

    ui_text(&(struct ui_text) {
        .parent = root,
        .pos = ui_pos_xy(ui_test_layer_text_x, ui_test_layer_text_y),
        .size = ui_size_fix(ui_test_layer_text_w, ui_test_layer_text_h),
        .str = ui_str(ui_test_layer_text_raw_, ui_white, ui_test_layer_text_pt),
        .fill.layers = {
            ui_layer_gradient(ui_gradient_direction_down, ui_hex(0xd9c94b), ui_hex(0xd96a4b)),
            ui_layer_opacity(ui_layer_gradient(ui_gradient_direction_right, ui_hex(0x4b9fd9), ui_hex_a(0x4b9fd9, 0.0f)), 0.7f),
        },
    });

    ui_text(&(struct ui_text) {
        .parent = root,
        .pos = ui_pos_xy(ui_test_layer_text_x, ui_test_layer_text_y + 120.0),
        .size = ui_size_fix(ui_test_layer_text_w, ui_test_layer_text_h),
        .str = ui_str(ui_test_layer_text_raw_, ui_white, ui_test_layer_text_pt),
        .fill.layers[0] = ui_layer_opacity(ui_layer_solid(ui_hex(0x7bd94b)), 0.5f),
    });

    return root;
}

#if cr_ios
static void ui_test_layer_text_clip(CGContextRef ctx, CTFontRef font, double x, double y) {
    NSDictionary* attributes = @{
        (id)kCTFontAttributeName: (__bridge id)font,
    };
    NSAttributedString* str = [[NSAttributedString alloc] initWithString:[NSString stringWithUTF8String:ui_test_layer_text_raw_] attributes:attributes];
    CTLineRef line = CTLineCreateWithAttributedString((CFAttributedStringRef)str);

    double baseline = y + CTFontGetAscent(font);
    CGMutablePathRef path = CGPathCreateMutable();

    CFArrayRef runs = CTLineGetGlyphRuns(line);
    for (CFIndex r = 0; r < CFArrayGetCount(runs); r++) {
        CTRunRef run = CFArrayGetValueAtIndex(runs, r);
        CFIndex count = CTRunGetGlyphCount(run);
        CGGlyph glyphs[64];
        CGPoint positions[64];
        assert(count <= 64);
        CTRunGetGlyphs(run, CFRangeMake(0, count), glyphs);
        CTRunGetPositions(run, CFRangeMake(0, count), positions);
        for (CFIndex i = 0; i < count; i++) {
            CGAffineTransform transform = CGAffineTransformMake(1.0, 0.0, 0.0, -1.0,
                                                                x + positions[i].x, baseline - positions[i].y);
            CGPathRef glyph_path = CTFontCreatePathForGlyph(font, glyphs[i], &transform);
            if (glyph_path) {
                CGPathAddPath(path, NULL, glyph_path);
                CGPathRelease(glyph_path);
            }
        }
    }

    CGContextAddPath(ctx, path);
    CGContextClip(ctx);
    CGPathRelease(path);

    CFRelease(line);
    [str release];
}

static void ui_test_layer_text_gradient(CGContextRef ctx, float4 c0, float4 c1, CGPoint start, CGPoint end) {
    CGColorSpaceRef space = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    CGFloat components[8] = { c0.x, c0.y, c0.z, c0.w, c1.x, c1.y, c1.z, c1.w };
    CGFloat locations[2] = { 0.0, 1.0 };
    CGGradientRef gradient = CGGradientCreateWithColorComponents(space, components, locations, 2);
    CGContextDrawLinearGradient(ctx, gradient, start, end, kCGGradientDrawsBeforeStartLocation | kCGGradientDrawsAfterEndLocation);
    CGGradientRelease(gradient);
    CGColorSpaceRelease(space);
}

static void ui_test_layer_text_pixels(struct ui_test_scene_state* state, uint32_t width, uint32_t height, float to_px_from_pt, float* out_rgba, uint32_t bytes_per_row) {
    uint64_t font_size;
    const uint8_t* font_data = ui_test_scenes_font_(state->scenes, false, &font_size);

    CGDataProviderRef provider = CGDataProviderCreateWithData(NULL, font_data, font_size, NULL);
    CGFontRef cg_font = CGFontCreateWithDataProvider(provider);
    CGDataProviderRelease(provider);
    CTFontRef font = ui_test_scene_text_ct_font_(cg_font, ui_test_layer_text_pt, 0.0f, false);

    double x = ui_test_layer_text_x;
    double y = ui_test_layer_text_y;

    CGContextRef ctx = ui_test_ref_begin_(width, height, ui_hex(0x1d2733));
    CGContextScaleCTM(ctx, to_px_from_pt, to_px_from_pt);

    CGContextSaveGState(ctx);
    ui_test_layer_text_clip(ctx, font, x, y);
    ui_test_layer_text_gradient(ctx, make_float4(0.851f, 0.788f, 0.294f, 1.0f), make_float4(0.851f, 0.416f, 0.294f, 1.0f),
                                CGPointMake(x, y), CGPointMake(x, y + ui_test_layer_text_h));
    CGContextRestoreGState(ctx);

    CGContextSaveGState(ctx);
    CGContextSetAlpha(ctx, 0.7f);
    CGContextBeginTransparencyLayer(ctx, NULL);
    ui_test_layer_text_clip(ctx, font, x, y);
    ui_test_layer_text_gradient(ctx, make_float4(0.294f, 0.624f, 0.851f, 1.0f), make_float4(0.294f, 0.624f, 0.851f, 0.0f),
                                CGPointMake(x, y), CGPointMake(x + ui_test_layer_text_w, y));
    CGContextEndTransparencyLayer(ctx);
    CGContextRestoreGState(ctx);

    CGContextSaveGState(ctx);
    CGContextSetAlpha(ctx, 0.5f);
    CGContextBeginTransparencyLayer(ctx, NULL);
    ui_test_layer_text_clip(ctx, font, x, y + 120.0);
    struct ui_color solid = ui_hex(0x7bd94b);
    ui_test_ref_set_fill_(ctx, solid);
    CGContextFillRect(ctx, CGRectMake(x, y + 120.0, ui_test_layer_text_w, ui_test_layer_text_h));
    CGContextEndTransparencyLayer(ctx);
    CGContextRestoreGState(ctx);

    CFRelease(font);
    CGFontRelease(cg_font);

    ui_test_ref_end_(ctx, width, height, out_rgba, bytes_per_row);
}
#endif

static const struct ui_test_scene ui_test_scenes_layer_table_[] = {
    {
        .label = "layer-opacity", .group = "layer",
        .capture = ui_test_scene_capture_screenshot,
        .size = { 1024.0, 1024.0 },
        .mean_threshold = 0.002f,
        .tile_threshold = 0.008f,
        .alarm_allowance = 32,
        ui_test_sui_scene(ui_test_scene_layer_opacity, 48),
    },
    {
        .label = "layer-clip", .group = "layer",
        .capture = ui_test_scene_capture_screenshot,
        .size = { 1024.0, 1024.0 },
        .mean_threshold = 0.002f,
        .tile_threshold = 0.008f,
        .alarm_allowance = 32,
        ui_test_sui_scene(ui_test_scene_layer_clip, 96),
    },
    {
        .label = "layer-blend", .group = "layer",
        .capture = ui_test_scene_capture_screenshot,
        .size = { 1024.0, 1024.0 },
        .mean_threshold = 0.002f,
        .tile_threshold = 0.008f,
        .alarm_allowance = 32,
        ui_test_sui_scene(ui_test_scene_layer_blend, 40),
    },
    {
        .label = "layer-fill-stack", .group = "layer",
        .capture = ui_test_scene_capture_screenshot,
        .size = { 512.0, 512.0 },
        .mean_threshold = 0.002f,
        .build = ui_test_layer_fill_stack_build,
        .present = ui_test_scene_present(ui_test_layer_fill_stack_present),
    },
    {
        .label = "layer-gradient-mix", .group = "layer",
        .capture = ui_test_scene_capture_screenshot,
        .size = { 512.0, 512.0 },
        .mean_threshold = 0.002f,
        .build = ui_test_layer_gradient_mix_build,
        .present = ui_test_scene_present(ui_test_layer_gradient_mix_present),
    },
    {
        .label = "layer-outline-stack", .group = "layer",
        .capture = ui_test_scene_capture_screenshot,
        .size = { 512.0, 512.0 },
        .mean_threshold = 0.002f,
        .build = ui_test_layer_outline_stack_build,
        .present = ui_test_scene_present(ui_test_layer_outline_stack_present),
    },
    {
        .label = "layer-outline-align", .group = "layer",
        .capture = ui_test_scene_capture_screenshot,
        .size = { 512.0, 512.0 },
        .mean_threshold = 0.002f,
        .build = ui_test_layer_outline_align_build,
        .present = ui_test_scene_present(ui_test_layer_outline_align_present),
    },
    {
        .label = "layer-order", .group = "layer",
        .capture = ui_test_scene_capture_screenshot,
        .size = { 512.0, 512.0 },
        .mean_threshold = 0.002f,
        .build = ui_test_layer_order_build,
        .present = ui_test_scene_present(ui_test_layer_order_present),
    },
    {
        .label = "layer-shadow", .group = "layer",
        .capture = ui_test_scene_capture_screenshot,
        .size = { 512.0, 512.0 },
        .mean_threshold = 0.004f,
        .build = ui_test_layer_shadow_build,
        .present = ui_test_scene_present(ui_test_layer_shadow_present),
    },
    {
        .label = "layer-shadow-translucent", .group = "layer",
        .capture = ui_test_scene_capture_pixels,
        .size = { 512.0, 512.0 }, .to_px_from_pt = 1.0f,
        .mean_threshold = 0.002f,
        .build = ui_test_layer_shadow_translucent_build,
        .pixels = ui_test_scene_pixels(ui_test_layer_shadow_translucent_pixels),
    },
    {
        .label = "layer-blur", .group = "layer",
        .capture = ui_test_scene_capture_screenshot,
        .size = { 512.0, 512.0 },
        .mean_threshold = 0.008f,
        .tile_threshold = 0.06f,
        .build = ui_test_layer_blur_build,
        .present = ui_test_scene_present(ui_test_layer_blur_present),
    },
    {
        .label = "layer-style-clip", .group = "layer",
        .capture = ui_test_scene_capture_screenshot,
        .size = { 512.0, 512.0 },
        .mean_threshold = 0.002f,
        .build = ui_test_layer_clip_build,
        .present = ui_test_scene_present(ui_test_layer_clip_present),
    },
    {
        .label = "layer-text", .group = "layer",
        .capture = ui_test_scene_capture_pixels,
        .size = { 512.0, 512.0 }, .to_px_from_pt = 1.0f,
        .mean_threshold = 0.002f,
        .tile_threshold = 0.012f,
        .build = ui_test_layer_text_build,
        .pixels = ui_test_scene_pixels(ui_test_layer_text_pixels),
    },
    {
        .label = "layer-node-opacity", .group = "layer",
        .capture = ui_test_scene_capture_screenshot,
        .size = { 512.0, 512.0 },
        .mean_threshold = 0.002f,
        .tile_threshold = 0.016f,
        .build = ui_test_layer_node_opacity_build,
        .present = ui_test_scene_present(ui_test_layer_node_opacity_present),
    },
};

const struct ui_test_scene* ui_test_scenes_layer_(struct ui_test_scenes* scenes, uint32_t* out_count) {
    *out_count = cr_count(ui_test_scenes_layer_table_);
    return ui_test_scenes_layer_table_;
}
