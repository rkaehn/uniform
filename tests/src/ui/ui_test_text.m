#include "ui_tests.h"
#include "ui_text_input.h"
#include "ui_core.h"
#include "ui_text.h"
#include <stdio.h>
#include <stdlib.h>


static void ui_test_check(bool ok, const char* label) {
    ui_test_group_check_("text", ok, label);
}

double2 ui_test_measure_text_(const char* raw, double fix_width) {
    struct ui_text desc = {
        .str = ui_str(raw, ui_white, 13.0),
    };

    if (fix_width > 0.0) {
        desc.size.width = ui_size_axis_fix(fix_width);
    }

    struct ui_node handle = ui_text(&desc);
    ui_layout(handle);

    double2 size = ui_get_size(handle);
    ui_release(handle);

    return size;
}

static struct ui_node_backing* ui_test_text_backing(struct ui_node handle) {
    return ui_node_pool_item(&ui_ctx()->node_pool, handle);
}

static int2 ui_test_measure_node(struct ui_node handle, int32_t width_266) {
    return ui_text_measure(ui_ctx(), ui_test_text_backing(handle), width_266 > 0 ? width_266 : int32_max);
}

static int2 ui_test_measure_str(const struct ui_str* str, int32_t width_266) {
    struct ui_node handle = ui_text(&(struct ui_text) { .str = *str });
    int2 size = ui_test_measure_node(handle, width_266);
    ui_release(handle);
    return size;
}

static int2 ui_test_measure_raw(const char* raw, int32_t width_266) {
    struct ui_str str = ui_str(raw, ui_white, 13.0);
    return ui_test_measure_str(&str, width_266);
}

double ui_test_text_advance_(const char* raw) {
    return (double)ui_test_measure_raw(raw, 0).x / 64.0;
}

static void ui_test_text_wrap(void) {
    double2 word_a = ui_test_measure_text_("go", 0.0);
    double2 word_b = ui_test_measure_text_("gopher", 0.0);
    double2 full = ui_test_measure_text_("go gopher", 0.0);

    ui_test_check(full.y > 0.0 && full.x > word_b.x && word_b.x > word_a.x, "measure sanity");

    double2 wrapped = ui_test_measure_text_("go gopher", full.x - 1.0);

    ui_test_check(fabs(wrapped.y - 2.0 * full.y) < 1.02, "wrap two lines");

    int2 word_b_266 = ui_test_measure_raw("gopher", 0);
    int2 full_266 = ui_test_measure_raw("go gopher", 0);
    int2 wrapped_266 = ui_test_measure_raw("go gopher", full_266.x - 64);

    int32_t wrap_diff = wrapped_266.x - word_b_266.x;
    ui_test_check(wrap_diff >= -2 && wrap_diff <= 2, "wrap keeps words intact");

    double2 tight = ui_test_measure_text_("go gopher", word_b.x + 1.0);
    ui_test_check(fabs(tight.y - 2.0 * full.y) < 1.02, "wrap at word width");

    double2 leading_newline = ui_test_measure_text_("\ngopher", 0.0);
    ui_test_check(fabs(leading_newline.y - 2.0 * full.y) < 1.02, "leading newline keeps line height");
}

static void ui_test_text_fill_width(void) {
    double2 full = ui_test_measure_text_("go gopher", 0.0);
    double fix_width = full.x - 1.0;

    struct ui_node root = ui_frame(&(struct ui_frame) {
        .size = ui_size_fix(fix_width, 400.0),
        .layout = ui_layout_stack_x(ui_dist_start, ui_align_start, 0, 0),
    });

    struct ui_node text = ui_text(&(struct ui_text) {
        .parent = root,
        .size.width = ui_size_axis_fill(),
        .str = ui_str("go gopher", ui_white, 13.0),
    });

    ui_layout(root);

    double2 size = ui_get_size(text);
    ui_release(root);

    ui_test_check(fabs(size.y - 2.0 * full.y) < 1.02, "fill width wrap height");
}

static void ui_test_text_measure_consistency(void) {
    struct ui_node a = ui_text(&(struct ui_text) { .str = ui_str("alpha beta gamma delta epsilon zeta", ui_white, 13.0) });
    struct ui_node b = ui_text(&(struct ui_text) { .str = ui_str("alpha beta gamma delta epsilon zeta", ui_black, 13.0) });

    int2 a_natural = ui_test_measure_node(a, 0);
    int32_t constrained = a_natural.x / 2;

    int2 a_size = ui_test_measure_node(a, constrained);
    int2 b_size = ui_test_measure_node(b, constrained);

    ui_test_check(ui_test_text_backing(a)->text.shaped == ui_test_text_backing(b)->text.shaped, "measure shares the shaped string across colors");
    ui_test_check(all_int2(a_size == b_size) && a_size.y > a_natural.y, "measure order independent");

    int2 a_repeat = ui_test_measure_node(a, constrained);

    ui_test_check(all_int2(a_repeat == a_size), "measure repeat stable");

    int2 a_wide = ui_test_measure_node(a, a_natural.x + 64);
    int2 b_wide = ui_test_measure_node(b, a_natural.x + 64);

    ui_test_check(all_int2(a_wide == b_wide) && a_wide.x == a_natural.x, "measure wide equals natural");

    int2 a_exact = ui_test_measure_node(a, a_natural.x);

    ui_test_check(all_int2(a_exact == a_natural), "measure at natural width stays single line");

    ui_release(a);
    ui_release(b);
}

