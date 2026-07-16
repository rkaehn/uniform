#include "ui_tests.h"
#include "ui_core.h"
#include "ui_atlas.h"
#include "ui_persistent.h"
#include <string.h>

#define ui_test_atlas_px 128u
#define ui_test_atlas_churn_renders 60u
#define ui_test_atlas_in_flight 8u
#define ui_test_atlas_open_sizes 16u

static const char* ui_test_atlas_raw = "Sphinx of black quartz, judge my vow. 0123456789";

static struct {
    uint8_t* first;
    uint8_t* cached;
    uint8_t* churned;
    uint8_t* reloaded;
    uint8_t* large;
    uint8_t* large_cached;
    uint8_t* together[ui_test_atlas_in_flight];
    uint8_t* alone[ui_test_atlas_in_flight];
    uint32_t churn_render;
    uint64_t churn_misses;
    struct gpu_command_buffer* open;
    struct gpu_texture* open_target;
    uint8_t* open_px;
    uint8_t* newer;
} ui_test_atlas_state;

static void ui_test_check(bool ok, const char* label) {
    ui_test_group_check_("atlas", ok, label);
}

static struct ui_node ui_test_atlas_text(struct ui_node root, const char* raw, double pt_size, double2 pos, double width) {
    ui_text(&(struct ui_text) {
        .parent = root,
        .pos = ui_pos_xy(pos.x, pos.y),
        .size.width = ui_size_axis_fix(width),
        .str = ui_str(raw, ui_white, pt_size),
    });

    return root;
}

static struct ui_node ui_test_atlas_root(void) {
    return ui_frame(&(struct ui_frame) {
        .size = ui_size_fix(ui_test_atlas_px, ui_test_atlas_px),
        .fill = ui_fill_solid(ui_black),
    });
}

static struct ui_node ui_test_atlas_paragraph(double pt_size) {
    return ui_test_atlas_text(ui_test_atlas_root(), ui_test_atlas_raw, pt_size, make_double2(4.0, 4.0), ui_test_atlas_px - 8.0);
}

static struct ui_node ui_test_atlas_line(double pt_size) {
    return ui_test_atlas_text(ui_test_atlas_root(), ui_test_atlas_raw, pt_size, make_double2(4.0, 4.0), 8000.0);
}

static struct ui_node ui_test_atlas_large(void) {
    return ui_test_atlas_text(ui_test_atlas_root(), "|", 1400.0, make_double2(-356.0, -300.0), 1000.0);
}

static struct ui_node ui_test_atlas_in_flight_text(uint32_t i) {
    return ui_test_atlas_text(ui_test_atlas_root(), ui_test_atlas_raw, 100.0 + 60.0 * (double)i, make_double2(-30.0 * (double)i, 2.0), 8000.0);
}

static struct ui_node ui_test_atlas_many_sizes(void) {
    struct ui_node root = ui_test_atlas_root();

    for (uint32_t i = 0; i < ui_test_atlas_open_sizes; i++) {
        ui_test_atlas_text(root, ui_test_atlas_raw, 150.0 + 30.0 * (double)i, make_double2(0.0, 0.0), 8000.0);
    }

    return root;
}

static uint8_t* ui_test_atlas_read(struct ui_node root) {
    struct ui_render_desc options = { .premultiplied = true };
    return ui_test_render_read_(root, &options, gpu_format_bgra8, make_uint2(ui_test_atlas_px, ui_test_atlas_px));
}

static struct gpu_texture* ui_test_atlas_target(uint32_t usage) {
    return gpu_texture_create(ui_ctx()->device, &(struct gpu_texture_desc) {
        .label = "test atlas target",
        .format = gpu_format_bgra8,
        .size = { ui_test_atlas_px, ui_test_atlas_px },
        .usage = gpu_texture_usage_render_target | gpu_texture_usage_sample | usage,
    });
}

static void ui_test_atlas_render_into(struct gpu_command_buffer* command_buffer, struct gpu_texture* target, struct ui_node root) {
    ui_layout(root);
    ui_render(root, command_buffer, target, (struct ui_render_desc) { .premultiplied = true });
    ui_release(root);
}

