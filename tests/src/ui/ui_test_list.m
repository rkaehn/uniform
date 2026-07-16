#include "ui_tests.h"
#include "ui_gestures.h"
#include "ui_list.h"
#include "ui_core.h"
#include <stdio.h>

typedef void ui_test_list_on_item_update_t(uint32_t index, void* content, double2 available_size, struct ui_list_on_item_update_result* out_result, void* user);

struct ui_test_list_fixture {
    struct ui_list* view;
    ui_test_list_on_item_update_t* on_item_update;
    void (*on_drag)(struct ui_list_drag drag, void* user);
    void* user;
};

struct ui_test_list_frame {
    struct ui_list_item_desc item_descs[256];
    uint32_t item_desc_count;
};

static struct ui_test_list_frame ui_test_list_collect_(struct ui_list* view) {
    struct ui_test_list_frame frame = { 0 };

    struct ui_list_iter iter = ui_list_iter(view);
    struct ui_list_item_desc desc;
    while (ui_list_iter_next(&iter, &desc)) {
        assert(frame.item_desc_count < cr_count(frame.item_descs));
        frame.item_descs[frame.item_desc_count++] = desc;
    }

    return frame;
}

static void ui_test_check(bool ok, const char* label) {
    ui_test_group_check_("list", ok, label);
}

struct ui_test_list_sizes {
    double2 base;
    double2 wide;
    uint32_t wide_index;
};

static void ui_test_list_sized_update_(uint32_t index, void* content, double2 available_size, struct ui_list_on_item_update_result* out_result, void* user) {
    const struct ui_test_list_sizes* sizes = user;

    out_result->size = index == sizes->wide_index ? sizes->wide : sizes->base;
}

static struct ui_test_list_fixture ui_test_list_fixture_(struct cr_mem* mem, uint64_t content_size, ui_test_list_on_item_update_t* on_item_update, void* user) {
    return (struct ui_test_list_fixture) {
        .view = ui_list_create(mem, content_size),
        .on_item_update = on_item_update,
        .user = user,
    };
}

static struct ui_test_list_fixture ui_test_list_make_(struct cr_mem* mem, struct ui_test_list_sizes* sizes) {
    return ui_test_list_fixture_(mem, 0, ui_test_list_sized_update_, sizes);
}

static void ui_test_list_fixture_destroy_(struct ui_test_list_fixture* fixture) {
    ui_list_destroy(fixture->view);
    fixture->view = NULL;
}

static void ui_test_list_append_(struct ui_list* view, uint32_t count) {
    uint32_t* adds = cr_malloc(cr_mem_libc, count * sizeof(adds[0]), 0, cr_loc);

    for (uint32_t i = 0; i < count; i++) {
        adds[i] = i;
    }

    ui_list_edit(view, &(struct ui_list_edit) { .adds = adds, .add_count = count });
    cr_free(cr_mem_libc, adds, cr_loc);
}

static void ui_test_list_add_(struct ui_list* view, uint32_t index, bool animated) {
    ui_list_edit(view, &(struct ui_list_edit) { .adds = &index, .add_count = 1, .animated = animated });
}

static void ui_test_list_delete_(struct ui_list* view, uint32_t index, bool animated) {
    ui_list_edit(view, &(struct ui_list_edit) { .deletes = &index, .delete_count = 1, .animated = animated });
}

static void ui_test_list_move_(struct ui_list* view, uint32_t src_index, uint32_t dst_index) {
    struct ui_list_item_move move = { .src_index = src_index, .dst_index = dst_index };
    ui_list_edit(view, &(struct ui_list_edit) { .moves = &move, .move_count = 1, .animated = true });
}

static struct ui_list_linear ui_test_list_column_layout_(double width, double height) {
    return (struct ui_list_linear) {
        .axis = ui_list_axis_y,
        .size = make_double2(width, height),
    };
}

static struct ui_list_linear ui_test_list_row_layout_(bool wrap, double width, double height) {
    return (struct ui_list_linear) {
        .axis = ui_list_axis_x,
        .wrap = wrap,
        .size = make_double2(width, height),
        .padding = make_double4(10.0, 10.0, 10.0, 10.0),
        .spacing = make_double2(5.0, 5.0),
    };
}

static void ui_test_list_update_(struct ui_test_list_fixture* fixture, struct ui_node node, struct ui_list_layout layout, bool snap) {
    ui_list_update(fixture->view, &(struct ui_list_update) {
        .node = node,
        .layout = layout,
        .snap = snap,
        .delta_s = 1.0 / 60.0,
        .on_item_update = fixture->on_item_update,
        .on_drag = fixture->on_drag,
        .user = fixture->user,
    });
}

static struct ui_test_list_frame ui_test_list_layout_step_(struct ui_test_list_fixture* fixture, struct ui_list_layout layout, bool snap) {
    struct ui_node root = ui_frame(&(struct ui_frame) {
        .size = ui_size_fix(layout.size.x, layout.size.y),
    });

    ui_test_list_update_(fixture, root, layout, snap);

    struct ui_test_list_frame frame = ui_test_list_collect_(fixture->view);

    ui_release(root);

    return frame;
}

static struct ui_test_list_frame ui_test_list_step_(struct ui_test_list_fixture* fixture, const struct ui_list_linear* linear) {
    return ui_test_list_layout_step_(fixture, ui_list_linear_layout(linear), false);
}

static struct ui_test_list_frame ui_test_list_snap_step_(struct ui_test_list_fixture* fixture, const struct ui_list_linear* linear) {
    return ui_test_list_layout_step_(fixture, ui_list_linear_layout(linear), true);
}

static struct ui_test_list_frame ui_test_list_settle_(struct ui_test_list_fixture* fixture, const struct ui_list_linear* linear, uint32_t frames) {
    struct ui_test_list_frame frame = { 0 };

    for (uint32_t i = 0; i < frames; i++) {
        frame = ui_test_list_step_(fixture, linear);
    }

    return frame;
}

static void ui_test_list_lifecycle(void) {
    struct cr_mem* mem = cr_mem_libc;

    uint32_t before = cr_rec_get_alloc_count();

    struct ui_input* prev_input = ui_input_get();
    ui_input_set(ui_input_create(mem));

    {
        struct ui_test_list_sizes sizes = { .base = make_double2(0.0, 20.0), .wide_index = uint32_max };

        struct ui_test_list_fixture fixture = ui_test_list_fixture_(mem, 16, ui_test_list_sized_update_, &sizes);

        struct ui_list_linear layout = ui_test_list_column_layout_(200.0, 200.0);

        ui_test_list_settle_(&fixture, &layout, 2);

        ui_test_list_fixture_destroy_(&fixture);
    }

    ui_input_destroy(ui_input_get());
    ui_input_set(prev_input);

    ui_test_check(cr_rec_get_alloc_count() == before, "component lifecycle alloc balance");
}

static void ui_test_list_growth(void) {
    struct cr_mem* mem = cr_mem_libc;

    uint32_t before = cr_rec_get_alloc_count();

    struct ui_input* prev_input = ui_input_get();
    ui_input_set(ui_input_create(mem));

    {
        struct ui_test_list_sizes sizes = { .base = make_double2(0.0, 20.0), .wide_index = uint32_max };

        struct ui_test_list_fixture fixture = ui_test_list_fixture_(mem, 16, ui_test_list_sized_update_, &sizes);

        uint32_t add_count = 300;
        ui_test_list_append_(fixture.view, add_count);

        struct ui_list_linear layout = ui_test_list_column_layout_(200.0, 200.0);

        struct ui_test_list_frame frame = ui_test_list_step_(&fixture, &layout);

        ui_test_check(frame.item_desc_count > 0 && frame.item_desc_count < add_count, "culls offscreen rows");
        ui_test_check(frame.item_desc_count > 0 && frame.item_descs[0].index == 0 && frame.item_descs[0].pos.x == 0.0 && frame.item_descs[0].pos.y == 0.0, "first row at origin");

        uint32_t deletes[64];
        for (uint32_t i = 0; i < cr_count(deletes); i++) {
            deletes[i] = i;
        }

        ui_list_edit(fixture.view, &(struct ui_list_edit) { .deletes = deletes, .delete_count = cr_count(deletes) });

        ui_test_list_fixture_destroy_(&fixture);
    }

    ui_input_destroy(ui_input_get());
    ui_input_set(prev_input);

    ui_test_check(cr_rec_get_alloc_count() == before, "growth alloc balance");
}

static void ui_test_list_horizontal(void) {
    struct cr_mem* mem = cr_mem_libc;

    struct ui_test_list_sizes sizes = { .base = make_double2(60.0, 80.0), .wide_index = uint32_max };

    struct ui_test_list_fixture fixture = ui_test_list_make_(mem, &sizes);
    ui_test_list_append_(fixture.view, 5);

    struct ui_list_linear layout = {
        .axis = ui_list_axis_x,
        .size = make_double2(200.0, 100.0),
    };

    struct ui_test_list_frame frame = ui_test_list_step_(&fixture, &layout);

    ui_test_check(frame.item_desc_count == 4, "horizontal culls on x");
    ui_test_check(frame.item_descs[1].pos.x == 60.0 && frame.item_descs[1].pos.y == 0.0, "horizontal linear");
    ui_test_check(frame.item_descs[0].size.x == 60.0 && frame.item_descs[0].size.y == 80.0, "horizontal item size");

    layout.dist = ui_dist_end;

    frame = ui_test_list_step_(&fixture, &layout);

    ui_test_check(frame.item_descs[0].pos.x == 0.0, "horizontal overflow stays reachable");

    ui_test_list_fixture_destroy_(&fixture);
}

static void ui_test_list_grid_layout(void) {
    struct cr_mem* mem = cr_mem_libc;

    uint32_t before = cr_rec_get_alloc_count();

    struct ui_input* prev_input = ui_input_get();
    ui_input_set(ui_input_create(mem));

    {
        struct ui_test_list_sizes sizes = { .base = make_double2(50.0, 50.0), .wide_index = uint32_max };

        struct ui_test_list_fixture fixture = ui_test_list_make_(mem, &sizes);
        ui_test_list_append_(fixture.view, 8);

        struct ui_list_linear layout = ui_test_list_row_layout_(true, 200.0, 300.0);
        layout.dist = ui_dist_center;

        struct ui_test_list_frame frame = ui_test_list_step_(&fixture, &layout);

        ui_test_check(frame.item_desc_count == 8, "snaps to size on first frame");

        frame = ui_test_list_settle_(&fixture, &layout, 200);

        ui_test_check(frame.item_desc_count == 8, "grid shows all items");
        ui_test_check(frame.item_descs[0].pos.x == 20.0 && frame.item_descs[0].pos.y == 10.0, "grid centers first row");
        ui_test_check(frame.item_descs[1].pos.x == 75.0 && frame.item_descs[1].pos.y == 10.0, "grid spaces columns");
        ui_test_check(frame.item_descs[3].pos.x == 20.0 && frame.item_descs[3].pos.y == 65.0, "grid wraps rows");
        ui_test_check(frame.item_descs[0].size.x == 50.0 && frame.item_descs[0].size.y == 50.0, "grid item size");

        ui_test_list_delete_(fixture.view, 0, true);

        frame = ui_test_list_step_(&fixture, &layout);

        ui_test_check(frame.item_desc_count == 8 && frame.item_descs[0].is_removed, "grid keeps removed item while fading");

        frame = ui_test_list_settle_(&fixture, &layout, 300);

        ui_test_check(frame.item_desc_count == 7, "grid drops faded item");
        ui_test_check(frame.item_descs[0].index == 0 && frame.item_descs[0].pos.x == 20.0 && frame.item_descs[0].pos.y == 10.0, "grid reflows after delete");
        ui_test_check(frame.item_descs[3].pos.x == 20.0 && frame.item_descs[3].pos.y == 65.0, "grid pulls next row up");

        ui_test_list_fixture_destroy_(&fixture);
    }

    ui_input_destroy(ui_input_get());
    ui_input_set(prev_input);

    ui_test_check(cr_rec_get_alloc_count() == before, "grid alloc balance");
}

