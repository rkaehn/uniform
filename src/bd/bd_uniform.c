#include "bd_private.h"
#include "bd_xcode.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct cr_str* bd_uniform_path(struct bd* bd, const char* relative) {
    struct cr_str* path = cr_str_create_copy(bd->mem, bd->uniform_path);
    cr_str_append_path_component_cstr(path, relative);
    return path;
}

struct cr_str* bd_uniform_index_html(struct cr_mem* mem, const char* title, const char* main) {
    return cr_str_create_format(mem,
        "<!DOCTYPE html>\n"
        "<html lang=\"en\">\n"
        "<head>\n"
        "<meta charset=\"utf-8\">\n"
        "<meta name=\"color-scheme\" content=\"dark light\">\n"
        "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no\">\n"
        "<link rel=\"icon\" href=\"data:,\">\n"
        "<title>%s</title>\n"
        "<style>\n"
        "html, body { margin: 0px; overflow: hidden; display: block; -webkit-touch-callout: none; -webkit-user-select: none; user-select: none; touch-action: none; -webkit-tap-highlight-color: transparent; }\n"
        "canvas { display: block; }\n"
        "</style>\n"
        "</head>\n"
        "<body>\n"
        "<script src=\"uniform.js\" data-main=\"%s\"></script>\n"
        "</body>\n"
        "</html>\n",
        title, main);
}

static struct cr_str* bd_uniform_runtime_path(struct bd* bd, const char* relative) {
    struct cr_str* path = bd_uniform_path(bd, relative);
    if (!cr_util_does_file_exist(path)) {
        bd_fail("the uniform runtime %s is missing, the release includes it and the repository builds it with make all", cr_str_get_data(path));
    }
    return path;
}

static struct cr_str* bd_uniform_copy_runtime(struct bd* bd, struct cr_str* dir, const char* platform, const char* name, struct cr_str* source) {
    struct cr_str* platform_dir = cr_str_create_copy(bd->mem, dir);
    cr_str_append_path_component_cstr(platform_dir, platform);
    struct cr_str* path = cr_str_create_copy(bd->mem, platform_dir);
    cr_str_append_path_component_cstr(path, name);
    bool ok = cr_util_create_dir(platform_dir) && cr_util_copy(bd->mem, source, path) && cr_util_set_executable(path);
    if (!ok) {
        bd_fail("cannot copy the uniform runtime %s to %s", cr_str_get_data(source), cr_str_get_data(path));
    }
    cr_str_destroy(platform_dir);
    cr_str_destroy(source);
    return path;
}

