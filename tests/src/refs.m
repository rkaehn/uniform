#include "refs.h"
#include "ui_core.h"
#include "ui_test_compare_shared.h"
#include "gpu.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#if cr_ios
#import <CoreImage/CoreImage.h>
#import <Photos/Photos.h>
#import <UIKit/UIKit.h>
#import <notify.h>
#include <unistd.h>

@interface SwiftUIShapesBridge : NSObject
+ (void)dismissScene;
@end
#endif

#if cr_macos || cr_ios || cr_wasm

static const char* tests_refs_ref_;

float tests_refs_use_(const char* ref) {
    float to_px_from_pt = 0.0f;
    int end = 0;
    sscanf(ref, "ref@%gx%n", &to_px_from_pt, &end);
    cr_check(end > 0 && to_px_from_pt > 0.0f, "the name of a reference folder starts with ref@<scale>x: %s", ref);
    tests_refs_ref_ = ref;
    return to_px_from_pt;
}

#endif

#if cr_macos || cr_ios

static struct cr_str* tests_refs_root_;

static struct cr_str* tests_refs_root(void) {
    if (tests_refs_root_ == NULL) {
        tests_refs_root_ = cr_util_get_documents_dir(cr_mem_libc);
        assert(tests_refs_root_ != NULL);
        cr_str_append_path_component_cstr(tests_refs_root_, "refs");
    }
    return tests_refs_root_;
}

const char* tests_refs_name_(void) {
    return tests_refs_ref_ != NULL ? tests_refs_ref_ : "none";
}

static struct cr_str* tests_refs_dir(void) {
    struct cr_str* dir = cr_str_create_copy(cr_mem_libc, tests_refs_root());
    cr_str_append_path_component_cstr(dir, tests_refs_ref_);
    return dir;
}

static struct cr_str* tests_refs_path(const char* name) {
    if (tests_refs_ref_ == NULL) {
        return NULL;
    }
    struct cr_str* path = tests_refs_dir();
    cr_str_append_path_component_format(path, "%s.tiff", name);
    return path;
}

uint32_t tests_refs_dir_list_(struct tests_refs_dir_info* out, uint32_t capacity) {
    uint32_t count = 0;
    struct cr_util_dir_iter dirs = cr_util_dir_iter(cr_mem_libc, tests_refs_root());
    while (cr_util_dir_iter_next(&dirs)) {
        cr_util_dir_iter_skip(&dirs);
        const char* name = cr_str_get_data(dirs.relative_path);
        if (dirs.file_type != cr_util_file_type_directory || name[0] == '.') {
            continue;
        }
        uint32_t index = count;
        while (index > 0 && strcmp(name, out[index - 1].name) > 0) {
            index--;
        }
        if (index == capacity) {
            continue;
        }
        count = min_uint32(count + 1, capacity);
        memmove(out + index + 1, out + index, (count - 1 - index) * sizeof(*out));
        struct tests_refs_dir_info* info = &out[index];
        memset(info, 0, sizeof(*info));
        snprintf(info->name, sizeof(info->name), "%s", name);
        struct cr_util_dir_iter files = cr_util_dir_iter(cr_mem_libc, dirs.full_path);
        while (cr_util_dir_iter_next(&files)) {
            cr_util_dir_iter_skip(&files);
            if (files.file_type == cr_util_file_type_regular && cr_str_get_data(files.relative_path)[0] != '.') {
                info->item_count += 1;
                info->size_bytes += files.file_size;
            }
        }
    }
    return count;
}

bool tests_refs_dir_delete_(const char* name) {
    if (name == NULL || name[0] == 0 || name[0] == '.' || strchr(name, cr_path_div) != NULL) {
        return false;
    }
    struct cr_str* dir = cr_str_create_copy(cr_mem_libc, tests_refs_root());
    cr_str_append_path_component_cstr(dir, name);
    enum cr_util_file_type file_type;
    bool ok = cr_util_get_file_type(dir, &file_type) && file_type == cr_util_file_type_directory && cr_util_remove(dir);
    cr_str_destroy(dir);
    return ok;
}

static uint8_t* tests_refs_read_file(const char* name, uint32_t* out_size) {
    uint8_t* result = NULL;
    struct cr_str* path = tests_refs_path(name);
    FILE* file = path != NULL ? fopen(cr_str_get_data(path), "rb") : NULL;
    if (file != NULL) {
        fseek(file, 0, SEEK_END);
        long size = ftell(file);
        fseek(file, 0, SEEK_SET);
        uint8_t* data = size > 0 ? malloc((size_t)size) : NULL;
        if (data != NULL && fread(data, 1, (size_t)size, file) == (size_t)size) {
            result = data;
            *out_size = (uint32_t)size;
        } else {
            free(data);
        }
        fclose(file);
    }
    if (path != NULL) {
        cr_str_destroy(path);
    }
    return result;
}

#endif

#if cr_macos || cr_ios || cr_wasm

struct tests_refs_tiff {
    uint32_t width;
    uint32_t height;
    uint32_t tile_w;
    uint32_t tile_h;
    uint32_t tile_count;
    const uint8_t* tile_offsets;
};

static uint32_t tests_refs_u16be(const uint8_t* p) {
    return ((uint32_t)p[0] << 8) | p[1];
}

