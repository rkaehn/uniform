#include "bd.h"
#include "bd_xcode.h"
#include <stdio.h>

int main(int argc, char** argv) {
    setbuf(stdout, NULL);

    struct bd* bd = bd_create(cr_mem_libc, "..");

    struct bd_config* config = bd_config_create_default(bd, NULL);
    bd_config_set_string(config, bd_all, bd_config_c_warnings, "strict");

    struct bd_config* config_app = bd_config_create_default(bd, &(struct bd_config_desc) { .bundle_identifier = "com.raffaelkaehn.demo", .team_identifier = "LSYVV8JF85" });
    bd_config_set_string(config_app, bd_all, bd_config_c_warnings, "strict");

    struct bd_target* demo = bd_target_create(bd, "demo", bd_target_type_app);
    bd_target_set_config(demo, config_app);
    bd_target_add_input(demo, "src");
    bd_add_lib_uniform(demo, bd_module_hud | bd_module_app);

    struct bd_target* site = bd_target_create(bd, "demo_site", bd_target_type_embed);
    bd_target_set_config(site, config);
    bd_target_add_input_ex(site, &(struct bd_input_desc) { .target = demo, .overrides.platform = bd_platform_wasm });

    struct bd_config* config_server = bd_config_create_copy(config);
    bd_config_add_string(config_server, bd_platform_macos, bd_config_run_args, "--certificate");
    bd_config_add_certificate_path(config_server, bd_platform_macos, bd_config_run_args);
    bd_config_add_string(config_server, bd_platform_macos, bd_config_run_args, "--private-key");
    bd_config_add_private_key_path(config_server, bd_platform_macos, bd_config_run_args);
    bd_config_add_string(config_server, bd_platform_macos, bd_config_run_args, "--port");
    bd_config_add_string(config_server, bd_platform_macos, bd_config_run_args, "9443");

    struct bd_target* server = bd_target_create(bd, "demo_server", bd_target_type_executable);
    bd_target_set_config(server, config_server);
    bd_target_add_input(server, "server/server.c");
    bd_target_add_input_ex(server, &(struct bd_input_desc) { .target = site, .filter = bd_platform_macos | bd_platform_linux });
    bd_add_lib_uniform(server, bd_module_cl);

    bd_set_defaults(bd, &(struct bd_defaults_desc) { .target = demo, .platform = bd_platform_macos, .profile = bd_profile_debug });

    bd_xcode_set_project_name(bd, "demo");
    bd_xcode_set_command_prefix(bd, "source ~/.config/zsh/.zshrc");
    bd_xcode_add_group_order(bd, "src");
    bd_xcode_add_group_order(bd, "server");
    bd_xcode_add_group_order(bd, "generated");
    bd_xcode_add_target(bd, demo);
    bd_xcode_add_target(bd, server);

    return bd_run(bd, argc, argv);
}
