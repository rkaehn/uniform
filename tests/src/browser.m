#include "tests_internal.h"
#include "app.h"
#include "gpu.h"
#include "hud.h"
#include "ui_gestures.h"
#include "ui_input.h"
#include "ui_util.h"
#include <stdio.h>
#include <string.h>

#define browser_scene_max 1024
#define browser_group_max 64

enum browser_panel {
    browser_panel_none,
    browser_panel_scenes,
    browser_panel_refs,
};

static const double _to_app_from_pt = cr_ios ? 1.0 : 0.85;

static struct {
    struct ui_test_scenes* scenes;
    struct ui_node prev_root;
    uint32_t scene;
    enum browser_panel panel;
    uint32_t panel_group;
    const char* groups[browser_group_max];
    uint32_t group_count;
} _browser;

#if cr_macos || cr_ios
static struct {
    struct tests_refs_dir_info dirs[64];
    uint32_t count;
    uint32_t gen;
    char armed[80];
    double armed_at_s;
} _refs;
#endif

static void groups_build(void) {
    if (_browser.group_count != 0) {
        return;
    }
    for (uint32_t i = 0; i < ui_test_scene_count_(_browser.scenes); i++) {
        const char* group = ui_test_scene_get_(_browser.scenes, i)->group;
        if (_browser.group_count == 0 || strcmp(_browser.groups[_browser.group_count - 1], group) != 0) {
            assert(_browser.group_count < browser_group_max);
            _browser.groups[_browser.group_count++] = group;
        }
    }
}

static uint32_t group_of_scene(uint32_t scene_index) {
    const char* group = ui_test_scene_get_(_browser.scenes, scene_index)->group;
    for (uint32_t g = 0; g < _browser.group_count; g++) {
        if (strcmp(_browser.groups[g], group) == 0) {
            return g;
        }
    }
    return 0;
}

static void group_on_tap(struct ui_tap_gesture gesture, void* user) {
    _browser.panel_group = (uint32_t)(uintptr_t)user;
}

static void scene_on_tap(struct ui_tap_gesture gesture, void* user) {
    _browser.scene = (uint32_t)(uintptr_t)user;
    _browser.panel = browser_panel_none;
}

static void panel_button(struct ui_node parent, void (*on_tap)(struct ui_tap_gesture gesture, void* user), void* user,
                         const char* text, struct ui_color fill, struct ui_color text_color, double width, double height, double pt_size) {
    struct ui_node cell = ui_frame(&(struct ui_frame) {
        .parent = parent,
        .size = ui_size_fix(width, height),
        .corners = ui_corners(4),
        .fill = ui_fill_solid(fill),
        .layout = ui_layout_stack_y(ui_dist_center, ui_align_center, 0, 0),
    });

    ui_tap(&(struct ui_tap) {
        .node = cell,
        .on_change = on_tap,
        .user = user,
    });

    ui_text(&(struct ui_text) {
        .parent = cell,
        .str = ui_str(text, text_color, pt_size),
    });
}

static void panel_cell(struct ui_node parent, void (*on_tap)(struct ui_tap_gesture gesture, void* user), uint32_t index,
                       const char* text, bool selected, const struct hud_theme* theme, double width, double height, double pt_size) {
    panel_button(parent, on_tap, (void*)(uintptr_t)index, text,
                 selected ? theme->blue : theme->background_secondary,
                 selected ? ui_white : theme->label_primary,
                 width, height, pt_size);
}

static void panel_text(struct ui_node parent, const char* text, struct ui_color color, double width, double pt_size) {
    struct ui_node frame = ui_frame(&(struct ui_frame) {
        .parent = parent,
        .size.width = width > 0.0 ? ui_size_axis_fix(width) : ui_size_axis_fit(),
        .layout = ui_layout_stack_x(ui_dist_start, ui_align_center, 0, 0),
    });

    ui_text(&(struct ui_text) {
        .parent = frame,
        .str = ui_str(text, color, pt_size),
    });
}

static struct ui_node panel_begin(struct ui_node root, double2 root_size, const struct hud_theme* theme, double2* out_avail) {
    struct ui_node container = ui_frame(&(struct ui_frame) {
        .parent = root,
        .size = ui_size(ui_size_axis_width(), ui_size_axis_height()),
    });

    double margin = 24;
    double top = 128;

    if (out_avail) {
        out_avail->x = max_double(root_size.x - 360 - 3 * margin, 320);
        out_avail->y = max_double(root_size.y - top - margin, 240);
    }

    return ui_frame(&(struct ui_frame) {
        .parent = container,
        .pos = ui_pos_xy(margin, top),
        .size = ui_size_fit(),
        .corners = ui_corners(10),
        .fill = ui_fill_solid(theme->background_primary),
        .layout = ui_layout_stack_y(ui_dist_start, ui_align_start, 10, 14),
    });
}

