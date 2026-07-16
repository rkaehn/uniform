#include "bd_private.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/xattr.h>

static int bd_func_clear(struct bd_task* task) {
    bool success = true;
    success = success && cr_util_remove(task->clear.temp_path);
    success = success && cr_util_remove(task->clear.products_path);
    success = success && cr_util_create_dir(task->clear.temp_path);
    success = success && cr_util_create_dir(task->clear.products_path);
    return success ? 0 : 1;
}

struct bd_arr_ptr* bd_c_src_to_object_args(struct bd_task* task) {
    struct cr_mem* mem = task->bd->mem;
    struct bd_arr_ptr* args = bd_arr_ptr_create(mem);

    bd_c_args(mem, task->config, task->filter, args);

    bd_arr_ptr_append(args, cr_str_create(mem, "-c"));
    bd_arr_ptr_append(args, cr_str_create_copy(mem, task->c_src_to_object.src_path));

    bd_arr_ptr_append(args, cr_str_create_format(mem, "-o%s", cr_str_get_data(task->c_src_to_object.object_path)));

    return args;
}

static int bd_func_c_src_to_object(struct bd_task* task) {
    struct bd_arr_ptr* args = bd_c_src_to_object_args(task);

    bd_print_args(args, task->output);

    return bd_exec(args, task->output);
}

static int bd_func_c_link_objects(struct bd_task* task) {
    struct cr_mem* mem = task->bd->mem;
    struct bd_arr_ptr* args = bd_arr_ptr_create(mem);

    bd_c_args(mem, task->config, task->filter, args);

    for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(task->c_link_objects.object_paths); i < i_end; i++) {
        struct cr_str* value = bd_arr_ptr_get(task->c_link_objects.object_paths, i);
        bd_arr_ptr_append(args, cr_str_create_copy(mem, value));
    }

    bd_arr_ptr_append(args, cr_str_create(mem, "-o"));
    bd_arr_ptr_append(args, cr_str_create_copy(mem, task->c_link_objects.output_path));

    bd_print_args(args, task->output);

    return bd_exec(args, task->output);
}

static int bd_func_c_archive(struct bd_task* task) {
    struct cr_mem* mem = task->bd->mem;
    struct bd_arr_ptr* args = bd_arr_ptr_create(mem);

    struct cr_str* script = cr_str_create_format(mem, "CREATE %s\n", cr_str_get_data(task->c_archive.output_path));
    for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(task->c_archive.object_paths); i < i_end; i++) {
        cr_str_append_format(script, "ADDMOD %s\n", cr_str_get_data(bd_arr_ptr_get(task->c_archive.object_paths, i)));
    }
    for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(task->c_archive.archive_paths); i < i_end; i++) {
        cr_str_append_format(script, "ADDLIB %s\n", cr_str_get_data(bd_arr_ptr_get(task->c_archive.archive_paths, i)));
    }
    cr_str_append_cstr(script, "SAVE\nEND\n");

    bool written = cr_str_write_to_file(script, task->c_archive.script_path);
    assert(written);
    cr_str_destroy(script);

    const char* format = (bd_filter_platform(task->filter) & (bd_platform_macos | bd_platform_ios)) ? "darwin" : "gnu";

    bd_arr_ptr_append(args, cr_str_create(mem, "/bin/sh"));
    bd_arr_ptr_append(args, cr_str_create(mem, "-c"));
    bd_arr_ptr_append(args, cr_str_create_format(mem, "%s --format=%s -M < %s", bd_config_get_string(task->config, task->filter, bd_config_ar_path), format, cr_str_get_data(task->c_archive.script_path)));

    bd_print_args(args, task->output);

    return bd_exec(args, task->output);
}

static int bd_func_swift_module_compile(struct bd_task* task) {
    struct cr_mem* mem = task->bd->mem;
    struct bd_arr_ptr* args = bd_arr_ptr_create(mem);

    const char* compiler_path = bd_config_get_string(task->config, task->filter, bd_config_swift_path);

    bd_arr_ptr_append(args, cr_str_create(mem, compiler_path));

    if ((bd_filter_platform(task->filter) & bd_platform_ios) && bd_config_has_value(task->config, task->filter, bd_config_min_ios_version)) {
        bd_arr_ptr_append(args, cr_str_create(mem, "-target"));
        bd_arr_ptr_append(args, cr_str_create_format(mem, "arm64-apple-ios%s", bd_ios_version_label(bd_config_get_int(task->config, task->filter, bd_config_min_ios_version))));
    }

    if ((bd_filter_platform(task->filter) & bd_platform_macos) && bd_config_has_value(task->config, task->filter, bd_config_min_macos_version)) {
        bd_arr_ptr_append(args, cr_str_create(mem, "-target"));
        bd_arr_ptr_append(args, cr_str_create_format(mem, "arm64-apple-macos%s", bd_macos_version_label(bd_config_get_int(task->config, task->filter, bd_config_min_macos_version))));
    }

    bd_arr_ptr_append(args, cr_str_create(mem, "-emit-library"));
    bd_arr_ptr_append(args, cr_str_create(mem, "-static"));
    bd_arr_ptr_append(args, cr_str_create(mem, "-emit-module"));
    bd_arr_ptr_append(args, cr_str_create(mem, "-parse-as-library"));

    bd_arr_ptr_append(args, cr_str_create(mem, "-module-name"));
    bd_arr_ptr_append(args, cr_str_create_copy(mem, task->swift_module_compile.module_name));

    bd_arr_ptr_append(args, cr_str_create(mem, "-emit-module-path"));
    bd_arr_ptr_append(args, cr_str_create_copy(mem, task->swift_module_compile.output_module_path));

    for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(task->swift_module_compile.import_paths); i < i_end; i++) {
        struct cr_str* import_path = bd_arr_ptr_get(task->swift_module_compile.import_paths, i);
        bd_arr_ptr_append(args, cr_str_create(mem, "-I"));
        bd_arr_ptr_append(args, cr_str_create_copy(mem, import_path));
    }

    if (bd_config_has_value(task->config, task->filter, bd_config_swift_bridging_header)) {
        bd_arr_ptr_append(args, cr_str_create(mem, "-import-objc-header"));
        bd_arr_ptr_append(args, cr_str_create(mem, bd_config_get_string(task->config, task->filter, bd_config_swift_bridging_header)));
    }

    if (bd_config_has_value(task->config, task->filter, bd_config_swift_include_paths)) {
        for (uint32_t i = 0; i < bd_config_get_array_count(task->config, task->filter, bd_config_swift_include_paths); i++) {
            const char* path = bd_config_get_array_string(task->config, task->filter, bd_config_swift_include_paths, i);
            bd_arr_ptr_append(args, cr_str_create_format(mem, "-I%s", path));
        }
    }

    if (bd_config_has_value(task->config, task->filter, bd_config_swift_flags)) {
        for (uint32_t i = 0; i < bd_config_get_array_count(task->config, task->filter, bd_config_swift_flags); i++) {
            const char* flag = bd_config_get_array_string(task->config, task->filter, bd_config_swift_flags, i);
            bd_arr_ptr_append(args, cr_str_create(mem, flag));
        }
    }

    bd_arr_ptr_append(args, cr_str_create(mem, "-o"));
    bd_arr_ptr_append(args, cr_str_create_copy(mem, task->swift_module_compile.output_a_path));

    for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(task->swift_module_compile.src_paths); i < i_end; i++) {
        struct cr_str* value = bd_arr_ptr_get(task->swift_module_compile.src_paths, i);
        bd_arr_ptr_append(args, cr_str_create_copy(mem, value));
    }

    bd_print_args(args, task->output);

    return bd_exec(args, task->output);
}