static void ui_test_atlas_free(uint8_t** px) {
    cr_free(cr_mem_libc, *px, cr_loc);
    *px = NULL;
}

static bool ui_test_atlas_equal(const uint8_t* a, const uint8_t* b) {
    return memcmp(a, b, ui_test_atlas_px * ui_test_atlas_px * 4) == 0;
}

static bool ui_test_atlas_lit(const uint8_t* px) {
    for (uint32_t i = 0; i < ui_test_atlas_px * ui_test_atlas_px; i++) {
        if (px[i * 4 + 1] > 128) {
            return true;
        }
    }
    return false;
}

static void ui_test_atlas_open_check(void* args) {
    ui_test_check(ui_test_atlas_equal(ui_test_atlas_state.open_px, ui_test_atlas_state.newer), "a command buffer committed after a newer one completed renders its glyphs intact");
    ui_test_atlas_free(&ui_test_atlas_state.open_px);
    ui_test_atlas_free(&ui_test_atlas_state.newer);
}

static void ui_test_atlas_open_commit(void* args) {
    struct ui* ui = ui_ctx();
    struct gpu_texture* scratch = ui_test_atlas_target(0);

    ui_test_atlas_state.open_px = cr_malloc(cr_mem_libc, ui_test_atlas_px * ui_test_atlas_px * 4, 16, cr_loc);

    cr_autoreleasepool {
        ui_test_atlas_render_into(ui_test_atlas_state.open, scratch, ui_test_atlas_many_sizes());
        ui_test_atlas_render_into(ui_test_atlas_state.open, scratch, ui_test_atlas_paragraph(12.0));

        uint64_t misses = ui_atlas_glyph_misses_();
        ui_test_atlas_render_into(ui_test_atlas_state.open, scratch, ui_test_atlas_paragraph(13.0));
        ui_test_check(ui_atlas_glyph_misses_() == misses, "glyphs of an open command buffer stay cached after a newer one completes");

        gpu_command_buffer_read_texture(ui_test_atlas_state.open, ui_test_atlas_state.open_target, ui_test_atlas_state.open_px, ui_test_atlas_px * 4);
        ui_test_await_(gpu_command_buffer_commit(ui_test_atlas_state.open));
    }

    gpu_texture_destroy(ui->device, scratch);
    gpu_texture_destroy(ui->device, ui_test_atlas_state.open_target);
    ui_test_atlas_state.open = NULL;
    ui_test_atlas_state.open_target = NULL;
    ui_test_then_(ui_test_atlas_open_check, NULL);
}

static void ui_test_atlas_open(void) {
    ui_test_atlas_state.open_target = ui_test_atlas_target(gpu_texture_usage_read);

    cr_autoreleasepool {
        ui_test_atlas_state.open = gpu_command_buffer_create(ui_ctx()->device, "test atlas open");
        ui_test_atlas_render_into(ui_test_atlas_state.open, ui_test_atlas_state.open_target, ui_test_atlas_paragraph(13.0));
    }

    ui_test_atlas_state.newer = ui_test_atlas_read(ui_test_atlas_paragraph(13.0));
    ui_test_then_(ui_test_atlas_open_commit, NULL);
}

static void ui_test_atlas_in_flight_check(void* args) {
    bool identical = true;

    for (uint32_t i = 0; i < ui_test_atlas_in_flight; i++) {
        identical = identical && ui_test_atlas_equal(ui_test_atlas_state.together[i], ui_test_atlas_state.alone[i]);
        ui_test_atlas_free(&ui_test_atlas_state.together[i]);
        ui_test_atlas_free(&ui_test_atlas_state.alone[i]);
    }

    ui_test_check(identical, "renders in flight during eviction match renders done one at a time");
    ui_test_atlas_open();
}

static void ui_test_atlas_in_flight_alone(void* args) {
    uint32_t i = (uint32_t)(uintptr_t)args;

    ui_test_atlas_state.alone[i] = ui_test_atlas_read(ui_test_atlas_in_flight_text(i));
    ui_test_then_(i + 1 < ui_test_atlas_in_flight ? ui_test_atlas_in_flight_alone : ui_test_atlas_in_flight_check, (void*)(uintptr_t)(i + 1));
}

