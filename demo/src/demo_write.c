#include "demo.h"
#include "demo_icons.h"
#include "ui_input.h"
#include "ui_gestures.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define write_action_max 128u
#define write_chip_pt 13
#define write_chip_weight 510
#define write_editor_pt 15
#define write_editor_weight 400
#define write_controls_height 48.0
#define write_panel_width_max 580.0

enum write_menu {
    write_menu_none,
    write_menu_env,
    write_menu_project,
    write_menu_branch,
    write_menu_mode,
    write_menu_model,
    write_menu_effort,

    write_menu_count,
};

enum write_action_kind {
    write_action_toggle_menu,
    write_action_select,
    write_action_close,
    write_action_toggle_worktree,
    write_action_send,
};

enum write_row_type {
    write_row_item,
    write_row_header,
    write_row_separator,
};

struct write_page;

struct write_action {
    struct write_page* page;
    enum write_action_kind kind;
    uint32_t menu;
    int32_t value;
};

struct write_row {
    enum write_row_type type;
    enum demo_icon icon;
    const char* label;
    const char* subtitle;
    const char* badge;
    const char* detail;
    bool chevron;
    bool dim;
    int32_t value;
};

struct write_menu_def {
    uint32_t id;
    double width;
    const struct write_row* rows;
    uint32_t row_count;
    int32_t checked;
    bool opens_up;
};

struct write_theme {
    struct ui_color bg;
    struct ui_color text_primary;
    struct ui_color text_secondary;
    struct ui_color placeholder;
    struct ui_color accent;
    struct ui_color accent_hover;
    struct ui_color on_accent;
    struct ui_color violet_fg;
    struct ui_color violet_fill;
    struct ui_color violet_fill_hover;
    struct ui_color violet_fill_open;
    struct ui_color chip_fill;
    struct ui_color chip_fill_hover;
    struct ui_color chip_fill_open;
    struct ui_color chip_fg;
    struct ui_color chip_fg_open;
    struct ui_color hairline;
    struct ui_color row_hover;
    struct ui_color row_press;
    struct ui_color badge_fill;
    struct ui_color thumb_fill;
    struct ui_color thumb_ring;
    struct ui_color track_dot;
    enum ui_glass_appearance glass;
    float selection_alpha;
};

struct write_anchor {
    double2 pos;
    double2 size;
};

struct write_chips {
    char branch_label[40];
    double env_w;
    double project_w;
    double branch_zone_w;
    double worktree_zone_w;
    double branch_w;
    double2 pos[3];
    double height;
};

struct write_page {
    struct cr_mem* mem;
    struct ui_text_input* input;
    uint32_t project;
    uint32_t branch;
    uint32_t mode;
    uint32_t model;
    bool worktree;
    double effort_t;
    bool effort_dragging;
    double effort_track_x;
    double effort_track_usable;
    uint32_t open_menu;
    double editor_content_h;
    struct write_anchor anchors[write_menu_count];
    struct write_action actions[write_action_max];
    uint32_t action_count;
};

static const char* const write_projects[] = { "uniform", "c", "page", "Plastic", "linux", "Circuitry", "rust_test", "ICB", "Metal-Timing-Issues", "Metal-Binary" };
static const char* const write_branches[] = { "main", "claude/build-targets-test-suite-20bb49", "claude/missing-bd-header-error-d7e40b", "claude/text-editing-polish-0e2499" };
static const char* const write_modes[] = { "Manual", "Accept edits", "Plan", "Auto", "Bypass permissions" };
static const char* const write_models[] = { "Fable 5", "Opus 4.8", "Sonnet 5", "Haiku 4.5" };
static const char* const write_efforts[] = { "Minimal", "Low", "Medium", "High", "Extra", "Max" };

#define write_effort_stops ((uint32_t)cr_count(write_efforts))

static struct write_theme write_theme(bool is_dark) {
    struct write_theme t;

