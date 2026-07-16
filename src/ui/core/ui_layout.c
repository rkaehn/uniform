#include "ui_core.h"
#include "ui_text.h"

static void ui_layout_p0(struct ui* ui, struct ui_node_backing* node, const struct ui_layout_p0* parent_p0);
static void ui_layout_p1(struct ui* ui, struct ui_node_backing* node, enum ui_layout_mode parent_layout_mode);
static void ui_layout_p2(struct ui* ui, struct ui_fx x, struct ui_fx y, struct ui_fx width, struct ui_fx height, bool is_root, struct ui_node_backing* node);
static struct ui_fx ui_layout_p2_cons(struct ui* ui, bool is_x, struct ui_fx size_cross, struct ui_node_backing* node);

void ui_layout(struct ui_node root) {
    struct ui* ui = ui_ctx();

    if (ui_node_is_null(root)) {
        return;
    }

    struct ui_node_backing* root_node = ui_node_check(ui, root, "the root");
    root_node->layout_to_px_from_pt = ui->to_px_from_pt;
    struct ui_node_backing* parent_node = ui_node_get_parent(ui, root_node);

    ui_layout_p0(ui, root_node, parent_node ? &parent_node->p0 : NULL);
    ui_layout_p1(ui, root_node, parent_node ? parent_node->p0.mode : ui_layout_mode_manual);
    ui_layout_p2(ui, root_node->p0.x.constant, root_node->p0.y.constant, root_node->p1.min_width, root_node->p1.min_height, true, root_node);
}

__attribute__((always_inline))
static struct ui_fx ui_layout_padding(const struct ui_layout_p0* p0, bool is_x) {
    return is_x ? ui_fx_add(p0->padding_left, p0->padding_right) : ui_fx_add(p0->padding_top, p0->padding_bottom);
}

__attribute__((always_inline))
static struct ui_fx ui_layout_anchor(enum ui_pos_anchor anchor, struct ui_fx size) {
    switch (anchor) {
    case ui_pos_anchor_start: return ui_fx_zero;
    case ui_pos_anchor_center: return ui_fx_div(size, ui_fx_make(2));
    case ui_pos_anchor_end: return size;
    }
}

__attribute__((always_inline))
static struct ui_fx ui_layout_pos(const struct ui_layout_pos_axis* pos, struct ui_fx parent_size, struct ui_fx size) {
    struct ui_fx offset = ui_fx_sub(ui_layout_anchor(pos->parent_anchor, parent_size), ui_layout_anchor(pos->anchor, size));
    return ui_fx_add(ui_fx_round_px(offset), pos->constant);
}

static struct ui_fx ui_layout_size_rel(const struct ui_layout_p0* p0, bool is_x, struct ui_fx parent_width, struct ui_fx parent_height) {
    const struct ui_layout_size_axis* size = is_x ? &p0->width : &p0->height;
    struct ui_fx parent_size = size->anchor == ui_size_anchor_width ? parent_width : parent_height;
    struct ui_fx scaled = ui_fx_make((int32_t)lround(ui_fx_to_double(parent_size) * size->multiplier));
    return ui_fx_max(ui_fx_add(scaled, size->constant), ui_layout_padding(p0, is_x));
}

__attribute__((always_inline))
static int32_t ui_layout_fill_extra_px(uint32_t fill_index, int32_t fill_extra_px, uint32_t fill_count) {
    return (int32_t)(((uint64_t)(fill_index + 1) * (uint64_t)fill_extra_px) / fill_count) -
           (int32_t)(((uint64_t)fill_index * (uint64_t)fill_extra_px) / fill_count);
}

__attribute__((always_inline))
static struct ui_layout_pos_axis ui_layout_p0_pos_axis(struct ui_node_pos_axis axis, double to_px_from_pt) {
    return (struct ui_layout_pos_axis) {
        .mode = axis.mode,
        .anchor = axis.anchor,
        .parent_anchor = axis.parent_anchor,
        .constant = ui_fx_make_px(axis.constant, to_px_from_pt),
    };
}

