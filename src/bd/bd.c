#include "bd_private.h"
#include <getopt.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char* bd_platform_label(uint32_t platform) {
    switch (platform) {
    case bd_platform_macos: return "macos";
    case bd_platform_ios: return "ios";
    case bd_platform_wasm: return "wasm";
    case bd_platform_linux: return "linux";
    default: assert(0);
    }
}

const char* bd_profile_label(uint32_t profile) {
    switch (profile) {
    case bd_profile_debug: return "debug";
    case bd_profile_release: return "release";
    default: assert(0);
    }
}

const char* bd_target_type_label(enum bd_target_type type) {
    switch (type) {
    case bd_target_type_swift_module: return "Swift module";
    case bd_target_type_library: return "library";
    case bd_target_type_executable: return "executable";
    case bd_target_type_app: return "app";
    case bd_target_type_ui_tests: return "UI tests";
    case bd_target_type_shaders: return "shaders";
    case bd_target_type_aggregate: return "aggregate";
    case bd_target_type_embed: return "embed";
    case bd_target_type_run: return "run";
    default: assert(0);
    }
}

const char* bd_macos_version_label(int version) {
    switch (version) {
    case bd_macos_13_0: return "13.0";
    case bd_macos_14_0: return "14.0";
    case bd_macos_15_0: return "15.0";
    case bd_macos_26_0: return "26.0";
    default: assert(0);
    }
}

const char* bd_ios_version_label(int version) {
    switch (version) {
    case bd_ios_16_0: return "16.0";
    case bd_ios_17_0: return "17.0";
    case bd_ios_18_0: return "18.0";
    case bd_ios_26_0: return "26.0";
    default: assert(0);
    }
}

struct bd_config* bd_config_create(struct bd* bd) {
    struct bd_config* config = cr_malloc(bd->mem, sizeof(*config), 0, cr_loc);

    memset(config, 0, sizeof(*config));
    config->bd = bd;

    bd_arr_ptr_append(bd->configs, config);

    return config;
}

__attribute__((format(printf, 4, 5)))
static void bd_config_add_format(struct bd_config* config, uint32_t filter, enum bd_config_name name, const char* fmt, ...) {
    char text[2048];
    va_list args;
    va_start(args, fmt);
    vsnprintf(text, sizeof(text), fmt, args);
    va_end(args);
    bd_config_add_string(config, filter, name, text);
}

struct bd_config* bd_config_create_default(struct bd* bd, const struct bd_config_desc* desc) {
    struct bd_config* config = bd_config_create(bd);
    const char* uniform = cr_str_get_data(bd->uniform_path);

    bd_config_set_string(config, bd_all, bd_config_c_path, "clang");
    bd_config_set_string(config, bd_platform_wasm, bd_config_c_path, "/opt/homebrew/opt/llvm/bin/clang");
    bd_config_set_string(config, bd_all, bd_config_ar_path, "/opt/homebrew/opt/llvm/bin/llvm-ar");
    bd_config_set_string(config, bd_all, bd_config_brotli_path, "/opt/homebrew/bin/brotli");
    bd_config_set_string(config, bd_all, bd_config_swift_path, "swiftc");
    bd_config_set_string(config, bd_all, bd_config_slang_path, "slangc");

    bd_config_add_string(config, bd_all, bd_config_c_flags, "-fno-objc-arc");
    bd_config_add_string(config, bd_all, bd_config_c_flags, "-fblocks");
    bd_config_add_string(config, bd_all, bd_config_c_flags, "-Wno-c23-extensions");
    bd_config_add_string(config, bd_profile_debug, bd_config_c_defines, "DEBUG");
    bd_config_set_int(config, bd_profile_debug, bd_config_c_optimize, 0);
    bd_config_set_int(config, bd_profile_release, bd_config_c_optimize, 2);
    bd_config_add_string(config, bd_platform_macos | bd_platform_ios | bd_profile_release, bd_config_c_link_flags, "-Wl,-dead_strip");

    bd_config_add_format(config, bd_all, bd_config_c_include_paths, "%s/include", uniform);
    bd_config_add_format(config, bd_all, bd_config_metal_include_paths, "%s/include", uniform);
    bd_config_add_format(config, bd_all, bd_config_slang_flags, "-I%s/include", uniform);
    bd_config_add_string(config, bd_platform_wasm, bd_config_slang_flags, "-Dcr_slang_wgsl=1");
    bd_config_add_string(config, bd_all, bd_config_metal_compiler_flags, "-w");
    bd_config_add_string(config, bd_profile_debug, bd_config_metal_compiler_flags, "-frecord-sources");
    bd_config_add_string(config, bd_profile_debug, bd_config_metal_compiler_flags, "-gline-tables-only");

#if cr_macos
    bd_config_set_string(config, bd_platform_macos, bd_config_c_sysroot, cr_str_get_data(bd->macos_sdk_path));
    bd_config_add_string(config, bd_platform_macos, bd_config_swift_flags, "-sdk");
    bd_config_add_string(config, bd_platform_macos, bd_config_swift_flags, cr_str_get_data(bd->macos_sdk_path));

    bd_config_set_string(config, bd_platform_ios, bd_config_c_sysroot, cr_str_get_data(bd->ios_sdk_path));
    bd_config_add_string(config, bd_platform_ios, bd_config_swift_flags, "-sdk");
    bd_config_add_string(config, bd_platform_ios, bd_config_swift_flags, cr_str_get_data(bd->ios_sdk_path));
    bd_config_add_format(config, bd_platform_ios, bd_config_c_link_flags, "-L%s/lib/darwin", cr_str_get_data(bd->clang_resource_dir));
    bd_config_add_string(config, bd_platform_ios, bd_config_c_link_libs, "clang_rt.ios");
#endif
    bd_config_add_string(config, bd_platform_macos, bd_config_frameworks, "AppKit");
    bd_config_add_string(config, bd_platform_macos, bd_config_frameworks, "Metal");
    bd_config_add_string(config, bd_platform_macos, bd_config_frameworks, "QuartzCore");
    bd_config_set_int(config, bd_platform_macos, bd_config_min_macos_version, bd_macos_15_0);

    bd_config_add_string(config, bd_platform_ios, bd_config_frameworks, "Foundation");
    bd_config_add_string(config, bd_platform_ios, bd_config_frameworks, "CoreGraphics");
    bd_config_add_string(config, bd_platform_ios, bd_config_frameworks, "UIKit");
    bd_config_add_string(config, bd_platform_ios, bd_config_frameworks, "Metal");
    bd_config_add_string(config, bd_platform_ios, bd_config_frameworks, "QuartzCore");
    bd_config_set_int(config, bd_platform_ios, bd_config_min_ios_version, bd_ios_16_0);

