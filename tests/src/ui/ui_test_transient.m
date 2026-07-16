#include "ui_tests.h"
#include "ui_core.h"
#include <stdio.h>


static void ui_test_check(bool ok, const char* label) {
    ui_test_group_check_("transient", ok, label);
}

static void ui_test_transient_eviction(void) {
    struct ui* ui = ui_ctx();

    struct ui_transient_tex tex = ui_transient_reserve(ui, gpu_format_rgba16f, 900, 900);
    ui_transient_release(ui, tex);

    uint64_t used_before = ui_transient_memory_used_(ui);

    for (uint32_t i = 0; i < 200; i++) {
        ui_transient_frame_end(ui);
    }

    uint64_t used_after = ui_transient_memory_used_(ui);

    ui_test_check(used_after < used_before, "texture eviction");
}

static void ui_test_transient_churn(void) {
    struct ui* ui = ui_ctx();

    uint64_t peak = 0;
    bool texs_valid = true;

    for (uint32_t i = 0; i < 400; i++) {
        uint32_t w = 2000 + (i % 97) * 8;
        uint32_t h = 1400 + (i % 89) * 6;
        uint32_t blur_h = 150 + (i % 13) * 8;

        struct ui_transient_tex a = ui_transient_reserve(ui, gpu_format_rgba16f, w, h);
        struct ui_transient_tex b = ui_transient_reserve(ui, gpu_format_rgba16f, w, blur_h);
        struct ui_transient_tex c = ui_transient_reserve_mips(ui, gpu_format_rgba16f, w, blur_h, 5);

        texs_valid = texs_valid &&
                     ui_transient_texture(ui, a) != NULL &&
                     ui_transient_texture(ui, b) != NULL &&
                     ui_transient_texture(ui, c) != NULL &&
                     ui_transient_texture(ui, b) != ui_transient_texture(ui, a) &&
                     ui_transient_texture(ui, c) != ui_transient_texture(ui, a) &&
                     ui_transient_texture(ui, c) != ui_transient_texture(ui, b) &&
                     ui_transient_mip_count(ui, c) == 5 &&
                     ui_transient_mip_view(ui, c, 4) != NULL;

        ui_transient_release(ui, c);
        ui_transient_release(ui, b);
        ui_transient_release(ui, a);
        ui_transient_frame_end(ui);

        peak = max_uint64(peak, ui_transient_memory_used_(ui));
    }

    for (uint32_t i = 0; i < 200; i++) {
        ui_transient_frame_end(ui);
    }

    ui_test_check(texs_valid, "churn texture validity");
    ui_test_check(peak <= (512ull << 20), "churn stays within limit");
}

static void ui_test_transient_regions(void) {
    struct ui* ui = ui_ctx();

    struct ui_transient_tex a = ui_transient_reserve_storage(ui, gpu_format_rgba16f, 200, 120);
    struct ui_transient_tex b = ui_transient_reserve_storage(ui, gpu_format_rgba16f, 300, 80);

    uint2 origin_a = ui_transient_tex_origin(ui, a);
    uint2 phys_a = ui_transient_tex_phys(ui, a);
    uint2 origin_b = ui_transient_tex_origin(ui, b);
    uint2 phys_b = ui_transient_tex_phys(ui, b);

    ui_test_check(ui_transient_texture(ui, a) == ui_transient_texture(ui, b), "regions share a page");
    ui_test_check(all_int2(ui_transient_tex_size(ui, a) == make_uint2(2048, 2048)), "region page size");
    ui_test_check(phys_a.x == 256 && phys_a.y == 128, "region phys rounds to po2");

    bool disjoint = origin_a.x + phys_a.x <= origin_b.x || origin_b.x + phys_b.x <= origin_a.x ||
                    origin_a.y + phys_a.y <= origin_b.y || origin_b.y + phys_b.y <= origin_a.y;
    ui_test_check(disjoint, "regions do not overlap");

    struct ui_transient_tex c = ui_transient_reserve_storage_dedicated(ui, gpu_format_rgba16f, 200, 120);
    ui_test_check(ui_transient_texture(ui, c) != ui_transient_texture(ui, a), "dedicated reserve gets own texture");
    ui_test_check(all_int2(ui_transient_tex_origin(ui, c) == make_uint2(0, 0)), "dedicated origin is zero");

    struct ui_transient_tex d = ui_transient_reserve_storage(ui, gpu_format_rgba16f, 1600, 1600);
    ui_test_check(ui_transient_texture(ui, d) != ui_transient_texture(ui, a), "oversized reserve is dedicated");
    ui_test_check(all_int2(ui_transient_tex_origin(ui, d) == make_uint2(0, 0)), "oversized origin is zero");

    struct ui_transient_tex e = ui_transient_reserve_storage_channel(ui, gpu_format_rgba16f, 64, 64, ui_transient_channel_blur_down);
    ui_test_check(ui_transient_texture(ui, e) != ui_transient_texture(ui, a), "channels use separate pages");
    ui_test_check(ui_transient_tex_channel(ui, e) == ui_transient_channel_blur_down, "region channel query");
    ui_test_check(ui_transient_tex_channel(ui, c) == uint32_max, "dedicated channel query is none");

    ui_transient_release(ui, e);
    ui_transient_release(ui, d);
    ui_transient_release(ui, c);
    ui_transient_release(ui, b);
    ui_transient_release(ui, a);
    ui_transient_frame_end(ui);

    for (uint32_t i = 0; i < 200; i++) {
        ui_transient_frame_end(ui);
    }
}

