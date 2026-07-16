#include "demo.h"
#include "tree.h"
#include "ui_input.h"
#include "ui_gestures.h"
#include "ui_list.h"
#include "ui_physics.h"
#include "hud.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define tree_row_height 36.0
#define tree_bar_height 36.0
#define tree_indent 16.0
#define tree_panel_width 320.0
#define tree_label_size 12
#define tree_label_capacity 12
#define tree_selection_max 256u

struct tree_page;

struct tree_data {
    uint32_t id;
    char label[tree_label_capacity];
    bool expanded;
    double turn;
    double indent;
    double dim;
    double select;
    struct ui_slide slide;
};

struct tree_row {
    uint32_t id;
    double indent;
    char label[tree_label_capacity];
    bool has_children;
    bool slide_right;
    double turn;
    double slide_offset;
    double dim;
    double select;
};

struct tree_ref {
    struct tree_page* page;
    uint32_t id;
};

struct tree_button {
    const char* label;
    void (*on_tap)(struct ui_tap_gesture gesture, void* user);
};

struct tree_page {
    struct cr_mem* mem;
    struct ui_list* view;
    struct cr_tree* tree;
    double delete_w;
    double delta_s;
    uint32_t next_id;
    uint32_t slide_index;
    uint32_t drag_src;
    uint32_t drag_level;
    bool drag_valid;
    bool drag_active;
    double2 drag_position;
    double drag_cursor_y;
    struct ui_list_linear linear;
    struct tree_ref* refs;
    uint32_t ref_count;
    uint32_t ref_capacity;
};

#define tree_node_size cr_tree_node_size(sizeof(struct tree_data))

static struct tree_data* tree_data_at(struct tree_page* page, uint32_t index) {
    return cr_tree_node_get_content(cr_tree_get_node(page->tree, index));
}

static uint32_t tree_index_of(struct tree_page* page, uint32_t id) {
    uint32_t count = cr_tree_get_node_count(page->tree);

    for (uint32_t i = 0; i < count; i++) {
        if (tree_data_at(page, i)->id == id) {
            return i;
        }
    }

    return uint32_max;
}

static bool tree_is_visible(struct tree_page* page, uint32_t index) {
    uint32_t parent = cr_tree_node_get_parent_index(cr_tree_get_node(page->tree, index));

    while (parent != uint32_max) {
        struct cr_tree_node* node = cr_tree_get_node(page->tree, parent);
        struct tree_data* data = cr_tree_node_get_content(node);

        if (data->expanded == false) {
            return false;
        }

        parent = cr_tree_node_get_parent_index(node);
    }

    return true;
}

static bool tree_is_group(struct cr_tree_node* node, void* user) {
    struct tree_data* data = cr_tree_node_get_content(node);

    return cr_tree_node_get_first_child_index(node) != uint32_max && data->expanded;
}

static uint32_t tree_selected(struct tree_page* page, uint32_t* out_indices, uint32_t capacity) {
    uint32_t count = cr_tree_get_node_count(page->tree);
    uint32_t selected_count = 0;

    for (uint32_t i = 0; i < count && selected_count < capacity; i++) {
        if (cr_tree_node_get_is_selected(cr_tree_get_node(page->tree, i))) {
            out_indices[selected_count++] = i;
        }
    }

    return selected_count;
}

static bool tree_is_dragged(struct tree_page* page, uint32_t index) {
    if (page->drag_src == uint32_max) {
        return false;
    }

    bool multi = cr_tree_node_get_is_selected(cr_tree_get_node(page->tree, page->drag_src));

    while (index != uint32_max) {
        struct cr_tree_node* node = cr_tree_get_node(page->tree, index);

        if (index == page->drag_src || (multi && cr_tree_node_get_is_selected(node))) {
            return true;
        }

        index = cr_tree_node_get_parent_index(node);
    }

    return false;
}