    bd_config_add_string(config, bd_platform_linux, bd_config_c_flags, "-target");
    bd_config_add_string(config, bd_platform_linux, bd_config_c_flags, "x86_64-linux-musl");
    bd_config_add_string(config, bd_platform_linux, bd_config_c_flags, "-static");
    bd_config_add_string(config, bd_platform_linux, bd_config_c_flags, "--ld-path=/opt/homebrew/opt/lld/bin/ld.lld");
    bd_config_add_string(config, bd_platform_linux, bd_config_c_flags, "--sysroot");
    bd_config_add_format(config, bd_platform_linux, bd_config_c_flags, "%s/external/musl-1.2.5/linux-x86_64", uniform);
    bd_config_add_string(config, bd_platform_linux, bd_config_c_flags, "-nostdlib");
    bd_config_add_format(config, bd_platform_linux, bd_config_c_link_flags, "-Wl,%s/external/musl-1.2.5/linux-x86_64/lib/crt1.o", uniform);
    bd_config_add_format(config, bd_platform_linux, bd_config_c_link_flags, "-Wl,%s/external/musl-1.2.5/linux-x86_64/lib/crti.o", uniform);
    bd_config_add_format(config, bd_platform_linux, bd_config_c_link_flags, "-Wl,%s/external/musl-1.2.5/linux-x86_64/lib/crtn.o", uniform);
    bd_config_add_string(config, bd_platform_linux, bd_config_c_link_libs, "c");

    bd_config_add_string(config, bd_platform_wasm, bd_config_c_flags, "--target=wasm32-wasip1-threads");
    bd_config_add_string(config, bd_platform_wasm, bd_config_c_flags, "-pthread");
    bd_config_add_string(config, bd_platform_wasm, bd_config_c_flags, "-msimd128");
    bd_config_add_string(config, bd_platform_wasm, bd_config_c_flags, "--no-default-config");
    bd_config_add_format(config, bd_platform_wasm, bd_config_c_flags, "--sysroot=%s/external/wasi-libc-27.0/wasm32-wasip1-threads", uniform);
    bd_config_add_string(config, bd_platform_wasm, bd_config_c_flags, "-nostdlib");
    bd_config_add_format(config, bd_platform_wasm, bd_config_c_link_flags, "-Wl,%s/external/wasi-libc-27.0/wasm32-wasip1-threads/lib/crt1-reactor.o", uniform);
    bd_config_add_string(config, bd_platform_wasm, bd_config_c_link_flags, "-Wl,--entry=_initialize");
    bd_config_add_format(config, bd_platform_wasm, bd_config_c_link_flags, "-L%s/external/wasi-libc-27.0/wasm32-wasip1-threads/lib", uniform);
    bd_config_add_format(config, bd_platform_wasm, bd_config_c_link_flags, "-L%s/external/compiler-rt-22.1.8/wasm32-wasip1-threads/lib", uniform);
    bd_config_add_string(config, bd_platform_wasm, bd_config_c_link_flags, "-Wl,--import-memory");
    bd_config_add_string(config, bd_platform_wasm, bd_config_c_link_flags, "-Wl,--export-memory");
    bd_config_add_string(config, bd_platform_wasm, bd_config_c_link_flags, "-Wl,--shared-memory");
    bd_config_add_string(config, bd_platform_wasm, bd_config_c_link_flags, "-Wl,--export-table");
    bd_config_add_string(config, bd_platform_wasm, bd_config_c_link_flags, "-Wl,--export-if-defined=__main_argc_argv");
    bd_config_add_string(config, bd_platform_wasm, bd_config_c_link_flags, "-Wl,--export=malloc");
    bd_config_add_string(config, bd_platform_wasm, bd_config_c_link_flags, "-Wl,--export=free");
    bd_config_add_string(config, bd_platform_wasm, bd_config_c_link_flags, "-Wl,--initial-memory=16777216");
    bd_config_add_string(config, bd_platform_wasm, bd_config_c_link_flags, "-Wl,--max-memory=2147483648");
    bd_config_add_string(config, bd_platform_wasm, bd_config_c_link_flags, "-Wl,-z,stack-size=1048576");
    bd_config_add_string(config, bd_platform_wasm, bd_config_c_link_libs, "c");
    bd_config_add_string(config, bd_platform_wasm, bd_config_c_link_libs, "pthread");
    bd_config_add_string(config, bd_platform_wasm, bd_config_c_link_libs, "setjmp");
    bd_config_add_string(config, bd_platform_wasm, bd_config_c_link_libs, "clang_rt.builtins");

    bd_config_set_int(config, bd_all, bd_config_run_port, 9443);
    bd_config_set_string(config, bd_platform_ios, bd_config_export, "app_store");
    bd_config_set_string(config, bd_platform_macos, bd_config_export, "online");
    bd_config_set_string(config, bd_platform_macos, bd_config_apple_installer_identity, "Mac Installer Distribution");
    bd_config_add_string(config, bd_all, bd_config_xcode_settings, "EXCLUDED_ARCHS=x86_64");

    if (desc != NULL && desc->team_identifier != NULL) {
        bd_config_set_string(config, bd_all, bd_config_apple_team_identifier, desc->team_identifier);
    }
    if (desc != NULL && desc->bundle_identifier != NULL) {
        bd_config_set_string(config, bd_all, bd_config_apple_bundle_identifier, desc->bundle_identifier);
        bd_config_add_string(config, bd_all, bd_config_xcode_settings, "ENABLE_APP_SANDBOX=YES");
        bd_config_add_string(config, bd_all, bd_config_xcode_settings, "ENABLE_USER_SELECTED_FILES=readwrite");
        bd_config_add_string(config, bd_all, bd_config_xcode_settings, "ENABLE_FILE_ACCESS_DOWNLOADS_FOLDER=readwrite");
        bd_config_add_string(config, bd_all, bd_config_xcode_settings, "ENABLE_OUTGOING_NETWORK_CONNECTIONS=YES");
        bd_config_add_string(config, bd_all, bd_config_xcode_settings, "SUPPORTS_MAC_DESIGNED_FOR_IPHONE_IPAD=YES");
        bd_config_add_string(config, bd_all, bd_config_xcode_settings, "CURRENT_PROJECT_VERSION=1");
        bd_config_add_string(config, bd_all, bd_config_xcode_settings, "MARKETING_VERSION=1.0");
        bd_config_add_string(config, bd_all, bd_config_apple_info_plist_bool, "ITSAppUsesNonExemptEncryption=NO");
        bd_config_add_string(config, bd_platform_ios, bd_config_apple_info_plist_array, "UISupportedInterfaceOrientations=UIInterfaceOrientationPortrait");
        bd_config_add_string(config, bd_platform_ios, bd_config_apple_info_plist_array, "UISupportedInterfaceOrientations~ipad=UIInterfaceOrientationLandscapeLeft UIInterfaceOrientationLandscapeRight UIInterfaceOrientationPortrait UIInterfaceOrientationPortraitUpsideDown");
        bd_config_add_string(config, bd_platform_ios, bd_config_apple_info_plist_bool, "UIStatusBarHidden=YES");
        bd_config_add_string(config, bd_platform_ios, bd_config_apple_info_plist_bool, "CADisableMinimumFrameDurationOnPhone=YES");
        bd_config_set_string(config, bd_platform_ios, bd_config_apple_scene_delegate, "AppSceneDelegate");
        bd_config_set_string(config, bd_platform_ios, bd_config_apple_signing_identity, "Apple Development");
        bd_config_set_string(config, bd_platform_ios, bd_config_apple_provisioning_profile, "iOS Team Provisioning Profile: *");
        char profile[512];
        snprintf(profile, sizeof(profile), "iOS Team Store Provisioning Profile: %s", desc->bundle_identifier);
        bd_config_set_string(config, bd_platform_ios, bd_config_apple_distribution_profile, profile);
    }