    if (is_dark) {
        t.bg = ui_hex(0x17161d);
        t.text_primary = ui_hex(0xf0eff5);
        t.text_secondary = ui_hex(0x9b9aa4);
        t.placeholder = ui_hex_a(0xf0eff5, 0.34f);
        t.accent = ui_hex(0x7c6bf2);
        t.accent_hover = ui_hex(0x8b7bf7);
        t.on_accent = ui_white;
        t.violet_fg = ui_hex(0xb3a7ff);
        t.violet_fill = ui_hex_a(0x8d7bff, 0.16f);
        t.violet_fill_hover = ui_hex_a(0x8d7bff, 0.22f);
        t.violet_fill_open = ui_hex_a(0x8d7bff, 0.30f);
        t.chip_fill = ui_hex_a(0xffffff, 0.07f);
        t.chip_fill_hover = ui_hex_a(0xffffff, 0.11f);
        t.chip_fill_open = ui_hex_a(0xffffff, 0.15f);
        t.chip_fg = ui_hex(0xb6b5bf);
        t.chip_fg_open = ui_hex(0xdcdbe4);
        t.hairline = ui_hex_a(0xffffff, 0.11f);
        t.row_hover = ui_hex_a(0xffffff, 0.08f);
        t.row_press = ui_hex_a(0xffffff, 0.12f);
        t.badge_fill = ui_hex_a(0xffffff, 0.09f);
        t.thumb_fill = ui_hex(0xf4f3f8);
        t.thumb_ring = ui_hex_a(0x000000, 0.22f);
        t.track_dot = ui_hex_a(0xffffff, 0.28f);
        t.glass = ui_glass_appearance_dark;
        t.selection_alpha = 0.38f;
    } else {
        t.bg = ui_white;
        t.text_primary = ui_hex(0x232227);
        t.text_secondary = ui_hex(0x6f6e78);
        t.placeholder = ui_hex_a(0x232227, 0.38f);
        t.accent = ui_hex(0x6b5ae8);
        t.accent_hover = ui_hex(0x5c4bdd);
        t.on_accent = ui_white;
        t.violet_fg = ui_hex(0x5748d2);
        t.violet_fill = ui_hex_a(0x6b5ae8, 0.11f);
        t.violet_fill_hover = ui_hex_a(0x6b5ae8, 0.16f);
        t.violet_fill_open = ui_hex_a(0x6b5ae8, 0.22f);
        t.chip_fill = ui_hex_a(0x1c1b20, 0.07f);
        t.chip_fill_hover = ui_hex_a(0x1c1b20, 0.10f);
        t.chip_fill_open = ui_hex_a(0x1c1b20, 0.14f);
        t.chip_fg = ui_hex(0x44434b);
        t.chip_fg_open = ui_hex(0x2e2d34);
        t.hairline = ui_hex_a(0x18181c, 0.12f);
        t.row_hover = ui_hex_a(0x18181c, 0.065f);
        t.row_press = ui_hex_a(0x18181c, 0.10f);
        t.badge_fill = ui_hex_a(0x1c1b20, 0.07f);
        t.thumb_fill = ui_white;
        t.thumb_ring = ui_hex_a(0x18181c, 0.14f);
        t.track_dot = ui_hex_a(0x18181c, 0.28f);
        t.glass = ui_glass_appearance_light;
        t.selection_alpha = 0.28f;
    }

    return t;
}

static struct ui_corners write_corners(double radius) {
    return (struct ui_corners) { .profile = ui_corner_profile_continuous, .radius = ui_pad(radius) };
}

static struct write_action* write_action(struct write_page* page, enum write_action_kind kind, uint32_t menu, int32_t value) {
    assert(page->action_count < write_action_max);

    struct write_action* action = &page->actions[page->action_count++];

    action->page = page;
    action->kind = kind;
    action->menu = menu;
    action->value = value;

    return action;
}

static void write_apply_select(struct write_page* page, uint32_t menu, int32_t value) {
    if (value < 0) {
        return;
    }

    switch (menu) {
    case write_menu_project: page->project = (uint32_t)value; break;
    case write_menu_branch: page->branch = (uint32_t)value; break;
    case write_menu_mode: page->mode = (uint32_t)value; break;
    case write_menu_model: page->model = (uint32_t)value; break;
    default: break;
    }
}

static void write_on_tap(struct ui_tap_gesture gesture, void* user) {
    struct write_action* action = user;
    struct write_page* page = action->page;

    switch (action->kind) {
    case write_action_toggle_menu: {
        page->open_menu = page->open_menu == action->menu ? write_menu_none : action->menu;
    } break;
    case write_action_select: {
        write_apply_select(page, action->menu, action->value);
        page->open_menu = write_menu_none;
    } break;
    case write_action_close: {
        page->open_menu = write_menu_none;
    } break;
    case write_action_toggle_worktree: {
        page->worktree = !page->worktree;
    } break;
    case write_action_send: {
        ui_text_input_set_text(page->input, "", 0);
    } break;
    }
}

static void write_noop_scroll(struct app_scroll_event event, void* user) {
}

static void write_noop_pointer(struct app_pointer_event event, struct ui_pointer pointer, void* user) {
}

static double write_text_width(const char* label) {
    return ceil(demo_measure(NULL, label, write_chip_pt, write_chip_weight, 0).x);
}

static void write_middle_truncate(char* dst, size_t dst_size, const char* src, size_t keep) {
    size_t len = strlen(src);

    if (len <= keep) {
        snprintf(dst, dst_size, "%s", src);
        return;
    }

    size_t head = keep / 2;
    size_t tail = keep - head;

    snprintf(dst, dst_size, "%.*s...%s", (int)head, src, src + len - tail);
}

static struct ui_node write_chip_begin(struct write_page* page, const struct write_theme* t, struct ui_node parent, uint32_t menu, struct ui_pos pos, double width, double spacing, double pad_left, double pad_right) {
    struct write_action* action = write_action(page, write_action_toggle_menu, menu, -1);
    struct ui_node chip = ui_node(parent, ui_key_auto);

    struct ui_tap_gesture tap = ui_tap(&(struct ui_tap) {
        .node = chip,
        .on_change = write_on_tap,
        .user = action,
    });
    struct ui_hover_gesture hover = ui_hover(&(struct ui_hover) {
        .node = chip,
    });

    bool is_violet = menu == write_menu_env || menu == write_menu_project || menu == write_menu_branch;
    bool is_active = page->open_menu == menu || ui_tap_gesture_has_touch(tap);
    bool is_hover = ui_hover_gesture_has_hover(hover);