static void scene_panel(struct ui_node root, double2 root_size, const struct hud_theme* theme) {
    double2 avail;
    struct ui_node panel = panel_begin(root, root_size, theme, &avail);

    struct ui_node split = ui_frame(&(struct ui_frame) {
        .parent = panel,
        .size = ui_size_fit(),
        .layout = ui_layout_stack_x(ui_dist_start, ui_align_start, 14, 0),
    });

    double group_w = 1;
    for (uint32_t g = 0; g < _browser.group_count; g++) {
        group_w = max_double(group_w, (double)strlen(_browser.groups[g]) * 7.9 + 60.0);
    }

    struct ui_node group_col = ui_frame(&(struct ui_frame) {
        .parent = split,
        .size = ui_size_fit(),
        .layout = ui_layout_stack_y(ui_dist_start, ui_align_start, 3, 0),
    });

    uint32_t scene_count = ui_test_scene_count_(_browser.scenes);

    for (uint32_t g = 0; g < _browser.group_count; g++) {
        uint32_t count = 0;
        for (uint32_t i = 0; i < scene_count; i++) {
            count += strcmp(ui_test_scene_get_(_browser.scenes, i)->group, _browser.groups[g]) == 0 ? 1 : 0;
        }
        char label[48];
        snprintf(label, sizeof(label), "%s  %u", _browser.groups[g], count);
        panel_cell(group_col, group_on_tap, g, label, g == _browser.panel_group, theme, group_w, 24, 12);
    }

    uint32_t shown[browser_scene_max];
    uint32_t shown_count = 0;
    uint32_t max_label = 1;

    for (uint32_t i = 0; i < scene_count; i++) {
        const struct ui_test_scene* scene = ui_test_scene_get_(_browser.scenes, i);
        if (strcmp(scene->group, _browser.groups[_browser.panel_group]) != 0) {
            continue;
        }
        assert(shown_count < browser_scene_max);
        shown[shown_count++] = i;
        max_label = max_uint32(max_label, (uint32_t)strlen(scene->label));
    }

    double cell_h = 24;
    double grid_spacing = 5;
    double content_w = max_double(avail.x - group_w - 70.0, 320.0);

    double cell_w = min_double((double)max_label * 7.9 + 16.0, 320.0);
    uint32_t cols = max_uint32((uint32_t)((content_w + grid_spacing) / (cell_w + grid_spacing)), 1);
    cols = min_uint32(cols, shown_count);
    uint32_t rows = (shown_count + cols - 1) / cols;
    uint32_t max_rows = max_uint32((uint32_t)(avail.y * 0.92 / (cell_h + grid_spacing)), 4);
    if (rows > max_rows) {
        cols = (shown_count + max_rows - 1) / max_rows;
        rows = (shown_count + cols - 1) / cols;
        cell_w = min_double(cell_w, content_w / (double)cols - grid_spacing);
    }

    struct ui_node grid = ui_frame(&(struct ui_frame) {
        .parent = split,
        .size = ui_size_fit(),
        .layout = ui_layout_stack_y(ui_dist_start, ui_align_start, grid_spacing, 0),
    });

    for (uint32_t r = 0; r < rows; r++) {
        struct ui_node row = ui_frame(&(struct ui_frame) {
            .parent = grid,
            .size = ui_size_fit(),
            .layout = ui_layout_stack_x(ui_dist_start, ui_align_start, grid_spacing, 0),
        });

        for (uint32_t k = r * cols; k < min_uint32((r + 1) * cols, shown_count); k++) {
            uint32_t scene_index = shown[k];
            const char* label = ui_test_scene_get_(_browser.scenes, scene_index)->label;
            panel_cell(row, scene_on_tap, scene_index, label, scene_index == _browser.scene, theme, cell_w, cell_h, 12);
        }
    }
}

#if cr_macos || cr_ios

static void refs_refresh(void) {
    _refs.count = tests_refs_dir_list_(_refs.dirs, cr_count(_refs.dirs));
}

static void refs_delete_on_tap(struct ui_tap_gesture gesture, void* user) {
    char name[80];
    snprintf(name, sizeof(name), "%s", ((const struct tests_refs_dir_info*)user)->name);

    if (strcmp(_refs.armed, name) == 0) {
        tests_refs_dir_delete_(name);
        _refs.armed[0] = 0;
        _refs.gen++;
        refs_refresh();
    } else {
        snprintf(_refs.armed, sizeof(_refs.armed), "%s", name);
        _refs.armed_at_s = cr_get_current_time();
    }
}