static void tree_on_move(const struct cr_tree_loc* adds, uint32_t add_count, const struct cr_tree_loc* deletes, uint32_t delete_count, const struct cr_tree_loc_2* moves, uint32_t move_count, const struct cr_tree_node* nodes, void* user) {
    struct tree_page* page = user;

    uint32_t* add_indices = cr_malloc(page->mem, add_count * sizeof(*add_indices), 0, cr_loc);
    uint32_t* delete_indices = cr_malloc(page->mem, delete_count * sizeof(*delete_indices), 0, cr_loc);
    struct ui_list_item_move* move_items = cr_malloc(page->mem, move_count * sizeof(*move_items), 0, cr_loc);

    for (uint32_t i = 0; i < add_count; i++) {
        add_indices[i] = cr_tree_loc_get_index(adds[i]);
    }
    for (uint32_t i = 0; i < delete_count; i++) {
        delete_indices[i] = cr_tree_loc_get_index(deletes[i]);
    }
    for (uint32_t i = 0; i < move_count; i++) {
        move_items[i] = (struct ui_list_item_move) {
            .src_index = cr_tree_loc_get_index(moves[i].src),
            .dst_index = cr_tree_loc_get_index(moves[i].dst),
        };
    }

    ui_list_edit(page->view, &(struct ui_list_edit) {
        .adds = add_indices,
        .add_count = add_count,
        .deletes = delete_indices,
        .delete_count = delete_count,
        .moves = move_items,
        .move_count = move_count,
        .animated = true,
    });

    cr_free(page->mem, add_indices, cr_loc);
    cr_free(page->mem, delete_indices, cr_loc);
    cr_free(page->mem, move_items, cr_loc);
}

static void tree_approach(double* value, double target, double rate, double delta_s) {
    *value += (target - *value) * (1.0 - exp(-rate * delta_s));

    if (fabs(target - *value) < 0.001) {
        *value = target;
    }
}

static void tree_on_swipe(void* user) {
    struct tree_page* page = user;
    uint32_t index = page->slide_index;
    bool selected = cr_tree_node_get_is_selected(cr_tree_get_node(page->tree, index));
    struct cr_tree_loc loc = cr_tree_loc_make_index(index);

    cr_tree_select(page->tree, &loc, 1, selected ? cr_tree_selection_mode_subtract : cr_tree_selection_mode_add);
}

static void tree_item_update(uint32_t index, void* content, double2 size, struct ui_list_on_item_update_result* out_result, void* user) {
    struct tree_page* page = user;
    struct cr_tree_node* node = cr_tree_get_node(page->tree, index);
    struct tree_data* data = cr_tree_node_get_content(node);
    struct tree_row* row = content;
    bool has_children = cr_tree_node_get_first_child_index(node) != uint32_max;
    double indent_target = (double)cr_tree_node_get_level(node) * tree_indent;

    tree_approach(&data->turn, data->expanded ? 1.0 : 0.0, 14.0, page->delta_s);
    tree_approach(&data->dim, tree_is_dragged(page, index) ? 0.35 : 1.0, 8.0, page->delta_s);
    tree_approach(&data->select, cr_tree_node_get_is_selected(node) ? 1.0 : 0.0, 8.0, page->delta_s);

    if (data->indent < 0.0) {
        data->indent = indent_target;
    } else {
        data->indent = ui_approach_px(data->indent, indent_target, 1.0 - exp(-8.0 * page->delta_s));
    }

    page->slide_index = index;
    ui_slide_set_on_slide(&data->slide, tree_on_swipe, page);
    ui_slide_on_update(&data->slide, size.x, page->delete_w, page->delta_s);

    row->id = data->id;
    row->indent = data->indent;
    memcpy(row->label, data->label, sizeof(row->label));
    row->has_children = has_children;
    row->slide_right = data->slide.is_right;
    row->turn = data->turn;
    row->slide_offset = ui_slide_get_offset(&data->slide);
    row->dim = data->dim;
    row->select = data->select;

    out_result->size.y = tree_row_height;

    if (has_children && data->expanded == false) {
        out_result->collapse_end_index = cr_tree_get_next_index(page->tree, index);
    }
}

static void tree_on_row_pan(struct ui_pan_gesture gesture, void* user) {
    struct tree_ref* ref = user;
    uint32_t index = tree_index_of(ref->page, ref->id);

    if (index != uint32_max) {
        ui_slide_on_pan(&tree_data_at(ref->page, index)->slide, gesture);
    }
}

