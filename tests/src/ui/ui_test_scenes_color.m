#include "ui_tests.h"
#include "ui_core.h"
#include "ui_color.h"

#if cr_ios
#import <Foundation/Foundation.h>
#import <CoreGraphics/CoreGraphics.h>
#endif

#if cr_ios
static CGColorSpaceRef ui_test_color_cg_space(enum ui_color_space color_space) {
    switch (color_space) {
    case ui_color_space_srgb: return CGColorSpaceCreateWithName(kCGColorSpaceExtendedSRGB);
    case ui_color_space_srgb_linear: return CGColorSpaceCreateWithName(kCGColorSpaceExtendedLinearSRGB);
    case ui_color_space_display_p3: return CGColorSpaceCreateWithName(kCGColorSpaceExtendedDisplayP3);
    case ui_color_space_display_p3_linear: return CGColorSpaceCreateWithName(kCGColorSpaceExtendedLinearDisplayP3);
    case ui_color_space_acescg_linear: return CGColorSpaceCreateWithName(kCGColorSpaceACESCGLinear);
    }
}

static CGContextRef ui_test_color_ref_begin(enum ui_color_space compositing, uint32_t width, uint32_t height) {
    CGColorSpaceRef space = ui_test_color_cg_space(compositing);
    CGContextRef ctx = CGBitmapContextCreate(NULL, width, height, 32, width * 16,
                                             space, kCGImageAlphaPremultipliedLast | kCGBitmapFloatComponents | kCGBitmapByteOrder32Host);
    CGColorSpaceRelease(space);
    assert(ctx);

    CGContextTranslateCTM(ctx, 0.0, (CGFloat)height);
    CGContextScaleCTM(ctx, 1.0, -1.0);

    return ctx;
}

static void ui_test_color_ref_rect(CGContextRef ctx, struct ui_color color, float x, float y, float w, float h) {
    CGColorSpaceRef space = ui_test_color_cg_space(color.color_space);
    CGFloat components[4] = { color.value.x, color.value.y, color.value.z, color.value.w };
    CGColorRef cg_color = CGColorCreate(space, components);

    CGContextSetFillColorWithColor(ctx, cg_color);
    CGContextFillRect(ctx, CGRectMake(x, y, w, h));

    CGColorRelease(cg_color);
    CGColorSpaceRelease(space);
}

static void ui_test_color_ref_end(CGContextRef ctx, enum ui_color_space compositing, enum ui_color_space output, uint32_t width, uint32_t height, float* out_rgba, uint32_t bytes_per_row) {
    const float* data = CGBitmapContextGetData(ctx);
    struct ui_convert convert = ui_convert_make(compositing, output);

    for (uint32_t y = 0; y < height; y++) {
        const float* row = data + (uint64_t)y * width * 4;
        float* out = (float*)((uint8_t*)out_rgba + (uint64_t)y * bytes_per_row);

        for (uint32_t x = 0; x < width; x++) {
            float a = row[x * 4 + 3];
            float3 c = make_float3(row[x * 4 + 0], row[x * 4 + 1], row[x * 4 + 2]);
            if (convert.needed) {
                c = a > 0.0f ? c / a : make_float3(0.0f, 0.0f, 0.0f);
                c = ui_convert_apply_float3(&convert, c);
                c *= a;
            }
            out[x * 4 + 0] = c.x;
            out[x * 4 + 1] = c.y;
            out[x * 4 + 2] = c.z;
            out[x * 4 + 3] = a;
        }
    }

    CGContextRelease(ctx);
}

#endif
static const float4 ui_test_color_grid_values_[4] = {
    { 1.0f, 0.0f, 0.0f, 1.0f },
    { 0.0f, 1.0f, 0.0f, 1.0f },
    { 0.25f, 0.5f, 0.75f, 1.0f },
    { 0.5f, 0.5f, 0.5f, 1.0f },
};

static const enum ui_color_space ui_test_color_grid_spaces_[5] = {
    ui_color_space_srgb,
    ui_color_space_srgb_linear,
    ui_color_space_display_p3,
    ui_color_space_display_p3_linear,
    ui_color_space_acescg_linear,
};

static struct ui_node ui_test_scene_color_grid_build(struct ui_test_scene_state* state, struct ui_node parent, double2 size, const struct ui_test_scene* scene) {
    struct ui_node root = ui_frame(&(struct ui_frame) {
        .parent = parent,
        .size = ui_size_fix(size.x, size.y),
        .fill = ui_fill_solid(ui_hex(0x202020)),
        .clip = true,
    });