    return config;
}

struct bd_config* bd_config_create_copy(struct bd_config* original) {
    struct bd_config* config = bd_config_create(original->bd);

    for (uint32_t i = 0; i < bd_config_name_count; i++) {
        for (uint32_t c = 0; c < bd_profile_count; c++) {
            for (uint32_t p = 0; p < bd_platform_count; p++) {
                if (original->values[i][c][p] != NULL) {
                    config->values[i][c][p] = prim_create_copy(config->bd->mem, original->values[i][c][p]);
                }
            }
        }
    }

    return config;
}

static void bd_config_destroy(struct bd_config* config) {
    for (uint32_t i = 0; i < bd_config_name_count; i++) {
        for (uint32_t c = 0; c < bd_profile_count; c++) {
            for (uint32_t p = 0; p < bd_platform_count; p++) {
                if (config->values[i][c][p] != NULL) {
                    prim_destroy(config->values[i][c][p]);
                }
            }
        }
    }
    cr_free(config->bd->mem, config, cr_loc);
}

void bd_config_set(struct bd_config* config, uint32_t filter, enum bd_config_name name, struct prim* set_value) {
    filter = bd_filter_fix(filter);

    for (uint32_t c = 0; c < bd_profile_count; c++) {
        if ((filter & bd_filter_profile_at_index(c)) == 0) continue;

        for (uint32_t p = 0; p < bd_platform_count; p++) {
            if ((filter & bd_filter_platform_at_index(p)) == 0) continue;

            if (config->values[name][c][p] != NULL) {
                prim_destroy(config->values[name][c][p]);
            }
            config->values[name][c][p] = prim_create_copy(config->bd->mem, set_value);
        }
    }
}

void bd_config_set_null(struct bd_config* config, uint32_t filter, enum bd_config_name name) {
    struct prim* prim = prim_create_null(config->bd->mem);
    bd_config_set(config, filter, name, prim);
    prim_destroy(prim);
}

void bd_config_set_false(struct bd_config* config, uint32_t filter, enum bd_config_name name) {
    struct prim* prim = prim_create_false(config->bd->mem);
    bd_config_set(config, filter, name, prim);
    prim_destroy(prim);
}

void bd_config_set_true(struct bd_config* config, uint32_t filter, enum bd_config_name name) {
    struct prim* prim = prim_create_true(config->bd->mem);
    bd_config_set(config, filter, name, prim);
    prim_destroy(prim);
}

void bd_config_set_number(struct bd_config* config, uint32_t filter, enum bd_config_name name, double number) {
    struct prim* prim = prim_create_number(config->bd->mem, number);
    bd_config_set(config, filter, name, prim);
    prim_destroy(prim);
}

void bd_config_set_int(struct bd_config* config, uint32_t filter, enum bd_config_name name, int number) {
    struct prim* prim = prim_create_number(config->bd->mem, number);
    bd_config_set(config, filter, name, prim);
    prim_destroy(prim);
}

void bd_config_set_string(struct bd_config* config, uint32_t filter, enum bd_config_name name, const char* string) {
    struct prim* prim = prim_create_string(config->bd->mem, string);
    bd_config_set(config, filter, name, prim);
    prim_destroy(prim);
}

static void bd_config_add(struct bd_config* config, uint32_t filter, enum bd_config_name name, struct prim* item) {
    filter = bd_filter_fix(filter);

    for (uint32_t c = 0; c < bd_profile_count; c++) {
        if ((filter & bd_filter_profile_at_index(c)) == 0) continue;

        for (uint32_t p = 0; p < bd_platform_count; p++) {
            if ((filter & bd_filter_platform_at_index(p)) == 0) continue;

            if (config->values[name][c][p] == NULL) {
                config->values[name][c][p] = prim_create_array(config->bd->mem);
            }
            prim_array_append(config->values[name][c][p], prim_create_copy(config->bd->mem, item));
        }
    }
}

void bd_config_add_string(struct bd_config* config, uint32_t filter, enum bd_config_name name, const char* string) {
    struct prim* item = prim_create_string(config->bd->mem, string);
    bd_config_add(config, filter, name, item);
    prim_destroy(item);
}

void bd_config_add_products_dir(struct bd_config* config, uint32_t filter, enum bd_config_name name) {
    struct prim* item = prim_create_object(config->bd->mem);
    prim_object_set(item, "products_dir", prim_create_null(config->bd->mem));
    bd_config_add(config, filter, name, item);
    prim_destroy(item);
}

void bd_config_add_certificate_path(struct bd_config* config, uint32_t filter, enum bd_config_name name) {
    struct prim* item = prim_create_object(config->bd->mem);
    prim_object_set(item, "certificate_path", prim_create_null(config->bd->mem));
    bd_config_add(config, filter, name, item);
    prim_destroy(item);
}

void bd_config_add_private_key_path(struct bd_config* config, uint32_t filter, enum bd_config_name name) {
    struct prim* item = prim_create_object(config->bd->mem);
    prim_object_set(item, "private_key_path", prim_create_null(config->bd->mem));
    bd_config_add(config, filter, name, item);
    prim_destroy(item);
}

