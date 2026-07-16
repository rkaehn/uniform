#pragma once

#include "bd.h"
#include "cr.h"
#include "prim.h"

#define cr_arr bd_arr_ptr
#define cr_arr_item void*
#include "cr_arr.h"

enum bd_mode {
    bd_mode_build,
    bd_mode_install,
    bd_mode_run,
    bd_mode_export,
};

struct bd_xcode;
struct cl_site;

enum bd_task_type {
    bd_task_type_clear,
    bd_task_type_c_src_to_object,
    bd_task_type_c_link_objects,
    bd_task_type_c_archive,
    bd_task_type_swift_module_compile,
    bd_task_type_slang_to_native,
    bd_task_type_metal_src_to_object,
    bd_task_type_metal_link_objects,
    bd_task_type_wgsl_bundle,
    bd_task_type_package_copy,
    bd_task_type_package_web,
    bd_task_type_package_app,
    bd_task_type_embed,
    bd_task_type_sign,
    bd_task_type_run,
};

struct bd_task {
    struct bd* bd;
    enum bd_task_type type;
    struct bd_arr_ptr* deps;
    struct cr_task* cr_task;
    struct cr_str* output;
    struct bd_config* config;
    uint32_t filter;
    bool export;
    bool success;
    struct {
        struct cr_str* temp_path;
        struct cr_str* products_path;
    } clear;
    struct {
        struct cr_str* src_path;
        struct cr_str* object_path;
    } c_src_to_object;
    struct {
        struct bd_arr_ptr* object_paths;
        struct cr_str* output_path;
    } c_link_objects;
    struct {
        struct bd_arr_ptr* object_paths;
        struct bd_arr_ptr* archive_paths;
        struct cr_str* output_path;
        struct cr_str* script_path;
    } c_archive;
    struct {
        struct bd_arr_ptr* src_paths;
        struct bd_arr_ptr* import_paths;
        struct cr_str* module_name;
        struct cr_str* output_a_path;
        struct cr_str* output_module_path;
    } swift_module_compile;
    struct {
        struct cr_str* slang_path;
        struct cr_str* output_path;
    } slang_to_native;
    struct {
        struct cr_str* src_path;
        struct cr_str* object_path;
    } metal_src_to_object;
    struct {
        struct bd_arr_ptr* object_paths;
        struct cr_str* output_path;
    } metal_link_objects;
    struct {
        struct bd_arr_ptr* input_paths;
        struct cr_str* output_path;
    } wgsl_bundle;
    struct {
        struct cr_str* dir_path;
        bool clear;
        struct bd_arr_ptr* input_paths;
        struct bd_arr_ptr* output_paths;
    } package_copy;
    struct {
        struct cr_str* exec_path;
        struct cr_str* entitlements_path;
    } sign;
    struct {
        struct bd_arr_ptr* args;
    } run;
    struct {
        struct bd_arr_ptr* input_paths;
        struct cr_str* temp_path;
        struct cr_str* public_path;
        struct cr_str* index_path;
    } package_web;
    struct {
        struct bd_arr_ptr* resource_paths;
        struct cr_str* exec_path;
        struct cr_str* entitlements_path;
        struct cr_str* app_path;
    } package_app;
    struct {
        struct bd_arr_ptr* input_paths;
        struct cr_str* output_path;
        struct cr_str* symbol_prefix;
    } embed;
};

struct bd_ref {
    struct bd* bd;
    struct bd_config* config;
    struct cr_str* path;
    struct bd_task* task;
    uint32_t filter;
};

struct bd_target_build {
    struct bd* bd;
    struct bd_target* target;
    uint32_t filter;
    enum bd_mode mode;
    struct cr_str* temp_path;
    struct cr_str* products_path;
    struct bd_task* clear;
    struct bd_arr_ptr* tasks;
    struct bd_arr_ptr* outputs;
};

struct bd_build {
    struct bd* bd;
    struct bd_arr_ptr* targets;
};

struct bd_config {
    struct bd* bd;
    struct prim* values[bd_config_name_count][bd_profile_count][bd_platform_count];
};

