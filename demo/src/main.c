#include "demo.h"
#include "app.h"
#include "gpu.h"
#include "hud.h"
#include "log.h"
#include "ui_input.h"
#include "ui_util.h"
#include <stdio.h>

static const struct demo_page* const demo_pages[] = {
    &demo_page_glass,
    &demo_page_canvas,
    &demo_page_collection,
    &demo_page_tree,
    &demo_page_circle,
    &demo_page_text,
    &demo_page_write,
};

#define demo_page_count ((uint32_t)cr_count(demo_pages))

static const double _to_app_from_pt = cr_ios ? 1.0 : 0.85;

static const char* demo_page_labels[cr_count(demo_pages)];

struct demo {
    struct cr_mem* mem;
    struct ui_font* font;
    struct ui_font* font_mono;
    struct ui_node prev_root;
    uint32_t shown_page;
    void* states[cr_count(demo_pages)];
};

static void demo_on_update(struct app_window* window) {
    struct demo* demo = app_window_get_user(window);
    struct gpu_device* device = app_get_device(app_window_get_app(window));
    struct app_target_desc target_desc = app_window_get_target_desc(window);
    double timestamp = app_window_get_timestamp(window);
    double delta_s = app_window_get_delta(window);
    bool is_dark = app_window_get_is_dark(window);

    hud_frame_begin(timestamp);

    double2 size = make_double2(target_desc.px_size.x, target_desc.px_size.y) / (target_desc.to_px_from_pt * _to_app_from_pt);

    ui_util_window_sync(window, demo->prev_root, _to_app_from_pt);

    uint32_t page_index = hud_select_labels("Demo", demo_page_count, 0, demo_page_labels);
    bool target_16f = target_desc.format.pixel_format == app_pixel_format_rgba16f;
    bool compositing_8bit = hud_toggle("8-bit compositing", target_16f == false);
    struct app_target_format format = target_desc.format;
    format.pixel_format = hud_toggle("8-bit target", target_16f == false) ? app_pixel_format_bgra8 : app_pixel_format_rgba16f;
    format.sampleable = hud_toggle("Sampleable", target_desc.format.sampleable);
    app_window_set_target_format(window, format);
    ui_input_set_palm_radius(hud_slider("Palm radius", 0.0, 200.0, ui_input_get_palm_radius(), hud_slider_display_mode_float));
    const struct demo_page* page = demo_pages[page_index];
    struct demo_theme theme = demo_theme(is_dark);
    bool appeared = demo->shown_page != page_index;

    demo->shown_page = page_index;

    if (demo->states[page_index] == NULL) {
        demo->states[page_index] = page->create(demo->mem);
    }

    struct ui_node root = ui_frame(&(struct ui_frame) {
        .size = ui_size_fix(size.x, size.y),
        .fill = ui_fill_solid(theme.bg),
    });

    struct ui_node page_root = ui_frame(&(struct ui_frame) {
        .parent = root,
        .key = ui_key_str(page->name),
        .size = ui_size_fix(size.x, size.y),
        .clip = true,
    });

    struct demo_frame frame = {
        .root = page_root,
        .size = size,
        .keyboard_inset = app_window_get_keyboard_inset(window) / _to_app_from_pt,
        .delta_s = delta_s,
        .appeared = appeared,
        .theme = &theme,
        .font_mono = demo->font_mono,
    };

    page->frame(demo->states[page_index], &frame);

    struct ui_render_desc render_desc = {
        .output_color_space = ui_color_space_from_app(target_desc.format.color_space),
        .compositing_format = compositing_8bit ? ui_pixel_format_bgra8 : ui_pixel_format_rgba16f,
        .premultiplied = true,
        .dither = target_16f == false,
    };

    hud_frame_label(ui_render_direct_to_target(render_desc, ui_pixel_format_from_app(target_desc.format.pixel_format), target_desc.format.sampleable) ? "Direct" : "Final");
    hud_on_update(root, is_dark, delta_s);

    ui_layout(root);

    ui_release(demo->prev_root);
    demo->prev_root = root;

    struct gpu_command_buffer* command_buffer = gpu_command_buffer_create(device, "demo frame");
    hud_frame_profile(command_buffer);
    struct gpu_texture* target = app_window_acquire_target(window);
    ui_render(root, command_buffer, target, render_desc);
    app_window_present(window, command_buffer, target);

    hud_frame_end();
}

static void demo_on_close_request(struct app_window* window) {
    app_quit(app_window_get_app(window));
}

static bool demo_text_input_hit_test(double2 pos, void* user) {
    return ui_input_has_focusable_at(pos);
}

static void demo_on_start(struct app* app) {
    struct demo* demo = app_get_user(app);
    struct gpu_device* device = app_get_device(app);

    ui_init(demo->mem, device);
    ui_input_set(ui_input_create(demo->mem));
    demo->font = ui_font_create(demo_font_data, demo_font_data_size);
    demo->font_mono = ui_font_create(demo_font_mono_data, demo_font_mono_data_size);
    ui_set_default_font(demo->font);
    hud_set(hud_create(demo->mem, device, demo->font_mono));
    hud_set_enabled(true);

    struct app_window* window = app_window_create(app, &(struct app_window_desc) {
        .user = demo,
        .on_update = demo_on_update,
        .on_close_request = demo_on_close_request,
        .title = "demo",
        .size = make_double2(1280.0, 800.0),
        .center = true,
        .resizable = true,
    });

    app_window_text_input_set_hit_test(window, demo_text_input_hit_test, NULL);
}

static void demo_on_quit(struct app* app) {
    struct demo* demo = app_get_user(app);

    for (uint32_t i = 0; i < demo_page_count; i++) {
        if (demo->states[i]) {
            demo_pages[i]->destroy(demo->states[i]);
        }
    }

    ui_release(demo->prev_root);
    hud_destroy(hud_get());
    ui_input_destroy(ui_input_get());
    ui_font_destroy(demo->font_mono);
    ui_font_destroy(demo->font);
    ui_deinit();
}

int main(int argc, char** argv) {
    setbuf(stdout, NULL);

    static struct demo demo;

    demo.mem = cr_mem_libc;
    demo.shown_page = uint32_max;

    for (uint32_t i = 0; i < demo_page_count; i++) {
        demo_page_labels[i] = demo_pages[i]->name;
    }

    log_init(NULL);
    log_set_mode(log_mode_term);

    app_run(&(struct app_desc) {
        .mem = demo.mem,
        .argc = argc,
        .argv = argv,
        .user = &demo,
        .on_start = demo_on_start,
        .on_quit = demo_on_quit,
    });

    return 0;
}