__attribute__((always_inline))
static struct ui_layout_size_axis ui_layout_p0_size_axis(struct ui_node_size_axis axis, double to_px_from_pt) {
    return (struct ui_layout_size_axis) {
        .mode = axis.mode,
        .anchor = axis.anchor,
        .multiplier = axis.multiplier,
        .constant = axis.mode == ui_size_mode_rel ? ui_fx_make_px(axis.constant, to_px_from_pt) : ui_fx_make_px_size(axis.constant, to_px_from_pt),
    };
}

__attribute__((always_inline))
static void ui_layout_p0_pin_pos(struct ui_layout_pos_axis* pos) {
    if (pos->mode == ui_pos_mode_auto) {
        pos->mode = ui_pos_mode_rel;
        pos->anchor = ui_pos_anchor_start;
        pos->parent_anchor = ui_pos_anchor_start;
    }
}

__attribute__((always_inline))
static void ui_layout_p0_pin_size(struct ui_layout_size_axis* size, enum ui_size_anchor anchor, bool has_parent) {
    if (has_parent == false) {
        if (size->mode == ui_size_mode_fill || size->mode == ui_size_mode_rel) {
            size->mode = ui_size_mode_fix;
        }
    } else if (size->mode == ui_size_mode_fill) {
        size->mode = ui_size_mode_rel;
        size->anchor = anchor;
        size->multiplier = 1.0;
        size->constant = ui_fx_zero;
    }
}

static void ui_layout_p0(struct ui* ui, struct ui_node_backing* node, const struct ui_layout_p0* parent_p0) {
    struct ui_layout_p0* p0 = &node->p0;

    p0->x = ui_layout_p0_pos_axis(node->pos.x, ui->to_px_from_pt);
    p0->y = ui_layout_p0_pos_axis(node->pos.y, ui->to_px_from_pt);
    p0->width = ui_layout_p0_size_axis(node->size.width, ui->to_px_from_pt);
    p0->height = ui_layout_p0_size_axis(node->size.height, ui->to_px_from_pt);
    p0->mode = ui_layout_mode_manual;
    p0->dist = ui_dist_start;
    p0->align = ui_align_start;
    p0->spacing = ui_fx_zero;
    p0->padding_left = ui_fx_zero;
    p0->padding_right = ui_fx_zero;
    p0->padding_top = ui_fx_zero;
    p0->padding_bottom = ui_fx_zero;

    switch (node->type) {
    case ui_content_type_none: {
        cr_check(0, "a node was declared with ui_node but never given content");
    } break;
    case ui_content_type_frame: {
        const struct ui_layout* layout = &node->frame.layout;
        if (layout->mode != ui_layout_mode_manual) {
            p0->mode = layout->mode;
            p0->dist = layout->dist;
            p0->align = layout->align;
            p0->spacing = ui_fx_make_px(layout->spacing, ui->to_px_from_pt);
            p0->padding_left = ui_fx_make_px(layout->padding_left, ui->to_px_from_pt);
            p0->padding_right = ui_fx_make_px(layout->padding_right, ui->to_px_from_pt);
            p0->padding_top = ui_fx_make_px(layout->padding_top, ui->to_px_from_pt);
            p0->padding_bottom = ui_fx_make_px(layout->padding_bottom, ui->to_px_from_pt);
        }
    } break;
    case ui_content_type_shape:
    case ui_content_type_text: {
    } break;
    case ui_content_type_custom: {
        if (p0->width.mode == ui_size_mode_fit) {
            p0->width.mode = ui_size_mode_fix;
        }
        if (p0->height.mode == ui_size_mode_fit) {
            p0->height.mode = ui_size_mode_fix;
        }
    } break;
    }

    bool is_parent_stack = parent_p0 != NULL && parent_p0->mode != ui_layout_mode_manual;
    bool is_auto = p0->x.mode == ui_pos_mode_auto && p0->y.mode == ui_pos_mode_auto;
    bool is_rel = p0->width.mode == ui_size_mode_rel || p0->height.mode == ui_size_mode_rel;
    bool flows = is_parent_stack && is_auto && is_rel == false && node->type != ui_content_type_custom;

    if (flows == false) {
        ui_layout_p0_pin_pos(&p0->x);
        ui_layout_p0_pin_pos(&p0->y);
        ui_layout_p0_pin_size(&p0->width, ui_size_anchor_width, parent_p0 != NULL);
        ui_layout_p0_pin_size(&p0->height, ui_size_anchor_height, parent_p0 != NULL);
    }

    for (struct ui_node_backing* child_node = ui_node_get_child(ui, node, 0); child_node != NULL; child_node = ui_node_get_next(ui, child_node)) {
        ui_layout_p0(ui, child_node, p0);
    }
}