static void ui_test_text_attributed(void) {
    struct ui_str str_small = ui_str("aaaa", ui_white, 13.0);
    struct ui_str str_big = ui_str("bbbb", ui_white, 26.0);

    struct ui_str str_mixed = {
        .raw = "aaaa bbbb",
        .attributes = {
            { .start = 0, .end = uint32_max, .type = ui_str_attribute_type_color, .color = ui_white },
            { .start = 0, .end = uint32_max, .type = ui_str_attribute_type_pt_size, .pt_size = 13.0 },
            { .start = 5, .end = 9, .type = ui_str_attribute_type_pt_size, .pt_size = 26.0 },
        },
    };

    int2 small = ui_test_measure_str(&str_small, 0);
    int2 big = ui_test_measure_str(&str_big, 0);
    int2 mixed = ui_test_measure_str(&str_mixed, 0);

    ui_test_check(mixed.y == big.y && mixed.x > small.x + big.x, "attributed line metrics from largest run");

    int2 wrapped = ui_test_measure_str(&str_mixed, mixed.x - 64);

    ui_test_check(abs(wrapped.y - (small.y + big.y)) <= 1, "attributed wrap per line metrics");
    ui_test_check(wrapped.x == big.x, "attributed wrap keeps runs intact");
}

static void ui_test_text_fit_chain(void) {
    double2 natural = ui_test_measure_text_("chain fit text", 0.0);

    struct ui_node root = ui_frame(&(struct ui_frame) {
        .size = ui_size_fit(),
        .layout = ui_layout_stack_y(ui_dist_start, ui_align_start, 0.0, 3.0),
    });

    struct ui_node a = ui_frame(&(struct ui_frame) {
        .parent = root,
        .size = ui_size_fit(),
        .layout = ui_layout_stack_x(ui_dist_start, ui_align_start, 0.0, 2.0),
    });

    struct ui_node b = ui_frame(&(struct ui_frame) {
        .parent = a,
        .size = ui_size_fit(),
        .layout = ui_layout_stack_y(ui_dist_start, ui_align_start, 0.0, 1.0),
    });

    ui_text(&(struct ui_text) {
        .parent = b,
        .str = ui_str("chain fit text", ui_white, 13.0),
    });

    ui_layout(root);
    double2 first = ui_get_size(root);

    ui_layout(root);
    double2 second = ui_get_size(root);

    ui_release(root);

    double expect_w = natural.x + 2.0 * (3.0 + 2.0 + 1.0);
    double expect_h = natural.y + 2.0 * (3.0 + 2.0 + 1.0);

    ui_test_check(fabs(first.x - expect_w) < 0.02 && fabs(first.y - expect_h) < 0.02, "fit chain matches standalone measure");
    ui_test_check(first.x == second.x && first.y == second.y, "layout repeat identical");
}

static void ui_test_text_fill_width_stack_y(void) {
    double2 full = ui_test_measure_text_("go gopher", 0.0);

    struct ui_node root = ui_frame(&(struct ui_frame) {
        .size.width = ui_size_axis_fix(full.x - 1.0),
        .layout = ui_layout_stack_y(ui_dist_start, ui_align_start, 0.0, 0.0),
    });

    struct ui_node text = ui_text(&(struct ui_text) {
        .parent = root,
        .size.width = ui_size_axis_fill(),
        .str = ui_str("go gopher", ui_white, 13.0),
    });

    ui_layout(root);

    double2 text_size = ui_get_size(text);
    double2 root_size = ui_get_size(root);

    ui_release(root);

    ui_test_check(fabs(text_size.y - 2.0 * full.y) < 1.02, "fill width stack_y wraps");
    ui_test_check(fabs(root_size.y - text_size.y) < 0.02, "stack_y fit height follows wrap");
}

struct ui_test_fill_row {
    double root_height;
    double2 size[3];
    double2 origin[3];
};

static struct ui_test_fill_row ui_test_text_fill_row(double width, const char* const* raws, uint32_t count) {
    struct ui_node root = ui_frame(&(struct ui_frame) {
        .size.width = ui_size_axis_fix(width),
        .layout = ui_layout_stack_x(ui_dist_start, ui_align_start, 0.0, 0.0),
    });
    struct ui_node texts[3];

    for (uint32_t i = 0; i < count; i++) {
        texts[i] = ui_text(&(struct ui_text) {
            .parent = root,
            .size.width = ui_size_axis_fill(),
            .str = ui_str(raws[i], ui_white, 13.0),
        });
    }

    ui_layout(root);

    struct ui_test_fill_row row = { .root_height = ui_get_size(root).y };

    for (uint32_t i = 0; i < count; i++) {
        row.size[i] = ui_get_size(texts[i]);
        row.origin[i] = ui_get_origin_from_root(root, texts[i]);
    }

    ui_release(root);
    return row;
}