    struct ui_color fill;

    if (is_violet) {
        fill = is_active ? t->violet_fill_open : is_hover ? t->violet_fill_hover : t->violet_fill;
    } else {
        fill = is_active ? t->chip_fill_open : is_hover ? t->chip_fill_hover : t->chip_fill;
    }

    ui_frame(&(struct ui_frame) {
        .node = chip,
        .pos = pos,
        .size = ui_size_fix(width, is_violet ? 30 : 28),
        .layout = ui_layout_stack_x(ui_dist_center, ui_align_center, spacing, make_double4(pad_left, pad_right, 0, 0)),
        .corners = write_corners(10),
        .fill = ui_fill_solid(fill),
    });

    return chip;
}

static void write_chip_icon(struct ui_node chip, enum demo_icon icon, struct ui_color color) {
    struct ui_node box = ui_frame(&(struct ui_frame) {
        .parent = chip,
        .size = ui_size_fix(18, 18),
    });

    demo_icon_draw_large(box, icon, color);
}

static void write_chip_label(struct ui_node chip, const char* label, struct ui_color color) {
    ui_text(&(struct ui_text) {
        .parent = chip,
        .str = ui_str_weight(label, color, write_chip_pt, write_chip_weight),
    });
}

static struct write_chips write_chips_layout(struct write_page* page, double panel_w) {
    struct write_chips l;

    write_middle_truncate(l.branch_label, sizeof(l.branch_label), write_branches[page->branch], 24);

    l.env_w = 11 + 18 + 5 + write_text_width("Local") + 11;
    l.project_w = 11 + 18 + 5 + write_text_width(write_projects[page->project]) + 11;
    l.branch_zone_w = 11 + 18 + 5 + write_text_width(l.branch_label) + 9;
    l.worktree_zone_w = 9 + 14 + 6 + write_text_width("worktree") + 11;
    l.branch_w = l.branch_zone_w + 1 + l.worktree_zone_w;

    double widths[3] = { l.env_w, l.project_w, l.branch_w };
    double2 cursor = make_double2(0, 0);

    for (uint32_t i = 0; i < 3; i++) {
        if (cursor.x > 0 && cursor.x + widths[i] > panel_w) {
            cursor = make_double2(0, cursor.y + 36);
        }
        l.pos[i] = cursor;
        cursor.x += widths[i] + 6;
    }

    l.height = cursor.y + 30;

    return l;
}

static void write_violet_chips(struct write_page* page, const struct write_theme* t, struct ui_node parent, double2 origin, const struct write_chips* l) {
    double2 env_pos = origin + l->pos[0];
    double2 project_pos = origin + l->pos[1];
    double2 branch_pos = origin + l->pos[2];

    struct ui_node env = write_chip_begin(page, t, parent, write_menu_env, ui_pos_xy(env_pos.x, env_pos.y), l->env_w, 5, 11, 11);
    write_chip_icon(env, demo_icon_laptop, t->violet_fg);
    write_chip_label(env, "Local", t->violet_fg);
    page->anchors[write_menu_env] = (struct write_anchor) { .pos = env_pos, .size = make_double2(l->env_w, 30) };

    struct ui_node project = write_chip_begin(page, t, parent, write_menu_project, ui_pos_xy(project_pos.x, project_pos.y), l->project_w, 5, 11, 11);
    write_chip_icon(project, demo_icon_folder, t->violet_fg);
    write_chip_label(project, write_projects[page->project], t->violet_fg);
    page->anchors[write_menu_project] = (struct write_anchor) { .pos = project_pos, .size = make_double2(l->project_w, 30) };

    struct ui_node branch = write_chip_begin(page, t, parent, write_menu_branch, ui_pos_xy(branch_pos.x, branch_pos.y), l->branch_w, 0, 0, 0);
    page->anchors[write_menu_branch] = (struct write_anchor) { .pos = branch_pos, .size = make_double2(l->branch_w, 30) };

    struct ui_node branch_zone = ui_frame(&(struct ui_frame) {
        .parent = branch,
        .size = ui_size_fix(l->branch_zone_w, 30),
        .layout = ui_layout_stack_x(ui_dist_center, ui_align_center, 5, make_double4(11, 9, 0, 0)),
    });

    write_chip_icon(branch_zone, demo_icon_branch, t->violet_fg);
    write_chip_label(branch_zone, l->branch_label, t->violet_fg);

    struct ui_color divider = t->violet_fg;
    divider.value.w = 0.25f;

    ui_frame(&(struct ui_frame) {
        .parent = branch,
        .size = ui_size_fix(1, 16),
        .fill = ui_fill_solid(divider),
    });

    struct write_action* worktree_action = write_action(page, write_action_toggle_worktree, 0, -1);

    struct ui_node worktree_zone = ui_frame(&(struct ui_frame) {
        .parent = branch,
        .size = ui_size_fix(l->worktree_zone_w, 30),
        .layout = ui_layout_stack_x(ui_dist_center, ui_align_center, 6, make_double4(9, 11, 0, 0)),
    });

    ui_tap(&(struct ui_tap) {
        .node = worktree_zone,
        .on_change = write_on_tap,
        .user = worktree_action,
    });

    struct ui_node box = ui_frame(&(struct ui_frame) {
        .parent = worktree_zone,
        .size = ui_size_fix(14, 14),
        .corners = write_corners(4),
        .fill = page->worktree ? ui_fill_solid(t->accent) : (struct ui_fill) { 0 },
        .outline = page->worktree ? (struct ui_outline) { 0 } : ui_outline_solid(ui_outline_align_inside, 1.0, t->violet_fg),
    });

    if (page->worktree) {
        demo_icon_draw_check_small(box, t->on_accent);
    }

    write_chip_label(worktree_zone, "worktree", t->violet_fg);
}