void bd_add_uniform(struct bd_target* target) {
    struct bd* bd = target->bd;
    struct cr_mem* mem = bd->mem;
    const char* name = cr_str_get_data(target->name);
    cr_check(target->config != NULL, "the target %s has no config, set one with bd_target_set_config before bd_add_uniform", name);

    struct cr_str* dir = cr_str_create_format(mem, "build/temp/uniform/%s", name);
    if (!cr_util_remove(dir)) {
        bd_fail("cannot clear %s", cr_str_get_data(dir));
    }

    struct cr_str* macos_runtime = bd_uniform_copy_runtime(bd, dir, "macos", name, bd_uniform_runtime_path(bd, "bin/macos-release/uniform"));
    struct cr_str* ios_runtime = bd_uniform_copy_runtime(bd, dir, "ios", name, bd_uniform_runtime_path(bd, "bin/ios-release/uniform"));
    struct cr_str* web_dir = bd_uniform_runtime_path(bd, "bin/wasm-release/uniform.js");
    cr_str_remove_last_path_component(web_dir);

    struct cr_str* wasm_dir = cr_str_create_format(mem, "%s/wasm", cr_str_get_data(dir));
    struct cr_str* index_path = cr_str_create_format(mem, "%s/index.html", cr_str_get_data(wasm_dir));
    struct cr_str* index_html = bd_uniform_index_html(mem, name, "main.js");
    bool ok = cr_util_create_dir(wasm_dir) && cr_str_write_to_file(index_html, index_path);
    if (!ok) {
        bd_fail("cannot write the uniform runtime files into %s", cr_str_get_data(dir));
    }

    bd_config_add_string(target->config, bd_platform_macos, bd_config_apple_entitlement_bool, "com.apple.security.network.client=YES");
    bd_config_add_string(target->config, bd_platform_macos, bd_config_apple_entitlement_bool, "com.apple.security.cs.allow-jit=YES");

    bd_target_add_input_ex(target, &(struct bd_input_desc) { .path = cr_str_get_data(macos_runtime), .filter = bd_platform_macos });
    bd_target_add_input_ex(target, &(struct bd_input_desc) { .path = cr_str_get_data(ios_runtime), .filter = bd_platform_ios });
    bd_target_add_input_ex(target, &(struct bd_input_desc) { .path = cr_str_get_data(index_path), .filter = bd_platform_wasm });
    bd_target_add_input_ex(target, &(struct bd_input_desc) { .path = cr_str_get_data(web_dir), .filter = bd_platform_wasm });

    cr_str_destroy(index_html);
    cr_str_destroy(index_path);
    cr_str_destroy(wasm_dir);
    cr_str_destroy(web_dir);
    cr_str_destroy(ios_runtime);
    cr_str_destroy(macos_runtime);
    cr_str_destroy(dir);
}

static uint32_t bd_module_resolve(uint32_t modules) {
    if (modules & bd_module_hud) modules |= bd_module_log | bd_module_ui;
    if (modules & bd_module_app) modules |= bd_module_gpu;
    if (modules & bd_module_ui) modules |= bd_module_tree | bd_module_gpu;
    if (modules & bd_module_bd) modules |= bd_module_prim | bd_module_cl;
    return modules | bd_module_cr;
}

static void bd_lib_add_input(struct bd_target* lib, const char* relative, uint32_t filter, const char* group) {
    struct cr_str* path = bd_uniform_path(lib->bd, relative);
    bd_target_add_input_ex(lib, &(struct bd_input_desc) { .path = cr_str_get_data(path), .filter = filter, .group = group });
    cr_str_destroy(path);
}

static void bd_lib_add_include(struct bd_config* config, uint32_t filter, const char* relative) {
    struct cr_str* path = bd_uniform_path(config->bd, relative);
    bd_config_add_string(config, filter, bd_config_c_include_paths, cr_str_get_data(path));
    cr_str_destroy(path);
}

static struct bd_target* bd_lib_create(struct bd* bd, uint32_t modules) {
    uint32_t gpu_platforms = bd_platform_macos | bd_platform_ios | bd_platform_wasm;
    uint32_t server_platforms = bd_platform_macos | bd_platform_linux;

    struct bd_config* config = bd_config_create_default(bd, NULL);
    bd_config_set_string(config, bd_all, bd_config_c_warnings, "strict");
    bd_config_add_string(config, bd_platform_wasm, bd_config_c_flags, "-mllvm");
    bd_config_add_string(config, bd_platform_wasm, bd_config_c_flags, "-wasm-enable-sjlj");
    bd_config_add_string(config, bd_all, bd_config_c_defines, "log_std=1");
    bd_config_add_string(config, bd_all & ~bd_platform_wasm, bd_config_c_defines, "log_mmap=1");
    bd_config_add_string(config, bd_all & ~(bd_platform_wasm | bd_platform_linux), bd_config_c_defines, "log_signal=1");
    bd_lib_add_include(config, bd_all, "src/shaders");
    bd_lib_add_include(config, bd_platform_macos, "external/freetype-2.14.1/macos-aarch64/include/freetype2");
    bd_lib_add_include(config, bd_platform_macos, "external/harfbuzz-11.5.1/macos-aarch64/include");
    bd_lib_add_include(config, bd_platform_ios, "external/freetype-2.14.1/ios-aarch64/include/freetype2");
    bd_lib_add_include(config, bd_platform_ios, "external/harfbuzz-11.5.1/ios-aarch64/include");
    bd_lib_add_include(config, bd_platform_wasm, "external/freetype-2.14.1/wasm32-wasip1-threads/include/freetype2");
    bd_lib_add_include(config, bd_platform_wasm, "external/harfbuzz-11.5.1/wasm32-wasip1-threads/include");
    bd_lib_add_include(config, server_platforms, "external/picohttpparser");
    bd_lib_add_include(config, bd_platform_macos, "external/bearssl-0.6/macos-aarch64/include");
    bd_lib_add_include(config, bd_platform_linux, "external/bearssl-0.6/linux-x86_64/include");