static int bd_func_package_copy(struct bd_task* task) {
    bool success = true;

    if (task->package_copy.clear) {
        success = success && cr_util_remove(task->package_copy.dir_path);
        success = success && cr_util_create_dir(task->package_copy.dir_path);
    }

    for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(task->package_copy.input_paths); i < i_end; i++) {
        struct cr_str* input = bd_arr_ptr_get(task->package_copy.input_paths, i);
        struct cr_str* output = bd_arr_ptr_get(task->package_copy.output_paths, i);
        success = success && cr_util_copy(task->bd->mem, input, output);
    }

    return success ? 0 : 1;
}

static void bd_remove_finder_info(struct cr_str* path) {
    removexattr(cr_str_get_data(path), XATTR_FINDERINFO_NAME, XATTR_NOFOLLOW);
    removexattr(cr_str_get_data(path), XATTR_RESOURCEFORK_NAME, XATTR_NOFOLLOW);
}

static bool bd_codesign(struct bd_task* task, struct cr_str* path, struct cr_str* entitlements_path, const char* identity_name) {
    struct cr_mem* mem = task->bd->mem;
    const char* name = task->export ? bd_export_identity_name(task->config, task->filter) : identity_name;
    struct cr_str* identity = name ? bd_apple_identity(mem, name, task->output) : cr_str_create(mem, "-");
    if (identity == NULL) {
        return false;
    }

    struct bd_arr_ptr* args = bd_args_create(mem, "codesign", "--force", NULL);
    if (task->export) {
        bd_arr_ptr_append(args, cr_str_create(mem, "--timestamp"));
        if (!bd_export_is_app_store(task->config, task->filter)) {
            bd_arr_ptr_append(args, cr_str_create(mem, "--options"));
            bd_arr_ptr_append(args, cr_str_create(mem, "runtime"));
        }
    }
    if (entitlements_path) {
        bd_arr_ptr_append(args, cr_str_create(mem, "--entitlements"));
        bd_arr_ptr_append(args, cr_str_create_copy(mem, entitlements_path));
    }
    bd_arr_ptr_append(args, cr_str_create(mem, "--sign"));
    bd_arr_ptr_append(args, identity);
    bd_arr_ptr_append(args, cr_str_create_copy(mem, path));

    bd_remove_finder_info(path);
    struct cr_util_dir_iter iter = cr_util_dir_iter(mem, path);
    while (cr_util_dir_iter_next(&iter)) {
        bd_remove_finder_info(iter.full_path);
    }

    bd_print_args(args, task->output);

    return bd_exec(args, task->output) == 0;
}

static bool bd_write_entitlements(struct bd_task* task, struct cr_str* path, bool provisioned) {
    struct cr_str* entitlements = bd_generate_entitlements(task->bd, task->config, task->filter, provisioned, task->export);
    bool written = cr_str_write_to_file(entitlements, path);
    if (!written) {
        cr_str_append_format(task->output, "cannot write %s\n", cr_str_get_data(path));
    }
    cr_str_destroy(entitlements);
    return written;
}

static int bd_func_sign(struct bd_task* task) {
    if (task->sign.entitlements_path && !bd_write_entitlements(task, task->sign.entitlements_path, false)) {
        return 1;
    }
    return bd_codesign(task, task->sign.exec_path, task->sign.entitlements_path, NULL) ? 0 : 1;
}

static int bd_func_run(struct bd_task* task) {
    struct cr_mem* mem = task->bd->mem;
    struct bd_arr_ptr* args = bd_arr_ptr_create(mem);
    for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(task->run.args); i < i_end; i++) {
        bd_arr_ptr_append(args, cr_str_create_copy(mem, bd_arr_ptr_get(task->run.args, i)));
    }
    bd_print_args(args, task->output);
    return bd_exec(args, task->output);
}

static int bd_func_package_web(struct bd_task* task) {
    struct cr_mem* mem = task->bd->mem;

    bool success = true;
    success = success && cr_util_create_dir(task->package_web.public_path);

    if (success && task->package_web.index_path) {
        struct cr_str* html = cr_str_create(mem,
            "<!DOCTYPE html>\n"
            "<html lang=\"en\">\n"
            "<head>\n"
            "<meta charset=\"utf-8\">\n"
            "<meta name=\"color-scheme\" content=\"dark light\">\n"
            "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no\">\n"
            "<link rel=\"icon\" href=\"data:,\">\n"
            "<style>\n"
            "html, body {\n"
            "    margin: 0px;\n"
            "    overflow: hidden;\n"
            "    display: block;\n"
            "    -webkit-touch-callout: none;\n"
            "    -webkit-user-select: none;\n"
            "    user-select: none;\n"
            "    touch-action: none;\n"
            "    -webkit-tap-highlight-color: transparent;\n"
            "}\n"
            "canvas {\n"
            "    display: block;\n"
            "}\n"
            "</style>\n"
            "</head>\n"
            "<body>\n");
        for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(task->package_web.input_paths); i < i_end; i++) {
            struct cr_str* input_path = bd_arr_ptr_get(task->package_web.input_paths, i);
            if (cr_str_get_file_type(input_path) != cr_file_type_js) continue;
            struct cr_str* input_comp = cr_str_get_last_path_component(mem, input_path);
            cr_str_append_format(html, "    <script src=\"%s\"></script>\n", cr_str_get_data(input_comp));
            cr_str_destroy(input_comp);
        }
        cr_str_append_cstr(html, "</body>\n</html>\n");

        success = cr_str_write_to_file(html, task->package_web.index_path);
        if (!success) {
            cr_str_append_format(task->output, "could not write: %s\n", cr_str_get_data(task->package_web.index_path));
        }
        cr_str_destroy(html);
    }

    for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(task->package_web.input_paths); i < i_end; i++) {
        struct cr_str* input_path = bd_arr_ptr_get(task->package_web.input_paths, i);
        struct cr_str* input_comp = cr_str_get_last_path_component(task->bd->mem, input_path);

        if (bd_filter_profile(task->filter) & bd_profile_debug) {
            struct cr_str* comp_path = cr_str_create_copy(mem, task->package_web.public_path);
            cr_str_append_path_component(comp_path, input_comp);

            success = success && cr_util_copy(task->bd->mem, input_path, comp_path);

            cr_str_destroy(comp_path);
        } else {
            struct cr_str* comp_path = cr_str_create_copy(mem, task->package_web.public_path);
            cr_str_append_path_component(comp_path, input_comp);
            cr_str_append_path_extension_cstr(comp_path, "br");

            struct bd_arr_ptr* args = bd_arr_ptr_create(mem);
            bd_arr_ptr_append(args, cr_str_create(mem, bd_config_get_string(task->config, task->filter, bd_config_brotli_path)));
            bd_arr_ptr_append(args, cr_str_create(mem, "-q"));
            bd_arr_ptr_append(args, cr_str_create(mem, "7"));
            bd_arr_ptr_append(args, cr_str_create(mem, "-f"));
            bd_arr_ptr_append(args, cr_str_create_copy(mem, input_path));
            bd_arr_ptr_append(args, cr_str_create(mem, "-o"));
            bd_arr_ptr_append(args, comp_path);

            bd_print_args(args, task->output);

            success = success && bd_exec(args, task->output) == 0;
        }

        cr_str_destroy(input_comp);
    }

    return success ? 0 : 1;
}

