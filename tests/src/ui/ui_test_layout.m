#include <assert.h>
#include <stdio.h>
#include "cr.h"
#include "ui.h"
#include "ui_tests.h"

#include <yoga/Yoga.h>

void* ui_yoga(struct ui_node root);
void ui_yoga_layout(void* _root_node);
bool ui_yoga_is_equal(struct ui_node root, void* _root_node);
void ui_yoga_release(void* _root_node);

static uint64_t random_state;

static int random_int(int n) {
    return (int)(cr_rand(&random_state) % (uint64_t)n);
}

static struct ui_size_axis random_size_axis(void) {
    int r = random_int(3);
    if (r == 0) return ui_size_axis_fit();
    if (r == 1) return ui_size_axis_fill();
    return ui_size_axis_fix((double)(random_int(150) + 20));
}

static struct ui_size_axis random_size_axis_rel(enum ui_size_anchor anchor) {
    return ui_size_axis_rel(anchor, 0.25 * (double)(random_int(4) + 1), 0.0);
}

static enum ui_dist random_dist(void) {
    int r = random_int(3);
    if (r == 0) return ui_dist_start;
    if (r == 1) return ui_dist_center;
    return ui_dist_end;
}

static enum ui_align random_align(void) {
    int r = random_int(3);
    if (r == 0) return ui_align_start;
    if (r == 1) return ui_align_center;
    return ui_align_end;
}

static enum ui_pos_anchor random_pos_anchor(void) {
    int r = random_int(3);
    if (r == 0) return ui_pos_anchor_start;
    if (r == 1) return ui_pos_anchor_center;
    return ui_pos_anchor_end;
}

static struct ui_pos_axis random_pos_axis(void) {
    return ui_pos_axis_rel(random_pos_anchor(), random_pos_anchor(), (double)(random_int(41) - 20));
}

static float random_pad(void) {
    return random_int(2) == 0 ? 0.0f : (float)random_int(16);
}

static void random_layout(struct ui_layout* layout) {
    layout->mode = random_int(2) == 0 ? ui_layout_mode_stack_x : ui_layout_mode_stack_y;
    layout->dist = random_dist();
    layout->align = random_align();
    layout->spacing = random_pad();
    layout->padding_left = random_pad();
    layout->padding_right = random_pad();
    layout->padding_top = random_pad();
    layout->padding_bottom = random_pad();
}

static uint32_t generate_random_tree(struct ui_node parent, int current_depth, int max_depth) {
    if (current_depth >= max_depth) {
        return 0;
    }

    uint32_t count = 0;
    int num_children = random_int(4) + 1;

    for (int i = 0; i < num_children; i++) {
        struct ui_frame frame_desc = {
            .parent = parent,
        };

        frame_desc.size.width = random_size_axis();
        frame_desc.size.height = random_size_axis();
        random_layout(&frame_desc.layout);

        if (random_int(5) == 0) {
            if (random_int(4) == 0) {
                frame_desc.pos = ui_pos_xy((double)(random_int(41) - 20), (double)(random_int(41) - 20));
                frame_desc.size = ui_size(random_size_axis_rel(ui_size_anchor_width), random_size_axis_rel(ui_size_anchor_height));
            } else {
                frame_desc.pos = ui_pos(random_pos_axis(), random_pos_axis());
                frame_desc.size = ui_size_fix((double)(random_int(150) + 20), (double)(random_int(150) + 20));
            }
        }

        struct ui_node child = ui_frame(&frame_desc);

        count += 1 + generate_random_tree(child, current_depth + 1, max_depth);
    }

    return count;
}

static struct ui_node build_random_tree(unsigned int seed, int max_depth, uint32_t* out_node_count) {
    random_state = seed;

    struct ui_frame root_desc = {
        .size = ui_size_fix(1024, 1024),
    };

    random_layout(&root_desc.layout);

    struct ui_node root = ui_frame(&root_desc);

    uint32_t count = 1 + generate_random_tree(root, 0, max_depth);

    if (out_node_count) {
        *out_node_count = count;
    }

    return root;
}

static bool test_ui_random_tree(unsigned int seed) {
    double saved_scale = ui_get_to_px_from_pt();
    struct ui_node root = build_random_tree(seed, 4, NULL);

    ui_set_to_px_from_pt(144.0f);
    ui_layout(root);

    void* yoga_root = ui_yoga(root);
    ui_yoga_layout(yoga_root);

    bool is_equal = ui_yoga_is_equal(root, yoga_root);

    ui_yoga_release(yoga_root);

    ui_set_to_px_from_pt(2.5f);
    ui_layout(root);

    ui_set_to_px_from_pt(saved_scale);
    ui_release(root);

    return is_equal;
}

static double time_ui_layout_us(struct ui_node root) {
    uint32_t reps = 1;
    double elapsed;

    while (1) {
        double t0 = cr_get_current_time();
        for (uint32_t i = 0; i < reps; i++) {
            ui_layout(root);
        }
        elapsed = cr_get_current_time() - t0;
        if (elapsed >= 100e-6 || reps >= (1u << 20)) {
            break;
        }
        reps *= 2;
    }

    double best = elapsed / (double)reps;

    for (uint32_t sample = 0; sample < 24; sample++) {
        double t0 = cr_get_current_time();
        for (uint32_t i = 0; i < reps; i++) {
            ui_layout(root);
        }
        best = min_double(best, (cr_get_current_time() - t0) / (double)reps);
    }

    return best * 1e6;
}

struct perf_cold {
    unsigned int seed;
    int max_depth;
    struct ui_node root;
};

static void perf_cold_ui(struct perf_cold* p) {
    struct ui_node root = build_random_tree(p->seed, p->max_depth, NULL);
    ui_layout(root);
    ui_release(root);
}

static void perf_cold_yoga(struct perf_cold* p) {
    void* yoga_root = ui_yoga(p->root);
    ui_yoga_layout(yoga_root);
    ui_yoga_release(yoga_root);
}

static double time_cold_us(void (*step)(struct perf_cold*), struct perf_cold* p, uint32_t iterations) {
    uint32_t reps = 1;
    while (reps < iterations) {
        double t0 = cr_get_current_time();
        for (uint32_t i = 0; i < reps; i++) {
            step(p);
        }
        if (cr_get_current_time() - t0 >= 100e-6) {
            break;
        }
        reps *= 2;
    }

    double best = 1e9;
    for (uint32_t done = 0; done < iterations; done += reps) {
        double t0 = cr_get_current_time();
        for (uint32_t i = 0; i < reps; i++) {
            step(p);
        }
        best = min_double(best, (cr_get_current_time() - t0) / (double)reps);
    }

    return best * 1e6;
}