static void write_menu_rows(struct write_page* page, const struct write_theme* t, struct ui_node content, const struct write_menu_def* def, bool interactive) {
    double content_w = def->width - 10;

    for (uint32_t i = 0; i < def->row_count; i++) {
        const struct write_row* row = &def->rows[i];

        switch (row->type) {
        case write_row_header: {
            struct ui_node header = ui_frame(&(struct ui_frame) {
                .parent = content,
                .size = ui_size_fix(content_w, i == 0 ? 21 : 24),
                .layout = ui_layout_stack_y(ui_dist_end, ui_align_start, 0, make_double4(9, 9, 0, 3)),
            });

            ui_text(&(struct ui_text) {
                .parent = header,
                .str = ui_str_weight(row->label, t->text_secondary, 11, 510),
            });
        } break;
        case write_row_separator: {
            struct ui_node separator = ui_frame(&(struct ui_frame) {
                .parent = content,
                .size = ui_size_fix(content_w, 9),
            });

            ui_frame(&(struct ui_frame) {
                .parent = separator,
                .pos = ui_pos_xy(0, 4),
                .size = ui_size_fix(content_w, 1),
                .fill = ui_fill_solid(t->hairline),
            });
        } break;
        case write_row_item: {
            struct ui_node item = ui_node(content, ui_key_auto);
            struct ui_fill fill = { 0 };

            if (interactive) {
                struct write_action* action = write_action(page, write_action_select, def->id, row->value);
                struct ui_tap_gesture tap = ui_tap(&(struct ui_tap) {
                    .node = item,
                    .on_change = write_on_tap,
                    .user = action,
                });
                struct ui_hover_gesture hover = ui_hover(&(struct ui_hover) {
                    .node = item,
                });

                if (ui_tap_gesture_has_touch(tap)) {
                    fill = ui_fill_solid(t->row_press);
                } else if (ui_hover_gesture_has_hover(hover)) {
                    fill = ui_fill_solid(t->row_hover);
                }
            }

            ui_frame(&(struct ui_frame) {
                .node = item,
                .size.width = ui_size_axis_fix(content_w),
                .layout = ui_layout_stack_y(ui_dist_start, ui_align_start, 0, 0),
                .corners = write_corners(9),
                .fill = fill,
            });

            struct ui_node line = ui_frame(&(struct ui_frame) {
                .parent = item,
                .size = ui_size_fix(content_w, 30),
                .layout = ui_layout_stack_x(ui_dist_start, ui_align_center, 7, make_double4(9, 9, 0, 0)),
            });

            struct ui_color label_color = row->dim ? t->text_secondary : t->text_primary;

            if (row->icon != demo_icon_none) {
                struct ui_node box = ui_frame(&(struct ui_frame) {
                    .parent = line,
                    .size = ui_size_fix(16, 16),
                });

                demo_icon_draw(box, row->icon, label_color);
            }

            ui_text(&(struct ui_text) {
                .parent = line,
                .str = ui_str_weight(row->label, label_color, 13, 400),
            });

            if (row->badge) {
                struct ui_node badge = ui_frame(&(struct ui_frame) {
                    .parent = line,
                    .layout = ui_layout_stack_x(ui_dist_center, ui_align_center, 0, ui_pad_xy(5, 2.5)),
                    .corners = write_corners(5),
                    .fill = ui_fill_solid(t->badge_fill),
                });

                ui_text(&(struct ui_text) {
                    .parent = badge,
                    .str = ui_str_weight(row->badge, t->text_secondary, 10.5, 510),
                });
            }

            ui_frame(&(struct ui_frame) {
                .parent = line,
                .size.width = ui_size_axis_fill(),
            });

            if (def->checked >= 0 && row->value == def->checked) {
                struct ui_node box = ui_frame(&(struct ui_frame) {
                    .parent = line,
                    .size = ui_size_fix(16, 16),
                });

                demo_icon_draw(box, demo_icon_check, t->text_primary);
            }

            if (row->detail) {
                ui_text(&(struct ui_text) {
                    .parent = line,
                    .str = ui_str_weight(row->detail, t->text_secondary, 12.5, 400),
                });
            }

            if (row->chevron) {
                struct ui_node box = ui_frame(&(struct ui_frame) {
                    .parent = line,
                    .size = ui_size_fix(16, 16),
                });

                demo_icon_draw(box, demo_icon_chevron_right, t->text_secondary);
            }

            if (row->subtitle) {
                double indent = row->icon != demo_icon_none ? 32.0 : 9.0;
                double subtitle_w = content_w - indent - 9;

                struct ui_str subtitle = {
                    .raw = row->subtitle,
                    .attributes = {
                        { .start = 0, .end = uint32_max, .type = ui_str_attribute_type_color, .color = t->text_secondary },
                        { .start = 0, .end = uint32_max, .type = ui_str_attribute_type_pt_size, .pt_size = 11.5 },
                    },
                };

                const char* code = strstr(row->subtitle, "claude rc");

                if (code) {
                    uint32_t start = (uint32_t)(code - row->subtitle);
                    uint32_t end = start + 9;

                    subtitle.attributes[2] = (struct ui_str_attribute) { .start = start, .end = end, .type = ui_str_attribute_type_weight, .weight = 590 };
                    subtitle.attributes[3] = (struct ui_str_attribute) { .start = start, .end = end, .type = ui_str_attribute_type_color, .color = t->text_primary };
                }

                struct ui_node holder = ui_frame(&(struct ui_frame) {
                    .parent = item,
                    .size.width = ui_size_axis_fix(content_w),
                    .layout = ui_layout_stack_y(ui_dist_start, ui_align_start, 0, make_double4(indent, 9, 0, 8)),
                });

                ui_text(&(struct ui_text) {
                    .parent = holder,
                    .size.width = ui_size_axis_fix(subtitle_w),
                    .str = subtitle,
                });
            }
        } break;
        }
    }
}