static uint32_t tests_refs_u32be(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

static uint32_t tests_refs_tag_scalar(const uint8_t* entry) {
    return tests_refs_u16be(entry + 2) == 3 ? tests_refs_u16be(entry + 8) : tests_refs_u32be(entry + 8);
}

static void tests_refs_tag_check_triple(const uint8_t* data, uint32_t size, const uint8_t* entry, uint32_t expected) {
    assert(tests_refs_u16be(entry + 2) == 3 && tests_refs_u32be(entry + 4) == 3);
    uint32_t offset = tests_refs_u32be(entry + 8);
    assert(offset + 6 <= size);
    for (uint32_t i = 0; i < 3; i++) {
        assert(tests_refs_u16be(data + offset + i * 2) == expected);
    }
}

static struct tests_refs_tiff tests_refs_parse(const uint8_t* data, uint32_t size) {
    struct tests_refs_tiff t = { 0 };
    assert(size > 8 && data[0] == 'M' && data[1] == 'M' && tests_refs_u16be(data + 2) == 42);
    uint32_t ifd = tests_refs_u32be(data + 4);
    assert(ifd + 2 <= size);
    uint32_t entry_count = tests_refs_u16be(data + ifd);
    assert(ifd + 2 + entry_count * 12 <= size);
    for (uint32_t i = 0; i < entry_count; i++) {
        const uint8_t* entry = data + ifd + 2 + i * 12;
        switch (tests_refs_u16be(entry)) {
        case 256: t.width = tests_refs_tag_scalar(entry); break;
        case 257: t.height = tests_refs_tag_scalar(entry); break;
        case 258: tests_refs_tag_check_triple(data, size, entry, 16); break;
        case 259: assert(tests_refs_tag_scalar(entry) == 1); break;
        case 262: assert(tests_refs_tag_scalar(entry) == 2); break;
        case 274: assert(tests_refs_tag_scalar(entry) == 1); break;
        case 277: assert(tests_refs_tag_scalar(entry) == 3); break;
        case 284: assert(tests_refs_tag_scalar(entry) == 1); break;
        case 322: t.tile_w = tests_refs_tag_scalar(entry); break;
        case 323: t.tile_h = tests_refs_tag_scalar(entry); break;
        case 324: {
            assert(tests_refs_u16be(entry + 2) == 4);
            t.tile_count = tests_refs_u32be(entry + 4);
            t.tile_offsets = t.tile_count == 1 ? entry + 8 : data + tests_refs_u32be(entry + 8);
        } break;
        case 338: assert(false); break;
        case 339: tests_refs_tag_check_triple(data, size, entry, 3); break;
        default: break;
        }
    }
    assert(t.width > 0 && t.height > 0 && t.tile_w > 0 && t.tile_h > 0 && t.tile_offsets != NULL);
    uint32_t tiles_x = (t.width + t.tile_w - 1) / t.tile_w;
    uint32_t tiles_y = (t.height + t.tile_h - 1) / t.tile_h;
    assert(t.tile_count == tiles_x * tiles_y);
    assert(t.tile_offsets + t.tile_count * 4 <= data + size);
    return t;
}

#if cr_macos || cr_ios

static float tests_refs_half_float(uint16_t bits) {
    __fp16 h;
    memcpy(&h, &bits, sizeof(h));
    return (float)h;
}

typedef uint8_t tests_refs_u8x16 __attribute__((vector_size(16)));

static inline void tests_refs_unpack8(const uint8_t* src, uint16_t* dst) {
    tests_refs_u8x16 a, b, c;
    memcpy(&a, src, 16);
    memcpy(&b, src + 16, 16);
    memcpy(&c, src + 32, 16);
    const tests_refs_u8x16 keep = { 255, 255, 255, 255, 255, 255, 0, 0, 255, 255, 255, 255, 255, 255, 0, 0 };
    const tests_refs_u8x16 alpha = { 0, 0, 0, 0, 0, 0, 0, 0x3c, 0, 0, 0, 0, 0, 0, 0, 0x3c };
#define tests_refs_px(s) (s) + 1, (s) + 0, (s) + 3, (s) + 2, (s) + 5, (s) + 4, (s), (s)
    tests_refs_u8x16 o0 = (__builtin_shufflevector(a, a, tests_refs_px(0), tests_refs_px(6)) & keep) | alpha;
    tests_refs_u8x16 o1 = (__builtin_shufflevector(a, b, tests_refs_px(12), tests_refs_px(18)) & keep) | alpha;
    tests_refs_u8x16 o2 = (__builtin_shufflevector(b, c, tests_refs_px(8), tests_refs_px(14)) & keep) | alpha;
    tests_refs_u8x16 o3 = (__builtin_shufflevector(c, c, tests_refs_px(4), tests_refs_px(10)) & keep) | alpha;
#undef tests_refs_px
    memcpy(dst, &o0, 16);
    memcpy(dst + 8, &o1, 16);
    memcpy(dst + 16, &o2, 16);
    memcpy(dst + 24, &o3, 16);
}

static uint16_t* tests_refs_decode_half(struct cr_mem* mem, const uint8_t* data, uint32_t size, const struct ui_test_scene* scene) {
    uint32_t width = scene->width;
    uint32_t height = scene->height;

    struct tests_refs_tiff t = tests_refs_parse(data, size);
    if (t.width != width || t.height != height) {
        return NULL;
    }
    uint16_t* rgba = cr_malloc(mem, (uint64_t)width * height * 4 * sizeof(uint16_t), 16, cr_loc);
    uint32_t tiles_x = (t.width + t.tile_w - 1) / t.tile_w;
    for (uint32_t i = 0; i < t.tile_count; i++) {
        uint32_t tile_x = (i % tiles_x) * t.tile_w;
        uint32_t tile_y = (i / tiles_x) * t.tile_h;
        uint32_t offset = tests_refs_u32be(t.tile_offsets + i * 4);
        assert(offset + t.tile_w * t.tile_h * 6 <= size);
        uint32_t copy_w = min_uint32(t.tile_w, width - tile_x);
        uint32_t copy_h = min_uint32(t.tile_h, height - tile_y);
        for (uint32_t y = 0; y < copy_h; y++) {
            const uint8_t* src = data + offset + (uint64_t)y * t.tile_w * 6;
            uint16_t* dst = rgba + ((uint64_t)(tile_y + y) * width + tile_x) * 4;
            uint32_t x = 0;
            for (; x + 8 <= copy_w; x += 8) {
                tests_refs_unpack8(src + (uint64_t)x * 6, dst + (uint64_t)x * 4);
            }
            for (; x < copy_w; x++) {
                dst[x * 4 + 0] = (uint16_t)tests_refs_u16be(src + x * 6);
                dst[x * 4 + 1] = (uint16_t)tests_refs_u16be(src + x * 6 + 2);
                dst[x * 4 + 2] = (uint16_t)tests_refs_u16be(src + x * 6 + 4);
                dst[x * 4 + 3] = 0x3c00;
            }
        }
    }
    return rgba;
}

static float* tests_refs_load_raw(struct cr_mem* mem, const struct ui_test_scene* scene) {
    uint32_t size = 0;
    uint8_t* data = tests_refs_read_file(scene->label, &size);
    uint16_t* half = data != NULL ? tests_refs_decode_half(mem, data, size, scene) : NULL;
    free(data);
    if (half == NULL) {
        return NULL;
    }
    uint64_t count = (uint64_t)scene->width * scene->height;
    float* rgba = cr_malloc(mem, count * 4 * sizeof(float), 16, cr_loc);
    for (uint64_t i = 0; i < count; i++) {
        float3 px = make_float3(tests_refs_half_float(half[i * 4 + 0]), tests_refs_half_float(half[i * 4 + 1]), tests_refs_half_float(half[i * 4 + 2]));
        if (scene->capture == ui_test_scene_capture_screenshot) {
            px = make_float3(max_float(px.x, 0.0f), max_float(px.y, 0.0f), max_float(px.z, 0.0f));
        }
        rgba[i * 4 + 0] = px.x;
        rgba[i * 4 + 1] = px.y;
        rgba[i * 4 + 2] = px.z;
        rgba[i * 4 + 3] = 1.0f;
    }
    cr_free(mem, half, cr_loc);
    return rgba;
}

bool tests_refs_has_(struct ui_test_scenes* scenes, uint32_t index) {
    const struct ui_test_scene* scene = ui_test_scene_get_(scenes, index);
    uint32_t size = 0;
    uint8_t* data = tests_refs_read_file(scene->label, &size);
    if (data == NULL) {
        return false;
    }
    struct tests_refs_tiff t = tests_refs_parse(data, size);
    free(data);
    return t.width == scene->width && t.height == scene->height;
}

#endif

extern const unsigned char* const tests_shaders_embed_data[];
extern const unsigned long long tests_shaders_embed_sizes[];

_Static_assert(ui_test_scene_band_shape_circle == ui_test_compare_band_circle, "");
_Static_assert(sizeof(struct ui_test_compare_params) == 48, "");
_Static_assert(sizeof(struct ui_test_compare_tile) == 48, "");

struct tests_refs_compare {
    struct cr_mem* mem;
    struct gpu_device* device;
    struct gpu_bind_layout* layout;
    struct gpu_compute_pipeline* pipeline;
};

struct tests_refs_metrics {
    double mean;
    double max;
    uint32_t max_x;
    uint32_t max_y;
    double over_005;
    uint32_t alarm_count;
    double tile_mean;
    uint32_t tile_x;
    uint32_t tile_y;
    double rim_mean;
    double bevel_mean;
    double interior_mean;
};

static struct tests_refs_compare* tests_refs_compare_create(void) {
    struct ui* ui = ui_ctx();
    struct tests_refs_compare* compare = cr_malloc(ui->mem, sizeof(struct tests_refs_compare), 16, cr_loc);
    memset(compare, 0, sizeof(*compare));

    compare->mem = ui->mem;
    compare->device = ui->device;

    struct gpu_binding_desc bindings[ui_test_compare_bind_count] = {
        [ui_test_compare_bind_params] = { .binding = ui_test_compare_bind_params, .type = gpu_binding_type_uniform, .stages = gpu_stage_compute },
        [ui_test_compare_bind_gpu] = { .binding = ui_test_compare_bind_gpu, .type = gpu_binding_type_texture, .stages = gpu_stage_compute },
        [ui_test_compare_bind_ref] = { .binding = ui_test_compare_bind_ref, .type = gpu_binding_type_storage_read, .stages = gpu_stage_compute },
        [ui_test_compare_bind_tiles] = { .binding = ui_test_compare_bind_tiles, .type = gpu_binding_type_storage, .stages = gpu_stage_compute },
    };
    compare->layout = gpu_bind_layout_create(compare->device, bindings, cr_count(bindings));

    struct gpu_library* library = gpu_library_create(compare->device, tests_shaders_embed_data[0], tests_shaders_embed_sizes[0], "ui_test_compare");
    compare->pipeline = gpu_compute_pipeline_create(compare->device, &(struct gpu_compute_pipeline_desc) {
        .library = library,
        .name = "ui_test_compare",
        .layouts = { compare->layout },
        .layout_count = 1,
        .workgroup_size = { 16, 16 },
    });
    gpu_library_destroy(compare->device, library);

    return compare;
}

static void tests_refs_compare_destroy(struct tests_refs_compare* compare) {
    gpu_compute_pipeline_destroy(compare->device, compare->pipeline);
    gpu_bind_layout_destroy(compare->device, compare->layout);
    cr_free(compare->mem, compare, cr_loc);
}

static uint2 tests_refs_tile_grid(const struct ui_test_scene* scene) {
    return (make_uint2(scene->width, scene->height) + (uint32_t)(ui_test_compare_tile_size - 1)) / (uint32_t)ui_test_compare_tile_size;
}

static struct ui_test_compare_tile* tests_refs_compare_run(struct tests_refs_compare* compare, const struct ui_test_scene* scene, struct gpu_texture* target, const uint8_t* data, uint32_t size, float alarm_diff, uint64_t* out_serial) {
    uint32_t width = scene->width;
    uint32_t height = scene->height;
    uint2 grid = tests_refs_tile_grid(scene);
    uint32_t tiles_x = grid.x;
    uint32_t tile_count = grid.x * grid.y;

    struct tests_refs_tiff t = tests_refs_parse(data, size);
    uint32_t offset = tests_refs_u32be(t.tile_offsets);
    uint32_t ref_size = align_up_uint32(width * height * 6, 4);
    assert(t.tile_count == 1 && t.tile_w == width && t.tile_h == height && offset + ref_size <= size && "references are one uncompressed tile");

    struct gpu_buffer* ref = gpu_buffer_create(compare->device, ref_size, gpu_buffer_usage_storage | gpu_buffer_usage_write, "ui test compare ref");
    gpu_buffer_write(compare->device, ref, 0, data + offset, ref_size);

    struct ui_test_compare_params params = {
        .size = make_uint2(width, height),
        .tiles_x = tiles_x,
        .flags = scene->capture == ui_test_scene_capture_screenshot ? ui_test_compare_flag_clamp : 0u,
        .alarm_diff = alarm_diff,
        .to_pt = 1.0f / scene->to_px_from_pt,
        .band_type = scene->band_shape.type,
        .band_radius = scene->band_shape.radius,
        .band_origin = scene->band_shape.origin,
        .band_size = scene->band_shape.size,
    };
    struct gpu_buffer* params_buffer = gpu_buffer_create(compare->device, sizeof(params), gpu_buffer_usage_uniform | gpu_buffer_usage_write, "ui test compare params");
    gpu_buffer_write(compare->device, params_buffer, 0, &params, sizeof(params));

    struct gpu_buffer* tiles = gpu_buffer_create(compare->device, tile_count * (uint32_t)sizeof(struct ui_test_compare_tile), gpu_buffer_usage_storage | gpu_buffer_usage_read, "ui test compare tiles");

    struct gpu_bind_desc binds[ui_test_compare_bind_count] = {
        [ui_test_compare_bind_params] = { .binding = ui_test_compare_bind_params, .buffer = params_buffer, .buffer_size = sizeof(params) },
        [ui_test_compare_bind_gpu] = { .binding = ui_test_compare_bind_gpu, .texture = target },
        [ui_test_compare_bind_ref] = { .binding = ui_test_compare_bind_ref, .buffer = ref, .buffer_size = ref_size },
        [ui_test_compare_bind_tiles] = { .binding = ui_test_compare_bind_tiles, .buffer = tiles, .buffer_size = tile_count * (uint32_t)sizeof(struct ui_test_compare_tile) },
    };
    struct gpu_bind_group* group = gpu_bind_group_create(compare->device, compare->layout, binds, cr_count(binds));

    struct ui_test_compare_tile* results = cr_malloc(compare->mem, tile_count * sizeof(struct ui_test_compare_tile), 16, cr_loc);

    cr_autoreleasepool {
        struct gpu_command_buffer* command_buffer = gpu_command_buffer_create(compare->device, "ui test compare");
        struct gpu_compute_pass* pass = gpu_command_buffer_begin_compute(command_buffer, "ui test compare");
        gpu_compute_pass_set_pipeline(pass, compare->pipeline);
        gpu_compute_pass_set_bind_group(pass, 0, group, NULL, 0);
        gpu_compute_pass_dispatch(pass, grid.x, grid.y, 1);
        gpu_compute_pass_end(pass);
        gpu_command_buffer_read_buffer(command_buffer, tiles, 0, results, tile_count * (uint32_t)sizeof(struct ui_test_compare_tile));
        *out_serial = gpu_command_buffer_commit(command_buffer);
    }

    gpu_bind_group_destroy(compare->device, group);
    gpu_buffer_destroy(compare->device, tiles);
    gpu_buffer_destroy(compare->device, params_buffer);
    gpu_buffer_destroy(compare->device, ref);

    return results;
}

static void tests_refs_metrics(const struct ui_test_scene* scene, const struct ui_test_compare_tile* results, struct tests_refs_metrics* out) {
    uint32_t width = scene->width;
    uint32_t height = scene->height;
    uint2 grid = tests_refs_tile_grid(scene);
    uint32_t tiles_x = grid.x;
    uint32_t tile_count = grid.x * grid.y;

    memset(out, 0, sizeof(*out));

    double sum = 0.0;
    uint64_t over_005 = 0;
    double band_sum[3] = { 0.0, 0.0, 0.0 };
    uint64_t band_count[3] = { 0, 0, 0 };
    float max_d = -1.0f;
    uint32_t max_idx = 0;

    for (uint32_t ti = 0; ti < tile_count; ti++) {
        const struct ui_test_compare_tile* tile = &results[ti];

        sum += tile->sum;
        over_005 += tile->over_005;
        out->alarm_count += tile->alarm;

        band_sum[0] += tile->rim_sum;
        band_sum[1] += tile->bevel_sum;
        band_sum[2] += tile->interior_sum;
        band_count[0] += tile->rim_count;
        band_count[1] += tile->bevel_count;
        band_count[2] += tile->interior_count;

        if (tile->max_d > max_d || (tile->max_d == max_d && tile->max_idx < max_idx)) {
            max_d = tile->max_d;
            max_idx = tile->max_idx;
        }

        uint32_t copy_w = min_uint32(ui_test_compare_tile_size, width - (ti % tiles_x) * ui_test_compare_tile_size);
        uint32_t copy_h = min_uint32(ui_test_compare_tile_size, height - (ti / tiles_x) * ui_test_compare_tile_size);
        uint32_t count = copy_w * copy_h;
        if (count < ui_test_compare_tile_size * ui_test_compare_tile_size / 4) {
            continue;
        }
        double m = (double)tile->sum / (double)count;
        if (m > out->tile_mean) {
            out->tile_mean = m;
            out->tile_x = (ti % tiles_x) * ui_test_compare_tile_size;
            out->tile_y = (ti / tiles_x) * ui_test_compare_tile_size;
        }
    }

    uint64_t eval_count = (uint64_t)width * height;
    out->mean = sum / (double)max_uint64(eval_count, 1);
    out->max = max_d;
    out->max_x = max_idx % width;
    out->max_y = max_idx / width;
    out->over_005 = 100.0 * (double)over_005 / (double)max_uint64(eval_count, 1);
    out->rim_mean = band_sum[0] / (double)max_uint64(band_count[0], 1);
    out->bevel_mean = band_sum[1] / (double)max_uint64(band_count[1], 1);
    out->interior_mean = band_sum[2] / (double)max_uint64(band_count[2], 1);
}

struct tests_refs_thresholds {
    float mean;
    float tile;
    float alarm_diff;
    uint32_t alarm_allowance;
    float rim;
    float bevel;
    float interior;
};

static struct tests_refs_thresholds tests_refs_thresholds(const struct ui_test_scene* scene) {
    return (struct tests_refs_thresholds) {
        .mean = scene->mean_threshold,
        .tile = scene->tile_threshold > 0.0f ? scene->tile_threshold : max_float(scene->mean_threshold * 4.0f, 0.004f),
        .alarm_diff = scene->alarm_diff != 0.0f ? scene->alarm_diff : 0.26f,
        .alarm_allowance = scene->alarm_allowance,
        .rim = scene->rim_threshold > 0.0f ? scene->rim_threshold : max_float(4.0f * scene->mean_threshold, 0.012f),
        .bevel = scene->bevel_threshold > 0.0f ? scene->bevel_threshold : max_float(2.0f * scene->mean_threshold, 0.008f),
        .interior = scene->interior_threshold > 0.0f ? scene->interior_threshold : scene->mean_threshold,
    };
}

struct tests_refs_perf {
    double build;
    double layout;
    double render;
    struct gpu_profile gpu;
};

static void tests_refs_perf_min(struct tests_refs_perf* perf, const struct ui_test_scene_render* render, struct gpu_profiler* profiler) {
    perf->build = min_double(perf->build, render->build_s);
    perf->layout = min_double(perf->layout, render->layout_s);
    perf->render = min_double(perf->render, render->render_s);

    if (render->profiled == false) {
        return;
    }
    struct gpu_profile profile;
    bool ready = gpu_profiler_poll(profiler, &profile);
    assert(ready && "a completed command has its profile");
    perf->gpu.span = min_double(perf->gpu.span, profile.span);
    for (uint32_t i = 0; i < profile.pass_count; i++) {
        if (i >= perf->gpu.pass_count) {
            perf->gpu.passes[i] = INFINITY;
            memcpy(perf->gpu.pass_names[i], profile.pass_names[i], sizeof(profile.pass_names[i]));
        }
        perf->gpu.passes[i] = min_double(perf->gpu.passes[i], profile.passes[i]);
    }
    perf->gpu.pass_count = max_uint32(perf->gpu.pass_count, profile.pass_count);
}

#define tests_refs_prefetch (cr_wasm ? 4u : 0u)
#define tests_refs_in_flight 8u

struct tests_refs_ref {
    bool ready;
    uint8_t* data;
    uint32_t size;
};

struct tests_refs_job {
    uint32_t index;
    uint32_t renders;
    struct ui_test_scene_state* state;
    struct gpu_texture* target;
    struct ui_test_scene_render render;
    struct tests_refs_perf perf;
    struct ui_test_compare_tile* tiles;
    uint64_t serial;
};

static struct tests_refs_suite {
    struct ui_test_scenes* scenes;
    uint32_t count;
    uint32_t iterations;
    uint32_t in_flight;
    bool* out_pass;
    bool pass;
    uint32_t requested;
    uint32_t started;
    uint32_t checked;
    uint32_t unreferenced;
    struct tests_refs_ref* refs;
    struct tests_refs_job jobs[tests_refs_in_flight];
    struct tests_refs_compare* compare;
    struct gpu_profiler* profiler;
} tests_refs_;

#if cr_wasm
static void tests_refs_on_fetch(void* user, void* data, uint32_t size) {
    struct tests_refs_ref* ref = user;
    ref->data = data;
    ref->size = size;
    ref->ready = true;
    ui_test_wake_();
}
#endif

static void tests_refs_request(uint32_t until) {
    for (; tests_refs_.requested < min_uint32(until, tests_refs_.count); tests_refs_.requested++) {
        const char* label = ui_test_scene_get_(tests_refs_.scenes, tests_refs_.requested)->label;
        struct tests_refs_ref* ref = &tests_refs_.refs[tests_refs_.requested];
#if cr_wasm
        char url[256];
        int n = snprintf(url, sizeof(url), "/refs/%s/%s.tiff", tests_refs_ref_, label);
        assert(n > 0 && (size_t)n < sizeof(url));
        cr_fetch(url, tests_refs_on_fetch, ref);
#else
        ref->data = tests_refs_read_file(label, &ref->size);
        ref->ready = true;
#endif
    }
}

static bool tests_refs_report(const struct ui_test_scene* scene, struct tests_refs_job* job) {
    if (job->tiles == NULL) {
        printf("scene %s: no reference\n", scene->label);
        tests_refs_.unreferenced++;
        return true;
    }

    uint32_t iterations = tests_refs_.iterations;
    if (iterations > 1) {
        struct tests_refs_perf* perf = &job->perf;
        tests_refs_perf_min(perf, &job->render, tests_refs_.profiler);
        printf("perf %s: build %.3f, layout %.3f, render %.3f", scene->label, perf->build * 1000.0, perf->layout * 1000.0, perf->render * 1000.0);
        if (perf->gpu.span < INFINITY) {
            printf(", gpu %.3f", perf->gpu.span * 1000.0);
            for (uint32_t i = 0; i < perf->gpu.pass_count; i++) {
                printf(", %s %.3f", perf->gpu.pass_names[i], perf->gpu.passes[i] * 1000.0);
            }
        }
        printf(" ms (min over %u)\n", iterations);
    }

    struct tests_refs_thresholds thresholds = tests_refs_thresholds(scene);
    struct tests_refs_metrics m;
    tests_refs_metrics(scene, job->tiles, &m);

    bool pass = m.mean <= (double)thresholds.mean
        && m.tile_mean <= (double)thresholds.tile
        && m.alarm_count <= thresholds.alarm_allowance;

    char bands[96] = "";
    if (scene->band_shape.type != ui_test_scene_band_shape_none) {
        pass = pass
            && m.rim_mean <= (double)thresholds.rim
            && m.bevel_mean <= (double)thresholds.bevel
            && m.interior_mean <= (double)thresholds.interior;
        snprintf(bands, sizeof(bands), "rim %.6f bevel %.6f int %.6f, ", m.rim_mean, m.bevel_mean, m.interior_mean);
    }

    printf("scene %s %ux%u px: mean %.6f, tile %.6f at (%u,%u), max %.4f at (%u,%u) px, over 0.05: %.3f%%, over %g: %u px, %sthr %g %g %u, %s\n",
           scene->label, scene->width, scene->height, m.mean, m.tile_mean, m.tile_x, m.tile_y, m.max, m.max_x, m.max_y, m.over_005,
           thresholds.alarm_diff, m.alarm_count, bands, thresholds.mean, thresholds.tile, thresholds.alarm_allowance,
           pass ? "ok" : "FAIL");
    return pass;
}

static void tests_refs_check(void) {
    struct tests_refs_job* job = &tests_refs_.jobs[tests_refs_.checked++ % tests_refs_.in_flight];
    tests_refs_.pass = tests_refs_report(ui_test_scene_get_(tests_refs_.scenes, job->index), job) && tests_refs_.pass;
    cr_free(tests_refs_.compare->mem, job->tiles, cr_loc);
}

static void tests_refs_finish(void* args) {
    while (tests_refs_.checked < tests_refs_.count) {
        tests_refs_check();
    }
    gpu_profiler_destroy(ui_ctx()->device, tests_refs_.profiler);
    tests_refs_compare_destroy(tests_refs_.compare);
    cr_free(cr_mem_libc, tests_refs_.refs, cr_loc);
    printf("refs: %u of %u scenes have a reference in %s\n", tests_refs_.count - tests_refs_.unreferenced, tests_refs_.count, tests_refs_ref_);
    *tests_refs_.out_pass = tests_refs_.pass && tests_refs_.unreferenced < tests_refs_.count;
}

static void tests_refs_start(void* args);

static void tests_refs_next(void) {
    if (tests_refs_.started == tests_refs_.count) {
        for (uint32_t i = 0; i < tests_refs_.in_flight; i++) {
            ui_test_await_(tests_refs_.jobs[i].serial);
        }
        ui_test_then_(tests_refs_finish, NULL);
        return;
    }

    tests_refs_request(tests_refs_.started + 1 + tests_refs_prefetch);
    ui_test_await_(tests_refs_.jobs[tests_refs_.started % tests_refs_.in_flight].serial);
    ui_test_await_flag_(&tests_refs_.refs[tests_refs_.started].ready);
    ui_test_then_(tests_refs_start, NULL);
}

static void tests_refs_render(void* args) {
    struct tests_refs_job* job = args;
    if (job->renders > 0) {
        tests_refs_perf_min(&job->perf, &job->render, tests_refs_.profiler);
    }

    const struct ui_test_scene* scene = ui_test_scene_get_(tests_refs_.scenes, job->index);
    job->render = ui_test_scene_render_(job->state, scene, job->target, tests_refs_.profiler, NULL);
    job->renders++;
    if (job->renders < tests_refs_.iterations) {
        ui_test_await_(job->render.serial);
        ui_test_then_(tests_refs_render, job);
        return;
    }

    job->state->destroy(job->state);

    job->serial = job->render.serial;
    struct tests_refs_ref* ref = &tests_refs_.refs[job->index];
    if (ref->data != NULL) {
        struct tests_refs_tiff t = tests_refs_parse(ref->data, ref->size);
        if (t.width == scene->width && t.height == scene->height) {
            job->tiles = tests_refs_compare_run(tests_refs_.compare, scene, job->target, ref->data, ref->size, tests_refs_thresholds(scene).alarm_diff, &job->serial);
        }
    }
    free(ref->data);
    ref->data = NULL;
    gpu_texture_destroy(ui_ctx()->device, job->target);

    tests_refs_next();
}

static void tests_refs_start(void* args) {
    if (tests_refs_.started - tests_refs_.checked == tests_refs_.in_flight) {
        tests_refs_check();
    }

    uint32_t index = tests_refs_.started++;
    const struct ui_test_scene* scene = ui_test_scene_get_(tests_refs_.scenes, index);
    struct tests_refs_job* job = &tests_refs_.jobs[index % tests_refs_.in_flight];
    *job = (struct tests_refs_job) {
        .index = index,
        .state = ui_test_scene_create_(tests_refs_.scenes, cr_mem_libc, scene),
        .target = gpu_texture_create(ui_ctx()->device, &(struct gpu_texture_desc) {
            .label = "test scene target",
            .format = gpu_format_rgba16f,
            .size = { scene->width, scene->height },
            .usage = gpu_texture_usage_render_target | gpu_texture_usage_sample,
        }),
        .perf = { .build = INFINITY, .layout = INFINITY, .render = INFINITY, .gpu.span = INFINITY },
    };
    tests_refs_render(job);
}

void tests_refs_test_(struct ui_test_scenes* scenes, uint32_t iterations, bool* out_pass) {
    if (tests_refs_ref_ == NULL) {
        printf("refs: no --ref, skipped\n");
        *out_pass = true;
        return;
    }
    uint32_t count = ui_test_scene_count_(scenes);
    assert(count > 0);

    tests_refs_ = (struct tests_refs_suite) {
        .scenes = scenes,
        .count = count,
        .iterations = iterations,
        .in_flight = iterations > 1 ? 1 : tests_refs_in_flight,
        .out_pass = out_pass,
        .pass = true,
        .refs = cr_malloc(cr_mem_libc, count * sizeof(struct tests_refs_ref), 16, cr_loc),
        .compare = tests_refs_compare_create(),
        .profiler = iterations > 1 ? gpu_profiler_create(ui_ctx()->device, gpu_profile_max_passes) : NULL,
    };
    memset(tests_refs_.refs, 0, count * sizeof(struct tests_refs_ref));
    ui_test_await_(gpu_device_get_pipelines_serial(ui_ctx()->device));
    tests_refs_next();
}

#endif

#if cr_macos || cr_ios

bool tests_refs_load_graphic_(struct ui_test_scenes* scenes, uint32_t index, struct ui_graphic** out) {
    const struct ui_test_scene* scene = ui_test_scene_get_(scenes, index);
    struct cr_mem* mem = cr_mem_libc;

    float* ref_raw = tests_refs_load_raw(mem, scene);
    if (ref_raw == NULL) {
        return false;
    }

    uint64_t count = (uint64_t)scene->width * scene->height * 4;
    __fp16* half = cr_malloc(mem, count * sizeof(__fp16), 16, cr_loc);
    for (uint64_t i = 0; i < count; i++) {
        half[i] = (__fp16)ref_raw[i];
    }
    *out = ui_graphic_create(scene->width, scene->height, ui_pixel_format_rgba16f, (const uint8_t*)half, scene->width * 4 * (uint32_t)sizeof(__fp16), true);
    cr_free(mem, half, cr_loc);
    cr_free(mem, ref_raw, cr_loc);
    return true;
}

#endif

#if cr_ios

static void tests_refs_store_u16be(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}

static void tests_refs_store_u32be(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static uint8_t* tests_refs_tiff_entry(uint8_t* p, uint32_t tag, uint32_t type, uint32_t count, uint32_t value) {
    tests_refs_store_u16be(p, tag);
    tests_refs_store_u16be(p + 2, type);
    tests_refs_store_u32be(p + 4, count);
    if (type == 3 && count == 1) {
        tests_refs_store_u16be(p + 8, value);
        tests_refs_store_u16be(p + 10, 0);
    } else {
        tests_refs_store_u32be(p + 8, value);
    }
    return p + 12;
}

static CFDataRef tests_refs_icc(enum ui_color_space space) {
    static CFDataRef icc[2];
    uint32_t i = space == ui_color_space_srgb ? 0 : 1;
    if (icc[i] == NULL) {
        CGColorSpaceRef cs = CGColorSpaceCreateWithName(i == 0 ? kCGColorSpaceSRGB : kCGColorSpaceDisplayP3);
        icc[i] = CGColorSpaceCopyICCData(cs);
        CGColorSpaceRelease(cs);
        assert(icc[i] != NULL && CFDataGetLength(icc[i]) > 4);
    }
    return icc[i];
}

static bool tests_refs_save(const char* name, uint32_t width, uint32_t height, enum ui_color_space space, const float* rgba) {
    assert(space == ui_color_space_srgb || space == ui_color_space_display_p3);
    CFDataRef icc = tests_refs_icc(space);
    uint32_t icc_size = (uint32_t)CFDataGetLength(icc);
    uint64_t data_size = (uint64_t)width * height * 6;
    uint32_t bits_offset = (uint32_t)(8 + data_size);
    uint32_t fmt_offset = bits_offset + 6;
    uint32_t icc_offset = fmt_offset + 6;
    uint32_t ifd = icc_offset + icc_size + (icc_size & 1);
    uint64_t total = ifd + 2 + 13 * 12 + 4;
    uint8_t* out = malloc(total);
    assert(out != NULL);

    out[0] = 'M';
    out[1] = 'M';
    tests_refs_store_u16be(out + 2, 42);
    tests_refs_store_u32be(out + 4, ifd);

    uint8_t* px = out + 8;
    for (uint64_t i = 0; i < (uint64_t)width * height; i++) {
        for (uint32_t ch = 0; ch < 3; ch++) {
            __fp16 h = (__fp16)rgba[i * 4 + ch];
            uint16_t bits;
            memcpy(&bits, &h, sizeof(bits));
            tests_refs_store_u16be(px, bits);
            px += 2;
        }
    }

    for (uint32_t i = 0; i < 3; i++) {
        tests_refs_store_u16be(out + bits_offset + i * 2, 16);
        tests_refs_store_u16be(out + fmt_offset + i * 2, 3);
    }
    memcpy(out + icc_offset, CFDataGetBytePtr(icc), icc_size);
    if (icc_size & 1) {
        out[icc_offset + icc_size] = 0;
    }

    uint8_t* e = out + ifd;
    tests_refs_store_u16be(e, 13);
    e += 2;
    e = tests_refs_tiff_entry(e, 256, 3, 1, width);
    e = tests_refs_tiff_entry(e, 257, 3, 1, height);
    e = tests_refs_tiff_entry(e, 258, 3, 3, bits_offset);
    e = tests_refs_tiff_entry(e, 259, 3, 1, 1);
    e = tests_refs_tiff_entry(e, 262, 3, 1, 2);
    e = tests_refs_tiff_entry(e, 274, 3, 1, 1);
    e = tests_refs_tiff_entry(e, 277, 3, 1, 3);
    e = tests_refs_tiff_entry(e, 284, 3, 1, 1);
    e = tests_refs_tiff_entry(e, 322, 3, 1, width);
    e = tests_refs_tiff_entry(e, 323, 3, 1, height);
    e = tests_refs_tiff_entry(e, 324, 4, 1, 8);
    e = tests_refs_tiff_entry(e, 339, 3, 3, fmt_offset);
    e = tests_refs_tiff_entry(e, 34675, 7, icc_size, icc_offset);
    tests_refs_store_u32be(e, 0);

    bool ok = false;
    struct cr_str* path = tests_refs_path(name);
    FILE* file = path != NULL ? fopen(cr_str_get_data(path), "wb") : NULL;
    if (file != NULL) {
        ok = fwrite(out, 1, total, file) == total;
        fclose(file);
    }
    if (path != NULL) {
        cr_str_destroy(path);
    }
    free(out);
    if (ok == false) {
        printf("refs: TIFF write failed for %s\n", name);
    }
    return ok;
}

static CIContext* tests_refs_context(void) {
    static CIContext* context;
    if (context == NULL) {
        CGColorSpaceRef linear = CGColorSpaceCreateWithName(kCGColorSpaceExtendedLinearDisplayP3);
        context = [[CIContext contextWithOptions:@{
            kCIContextWorkingColorSpace: (__bridge id)linear,
            kCIContextWorkingFormat: @(kCIFormatRGBAh),
        }] retain];
        CGColorSpaceRelease(linear);
    }
    return context;
}

static bool tests_refs_photos_authorized(double timeout_s) {
    __block PHAuthorizationStatus status = [PHPhotoLibrary authorizationStatusForAccessLevel:PHAccessLevelReadWrite];
    if (status == PHAuthorizationStatusNotDetermined) {
        __block bool done = false;
        [PHPhotoLibrary requestAuthorizationForAccessLevel:PHAccessLevelReadWrite handler:^(PHAuthorizationStatus result) {
            status = result;
            done = true;
            CFRunLoopWakeUp(CFRunLoopGetMain());
        }];
        double deadline = cr_get_current_time() + timeout_s;
        while (done == false && cr_get_current_time() < deadline) {
            CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.1, true);
        }
    }
    return status == PHAuthorizationStatusAuthorized;
}

static NSMutableArray* tests_refs_imported_ids_;

static bool tests_refs_import_screenshot(const char* name, uint32_t width_px, uint32_t height_px, enum ui_color_space space, double created_after, double timeout_s) {
    if (@available(iOS 17.0, *)) {
        NSDate* window_start = [NSDate dateWithTimeIntervalSinceReferenceDate:created_after];
        double deadline = cr_get_current_time() + timeout_s;
        NSData* heic = nil;
        NSString* asset_id = nil;

        while (heic == nil && cr_get_current_time() < deadline) {
            @autoreleasepool {
                PHFetchOptions* options = [[PHFetchOptions new] autorelease];
                options.sortDescriptors = @[[NSSortDescriptor sortDescriptorWithKey:@"creationDate" ascending:NO]];
                options.fetchLimit = 1;
                PHAsset* newest = [PHAsset fetchAssetsWithMediaType:PHAssetMediaTypeImage options:options].firstObject;

                bool is_new = newest != nil && newest.creationDate != nil
                    && [newest.creationDate compare:window_start] == NSOrderedDescending;
                bool is_screenshot = newest != nil && (newest.mediaSubtypes & PHAssetMediaSubtypePhotoScreenshot) != 0;

                if (is_new && is_screenshot) {
                    __block NSData* data = nil;
                    PHImageRequestOptions* request = [[PHImageRequestOptions new] autorelease];
                    request.synchronous = YES;
                    request.networkAccessAllowed = NO;
                    request.version = PHImageRequestOptionsVersionCurrent;
                    [PHImageManager.defaultManager requestImageDataAndOrientationForAsset:newest
                                                                                  options:request
                                                                            resultHandler:^(NSData* d, NSString* uti, CGImagePropertyOrientation orientation, NSDictionary* info) {
                        data = [d retain];
                    }];
                    heic = data;
                    asset_id = [newest.localIdentifier copy];
                }
            }
            if (heic == nil) {
                CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.2, true);
            }
        }
        if (asset_id != nil) {
            if (tests_refs_imported_ids_ == nil) {
                tests_refs_imported_ids_ = [NSMutableArray new];
            }
            [tests_refs_imported_ids_ addObject:asset_id];
            [asset_id release];
        }
        if (heic == nil) {
            printf("refs: no importable screenshot for %s within %.1fs\n", name, timeout_s);
            return false;
        }

        bool ok = false;
        @autoreleasepool {
            CIImage* full = [CIImage imageWithData:heic options:@{ kCIImageExpandToHDR: @YES }];
            if (full == NULL) {
                printf("refs: screenshot decode failed for %s\n", name);
            } else if (full.extent.size.width < width_px || full.extent.size.height < height_px) {
                printf("refs: screenshot %.0fx%.0f smaller than scene %ux%u for %s\n",
                       full.extent.size.width, full.extent.size.height, width_px, height_px, name);
            } else {
                CGColorSpaceRef cs = CGColorSpaceCreateWithName(space == ui_color_space_srgb ? kCGColorSpaceExtendedSRGB : kCGColorSpaceExtendedDisplayP3);
                CGFloat y = full.extent.size.height - height_px;
                CIImage* cropped = [[full imageByCroppingToRect:CGRectMake(0.0, y, width_px, height_px)]
                    imageByApplyingTransform:CGAffineTransformMakeTranslation(0.0, -y)];
                float* rgba = malloc((uint64_t)width_px * height_px * 4 * sizeof(float));
                assert(rgba != NULL);
                [tests_refs_context() render:cropped
                                    toBitmap:rgba
                                    rowBytes:(ptrdiff_t)width_px * 16
                                      bounds:CGRectMake(0.0, 0.0, width_px, height_px)
                                      format:kCIFormatRGBAf
                                  colorSpace:cs];
                CGColorSpaceRelease(cs);
                ok = tests_refs_save(name, width_px, height_px, space, rgba);
                free(rgba);
            }
        }
        [heic release];
        return ok;
    }
    return false;
}