void bd_config_add_c_args(struct bd_config* config, uint32_t filter, enum bd_config_name name, uint32_t args_filter) {
    args_filter = bd_filter_platform(args_filter) | bd_filter_profile(args_filter);
    struct prim* item = prim_create_object(config->bd->mem);
    prim_object_set(item, "c_args", prim_create_number(config->bd->mem, (double)args_filter));
    bd_config_add(config, filter, name, item);
    prim_destroy(item);
}

void bd_config_add_path(struct bd_config* config, uint32_t filter, enum bd_config_name name, const char* path) {
    struct prim* item = prim_create_object(config->bd->mem);
    prim_object_set(item, "path", prim_create_string(config->bd->mem, path));
    bd_config_add(config, filter, name, item);
    prim_destroy(item);
}

struct prim* bd_config_get(struct bd_config* config, uint32_t filter, enum bd_config_name name) {
    uint32_t profile = bd_filter_profile(filter);
    uint32_t platform = bd_filter_platform(filter);
    return config->values[name][ctz_uint32(profile) - bd_platform_count][ctz_uint32(platform)];
}

bool bd_config_has_value(struct bd_config* config, uint32_t filter, enum bd_config_name name) {
    return bd_config_get(config, filter, name) != NULL;
}

bool bd_config_is_null(struct bd_config* config, uint32_t filter, enum bd_config_name name) {
    return prim_is_null(bd_config_get(config, filter, name));
}

bool bd_config_is_false(struct bd_config* config, uint32_t filter, enum bd_config_name name) {
    return prim_is_false(bd_config_get(config, filter, name));
}

bool bd_config_is_true(struct bd_config* config, uint32_t filter, enum bd_config_name name) {
    return prim_is_true(bd_config_get(config, filter, name));
}

double bd_config_get_number(struct bd_config* config, uint32_t filter, enum bd_config_name name) {
    return prim_number_get(bd_config_get(config, filter, name));
}

int bd_config_get_int(struct bd_config* config, uint32_t filter, enum bd_config_name name) {
    return (int)prim_number_get(bd_config_get(config, filter, name));
}

const char* bd_config_get_string(struct bd_config* config, uint32_t filter, enum bd_config_name name) {
    return prim_string_get(bd_config_get(config, filter, name));
}

uint32_t bd_config_get_array_count(struct bd_config* config, uint32_t filter, enum bd_config_name name) {
    return prim_array_get_count(bd_config_get(config, filter, name));
}

const char* bd_config_get_array_string(struct bd_config* config, uint32_t filter, enum bd_config_name name, uint32_t index) {
    return prim_string_get(prim_array_get(bd_config_get(config, filter, name), index));
}

void bd_print_args(struct bd_arr_ptr* args, struct cr_str* output) {
    for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(args); i < i_end; i++) {
        struct cr_str* arg = bd_arr_ptr_get(args, i);
        cr_str_append(output, arg);
        cr_str_append_data(output, (i < bd_arr_ptr_get_count(args) - 1) ? " " : "\n", 1);
    }
}

void bd_fail(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "bd: ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    exit(1);
}

struct bd_arr_ptr* bd_args_create(struct cr_mem* mem, ...) {
    struct bd_arr_ptr* args = bd_arr_ptr_create(mem);
    va_list list;
    va_start(list, mem);
    for (const char* arg = va_arg(list, const char*); arg != NULL; arg = va_arg(list, const char*)) {
        bd_arr_ptr_append(args, cr_str_create(mem, arg));
    }
    va_end(list);
    return args;
}

void bd_args_destroy(struct bd_arr_ptr* args) {
    for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(args); i < i_end; i++) {
        cr_str_destroy(bd_arr_ptr_get(args, i));
    }
    bd_arr_ptr_destroy(args);
}

int bd_exec(struct bd_arr_ptr* args, struct cr_str* output) {
    int exit_code = cr_exec((struct cr_str**)bd_arr_ptr_get_data(args), bd_arr_ptr_get_count(args), output, output);
    bd_args_destroy(args);
    return exit_code;
}

int bd_exec_inherit(struct bd_arr_ptr* args) {
    for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(args); i < i_end; i++) {
        printf("%s%s", i > 0 ? " " : "", cr_str_get_data(bd_arr_ptr_get(args, i)));
    }
    printf("\n");
    fflush(stdout);
    void (*interrupt_handler)(int) = signal(SIGINT, SIG_IGN);
    void (*quit_handler)(int) = signal(SIGQUIT, SIG_IGN);
    int exit_code = cr_exec((struct cr_str**)bd_arr_ptr_get_data(args), bd_arr_ptr_get_count(args), NULL, NULL);
    signal(SIGINT, interrupt_handler);
    signal(SIGQUIT, quit_handler);
    bd_args_destroy(args);
    return exit_code < 0 ? 1 : exit_code;
}

struct cr_str* bd_args_join(struct cr_mem* mem, struct bd_arr_ptr* args) {
    struct cr_str* line = cr_str_create_empty(mem);
    bd_print_args(args, line);
    bd_args_destroy(args);
    return line;
}

struct cr_str* bd_path_resolve(struct bd* bd, struct cr_str* raw) {
    if (cr_str_is_absolute_path(raw)) {
        return cr_str_create_copy(bd->mem, raw);
    }

    struct cr_str* path = cr_str_create_copy(bd->mem, bd->cwd_path);
    cr_str_append_path_component(path, raw);

    return path;
}