    for (uint32_t col = 0; col < 5; col++) {
        for (uint32_t row = 0; row < 4; row++) {
            ui_frame(&(struct ui_frame) {
                .parent = root,
                .pos = ui_pos_xy(16.0 + (double)col * 96.0, 16.0 + (double)row * 96.0),
                .size = ui_size_fix(88.0, 88.0),
                .fill = ui_fill_solid(ui_color(ui_test_color_grid_values_[row], ui_test_color_grid_spaces_[col])),
            });
        }
    }

    ui_frame(&(struct ui_frame) {
        .parent = root,
        .pos = ui_pos_xy(40.0, 40.0),
        .size = ui_size_fix(432.0, 320.0),
        .fill = ui_fill_solid(ui_gray_a(1.0f, 0.5f)),
    });

    ui_frame(&(struct ui_frame) {
        .parent = root,
        .pos = ui_pos_xy(16.0, 424.0),
        .size = ui_size_fix(480.0, 72.0),
        .fill = ui_fill_solid(ui_rgba_cs(1.0f, 0.0f, 0.0f, 0.5f, ui_color_space_display_p3)),
    });

    return root;
}

#if cr_ios
static void ui_test_scene_color_grid_pixels_impl(enum ui_color_space compositing, enum ui_color_space output,
                                                 uint32_t width, uint32_t height, float s, float* out_rgba, uint32_t bytes_per_row) {
    CGContextRef ctx = ui_test_color_ref_begin(compositing, width, height);

    ui_test_color_ref_rect(ctx, ui_hex(0x202020), 0.0f, 0.0f, (float)width, (float)height);

    for (uint32_t col = 0; col < 5; col++) {
        for (uint32_t row = 0; row < 4; row++) {
            struct ui_color color = ui_color(ui_test_color_grid_values_[row], ui_test_color_grid_spaces_[col]);
            ui_test_color_ref_rect(ctx, color, (16.0f + (float)col * 96.0f) * s, (16.0f + (float)row * 96.0f) * s, 88.0f * s, 88.0f * s);
        }
    }

    ui_test_color_ref_rect(ctx, ui_gray_a(1.0f, 0.5f), 40.0f * s, 40.0f * s, 432.0f * s, 320.0f * s);
    ui_test_color_ref_rect(ctx, ui_rgba_cs(1.0f, 0.0f, 0.0f, 0.5f, ui_color_space_display_p3), 16.0f * s, 424.0f * s, 480.0f * s, 72.0f * s);

    ui_test_color_ref_end(ctx, compositing, output, width, height, out_rgba, bytes_per_row);
}

static void ui_test_scene_color_grid_p3_pixels(struct ui_test_scene_state* state, uint32_t width, uint32_t height, float to_px_from_pt, float* out_rgba, uint32_t bytes_per_row) {
    ui_test_scene_color_grid_pixels_impl(ui_color_space_srgb, ui_color_space_display_p3, width, height, to_px_from_pt, out_rgba, bytes_per_row);
}
#endif

static struct ui_node ui_test_scene_color_blend_build(struct ui_test_scene_state* state, struct ui_node parent, double2 size, const struct ui_test_scene* scene) {
    struct ui_node root = ui_frame(&(struct ui_frame) {
        .parent = parent,
        .size = ui_size_fix(size.x, size.y),
        .fill = ui_fill_solid(ui_hex(0x404040)),
        .clip = true,
    });

    struct ui_color row_0[3] = { ui_red, ui_green, ui_blue };
    struct ui_color row_1[3] = { ui_cyan, ui_magenta, ui_yellow };

    for (uint32_t i = 0; i < 3; i++) {
        ui_frame(&(struct ui_frame) {
            .parent = root,
            .pos = ui_pos_xy(32.0 + (double)i * 160.0, 64.0),
            .size = ui_size_fix(128.0, 128.0),
            .fill = ui_fill_solid(row_0[i]),
        });
        ui_frame(&(struct ui_frame) {
            .parent = root,
            .pos = ui_pos_xy(32.0 + (double)i * 160.0, 256.0),
            .size = ui_size_fix(128.0, 128.0),
            .fill = ui_fill_solid(row_1[i]),
        });
    }