static uint32_t tests_refs_delete_imported(double timeout_s) {
    if (tests_refs_imported_ids_.count == 0) {
        return 0;
    }
    __block uint32_t deleted = 0;
    @autoreleasepool {
        PHFetchResult* assets = [PHAsset fetchAssetsWithLocalIdentifiers:tests_refs_imported_ids_ options:nil];
        if (assets.count == 0) {
            [tests_refs_imported_ids_ removeAllObjects];
            return 0;
        }
        printf("refs: deleting %lu imported screenshots (confirm on device)\n", (unsigned long)assets.count);

        __block bool done = false;
        __block bool success = false;
        [PHPhotoLibrary.sharedPhotoLibrary performChanges:^{
            [PHAssetChangeRequest deleteAssets:assets];
        } completionHandler:^(BOOL ok, NSError* error) {
            if (ok == false && error != nil) {
                printf("refs: screenshot delete failed: %s\n", error.localizedDescription.UTF8String);
            }
            success = ok;
            done = true;
            CFRunLoopWakeUp(CFRunLoopGetMain());
        }];

        double deadline = cr_get_current_time() + timeout_s;
        while (done == false && cr_get_current_time() < deadline) {
            CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.1, true);
        }
        if (done == false) {
            printf("refs: screenshot delete timed out after %.0fs (unconfirmed)\n", timeout_s);
        }
        deleted = success ? (uint32_t)assets.count : 0;
    }
    [tests_refs_imported_ids_ removeAllObjects];
    return deleted;
}