void bd_c_flags_args(struct cr_mem* mem, struct bd_config* config, uint32_t filter, struct bd_arr_ptr* args) {
    if ((bd_filter_platform(filter) & bd_platform_ios) &&
        bd_config_has_value(config, filter, bd_config_min_ios_version)) {
        int min_version = bd_config_get_int(config, filter, bd_config_min_ios_version);
        bd_arr_ptr_append(args, cr_str_create_format(mem, "-miphoneos-version-min=%s", bd_ios_version_label(min_version)));
    }

    if ((bd_filter_platform(filter) & bd_platform_macos) &&
        bd_config_has_value(config, filter, bd_config_min_macos_version)) {
        int min_version = bd_config_get_int(config, filter, bd_config_min_macos_version);
        bd_arr_ptr_append(args, cr_str_create_format(mem, "-mmacosx-version-min=%s", bd_macos_version_label(min_version)));
    }

    if (bd_config_has_value(config, filter, bd_config_c_sysroot)) {
        bd_arr_ptr_append(args, cr_str_create(mem, "-isysroot"));
        bd_arr_ptr_append(args, cr_str_create(mem, bd_config_get_string(config, filter, bd_config_c_sysroot)));
    }

    if (bd_config_has_value(config, filter, bd_config_c_optimize)) {
        bd_arr_ptr_append(args, cr_str_create_format(mem, "-O%d", bd_config_get_int(config, filter, bd_config_c_optimize)));
    }

    if (bd_config_has_value(config, filter, bd_config_c_include_paths)) {
        uint32_t path_count = bd_config_get_array_count(config, filter, bd_config_c_include_paths);

        for (uint32_t i = 0; i < path_count; i++) {
            const char* path = bd_config_get_array_string(config, filter, bd_config_c_include_paths, i);
            bd_arr_ptr_append(args, cr_str_create_format(mem, "-I%s", path));
        }
    }

    if (bd_config_has_value(config, filter, bd_config_c_defines)) {
        uint32_t define_count = bd_config_get_array_count(config, filter, bd_config_c_defines);

        for (uint32_t i = 0; i < define_count; i++) {
            const char* define = bd_config_get_array_string(config, filter, bd_config_c_defines, i);
            bd_arr_ptr_append(args, cr_str_create_format(mem, "-D%s", define));
        }
    }

    if (bd_config_has_value(config, filter, bd_config_c_flags)) {
        uint32_t flag_count = bd_config_get_array_count(config, filter, bd_config_c_flags);

        for (uint32_t i = 0; i < flag_count; i++) {
            const char* flag = bd_config_get_array_string(config, filter, bd_config_c_flags, i);
            bd_arr_ptr_append(args, cr_str_create(mem, flag));
        }
    }

    if ((bd_filter_platform(filter) & (bd_platform_linux | bd_platform_wasm)) == 0 &&
        bd_config_has_value(config, filter, bd_config_c_asan) &&
        bd_config_is_true(config, filter, bd_config_c_asan)) {
        bd_arr_ptr_append(args, cr_str_create(mem, "-fsanitize=address"));
    }

    if ((bd_filter_platform(filter) & (bd_platform_linux | bd_platform_wasm)) == 0 &&
        bd_config_has_value(config, filter, bd_config_c_tsan) &&
        bd_config_is_true(config, filter, bd_config_c_tsan)) {
        bd_arr_ptr_append(args, cr_str_create(mem, "-fsanitize=thread"));
    }

    if (bd_config_has_value(config, filter, bd_config_c_link_flags)) {
        uint32_t flag_count = bd_config_get_array_count(config, filter, bd_config_c_link_flags);

        for (uint32_t i = 0; i < flag_count; i++) {
            const char* flag = bd_config_get_array_string(config, filter, bd_config_c_link_flags, i);
            bd_arr_ptr_append(args, cr_str_create(mem, flag));
        }
    }

    if (bd_config_has_value(config, filter, bd_config_c_link_libs)) {
        uint32_t lib_count = bd_config_get_array_count(config, filter, bd_config_c_link_libs);

        for (uint32_t i = 0; i < lib_count; i++) {
            const char* lib = bd_config_get_array_string(config, filter, bd_config_c_link_libs, i);
            bd_arr_ptr_append(args, cr_str_create_format(mem, "-l%s", lib));
        }
    }

    if (bd_config_has_value(config, filter, bd_config_frameworks)) {
        uint32_t framework_count = bd_config_get_array_count(config, filter, bd_config_frameworks);

        for (uint32_t i = 0; i < framework_count; i++) {
            const char* framework = bd_config_get_array_string(config, filter, bd_config_frameworks, i);
            bd_arr_ptr_append(args, cr_str_create(mem, "-framework"));
            bd_arr_ptr_append(args, cr_str_create(mem, framework));
        }
    }
}

void bd_c_args(struct cr_mem* mem, struct bd_config* config, uint32_t filter, struct bd_arr_ptr* args) {
    const char* compiler_path = bd_config_get_string(config, filter, bd_config_c_path);
    bd_arr_ptr_append(args, cr_str_create(mem, compiler_path));

    bd_c_flags_args(mem, config, filter, args);

    bd_arr_ptr_append(args, cr_str_create(mem, "-Wno-unused-command-line-argument"));

    if (bd_config_has_value(config, filter, bd_config_c_warnings)) {
        const char* warnings = bd_config_get_string(config, filter, bd_config_c_warnings);
        if (strcmp(warnings, "none") == 0) {
            bd_arr_ptr_append(args, cr_str_create(mem, "-w"));
        } else if (strcmp(warnings, "strict") == 0) {
            bd_arr_ptr_append(args, cr_str_create(mem, "-Werror"));
            bd_arr_ptr_append(args, cr_str_create(mem, "-Wall"));
            bd_arr_ptr_append(args, cr_str_create(mem, "-Wextra"));
            bd_arr_ptr_append(args, cr_str_create(mem, "-Wshadow"));
            bd_arr_ptr_append(args, cr_str_create(mem, "-Wsign-conversion"));
            bd_arr_ptr_append(args, cr_str_create(mem, "-Wshorten-64-to-32"));
            bd_arr_ptr_append(args, cr_str_create(mem, "-Wimplicit-float-conversion"));
            bd_arr_ptr_append(args, cr_str_create(mem, "-Wmissing-prototypes"));
            bd_arr_ptr_append(args, cr_str_create(mem, "-Wno-unused-parameter"));
            bd_arr_ptr_append(args, cr_str_create(mem, "-Wno-unused-variable"));
            bd_arr_ptr_append(args, cr_str_create(mem, "-Wno-unused-but-set-variable"));
            bd_arr_ptr_append(args, cr_str_create(mem, "-Wno-unused-function"));
            bd_arr_ptr_append(args, cr_str_create(mem, "-Wno-missing-designated-field-initializers"));
        } else {
            bd_fail("bd_config_c_warnings must be none or strict, not %s", warnings);
        }
    }
}

bool bd_config_is_string(struct bd_config* config, uint32_t filter, enum bd_config_name name, const char* value) {
    struct prim* prim = bd_config_get(config, filter, name);
    return prim != NULL && prim_is_string(prim) && strcmp(prim_string_get(prim), value) == 0;
}

static struct cr_str* bd_dir_path(struct bd* bd, struct cr_str* base, struct bd_target* target, uint32_t filter) {
    struct cr_str* path = cr_str_create_copy(bd->mem, base);
    cr_str_append_path_component_format(path, "%s-%s-%s", cr_str_get_data(target->name), bd_platform_label(bd_filter_platform(filter)), bd_profile_label(bd_filter_profile(filter)));
    return path;
}

struct cr_str* bd_products_path(struct bd* bd, struct bd_target* target, uint32_t filter) {
    return bd_dir_path(bd, bd->products_path, target, filter);
}

struct cr_str* bd_temp_path(struct bd* bd, struct bd_target* target, uint32_t filter) {
    return bd_dir_path(bd, bd->temp_path, target, filter);
}