    ui_frame(&(struct ui_frame) {
        .parent = root,
        .pos = ui_pos_xy(16.0, 96.0),
        .size = ui_size_fix(480.0, 64.0),
        .fill = ui_fill_solid(ui_gray_a(1.0f, 0.5f)),
    });

    ui_frame(&(struct ui_frame) {
        .parent = root,
        .pos = ui_pos_xy(16.0, 288.0),
        .size = ui_size_fix(480.0, 64.0),
        .fill = ui_fill_solid(ui_gray_a(0.0f, 0.5f)),
    });

    ui_frame(&(struct ui_frame) {
        .parent = root,
        .pos = ui_pos_xy(224.0, 32.0),
        .size = ui_size_fix(64.0, 448.0),
        .fill = ui_fill_solid(ui_rgba(1.0f, 0.0f, 0.0f, 0.5f)),
    });

    return root;
}

#if cr_ios
static void ui_test_scene_color_blend_pixels_impl(enum ui_color_space compositing, enum ui_color_space output,
                                                  uint32_t width, uint32_t height, float s, float* out_rgba, uint32_t bytes_per_row) {
    CGContextRef ctx = ui_test_color_ref_begin(compositing, width, height);

    ui_test_color_ref_rect(ctx, ui_hex(0x404040), 0.0f, 0.0f, (float)width, (float)height);

    struct ui_color row_0[3] = { ui_red, ui_green, ui_blue };
    struct ui_color row_1[3] = { ui_cyan, ui_magenta, ui_yellow };

    for (uint32_t i = 0; i < 3; i++) {
        ui_test_color_ref_rect(ctx, row_0[i], (32.0f + (float)i * 160.0f) * s, 64.0f * s, 128.0f * s, 128.0f * s);
        ui_test_color_ref_rect(ctx, row_1[i], (32.0f + (float)i * 160.0f) * s, 256.0f * s, 128.0f * s, 128.0f * s);
    }

    ui_test_color_ref_rect(ctx, ui_gray_a(1.0f, 0.5f), 16.0f * s, 96.0f * s, 480.0f * s, 64.0f * s);
    ui_test_color_ref_rect(ctx, ui_gray_a(0.0f, 0.5f), 16.0f * s, 288.0f * s, 480.0f * s, 64.0f * s);
    ui_test_color_ref_rect(ctx, ui_rgba(1.0f, 0.0f, 0.0f, 0.5f), 224.0f * s, 32.0f * s, 64.0f * s, 448.0f * s);

    ui_test_color_ref_end(ctx, compositing, output, width, height, out_rgba, bytes_per_row);
}

static void ui_test_scene_color_blend_gamma_p3_pixels(struct ui_test_scene_state* state, uint32_t width, uint32_t height, float to_px_from_pt, float* out_rgba, uint32_t bytes_per_row) {
    ui_test_scene_color_blend_pixels_impl(ui_color_space_srgb, ui_color_space_display_p3, width, height, to_px_from_pt, out_rgba, bytes_per_row);
}
#endif

#define ui_test_scene_ramps_size 64

static void ui_test_scene_ramps_pixels(uint8_t* raw, uint32_t size) {
    for (uint32_t y = 0; y < size; y++) {
        for (uint32_t x = 0; x < size; x++) {
            uint8_t v = (uint8_t)(x * 4);
            uint32_t band = y / (size / 4);
            uint8_t* px = raw + (y * size + x) * 4;
            px[0] = band == 2 || band == 3 ? v : 0;
            px[1] = band == 1 || band == 3 ? v : 0;
            px[2] = band == 0 || band == 3 ? v : 0;
            px[3] = 255;
        }
    }
}

struct ui_test_color_state {
    struct ui_test_scene_state base;
    struct ui_graphic* graphic;
};

static struct ui_graphic* ui_test_scene_ramps_graphic(void) {
    uint8_t raw[ui_test_scene_ramps_size * ui_test_scene_ramps_size * 4];
    ui_test_scene_ramps_pixels(raw, ui_test_scene_ramps_size);
    return ui_graphic_create(ui_test_scene_ramps_size, ui_test_scene_ramps_size, ui_pixel_format_bgra8, raw, ui_test_scene_ramps_size * 4, true);
}

static struct ui_node ui_test_scene_color_graphic_build(struct ui_test_scene_state* state, struct ui_node parent, double2 size, const struct ui_test_scene* scene) {
    struct ui_graphic* graphic = ((struct ui_test_color_state*)state)->graphic;