static void test_ui_perf(unsigned int seed, int max_depth, uint32_t iterations) {
    uint32_t node_count = 0;
    struct ui_node root = build_random_tree(seed, max_depth, &node_count);

    double layout_us = time_ui_layout_us(root);

    struct perf_cold p = { .seed = seed, .max_depth = max_depth, .root = root };
    double ui_cold_us = time_cold_us(perf_cold_ui, &p, iterations);
    double yoga_cold_us = time_cold_us(perf_cold_yoga, &p, iterations);

    ui_release(root);

    printf("layout perf %u nodes: ui %.1f us, ui cold %.1f us, yoga cold %.1f us\n", node_count, layout_us, ui_cold_us, yoga_cold_us);
}

static const char* test_ui_paragraph(void) {
    static char buf[4096];

    if (buf[0] == 0) {
        uint32_t offset = 0;
        while (offset + 128 < sizeof(buf)) {
            offset += (uint32_t)snprintf(buf + offset, sizeof(buf) - offset, "Sphinx of black quartz judge my vow %u. The five boxing wizards jump quickly %u. ", offset, offset * 7);
        }
    }

    return buf;
}

static struct ui_node build_labels_tree(uint32_t rows, uint32_t* out_node_count) {
    struct ui_node root = ui_frame(&(struct ui_frame) {
        .size = ui_size(ui_size_axis_fix(800.0f), ui_size_axis_fit()),
        .layout = ui_layout_stack_y(ui_dist_start, ui_align_start, 4.0f, 12.0f),
    });

    for (uint32_t i = 0; i < rows; i++) {
        struct ui_node row = ui_frame(&(struct ui_frame) {
            .parent = root,
            .size = ui_size(ui_size_axis_fill(), ui_size_axis_fit()),
            .layout = ui_layout_stack_x(ui_dist_start, ui_align_center, 8.0f, 0.0f),
        });

        char label[64];
        snprintf(label, sizeof(label), "Row %u", i);
        ui_text(&(struct ui_text) {
            .parent = row,
            .str = ui_str(label, ui_white, 13.0f),
        });

        snprintf(label, sizeof(label), "value %u units and change", i * 37);
        ui_text(&(struct ui_text) {
            .parent = row,
            .str = ui_str(label, ui_white, 13.0f),
        });
    }

    if (out_node_count) {
        *out_node_count = 1 + rows * 3;
    }

    return root;
}

static struct ui_node build_paragraph_tree(const struct ui_str* str, uint32_t* out_node_count) {
    struct ui_node root = ui_frame(&(struct ui_frame) {
        .size = ui_size(ui_size_axis_fix(400.0f), ui_size_axis_fit()),
        .layout = ui_layout_stack_y(ui_dist_start, ui_align_start, 0.0f, 0.0f),
    });

    ui_text(&(struct ui_text) {
        .parent = root,
        .size.width = ui_size_axis_fill(),
        .str = *str,
    });

    if (out_node_count) {
        *out_node_count = 2;
    }

    return root;
}

static struct ui_str test_ui_attributed_str(void) {
    const char* raw = test_ui_paragraph();
    uint32_t raw_size = (uint32_t)strlen(raw);

    enum { test_ui_span_count = 96 };
    static struct ui_str_attribute spans[test_ui_span_count];

    uint32_t stride = (raw_size - 32) / test_ui_span_count;

    for (uint32_t i = 0; i < test_ui_span_count; i++) {
        uint32_t start = 8 + i * stride;
        if (i % 2 == 0) {
            spans[i] = (struct ui_str_attribute) { .start = start, .end = start + 11, .type = ui_str_attribute_type_pt_size, .pt_size = 16.0f };
        } else {
            spans[i] = (struct ui_str_attribute) { .start = start, .end = start + 11, .type = ui_str_attribute_type_weight, .weight = 700.0f };
        }
    }

    return (struct ui_str) {
        .raw = raw,
        .attributes = {
            { .start = 0, .end = uint32_max, .type = ui_str_attribute_type_color, .color = ui_white },
            { .start = 0, .end = uint32_max, .type = ui_str_attribute_type_pt_size, .pt_size = 13.0f },
        },
        .extra_attributes = spans,
        .extra_attribute_count = test_ui_span_count,
    };
}

static struct ui_node build_chain_tree(bool is_x, bool with_text, uint32_t depth, uint32_t* out_node_count) {
    struct ui_node root = ui_frame(&(struct ui_frame) {
        .size = ui_size(ui_size_axis_fix(600.0f), is_x ? ui_size_axis_fix(600.0f) : ui_size_axis_fit()),
        .layout = is_x ? ui_layout_stack_x(ui_dist_start, ui_align_start, 0.0f, 0.0f) : ui_layout_stack_y(ui_dist_start, ui_align_start, 0.0f, 0.0f),
    });

    struct ui_node parent = root;

    for (uint32_t i = 0; i < depth; i++) {
        parent = ui_frame(&(struct ui_frame) {
            .parent = parent,
            .size = ui_size(is_x ? ui_size_axis_fit() : ui_size_axis_fill(), is_x ? ui_size_axis_fill() : ui_size_axis_fit()),
            .layout = is_x ? ui_layout_stack_x(ui_dist_start, ui_align_start, 0.0f, 0.0f) : ui_layout_stack_y(ui_dist_start, ui_align_start, 0.0f, 0.0f),
        });
    }

    if (with_text) {
        ui_text(&(struct ui_text) {
            .parent = parent,
            .size.width = ui_size_axis_fill(),
            .str = ui_str(test_ui_paragraph(), ui_white, 13.0f),
        });
    } else {
        ui_frame(&(struct ui_frame) {
            .parent = parent,
            .size = ui_size_fix(40.0f, 20.0f),
        });
    }

    if (out_node_count) {
        *out_node_count = depth + 2;
    }

    return root;
}

static struct ui_node build_chain_y_tree(uint32_t depth, uint32_t* out_node_count) {
    return build_chain_tree(false, false, depth, out_node_count);
}

static struct ui_node build_chain_y_text_tree(uint32_t depth, uint32_t* out_node_count) {
    return build_chain_tree(false, true, depth, out_node_count);
}