static void refs_panel(struct ui_node root, double2 root_size, const struct hud_theme* theme) {
    if (_refs.armed[0] != 0 && cr_get_current_time() - _refs.armed_at_s > 4.0) {
        _refs.armed[0] = 0;
    }

    struct ui_node panel = panel_begin(root, root_size, theme, NULL);

    panel_text(panel, "reference folders", theme->label_primary, 0, 15);

    if (_refs.count == 0) {
        panel_text(panel, "none found in Documents/refs", theme->label_primary, 0, 13);
    }

    double name_w = 1;
    for (uint32_t i = 0; i < _refs.count; i++) {
        name_w = max_double(name_w, (double)strlen(_refs.dirs[i].name) * 7.9 + 8.0);
    }

    for (uint32_t i = 0; i < _refs.count; i++) {
        struct tests_refs_dir_info* info = &_refs.dirs[i];

        struct ui_node row = ui_frame(&(struct ui_frame) {
            .parent = panel,
            .key = ui_key_str(info->name),
            .size = ui_size_fit(),
            .layout = ui_layout_stack_x(ui_dist_start, ui_align_center, 10, 0),
        });

        panel_text(row, info->name, theme->label_primary, name_w, 13);

        char text[32];
        snprintf(text, sizeof(text), "%u items", info->item_count);
        panel_text(row, text, theme->label_primary, 84, 13);

        if (info->size_bytes >= 1000000000ull) {
            snprintf(text, sizeof(text), "%.1f GB", (double)info->size_bytes / 1e9);
        } else {
            snprintf(text, sizeof(text), "%.1f MB", (double)info->size_bytes / 1e6);
        }
        panel_text(row, text, theme->label_primary, 84, 13);

        bool armed = strcmp(_refs.armed, info->name) == 0;
        panel_button(row, refs_delete_on_tap, info,
                     armed ? "sure?" : "delete",
                     armed ? ui_hex(0xd63a3a) : theme->background_secondary,
                     armed ? ui_white : theme->label_primary,
                     70, 24, 12);
    }
}

static bool reference_offered(void) {
    static uint32_t _scene = uint32_max;
    static uint32_t _gen = uint32_max;
    static bool _offered;
    if (_scene != _browser.scene || _gen != _refs.gen) {
        _offered = tests_refs_has_(_browser.scenes, _browser.scene);
        _scene = _browser.scene;
        _gen = _refs.gen;
    }
    return _offered;
}

static bool reference_graphic(struct ui_graphic** out) {
    static uint32_t _scene = uint32_max;
    static uint32_t _gen = uint32_max;
    static bool _available;
    static struct ui_graphic* _graphic;
    if (_scene != _browser.scene || _gen != _refs.gen) {
        if (_available) {
            ui_graphic_destroy(_graphic);
        }
        _available = tests_refs_load_graphic_(_browser.scenes, _browser.scene, &_graphic);
        _scene = _browser.scene;
        _gen = _refs.gen;
    }
    *out = _graphic;
    return _available;
}

#endif

static struct ui_graphic* rendered_scene_graphic(struct gpu_device* device, const struct ui_test_scene* scene, double frame_to_px_from_pt) {
    static uint32_t _scene = uint32_max;
    static struct ui_graphic* _graphic;
    static struct ui_test_scene_state* _state;
    if (_scene == _browser.scene) {
        return _graphic;
    }
    if (_scene != uint32_max) {
        ui_graphic_destroy(_graphic);
        _state->destroy(_state);
    }
    _state = ui_test_scene_create_(_browser.scenes, cr_mem_libc, scene);

    struct gpu_texture* texture = gpu_texture_create(device, &(struct gpu_texture_desc) {
        .label = "browser scene",
        .format = gpu_format_rgba16f,
        .size = { scene->width, scene->height },
        .usage = gpu_texture_usage_render_target | gpu_texture_usage_sample,
    });

    double2 size = make_double2(scene->width / (double)scene->to_px_from_pt, scene->height / (double)scene->to_px_from_pt);
    ui_set_to_px_from_pt(scene->to_px_from_pt);
    struct ui_node root = scene->build(_state, ui_node_null, size, scene);
    ui_layout(root);

    struct ui_render_desc desc = {
        .output_color_space = scene->output_color_space,
        .compositing_format = ui_pixel_format_rgba16f,
        .premultiplied = true,
    };
    struct gpu_command_buffer* command_buffer = gpu_command_buffer_create(device, "browser scene");
    ui_render(root, command_buffer, texture, desc);
    ui_set_to_px_from_pt(frame_to_px_from_pt);
    gpu_command_buffer_commit(command_buffer);
    ui_release(root);

    _graphic = ui_graphic_create_texture(texture);
    _scene = _browser.scene;
    return _graphic;
}