static void ui_layout_p1(struct ui* ui, struct ui_node_backing* node, enum ui_layout_mode parent_layout_mode) {
    bool is_text;
    const struct ui_layout_p0* p0;
    struct ui_layout_p1* p1;
    {
        is_text = node->type == ui_content_type_text;
        p0 = &node->p0;
        p1 = &node->p1;
    }

    struct ui_fx width_padding = ui_layout_padding(p0, true);
    struct ui_fx height_padding = ui_layout_padding(p0, false);

    uint32_t fill_count = 0;

    uint32_t stack_count = 0;

    struct ui_fx min_content_main = ui_fx_zero;
    struct ui_fx min_content_cross = ui_fx_zero;

    for (struct ui_node_backing* child_node = ui_node_get_child(ui, node, 0); child_node != NULL; child_node = ui_node_get_next(ui, child_node)) {
        ui_layout_p1(ui, child_node, p0->mode);

        if (child_node->p0.x.mode != ui_pos_mode_auto) {
            continue;
        }

        stack_count++;

        switch (p0->mode) {
        case ui_layout_mode_manual:
            assert(0);
        case ui_layout_mode_stack_x: {
            if (child_node->p0.width.mode == ui_size_mode_fill) {
                fill_count++;
            }
            min_content_main = ui_fx_add(min_content_main, child_node->p1.min_width);
            min_content_cross = ui_fx_max(min_content_cross, child_node->p1.min_height);
        } break;
        case ui_layout_mode_stack_y: {
            if (child_node->p0.height.mode == ui_size_mode_fill) {
                fill_count++;
            }
            min_content_main = ui_fx_add(min_content_main, child_node->p1.min_height);
            min_content_cross = ui_fx_max(min_content_cross, child_node->p1.min_width);
        } break;
        }
    }

    if (stack_count > 1) {
        struct ui_fx spacing_total = ui_fx_mul(p0->spacing, ui_fx_make((int32_t)(stack_count - 1)));
        min_content_main = ui_fx_add(min_content_main, spacing_total);
    }

    if (p0->mode == ui_layout_mode_stack_x && p0->width.mode == ui_size_mode_fix && fill_count > 0) {
        struct ui_fx rem = ui_fx_sub(ui_fx_sub(ui_fx_max(p0->width.constant, width_padding), width_padding), min_content_main);

        if (ui_fx_is_positive(rem)) {
            int32_t rem_px = ui_fx_get_px(rem);
            struct ui_fx fill = ui_fx_make(rem_px / (int32_t)fill_count);
            int32_t fill_extra_px = rem_px % (int32_t)fill_count;
            uint32_t fill_index = 0;

            for (struct ui_node_backing* child_node = ui_node_get_child(ui, node, 0); child_node != NULL; child_node = ui_node_get_next(ui, child_node)) {
                if (child_node->p0.x.mode != ui_pos_mode_auto || child_node->p0.width.mode != ui_size_mode_fill) {
                    continue;
                }

                struct ui_fx child_width = ui_fx_add(child_node->p1.min_width, fill);
                child_width = ui_fx_add(child_width, ui_fx_make(ui_layout_fill_extra_px(fill_index++, fill_extra_px, fill_count)));

                if (child_node->type != ui_content_type_text || child_node->p0.height.mode != ui_size_mode_fit) {
                    continue;
                }

                int32_t measure_width_266 = max_int32(ui_fx_to_text_266(child_width, ui->to_px_from_pt), 0);
                int2 measured_266 = ui_text_measure(ui, child_node, measure_width_266);

                child_node->p1.min_height = ui_fx_from_text_266(measured_266.y, ui->to_px_from_pt);
                min_content_cross = ui_fx_max(min_content_cross, child_node->p1.min_height);
            }
        }
    }

    if (is_text && (p0->width.mode == ui_size_mode_fit || p0->height.mode == ui_size_mode_fit)) {
        int32_t width_266 = p0->width.mode == ui_size_mode_fix ? ui_fx_to_text_266(p0->width.constant, ui->to_px_from_pt) : int32_max;
        int2 measured_266 = ui_text_measure(ui, node, width_266);

        min_content_main = ui_fx_from_text_266(measured_266.y, ui->to_px_from_pt);
        min_content_cross = ui_fx_from_text_266(measured_266.x, ui->to_px_from_pt);
    }

    struct ui_fx fit_width;
    if (p0->mode == ui_layout_mode_stack_x) {
        fit_width = ui_fx_add(min_content_main, width_padding);
    } else if (p0->mode == ui_layout_mode_stack_y || is_text) {
        fit_width = ui_fx_add(min_content_cross, width_padding);
    } else {
        fit_width = ui_fx_zero;
    }

    struct ui_fx fit_height;
    if (p0->mode == ui_layout_mode_stack_x) {
        fit_height = ui_fx_add(min_content_cross, height_padding);
    } else if (p0->mode == ui_layout_mode_stack_y || is_text) {
        fit_height = ui_fx_add(min_content_main, height_padding);
    } else {
        fit_height = ui_fx_zero;
    }

    if (node->type == ui_content_type_shape && node->shape.shape_type == ui_shape_type_line) {
        struct ui_fx thickness = ui_fx_make_px_size(node->shape.thickness > 0.0f ? node->shape.thickness : 1.0f, ui->to_px_from_pt);
        fit_width = ui_fx_max(fit_width, thickness);
        fit_height = ui_fx_max(fit_height, thickness);
    }

    struct ui_fx min_width;
    struct ui_fx min_height;

    switch (p0->width.mode) {
    case ui_size_mode_fit: {
        min_width = fit_width;
    } break;
    case ui_size_mode_fill: {
        if (parent_layout_mode == ui_layout_mode_stack_y) {
            min_width = fit_width;
        } else {
            min_width = width_padding;
        }
    } break;
    case ui_size_mode_fix: {
        min_width = ui_fx_max(p0->width.constant, width_padding);
    } break;
    case ui_size_mode_rel: {
        min_width = ui_fx_zero;
    } break;
    }

    switch (p0->height.mode) {
    case ui_size_mode_fit: {
        min_height = fit_height;
    } break;
    case ui_size_mode_fill: {
        if (parent_layout_mode == ui_layout_mode_stack_x) {
            min_height = fit_height;
        } else {
            min_height = height_padding;
        }
    } break;
    case ui_size_mode_fix: {
        min_height = ui_fx_max(p0->height.constant, height_padding);
    } break;
    case ui_size_mode_rel: {
        min_height = ui_fx_zero;
    } break;
    }

    p1->fill_count = fill_count;
    p1->min_width = min_width;
    p1->min_height = min_height;
    p1->cons_valid = false;
}

