#include "demo.h"
#include "demo_items.h"
#include "hud.h"
#include <math.h>
#include <string.h>

#define collection_initial_count 12u
#define collection_gap 24.0
#define collection_strip_height 128.0
#define collection_label_height 32.0
#define collection_panel_width_min 220.0
#define collection_panel_height_min 120.0
#define collection_panel_count 3u

struct collection_view {
    const char* name;
    double2 (*size)(uint32_t id, bool expanded, void* user);
    struct demo_items items;
    struct ui_list_linear linear;
};

struct collection_page {
    struct cr_mem* mem;
    uint64_t seed;
    struct collection_view views[4];
};

static double2 collection_list_size(uint32_t id, bool expanded, void* user) {
    return make_double2(0.0, expanded ? 96.0 : 44.0);
}

static double2 collection_grid_size(uint32_t id, bool expanded, void* user) {
    return expanded ? make_double2(132.0, 132.0) : make_double2(64.0, 64.0);
}

static double2 collection_mosaic_size(uint32_t id, bool expanded, void* user) {
    double2 size = make_double2(56.0 + (double)(id % 3) * 44.0, 56.0);

    return expanded ? size * 1.5 : size;
}

static double2 collection_strip_size(uint32_t id, bool expanded, void* user) {
    double width = 64.0 + (double)(id % 3) * 28.0;

    return make_double2(expanded ? width * 1.6 : width, 0.0);
}

static struct ui_list_linear_insert collection_insert(const struct ui_list_item_desc* target, double2 position, void* user) {
    return ui_list_linear_insert(user, target, position);
}

static void* collection_create(struct cr_mem* mem) {
    struct collection_page* page = cr_malloc(mem, sizeof(*page), 0, cr_loc);

    memset(page, 0, sizeof(*page));
    page->mem = mem;
    page->seed = 1;
    page->views[0] = (struct collection_view) { .name = "List", .size = collection_list_size };
    page->views[1] = (struct collection_view) { .name = "Grid", .size = collection_grid_size };
    page->views[2] = (struct collection_view) { .name = "Mosaic", .size = collection_mosaic_size };
    page->views[3] = (struct collection_view) { .name = "Strip", .size = collection_strip_size };

    for (uint32_t i = 0; i < cr_count(page->views); i++) {
        demo_items_init(&page->views[i].items, mem, collection_initial_count);
    }

    return page;
}

static void collection_destroy(void* state) {
    struct collection_page* page = state;

    for (uint32_t i = 0; i < cr_count(page->views); i++) {
        demo_items_deinit(&page->views[i].items);
    }

    cr_free(page->mem, page, cr_loc);
}

static bool collection_has_drag(const struct collection_page* page) {
    for (uint32_t i = 0; i < cr_count(page->views); i++) {
        if (demo_items_is_dragging(&page->views[i].items)) {
            return true;
        }
    }

    return false;
}

static void collection_panel(struct ui_node parent, double2 pos, double2 size, struct collection_view* view, struct ui_list_linear linear, const struct demo_frame* frame) {
    const struct demo_theme* theme = frame->theme;

    struct ui_node panel = ui_frame(&(struct ui_frame) {
        .parent = parent,
        .pos = ui_pos_xy(pos.x, pos.y),
        .size = ui_size_fix(size.x, size.y),
    });

    ui_text(&(struct ui_text) {
        .parent = panel,
        .str = ui_str_weight(view->name, theme->fg, 17, 590),
    });

    view->linear = linear;
    view->linear.size = make_double2(size.x, size.y - collection_label_height);

    struct ui_node host = demo_host(panel, make_double2(0, collection_label_height), view->linear.size, theme);

    demo_items_frame(&view->items, &(struct demo_items_desc) {
        .host = host,
        .layout = ui_list_linear_layout(&view->linear),
        .size = view->size,
        .insert = collection_insert,
        .user = &view->linear,
        .snap = frame->appeared,
        .delta_s = frame->delta_s,
        .theme = theme,
    });
}

static void collection_frame(void* state, const struct demo_frame* frame) {
    struct collection_page* page = state;

    double spacing = hud_slider("Spacing", 0, 24, 10, hud_slider_display_mode_float);
    static const char* const dist_labels[] = { "Start", "Center", "End" };
    enum ui_dist dist = (enum ui_dist)hud_select_labels("Dist", cr_count(dist_labels), 1, dist_labels);

    bool has_drag = collection_has_drag(page);
    uint32_t count = page->views[0].items.count;

    if (hud_button("Add item", "Add") && has_drag == false) {
        uint32_t index = (uint32_t)(cr_rand(&page->seed) % (count + 1));

        for (uint32_t i = 0; i < cr_count(page->views); i++) {
            demo_items_add(&page->views[i].items, index);
        }
    }

    if (hud_button("Remove item", "Remove") && has_drag == false && count > 0) {
        uint32_t index = (uint32_t)(cr_rand(&page->seed) % count);

        for (uint32_t i = 0; i < cr_count(page->views); i++) {
            demo_items_remove(&page->views[i].items, index);
        }
    }

    double top = demo_title(frame, "List", "tap an item to resize it, long press to reorder, add and remove in the panel");

    double2 size = frame->size;
    double row_w = max_double(size.x - 2.0 * demo_margin, 120.0);
    uint32_t columns = (uint32_t)clamp_double(floor((row_w + collection_gap) / (collection_panel_width_min + collection_gap)), 1.0, (double)collection_panel_count);
    uint32_t rows = (collection_panel_count + columns - 1) / columns;
    double panel_w = (row_w - collection_gap * (double)(columns - 1)) / (double)columns;
    double panel_h = max_double((size.y - top - demo_margin - collection_strip_height - collection_gap * (double)rows) / (double)rows, collection_panel_height_min);
    double4 padding = ui_pad(10);
    double2 spacing2 = make_double2(spacing, spacing);

    struct ui_list_linear vertical = { .axis = ui_list_axis_y, .padding = padding, .spacing = spacing2 };
    struct ui_list_linear grid = { .axis = ui_list_axis_x, .wrap = true, .padding = padding, .spacing = spacing2, .dist = dist };
    struct ui_list_linear strip = { .axis = ui_list_axis_x, .padding = padding, .spacing = spacing2 };

    for (uint32_t i = 0; i < collection_panel_count; i++) {
        uint32_t column = i % columns;
        uint32_t row = i / columns;
        bool alone = i + 1 == collection_panel_count && column == 0;
        double2 pos = make_double2(demo_margin + (panel_w + collection_gap) * (double)column, top + (panel_h + collection_gap) * (double)row);

        collection_panel(frame->root, pos, make_double2(alone ? row_w : panel_w, panel_h), &page->views[i], i == 0 ? vertical : grid, frame);
    }

    collection_panel(frame->root, make_double2(demo_margin, top + (panel_h + collection_gap) * (double)rows), make_double2(row_w, collection_strip_height), &page->views[3], strip, frame);
}

const struct demo_page demo_page_collection = {
    .name = "Collection",
    .create = collection_create,
    .destroy = collection_destroy,
    .frame = collection_frame,
};
