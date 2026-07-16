#include "jg.h"

struct cr_mem* jg_mem;
struct jg_model jg_model;
const char* jg_out_dir;
uint32_t jg_error_count;
const char* const jg_abi_labels[jg_abi_count] = { "native", "wasm" };

const struct jg_header jg_headers[] = {
    { "include/app.h", jg_platform_all },
    { "include/app_event.h", jg_platform_all },
    { "include/gpu.h", jg_platform_all },
    { "include/ui.h", jg_platform_all },
    { "include/ui_input.h", jg_platform_all },
    { "include/ui_gestures.h", jg_platform_all },
    { "include/ui_text_input.h", jg_platform_all },
    { "include/ui_list.h", jg_platform_all },
    { "include/ui_physics.h", jg_platform_all },
    { "include/ui_util.h", jg_platform_all },
    { "include/hud.h", jg_platform_all },
    { "include/log.h", jg_platform_all },
    { "include/bd.h", jg_platform_macos | jg_platform_tool },
    { "include/bd_xcode.h", jg_platform_macos | jg_platform_tool },
    { "src/js/js.h", jg_platform_all },
};
const uint32_t jg_header_count = cr_count(jg_headers);

const char* const jg_support_headers[] = { "cr_handle.h", "cr_math.h" };
const uint32_t jg_support_header_count = cr_count(jg_support_headers);

const char* const jg_excludes[] = {
    "ui_key",
    "log_s",
    "bd_config_set",
    "bd_config_get",
    "ui_node_null",
    "ui_key_auto",
    "ui_pointer_null",
};
const uint32_t jg_exclude_count = cr_count(jg_excludes);

const struct jg_scope_rule jg_scope_rules[] = {
    { "app_desc", jg_scope_persistent, "app_get_user(p0)" },
    { "app_window_desc", jg_scope_persistent, "app_window_get_user(p0)" },
    { "ui_custom_desc", jg_scope_tree, "p0->user" },
    { "ui_gesture_desc", jg_scope_owner, NULL },
    { "ui_tap", jg_scope_input, NULL },
    { "ui_long_press", jg_scope_input, NULL },
    { "ui_pan", jg_scope_input, NULL },
    { "ui_hover", jg_scope_input, NULL },
    { "ui_pinch", jg_scope_input, NULL },
    { "ui_rotate", jg_scope_input, NULL },
    { "ui_on_pointer", jg_scope_input, NULL },
    { "ui_on_scroll", jg_scope_input, NULL },
    { "ui_on_text", jg_scope_input, NULL },
    { "ui_list_layout", jg_scope_call, NULL },
    { "ui_list_update", jg_scope_call, NULL },
    { "ui_hit_test", jg_scope_call, NULL },
    { "log_view_glyphs", jg_scope_call, NULL },
    { "app_window_text_input_set_hit_test", jg_scope_replace, NULL },
    { "ui_slide_set_on_slide", jg_scope_replace, NULL },
    { "ui_slide", jg_scope_persistent, NULL },
    { "gpu_device_set_on_completed", jg_scope_replace, NULL },
};
const uint32_t jg_scope_rule_count = cr_count(jg_scope_rules);

const char* const jg_name_overrides[][2] = {
    { "app_key_0", "digit0" }, { "app_key_1", "digit1" }, { "app_key_2", "digit2" }, { "app_key_3", "digit3" }, { "app_key_4", "digit4" },
    { "app_key_5", "digit5" }, { "app_key_6", "digit6" }, { "app_key_7", "digit7" }, { "app_key_8", "digit8" }, { "app_key_9", "digit9" },
    { "bd_ios_16_0", "ios16" }, { "bd_ios_17_0", "ios17" }, { "bd_ios_18_0", "ios18" }, { "bd_ios_26_0", "ios26" },
    { "bd_macos_13_0", "macos13" }, { "bd_macos_14_0", "macos14" }, { "bd_macos_15_0", "macos15" }, { "bd_macos_26_0", "macos26" },
};
const uint32_t jg_name_override_count = cr_count(jg_name_overrides);

static const char* const jg_bundle_web[] = { "src/web/lib.js", "gen_wasm.js", "src/js/runtime.js", "src/js/api.js", "src/js/web.js" };
static const char* const jg_bundle_native[] = { "gen_native.js", "src/js/runtime.js", "src/js/api.js", "src/js/native.js" };