static void ui_layout_p2(struct ui* ui, struct ui_fx x, struct ui_fx y, struct ui_fx width, struct ui_fx height, bool is_root, struct ui_node_backing* node) {
    const struct ui_layout_p0* p0;
    const struct ui_layout_p1* p1;
    struct ui_layout_p2* p2;
    {
        p0 = &node->p0;
        p1 = &node->p1;
        p2 = &node->p2;
    }

    struct ui_fx width_padding = ui_layout_padding(p0, true);
    struct ui_fx height_padding = ui_layout_padding(p0, false);
    struct ui_fx offset = ui_fx_zero;
    int32_t fill_extra_px = 0;
    struct ui_fx fill_width = ui_fx_zero;
    struct ui_fx fill_height = ui_fx_zero;

    bool is_stack_x = p0->mode == ui_layout_mode_stack_x;
    bool is_stack_y = p0->mode == ui_layout_mode_stack_y;

    if (is_stack_x || is_stack_y) {
        struct ui_fx min_content_main = ui_layout_p2_cons(ui, is_stack_x, is_stack_x ? height : width, node);

        if (is_root) {
            if (is_stack_x && p0->width.mode == ui_size_mode_fit) {
                width = ui_fx_add(min_content_main, width_padding);
            }
            if (is_stack_y && p0->height.mode == ui_size_mode_fit) {
                height = ui_fx_add(min_content_main, height_padding);
            }
        }

        struct ui_fx rem;
        if (is_stack_x) {
            rem = ui_fx_sub(ui_fx_sub(width, min_content_main), width_padding);
        } else {
            rem = ui_fx_sub(ui_fx_sub(height, min_content_main), height_padding);
        }

        struct ui_fx fill = ui_fx_zero;
        if (ui_fx_is_positive(rem) && p1->fill_count != 0) {
            int32_t rem_px = ui_fx_get_px(rem);
            fill = ui_fx_make(rem_px / (int32_t)p1->fill_count);
            fill_extra_px = rem_px % (int32_t)p1->fill_count;
            assert(fill_extra_px >= 0);
        } else {
            switch (p0->dist) {
            case ui_dist_start: {
                offset = ui_fx_zero;
            } break;
            case ui_dist_center: {
                offset = ui_fx_round_px(ui_fx_div(rem, ui_fx_make(2)));
            } break;
            case ui_dist_end: {
                offset = rem;
            } break;
            }
        }

        if (is_stack_x) {
            offset = ui_fx_add(offset, p0->padding_left);
            fill_width = fill;
            fill_height = ui_fx_max(ui_fx_sub(height, height_padding), ui_fx_zero);
        } else {
            offset = ui_fx_add(offset, p0->padding_top);
            fill_width = ui_fx_max(ui_fx_sub(width, width_padding), ui_fx_zero);
            fill_height = fill;
        }
    }

    assert(ui_fx_is_whole_px(x));
    assert(ui_fx_is_whole_px(y));
    assert(ui_fx_is_whole_px(width));
    assert(ui_fx_is_whole_px(height));

    p2->x = x;
    p2->y = y;
    p2->width = width;
    p2->height = height;

    uint32_t fill_count_current = 0;

    for (struct ui_node_backing* child_node = ui_node_get_child(ui, node, 0); child_node != NULL; child_node = ui_node_get_next(ui, child_node)) {
        bool child_is_text;
        const struct ui_layout_p0* child_p0;
        const struct ui_layout_p1* child_p1;
        {
            child_is_text = child_node->type == ui_content_type_text;
            child_p0 = &child_node->p0;
            child_p1 = &child_node->p1;
        }

        struct ui_fx child_width;
        struct ui_fx child_height;
        struct ui_fx child_x;
        struct ui_fx child_y;

        if (child_p0->x.mode == ui_pos_mode_auto) {
            switch (child_p0->width.mode) {
            case ui_size_mode_fit:
            case ui_size_mode_fix: {
                child_width = child_p1->min_width;
            } break;
            case ui_size_mode_fill: {
                if (is_stack_x) {
                    child_width = ui_fx_add(child_p1->min_width, fill_width);
                } else {
                    child_width = ui_fx_max(fill_width, ui_layout_padding(child_p0, true));
                }
            } break;
            case ui_size_mode_rel:
                assert(0);
            }

            switch (child_p0->height.mode) {
            case ui_size_mode_fit:
            case ui_size_mode_fix: {
                child_height = child_p1->min_height;
            } break;
            case ui_size_mode_fill: {
                if (is_stack_y) {
                    child_height = ui_fx_add(child_p1->min_height, fill_height);
                } else {
                    child_height = ui_fx_max(fill_height, ui_layout_padding(child_p0, false));
                }
            } break;
            case ui_size_mode_rel:
                assert(0);
            }

            if ((is_stack_x && child_p0->width.mode == ui_size_mode_fill) || (is_stack_y && child_p0->height.mode == ui_size_mode_fill)) {
                int32_t extra_px = ui_layout_fill_extra_px(fill_count_current++, fill_extra_px, p1->fill_count);
                if (extra_px != 0) {
                    if (is_stack_x) {
                        child_width = ui_fx_add(child_width, ui_fx_make(extra_px));
                    } else {
                        child_height = ui_fx_add(child_height, ui_fx_make(extra_px));
                    }
                }
            }

            if (child_is_text && child_p0->width.mode == ui_size_mode_fill && child_p0->height.mode == ui_size_mode_fit) {
                int32_t measure_width_266 = max_int32(ui_fx_to_text_266(child_width, ui->to_px_from_pt), 0);
                child_height = ui_fx_from_text_266(ui_text_measure(ui, child_node, measure_width_266).y, ui->to_px_from_pt);
            }

            switch (p0->mode) {
            case ui_layout_mode_manual:
                assert(0);
            case ui_layout_mode_stack_x: {
                child_x = offset;
                if (child_p0->height.mode == ui_size_mode_fill) {
                    child_y = ui_fx_zero;
                } else {
                    switch (p0->align) {
                    case ui_align_start: {
                        child_y = ui_fx_zero;
                    } break;
                    case ui_align_center: {
                        child_y = height;
                        child_y = ui_fx_sub(child_y, child_height);
                        child_y = ui_fx_sub(child_y, height_padding);
                        child_y = ui_fx_round_px(ui_fx_div(child_y, ui_fx_make(2)));
                    } break;
                    case ui_align_end: {
                        child_y = height;
                        child_y = ui_fx_sub(child_y, child_height);
                        child_y = ui_fx_sub(child_y, height_padding);
                    } break;
                    }
                }
                child_y = ui_fx_add(child_y, p0->padding_top);
                offset = ui_fx_add(offset, child_width);
                offset = ui_fx_add(offset, p0->spacing);
            } break;
            case ui_layout_mode_stack_y: {
                if (child_p0->width.mode == ui_size_mode_fill) {
                    child_x = ui_fx_zero;
                } else {
                    switch (p0->align) {
                    case ui_align_start: {
                        child_x = ui_fx_zero;
                    } break;
                    case ui_align_center: {
                        child_x = width;
                        child_x = ui_fx_sub(child_x, child_width);
                        child_x = ui_fx_sub(child_x, width_padding);
                        child_x = ui_fx_round_px(ui_fx_div(child_x, ui_fx_make(2)));
                    } break;
                    case ui_align_end: {
                        child_x = width;
                        child_x = ui_fx_sub(child_x, child_width);
                        child_x = ui_fx_sub(child_x, width_padding);
                    } break;
                    }
                }
                child_x = ui_fx_add(child_x, p0->padding_left);
                child_y = offset;
                offset = ui_fx_add(offset, child_height);
                offset = ui_fx_add(offset, p0->spacing);
            } break;
            }
        } else {
            assert(child_p0->width.mode != ui_size_mode_fill);
            assert(child_p0->height.mode != ui_size_mode_fill);

            bool is_width_rel = child_p0->width.mode == ui_size_mode_rel;
            bool is_height_rel = child_p0->height.mode == ui_size_mode_rel;

            child_width = is_width_rel ? ui_layout_size_rel(child_p0, true, width, height) : child_p1->min_width;
            child_height = is_height_rel ? ui_layout_size_rel(child_p0, false, width, height) : child_p1->min_height;

            if (child_p0->mode != ui_layout_mode_manual || child_is_text) {
                bool is_x = child_p0->mode == ui_layout_mode_stack_x;
                ui_layout_p2_cons(ui, is_x, is_x ? child_height : child_width, child_node);
                child_width = is_width_rel ? child_width : child_p1->min_width;
                child_height = is_height_rel ? child_height : child_p1->min_height;
            }

            child_x = ui_layout_pos(&child_p0->x, width, child_width);
            child_y = ui_layout_pos(&child_p0->y, height, child_height);
        }

        ui_layout_p2(ui, child_x, child_y, child_width, child_height, false, child_node);
    }
}

