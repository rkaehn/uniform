#include "ui_tests.h"
#include "ui_sdf.h"
#include "ui_render.h"
#include "ui_path.h"
#include <stdio.h>


struct ui_test_field_result {
    const char* label;
    uint32_t width;
    uint32_t height;
    uint32_t point_count;
    double max_err;
    uint32_t max_x;
    uint32_t max_y;
    uint32_t outlier_count;
    bool pass;
};

static float3 ui_test_field_path_distance(float2 p, const float2* positions, uint32_t count) {
    float2 d0 = p - positions[0];
    float d = d0.x * d0.x + d0.y * d0.y;
    float2 nearest = d0;
    float s = 1.0f;

    for (uint32_t i = 0, j = count - 1; i < count; j = i, i++) {
        float2 e = positions[j] - positions[i];
        float2 w = p - positions[i];

        float h = (w.x * e.x + w.y * e.y) / (e.x * e.x + e.y * e.y);
        h = min_float(max_float(h, 0.0f), 1.0f);
        float2 b = w - e * h;
        float bd = b.x * b.x + b.y * b.y;
        if (bd < d) {
            d = bd;
            nearest = b;
        }

        bool c1 = p.y >= positions[i].y;
        bool c2 = p.y < positions[j].y;
        bool c3 = e.x * w.y > e.y * w.x;

        if ((c1 && c2 && c3) || (!c1 && !c2 && !c3)) {
            s = -s;
        }
    }

    float len = sqrtf(d);
    float2 g = len > 1e-6f ? nearest * (s / len) : make_float2(1.0f, 0.0f);
    return make_float3(s * len, g.x, g.y);
}

static float3 ui_test_field_fold(uint32_t combine, float smoothing, uint32_t i, float3 f, float3 v) {
    if (i == 0) {
        return v;
    }

    if (combine != ui_sdf_combine_union) {
        v = make_float3(-v.x, -v.y, -v.z);
    }

    bool f_wins = combine == ui_sdf_combine_union ? f.x < v.x : f.x > v.x;

    float d = f_wins ? f.x : v.x;
    float2 g = f_wins ? make_float2(f.y, f.z) : make_float2(v.y, v.z);
    if (smoothing > 0.0f) {
        float k = smoothing * (0.5f - 0.5f * (f.y * v.y + f.z * v.z));
        float h = max_float(k - fabsf(f.x - v.x), 0.0f);
        h = h > 0.0f ? h / k : 0.0f;
        float bump = h * h * k * 0.25f;
        d += combine == ui_sdf_combine_union ? -bump : bump;
        float wv = f_wins ? 0.5f * h : 1.0f - 0.5f * h;
        g = make_float2(f.y + (v.y - f.y) * wv, f.z + (v.z - f.z) * wv);
        if (g.x * g.x + g.y * g.y < 1e-12f) {
            g = make_float2(f.y, f.z);
        }
    }
    return make_float3(d, g.x, g.y);
}

#define ui_test_field_size 1024

struct ui_test_field_case {
    const char* label;
    uint32_t circle_count;
    uint32_t combine;
    float smoothing;
    float band_lo;
    float band_hi;
    double max_err_threshold;
    double rect_height_factor;
    double rect_radius_factor;
    bool rect_circular;
};

static const struct ui_test_field_case ui_test_field_cases_[] = {
    { .label = "union", .circle_count = 1, .combine = ui_sdf_combine_union, .band_lo = -2.0f, .band_hi = 2.0f, .max_err_threshold = 0.05 },
    { .label = "subtract", .circle_count = 1, .combine = ui_sdf_combine_subtract, .band_lo = -2.0f, .band_hi = 2.0f, .max_err_threshold = 0.05 },
    { .label = "union smooth", .circle_count = 1, .combine = ui_sdf_combine_union, .smoothing = 24.0f, .band_lo = -80.0f, .band_hi = 2.0f, .max_err_threshold = 0.05 },
    { .label = "subtract smooth", .circle_count = 1, .combine = ui_sdf_combine_subtract, .smoothing = 24.0f, .band_lo = -80.0f, .band_hi = 2.0f, .max_err_threshold = 0.05 },
    { .label = "union 5 smooth", .circle_count = 5, .combine = ui_sdf_combine_union, .smoothing = 32.0f, .band_lo = -80.0f, .band_hi = 2.0f, .max_err_threshold = 0.05 },
    { .label = "rect circle", .circle_count = 0, .combine = ui_sdf_combine_union, .band_lo = -2.0f, .band_hi = 2.0f, .max_err_threshold = 0.005,
      .rect_radius_factor = 0.375, .rect_circular = true },
    { .label = "rect circle over", .circle_count = 0, .combine = ui_sdf_combine_union, .band_lo = -2.0f, .band_hi = 2.0f, .max_err_threshold = 0.005,
      .rect_radius_factor = 0.6, .rect_circular = true },
    { .label = "rect circle union", .circle_count = 1, .combine = ui_sdf_combine_union, .smoothing = 24.0f, .band_lo = -80.0f, .band_hi = 2.0f, .max_err_threshold = 0.005,
      .rect_radius_factor = 0.375, .rect_circular = true },
    { .label = "rect capsule", .circle_count = 0, .combine = ui_sdf_combine_union, .band_lo = -2.0f, .band_hi = 2.0f, .max_err_threshold = 0.05,
      .rect_height_factor = 0.375, .rect_radius_factor = 0.25, .rect_circular = true },
    { .label = "rect continuous max", .circle_count = 0, .combine = ui_sdf_combine_union, .band_lo = -2.0f, .band_hi = 2.0f, .max_err_threshold = 0.05,
      .rect_radius_factor = 0.375 },
};

