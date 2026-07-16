#include "bd_private.h"
#include <stdio.h>
#include <string.h>

void bd_export_check(struct bd* bd, struct bd_target* target, uint32_t filter) {
    struct cr_mem* mem = bd->mem;
    struct bd_config* config = target->config;
    uint32_t platform = bd_filter_platform(filter);
    const char* name = cr_str_get_data(target->name);

    if (bd_filter_profile(filter) != bd_profile_release) {
        bd_fail("--export needs --profile=release");
    }
    if (platform != bd_platform_macos && platform != bd_platform_ios) {
        bd_fail("--export supports macos and ios, not %s", bd_platform_label(platform));
    }
    if (!bd_target_is_program(target)) {
        bd_fail("--export needs an app or an executable, not the %s target %s", bd_target_type_label(target->type), name);
    }

    bool app_store = bd_export_is_app_store(config, filter);
    if (target->type != bd_target_type_app) {
        if (platform == bd_platform_ios) bd_fail("--export on ios needs an app, not the executable %s", name);
        if (app_store) bd_fail("the App Store takes apps only, not the executable %s, set bd_config_export to online", name);
    }
    if (platform == bd_platform_ios && !app_store) {
        bd_fail("an iOS app exports for the App Store only, set bd_config_export to app_store");
    }

    if (app_store) {
        bd_config_require_string(config, filter, bd_config_apple_distribution_profile, "an App Store export needs bd_config_apple_distribution_profile, the name of an App Store provisioning profile");
        if (platform == bd_platform_macos) {
            bd_config_require_string(config, filter, bd_config_apple_installer_identity, "a Mac App Store export needs bd_config_apple_installer_identity, the name of the installer signing identity");
        }
    } else {
        bd_config_require_string(config, filter, bd_config_apple_notary_profile, "an online export needs bd_config_apple_notary_profile, a keychain profile from xcrun notarytool store-credentials");
    }

    struct cr_str* output = cr_str_create_empty(mem);
    struct cr_str* identity = bd_apple_identity(mem, bd_export_identity_name(config, filter), output);
    if (identity == NULL) {
        bd_fail("%s", cr_str_get_data(output));
    }
    cr_str_destroy(identity);
    if (app_store) {
        struct cr_str* profile = bd_apple_provisioning_profile(mem, bd_config_get_string(config, filter, bd_config_apple_distribution_profile), output);
        if (profile == NULL) {
            bd_fail("%s", cr_str_get_data(output));
        }
        cr_str_destroy(profile);
    }
    cr_str_destroy(output);
}

static struct cr_str* bd_export_file(struct bd* bd, struct bd_target* target, uint32_t filter, const char* extension) {
    struct cr_str* path = bd_products_path(bd, target, filter);
    const char* name = target->type == bd_target_type_app ? cr_str_get_data(target->name) : bd_executable_name(target, filter);
    cr_str_append_path_component_format(path, "%s.%s", name, extension);
    return path;
}

static struct bd_arr_ptr* bd_export_zip_command(struct cr_mem* mem, struct cr_str* source, struct cr_str* zip, bool keep_parent) {
    struct bd_arr_ptr* args = bd_args_create(mem, "ditto", "-c", "-k", "--norsrc", NULL);
    if (keep_parent) {
        bd_arr_ptr_append(args, cr_str_create(mem, "--keepParent"));
    }
    bd_arr_ptr_append(args, cr_str_create_copy(mem, source));
    bd_arr_ptr_append(args, zip);
    return args;
}

static void bd_export_ipa_commands(struct bd* bd, struct bd_target* target, uint32_t filter, struct bd_arr_ptr* commands) {
    struct cr_mem* mem = bd->mem;
    struct cr_str* root = bd_temp_path(bd, target, filter);
    cr_str_append_path_component_cstr(root, "ipa");
    struct cr_str* payload_app = cr_str_create_copy(mem, root);
    cr_str_append_path_component_format(payload_app, "Payload/%s.app", cr_str_get_data(target->name));

    struct bd_arr_ptr* copy = bd_args_create(mem, "ditto", NULL);
    bd_arr_ptr_append(copy, bd_app_path(bd, target, filter));
    bd_arr_ptr_append(copy, payload_app);
    bd_arr_ptr_append(commands, copy);
    bd_arr_ptr_append(commands, bd_export_zip_command(mem, root, bd_export_file(bd, target, filter, "ipa"), false));

    cr_str_destroy(root);
}

static void bd_export_pkg_commands(struct bd* bd, struct bd_target* target, uint32_t filter, struct bd_arr_ptr* commands) {
    struct cr_mem* mem = bd->mem;
    struct bd_arr_ptr* build = bd_args_create(mem, "productbuild", "--component", NULL);
    bd_arr_ptr_append(build, bd_app_path(bd, target, filter));
    bd_arr_ptr_append(build, cr_str_create(mem, "/Applications"));
    bd_arr_ptr_append(build, cr_str_create(mem, "--sign"));
    bd_arr_ptr_append(build, cr_str_create(mem, bd_config_get_string(target->config, filter, bd_config_apple_installer_identity)));
    bd_arr_ptr_append(build, bd_export_file(bd, target, filter, "pkg"));
    bd_arr_ptr_append(commands, build);
}

static void bd_export_notarized_commands(struct bd* bd, struct bd_target* target, uint32_t filter, struct bd_arr_ptr* commands) {
    struct cr_mem* mem = bd->mem;
    bool app = target->type == bd_target_type_app;
    struct cr_str* product = app ? bd_app_path(bd, target, filter) : bd_program_path(bd, target, filter);
    struct cr_str* submission = bd_temp_path(bd, target, filter);
    cr_str_append_path_component_format(submission, "%s.zip", cr_str_get_data(target->name));

    bd_arr_ptr_append(commands, bd_export_zip_command(mem, product, cr_str_create_copy(mem, submission), true));

    struct bd_arr_ptr* notarize = bd_args_create(mem, "xcrun", "notarytool", "submit", NULL);
    bd_arr_ptr_append(notarize, submission);
    bd_arr_ptr_append(notarize, cr_str_create(mem, "--keychain-profile"));
    bd_arr_ptr_append(notarize, cr_str_create(mem, bd_config_get_string(target->config, filter, bd_config_apple_notary_profile)));
    bd_arr_ptr_append(notarize, cr_str_create(mem, "--wait"));
    bd_arr_ptr_append(commands, notarize);

    if (app) {
        struct bd_arr_ptr* staple = bd_args_create(mem, "xcrun", "stapler", "staple", NULL);
        bd_arr_ptr_append(staple, cr_str_create_copy(mem, product));
        bd_arr_ptr_append(commands, staple);
    }

    bd_arr_ptr_append(commands, bd_export_zip_command(mem, product, bd_export_file(bd, target, filter, "zip"), true));

    cr_str_destroy(product);
}

void bd_export_commands(struct bd* bd, struct bd_target* target, uint32_t filter, struct bd_arr_ptr* commands) {
    if (bd_filter_platform(filter) == bd_platform_ios) {
        bd_export_ipa_commands(bd, target, filter, commands);
    } else if (bd_export_is_app_store(target->config, filter)) {
        bd_export_pkg_commands(bd, target, filter, commands);
    } else {
        bd_export_notarized_commands(bd, target, filter, commands);
    }
}