static void tests_refs_spin(double seconds) {
    double deadline = cr_get_current_time() + seconds;
    while (cr_get_current_time() < deadline) {
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.05, false);
    }
}

static void tests_refs_slider_apply(float amount) {
    NSUserDefaults* defaults = [[[NSUserDefaults alloc] initWithSuiteName:@"com.apple.UIKit"] autorelease];
    if (amount < 0.0f) {
        [defaults removeObjectForKey:@"UIViewGlassTintAmount"];
    } else {
        [defaults setObject:@(amount) forKey:@"UIViewGlassTintAmount"];
    }
    [defaults synchronize];
    notify_post("UIViewGlassTintAmountUpdateNotification");
    tests_refs_spin(0.3);
    notify_post("UIViewGlassTintAmountUpdateNotification");
    tests_refs_spin(0.3);
}

static bool tests_refs_slider_scan(CALayer* layer, double* normal, double* blur_radius) {
    bool found = false;
    for (id filter in layer.filters) {
        @try {
            NSNumber* n = nil;
            if ([[filter valueForKey:@"name"] isEqualToString:@"glassBackground"]) {
                n = [filter valueForKey:@"inputBlurFillNormalOpacity"];
            }
            if (n != nil) {
                *normal = n.doubleValue;
                *blur_radius = [[filter valueForKey:@"inputBlurRadius"] doubleValue];
                found = true;
            }
        } @catch (id e) {
        }
    }
    for (CALayer* sub in layer.sublayers) {
        found = tests_refs_slider_scan(sub, normal, blur_radius) || found;
    }
    return found;
}

