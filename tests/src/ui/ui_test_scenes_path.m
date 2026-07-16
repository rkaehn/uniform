#include "ui_tests.h"
#include <math.h>

static struct ui_test_sui_item ui_test_path_item(struct ui_path* path, float4 color, double thickness) {
    return (struct ui_test_sui_item) {
        .kind = ui_test_sui_kind_path,
        .path = path,
        .path_thickness = (float)thickness,
        .fill = color,
    };
}

static double2 ui_test_path_polar(double2 center, double radius, double angle) {
    return make_double2(center.x + radius * cos(angle), center.y + radius * sin(angle));
}

static void ui_test_path_star(struct ui_path* path, double2 center, double outer, double inner, uint32_t points, double rot) {
    for (uint32_t i = 0; i < points * 2; i++) {
        double radius = i % 2 == 0 ? outer : inner;
        double angle = rot + (double)i * M_PI / (double)points;
        double2 p = ui_test_path_polar(center, radius, angle);
        if (i == 0) {
            ui_path_move_to(path, p);
        } else {
            ui_path_line_to(path, p);
        }
    }
    ui_path_close(path);
}

static void ui_test_path_pentagram(struct ui_path* path, double2 center, double radius, double rot) {
    for (uint32_t i = 0; i < 5; i++) {
        double angle = rot + (double)(i * 2) * 0.4 * M_PI;
        double2 p = ui_test_path_polar(center, radius, angle);
        if (i == 0) {
            ui_path_move_to(path, p);
        } else {
            ui_path_line_to(path, p);
        }
    }
    ui_path_close(path);
}

static void ui_test_path_circle(struct ui_path* path, double2 center, double radius) {
    ui_path_move_to(path, make_double2(center.x + radius, center.y));
    ui_path_arc(path, center, radius, 0.0, 2.0 * M_PI, false);
}

static void ui_test_path_wave(struct ui_path* path, double2 start, double width, double amplitude, uint32_t periods) {
    double step = width / (double)periods;
    ui_path_move_to(path, start);
    for (uint32_t i = 0; i < periods; i++) {
        double x = start.x + (double)i * step;
        ui_path_cubic_to(path,
                         make_double2(x + step * 0.36, start.y - amplitude),
                         make_double2(x + step * 0.64, start.y + amplitude),
                         make_double2(x + step, start.y));
    }
}

static void ui_test_path_blob(struct ui_path* path, double2 center, double radius) {
    double2 top = make_double2(center.x, center.y - radius);
    ui_path_move_to(path, top);
    ui_path_cubic_to(path,
                     make_double2(center.x + radius * 1.2, center.y - radius * 0.9),
                     make_double2(center.x + radius * 1.1, center.y + radius * 0.4),
                     make_double2(center.x + radius * 0.3, center.y + radius * 0.9));
    ui_path_cubic_to(path,
                     make_double2(center.x - radius * 0.3, center.y + radius * 1.3),
                     make_double2(center.x - radius * 1.3, center.y + radius * 0.5),
                     make_double2(center.x - radius * 0.9, center.y - radius * 0.2));
    ui_path_cubic_to(path,
                     make_double2(center.x - radius * 0.6, center.y - radius * 0.7),
                     make_double2(center.x - radius * 0.4, center.y - radius),
                     top);
    ui_path_close(path);
}

static void ui_test_path_flower(struct ui_path* path, double2 center, double outer, double inner, uint32_t petals) {
    double2 start = ui_test_path_polar(center, inner, 0.0);
    ui_path_move_to(path, start);
    for (uint32_t i = 0; i < petals; i++) {
        double control_angle = ((double)i + 0.5) * 2.0 * M_PI / (double)petals;
        double end_angle = (double)(i + 1) * 2.0 * M_PI / (double)petals;
        ui_path_quad_to(path, ui_test_path_polar(center, outer, control_angle), ui_test_path_polar(center, inner, end_angle));
    }
    ui_path_close(path);
}

