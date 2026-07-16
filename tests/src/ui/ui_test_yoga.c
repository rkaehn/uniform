#include "cr.h"

#include "ui_core.h"
#include <yoga/Yoga.h>

void* ui_yoga(struct ui_node root);
void ui_yoga_layout(void* _root_node);
bool ui_yoga_is_equal(struct ui_node root, void* _root_node);
void ui_yoga_release(void* _root_node);

static YGJustify ui_to_yoga_justify(enum ui_dist dist) {
    switch (dist) {
    case ui_dist_start: return YGJustifyFlexStart;
    case ui_dist_center: return YGJustifyCenter;
    case ui_dist_end: return YGJustifyFlexEnd;
    }
}

static YGAlign ui_to_yoga_align(enum ui_align align) {
    switch (align) {
    case ui_align_start: return YGAlignFlexStart;
    case ui_align_center: return YGAlignCenter;
    case ui_align_end: return YGAlignFlexEnd;
    }
}

static float ui_to_yoga_anchor(enum ui_pos_anchor anchor) {
    switch (anchor) {
    case ui_pos_anchor_start: return 0.0f;
    case ui_pos_anchor_center: return 0.5f;
    case ui_pos_anchor_end: return 1.0f;
    }
}

static void ui_yoga_size(YGNodeRef node, const struct ui_layout_size_axis* size, bool is_x, enum ui_layout_mode parent_layout) {
    enum ui_layout_mode along = is_x ? ui_layout_mode_stack_x : ui_layout_mode_stack_y;
    enum ui_layout_mode across = is_x ? ui_layout_mode_stack_y : ui_layout_mode_stack_x;

    switch (size->mode) {
    case ui_size_mode_fit: {
        if (is_x) {
            YGNodeStyleSetWidthAuto(node);
        } else {
            YGNodeStyleSetHeightAuto(node);
        }
    } break;
    case ui_size_mode_fill: {
        if (parent_layout == along) {
            YGNodeStyleSetFlexGrow(node, 1.0f);
            YGNodeStyleSetFlexBasis(node, 0.0f);
        } else {
            assert(parent_layout == across);
            YGNodeStyleSetAlignSelf(node, YGAlignStretch);
        }
    } break;
    case ui_size_mode_fix: {
        float value = (float)ui_fx_to_double(size->constant);
        if (is_x) {
            YGNodeStyleSetWidth(node, value);
        } else {
            YGNodeStyleSetHeight(node, value);
        }
    } break;
    case ui_size_mode_rel: {
        assert(size->anchor == (is_x ? ui_size_anchor_width : ui_size_anchor_height));
        assert(ui_fx_to_double(size->constant) == 0.0);
        float percent = (float)(100.0 * size->multiplier);
        if (is_x) {
            YGNodeStyleSetWidthPercent(node, percent);
        } else {
            YGNodeStyleSetHeightPercent(node, percent);
        }
    } break;
    }
}

static void ui_yoga_pos(YGNodeRef node, YGEdge edge, const struct ui_layout_pos_axis* pos, double size) {
    YGNodeStyleSetPositionPercent(node, edge, 100.0f * ui_to_yoga_anchor(pos->parent_anchor));
    YGNodeStyleSetMargin(node, edge, (float)(ui_fx_to_double(pos->constant) - (double)ui_to_yoga_anchor(pos->anchor) * size));
}

static YGNodeRef ui_yoga_node(struct ui* ui, struct ui_node_backing* ui_backing, YGConfigRef config) {
    YGNodeRef node = YGNodeNewWithConfig(config);
    struct ui_layout_p0* layout = &ui_backing->p0;

    struct ui_node_backing* parent_node = ui_node_get_parent(ui, ui_backing);
    enum ui_layout_mode parent_layout = parent_node ? parent_node->p0.mode : ui_layout_mode_manual;

    switch (layout->mode) {
    case ui_layout_mode_manual: {
        YGNodeStyleSetFlexDirection(node, YGFlexDirectionColumn);
    } break;
    case ui_layout_mode_stack_x: {
        YGNodeStyleSetFlexDirection(node, YGFlexDirectionRow);
        YGNodeStyleSetGap(node, YGGutterColumn, (float)(ui_fx_to_double(layout->spacing)));
    } break;
    case ui_layout_mode_stack_y: {
        YGNodeStyleSetFlexDirection(node, YGFlexDirectionColumn);
        YGNodeStyleSetGap(node, YGGutterRow, (float)(ui_fx_to_double(layout->spacing)));
    } break;
    }

    YGNodeStyleSetJustifyContent(node, ui_to_yoga_justify(layout->dist));
    YGNodeStyleSetAlignItems(node, ui_to_yoga_align(layout->align));

    YGNodeStyleSetPadding(node, YGEdgeLeft, (float)(ui_fx_to_double(layout->padding_left)));
    YGNodeStyleSetPadding(node, YGEdgeRight, (float)(ui_fx_to_double(layout->padding_right)));
    YGNodeStyleSetPadding(node, YGEdgeTop, (float)(ui_fx_to_double(layout->padding_top)));
    YGNodeStyleSetPadding(node, YGEdgeBottom, (float)(ui_fx_to_double(layout->padding_bottom)));

    ui_yoga_size(node, &layout->width, true, parent_layout);
    ui_yoga_size(node, &layout->height, false, parent_layout);

    if (layout->x.mode != ui_pos_mode_auto) {
        YGNodeStyleSetPositionType(node, YGPositionTypeAbsolute);

        double eff_width = 0.0;
        double eff_height = 0.0;

        if (layout->width.mode == ui_size_mode_fix) {
            eff_width = fmax(ui_fx_to_double(layout->width.constant), ui_fx_to_double(layout->padding_left) + ui_fx_to_double(layout->padding_right));
        } else {
            assert(layout->x.anchor == ui_pos_anchor_start);
        }

        if (layout->height.mode == ui_size_mode_fix) {
            eff_height = fmax(ui_fx_to_double(layout->height.constant), ui_fx_to_double(layout->padding_top) + ui_fx_to_double(layout->padding_bottom));
        } else {
            assert(layout->y.anchor == ui_pos_anchor_start);
        }

        ui_yoga_pos(node, YGEdgeLeft, &layout->x, eff_width);
        ui_yoga_pos(node, YGEdgeTop, &layout->y, eff_height);
    } else {
        YGNodeStyleSetPositionType(node, YGPositionTypeRelative);
    }

    uint32_t i = 0;

    for (struct ui_node_backing* ui_child_node = ui_node_get_child(ui, ui_backing, 0); ui_child_node != NULL; ui_child_node = ui_node_get_next(ui, ui_child_node)) {
        YGNodeRef child_node = ui_yoga_node(ui, ui_child_node, config);
        YGNodeInsertChild(node, child_node, i);
        i++;
    }

    return node;
}