static struct ui_node build_chain_x_tree(uint32_t depth, uint32_t* out_node_count) {
    return build_chain_tree(true, false, depth, out_node_count);
}

static struct ui_node build_flat_tree(uint32_t count, uint32_t* out_node_count) {
    struct ui_node root = ui_frame(&(struct ui_frame) {
        .size = ui_size(ui_size_axis_fix(600.0f), ui_size_axis_fit()),
        .layout = ui_layout_stack_y(ui_dist_start, ui_align_start, 1.0f, 0.0f),
    });

    for (uint32_t i = 0; i < count; i++) {
        ui_frame(&(struct ui_frame) {
            .parent = root,
            .size = ui_size_fix(50.0f, 8.0f),
        });
    }

    if (out_node_count) {
        *out_node_count = count + 1;
    }

    return root;
}

static struct ui_node build_fill_grid_tree(uint32_t rows, uint32_t* out_node_count) {
    struct ui_node root = ui_frame(&(struct ui_frame) {
        .size = ui_size_fix(600.0f, 800.0f),
        .layout = ui_layout_stack_y(ui_dist_start, ui_align_start, 0.0f, 0.0f),
    });

    for (uint32_t i = 0; i < rows; i++) {
        struct ui_node row = ui_frame(&(struct ui_frame) {
            .parent = root,
            .size = ui_size_fill(),
            .layout = ui_layout_stack_x(ui_dist_start, ui_align_start, 2.0f, 0.0f),
        });

        for (uint32_t j = 0; j < 3; j++) {
            ui_frame(&(struct ui_frame) {
                .parent = row,
                .size = ui_size_fill(),
            });
        }
    }

    if (out_node_count) {
        *out_node_count = 1 + rows * 4;
    }

    return root;
}

static void test_ui_perf_scaling(void) {
    struct {
        const char* label;
        struct ui_node (*build)(uint32_t n, uint32_t* out_node_count);
        uint32_t n_a;
        uint32_t n_b;
    } cases[] = {
        { "chain_y", build_chain_y_tree, 256, 1024 },
        { "chain_y_text", build_chain_y_text_tree, 256, 1024 },
        { "chain_x", build_chain_x_tree, 256, 1024 },
        { "flat", build_flat_tree, 512, 2048 },
        { "fill_grid", build_fill_grid_tree, 128, 512 },
        { "labels", build_labels_tree, 75, 300 },
    };

    for (uint32_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        uint32_t count_a = 0;
        struct ui_node root_a = cases[i].build(cases[i].n_a, &count_a);
        double us_a = time_ui_layout_us(root_a);
        ui_release(root_a);

        uint32_t count_b = 0;
        struct ui_node root_b = cases[i].build(cases[i].n_b, &count_b);
        double us_b = time_ui_layout_us(root_b);
        ui_release(root_b);

        double per_a = us_a * 1000.0 / (double)count_a;
        double per_b = us_b * 1000.0 / (double)count_b;

        printf("layout perf scaling %s: %u nodes %.1f us (%.0f ns/node), %u nodes %.1f us (%.0f ns/node), per-node ratio %.2f\n",
               cases[i].label, count_a, us_a, per_a, count_b, us_b, per_b, per_b / per_a);
    }
}

static void test_ui_perf_text_one(const char* label, struct ui_node root, uint32_t node_count) {
    double layout_us = time_ui_layout_us(root);
    ui_release(root);

    printf("layout perf text %s: %u nodes, ui %.1f us\n", label, node_count, layout_us);
}

static void test_ui_perf_text(void) {
    uint32_t node_count;
    struct ui_node root;

    root = build_labels_tree(300, &node_count);
    test_ui_perf_text_one("labels", root, node_count);

    struct ui_str paragraph = ui_str(test_ui_paragraph(), ui_white, 13.0f);
    root = build_paragraph_tree(&paragraph, &node_count);
    test_ui_perf_text_one("paragraph", root, node_count);

    struct ui_str attributed = test_ui_attributed_str();
    root = build_paragraph_tree(&attributed, &node_count);
    test_ui_perf_text_one("attributed", root, node_count);
}

void ui_test_yoga_(void) {
    uint32_t fail_count = 0;
    int32_t first_fail = -1;

    for (int i = 0; i < 1024; i++) {
        if (test_ui_random_tree((unsigned int)i) == false) {
            fail_count++;
            if (first_fail < 0) {
                first_fail = i;
            }
        }
    }

    if (fail_count == 0) {
        test_ui_perf(1, 2, 4096);
        test_ui_perf(1, 4, 1024);
        test_ui_perf(1, 7, 128);
        test_ui_perf_text();
        test_ui_perf_scaling();
    }


    if (fail_count > 0) {
        printf("ui_test_yoga: FAILED (%u/1024 seeds mismatched, first seed %d)\n", fail_count, first_fail);
    }
    assert(fail_count == 0);

    printf("ui_test_yoga: ok\n");
}

#include "ui_core.h"
#include "ui_physics.h"
#include <stdio.h>

static void ui_test_check(bool ok, const char* label) {
    ui_test_group_check_("layout", ok, label);
}

static void ui_test_offset_pixel_grid(void) {
    double saved_scale = ui_get_to_px_from_pt();
    ui_set_to_px_from_pt(3.0f);

    ui_test_check(fabs(ui_round_px(10.37) * 3.0 - 31.0) < 1e-9, "round px snaps to pixel grid");

    struct ui_scroll scroll;
    ui_scroll_init(&scroll);
    scroll.offset = 123.456;
    ui_test_check(fabs(ui_scroll_get_offset(&scroll) * 3.0 - 370.0) < 1e-9, "scroll offset presents on pixel grid");

    struct ui_slide slide;
    ui_slide_init(&slide, ui_slide_mode_none, ui_slide_mode_none);
    slide.offset = -45.678;
    ui_test_check(fabs(ui_slide_get_offset(&slide) * 3.0 + 137.0) < 1e-9, "slide offset presents on pixel grid");

    ui_set_to_px_from_pt(saved_scale);
}