static void bd_plist_append_string(struct cr_str* plist, const char* string) {
    for (const char* c = string; *c; c++) {
        switch (*c) {
        case '&': cr_str_append_cstr(plist, "&amp;"); break;
        case '<': cr_str_append_cstr(plist, "&lt;"); break;
        case '>': cr_str_append_cstr(plist, "&gt;"); break;
        default: cr_str_append_data(plist, c, 1); break;
        }
    }
}

struct bd_plist_names {
    enum bd_config_name string;
    enum bd_config_name boolean;
    enum bd_config_name array;
    const char* label;
};

static const struct bd_plist_names bd_info_plist_names = { bd_config_apple_info_plist_string, bd_config_apple_info_plist_bool, bd_config_apple_info_plist_array, "bd_config_apple_info_plist" };
static const struct bd_plist_names bd_entitlement_names = { bd_config_apple_entitlement_string, bd_config_apple_entitlement_bool, bd_config_apple_entitlement_array, "bd_config_apple_entitlement" };

static void bd_plist_append_entries(struct cr_str* plist, struct bd_config* config, uint32_t filter, const struct bd_plist_names* names) {
    if (bd_config_has_value(config, filter, names->string)) {
        for (uint32_t i = 0, i_end = bd_config_get_array_count(config, filter, names->string); i < i_end; i++) {
            const char* item = bd_config_get_array_string(config, filter, names->string, i);
            const char* equals = strchr(item, '=');
            if (equals == NULL) {
                bd_fail("%s_string items must be KEY=VALUE, not %s", names->label, item);
            }
            cr_str_append_format(plist, "\t<key>%.*s</key>\n\t<string>", (int)(equals - item), item);
            bd_plist_append_string(plist, equals + 1);
            cr_str_append_cstr(plist, "</string>\n");
        }
    }
    if (bd_config_has_value(config, filter, names->boolean)) {
        for (uint32_t i = 0, i_end = bd_config_get_array_count(config, filter, names->boolean); i < i_end; i++) {
            const char* item = bd_config_get_array_string(config, filter, names->boolean, i);
            const char* equals = strchr(item, '=');
            if (equals == NULL || (strcmp(equals + 1, "YES") != 0 && strcmp(equals + 1, "NO") != 0)) {
                bd_fail("%s_bool items must be KEY=YES or KEY=NO, not %s", names->label, item);
            }
            cr_str_append_format(plist, "\t<key>%.*s</key>\n\t<%s/>\n", (int)(equals - item), item, strcmp(equals + 1, "YES") == 0 ? "true" : "false");
        }
    }
    if (bd_config_has_value(config, filter, names->array)) {
        for (uint32_t i = 0, i_end = bd_config_get_array_count(config, filter, names->array); i < i_end; i++) {
            const char* item = bd_config_get_array_string(config, filter, names->array, i);
            const char* equals = strchr(item, '=');
            if (equals == NULL) {
                bd_fail("%s_array items must be KEY=VALUE VALUE, not %s", names->label, item);
            }
            cr_str_append_format(plist, "\t<key>%.*s</key>\n\t<array>\n", (int)(equals - item), item);
            for (const char* value = equals + 1; *value;) {
                const char* space = strchr(value, ' ');
                uint32_t count = space ? (uint32_t)(space - value) : (uint32_t)strlen(value);
                cr_str_append_format(plist, "\t\t<string>%.*s</string>\n", (int)count, value);
                value += count + (space ? 1u : 0u);
            }
            cr_str_append_cstr(plist, "\t</array>\n");
        }
    }
}

bool bd_config_has_entitlements(struct bd_config* config, uint32_t filter) {
    return bd_config_has_value(config, filter, bd_config_apple_entitlement_string) ||
        bd_config_has_value(config, filter, bd_config_apple_entitlement_bool) ||
        bd_config_has_value(config, filter, bd_config_apple_entitlement_array);
}

static struct cr_str* bd_generate_info_plist(struct bd_task* task) {
    struct cr_mem* mem = task->bd->mem;
    struct bd_config* config = task->config;
    uint32_t filter = task->filter;

    const char* bundle = bd_config_require_string(config, filter, bd_config_apple_bundle_identifier, "an app needs bd_config_apple_bundle_identifier");

    struct cr_str* exec_name = cr_str_get_last_path_component(mem, task->package_app.exec_path);
    struct cr_str* plist = cr_str_create_format(mem,
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
        "<plist version=\"1.0\">\n"
        "<dict>\n"
        "\t<key>CFBundleDevelopmentRegion</key>\n"
        "\t<string>en</string>\n"
        "\t<key>CFBundleExecutable</key>\n"
        "\t<string>%s</string>\n"
        "\t<key>CFBundleIdentifier</key>\n"
        "\t<string>%s</string>\n"
        "\t<key>CFBundleInfoDictionaryVersion</key>\n"
        "\t<string>6.0</string>\n"
        "\t<key>CFBundleName</key>\n"
        "\t<string>%s</string>\n"
        "\t<key>CFBundlePackageType</key>\n"
        "\t<string>APPL</string>\n"
        "\t<key>CFBundleShortVersionString</key>\n"
        "\t<string>1.0</string>\n"
        "\t<key>CFBundleVersion</key>\n"
        "\t<string>1</string>\n",
        cr_str_get_data(exec_name),
        bundle,
        cr_str_get_data(exec_name));

    switch (bd_filter_platform(filter)) {
    case bd_platform_macos: {
        if (!bd_config_has_value(config, filter, bd_config_min_macos_version)) {
            bd_fail("a macOS app needs bd_config_min_macos_version");
        }
        cr_str_append_format(plist,
            "\t<key>LSMinimumSystemVersion</key>\n"
            "\t<string>%s</string>\n",
            bd_macos_version_label(bd_config_get_int(config, filter, bd_config_min_macos_version)));
    } break;
    case bd_platform_ios: {
        if (!bd_config_has_value(config, filter, bd_config_min_ios_version)) {
            bd_fail("an iOS app needs bd_config_min_ios_version");
        }
        cr_str_append_format(plist,
            "\t<key>CFBundleSupportedPlatforms</key>\n"
            "\t<array>\n"
            "\t\t<string>iPhoneOS</string>\n"
            "\t</array>\n"
            "\t<key>MinimumOSVersion</key>\n"
            "\t<string>%s</string>\n"
            "\t<key>UIDeviceFamily</key>\n"
            "\t<array>\n"
            "\t\t<integer>1</integer>\n"
            "\t\t<integer>2</integer>\n"
            "\t</array>\n"
            "\t<key>UILaunchScreen</key>\n"
            "\t<dict/>\n",
            bd_ios_version_label(bd_config_get_int(config, filter, bd_config_min_ios_version)));
        if (bd_config_has_value(config, filter, bd_config_apple_scene_delegate)) {
            cr_str_append_format(plist,
                "\t<key>UIApplicationSceneManifest</key>\n"
                "\t<dict>\n"
                "\t\t<key>UIApplicationSupportsMultipleScenes</key>\n"
                "\t\t<true/>\n"
                "\t\t<key>UISceneConfigurations</key>\n"
                "\t\t<dict>\n"
                "\t\t\t<key>UIWindowSceneSessionRoleApplication</key>\n"
                "\t\t\t<array>\n"
                "\t\t\t\t<dict>\n"
                "\t\t\t\t\t<key>UISceneConfigurationName</key>\n"
                "\t\t\t\t\t<string>Default Configuration</string>\n"
                "\t\t\t\t\t<key>UISceneDelegateClassName</key>\n"
                "\t\t\t\t\t<string>%s</string>\n"
                "\t\t\t\t</dict>\n"
                "\t\t\t</array>\n"
                "\t\t</dict>\n"
                "\t</dict>\n",
                bd_config_get_string(config, filter, bd_config_apple_scene_delegate));
        }
    } break;
    default:
        assert(0);
    }

