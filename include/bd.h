#pragma once

#include "cr.h"
#include "obj.h"

struct bd;
struct bd_config;
struct bd_target;

#define bd_platform_count 4
#define bd_profile_count 2

enum cr_closed_flag bd_filter {
    bd_platform_macos = 1 << 0,
    bd_platform_ios = 1 << 1,
    bd_platform_wasm = 1 << 2,
    bd_platform_linux = 1 << 3,
    bd_profile_debug = 1 << bd_platform_count,
    bd_profile_release = 1 << (bd_platform_count + 1),
    bd_all = (1 << (bd_platform_count + bd_profile_count)) - 1,
};

enum cr_closed bd_ios {
    bd_ios_16_0,
    bd_ios_17_0,
    bd_ios_18_0,
    bd_ios_26_0,
};

enum cr_closed bd_macos {
    bd_macos_13_0,
    bd_macos_14_0,
    bd_macos_15_0,
    bd_macos_26_0,
};

enum cr_closed bd_config_name {
    bd_config_uniform_path,
    bd_config_c_path,
    bd_config_c_flags,
    bd_config_c_link_flags,
    bd_config_c_optimize,
    bd_config_c_asan,
    bd_config_c_tsan,
    bd_config_c_warnings,
    bd_config_c_include_paths,
    bd_config_c_defines,
    bd_config_c_link_libs,
    bd_config_ar_path,
    bd_config_c_sysroot,
    bd_config_min_ios_version,
    bd_config_min_macos_version,
    bd_config_frameworks,
    bd_config_swift_path,
    bd_config_swift_flags,
    bd_config_swift_include_paths,
    bd_config_swift_bridging_header,
    bd_config_slang_path,
    bd_config_slang_flags,
    bd_config_metal_compiler_flags,
    bd_config_metal_linker_flags,
    bd_config_metal_optimize,
    bd_config_metal_include_paths,
    bd_config_brotli_path,
    bd_config_apple_team_identifier,
    bd_config_apple_bundle_identifier,
    bd_config_apple_scene_delegate,
    bd_config_apple_signing_identity,
    bd_config_apple_provisioning_profile,
    bd_config_apple_distribution_identity,
    bd_config_apple_distribution_profile,
    bd_config_apple_installer_identity,
    bd_config_apple_notary_profile,
    bd_config_apple_info_plist_string,
    bd_config_apple_info_plist_bool,
    bd_config_apple_info_plist_array,
    bd_config_apple_entitlement_string,
    bd_config_apple_entitlement_bool,
    bd_config_apple_entitlement_array,
    bd_config_executable_name,
    bd_config_install_path,
    bd_config_run_args,
    bd_config_run_device,
    bd_config_run_port,
    bd_config_export,
    bd_config_xcode_settings,
    bd_config_name_count,
};

enum cr_closed bd_target_type {
    bd_target_type_swift_module,
    bd_target_type_library,
    bd_target_type_executable,
    bd_target_type_app,
    bd_target_type_ui_tests,
    bd_target_type_shaders,
    bd_target_type_aggregate,
    bd_target_type_embed,
    bd_target_type_run,
};

struct bd_input_desc {
    struct bd_target* target;
    const char* path;
    const char* name;
    uint32_t filter;
    bool is_indirect;
    bool xcode_skip;
    const char* group;
    struct {
        struct bd_config* config;
        uint32_t platform;
        uint32_t profile;
    } overrides;
};

struct bd_defaults_desc {
    struct bd_target* target;
    uint32_t platform;
    uint32_t profile;
};

struct bd_xcode_dependency_desc {
    const char* project;
    const char* target;
    uint32_t filter;
    bool embed;
};

struct bd* bd_create(void);

struct bd_config* bd_config_create(struct bd* bd);
struct bd_config* bd_config_create_default(struct bd* bd);
struct bd_config* bd_config_create_copy(struct bd_config* original);

void bd_config_set(struct bd_config* config, uint32_t filter, enum bd_config_name name, struct obj* set_value);
void bd_config_set_null(struct bd_config* config, uint32_t filter, enum bd_config_name name);
void bd_config_set_false(struct bd_config* config, uint32_t filter, enum bd_config_name name);
void bd_config_set_true(struct bd_config* config, uint32_t filter, enum bd_config_name name);
void bd_config_set_number(struct bd_config* config, uint32_t filter, enum bd_config_name name, double number);
void bd_config_set_int(struct bd_config* config, uint32_t filter, enum bd_config_name name, int number);
void bd_config_set_string(struct bd_config* config, uint32_t filter, enum bd_config_name name, const char* string);
void bd_config_add_string(struct bd_config* config, uint32_t filter, enum bd_config_name name, const char* string);
void bd_config_add_path(struct bd_config* config, uint32_t filter, enum bd_config_name name, const char* path);
void bd_config_add_products_dir(struct bd_config* config, uint32_t filter, enum bd_config_name name);
void bd_config_add_certificate_path(struct bd_config* config, uint32_t filter, enum bd_config_name name);
void bd_config_add_private_key_path(struct bd_config* config, uint32_t filter, enum bd_config_name name);
void bd_config_add_c_args(struct bd_config* config, uint32_t filter, enum bd_config_name name, uint32_t args_filter);

struct obj* bd_config_get(struct bd_config* config, uint32_t filter, enum bd_config_name name);
bool bd_config_has_value(struct bd_config* config, uint32_t filter, enum bd_config_name name);
bool bd_config_is_null(struct bd_config* config, uint32_t filter, enum bd_config_name name);
bool bd_config_is_false(struct bd_config* config, uint32_t filter, enum bd_config_name name);
bool bd_config_is_true(struct bd_config* config, uint32_t filter, enum bd_config_name name);
double bd_config_get_number(struct bd_config* config, uint32_t filter, enum bd_config_name name);
int bd_config_get_int(struct bd_config* config, uint32_t filter, enum bd_config_name name);
const char* bd_config_get_string(struct bd_config* config, uint32_t filter, enum bd_config_name name);
uint32_t bd_config_get_array_count(struct bd_config* config, uint32_t filter, enum bd_config_name name);
const char* bd_config_get_array_string(struct bd_config* config, uint32_t filter, enum bd_config_name name, uint32_t index);

struct bd_target* bd_target_create(struct bd* bd, const char* name, enum bd_target_type type);
void bd_target_set_config(struct bd_target* target, struct bd_config* config);
struct bd_config* bd_target_get_config(struct bd_target* target);
void bd_target_add_input_ex(struct bd_target* target, const struct bd_input_desc* desc);
void bd_target_add_input(struct bd_target* target, const char* path);
struct bd_target* bd_target_add_uniform(struct bd_target* target);

const char* bd_platform_label(uint32_t platform);
const char* bd_profile_label(uint32_t profile);

void bd_set_defaults(struct bd* bd, const struct bd_defaults_desc* desc);

int bd_run(struct bd* bd, int argc, char** argv);

void bd_xcode_set_project_name(struct bd* bd, const char* name);
void bd_xcode_set_command_prefix(struct bd* bd, const char* command);
void bd_xcode_add_group_order(struct bd* bd, const char* group);
void bd_xcode_add_target(struct bd* bd, struct bd_target* target);
void bd_xcode_add_dependency(struct bd* bd, struct bd_target* target, const struct bd_xcode_dependency_desc* desc);
