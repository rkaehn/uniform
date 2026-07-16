#include "ui_tests.h"

static struct ui_test_sui_item ui_test_rect_item(double x, double y, double w, double h, double radius, bool circular, float4 fill) {
    return (struct ui_test_sui_item) {
        .kind = ui_test_sui_kind_rect,
        .origin = make_double2(x, y),
        .size = make_double2(w, h),
        .radius = make_double4(radius, radius, radius, radius),
        .circular = circular,
        .fill = fill,
    };
}

static struct ui_test_sui_item ui_test_rect_stroke_item(double x, double y, double w, double h, double radius, bool circular,
                                                        enum ui_outline_align align, float thickness, float4 stroke) {
    struct ui_test_sui_item item = ui_test_rect_item(x, y, w, h, radius, circular, make_float4(0.0f, 0.0f, 0.0f, 0.0f));
    item.stroke = stroke;
    item.stroke_width = thickness;
    item.stroke_align = align;
    return item;
}

static struct ui_test_sui_item ui_test_ellipse_item(double x, double y, double w, double h, float4 fill) {
    return (struct ui_test_sui_item) {
        .kind = ui_test_sui_kind_ellipse,
        .origin = make_double2(x, y),
        .size = make_double2(w, h),
        .fill = fill,
    };
}

#define ui_test_rect_grid_item_max 152

static void ui_test_rect_grid_items(struct ui_test_sui_item* items, uint32_t* out_count, bool circular) {
    uint32_t count = 0;
    for (uint32_t p = 0; p < 4; p++) {
        double px = 16.0 + (double)(p % 2) * 504.0;
        double py = 16.0 + (double)(p / 2) * 504.0;
        items[count++] = ui_test_rect_item(px, py, 488.0, 488.0, 32.0, circular, ui_system_background.value);
        for (uint32_t t = 0; t < 36; t++) {
            double tx = px + 32.0 + (double)(t % 6) * 72.0;
            double ty = py + 32.0 + (double)(t / 6) * 72.0;
            items[count++] = ui_test_rect_item(tx, ty, 64.0, 64.0, 12.0, circular, ui_test_palette_(p * 5 + t));
        }
    }
    *out_count = count;
}

static void ui_test_scene_rect_grid_fill(struct ui_test_sui_spec* spec, struct ui_test_sui_item* items) {
    uint32_t count;
    ui_test_rect_grid_items(items, &count, false);
    *spec = (struct ui_test_sui_spec) { .background = ui_system_gray6.value, .items = items, .item_count = count };
}

static void ui_test_scene_rect_grid_circular_fill(struct ui_test_sui_spec* spec, struct ui_test_sui_item* items) {
    uint32_t count;
    ui_test_rect_grid_items(items, &count, true);
    *spec = (struct ui_test_sui_spec) { .background = ui_system_gray6.value, .items = items, .item_count = count };
}