    struct ui_node root = ui_frame(&(struct ui_frame) {
        .parent = parent,
        .size = ui_size_fix(size.x, size.y),
        .fill = ui_fill_solid(ui_hex(0x202020)),
        .clip = true,
    });

    ui_frame(&(struct ui_frame) {
        .parent = root,
        .pos = ui_pos_xy(32.0, 32.0),
        .size = ui_size_fix(64.0, 64.0),
        .fill.layers[0] = ui_layer_graphic_ex(graphic, ui_address_mode_fill, ui_filter_linear, make_double2(1.0, 1.0), ui_white, ui_color_space_display_p3, true),
    });

    ui_frame(&(struct ui_frame) {
        .parent = root,
        .pos = ui_pos_xy(128.0, 32.0),
        .size = ui_size_fix(64.0, 64.0),
        .fill = ui_fill_graphic(graphic, ui_address_mode_fill, ui_white),
    });

    return root;
}

#if cr_ios
static void ui_test_scene_color_graphic_ref_image(CGContextRef ctx, const uint8_t* raw, enum ui_color_space color_space, float x, float y, float w, float h) {
    CGColorSpaceRef space = ui_test_color_cg_space(color_space);
    CGDataProviderRef provider = CGDataProviderCreateWithData(NULL, raw, (size_t)ui_test_scene_ramps_size * ui_test_scene_ramps_size * 4, NULL);
    CGImageRef image = CGImageCreate(ui_test_scene_ramps_size, ui_test_scene_ramps_size, 8, 32, ui_test_scene_ramps_size * 4,
                                     space, kCGBitmapByteOrder32Little | kCGImageAlphaPremultipliedFirst, provider, NULL, false, kCGRenderingIntentDefault);

    CGContextSaveGState(ctx);
    CGContextSetInterpolationQuality(ctx, kCGInterpolationNone);
    CGContextTranslateCTM(ctx, x, y + h);
    CGContextScaleCTM(ctx, 1.0, -1.0);
    CGContextDrawImage(ctx, CGRectMake(0.0f, 0.0f, w, h), image);
    CGContextRestoreGState(ctx);

    CGImageRelease(image);
    CGDataProviderRelease(provider);
    CGColorSpaceRelease(space);
}

static void ui_test_scene_color_graphic_pixels(struct ui_test_scene_state* state, uint32_t width, uint32_t height, float to_px_from_pt, float* out_rgba, uint32_t bytes_per_row) {
    float s = to_px_from_pt;

    uint8_t raw[ui_test_scene_ramps_size * ui_test_scene_ramps_size * 4];
    ui_test_scene_ramps_pixels(raw, ui_test_scene_ramps_size);

    CGContextRef ctx = ui_test_color_ref_begin(ui_color_space_srgb, width, height);

    ui_test_color_ref_rect(ctx, ui_hex(0x202020), 0.0f, 0.0f, (float)width, (float)height);

    ui_test_scene_color_graphic_ref_image(ctx, raw, ui_color_space_display_p3, 32.0f * s, 32.0f * s, 64.0f * s, 64.0f * s);
    ui_test_scene_color_graphic_ref_image(ctx, raw, ui_color_space_srgb, 128.0f * s, 32.0f * s, 64.0f * s, 64.0f * s);

    ui_test_color_ref_end(ctx, ui_color_space_srgb, ui_color_space_display_p3, width, height, out_rgba, bytes_per_row);
}

#endif

#define ui_test_scene_sample_size 64

static void ui_test_scene_sample_pixels(uint8_t* raw, uint32_t size) {
    for (uint32_t y = 0; y < size; y++) {
        for (uint32_t x = 0; x < size; x++) {
            bool checker = ((x / 8) + (y / 8)) % 2 == 0;
            uint8_t* px = raw + (y * size + x) * 4;
            px[0] = (uint8_t)(x * 4);
            px[1] = checker ? 200 : 40;
            px[2] = (uint8_t)(y * 4);
            px[3] = 255;
        }
    }
}

static struct ui_graphic* ui_test_scene_sample_graphic(void) {
    uint8_t raw[ui_test_scene_sample_size * ui_test_scene_sample_size * 4];
    ui_test_scene_sample_pixels(raw, ui_test_scene_sample_size);
    return ui_graphic_create(ui_test_scene_sample_size, ui_test_scene_sample_size, ui_pixel_format_bgra8, raw, ui_test_scene_sample_size * 4, true);
}