static void ui_test_scene_path_fill_fill(struct ui_test_sui_spec* spec, struct ui_test_sui_item* items) {
    uint32_t count = 0;

    struct ui_path* path = ui_path_create();
    ui_test_path_pentagram(path, make_double2(192.0, 192.0), 150.0, -0.5 * M_PI);
    items[count++] = ui_test_path_item(path, ui_test_palette_(0), 0.0);

    path = ui_path_create();
    ui_test_path_circle(path, make_double2(512.0, 192.0), 150.0);
    ui_test_path_circle(path, make_double2(512.0, 192.0), 80.0);
    items[count++] = ui_test_path_item(path, ui_test_palette_(7), 0.0);

    path = ui_path_create();
    ui_test_path_blob(path, make_double2(832.0, 200.0), 120.0);
    items[count++] = ui_test_path_item(path, ui_test_palette_(3), 0.0);

    path = ui_path_create();
    ui_path_move_to(path, make_double2(64.0, 440.0));
    ui_path_line_to(path, make_double2(320.0, 640.0));
    ui_path_line_to(path, make_double2(320.0, 440.0));
    ui_path_line_to(path, make_double2(64.0, 640.0));
    ui_path_close(path);
    items[count++] = ui_test_path_item(path, ui_test_palette_(1), 0.0);

    path = ui_path_create();
    ui_test_path_star(path, make_double2(512.0, 540.0), 150.0, 110.0, 16, 0.0);
    items[count++] = ui_test_path_item(path, ui_test_palette_(8), 0.0);

    path = ui_path_create();
    ui_path_move_to(path, make_double2(736.0, 460.0));
    ui_path_line_to(path, make_double2(816.0, 620.0));
    ui_path_line_to(path, make_double2(656.0 + 80.0, 460.0));
    ui_path_close(path);
    ui_path_move_to(path, make_double2(856.0, 460.0));
    ui_path_line_to(path, make_double2(984.0, 460.0));
    ui_path_line_to(path, make_double2(984.0, 560.0));
    ui_path_line_to(path, make_double2(856.0, 560.0));
    ui_path_close(path);
    ui_test_path_circle(path, make_double2(908.0, 620.0), 36.0);
    items[count++] = ui_test_path_item(path, ui_test_palette_(5), 0.0);

    path = ui_path_create();
    ui_path_move_to(path, make_double2(96.0, 780.0));
    ui_path_line_to(path, make_double2(240.0, 780.0));
    ui_path_line_to(path, make_double2(240.0, 732.0));
    ui_path_line_to(path, make_double2(336.0, 828.0));
    ui_path_line_to(path, make_double2(240.0, 924.0));
    ui_path_line_to(path, make_double2(240.0, 876.0));
    ui_path_line_to(path, make_double2(96.0, 876.0));
    ui_path_close(path);
    items[count++] = ui_test_path_item(path, ui_test_palette_(9), 0.0);

    path = ui_path_create();
    ui_test_path_circle(path, make_double2(472.0, 828.0), 96.0);
    ui_test_path_circle(path, make_double2(568.0, 828.0), 96.0);
    items[count++] = ui_test_path_item(path, ui_test_palette_(10), 0.0);

    path = ui_path_create();
    ui_test_path_flower(path, make_double2(852.0, 828.0), 140.0, 76.0, 8);
    items[count++] = ui_test_path_item(path, ui_test_palette_(11), 0.0);

    assert(count <= 12);
    *spec = (struct ui_test_sui_spec) { .background = ui_system_background.value, .items = items, .item_count = count };
}

static void ui_test_scene_path_stroke_fill(struct ui_test_sui_spec* spec, struct ui_test_sui_item* items) {
    uint32_t count = 0;
    struct ui_path* path;

    static const double thicknesses[] = { 0.5, 1.0, 2.0, 4.0, 8.0, 16.0, 32.0 };
    for (uint32_t i = 0; i < cr_count(thicknesses); i++) {
        path = ui_path_create();
        ui_test_path_wave(path, make_double2(48.0, 80.0 + (double)i * 92.0), 928.0, 26.0, 4);
        items[count++] = ui_test_path_item(path, ui_test_palette_(i), thicknesses[i]);
    }

    path = ui_path_create();
    ui_path_move_to(path, make_double2(96.0, 900.0));
    ui_path_line_to(path, make_double2(240.0, 760.0));
    ui_path_line_to(path, make_double2(240.0, 900.0));
    ui_path_close(path);
    items[count++] = ui_test_path_item(path, ui_test_palette_(8), 6.0);

    double2 arc_center = make_double2(430.0, 840.0);
    path = ui_path_create();
    ui_path_move_to(path, ui_test_path_polar(arc_center, 84.0, -0.5 * M_PI));
    ui_path_arc(path, arc_center, 84.0, -0.5 * M_PI, M_PI, false);
    items[count++] = ui_test_path_item(path, ui_test_palette_(9), 12.0);

    path = ui_path_create();
    ui_test_path_blob(path, make_double2(680.0, 840.0), 82.0);
    items[count++] = ui_test_path_item(path, ui_test_palette_(10), 3.0);

    path = ui_path_create();
    ui_test_path_flower(path, make_double2(890.0, 840.0), 96.0, 52.0, 6);
    items[count++] = ui_test_path_item(path, ui_test_palette_(11), 2.0);

    assert(count <= 16);
    *spec = (struct ui_test_sui_spec) { .background = ui_system_background.value, .items = items, .item_count = count };
}