static void ui_test_atlas_large_check(void* args) {
    ui_test_check(ui_test_atlas_lit(ui_test_atlas_state.large), "glyph larger than a page renders");
    ui_test_check(ui_test_atlas_equal(ui_test_atlas_state.large, ui_test_atlas_state.large_cached), "glyph larger than a page renders the same from the cache");
    ui_test_atlas_free(&ui_test_atlas_state.large);
    ui_test_atlas_free(&ui_test_atlas_state.large_cached);

    for (uint32_t i = 0; i < ui_test_atlas_in_flight; i++) {
        ui_test_atlas_state.together[i] = ui_test_atlas_read(ui_test_atlas_in_flight_text(i));
    }
    ui_test_then_(ui_test_atlas_in_flight_alone, (void*)(uintptr_t)0);
}

static void ui_test_atlas_large_cached(void* args) {
    uint64_t misses = ui_atlas_glyph_misses_();

    ui_test_atlas_state.large_cached = ui_test_atlas_read(ui_test_atlas_large());
    ui_test_check(ui_atlas_glyph_misses_() == misses, "glyph larger than a page stays cached");
    ui_test_then_(ui_test_atlas_large_check, NULL);
}

static void ui_test_atlas_reloaded_check(void* args) {
    ui_test_check(ui_test_atlas_equal(ui_test_atlas_state.first, ui_test_atlas_state.reloaded), "glyphs rasterized again after eviction render identically");
    ui_test_atlas_free(&ui_test_atlas_state.first);
    ui_test_atlas_free(&ui_test_atlas_state.reloaded);

    ui_test_atlas_state.large = ui_test_atlas_read(ui_test_atlas_large());
    ui_test_then_(ui_test_atlas_large_cached, NULL);
}

static void ui_test_atlas_churn(void* args) {
    struct ui* ui = ui_ctx();

    ui_test_atlas_free(&ui_test_atlas_state.churned);

    if (ui_test_atlas_state.churn_render < ui_test_atlas_churn_renders) {
        ui_test_atlas_state.churned = ui_test_atlas_read(ui_test_atlas_line(24.0 + 6.0 * ui_test_atlas_state.churn_render++));
        ui_test_then_(ui_test_atlas_churn, NULL);
        return;
    }

    ui_test_check(ui_atlas_glyph_count_() < ui_atlas_glyph_misses_() - ui_test_atlas_state.churn_misses, "churn evicts glyphs");
    ui_test_check(ui_atlas_glyph_bytes_() <= (8u << 20), "churn keeps the atlas within its budget");
    ui_test_check(ui_persistent_page_count(ui, gpu_format_r8) <= 12, "churn evicts glyphs instead of adding pages");

    uint64_t misses = ui_atlas_glyph_misses_();
    ui_test_atlas_state.reloaded = ui_test_atlas_read(ui_test_atlas_paragraph(13.0));
    ui_test_check(ui_atlas_glyph_misses_() > misses, "churn evicts the least recently used glyphs");
    ui_test_then_(ui_test_atlas_reloaded_check, NULL);
}

static void ui_test_atlas_cached_check(void* args) {
    ui_test_check(ui_test_atlas_equal(ui_test_atlas_state.first, ui_test_atlas_state.cached), "cached glyphs render identically");
    ui_test_atlas_free(&ui_test_atlas_state.cached);

    ui_test_atlas_state.churn_render = 0;
    ui_test_atlas_state.churn_misses = ui_atlas_glyph_misses_();
    ui_test_atlas_churn(NULL);
}

static void ui_test_atlas_cached(void* args) {
    uint64_t misses = ui_atlas_glyph_misses_();

    ui_test_atlas_state.cached = ui_test_atlas_read(ui_test_atlas_paragraph(13.0));
    ui_test_check(ui_atlas_glyph_misses_() == misses, "glyphs stay cached after their render completes");
    ui_test_then_(ui_test_atlas_cached_check, NULL);
}

void ui_test_atlas_(void) {
    ui_test_atlas_state.first = ui_test_atlas_read(ui_test_atlas_paragraph(13.0));
    ui_test_then_(ui_test_atlas_cached, NULL);
}
