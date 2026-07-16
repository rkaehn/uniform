#include "bd.h"
#include "bd_xcode.h"
#include <stdio.h>

static struct bd_input_desc build_gen_output(struct bd_target* gen, const char* name) {
    return (struct bd_input_desc) { .target = gen, .name = name, .overrides.platform = bd_platform_macos, .overrides.profile = bd_profile_release };
}

static struct bd_target* build_embed(struct bd* bd, struct bd_config* config, const char* name, const struct bd_input_desc* input) {
    struct bd_target* embed = bd_target_create(bd, name, bd_target_type_embed);
    bd_target_set_config(embed, config);
    bd_target_add_input_ex(embed, input);
    return embed;
}

static struct bd_config* build_config(struct bd* bd) {
    struct bd_config* config = bd_config_create_default(bd, NULL);
    bd_config_set_string(config, bd_all, bd_config_c_warnings, "strict");
    return config;
}

static void build_add_runtime_sources(struct bd_target* target) {
    bd_target_add_input(target, "src/uniform/main.c");
    bd_target_add_input(target, "src/uniform/bd.c");
    bd_target_add_input(target, "src/uniform/serve.c");
    bd_target_add_input(target, "src/uniform/jsc.m");
    bd_target_add_input(target, "src/js/js.c");
}