bool bd_target_is_program(struct bd_target* target) {
    return target->type == bd_target_type_app || target->type == bd_target_type_executable;
}

const char* bd_executable_name(struct bd_target* target, uint32_t filter) {
    if (bd_config_has_value(target->config, filter, bd_config_executable_name)) {
        return bd_config_get_string(target->config, filter, bd_config_executable_name);
    }
    return cr_str_get_data(target->name);
}

struct cr_str* bd_app_path(struct bd* bd, struct bd_target* target, uint32_t filter) {
    struct cr_str* path = bd_products_path(bd, target, filter);
    cr_str_append_path_component_format(path, "%s.app", cr_str_get_data(target->name));
    return path;
}

struct cr_str* bd_program_path(struct bd* bd, struct bd_target* target, uint32_t filter) {
    uint32_t platform = bd_filter_platform(filter);
    struct cr_str* path;
    if (target->type == bd_target_type_app && (platform == bd_platform_macos || platform == bd_platform_ios)) {
        path = bd_app_path(bd, target, filter);
        if (platform == bd_platform_macos) {
            cr_str_append_path_component_cstr(path, "Contents/MacOS");
        }
    } else {
        path = bd_products_path(bd, target, filter);
    }
    cr_str_append_path_component_cstr(path, bd_executable_name(target, filter));
    return path;
}

static struct cr_str* bd_config_certificate_file(struct cr_mem* mem, const char* file) {
    struct cr_str* path = bd_cert_dir(mem);
    cr_str_append_path_component_cstr(path, file);
    return path;
}

void bd_config_args_expand(struct bd* bd, struct bd_config* config, uint32_t filter, enum bd_config_name name, struct cr_str* products_path, struct bd_arr_ptr* args) {
    struct cr_mem* mem = bd->mem;
    struct prim* items = bd_config_get(config, filter, name);
    for (uint32_t i = 0, i_end = items ? prim_array_get_count(items) : 0; i < i_end; i++) {
        struct prim* item = prim_array_get(items, i);
        if (prim_is_string(item)) {
            bd_arr_ptr_append(args, cr_str_create(mem, prim_string_get(item)));
        } else if (prim_is_object(item) && prim_object_get(item, "path") != NULL) {
            struct cr_str* raw = cr_str_create(mem, prim_string_get(prim_object_get(item, "path")));
            bd_arr_ptr_append(args, bd_path_resolve(bd, raw));
            cr_str_destroy(raw);
        } else if (prim_is_object(item) && prim_object_get(item, "products_dir") != NULL) {
            bd_arr_ptr_append(args, cr_str_create_copy(mem, products_path));
        } else if (prim_is_object(item) && prim_object_get(item, "certificate_path") != NULL) {
            bd_arr_ptr_append(args, bd_config_certificate_file(mem, "certificate.pem"));
        } else if (prim_is_object(item) && prim_object_get(item, "private_key_path") != NULL) {
            bd_arr_ptr_append(args, bd_config_certificate_file(mem, "private_key.pem"));
        } else if (prim_is_object(item) && prim_object_get(item, "c_args") != NULL) {
            bd_c_args(mem, config, (uint32_t)prim_number_get(prim_object_get(item, "c_args")), args);
        } else {
            struct cr_str* text = prim_as_str(mem, item, false);
            bd_fail("unknown argument item %s", cr_str_get_data(text));
        }
    }
}

const char* bd_config_require_string(struct bd_config* config, uint32_t filter, enum bd_config_name name, const char* message) {
    if (!bd_config_has_value(config, filter, name)) {
        bd_fail("%s", message);
    }
    return bd_config_get_string(config, filter, name);
}

bool bd_export_is_app_store(struct bd_config* config, uint32_t filter) {
    const char* value = bd_config_require_string(config, filter, bd_config_export, "an export needs bd_config_export, app_store or online");
    if (strcmp(value, "app_store") == 0) {
        return true;
    }
    if (strcmp(value, "online") != 0) {
        bd_fail("bd_config_export must be app_store or online, not %s", value);
    }
    return false;
}

const char* bd_export_identity_name(struct bd_config* config, uint32_t filter) {
    if (bd_config_has_value(config, filter, bd_config_apple_distribution_identity)) {
        return bd_config_get_string(config, filter, bd_config_apple_distribution_identity);
    }
    return bd_export_is_app_store(config, filter) ? "Apple Distribution" : "Developer ID Application";
}

static struct _bd_input_desc* bd_input_desc_create(struct bd* bd, const struct bd_input_desc* input_desc) {
    struct _bd_input_desc* desc = cr_malloc(bd->mem, sizeof(*desc), 0, cr_loc);

    memset(desc, 0, sizeof(*desc));
    desc->bd = bd;
    desc->target = input_desc->target;
    if (input_desc->path) {
        desc->path = cr_str_create(bd->mem, input_desc->path);
    }
    if (input_desc->name) {
        desc->name = cr_str_create(bd->mem, input_desc->name);
    }
    desc->filter = bd_filter_fix(input_desc->filter);
    desc->is_indirect = input_desc->is_indirect;
    desc->xcode_skip = input_desc->xcode_skip;
    if (input_desc->group) {
        desc->group = cr_str_create(bd->mem, input_desc->group);
    }
    if (input_desc->overrides.config) {
        desc->overrides.config = bd_config_create_copy(input_desc->overrides.config);
    }
    desc->overrides.platform = input_desc->overrides.platform;
    desc->overrides.profile = input_desc->overrides.profile;

    return desc;
}

static void bd_input_desc_destroy(struct _bd_input_desc* desc) {
    if (desc->path) {
        cr_str_destroy(desc->path);
    }
    if (desc->name) {
        cr_str_destroy(desc->name);
    }
    if (desc->group) {
        cr_str_destroy(desc->group);
    }
    cr_free(desc->bd->mem, desc, cr_loc);
}

struct bd_target* bd_target_create(struct bd* bd, const char* name, enum bd_target_type type) {
    struct bd_target* target = cr_malloc(bd->mem, sizeof(*target), 0, cr_loc);

    memset(target, 0, sizeof(*target));
    target->bd = bd;
    target->name = cr_str_create(bd->mem, name);
    target->type = type;
    target->inputs = bd_arr_ptr_create(bd->mem);

    bd_arr_ptr_append(bd->targets, target);

    return target;
}

static void bd_target_destroy(struct bd_target* target) {
    cr_str_destroy(target->name);
    for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(target->inputs); i < i_end; i++) {
        bd_input_desc_destroy(bd_arr_ptr_get(target->inputs, i));
    }
    bd_arr_ptr_destroy(target->inputs);
    cr_free(target->bd->mem, target, cr_loc);
}