    bd_plist_append_entries(plist, config, filter, &bd_info_plist_names);

    cr_str_append_cstr(plist, "</dict>\n</plist>\n");
    cr_str_destroy(exec_name);
    return plist;
}

struct cr_str* bd_generate_entitlements(struct bd* bd, struct bd_config* config, uint32_t filter, bool provisioned, bool export) {
    struct cr_str* plist = cr_str_create(bd->mem,
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
        "<plist version=\"1.0\">\n"
        "<dict>\n");
    if (provisioned) {
        const char* bundle = bd_config_require_string(config, filter, bd_config_apple_bundle_identifier, "an app needs bd_config_apple_bundle_identifier");
        const char* team = bd_apple_team(bd, config, filter);
        if (team == NULL) {
            bd_fail("an app needs bd_config_apple_team_identifier for its entitlements");
        }
        cr_str_append_format(plist,
            "\t<key>application-identifier</key>\n"
            "\t<string>%s.%s</string>\n"
            "\t<key>com.apple.developer.team-identifier</key>\n"
            "\t<string>%s</string>\n"
            "\t<key>get-task-allow</key>\n"
            "\t<%s/>\n",
            team, bundle, team, export ? "false" : "true");
        if (export) {
            cr_str_append_cstr(plist, "\t<key>beta-reports-active</key>\n\t<true/>\n");
        }
        if (bd_filter_platform(filter) == bd_platform_macos) {
            cr_str_append_cstr(plist, "\t<key>com.apple.security.app-sandbox</key>\n\t<true/>\n");
        }
    }
    bd_plist_append_entries(plist, config, filter, &bd_entitlement_names);
    cr_str_append_cstr(plist, "</dict>\n</plist>\n");
    return plist;
}

struct cr_str* bd_apple_identity(struct cr_mem* mem, const char* name, struct cr_str* output) {
    struct cr_str* args[] = {
        cr_str_create(mem, "security"),
        cr_str_create(mem, "find-identity"),
        cr_str_create(mem, "-v"),
        cr_str_create(mem, "-p"),
        cr_str_create(mem, "codesigning"),
    };
    struct cr_str* listing = cr_str_create_empty(mem);
    int exit_code = cr_exec(args, cr_count(args), listing, output);
    for (uint32_t i = 0; i < cr_count(args); i++) {
        cr_str_destroy(args[i]);
    }

    struct cr_str* hash = NULL;
    if (exit_code == 0) {
        for (const char* line = cr_str_get_data(listing); *line && hash == NULL;) {
            const char* end = strchr(line, '\n');
            if (end == NULL) end = line + strlen(line);
            const char* quote = memchr(line, '"', (size_t)(end - line));
            if (quote && quote - line > 40 && quote[-1] == ' ' && strncmp(quote + 1, name, strlen(name)) == 0 && strnstr(line, "REVOKED", (size_t)(end - line)) == NULL) {
                hash = cr_str_create_data(mem, quote - 41, 40);
            }
            line = *end ? end + 1 : end;
        }
    }
    if (hash == NULL) {
        cr_str_append_format(output, "no valid signing identity starts with \"%s\", security find-identity -v -p codesigning lists the identities\n", name);
    }

    cr_str_destroy(listing);
    return hash;
}

struct cr_str* bd_apple_provisioning_profile(struct cr_mem* mem, const char* name, struct cr_str* output) {
    const char* home = getenv("HOME");
    if (home == NULL) {
        bd_fail("HOME is not set");
    }

    struct cr_str* dir = cr_str_create_format(mem, "%s/Library/Developer/Xcode/UserData/Provisioning Profiles", home);
    struct cr_str* needle = cr_str_create_format(mem, "<string>%s</string>", name);
    struct cr_str* profile = NULL;
    time_t profile_time = 0;

    struct cr_util_dir_iter iter = cr_util_dir_iter(mem, dir);
    while (cr_util_dir_iter_next(&iter)) {
        if (iter.file_type != cr_util_file_type_regular) continue;

        struct cr_str* contents = cr_str_create_with_contents_of_file(mem, iter.full_path);
        if (contents == NULL) continue;

        struct stat st;
        bool matches = memmem(cr_str_get_data(contents), cr_str_get_count(contents), cr_str_get_data(needle), cr_str_get_count(needle)) != NULL;
        if (matches && stat(cr_str_get_data(iter.full_path), &st) == 0 && (profile == NULL || st.st_mtime > profile_time)) {
            if (profile) cr_str_destroy(profile);
            profile = contents;
            profile_time = st.st_mtime;
        } else {
            cr_str_destroy(contents);
        }
    }
    if (profile == NULL) {
        cr_str_append_format(output, "no provisioning profile named \"%s\" in %s, Xcode downloads the profiles of a signed-in team there\n", name, cr_str_get_data(dir));
    }

    cr_str_destroy(needle);
    cr_str_destroy(dir);
    return profile;
}

const char* bd_apple_team(struct bd* bd, struct bd_config* config, uint32_t filter) {
    if (bd_config_has_value(config, filter, bd_config_apple_team_identifier)) {
        return bd_config_get_string(config, filter, bd_config_apple_team_identifier);
    }
    if (bd->team != NULL) {
        return cr_str_get_data(bd->team);
    }
    if (!bd_config_has_value(config, filter, bd_config_apple_provisioning_profile)) {
        return NULL;
    }
    struct cr_mem* mem = bd->mem;
    const char* name = bd_config_get_string(config, filter, bd_config_apple_provisioning_profile);
    struct cr_str* output = cr_str_create_empty(mem);
    struct cr_str* profile = bd_apple_provisioning_profile(mem, name, output);
    if (profile == NULL) {
        bd_fail("cannot read the team identifier from the provisioning profile, set bd_config_apple_team_identifier: %s", cr_str_get_data(output));
    }
    const char* key = "<key>TeamIdentifier</key>";
    const char* open = "<string>";
    const char* close = "</string>";
    const char* data = cr_str_get_data(profile);
    size_t size = cr_str_get_count(profile);
    const char* at = memmem(data, size, key, strlen(key));
    const char* start = at ? memmem(at, size - (size_t)(at - data), open, strlen(open)) : NULL;
    const char* end = start ? memmem(start, size - (size_t)(start - data), close, strlen(close)) : NULL;
    if (end == NULL) {
        bd_fail("the provisioning profile \"%s\" names no team, set bd_config_apple_team_identifier", name);
    }
    start += strlen(open);
    bd->team = cr_str_create_data(mem, start, (uint32_t)(end - start));
    cr_str_destroy(profile);
    cr_str_destroy(output);
    return cr_str_get_data(bd->team);
}