static void tree_on_row_tap(struct ui_tap_gesture gesture, void* user) {
    struct tree_ref* ref = user;
    struct tree_page* page = ref->page;
    uint32_t index = tree_index_of(page, ref->id);

    if (index == uint32_max) {
        return;
    }

    struct cr_tree_loc loc = cr_tree_loc_make_index(index);

    cr_tree_select(page->tree, &loc, 1, cr_tree_selection_mode_replace);
}

static void tree_on_arrow_tap(struct ui_tap_gesture gesture, void* user) {
    struct tree_ref* ref = user;
    uint32_t index = tree_index_of(ref->page, ref->id);

    if (index != uint32_max) {
        struct tree_data* data = tree_data_at(ref->page, index);

        data->expanded = !data->expanded;
    }
}

static void tree_on_delete_tap(struct ui_tap_gesture gesture, void* user) {
    struct tree_ref* ref = user;
    struct tree_page* page = ref->page;

    if (page->drag_src != uint32_max) {
        return;
    }

    uint32_t index = tree_index_of(page, ref->id);

    if (index != uint32_max) {
        cr_tree_delete(page->tree, &index, 1);
    }
}

static void tree_data_init(struct tree_page* page, struct tree_data* data, const char* label) {
    *data = (struct tree_data) { .id = page->next_id++, .expanded = true, .turn = 1.0, .indent = -1.0, .dim = 1.0 };
    ui_slide_init(&data->slide, ui_slide_mode_trigger, ui_slide_mode_actions);
    snprintf(data->label, sizeof(data->label), "%s", label);
}

static void tree_on_bar_add(struct ui_tap_gesture gesture, void* user) {
    struct tree_page* page = user;

    if (page->drag_src != uint32_max) {
        return;
    }

    uint32_t count = cr_tree_get_node_count(page->tree);
    uint32_t after = uint32_max;

    for (uint32_t i = 0; i < count; i++) {
        if (cr_tree_node_get_is_selected(cr_tree_get_node(page->tree, i))) {
            after = i;
        }
    }

    _Alignas(cr_tree_node_align) uint8_t buffer[tree_node_size];
    struct cr_tree_node* node = (struct cr_tree_node*)buffer;
    char label[tree_label_capacity];

    cr_tree_node_init(node);
    snprintf(label, sizeof(label), "Item %u", page->next_id);
    tree_data_init(page, cr_tree_node_get_content(node), label);
    cr_tree_add(page->tree, after, node, 1, false);
}

static void tree_on_bar_delete(struct ui_tap_gesture gesture, void* user) {
    struct tree_page* page = user;

    if (page->drag_src != uint32_max) {
        return;
    }

    uint32_t indices[tree_selection_max];
    uint32_t selected_count = tree_selected(page, indices, cr_count(indices));

    if (selected_count > 0) {
        cr_tree_delete(page->tree, indices, selected_count);
    }
}

static void tree_on_bar_group(struct ui_tap_gesture gesture, void* user) {
    struct tree_page* page = user;

    if (page->drag_src != uint32_max) {
        return;
    }

    uint32_t indices[tree_selection_max];
    uint32_t selected_count = tree_selected(page, indices, cr_count(indices));

    if (selected_count == 0) {
        return;
    }

    uint32_t group_index;

    cr_tree_group(page->tree, indices, selected_count, &group_index, true);

    if (group_index != uint32_max) {
        char label[tree_label_capacity];

        snprintf(label, sizeof(label), "Group %u", page->next_id);
        tree_data_init(page, tree_data_at(page, group_index), label);
    }
}