static void ui_test_scene_rect_radii_fill(struct ui_test_sui_spec* spec, struct ui_test_sui_item* items) {
    uint32_t count = 0;

    static const double square_radii_continuous[] = { 0.0, 2.0, 4.0, 8.0, 12.0, 16.0, 24.0, 28.0 };
    static const double square_radii_circular[] = { 0.0, 2.0, 4.0, 8.0, 16.0, 24.0, 32.0, 48.0 };
    for (uint32_t i = 0; i < cr_count(square_radii_continuous); i++) {
        items[count++] = ui_test_rect_item(32.0 + (double)i * 120.0, 32.0, 96.0, 96.0, square_radii_continuous[i], false, ui_system_blue.value);
        items[count++] = ui_test_rect_item(32.0 + (double)i * 120.0, 160.0, 96.0, 96.0, square_radii_circular[i], true, ui_system_teal.value);
    }

    static const double wide_radii[] = { 2.0, 4.0, 6.0, 8.0, 12.0, 16.0, 20.0 };
    static const double tall_radii[] = { 2.0, 4.0, 6.0, 8.0, 10.0, 12.0, 14.0 };
    for (uint32_t i = 0; i < cr_count(wide_radii); i++) {
        items[count++] = ui_test_rect_item(32.0 + (double)i * 136.0, 288.0, 128.0, 64.0, wide_radii[i], false, ui_system_indigo.value);
        items[count++] = ui_test_rect_item(32.0 + (double)i * 136.0, 384.0, 48.0, 128.0, tall_radii[i], false, ui_system_purple.value);
    }

    items[count++] = ui_test_rect_item(32.0, 760.0, 160.0, 56.0, 28.0, true, ui_system_green.value);
    items[count++] = ui_test_rect_item(224.0, 760.0, 240.0, 72.0, 36.0, true, ui_system_green.value);
    items[count++] = ui_test_rect_item(496.0, 760.0, 96.0, 96.0, 48.0, true, ui_system_green.value);
    items[count++] = ui_test_rect_item(624.0, 760.0, 320.0, 80.0, 40.0, true, ui_system_mint.value);

    struct { double w, h, r; } tiny[] = {
        { 12.0, 12.0, 3.0 }, { 16.0, 16.0, 5.0 }, { 24.0, 24.0, 7.0 }, { 8.0, 8.0, 2.0 }, { 6.0, 6.0, 1.0 }, { 20.0, 10.0, 3.0 },
    };
    for (uint32_t rep = 0; rep < 3; rep++) {
        double tiny_x = 32.0 + (double)rep * 320.0;
        for (uint32_t i = 0; i < cr_count(tiny); i++) {
            items[count++] = ui_test_rect_item(tiny_x, 908.0, tiny[i].w, tiny[i].h, tiny[i].r, rep == 1, ui_test_palette_(rep * 6 + i));
            tiny_x += tiny[i].w + 24.0;
        }
    }

    assert(count <= 80);
    *spec = (struct ui_test_sui_spec) { .background = ui_system_background.value, .items = items, .item_count = count };
}

static const double4 ui_test_rect_uneven_combos_[6] = {
    { 48.0, 0.0, 0.0, 0.0 },
    { 48.0, 0.0, 0.0, 48.0 },
    { 48.0, 48.0, 0.0, 0.0 },
    { 8.0, 24.0, 48.0, 56.0 },
    { 48.0, 8.0, 40.0, 16.0 },
    { 0.0, 64.0, 64.0, 0.0 },
};

static const double4 ui_test_rect_uneven_tight_[3] = {
    { 45.0, 8.0, 8.0, 45.0 },
    { 60.0, 60.0, 0.0, 0.0 },
    { 32.0, 32.0, 32.0, 32.0 },
};

static void ui_test_scene_rect_uneven_fill(struct ui_test_sui_spec* spec, struct ui_test_sui_item* items) {
    uint32_t count = 0;

    for (uint32_t i = 0; i < 6; i++) {
        struct ui_test_sui_item item = ui_test_rect_item(32.0 + (double)(i % 3) * 330.0,
                                                         32.0 + (double)(i / 3) * 172.0,
                                                         200.0, 140.0, 0.0, true,
                                                         ui_test_palette_(i));
        item.radius = ui_test_rect_uneven_combos_[i];
        items[count++] = item;
    }

    for (uint32_t i = 0; i < 3; i++) {
        struct ui_test_sui_item item = ui_test_rect_item(32.0 + (double)i * 330.0, 376.0,
                                                         200.0, 100.0, 0.0, true,
                                                         ui_test_palette_(6 + i));
        item.radius = ui_test_rect_uneven_tight_[i];
        items[count++] = item;
    }

    for (uint32_t i = 0; i < 3; i++) {
        struct ui_test_sui_item item = ui_test_rect_item(32.0 + (double)i * 330.0, 528.0,
                                                         200.0, 320.0, 0.0, true,
                                                         ui_test_palette_(9 + i));
        item.radius = ui_test_rect_uneven_combos_[i + 3];
        items[count++] = item;
    }

    assert(count <= 16);
    *spec = (struct ui_test_sui_spec) { .background = ui_system_gray6.value, .items = items, .item_count = count };
}

