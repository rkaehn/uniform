#include "app.h"
#include "gpu.h"
#include "ui.h"
#include "ui_gestures.h"
#include "ui_input.h"
#include "ui_util.h"
#include <stdio.h>

cr_warning_push
cr_warning_ignore_everything
static const uint8_t simple_font[] = {
#embed "../../assets/JetBrainsMono.ttf"
};
cr_warning_pop

struct context {
    struct ui_font* font;
    struct ui_node prev_root;
};

static void simple_on_tap(struct ui_tap_gesture gesture, void* user) {
    if (ui_tap_gesture_get_phase(gesture) == ui_gesture_phase_recognized) {
        printf("tap\n");
    }
}

static void simple_on_update(struct app_window* window) {
    struct context* context = app_window_get_user(window);
    struct gpu_device* device = app_get_device(app_window_get_app(window));
    struct app_target_desc target_desc = app_window_get_target_desc(window);
    bool is_dark = app_window_get_is_dark(window);

    ui_util_window_sync(window, context->prev_root, 1.0);

    double2 size = target_desc.pt_size;
    struct ui_color bg = is_dark ? ui_black : ui_white;
    struct ui_color fg = is_dark ? ui_white : ui_black;

    struct ui_node root = ui_frame(&(struct ui_frame) {
        .size = ui_size_fix(size.x, size.y),
        .fill = ui_fill_solid(bg),
    });

    struct ui_node node = ui_node(root, ui_key_auto);

    struct ui_tap_gesture tap = ui_tap(&(struct ui_tap) {
        .node = node,
        .on_change = simple_on_tap,
    });
    struct ui_hover_gesture hover = ui_hover(&(struct ui_hover) {
        .node = node,
    });

    struct ui_color fill = fg;
    if (ui_tap_gesture_has_touch(tap)) {
        fill = ui_red;
    } else if (ui_hover_gesture_has_hover(hover)) {
        fill = ui_gray50;
    }

    ui_frame(&(struct ui_frame) {
        .node = node,
        .pos = ui_pos_center(),
        .layout = ui_layout_stack_x(ui_dist_center, ui_align_center, 0, ui_pad_xy(16, 8)),
        .fill = ui_fill_solid(fill),
    });

    ui_text(&(struct ui_text) {
        .parent = node,
        .str = ui_str("Button", bg, 13),
    });

    ui_layout(root);
    ui_release(context->prev_root);
    context->prev_root = root;

    struct gpu_command_buffer* command_buffer = gpu_command_buffer_create(device, "simple");
    struct gpu_texture* target = app_window_acquire_target(window);
    ui_render(root, command_buffer, target, ui_util_window_render_desc(window));
    app_window_present(window, command_buffer, target);
}

static void simple_on_close_request(struct app_window* window) {
    app_quit(app_window_get_app(window));
}

static void simple_on_start(struct app* app) {
    struct context* context = app_get_user(app);

    ui_init(cr_mem_libc, app_get_device(app));
    ui_input_set(ui_input_create(cr_mem_libc));
    context->font = ui_font_create(simple_font, sizeof(simple_font));
    ui_set_default_font(context->font);

    app_window_create(app, &(struct app_window_desc) {
        .user = context,
        .on_update = simple_on_update,
        .on_close_request = simple_on_close_request,
        .title = "simple",
        .size = make_double2(480.0, 320.0),
        .center = true,
        .resizable = true,
    });
}

static void simple_on_quit(struct app* app) {
    struct context* context = app_get_user(app);

    ui_release(context->prev_root);
    ui_font_destroy(context->font);
    ui_input_destroy(ui_input_get());
    ui_deinit();
}

int main(int argc, char** argv) {
    setbuf(stdout, NULL);

    static struct context context;

    app_run(&(struct app_desc) {
        .mem = cr_mem_libc,
        .argc = argc,
        .argv = argv,
        .user = &context,
        .on_start = simple_on_start,
        .on_quit = simple_on_quit,
    });

    return 0;
}