static struct ui_fx ui_layout_p2_cons(struct ui* ui, bool is_x, struct ui_fx size_cross, struct ui_node_backing* node) {
    bool is_text;
    const struct ui_layout_p0* p0;
    struct ui_layout_p1* p1;
    {
        is_text = node->type == ui_content_type_text;
        p0 = &node->p0;
        p1 = &node->p1;
    }

    if (p1->cons_valid && p1->cons_is_x == is_x && p1->cons_size_cross_raw == size_cross._raw) {
        return p1->cons_min_content_main;
    }

    struct ui_fx fill_cross = ui_fx_max(ui_fx_sub(size_cross, ui_layout_padding(p0, is_x == false)), ui_fx_zero);
    uint32_t stack_count = 0;

    struct ui_fx min_content_main = ui_fx_zero;

    for (struct ui_node_backing* child_node = ui_node_get_child(ui, node, 0); child_node != NULL; child_node = ui_node_get_next(ui, child_node)) {
        bool child_is_text;
        const struct ui_layout_p0* child_p0;
        const struct ui_layout_p1* child_p1;
        {
            child_is_text = child_node->type == ui_content_type_text;
            child_p0 = &child_node->p0;
            child_p1 = &child_node->p1;
        }

        if (child_p0->x.mode != ui_pos_mode_auto) {
            continue;
        }

        stack_count++;

        if ((is_x && child_p0->width.mode == ui_size_mode_fit && (child_is_text || child_p0->mode == ui_layout_mode_stack_x)) ||
            (!is_x && child_p0->height.mode == ui_size_mode_fit && (child_is_text || child_p0->mode == ui_layout_mode_stack_y))) {
            enum ui_size_mode child_mode_cross = is_x ? child_p0->height.mode : child_p0->width.mode;
            struct ui_fx child_size_cross;

            switch (child_mode_cross) {
            case ui_size_mode_fit:
            case ui_size_mode_fix: {
                child_size_cross = is_x ? child_p1->min_height : child_p1->min_width;
            } break;
            case ui_size_mode_fill: {
                child_size_cross = fill_cross;
            } break;
            case ui_size_mode_rel:
                assert(0);
            }

            ui_layout_p2_cons(ui, is_x, child_size_cross, child_node);
        }

        min_content_main = ui_fx_add(min_content_main, is_x ? child_p1->min_width : child_p1->min_height);
    }