static void ui_test_scene_rect_continuous_fill(struct ui_test_sui_spec* spec, struct ui_test_sui_item* items) {
    uint32_t count = 0;

    for (uint32_t i = 0; i < 6; i++) {
        struct ui_test_sui_item item = ui_test_rect_item(32.0 + (double)(i % 3) * 330.0,
                                                         32.0 + (double)(i / 3) * 172.0,
                                                         200.0, 140.0, 0.0, false,
                                                         ui_test_palette_(i));
        item.radius = ui_test_rect_uneven_combos_[i];
        items[count++] = item;
    }

    for (uint32_t i = 0; i < 3; i++) {
        struct ui_test_sui_item item = ui_test_rect_item(32.0 + (double)i * 330.0, 376.0,
                                                         200.0, 100.0, 0.0, false,
                                                         ui_test_palette_(6 + i));
        item.radius = ui_test_rect_uneven_tight_[i];
        items[count++] = item;
    }

    items[count++] = ui_test_rect_item(32.0, 540.0, 160.0, 56.0, 16.0, false, ui_system_mint.value);
    items[count++] = ui_test_rect_item(224.0, 540.0, 176.0, 88.0, 28.0, false, ui_system_mint.value);
    items[count++] = ui_test_rect_item(432.0, 540.0, 240.0, 72.0, 20.0, false, ui_system_mint.value);

    static const float thick[] = { 8.0f, 16.0f, 32.0f };
    for (uint32_t i = 0; i < cr_count(thick); i++) {
        items[count++] = ui_test_rect_stroke_item(64.0 + (double)i * 200.0, 832.0, 104.0, 72.0, 20.0, false,
                                                  ui_outline_align_center, thick[i], ui_system_blue.value);
    }

    assert(count <= 32);
    *spec = (struct ui_test_sui_spec) { .background = ui_system_gray6.value, .items = items, .item_count = count };
}

static void ui_test_scene_rect_fractional_fill(struct ui_test_sui_spec* spec, struct ui_test_sui_item* items) {
    uint32_t count = 0;

    static const double fractions[4] = { 0.0, 0.5, 0.0, 0.5 };
    for (uint32_t i = 0; i < 4; i++) {
        items[count++] = ui_test_rect_item(32.0 + (double)i * 112.0 + fractions[i], 32.0, 80.0, 80.0, 16.0, false, ui_system_blue.value);
        items[count++] = ui_test_rect_item(32.0 + (double)i * 112.0, 144.0 + fractions[i], 80.0 + fractions[3 - i], 80.0 + fractions[i], 16.0, false, ui_system_teal.value);
    }

    static const struct { double y, h; } strips[] = {
        { 280.0, 0.5 }, { 290.5, 0.5 }, { 301.0, 0.5 }, { 311.5, 0.5 },
        { 324.0, 1.0 }, { 334.5, 1.0 },
        { 348.0, 1.5 }, { 358.5, 1.5 },
        { 372.0, 2.5 }, { 382.5, 2.5 },
    };
    for (uint32_t i = 0; i < cr_count(strips); i++) {
        items[count++] = ui_test_rect_item(32.0, strips[i].y, 448.0, strips[i].h, 0.0, false, ui_label.value);
    }

    static const struct { double x, w; } columns[] = {
        { 32.0, 0.5 }, { 42.5, 0.5 }, { 53.0, 0.5 }, { 63.5, 0.5 },
        { 76.0, 1.0 }, { 86.5, 1.0 },
        { 100.0, 1.5 }, { 110.5, 1.5 },
    };
    for (uint32_t i = 0; i < cr_count(columns); i++) {
        items[count++] = ui_test_rect_item(columns[i].x, 408.0, columns[i].w, 88.0, 0.0, false, ui_label.value);
    }

    for (uint32_t i = 0; i < 6; i++) {
        items[count++] = ui_test_rect_item(160.0 + (double)i * 56.5, 424.5, 40.5, 40.5, 8.0, false, ui_test_palette_(i));
    }

    assert(count <= 40);
    *spec = (struct ui_test_sui_spec) { .background = ui_system_background.value, .items = items, .item_count = count };
}

