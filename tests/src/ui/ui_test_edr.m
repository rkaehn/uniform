#include "ui_tests.h"
#include "ui_core.h"
#include "ui_color.h"
#include "gpu.h"
#include <stdio.h>


struct ui_test_edr_result {
    const char* label;
    bool pass;
    uint32_t width;
    uint32_t height;
    double max_err;
    uint32_t max_x;
    uint32_t max_y;
};

#define ui_test_edr_size 512
#define ui_test_edr_margin 2

struct ui_test_edr_desc {
    const char* label;
    enum ui_color_space output;
    struct ui_color rect_color;
    bool gamut_scene;
};

static const struct ui_test_edr_desc ui_test_edr_cases_[] = {
    {
        .label = "passthrough linear",
        .output = ui_color_space_srgb_linear,
        .rect_color = ui_rgba_cs(2.0f, 0.0f, 0.0f, 1.0f, ui_color_space_srgb_linear),
    },
    {
        .label = "passthrough encoded",
        .output = ui_color_space_srgb,
        .rect_color = ui_rgba(2.0f, 0.0f, 0.0f, 1.0f),
    },
    {
        .label = "gamut preserved",
        .output = ui_color_space_display_p3,
        .rect_color = ui_rgba(1.5f, 1.5f, 1.5f, 1.0f),
        .gamut_scene = true,
    },
};

#define ui_test_edr_gamut_color ui_rgba_cs(1.0f, 0.0f, 0.0f, 1.0f, ui_color_space_display_p3)

static const struct cr_rect_int32 ui_test_edr_rect_ = { .origin = { 224, 224 }, .size = { 64, 64 } };
static const struct cr_rect_int32 ui_test_edr_gamut_rect_ = { .origin = { 64, 320 }, .size = { 128, 128 } };

static float3 ui_test_edr_expected_linear(const struct ui_test_edr_desc* t, struct ui_color color) {
    float3 v = color.value.xyz;

    if (color.color_space != ui_color_space_srgb) {
        struct ui_convert to_compositing = ui_convert_make(color.color_space, ui_color_space_srgb);
        v = ui_convert_apply_float3(&to_compositing, v);
    }

    struct ui_convert convert = ui_convert_make_always(ui_color_space_srgb, t->output);
    if (convert.decode) {
        v = to_linear_srgb_from_srgb_signed_float3(v);
    }
    v = mul_float3x3_float3(convert.matrix, v);

    return v;
}

static bool ui_test_edr_rect_contains(struct cr_rect_int32 rect, uint32_t x, uint32_t y, int32_t inset) {
    return (int32_t)x >= rect.origin.x + inset && (int32_t)x < rect.origin.x + rect.size.x - inset &&
           (int32_t)y >= rect.origin.y + inset && (int32_t)y < rect.origin.y + rect.size.y - inset;
}

struct ui_test_edr_read {
    __fp16* raw;
};

static struct ui_test_edr_read ui_test_edr_reads_[cr_count(ui_test_edr_cases_)];