static double write_menu_height(struct write_page* page, const struct write_theme* t, const struct write_menu_def* def) {
    struct ui_node probe = ui_frame(&(struct ui_frame) {
        .size.width = ui_size_axis_fix(def->width),
        .layout = ui_layout_stack_y(ui_dist_start, ui_align_start, 0, ui_pad(5)),
    });

    write_menu_rows(page, t, probe, def, false);
    ui_layout(probe);
    double height = ui_get_size(probe).y;
    ui_release(probe);

    return height;
}

static double write_menu_x(struct write_page* page, const struct write_menu_def* def, double2 size) {
    return clamp_double(round(page->anchors[def->id].pos.x), 12, max_double(12, size.x - def->width - 12));
}

static struct ui_node write_menu_panel(struct write_page* page, const struct write_theme* t, struct ui_node root, const struct write_menu_def* def, double2 size, double height) {
    struct write_anchor anchor = page->anchors[def->id];
    double x = write_menu_x(page, def, size);
    double y = def->opens_up
        ? round(max_double(12, anchor.pos.y - 8 - height))
        : round(min_double(anchor.pos.y + anchor.size.y + 8, max_double(12, size.y - 12 - height)));

    struct ui_node panel = ui_frame(&(struct ui_frame) {
        .parent = root,
        .pos = ui_pos_xy(x, y),
        .size.width = ui_size_axis_fix(def->width),
        .layout = ui_layout_stack_y(ui_dist_start, ui_align_start, 0, ui_pad(5)),
        .corners = write_corners(14),
        .fill.layers[0] = ui_layer_glass(ui_glass_variant_regular, t->glass),
    });

    ui_on_pointer(&(struct ui_on_pointer) {
        .node = panel,
        .func = write_noop_pointer,
    });
    ui_on_scroll(&(struct ui_on_scroll) {
        .node = panel,
        .func = write_noop_scroll,
    });

    return panel;
}

static void write_menu_overlay_rows(struct write_page* page, const struct write_theme* t, struct ui_node root, const struct write_menu_def* def, double2 size) {
    struct ui_node panel = write_menu_panel(page, t, root, def, size, write_menu_height(page, t, def));

    write_menu_rows(page, t, panel, def, true);
}

static void write_effort_on_pan(struct ui_pan_gesture gesture, void* user) {
    struct write_page* page = user;
    enum ui_gesture_phase phase = ui_pan_gesture_get_phase(gesture);

    if (phase == ui_gesture_phase_began || phase == ui_gesture_phase_changed) {
        double x = ui_pan_gesture_get_position(gesture).x;

        page->effort_dragging = true;
        page->effort_t = clamp_double((x - page->effort_track_x) / page->effort_track_usable, 0, 1);
    } else if (phase == ui_gesture_phase_ended || phase == ui_gesture_phase_canceled) {
        page->effort_dragging = false;
    }
}

static void write_effort_on_pointer(struct app_pointer_event event, struct ui_pointer pointer, void* user) {
    struct write_page* page = user;

    if (event.type == app_pointer_event_type_touch_began) {
        page->effort_t = clamp_double((event.pos.x - page->effort_track_x) / page->effort_track_usable, 0, 1);
    }
}