static bool ui_test_fill_row_equal(const struct ui_test_fill_row* a, const struct ui_test_fill_row* b) {
    bool equal = a->root_height == b->root_height;

    for (uint32_t i = 0; i < 3; i++) {
        equal = equal && is_equal_double2(a->size[i], b->size[i]) && is_equal_double2(a->origin[i], b->origin[i]);
    }

    return equal;
}

static void ui_test_text_fill_width_stack_x(void) {
    const char* raws[2] = { "alpha beta gamma delta epsilon zeta", "one two three four five six seven eight" };
    double2 line_a = ui_test_measure_text_(raws[0], 0.0);
    double2 line_b = ui_test_measure_text_(raws[1], 0.0);

    struct ui_test_fill_row first = ui_test_text_fill_row(301.0, raws, 2);
    bool stable = true;

    for (uint32_t frame = 1; frame < 4; frame++) {
        struct ui_test_fill_row row = ui_test_text_fill_row(301.0, raws, 2);
        stable = stable && ui_test_fill_row_equal(&row, &first);
    }

    ui_test_check(first.size[0].y > line_a.y && first.size[1].y > line_b.y, "fill stack_x wraps both texts");
    ui_test_check(stable, "fill stack_x layout repeats identical");
    ui_test_check(fabs(first.size[0].x - first.size[1].x) <= 1.0 && first.size[0].x + first.size[1].x == 301.0 && first.origin[1].x == first.size[0].x, "fill stack_x widths differ by at most one pixel");

    int2 natural_266 = ui_test_measure_raw(raws[0], 0);
    int32_t fit_px = (natural_266.x + 63) / 64;
    const char* mixed[3] = { "a", raws[0], raws[0] };
    struct ui_test_fill_row row = ui_test_text_fill_row(3.0 * fit_px - 1.0, mixed, 3);

    ui_test_check(ui_test_measure_raw(raws[0], (fit_px - 1) * 64).y > natural_266.y, "fill stack_x share width wraps");
    ui_test_check(row.size[1].x == fit_px && row.size[2].x == fit_px && fabs(row.size[1].y - line_a.y) < 0.02, "fill stack_x remainder pixels keep one line");
    ui_test_check(fabs(row.root_height - line_a.y) < 0.02, "fill stack_x fit height follows final widths");
}

static void ui_test_text_edge_cases(void) {
    double2 empty = ui_test_measure_text_("", 0.0);
    ui_test_check(empty.x == 0.0 && empty.y == 0.0, "empty string");

    double2 invalid_a = ui_test_measure_text_("\xe2", 0.0);
    double2 invalid_b = ui_test_measure_text_("abc\xc3", 100.0);
    double2 invalid_c = ui_test_measure_text_("\xff\xfe\x80go", 0.0);
    ui_test_check(isfinite(invalid_a.x) && isfinite(invalid_b.y) && invalid_c.x > 0.0, "invalid utf8");

    double2 spaces = ui_test_measure_text_("   ", 0.0);
    double2 controls = ui_test_measure_text_("\t\r\n", 0.0);
    ui_test_check(spaces.x > 0.0 && spaces.y > 0.0 && isfinite(controls.x) && controls.y > spaces.y, "whitespace only");

    struct ui_str zero = ui_str("zero size", ui_white, 0.0);
    int2 zero_size = ui_test_measure_str(&zero, 0);
    ui_test_check(zero_size.x == 0 && zero_size.y == 0, "zero pt size measures empty");
}

static int2 ui_test_measure_attributed(const char* raw, const struct ui_str_attribute* attributes, uint32_t count) {
    struct ui_str str = { .raw = raw, .extra_attributes = attributes, .extra_attribute_count = count };
    return ui_test_measure_str(&str, 0);
}

static void ui_test_text_attribute_ranges(void) {
    struct ui_str_attribute reversed = { .start = 3, .end = 1, .type = ui_str_attribute_type_pt_size, .pt_size = 40.0 };
    struct ui_str_attribute beyond = { .start = 100, .end = 200, .type = ui_str_attribute_type_pt_size, .pt_size = 40.0 };
    struct ui_str_attribute empty = { .start = 2, .end = 2, .type = ui_str_attribute_type_pt_size, .pt_size = 40.0 };
    int2 plain = ui_test_measure_attributed("abcd", NULL, 0);

    ui_test_check(all_int2(ui_test_measure_attributed("abcd", &reversed, 1) == plain), "reversed attribute range is ignored");
    ui_test_check(all_int2(ui_test_measure_attributed("abcd", &beyond, 1) == plain), "attribute range beyond the string is ignored");
    ui_test_check(all_int2(ui_test_measure_attributed("abcd", &empty, 1) == plain), "empty attribute range is ignored");

    struct ui_str_attribute same_start[2] = {
        { .start = 0, .end = 4, .type = ui_str_attribute_type_pt_size, .pt_size = 10.0 },
        { .start = 0, .end = 4, .type = ui_str_attribute_type_pt_size, .pt_size = 30.0 },
    };
    struct ui_str_attribute big = { .start = 0, .end = uint32_max, .type = ui_str_attribute_type_pt_size, .pt_size = 30.0 };
    ui_test_check(all_int2(ui_test_measure_attributed("abcd", same_start, 2) == ui_test_measure_attributed("abcd", &big, 1)), "later attribute wins at the same start");

    struct ui_str_attribute nested[2] = {
        { .start = 0, .end = 4, .type = ui_str_attribute_type_pt_size, .pt_size = 30.0 },
        { .start = 2, .end = 4, .type = ui_str_attribute_type_pt_size, .pt_size = 10.0 },
    };
    struct ui_str_attribute small = { .start = 0, .end = uint32_max, .type = ui_str_attribute_type_pt_size, .pt_size = 10.0 };
    int2 nested_size = ui_test_measure_attributed("abcd", nested, 2);
    int2 inner = ui_test_measure_attributed("ab", &big, 1);
    int2 outer = ui_test_measure_attributed("cd", &small, 1);
    ui_test_check(abs(nested_size.x - inner.x - outer.x) <= 2 && nested_size.y == inner.y, "inner attribute wins inside an outer range");

    static char raw[3001];
    static struct ui_str_attribute many[3000];
    for (uint32_t i = 0; i < 3000; i++) {
        raw[i] = (char)('a' + i % 26);
        many[i] = (struct ui_str_attribute) { .start = i, .end = i + 1, .type = ui_str_attribute_type_pt_size, .pt_size = 13.0 };
    }
    raw[3000] = 0;
    ui_test_check(all_int2(ui_test_measure_attributed(raw, many, 3000) == ui_test_measure_attributed(raw, NULL, 0)), "3000 attributes resolve to one run");
}