static bool tests_refs_slider_verify(float amount, bool regular) {
    double normal = 0.0;
    double blur_radius = 0.0;
    bool found = false;
    for (UIScene* scene in UIApplication.sharedApplication.connectedScenes) {
        if ([scene isKindOfClass:[UIWindowScene class]] == false) {
            continue;
        }
        for (UIWindow* window in ((UIWindowScene*)scene).windows) {
            found = tests_refs_slider_scan(window.layer, &normal, &blur_radius) || found;
        }
    }
    double a = round((double)amount * 20.0) / 20.0;
    double lo = min_double(1.0, 2.0 * a);
    double hi = max_double(0.0, 2.0 * a - 1.0);
    double want_normal = regular ? a : hi;
    bool ok = found && fabs(normal - want_normal) < 5e-4 && (regular || fabs(blur_radius - 0.75 * lo) < 5e-4);
    if (ok == false) {
        printf("slider verify: amount %.2f %s, normal %.4f (want %.4f), blur %.4f%s\n",
               (double)amount, regular ? "regular" : "clear", normal, want_normal, blur_radius, found ? "" : ", no filter");
    }
    return ok;
}

static bool tests_refs_capture_pixels(struct ui_test_scenes* scenes, const struct ui_test_scene* scene) {
    assert(scene->pixels != NULL);
    struct cr_mem* mem = cr_mem_libc;
    struct ui_test_scene_state* state = ui_test_scene_create_(scenes, mem, scene);
    float* rgba = cr_malloc(mem, (uint64_t)scene->width * scene->height * 4 * sizeof(float), 16, cr_loc);
    cr_autoreleasepool {
        scene->pixels(state, scene->width, scene->height, scene->to_px_from_pt, rgba, scene->width * 4 * (uint32_t)sizeof(float));
    }
    state->destroy(state);
    bool ok = tests_refs_save(scene->label, scene->width, scene->height, scene->output_color_space, rgba);
    cr_free(mem, rgba, cr_loc);
    return ok;
}