static void ui_test_scene_rect_outline_fill(struct ui_test_sui_spec* spec, struct ui_test_sui_item* items) {
    uint32_t count = 0;

    static const float center_ladder[] = { 0.5f, 1.0f, 2.0f, 4.0f, 8.0f, 16.0f, 32.0f };
    for (uint32_t i = 0; i < cr_count(center_ladder); i++) {
        items[count++] = ui_test_rect_stroke_item(48.0 + (double)i * 136.0, 48.0, 104.0, 72.0, 20.0, i >= 4,
                                                  ui_outline_align_center, center_ladder[i], ui_system_blue.value);
        struct ui_test_sui_item circle = ui_test_rect_stroke_item(48.0 + (double)i * 136.0, 168.0, 88.0, 88.0, 0.0, false,
                                                                  ui_outline_align_center, center_ladder[i], ui_system_red.value);
        circle.kind = ui_test_sui_kind_ellipse;
        items[count++] = circle;
    }

    static const float inside_ladder[] = { 1.0f, 2.0f, 4.0f, 8.0f, 16.0f, 24.0f };
    for (uint32_t i = 0; i < cr_count(inside_ladder); i++) {
        items[count++] = ui_test_rect_stroke_item(48.0 + (double)i * 160.0, 312.0, 120.0, 80.0, 24.0, true,
                                                  ui_outline_align_inside, inside_ladder[i], ui_system_indigo.value);
    }

    static const float outside_ladder[] = { 1.0f, 2.0f, 4.0f, 8.0f, 16.0f };
    for (uint32_t i = 0; i < cr_count(outside_ladder); i++) {
        items[count++] = ui_test_rect_stroke_item(64.0 + (double)i * 176.0, 440.0, 120.0, 80.0, 24.0, true,
                                                  ui_outline_align_outside, outside_ladder[i], ui_system_teal.value);
    }

    static const float circle_inside[] = { 2.0f, 6.0f, 12.0f, 20.0f };
    for (uint32_t i = 0; i < cr_count(circle_inside); i++) {
        struct ui_test_sui_item circle = ui_test_rect_stroke_item(48.0 + (double)i * 128.0, 576.0, 88.0, 88.0, 0.0, false,
                                                                  ui_outline_align_inside, circle_inside[i], ui_system_purple.value);
        circle.kind = ui_test_sui_kind_ellipse;
        items[count++] = circle;
    }
    static const float circle_outside[] = { 2.0f, 6.0f, 12.0f };
    for (uint32_t i = 0; i < cr_count(circle_outside); i++) {
        struct ui_test_sui_item circle = ui_test_rect_stroke_item(592.0 + (double)i * 144.0, 576.0, 88.0, 88.0, 0.0, false,
                                                                  ui_outline_align_outside, circle_outside[i], ui_system_pink.value);
        circle.kind = ui_test_sui_kind_ellipse;
        items[count++] = circle;
    }

    items[count++] = ui_test_rect_item(32.0, 712.0, 960.0, 128.0, 16.0, false, ui_system_gray4.value);
    static const float translucent_ladder[] = { 2.0f, 8.0f, 24.0f };
    for (uint32_t i = 0; i < cr_count(translucent_ladder); i++) {
        struct ui_test_sui_item item = ui_test_rect_stroke_item(64.0 + (double)i * 200.0, 736.0, 128.0, 80.0, 20.0, false,
                                                                ui_outline_align_center, translucent_ladder[i],
                                                                make_float4(ui_system_purple.value.x, ui_system_purple.value.y, ui_system_purple.value.z, 0.5f));
        items[count++] = item;
    }
    struct ui_test_sui_item combo = ui_test_rect_item(672.0, 736.0, 128.0, 80.0, 20.0, false, ui_system_teal.value);
    combo.stroke = ui_system_indigo.value;
    combo.stroke_width = 6.0f;
    combo.stroke_align = ui_outline_align_center;
    items[count++] = combo;
    struct ui_test_sui_item combo_translucent = ui_test_rect_item(832.0, 736.0, 128.0, 80.0, 20.0, false, ui_system_yellow.value);
    combo_translucent.stroke = make_float4(0.0f, 0.0f, 0.0f, 0.4f);
    combo_translucent.stroke_width = 10.0f;
    combo_translucent.stroke_align = ui_outline_align_center;
    items[count++] = combo_translucent;

    static const float hairline_ladder[] = { 0.25f, 0.5f, 0.75f };
    for (uint32_t i = 0; i < cr_count(hairline_ladder); i++) {
        items[count++] = ui_test_rect_stroke_item(48.0 + (double)i * 152.0, 888.0, 104.0, 72.0, 16.0, false,
                                                  ui_outline_align_center, hairline_ladder[i], ui_label.value);
        struct ui_test_sui_item circle = ui_test_rect_stroke_item(536.0 + (double)i * 152.0, 888.0, 72.0, 72.0, 0.0, false,
                                                                  ui_outline_align_center, hairline_ladder[i], ui_label.value);
        circle.kind = ui_test_sui_kind_ellipse;
        items[count++] = circle;
    }

    assert(count <= 64);
    *spec = (struct ui_test_sui_spec) { .background = ui_system_gray6.value, .items = items, .item_count = count };
}