static void ui_test_release_velocity(void) {
    double dt = 1.0 / 60.0;
    double finger_velocity = -2500.0;

    struct ui_slide slide;
    ui_slide_init(&slide, ui_slide_mode_trigger, ui_slide_mode_actions);
    ui_slide_on_drag(&slide, ui_gesture_phase_began, 0.0, 0.0);
    ui_slide_on_update(&slide, 320.0, 76.0, dt);

    double slide_prev = slide.offset;
    double slide_release = slide.offset;

    for (uint32_t i = 1; i <= 7; i++) {
        ui_slide_on_drag(&slide, ui_gesture_phase_changed, finger_velocity * dt * (double)i, finger_velocity);
        ui_slide_on_update(&slide, 320.0, 76.0, dt);
        slide_prev = slide_release;
        slide_release = slide.offset;
    }

    double slide_step = fabs(slide_release - slide_prev);
    double slide_outward = 0.0;

    ui_slide_on_drag(&slide, ui_gesture_phase_ended, finger_velocity * dt * 7.0, finger_velocity);

    for (uint32_t i = 0; i < 120; i++) {
        double before = slide.offset;

        ui_slide_on_update(&slide, 320.0, 76.0, dt);
        slide_outward = max_double(slide_outward, before - slide.offset);
    }

    ui_test_check(slide_outward <= slide_step, "slide release keeps the visible drag speed");
    ui_test_check(slide.phase == ui_slide_phase_idle && slide.offset == -76.0, "slide release settles on the extension");

    struct ui_scroll scroll;
    ui_scroll_init(&scroll);
    scroll.offset = 600.0;

    double timestamp = 0.0;
    ui_scroll_on_drag(&scroll, ui_gesture_phase_began, 0.0, 0.0, timestamp);
    ui_scroll_on_update(&scroll, scroll.offset, scroll.offset + 400.0, 0.0, 1000.0, dt);

    double scroll_prev = scroll.offset;
    double scroll_release = scroll.offset;

    for (uint32_t i = 1; i <= 7; i++) {
        timestamp += dt;
        ui_scroll_on_drag(&scroll, ui_gesture_phase_changed, finger_velocity * dt * (double)i, finger_velocity, timestamp);
        ui_scroll_on_update(&scroll, scroll.offset, scroll.offset + 400.0, 0.0, 1000.0, dt);
        scroll_prev = scroll_release;
        scroll_release = scroll.offset;
    }

    double scroll_step = fabs(scroll_release - scroll_prev);
    double scroll_outward = 0.0;

    ui_scroll_on_drag(&scroll, ui_gesture_phase_ended, finger_velocity * dt * 7.0, finger_velocity, timestamp);

    for (uint32_t i = 0; i < 120; i++) {
        double before = scroll.offset;

        ui_scroll_on_update(&scroll, scroll.offset, scroll.offset + 400.0, 0.0, 1000.0, dt);
        scroll_outward = max_double(scroll_outward, scroll.offset - before);
    }

    ui_test_check(scroll_outward <= scroll_step, "scroll release keeps the visible drag speed");
    ui_test_check(scroll.phase == ui_scroll_phase_idle && scroll.offset == 600.0, "scroll release settles on the edge");
}

static void ui_test_scroll_step(struct ui_scroll* scroll, double view, double content, double dt) {
    ui_scroll_on_update(scroll, scroll->offset, scroll->offset + view, 0.0, content, dt);
}

static uint32_t ui_test_scroll_settle(struct ui_scroll* scroll, double view, double content, double dt, double* out_low, double* out_high) {
    uint32_t frames = 0;

    *out_low = scroll->offset;
    *out_high = scroll->offset;

    while (scroll->phase != ui_scroll_phase_idle && frames < 600) {
        ui_test_scroll_step(scroll, view, content, dt);
        *out_low = min_double(*out_low, scroll->offset);
        *out_high = max_double(*out_high, scroll->offset);
        frames++;
    }

    return frames;
}

