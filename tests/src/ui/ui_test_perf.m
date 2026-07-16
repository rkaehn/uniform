#include "ui_tests.h"
#include "ui_core.h"
#include "ui_gestures.h"
#include "ui_input.h"
#include "gpu.h"
#include "cr_mem.h"
#include <stdio.h>
#include <stdlib.h>

#define ui_test_perf_rows 20u
#define ui_test_perf_cols 25u
#define ui_test_perf_warmup 10u
#define ui_test_perf_cells 2000u

struct ui_test_perf_micro {
    struct ui_node root;
    struct ui_node child;
    struct ui_node cells[ui_test_perf_cells];
};

static void ui_test_perf_check(bool ok, const char* label) {
    ui_test_group_check_("perf", ok, label);
}

static void ui_test_perf_on_tap(struct ui_tap_gesture gesture, void* user) {
}

static int ui_test_perf_compare(const void* a, const void* b) {
    double x = *(const double*)a;
    double y = *(const double*)b;
    return x < y ? -1 : x > y;
}

static const char* ui_test_perf_format(double* samples, uint32_t count, char* buf, size_t size) {
    qsort(samples, count, sizeof(samples[0]), ui_test_perf_compare);
    snprintf(buf, size, "%.3f/%.3f", samples[0], samples[count / 2]);
    return buf;
}

static struct ui_node ui_test_perf_scene(void) {
    struct ui_node root = ui_frame(&(struct ui_frame) {
        .size = ui_size_fix(1024.0, 1024.0),
        .layout = ui_layout_stack_y(ui_dist_start, ui_align_start, 4.0, ui_pad(8)),
        .fill = ui_fill_solid(ui_gray10),
    });
    for (uint32_t r = 0; r < ui_test_perf_rows; r++) {
        struct ui_node row = ui_frame(&(struct ui_frame) {
            .parent = root,
            .key = ui_key_u64(r),
            .layout = ui_layout_stack_x(ui_dist_start, ui_align_center, 4.0, ui_pad(0)),
        });
        for (uint32_t c = 0; c < ui_test_perf_cols; c++) {
            struct ui_node cell = ui_frame(&(struct ui_frame) {
                .parent = row,
                .key = ui_key_u64(c),
                .size = ui_size_fix(36.0, 24.0),
                .fill = ui_fill_solid(ui_gray50),
                .corners = ui_corners(4.0),
            });
            ui_tap(&(struct ui_tap) { .node = cell, .on_change = ui_test_perf_on_tap });
            ui_text(&(struct ui_text) { .parent = cell, .str = ui_str("ab", ui_white, 12.0) });
        }
    }
    return root;
}

#if cr_macos || cr_ios

static void ui_test_perf_frames(uint32_t iterations) {
    struct ui* ui = ui_ctx();
    struct gpu_texture* target = gpu_texture_create(ui->device, &(struct gpu_texture_desc) {
        .label = "perf",
        .format = gpu_format_bgra8,
        .size = { 1024, 1024 },
        .usage = gpu_texture_usage_render_target | gpu_texture_usage_sample,
    });
    uint32_t warmup = iterations > 1 ? ui_test_perf_warmup : 0;
    double* build = cr_malloc(cr_mem_libc, sizeof(double) * iterations * 4, 0, cr_loc);
    double* layout = build + iterations;
    double* render = layout + iterations;
    double* release = render + iterations;
    struct ui_node prev = ui_node_null;

    for (uint32_t i = 0; i < warmup + iterations; i++) {
        ui_input_dispatch(prev, 1.0 + i * 0.016, NULL, 0, 1.0);
        double t0 = cr_get_current_time();
        struct ui_node root = ui_test_perf_scene();
        double t1 = cr_get_current_time();
        ui_layout(root);
        double t2 = cr_get_current_time();
        uint64_t serial;
        cr_autoreleasepool {
            struct gpu_command_buffer* command_buffer = gpu_command_buffer_create(ui->device, "perf");
            ui_render(root, command_buffer, target, (struct ui_render_desc) { 0 });
            serial = gpu_command_buffer_commit(command_buffer);
        }
        double t3 = cr_get_current_time();
        ui_release(prev);
        double t4 = cr_get_current_time();
        prev = root;
        if (i >= warmup) {
            build[i - warmup] = (t1 - t0) * 1000.0;
            layout[i - warmup] = (t2 - t1) * 1000.0;
            render[i - warmup] = (t3 - t2) * 1000.0;
            release[i - warmup] = (t4 - t3) * 1000.0;
        }
        gpu_device_wait(ui->device, serial);
    }
    ui_release(prev);
    ui_input_dispatch(ui_node_null, 100.0, NULL, 0, 1.0);
    gpu_texture_destroy(ui->device, target);

    if (iterations > 1) {
        char b[32], l[32], r[32], f[32];
        uint32_t nodes = 1 + ui_test_perf_rows + ui_test_perf_rows * ui_test_perf_cols * 2;
        printf("perf scene %u nodes, %u gestures: build %s, layout %s, render %s, release %s ms (min/median over %u frames)\n", nodes, ui_test_perf_rows * ui_test_perf_cols,
               ui_test_perf_format(build, iterations, b, sizeof(b)), ui_test_perf_format(layout, iterations, l, sizeof(l)),
               ui_test_perf_format(render, iterations, r, sizeof(r)), ui_test_perf_format(release, iterations, f, sizeof(f)), iterations);
    }
    cr_free(cr_mem_libc, build, cr_loc);
    ui_test_perf_check(ui_is_alive(prev) == false, "the perf scene builds, lays out and renders");
}