static void ui_test_scene_ellipse_fill(struct ui_test_sui_spec* spec, struct ui_test_sui_item* items) {
    uint32_t count = 0;

    static const double ladder[] = { 8.0, 16.0, 32.0, 64.0, 128.0, 256.0 };
    double ladder_x = 32.0;
    for (uint32_t i = 0; i < cr_count(ladder); i++) {
        items[count++] = ui_test_ellipse_item(ladder_x, 32.0, ladder[i], ladder[i], ui_test_palette_(i));
        ladder_x += ladder[i] + 24.0;
    }

    for (uint32_t i = 0; i < 64; i++) {
        items[count++] = ui_test_ellipse_item(736.0 + (double)(i % 8) * 32.0, 32.0 + (double)(i / 8) * 32.0, 24.0, 24.0,
                                              ui_test_palette_(i));
    }

    items[count++] = ui_test_ellipse_item(32.0, 384.0, 512.0, 512.0, ui_system_mint.value);

    items[count++] = ui_test_ellipse_item(608.0, 352.0, 384.0, 96.0, ui_system_orange.value);
    struct ui_test_sui_item stroked_wide = ui_test_ellipse_item(608.0, 464.0, 384.0, 96.0, make_float4(0.0f, 0.0f, 0.0f, 0.0f));
    stroked_wide.stroke = ui_system_indigo.value;
    stroked_wide.stroke_width = 4.0f;
    stroked_wide.stroke_align = ui_outline_align_center;
    items[count++] = stroked_wide;
    items[count++] = ui_test_ellipse_item(608.0, 576.0, 96.0, 320.0, ui_system_purple.value);
    items[count++] = ui_test_ellipse_item(736.0, 576.0, 256.0, 320.0, ui_system_cyan.value);
    items[count++] = ui_test_ellipse_item(608.0, 928.0, 384.0, 48.0, ui_system_brown.value);

    static const float circle_strokes[] = { 2.0f, 8.0f, 16.0f };
    for (uint32_t i = 0; i < cr_count(circle_strokes); i++) {
        struct ui_test_sui_item circle = ui_test_ellipse_item(64.0 + (double)i * 160.0, 928.0, 64.0, 64.0, make_float4(0.0f, 0.0f, 0.0f, 0.0f));
        circle.stroke = ui_system_red.value;
        circle.stroke_width = circle_strokes[i];
        circle.stroke_align = ui_outline_align_center;
        items[count++] = circle;
    }

    assert(count <= 96);
    *spec = (struct ui_test_sui_spec) { .background = ui_system_background.value, .items = items, .item_count = count };
}

static struct ui_test_sui_item ui_test_rect_transform_item(double x, double y, double w, double h, double radius, bool circular,
                                                           double angle_deg, double2 scale, double2 translate, float4 fill) {
    struct ui_test_sui_item item = ui_test_rect_item(x, y, w, h, radius, circular, fill);
    double a = angle_deg * M_PI / 180.0;
    item.transform_x = make_double2(cos(a) * scale.x, sin(a) * scale.x);
    item.transform_y = make_double2(-sin(a) * scale.y, cos(a) * scale.y);
    item.transform_t = translate;
    return item;
}