static void ui_test_transient_region_mips(void) {
    struct ui* ui = ui_ctx();

    struct ui_transient_tex a = ui_transient_reserve_mips_storage(ui, gpu_format_rgba16f, 200, 120, 6);
    uint2 origin = ui_transient_tex_origin(ui, a);
    uint2 phys = ui_transient_tex_phys(ui, a);

    ui_test_check(ui_transient_mip_count(ui, a) == 6, "region mip count");
    ui_test_check((origin.x % phys.x) == 0 && (origin.y % phys.y) == 0, "region origin aligned to phys");

    bool views_ok = true;
    for (uint32_t level = 0; level < 6; level++) {
        views_ok = views_ok && ui_transient_mip_view(ui, a, level) != NULL;
        views_ok = views_ok && ((origin.x >> level) << level) == origin.x;
        views_ok = views_ok && ((origin.y >> level) << level) == origin.y;
    }
    ui_test_check(views_ok, "region mip views and alignment");

    struct ui_transient_tex b = ui_transient_reserve_storage(ui, gpu_format_rgba16f, 200, 120);
    ui_test_check(ui_transient_texture(ui, b) != ui_transient_texture(ui, a), "mipped pool separate from flat pool");

    ui_transient_release(ui, b);
    ui_transient_release(ui, a);
    ui_transient_frame_end(ui);

    for (uint32_t i = 0; i < 200; i++) {
        ui_transient_frame_end(ui);
    }
}

static void ui_test_transient_region_reuse(void) {
    struct ui* ui = ui_ctx();

    struct ui_transient_tex first = ui_transient_reserve_storage(ui, gpu_format_rgba16f, 100, 100);
    struct gpu_texture* page = ui_transient_texture(ui, first);
    ui_transient_release(ui, first);
    ui_transient_frame_end(ui);

    uint64_t used = ui_transient_memory_used_(ui);
    bool same_page = true;

    for (uint32_t i = 0; i < 400; i++) {
        uint32_t w = 40 + (i % 97) * 8;
        uint32_t h = 40 + (i % 89) * 6;

        struct ui_transient_tex tex = ui_transient_reserve_storage(ui, gpu_format_rgba16f, w, h);
        same_page = same_page && ui_transient_texture(ui, tex) == page;
        ui_transient_release(ui, tex);
        ui_transient_frame_end(ui);
    }

    ui_test_check(same_page, "region churn reuses one page");
    ui_test_check(ui_transient_memory_used_(ui) == used, "region churn allocates no memory");

    for (uint32_t i = 0; i < 200; i++) {
        ui_transient_frame_end(ui);
    }

    ui_test_check(ui_transient_memory_used_(ui) < used, "idle page eviction");
}

void ui_test_transient_(void) {

    ui_test_transient_eviction();
    ui_test_transient_churn();
    ui_test_transient_regions();
    ui_test_transient_region_mips();
    ui_test_transient_region_reuse();

}

