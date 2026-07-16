#include "cr.h"
#include "ui.h"
#include "ui_list.h"
#include "ui_list_internal.h"
#include "../core/ui_fx.h"

struct ui_list_linear_metrics {
    enum ui_list_axis main_axis;
    enum ui_list_axis cross_axis;
    double to_px_from_pt;
    double2 padding_start;
    double2 padding_end;
    double main_space;
    double cross_space;
    double spacing_main;
    double spacing_cross;
};

static struct ui_list_linear_metrics ui_list_linear_metrics_make(const struct ui_list_linear* linear) {
    struct ui_list_linear_metrics metrics;
    double to_px_from_pt = ui_get_to_px_from_pt();

    metrics.main_axis = linear->axis;
    metrics.cross_axis = ui_list_cross_axis(linear->axis);
    metrics.to_px_from_pt = to_px_from_pt;

    double2 size = make_double2(ui_round_px_ex(linear->size.x, to_px_from_pt), ui_round_px_ex(linear->size.y, to_px_from_pt));

    metrics.padding_start = make_double2(ui_round_px_ex(linear->padding.x, to_px_from_pt), ui_round_px_ex(linear->padding.z, to_px_from_pt));
    metrics.padding_end = make_double2(ui_round_px_ex(linear->padding.y, to_px_from_pt), ui_round_px_ex(linear->padding.w, to_px_from_pt));

    double2 space = max_double2(size - metrics.padding_start - metrics.padding_end, make_double2(0.0, 0.0));

    metrics.main_space = ui_list_component(space, metrics.main_axis);
    metrics.cross_space = ui_list_component(space, metrics.cross_axis);

    metrics.spacing_main = ui_round_px_ex(ui_list_component(linear->spacing, metrics.main_axis), to_px_from_pt);
    metrics.spacing_cross = ui_round_px_ex(ui_list_component(linear->spacing, metrics.cross_axis), to_px_from_pt);

    return metrics;
}

struct ui_list_linear_line {
    uint32_t end;
    double main_extent;
    double cross_extent;
};

static struct ui_list_linear_line ui_list_linear_line(const struct ui_list_linear_metrics* metrics, bool wrap, const struct ui_list_layout_item* items, uint32_t count, uint32_t first, bool rebase) {
    struct ui_list_linear_line line = { .end = first };
    double wrap_extent = 0.0;

    while (line.end < count) {
        const struct ui_list_layout_item* item = &items[line.end];
        double wrap_main = ui_list_component(rebase ? item->prev_size : item->size, metrics->main_axis);
        double spacing = line.end > first ? metrics->spacing_main : 0.0;

        if (wrap && line.end > first && wrap_extent + metrics->spacing_main + wrap_main > metrics->main_space + 0.0001) {
            break;
        }

        wrap_extent += spacing + wrap_main;
        line.main_extent += spacing + ui_list_component(item->size, metrics->main_axis);
        line.cross_extent = max_double(line.cross_extent, ui_list_component(item->size, metrics->cross_axis));
        line.end++;
    }

    return line;
}