static void ui_test_scene_rect_transform_fill(struct ui_test_sui_spec* spec, struct ui_test_sui_item* items) {
    uint32_t count = 0;
    double2 one = make_double2(1.0, 1.0);
    double2 zero = make_double2(0.0, 0.0);

    static const double angles[] = { 5.0, 15.0, 30.0, 45.0, 60.0, 80.0 };
    for (uint32_t i = 0; i < cr_count(angles); i++) {
        items[count++] = ui_test_rect_transform_item(144.0 + (double)i * 148.0, 48.0, 96.0, 96.0, 16.0, false,
                                                     angles[i], one, zero, ui_system_blue.value);
        items[count++] = ui_test_rect_transform_item(144.0 + (double)i * 148.0, 224.0, 96.0, 96.0, 16.0, true,
                                                     angles[i], one, zero, ui_system_teal.value);
    }

    static const double2 scales[] = { { 0.5, 0.5 }, { 0.75, 0.75 }, { 1.25, 1.25 }, { 1.5, 1.5 }, { 1.5, 0.75 }, { 0.6, 1.4 } };
    for (uint32_t i = 0; i < cr_count(scales); i++) {
        items[count++] = ui_test_rect_transform_item(48.0 + (double)i * 164.0, 400.0, 96.0, 96.0, 12.0, i >= 4,
                                                     0.0, scales[i], zero, ui_system_indigo.value);
    }

    static const double combo_angles[] = { 15.0, 30.0, 45.0, 60.0 };
    for (uint32_t i = 0; i < cr_count(combo_angles); i++) {
        items[count++] = ui_test_rect_transform_item(176.0 + (double)i * 216.0, 592.0, 80.0, 80.0, 10.0, false,
                                                     combo_angles[i], make_double2(1.5, 1.5), make_double2(10.5, 6.25), ui_system_purple.value);
    }

    static const double outline_angles[] = { 12.0, 33.0, 57.0, 78.0 };
    for (uint32_t i = 0; i < cr_count(outline_angles); i++) {
        struct ui_test_sui_item item = ui_test_rect_transform_item(160.0 + (double)i * 220.0, 800.0, 96.0, 96.0, 20.0, true,
                                                                   outline_angles[i], one, zero, ui_system_green.value);
        item.stroke = ui_system_red.value;
        item.stroke_width = 4.0f;
        item.stroke_align = ui_outline_align_center;
        items[count++] = item;

        items[count++] = ui_test_rect_transform_item(160.0 + (double)i * 220.0 + 128.0, 800.0 + 0.35, 48.0, 48.0, 24.0, true,
                                                     outline_angles[i], one, make_double2(0.4, 0.0), ui_system_orange.value);
    }

    assert(count <= 64);
    *spec = (struct ui_test_sui_spec) { .background = ui_system_background.value, .items = items, .item_count = count };
}

#define ui_test_rect_perf_grid 40
#define ui_test_rect_perf_item_max (ui_test_rect_perf_grid * ui_test_rect_perf_grid + 1)

static void ui_test_scene_rect_many_fill(struct ui_test_sui_spec* spec, struct ui_test_sui_item* items) {
    uint32_t count = 0;
    for (uint32_t row = 0; row < ui_test_rect_perf_grid; row++) {
        for (uint32_t col = 0; col < ui_test_rect_perf_grid; col++) {
            double x = 4.0 + (double)col * 16.0;
            double y = 5.0 + (double)row * 17.0;
            float4 fill = make_float4(0.2f + 0.01f * (float)col, 0.4f, 0.6f, 1.0f);
            struct ui_test_sui_item item = (col & 1)
                ? ui_test_rect_item(x, y, 15.0, 14.0, 4.0, false, fill)
                : ui_test_rect_item(x, y, 15.0, 14.0, 7.0, true, fill);
            if ((col & 3) == 0) {
                item.stroke = ui_gray(0.9f).value;
                item.stroke_width = 1.0f;
                item.stroke_align = ui_outline_align_inside;
            }
            items[count++] = item;
        }
    }
    assert(count <= ui_test_rect_perf_item_max);
    *spec = (struct ui_test_sui_spec) { .background = ui_gray(0.1f).value, .items = items, .item_count = count };
}

#define ui_test_nest_deep_depth 500
#define ui_test_nest_deep_leaf 4.0

static struct ui_node ui_test_scene_nest_deep_build(struct ui_test_scene_state* state, struct ui_node parent, double2 size, const struct ui_test_scene* scene) {
    struct ui_node root = ui_frame(&(struct ui_frame) {
        .parent = parent,
        .size = ui_size_fix(size.x, size.y),
        .fill = ui_fill_solid(ui_gray(0.1f)),
    });
    struct ui_node chain = root;

    for (uint32_t i = 0; i < ui_test_nest_deep_depth; i++) {
        chain = ui_frame(&(struct ui_frame) {
            .parent = chain,
            .size = ui_size_fit(),
            .layout = ui_layout_stack_y(ui_dist_start, ui_align_start, 0, ui_pad(1)),
        });
    }

    ui_rect(&(struct ui_rect) {
        .parent = chain,
        .size = ui_size_fix(ui_test_nest_deep_leaf, ui_test_nest_deep_leaf),
        .fill = ui_fill_solid(ui_white),
    });

    return root;
}