static void write_effort_overlay(struct write_page* page, const struct write_theme* t, struct ui_node root, const struct write_menu_def* def, double2 size) {
    double pad_x = 14;
    double height = 98;
    struct ui_node panel = write_menu_panel(page, t, root, def, size, height);
    double content_w = def->width - 2 * pad_x;

    struct ui_node content = ui_frame(&(struct ui_frame) {
        .parent = panel,
        .size.width = ui_size_axis_fix(def->width - 10),
        .layout = ui_layout_stack_y(ui_dist_start, ui_align_start, 0, make_double4(pad_x - 5, pad_x - 5, 7, 7)),
    });

    struct ui_node header = ui_frame(&(struct ui_frame) {
        .parent = content,
        .size = ui_size_fix(content_w, 20),
        .layout = ui_layout_stack_x(ui_dist_start, ui_align_center, 6, 0),
    });

    ui_text(&(struct ui_text) {
        .parent = header,
        .str = ui_str_weight("Effort", t->text_primary, 13, 590),
    });

    uint32_t effort_index = (uint32_t)round(page->effort_t * (double)(write_effort_stops - 1));

    ui_text(&(struct ui_text) {
        .parent = header,
        .str = ui_str_weight(write_efforts[effort_index], t->text_secondary, 13, 510),
    });

    ui_frame(&(struct ui_frame) {
        .parent = header,
        .size.width = ui_size_axis_fill(),
    });

    struct ui_node help = ui_frame(&(struct ui_frame) {
        .parent = header,
        .size = ui_size_fix(16, 16),
        .layout = ui_layout_stack_x(ui_dist_center, ui_align_center, 0, 0),
    });

    demo_icon_draw_help_ring(help, t->text_secondary);
    ui_text(&(struct ui_text) {
        .parent = help,
        .str = ui_str_weight("?", t->text_secondary, 9.5, 590),
    });

    ui_frame(&(struct ui_frame) {
        .parent = content,
        .size = ui_size_fix(content_w, 10),
    });

    struct ui_node labels = ui_frame(&(struct ui_frame) {
        .parent = content,
        .size = ui_size_fix(content_w, 16),
        .layout = ui_layout_stack_x(ui_dist_start, ui_align_center, 0, 0),
    });

    ui_text(&(struct ui_text) {
        .parent = labels,
        .str = ui_str_weight("Faster", t->text_secondary, 11.5, 400),
    });
    ui_frame(&(struct ui_frame) {
        .parent = labels,
        .size.width = ui_size_axis_fill(),
    });
    ui_text(&(struct ui_text) {
        .parent = labels,
        .str = ui_str_weight("Smarter", t->text_secondary, 11.5, 400),
    });

    double thumb = 20;
    double usable = content_w - thumb;

    struct ui_node track = ui_frame(&(struct ui_frame) {
        .parent = content,
        .size = ui_size_fix(content_w, 28),
        .corners = ui_corners_circular(14),
        .fill = ui_fill_solid(t->chip_fill),
    });

    ui_pan(&(struct ui_pan) {
        .node = track,
        .on_change = write_effort_on_pan,
        .user = page,
    });
    ui_on_pointer(&(struct ui_on_pointer) {
        .node = track,
        .func = write_effort_on_pointer,
        .user = page,
    });

    for (uint32_t i = 0; i < write_effort_stops; i++) {
        double dot = 4.5;
        double cx = thumb * 0.5 + usable * (double)i / (double)(write_effort_stops - 1);
        bool is_max = i == write_effort_stops - 1;

        ui_ellipse(&(struct ui_ellipse) {
            .parent = track,
            .pos = ui_pos_xy(cx - dot * 0.5, 14 - dot * 0.5),
            .size = ui_size_fix(dot, dot),
            .fill = ui_fill_solid(is_max ? t->accent : t->track_dot),
        });
    }

    ui_frame(&(struct ui_frame) {
        .parent = track,
        .pos = ui_pos_xy(usable * page->effort_t, 4),
        .size = ui_size_fix(thumb, thumb),
        .corners = ui_corners_circular(thumb * 0.5),
        .fill = ui_fill_solid(t->thumb_fill),
        .outline = ui_outline_solid(ui_outline_align_inside, 1, t->thumb_ring),
    });

    page->effort_track_x = write_menu_x(page, def, size) + pad_x + thumb * 0.5;
    page->effort_track_usable = usable;
}