static bool bd_profile_includes_entitlements(struct bd_task* task, struct cr_str* profile, const char* name) {
    const enum bd_config_name names[] = { bd_config_apple_entitlement_string, bd_config_apple_entitlement_bool, bd_config_apple_entitlement_array };
    bool included = true;
    for (uint32_t n = 0; n < cr_count(names); n++) {
        if (!bd_config_has_value(task->config, task->filter, names[n])) continue;
        for (uint32_t i = 0, i_end = bd_config_get_array_count(task->config, task->filter, names[n]); i < i_end; i++) {
            const char* item = bd_config_get_array_string(task->config, task->filter, names[n], i);
            const char* equals = strchr(item, '=');
            int count = equals ? (int)(equals - item) : (int)strlen(item);
            struct cr_str* key = cr_str_create_format(task->bd->mem, "<key>%.*s</key>", count, item);
            if (memmem(cr_str_get_data(profile), cr_str_get_count(profile), cr_str_get_data(key), cr_str_get_count(key)) == NULL) {
                cr_str_append_format(task->output, "the provisioning profile \"%s\" does not include %.*s, enable the capability for the app identifier and use its profile\n", name, count, item);
                included = false;
            }
            cr_str_destroy(key);
        }
    }
    return included;
}

static int bd_func_package_app(struct bd_task* task) {
    struct cr_mem* mem = task->bd->mem;
    uint32_t platform = bd_filter_platform(task->filter);
    struct cr_str* app_path = task->package_app.app_path;

    struct cr_str* exec_dir = cr_str_create_copy(mem, app_path);
    struct cr_str* resources_dir = cr_str_create_copy(mem, app_path);
    struct cr_str* info_plist_path = cr_str_create_copy(mem, app_path);
    if (platform == bd_platform_macos) {
        cr_str_append_path_component_cstr(exec_dir, "Contents/MacOS");
        cr_str_append_path_component_cstr(resources_dir, "Contents/Resources");
        cr_str_append_path_component_cstr(info_plist_path, "Contents");
    }
    cr_str_append_path_component_cstr(info_plist_path, "Info.plist");

    bool success = true;
    success = success && cr_util_create_dir(exec_dir);
    success = success && cr_util_create_dir(resources_dir);
    success = success && cr_util_copy(mem, task->package_app.exec_path, exec_dir);
    if (success) {
        struct cr_str* info_plist = bd_generate_info_plist(task);
        success = cr_str_write_to_file(info_plist, info_plist_path);
        cr_str_destroy(info_plist);
    }
    for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(task->package_app.resource_paths); i < i_end; i++) {
        struct cr_str* value = bd_arr_ptr_get(task->package_app.resource_paths, i);
        success = success && cr_util_copy(mem, value, resources_dir);
    }
    bool app_store = task->export && bd_export_is_app_store(task->config, task->filter);
    const char* profile_file = platform == bd_platform_ios ? "embedded.mobileprovision" : app_store ? "Contents/embedded.provisionprofile" : NULL;
    if (success && task->package_app.entitlements_path) {
        success = bd_write_entitlements(task, task->package_app.entitlements_path, profile_file != NULL);
    }
    if (success && profile_file) {
        const char* name = task->export
            ? bd_config_get_string(task->config, task->filter, bd_config_apple_distribution_profile)
            : bd_config_require_string(task->config, task->filter, bd_config_apple_provisioning_profile, "an iOS app needs bd_config_apple_provisioning_profile");
        struct cr_str* profile = bd_apple_provisioning_profile(mem, name, task->output);
        success = profile != NULL;
        if (success) {
            success = bd_profile_includes_entitlements(task, profile, name);
            struct cr_str* profile_path = cr_str_create_copy(mem, app_path);
            cr_str_append_path_component_cstr(profile_path, profile_file);
            success = success && cr_str_write_to_file(profile, profile_path);
            cr_str_destroy(profile_path);
            cr_str_destroy(profile);
        }
    }
    if (success) {
        const char* identity = NULL;
        if (bd_config_has_value(task->config, task->filter, bd_config_apple_signing_identity)) {
            identity = bd_config_get_string(task->config, task->filter, bd_config_apple_signing_identity);
        }
        success = bd_codesign(task, app_path, task->package_app.entitlements_path, identity);
    }

    cr_str_destroy(exec_dir);
    cr_str_destroy(resources_dir);
    cr_str_destroy(info_plist_path);

    return success ? 0 : 1;
}

static bool bd_embed_add_file(struct bd_task* task, struct bd_arr_ptr* names, struct bd_arr_ptr* paths, struct cr_str* name, struct cr_str* path) {
    struct cr_mem* mem = task->bd->mem;

    struct cr_str* full_path;
    if (cr_str_is_absolute_path(path)) {
        full_path = cr_str_create_copy(mem, path);
    } else {
        full_path = cr_str_create_copy(mem, task->bd->cwd_path);
        cr_str_append_path_component(full_path, path);
    }

    struct stat st;
    if (stat(cr_str_get_data(full_path), &st) != 0 || st.st_size == 0) {
        cr_str_append_format(task->output, "embed input empty or unreadable: %s\n", cr_str_get_data(full_path));
        cr_str_destroy(full_path);
        return false;
    }

    bd_arr_ptr_append(names, cr_str_create_copy(mem, name));
    bd_arr_ptr_append(paths, full_path);
    return true;
}

static void bd_embed_sort_files(struct bd_arr_ptr* names, struct bd_arr_ptr* paths) {
    for (uint32_t i = 1, i_end = bd_arr_ptr_get_count(names); i < i_end; i++) {
        for (uint32_t j = i; j > 0 && strcmp(cr_str_get_data(bd_arr_ptr_get(names, j - 1)), cr_str_get_data(bd_arr_ptr_get(names, j))) > 0; j--) {
            void* name = bd_arr_ptr_get(names, j);
            bd_arr_ptr_set(names, j, bd_arr_ptr_get(names, j - 1));
            bd_arr_ptr_set(names, j - 1, name);
            void* path = bd_arr_ptr_get(paths, j);
            bd_arr_ptr_set(paths, j, bd_arr_ptr_get(paths, j - 1));
            bd_arr_ptr_set(paths, j - 1, path);
        }
    }
}