static void tree_reset(struct tree_page* page) {
    static const struct { const char* label; uint32_t level; } seed[] = {
        { "Projects", 0 },
        { "Design", 1 },
        { "Icons", 2 },
        { "Layout", 2 },
        { "Engine", 1 },
        { "Renderer", 2 },
        { "Physics", 2 },
        { "Audio", 2 },
        { "Inbox", 0 },
        { "Ideas", 1 },
        { "Sketches", 1 },
        { "Archive", 0 },
        { "2025", 1 },
    };

    uint32_t count = cr_tree_get_node_count(page->tree);

    if (count > 0) {
        uint32_t* indices = cr_malloc(page->mem, count * sizeof(*indices), 0, cr_loc);

        for (uint32_t i = 0; i < count; i++) {
            indices[i] = i;
        }

        cr_tree_delete(page->tree, indices, count);
        cr_free(page->mem, indices, cr_loc);
    }

    _Alignas(cr_tree_node_align) uint8_t buffer[cr_count(seed) * tree_node_size];
    struct cr_tree_loc adds[cr_count(seed)];

    for (uint32_t i = 0; i < cr_count(seed); i++) {
        struct cr_tree_node* node = cr_tree_node_at(buffer, i, tree_node_size);

        cr_tree_node_init(node);
        tree_data_init(page, cr_tree_node_get_content(node), seed[i].label);
        adds[i] = cr_tree_loc_make_index_level(i, seed[i].level);
    }

    cr_tree_move(page->tree, adds, cr_count(seed), NULL, 0, NULL, 0, (const struct cr_tree_node*)buffer);
}

static uint32_t tree_drag_indices(struct tree_page* page, uint32_t* out_indices, uint32_t capacity) {
    if (cr_tree_node_get_is_selected(cr_tree_get_node(page->tree, page->drag_src))) {
        return tree_selected(page, out_indices, capacity);
    }

    out_indices[0] = page->drag_src;
    return 1;
}

static void tree_track_drag(struct tree_page* page) {
    struct ui_list_item_desc target;

    page->drag_valid = false;

    if (ui_list_get_item_near_position(page->view, page->drag_position, &target) == false) {
        return;
    }

    struct ui_list_linear_insert insert = ui_list_linear_insert(&page->linear, &target, page->drag_position);
    uint32_t count = cr_tree_get_node_count(page->tree);
    uint32_t gap = insert.index;
    uint32_t above = uint32_max;

    for (uint32_t i = gap; i-- > 0;) {
        if (tree_is_visible(page, i)) {
            above = i;
            break;
        }
    }

    uint32_t max_level = 0;

    if (above != uint32_max) {
        struct cr_tree_node* node = cr_tree_get_node(page->tree, above);
        struct tree_data* data = cr_tree_node_get_content(node);
        bool collapsed = cr_tree_node_get_first_child_index(node) != uint32_max && data->expanded == false;

        gap = collapsed ? cr_tree_get_next_index(page->tree, above) : above + 1;
        max_level = cr_tree_node_get_level(node) + (collapsed ? 0u : 1u);
    } else {
        gap = 0;
    }

    double finger = floor((page->drag_position.x - 36.0) / tree_indent + 0.5);
    uint32_t level = finger < 0.0 ? 0u : min_uint32((uint32_t)finger, max_level);

    uint32_t indices[tree_selection_max];
    uint32_t index_count = tree_drag_indices(page, indices, cr_count(indices));

    struct cr_tree_cursor_info info = { 0 };
    bool bottom = gap >= count;

    if (cr_tree_update_drag(page->tree, indices, index_count, bottom, bottom ? count - 1 : gap, level, &info, tree_is_group, page)) {
        page->drag_valid = true;
        page->drag_active = info.is_active;
        page->drag_level = info.level;
        page->drag_cursor_y = insert.cursor_pos.y;
    }
}

static void tree_on_drag(struct ui_list_drag drag, void* user) {
    struct tree_page* page = user;
    struct ui_list_item_desc target;

    page->drag_position = drag.position;

    switch (drag.phase) {
    case ui_list_drag_phase_began: {
        page->drag_src = ui_list_get_item_at_position(page->view, drag.position, &target) ? target.index : uint32_max;
    } break;
    case ui_list_drag_phase_changed: break;
    case ui_list_drag_phase_ended: {
        if (page->drag_src != uint32_max) {
            tree_track_drag(page);
            cr_tree_end_drag(page->tree, page->drag_valid);
            page->drag_src = uint32_max;
        }
    } break;
    case ui_list_drag_phase_canceled: {
        if (page->drag_src != uint32_max) {
            cr_tree_end_drag(page->tree, false);
            page->drag_src = uint32_max;
        }
    } break;
    }
}

