#include "bd.h"
#include "bd_xcode.h"
#include <glob.h>
#include <stdio.h>

static void build_add_refs_arg(struct bd_config* config) {
    glob_t paths;
    if (glob("~/Documents/refs", GLOB_TILDE, NULL, &paths) == 0) {
        bd_config_add_string(config, bd_platform_macos, bd_config_run_args, "--refs");
        bd_config_add_string(config, bd_platform_macos, bd_config_run_args, paths.gl_pathv[0]);
    }
    globfree(&paths);
}

static void build_configure(struct bd_config* config) {
    bd_config_set_string(config, bd_all, bd_config_c_warnings, "strict");
    bd_config_set_string(config, bd_all, bd_config_swift_bridging_header, "swift/bridge.h");
    bd_config_add_string(config, bd_all, bd_config_swift_include_paths, "../include");
    bd_config_add_string(config, bd_all, bd_config_swift_include_paths, "../src/ui/core");
    bd_config_add_string(config, bd_all, bd_config_swift_include_paths, "../src/shaders");
    bd_config_add_string(config, bd_all, bd_config_swift_include_paths, "src/ui");
}

static struct bd_config* build_server_config(struct bd_config* config, int port) {
    struct bd_config* config_server = bd_config_create_copy(config);
    char port_text[16];

    snprintf(port_text, sizeof(port_text), "%d", port);
    bd_config_add_string(config_server, bd_platform_macos, bd_config_run_args, "--certificate");
    bd_config_add_certificate_path(config_server, bd_platform_macos, bd_config_run_args);
    bd_config_add_string(config_server, bd_platform_macos, bd_config_run_args, "--private-key");
    bd_config_add_private_key_path(config_server, bd_platform_macos, bd_config_run_args);
    bd_config_add_string(config_server, bd_platform_macos, bd_config_run_args, "--port");
    bd_config_add_string(config_server, bd_platform_macos, bd_config_run_args, port_text);

    return config_server;
}