static int bd_func_embed(struct bd_task* task) {
    struct cr_mem* mem = task->bd->mem;

    struct bd_arr_ptr* names = bd_arr_ptr_create(mem);
    struct bd_arr_ptr* paths = bd_arr_ptr_create(mem);

    bool success = true;

    for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(task->embed.input_paths); i < i_end; i++) {
        struct cr_str* input_path = bd_arr_ptr_get(task->embed.input_paths, i);

        enum cr_util_file_type file_type;
        if (!cr_util_get_file_type(input_path, &file_type)) {
            cr_str_append_format(task->output, "embed input missing: %s\n", cr_str_get_data(input_path));
            success = false;
            continue;
        }

        if (file_type == cr_util_file_type_directory) {
            struct cr_util_dir_iter iter = cr_util_dir_iter(mem, input_path);
            while (cr_util_dir_iter_next(&iter)) {
                if (iter.file_type == cr_util_file_type_regular && cr_str_get_file_type(iter.full_path) != cr_file_type_ds_store) {
                    success = bd_embed_add_file(task, names, paths, iter.relative_path, iter.full_path) && success;
                }
            }
        } else {
            struct cr_str* name = cr_str_get_last_path_component(mem, input_path);
            success = bd_embed_add_file(task, names, paths, name, input_path) && success;
            cr_str_destroy(name);
        }
    }

    bd_embed_sort_files(names, paths);

    uint32_t count = bd_arr_ptr_get_count(names);

    if (count == 0) {
        cr_str_append_cstr(task->output, "embed has no input files\n");
        success = false;
    }

    if (success) {
        struct cr_str* text = cr_str_create_empty(mem);
        const char* prefix = cr_str_get_data(task->embed.symbol_prefix);

        cr_str_append_cstr(text, "#pragma clang diagnostic ignored \"-Weverything\"\n");

        for (uint32_t i = 0; i < count; i++) {
            cr_str_append_format(text, "static const unsigned char %s_data_%u[] = {\n#embed \"%s\"\n    , 0\n};\n", prefix, i, cr_str_get_data(bd_arr_ptr_get(paths, i)));
        }

        cr_str_append_format(text, "const char* const %s_names[] = {\n", prefix);
        for (uint32_t i = 0; i < count; i++) {
            cr_str_append_format(text, "    \"%s\",\n", cr_str_get_data(bd_arr_ptr_get(names, i)));
        }
        cr_str_append_cstr(text, "};\n");

        cr_str_append_format(text, "const unsigned char* const %s_data[] = {\n", prefix);
        for (uint32_t i = 0; i < count; i++) {
            cr_str_append_format(text, "    %s_data_%u,\n", prefix, i);
        }
        cr_str_append_cstr(text, "};\n");

        cr_str_append_format(text, "const unsigned long long %s_sizes[] = {\n", prefix);
        for (uint32_t i = 0; i < count; i++) {
            cr_str_append_format(text, "    sizeof(%s_data_%u) - 1,\n", prefix, i);
        }
        cr_str_append_cstr(text, "};\n");

        cr_str_append_format(text, "const unsigned long long %s_count = %u;\n", prefix, count);

        success = cr_str_write_to_file(text, task->embed.output_path);
        if (!success) {
            cr_str_append_format(task->output, "embed could not write: %s\n", cr_str_get_data(task->embed.output_path));
        }

        cr_str_destroy(text);
    }

    for (uint32_t i = 0; i < count; i++) {
        cr_str_destroy(bd_arr_ptr_get(names, i));
        cr_str_destroy(bd_arr_ptr_get(paths, i));
    }
    bd_arr_ptr_destroy(names);
    bd_arr_ptr_destroy(paths);

    return success ? 0 : 1;
}

static void bd_slang_metal_fixup(struct cr_mem* mem, struct cr_str* path) {
    static const struct {
        const char* from;
        const char* to;
    } rules[] = {
        { "packed_float4 device*", "float4 device*" },
        { "packed_float2 device*", "float2 device*" },
        { "packed_float4 constant*", "float4 constant*" },
        { "packed_float2 constant*", "float2 constant*" },
    };

    struct cr_str* text = cr_str_create_with_contents_of_file(mem, path);
    if (text == NULL) {
        return;
    }

    struct cr_str* out = cr_str_create_empty(mem);
    const char* cursor = cr_str_get_data(text);

    while (*cursor != '\0') {
        const char* next = NULL;
        uint32_t rule = 0;

        for (uint32_t i = 0; i < cr_count(rules); i++) {
            const char* hit = strstr(cursor, rules[i].from);
            if (hit && (next == NULL || hit < next)) {
                next = hit;
                rule = i;
            }
        }

        if (next == NULL) {
            cr_str_append_cstr(out, cursor);
            break;
        }

        cr_str_append_data(out, cursor, (uint32_t)(next - cursor));
        cr_str_append_cstr(out, rules[rule].to);
        cursor = next + strlen(rules[rule].from);
    }

    cr_str_write_to_file(out, path);
    cr_str_destroy(out);
    cr_str_destroy(text);
}

static int bd_func_slang_to_native(struct bd_task* task) {
    struct cr_mem* mem = task->bd->mem;
    struct bd_arr_ptr* args = bd_arr_ptr_create(mem);

    const char* compiler_path = bd_config_get_string(task->config, task->filter, bd_config_slang_path);
    bd_arr_ptr_append(args, cr_str_create(mem, compiler_path));

   if (bd_config_has_value(task->config, task->filter, bd_config_slang_flags)) {
        for (uint32_t i = 0; i < bd_config_get_array_count(task->config, task->filter, bd_config_slang_flags); i++) {
            const char* flag = bd_config_get_array_string(task->config, task->filter, bd_config_slang_flags, i);
            bd_arr_ptr_append(args, cr_str_create(mem, flag));
        }
    }

    switch (bd_filter_platform(task->filter)) {
    case bd_platform_macos:
    case bd_platform_ios: {
        bd_arr_ptr_append(args, cr_str_create(mem, "-target"));
        bd_arr_ptr_append(args, cr_str_create(mem, "metal"));
    } break;
    case bd_platform_wasm: {
        bd_arr_ptr_append(args, cr_str_create(mem, "-target"));
        bd_arr_ptr_append(args, cr_str_create(mem, "wgsl"));
    } break;
    case bd_platform_linux:
        assert(0);
    default:
        assert(0);
    }

    bd_arr_ptr_append(args, cr_str_create_copy(mem, task->slang_to_native.slang_path));

    bd_arr_ptr_append(args, cr_str_create(mem, "-o"));
    bd_arr_ptr_append(args, cr_str_create_copy(mem, task->slang_to_native.output_path));

    bd_print_args(args, task->output);

    int result = bd_exec(args, task->output);

    if (result == 0) {
        switch (bd_filter_platform(task->filter)) {
        case bd_platform_macos:
        case bd_platform_ios: {
            bd_slang_metal_fixup(mem, task->slang_to_native.output_path);
        } break;
        default:
            break;
        }
    }

    return result;
}

