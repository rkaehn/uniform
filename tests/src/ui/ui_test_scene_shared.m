#include "ui_tests.h"
#include "ui_core.h"
#include "ui_render.h"
#include "gpu.h"
#include <stdio.h>

#if cr_ios
#import <Foundation/Foundation.h>
#import <CoreGraphics/CoreGraphics.h>
#endif

float4 ui_test_palette_(uint32_t i) {
    static const uint32_t hex[] = {
        0xff3b30, 0xff9500, 0xffcc00, 0x34c759, 0x00c7be, 0x30b0c7,
        0x32ade6, 0x007aff, 0x5856d6, 0xaf52de, 0xff2d55, 0xa2845e,
    };
    return ui_hex(hex[i % cr_count(hex)]).value;
}

void ui_test_group_check_(const char* group, bool ok, const char* label) {
    printf("%s %s: %s\n", group, label, ok ? "ok" : "FAIL");
    assert(ok);
}

void ui_test_scene_state_free_(struct ui_test_scene_state* state) {
    cr_free(state->mem, state, cr_loc);
}

#if cr_ios

void ui_test_ref_set_fill_(CGContextRef ctx, struct ui_color color) {
    assert(color.color_space == ui_color_space_srgb);
    CGColorSpaceRef space = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    CGFloat components[4] = { color.value.x, color.value.y, color.value.z, color.value.w };
    CGColorRef cg_color = CGColorCreate(space, components);

    CGContextSetFillColorWithColor(ctx, cg_color);
    CGContextSetStrokeColorWithColor(ctx, cg_color);

    CGColorRelease(cg_color);
    CGColorSpaceRelease(space);
}

CGContextRef ui_test_ref_begin_(uint32_t width, uint32_t height, struct ui_color background) {
    CGColorSpaceRef space = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    CGContextRef ctx = CGBitmapContextCreate(NULL, width, height, 32, width * 16,
                                             space, kCGImageAlphaPremultipliedLast | kCGBitmapFloatComponents | kCGBitmapByteOrder32Host);
    CGColorSpaceRelease(space);
    assert(ctx);

    CGContextTranslateCTM(ctx, 0.0, (CGFloat)height);
    CGContextScaleCTM(ctx, 1.0, -1.0);

    if (background.value.w > 0.0f) {
        ui_test_ref_set_fill_(ctx, background);
        CGContextFillRect(ctx, CGRectMake(0.0, 0.0, (CGFloat)width, (CGFloat)height));
    }

    return ctx;
}

void ui_test_ref_end_(CGContextRef ctx, uint32_t width, uint32_t height, float* out_rgba, uint32_t bytes_per_row) {
    const float* data = CGBitmapContextGetData(ctx);

    for (uint32_t y = 0; y < height; y++) {
        const float* row = data + (uint64_t)y * width * 4;
        float* out = (float*)((uint8_t*)out_rgba + (uint64_t)y * bytes_per_row);

        memcpy(out, row, (uint64_t)width * 4 * sizeof(float));
    }

    CGContextRelease(ctx);
}
#endif

struct ui_test_scene_render ui_test_scene_render_(struct ui_test_scene_state* state, const struct ui_test_scene* scene, struct gpu_texture* target, struct gpu_profiler* profiler, void* out) {
    struct ui* ui = ui_ctx();
    struct ui_test_scene_render render = { 0 };
    double saved_to_px_from_pt = ui_get_to_px_from_pt();
    ui_set_to_px_from_pt(scene->to_px_from_pt);

    double2 size = make_double2((double)scene->width, (double)scene->height) / scene->to_px_from_pt;
    double build_begin_s = cr_get_current_time();
    struct ui_node root = scene->build(state, ui_node_null, size, scene);
    double layout_begin_s = cr_get_current_time();
    ui_layout(root);
    render.layout_s = cr_get_current_time() - layout_begin_s;
    render.build_s = layout_begin_s - build_begin_s;

    cr_autoreleasepool {
        double render_begin_s = cr_get_current_time();
        struct gpu_command_buffer* command_buffer = gpu_command_buffer_create(ui->device, "test scene");
        render.profiled = gpu_command_buffer_profile(command_buffer, profiler);
        struct ui_render_desc desc = {
            .output_color_space = scene->output_color_space,
            .compositing_format = ui_pixel_format_rgba16f,
            .premultiplied = true,
        };
        ui_render(root, command_buffer, target, desc);
        if (out != NULL) {
            gpu_command_buffer_read_texture(command_buffer, target, out, scene->width * 4 * (uint32_t)sizeof(__fp16));
        }
        render.serial = gpu_command_buffer_commit(command_buffer);
        render.render_s = cr_get_current_time() - render_begin_s;
    }

    ui_release(root);
    ui_set_to_px_from_pt(saved_to_px_from_pt);
    return render;
}

__fp16* ui_test_scene_render_read_(struct ui_test_scenes* scenes, const struct ui_test_scene* scene) {
    struct ui* ui = ui_ctx();
    struct cr_mem* mem = cr_mem_libc;
    uint32_t bytes_per_row = scene->width * 4 * (uint32_t)sizeof(__fp16);
    __fp16* out = cr_malloc(mem, (uint64_t)bytes_per_row * scene->height, 16, cr_loc);
    struct gpu_texture* target = gpu_texture_create(ui->device, &(struct gpu_texture_desc) {
        .label = "test scene target",
        .format = gpu_format_rgba16f,
        .size = { scene->width, scene->height },
        .usage = gpu_texture_usage_render_target | gpu_texture_usage_sample | gpu_texture_usage_read,
    });

    struct ui_test_scene_state* state = ui_test_scene_create_(scenes, mem, scene);
    ui_test_await_(ui_test_scene_render_(state, scene, target, NULL, out).serial);
    state->destroy(state);
    gpu_texture_destroy(ui->device, target);
    return out;
}

void* ui_test_render_read_(struct ui_node root, const struct ui_render_desc* desc, enum gpu_format format, uint2 size) {
    struct ui* ui = ui_ctx();
    assert(format == gpu_format_bgra8 || format == gpu_format_rgba16f);
    uint32_t bytes_per_row = size.x * (format == gpu_format_rgba16f ? 8u : 4u);
    uint8_t* out = cr_malloc(cr_mem_libc, (uint64_t)bytes_per_row * size.y, 16, cr_loc);

    ui_layout(root);
    struct gpu_texture* target = gpu_texture_create(ui->device, &(struct gpu_texture_desc) {
        .label = "test render target",
        .format = format,
        .size = size,
        .usage = gpu_texture_usage_render_target | gpu_texture_usage_sample | gpu_texture_usage_read,
    });

    cr_autoreleasepool {
        struct gpu_command_buffer* command_buffer = gpu_command_buffer_create(ui->device, "test render");
        ui_render(root, command_buffer, target, *desc);
        gpu_command_buffer_read_texture(command_buffer, target, out, bytes_per_row);
        ui_test_await_(gpu_command_buffer_commit(command_buffer));
    }
    gpu_texture_destroy(ui->device, target);
    ui_release(root);
    return out;
}