static double2 ui_list_linear_place(struct ui_list_layout_item* items, uint32_t count, bool rebase, void* user) {
    const struct ui_list_linear* linear = user;
    struct ui_list_linear_metrics metrics = ui_list_linear_metrics_make(linear);

    double content_main = 0.0;
    double content_cross = 0.0;

    double padding_main = ui_list_component(metrics.padding_start, metrics.main_axis);
    double cross_position = ui_list_component(metrics.padding_start, metrics.cross_axis);

    for (uint32_t first = 0, line_index = 0; first < count; line_index++) {
        struct ui_list_linear_line line = ui_list_linear_line(&metrics, linear->wrap, items, count, first, rebase);

        content_main = max_double(content_main, line.main_extent);
        content_cross += (line_index > 0 ? metrics.spacing_cross : 0.0) + line.cross_extent;

        double leftover = max_double(metrics.main_space - line.main_extent, 0.0);
        double main_position = padding_main;

        if (linear->dist == ui_dist_center) {
            main_position = ui_round_px_fx_ex(main_position + leftover * 0.5, metrics.to_px_from_pt);
        } else if (linear->dist == ui_dist_end) {
            main_position += leftover;
        }

        double align_space = linear->wrap ? line.cross_extent : metrics.cross_space;

        for (uint32_t i = first; i < line.end; i++) {
            double2 size = items[i].size;

            double align_leftover = align_space - ui_list_component(size, metrics.cross_axis);
            double item_cross_position = cross_position;

            if (linear->align == ui_align_center) {
                item_cross_position = ui_round_px_fx_ex(cross_position + align_leftover * 0.5, metrics.to_px_from_pt);
            } else if (linear->align == ui_align_end) {
                item_cross_position = cross_position + align_leftover;
            }

            items[i].offset = ui_list_from_components(metrics.main_axis, main_position, item_cross_position);
            main_position += ui_list_component(size, metrics.main_axis) + metrics.spacing_main;
        }

        cross_position += line.cross_extent + metrics.spacing_cross;
        first = line.end;
    }

    double2 content_size = ui_list_from_components(metrics.main_axis, content_main, content_cross);

    return content_size + metrics.padding_start + metrics.padding_end;
}

static double2 ui_list_linear_item_size(double2 size, double2 available, void* user) {
    const struct ui_list_linear* linear = user;
    enum ui_list_axis main_axis = linear->axis;
    enum ui_list_axis cross_axis = ui_list_cross_axis(main_axis);

    double size_main = ui_list_component(size, main_axis);
    double size_cross = ui_list_component(size, cross_axis);
    double main_available = ui_list_component(available, main_axis);

    if (size_cross <= 0.0) {
        size_cross = ui_list_component(available, cross_axis);
    }

    if (main_available > 0.0 && size_main > main_available) {
        size_cross *= main_available / size_main;
        size_main = main_available;
    }

    return ui_list_from_components(main_axis, size_main, size_cross);
}

struct ui_list_layout ui_list_linear_layout(const struct ui_list_linear* linear) {
    struct ui_list_linear_metrics metrics = ui_list_linear_metrics_make(linear);

    return (struct ui_list_layout) {
        .place = ui_list_linear_place,
        .item_size = ui_list_linear_item_size,
        .user = (struct ui_list_linear*)linear,
        .scroll_axis = linear->wrap ? metrics.cross_axis : metrics.main_axis,
        .size = linear->size,
        .item_available = ui_list_from_components(metrics.main_axis, metrics.main_space, metrics.cross_space),
    };
}

struct ui_list_linear_insert ui_list_linear_insert(const struct ui_list_linear* linear, const struct ui_list_item_desc* desc, double2 position) {
    cr_check(desc->is_removed == false, "a removed item cannot be an insertion target");

    struct ui_list_linear_metrics metrics = ui_list_linear_metrics_make(linear);

    double2 position_in_target = position - desc->pos;

    double offset_main = ui_list_component(desc->pos, metrics.main_axis);
    double size_main = ui_list_component(desc->size, metrics.main_axis);
    double size_cross = ui_list_component(desc->size, metrics.cross_axis);

    bool insert_after = ui_list_component(position_in_target, metrics.main_axis) >= size_main * 0.5;

    double cursor_main = insert_after
        ? offset_main + size_main + metrics.spacing_main * 0.5
        : offset_main - metrics.spacing_main * 0.5;
    double cursor_cross = ui_list_component(desc->pos, metrics.cross_axis) + size_cross * 0.5;

    struct ui_list_linear_insert insert = {
        .index = desc->index + (insert_after ? 1u : 0u),
        .cursor_pos = ui_list_from_components(metrics.main_axis, cursor_main, cursor_cross),
        .cursor_size = ui_list_from_components(metrics.main_axis, 0.0, size_cross),
    };

    insert.cursor_pos.x = ui_round_px_fx_ex(insert.cursor_pos.x, metrics.to_px_from_pt);
    insert.cursor_pos.y = ui_round_px_fx_ex(insert.cursor_pos.y, metrics.to_px_from_pt);

    return insert;
}