static void jg_print(const char* fmt, va_list args) {
    fprintf(stderr, "js_gen: ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
}

void jg_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    jg_print(fmt, args);
    va_end(args);
    jg_error_count++;
}

void jg_fatal(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    jg_print(fmt, args);
    va_end(args);
    exit(1);
}

const char* jg_strdup(const char* str) {
    size_t len = strlen(str);
    char* copy = cr_malloc(jg_mem, len + 1, 0, cr_loc);
    memcpy(copy, str, len + 1);
    return copy;
}

bool jg_starts_with(const char* str, const char* prefix) {
    return strncmp(str, prefix, strlen(prefix)) == 0;
}

bool jg_ends_with(const char* str, const char* suffix) {
    size_t len = strlen(str);
    size_t suffix_len = strlen(suffix);
    return len >= suffix_len && strcmp(str + len - suffix_len, suffix) == 0;
}

void jg_camel(const char* snake, char* out, size_t out_size, bool pascal) {
    size_t o = 0;
    bool upper = pascal;
    for (const char* p = snake; *p != 0 && o + 1 < out_size; p++) {
        if (*p == '_') {
            upper = true;
            continue;
        }
        char c = *p;
        if (upper && c >= 'a' && c <= 'z') {
            c = (char)(c - 'a' + 'A');
        }
        out[o++] = c;
        upper = false;
    }
    out[o] = 0;
}

const char* jg_base_name(const char* path) {
    const char* slash = strrchr(path, '/');
    return slash != NULL ? slash + 1 : path;
}

struct cr_str* jg_read_file(const char* path) {
    struct cr_str* path_str = cr_str_create(jg_mem, path);
    struct cr_str* text = cr_str_create_with_contents_of_file(jg_mem, path_str);
    cr_str_destroy(path_str);
    if (text == NULL) {
        jg_fatal("could not read %s", path);
    }
    return text;
}

void jg_write_output(const char* name, struct cr_str* text) {
    struct cr_str* path = cr_str_create_format(jg_mem, "%s/%s", jg_out_dir, name);
    if (cr_str_write_to_file(text, path) == false) {
        jg_fatal("could not write %s", cr_str_get_data(path));
    }
    cr_str_destroy(path);
}

bool jg_is_excluded(const char* name) {
    for (uint32_t i = 0; i < jg_exclude_count; i++) {
        if (strcmp(jg_excludes[i], name) == 0) {
            return true;
        }
    }
    return false;
}

uint32_t jg_header_platforms(const char* file) {
    for (uint32_t i = 0; i < jg_header_count; i++) {
        if (strcmp(file, jg_base_name(jg_headers[i].path)) == 0) {
            return jg_headers[i].platforms;
        }
    }
    for (uint32_t i = 0; i < jg_support_header_count; i++) {
        if (strcmp(file, jg_support_headers[i]) == 0) {
            return jg_platform_all;
        }
    }
    return 0;
}

uint32_t jg_abi_platforms(enum jg_abi abi) {
    return abi == jg_abi_wasm ? jg_platform_wasm : jg_platform_macos | jg_platform_ios;
}

const char* jg_hand_written_text(void) {
    static struct cr_str* source;
    if (source == NULL) {
        source = jg_read_file("src/js/api.js");
        struct cr_str* runtime = jg_read_file("src/js/runtime.js");
        cr_str_append(source, runtime);
        cr_str_destroy(runtime);
    }
    return cr_str_get_data(source);
}

bool jg_has_identifier(const char* text, const char* name) {
    size_t len = strlen(name);
    for (const char* at = strstr(text, name); at != NULL; at = strstr(at + 1, name)) {
        char after = at[len];
        bool before_ok = at == text || (isalnum((unsigned char)at[-1]) == 0 && at[-1] != '_');
        if (before_ok && isalnum((unsigned char)after) == 0 && after != '_') {
            return true;
        }
    }
    return false;
}

static bool jg_api_defines(const char* ns, const char* js) {
    char line[jg_name * 2];
    snprintf(line, sizeof(line), "\n%s.%s = ", ns, js);
    return strstr(jg_hand_written_text(), line) != NULL;
}