static void write_menu_overlay(struct write_page* page, const struct write_theme* t, struct ui_node root, double2 size, uint32_t menu) {
    static const struct write_row env_rows[] = {
        { .type = write_row_item, .icon = demo_icon_laptop, .label = "Local", .value = 0 },
        { .type = write_row_separator },
        { .type = write_row_header, .label = "Cloud" },
        { .type = write_row_item, .icon = demo_icon_plus, .label = "Add cloud environment...", .dim = true, .value = -1 },
        { .type = write_row_separator },
        { .type = write_row_header, .label = "Remote Control" },
        { .type = write_row_item, .icon = demo_icon_external, .label = "Set up Remote Control", .subtitle = "Run claude rc on your machine to code from here.", .value = -1 },
        { .type = write_row_separator },
        { .type = write_row_header, .label = "SSH" },
        { .type = write_row_item, .icon = demo_icon_plus, .label = "Add SSH host...", .dim = true, .value = -1 },
    };

    static const struct write_row mode_rows[] = {
        { .type = write_row_header, .label = "Mode" },
        { .type = write_row_item, .label = "Manual", .detail = "1", .value = 0 },
        { .type = write_row_item, .label = "Accept edits", .detail = "2", .value = 1 },
        { .type = write_row_item, .label = "Plan", .detail = "3", .value = 2 },
        { .type = write_row_item, .label = "Auto", .detail = "4", .value = 3 },
        { .type = write_row_item, .label = "Bypass permissions", .detail = "5", .value = 4 },
    };

    static const struct write_row model_rows[] = {
        { .type = write_row_header, .label = "Models" },
        { .type = write_row_item, .label = "Fable 5", .badge = "Included until July 19", .detail = "1", .value = 0 },
        { .type = write_row_item, .label = "Opus 4.8", .detail = "2", .value = 1 },
        { .type = write_row_item, .label = "Sonnet 5", .detail = "3", .value = 2 },
        { .type = write_row_item, .label = "Haiku 4.5", .detail = "4", .value = 3 },
        { .type = write_row_separator },
        { .type = write_row_item, .label = "More models", .chevron = true, .value = -1 },
    };

    static struct write_row project_rows[cr_count(write_projects) + 3];
    static struct write_row branch_rows[cr_count(write_branches)];

    switch (menu) {
    case write_menu_env: {
        struct write_menu_def def = { .id = write_menu_env, .width = 272, .rows = env_rows, .row_count = cr_count(env_rows), .checked = 0, .opens_up = true };
        write_menu_overlay_rows(page, t, root, &def, size);
    } break;
    case write_menu_project: {
        uint32_t n = 0;

        project_rows[n++] = (struct write_row) { .type = write_row_header, .label = "Recent" };
        for (uint32_t i = 0; i < cr_count(write_projects); i++) {
            project_rows[n++] = (struct write_row) { .type = write_row_item, .label = write_projects[i], .value = (int32_t)i };
        }
        project_rows[n++] = (struct write_row) { .type = write_row_separator };
        project_rows[n++] = (struct write_row) { .type = write_row_item, .label = "Open folder...", .value = -1 };

        struct write_menu_def def = { .id = write_menu_project, .width = 216, .rows = project_rows, .row_count = n, .checked = (int32_t)page->project, .opens_up = true };
        write_menu_overlay_rows(page, t, root, &def, size);
    } break;
    case write_menu_branch: {
        for (uint32_t i = 0; i < cr_count(write_branches); i++) {
            branch_rows[i] = (struct write_row) { .type = write_row_item, .label = write_branches[i], .value = (int32_t)i };
        }

        struct write_menu_def def = { .id = write_menu_branch, .width = 296, .rows = branch_rows, .row_count = cr_count(branch_rows), .checked = (int32_t)page->branch, .opens_up = true };
        write_menu_overlay_rows(page, t, root, &def, size);
    } break;
    case write_menu_mode: {
        struct write_menu_def def = { .id = write_menu_mode, .width = 212, .rows = mode_rows, .row_count = cr_count(mode_rows), .checked = (int32_t)page->mode };
        write_menu_overlay_rows(page, t, root, &def, size);
    } break;
    case write_menu_model: {
        struct write_menu_def def = { .id = write_menu_model, .width = 256, .rows = model_rows, .row_count = cr_count(model_rows), .checked = (int32_t)page->model };
        write_menu_overlay_rows(page, t, root, &def, size);
    } break;
    case write_menu_effort: {
        struct write_menu_def def = { .id = write_menu_effort, .width = 268 };
        write_effort_overlay(page, t, root, &def, size);
    } break;
    default:
        break;
    }
}

static void write_send_button(struct write_page* page, const struct write_theme* t, struct ui_node panel, double2 panel_size, bool has_text) {
    struct write_action* action = write_action(page, write_action_send, 0, -1);
    struct ui_node button = ui_node(panel, ui_key_auto);

    struct ui_tap_gesture tap = ui_tap(&(struct ui_tap) {
        .node = button,
        .on_change = write_on_tap,
        .user = action,
    });
    struct ui_hover_gesture hover = ui_hover(&(struct ui_hover) {
        .node = button,
    });

    struct ui_color fill = t->accent;
    struct ui_color arrow = t->on_accent;

    if (has_text == false) {
        fill = t->violet_fill;
        arrow = t->violet_fg;
    } else if (ui_tap_gesture_has_touch(tap) || ui_hover_gesture_has_hover(hover)) {
        fill = t->accent_hover;
    }

    ui_frame(&(struct ui_frame) {
        .node = button,
        .pos = ui_pos_xy(panel_size.x - 40, panel_size.y - 40),
        .size = ui_size_fix(32, 32),
        .corners = ui_corners_circular(16),
        .fill = ui_fill_solid(fill),
    });

    demo_icon_draw_send_arrow(button, arrow);
}

static double write_setting_chip(struct write_page* page, const struct write_theme* t, struct ui_node parent, uint32_t menu, const char* label, double2 anchor_pos) {
    double width = 12 + write_text_width(label) + 12;
    struct ui_node chip = write_chip_begin(page, t, parent, menu, (struct ui_pos) { 0 }, width, 5, 9, 9);

    write_chip_label(chip, label, page->open_menu == menu ? t->chip_fg_open : t->chip_fg);
    page->anchors[menu] = (struct write_anchor) { .pos = anchor_pos, .size = make_double2(width, 28) };

    return width;
}