static void ui_test_list_collapse_item_update(uint32_t index, void* content, double2 available_size, struct ui_list_on_item_update_result* out_result, void* user) {
    out_result->size.y = 20.0;
    out_result->collapse_end_index = index == 0 ? 3 : 0;
}

static void ui_test_list_collapse(void) {
    struct cr_mem* mem = cr_mem_libc;

    struct ui_test_list_fixture fixture = ui_test_list_fixture_(mem, 0, ui_test_list_collapse_item_update, NULL);
    ui_test_list_append_(fixture.view, 4);

    struct ui_list_linear layout = ui_test_list_column_layout_(200.0, 300.0);

    struct ui_test_list_frame frame = ui_test_list_step_(&fixture, &layout);

    ui_test_check(frame.item_desc_count == 2, "collapse starts hidden");

    frame = ui_test_list_settle_(&fixture, &layout, 300);

    ui_test_check(frame.item_desc_count == 2, "collapse hides subtree");
    ui_test_check(frame.item_descs[0].index == 0 && frame.item_descs[0].pos.y == 0.0, "collapse keeps group row");
    ui_test_check(frame.item_descs[1].index == 3 && frame.item_descs[1].pos.y == 20.0, "collapse pulls next row up");

    struct ui_list_item_desc target;

    ui_test_check(ui_list_get_item_at_position(fixture.view, make_double2(100.0, 25.0), &target) && target.index == 3, "collapse skips hidden rows for hit test");

    ui_test_list_fixture_destroy_(&fixture);
}

struct ui_test_list_stack_state {
    const double (*sizes)[2];
    bool is_x;
};

static void ui_test_list_stack_item_update(uint32_t index, void* content, double2 available_size, struct ui_list_on_item_update_result* out_result, void* user) {
    const struct ui_test_list_stack_state* state = user;
    const double* item = state->sizes[index];

    out_result->size = state->is_x ? make_double2(item[1], item[0]) : make_double2(item[0], item[1]);
}

static void ui_test_list_matches_stack(void) {
    struct cr_mem* mem = cr_mem_libc;

    static const double stack_sizes[5][2] = {
        { 50.0, 20.0 },
        { 30.0, 24.0 },
        { 40.0, 18.0 },
        { 44.0, 0.1 },
        { 60.0, 16.0 },
    };

    static const double stack_sizes_frac[5][2] = {
        { 50.3, 20.9 },
        { 29.6, 24.2 },
        { 40.7, 17.8 },
        { 44.1, 0.1 },
        { 60.4, 16.3 },
    };

    double saved_scale = ui_get_to_px_from_pt();

    const double scales[] = { 1.0, 2.0, 3.0 };

    const struct {
        const char* label;
        const double (*sizes)[2];
        double main_size;
        double cross_size;
        double4 pad;
        double spacing;
    } configs[] = {
        { "integer", stack_sizes, 123.0, 61.0, make_double4(9.0, 4.0, 5.0, 3.0), 7.0 },
        { "fractional", stack_sizes_frac, 123.4, 61.7, make_double4(9.3, 4.1, 5.7, 3.2), 7.3 },
    };

    struct ui_test_list_stack_state state = { 0 };

    for (uint32_t s = 0; s < cr_count(scales); s++) {
        for (uint32_t c = 0; c < cr_count(configs); c++) {
            double scale = scales[s];
            ui_set_to_px_from_pt(scale);
            state.sizes = configs[c].sizes;

            double main_size = configs[c].main_size;
            double cross_size = configs[c].cross_size;
            double spacing = configs[c].spacing;

            bool ok = true;

            for (uint32_t axis = 0; axis < 2; axis++) {
                bool is_x = axis == 1;
                state.is_x = is_x;

                double4 cfg_pad = configs[c].pad;
                double4 pad = is_x ? cfg_pad : make_double4(cfg_pad.z, cfg_pad.w, cfg_pad.x, cfg_pad.y);
                double2 size = is_x ? make_double2(main_size, cross_size) : make_double2(cross_size, main_size);

                for (uint32_t dist = 0; dist < 3; dist++) {
                    for (uint32_t align = 0; align < 3; align++) {
                        struct ui_node stack_root = ui_frame(&(struct ui_frame) {
                            .size = ui_size_fix(size.x, size.y),
                            .layout = is_x
                                ? ui_layout_stack_x((enum ui_dist)dist, (enum ui_align)align, spacing, pad)
                                : ui_layout_stack_y((enum ui_dist)dist, (enum ui_align)align, spacing, pad),
                        });

                        struct ui_node kids[cr_count(stack_sizes)];

                        for (uint32_t i = 0; i < cr_count(kids); i++) {
                            const double* item = configs[c].sizes[i];
                            double w = is_x ? item[1] : item[0];
                            double h = is_x ? item[0] : item[1];

                            kids[i] = ui_frame(&(struct ui_frame) {
                                .parent = stack_root,
                                .size = ui_size_fix(w, h),
                            });
                        }

                        ui_layout(stack_root);

                        struct ui_test_list_fixture fixture = ui_test_list_fixture_(mem, 0, ui_test_list_stack_item_update, &state);
                        ui_test_list_append_(fixture.view, 5);

                        struct ui_list_linear linear = {
                            .axis = is_x ? ui_list_axis_x : ui_list_axis_y,
                            .size = size,
                            .padding = pad,
                            .spacing = make_double2(spacing, spacing),
                            .dist = (enum ui_dist)dist,
                            .align = (enum ui_align)align,
                        };

                        struct ui_test_list_frame frame = ui_test_list_step_(&fixture, &linear);

                        ok = ok && frame.item_desc_count == cr_count(kids);

                        for (uint32_t i = 0; i < cr_count(kids) && i < frame.item_desc_count; i++) {
                            double2 kid_origin = ui_get_origin_from_root(stack_root, kids[i]);
                            double2 kid_size = ui_get_size(kids[i]);
                            const struct ui_list_item_desc* desc = &frame.item_descs[i];

                            ok = ok && lround(kid_origin.x * scale) == lround(desc->pos.x * scale);
                            ok = ok && lround(kid_origin.y * scale) == lround(desc->pos.y * scale);
                            ok = ok && lround((kid_origin.x + kid_size.x) * scale) == lround((desc->pos.x + desc->size.x) * scale);
                            ok = ok && lround((kid_origin.y + kid_size.y) * scale) == lround((desc->pos.y + desc->size.y) * scale);
                        }

                        ui_test_list_fixture_destroy_(&fixture);
                        ui_release(stack_root);
                    }
                }
            }

            char label[64];
            snprintf(label, sizeof(label), "matches stack layout at %.0fx %s", scale, configs[c].label);
            ui_test_check(ok, label);
        }
    }

    ui_set_to_px_from_pt(saved_scale);
}

static bool ui_test_on_px_grid(double v, double to_px_from_pt) {
    double px = v * to_px_from_pt;
    return fabs(px - round(px)) < 1e-6;
}

static bool ui_test_list_descs_on_px_grid(const struct ui_test_list_frame* frame, double to_px_from_pt) {
    bool ok = true;

    for (uint32_t i = 0; i < frame->item_desc_count; i++) {
        const struct ui_list_item_desc* desc = &frame->item_descs[i];
        ok = ok && ui_test_on_px_grid(desc->pos.x, to_px_from_pt) && ui_test_on_px_grid(desc->pos.y, to_px_from_pt);
        ok = ok && ui_test_on_px_grid(desc->size.x, to_px_from_pt) && ui_test_on_px_grid(desc->size.y, to_px_from_pt);
    }

    return ok;
}

static void ui_test_list_pixel_grid(void) {
    struct cr_mem* mem = cr_mem_libc;

    double saved_scale = ui_get_to_px_from_pt();

    {
        ui_set_to_px_from_pt(3.0);

        struct ui_test_list_sizes sizes = {
            .base = make_double2(0.0, 27.3),
            .wide = make_double2(0.0, 61.7),
            .wide_index = uint32_max,
        };

        struct ui_test_list_fixture fixture = ui_test_list_make_(mem, &sizes);
        ui_test_list_append_(fixture.view, 6);

        struct ui_list_linear layout = {
            .axis = ui_list_axis_y,
            .size = make_double2(181.3, 400.4),
            .padding = make_double4(7.3, 6.1, 9.9, 4.2),
            .spacing = make_double2(0.0, 8.6),
            .dist = ui_dist_center,
        };

        struct ui_test_list_frame frame = ui_test_list_step_(&fixture, &layout);

        ui_test_check(frame.item_desc_count == 6, "pixel grid shows all rows");
        ui_test_check(ui_test_list_descs_on_px_grid(&frame, 3.0), "pixel grid snaps fractional layout");

        bool gaps_ok = true;
        for (uint32_t i = 0; i + 1 < frame.item_desc_count; i++) {
            double gap = frame.item_descs[i + 1].pos.y - (frame.item_descs[i].pos.y + frame.item_descs[i].size.y);
            gaps_ok = gaps_ok && fabs(gap * 3.0 - 26.0) < 1e-6;
        }
        ui_test_check(gaps_ok, "pixel grid uniform gaps");

        sizes.wide_index = 2;

        bool anim_ok = true;
        bool moved = false;
        double prev_pos = frame.item_descs[3].pos.y;

        for (uint32_t i = 0; i < 120; i++) {
            struct ui_test_list_frame step = ui_test_list_step_(&fixture, &layout);

            anim_ok = anim_ok && ui_test_list_descs_on_px_grid(&step, 3.0);
            moved = moved || step.item_descs[3].pos.y != prev_pos;
            prev_pos = step.item_descs[3].pos.y;
            frame = step;
        }

        ui_test_check(moved, "pixel grid animates reflow");
        ui_test_check(anim_ok, "pixel grid keeps animation on grid");
        ui_test_check(fabs(frame.item_descs[2].size.y * 3.0 - 185.0) < 1e-6, "pixel grid resize settles on grid");

        bool settled_gaps_ok = true;
        for (uint32_t i = 0; i + 1 < frame.item_desc_count; i++) {
            double gap = frame.item_descs[i + 1].pos.y - (frame.item_descs[i].pos.y + frame.item_descs[i].size.y);
            settled_gaps_ok = settled_gaps_ok && fabs(gap * 3.0 - 26.0) < 1e-6;
        }
        ui_test_check(settled_gaps_ok, "pixel grid uniform gaps after resize");

        ui_test_list_fixture_destroy_(&fixture);
    }

    {
        ui_set_to_px_from_pt(2.0);

        struct ui_test_list_sizes sizes = {
            .base = make_double2(41.3, 41.3),
            .wide_index = uint32_max,
        };

        struct ui_test_list_fixture fixture = ui_test_list_make_(mem, &sizes);
        ui_test_list_append_(fixture.view, 6);

        struct ui_list_linear layout = {
            .axis = ui_list_axis_x,
            .wrap = true,
            .size = make_double2(200.7, 300.2),
            .padding = make_double4(10.3, 10.3, 10.3, 10.3),
            .spacing = make_double2(6.7, 6.7),
            .dist = ui_dist_center,
        };

        struct ui_test_list_frame frame = ui_test_list_step_(&fixture, &layout);

        ui_test_check(frame.item_desc_count == 6, "pixel grid wrap shows all items");
        ui_test_check(ui_test_list_descs_on_px_grid(&frame, 2.0), "pixel grid wrap snaps fractional layout");
        ui_test_check(frame.item_descs[0].size.x == 41.5 && frame.item_descs[0].size.y == 41.5, "pixel grid wrap item size");
        ui_test_check(frame.item_descs[0].pos.x == 31.5 && frame.item_descs[0].pos.y == 10.5, "pixel grid wrap centers row");
        ui_test_check(frame.item_descs[1].pos.x == 79.5 && frame.item_descs[2].pos.x == 127.5, "pixel grid wrap uniform columns");
        ui_test_check(frame.item_descs[3].pos.x == 31.5 && frame.item_descs[3].pos.y == 58.5, "pixel grid wrap aligns rows");

        ui_test_list_fixture_destroy_(&fixture);
    }

    ui_set_to_px_from_pt(saved_scale);
}