static void ui_test_scroll_paths(void) {
    double saved_scale = ui_get_to_px_from_pt();
    double dt = 1.0 / 120.0;
    double low;
    double high;
    struct ui_scroll scroll;

    ui_scroll_init(&scroll);
    ui_test_check(ui_scroll_on_drag(&scroll, ui_gesture_phase_possible, 0.0, 0.0, 1.0) == false && scroll.phase == ui_scroll_phase_idle, "a touch-down on a resting scroll is not claimed");

    scroll.offset = 100.0;
    scroll.velocity = 2000.0;
    scroll.phase = ui_scroll_phase_decelerating;
    ui_test_scroll_step(&scroll, 400.0, 1000.0, dt);

    double held = scroll.offset;
    bool claimed = ui_scroll_on_drag(&scroll, ui_gesture_phase_possible, 0.0, 0.0, 1.1);
    ui_test_scroll_step(&scroll, 400.0, 1000.0, dt);
    ui_test_check(claimed && scroll.phase == ui_scroll_phase_holding && scroll.offset == held && scroll.velocity == 0.0, "a touch-down holds a decelerating scroll on the same frame");

    ui_scroll_on_drag(&scroll, ui_gesture_phase_failed, 0.0, 0.0, 1.2);
    ui_test_scroll_step(&scroll, 400.0, 1000.0, dt);
    ui_test_check(scroll.phase == ui_scroll_phase_idle && scroll.offset == held, "lifting the holding touch leaves the scroll at rest");

    ui_scroll_init(&scroll);
    scroll.offset = -50.0;
    ui_scroll_on_drag(&scroll, ui_gesture_phase_began, 10.0, 0.0, 2.0);
    ui_test_scroll_step(&scroll, 400.0, 1000.0, dt);
    ui_test_check(fabs(scroll.offset + 50.0) < 1.0e-9, "the first tracking frame keeps an overscrolled offset");

    ui_scroll_on_drag(&scroll, ui_gesture_phase_changed, 40.0, 0.0, 2.0 + dt);
    ui_test_scroll_step(&scroll, 400.0, 1000.0, dt);
    ui_test_check(scroll.offset < -50.0 && scroll.offset > -80.0, "overscroll stretches less than the finger moves");

    ui_scroll_init(&scroll);
    scroll.offset = 100.0;
    ui_scroll_on_drag(&scroll, ui_gesture_phase_began, 0.0, 0.0, 3.0);
    ui_test_scroll_step(&scroll, 400.0, 1000.0, dt);
    ui_scroll_on_drag(&scroll, ui_gesture_phase_changed, -40.0, 0.0, 3.0 + dt);
    ui_test_scroll_step(&scroll, 400.0, 1000.0, dt);
    ui_test_check(scroll.offset == 140.0, "a drag inside the content tracks the finger exactly");

    ui_scroll_on_drag(&scroll, ui_gesture_phase_ended, -40.0, 0.0, 3.0 + dt);
    ui_test_scroll_step(&scroll, 400.0, 1000.0, dt);
    ui_test_check(scroll.phase == ui_scroll_phase_idle && scroll.offset == 140.0, "releasing a resting drag inside the content idles at once");

    ui_scroll_init(&scroll);
    scroll.offset = 100.0;
    ui_scroll_on_drag(&scroll, ui_gesture_phase_began, 0.0, 0.0, 4.0);
    ui_test_scroll_step(&scroll, 400.0, 1000.0, dt);
    ui_scroll_on_drag(&scroll, ui_gesture_phase_changed, -40.0, -600.0, 4.0 + dt);
    ui_test_scroll_step(&scroll, 400.0, 1000.0, dt);
    ui_scroll_on_drag(&scroll, ui_gesture_phase_ended, -40.0, -600.0, 4.0 + dt);
    ui_test_scroll_step(&scroll, 400.0, 1000.0, dt);
    ui_test_check(scroll.phase == ui_scroll_phase_decelerating && scroll.offset > 140.0, "a fling inside the content starts decelerating on the release frame");

    double released = scroll.offset;
    uint32_t frames = ui_test_scroll_settle(&scroll, 400.0, 1000.0, dt, &low, &high);
    ui_test_check(low == released && scroll.offset == high && scroll.offset > 400.0 && scroll.offset < 450.0 && frames < 600, "a fling inside the content glides forward and comes to rest inside");

    ui_scroll_init(&scroll);
    scroll.offset = 595.0;
    scroll.velocity = 2400.0;
    scroll.phase = ui_scroll_phase_decelerating;
    ui_test_scroll_step(&scroll, 400.0, 1000.0, dt);
    ui_test_check(scroll.offset > 600.0 && scroll.offset < 600.0 + 15.0 && scroll.velocity > 0.0, "a glide that crosses the edge mid-frame hands the rest of the frame to the bounce");

    frames = ui_test_scroll_settle(&scroll, 400.0, 1000.0, dt, &low, &high);
    ui_test_check(low >= 600.0 && high < 700.0 && scroll.offset == 600.0 && frames < 150, "the bounce returns to the edge exactly without crossing back");

    ui_scroll_init(&scroll);
    scroll.offset = 650.0;
    scroll.velocity = -1500.0;
    scroll.phase = ui_scroll_phase_decelerating;

    bool reentered = false;
    bool glided_on = false;

    for (uint32_t i = 0; i < 600 && scroll.phase != ui_scroll_phase_idle; i++) {
        double before = scroll.offset;

        ui_test_scroll_step(&scroll, 400.0, 1000.0, dt);

        if (scroll.offset < 600.0 && scroll.phase == ui_scroll_phase_decelerating) {
            reentered = true;
        }
        if (reentered && before < 600.0 && scroll.offset < before) {
            glided_on = true;
        }
    }
    ui_test_check(reentered && glided_on && scroll.phase == ui_scroll_phase_idle && scroll.offset > 0.0 && scroll.offset < 600.0, "a bounce flung back into the content keeps scrolling inside");

    ui_scroll_init(&scroll);
    scroll.offset = 590.0;
    ui_scroll_on_wheel(&scroll, 30.0);
    ui_test_scroll_step(&scroll, 400.0, 1000.0, dt);
    ui_test_check(scroll.offset == 600.0 && scroll.phase == ui_scroll_phase_idle, "a wheel delta stops at the edge");

    scroll.offset = 100.0;
    scroll.velocity = 2000.0;
    scroll.phase = ui_scroll_phase_decelerating;
    ui_scroll_on_wheel(&scroll, 10.0);
    ui_test_scroll_step(&scroll, 400.0, 1000.0, dt);
    ui_test_check(scroll.offset == 110.0 && scroll.velocity == 0.0 && scroll.phase == ui_scroll_phase_idle, "a wheel delta ends a glide and moves by the delta alone");

    scroll.offset = 620.0;
    scroll.phase = ui_scroll_phase_decelerating;
    ui_scroll_on_wheel(&scroll, 50.0);
    ui_test_scroll_step(&scroll, 400.0, 1000.0, dt);
    ui_test_check(scroll.offset == 620.0 && scroll.phase == ui_scroll_phase_idle, "a wheel delta cannot push a bounce further out");

    ui_test_scroll_step(&scroll, 400.0, 1000.0, dt);
    ui_test_check(scroll.phase == ui_scroll_phase_decelerating && scroll.offset < 620.0, "an idle overscroll starts springing back on the next frame");

    frames = ui_test_scroll_settle(&scroll, 400.0, 1000.0, dt, &low, &high);
    ui_test_check(scroll.offset == 600.0 && low == 600.0 && frames < 150, "an idle overscroll settles on the edge exactly");

    ui_scroll_init(&scroll);
    scroll.offset = 500.0;
    ui_test_scroll_step(&scroll, 400.0, 1000.0, dt);
    ui_test_check(scroll.phase == ui_scroll_phase_idle && scroll.offset == 500.0, "content inside its bounds rests");

    ui_test_scroll_step(&scroll, 400.0, 700.0, dt);
    ui_test_check(scroll.phase == ui_scroll_phase_decelerating && scroll.offset < 500.0, "content left outside by a size change starts springing back the same frame");

    frames = ui_test_scroll_settle(&scroll, 400.0, 700.0, dt, &low, &high);
    ui_test_check(scroll.offset == 300.0 && low == 300.0 && frames < 150, "a size change settles on the new edge exactly");

    ui_scroll_init(&scroll);
    ui_scroll_on_drag(&scroll, ui_gesture_phase_began, 0.0, 0.0, 5.0);
    ui_test_scroll_step(&scroll, 400.0, 300.0, dt);
    ui_scroll_on_drag(&scroll, ui_gesture_phase_changed, 50.0, 0.0, 5.0 + dt);
    ui_test_scroll_step(&scroll, 400.0, 300.0, dt);
    ui_test_check(scroll.offset < 0.0 && scroll.offset > -50.0, "content shorter than the view still rubber bands");

    ui_scroll_on_drag(&scroll, ui_gesture_phase_ended, 50.0, 0.0, 5.0 + dt);
    ui_test_scroll_step(&scroll, 400.0, 300.0, dt);
    frames = ui_test_scroll_settle(&scroll, 400.0, 300.0, dt, &low, &high);
    ui_test_check(scroll.offset == 0.0 && high == 0.0 && frames < 150, "content shorter than the view settles at its start");

    ui_scroll_init(&scroll);
    ui_scroll_on_drag(&scroll, ui_gesture_phase_began, 0.0, 0.0, 6.0);
    ui_test_scroll_step(&scroll, 0.0, 1000.0, dt);
    ui_scroll_on_drag(&scroll, ui_gesture_phase_changed, 50.0, 0.0, 6.0 + dt);
    ui_test_scroll_step(&scroll, 0.0, 1000.0, dt);
    ui_test_check(scroll.offset == 0.0, "a zero-length view pins the content at its edge");

    ui_scroll_init(&scroll);
    scroll.offset = 100.0;
    scroll.velocity = 2000.0;
    scroll.phase = ui_scroll_phase_decelerating;
    ui_test_scroll_step(&scroll, 400.0, 1000.0, 0.0);
    ui_test_check(scroll.offset == 100.0 && scroll.velocity == 2000.0 && scroll.phase == ui_scroll_phase_decelerating, "a zero time step changes nothing");

    ui_test_scroll_step(&scroll, 400.0, 1000.0, 2.0);
    ui_test_check(scroll.offset == 600.0 && scroll.phase == ui_scroll_phase_idle, "a long time step completes the glide and the bounce at once");

    ui_set_to_px_from_pt(2.0f);
    scroll.offset = 123.3;
    ui_test_check(ui_scroll_get_offset(&scroll) == 123.5, "the presented offset snaps to the pixel grid");
    ui_set_to_px_from_pt(saved_scale);
}