static void tree_refs_reserve(struct tree_page* page, uint32_t count) {
    page->ref_count = 0;

    if (page->ref_capacity < count) {
        page->refs = cr_realloc(page->mem, page->refs, count * sizeof(*page->refs), cr_loc);
        page->ref_capacity = count;
    }
}

static struct tree_ref* tree_ref(struct tree_page* page, uint32_t id) {
    assert(page->ref_count < page->ref_capacity);

    struct tree_ref* ref = &page->refs[page->ref_count++];

    ref->page = page;
    ref->id = id;

    return ref;
}

static void* tree_create(struct cr_mem* mem) {
    struct tree_page* page = cr_malloc(mem, sizeof(*page), 0, cr_loc);

    memset(page, 0, sizeof(*page));
    page->mem = mem;
    page->view = ui_list_create(mem, sizeof(struct tree_row));
    page->tree = cr_tree_create(mem, mem, sizeof(struct tree_data));
    cr_tree_set_on_move(page->tree, tree_on_move, page);
    page->next_id = 1;
    page->drag_src = uint32_max;
    tree_reset(page);

    return page;
}

static void tree_destroy(void* state) {
    struct tree_page* page = state;

    cr_tree_destroy(page->tree);
    ui_list_destroy(page->view);
    cr_free(page->mem, page->refs, cr_loc);
    cr_free(page->mem, page, cr_loc);
}

static void tree_arrow(struct ui_node body, double indent, double turn, struct ui_color color, struct tree_ref* ref, bool is_live) {
    struct ui_node arrow = ui_frame(&(struct ui_frame) {
        .parent = body,
        .pos = ui_pos_xy(indent + 2, (tree_row_height - 24) * 0.5),
        .size = ui_size_fix(24, 24),
    });

    if (is_live) {
        ui_tap(&(struct ui_tap) {
            .node = arrow,
            .on_change = tree_on_arrow_tap,
            .user = ref,
        });
    }

    double angle = turn * M_PI * 0.5;
    double c = cos(angle);
    double s = sin(angle);
    static const double2 points[] = { { -2.0, -4.0 }, { 2.0, 0.0 }, { -2.0, 4.0 } };
    struct ui_path* path = ui_path_create();

    for (uint32_t i = 0; i < cr_count(points); i++) {
        double2 p = points[i];
        double2 rotated = make_double2(p.x * c - p.y * s, p.x * s + p.y * c) + 12.0;

        if (i == 0) {
            ui_path_move_to(path, rotated);
        } else {
            ui_path_line_to(path, rotated);
        }
    }

    ui_shape(&(struct ui_shape) {
        .parent = arrow,
        .size = ui_size_fix(24, 24),
        .mode = ui_shape_mode_stroke,
        .thickness = 2.0,
        .path = path,
        .fill = ui_fill_solid(color),
    });
    ui_path_destroy(path);
}