static struct ui_node ui_test_text_node_pt(const char* raw, double fix_width, double pt_size) {
    struct ui_text desc = {
        .str = ui_str(raw, ui_white, pt_size),
    };

    if (fix_width > 0.0) {
        desc.size.width = ui_size_axis_fix(fix_width);
    }

    struct ui_node handle = ui_text(&desc);
    ui_layout(handle);
    return handle;
}

static struct ui_node ui_test_text_node(const char* raw, double fix_width) {
    return ui_test_text_node_pt(raw, fix_width, 13.0);
}

static uint32_t ui_test_text_line_index(struct ui_node text, uint32_t offset) {
    for (uint32_t i = ui_text_get_line_count(text); i-- > 1;) {
        if (offset >= ui_text_get_line(text, i).start) {
            return i;
        }
    }
    return 0;
}

static bool ui_test_text_ends_soft(struct ui_node text, uint32_t line_index, uint32_t offset) {
    struct ui_text_line line = ui_text_get_line(text, line_index);
    return line_index + 1 < ui_text_get_line_count(text) && line.end == offset && line.visual_end == offset;
}

static bool ui_test_text_is_soft(struct ui_node text, uint32_t offset) {
    uint32_t index = ui_test_text_line_index(text, offset);
    return index > 0 && ui_test_text_ends_soft(text, index - 1, offset);
}

static struct ui_text_edit_rect ui_test_text_caret(struct ui_node text, uint32_t offset, bool upstream) {
    uint32_t index = ui_test_text_line_index(text, offset);
    if (upstream && index > 0 && ui_test_text_ends_soft(text, index - 1, offset)) {
        index--;
    }
    struct ui_text_line line = ui_text_get_line(text, index);

    return (struct ui_text_edit_rect) {
        .x = ui_text_get_x(text, index, offset),
        .y = line.y,
        .height = line.height,
    };
}

static uint32_t ui_test_text_offset_at(struct ui_node text, double2 pos, bool* out_upstream) {
    uint32_t line_count = ui_text_get_line_count(text);

    uint32_t index = 0;
    while (index + 1 < line_count) {
        struct ui_text_line line = ui_text_get_line(text, index);
        if (pos.y < line.y + line.height) {
            break;
        }
        index++;
    }

    uint32_t offset = ui_text_get_offset(text, index, pos.x);
    if (out_upstream) {
        *out_upstream = ui_test_text_ends_soft(text, index, offset);
    }
    return offset;
}

static double ui_test_text_height(struct ui_node text) {
    struct ui_text_line last = ui_text_get_line(text, ui_text_get_line_count(text) - 1);
    return last.y + last.height;
}

static double ui_test_text_width(struct ui_node text) {
    double width = 0.0;
    for (uint32_t i = 0; i < ui_text_get_line_count(text); i++) {
        width = max_double(width, ui_text_get_line(text, i).width);
    }
    return width;
}