static struct ui_node ui_test_scene_color_sample_build(struct ui_test_scene_state* state, struct ui_node parent, double2 size, const struct ui_test_scene* scene) {
    struct ui_graphic* graphic = ((struct ui_test_color_state*)state)->graphic;

    struct ui_node root = ui_frame(&(struct ui_frame) {
        .parent = parent,
        .size = ui_size_fix(size.x, size.y),
        .fill = ui_fill_solid(ui_hex(0xf2f2f7)),
        .clip = true,
    });

    ui_frame(&(struct ui_frame) {
        .parent = root,
        .pos = ui_pos_xy(32.0, 32.0),
        .size = ui_size_fix(64.0, 64.0),
        .fill = ui_fill_graphic(graphic, ui_address_mode_fill, ui_white),
    });

    ui_frame(&(struct ui_frame) {
        .parent = root,
        .pos = ui_pos_xy(128.0, 32.0),
        .size = ui_size_fix(256.0, 256.0),
        .fill = ui_fill_graphic(graphic, ui_address_mode_fill, ui_white),
    });

    ui_frame(&(struct ui_frame) {
        .parent = root,
        .pos = ui_pos_xy(32.0, 320.0),
        .size = ui_size_fix(352.0, 160.0),
        .fill = ui_fill_graphic(graphic, ui_address_mode_repeat, ui_white),
    });

    return root;
}

#if cr_ios
static float3 ui_test_scene_sample_bilinear(const uint8_t* bgra, float cx, float cy, bool wrap) {
    float sx = cx - 0.5f;
    float sy = cy - 0.5f;

    int32_t x0 = (int32_t)floorf(sx);
    int32_t y0 = (int32_t)floorf(sy);
    float fx = sx - (float)x0;
    float fy = sy - (float)y0;

    int32_t size = (int32_t)ui_test_scene_sample_size;
    int32_t xs[2] = { x0, x0 + 1 };
    int32_t ys[2] = { y0, y0 + 1 };

    for (int i = 0; i < 2; i++) {
        if (wrap) {
            xs[i] = ((xs[i] % size) + size) % size;
            ys[i] = ((ys[i] % size) + size) % size;
        } else {
            xs[i] = min_int32(max_int32(xs[i], 0), size - 1);
            ys[i] = min_int32(max_int32(ys[i], 0), size - 1);
        }
    }

    float3 result = make_float3(0.0f, 0.0f, 0.0f);
    float weights[2][2] = { { (1.0f - fx) * (1.0f - fy), fx * (1.0f - fy) }, { (1.0f - fx) * fy, fx * fy } };

    for (int j = 0; j < 2; j++) {
        for (int i = 0; i < 2; i++) {
            const uint8_t* px = bgra + (ys[j] * size + xs[i]) * 4;
            result += make_float3((float)px[2], (float)px[1], (float)px[0]) * weights[j][i];
        }
    }

    return result / 255.0f;
}

static void ui_test_scene_sample_ref_block(float* out_rgba, uint32_t bytes_per_row, uint32_t width, uint32_t height,
                                           const uint8_t* bgra, float bx, float by, float bw, float bh, bool wrap) {
    float size = (float)ui_test_scene_sample_size;

    int32_t x_start = (int32_t)bx;
    int32_t y_start = (int32_t)by;
    int32_t x_end = min_int32((int32_t)(bx + bw), (int32_t)width);
    int32_t y_end = min_int32((int32_t)(by + bh), (int32_t)height);

    for (int32_t y = y_start; y < y_end; y++) {
        float* row = (float*)((uint8_t*)out_rgba + (uint32_t)y * bytes_per_row);

        for (int32_t x = x_start; x < x_end; x++) {
            float u = ((float)x + 0.5f - bx) / bw;
            float v = ((float)y + 0.5f - by) / bh;

            float cx;
            float cy;

            if (wrap) {
                cx = (u * bw / size - floorf(u * bw / size)) * size;
                cy = (v * bh / size - floorf(v * bh / size)) * size;
            } else {
                cx = clamp_float(u, 0.0f, 1.0f) * size;
                cy = clamp_float(v, 0.0f, 1.0f) * size;
                cx = clamp_float(cx, 0.5f, size - 0.5f);
                cy = clamp_float(cy, 0.5f, size - 0.5f);
            }

            float3 color = ui_test_scene_sample_bilinear(bgra, cx, cy, wrap);

            row[x * 4 + 0] = color.x;
            row[x * 4 + 1] = color.y;
            row[x * 4 + 2] = color.z;
            row[x * 4 + 3] = 1.0f;
        }
    }
}