    struct bd_config* config_pico = bd_config_create_copy(config);
    bd_config_set_string(config_pico, bd_all, bd_config_c_warnings, "none");

    struct bd_target* shaders_embed = NULL;
    if (modules & bd_module_ui) {
        struct bd_target* shaders = bd_target_create(bd, "shaders", bd_target_type_shaders);
        bd_target_set_config(shaders, config);
        bd_lib_add_input(shaders, "src/shaders", bd_all, NULL);

        shaders_embed = bd_target_create(bd, "shaders_embed", bd_target_type_embed);
        bd_target_set_config(shaders_embed, config);
        bd_target_add_input_ex(shaders_embed, &(struct bd_input_desc) { .target = shaders });
    }

    struct bd_target* lib = bd_target_create(bd, "uniform", bd_target_type_library);
    bd_target_set_config(lib, config);
    bd_lib_add_input(lib, "include", bd_all, NULL);
    bd_lib_add_input(lib, "src/cr", bd_all, NULL);
    if (modules & bd_module_prim) {
        bd_lib_add_input(lib, "src/prim", bd_all, NULL);
    }
    if (modules & bd_module_tree) {
        bd_lib_add_input(lib, "src/tree", bd_all, NULL);
    }
    if (modules & bd_module_log) {
        bd_lib_add_input(lib, "src/log", gpu_platforms, NULL);
    }
    if (modules & bd_module_gpu) {
        bd_lib_add_input(lib, "src/gpu", gpu_platforms, NULL);
    }
    if (modules & bd_module_ui) {
        bd_lib_add_input(lib, "src/ui", gpu_platforms, NULL);
        bd_lib_add_input(lib, "external/freetype-2.14.1/macos-aarch64/lib/libfreetype.a", bd_platform_macos, "external");
        bd_lib_add_input(lib, "external/harfbuzz-11.5.1/macos-aarch64/lib/libharfbuzz.a", bd_platform_macos, "external");
        bd_lib_add_input(lib, "external/freetype-2.14.1/ios-aarch64/lib/libfreetype.a", bd_platform_ios, "external");
        bd_lib_add_input(lib, "external/harfbuzz-11.5.1/ios-aarch64/lib/libharfbuzz.a", bd_platform_ios, "external");
        bd_lib_add_input(lib, "external/freetype-2.14.1/wasm32-wasip1-threads/lib/libfreetype.a", bd_platform_wasm, "external");
        bd_lib_add_input(lib, "external/harfbuzz-11.5.1/wasm32-wasip1-threads/lib/libharfbuzz.a", bd_platform_wasm, "external");
        bd_target_add_input_ex(lib, &(struct bd_input_desc) { .target = shaders_embed, .filter = gpu_platforms });
    }
    if (modules & bd_module_hud) {
        bd_lib_add_input(lib, "src/hud", gpu_platforms, NULL);
    }
    if (modules & bd_module_app) {
        bd_lib_add_input(lib, "src/app", gpu_platforms, NULL);
        bd_lib_add_input(lib, "src/web", bd_platform_wasm, NULL);
    }
    if (modules & bd_module_cl) {
        bd_lib_add_input(lib, "src/cl", server_platforms, NULL);
        struct cr_str* pico = bd_uniform_path(bd, "external/picohttpparser/picohttpparser.c");
        bd_target_add_input_ex(lib, &(struct bd_input_desc) { .path = cr_str_get_data(pico), .filter = server_platforms, .group = "external", .overrides.config = config_pico });
        cr_str_destroy(pico);
        bd_lib_add_input(lib, "external/bearssl-0.6/macos-aarch64/lib/libbearssl.a", bd_platform_macos, "external");
        bd_lib_add_input(lib, "external/bearssl-0.6/linux-x86_64/lib/libbearssl.a", bd_platform_linux, "external");
    }
    if (modules & bd_module_bd) {
        bd_lib_add_input(lib, "src/bd", bd_platform_macos, NULL);
    }