struct ui_test_list_ladder {
    double step_x;
    double row_h;
};

static double2 ui_test_list_ladder_place_(struct ui_list_layout_item* items, uint32_t count, bool rebase, void* user) {
    const struct ui_test_list_ladder* ladder = user;
    double2 extent = make_double2(0.0, 0.0);

    for (uint32_t i = 0; i < count; i++) {
        items[i].offset = make_double2(ladder->step_x, ladder->row_h) * (double)items[i].index;
        extent = max_double2(extent, items[i].offset + items[i].size);
    }

    return extent;
}

static void ui_test_list_custom_layout(void) {
    struct cr_mem* mem = cr_mem_libc;

    struct ui_test_list_sizes sizes = { .base = make_double2(50.0, 20.0), .wide_index = uint32_max };

    struct ui_test_list_fixture fixture = ui_test_list_make_(mem, &sizes);
    ui_test_list_append_(fixture.view, 4);

    double2 bounds = make_double2(300.0, 300.0);
    struct ui_test_list_ladder ladder = { .step_x = 30.0, .row_h = 25.0 };

    struct ui_list_layout layout = {
        .place = ui_test_list_ladder_place_,
        .user = &ladder,
        .scroll_axis = ui_list_axis_y,
        .size = bounds,
        .item_available = bounds,
    };

    struct ui_test_list_frame frame = ui_test_list_layout_step_(&fixture, layout, false);

    ui_test_check(frame.item_desc_count == 4 && frame.item_descs[3].pos.x == 90.0 && frame.item_descs[3].pos.y == 75.0, "custom layout places items");
    ui_test_check(frame.item_descs[0].size.x == 50.0 && frame.item_descs[0].size.y == 20.0, "custom layout keeps reported sizes");

    ladder.step_x = 60.0;

    frame = ui_test_list_layout_step_(&fixture, layout, false);

    ui_test_check(frame.item_descs[3].pos.x > 90.0 && frame.item_descs[3].pos.x < 180.0 && frame.item_descs[3].pos.y == 75.0, "custom layout state animates per frame");

    for (uint32_t i = 0; i < 300; i++) {
        frame = ui_test_list_layout_step_(&fixture, layout, false);
    }

    ui_test_check(frame.item_descs[3].pos.x == 180.0, "custom layout settles");

    struct ui_list_linear linear = ui_test_list_column_layout_(300.0, 300.0);

    frame = ui_test_list_step_(&fixture, &linear);

    ui_test_check(frame.item_descs[3].pos.x < 180.0 && frame.item_descs[3].pos.y < 75.0, "custom layout swaps to linear animated");

    frame = ui_test_list_settle_(&fixture, &linear, 300);

    ui_test_check(frame.item_descs[3].pos.x == 0.0 && frame.item_descs[3].pos.y == 60.0 && frame.item_descs[3].size.x == 50.0, "custom layout swap settles to linear");

    ui_test_list_fixture_destroy_(&fixture);
}

static void ui_test_list_item_sizes(void) {
    struct cr_mem* mem = cr_mem_libc;

    struct ui_test_list_sizes sizes = {
        .base = make_double2(50.0, 50.0),
        .wide = make_double2(105.0, 50.0),
        .wide_index = uint32_max,
    };

    struct ui_test_list_fixture fixture = ui_test_list_make_(mem, &sizes);
    ui_test_list_append_(fixture.view, 5);

    struct ui_list_linear layout = ui_test_list_row_layout_(true, 200.0, 300.0);
    layout.dist = ui_dist_center;

    struct ui_test_list_frame frame = ui_test_list_settle_(&fixture, &layout, 100);

    ui_test_check(frame.item_desc_count == 5 && frame.item_descs[0].size.x == 50.0, "sized items default to layout size");
    ui_test_check(frame.item_descs[1].pos.x == 75.0, "sized items start uniform");

    sizes.wide_index = 0;

    frame = ui_test_list_step_(&fixture, &layout);

    ui_test_check(frame.item_descs[0].size.x == 105.0, "item size applies instantly");

    frame = ui_test_list_settle_(&fixture, &layout, 300);

    ui_test_check(frame.item_descs[0].size.x == 105.0 && frame.item_descs[0].size.y == 50.0, "item size settles");
    ui_test_check(frame.item_descs[0].pos.x == 20.0 && frame.item_descs[1].pos.x == 130.0, "sized row reflows");
    ui_test_check(frame.item_descs[2].pos.x == 20.0 && frame.item_descs[2].pos.y == 65.0, "sized overflow wraps");

    ui_test_list_fixture_destroy_(&fixture);
}

static void ui_test_list_resize(void) {
    struct cr_mem* mem = cr_mem_libc;

    struct ui_test_list_sizes sizes = { .base = make_double2(0.0, 20.0), .wide_index = uint32_max };

    struct ui_test_list_fixture fixture = ui_test_list_make_(mem, &sizes);
    ui_test_list_append_(fixture.view, 4);

    struct ui_list_linear layout = ui_test_list_column_layout_(200.0, 100.0);

    struct ui_test_list_frame frame = ui_test_list_settle_(&fixture, &layout, 10);

    ui_test_check(frame.item_desc_count == 4 && frame.item_descs[0].size.x == 200.0, "resize settles at initial width");

    layout = ui_test_list_column_layout_(320.0, 160.0);

    frame = ui_test_list_snap_step_(&fixture, &layout);

    ui_test_check(frame.item_descs[0].size.x == 320.0 && frame.item_descs[3].size.x == 320.0, "resize snaps item widths");
    ui_test_check(frame.item_descs[3].pos.y == 60.0, "resize snap keeps rows packed");

    layout = ui_test_list_column_layout_(200.0, 160.0);

    frame = ui_test_list_step_(&fixture, &layout);

    ui_test_check(frame.item_descs[0].size.x == 200.0, "resize tracks width instantly");

    ui_test_list_settle_(&fixture, &layout, 300);

    layout = ui_test_list_column_layout_(200.0, 260.0);

    frame = ui_test_list_step_(&fixture, &layout);

    ui_test_check(frame.item_descs[0].size.x == 200.0 && frame.item_descs[0].pos.y == 0.0, "resize scroll extent leaves items settled");

    ui_test_list_fixture_destroy_(&fixture);
}

static void ui_test_list_resize_packs(void) {
    struct cr_mem* mem = cr_mem_libc;

    struct ui_test_list_sizes sizes = {
        .base = make_double2(50.0, 50.0),
        .wide = make_double2(105.0, 50.0),
        .wide_index = uint32_max,
    };

    struct ui_test_list_fixture fixture = ui_test_list_make_(mem, &sizes);
    ui_test_list_append_(fixture.view, 5);

    struct ui_list_linear layout = ui_test_list_row_layout_(false, 400.0, 70.0);

    struct ui_test_list_frame frame = ui_test_list_settle_(&fixture, &layout, 100);

    ui_test_check(frame.item_desc_count == 5 && frame.item_descs[3].pos.x == 175.0, "resize starts uniform");

    sizes.wide_index = 3;

    frame = ui_test_list_step_(&fixture, &layout);

    ui_test_check(frame.item_descs[3].size.x == 105.0, "resize applies size instantly");
    ui_test_check(frame.item_descs[4].pos.x == 285.0, "resize packs followers instantly");
    ui_test_check(frame.item_descs[0].pos.x == 10.0 && frame.item_descs[2].pos.x == 120.0, "resize leaves predecessors");

    sizes.wide_index = 0;

    frame = ui_test_list_step_(&fixture, &layout);

    ui_test_check(frame.item_descs[0].size.x == 105.0 && frame.item_descs[3].size.x == 50.0, "resize repacks on change");
    ui_test_check(frame.item_descs[1].pos.x == 120.0 && frame.item_descs[3].pos.x == 230.0 && frame.item_descs[4].pos.x == 285.0, "resize repack shifts followers instantly");

    sizes.wide_index = uint32_max;

    frame = ui_test_list_step_(&fixture, &layout);

    ui_test_check(frame.item_descs[0].size.x == 50.0 && frame.item_descs[1].pos.x == 65.0 && frame.item_descs[4].pos.x == 230.0, "resize reverts packed");

    ui_test_list_delete_(fixture.view, 0, false);

    frame = ui_test_list_step_(&fixture, &layout);

    ui_test_check(frame.item_desc_count == 4, "delete applies instantly");
    ui_test_check(frame.item_descs[0].pos.x == 10.0 && frame.item_descs[3].pos.x == 175.0, "delete packs instantly");

    frame = ui_test_list_settle_(&fixture, &layout, 100);

    ui_test_check(frame.item_descs[0].pos.x == 10.0 && frame.item_descs[3].pos.x == 175.0, "delete stays settled");

    ui_test_list_fixture_destroy_(&fixture);
}

static void ui_test_list_driven_resize(void) {
    struct cr_mem* mem = cr_mem_libc;

    struct ui_test_list_sizes sizes = {
        .base = make_double2(50.0, 50.0),
        .wide = make_double2(50.0, 50.0),
        .wide_index = 0,
    };

    struct ui_test_list_fixture fixture = ui_test_list_make_(mem, &sizes);
    ui_test_list_append_(fixture.view, 3);

    struct ui_list_linear layout = ui_test_list_row_layout_(false, 400.0, 70.0);

    struct ui_test_list_frame frame = ui_test_list_step_(&fixture, &layout);

    ui_test_check(frame.item_descs[1].pos.x == 65.0 && frame.item_descs[2].pos.x == 120.0, "driven resize starts uniform");

    bool packed = true;

    for (uint32_t k = 1; k <= 11; k++) {
        sizes.wide.x = 50.0 + 5.0 * (double)k;

        frame = ui_test_list_step_(&fixture, &layout);

        packed = packed && frame.item_descs[0].size.x == sizes.wide.x;
        packed = packed && frame.item_descs[1].pos.x == 10.0 + sizes.wide.x + 5.0;
        packed = packed && frame.item_descs[2].pos.x == frame.item_descs[1].pos.x + 55.0;
    }

    ui_test_check(packed, "driven resize stays packed each frame");
    ui_test_check(frame.item_descs[1].pos.x == 120.0 && frame.item_descs[2].pos.x == 175.0, "driven resize lands settled");

    ui_test_list_fixture_destroy_(&fixture);
}