static void tree_rows(struct tree_page* page, struct ui_node list, const struct demo_frame* frame) {
    const struct demo_theme* theme = frame->theme;
    float gray = theme->is_dark ? 1.0f : 0.0f;
    struct ui_list_iter iter = ui_list_iter(page->view);
    struct ui_list_item_desc desc;

    while (ui_list_iter_next(&iter, &desc)) {
        struct tree_row* row = desc.content;
        struct tree_ref* ref = tree_ref(page, row->id);
        bool is_live = desc.is_removed == false;
        double offset = row->slide_offset;
        double w = desc.size.x;
        double h = desc.size.y;
        double indent = 8.0 + row->indent;
        double opacity = desc.opacity * row->dim;

        struct ui_node cell = ui_frame(&(struct ui_frame) {
            .parent = list,
            .key = ui_key_u64(row->id),
            .pos = ui_pos_xy(desc.pos.x, desc.pos.y),
            .size = ui_size_fix(w, h),
            .clip = true,
            .blend = { .mode = ui_blend_mode_normal, .opacity = opacity },
        });

        if (offset < 0.0 && row->slide_right == false) {
            double button_w = max_double(-offset, page->delete_w);

            struct ui_node button = ui_frame(&(struct ui_frame) {
                .parent = cell,
                .key = ui_key("delete"),
                .pos = ui_pos_xy(w + offset, 0),
                .size = ui_size_fix(button_w, h),
                .fill = ui_fill_solid(demo_palette[0]),
                .layout = ui_layout_stack_x(ui_dist_center, ui_align_center, 0, ui_pad_xy(16, 0)),
            });

            if (is_live) {
                ui_tap(&(struct ui_tap) {
                    .node = button,
                    .on_change = tree_on_delete_tap,
                    .user = ref,
                });
            }

            ui_text(&(struct ui_text) {
                .parent = button,
                .str = demo_str(frame->font_mono, "DELETE", ui_white, tree_label_size, 400),
            });
        }

        struct ui_color select_color = demo_palette[1];
        select_color.value.w = 0.35f * (float)row->select;

        struct ui_node body = ui_frame(&(struct ui_frame) {
            .parent = cell,
            .pos = ui_pos_xy(offset, 0),
            .size = ui_size_fix(w, h),
            .fill = row->select > 0.0 ? ui_fill_solid(select_color) : (struct ui_fill) { 0 },
        });

        if (is_live) {
            ui_pan(&(struct ui_pan) {
                .node = body,
                .on_change = tree_on_row_pan,
                .user = ref,
                .direction = ui_pan_direction_x,
            });
            ui_tap(&(struct ui_tap) {
                .node = body,
                .on_change = tree_on_row_tap,
                .user = ref,
            });
        }

        if (row->has_children) {
            tree_arrow(body, indent, row->turn, ui_gray_a(gray, 0.25f), ref, is_live);
        }

        ui_text(&(struct ui_text) {
            .parent = body,
            .pos = ui_pos_xy(indent + 28, 10),
            .str = demo_str(frame->font_mono, row->label, theme->fg, tree_label_size, 400),
        });
    }
}

static void tree_cursor(struct tree_page* page, struct ui_node list, double panel_w, const struct demo_theme* theme) {
    double cursor_x = 28.0 + (double)page->drag_level * tree_indent;
    double cursor_y = page->drag_cursor_y;
    struct ui_color color = page->drag_active ? demo_palette[1] : ui_gray_a(theme->is_dark ? 1.0f : 0.0f, 0.4f);

    struct ui_node cursor = ui_frame(&(struct ui_frame) {
        .parent = list,
        .pos = ui_pos_xy(cursor_x, cursor_y - 4),
        .size = ui_size_fix(panel_w - cursor_x, 8),
        .shape_group.op = ui_shape_group_op_union,
        .fill = ui_fill_solid(color),
    });

    struct ui_path* ring = ui_path_create();
    ui_path_ellipse(ring, make_double2(1, 1), make_double2(6, 6));

    ui_shape(&(struct ui_shape) {
        .parent = cursor,
        .size = ui_size_fix(8, 8),
        .mode = ui_shape_mode_stroke,
        .thickness = 2,
        .path = ring,
    });
    ui_path_destroy(ring);

    ui_line(&(struct ui_line) {
        .parent = cursor,
        .pos = ui_pos_xy(6, 3),
        .size = ui_size_fix(panel_w - cursor_x - 6.0, 2),
        .thickness = 2,
    });
}

