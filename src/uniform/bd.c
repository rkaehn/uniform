#include "uniform.h"

#if uniform_tool

#include "bd_private.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct cr_str* uniform_root(struct cr_mem* mem) {
    struct cr_str* root = cr_util_get_executable_path(mem);
    if (root == NULL) {
        fprintf(stderr, "uniform: cannot locate the executable\n");
        exit(1);
    }
    for (int i = 0; i < 3; i++) {
        cr_str_remove_last_path_component(root);
    }
    return root;
}

int uniform_device(const char* script_path, const char* device, int argc, char** argv, int script_args) {
    struct cr_mem* mem = cr_mem_libc;
    uint8_t raw[cr_str_size];
    struct cr_str* script = cr_util_get_real_path(mem, cr_str_temp(raw, (char*)script_path));
    if (script == NULL) {
        fprintf(stderr, "uniform: cannot read %s\n", script_path);
        return 1;
    }
    struct cr_str* dir = cr_str_create_copy(mem, script);
    cr_str_remove_last_path_component(dir);
    struct cr_str* name = cr_str_get_last_path_component(mem, dir);
    char* name_raw = cr_str_get_raw(name);
    for (uint32_t i = 0, i_end = cr_str_get_count(name); i < i_end; i++) {
        char c = name_raw[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-')) {
            name_raw[i] = '-';
        }
    }
    struct cr_str* bundle = cr_str_create_format(mem, "uniform.%s", cr_str_get_data(name));
    struct cr_str* root = uniform_root(mem);

    struct bd* bd = bd_create(mem, cr_str_get_data(root));
    struct bd_target* app = bd_target_create(bd, cr_str_get_data(name), bd_target_type_app);
    struct bd_config* config = bd_config_create_default(bd, &(struct bd_config_desc) { .bundle_identifier = cr_str_get_data(bundle) });
    bd_config_add_string(config, bd_platform_ios, bd_config_apple_info_plist_bool, "UIFileSharingEnabled=YES");
    bd_config_add_string(config, bd_platform_ios, bd_config_apple_info_plist_bool, "LSSupportsOpeningDocumentsInPlace=YES");
    bd_config_add_string(config, bd_platform_ios, bd_config_apple_info_plist_bool, "UniformEditableScript=YES");
    bd_target_set_config(app, config);

    struct cr_util_dir_iter iter = cr_util_dir_iter(mem, dir);
    while (cr_util_dir_iter_next(&iter)) {
        const char* file = cr_str_get_data(iter.relative_path);
        if (iter.file_type == cr_util_file_type_directory) {
            cr_util_dir_iter_skip(&iter);
        } else if (iter.file_type == cr_util_file_type_regular && file[0] != '.' && strcmp(file, "main.js") != 0) {
            bd_target_add_input(app, cr_str_get_data(iter.full_path));
        }
    }
    bd_add_uniform(app);

    struct cr_str* main_path = cr_str_create_format(mem, "build/temp/uniform/%s/main.js", cr_str_get_data(name));
    if (cr_util_copy(mem, script, main_path) == false) {
        fprintf(stderr, "uniform: cannot write %s\n", cr_str_get_data(main_path));
        exit(1);
    }
    bd_target_add_input(app, cr_str_get_data(main_path));
    bd_set_defaults(bd, &(struct bd_defaults_desc) { .target = app, .platform = bd_platform_ios, .profile = bd_profile_release });

    cr_str_destroy(main_path);
    cr_str_destroy(root);
    cr_str_destroy(bundle);
    cr_str_destroy(name);
    cr_str_destroy(dir);
    cr_str_destroy(script);

    char device_arg[256];
    snprintf(device_arg, sizeof(device_arg), "--device=%s", device);
    char** run_argv = malloc((size_t)(argc - script_args + 4) * sizeof(*run_argv));
    int run_argc = 0;
    run_argv[run_argc++] = argv[0];
    run_argv[run_argc++] = "--run";
    if (device[0] != 0) {
        run_argv[run_argc++] = device_arg;
    }
    run_argv[run_argc++] = "--";
    for (int i = script_args; i < argc; i++) {
        run_argv[run_argc++] = argv[i];
    }
    int result = bd_run(bd, run_argc, run_argv);
    free(run_argv);
    return result;
}

int uniform_certificate(const char* device) {
    return device != NULL ? bd_cert_install(cr_mem_libc, device) : bd_cert_create(cr_mem_libc);
}

#endif
