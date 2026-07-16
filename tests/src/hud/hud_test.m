#include "ui_tests.h"
#include "hud_tests.h"
#include "hud.h"
#include "log.h"
#include <stdio.h>


static void ui_test_check(bool ok, const char* label) {
    ui_test_group_check_("hud", ok, label);
}

static void ui_test_hud_frame(struct ui_node* prev_root) {
    ui_input_dispatch(*prev_root, 0.0, NULL, 0, 1.0);

    struct ui_node root = ui_frame(&(struct ui_frame) {
        .size = ui_size_fix(800, 600),
    });

    hud_on_update(root, true, 1.0 / 60.0);
    ui_layout(root);

    ui_release(*prev_root);
    *prev_root = root;
}

static void ui_test_hud_config_refresh(void) {
    struct ui_node prev_root = ui_node_null;

    double v1 = hud_slider("runtime cfg slider", 0.0f, 100.0f, 25.0f, hud_slider_display_mode_float);
    ui_test_hud_frame(&prev_root);
    double v2 = hud_slider("runtime cfg slider", 0.0f, 200.0f, 25.0f, hud_slider_display_mode_float);
    ui_test_hud_frame(&prev_root);
    ui_test_check(fabs(v1 - 25.0) < 0.01f, "slider initial value");
    ui_test_check(fabs(v2 - 50.0) < 0.01f, "slider range refresh");

    uint32_t s1 = hud_select("runtime cfg select", 5, 4);
    ui_test_hud_frame(&prev_root);
    uint32_t s2 = hud_select("runtime cfg select", 3, 0);
    ui_test_hud_frame(&prev_root);
    ui_test_check(s1 == 4, "select initial option");
    ui_test_check(s2 == 0, "select clamps stale option");

    ui_release(prev_root);
}

static struct {
    struct ui_font* font;
    struct ui_input* prev_input;
    uint8_t* raw;
    uint32_t after_first;
} ui_test_hud_log_;

static void ui_test_hud_log_cycle(struct ui_font* font) {
    log_init(NULL);
    log_s("hud log line one\nhud log line two");
    log_set_pref(log_pref_is_visible, 1);

    struct ui* ui = ui_ctx();
    struct hud* prev_hud = hud_get();
    struct hud* hud = hud_create(cr_mem_libc, ui->device, font);
    hud_set(hud);

    struct ui_node prev_root = ui_node_null;

    for (uint32_t i = 0; i < 3; i++) {
        ui_input_dispatch(prev_root, 0.0, NULL, 0, 1.0);

        struct ui_node root = ui_frame(&(struct ui_frame) {
            .size = ui_size_fix(800, 600),
        });

        hud_on_update(root, true, 1.0 / 60.0);
        ui_layout(root);

        if (i == 2) {
            struct gpu_texture* target = gpu_texture_create(ui->device, &(struct gpu_texture_desc) {
                .label = "test hud target",
                .format = gpu_format_bgra8,
                .size = { 800, 600 },
                .usage = gpu_texture_usage_render_target | gpu_texture_usage_read,
            });
            struct gpu_command_buffer* command_buffer = gpu_command_buffer_create(ui->device, "test hud");

            ui_render(root, command_buffer, target, (struct ui_render_desc) { 0 });

            ui_test_hud_log_.raw = cr_malloc(cr_mem_libc, 800 * 600 * 4, 0, cr_loc);
            gpu_command_buffer_read_texture(command_buffer, target, ui_test_hud_log_.raw, 800 * 4);
            ui_test_await_(gpu_command_buffer_commit(command_buffer));

            gpu_texture_destroy(ui->device, target);
        }

        if (ui_node_is_null(prev_root) == false) {
            ui_release(prev_root);
        }
        prev_root = root;
    }

    ui_release(prev_root);
    ui_input_dispatch(ui_node_null, 0.0, NULL, 0, 1.0);

    hud_destroy(hud);
    hud_set(prev_hud);
    log_deinit();
}

static void ui_test_hud_log_second(void* args) {
    cr_free(cr_mem_libc, ui_test_hud_log_.raw, cr_loc);
    ui_reset_slots(ui_ctx());
    uint32_t after_second = cr_rec_get_alloc_count();

    ui_input_destroy(ui_input_get());
    ui_input_set(ui_test_hud_log_.prev_input);

    ui_test_check(ui_test_hud_log_.after_first == after_second, "log view cycle alloc steady");
}

static void ui_test_hud_log_first(void* args) {
    struct ui* ui = ui_ctx();
    const uint8_t* raw = ui_test_hud_log_.raw;

    uint32_t text_bright = 0;
    for (uint32_t y = 536; y < 600; y++) {
        for (uint32_t x = 0; x < 720; x++) {
            text_bright += raw[(y * 800 + x) * 4 + 1] > 40 ? 1 : 0;
        }
    }

    uint32_t empty_bright = 0;
    for (uint32_t y = 200; y < 400; y++) {
        for (uint32_t x = 100; x < 700; x++) {
            empty_bright += raw[(y * 800 + x) * 4 + 1] > 40 ? 1 : 0;
        }
    }

    cr_free(cr_mem_libc, ui_test_hud_log_.raw, cr_loc);

    ui_test_check(text_bright > 50, "log view draws glyphs");
    ui_test_check(empty_bright == 0, "log view empty area stays clear");

    ui_reset_slots(ui);
    ui_test_hud_log_.after_first = cr_rec_get_alloc_count();

    ui_test_hud_log_cycle(ui_test_hud_log_.font);
    ui_test_then_(ui_test_hud_log_second, NULL);
}

static void ui_test_hud_log_view(struct ui_font* font) {
    ui_test_hud_log_.font = font;
    ui_test_hud_log_.prev_input = ui_input_get();
    ui_input_set(ui_input_create(cr_mem_libc));

    ui_test_hud_log_cycle(font);
    ui_test_then_(ui_test_hud_log_first, NULL);
}

void hud_test(struct ui_font* font) {

    ui_test_hud_config_refresh();
    ui_test_hud_log_view(font);

}