    return lib;
}

static void bd_lib_add_prebuilt(struct bd_target* target) {
    struct bd* bd = target->bd;
    struct cr_mem* mem = bd->mem;
    for (uint32_t p = 0; p < bd_platform_count; p++) {
        for (uint32_t c = 0; c < bd_profile_count; c++) {
            uint32_t platform = bd_filter_platform_at_index(p);
            uint32_t profile = bd_filter_profile_at_index(c);

            struct cr_str* dir = cr_str_create_format(mem, "lib/%s-%s", bd_platform_label(platform), bd_profile_label(profile));
            struct cr_str* dir_path = bd_uniform_path(bd, cr_str_get_data(dir));
            enum cr_util_file_type type;
            if (!cr_util_get_file_type(dir_path, &type) || type != cr_util_file_type_directory) {
                cr_str_destroy(dir);
                dir = cr_str_create_format(mem, "lib/%s-release", bd_platform_label(platform));
            }
            cr_str_destroy(dir_path);

            struct cr_str* archive = cr_str_create_format(mem, "%s/libuniform.a", cr_str_get_data(dir));
            bd_lib_add_input(target, cr_str_get_data(archive), platform | profile, "external");
            cr_str_destroy(archive);

            if (platform == bd_platform_wasm) {
                struct cr_str* script = cr_str_create_format(mem, "%s/lib.js", cr_str_get_data(dir));
                bd_lib_add_input(target, cr_str_get_data(script), platform | profile, "external");
                cr_str_destroy(script);
            }
            cr_str_destroy(dir);
        }
    }
}

struct bd_target* bd_add_lib_uniform(struct bd_target* target, uint32_t modules) {
    struct bd* bd = target->bd;

    if (!bd->uniform_has_src) {
        bd_lib_add_prebuilt(target);
        return NULL;
    }

    modules = bd_module_resolve(modules);

    struct bd_target* lib = NULL;
    for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(bd->libs); lib == NULL && i < i_end; i++) {
        struct bd_lib* entry = bd_arr_ptr_get(bd->libs, i);
        if (entry->modules == modules) {
            lib = entry->target;
        }
    }
    if (lib == NULL) {
        lib = bd_lib_create(bd, modules);
        struct bd_lib* entry = cr_malloc(bd->mem, sizeof(*entry), 0, cr_loc);
        entry->modules = modules;
        entry->target = lib;
        bd_arr_ptr_append(bd->libs, entry);
    }

    bd_target_add_input_ex(target, &(struct bd_input_desc) { .target = lib, .xcode_skip = !bd->uniform_is_cwd });
    if (!bd->uniform_is_cwd) {
        struct cr_str* project = bd_uniform_path(bd, "uniform.xcodeproj");
        bd_xcode_add_dependency(bd, target, &(struct bd_xcode_dependency_desc) { .project = cr_str_get_data(project), .target = "uniform" });
        cr_str_destroy(project);
    }

    return lib;
}