static void tree_bar(struct tree_page* page, struct ui_node host, double panel_w, const struct demo_frame* frame) {
    static const struct tree_button single[2] = { { "DEL", tree_on_bar_delete }, { "ADD", tree_on_bar_add } };
    static const struct tree_button multi[2] = { { "GRP", tree_on_bar_group }, { "DEL", tree_on_bar_delete } };

    const struct demo_theme* theme = frame->theme;
    uint32_t selected_indices[tree_selection_max];
    uint32_t selected_count = tree_selected(page, selected_indices, cr_count(selected_indices));
    const struct tree_button* buttons = selected_count > 1 ? multi : single;
    double bar_fade = clamp_double(ui_list_get_scroll_offset(page->view) / 4.0, 0.0, 1.0);

    if (bar_fade > 0.0) {
        ui_frame(&(struct ui_frame) {
            .parent = host,
            .key = ui_key("bar fade"),
            .size = ui_size_fix(panel_w, tree_bar_height),
            .fill.layers[0] = ui_layer_background_blur(16),
            .blend = { .mode = ui_blend_mode_normal, .opacity = bar_fade },
        });
    }

    struct ui_node bar = ui_frame(&(struct ui_frame) {
        .parent = host,
        .size = ui_size_fix(panel_w, tree_bar_height),
        .layout = ui_layout_stack_x(ui_dist_end, ui_align_center, 0, ui_pad_xy(8, 0)),
    });

    for (uint32_t i = 0; i < 2; i++) {
        struct ui_node button = ui_frame(&(struct ui_frame) {
            .parent = bar,
            .key = ui_key_str(buttons[i].label),
            .size.height = ui_size_axis_fix(tree_bar_height),
            .layout = ui_layout_stack_x(ui_dist_start, ui_align_center, 0, ui_pad_xy(8, 0)),
        });

        ui_tap(&(struct ui_tap) {
            .node = button,
            .on_change = buttons[i].on_tap,
            .user = page,
        });

        ui_text(&(struct ui_text) {
            .parent = button,
            .str = demo_str(frame->font_mono, buttons[i].label, theme->fg, tree_label_size, 400),
        });
    }

    struct ui_color hairline = theme->hairline;
    hairline.value.w *= (float)bar_fade;

    ui_frame(&(struct ui_frame) {
        .parent = host,
        .pos = ui_pos_xy(0, tree_bar_height),
        .size = ui_size_fix(panel_w, 1.0 / ui_get_to_px_from_pt()),
        .fill = ui_fill_solid(hairline),
    });
}

static void tree_frame(void* state, const struct demo_frame* frame) {
    struct tree_page* page = state;
    const struct demo_theme* theme = frame->theme;

    page->delta_s = frame->delta_s;
    page->delete_w = ceil(demo_measure(frame->font_mono, "DELETE", tree_label_size, 400, 0).x) + 32.0;

    if (hud_button("Tree", "Reset") && page->drag_src == uint32_max) {
        tree_reset(page);
    }

    double top = demo_title(frame, "Tree", "tap to select, swipe right to extend the selection, swipe left to delete, long press to reorder, drag sideways to change level");

    double2 size = frame->size;
    double panel_w = max_double(min_double(tree_panel_width, size.x - 2.0 * demo_margin), 160.0);
    double panel_h = max_double(size.y - top - demo_margin, 160.0);

    struct ui_node host = ui_frame(&(struct ui_frame) {
        .parent = frame->root,
        .pos = ui_pos_xy((size.x - panel_w) * 0.5, top),
        .size = ui_size_fix(panel_w, panel_h),
        .corners = ui_corners(demo_corner),
        .fill = ui_fill_solid(theme->panel),
        .clip = true,
        .backdrop_root = true,
    });

    struct ui_node list = ui_frame(&(struct ui_frame) {
        .parent = host,
        .size = ui_size_fix(panel_w, panel_h),
    });

    page->linear = (struct ui_list_linear) {
        .axis = ui_list_axis_y,
        .size = make_double2(panel_w, panel_h),
        .padding = make_double4(0, 0, tree_bar_height, 0),
    };

    ui_list_update(page->view, &(struct ui_list_update) {
        .node = list,
        .layout = ui_list_linear_layout(&page->linear),
        .snap = frame->appeared,
        .delta_s = frame->delta_s,
        .on_item_update = tree_item_update,
        .on_drag = tree_on_drag,
        .user = page,
    });

    bool dragging = page->drag_src != uint32_max;

    if (dragging) {
        tree_track_drag(page);
    }

    tree_refs_reserve(page, cr_tree_get_node_count(page->tree));
    tree_rows(page, list, frame);

    if (dragging && page->drag_valid) {
        tree_cursor(page, list, panel_w, theme);
    }

    tree_bar(page, host, panel_w, frame);
}

const struct demo_page demo_page_tree = {
    .name = "Tree",
    .create = tree_create,
    .destroy = tree_destroy,
    .frame = tree_frame,
};