static void ui_test_list_driven_rewrap(void) {
    struct cr_mem* mem = cr_mem_libc;

    struct ui_test_list_sizes sizes = {
        .base = make_double2(90.0, 50.0),
        .wide = make_double2(50.0, 50.0),
        .wide_index = 0,
    };

    struct ui_test_list_fixture fixture = ui_test_list_make_(mem, &sizes);
    ui_test_list_append_(fixture.view, 4);

    struct ui_list_linear layout = ui_test_list_row_layout_(true, 400.0, 200.0);

    struct ui_test_list_frame frame = ui_test_list_step_(&fixture, &layout);

    ui_test_check(frame.item_desc_count == 4 && frame.item_descs[3].pos.x == 255.0 && frame.item_descs[3].pos.y == 10.0, "driven rewrap starts on one line");

    bool packed = true;

    for (uint32_t k = 1; k <= 9; k++) {
        sizes.wide.x = 50.0 + 5.0 * (double)k;

        frame = ui_test_list_step_(&fixture, &layout);

        packed = packed && frame.item_descs[1].pos.x == 15.0 + sizes.wide.x;
        packed = packed && frame.item_descs[3].pos.x == 205.0 + sizes.wide.x && frame.item_descs[3].pos.y == 10.0;
    }

    ui_test_check(packed, "driven rewrap stays packed while fitting");

    sizes.wide.x = 100.0;

    frame = ui_test_list_step_(&fixture, &layout);

    ui_test_check(frame.item_descs[0].size.x == 100.0, "driven rewrap applies size at the flip");
    ui_test_check(frame.item_descs[1].pos.x == 115.0 && frame.item_descs[2].pos.x == 210.0, "driven rewrap keeps line mates packed at the flip");
    ui_test_check(frame.item_descs[3].pos.x > 10.0 && frame.item_descs[3].pos.x < 305.0 && frame.item_descs[3].pos.y > 10.0 && frame.item_descs[3].pos.y < 65.0, "driven rewrap animates the falling item");

    frame = ui_test_list_settle_(&fixture, &layout, 300);

    ui_test_check(frame.item_descs[1].pos.x == 115.0 && frame.item_descs[2].pos.x == 210.0, "driven rewrap settles followers");
    ui_test_check(frame.item_descs[3].pos.x == 10.0 && frame.item_descs[3].pos.y == 65.0, "driven rewrap settles the falling item");

    ui_test_list_fixture_destroy_(&fixture);
}

static void ui_test_list_ghost_resize(void) {
    struct cr_mem* mem = cr_mem_libc;

    struct ui_test_list_sizes sizes = {
        .base = make_double2(50.0, 50.0),
        .wide = make_double2(105.0, 50.0),
        .wide_index = uint32_max,
    };

    struct ui_test_list_fixture fixture = ui_test_list_make_(mem, &sizes);
    ui_test_list_append_(fixture.view, 3);

    struct ui_list_linear layout = ui_test_list_row_layout_(false, 400.0, 70.0);

    ui_test_list_settle_(&fixture, &layout, 100);

    ui_test_list_delete_(fixture.view, 1, true);

    struct ui_test_list_frame frame = ui_test_list_step_(&fixture, &layout);

    ui_test_check(frame.item_desc_count == 3 && frame.item_descs[1].is_removed && frame.item_descs[1].index == uint32_max, "ghost resize starts fade");
    ui_test_check(frame.item_descs[2].index == 1, "ghost resize keeps live indices");

    struct ui_list_item_desc target;

    ui_test_check(ui_list_get_item_at_position(fixture.view, frame.item_descs[1].pos + frame.item_descs[1].size * 0.5, &target) == false, "ghost resize hit test skips ghosts");

    sizes.wide_index = 0;

    frame = ui_test_list_step_(&fixture, &layout);

    ui_test_check(frame.item_descs[0].size.x == 105.0, "ghost resize applies size");
    ui_test_check(frame.item_descs[1].is_removed && frame.item_descs[1].pos.x == 65.0, "ghost resize leaves ghost in place");
    ui_test_check(frame.item_descs[2].pos.x > 120.0 && frame.item_descs[2].pos.x < 175.0, "ghost resize shifts follower by resize only");

    frame = ui_test_list_settle_(&fixture, &layout, 300);

    ui_test_check(frame.item_desc_count == 2 && frame.item_descs[1].pos.x == 120.0, "ghost resize settles");

    sizes.wide_index = uint32_max;

    frame = ui_test_list_settle_(&fixture, &layout, 300);

    ui_test_check(frame.item_descs[0].size.x == 50.0 && frame.item_descs[1].pos.x == 65.0, "resize revert repacks");

    ui_test_list_add_(fixture.view, 1, false);
    ui_test_list_delete_(fixture.view, 1, true);

    frame = ui_test_list_step_(&fixture, &layout);

    ui_test_check(frame.item_desc_count == 2 && frame.item_descs[1].pos.x == 65.0, "unborn delete leaves layout untouched");

    frame = ui_test_list_settle_(&fixture, &layout, 300);

    sizes.wide_index = 0;

    ui_test_list_add_(fixture.view, 0, true);

    frame = ui_test_list_step_(&fixture, &layout);

    ui_test_check(frame.item_desc_count == 3 && frame.item_descs[0].size.x == 105.0 && frame.item_descs[0].opacity < 1.0, "birth applies size immediately");
    ui_test_check(frame.item_descs[1].pos.x > 10.0 && frame.item_descs[1].pos.x < 120.0, "birth parts followers animated");

    ui_test_list_fixture_destroy_(&fixture);
}

static void ui_test_list_insert_remove_animated(void) {
    struct cr_mem* mem = cr_mem_libc;

    struct ui_test_list_sizes sizes = { .base = make_double2(50.0, 50.0), .wide_index = uint32_max };

    struct ui_test_list_fixture fixture = ui_test_list_make_(mem, &sizes);
    ui_test_list_append_(fixture.view, 5);

    struct ui_list_linear layout = ui_test_list_row_layout_(false, 400.0, 70.0);

    ui_test_list_settle_(&fixture, &layout, 100);

    ui_test_list_add_(fixture.view, 2, true);

    struct ui_test_list_frame frame = ui_test_list_step_(&fixture, &layout);

    ui_test_check(frame.item_desc_count == 6 && frame.item_descs[2].index == 2 && frame.item_descs[2].size.x == 50.0, "animated insert appears at full size");
    ui_test_check(frame.item_descs[3].index == 3 && frame.item_descs[3].pos.x > 120.0 && frame.item_descs[3].pos.x < 175.0, "animated insert parts followers");

    frame = ui_test_list_step_(&fixture, &layout);

    ui_test_check(frame.item_descs[2].index == 2 && frame.item_descs[2].pos.x == 120.0, "animated insert holds slot");
    ui_test_check(frame.item_descs[2].opacity < 1.0, "animated insert fades in");

    frame = ui_test_list_settle_(&fixture, &layout, 300);

    ui_test_check(frame.item_desc_count == 6 && frame.item_descs[2].size.x == 50.0 && frame.item_descs[3].pos.x == 175.0, "animated insert settles");

    ui_test_list_delete_(fixture.view, 2, true);

    frame = ui_test_list_step_(&fixture, &layout);

    ui_test_check(frame.item_desc_count == 6 && frame.item_descs[2].is_removed, "animated remove keeps ghost");
    ui_test_check(frame.item_descs[2].size.x == 50.0 && frame.item_descs[2].opacity < 1.0, "animated remove fades at size");
    ui_test_check(frame.item_descs[3].index == 2 && frame.item_descs[3].pos.x < 175.0, "animated remove pulls followers");

    frame = ui_test_list_settle_(&fixture, &layout, 300);

    ui_test_check(frame.item_desc_count == 5 && frame.item_descs[2].index == 2 && frame.item_descs[2].pos.x == 120.0, "animated remove settles");

    ui_test_list_fixture_destroy_(&fixture);
}

static void ui_test_list_reorder_item_update(uint32_t index, void* content, double2 available_size, struct ui_list_on_item_update_result* out_result, void* user) {
    uint32_t* next_id = user;
    uint32_t* id = content;

    if (*id == 0) {
        *id = (*next_id)++;
    }

    out_result->size.y = 20.0;
}

struct ui_test_list_drag_ctx {
    struct ui_list* view;
    struct ui_list_linear linear;
    struct ui_test_list_sizes sizes;
    uint32_t next_id;
    struct ui_list_drag last;
    uint32_t began_count;
    uint32_t changed_count;
    uint32_t ended_count;
    uint32_t canceled_count;
    bool has_src;
    uint32_t src_index;
    void* src_content;
    bool has_hit;
    struct ui_list_item_desc hit;
    bool has_insert;
    struct ui_list_item_desc target;
    struct ui_list_linear_insert insert;
    double2 probe;
    bool probe_hit;
    bool move_on_end;
    bool moved;
    bool has_after_move;
    struct ui_list_item_desc after_move;
    uint32_t button_taps;
};

static struct ui_test_list_drag_ctx ui_test_list_drag_ctx_(struct ui_list_linear linear) {
    return (struct ui_test_list_drag_ctx) {
        .linear = linear,
        .sizes = { .base = make_double2(0.0, 20.0), .wide_index = uint32_max },
        .next_id = 1,
        .src_index = uint32_max,
        .probe = make_double2(-1.0, -1.0),
    };
}

static void ui_test_list_drag_ctx_sized_update_(uint32_t index, void* content, double2 available_size, struct ui_list_on_item_update_result* out_result, void* user) {
    struct ui_test_list_drag_ctx* ctx = user;

    ui_test_list_sized_update_(index, content, available_size, out_result, &ctx->sizes);
}

static void ui_test_list_drag_ctx_reorder_update_(uint32_t index, void* content, double2 available_size, struct ui_list_on_item_update_result* out_result, void* user) {
    struct ui_test_list_drag_ctx* ctx = user;

    ui_test_list_reorder_item_update(index, content, available_size, out_result, &ctx->next_id);
}

static uint32_t ui_test_list_find_content_(struct ui_list* view, const void* content) {
    struct ui_list_iter iter = ui_list_iter(view);
    struct ui_list_item_desc desc;

    while (ui_list_iter_next(&iter, &desc)) {
        if (desc.content == content && desc.is_removed == false) {
            return desc.index;
        }
    }

    return uint32_max;
}

static void ui_test_list_on_drag_(struct ui_list_drag drag, void* user) {
    struct ui_test_list_drag_ctx* ctx = user;

    ctx->last = drag;

    switch (drag.phase) {
    case ui_list_drag_phase_began:
        ctx->began_count++;
        ctx->has_src = ui_list_get_item_at_position(ctx->view, drag.position, &ctx->target);
        ctx->src_index = ctx->has_src ? ctx->target.index : uint32_max;
        ctx->src_content = ctx->has_src ? ctx->target.content : NULL;
        break;
    case ui_list_drag_phase_changed:
        ctx->changed_count++;
        break;
    case ui_list_drag_phase_ended:
        ctx->ended_count++;
        break;
    case ui_list_drag_phase_canceled:
        ctx->canceled_count++;
        return;
    }

    ctx->has_hit = ui_list_get_item_at_position(ctx->view, drag.position, &ctx->hit);
    ctx->has_insert = ui_list_get_item_near_position(ctx->view, drag.position, &ctx->target);

    if (ctx->has_insert) {
        ctx->insert = ui_list_linear_insert(&ctx->linear, &ctx->target, drag.position);
    }

    if (ctx->probe.x >= 0.0) {
        struct ui_list_item_desc probe_desc;
        ctx->probe_hit = ui_list_get_item_at_position(ctx->view, ctx->probe, &probe_desc);
    }

    if (drag.phase == ui_list_drag_phase_ended && ctx->move_on_end && ctx->has_src && ctx->has_insert) {
        uint32_t src = ctx->src_content ? ui_test_list_find_content_(ctx->view, ctx->src_content) : ctx->src_index;
        uint32_t dst = ctx->insert.index > src ? ctx->insert.index - 1 : ctx->insert.index;

        if (src != uint32_max && dst != src) {
            ui_test_list_move_(ctx->view, src, dst);
            ctx->moved = true;
            ctx->has_after_move = ui_list_get_item_at_position(ctx->view, drag.position, &ctx->after_move);
        }
    }
}