static bool tests_refs_capture_screenshot(struct ui_test_scenes* scenes, const struct ui_test_scene* scene) {
    static float applied_slider = -1000.0f;
    const struct ui_test_capture_scene* s = scene->cap;
    assert(s != NULL || scene->present != NULL);
    struct ui_test_scene_state* state = s == NULL ? ui_test_scene_create_(scenes, cr_mem_libc, scene) : NULL;
    if (s != NULL && fabsf(s->slider - applied_slider) > 1e-3f) {
        tests_refs_slider_apply(s->slider);
        applied_slider = s->slider;
    }

    bool ok = false;
    for (uint32_t attempt = 0; attempt < 3 && ok == false; attempt++) {
        double presented_at = CFAbsoluteTimeGetCurrent();
        if (s != NULL) {
            ui_test_glass_scene_present_(scene);
        } else {
            scene->present(state, scene->width, scene->height, scene->to_px_from_pt);
        }
        tests_refs_spin(0.4);

        if (s != NULL && s->kind == ui_test_capture_kind_glass && tests_refs_slider_verify(s->slider, s->regular) == false) {
            printf("capture %s: slider %.2f not active — reapplying\n", s->name, (double)s->slider);
            tests_refs_slider_apply(s->slider);
            continue;
        }

        ok = tests_refs_import_screenshot(scene->label, scene->width, scene->height, scene->output_color_space, presented_at, 8.0);
    }

    if (state != NULL) {
        state->destroy(state);
    }
    return ok;
}