static int bd_func_metal_src_to_object(struct bd_task* task) {
    struct cr_mem* mem = task->bd->mem;
    struct bd_arr_ptr* args = bd_arr_ptr_create(mem);

    bd_arr_ptr_append(args, cr_str_create(mem, "xcrun"));
    bd_arr_ptr_append(args, cr_str_create(mem, "metal"));

    if ((bd_filter_platform(task->filter) & bd_platform_ios) && bd_config_has_value(task->config, task->filter, bd_config_min_ios_version)) {
        bd_arr_ptr_append(args, cr_str_create_format(mem, "-mios-version-min=%s", bd_ios_version_label(bd_config_get_int(task->config, task->filter, bd_config_min_ios_version))));
    }
    if ((bd_filter_platform(task->filter) & bd_platform_macos) && bd_config_has_value(task->config, task->filter, bd_config_min_macos_version)) {
        bd_arr_ptr_append(args, cr_str_create_format(mem, "-mmacosx-version-min=%s", bd_macos_version_label(bd_config_get_int(task->config, task->filter, bd_config_min_macos_version))));
    }

    if (bd_config_has_value(task->config, task->filter, bd_config_metal_optimize)) {
        bd_arr_ptr_append(args, cr_str_create_format(mem, "-O%d", bd_config_get_int(task->config, task->filter, bd_config_metal_optimize)));
    }

    if (bd_config_has_value(task->config, task->filter, bd_config_metal_include_paths)) {
        for (uint32_t i = 0; i < bd_config_get_array_count(task->config, task->filter, bd_config_metal_include_paths); i++) {
            const char* path = bd_config_get_array_string(task->config, task->filter, bd_config_metal_include_paths, i);
            bd_arr_ptr_append(args, cr_str_create_format(mem, "-I%s", path));
        }
    }

   if (bd_config_has_value(task->config, task->filter, bd_config_metal_compiler_flags)) {
        for (uint32_t i = 0; i < bd_config_get_array_count(task->config, task->filter, bd_config_metal_compiler_flags); i++) {
            const char* flag = bd_config_get_array_string(task->config, task->filter, bd_config_metal_compiler_flags, i);
            bd_arr_ptr_append(args, cr_str_create(mem, flag));
        }
    }

    bd_arr_ptr_append(args, cr_str_create(mem, "-c"));
    bd_arr_ptr_append(args, cr_str_create_copy(mem, task->metal_src_to_object.src_path));

    bd_arr_ptr_append(args, cr_str_create(mem, "-o"));
    bd_arr_ptr_append(args, cr_str_create_copy(mem, task->metal_src_to_object.object_path));

    bd_print_args(args, task->output);

    return bd_exec(args, task->output);
}

static int bd_func_metal_link_objects(struct bd_task* task) {
    struct cr_mem* mem = task->bd->mem;
    struct bd_arr_ptr* args = bd_arr_ptr_create(mem);

    bd_arr_ptr_append(args, cr_str_create(mem, "xcrun"));
    bd_arr_ptr_append(args, cr_str_create(mem, "metallib"));

    for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(task->metal_link_objects.object_paths); i < i_end; i++) {
        struct cr_str* value = bd_arr_ptr_get(task->metal_link_objects.object_paths, i);
        bd_arr_ptr_append(args, cr_str_create_copy(mem, value));
    }

    bd_arr_ptr_append(args, cr_str_create(mem, "-o"));
    bd_arr_ptr_append(args, cr_str_create_copy(mem, task->metal_link_objects.output_path));

    bd_print_args(args, task->output);

    return bd_exec(args, task->output);
}

static int bd_func_wgsl_bundle(struct bd_task* task) {
    struct cr_mem* mem = task->bd->mem;
    struct cr_str* bundle = cr_str_create_empty(mem);
    bool success = true;

    for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(task->wgsl_bundle.input_paths); success && i < i_end; i++) {
        struct cr_str* input_path = bd_arr_ptr_get(task->wgsl_bundle.input_paths, i);
        struct cr_str* module = cr_str_create_with_contents_of_file(mem, input_path);
        if (module == NULL) {
            cr_str_append_format(task->output, "could not read: %s\n", cr_str_get_data(input_path));
            success = false;
            break;
        }
        cr_str_append(bundle, module);
        cr_str_append_data(bundle, "", 1);
        cr_str_destroy(module);
    }

    if (success && !cr_str_write_to_file(bundle, task->wgsl_bundle.output_path)) {
        cr_str_append_format(task->output, "could not write: %s\n", cr_str_get_data(task->wgsl_bundle.output_path));
        success = false;
    }

    cr_str_destroy(bundle);
    return success ? 0 : 1;
}

void bd_func(void* args) {
    struct bd_task* task = args;
    struct cr_mem* mem = task->bd->mem;

    bool success = true;

    for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(task->deps); i < i_end; i++) {
        struct bd_task* dep_task = bd_arr_ptr_get(task->deps, i);
        if (dep_task->success == false) {
            success = false;
            break;
        }
    }

    int (*func)(struct bd_task*) = NULL;
    struct cr_str* desc = NULL;

    switch (task->type) {
    case bd_task_type_clear: {
        func = bd_func_clear;
        desc = cr_str_create_format(mem, "clear \"%s\"", cr_str_get_data(task->clear.products_path));
    } break;
    case bd_task_type_c_src_to_object: {
        func = bd_func_c_src_to_object;
        desc = cr_str_create_format(mem, "c_src_to_object \"%s\"", cr_str_get_data(task->c_src_to_object.src_path));
    } break;
    case bd_task_type_c_link_objects: {
        func = bd_func_c_link_objects;
        desc = cr_str_create_format(mem, "c_link_objects \"%s\"", cr_str_get_data(task->c_link_objects.output_path));
    } break;
    case bd_task_type_c_archive: {
        func = bd_func_c_archive;
        desc = cr_str_create_format(mem, "c_archive \"%s\"", cr_str_get_data(task->c_archive.output_path));
    } break;
    case bd_task_type_swift_module_compile: {
        func = bd_func_swift_module_compile;
        desc = cr_str_create_format(mem, "swift_module_compile \"%s\"", cr_str_get_data(task->swift_module_compile.module_name));
    } break;
    case bd_task_type_slang_to_native: {
        func = bd_func_slang_to_native;
        desc = cr_str_create_format(mem, "slang_to_native");
    } break;
    case bd_task_type_metal_src_to_object: {
        func = bd_func_metal_src_to_object;
        desc = cr_str_create_format(mem, "metal_src_to_object");
    } break;
    case bd_task_type_metal_link_objects: {
        func = bd_func_metal_link_objects;
        desc = cr_str_create_format(mem, "metal_link_objects");
    } break;
    case bd_task_type_wgsl_bundle: {
        func = bd_func_wgsl_bundle;
        desc = cr_str_create_format(mem, "wgsl_bundle \"%s\"", cr_str_get_data(task->wgsl_bundle.output_path));
    } break;
    case bd_task_type_package_copy: {
        func = bd_func_package_copy;
        desc = cr_str_create_format(mem, "package_copy");
    } break;
    case bd_task_type_sign: {
        func = bd_func_sign;
        desc = cr_str_create_format(mem, "sign");
    } break;
    case bd_task_type_run: {
        func = bd_func_run;
        desc = cr_str_create_format(mem, "run \"%s\"", cr_str_get_data(bd_arr_ptr_get(task->run.args, 0)));
    } break;
    case bd_task_type_package_web: {
        func = bd_func_package_web;
        desc = cr_str_create_format(mem, "package_web");
    } break;
    case bd_task_type_package_app: {
        func = bd_func_package_app;
        desc = cr_str_create_format(mem, "package_app");
    } break;
    case bd_task_type_embed: {
        func = bd_func_embed;
        desc = cr_str_create_format(mem, "embed \"%s\"", cr_str_get_data(task->embed.output_path));
    } break;
    }

    bool deps_ok = success;

    double start = cr_get_current_time();
    success = success && func(task) == 0;
    double end = cr_get_current_time();

    if (task->bd->verbose || (deps_ok && !success)) {
        printf("[%s in %f s]%s\n%s", cr_str_get_data(desc), end - start, success ? "" : " FAILED", cr_str_get_data(task->output));
    }

    cr_str_destroy(desc);

    task->success = success;
}