static struct ui_test_list_fixture ui_test_list_drag_fixture_(struct cr_mem* mem, uint64_t content_size, ui_test_list_on_item_update_t* on_item_update, struct ui_test_list_drag_ctx* ctx) {
    struct ui_test_list_fixture fixture = ui_test_list_fixture_(mem, content_size, on_item_update, ctx);

    fixture.on_drag = ui_test_list_on_drag_;
    ctx->view = fixture.view;

    return fixture;
}

static struct app_event ui_test_list_touch_(enum app_pointer_event_type type, double x, double y, double timestamp, uint64_t id) {
    struct app_event event = ui_test_direct_touch_event_(type, x, y, timestamp);

    event.pointer.id = id;

    return event;
}

static void ui_test_list_button_delete_(struct ui_tap_gesture gesture, void* user) {
    struct ui_test_list_drag_ctx* ctx = user;

    ui_test_list_delete_(ctx->view, 0, true);
    ctx->button_taps++;
}

static struct ui_list_linear ui_test_list_input_linear_(void) {
    return ui_test_list_column_layout_(200.0, 100.0);
}

static void ui_test_list_input_step_ex_(struct ui_node* prev_root, struct ui_test_list_fixture* fixture, struct ui_test_list_drag_ctx* button_ctx, double timestamp, const struct app_event* events, uint32_t event_count) {
    ui_input_dispatch(*prev_root, timestamp, events, event_count, 1.0);

    struct ui_node root = ui_frame(&(struct ui_frame) {
        .size = ui_size_fix(200, 300),
    });

    struct ui_list_linear linear = ui_test_list_input_linear_();

    struct ui_node host = ui_frame(&(struct ui_frame) {
        .parent = root,
        .size = ui_size_fix(linear.size.x, linear.size.y),
        .clip = true,
    });

    ui_test_list_update_(fixture, host, ui_list_linear_layout(&linear), false);

    if (button_ctx) {
        struct ui_node button = ui_frame(&(struct ui_frame) {
            .parent = root,
            .pos = ui_pos_xy(0, 200),
            .size = ui_size_fix(200, 50),
        });

        ui_tap(&(struct ui_tap) {
            .node = button,
            .on_change = ui_test_list_button_delete_,
            .user = button_ctx,
        });
    }

    ui_layout(root);

    if (ui_node_is_null(*prev_root) == false) {
        ui_release(*prev_root);
    }
    *prev_root = root;
}

static void ui_test_list_input_step_(struct ui_node* prev_root, struct ui_test_list_fixture* fixture, double timestamp, const struct app_event* events, uint32_t event_count) {
    ui_test_list_input_step_ex_(prev_root, fixture, NULL, timestamp, events, event_count);
}

static void ui_test_list_input_idle_(struct ui_node* prev_root, struct ui_test_list_fixture* fixture, double* timestamp, uint32_t frames, double step_s) {
    for (uint32_t i = 0; i < frames; i++) {
        *timestamp += step_s;
        ui_test_list_input_step_(prev_root, fixture, *timestamp, NULL, 0);
    }
}

static void ui_test_list_input_press_(struct ui_node* prev_root, struct ui_test_list_fixture* fixture, double* timestamp, double x, double y) {
    struct app_event press = ui_test_direct_touch_event_(app_pointer_event_type_touch_began, x, y, *timestamp);

    ui_test_list_input_step_(prev_root, fixture, *timestamp, &press, 1);
    ui_test_list_input_idle_(prev_root, fixture, timestamp, 10, 0.1);
}

static void ui_test_list_input_release_(struct ui_node* prev_root, struct ui_test_list_fixture* fixture, double* timestamp, double x, double y) {
    *timestamp += 0.016;

    struct app_event release = ui_test_direct_touch_event_(app_pointer_event_type_touch_ended, x, y, *timestamp);

    ui_test_list_input_step_(prev_root, fixture, *timestamp, &release, 1);
}

static void ui_test_list_input_move_(struct ui_node* prev_root, struct ui_test_list_fixture* fixture, double* timestamp, double x, double y) {
    *timestamp += 0.016;

    struct app_event move = ui_test_direct_touch_event_(app_pointer_event_type_touch_moved, x, y, *timestamp);

    ui_test_list_input_step_(prev_root, fixture, *timestamp, &move, 1);
}

static bool ui_test_list_ids_match_(const struct ui_test_list_frame* frame, const uint32_t* expected, uint32_t count) {
    bool ok = frame->item_desc_count == count;

    for (uint32_t i = 0; i < count && ok; i++) {
        uint32_t id = *(uint32_t*)frame->item_descs[i].content;
        ok = frame->item_descs[i].is_removed == false && id == expected[i] && frame->item_descs[i].pos.y == (double)i * 20.0;
    }

    return ok;
}

static void ui_test_list_drag_targets(void) {
    struct cr_mem* mem = cr_mem_libc;

    struct ui_test_list_drag_ctx ctx = ui_test_list_drag_ctx_(ui_test_list_input_linear_());
    struct ui_test_list_fixture fixture = ui_test_list_drag_fixture_(mem, 0, ui_test_list_drag_ctx_sized_update_, &ctx);
    ui_test_list_append_(fixture.view, 4);

    struct ui_node prev_root = ui_node_null;
    double timestamp = 1.0;

    ui_test_list_input_idle_(&prev_root, &fixture, &timestamp, 40, 1.0 / 60.0);

    ui_test_check(ctx.began_count == 0, "drag idle");

    ui_test_list_input_press_(&prev_root, &fixture, &timestamp, 100.0, 30.0);

    ui_test_check(ctx.began_count == 1 && ctx.last.phase == ui_list_drag_phase_began, "drag begins on long press");
    ui_test_check(ctx.last.position.x == 100.0 && ctx.last.position.y == 30.0, "drag reports position in view space");
    ui_test_check(ctx.has_src && ctx.src_index == 1, "drag targets pressed row");
    ui_test_check(ctx.has_insert && ctx.insert.index == 2 && ctx.insert.cursor_pos.y == 40.0, "drag insert after pressed row center");

    ui_test_list_input_release_(&prev_root, &fixture, &timestamp, 100.0, 30.0);

    ui_test_check(ctx.ended_count == 1 && ctx.last.phase == ui_list_drag_phase_ended, "drag ends on release");

    ui_test_list_input_idle_(&prev_root, &fixture, &timestamp, 2, 0.016);

    ui_test_check(ctx.began_count == 1 && ctx.changed_count == 0 && ctx.ended_count == 1 && ctx.canceled_count == 0, "drag settles after release");

    timestamp += 1.0;

    ui_test_list_input_press_(&prev_root, &fixture, &timestamp, 100.0, 90.0);

    ui_test_check(ctx.began_count == 2 && ctx.has_src == false, "drag outside rows has no row under the finger");
    ui_test_check(ctx.has_insert && ctx.target.index == 3 && ctx.insert.index == 4 && ctx.insert.cursor_pos.y == 80.0, "drag outside rows inserts after the nearest row");

    ui_test_list_input_release_(&prev_root, &fixture, &timestamp, 100.0, 90.0);

    ui_test_check(ctx.ended_count == 2 && ctx.last.phase == ui_list_drag_phase_ended, "drag outside rows ends on release");

    timestamp += 1.0;

    ui_test_list_input_press_(&prev_root, &fixture, &timestamp, 100.0, 75.0);

    ui_test_check(ctx.began_count == 3 && ctx.has_src && ctx.src_index == 3, "drag targets last row");
    ui_test_check(ctx.insert.index == 4 && ctx.insert.cursor_pos.y == 80.0, "drag lower half inserts after last row");

    ui_test_list_input_release_(&prev_root, &fixture, &timestamp, 100.0, 75.0);

    ui_test_check(ctx.ended_count == 3, "drag last row ends on release");

    ui_release(prev_root);
    ui_test_list_fixture_destroy_(&fixture);
}

static void ui_test_list_drag_dead_node(void) {
    struct cr_mem* mem = cr_mem_libc;

    struct ui_test_list_drag_ctx ctx = ui_test_list_drag_ctx_(ui_test_list_input_linear_());
    struct ui_test_list_fixture fixture = ui_test_list_drag_fixture_(mem, 0, ui_test_list_drag_ctx_sized_update_, &ctx);
    ui_test_list_append_(fixture.view, 4);

    struct ui_node prev_root = ui_node_null;
    double timestamp = 1.0;

    ui_test_list_input_idle_(&prev_root, &fixture, &timestamp, 40, 1.0 / 60.0);
    ui_test_list_input_press_(&prev_root, &fixture, &timestamp, 100.0, 30.0);

    ui_test_check(ctx.began_count == 1, "dead node drag begins");

    ui_release(prev_root);
    prev_root = ui_node_null;

    ui_test_list_input_idle_(&prev_root, &fixture, &timestamp, 1, 0.016);

    ui_test_check(ctx.canceled_count == 1 && ctx.last.phase == ui_list_drag_phase_canceled, "dead node cancels the drag");
    ui_test_check(ctx.last.position.x == 100.0 && ctx.last.position.y == 30.0, "dead node cancel keeps the last position");

    ui_test_list_input_idle_(&prev_root, &fixture, &timestamp, 1, 0.016);

    ui_test_check(ctx.canceled_count == 1 && ctx.began_count == 1, "dead node drag stays idle");

    ui_test_list_input_release_(&prev_root, &fixture, &timestamp, 100.0, 30.0);
    ui_test_list_input_idle_(&prev_root, &fixture, &timestamp, 1, 0.016);

    ui_test_check(ctx.ended_count == 0 && ctx.canceled_count == 1, "dead node release reports nothing");

    ui_release(prev_root);
    ui_test_list_fixture_destroy_(&fixture);
}