static bool tests_refs_capture_scene(struct ui_test_scenes* scenes, const struct ui_test_scene* scene) {
    if (scene->capture == ui_test_scene_capture_pixels) {
        return tests_refs_capture_pixels(scenes, scene);
    }
    assert(scene->capture == ui_test_scene_capture_screenshot);
    return tests_refs_capture_screenshot(scenes, scene);
}

void tests_refs_capture_(struct ui_test_scenes* scenes, double screen_to_px_from_pt, double2 screen_size, const char* only) {
    struct cr_str* log_path = cr_util_get_documents_dir(cr_mem_libc);
    cr_str_append_path_component_cstr(log_path, "capture.log");
    if (freopen(cr_str_get_data(log_path), "w", stdout) != NULL) {
        setbuf(stdout, NULL);
        dup2(STDOUT_FILENO, STDERR_FILENO);
    }
    cr_str_destroy(log_path);

    cr_check(tests_refs_ref_ != NULL, "a capture writes into the reference folder of --ref");
    cr_check(scenes->desc.screen_to_px_from_pt == (float)screen_to_px_from_pt, "the references %s are not for this %gx screen", tests_refs_ref_, screen_to_px_from_pt);

    if (tests_refs_photos_authorized(60.0) == false) {
        printf("capture: no full access to Photos, screenshots cannot be imported\n");
        return;
    }

    struct cr_str* dir = tests_refs_dir();
    cr_util_create_dir(dir);
    cr_str_destroy(dir);
    printf("refs: capturing %s into Documents/refs/%s\n", only != NULL ? only : "all scenes", tests_refs_ref_);

    uint32_t written = 0;
    uint32_t failures = 0;
    uint32_t larger = 0;
    uint32_t consecutive_failures = 0;

    for (uint32_t i = 0; i < ui_test_scene_count_(scenes); i++) {
        const struct ui_test_scene* scene = ui_test_scene_get_(scenes, i);
        if (only != NULL && strcmp(scene->label, only) != 0) {
            continue;
        }
        if (scene->capture == ui_test_scene_capture_screenshot && all_long2((long2)(scene->size <= screen_size)) == false) {
            larger++;
            continue;
        }
        double start_s = cr_get_current_time();
        bool ok = false;
        cr_autoreleasepool {
            ok = tests_refs_capture_scene(scenes, scene);
        }
        printf("capture %s: %s (%.2fs)\n", scene->label, ok ? "ok" : "FAIL", cr_get_current_time() - start_s);

        written += ok ? 1 : 0;
        failures += ok ? 0 : 1;
        consecutive_failures = ok ? 0 : consecutive_failures + 1;
        if (consecutive_failures >= 3) {
            printf("capture: 3 consecutive failures — aborting\n");
            break;
        }
    }

    tests_refs_slider_apply(-1.0f);
    [SwiftUIShapesBridge dismissScene];
    printf("capture: %u written, %u failures, %u larger than the %gx%g pt screen\n", written, failures, larger, screen_size.x, screen_size.y);
    printf("capture: %u screenshots deleted from Photos\n", tests_refs_delete_imported(120.0));
}

#endif
