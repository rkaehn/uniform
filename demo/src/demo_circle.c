#include "demo.h"
#include "demo_items.h"
#include "hud.h"
#include <math.h>
#include <string.h>

#define circle_initial_count 8u
#define circle_item_margin 58.0

struct circle_layout {
    double2 bounds;
    double radius;
    uint32_t count;
};

struct circle_page {
    struct cr_mem* mem;
    uint64_t seed;
    struct demo_items items;
    struct circle_layout layout;
};

static double2 circle_item_size(uint32_t id, bool expanded, void* user) {
    return expanded ? make_double2(96.0, 96.0) : make_double2(56.0, 56.0);
}

static double2 circle_point(const struct circle_layout* layout, double angle) {
    double2 pos = layout->bounds * 0.5 + layout->radius * make_double2(cos(angle), sin(angle));

    return make_double2(ui_round_px(pos.x), ui_round_px(pos.y));
}

static double2 circle_place(struct ui_list_layout_item* items, uint32_t count, bool rebase, void* user) {
    struct circle_layout* layout = user;

    layout->count = count;

    for (uint32_t i = 0; i < count; i++) {
        double angle = 2.0 * M_PI * (double)i / (double)count - 0.5 * M_PI;

        items[i].offset = circle_point(layout, angle) - items[i].size * 0.5;
    }

    return layout->bounds;
}

static struct ui_list_linear_insert circle_insert(const struct ui_list_item_desc* target, double2 position, void* user) {
    const struct circle_layout* layout = user;
    double2 center = layout->bounds * 0.5;
    double2 to_target = target->pos + target->size * 0.5 - center;
    double2 to_drag = position - center;

    bool insert_after = to_target.x * to_drag.y - to_target.y * to_drag.x > 0.0;

    double step = 2.0 * M_PI / (double)layout->count;
    double angle = atan2(to_target.y, to_target.x) + (insert_after ? 0.5 : -0.5) * step;

    return (struct ui_list_linear_insert) {
        .index = target->index + (insert_after ? 1u : 0u),
        .cursor_pos = circle_point(layout, angle),
        .cursor_size = fabs(cos(angle)) > fabs(sin(angle)) ? make_double2(24.0, 0.0) : make_double2(0.0, 24.0),
    };
}

static void* circle_create(struct cr_mem* mem) {
    struct circle_page* page = cr_malloc(mem, sizeof(*page), 0, cr_loc);

    memset(page, 0, sizeof(*page));
    page->mem = mem;
    page->seed = 1;
    demo_items_init(&page->items, mem, circle_initial_count);

    return page;
}

static void circle_destroy(void* state) {
    struct circle_page* page = state;

    demo_items_deinit(&page->items);
    cr_free(page->mem, page, cr_loc);
}

static void circle_frame(void* state, const struct demo_frame* frame) {
    struct circle_page* page = state;
    struct demo_items* items = &page->items;

    double radius = hud_slider("Radius", 100, 280, 180, hud_slider_display_mode_float);
    bool has_drag = demo_items_is_dragging(items);

    if (hud_button("Add item", "Add") && has_drag == false) {
        demo_items_add(items, (uint32_t)(cr_rand(&page->seed) % (items->count + 1)));
    }

    if (hud_button("Remove item", "Remove") && has_drag == false && items->count > 0) {
        demo_items_remove(items, (uint32_t)(cr_rand(&page->seed) % items->count));
    }

    double top = demo_title(frame, "Circle Layout", "a custom list layout, tap an item to resize it, long press to reorder, add and remove in the panel");

    double2 size = frame->size;
    double2 host_size = max_double2(size - make_double2(2.0 * demo_margin, top + demo_margin), make_double2(240.0, 240.0));

    struct ui_node host = demo_host(frame->root, make_double2(demo_margin, top), host_size, frame->theme);

    page->layout.bounds = host_size;
    page->layout.radius = min_double(radius, min_double(host_size.x, host_size.y) * 0.5 - circle_item_margin);

    demo_items_frame(items, &(struct demo_items_desc) {
        .host = host,
        .layout = {
            .place = circle_place,
            .user = &page->layout,
            .scroll_axis = ui_list_axis_y,
            .size = host_size,
            .item_available = host_size,
        },
        .size = circle_item_size,
        .insert = circle_insert,
        .user = &page->layout,
        .snap = frame->appeared,
        .delta_s = frame->delta_s,
        .theme = frame->theme,
    });
}

const struct demo_page demo_page_circle = {
    .name = "Circle",
    .create = circle_create,
    .destroy = circle_destroy,
    .frame = circle_frame,
};