void* ui_yoga(struct ui_node root) {
    struct ui* ui = ui_ctx();
    struct ui_node_backing* ui_root_node = ui_node_pool_item(&ui->node_pool, root);

    YGConfigRef config = YGConfigNew();

    YGConfigSetPointScaleFactor(config, 0.0f);
    YGConfigSetExperimentalFeatureEnabled(config, YGExperimentalFeatureWebFlexBasis, true);

    YGNodeRef root_node = ui_yoga_node(ui, ui_root_node, config);

    return root_node;
}

void ui_yoga_layout(void* _root_node) {
    YGNodeRef root_node = _root_node;

    YGNodeCalculateLayout(root_node, YGUndefined, YGUndefined, YGDirectionLTR);
}

static inline bool is_float_equal(double a, double b) {
    return fabs(a - b) <= 1.5;
}

static bool ui_yoga_is_equal_node(struct ui* ui, struct ui_node_backing* ui_backing, YGNodeRef yoga_node, struct ui_fx avail_width, struct ui_fx avail_height) {
    const struct ui_layout_p0* p0 = &ui_backing->p0;
    struct ui_fx padding_width = ui_fx_add(p0->padding_left, p0->padding_right);
    struct ui_fx padding_height = ui_fx_add(p0->padding_top, p0->padding_bottom);

    float yoga_x = YGNodeLayoutGetLeft(yoga_node);
    float yoga_y = YGNodeLayoutGetTop(yoga_node);
    float yoga_w = YGNodeLayoutGetWidth(yoga_node);
    float yoga_h = YGNodeLayoutGetHeight(yoga_node);

    double ui_x = ui_fx_to_double(ui_backing->p2.x);
    double ui_y = ui_fx_to_double(ui_backing->p2.y);
    double ui_w = ui_fx_to_double(ui_backing->p2.width);
    double ui_h = ui_fx_to_double(ui_backing->p2.height);

    bool is_equal = is_float_equal(ui_x, yoga_x) && is_float_equal(ui_y, yoga_y) &&
                    is_float_equal(ui_w, yoga_w) && is_float_equal(ui_h, yoga_h);

    struct ui_fx child_avail_width = ui_fx_sub(ui_fx_min(avail_width, ui_backing->p2.width), padding_width);
    struct ui_fx child_avail_height = ui_fx_sub(ui_fx_min(avail_height, ui_backing->p2.height), padding_height);

    if (ui_fx_is_positive(child_avail_width) == false || ui_fx_is_positive(child_avail_height) == false) {
        return is_equal;
    }

    for (struct ui_node_backing* ui_child = ui_node_get_child(ui, ui_backing, 0); ui_child != NULL; ui_child = ui_node_get_next(ui, ui_child)) {
        const struct ui_layout_p0* child_p0 = &ui_child->p0;
        struct ui_fx child_padding_width = ui_fx_add(child_p0->padding_left, child_p0->padding_right);
        struct ui_fx child_padding_height = ui_fx_add(child_p0->padding_top, child_p0->padding_bottom);

        if (ui_fx_is_positive(ui_fx_sub(child_avail_width, child_padding_width)) == false ||
            ui_fx_is_positive(ui_fx_sub(child_avail_height, child_padding_height)) == false) {
            return is_equal;
        }
    }

    uint32_t i = 0;
    for (struct ui_node_backing* ui_child = ui_node_get_child(ui, ui_backing, 0); ui_child != NULL; ui_child = ui_node_get_next(ui, ui_child)) {
        YGNodeRef yoga_child = YGNodeGetChild(yoga_node, i);

        if (ui_yoga_is_equal_node(ui, ui_child, yoga_child, child_avail_width, child_avail_height) == false) {
            is_equal = false;
        }
        i++;
    }

    return is_equal;
}

bool ui_yoga_is_equal(struct ui_node root, void* _root_node) {
    struct ui* ui = ui_ctx();
    YGNodeRef root_node = _root_node;
    struct ui_node_backing* ui_root_node = ui_node_pool_item(&ui->node_pool, root);

    return ui_yoga_is_equal_node(ui, ui_root_node, root_node, ui_root_node->p2.width, ui_root_node->p2.height);
}

void ui_yoga_release(void* _root_node) {
    YGNodeRef root_node = _root_node;

    YGConfigRef config = (YGConfigRef)YGNodeGetConfig(root_node);

    YGNodeFreeRecursive(root_node);

    YGConfigFree(config);
}