struct ui_test_field_read {
    uint32_t index;
    uint32_t width_mid;
    __fp16* fine_data;
    __fp16* mid_data;
};

static struct ui_test_field_read ui_test_field_read_;

static void ui_test_field_case_(uint32_t index);

static uint32_t ui_test_field_prims(const struct ui_test_field_case* c, struct ui_sdf_prim* prims) {
    double width = (double)ui_test_field_size;
    double height = (double)ui_test_field_size;

    double rect_w = 0.75 * width;
    double rect_h = (c->rect_height_factor > 0.0 ? c->rect_height_factor : 0.75) * height;
    double rect_r = (c->rect_radius_factor > 0.0 ? c->rect_radius_factor : 0.125) * width;

    prims[0] = (struct ui_sdf_prim) { .type = ui_sdf_prim_type_rect, .rect = {
        .origin = make_double2(0.5 * (width - rect_w), 0.5 * (height - rect_h)),
        .width = rect_w,
        .height = rect_h,
        .radius = rect_r,
        .continuous = c->rect_circular == false,
    } };

    uint32_t prim_count = 1 + c->circle_count;
    assert(prim_count <= ui_sdf_max_prims);

    for (uint32_t k = 0; k < c->circle_count; k++) {
        double angle = 2.4 * (double)k;
        prims[1 + k] = (struct ui_sdf_prim) { .type = ui_sdf_prim_type_circle, .circle = {
            .center = make_double2((0.5 + 0.3 * cos(angle)) * width, (0.5 + 0.3 * sin(angle)) * height),
            .radius = (0.1 + 0.02 * (double)k) * width,
        } };
    }

    return prim_count;
}

static void ui_test_field_check(void* args) {
    const struct ui_test_field_read* read = args;
    struct cr_mem* mem = ui_render_sdf()->mem;

    const struct ui_test_field_case* c = &ui_test_field_cases_[read->index];

    uint32_t width = ui_test_field_size;
    uint32_t height = ui_test_field_size;

    struct ui_sdf_prim prims[ui_sdf_max_prims] = { 0 };
    uint32_t prim_count = ui_test_field_prims(c, prims);

    double rect_w = prims[0].rect.width;
    double rect_h = prims[0].rect.height;
    double rect_r = prims[0].rect.radius;

    bool rect_is_circle = fabs(rect_w - rect_h) < 1e-3 && rect_r >= 0.5 * min_double(rect_w, rect_h) - 1e-3;
    float2 rect_circle_center = to_float2_from_double2(prims[0].rect.origin + make_double2(0.5 * rect_w, 0.5 * rect_h));
    float rect_circle_radius = (float)(0.5 * min_double(rect_w, rect_h));

    const __fp16* fine_data = read->fine_data;
    const __fp16* mid_data = read->mid_data;

    struct ui_path* path = ui_path_create();
    double rect_radius = prims[0].rect.radius;
    ui_path_rounded_rect(path, prims[0].rect.origin, make_double2(prims[0].rect.width, prims[0].rect.height),
                         (struct ui_corners) {
                             .profile = prims[0].rect.continuous ? ui_corner_profile_continuous : ui_corner_profile_circular,
                             .radius = make_double4(rect_radius, rect_radius, rect_radius, rect_radius),
                         });

    struct ui_arr_double2* points = ui_arr_double2_create(mem);
    ui_path_to_points(path, make_double2(0.0, 0.0), 1.0, points, NULL, 0.1, M_PI);
    uint32_t point_count = ui_arr_double2_get_count(points);
    ui_path_destroy(path);

    float2* positions = cr_malloc(mem, sizeof(float2) * point_count, 256, cr_loc);
    for (uint32_t i = 0; i < point_count; i++) {
        positions[i] = to_float2_from_double2(ui_arr_double2_get(points, i));
    }
    ui_arr_double2_destroy(points);

    struct ui_test_field_result result = {
        .label = c->label,
        .width = width,
        .height = height,
        .point_count = point_count,
    };

    uint32_t stride = 3;

    for (uint32_t y = 0; y < height; y += stride) {
        for (uint32_t x = 0; x < width; x += stride) {
            float2 p = make_float2((float)x + 0.5f, (float)y + 0.5f);

            float3 ref_f = make_float3(0.0f, 0.0f, 0.0f);
            for (uint32_t i = 0; i < prim_count; i++) {
                float3 v;
                if (prims[i].type == ui_sdf_prim_type_circle) {
                    float2 w = p - to_float2_from_double2(prims[i].circle.center);
                    float len = sqrtf(w.x * w.x + w.y * w.y);
                    float2 g = len > 1e-6f ? w * (1.0f / len) : make_float2(1.0f, 0.0f);
                    v = make_float3(len - (float)prims[i].circle.radius, g.x, g.y);
                } else if (rect_is_circle) {
                    float2 w = p - rect_circle_center;
                    float len = sqrtf(w.x * w.x + w.y * w.y);
                    float2 g = len > 1e-6f ? w * (1.0f / len) : make_float2(1.0f, 0.0f);
                    v = make_float3(len - rect_circle_radius, g.x, g.y);
                } else {
                    v = ui_test_field_path_distance(p, positions, point_count);
                }
                ref_f = ui_test_field_fold(c->combine, c->smoothing, i, ref_f, v);
            }
            float ref = ref_f.x;

            float approx = (float)mid_data[4 * ((y / ui_sdf_mid_scale) * read->width_mid + x / ui_sdf_mid_scale)];
            bool in_band = ui_sdf_near_band(approx, c->band_lo, c->band_hi);
            float gpu = in_band ? (float)fine_data[4 * (y * width + x)] : approx;

            float f16_ulp = fabsf(ref) * 0x1p-10f;
            float top_err = 0.5f * (float)ui_sdf_top_scale * (float)M_SQRT2;

            if (ref >= c->band_lo && ref <= c->band_hi) {
                double err = max_double(fabs((double)gpu - (double)ref) - (double)f16_ulp, 0.0);
                if (err > result.max_err) {
                    result.max_err = err;
                    result.max_x = x;
                    result.max_y = y;
                }
            } else if (fabsf(gpu - ref) > top_err + f16_ulp + c->smoothing) {
                result.outlier_count++;
            }
        }
    }

    result.pass = result.max_err <= c->max_err_threshold && result.outlier_count == 0;

    cr_free(mem, positions, cr_loc);
    cr_free(mem, read->mid_data, cr_loc);
    cr_free(mem, read->fine_data, cr_loc);

    printf("field %s %ux%u px: %u points, max err %.5f px at (%u,%u), outliers %u, %s\n",
           result.label, result.width, result.height, result.point_count, result.max_err, result.max_x, result.max_y, result.outlier_count,
           result.pass ? "ok" : "FAIL");
    assert(result.pass);

    if (read->index + 1 < cr_count(ui_test_field_cases_)) {
        ui_test_field_case_(read->index + 1);
    }
}