static void ui_test_list_reorder(void) {
    struct cr_mem* mem = cr_mem_libc;

    struct ui_test_list_drag_ctx ctx = ui_test_list_drag_ctx_(ui_test_list_input_linear_());
    ctx.move_on_end = true;

    struct ui_test_list_fixture fixture = ui_test_list_drag_fixture_(mem, sizeof(uint32_t), ui_test_list_drag_ctx_reorder_update_, &ctx);
    ui_test_list_append_(fixture.view, 4);

    struct ui_node prev_root = ui_node_null;
    double timestamp = 1.0;

    ui_test_list_input_idle_(&prev_root, &fixture, &timestamp, 40, 1.0 / 60.0);
    ui_test_list_input_press_(&prev_root, &fixture, &timestamp, 100.0, 30.0);

    ui_test_check(ctx.began_count == 1 && ctx.src_index == 1, "reorder picks up source row");

    ui_test_list_input_move_(&prev_root, &fixture, &timestamp, 100.0, 78.0);

    ui_test_check(ctx.changed_count >= 1 && ctx.last.phase == ui_list_drag_phase_changed, "reorder tracks movement");
    ui_test_check(ctx.last.position.y == 78.0 && ctx.has_insert && ctx.insert.index == 4, "reorder targets end slot");
    ui_test_check(ctx.insert.cursor_pos.y == 80.0 && ctx.insert.cursor_size.x == 200.0 && ctx.insert.cursor_size.y == 0.0, "reorder cursor below last row");

    ui_test_list_input_idle_(&prev_root, &fixture, &timestamp, 1, 0.016);
    ui_test_list_input_release_(&prev_root, &fixture, &timestamp, 100.0, 78.0);

    ui_test_check(ctx.ended_count == 1 && ctx.insert.index == 4 && ctx.moved, "reorder drop moves inside the callback");
    ui_test_check(ctx.has_after_move && ctx.after_move.index == 2 && *(uint32_t*)ctx.after_move.content == 4, "reorder query after edit sees new indices");

    ui_test_list_input_idle_(&prev_root, &fixture, &timestamp, 300, 1.0 / 60.0);

    struct ui_test_list_frame frame = ui_test_list_step_(&fixture, &ctx.linear);

    uint32_t expected[] = { 1, 3, 4, 2 };

    ui_test_check(ui_test_list_ids_match_(&frame, expected, cr_count(expected)), "reorder applies move");

    ui_release(prev_root);
    ui_test_list_fixture_destroy_(&fixture);
}

static void ui_test_list_drag_lost_end(void) {
    struct cr_mem* mem = cr_mem_libc;

    struct ui_test_list_drag_ctx ctx = ui_test_list_drag_ctx_(ui_test_list_input_linear_());
    ctx.move_on_end = true;

    struct ui_test_list_fixture fixture = ui_test_list_drag_fixture_(mem, sizeof(uint32_t), ui_test_list_drag_ctx_reorder_update_, &ctx);
    ui_test_list_append_(fixture.view, 4);

    struct ui_node prev_root = ui_node_null;
    double timestamp = 1.0;

    ui_test_list_input_idle_(&prev_root, &fixture, &timestamp, 40, 1.0 / 60.0);
    ui_test_list_input_press_(&prev_root, &fixture, &timestamp, 100.0, 30.0);

    ui_test_check(ctx.began_count == 1, "lost end first drag begins");

    ui_test_list_input_press_(&prev_root, &fixture, &timestamp, 100.0, 30.0);

    ui_test_check(ctx.canceled_count == 1 && ctx.began_count == 2 && ctx.src_index == 1, "lost end cancels the stale drag and begins again");

    ui_test_list_input_move_(&prev_root, &fixture, &timestamp, 100.0, 78.0);
    ui_test_list_input_release_(&prev_root, &fixture, &timestamp, 100.0, 78.0);

    ui_test_check(ctx.ended_count == 1 && ctx.insert.index == 4 && ctx.moved, "lost end reorder completes");

    ui_test_list_input_idle_(&prev_root, &fixture, &timestamp, 300, 1.0 / 60.0);

    struct ui_test_list_frame frame = ui_test_list_step_(&fixture, &ctx.linear);

    uint32_t expected[] = { 1, 3, 4, 2 };

    ui_test_check(ui_test_list_ids_match_(&frame, expected, cr_count(expected)), "lost end reorder applies move");

    ui_release(prev_root);
    ui_test_list_fixture_destroy_(&fixture);
}

static void ui_test_list_edit_in_callbacks(void) {
    struct cr_mem* mem = cr_mem_libc;

    struct ui_test_list_drag_ctx ctx = ui_test_list_drag_ctx_(ui_test_list_input_linear_());
    ctx.move_on_end = true;
    ctx.probe = make_double2(100.0, 10.0);

    struct ui_test_list_fixture fixture = ui_test_list_drag_fixture_(mem, sizeof(uint32_t), ui_test_list_drag_ctx_reorder_update_, &ctx);
    ui_test_list_append_(fixture.view, 4);

    struct ui_node prev_root = ui_node_null;
    double timestamp = 1.0;

    for (uint32_t i = 0; i < 40; i++) {
        timestamp += 1.0 / 60.0;
        ui_test_list_input_step_ex_(&prev_root, &fixture, &ctx, timestamp, NULL, 0);
    }

    struct app_event press = ui_test_list_touch_(app_pointer_event_type_touch_began, 100.0, 50.0, timestamp, 91);
    ui_test_list_input_step_ex_(&prev_root, &fixture, &ctx, timestamp, &press, 1);

    for (uint32_t i = 0; i < 10; i++) {
        timestamp += 0.1;
        ui_test_list_input_step_ex_(&prev_root, &fixture, &ctx, timestamp, NULL, 0);
    }

    ui_test_check(ctx.began_count == 1 && ctx.src_index == 2 && ctx.probe_hit, "callback edit drag begins on row 2");

    timestamp += 0.016;

    struct app_event batch[] = {
        ui_test_list_touch_(app_pointer_event_type_touch_began, 100.0, 225.0, timestamp, 92),
        ui_test_list_touch_(app_pointer_event_type_touch_ended, 100.0, 225.0, timestamp, 92),
        ui_test_list_touch_(app_pointer_event_type_touch_moved, 100.0, 75.0, timestamp, 91),
    };
    ui_test_list_input_step_ex_(&prev_root, &fixture, &ctx, timestamp, batch, cr_count(batch));

    ui_test_check(ctx.button_taps == 1 && ctx.changed_count >= 1, "button edit and drag change share a dispatch");
    ui_test_check(ctx.has_hit && ctx.hit.index == 2 && *(uint32_t*)ctx.hit.content == 4, "drag query after button delete sees shifted indices");
    ui_test_check(ctx.probe_hit == false, "drag query after button delete skips the ghost");

    timestamp += 0.016;

    struct app_event release = ui_test_list_touch_(app_pointer_event_type_touch_ended, 100.0, 75.0, timestamp, 91);
    ui_test_list_input_step_ex_(&prev_root, &fixture, &ctx, timestamp, &release, 1);

    ui_test_check(ctx.ended_count == 1 && ctx.moved && ctx.insert.index == 3, "drop after button delete moves the resolved source");

    for (uint32_t i = 0; i < 300; i++) {
        timestamp += 1.0 / 60.0;
        ui_test_list_input_step_ex_(&prev_root, &fixture, &ctx, timestamp, NULL, 0);
    }

    struct ui_test_list_frame frame = ui_test_list_step_(&fixture, &ctx.linear);

    uint32_t expected[] = { 2, 4, 3 };

    ui_test_check(ui_test_list_ids_match_(&frame, expected, cr_count(expected)), "callback edits settle in order");

    ui_release(prev_root);
    ui_test_list_fixture_destroy_(&fixture);
}

static void ui_test_list_queries_after_edit(void) {
    struct cr_mem* mem = cr_mem_libc;

    uint32_t next_id = 1;

    struct ui_test_list_fixture fixture = ui_test_list_fixture_(mem, sizeof(uint32_t), ui_test_list_reorder_item_update, &next_id);
    ui_test_list_append_(fixture.view, 4);

    struct ui_list_linear linear = ui_test_list_input_linear_();
    struct ui_list_item_desc desc;

    ui_test_check(ui_list_get_item_at_position(fixture.view, make_double2(100.0, 10.0), &desc) == false, "queries before the first frame miss");

    ui_test_list_settle_(&fixture, &linear, 2);

    ui_test_list_delete_(fixture.view, 0, true);

    struct ui_test_list_frame frame = ui_test_list_collect_(fixture.view);

    ui_test_check(frame.item_desc_count == 4 && frame.item_descs[0].is_removed && frame.item_descs[0].index == uint32_max, "animated delete turns the row into a ghost before update");
    ui_test_check(frame.item_descs[1].index == 0 && *(uint32_t*)frame.item_descs[1].content == 2 && frame.item_descs[1].pos.y == 20.0, "animated delete shifts indices and keeps positions before update");
    ui_test_check(ui_list_get_item_at_position(fixture.view, make_double2(100.0, 10.0), &desc) == false, "ghost misses hit test before update");
    ui_test_check(ui_list_get_item_at_position(fixture.view, make_double2(100.0, 30.0), &desc) && desc.index == 0, "hit test after delete uses shifted index");
    ui_test_check(ui_list_get_item_near_position(fixture.view, make_double2(100.0, 5.0), &desc) && desc.index == 0 && *(uint32_t*)desc.content == 2, "near query after delete skips the ghost");

    ui_test_list_add_(fixture.view, 0, false);

    frame = ui_test_list_collect_(fixture.view);

    ui_test_check(frame.item_desc_count == 4 && frame.item_descs[1].index == 1 && *(uint32_t*)frame.item_descs[1].content == 2, "unmeasured add shifts indices and stays out of the snapshot");
    ui_test_check(ui_list_get_item_at_position(fixture.view, make_double2(100.0, 30.0), &desc) && desc.index == 1, "hit test after add uses shifted index");

    ui_test_list_move_(fixture.view, 1, 3);

    ui_test_check(ui_list_get_item_at_position(fixture.view, make_double2(100.0, 30.0), &desc) && desc.index == 3 && *(uint32_t*)desc.content == 2, "moved row reports its new index at its old position");
    ui_test_check(ui_list_get_item_at_position(fixture.view, make_double2(100.0, 50.0), &desc) && desc.index == 1 && *(uint32_t*)desc.content == 3, "row behind the move reports its new index");

    frame = ui_test_list_settle_(&fixture, &linear, 300);

    uint32_t expected[] = { 5, 3, 4, 2 };

    ui_test_check(ui_test_list_ids_match_(&frame, expected, cr_count(expected)), "edits before update settle in order");

    ui_test_list_fixture_destroy_(&fixture);
}

static void ui_test_list_no_on_drag(void) {
    struct cr_mem* mem = cr_mem_libc;

    for (uint32_t pass = 0; pass < 2; pass++) {
        bool has_on_drag = pass == 1;

        struct ui_test_list_drag_ctx ctx = ui_test_list_drag_ctx_(ui_test_list_input_linear_());
        struct ui_test_list_fixture fixture = ui_test_list_drag_fixture_(mem, 0, ui_test_list_drag_ctx_sized_update_, &ctx);

        if (has_on_drag == false) {
            fixture.on_drag = NULL;
        }

        ui_test_list_append_(fixture.view, 20);

        struct ui_node prev_root = ui_node_null;
        double timestamp = 1.0;

        ui_test_list_input_idle_(&prev_root, &fixture, &timestamp, 10, 1.0 / 60.0);
        ui_test_list_input_press_(&prev_root, &fixture, &timestamp, 100.0, 80.0);

        for (uint32_t i = 1; i <= 10; i++) {
            ui_test_list_input_move_(&prev_root, &fixture, &timestamp, 100.0, 80.0 - 5.0 * (double)i);
        }

        ui_test_list_input_release_(&prev_root, &fixture, &timestamp, 100.0, 30.0);
        ui_test_list_input_idle_(&prev_root, &fixture, &timestamp, 30, 1.0 / 60.0);

        double offset = ui_list_get_scroll_offset(fixture.view);

        if (has_on_drag) {
            ui_test_check(ctx.began_count == 1 && ctx.ended_count == 1 && offset == 0.0, "long press with on_drag claims the pointer from scrolling");
        } else {
            ui_test_check(ctx.began_count == 0 && offset > 0.0, "long press without on_drag leaves scrolling free");
        }

        ui_release(prev_root);
        ui_test_list_fixture_destroy_(&fixture);
    }
}