static struct ui_fill graphic_fill(struct ui_graphic* graphic, enum ui_color_space color_space) {
    return (struct ui_fill) {
        .layers[0] = ui_layer_graphic_ex(graphic, ui_address_mode_fill, ui_filter_linear, make_double2(1.0, 1.0), ui_white, color_space, true),
    };
}

void tests_browser_frame_(struct app_window* window, struct ui_test_scenes* scenes) {
    _browser.scenes = scenes;

    struct gpu_device* device = app_get_device(app_window_get_app(window));
    struct app_target_desc target_desc = app_window_get_target_desc(window);
    double timestamp = app_window_get_timestamp(window);

    double delta_s = app_window_get_delta(window);

    hud_frame_begin(timestamp);

    double to_px_from_pt = target_desc.to_px_from_pt * _to_app_from_pt;

    ui_util_window_sync(window, _browser.prev_root, _to_app_from_pt);

    groups_build();

    bool is_dark = app_window_get_is_dark(window);
    double2 root_size = make_double2(target_desc.px_size.x / to_px_from_pt, target_desc.px_size.y / to_px_from_pt);

    struct ui_node root = ui_frame(&(struct ui_frame) {
        .size = ui_size_fix(root_size.x, root_size.y),
        .layout = ui_layout_stack_x(ui_dist_center, ui_align_center, 20, 0),
        .fill = ui_fill_solid(is_dark ? ui_gray10 : ui_gray80),
    });

    if (hud_button("Scene", ui_test_scene_get_(_browser.scenes, _browser.scene)->label)) {
        _browser.panel = _browser.panel == browser_panel_scenes ? browser_panel_none : browser_panel_scenes;
        _browser.panel_group = group_of_scene(_browser.scene);
    }
    _browser.scene = hud_stepper("Step", ui_test_scene_count_(_browser.scenes), _browser.scene);
    const struct ui_test_scene* scene = ui_test_scene_get_(_browser.scenes, _browser.scene);
    hud_label("Group", scene->group);

#if cr_macos || cr_ios
    if (hud_button("Refs", tests_refs_name_())) {
        _browser.panel = _browser.panel == browser_panel_refs ? browser_panel_none : browser_panel_refs;
        refs_refresh();
    }
#endif

    app_window_set_target_format(window, (struct app_target_format) {
        .color_space = target_desc.format.color_space,
        .pixel_format = app_pixel_format_rgba16f,
        .sampleable = false,
    });

    bool show_reference = false;
#if cr_macos || cr_ios
    if (reference_offered()) {
        show_reference = hud_toggle("Reference", false);
    }
#endif
    bool fill_screen = hud_toggle("Fill screen", false);

    double2 scene_size = make_double2(scene->width / (double)scene->to_px_from_pt, scene->height / (double)scene->to_px_from_pt);
    double2 display_size = fill_screen
        ? aspect_fit_double2(scene_size, root_size)
        : make_double2(scene->width / to_px_from_pt, scene->height / to_px_from_pt);

    struct ui_fill display_fill = ui_fill_solid(ui_rgba(0.0f, 1.0f, 1.0f, 1.0f));
    if (show_reference) {
#if cr_macos || cr_ios
        struct ui_graphic* reference;
        if (reference_graphic(&reference)) {
            display_fill = graphic_fill(reference, scene->output_color_space);
        }
#endif
    } else {
        display_fill = graphic_fill(rendered_scene_graphic(device, scene, to_px_from_pt), scene->output_color_space);
    }
    ui_frame(&(struct ui_frame) {
        .parent = root,
        .size = ui_size_fix(display_size.x, display_size.y),
        .fill = display_fill,
    });

    hud_on_update(root, is_dark, delta_s);

    struct hud_theme theme = hud_theme(is_dark);
    if (_browser.panel == browser_panel_scenes) {
        scene_panel(root, root_size, &theme);
    }
#if cr_macos || cr_ios
    if (_browser.panel == browser_panel_refs) {
        refs_panel(root, root_size, &theme);
    }
#endif

    ui_layout(root);

    ui_release(_browser.prev_root);
    _browser.prev_root = root;

    struct ui_render_desc render_desc = {
        .output_color_space = ui_color_space_from_app(target_desc.format.color_space),
        .compositing_format = ui_pixel_format_rgba16f,
        .premultiplied = true,
    };

    struct gpu_command_buffer* command_buffer = gpu_command_buffer_create(device, "browser frame");
    hud_frame_profile(command_buffer);
    struct gpu_texture* target = app_window_acquire_target(window);
    ui_render(root, command_buffer, target, render_desc);
    app_window_present(window, command_buffer, target);

    hud_frame_end();
}