void bd_target_set_config(struct bd_target* target, struct bd_config* config) {
    target->config = bd_config_create_copy(config);
}

struct bd_config* bd_target_get_config(struct bd_target* target) {
    cr_check(target->config != NULL, "the target %s has no config, set one with bd_target_set_config first", cr_str_get_data(target->name));
    return target->config;
}

void bd_target_add_input_ex(struct bd_target* target, const struct bd_input_desc* desc) {
    bd_arr_ptr_append(target->inputs, bd_input_desc_create(target->bd, desc));
}

void bd_target_add_input(struct bd_target* target, const char* path) {
    bd_target_add_input_ex(target, &(struct bd_input_desc) {
        .path = path,
    });
}

#if cr_macos
static struct cr_str* bd_exec_line(struct cr_mem* mem, struct bd_arr_ptr* args) {
    struct cr_str* line = cr_str_create_empty(mem);
    bd_exec(args, line);
    cr_str_remove_last(line, 1);
    return line;
}
#endif

struct bd* bd_create(struct cr_mem* mem, const char* uniform_path) {
    cr_check(uniform_path != NULL, "bd_create needs the path to uniform, the repository or the unpacked release");

    struct bd* bd = cr_malloc(mem, sizeof(*bd), 0, cr_loc);

    memset(bd, 0, sizeof(*bd));
    bd->mem = mem;
    bd->exec = cr_executor_create(mem, (int)cr_get_hardware_concurrency());
    bd->targets = bd_arr_ptr_create(mem);
    bd->configs = bd_arr_ptr_create(mem);
    bd->libs = bd_arr_ptr_create(mem);

    struct cr_str* cwd = cr_util_get_working_dir(mem);
    bd->cwd_path = cr_util_get_real_path(mem, cwd);
    cr_str_destroy(cwd);
    assert(bd->cwd_path);

    uint8_t raw[cr_str_size];
    bd->uniform_path = cr_util_get_real_path(mem, cr_str_temp(raw, (char*)uniform_path));
    if (bd->uniform_path == NULL) {
        bd_fail("the uniform path %s does not exist", uniform_path);
    }
    struct cr_str* header = bd_uniform_path(bd, "include/bd.h");
    if (!cr_util_does_file_exist(header)) {
        bd_fail("%s is neither the uniform repository nor an unpacked release, it has no include/bd.h", cr_str_get_data(bd->uniform_path));
    }
    cr_str_destroy(header);
    struct cr_str* src = bd_uniform_path(bd, "src");
    enum cr_util_file_type src_type;
    bd->uniform_has_src = cr_util_get_file_type(src, &src_type) && src_type == cr_util_file_type_directory;
    bd->uniform_is_cwd = cr_str_is_equal(bd->uniform_path, bd->cwd_path);
    cr_str_destroy(src);
#if cr_macos
    bd->macos_sdk_path = bd_exec_line(mem, bd_args_create(mem, "xcrun", "--sdk", "macosx", "--show-sdk-path", NULL));
    bd->ios_sdk_path = bd_exec_line(mem, bd_args_create(mem, "xcrun", "--sdk", "iphoneos", "--show-sdk-path", NULL));
    bd->clang_resource_dir = bd_exec_line(mem, bd_args_create(mem, "xcrun", "clang", "-print-resource-dir", NULL));
#endif

    return bd;
}

static void bd_destroy(struct bd* bd) {
    if (bd->device) {
        cr_str_destroy(bd->device);
    }
    if (bd->team) {
        cr_str_destroy(bd->team);
    }
    cr_str_destroy(bd->uniform_path);
    cr_str_destroy(bd->cwd_path);
    cr_str_destroy(bd->output_path);
    cr_str_destroy(bd->temp_path);
    cr_str_destroy(bd->products_path);
#if cr_macos
    cr_str_destroy(bd->macos_sdk_path);
    cr_str_destroy(bd->ios_sdk_path);
    cr_str_destroy(bd->clang_resource_dir);
#endif

    if (bd->xcode) {
        bd_xcode_destroy(bd->xcode);
    }

    for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(bd->targets); i < i_end; i++) {
        bd_target_destroy(bd_arr_ptr_get(bd->targets, i));
    }
    bd_arr_ptr_destroy(bd->targets);
    for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(bd->configs); i < i_end; i++) {
        bd_config_destroy(bd_arr_ptr_get(bd->configs, i));
    }
    bd_arr_ptr_destroy(bd->configs);
    for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(bd->libs); i < i_end; i++) {
        cr_free(bd->mem, bd_arr_ptr_get(bd->libs, i), cr_loc);
    }
    bd_arr_ptr_destroy(bd->libs);
    cr_executor_destroy(bd->exec);
    cr_free(bd->mem, bd, cr_loc);
}

void bd_set_defaults(struct bd* bd, const struct bd_defaults_desc* desc) {
    assert(desc->platform == 0 || bd_filter_platform(desc->platform) == desc->platform);
    assert(desc->profile == 0 || bd_filter_profile(desc->profile) == desc->profile);
    bd->default_target = desc->target;
    bd->default_platform = desc->platform;
    bd->default_profile = desc->profile;
}

static struct bd_target* bd_find_target(struct bd* bd, const char* name) {
    struct bd_target* found = NULL;
    for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(bd->targets); i < i_end; i++) {
        struct bd_target* target = bd_arr_ptr_get(bd->targets, i);
        if (strcmp(name, cr_str_get_data(target->name)) == 0) {
            found = target;
        }
    }
    return found;
}

static uint32_t bd_parse_platform(const char* label) {
    for (uint32_t i = 0; i < bd_platform_count; i++) {
        if (strcmp(label, bd_platform_label(bd_filter_platform_at_index(i))) == 0) {
            return bd_filter_platform_at_index(i);
        }
    }
    return 0;
}

static uint32_t bd_parse_profile(const char* label) {
    for (uint32_t i = 0; i < bd_profile_count; i++) {
        if (strcmp(label, bd_profile_label(bd_filter_profile_at_index(i))) == 0) {
            return bd_filter_profile_at_index(i);
        }
    }
    return 0;
}

static const char* const bd_usage =
    "usage: bd [--target=<name>] [--platform=macos|ios|wasm|linux] [--profile=debug|release] [--output=<dir>] [--verbose]\n"
    "          [--install | --run | --export] [--device[=<name>]] [-- <program arguments>]\n"
    "       bd --xcodegen [--output=<dir>]";