struct bd_task* bd_task_create(struct bd_target_build* target_build, enum bd_task_type type, struct bd_config* config) {
    struct bd_task* task = cr_malloc(target_build->bd->mem, sizeof(*task), 0, cr_loc);

    memset(task, 0, sizeof(*task));
    task->bd = target_build->bd;
    task->type = type;
    task->output = cr_str_create_empty(target_build->bd->mem);
    task->deps = bd_arr_ptr_create(target_build->bd->mem);
    task->config = config;
    task->filter = target_build->filter;
    task->export = target_build->mode == bd_mode_export;

    if (target_build->clear) {
        bd_arr_ptr_append(task->deps, target_build->clear);
    }

    bd_arr_ptr_append(target_build->tasks, task);

    return task;
}

void bd_task_destroy(struct bd_task* task) {
    if (task->cr_task) {
        cr_task_release(task->cr_task);
    }
    cr_str_destroy(task->output);
    switch (task->type) {
    case bd_task_type_clear: {
        cr_str_destroy(task->clear.temp_path);
        cr_str_destroy(task->clear.products_path);
    } break;
    case bd_task_type_c_src_to_object: {
        cr_str_destroy(task->c_src_to_object.src_path);
        cr_str_destroy(task->c_src_to_object.object_path);
    } break;
    case bd_task_type_c_link_objects: {
        for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(task->c_link_objects.object_paths); i < i_end; i++) {
            cr_str_destroy(bd_arr_ptr_get(task->c_link_objects.object_paths, i));
        }
        bd_arr_ptr_destroy(task->c_link_objects.object_paths);
        cr_str_destroy(task->c_link_objects.output_path);
    } break;
    case bd_task_type_c_archive: {
        for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(task->c_archive.object_paths); i < i_end; i++) {
            cr_str_destroy(bd_arr_ptr_get(task->c_archive.object_paths, i));
        }
        bd_arr_ptr_destroy(task->c_archive.object_paths);
        for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(task->c_archive.archive_paths); i < i_end; i++) {
            cr_str_destroy(bd_arr_ptr_get(task->c_archive.archive_paths, i));
        }
        bd_arr_ptr_destroy(task->c_archive.archive_paths);
        cr_str_destroy(task->c_archive.output_path);
        cr_str_destroy(task->c_archive.script_path);
    } break;
    case bd_task_type_swift_module_compile: {
        for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(task->swift_module_compile.src_paths); i < i_end; i++) {
            cr_str_destroy(bd_arr_ptr_get(task->swift_module_compile.src_paths, i));
        }
        bd_arr_ptr_destroy(task->swift_module_compile.src_paths);
        for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(task->swift_module_compile.import_paths); i < i_end; i++) {
            cr_str_destroy(bd_arr_ptr_get(task->swift_module_compile.import_paths, i));
        }
        bd_arr_ptr_destroy(task->swift_module_compile.import_paths);
        cr_str_destroy(task->swift_module_compile.module_name);
        cr_str_destroy(task->swift_module_compile.output_a_path);
        cr_str_destroy(task->swift_module_compile.output_module_path);
    } break;
    case bd_task_type_slang_to_native: {
        cr_str_destroy(task->slang_to_native.slang_path);
        cr_str_destroy(task->slang_to_native.output_path);
    } break;
    case bd_task_type_metal_src_to_object: {
        cr_str_destroy(task->metal_src_to_object.src_path);
        cr_str_destroy(task->metal_src_to_object.object_path);
    } break;
    case bd_task_type_metal_link_objects: {
        for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(task->metal_link_objects.object_paths); i < i_end; i++) {
            cr_str_destroy(bd_arr_ptr_get(task->metal_link_objects.object_paths, i));
        }
        bd_arr_ptr_destroy(task->metal_link_objects.object_paths);
        cr_str_destroy(task->metal_link_objects.output_path);
    } break;
    case bd_task_type_wgsl_bundle: {
        for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(task->wgsl_bundle.input_paths); i < i_end; i++) {
            cr_str_destroy(bd_arr_ptr_get(task->wgsl_bundle.input_paths, i));
        }
        bd_arr_ptr_destroy(task->wgsl_bundle.input_paths);
        cr_str_destroy(task->wgsl_bundle.output_path);
    } break;
    case bd_task_type_sign: {
        cr_str_destroy(task->sign.exec_path);
        if (task->sign.entitlements_path) {
            cr_str_destroy(task->sign.entitlements_path);
        }
    } break;
    case bd_task_type_run: {
        for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(task->run.args); i < i_end; i++) {
            cr_str_destroy(bd_arr_ptr_get(task->run.args, i));
        }
        bd_arr_ptr_destroy(task->run.args);
    } break;
    case bd_task_type_package_copy: {
        for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(task->package_copy.input_paths); i < i_end; i++) {
            cr_str_destroy(bd_arr_ptr_get(task->package_copy.input_paths, i));
        }
        bd_arr_ptr_destroy(task->package_copy.input_paths);
        for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(task->package_copy.output_paths); i < i_end; i++) {
            cr_str_destroy(bd_arr_ptr_get(task->package_copy.output_paths, i));
        }
        bd_arr_ptr_destroy(task->package_copy.output_paths);
        cr_str_destroy(task->package_copy.dir_path);
    } break;
    case bd_task_type_package_web: {
        for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(task->package_web.input_paths); i < i_end; i++) {
            cr_str_destroy(bd_arr_ptr_get(task->package_web.input_paths, i));
        }
        bd_arr_ptr_destroy(task->package_web.input_paths);
        cr_str_destroy(task->package_web.temp_path);
        cr_str_destroy(task->package_web.public_path);
        if (task->package_web.index_path) {
            cr_str_destroy(task->package_web.index_path);
        }
    } break;
    case bd_task_type_package_app: {
        for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(task->package_app.resource_paths); i < i_end; i++) {
            cr_str_destroy(bd_arr_ptr_get(task->package_app.resource_paths, i));
        }
        bd_arr_ptr_destroy(task->package_app.resource_paths);
        cr_str_destroy(task->package_app.exec_path);
        if (task->package_app.entitlements_path) {
            cr_str_destroy(task->package_app.entitlements_path);
        }
        cr_str_destroy(task->package_app.app_path);
    } break;
    case bd_task_type_embed: {
        for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(task->embed.input_paths); i < i_end; i++) {
            cr_str_destroy(bd_arr_ptr_get(task->embed.input_paths, i));
        }
        bd_arr_ptr_destroy(task->embed.input_paths);
        cr_str_destroy(task->embed.output_path);
        cr_str_destroy(task->embed.symbol_prefix);
    } break;
    }
    bd_arr_ptr_destroy(task->deps);
    cr_free(task->bd->mem, task, cr_loc);
}