static void ui_test_scene_path_thin_fill(struct ui_test_sui_spec* spec, struct ui_test_sui_item* items) {
    uint32_t count = 0;
    struct ui_path* path;

    static const double thicknesses[] = { 0.25, 0.5, 0.75, 1.0, 1.5 };
    for (uint32_t i = 0; i < cr_count(thicknesses); i++) {
        path = ui_path_create();
        ui_test_path_wave(path, make_double2(32.0, 56.0 + (double)i * 48.0), 960.0, 18.0, 12);
        items[count++] = ui_test_path_item(path, ui_label.value, thicknesses[i]);
    }

    path = ui_path_create();
    for (uint32_t tile = 0; tile < 84; tile++) {
        uint32_t tx = tile % 14;
        uint32_t ty = tile / 14;
        double x = 64.0 + (double)tx * 64.0;
        double y = 688.0 + (double)ty * 48.0;
        if ((tx + ty) % 2 == 0) {
            ui_path_move_to(path, make_double2(x, y + 24.0));
            ui_path_arc(path, make_double2(x, y), 24.0, 0.5 * M_PI, 0.0, true);
        } else {
            ui_path_move_to(path, make_double2(x + 64.0 - 24.0, y));
            ui_path_arc(path, make_double2(x + 64.0, y), 24.0, M_PI, 0.5 * M_PI, true);
        }
    }
    items[count++] = ui_test_path_item(path, ui_system_red.value, 2.0);

    assert(count <= 12);
    *spec = (struct ui_test_sui_spec) { .background = ui_system_background.value, .items = items, .item_count = count };
}

static void ui_test_scene_path_overlap_fill(struct ui_test_sui_spec* spec, struct ui_test_sui_item* items) {
    uint32_t count = 0;

    struct ui_path* path = ui_path_create();
    for (uint32_t i = 0; i < 24; i++) {
        ui_path_move_to(path, make_double2(32.0, 128.0));
        double t = (double)i / 23.0;
        ui_path_line_to(path, make_double2(992.0, 136.0 + t * 720.0));
    }
    items[count++] = ui_test_path_item(path, ui_system_blue.value, 0.5);

    assert(count <= 2);
    *spec = (struct ui_test_sui_spec) { .background = ui_system_background.value, .items = items, .item_count = count };
}

static void ui_test_scene_path_dense_fill(struct ui_test_sui_spec* spec, struct ui_test_sui_item* items) {
    uint32_t count = 0;
    struct ui_path* path;

    for (uint32_t i = 0; i < 100; i++) {
        double2 center = make_double2(80.0 + (double)(i % 10) * 96.0, 80.0 + (double)(i / 10) * 68.0);
        path = ui_path_create();
        ui_test_path_star(path, center, 30.0, 13.0, 5, -0.5 * M_PI);
        items[count++] = ui_test_path_item(path, ui_test_palette_(i), 0.0);
    }

    path = ui_path_create();
    ui_test_path_wave(path, make_double2(32.0, 800.0), 960.0, 32.0, 48);
    items[count++] = ui_test_path_item(path, ui_system_indigo.value, 2.0);

    path = ui_path_create();
    ui_test_path_wave(path, make_double2(32.0, 900.0), 960.0, 40.0, 24);
    items[count++] = ui_test_path_item(path, ui_system_teal.value, 4.0);

    assert(count <= 112);
    *spec = (struct ui_test_sui_spec) { .background = ui_system_background.value, .items = items, .item_count = count };
}

#define ui_test_path_many_count 128
#define ui_test_path_many_segments 32