static void ui_test_scene_color_sample_pixels(struct ui_test_scene_state* state, uint32_t width, uint32_t height, float to_px_from_pt, float* out_rgba, uint32_t bytes_per_row) {
    float s = to_px_from_pt;
    float4 background = ui_hex(0xf2f2f7).value;

    for (uint32_t y = 0; y < height; y++) {
        float* row = (float*)((uint8_t*)out_rgba + y * bytes_per_row);
        for (uint32_t x = 0; x < width; x++) {
            row[x * 4 + 0] = background.x;
            row[x * 4 + 1] = background.y;
            row[x * 4 + 2] = background.z;
            row[x * 4 + 3] = 1.0f;
        }
    }

    uint8_t raw[ui_test_scene_sample_size * ui_test_scene_sample_size * 4];
    ui_test_scene_sample_pixels(raw, ui_test_scene_sample_size);

    ui_test_scene_sample_ref_block(out_rgba, bytes_per_row, width, height, raw, 32.0f * s, 32.0f * s, 64.0f * s, 64.0f * s, false);
    ui_test_scene_sample_ref_block(out_rgba, bytes_per_row, width, height, raw, 128.0f * s, 32.0f * s, 256.0f * s, 256.0f * s, false);
    ui_test_scene_sample_ref_block(out_rgba, bytes_per_row, width, height, raw, 32.0f * s, 320.0f * s, 352.0f * s, 160.0f * s, true);
}
#endif

static void ui_test_scene_mask_pixels(uint8_t* raw, uint32_t size) {
    for (uint32_t y = 0; y < size; y++) {
        for (uint32_t x = 0; x < size; x++) {
            bool band = (y / 8) % 2 == 0;
            raw[y * size + x] = band ? (uint8_t)(x * 4) : (uint8_t)(255 - x * 4);
        }
    }
}

static struct ui_graphic* ui_test_scene_mask_graphic(void) {
    uint8_t raw[ui_test_scene_sample_size * ui_test_scene_sample_size];
    ui_test_scene_mask_pixels(raw, ui_test_scene_sample_size);
    return ui_graphic_create(ui_test_scene_sample_size, ui_test_scene_sample_size, ui_pixel_format_r8, raw, ui_test_scene_sample_size, true);
}

static struct ui_node ui_test_scene_color_sample_r8_build(struct ui_test_scene_state* state, struct ui_node parent, double2 size, const struct ui_test_scene* scene) {
    struct ui_graphic* graphic = ((struct ui_test_color_state*)state)->graphic;

    struct ui_node root = ui_frame(&(struct ui_frame) {
        .parent = parent,
        .size = ui_size_fix(size.x, size.y),
        .fill = ui_fill_solid(ui_hex(0xf2f2f7)),
        .clip = true,
    });

    ui_frame(&(struct ui_frame) {
        .parent = root,
        .pos = ui_pos_xy(32.0, 32.0),
        .size = ui_size_fix(64.0, 64.0),
        .fill = ui_fill_graphic(graphic, ui_address_mode_fill, ui_hex(0x000000)),
    });

    ui_frame(&(struct ui_frame) {
        .parent = root,
        .pos = ui_pos_xy(128.0, 32.0),
        .size = ui_size_fix(256.0, 256.0),
        .fill = ui_fill_graphic(graphic, ui_address_mode_fill, ui_hex(0xff3b30)),
    });

    ui_frame(&(struct ui_frame) {
        .parent = root,
        .pos = ui_pos_xy(32.0, 320.0),
        .size = ui_size_fix(352.0, 160.0),
        .fill = ui_fill_graphic(graphic, ui_address_mode_repeat, ui_hex(0x007aff)),
    });

    return root;
}