static void ui_test_edr_check(void* args) {
    const struct ui_test_edr_read* read = args;
    const struct ui_test_edr_desc* t = &ui_test_edr_cases_[read - ui_test_edr_reads_];
    const __fp16* raw = read->raw;

    struct ui_test_edr_result result = {
        .label = t->label,
        .width = ui_test_edr_size,
        .height = ui_test_edr_size,
    };

    float3 expected_rect = ui_test_edr_expected_linear(t, t->rect_color);
    float3 expected_bg = ui_test_edr_expected_linear(t, ui_black);
    float3 expected_gamut = ui_test_edr_expected_linear(t, ui_test_edr_gamut_color);
    bool output_is_linear = t->output == ui_color_space_srgb_linear;

    if (output_is_linear == false) {
        expected_rect = to_srgb_from_linear_srgb_signed_float3(expected_rect);
        expected_bg = to_srgb_from_linear_srgb_signed_float3(expected_bg);
        expected_gamut = to_srgb_from_linear_srgb_signed_float3(expected_gamut);
    }

    for (uint32_t y = 0; y < ui_test_edr_size; y++) {
        for (uint32_t x = 0; x < ui_test_edr_size; x++) {
            const __fp16* px = raw + ((uint64_t)y * ui_test_edr_size + x) * 4;
            float3 v = make_float3((float)px[0], (float)px[1], (float)px[2]);

            float3 expected;
            if (ui_test_edr_rect_contains(ui_test_edr_rect_, x, y, ui_test_edr_margin)) {
                expected = expected_rect;
            } else if (t->gamut_scene && ui_test_edr_rect_contains(ui_test_edr_gamut_rect_, x, y, ui_test_edr_margin)) {
                expected = expected_gamut;
            } else if (ui_test_edr_rect_contains(ui_test_edr_rect_, x, y, -ui_test_edr_margin) ||
                       (t->gamut_scene && ui_test_edr_rect_contains(ui_test_edr_gamut_rect_, x, y, -ui_test_edr_margin))) {
                continue;
            } else {
                expected = expected_bg;
            }

            float3 diff = v - expected;
            double err = max_double(fabs((double)diff.x), max_double(fabs((double)diff.y), fabs((double)diff.z)));
            if (err > result.max_err) {
                result.max_err = err;
                result.max_x = x;
                result.max_y = y;
            }
        }
    }

    cr_free(cr_mem_libc, read->raw, cr_loc);

    result.pass = result.max_err <= 0.004;

    printf("edr %s %ux%u px: max err %.5f at (%u,%u), %s\n",
           result.label, result.width, result.height, result.max_err, result.max_x, result.max_y,
           result.pass ? "ok" : "FAIL");
    assert(result.pass);
}

static void ui_test_edr_case_(uint32_t index) {
    const struct ui_test_edr_desc* t = &ui_test_edr_cases_[index];

    struct ui_node root = ui_frame(&(struct ui_frame) {
        .size = ui_size_fix((float)ui_test_edr_size, (float)ui_test_edr_size),
        .fill = ui_fill_solid(ui_black),
        .clip = true,
    });

    ui_frame(&(struct ui_frame) {
        .parent = root,
        .pos = ui_pos_xy((float)ui_test_edr_rect_.origin.x, (float)ui_test_edr_rect_.origin.y),
        .size = ui_size_fix((float)ui_test_edr_rect_.size.x, (float)ui_test_edr_rect_.size.y),
        .fill = ui_fill_solid(t->rect_color),
    });

    if (t->gamut_scene) {
        ui_frame(&(struct ui_frame) {
            .parent = root,
            .pos = ui_pos_xy((float)ui_test_edr_gamut_rect_.origin.x, (float)ui_test_edr_gamut_rect_.origin.y),
            .size = ui_size_fix((float)ui_test_edr_gamut_rect_.size.x, (float)ui_test_edr_gamut_rect_.size.y),
            .fill = ui_fill_solid(ui_test_edr_gamut_color),
        });
    }

    struct ui_render_desc desc = {
        .output_color_space = t->output,
        .compositing_format = ui_pixel_format_rgba16f,
    };
    double saved_to_px = ui_get_to_px_from_pt();
    ui_set_to_px_from_pt(1.0f);
    ui_test_edr_reads_[index].raw = ui_test_render_read_(root, &desc, gpu_format_rgba16f, make_uint2(ui_test_edr_size, ui_test_edr_size));
    ui_set_to_px_from_pt(saved_to_px);

    ui_test_then_(ui_test_edr_check, &ui_test_edr_reads_[index]);
}

void ui_test_edr_(void) {
    for (uint32_t i = 0; i < cr_count(ui_test_edr_cases_); i++) {
        ui_test_edr_case_(i);
    }
}