static void ui_test_slide_on_slide(void* user) {
    uint32_t* count = user;
    *count += 1;
}

static uint32_t ui_test_slide_settle(struct ui_slide* slide, double dt) {
    uint32_t frames = 0;

    while (slide->phase != ui_slide_phase_idle && frames < 600) {
        ui_slide_on_update(slide, 320.0, 76.0, dt);
        frames++;
    }

    return frames;
}

static void ui_test_slide_paths(void) {
    double dt = 1.0 / 60.0;
    uint32_t slides = 0;
    struct ui_slide slide;

    ui_slide_init(&slide, ui_slide_mode_trigger, ui_slide_mode_actions);
    ui_slide_set_on_slide(&slide, ui_test_slide_on_slide, &slides);

    ui_slide_on_drag(&slide, ui_gesture_phase_began, 5.0, 0.0);
    ui_slide_on_update(&slide, 320.0, 76.0, dt);
    ui_slide_on_drag(&slide, ui_gesture_phase_changed, 200.0, 0.0);
    ui_slide_on_update(&slide, 320.0, 76.0, dt);
    ui_test_check(slide.offset > 0.0 && slide.offset < 200.0, "a trigger swipe rubber bands from the start");

    ui_slide_on_drag(&slide, ui_gesture_phase_ended, 200.0, 0.0);
    ui_slide_on_update(&slide, 320.0, 76.0, dt);
    uint32_t frames = ui_test_slide_settle(&slide, dt);
    ui_test_check(slides == 1 && slide.is_extended == false && slide.offset == 0.0 && frames < 90, "a far trigger swipe fires once and snaps home");

    ui_slide_on_drag(&slide, ui_gesture_phase_began, 5.0, 0.0);
    ui_slide_on_update(&slide, 320.0, 76.0, dt);
    ui_slide_on_drag(&slide, ui_gesture_phase_changed, 40.0, 1500.0);
    ui_slide_on_update(&slide, 320.0, 76.0, dt);
    ui_slide_on_drag(&slide, ui_gesture_phase_ended, 40.0, 1500.0);
    ui_slide_on_update(&slide, 320.0, 76.0, dt);
    ui_test_slide_settle(&slide, dt);
    ui_test_check(slides == 2 && slide.offset == 0.0, "a fast trigger swipe fires from a short distance");

    ui_slide_on_drag(&slide, ui_gesture_phase_began, 5.0, 0.0);
    ui_slide_on_update(&slide, 320.0, 76.0, dt);
    ui_slide_on_drag(&slide, ui_gesture_phase_changed, -100.0, 0.0);
    ui_slide_on_update(&slide, 320.0, 76.0, dt);
    ui_test_check(slide.offset < 0.0 && slide.offset > -48.0, "swiping against the chosen side is held by the stiff band");

    ui_slide_on_drag(&slide, ui_gesture_phase_ended, -100.0, 0.0);
    ui_slide_on_update(&slide, 320.0, 76.0, dt);
    ui_test_slide_settle(&slide, dt);
    ui_test_check(slides == 2 && slide.offset == 0.0, "swiping against the chosen side neither fires nor extends");

    ui_slide_on_drag(&slide, ui_gesture_phase_began, -5.0, 0.0);
    ui_slide_on_update(&slide, 320.0, 76.0, dt);
    ui_slide_on_drag(&slide, ui_gesture_phase_changed, -60.0, 0.0);
    ui_slide_on_update(&slide, 320.0, 76.0, dt);
    ui_test_check(slide.offset == -55.0, "an actions swipe tracks the finger inside the extension");

    ui_slide_on_drag(&slide, ui_gesture_phase_ended, -60.0, 0.0);
    ui_slide_on_update(&slide, 320.0, 76.0, dt);
    frames = ui_test_slide_settle(&slide, dt);
    ui_test_check(slide.is_extended && slide.offset == -76.0 && frames < 90, "an actions swipe past half the extension settles extended");

    ui_slide_on_drag(&slide, ui_gesture_phase_began, 0.0, 0.0);
    ui_slide_on_update(&slide, 320.0, 76.0, dt);
    ui_test_check(slide.is_right == false && fabs(slide.offset + 76.0) < 1.0e-9, "a drag on an extended row keeps its side and position");

    ui_slide_on_drag(&slide, ui_gesture_phase_changed, 30.0, 800.0);
    ui_slide_on_update(&slide, 320.0, 76.0, dt);
    ui_slide_on_drag(&slide, ui_gesture_phase_ended, 30.0, 800.0);
    ui_slide_on_update(&slide, 320.0, 76.0, dt);
    ui_test_slide_settle(&slide, dt);
    ui_test_check(slide.is_extended == false && slide.offset == 0.0, "a flick back closes the extension");

    ui_slide_on_drag(&slide, ui_gesture_phase_began, -5.0, 0.0);
    ui_slide_on_update(&slide, 320.0, 76.0, dt);
    ui_slide_on_drag(&slide, ui_gesture_phase_changed, -60.0, 0.0);
    ui_slide_on_update(&slide, 320.0, 76.0, dt);
    ui_slide_on_drag(&slide, ui_gesture_phase_canceled, -60.0, 0.0);
    ui_slide_on_update(&slide, 320.0, 76.0, dt);
    ui_test_slide_settle(&slide, dt);
    ui_test_check(slides == 2 && slide.is_extended == false && slide.offset == 0.0, "a canceled swipe snaps back without deciding anything");

    ui_slide_on_drag(&slide, ui_gesture_phase_began, -5.0, 0.0);
    ui_slide_on_update(&slide, 320.0, 76.0, dt);
    ui_slide_on_drag(&slide, ui_gesture_phase_changed, -300.0, -100000.0);
    ui_slide_on_update(&slide, 320.0, 76.0, dt);
    ui_slide_on_drag(&slide, ui_gesture_phase_ended, -300.0, -100000.0);

    double before = slide.offset;
    ui_slide_on_update(&slide, 320.0, 76.0, dt);
    ui_test_check(before - slide.offset <= 2500.0 * dt && fabs(slide.velocity) <= 2500.0, "an absurd release speed is clamped");

    ui_test_slide_settle(&slide, dt);
    ui_test_check(slide.is_extended && slide.offset == -76.0, "a fast actions swipe settles extended");
}