#if cr_ios
static float ui_test_scene_mask_bilinear(const uint8_t* mask, float cx, float cy, bool wrap) {
    float sx = cx - 0.5f;
    float sy = cy - 0.5f;

    int32_t x0 = (int32_t)floorf(sx);
    int32_t y0 = (int32_t)floorf(sy);
    float fx = sx - (float)x0;
    float fy = sy - (float)y0;

    int32_t size = (int32_t)ui_test_scene_sample_size;
    int32_t xs[2] = { x0, x0 + 1 };
    int32_t ys[2] = { y0, y0 + 1 };

    for (int i = 0; i < 2; i++) {
        if (wrap) {
            xs[i] = ((xs[i] % size) + size) % size;
            ys[i] = ((ys[i] % size) + size) % size;
        } else {
            xs[i] = min_int32(max_int32(xs[i], 0), size - 1);
            ys[i] = min_int32(max_int32(ys[i], 0), size - 1);
        }
    }

    float v00 = (float)mask[ys[0] * size + xs[0]];
    float v10 = (float)mask[ys[0] * size + xs[1]];
    float v01 = (float)mask[ys[1] * size + xs[0]];
    float v11 = (float)mask[ys[1] * size + xs[1]];

    return (v00 * (1.0f - fx) + v10 * fx) * (1.0f - fy) + (v01 * (1.0f - fx) + v11 * fx) * fy;
}

static void ui_test_scene_sample_r8_ref_block(float* out_rgba, uint32_t bytes_per_row, uint32_t width, uint32_t height,
                                              const uint8_t* mask, float4 tint, float4 background,
                                              float bx, float by, float bw, float bh, bool wrap) {
    float size = (float)ui_test_scene_sample_size;

    int32_t x_start = (int32_t)bx;
    int32_t y_start = (int32_t)by;
    int32_t x_end = min_int32((int32_t)(bx + bw), (int32_t)width);
    int32_t y_end = min_int32((int32_t)(by + bh), (int32_t)height);

    for (int32_t y = y_start; y < y_end; y++) {
        float* row = (float*)((uint8_t*)out_rgba + (uint32_t)y * bytes_per_row);

        for (int32_t x = x_start; x < x_end; x++) {
            float u = ((float)x + 0.5f - bx) / bw;
            float v = ((float)y + 0.5f - by) / bh;

            float cx;
            float cy;

            if (wrap) {
                cx = (u * bw / size - floorf(u * bw / size)) * size;
                cy = (v * bh / size - floorf(v * bh / size)) * size;
            } else {
                cx = clamp_float(u, 0.0f, 1.0f) * size;
                cy = clamp_float(v, 0.0f, 1.0f) * size;
                cx = clamp_float(cx, 0.5f, size - 0.5f);
                cy = clamp_float(cy, 0.5f, size - 0.5f);
            }

            float coverage = ui_test_scene_mask_bilinear(mask, cx, cy, wrap) / 255.0f;

            float alpha = tint.w * coverage;
            float3 src = tint.xyz * coverage;
            float3 dst = background.xyz;
            float3 blended = src + dst * (1.0f - alpha);

            row[x * 4 + 0] = blended.x;
            row[x * 4 + 1] = blended.y;
            row[x * 4 + 2] = blended.z;
            row[x * 4 + 3] = 1.0f;
        }
    }
}

static void ui_test_scene_color_sample_r8_pixels(struct ui_test_scene_state* state, uint32_t width, uint32_t height, float to_px_from_pt, float* out_rgba, uint32_t bytes_per_row) {
    float s = to_px_from_pt;
    float4 background = ui_hex(0xf2f2f7).value;

    for (uint32_t y = 0; y < height; y++) {
        float* row = (float*)((uint8_t*)out_rgba + y * bytes_per_row);
        for (uint32_t x = 0; x < width; x++) {
            row[x * 4 + 0] = background.x;
            row[x * 4 + 1] = background.y;
            row[x * 4 + 2] = background.z;
            row[x * 4 + 3] = 1.0f;
        }
    }

    uint8_t mask[ui_test_scene_sample_size * ui_test_scene_sample_size];
    ui_test_scene_mask_pixels(mask, ui_test_scene_sample_size);

    ui_test_scene_sample_r8_ref_block(out_rgba, bytes_per_row, width, height, mask, ui_hex(0x000000).value, background,
                                      32.0f * s, 32.0f * s, 64.0f * s, 64.0f * s, false);
    ui_test_scene_sample_r8_ref_block(out_rgba, bytes_per_row, width, height, mask, ui_hex(0xff3b30).value, background,
                                      128.0f * s, 32.0f * s, 256.0f * s, 256.0f * s, false);
    ui_test_scene_sample_r8_ref_block(out_rgba, bytes_per_row, width, height, mask, ui_hex(0x007aff).value, background,
                                      32.0f * s, 320.0f * s, 352.0f * s, 160.0f * s, true);
}
#endif