struct _bd_input_desc {
    struct bd* bd;
    struct bd_target* target;
    struct cr_str* path;
    struct cr_str* name;
    uint32_t filter;
    bool is_indirect;
    bool xcode_skip;
    struct cr_str* group;
    struct {
        struct bd_config* config;
        uint32_t platform;
        uint32_t profile;
    } overrides;
};

struct bd_target {
    struct bd* bd;
    struct cr_str* name;
    enum bd_target_type type;
    struct bd_config* config;
    struct bd_arr_ptr* inputs;
};

struct bd_lib {
    uint32_t modules;
    struct bd_target* target;
};

struct bd_cert {
    struct cr_mem* mem;
    struct cr_str* host;
    struct cr_str* certificate;
    struct cr_str* private_key;
};

struct bd_serve_desc {
    struct cl_site* site;
    struct cr_str* dir;
    int port;
    const char* bind_address;
    const char* path;
    const char* device;
};

struct bd {
    struct cr_mem* mem;
    struct cr_executor* exec;
    struct bd_arr_ptr* targets;
    struct bd_arr_ptr* configs;
    struct bd_arr_ptr* libs;
    struct bd_xcode* xcode;
    struct cr_str* uniform_path;
    bool uniform_has_src;
    bool uniform_is_cwd;
    struct cr_str* cwd_path;
    struct cr_str* output_path;
    struct cr_str* temp_path;
    struct cr_str* products_path;
    struct cr_str* macos_sdk_path;
    struct cr_str* ios_sdk_path;
    struct cr_str* clang_resource_dir;
    bool verbose;
    enum bd_mode mode;
    struct bd_target* default_target;
    uint32_t default_platform;
    uint32_t default_profile;
    bool device_set;
    struct cr_str* device;
    struct cr_str* team;
};

#define bd_filter_platforms_mask ((1u << bd_platform_count) - 1u)
#define bd_filter_profiles_mask (((1u << bd_profile_count) - 1u) << bd_platform_count)

static inline uint32_t bd_filter_platform(uint32_t filter) {
    filter = filter & bd_filter_platforms_mask;
    assert(popcount_uint32(filter) == 1);
    return filter;
}

static inline uint32_t bd_filter_profile(uint32_t filter) {
    filter = filter & bd_filter_profiles_mask;
    assert(popcount_uint32(filter) == 1);
    return filter;
}

static inline uint32_t bd_filter_platform_at_index(uint32_t index) {
    assert(index < bd_platform_count);
    return 1u << index;
}

static inline uint32_t bd_filter_profile_at_index(uint32_t index) {
    assert(index < bd_profile_count);
    return 1u << (index + bd_platform_count);
}

static inline bool bd_filter_matches(uint32_t filter, uint32_t test) {
    return (filter & bd_filter_platform(test)) && (filter & bd_filter_profile(test));
}

static inline uint32_t bd_filter_fix(uint32_t raw) {
    raw &= (bd_filter_profiles_mask | bd_filter_platforms_mask);

    if ((raw & bd_filter_profiles_mask) == 0) {
        raw |= bd_filter_profiles_mask;
    }

    if ((raw & bd_filter_platforms_mask) == 0) {
        raw |= bd_filter_platforms_mask;
    }

    return raw;
}

const char* bd_macos_version_label(int version);
const char* bd_ios_version_label(int version);
const char* bd_target_type_label(enum bd_target_type type);

__attribute__((format(printf, 1, 2), noreturn))
void bd_fail(const char* fmt, ...);

struct bd_arr_ptr* bd_args_create(struct cr_mem* mem, ...);
void bd_print_args(struct bd_arr_ptr* args, struct cr_str* output);
void bd_args_destroy(struct bd_arr_ptr* args);
int bd_exec(struct bd_arr_ptr* args, struct cr_str* output);
int bd_exec_inherit(struct bd_arr_ptr* args);
struct cr_str* bd_args_join(struct cr_mem* mem, struct bd_arr_ptr* args);
void bd_c_flags_args(struct cr_mem* mem, struct bd_config* config, uint32_t filter, struct bd_arr_ptr* args);
void bd_c_args(struct cr_mem* mem, struct bd_config* config, uint32_t filter, struct bd_arr_ptr* args);