static void* write_create(struct cr_mem* mem) {
    struct write_page* page = cr_malloc(mem, sizeof(*page), 0, cr_loc);

    memset(page, 0, sizeof(*page));
    page->mem = mem;
    page->input = ui_text_input_create(mem);
    page->mode = 4;
    page->worktree = true;
    page->effort_t = 4.0 / (double)(write_effort_stops - 1);
    page->editor_content_h = 20;

    return page;
}

static void write_destroy(void* state) {
    struct write_page* page = state;

    ui_text_input_destroy(page->input);
    cr_free(page->mem, page, cr_loc);
}

static void write_frame(void* state, const struct demo_frame* frame) {
    struct write_page* page = state;
    struct write_theme t = write_theme(frame->theme->is_dark);
    struct ui_node root = frame->root;
    double2 size = frame->size;

    page->action_count = 0;

    ui_frame(&(struct ui_frame) {
        .parent = root,
        .size = ui_size_fix(size.x, size.y),
        .fill = ui_fill_solid(t.bg),
    });

    double content_top = demo_title(frame, "Write", "compose a prompt, tune it with the chips");

    if (page->effort_dragging == false) {
        double stops = (double)(write_effort_stops - 1);
        double target = round(page->effort_t * stops) / stops;

        page->effort_t += (target - page->effort_t) * min_double(1.0, frame->delta_s * 16.0);
    }

    double avail = size.y - frame->keyboard_inset;
    double side_margin = size.x < 520 ? 32 : 48;
    double panel_w = round(max_double(min_double(write_panel_width_max, size.x - side_margin), 200.0));
    double max_editor = max_double(120, round(avail * 0.42));
    double editor_h = clamp_double(ceil(page->editor_content_h + 24), 92, max_editor);
    double2 panel_size = make_double2(panel_w, editor_h + write_controls_height);

    struct write_chips chips = write_chips_layout(page, panel_w);

    double total_h = chips.height + 12 + panel_size.y;
    double top = round(max_double(content_top, (avail - total_h) * 0.46));
    double2 panel_pos = make_double2(round((size.x - panel_w) * 0.5), top + chips.height + 12);

    write_violet_chips(page, &t, root, make_double2(panel_pos.x, top), &chips);

    struct ui_node panel = ui_frame(&(struct ui_frame) {
        .parent = root,
        .pos = ui_pos_xy(panel_pos.x, panel_pos.y),
        .size = ui_size_fix(panel_size.x, panel_size.y),
        .corners = write_corners(24),
        .fill.layers[0] = ui_layer_glass(ui_glass_variant_regular, t.glass),
    });

    struct ui_node editor = ui_frame(&(struct ui_frame) {
        .parent = panel,
        .size = ui_size_fix(panel_w, editor_h),
        .layout = ui_layout_stack_y(ui_dist_start, ui_align_start, 0, 0),
    });

    struct demo_field_style style = {
        .text = t.text_primary,
        .placeholder = t.placeholder,
        .accent = t.accent,
        .selection_alpha = t.selection_alpha,
    };

    struct ui_text_input_on_update_result editor_result = demo_field(page->input, root, editor, (struct ui_text_input_layout) {
        .width = panel_w,
        .min_height = editor_h,
        .max_height = editor_h,
        .pt_size = write_editor_pt,
        .weight = write_editor_weight,
        .padding = make_double4(16, 16, 14, 10),
        .multiline = true,
    }, "Start writing...", &style, frame->delta_s);

    page->editor_content_h = editor_result.content_height;

    struct ui_node controls = ui_frame(&(struct ui_frame) {
        .parent = panel,
        .pos = ui_pos_xy(0, editor_h),
        .size = ui_size_fix(panel_w, write_controls_height),
        .layout = ui_layout_stack_x(ui_dist_start, ui_align_start, 6, make_double4(12, 12, 8, 12)),
    });

    uint32_t effort_index = (uint32_t)round(page->effort_t * (double)(write_effort_stops - 1));
    double2 chip_pos = panel_pos + make_double2(12, editor_h + 8);

    chip_pos.x += write_setting_chip(page, &t, controls, write_menu_mode, write_modes[page->mode], chip_pos) + 6;
    chip_pos.x += write_setting_chip(page, &t, controls, write_menu_model, write_models[page->model], chip_pos) + 6;
    write_setting_chip(page, &t, controls, write_menu_effort, write_efforts[effort_index], chip_pos);

    write_send_button(page, &t, panel, panel_size, editor_result.text_size > 0);

    if (page->open_menu != write_menu_none) {
        struct write_action* action = write_action(page, write_action_close, 0, -1);

        struct ui_node catcher = ui_frame(&(struct ui_frame) {
            .parent = root,
            .size = ui_size_fix(size.x, size.y),
        });

        ui_tap(&(struct ui_tap) {
            .node = catcher,
            .on_change = write_on_tap,
            .user = action,
        });
        ui_on_scroll(&(struct ui_on_scroll) {
            .node = catcher,
            .func = write_noop_scroll,
        });

        write_menu_overlay(page, &t, root, size, page->open_menu);
    }
}

const struct demo_page demo_page_write = {
    .name = "Write",
    .create = write_create,
    .destroy = write_destroy,
    .frame = write_frame,
};