static void ui_test_color_state_destroy(struct ui_test_scene_state* state) {
    struct ui_test_color_state* color = (struct ui_test_color_state*)state;
    ui_graphic_destroy(color->graphic);
    cr_free(state->mem, state, cr_loc);
}

static struct ui_test_scene_state* ui_test_color_state_create(struct ui_test_scenes* scenes, struct cr_mem* mem, struct ui_graphic* graphic) {
    struct ui_test_color_state* color = cr_malloc(mem, sizeof(*color), 16, cr_loc);
    *color = (struct ui_test_color_state) {
        .base = {
            .scenes = scenes,
            .mem = mem,
            .destroy = ui_test_color_state_destroy,
        },
        .graphic = graphic,
    };
    return &color->base;
}

static struct ui_test_scene_state* ui_test_color_graphic_create(struct ui_test_scenes* scenes, struct cr_mem* mem, const void* ctx) {
    return ui_test_color_state_create(scenes, mem, ui_test_scene_ramps_graphic());
}

static struct ui_test_scene_state* ui_test_color_sample_create(struct ui_test_scenes* scenes, struct cr_mem* mem, const void* ctx) {
    return ui_test_color_state_create(scenes, mem, ui_test_scene_sample_graphic());
}

static struct ui_test_scene_state* ui_test_color_sample_r8_create(struct ui_test_scenes* scenes, struct cr_mem* mem, const void* ctx) {
    return ui_test_color_state_create(scenes, mem, ui_test_scene_mask_graphic());
}

static const struct ui_test_scene ui_test_scenes_color_table_[] = {
    {
        .label = "color-sample", .group = "color",
        .capture = ui_test_scene_capture_pixels,
        .alarm_diff = 0.063f,
        .size = { 512.0, 512.0 }, .to_px_from_pt = 1.0f,
        .mean_threshold = 0.002f,
        .tile_threshold = 0.008f,
        .create = ui_test_color_sample_create,
        .build = ui_test_scene_color_sample_build,
        .pixels = ui_test_scene_pixels(ui_test_scene_color_sample_pixels),
    },
    {
        .label = "color-sample-r8", .group = "color",
        .capture = ui_test_scene_capture_pixels,
        .alarm_diff = 0.063f,
        .size = { 512.0, 512.0 }, .to_px_from_pt = 1.0f,
        .mean_threshold = 0.002f,
        .create = ui_test_color_sample_r8_create,
        .build = ui_test_scene_color_sample_r8_build,
        .pixels = ui_test_scene_pixels(ui_test_scene_color_sample_r8_pixels),
    },
    {
        .label = "color-grid-p3", .group = "color",
        .capture = ui_test_scene_capture_pixels,
        .alarm_diff = 0.032f,
        .size = { 512.0, 512.0 }, .to_px_from_pt = 1.0f,
        .mean_threshold = 0.002f,
        .output_color_space = ui_color_space_display_p3,
        .build = ui_test_scene_color_grid_build,
        .pixels = ui_test_scene_pixels(ui_test_scene_color_grid_p3_pixels),
    },
    {
        .label = "color-blend-gamma-p3", .group = "color",
        .capture = ui_test_scene_capture_pixels,
        .alarm_diff = 0.032f,
        .size = { 512.0, 512.0 }, .to_px_from_pt = 1.0f,
        .mean_threshold = 0.002f,
        .output_color_space = ui_color_space_display_p3,
        .build = ui_test_scene_color_blend_build,
        .pixels = ui_test_scene_pixels(ui_test_scene_color_blend_gamma_p3_pixels),
    },
    {
        .label = "color-graphic-p3", .group = "color",
        .capture = ui_test_scene_capture_pixels,
        .alarm_diff = 0.032f,
        .size = { 512.0, 512.0 }, .to_px_from_pt = 1.0f,
        .mean_threshold = 0.002f,
        .output_color_space = ui_color_space_display_p3,
        .create = ui_test_color_graphic_create,
        .build = ui_test_scene_color_graphic_build,
        .pixels = ui_test_scene_pixels(ui_test_scene_color_graphic_pixels),
    },
};

const struct ui_test_scene* ui_test_scenes_color_(struct ui_test_scenes* scenes, uint32_t* out_count) {
    *out_count = cr_count(ui_test_scenes_color_table_);
    return ui_test_scenes_color_table_;
}