int main(int argc, char** argv) {
    setbuf(stdout, NULL);

    struct bd* bd = bd_create(cr_mem_libc, ".");

    struct bd_config* gen_config = build_config(bd);
    bd_config_add_string(gen_config, bd_all, bd_config_c_include_paths, "/opt/homebrew/opt/llvm/include");
    bd_config_add_string(gen_config, bd_all, bd_config_c_link_flags, "-L/opt/homebrew/opt/llvm/lib");
    bd_config_add_string(gen_config, bd_all, bd_config_c_link_flags, "-Wl,-rpath,/opt/homebrew/opt/llvm/lib");
    bd_config_add_string(gen_config, bd_all, bd_config_c_link_libs, "clang");

    struct bd_target* js_gen = bd_target_create(bd, "js_gen", bd_target_type_executable);
    bd_target_set_config(js_gen, gen_config);
    bd_target_add_input(js_gen, "tools/js_gen");
    bd_target_add_input(js_gen, "src/cr/cr_mem.c");
    bd_target_add_input(js_gen, "src/cr/cr_str.c");
    bd_target_add_input(js_gen, "src/cr/cr_task.c");
    bd_target_add_input(js_gen, "src/cr/cr_util.c");

    struct bd_config* config = build_config(bd);
    bd_config_add_string(config, bd_all, bd_config_c_include_paths, "src/js");
    bd_config_add_string(config, bd_all, bd_config_c_include_paths, "src/bd");
    bd_config_add_string(config, bd_all, bd_config_c_include_paths, "src/uniform");
    bd_config_add_string(config, bd_all, bd_config_c_include_paths, "build/products/gen-macos-release");
    bd_config_add_string(config, bd_platform_macos | bd_platform_ios, bd_config_frameworks, "JavaScriptCore");

    struct bd_config* tool_config = bd_config_create_copy(config);
    bd_config_add_string(tool_config, bd_all, bd_config_c_defines, "uniform_tool=1");

    struct bd_config* run_config = bd_config_create_copy(tool_config);
    bd_config_add_string(run_config, bd_all, bd_config_run_args, "--out");
    bd_config_add_products_dir(run_config, bd_all, bd_config_run_args);
    bd_config_add_string(run_config, bd_all, bd_config_run_args, "--native");
    bd_config_add_c_args(run_config, bd_all, bd_config_run_args, bd_platform_macos | bd_profile_release);
    bd_config_add_string(run_config, bd_all, bd_config_run_args, "--wasm");
    bd_config_add_c_args(run_config, bd_all, bd_config_run_args, bd_platform_wasm | bd_profile_release);

    struct bd_target* gen = bd_target_create(bd, "gen", bd_target_type_run);
    bd_target_set_config(gen, run_config);
    bd_target_add_input_ex(gen, &(struct bd_input_desc) { .target = js_gen });
    struct bd_input_desc gen_c = build_gen_output(gen, "js_gen.c");
    struct bd_input_desc gen_jsc = build_gen_output(gen, "jsc_gen.c");
    struct bd_input_desc gen_web = build_gen_output(gen, "uniform.js");
    struct bd_input_desc gen_native = build_gen_output(gen, "uniform_native.js");
    struct bd_input_desc gen_dts = build_gen_output(gen, "uniform.d.ts");

    struct bd_target* wasm = bd_target_create(bd, "runtime_wasm", bd_target_type_executable);
    bd_target_set_config(wasm, config);
    bd_target_add_input_ex(wasm, &gen_c);
    bd_target_add_input(wasm, "src/js/js.c");
    bd_add_lib_uniform(wasm, bd_module_all);

    struct bd_config* web_config = bd_config_create_copy(config);
    bd_config_set_string(web_config, bd_all, bd_config_install_path, "bin");

    struct bd_target* web = bd_target_create(bd, "runtime_web", bd_target_type_aggregate);
    bd_target_set_config(web, web_config);
    bd_target_add_input_ex(web, &(struct bd_input_desc) { .target = wasm, .name = "index.wasm" });
    bd_target_add_input_ex(web, &gen_web);

    struct bd_target* runtime_js = build_embed(bd, config, "runtime_js", &gen_native);
    struct bd_target* types_dts = build_embed(bd, config, "types_dts", &gen_dts);
    struct bd_target* default_js = build_embed(bd, config, "default_js", &(struct bd_input_desc) { .path = "src/js/default.js" });

    struct bd_config* ios_config = bd_config_create_copy(config);
    bd_config_set_string(ios_config, bd_all, bd_config_executable_name, "uniform");
    bd_config_set_string(ios_config, bd_all, bd_config_install_path, "bin");

    struct bd_target* ios = bd_target_create(bd, "runtime_ios", bd_target_type_executable);
    bd_target_set_config(ios, ios_config);
    build_add_runtime_sources(ios);
    bd_target_add_input_ex(ios, &gen_c);
    bd_target_add_input_ex(ios, &gen_jsc);
    bd_target_add_input_ex(ios, &(struct bd_input_desc) { .target = runtime_js });
    bd_target_add_input_ex(ios, &(struct bd_input_desc) { .target = default_js });
    bd_add_lib_uniform(ios, bd_module_all);

    struct bd_config* runtime_config = bd_config_create_copy(tool_config);
    bd_config_set_string(runtime_config, bd_all, bd_config_executable_name, "uniform");
    bd_config_set_string(runtime_config, bd_all, bd_config_install_path, "bin");
    bd_config_set_string(runtime_config, bd_platform_macos, bd_config_apple_notary_profile, "uniform-notary");
    bd_config_add_string(runtime_config, bd_platform_macos, bd_config_apple_entitlement_bool, "com.apple.security.cs.allow-jit=YES");

    struct bd_target* runtime = bd_target_create(bd, "runtime", bd_target_type_executable);
    bd_target_set_config(runtime, runtime_config);
    build_add_runtime_sources(runtime);
    bd_target_add_input_ex(runtime, &gen_c);
    bd_target_add_input_ex(runtime, &gen_jsc);
    bd_target_add_input_ex(runtime, &(struct bd_input_desc) { .target = runtime_js });
    bd_target_add_input_ex(runtime, &(struct bd_input_desc) { .target = default_js });
    bd_target_add_input_ex(runtime, &(struct bd_input_desc) { .target = types_dts });
    struct bd_target* lib = bd_add_lib_uniform(runtime, bd_module_all);

    bd_config_set_string(bd_target_get_config(lib), bd_all, bd_config_install_path, "lib");

    struct bd_target* dist = bd_target_create(bd, "dist", bd_target_type_aggregate);
    bd_target_set_config(dist, config);
    bd_target_add_input_ex(dist, &(struct bd_input_desc) { .target = runtime, .is_indirect = true, .overrides.platform = bd_platform_macos });
    bd_target_add_input_ex(dist, &(struct bd_input_desc) { .target = ios, .is_indirect = true, .overrides.platform = bd_platform_ios });
    bd_target_add_input_ex(dist, &(struct bd_input_desc) { .target = web, .is_indirect = true, .overrides.platform = bd_platform_wasm });
    bd_target_add_input_ex(dist, &(struct bd_input_desc) { .target = lib, .is_indirect = true, .overrides.platform = bd_platform_linux });

    bd_set_defaults(bd, &(struct bd_defaults_desc) { .target = runtime, .platform = bd_platform_macos, .profile = bd_profile_release });

    bd_xcode_set_project_name(bd, "uniform");
    bd_xcode_set_command_prefix(bd, "source ~/.config/zsh/.zshrc");
    bd_xcode_add_group_order(bd, "include");
    bd_xcode_add_group_order(bd, "src");
    bd_xcode_add_group_order(bd, "external");
    bd_xcode_add_target(bd, lib);

    return bd_run(bd, argc, argv);
}