static void ui_test_layout_pixel_grid(void) {
    double saved_scale = ui_get_to_px_from_pt();
    ui_set_to_px_from_pt(3.0f);

    struct ui_node root = ui_frame(&(struct ui_frame) {
        .size = ui_size_fix(181.3f, 400.7f),
        .layout = ui_layout_stack_y(ui_dist_start, ui_align_start, 8.6f, ui_pad_xy(7.3f, 9.9f)),
    });

    struct ui_node children[3];
    for (uint32_t i = 0; i < cr_count(children); i++) {
        children[i] = ui_frame(&(struct ui_frame) {
            .parent = root,
            .size = ui_size_fix(120.2f, 27.3f),
        });
    }

    struct ui_node hairline = ui_frame(&(struct ui_frame) {
        .parent = root,
        .size = ui_size_fix(120.2f, 0.1f),
    });

    ui_layout(root);

    long edges[cr_count(children)][2];
    long widths[cr_count(children)];

    for (uint32_t i = 0; i < cr_count(children); i++) {
        double2 child_origin = ui_get_origin_from_root(root, children[i]);
        double2 child_size = ui_get_size(children[i]);
        edges[i][0] = lround(child_origin.y * 3.0);
        edges[i][1] = lround((child_origin.y + child_size.y) * 3.0);
        widths[i] = lround((child_origin.x + child_size.x) * 3.0) - lround(child_origin.x * 3.0);
    }

    bool heights_ok = true;
    bool gaps_ok = true;
    bool widths_ok = true;

    for (uint32_t i = 0; i < cr_count(children); i++) {
        heights_ok = heights_ok && (edges[i][1] - edges[i][0]) == (edges[0][1] - edges[0][0]);
        widths_ok = widths_ok && widths[i] == widths[0];
    }
    for (uint32_t i = 0; i + 1 < cr_count(children); i++) {
        gaps_ok = gaps_ok && (edges[i + 1][0] - edges[i][1]) == (edges[1][0] - edges[0][1]);
    }

    ui_test_check(heights_ok, "pixel grid uniform stack heights");
    ui_test_check(widths_ok, "pixel grid uniform stack widths");
    ui_test_check(gaps_ok, "pixel grid uniform stack gaps");

    double2 hairline_origin = ui_get_origin_from_root(root, hairline);
    double2 hairline_size = ui_get_size(hairline);
    ui_test_check(lround((hairline_origin.y + hairline_size.y) * 3.0) - lround(hairline_origin.y * 3.0) == 1, "pixel grid keeps hairline visible");

    ui_release(root);
    ui_set_to_px_from_pt(saved_scale);
}

static void ui_test_node_alloc_balance(void) {
    uint32_t before = ui_get_node_alloc_count();

    struct ui_node root = ui_frame(&(struct ui_frame) {
        .size = ui_size_fix(100, 100),
        .layout = ui_layout_stack_y(ui_dist_start, ui_align_start, 4, 0),
    });

    for (uint32_t i = 0; i < 8; i++) {
        struct ui_node row = ui_frame(&(struct ui_frame) {
            .parent = root,
            .size = ui_size_fix(100, 10),
        });
        ui_text(&(struct ui_text) {
            .parent = row,
            .str = ui_str("row", ui_white, 10.0f),
        });
    }

    ui_layout(root);
    ui_test_check(ui_get_node_alloc_count() == before + 17, "node alloc count tracks tree");

    ui_release(root);
    ui_test_check(ui_get_node_alloc_count() == before, "node alloc count balances on release");
}

struct ui_test_scale_sweep {
    double saved_to_px_from_pt;
    uint64_t peak;
    uint32_t frame;
};

