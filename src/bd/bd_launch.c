#include "bd_private.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t bd_device_list(struct cr_mem* mem, const char* filter, struct cr_str** identifier, struct cr_str* list) {
    struct bd_arr_ptr* args = bd_args_create(mem, "xcrun", "devicectl", "list", "devices", "--filter", filter,
        "--hide-default-columns", "--columns", "Identifier", "--hide-headers", NULL);
    struct cr_str* output = cr_str_create_empty(mem);
    int code = cr_exec((struct cr_str**)bd_arr_ptr_get_data(args), bd_arr_ptr_get_count(args), output, NULL);
    bd_args_destroy(args);
    if (code != 0) {
        bd_fail("cannot list the devices with xcrun devicectl");
    }

    uint32_t count = 0;
    for (const char* line = cr_str_get_data(output); *line != 0;) {
        const char* end = strchr(line, '\n');
        size_t size = end != NULL ? (size_t)(end - line) : strlen(line);
        const char* annotation = memmem(line, size, " (", 2);
        if (annotation != NULL) {
            if (*identifier == NULL) {
                *identifier = cr_str_create_data(mem, line, (uint32_t)(annotation - line));
            }
            cr_str_append_format(list, "%s%.*s", count > 0 ? ", " : "", (int)(annotation - line), line);
            count++;
        }
        line = end != NULL ? end + 1 : line + size;
    }
    cr_str_destroy(output);
    return count;
}

struct cr_str* bd_device_resolve(struct cr_mem* mem, const char* device) {
    if (device[0] != 0) {
        return cr_str_create(mem, device);
    }

    struct cr_str* identifier = NULL;
    struct cr_str* list = cr_str_create_empty(mem);
    uint32_t count = bd_device_list(mem, "connectionProperties.tunnelState == 'connected' AND hardwareProperties.reality == 'physical'", &identifier, list);
    if (count == 0) {
        count = bd_device_list(mem, "connectionProperties.transportType == 'wired' AND hardwareProperties.reality == 'physical'", &identifier, list);
    }
    if (count == 0) {
        bd_fail("no device is connected, connect one or name one with --device=<name>, see xcrun devicectl list devices");
    }
    if (count > 1) {
        bd_fail("%u devices are connected, %s, name one with --device=<name>", count, cr_str_get_data(list));
    }
    cr_str_destroy(list);
    return identifier;
}

static struct cr_str* bd_launch_device(struct bd* bd, struct bd_target* target, uint32_t filter) {
    if (bd->device == NULL) {
        const char* device = bd_config_has_value(target->config, filter, bd_config_run_device) ? bd_config_get_string(target->config, filter, bd_config_run_device) : "";
        bd->device = bd_device_resolve(bd->mem, device);
    }
    return bd->device;
}

static void bd_launch_append_args(struct bd* bd, struct bd_target* target, uint32_t filter, struct bd_arr_ptr* extra, struct bd_arr_ptr* args) {
    struct cr_str* products_path = bd_products_path(bd, target, filter);
    bd_config_args_expand(bd, target->config, filter, bd_config_run_args, products_path, args);
    cr_str_destroy(products_path);
    for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(extra); i < i_end; i++) {
        bd_arr_ptr_append(args, cr_str_create_copy(bd->mem, bd_arr_ptr_get(extra, i)));
    }
}

static int bd_launch_local(struct bd* bd, struct bd_target* target, uint32_t filter, struct bd_arr_ptr* extra) {
    struct bd_arr_ptr* args = bd_arr_ptr_create(bd->mem);
    bd_arr_ptr_append(args, bd_program_path(bd, target, filter));
    bd_launch_append_args(bd, target, filter, extra, args);
    return bd_exec_inherit(args);
}

static int bd_launch_ios(struct bd* bd, struct bd_target* target, uint32_t filter, struct bd_arr_ptr* extra) {
    struct cr_mem* mem = bd->mem;
    struct bd_arr_ptr* args = bd_args_create(mem, "xcrun", "devicectl", "device", "process", "launch", "--console", "--terminate-existing", "--device", NULL);
    bd_arr_ptr_append(args, cr_str_create_copy(mem, bd_launch_device(bd, target, filter)));
    bd_arr_ptr_append(args, cr_str_create(mem, bd_config_get_string(target->config, filter, bd_config_apple_bundle_identifier)));
    bd_launch_append_args(bd, target, filter, extra, args);
    return bd_exec_inherit(args);
}

static int bd_launch_wasm(struct bd* bd, struct bd_target* target, uint32_t filter, struct bd_arr_ptr* extra) {
    struct cr_mem* mem = bd->mem;
    struct bd_arr_ptr* args = bd_arr_ptr_create(mem);
    bd_launch_append_args(bd, target, filter, extra, args);
    struct cr_str* path = bd_serve_path(mem, args);
    bd_args_destroy(args);

    int result = bd_serve(bd, target, filter, cr_str_get_data(path));
    cr_str_destroy(path);
    return result;
}

void bd_install_check(struct bd* bd, struct bd_target* target, uint32_t filter) {
    if (bd_filter_platform(filter) != bd_platform_ios || target->type != bd_target_type_app) {
        bd_fail("--install needs an iOS app, not the %s target %s on %s", bd_target_type_label(target->type), cr_str_get_data(target->name), bd_platform_label(bd_filter_platform(filter)));
    }
}

void bd_install_commands(struct bd* bd, struct bd_target* target, uint32_t filter, struct bd_arr_ptr* commands) {
    struct cr_mem* mem = bd->mem;
    if (bd_filter_platform(filter) != bd_platform_ios) {
        return;
    }
    struct bd_arr_ptr* args = bd_args_create(mem, "xcrun", "devicectl", "device", "install", "app", "--device", NULL);
    bd_arr_ptr_append(args, cr_str_create_copy(mem, bd_launch_device(bd, target, filter)));
    bd_arr_ptr_append(args, bd_app_path(bd, target, filter));
    bd_arr_ptr_append(commands, args);
}

void bd_launch_check(struct bd* bd, struct bd_target* target, uint32_t filter) {
    uint32_t platform = bd_filter_platform(filter);
    if (platform == bd_platform_linux) {
        bd_fail("--run supports macos, ios and wasm, not linux");
    }
    if (!bd_target_is_program(target)) {
        bd_fail("--run needs an app or an executable, not the %s target %s", bd_target_type_label(target->type), cr_str_get_data(target->name));
    }
    if (platform != bd_platform_macos && target->type != bd_target_type_app) {
        bd_fail("--run on %s needs an app, not the executable %s", bd_platform_label(platform), cr_str_get_data(target->name));
    }
}

int bd_launch(struct bd* bd, struct bd_target* target, uint32_t filter, struct bd_arr_ptr* extra) {
    switch (bd_filter_platform(filter)) {
    case bd_platform_macos: return bd_launch_local(bd, target, filter, extra);
    case bd_platform_ios: return bd_launch_ios(bd, target, filter, extra);
    case bd_platform_wasm: return bd_launch_wasm(bd, target, filter, extra);
    default: assert(0);
    }
}