#if cr_ios
static void ui_test_scene_nest_deep_pixels(struct ui_test_scene_state* state, uint32_t width, uint32_t height, float to_px_from_pt, float* out_rgba, uint32_t bytes_per_row) {
    CGContextRef ctx = ui_test_ref_begin_(width, height, ui_gray(0.1f));
    CGContextScaleCTM(ctx, to_px_from_pt, to_px_from_pt);
    ui_test_ref_set_fill_(ctx, ui_white);
    CGContextFillRect(ctx, CGRectMake(ui_test_nest_deep_depth, ui_test_nest_deep_depth, ui_test_nest_deep_leaf, ui_test_nest_deep_leaf));
    ui_test_ref_end_(ctx, width, height, out_rgba, bytes_per_row);
}
#endif

static uint32_t ui_test_rect_transform_grid_items(struct ui_test_sui_item* items, bool unique) {
    float4 fill = make_float4(0.25f, 0.45f, 0.65f, 1.0f);
    uint32_t count = 0;
    for (uint32_t row = 0; row < ui_test_rect_perf_grid; row++) {
        for (uint32_t col = 0; col < ui_test_rect_perf_grid; col++) {
            double x = 4.0 + (double)col * 25.0;
            double y = 4.0 + (double)row * 25.0;
            if (unique) {
                double angle_deg = ((double)(count % 13) - 6.0) * 0.25;
                items[count++] = ui_test_rect_transform_item(x, y, 22.0, 22.0, 5.0, false, angle_deg, make_double2(1.0, 1.0), make_double2(0.0, 0.0), fill);
            } else {
                items[count++] = ui_test_rect_item(x, y, 22.0, 22.0, 5.0, false, fill);
            }
        }
    }
    return count;
}

static void ui_test_scene_transform_none_fill(struct ui_test_sui_spec* spec, struct ui_test_sui_item* items) {
    uint32_t count = ui_test_rect_transform_grid_items(items, false);
    *spec = (struct ui_test_sui_spec) { .background = ui_gray(0.1f).value, .items = items, .item_count = count };
}

static void ui_test_scene_transform_unique_fill(struct ui_test_sui_spec* spec, struct ui_test_sui_item* items) {
    uint32_t count = ui_test_rect_transform_grid_items(items, true);
    *spec = (struct ui_test_sui_spec) { .background = ui_gray(0.1f).value, .items = items, .item_count = count };
}

static void ui_test_scene_transform_shared_fill(struct ui_test_sui_spec* spec, struct ui_test_sui_item* items) {
    struct ui_test_sui_item container = ui_test_rect_item(0.0, 0.0, 1024.0, 1024.0, 0.0, false, make_float4(0.0f, 0.0f, 0.0f, 0.0f));
    container.transform_x = make_double2(0.99987, 0.01605);
    container.transform_y = make_double2(-0.01605, 0.99987);
    container.transform_t = make_double2(2.5, 1.25);
    container.group = 1;
    items[0] = container;
    uint32_t count = 1 + ui_test_rect_transform_grid_items(items + 1, false);
    for (uint32_t i = 1; i < count; i++) {
        items[i].group = 1;
    }
    assert(count <= ui_test_rect_perf_item_max);
    *spec = (struct ui_test_sui_spec) { .background = ui_gray(0.1f).value, .items = items, .item_count = count };
}