static void ui_test_list_embedded_tap(struct ui_tap_gesture gesture, void* user) {
}

static struct ui_list_linear ui_test_list_embedded_linear_(void) {
    return (struct ui_list_linear) {
        .axis = ui_list_axis_y,
        .size = make_double2(200.0, 300.0),
        .padding = make_double4(10, 10, 10, 10),
    };
}

static void ui_test_list_embedded_step_(struct ui_node* prev_root, struct ui_test_list_fixture* fixture, double timestamp, const struct app_event* events, uint32_t event_count) {
    ui_input_dispatch(*prev_root, timestamp, events, event_count, 1.0);

    struct ui_node root = ui_frame(&(struct ui_frame) {
        .size = ui_size_fix(400, 400),
    });

    struct ui_list_linear linear = ui_test_list_embedded_linear_();

    struct ui_node host = ui_frame(&(struct ui_frame) {
        .parent = root,
        .pos = ui_pos_xy(40, 60),
        .size = ui_size_fix(linear.size.x, linear.size.y),
        .corners = ui_corners(16),
        .fill = ui_fill_solid(ui_gray_a(1.0f, 0.06f)),
        .clip = true,
    });

    ui_test_list_update_(fixture, host, ui_list_linear_layout(&linear), false);

    struct ui_list_iter iter = ui_list_iter(fixture->view);
    struct ui_list_item_desc desc;
    while (ui_list_iter_next(&iter, &desc)) {
        struct ui_node item = ui_frame(&(struct ui_frame) {
            .parent = host,
            .pos = ui_pos_xy(desc.pos.x, desc.pos.y),
            .size = ui_size_fix(desc.size.x, desc.size.y),
            .corners = ui_corners(6),
            .fill = ui_fill_solid(ui_white),
            .blend = { .mode = ui_blend_mode_normal, .opacity = desc.opacity },
        });

        if (desc.is_removed == false) {
            ui_tap(&(struct ui_tap) {
                .node = item,
                .on_change = ui_test_list_embedded_tap,
            });
        }
    }

    ui_layout(root);

    if (ui_node_is_null(*prev_root) == false) {
        ui_release(*prev_root);
    }
    *prev_root = root;
}

static void ui_test_list_reorder_embedded(void) {
    struct cr_mem* mem = cr_mem_libc;

    struct ui_test_list_drag_ctx ctx = ui_test_list_drag_ctx_(ui_test_list_embedded_linear_());
    ctx.move_on_end = true;

    struct ui_test_list_fixture fixture = ui_test_list_drag_fixture_(mem, sizeof(uint32_t), ui_test_list_drag_ctx_reorder_update_, &ctx);
    ui_test_list_append_(fixture.view, 4);

    struct ui_node prev_root = ui_node_null;
    double timestamp = 1.0;

    for (uint32_t i = 0; i < 40; i++) {
        ui_test_list_embedded_step_(&prev_root, &fixture, timestamp, NULL, 0);
        timestamp += 1.0 / 60.0;
    }

    struct app_event press = ui_test_direct_touch_event_(app_pointer_event_type_touch_began, 140.0, 100.0, timestamp);
    ui_test_list_embedded_step_(&prev_root, &fixture, timestamp, &press, 1);

    for (uint32_t i = 0; i < 10; i++) {
        timestamp += 0.1;
        ui_test_list_embedded_step_(&prev_root, &fixture, timestamp, NULL, 0);
    }

    ui_test_check(ctx.began_count == 1 && ctx.src_index == 1, "embedded reorder picks up source row");
    ui_test_check(ctx.last.position.x == 100.0 && ctx.last.position.y == 40.0, "embedded drag position is host local");

    timestamp += 0.016;
    struct app_event move_event = ui_test_direct_touch_event_(app_pointer_event_type_touch_moved, 140.0, 145.0, timestamp);
    ui_test_list_embedded_step_(&prev_root, &fixture, timestamp, &move_event, 1);

    ui_test_check(ctx.changed_count >= 1 && ctx.last.position.y == 85.0, "embedded reorder tracks movement");
    ui_test_check(ctx.has_insert && ctx.insert.index == 4, "embedded reorder targets end slot");
    ui_test_check(ctx.insert.cursor_pos.y == 90.0, "embedded reorder cursor below last row");

    timestamp += 0.016;
    struct app_event release = ui_test_direct_touch_event_(app_pointer_event_type_touch_ended, 140.0, 145.0, timestamp);
    ui_test_list_embedded_step_(&prev_root, &fixture, timestamp, &release, 1);

    ui_test_check(ctx.ended_count == 1 && ctx.insert.index == 4 && ctx.moved, "embedded reorder drop moves inside the callback");

    for (uint32_t i = 0; i < 300; i++) {
        timestamp += 1.0 / 60.0;
        ui_test_list_embedded_step_(&prev_root, &fixture, timestamp, NULL, 0);
    }

    struct ui_test_list_frame frame = ui_test_list_step_(&fixture, &ctx.linear);

    uint32_t expected[] = { 1, 3, 4, 2 };
    bool order_ok = frame.item_desc_count == 4;

    for (uint32_t i = 0; i < cr_count(expected) && order_ok; i++) {
        uint32_t id = *(uint32_t*)frame.item_descs[i].content;
        order_ok = id == expected[i];
    }

    ui_test_check(order_ok, "embedded reorder applies move");

    ui_release(prev_root);
    ui_test_list_fixture_destroy_(&fixture);
}

static struct app_event ui_test_list_wheel_event_(double delta_y, double timestamp) {
    return (struct app_event) {
        .type = app_event_type_scroll,
        .scroll = { .delta = make_double2(0.0, delta_y), .pos = make_double2(100.0, 50.0), .timestamp = timestamp },
    };
}

static void ui_test_list_wheel(void) {
    struct cr_mem* mem = cr_mem_libc;

    struct ui_test_list_sizes sizes = { .base = make_double2(0.0, 20.0), .wide_index = uint32_max };

    struct ui_test_list_fixture fixture = ui_test_list_make_(mem, &sizes);
    ui_test_list_append_(fixture.view, 4);

    struct ui_node prev_root = ui_node_null;
    double timestamp = 1.0;

    for (uint32_t i = 0; i < 10; i++) {
        ui_test_list_input_step_(&prev_root, &fixture, timestamp, NULL, 0);
        timestamp += 1.0 / 60.0;
    }

    bool still = true;

    for (uint32_t i = 0; i < 30; i++) {
        struct app_event wheel = ui_test_list_wheel_event_(10.0, timestamp);
        ui_test_list_input_step_(&prev_root, &fixture, timestamp, &wheel, 1);
        still = still && ui_list_get_scroll_offset(fixture.view) == 0.0;
        timestamp += 1.0 / 60.0;
    }

    ui_test_check(still, "wheel leaves fitting content still");

    struct app_event pan_press = ui_test_direct_touch_event_(app_pointer_event_type_touch_began, 100.0, 80.0, timestamp);
    ui_test_list_input_step_(&prev_root, &fixture, timestamp, &pan_press, 1);

    bool pan_still = true;

    for (uint32_t i = 0; i < 20; i++) {
        timestamp += 1.0 / 60.0;
        struct app_event pan_move = ui_test_direct_touch_event_(app_pointer_event_type_touch_moved, 100.0, 80.0 - 3.0 * (double)i, timestamp);
        ui_test_list_input_step_(&prev_root, &fixture, timestamp, &pan_move, 1);
        pan_still = pan_still && ui_list_get_scroll_offset(fixture.view) == 0.0;
    }

    timestamp += 1.0 / 60.0;
    struct app_event pan_release = ui_test_direct_touch_event_(app_pointer_event_type_touch_ended, 100.0, 23.0, timestamp);
    ui_test_list_input_step_(&prev_root, &fixture, timestamp, &pan_release, 1);

    for (uint32_t i = 0; i < 60; i++) {
        timestamp += 1.0 / 60.0;
        ui_test_list_input_step_(&prev_root, &fixture, timestamp, NULL, 0);
        pan_still = pan_still && ui_list_get_scroll_offset(fixture.view) == 0.0;
    }

    ui_test_check(pan_still, "pan leaves fitting content still");

    ui_test_list_append_(fixture.view, 20);

    for (uint32_t i = 0; i < 10; i++) {
        ui_test_list_input_step_(&prev_root, &fixture, timestamp, NULL, 0);
        timestamp += 1.0 / 60.0;
    }

    bool scrolled = false;

    for (uint32_t i = 0; i < 30; i++) {
        struct app_event wheel = ui_test_list_wheel_event_(10.0, timestamp);
        ui_test_list_input_step_(&prev_root, &fixture, timestamp, &wheel, 1);
        scrolled = scrolled || ui_list_get_scroll_offset(fixture.view) != 0.0;
        timestamp += 1.0 / 60.0;
    }

    ui_test_check(scrolled, "wheel scrolls overflowing content");

    for (uint32_t i = 0; i < 120; i++) {
        timestamp += 1.0 / 60.0;
        ui_test_list_input_step_(&prev_root, &fixture, timestamp, NULL, 0);
    }

    double scroll_offset = ui_list_get_scroll_offset(fixture.view);
    struct ui_test_list_frame frame = ui_test_list_collect_(fixture.view);

    bool scrolled_descs_ok = scroll_offset > 0.0 && frame.item_desc_count > 0 && frame.item_descs[0].index > 0;

    for (uint32_t i = 0; i < frame.item_desc_count; i++) {
        double expected = (double)frame.item_descs[i].index * 20.0 - scroll_offset;
        scrolled_descs_ok = scrolled_descs_ok && fabs(frame.item_descs[i].pos.y - expected) < 1e-9;
    }

    ui_test_check(scrolled_descs_ok, "wheel offsets item positions by scroll");

    struct ui_list_item_desc target;

    ui_test_check(ui_list_get_item_at_position(fixture.view, frame.item_descs[1].pos + make_double2(100.0, 10.0), &target) && target.index == frame.item_descs[1].index, "scrolled hit test uses view space");
    ui_test_check(ui_list_get_item_at_position(fixture.view, make_double2(100.0, -50.0), &target) == false, "hit test outside the view misses");

    if (ui_node_is_null(prev_root) == false) {
        ui_release(prev_root);
    }

    ui_test_list_fixture_destroy_(&fixture);
}

struct ui_test_list_rows {
    uint32_t taps;
    uint32_t slides;
};

static void ui_test_list_row_tap_(struct ui_tap_gesture gesture, void* user) {
    struct ui_test_list_rows* rows = user;

    rows->taps++;
}

static void ui_test_list_row_pan_(struct ui_pan_gesture gesture, void* user) {
    struct ui_test_list_rows* rows = user;

    if (ui_pan_gesture_get_phase(gesture) == ui_gesture_phase_began) {
        rows->slides++;
    }
}