static void jg_check_coverage(void) {
    for (uint32_t i = 0; i < jg_model.func_count; i++) {
        struct jg_func* func = &jg_model.funcs[i];
        if (func->is_helper && jg_api_defines(func->ns, func->js) == false) {
            jg_error("%s is an inline helper; define %s.%s in src/js/api.js or exclude it", func->name, func->ns, func->js);
        }
    }
    for (uint32_t i = 0; i < jg_model.macro_count; i++) {
        struct jg_macro* macro = &jg_model.macros[i];
        if (macro->is_number || jg_is_excluded(macro->name)) {
            continue;
        }
        if (jg_api_defines(macro->ns, macro->js) == false) {
            jg_error("%s is a macro; define %s.%s in src/js/api.js or exclude it", macro->name, macro->ns, macro->js);
        }
    }
    for (uint32_t i = 0; i < jg_exclude_count; i++) {
        bool known = jg_find_func(jg_excludes[i]) != NULL;
        for (uint32_t m = 0; known == false && m < jg_model.macro_count; m++) {
            known = strcmp(jg_model.macros[m].name, jg_excludes[i]) == 0;
        }
        if (known == false) {
            jg_error("exclude entry %s is neither a function nor a macro", jg_excludes[i]);
        }
    }
}

static void jg_emit_bundle(const char* const* parts, uint32_t count, const char* path) {
    struct cr_str* out = cr_str_create(jg_mem, "(function () {\n'use strict';\n\nconst uniform = globalThis.uniform || (globalThis.uniform = {});\nconst app = {};\nconst gpu = {};\nconst ui = {};\nconst hud = {};\nconst log = {};\nconst bd = {};\n");
    for (uint32_t i = 0; i < count; i++) {
        char part[1024];
        if (jg_starts_with(parts[i], "gen_")) {
            snprintf(part, sizeof(part), "%s/%s", jg_out_dir, parts[i]);
        } else {
            snprintf(part, sizeof(part), "%s", parts[i]);
        }
        struct cr_str* text = jg_read_file(part);
        const char* data = cr_str_get_data(text);
        const char* strict = "'use strict';\n";
        if (jg_starts_with(data, strict)) {
            data += strlen(strict);
        }
        cr_str_append_cstr(out, "\n");
        cr_str_append_cstr(out, data);
        cr_str_destroy(text);
    }
    cr_str_append_cstr(out, "})();\n");
    jg_write_output(path, out);
    cr_str_destroy(out);
}

__attribute__((noreturn)) static void jg_usage(void) {
    jg_fatal("usage: js_gen --out <dir> --native <cc> <flags...> --wasm <cc> <flags...>");
}

static bool jg_is_option(const char* arg) {
    return strcmp(arg, "--out") == 0 || strcmp(arg, "--native") == 0 || strcmp(arg, "--wasm") == 0;
}

int main(int argc, char** argv) {
    jg_mem = cr_mem_libc;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
            jg_out_dir = argv[++i];
        } else if (strcmp(argv[i], "--native") == 0 || strcmp(argv[i], "--wasm") == 0) {
            int end = i + 1;
            while (end < argc && jg_is_option(argv[end]) == false) {
                end++;
            }
            jg_parse_set_args(strcmp(argv[i], "--native") == 0 ? jg_abi_native : jg_abi_wasm, argv + i + 1, (uint32_t)(end - i - 1));
            i = end - 1;
        } else {
            jg_usage();
        }
    }
    if (jg_out_dir == NULL) {
        jg_usage();
    }

    jg_parse();
    jg_analyze();
    jg_check_coverage();
    if (jg_error_count > 0) {
        jg_fatal("%u errors", jg_error_count);
    }

    jg_emit_c();
    jg_emit_jsc();
    jg_emit_js(jg_abi_native, "gen_native.js");
    jg_emit_js(jg_abi_wasm, "gen_wasm.js");
    jg_emit_dts();
    jg_emit_bundle(jg_bundle_web, cr_count(jg_bundle_web), "uniform.js");
    jg_emit_bundle(jg_bundle_native, cr_count(jg_bundle_native), "uniform_native.js");
    if (jg_error_count > 0) {
        jg_fatal("%u errors", jg_error_count);
    }
    return 0;
}