static void ui_test_scene_path_many_fill(struct ui_test_sui_spec* spec, struct ui_test_sui_item* items) {
    for (uint32_t s = 0; s < ui_test_path_many_count; s++) {
        struct ui_path* path = ui_path_create();
        float cx = 40.0f + (float)(s % 16) * 60.0f;
        float cy = 40.0f + (float)(s / 16) * 120.0f;
        float radius = 26.0f + (float)(s % 5);

        float a0 = 0.0f;
        float r0 = radius * (1.0f + 0.25f * sinf(6.0f * a0));
        ui_path_move_to(path, make_double2(cx + r0 * cosf(a0), cy + r0 * sinf(a0)));

        for (uint32_t k = 0; k < ui_test_path_many_segments; k++) {
            float b0 = (float)k * (float)M_PI * 2.0f / (float)ui_test_path_many_segments;
            float b1 = (float)(k + 1) * (float)M_PI * 2.0f / (float)ui_test_path_many_segments;
            float ra = radius * (1.0f + 0.25f * sinf(6.0f * b0));
            float rb = radius * (1.0f + 0.25f * sinf(6.0f * b1));
            float p0x = cx + ra * cosf(b0);
            float p0y = cy + ra * sinf(b0);
            float p1x = cx + rb * cosf(b1);
            float p1y = cy + rb * sinf(b1);
            float d = 0.35f * radius * ((float)M_PI * 2.0f / (float)ui_test_path_many_segments);
            ui_path_cubic_to(path,
                             make_double2(p0x - d * sinf(b0), p0y + d * cosf(b0)),
                             make_double2(p1x + d * sinf(b1), p1y - d * cosf(b1)),
                             make_double2(p1x, p1y));
        }

        ui_path_close(path);

        items[s] = ui_test_path_item(path, make_float4(0.3f, 0.5f, 0.7f, 0.8f), 0.0);
    }
    *spec = (struct ui_test_sui_spec) { .background = ui_gray(0.1f).value, .items = items, .item_count = ui_test_path_many_count };
}

static const struct ui_test_scene ui_test_scenes_path_table_[] = {
    {
        .label = "path-fill", .group = "path",
        .capture = ui_test_scene_capture_screenshot,
        .size = { 1024.0, 1024.0 },
        .mean_threshold = 0.002f,
        .tile_threshold = 0.008f,
        .alarm_allowance = 32,
        ui_test_sui_scene(ui_test_scene_path_fill, 12),
    },
    {
        .label = "path-stroke", .group = "path",
        .capture = ui_test_scene_capture_screenshot,
        .size = { 1024.0, 1024.0 },
        .mean_threshold = 0.002f,
        .tile_threshold = 0.014f,
        .alarm_allowance = 32,
        ui_test_sui_scene(ui_test_scene_path_stroke, 16),
    },
    {
        .label = "path-thin", .group = "path",
        .capture = ui_test_scene_capture_screenshot,
        .size = { 1024.0, 1024.0 },
        .mean_threshold = 0.002f,
        .tile_threshold = 0.03f,
        .alarm_allowance = 4096,
        ui_test_sui_scene(ui_test_scene_path_thin, 12),
    },
    {
        .label = "path-overlap", .group = "path",
        .capture = ui_test_scene_capture_screenshot,
        .size = { 1024.0, 1024.0 },
        .mean_threshold = 0.002f,
        .tile_threshold = 0.012f,
        .alarm_allowance = 64,
        ui_test_sui_scene(ui_test_scene_path_overlap, 2),
    },
    {
        .label = "path-dense", .group = "path",
        .capture = ui_test_scene_capture_screenshot,
        .size = { 1024.0, 1024.0 },
        .mean_threshold = 0.002f,
        .tile_threshold = 0.008f,
        .alarm_allowance = 32,
        ui_test_sui_scene(ui_test_scene_path_dense, 112),
    },
    {
        .label = "path-many", .group = "path",
        .capture = ui_test_scene_capture_screenshot,
        .size = { 1024.0, 1024.0 },
        .mean_threshold = 0.002f,
        .tile_threshold = 0.012f,
        .alarm_allowance = 256,
        ui_test_sui_scene(ui_test_scene_path_many, ui_test_path_many_count),
    },
};

const struct ui_test_scene* ui_test_scenes_path_(struct ui_test_scenes* scenes, uint32_t* out_count) {
    *out_count = cr_count(ui_test_scenes_path_table_);
    return ui_test_scenes_path_table_;
}