static void ui_test_field_case_(uint32_t index) {
    struct ui* ui = ui_ctx();
    struct ui_sdf* sdf = ui_render_sdf();

    const struct ui_test_field_case* c = &ui_test_field_cases_[index];

    uint32_t width = ui_test_field_size;
    uint32_t height = ui_test_field_size;

    struct ui_sdf_prim prims[ui_sdf_max_prims] = { 0 };
    struct ui_sdf_geometry geometry = {
        .prims = prims,
        .prim_count = ui_test_field_prims(c, prims),
        .combine = c->combine,
        .smoothing = c->smoothing,
        .band_lo = c->band_lo,
        .band_hi = c->band_hi,
        .flatten_epsilon = 0.1,
        .flatten_max_angle = M_PI,
        .to_px_offset = make_double2(0.0, 0.0),
    };

    struct ui_sdf_frame frame = { 0 };
    ui_sdf_frame_reserve(sdf, ui, &frame, width, height, true);
    ui_sdf_frame_build(sdf, ui, &frame, &geometry);

    struct ui_test_field_read* read = &ui_test_field_read_;
    *read = (struct ui_test_field_read) {
        .index = index,
        .width_mid = frame.width_mid,
        .fine_data = cr_malloc(sdf->mem, 4 * sizeof(__fp16) * width * height, 256, cr_loc),
        .mid_data = cr_malloc(sdf->mem, 4 * sizeof(__fp16) * frame.width_mid * frame.height_mid, 256, cr_loc),
    };

    struct gpu_command_buffer* command_buffer = gpu_command_buffer_create(sdf->device, "test field");

    struct gpu_compute_pass* pass = gpu_command_buffer_begin_compute(command_buffer, "test field top mid");
    ui_sdf_encode_top(sdf, pass, &frame);
    ui_sdf_encode_mid(sdf, pass, &frame);
    gpu_compute_pass_end(pass);

    pass = gpu_command_buffer_begin_compute(command_buffer, "test field refine");
    ui_sdf_encode_refine(sdf, pass, &frame);
    gpu_compute_pass_end(pass);

    gpu_command_buffer_read_texture(command_buffer, frame.fine, read->fine_data, 4 * (uint32_t)sizeof(__fp16) * width);
    gpu_command_buffer_read_texture(command_buffer, frame.mid, read->mid_data, 4 * (uint32_t)sizeof(__fp16) * frame.width_mid);
    ui_test_await_(gpu_command_buffer_commit(command_buffer));

    ui_sdf_frame_destroy(sdf, ui, NULL, &frame);
    ui_test_then_(ui_test_field_check, read);
}

void ui_test_field_(void) {
    ui_test_field_case_(0);
}