static void ui_test_text_edit_affinity(void) {
    double2 full = ui_test_measure_text_("go gopher", 0.0);
    double2 word = ui_test_measure_text_("gopher", 0.0);

    struct ui_node text = ui_test_text_node("go gopher", full.x - 1.0);

    ui_test_check(ui_text_get_line_count(text) == 2, "edit wrap two lines");
    ui_test_check(ui_test_text_is_soft(text, 3), "edit soft boundary at wrap");
    ui_test_check(ui_test_text_is_soft(text, 0) == false, "edit doc start not soft");

    struct ui_text_edit_rect downstream = ui_test_text_caret(text, 3, false);
    struct ui_text_edit_rect upstream = ui_test_text_caret(text, 3, true);
    ui_test_check(downstream.y > upstream.y && downstream.x == 0.0 && upstream.x > 0.0, "edit caret affinity position");

    bool hit_upstream = false;
    uint32_t offset = ui_test_text_offset_at(text, make_double2(1000.0, upstream.y + upstream.height * 0.5), &hit_upstream);
    ui_test_check(offset == 3 && hit_upstream, "edit click past soft line end");

    offset = ui_test_text_offset_at(text, make_double2(0.0, downstream.y + downstream.height * 0.5), &hit_upstream);
    ui_test_check(offset == 3 && hit_upstream == false, "edit click at next line start");

    struct ui_text_line line0 = ui_text_get_line(text, 0);
    struct ui_text_line line1 = ui_text_get_line(text, 1);
    ui_test_check(line0.start == 0 && line0.visual_end == 3, "edit upstream line range");
    ui_test_check(line1.start == 3, "edit downstream line range");

    uint32_t moved = ui_text_get_offset(text, 1, 500.0);
    ui_test_check(moved == 9 && ui_test_text_ends_soft(text, 1, moved) == false, "edit line move down from upstream");

    moved = ui_text_get_offset(text, 0, 500.0);
    ui_test_check(moved == 3 && ui_test_text_ends_soft(text, 0, moved), "edit line move up lands upstream");

    ui_release(text);

    struct ui_node hard = ui_test_text_node("ab\ncd", 10000.0);
    ui_test_check(ui_text_get_line_count(hard) == 2, "edit hard newline two lines");
    ui_test_check(ui_test_text_is_soft(hard, 3) == false, "edit hard boundary not soft");
    struct ui_text_edit_rect hard_upstream = ui_test_text_caret(hard, 3, true);
    struct ui_text_edit_rect hard_downstream = ui_test_text_caret(hard, 3, false);
    ui_test_check(hard_upstream.y == hard_downstream.y && hard_upstream.y > 0.0, "edit hard boundary ignores affinity");
    ui_release(hard);

    struct ui_node single = ui_test_text_node("go gopher", 0.0);
    ui_test_check(ui_text_get_line_count(single) == 1, "edit unbounded single line");
    double natural = ui_test_text_width(single);
    ui_test_check(natural > word.x && natural <= full.x + 1.0, "edit layout width natural");
    ui_release(single);
}

static void ui_test_text_edit_trailing_newline(void) {
    struct ui_node ab = ui_test_text_node("ab", 10000.0);
    double line_height = ui_test_text_height(ab);
    ui_release(ab);

    struct ui_node text = ui_test_text_node("ab\n", 10000.0);
    ui_test_check(ui_text_get_line_count(text) == 2, "trailing newline adds line");
    ui_test_check(fabs(ui_test_text_height(text) - 2.0 * line_height) < 0.5, "trailing newline layout height");

    struct ui_text_edit_rect caret = ui_test_text_caret(text, 3, false);
    ui_test_check(caret.height > 1.0, "trailing newline caret has height");
    ui_test_check(caret.x == 0.0 && fabs(caret.y - line_height) < 0.5, "trailing newline caret position");

    bool upstream = true;
    uint32_t offset = ui_test_text_offset_at(text, make_double2(50.0, caret.y + caret.height * 0.5), &upstream);
    ui_test_check(offset == 3 && upstream == false, "trailing newline line is clickable");
    ui_release(text);

    struct ui_node multi = ui_test_text_node("ab\n\n", 10000.0);
    ui_test_check(ui_text_get_line_count(multi) == 3, "double trailing newline adds lines");
    ui_test_check(fabs(ui_test_text_height(multi) - 3.0 * line_height) < 0.5, "double trailing newline layout height");
    struct ui_text_edit_rect multi_caret = ui_test_text_caret(multi, 4, false);
    ui_test_check(multi_caret.height > 1.0 && fabs(multi_caret.y - 2.0 * line_height) < 0.5, "double trailing newline caret");
    ui_release(multi);

    struct ui_node only = ui_test_text_node("\n", 10000.0);
    ui_test_check(ui_text_get_line_count(only) == 2, "newline only two lines");
    struct ui_text_edit_rect only_start = ui_test_text_caret(only, 0, false);
    struct ui_text_edit_rect only_end = ui_test_text_caret(only, 1, false);
    ui_test_check(only_start.height > 1.0 && only_end.height > 1.0 && only_end.y > only_start.y, "newline only caret rects");
    ui_release(only);

    struct ui_node empty = ui_test_text_node("", 10000.0);
    struct ui_text_edit_rect empty_caret = ui_test_text_caret(empty, 0, false);
    ui_test_check(empty_caret.height > 1.0 && empty_caret.x == 0.0 && empty_caret.y == 0.0, "empty text caret rect");
    ui_release(empty);
}

static void ui_test_empty_text_metrics(void) {
    struct ui_node empty = ui_test_text_node_pt("", 10000.0, 20.0);
    struct ui_node x = ui_test_text_node_pt("x", 10000.0, 20.0);

    ui_test_check(ui_text_get_line_count(empty) == 1, "empty text has one line");
    ui_test_check(fabs(ui_test_text_height(empty) - ui_test_text_height(x)) < 0.01, "empty text uses configured pt size");

    struct ui_text_edit_rect empty_caret = ui_test_text_caret(empty, 0, false);
    struct ui_text_edit_rect x_caret = ui_test_text_caret(x, 0, false);
    ui_test_check(fabs(empty_caret.height - x_caret.height) < 0.01, "empty text caret height matches");

    ui_release(empty);
    ui_release(x);
}