static void ui_test_scale_sweep_frames(void* args) {
    struct ui_test_scale_sweep* sweep = args;
    struct ui* ui = ui_ctx();

    if (sweep->frame == 180) {
        for (uint32_t i = 0; i < 200; i++) {
            ui_transient_frame_end(ui);
        }

        ui_test_check(sweep->peak <= (512ull << 20), "scale sweep stays within limit");
        return;
    }

    uint32_t end = min_uint32(sweep->frame + 8, 180);
    uint64_t serial = 0;

    for (uint32_t i = sweep->frame; i < end; i++) {
        float to_px_from_pt = 0.4f + (float)(i % 60) * 0.05f;
        uint32_t target_w = 1800 + (i % 23) * 16;
        uint32_t target_h = 1200 + (i % 19) * 16;

        ui_set_to_px_from_pt(to_px_from_pt);

        float2 root_size = make_float2((float)target_w / to_px_from_pt, (float)target_h / to_px_from_pt);

        struct ui_node root = ui_frame(&(struct ui_frame) {
            .size = ui_size_fix(root_size.x, root_size.y),
            .fill = ui_fill_solid(ui_gray20),
        });

        struct ui_node bar = ui_frame(&(struct ui_frame) {
            .parent = root,
            .size.width = ui_size_axis_width(),
            .size.height = ui_size_axis_fix(44),
            .layout = ui_layout_stack_x(ui_dist_center, ui_align_center, 0, 0),
            .fill.layers = { ui_layer_background_blur(10), ui_layer_solid(ui_gray_a(0.5f, 0.5f)) },
        });

        char label[64];
        snprintf(label, sizeof(label), "scale %.2f frame %u", to_px_from_pt, i);

        ui_text(&(struct ui_text) {
            .parent = bar,
            .str = ui_str(label, ui_white, 15),
        });

        struct ui_node panel = ui_frame(&(struct ui_frame) {
            .parent = root,
            .pos = ui_pos_center(),
            .size = ui_size_fix(360, 220),
            .corners = ui_corners(24),
            .fill = ui_fill_solid(ui_gray70),
            .shadow = ui_shadow(0, 8, 8, ui_gray_a(0, 0.4f)),
        });

        ui_text(&(struct ui_text) {
            .parent = panel,
            .str = ui_str(label, ui_gray10, 17),
        });

        ui_layout(root);

        struct gpu_texture* target = gpu_texture_create(ui->device, &(struct gpu_texture_desc) {
            .label = "test layout target",
            .format = gpu_format_bgra8,
            .size = { target_w, target_h },
            .usage = gpu_texture_usage_render_target,
        });

        cr_autoreleasepool {
            struct gpu_command_buffer* cb = gpu_command_buffer_create(ui->device, "test layout");
            ui_render(root, cb, target, (struct ui_render_desc) { 0 });
            serial = gpu_command_buffer_commit(cb);
        }

        gpu_texture_destroy(ui->device, target);
        ui_release(root);

        sweep->peak = max_uint64(sweep->peak, ui_transient_memory_used_(ui));
    }

    sweep->frame = end;
    ui_set_to_px_from_pt(sweep->saved_to_px_from_pt);
    ui_test_await_(serial);
    ui_test_then_(ui_test_scale_sweep_frames, sweep);
}

static void ui_test_scale_sweep(void) {
    static struct ui_test_scale_sweep sweep;
    sweep = (struct ui_test_scale_sweep) { .saved_to_px_from_pt = ui_ctx()->to_px_from_pt };
    ui_test_scale_sweep_frames(&sweep);
}

static void ui_test_layout_axes(void) {
    double saved_scale = ui_get_to_px_from_pt();
    ui_set_to_px_from_pt(1.0);

    struct ui_node root = ui_frame(&(struct ui_frame) {
        .size = ui_size_fix(200, 100),
    });

    struct ui_node corner = ui_frame(&(struct ui_frame) {
        .parent = root,
        .pos = ui_pos(ui_pos_axis_end(), ui_pos_axis_center()),
        .size = ui_size_fix(50, 20),
    });

    struct ui_node mixed = ui_frame(&(struct ui_frame) {
        .parent = root,
        .pos = ui_pos(ui_pos_axis_rel(ui_pos_anchor_center, ui_pos_anchor_start, 30), ui_pos_axis_rel(ui_pos_anchor_end, ui_pos_anchor_start, 0)),
        .size = ui_size_fix(20, 10),
    });

    struct ui_node relative = ui_frame(&(struct ui_frame) {
        .parent = root,
        .size = ui_size(ui_size_axis_rel(ui_size_anchor_width, 0.5, -10), ui_size_axis_height()),
    });

    struct ui_node filled = ui_frame(&(struct ui_frame) {
        .parent = root,
        .pos = ui_pos_center(),
        .size = ui_size_fill(),
    });

    struct ui_node square = ui_frame(&(struct ui_frame) {
        .parent = root,
        .size = ui_size(ui_size_axis_rel(ui_size_anchor_height, 1.0, 0.0), ui_size_axis_rel(ui_size_anchor_height, 0.5, 0.0)),
    });

    ui_layout(root);

    double2 corner_origin = ui_get_origin_from_root(root, corner);
    ui_test_check(corner_origin.x == 150.0 && corner_origin.y == 40.0, "axes pin end and center");

    double2 mixed_origin = ui_get_origin_from_root(root, mixed);
    ui_test_check(mixed_origin.x == 20.0 && mixed_origin.y == -10.0, "axes pin node anchor to parent anchor plus constant");

    double2 relative_size = ui_get_size(relative);
    ui_test_check(relative_size.x == 90.0 && relative_size.y == 100.0, "axes relative size uses multiplier and constant");

    double2 filled_origin = ui_get_origin_from_root(root, filled);
    double2 filled_size = ui_get_size(filled);
    ui_test_check(filled_origin.x == 0.0 && filled_origin.y == 0.0 && filled_size.x == 200.0 && filled_size.y == 100.0, "axes fill outside a stack takes the parent size");

    double2 square_size = ui_get_size(square);
    ui_test_check(square_size.x == 100.0 && square_size.y == 50.0, "axes relative size can reference the other dimension");

    ui_release(root);

    struct ui_node row = ui_frame(&(struct ui_frame) {
        .size = ui_size_fix(300, 50),
        .layout = ui_layout_stack_x(ui_dist_start, ui_align_start, 0, ui_pad_xy(10, 0)),
    });

    struct ui_node quarter = ui_frame(&(struct ui_frame) {
        .parent = row,
        .size = ui_size(ui_size_axis_rel(ui_size_anchor_width, 0.25, 0.0), ui_size_axis_rel(ui_size_anchor_height, 0.5, 0.0)),
    });

    struct ui_node rest = ui_frame(&(struct ui_frame) {
        .parent = row,
        .size = ui_size_fill(),
    });

    ui_layout(row);

    double2 quarter_origin = ui_get_origin_from_root(row, quarter);
    double2 quarter_size = ui_get_size(quarter);
    double2 rest_origin = ui_get_origin_from_root(row, rest);
    double2 rest_size = ui_get_size(rest);
    ui_test_check(quarter_origin.x == 0.0 && quarter_size.x == 75.0 && quarter_size.y == 25.0, "axes relative size leaves the stack flow");
    ui_test_check(rest_origin.x == 10.0 && rest_size.x == 280.0 && rest_size.y == 50.0, "axes fill ignores relative siblings");

    ui_release(row);
    ui_set_to_px_from_pt(saved_scale);
}

void ui_test_layout_(void) {

    ui_test_layout_pixel_grid();
    ui_test_layout_axes();
    ui_test_offset_pixel_grid();
    ui_test_release_velocity();
    ui_test_scroll_paths();
    ui_test_slide_paths();
    ui_test_node_alloc_balance();
    ui_test_scale_sweep();

}

