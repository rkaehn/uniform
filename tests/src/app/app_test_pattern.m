#include "app_tests_internal.h"

#if cr_macos

#include "gpu.h"
#import <Metal/Metal.h>
#include <string.h>

struct app_test_pattern {
    id<MTLRenderPipelineState> mtl_pipeline16f;
    id<MTLRenderPipelineState> mtl_pipeline8;
};

struct app_test_pattern_rect {
    float4 rect;
    float4 color;
};

static const char* app_test_pattern_shader =
    "#include <metal_stdlib>\n"
    "using namespace metal;\n"
    "struct R { float4 rect; float4 color; };\n"
    "struct V { float4 pos [[position]]; float4 color; };\n"
    "vertex V app_test_pattern_vs(uint vid [[vertex_id]], uint iid [[instance_id]], constant R* rects [[buffer(0)]], constant float2& size [[buffer(1)]]) {\n"
    "    float2 corners[6] = { float2(0, 0), float2(1, 0), float2(0, 1), float2(1, 0), float2(1, 1), float2(0, 1) };\n"
    "    float2 c = corners[vid];\n"
    "    float4 r = rects[iid].rect;\n"
    "    float2 p = (r.xy + c * r.zw) / size * 2.0 - 1.0;\n"
    "    p.y = -p.y;\n"
    "    V v;\n"
    "    v.pos = float4(p, 0, 1);\n"
    "    v.color = rects[iid].color;\n"
    "    return v;\n"
    "}\n"
    "fragment float4 app_test_pattern_fs(V v [[stage_in]]) { return v.color; }\n";

struct app_test_pattern* app_test_pattern_create_(struct gpu_device* device) {
    id<MTLDevice> mtl_device = (id<MTLDevice>)gpu_device_get_native_device(device);
    NSError* error = nil;
    id<MTLLibrary> mtl_library = [mtl_device newLibraryWithSource:[NSString stringWithUTF8String:app_test_pattern_shader] options:nil error:&error];
    assert(mtl_library != nil);
    id<MTLFunction> mtl_vs = [mtl_library newFunctionWithName:@"app_test_pattern_vs"];
    id<MTLFunction> mtl_fs = [mtl_library newFunctionWithName:@"app_test_pattern_fs"];
    MTLRenderPipelineDescriptor* mtl_desc = [[MTLRenderPipelineDescriptor alloc] init];
    mtl_desc.vertexFunction = mtl_vs;
    mtl_desc.fragmentFunction = mtl_fs;
    struct app_test_pattern* pattern = cr_malloc(cr_mem_libc, sizeof(*pattern), 0, cr_loc);
    memset(pattern, 0, sizeof(*pattern));
    mtl_desc.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA16Float;
    pattern->mtl_pipeline16f = [mtl_device newRenderPipelineStateWithDescriptor:mtl_desc error:&error];
    mtl_desc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
    pattern->mtl_pipeline8 = [mtl_device newRenderPipelineStateWithDescriptor:mtl_desc error:&error];
    [mtl_desc release];
    [mtl_vs release];
    [mtl_fs release];
    [mtl_library release];
    assert(pattern->mtl_pipeline16f != nil && pattern->mtl_pipeline8 != nil);
    return pattern;
}

void app_test_pattern_destroy_(struct app_test_pattern* pattern) {
    [pattern->mtl_pipeline16f release];
    [pattern->mtl_pipeline8 release];
    cr_free(cr_mem_libc, pattern, cr_loc);
}

void app_test_pattern_encode_(const struct app_test_pattern* pattern, struct gpu_command_buffer* command_buffer, struct gpu_texture* target, uint2 px_size, uint32_t frame, float4 fill) {
    float2 size = to_float2_from_uint2(px_size);
    float4 border = make_float4(fill.w, 0.0f, fill.w, fill.w);
    uint32_t values[app_test_barcode_row_count] = {
        [app_test_barcode_row_width] = px_size.x,
        [app_test_barcode_row_height] = px_size.y,
        [app_test_barcode_row_frame] = frame,
        [app_test_barcode_row_sync] = app_test_barcode_sync,
    };
    struct app_test_pattern_rect rects[4 + app_test_barcode_row_count * app_test_barcode_bits];
    uint32_t n = 0;
    rects[n++] = (struct app_test_pattern_rect) { make_float4(0.0f, 0.0f, size.x, 4.0f), border };
    rects[n++] = (struct app_test_pattern_rect) { make_float4(0.0f, size.y - 4.0f, size.x, 4.0f), border };
    rects[n++] = (struct app_test_pattern_rect) { make_float4(0.0f, 0.0f, 4.0f, size.y), border };
    rects[n++] = (struct app_test_pattern_rect) { make_float4(size.x - 4.0f, 0.0f, 4.0f, size.y), border };
    float cell = (float)app_test_barcode_cell;
    for (uint32_t row = 0; row < app_test_barcode_row_count; row++) {
        for (uint32_t bit = 0; bit < app_test_barcode_bits; bit++) {
            float on = (values[row] >> bit) & 1u ? fill.w : 0.0f;
            float4 rect = make_float4((float)(app_test_barcode_x + bit * app_test_barcode_cell), (float)(app_test_barcode_y + row * app_test_barcode_cell), cell, cell);
            rects[n++] = (struct app_test_pattern_rect) { rect, make_float4(on, on, on, fill.w) };
        }
    }

    id<MTLCommandBuffer> mtl_command_buffer = (id<MTLCommandBuffer>)gpu_command_buffer_get_native(command_buffer);
    id<MTLTexture> mtl_target = (id<MTLTexture>)gpu_texture_get_native(target);
    MTLRenderPassDescriptor* mtl_pass = [MTLRenderPassDescriptor renderPassDescriptor];
    mtl_pass.colorAttachments[0].texture = mtl_target;
    mtl_pass.colorAttachments[0].loadAction = MTLLoadActionClear;
    mtl_pass.colorAttachments[0].storeAction = MTLStoreActionStore;
    float4 clear = premultiply_float4(fill);
    mtl_pass.colorAttachments[0].clearColor = MTLClearColorMake(clear.x, clear.y, clear.z, clear.w);
    id<MTLRenderCommandEncoder> mtl_enc = [mtl_command_buffer renderCommandEncoderWithDescriptor:mtl_pass];
    [mtl_enc setRenderPipelineState:mtl_target.pixelFormat == MTLPixelFormatRGBA16Float ? pattern->mtl_pipeline16f : pattern->mtl_pipeline8];
    [mtl_enc setVertexBytes:rects length:n * sizeof(rects[0]) atIndex:0];
    [mtl_enc setVertexBytes:&size length:sizeof(size) atIndex:1];
    [mtl_enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6 instanceCount:n];
    [mtl_enc endEncoding];
}

#endif