static struct ui_str ui_test_str_font(const char* raw, struct ui_font* font, double weight, struct ui_color color) {
    struct ui_str str = ui_str_weight(raw, color, 13.0, weight);
    str.attributes[3] = (struct ui_str_attribute) { .start = 0, .end = uint32_max, .type = ui_str_attribute_type_font, .font = font };
    return str;
}

static struct ui_font* ui_test_font(bool ui_font) {
    uint64_t size;
    const uint8_t* data = ui_test_font_data_(ui_font, &size);
    return ui_font_create(data, size);
}

static void ui_test_text_variations(void) {
    struct ui_font* inter = ui_test_font(true);
    struct ui_str regular = ui_test_str_font("variations", inter, 400.0, ui_white);
    struct ui_str bold = ui_test_str_font("variations", inter, 700.0, ui_white);
    struct ui_str regular_black = ui_test_str_font("variations", inter, 400.0, ui_black);
    int2 regular_size = ui_test_measure_str(&regular, 0);

    ui_test_check(ui_test_measure_str(&bold, 0).x > regular_size.x, "variations shape with their weight");
    ui_test_check(all_int2(ui_test_measure_str(&regular_black, 0) == regular_size), "variations shape independently of the previous weight");

    static char raw[41];
    static struct ui_str_attribute weights[20];
    for (uint32_t i = 0; i < 20; i++) {
        raw[i * 2] = 'w';
        raw[i * 2 + 1] = 'x';
        weights[i] = (struct ui_str_attribute) { .start = i * 2, .end = i * 2 + 2, .type = ui_str_attribute_type_weight, .weight = 100.0 + 40.0 * (double)i };
    }
    raw[40] = 0;

    struct ui_str many_white = { .raw = raw, .attributes[0] = { .start = 0, .end = uint32_max, .type = ui_str_attribute_type_font, .font = inter }, .extra_attributes = weights, .extra_attribute_count = 20 };
    struct ui_str many_black = many_white;
    many_black.attributes[1] = (struct ui_str_attribute) { .start = 0, .end = uint32_max, .type = ui_str_attribute_type_color, .color = ui_black };

    int2 many_size = ui_test_measure_str(&many_white, 0);
    for (uint32_t i = 0; i < 20; i++) {
        struct ui_str other = ui_test_str_font("other weight", inter, 110.0 + 40.0 * (double)i, ui_white);
        ui_test_measure_str(&other, 0);
    }

    ui_test_check(many_size.x > 0 && all_int2(ui_test_measure_str(&many_black, 0) == many_size), "weights beyond the variation slots shape identically");

    ui_font_destroy(inter);
}

static void ui_test_text_line_positions(void) {
    static char raw[200];

    for (uint32_t i = 0; i < 100; i++) {
        raw[i * 2] = 'x';
        raw[i * 2 + 1] = i + 1 < 100 ? '\n' : 0;
    }

    struct ui_node text = ui_test_text_node_pt(raw, 10000.0, 11.0);
    struct ui_text_line first = ui_text_get_line(text, 0);
    struct ui_text_line last = ui_text_get_line(text, 99);

    ui_test_check(ui_text_get_line_count(text) == 100 && fabs(last.y - 99.0 * first.height) < 1e-6, "line positions do not accumulate rounding");
    ui_release(text);
}

static void ui_test_text_shape_cache(void) {
    struct ui_node first = ui_text(&(struct ui_text) { .str = ui_str("shape cache", ui_white, 13.0) });

    ui_test_measure_node(first, 0);
    const struct ui_shaped* shaped = ui_test_text_backing(first)->text.shaped;
    ui_release(first);

    struct ui_node again = ui_text(&(struct ui_text) { .str = ui_str("shape cache", ui_white, 13.0) });
    struct ui_node bigger = ui_text(&(struct ui_text) { .str = ui_str("shape cache", ui_white, 14.0) });

    ui_test_measure_node(again, 0);
    ui_test_measure_node(bigger, 0);
    ui_test_check(ui_test_text_backing(again)->text.shaped == shaped, "shape cache reuses a released string");
    ui_test_check(ui_test_text_backing(bigger)->text.shaped != shaped, "shape cache keys on the resolved runs");
    ui_release(again);
    ui_release(bigger);

    struct ui_font* inter = ui_test_font(true);
    struct ui_str with_inter = ui_test_str_font("shape cache", inter, 400.0, ui_white);
    int2 inter_size = ui_test_measure_str(&with_inter, 0);
    ui_font_destroy(inter);

    struct ui_font* mono = ui_test_font(false);
    struct ui_str with_mono = ui_test_str_font("shape cache", mono, 400.0, ui_white);
    int2 mono_size = ui_test_measure_str(&with_mono, 0);
    ui_font_destroy(mono);

    ui_test_check(mono_size.x != inter_size.x && mono_size.x == ui_test_measure_raw("shape cache", 0).x, "shape cache forgets a destroyed font");

    for (uint32_t i = 0; i < 24576; i++) {
        char raw[64];
        snprintf(raw, sizeof(raw), "released string %u, long enough to take some room", i);
        ui_release(ui_test_text_node(raw, 0.0));
    }

    uint64_t unused = ui_text_unused_bytes_();
    ui_test_check(unused > (4u << 20) && unused <= (8u << 20), "shape cache bounds released strings");
}