static struct cr_str* bd_target_names(struct bd* bd) {
    struct cr_str* names = cr_str_create_empty(bd->mem);
    for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(bd->targets); i < i_end; i++) {
        struct bd_target* target = bd_arr_ptr_get(bd->targets, i);
        if (bd_find_target(bd, cr_str_get_data(target->name)) != target) continue;
        cr_str_append_format(names, "%s%s", cr_str_get_count(names) > 0 ? ", " : "", cr_str_get_data(target->name));
    }
    return names;
}

static void bd_set_mode(struct bd* bd, enum bd_mode mode) {
    if (bd->mode != bd_mode_build) {
        bd_fail("--install, --run and --export exclude each other\n%s", bd_usage);
    }
    bd->mode = mode;
}

static int bd_finish(struct bd* bd, struct bd_arr_ptr* args, int result) {
    bd_args_destroy(args);
    bd_destroy(bd);
    if (cr_rec_get_alloc_count() != 0) {
        cr_rec_print_allocs();
    }
    return result;
}

int bd_run(struct bd* bd, int argc, char** argv) {
    struct cr_str* output_path = NULL;
    struct bd_target* target = bd->default_target;
    uint32_t platform = bd->default_platform;
    uint32_t profile = bd->default_profile;
    bool xcodegen = false;

    struct option options[] = {
        { "output", required_argument, NULL, 'o' },
        { "target", required_argument, NULL, 't' },
        { "platform", required_argument, NULL, 'p' },
        { "profile", required_argument, NULL, 'c' },
        { "xcodegen", no_argument, NULL, 'x' },
        { "verbose", no_argument, NULL, 'v' },
        { "device", optional_argument, NULL, 'd' },
        { "help", no_argument, NULL, 'h' },
        { "install", no_argument, NULL, 'i' },
        { "run", no_argument, NULL, 'r' },
        { "export", no_argument, NULL, 'e' },
        { 0 },
    };
    while (1) {
        int c = getopt_long(argc, argv, ":", options, NULL);
        if (c == -1) break;
        switch (c) {
        case 'o': {
            if (output_path) {
                cr_str_destroy(output_path);
            }
            output_path = cr_str_create(bd->mem, optarg);
        } break;
        case 't': {
            target = bd_find_target(bd, optarg);
            if (target == NULL) {
                bd_fail("unknown target %s, the targets are %s", optarg, cr_str_get_data(bd_target_names(bd)));
            }
        } break;
        case 'p': {
            platform = bd_parse_platform(optarg);
            if (platform == 0) {
                bd_fail("unknown platform %s\n%s", optarg, bd_usage);
            }
        } break;
        case 'c': {
            profile = bd_parse_profile(optarg);
            if (profile == 0) {
                bd_fail("unknown profile %s\n%s", optarg, bd_usage);
            }
        } break;
        case 'x': {
            xcodegen = true;
        } break;
        case 'v': {
            bd->verbose = true;
        } break;
        case 'd': {
            bd->device_set = true;
            if (bd->device) {
                cr_str_destroy(bd->device);
            }
            bd->device = optarg != NULL && optarg[0] != 0 ? cr_str_create(bd->mem, optarg) : NULL;
        } break;
        case 'h': {
            printf("%s\n", bd_usage);
            exit(0);
        }
        case 'i': {
            bd_set_mode(bd, bd_mode_install);
        } break;
        case 'r': {
            bd_set_mode(bd, bd_mode_run);
        } break;
        case 'e': {
            bd_set_mode(bd, bd_mode_export);
        } break;
        case ':':
            bd_fail("%s needs a value\n%s", argv[optind - 1], bd_usage);
        default:
            bd_fail("unknown argument %s\n%s", argv[optind - 1], bd_usage);
        }
    }

    struct bd_arr_ptr* args = bd_arr_ptr_create(bd->mem);
    if (optind < argc) {
        if (strcmp(argv[optind - 1], "--") != 0) {
            bd_fail("unexpected argument %s, program arguments follow --\n%s", argv[optind], bd_usage);
        }
        if (bd->mode != bd_mode_run) {
            bd_fail("program arguments after -- need --run\n%s", bd_usage);
        }
        for (int i = optind; i < argc; i++) {
            bd_arr_ptr_append(args, cr_str_create(bd->mem, argv[i]));
        }
    }
    if (xcodegen && bd->mode != bd_mode_build) {
        bd_fail("--xcodegen takes no mode\n%s", bd_usage);
    }

    if (output_path == NULL) {
        output_path = cr_str_create(bd->mem, "build");
    }

    bd->output_path = output_path;
    bd->temp_path = cr_str_create_copy(bd->mem, bd->output_path);
    cr_str_append_path_component_cstr(bd->temp_path, "temp");
    bd->products_path = cr_str_create_copy(bd->mem, bd->output_path);
    cr_str_append_path_component_cstr(bd->products_path, "products");

    cr_util_create_dir(bd->output_path);

    bd_compile_commands_write(bd);

    if (xcodegen) {
        return bd_finish(bd, args, bd_xcodegen_run(bd));
    }

    if (target == NULL) {
        bd_fail("--target is missing and the build has no default target, the targets are %s\n%s", cr_str_get_data(bd_target_names(bd)), bd_usage);
    }
    if (platform == 0) {
        bd_fail("--platform is missing and the build has no default platform\n%s", bd_usage);
    }
    if (profile == 0) {
        bd_fail("--profile is missing and the build has no default profile\n%s", bd_usage);
    }
    uint32_t filter = platform | profile;

    switch (bd->mode) {
    case bd_mode_build: break;
    case bd_mode_install: bd_install_check(bd, target, filter); break;
    case bd_mode_run: bd_launch_check(bd, target, filter); break;
    case bd_mode_export: bd_export_check(bd, target, filter); break;
    }

    double start = cr_get_current_time();

    struct bd_build* build = bd_build_create(bd);
    bd_build_target(build, target, platform, profile, bd->mode);
    bool success = bd_build_run(build);
    bd_build_destroy(build);

    static const char* const done_labels[] = { "Built", "Installed", "Built", "Exported" };
    static const char* const failed_labels[] = { "Failed to build", "Failed to install", "Failed to build", "Failed to export" };
    printf("[%s %s-%s-%s in %.3f s]\n", success ? done_labels[bd->mode] : failed_labels[bd->mode], cr_str_get_data(target->name), bd_platform_label(platform), bd_profile_label(profile), cr_get_current_time() - start);

    if (!success) {
        return bd_finish(bd, args, 1);
    }

    int result = bd->mode == bd_mode_run ? bd_launch(bd, target, filter, args) : 0;

    return bd_finish(bd, args, result);
}