static void ui_test_list_rows_step_(struct ui_node* prev_root, struct ui_test_list_fixture* fixture, struct ui_test_list_rows* rows, double timestamp, const struct app_event* events, uint32_t event_count) {
    ui_input_dispatch(*prev_root, timestamp, events, event_count, 1.0);

    struct ui_node root = ui_frame(&(struct ui_frame) {
        .size = ui_size_fix(200, 300),
    });

    struct ui_list_linear linear = ui_test_list_input_linear_();

    struct ui_node host = ui_frame(&(struct ui_frame) {
        .parent = root,
        .size = ui_size_fix(linear.size.x, linear.size.y),
        .clip = true,
    });

    ui_test_list_update_(fixture, host, ui_list_linear_layout(&linear), false);

    struct ui_list_iter iter = ui_list_iter(fixture->view);
    struct ui_list_item_desc desc;
    while (ui_list_iter_next(&iter, &desc)) {
        struct ui_node row = ui_frame(&(struct ui_frame) {
            .parent = host,
            .key = ui_key_u64(desc.index),
            .pos = ui_pos_xy(desc.pos.x, desc.pos.y),
            .size = ui_size_fix(desc.size.x, desc.size.y),
        });

        ui_pan(&(struct ui_pan) {
            .node = row,
            .on_change = ui_test_list_row_pan_,
            .user = rows,
            .direction = ui_pan_direction_x,
        });
        ui_tap(&(struct ui_tap) {
            .node = row,
            .on_change = ui_test_list_row_tap_,
            .user = rows,
        });
    }

    ui_layout(root);

    if (ui_node_is_null(*prev_root) == false) {
        ui_release(*prev_root);
    }
    *prev_root = root;
}

static void ui_test_list_row_gestures(void) {
    struct cr_mem* mem = cr_mem_libc;

    struct ui_test_list_sizes sizes = { .base = make_double2(200.0, 20.0), .wide_index = uint32_max };
    struct ui_test_list_fixture fixture = ui_test_list_make_(mem, &sizes);
    struct ui_test_list_rows rows = { 0 };

    ui_test_list_append_(fixture.view, 40);

    struct ui_node prev_root = ui_node_null;
    double timestamp = 1.0;
    double dt = 1.0 / 60.0;

    for (uint32_t i = 0; i < 10; i++) {
        ui_test_list_rows_step_(&prev_root, &fixture, &rows, timestamp, NULL, 0);
        timestamp += dt;
    }

    struct app_event swipe_down = ui_test_list_touch_(app_pointer_event_type_touch_began, 100.0, 50.0, timestamp, 101);
    ui_test_list_rows_step_(&prev_root, &fixture, &rows, timestamp, &swipe_down, 1);

    for (uint32_t k = 1; k <= 3; k++) {
        timestamp += dt;
        struct app_event swipe_move = ui_test_list_touch_(app_pointer_event_type_touch_moved, 100.0 + 8.0 * (double)k, 51.0, timestamp, 101);
        ui_test_list_rows_step_(&prev_root, &fixture, &rows, timestamp, &swipe_move, 1);
    }

    ui_test_check(rows.slides == 1 && ui_list_get_scroll_offset(fixture.view) == 0.0, "a quick sideways swipe on a row of a scrolling list goes to the row");

    timestamp += dt;
    struct app_event swipe_up = ui_test_list_touch_(app_pointer_event_type_touch_ended, 124.0, 51.0, timestamp, 101);
    ui_test_list_rows_step_(&prev_root, &fixture, &rows, timestamp, &swipe_up, 1);

    timestamp += dt;
    struct app_event fling_down = ui_test_list_touch_(app_pointer_event_type_touch_began, 100.0, 90.0, timestamp, 102);
    ui_test_list_rows_step_(&prev_root, &fixture, &rows, timestamp, &fling_down, 1);

    for (uint32_t k = 1; k <= 5; k++) {
        timestamp += dt;
        struct app_event fling_move = ui_test_list_touch_(app_pointer_event_type_touch_moved, 100.0, 90.0 - 15.0 * (double)k, timestamp, 102);
        ui_test_list_rows_step_(&prev_root, &fixture, &rows, timestamp, &fling_move, 1);
    }

    timestamp += dt;
    struct app_event fling_up = ui_test_list_touch_(app_pointer_event_type_touch_ended, 100.0, 15.0, timestamp, 102);
    ui_test_list_rows_step_(&prev_root, &fixture, &rows, timestamp, &fling_up, 1);

    double released = ui_list_get_scroll_offset(fixture.view);

    timestamp += dt;
    ui_test_list_rows_step_(&prev_root, &fixture, &rows, timestamp, NULL, 0);

    double coasting = ui_list_get_scroll_offset(fixture.view);

    ui_test_check(rows.taps == 0 && rows.slides == 1 && coasting > released, "a fling keeps the list moving and reaches no row");

    timestamp += dt;
    struct app_event stop_down = ui_test_list_touch_(app_pointer_event_type_touch_began, 100.0, 50.0, timestamp, 103);
    ui_test_list_rows_step_(&prev_root, &fixture, &rows, timestamp, &stop_down, 1);

    double caught = ui_list_get_scroll_offset(fixture.view);

    timestamp += 0.05;
    struct app_event stop_up = ui_test_list_touch_(app_pointer_event_type_touch_ended, 100.0, 50.0, timestamp, 103);
    ui_test_list_rows_step_(&prev_root, &fixture, &rows, timestamp, &stop_up, 1);

    for (uint32_t i = 0; i < 30; i++) {
        timestamp += dt;
        ui_test_list_rows_step_(&prev_root, &fixture, &rows, timestamp, NULL, 0);
    }

    ui_test_check(caught == coasting && ui_list_get_scroll_offset(fixture.view) == caught, "a tap on a moving list stops it in that frame");
    ui_test_check(rows.taps == 0 && rows.slides == 1, "the tap that stops the list does not tap the row under it");

    timestamp += dt;
    struct app_event tap_down = ui_test_list_touch_(app_pointer_event_type_touch_began, 100.0, 50.0, timestamp, 104);
    ui_test_list_rows_step_(&prev_root, &fixture, &rows, timestamp, &tap_down, 1);

    timestamp += 0.05;
    struct app_event tap_up = ui_test_list_touch_(app_pointer_event_type_touch_ended, 100.0, 50.0, timestamp, 104);
    ui_test_list_rows_step_(&prev_root, &fixture, &rows, timestamp, &tap_up, 1);

    ui_test_check(rows.taps == 1, "a tap on the list at rest taps the row");

    ui_release(prev_root);
    ui_test_list_fixture_destroy_(&fixture);
}

static void ui_test_list_perf_sample_(uint64_t* seed, uint32_t* out, uint32_t count, uint32_t range) {
    uint32_t needed = count;

    for (uint32_t i = 0; i < range && needed > 0; i++) {
        if (cr_rand(seed) % (range - i) < needed) {
            out[count - needed] = i;
            needed--;
        }
    }

    assert(needed == 0);
}

static void ui_test_list_perf(void) {
    struct cr_mem* mem = cr_mem_libc;

    uint32_t item_count = 1u << 16;
    uint32_t frame_count = 16;

    struct ui_test_list_sizes column_sizes = { .base = make_double2(0.0, 20.0), .wide_index = uint32_max };
    struct ui_test_list_sizes grid_sizes = { .base = make_double2(50.0, 50.0), .wide_index = uint32_max };

    struct ui_list_linear column_layout = ui_test_list_column_layout_(400.0, 800.0);
    struct ui_list_linear grid_layout = ui_test_list_row_layout_(true, 400.0, 800.0);

    double frame_ms[2] = { 0 };

    for (uint32_t c = 0; c < 2; c++) {
        struct ui_test_list_fixture fixture = ui_test_list_make_(mem, c == 0 ? &column_sizes : &grid_sizes);
        const struct ui_list_linear* layout = c == 0 ? &column_layout : &grid_layout;

        ui_test_list_append_(fixture.view, item_count);

        double begin_s = cr_get_current_time();

        for (uint32_t i = 0; i < frame_count; i++) {
            ui_test_list_step_(&fixture, layout);
        }

        frame_ms[c] = (cr_get_current_time() - begin_s) * 1000.0 / (double)frame_count;

        ui_test_list_fixture_destroy_(&fixture);
    }

    printf("list perf frame %u items: column %.2f ms, grid %.2f ms\n", item_count, frame_ms[0], frame_ms[1]);

    struct ui_test_list_fixture fixture = ui_test_list_make_(mem, &column_sizes);
    ui_test_list_append_(fixture.view, item_count);
    ui_test_list_step_(&fixture, &column_layout);

    uint64_t seed = 1;

    uint32_t* srcs = cr_malloc(mem, item_count * sizeof(srcs[0]), 0, cr_loc);
    uint32_t* dsts = cr_malloc(mem, item_count * sizeof(dsts[0]), 0, cr_loc);
    struct ui_list_item_move* moves = cr_malloc(mem, item_count * sizeof(moves[0]), 0, cr_loc);

    double op_ms[3][13] = { 0 };
    uint32_t size_count = 0;

    for (uint32_t n = 1u << 4; n <= item_count; n <<= 1) {
        assert(size_count < cr_count(op_ms[0]));

        ui_test_list_perf_sample_(&seed, srcs, n, item_count + n);
        double begin_s = cr_get_current_time();
        ui_list_edit(fixture.view, &(struct ui_list_edit) { .adds = srcs, .add_count = n });
        op_ms[0][size_count] = (cr_get_current_time() - begin_s) * 1000.0;

        ui_test_list_perf_sample_(&seed, srcs, n, item_count + n);
        begin_s = cr_get_current_time();
        ui_list_edit(fixture.view, &(struct ui_list_edit) { .deletes = srcs, .delete_count = n });
        op_ms[1][size_count] = (cr_get_current_time() - begin_s) * 1000.0;

        ui_test_list_perf_sample_(&seed, srcs, n, item_count);
        ui_test_list_perf_sample_(&seed, dsts, n, item_count);
        for (uint32_t i = 0; i < n; i++) {
            moves[i] = (struct ui_list_item_move) { .src_index = srcs[i], .dst_index = dsts[i] };
        }
        begin_s = cr_get_current_time();
        ui_list_edit(fixture.view, &(struct ui_list_edit) { .moves = moves, .move_count = n });
        op_ms[2][size_count] = (cr_get_current_time() - begin_s) * 1000.0;

        ui_test_list_step_(&fixture, &column_layout);
        size_count++;
    }

    cr_free(mem, srcs, cr_loc);
    cr_free(mem, dsts, cr_loc);
    cr_free(mem, moves, cr_loc);
    ui_test_list_fixture_destroy_(&fixture);

    const char* op_labels[] = { "add", "delete", "move" };

    for (uint32_t op = 0; op < cr_count(op_labels); op++) {
        printf("list perf %s ms 2^4..2^16:", op_labels[op]);
        for (uint32_t s = 0; s < size_count; s++) {
            printf(" %.2f", op_ms[op][s]);
        }
        printf("\n");
    }
}

void ui_test_list_(void) {

    ui_test_list_lifecycle();
    ui_test_list_growth();

    ui_test_list_horizontal();
    ui_test_list_grid_layout();
    ui_test_list_collapse();
    ui_test_list_matches_stack();
    ui_test_list_pixel_grid();
    ui_test_list_custom_layout();

    ui_test_list_item_sizes();
    ui_test_list_resize();
    ui_test_list_resize_packs();
    ui_test_list_driven_resize();
    ui_test_list_driven_rewrap();
    ui_test_list_ghost_resize();
    ui_test_list_insert_remove_animated();

    ui_test_list_drag_targets();
    ui_test_list_drag_dead_node();
    ui_test_list_reorder();
    ui_test_list_reorder_embedded();
    ui_test_list_wheel();
    ui_test_list_row_gestures();
    ui_test_list_drag_lost_end();
    ui_test_list_edit_in_callbacks();
    ui_test_list_queries_after_edit();
    ui_test_list_no_on_drag();

    ui_test_list_perf();

}
