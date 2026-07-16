#include "app.h"
#include "gpu.h"
#include "ui.h"
#include "ui_input.h"
#include "ui_util.h"
#include <stdio.h>

extern const unsigned char* const triangle_shaders_embed_data[];
extern const unsigned long long triangle_shaders_embed_sizes[];

#define triangle_texture_size 512u

struct context {
    struct ui_node prev_root;
    struct gpu_render_pipeline* pipeline;
    struct gpu_texture* texture;
    struct ui_graphic* graphic;
};

static void triangle_on_update(struct app_window* window) {
    struct context* context = app_window_get_user(window);
    struct gpu_device* device = app_get_device(app_window_get_app(window));
    struct app_target_desc target_desc = app_window_get_target_desc(window);
    bool is_dark = app_window_get_is_dark(window);

    ui_util_window_sync(window, context->prev_root, 1.0);

    double2 size = target_desc.pt_size;

    struct ui_node root = ui_frame(&(struct ui_frame) {
        .size = ui_size_fix(size.x, size.y),
        .layout = ui_layout_stack_y(ui_dist_center, ui_align_center, 0, 0),
        .fill = ui_fill_solid(is_dark ? ui_black : ui_white),
    });

    ui_frame(&(struct ui_frame) {
        .parent = root,
        .size = ui_size_fix(256.0, 256.0),
        .fill = ui_fill_graphic(context->graphic, ui_address_mode_fill, ui_white),
    });

    ui_layout(root);
    ui_release(context->prev_root);
    context->prev_root = root;

    struct gpu_command_buffer* command_buffer = gpu_command_buffer_create(device, "triangle");

    struct gpu_render_pass* pass = gpu_command_buffer_begin_render(command_buffer, context->texture, gpu_load_clear, "triangle");
    gpu_render_pass_set_pipeline(pass, context->pipeline);
    gpu_render_pass_draw(pass, 0, 3);
    gpu_render_pass_end(pass);

    struct gpu_texture* target = app_window_acquire_target(window);
    ui_render(root, command_buffer, target, ui_util_window_render_desc(window));
    app_window_present(window, command_buffer, target);
}

static void triangle_on_close_request(struct app_window* window) {
    app_quit(app_window_get_app(window));
}

static void triangle_on_start(struct app* app) {
    struct context* context = app_get_user(app);
    struct gpu_device* device = app_get_device(app);

    ui_init(cr_mem_libc, device);
    ui_input_set(ui_input_create(cr_mem_libc));

    struct gpu_library* library = gpu_library_create(device, triangle_shaders_embed_data[0], triangle_shaders_embed_sizes[0], "triangle");
    context->pipeline = gpu_render_pipeline_create(device, &(struct gpu_render_pipeline_desc) {
        .library = library,
        .vertex_name = "triangle_v",
        .fragment_name = "triangle_f",
        .color_format = gpu_format_bgra8,
    });
    gpu_library_destroy(device, library);

    context->texture = gpu_texture_create(device, &(struct gpu_texture_desc) {
        .label = "triangle",
        .format = gpu_format_bgra8,
        .size = make_uint2(triangle_texture_size, triangle_texture_size),
        .usage = gpu_texture_usage_render_target | gpu_texture_usage_sample,
    });

    context->graphic = ui_graphic_create_texture(context->texture);

    app_window_create(app, &(struct app_window_desc) {
        .user = context,
        .on_update = triangle_on_update,
        .on_close_request = triangle_on_close_request,
        .title = "triangle",
        .size = make_double2(640.0, 480.0),
        .center = true,
        .resizable = true,
    });
}

static void triangle_on_quit(struct app* app) {
    struct context* context = app_get_user(app);
    struct gpu_device* device = app_get_device(app);

    ui_release(context->prev_root);
    ui_graphic_destroy(context->graphic);
    gpu_render_pipeline_destroy(device, context->pipeline);
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
        .on_start = triangle_on_start,
        .on_quit = triangle_on_quit,
    });

    return 0;
}