#endif

static void ui_test_perf_reset(struct ui_test_perf_micro* m) {
    ui_release(m->root);
    ui_input_dispatch(ui_node_null, 200.0, NULL, 0, 1.0);
    m->root = ui_frame(&(struct ui_frame) { .size = ui_size_fix(100.0, 100.0) });
    m->child = ui_frame(&(struct ui_frame) { .parent = m->root, .size = ui_size_fix(10.0, 10.0) });
    for (uint32_t i = 0; i < ui_test_perf_cells; i++) {
        m->cells[i] = ui_frame(&(struct ui_frame) { .parent = m->root, .size = ui_size_fix(1.0, 1.0) });
    }
    ui_layout(m->root);
}

static void ui_test_perf_is_alive(struct ui_test_perf_micro* m, uint32_t i) {
    ui_is_alive(m->child);
}

static void ui_test_perf_get_size(struct ui_test_perf_micro* m, uint32_t i) {
    ui_get_size(m->child);
}

static void ui_test_perf_derive_key(struct ui_test_perf_micro* m, uint32_t i) {
    ui_derive_key(m->root, ui_key_str("abc"));
}

static void ui_test_perf_frame(struct ui_test_perf_micro* m, uint32_t i) {
    ui_frame(&(struct ui_frame) { .parent = m->root, .size = ui_size_fix(10.0, 10.0), .fill = ui_fill_solid(ui_red), .corners = ui_corners(4.0) });
}

static void ui_test_perf_text(struct ui_test_perf_micro* m, uint32_t i) {
    ui_text(&(struct ui_text) { .parent = m->root, .str = ui_str("ab", ui_white, 12.0) });
}

static void ui_test_perf_tap(struct ui_test_perf_micro* m, uint32_t i) {
    ui_tap(&(struct ui_tap) { .node = m->cells[i], .on_change = ui_test_perf_on_tap });
}

static void ui_test_perf_time(const char* label, uint32_t runs, uint32_t count, void (*step)(struct ui_test_perf_micro*, uint32_t), bool reset, struct ui_test_perf_micro* m) {
    double* samples = cr_malloc(cr_mem_libc, sizeof(double) * runs, 0, cr_loc);
    for (uint32_t run = 0; run < runs; run++) {
        double begin = cr_get_current_time();
        for (uint32_t i = 0; i < count; i++) {
            step(m, i);
        }
        samples[run] = (cr_get_current_time() - begin) * 1000000.0 / count;
        if (reset) {
            ui_test_perf_reset(m);
        }
    }
    if (runs > 1) {
        char buf[32];
        printf("perf %s: %s us (min/median over %u runs of %u)\n", label, ui_test_perf_format(samples, runs, buf, sizeof(buf)), runs, count);
    }
    cr_free(cr_mem_libc, samples, cr_loc);
}

static void ui_test_perf_micro(uint32_t iterations) {
    struct ui_test_perf_micro* m = cr_malloc(cr_mem_libc, sizeof(*m), 0, cr_loc);
    m->root = ui_node_null;
    ui_test_perf_reset(m);
    uint32_t calls = iterations > 1 ? 100000 : 100;
    uint32_t nodes_per_run = iterations > 1 ? ui_test_perf_cells : 20;
    ui_test_perf_time("isAlive", iterations, calls, ui_test_perf_is_alive, false, m);
    ui_test_perf_time("getSize", iterations, calls, ui_test_perf_get_size, false, m);
    ui_test_perf_time("deriveKey", iterations, calls, ui_test_perf_derive_key, false, m);
    ui_test_perf_time("frame", iterations, nodes_per_run, ui_test_perf_frame, true, m);
    ui_test_perf_time("text", iterations, nodes_per_run, ui_test_perf_text, true, m);
    ui_test_perf_time("tap", iterations, nodes_per_run, ui_test_perf_tap, true, m);
    ui_release(m->root);
    ui_input_dispatch(ui_node_null, 300.0, NULL, 0, 1.0);
    cr_free(cr_mem_libc, m, cr_loc);
}

void ui_test_perf_(uint32_t iterations) {
    double saved_to_px_from_pt = ui_get_to_px_from_pt();
    ui_set_to_px_from_pt(1.0);
#if cr_macos || cr_ios
    ui_test_perf_frames(iterations);
#endif
    ui_test_perf_micro(iterations);
    ui_set_to_px_from_pt(saved_to_px_from_pt);
}