#if cr_macos || cr_ios

#define ui_test_text_perf_renders 30u
#define ui_test_text_perf_px 2048u
#define ui_test_text_perf_scale 2.0

static struct ui_node ui_test_text_perf_root(void) {
    return ui_frame(&(struct ui_frame) {
        .size = ui_size_fix(ui_test_text_perf_px / ui_test_text_perf_scale, ui_test_text_perf_px / ui_test_text_perf_scale),
        .layout = ui_layout_stack_y(ui_dist_start, ui_align_start, 4.0, make_double4(12.0, 12.0, 12.0, 12.0)),
        .fill = ui_fill_solid(ui_black),
    });
}

static struct ui_node ui_test_text_perf_row(struct ui_node root) {
    return ui_frame(&(struct ui_frame) {
        .parent = root,
        .size.width = ui_size_axis_fill(),
        .layout = ui_layout_stack_x(ui_dist_start, ui_align_center, 8.0, make_double4(0.0, 0.0, 0.0, 0.0)),
    });
}

static void ui_test_text_perf_fill(char* text, uint32_t size) {
    uint32_t offset = 0;

    while (offset + 128 < size) {
        offset += (uint32_t)snprintf(text + offset, size - offset, "Sphinx of black quartz judge my vow %u. The five boxing wizards jump quickly %u. ", offset, offset * 7);
    }
}

static struct ui_node ui_test_text_perf_paragraph(uint32_t render) {
    static char text[16384];

    if (text[0] == 0) {
        ui_test_text_perf_fill(text, sizeof(text));
    }

    struct ui_node root = ui_test_text_perf_root();
    ui_text(&(struct ui_text) {
        .parent = root,
        .size.width = ui_size_axis_fill(),
        .str = ui_str(text, ui_white, 11.0),
    });
    return root;
}

static struct ui_node ui_test_text_perf_document(uint32_t render) {
    static char text[65536];

    if (text[0] == 0) {
        ui_test_text_perf_fill(text, sizeof(text));
    }

    struct ui_node root = ui_test_text_perf_root();
    struct ui_node viewport = ui_frame(&(struct ui_frame) {
        .parent = root,
        .size = ui_size_fix(1000.0, 240.0),
        .clip = true,
    });
    ui_text(&(struct ui_text) {
        .parent = viewport,
        .pos = ui_pos_xy(0.0, -2400.0),
        .size.width = ui_size_axis_fix(1000.0),
        .str = ui_str(text, ui_white, 11.0),
    });
    return root;
}

static struct ui_node ui_test_text_perf_labels(uint32_t render) {
    struct ui_node root = ui_test_text_perf_root();

    for (uint32_t row = 0; row < 30; row++) {
        struct ui_node line = ui_test_text_perf_row(root);

        for (uint32_t col = 0; col < 30; col++) {
            char label[32];
            snprintf(label, sizeof(label), "Label %u", row * 30 + col);
            ui_text(&(struct ui_text) {
                .parent = line,
                .str = ui_str(label, ui_white, 9.0 + (double)((row * 30 + col) % 7)),
            });
        }
    }

    return root;
}

static struct ui_node ui_test_text_perf_rows_ex(uint32_t change) {
    struct ui_node root = ui_test_text_perf_root();

    for (uint32_t i = 0; i < 160; i++) {
        struct ui_node row = ui_test_text_perf_row(root);
        char label[64];

        snprintf(label, sizeof(label), "Row %u", i + change * 1000);
        ui_text(&(struct ui_text) {
            .parent = row,
            .str = ui_str_weight(label, ui_white, 13.0, 600.0),
        });

        snprintf(label, sizeof(label), "value %u units and change", i * 37 + change);
        ui_text(&(struct ui_text) {
            .parent = row,
            .str = ui_str(label, ui_white, 13.0),
        });
    }

    return root;
}

static struct ui_node ui_test_text_perf_rows(uint32_t render) {
    return ui_test_text_perf_rows_ex(0);
}

static struct ui_font* ui_test_text_perf_inter_;

static struct ui_str ui_test_text_perf_inter_str(const char* raw, double pt_size) {
    struct ui_str str = ui_test_str_font(raw, ui_test_text_perf_inter_, 400.0, ui_white);
    str.attributes[1].pt_size = pt_size;
    return str;
}

static struct ui_node ui_test_text_perf_inter_labels(uint32_t render) {
    struct ui_node root = ui_test_text_perf_root();

    for (uint32_t row = 0; row < 30; row++) {
        struct ui_node line = ui_test_text_perf_row(root);

        for (uint32_t col = 0; col < 30; col++) {
            char label[32];
            snprintf(label, sizeof(label), "Label %u", row * 30 + col);
            ui_text(&(struct ui_text) {
                .parent = line,
                .str = ui_test_text_perf_inter_str(label, 15.0 + (double)((row * 30 + col) % 7)),
            });
        }
    }