struct cr_str* bd_path_resolve(struct bd* bd, struct cr_str* raw);
struct cr_str* bd_products_path(struct bd* bd, struct bd_target* target, uint32_t filter);
struct cr_str* bd_temp_path(struct bd* bd, struct bd_target* target, uint32_t filter);
const char* bd_executable_name(struct bd_target* target, uint32_t filter);
struct cr_str* bd_app_path(struct bd* bd, struct bd_target* target, uint32_t filter);
struct cr_str* bd_program_path(struct bd* bd, struct bd_target* target, uint32_t filter);
void bd_config_args_expand(struct bd* bd, struct bd_config* config, uint32_t filter, enum bd_config_name name, struct cr_str* products_path, struct bd_arr_ptr* args);
const char* bd_config_require_string(struct bd_config* config, uint32_t filter, enum bd_config_name name, const char* message);
bool bd_config_is_string(struct bd_config* config, uint32_t filter, enum bd_config_name name, const char* value);
bool bd_export_is_app_store(struct bd_config* config, uint32_t filter);
const char* bd_export_identity_name(struct bd_config* config, uint32_t filter);
struct cr_str* bd_apple_identity(struct cr_mem* mem, const char* name, struct cr_str* output);
struct cr_str* bd_apple_provisioning_profile(struct cr_mem* mem, const char* name, struct cr_str* output);
const char* bd_apple_team(struct bd* bd, struct bd_config* config, uint32_t filter);
struct cr_str* bd_uniform_path(struct bd* bd, const char* relative);
struct cr_str* bd_uniform_index_html(struct cr_mem* mem, const char* title, const char* main);

bool bd_target_is_program(struct bd_target* target);

struct bd_task* bd_task_create(struct bd_target_build* target_build, enum bd_task_type type, struct bd_config* config);
void bd_task_destroy(struct bd_task* task);
struct bd_arr_ptr* bd_c_src_to_object_args(struct bd_task* task);
void bd_func(void* args);

bool bd_config_has_entitlements(struct bd_config* config, uint32_t filter);
struct cr_str* bd_generate_entitlements(struct bd* bd, struct bd_config* config, uint32_t filter, bool provisioned, bool export);

struct bd_build* bd_build_create(struct bd* bd);
void bd_build_destroy(struct bd_build* build);
struct bd_target_build* bd_build_target(struct bd_build* build, struct bd_target* target, uint32_t platform, uint32_t profile, enum bd_mode mode);
bool bd_build_run(struct bd_build* build);
void bd_compile_commands_write(struct bd* bd);

struct cr_str* bd_cert_dir(struct cr_mem* mem);
struct cr_str* bd_cert_host(struct cr_mem* mem);
int bd_cert_create(struct cr_mem* mem);
int bd_cert_install(struct cr_mem* mem, const char* device);
struct bd_cert* bd_cert_load(struct cr_mem* mem);
void bd_cert_destroy(struct bd_cert* cert);

struct cr_str* bd_device_resolve(struct cr_mem* mem, const char* device);
void bd_install_check(struct bd* bd, struct bd_target* target, uint32_t filter);
void bd_install_commands(struct bd* bd, struct bd_target* target, uint32_t filter, struct bd_arr_ptr* commands);
void bd_launch_check(struct bd* bd, struct bd_target* target, uint32_t filter);
int bd_launch(struct bd* bd, struct bd_target* target, uint32_t filter, struct bd_arr_ptr* args);

struct cr_str* bd_serve_path(struct cr_mem* mem, struct bd_arr_ptr* args);
int bd_serve_site(struct cr_mem* mem, const struct bd_serve_desc* desc);
int bd_serve(struct bd* bd, struct bd_target* target, uint32_t filter, const char* path);

void bd_export_check(struct bd* bd, struct bd_target* target, uint32_t filter);
void bd_export_commands(struct bd* bd, struct bd_target* target, uint32_t filter, struct bd_arr_ptr* commands);

void bd_xcode_destroy(struct bd_xcode* xcode);
struct bd_arr_ptr* bd_xcode_targets(struct bd* bd);
int bd_xcodegen_run(struct bd* bd);
