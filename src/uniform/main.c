#include "uniform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if uniform_tool

extern const unsigned char* const types_dts_data[];
extern const unsigned long long types_dts_sizes[];

static void uniform_write_default_script(struct cr_str* path);

static int uniform_usage(void) {
    fprintf(stderr, "usage: uniform <script.js> [args...]\n       uniform --default [args...]\n       uniform - [args...]\n       uniform --serve [--port <port>] [--bind <address>] [--device[=<name>]] <script.js> [args...]\n       uniform --device[=<name>] <script.js> [args...]\n       uniform --certificate [--device[=<name>]]\n       uniform --types\n");
    return 1;
}

static int uniform_command(int argc, char** argv) {
    bool serve = false;
    bool certificate = false;
    bool default_script = false;
    int port = 9443;
    const char* bind_address = NULL;
    const char* device = NULL;
    int i = 1;
    for (; i < argc && argv[i][0] == '-' && strcmp(argv[i], "-") != 0; i++) {
        if (strcmp(argv[i], "--types") == 0) {
            fwrite(types_dts_data[0], 1, (size_t)types_dts_sizes[0], stdout);
            return 0;
        } else if (strcmp(argv[i], "--serve") == 0) {
            serve = true;
        } else if (strcmp(argv[i], "--certificate") == 0) {
            certificate = true;
        } else if (strcmp(argv[i], "--default") == 0) {
            default_script = true;
        } else if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--bind") == 0 && i + 1 < argc) {
            bind_address = argv[++i];
        } else if (strcmp(argv[i], "--device") == 0) {
            device = "";
        } else if (strncmp(argv[i], "--device=", 9) == 0) {
            device = argv[i] + 9;
        } else {
            return uniform_usage();
        }
    }
    if (certificate) {
        if (serve || default_script || i < argc) {
            return uniform_usage();
        }
        return uniform_certificate(device);
    }
    const char* script_path = "main.js";
    int script_args = i;
    if (default_script) {
        uint8_t raw[cr_str_size];
        struct cr_str* path = cr_str_temp(raw, (char*)script_path);
        if (cr_util_does_file_exist(path) == false) {
            uniform_write_default_script(path);
        }
    } else if (i < argc) {
        script_path = argv[i];
        script_args = i + 1;
    } else {
        return uniform_usage();
    }
    if (serve) {
        return uniform_serve(script_path, port, bind_address, device, argc, argv, script_args);
    }
    if (device) {
        return uniform_device(script_path, device, argc, argv, script_args);
    }
    return uniform_run(script_path, argc, argv, script_args);
}

#endif

extern const unsigned char* const default_js_data[];
extern const unsigned long long default_js_sizes[];

static void uniform_write_default_script(struct cr_str* path) {
    struct cr_str* data = cr_str_create_data(cr_mem_libc, (const char*)default_js_data[0], (uint32_t)default_js_sizes[0]);
    cr_str_write_to_file(data, path);
    cr_str_destroy(data);
}

#if cr_ios
#include <CoreFoundation/CoreFoundation.h>

static bool uniform_app_editable_script(void) {
    CFBundleRef bundle = CFBundleGetMainBundle();
    CFTypeRef value = bundle != NULL ? CFBundleGetValueForInfoDictionaryKey(bundle, CFSTR("UniformEditableScript")) : NULL;
    return value != NULL && CFGetTypeID(value) == CFBooleanGetTypeID() && CFBooleanGetValue(value);
}
#else
static bool uniform_app_editable_script(void) {
    return false;
}
#endif

static void uniform_seed_script(struct cr_str* bundled, struct cr_str* script) {
    struct cr_str* source = cr_str_create_with_contents_of_file(cr_mem_libc, bundled);
    if (source == NULL) {
        return;
    }
    struct cr_str* seeded_path = cr_str_create_format(cr_mem_libc, "%s/Library/Application Support/main.js", getenv("HOME"));
    struct cr_str* seeded = cr_str_create_with_contents_of_file(cr_mem_libc, seeded_path);
    if (seeded == NULL || cr_str_is_equal(seeded, source) == false || cr_util_does_file_exist(script) == false) {
        struct cr_str* seeded_dir = cr_str_create_copy(cr_mem_libc, seeded_path);
        cr_str_remove_last_path_component(seeded_dir);
        cr_util_create_dir(seeded_dir);
        cr_str_write_to_file(source, script);
        cr_str_write_to_file(source, seeded_path);
        cr_str_destroy(seeded_dir);
    }
    if (seeded != NULL) {
        cr_str_destroy(seeded);
    }
    cr_str_destroy(seeded_path);
    cr_str_destroy(source);
}

static struct cr_str* uniform_app_script(void) {
    struct cr_str* bundled = cr_util_get_resources_dir(cr_mem_libc);
    if (bundled == NULL) {
        return NULL;
    }
    cr_str_append_path_component_cstr(bundled, "main.js");
    bool has_bundled = cr_util_does_file_exist(bundled);
    if (has_bundled && uniform_app_editable_script() == false) {
        return bundled;
    }
    struct cr_str* script = cr_util_get_documents_dir(cr_mem_libc);
    cr_util_create_dir(script);
    cr_str_append_path_component_cstr(script, "main.js");
    if (has_bundled) {
        uniform_seed_script(bundled, script);
    } else if (cr_util_does_file_exist(script) == false) {
        uniform_write_default_script(script);
    }
    cr_str_destroy(bundled);
    return script;
}

int main(int argc, char** argv) {
    setbuf(stdout, NULL);

    struct cr_str* script = uniform_app_script();
    if (script != NULL) {
        int status = uniform_run(cr_str_get_data(script), argc, argv, 1);
        cr_str_destroy(script);
        return status;
    }

#if uniform_tool
    return uniform_command(argc, argv);
#else
    return 1;
#endif
}