    if (stack_count > 1) {
        struct ui_fx spacing_total = ui_fx_mul(p0->spacing, ui_fx_make((int32_t)(stack_count - 1)));
        min_content_main = ui_fx_add(min_content_main, spacing_total);
    }

    if (is_text) {
        int32_t size_cross_266 = max_int32(ui_fx_to_text_266(size_cross, ui->to_px_from_pt), 0);
        int2 measured_266 = ui_text_measure(ui, node, is_x ? int32_max : size_cross_266);
        min_content_main = ui_fx_from_text_266(is_x ? measured_266.x : measured_266.y, ui->to_px_from_pt);

        enum ui_size_mode mode_cross = is_x ? p0->height.mode : p0->width.mode;
        if (mode_cross == ui_size_mode_fit) {
            struct ui_fx min_content_cross = ui_fx_from_text_266(is_x ? measured_266.y : measured_266.x, ui->to_px_from_pt);
            if (is_x) {
                p1->min_height = min_content_cross;
            } else {
                p1->min_width = min_content_cross;
            }
        }
    }

    struct ui_fx padding_main = ui_layout_padding(p0, is_x);

    if (is_x && p0->width.mode == ui_size_mode_fit) {
        p1->min_width = ui_fx_add(min_content_main, padding_main);
    } else if (!is_x && p0->height.mode == ui_size_mode_fit) {
        p1->min_height = ui_fx_add(min_content_main, padding_main);
    }

    p1->cons_valid = true;
    p1->cons_is_x = is_x;
    p1->cons_size_cross_raw = size_cross._raw;
    p1->cons_min_content_main = min_content_main;

    return min_content_main;
}