    return root;
}

static struct ui_node ui_test_text_perf_inter_changing_rows(uint32_t render) {
    struct ui_node root = ui_test_text_perf_root();

    for (uint32_t i = 0; i < 160; i++) {
        struct ui_node row = ui_test_text_perf_row(root);
        char label[64];

        snprintf(label, sizeof(label), "Row %u", i + render * 1000);
        ui_text(&(struct ui_text) {
            .parent = row,
            .str = ui_test_text_perf_inter_str(label, 17.0),
        });

        snprintf(label, sizeof(label), "value %u units and change", i * 37 + render);
        ui_text(&(struct ui_text) {
            .parent = row,
            .str = ui_test_text_perf_inter_str(label, 17.0),
        });
    }

    return root;
}

static struct ui_node ui_test_text_perf_changing_rows(uint32_t render) {
    return ui_test_text_perf_rows_ex(render);
}

static int ui_test_text_perf_compare(const void* a, const void* b) {
    double x = *(const double*)a;
    double y = *(const double*)b;
    return x < y ? -1 : x > y;
}

static double ui_test_text_perf_median(double* values, uint32_t count) {
    qsort(values, count, sizeof(values[0]), ui_test_text_perf_compare);
    return values[count / 2];
}

static void ui_test_text_perf_scene(const char* label, struct ui_node (*build)(uint32_t render), struct gpu_texture* target) {
    struct ui* ui = ui_ctx();
    double build_ms[ui_test_text_perf_renders];
    double layout_ms[ui_test_text_perf_renders];
    double render_ms[ui_test_text_perf_renders];

    for (uint32_t render = 0; render < ui_test_text_perf_renders; render++) {
        double t0 = cr_get_current_time();
        struct ui_node root = build(render);
        double t1 = cr_get_current_time();
        ui_layout(root);
        double t2 = cr_get_current_time();

        uint64_t serial;
        cr_autoreleasepool {
            struct gpu_command_buffer* command_buffer = gpu_command_buffer_create(ui->device, "text perf");
            ui_render(root, command_buffer, target, (struct ui_render_desc) { .premultiplied = true });
            serial = gpu_command_buffer_commit(command_buffer);
        }
        double t3 = cr_get_current_time();

        ui_release(root);
        gpu_device_wait(ui->device, serial);

        build_ms[render] = (t1 - t0) * 1000.0;
        layout_ms[render] = (t2 - t1) * 1000.0;
        render_ms[render] = (t3 - t2) * 1000.0;
    }

    printf("text perf %s: build %.3f, layout %.3f, render %.3f ms (median of %u renders)\n", label,
           ui_test_text_perf_median(build_ms, ui_test_text_perf_renders), ui_test_text_perf_median(layout_ms, ui_test_text_perf_renders),
           ui_test_text_perf_median(render_ms, ui_test_text_perf_renders), ui_test_text_perf_renders);
}

static void ui_test_text_perf(void) {
    struct ui* ui = ui_ctx();
    double saved_to_px_from_pt = ui_get_to_px_from_pt();
    ui_set_to_px_from_pt(ui_test_text_perf_scale);

    struct gpu_texture* target = gpu_texture_create(ui->device, &(struct gpu_texture_desc) {
        .label = "text perf target",
        .format = gpu_format_bgra8,
        .size = { ui_test_text_perf_px, ui_test_text_perf_px },
        .usage = gpu_texture_usage_render_target | gpu_texture_usage_sample,
    });

    ui_test_text_perf_scene("paragraph", ui_test_text_perf_paragraph, target);
    ui_test_text_perf_scene("scrolled document", ui_test_text_perf_document, target);
    ui_test_text_perf_scene("labels", ui_test_text_perf_labels, target);
    ui_test_text_perf_scene("rows", ui_test_text_perf_rows, target);
    ui_test_text_perf_scene("changing rows", ui_test_text_perf_changing_rows, target);

    ui_test_text_perf_inter_ = ui_test_font(true);
    ui_test_text_perf_scene("inter labels", ui_test_text_perf_inter_labels, target);
    ui_test_text_perf_scene("inter changing rows", ui_test_text_perf_inter_changing_rows, target);
    ui_font_destroy(ui_test_text_perf_inter_);

    gpu_texture_destroy(ui->device, target);
    ui_set_to_px_from_pt(saved_to_px_from_pt);
}

#else

static void ui_test_text_perf(void) {
}

#endif

void ui_test_text_(void) {

    ui_test_text_wrap();
    ui_test_text_fill_width();
    ui_test_text_measure_consistency();
    ui_test_text_attributed();
    ui_test_text_fit_chain();
    ui_test_text_fill_width_stack_y();
    ui_test_text_fill_width_stack_x();
    ui_test_text_edge_cases();
    ui_test_text_attribute_ranges();
    ui_test_empty_text_metrics();
    ui_test_text_edit_affinity();
    ui_test_text_edit_trailing_newline();
    ui_test_text_variations();
    ui_test_text_line_positions();
    ui_test_text_shape_cache();
    ui_test_text_perf();

}

