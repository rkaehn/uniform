#include <stdio.h>
#include "cr.h"
#include "ui_shelf.h"
#include "ui_tests.h"

struct ui_test_shelf_entry {
    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h;
};

static uint32_t ui_test_shelf_rand(uint64_t* state) {
    return (uint32_t)cr_rand(state);
}

static bool ui_test_shelf_mark(uint8_t* grid, uint32_t grid_w, uint32_t grid_h, struct ui_test_shelf_entry e, uint8_t value) {
    uint32_t x0 = e.x / ui_shelf_leaf_px;
    uint32_t y0 = e.y / ui_shelf_leaf_px;
    uint32_t x1 = x0 + (1u << ui_shelf_order_for_px(e.w));
    uint32_t y1 = y0 + (1u << ui_shelf_order_for_px(e.h));

    if (x1 > grid_w || y1 > grid_h) {
        return false;
    }

    for (uint32_t y = y0; y < y1; y++) {
        for (uint32_t x = x0; x < x1; x++) {
            if (grid[y * grid_w + x] == value) {
                return false;
            }
            grid[y * grid_w + x] = value;
        }
    }

    return true;
}

static bool ui_test_shelf_random(struct cr_mem* mem, uint32_t width_px, uint32_t height_px, uint32_t max_extent, uint64_t rng) {
    uint32_t grid_w = width_px / ui_shelf_leaf_px;
    uint32_t grid_h = height_px / ui_shelf_leaf_px;
    bool ok = true;

    struct ui_shelf shelf;
    ui_shelf_init(&shelf, mem, width_px, height_px);

    uint8_t* grid = cr_malloc(mem, (uint64_t)grid_w * grid_h, 0, cr_loc);
    memset(grid, 0, grid_w * grid_h);

    struct ui_test_shelf_entry* entries = cr_malloc(mem, 4096 * sizeof(*entries), 0, cr_loc);
    uint32_t entry_count = 0;

    for (uint32_t iter = 0; iter < 100000; iter++) {
        bool do_free = entry_count > 0 && (entry_count == 4096 || ui_test_shelf_rand(&rng) % 5 < 2);

        if (do_free) {
            uint32_t i = ui_test_shelf_rand(&rng) % entry_count;
            struct ui_test_shelf_entry e = entries[i];
            entries[i] = entries[--entry_count];
            ok = ok && ui_test_shelf_mark(grid, grid_w, grid_h, e, 0);
            ui_shelf_free(&shelf, e.x, e.y, e.w, e.h);
        } else {
            struct ui_test_shelf_entry e;
            e.w = (uint16_t)(1 + ui_test_shelf_rand(&rng) % max_extent);
            e.h = (uint16_t)(1 + ui_test_shelf_rand(&rng) % max_extent);

            if (ui_shelf_alloc(&shelf, e.w, e.h, &e.x, &e.y)) {
                ok = ok && e.x + e.w <= width_px && e.y + e.h <= height_px;
                ok = ok && ui_test_shelf_mark(grid, grid_w, grid_h, e, 1);
                entries[entry_count++] = e;
            }
        }
    }

    while (entry_count > 0) {
        struct ui_test_shelf_entry e = entries[--entry_count];
        ok = ok && ui_test_shelf_mark(grid, grid_w, grid_h, e, 0);
        ui_shelf_free(&shelf, e.x, e.y, e.w, e.h);
    }

    ok = ok && ui_shelf_is_empty(&shelf);

    for (uint32_t i = 0; i < grid_w * grid_h; i++) {
        ok = ok && grid[i] == 0;
    }

    uint16_t x = 0;
    uint16_t y = 0;
    ok = ok && ui_shelf_alloc(&shelf, width_px, height_px, &x, &y);
    ui_shelf_free(&shelf, x, y, width_px, height_px);

    cr_free(mem, entries, cr_loc);
    cr_free(mem, grid, cr_loc);
    ui_shelf_deinit(&shelf);

    return ok;
}

void ui_test_shelf_(void) {
    struct cr_mem* mem = cr_mem_libc;
    bool ok = true;

    {
        struct ui_shelf shelf;
        ui_shelf_init(&shelf, mem, 512, 512);

        struct ui_test_shelf_entry entries[64];
        for (uint32_t i = 0; i < 64; i++) {
            ok = ok && ui_shelf_alloc(&shelf, 64, 64, &entries[i].x, &entries[i].y);
            entries[i].w = 64;
            entries[i].h = 64;
        }

        ok = ok && ui_shelf_is_empty(&shelf) == false;

        uint16_t x = 0;
        uint16_t y = 0;
        ok = ok && ui_shelf_alloc(&shelf, 64, 64, &x, &y) == false;
        ok = ok && ui_shelf_alloc(&shelf, 1, 1, &x, &y) == false;

        for (uint32_t i = 0; i < 64; i++) {
            ui_shelf_free(&shelf, entries[i].x, entries[i].y, 64, 64);
        }

        ok = ok && ui_shelf_is_empty(&shelf);

        ok = ok && ui_shelf_alloc(&shelf, 512, 512, &x, &y);
        ok = ok && x == 0 && y == 0;
        ui_shelf_free(&shelf, x, y, 512, 512);

        ui_shelf_deinit(&shelf);
    }

    ok = ok && ui_test_shelf_random(mem, 1024, 1024, 120, 0x12345678);
    ok = ok && ui_test_shelf_random(mem, 2048, 512, 200, 0xdeadbeef);
    ok = ok && ui_test_shelf_random(mem, 256, 4096, 100, 0xcafef00d);

    {
        struct ui_shelf shelf;
        ui_shelf_init(&shelf, mem, 1024, 1024);

        struct ui_test_shelf_entry* entries = cr_malloc(mem, 4096 * sizeof(*entries), 0, cr_loc);
        uint32_t entry_count = 0;
        uint64_t rng = 0;

        uint32_t op_count = 1000000;
        double t0 = cr_get_current_time();

        for (uint32_t iter = 0; iter < op_count; iter++) {
            bool do_free = entry_count > 0 && (entry_count == 4096 || ui_test_shelf_rand(&rng) % 5 < 2);

            if (do_free) {
                uint32_t i = ui_test_shelf_rand(&rng) % entry_count;
                struct ui_test_shelf_entry e = entries[i];
                entries[i] = entries[--entry_count];
                ui_shelf_free(&shelf, e.x, e.y, e.w, e.h);
            } else {
                struct ui_test_shelf_entry e;
                e.w = (uint16_t)(4 + ui_test_shelf_rand(&rng) % 60);
                e.h = (uint16_t)(4 + ui_test_shelf_rand(&rng) % 60);

                if (ui_shelf_alloc(&shelf, e.w, e.h, &e.x, &e.y)) {
                    entries[entry_count++] = e;
                }
            }
        }

        double dt = cr_get_current_time() - t0;
        printf("test_shelf: %.0f ns/op (%u ops)\n", dt / (double)op_count * 1e9, op_count);

        while (entry_count > 0) {
            struct ui_test_shelf_entry e = entries[--entry_count];
            ui_shelf_free(&shelf, e.x, e.y, e.w, e.h);
        }

        cr_free(mem, entries, cr_loc);
        ui_shelf_deinit(&shelf);
    }

    printf("test_shelf: %s\n", ok ? "ok" : "FAILED");
    assert(ok);
}