static const struct ui_test_scene ui_test_scenes_rect_table_[] = {
    {
        .label = "rect-transform", .group = "rect",
        .capture = ui_test_scene_capture_screenshot,
        .size = { 1024.0, 1024.0 },
        .mean_threshold = 0.002f,
        .tile_threshold = 0.012f,
        .alarm_allowance = 64,
        ui_test_sui_scene(ui_test_scene_rect_transform, 64),
    },
    {
        .label = "rect-grid", .group = "rect",
        .capture = ui_test_scene_capture_screenshot,
        .size = { 1024.0, 1024.0 },
        .mean_threshold = 0.002f,
        .tile_threshold = 0.012f,
        .alarm_allowance = 64,
        ui_test_sui_scene(ui_test_scene_rect_grid, ui_test_rect_grid_item_max),
    },
    {
        .label = "rect-grid-circular", .group = "rect",
        .capture = ui_test_scene_capture_screenshot,
        .size = { 1024.0, 1024.0 },
        .mean_threshold = 0.002f,
        .tile_threshold = 0.012f,
        .alarm_allowance = 64,
        ui_test_sui_scene(ui_test_scene_rect_grid_circular, ui_test_rect_grid_item_max),
    },
    {
        .label = "rect-radii", .group = "rect",
        .capture = ui_test_scene_capture_screenshot,
        .size = { 1024.0, 1024.0 },
        .mean_threshold = 0.002f,
        .tile_threshold = 0.012f,
        .alarm_allowance = 64,
        ui_test_sui_scene(ui_test_scene_rect_radii, 80),
    },
    {
        .label = "rect-uneven", .group = "rect",
        .capture = ui_test_scene_capture_screenshot,
        .size = { 1024.0, 1024.0 },
        .mean_threshold = 0.002f,
        .tile_threshold = 0.012f,
        .alarm_allowance = 64,
        ui_test_sui_scene(ui_test_scene_rect_uneven, 16),
    },
    {
        .label = "rect-continuous", .group = "rect",
        .capture = ui_test_scene_capture_screenshot,
        .size = { 1024.0, 1024.0 },
        .mean_threshold = 0.002f,
        .tile_threshold = 0.012f,
        .alarm_allowance = 64,
        ui_test_sui_scene(ui_test_scene_rect_continuous, 32),
    },
    {
        .label = "rect-fractional", .group = "rect",
        .capture = ui_test_scene_capture_screenshot,
        .size = { 512.0, 512.0 },
        .mean_threshold = 0.002f,
        .tile_threshold = 0.012f,
        .alarm_allowance = 64,
        ui_test_sui_scene(ui_test_scene_rect_fractional, 40),
    },
    {
        .label = "rect-outline", .group = "rect",
        .capture = ui_test_scene_capture_screenshot,
        .size = { 1024.0, 1024.0 },
        .mean_threshold = 0.002f,
        .tile_threshold = 0.012f,
        .alarm_allowance = 64,
        ui_test_sui_scene(ui_test_scene_rect_outline, 64),
    },
    {
        .label = "ellipse", .group = "rect",
        .capture = ui_test_scene_capture_screenshot,
        .size = { 1024.0, 1024.0 },
        .mean_threshold = 0.002f,
        .tile_threshold = 0.012f,
        .alarm_allowance = 64,
        ui_test_sui_scene(ui_test_scene_ellipse, 96),
    },
    {
        .label = "rect-many", .group = "rect",
        .capture = ui_test_scene_capture_screenshot,
        .size = { 1024.0, 1024.0 },
        .mean_threshold = 0.003f,
        .tile_threshold = 0.012f,
        .alarm_allowance = 1024,
        ui_test_sui_scene(ui_test_scene_rect_many, ui_test_rect_perf_item_max),
    },
    {
        .label = "nest-deep", .group = "rect",
        .capture = ui_test_scene_capture_pixels,
        .size = { 1024.0, 1024.0 }, .to_px_from_pt = 2.0f,
        .mean_threshold = 0.002f,
        .build = ui_test_scene_nest_deep_build,
        .pixels = ui_test_scene_pixels(ui_test_scene_nest_deep_pixels),
    },
    {
        .label = "transform-none", .group = "rect",
        .capture = ui_test_scene_capture_screenshot,
        .size = { 1024.0, 1024.0 },
        .mean_threshold = 0.003f,
        .tile_threshold = 0.012f,
        .alarm_allowance = 1024,
        ui_test_sui_scene(ui_test_scene_transform_none, ui_test_rect_perf_item_max),
    },
    {
        .label = "transform-shared", .group = "rect",
        .capture = ui_test_scene_capture_screenshot,
        .size = { 1024.0, 1024.0 },
        .mean_threshold = 0.004f,
        .tile_threshold = 0.02f,
        .alarm_allowance = 2048,
        ui_test_sui_scene(ui_test_scene_transform_shared, ui_test_rect_perf_item_max),
    },
    {
        .label = "transform-unique", .group = "rect",
        .capture = ui_test_scene_capture_screenshot,
        .size = { 1024.0, 1024.0 },
        .mean_threshold = 0.004f,
        .tile_threshold = 0.02f,
        .alarm_allowance = 2048,
        ui_test_sui_scene(ui_test_scene_transform_unique, ui_test_rect_perf_item_max),
    },
};

const struct ui_test_scene* ui_test_scenes_rect_(struct ui_test_scenes* scenes, uint32_t* out_count) {
    *out_count = cr_count(ui_test_scenes_rect_table_);
    return ui_test_scenes_rect_table_;
}