int main(int argc, char** argv) {
    setbuf(stdout, NULL);

    struct bd* bd = bd_create(cr_mem_libc, "..");

    struct bd_config* config = bd_config_create_default(bd, NULL);
    build_configure(config);

    struct bd_config* config_app = bd_config_create_default(bd, &(struct bd_config_desc) { .bundle_identifier = "com.raffaelkaehn.tests", .team_identifier = "LSYVV8JF85" });
    build_configure(config_app);
    bd_config_add_string(config_app, bd_platform_ios, bd_config_apple_entitlement_bool, "com.apple.developer.sustained-execution=YES");
    bd_config_set_string(config_app, bd_platform_ios, bd_config_apple_provisioning_profile, "iOS Team Provisioning Profile: com.raffaelkaehn.tests");
    bd_config_add_string(config_app, bd_all, bd_config_apple_info_plist_string, "NSPhotoLibraryUsageDescription=The test harness imports its own native screenshots as reference images.");
    bd_config_add_string(config_app, bd_platform_ios, bd_config_frameworks, "Photos");
    bd_config_add_string(config_app, bd_all, bd_config_c_include_paths, "src/app");
    bd_config_add_string(config_app, bd_all, bd_config_c_include_paths, "src/cr");
    bd_config_add_string(config_app, bd_all, bd_config_c_include_paths, "src/prim");
    bd_config_add_string(config_app, bd_all, bd_config_c_include_paths, "src/tree");
    bd_config_add_string(config_app, bd_all, bd_config_c_include_paths, "src/log");
    bd_config_add_string(config_app, bd_all, bd_config_c_include_paths, "src/hud");
    bd_config_add_string(config_app, bd_all, bd_config_c_include_paths, "src/ui");
    bd_config_add_string(config_app, bd_all, bd_config_c_include_paths, "shaders");
    bd_config_add_string(config_app, bd_all, bd_config_c_include_paths, "../src/app");
    bd_config_add_string(config_app, bd_all, bd_config_c_include_paths, "../src/ui/core");
    bd_config_add_string(config_app, bd_all, bd_config_c_include_paths, "../src/shaders");
    bd_config_add_string(config_app, bd_platform_macos, bd_config_frameworks, "ScreenCaptureKit");
    bd_config_add_string(config_app, bd_platform_macos, bd_config_frameworks, "CoreMedia");
    bd_config_add_string(config_app, bd_platform_macos, bd_config_c_include_paths, "../external/yoga-9d159f2d/macos-aarch64/include");
    bd_config_add_string(config_app, bd_platform_ios, bd_config_c_include_paths, "../external/yoga-9d159f2d/ios-aarch64/include");
    bd_config_add_string(config_app, bd_platform_wasm, bd_config_c_include_paths, "../external/yoga-9d159f2d/wasm32-wasip1-threads/include");
    bd_config_add_string(config_app, bd_platform_macos | bd_platform_ios | bd_platform_wasm, bd_config_c_link_libs, "c++");
    bd_config_add_string(config_app, bd_platform_wasm, bd_config_c_link_libs, "c++abi");
    bd_config_add_string(config_app, bd_platform_wasm, bd_config_c_link_flags, "-L../external/libcxx-22.1.8/wasm32-wasip1-threads/lib");

    struct bd_target* shaders = bd_target_create(bd, "tests_shaders", bd_target_type_shaders);
    bd_target_set_config(shaders, config);
    bd_target_add_input(shaders, "shaders");

    struct bd_target* shaders_embed = bd_target_create(bd, "tests_shaders_embed", bd_target_type_embed);
    bd_target_set_config(shaders_embed, config);
    bd_target_add_input_ex(shaders_embed, &(struct bd_input_desc) { .target = shaders });

    struct bd_target* lib_path = bd_target_create(bd, "Path", bd_target_type_swift_module);
    bd_target_set_config(lib_path, config);
    bd_target_add_input(lib_path, "swift/Path");

    struct bd_target* tests = bd_target_create(bd, "tests", bd_target_type_app);
    bd_target_set_config(tests, config_app);
    bd_target_add_input(tests, "src");
    bd_target_add_input_ex(tests, &(struct bd_input_desc) { .target = lib_path, .filter = bd_platform_ios });
    bd_target_add_input_ex(tests, &(struct bd_input_desc) { .path = "../external/yoga-9d159f2d/macos-aarch64/lib/libyogacore.a", .filter = bd_platform_macos, .group = "external" });
    bd_target_add_input_ex(tests, &(struct bd_input_desc) { .path = "../external/yoga-9d159f2d/ios-aarch64/lib/libyogacore.a", .filter = bd_platform_ios, .group = "external" });
    bd_target_add_input_ex(tests, &(struct bd_input_desc) { .path = "../external/yoga-9d159f2d/wasm32-wasip1-threads/lib/libyogacore.a", .filter = bd_platform_wasm, .group = "external" });
    bd_target_add_input_ex(tests, &(struct bd_input_desc) { .target = shaders_embed });
    bd_add_lib_uniform(tests, bd_module_prim | bd_module_hud | bd_module_app);

    struct bd_target* site = bd_target_create(bd, "tests_site", bd_target_type_embed);
    bd_target_set_config(site, config);
    bd_target_add_input_ex(site, &(struct bd_input_desc) { .target = tests, .overrides.platform = bd_platform_wasm });

    struct bd_config* config_server = build_server_config(config, 9443);
    build_add_refs_arg(config_server);

    struct bd_target* server = bd_target_create(bd, "tests_server", bd_target_type_executable);
    bd_target_set_config(server, config_server);
    bd_target_add_input(server, "server/server.c");
    bd_target_add_input_ex(server, &(struct bd_input_desc) { .target = site, .filter = bd_platform_macos | bd_platform_linux });
    bd_add_lib_uniform(server, bd_module_cl);

    struct bd_config* config_server_tests = build_server_config(config, 9446);
    bd_config_add_string(config_server_tests, bd_all, bd_config_c_include_paths, "../src/cl");
    bd_config_add_string(config_server_tests, bd_all, bd_config_c_include_paths, "../external/picohttpparser");
    bd_config_add_string(config_server_tests, bd_platform_macos, bd_config_c_include_paths, "../external/bearssl-0.6/macos-aarch64/include");
    bd_config_add_string(config_server_tests, bd_platform_linux, bd_config_c_include_paths, "../external/bearssl-0.6/linux-x86_64/include");

    struct bd_target* server_tests = bd_target_create(bd, "server_tests", bd_target_type_executable);
    bd_target_set_config(server_tests, config_server_tests);
    bd_target_add_input(server_tests, "server/server_tests.c");
    bd_add_lib_uniform(server_tests, bd_module_cl);

    struct bd_config* config_capture = bd_config_create_copy(config);
    bd_config_set_string(config_capture, bd_all, bd_config_apple_bundle_identifier, "com.raffaelkaehn.capture");

    struct bd_target* capture = bd_target_create(bd, "capture", bd_target_type_ui_tests);
    bd_target_set_config(capture, config_capture);
    bd_target_add_input_ex(capture, &(struct bd_input_desc) { .path = "capture/Capture.m", .filter = bd_platform_ios });

    bd_set_defaults(bd, &(struct bd_defaults_desc) { .target = tests, .platform = bd_platform_macos, .profile = bd_profile_debug });

    bd_xcode_set_project_name(bd, "tests");
    bd_xcode_set_command_prefix(bd, "source ~/.config/zsh/.zshrc");
    bd_xcode_add_group_order(bd, "src");
    bd_xcode_add_group_order(bd, "server");
    bd_xcode_add_group_order(bd, "shaders");
    bd_xcode_add_group_order(bd, "swift");
    bd_xcode_add_group_order(bd, "capture");
    bd_xcode_add_group_order(bd, "external");
    bd_xcode_add_group_order(bd, "generated");
    bd_xcode_add_target(bd, tests);
    bd_xcode_add_target(bd, server);
    bd_xcode_add_target(bd, server_tests);
    bd_xcode_add_target(bd, capture);

    return bd_run(bd, argc, argv);
}
