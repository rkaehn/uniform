#include "bd_xcode.h"
#include "bd_private.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct bd_xcode_dependency {
    struct bd_target* target;
    struct cr_str* project;
    struct cr_str* target_name;
    uint32_t filter;
    bool embed;
};

struct bd_xcode {
    struct bd* bd;
    struct cr_str* project_name;
    struct cr_str* command_prefix;
    struct bd_arr_ptr* group_order;
    struct bd_arr_ptr* targets;
    struct bd_arr_ptr* dependencies;
};

static struct bd_xcode* bd_xcode_state(struct bd* bd) {
    if (bd->xcode == NULL) {
        struct bd_xcode* xcode = cr_malloc(bd->mem, sizeof(*xcode), 0, cr_loc);
        memset(xcode, 0, sizeof(*xcode));
        xcode->bd = bd;
        xcode->group_order = bd_arr_ptr_create(bd->mem);
        xcode->targets = bd_arr_ptr_create(bd->mem);
        xcode->dependencies = bd_arr_ptr_create(bd->mem);
        bd->xcode = xcode;
    }
    return bd->xcode;
}

void bd_xcode_destroy(struct bd_xcode* xcode) {
    if (xcode->project_name) {
        cr_str_destroy(xcode->project_name);
    }
    if (xcode->command_prefix) {
        cr_str_destroy(xcode->command_prefix);
    }
    for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(xcode->group_order); i < i_end; i++) {
        cr_str_destroy(bd_arr_ptr_get(xcode->group_order, i));
    }
    bd_arr_ptr_destroy(xcode->group_order);
    bd_arr_ptr_destroy(xcode->targets);
    for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(xcode->dependencies); i < i_end; i++) {
        struct bd_xcode_dependency* entry = bd_arr_ptr_get(xcode->dependencies, i);
        cr_str_destroy(entry->project);
        cr_str_destroy(entry->target_name);
        cr_free(xcode->bd->mem, entry, cr_loc);
    }
    bd_arr_ptr_destroy(xcode->dependencies);
    cr_free(xcode->bd->mem, xcode, cr_loc);
}

void bd_xcode_set_project_name(struct bd* bd, const char* name) {
    struct bd_xcode* xcode = bd_xcode_state(bd);
    assert(xcode->project_name == NULL);
    xcode->project_name = cr_str_create(bd->mem, name);
}

void bd_xcode_set_command_prefix(struct bd* bd, const char* command) {
    struct bd_xcode* xcode = bd_xcode_state(bd);
    assert(xcode->command_prefix == NULL);
    xcode->command_prefix = cr_str_create(bd->mem, command);
}

void bd_xcode_add_group_order(struct bd* bd, const char* group) {
    struct bd_xcode* xcode = bd_xcode_state(bd);
    bd_arr_ptr_append(xcode->group_order, cr_str_create(bd->mem, group));
}

void bd_xcode_add_target(struct bd* bd, struct bd_target* target) {
    struct bd_xcode* xcode = bd_xcode_state(bd);
    bd_arr_ptr_append(xcode->targets, target);
}

struct bd_arr_ptr* bd_xcode_targets(struct bd* bd) {
    return bd_xcode_state(bd)->targets;
}

void bd_xcode_add_dependency(struct bd* bd, struct bd_target* target, const struct bd_xcode_dependency_desc* desc) {
    struct bd_xcode* xcode = bd_xcode_state(bd);
    struct bd_xcode_dependency* entry = cr_malloc(bd->mem, sizeof(*entry), 0, cr_loc);
    memset(entry, 0, sizeof(*entry));
    entry->target = target;
    entry->project = cr_str_create(bd->mem, desc->project);
    entry->target_name = cr_str_create(bd->mem, desc->target);
    entry->filter = bd_filter_fix(desc->filter);
    entry->embed = desc->embed;
    bd_arr_ptr_append(xcode->dependencies, entry);
}

static struct cr_str* bd_x_project_ref_name(struct cr_mem* mem, struct cr_str* project) {
    struct cr_str* name = cr_str_get_last_path_component(mem, project);
    cr_str_remove_path_extension(name);
    return name;
}

static void bd_x_append_command_prefix(struct bd_xcode* xcode, struct cr_str* scripts) {
    if (xcode->command_prefix) {
        cr_str_append_format(scripts, "                  %s\n", cr_str_get_data(xcode->command_prefix));
    }
}

#define bd_x_platform_count 2

static const uint32_t bd_x_platforms[bd_x_platform_count] = { bd_platform_macos, bd_platform_ios };
static const char* bd_x_sdk_suffixes[bd_x_platform_count] = { "[sdk=macosx*]", "[sdk=iphoneos*]" };

struct bd_xs_entry {
    struct cr_str* key;
    struct cr_str* values[bd_x_platform_count][bd_profile_count];
};

struct bd_xs {
    struct bd* bd;
    struct bd_arr_ptr* entries;
};

static struct bd_xs* bd_xs_create(struct bd* bd) {
    struct bd_xs* xs = cr_malloc(bd->mem, sizeof(*xs), 0, cr_loc);
    memset(xs, 0, sizeof(*xs));
    xs->bd = bd;
    xs->entries = bd_arr_ptr_create(bd->mem);
    return xs;
}

static void bd_xs_destroy(struct bd_xs* xs) {
    for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(xs->entries); i < i_end; i++) {
        struct bd_xs_entry* entry = bd_arr_ptr_get(xs->entries, i);
        cr_str_destroy(entry->key);
        for (uint32_t p = 0; p < bd_x_platform_count; p++) {
            for (uint32_t c = 0; c < bd_profile_count; c++) {
                if (entry->values[p][c]) {
                    cr_str_destroy(entry->values[p][c]);
                }
            }
        }
        cr_free(xs->bd->mem, entry, cr_loc);
    }
    bd_arr_ptr_destroy(xs->entries);
    cr_free(xs->bd->mem, xs, cr_loc);
}

static void bd_xs_set(struct bd_xs* xs, const char* key, uint32_t p, uint32_t c, struct cr_str* value) {
    struct bd_xs_entry* entry = NULL;

    for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(xs->entries); i < i_end; i++) {
        struct bd_xs_entry* entry_i = bd_arr_ptr_get(xs->entries, i);
        if (strcmp(key, cr_str_get_data(entry_i->key)) == 0) {
            entry = entry_i;
            break;
        }
    }

    if (entry == NULL) {
        entry = cr_malloc(xs->bd->mem, sizeof(*entry), 0, cr_loc);
        memset(entry, 0, sizeof(*entry));
        entry->key = cr_str_create(xs->bd->mem, key);
        bd_arr_ptr_append(xs->entries, entry);
    }

    if (entry->values[p][c] != NULL) {
        cr_str_destroy(entry->values[p][c]);
    }
    entry->values[p][c] = value;
}

static void bd_xs_set_for_platforms(struct bd_xs* xs, const char* key, uint32_t platforms, const char* value) {
    for (uint32_t p = 0; p < bd_x_platform_count; p++) {
        if ((platforms & bd_x_platforms[p]) == 0) continue;
        for (uint32_t c = 0; c < bd_profile_count; c++) {
            bd_xs_set(xs, key, p, c, cr_str_create(xs->bd->mem, value));
        }
    }
}

static bool bd_xs_value_equal(struct cr_str* a, struct cr_str* b) {
    if (a == NULL && b == NULL) return true;
    if (a == NULL || b == NULL) return false;
    return cr_str_is_equal(a, b);
}

static void bd_x_append_indent(struct cr_str* str, uint32_t indent) {
    for (uint32_t i = 0; i < indent; i++) {
        cr_str_append_data(str, " ", 1);
    }
}

static void bd_x_append_yaml_string(struct cr_str* str, struct cr_str* value) {
    cr_str_append_cstr(str, "\"");
    const char* data = cr_str_get_data(value);
    for (uint32_t i = 0, i_end = cr_str_get_count(value); i < i_end; i++) {
        if (data[i] == '"' || data[i] == '\\') {
            cr_str_append_data(str, "\\", 1);
        }
        cr_str_append_data(str, &data[i], 1);
    }
    cr_str_append_cstr(str, "\"");
}

static void bd_x_append_yaml_value(struct cr_str* str, struct cr_str* value) {
    bd_x_append_yaml_string(str, value);
    cr_str_append_cstr(str, "\n");
}

static void bd_x_append_info_plist_properties(struct cr_mem* mem, struct cr_str* yml, struct bd_config* config) {
    static const enum bd_config_name names[] = { bd_config_apple_info_plist_string, bd_config_apple_info_plist_bool, bd_config_apple_info_plist_array };
    struct bd_arr_ptr* keys = bd_arr_ptr_create(mem);

    for (uint32_t p = 0; p < bd_x_platform_count; p++) {
        uint32_t filter = bd_x_platforms[p] | bd_profile_release;
        for (uint32_t n = 0; n < cr_count(names); n++) {
            enum bd_config_name name = names[n];
            if (!bd_config_has_value(config, filter, name)) continue;
            for (uint32_t i = 0, i_end = bd_config_get_array_count(config, filter, name); i < i_end; i++) {
                const char* item = bd_config_get_array_string(config, filter, name, i);
                const char* equals = strchr(item, '=');
                if (equals == NULL) {
                    bd_fail("Info.plist items must be KEY=VALUE, not %s", item);
                }
                struct cr_str* key = cr_str_create_data(mem, item, (uint32_t)(equals - item));
                bool seen = false;
                for (uint32_t k = 0, k_end = bd_arr_ptr_get_count(keys); k < k_end && !seen; k++) {
                    seen = cr_str_is_equal(bd_arr_ptr_get(keys, k), key);
                }
                if (seen) {
                    cr_str_destroy(key);
                    continue;
                }
                bd_arr_ptr_append(keys, key);
                cr_str_append_format(yml, "                \"%s\": ", cr_str_get_data(key));
                if (name == bd_config_apple_info_plist_string) {
                    struct cr_str* value = cr_str_create(mem, equals + 1);
                    bd_x_append_yaml_value(yml, value);
                    cr_str_destroy(value);
                } else if (name == bd_config_apple_info_plist_bool) {
                    cr_str_append_format(yml, "%s\n", strcmp(equals + 1, "YES") == 0 ? "true" : "false");
                } else {
                    cr_str_append_cstr(yml, "[");
                    for (const char* value = equals + 1; *value;) {
                        const char* space = strchr(value, ' ');
                        uint32_t count = space ? (uint32_t)(space - value) : (uint32_t)strlen(value);
                        cr_str_append_format(yml, "%s\"%.*s\"", value == equals + 1 ? "" : ", ", (int)count, value);
                        value += count + (space ? 1u : 0u);
                    }
                    cr_str_append_cstr(yml, "]\n");
                }
            }
        }
    }

    for (uint32_t k = 0, k_end = bd_arr_ptr_get_count(keys); k < k_end; k++) {
        cr_str_destroy(bd_arr_ptr_get(keys, k));
    }
    bd_arr_ptr_destroy(keys);
}

static void bd_xs_emit_key(struct cr_str* base, struct cr_str* dbg, struct cr_str* rel, uint32_t indent, const char* key, const char* suffix, struct cr_str* value_debug, struct cr_str* value_release) {
    if (bd_xs_value_equal(value_debug, value_release)) {
        if (value_debug) {
            bd_x_append_indent(base, indent + 8);
            cr_str_append_format(base, "%s%s: ", key, suffix);
            bd_x_append_yaml_value(base, value_debug);
        }
    } else {
        if (value_debug) {
            bd_x_append_indent(dbg, indent + 12);
            cr_str_append_format(dbg, "%s%s: ", key, suffix);
            bd_x_append_yaml_value(dbg, value_debug);
        }
        if (value_release) {
            bd_x_append_indent(rel, indent + 12);
            cr_str_append_format(rel, "%s%s: ", key, suffix);
            bd_x_append_yaml_value(rel, value_release);
        }
    }
}

static void bd_xs_emit(struct bd_xs* xs, struct cr_str* yml, uint32_t indent, uint32_t platforms) {
    struct cr_mem* mem = xs->bd->mem;
    bool both = platforms == (bd_platform_macos | bd_platform_ios);
    uint32_t single = platforms == bd_platform_ios ? 1u : 0u;
    struct cr_str* base = cr_str_create_empty(mem);
    struct cr_str* dbg = cr_str_create_empty(mem);
    struct cr_str* rel = cr_str_create_empty(mem);

    for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(xs->entries); i < i_end; i++) {
        struct bd_xs_entry* entry = bd_arr_ptr_get(xs->entries, i);
        const char* key = cr_str_get_data(entry->key);

        bool split_platforms = both &&
            !(bd_xs_value_equal(entry->values[0][0], entry->values[1][0]) &&
              bd_xs_value_equal(entry->values[0][1], entry->values[1][1]));

        if (split_platforms) {
            for (uint32_t p = 0; p < bd_x_platform_count; p++) {
                bd_xs_emit_key(base, dbg, rel, indent, key, bd_x_sdk_suffixes[p], entry->values[p][0], entry->values[p][1]);
            }
        } else {
            bd_xs_emit_key(base, dbg, rel, indent, key, "", entry->values[single][0], entry->values[single][1]);
        }
    }

    if (cr_str_get_count(base) > 0 || cr_str_get_count(dbg) > 0 || cr_str_get_count(rel) > 0) {
        bd_x_append_indent(yml, indent);
        cr_str_append_cstr(yml, "settings:\n");
        if (cr_str_get_count(base) > 0) {
            bd_x_append_indent(yml, indent + 4);
            cr_str_append_cstr(yml, "base:\n");
            cr_str_append(yml, base);
        }
        if (cr_str_get_count(dbg) > 0 || cr_str_get_count(rel) > 0) {
            bd_x_append_indent(yml, indent + 4);
            cr_str_append_cstr(yml, "configs:\n");
            if (cr_str_get_count(dbg) > 0) {
                bd_x_append_indent(yml, indent + 8);
                cr_str_append_cstr(yml, "debug:\n");
                cr_str_append(yml, dbg);
            }
            if (cr_str_get_count(rel) > 0) {
                bd_x_append_indent(yml, indent + 8);
                cr_str_append_cstr(yml, "release:\n");
                cr_str_append(yml, rel);
            }
        }
    }

    cr_str_destroy(base);
    cr_str_destroy(dbg);
    cr_str_destroy(rel);
}

static void bd_x_append_join(struct cr_mem* mem, struct cr_str** inout, struct bd_config* config, uint32_t filter, enum bd_config_name name, const char* prefix) {
    if (!bd_config_has_value(config, filter, name)) return;

    for (uint32_t i = 0, i_end = bd_config_get_array_count(config, filter, name); i < i_end; i++) {
        if (*inout == NULL) {
            *inout = cr_str_create_empty(mem);
        } else {
            cr_str_append_cstr(*inout, " ");
        }
        cr_str_append_cstr(*inout, prefix);
        cr_str_append_cstr(*inout, bd_config_get_array_string(config, filter, name, i));
    }
}

static void bd_x_collect_config_settings(struct bd_xs* xs, struct bd_config* config, uint32_t platforms, struct cr_str* (*extra_ldflags)[bd_profile_count], struct cr_str* (*extra_libtoolflags)[bd_profile_count]) {
    struct cr_mem* mem = xs->bd->mem;

    for (uint32_t p = 0; p < bd_x_platform_count; p++) {
        if ((platforms & bd_x_platforms[p]) == 0) continue;
        for (uint32_t c = 0; c < bd_profile_count; c++) {
            uint32_t filter = bd_x_platforms[p] | bd_filter_profile_at_index(c);

            if (bd_config_has_value(config, filter, bd_config_c_optimize)) {
                bd_xs_set(xs, "GCC_OPTIMIZATION_LEVEL", p, c, cr_str_create_format(mem, "%d", bd_config_get_int(config, filter, bd_config_c_optimize)));
            }

            struct cr_str* value = NULL;
            bd_x_append_join(mem, &value, config, filter, bd_config_c_include_paths, "");
            if (value) bd_xs_set(xs, "HEADER_SEARCH_PATHS", p, c, value);

            value = NULL;
            bd_x_append_join(mem, &value, config, filter, bd_config_c_defines, "");
            if (value) bd_xs_set(xs, "GCC_PREPROCESSOR_DEFINITIONS", p, c, value);

            value = NULL;
            bd_x_append_join(mem, &value, config, filter, bd_config_c_flags, "");
            if (value) bd_xs_set(xs, "OTHER_CFLAGS", p, c, value);

            value = NULL;
            bd_x_append_join(mem, &value, config, filter, bd_config_c_link_flags, "");
            bd_x_append_join(mem, &value, config, filter, bd_config_c_link_libs, "-l");
            bd_x_append_join(mem, &value, config, filter, bd_config_frameworks, "-framework ");
            if (extra_ldflags && extra_ldflags[p][c]) {
                if (value) {
                    cr_str_append_cstr(value, " ");
                } else {
                    value = cr_str_create_empty(mem);
                }
                cr_str_append(value, extra_ldflags[p][c]);
            }
            if (value) bd_xs_set(xs, "OTHER_LDFLAGS", p, c, value);

            if (extra_libtoolflags && extra_libtoolflags[p][c]) {
                bd_xs_set(xs, "OTHER_LIBTOOLFLAGS", p, c, cr_str_create_format(mem, "$(inherited) %s", cr_str_get_data(extra_libtoolflags[p][c])));
            }

            value = NULL;
            bd_x_append_join(mem, &value, config, filter, bd_config_metal_include_paths, "");
            if (value) bd_xs_set(xs, "MTL_HEADER_SEARCH_PATHS", p, c, value);

            value = NULL;
            if (bd_config_has_value(config, filter, bd_config_metal_optimize)) {
                value = cr_str_create_format(mem, "-O%d", bd_config_get_int(config, filter, bd_config_metal_optimize));
            }
            bd_x_append_join(mem, &value, config, filter, bd_config_metal_compiler_flags, "");
            if (value) bd_xs_set(xs, "MTL_COMPILER_FLAGS", p, c, value);

            value = NULL;
            bd_x_append_join(mem, &value, config, filter, bd_config_swift_include_paths, "");
            if (value) bd_xs_set(xs, "SWIFT_INCLUDE_PATHS", p, c, value);

            if (bd_config_has_value(config, filter, bd_config_swift_bridging_header)) {
                bd_xs_set(xs, "SWIFT_OBJC_BRIDGING_HEADER", p, c, cr_str_create(mem, bd_config_get_string(config, filter, bd_config_swift_bridging_header)));
            }

            uint32_t macos_filter = bd_platform_macos | bd_filter_profile_at_index(c);
            if ((platforms & bd_platform_macos) && bd_config_has_value(config, macos_filter, bd_config_min_macos_version)) {
                bd_xs_set(xs, "MACOSX_DEPLOYMENT_TARGET", p, c, cr_str_create(mem, bd_macos_version_label(bd_config_get_int(config, macos_filter, bd_config_min_macos_version))));
            }

            if (platforms & bd_platform_ios) {
                uint32_t ios_filter = bd_platform_ios | bd_filter_profile_at_index(c);
                if (bd_config_has_value(config, ios_filter, bd_config_min_ios_version)) {
                    bd_xs_set(xs, "IPHONEOS_DEPLOYMENT_TARGET", p, c, cr_str_create(mem, bd_ios_version_label(bd_config_get_int(config, ios_filter, bd_config_min_ios_version))));
                }
            }

            if (bd_config_has_value(config, filter, bd_config_apple_bundle_identifier)) {
                bd_xs_set(xs, "PRODUCT_BUNDLE_IDENTIFIER", p, c, cr_str_create(mem, bd_config_get_string(config, filter, bd_config_apple_bundle_identifier)));
            }

            if (bd_config_is_string(config, filter, bd_config_c_warnings, "none")) {
                bd_xs_set(xs, "GCC_WARN_INHIBIT_ALL_WARNINGS", p, c, cr_str_create(mem, "YES"));
            }

            if (bd_config_has_value(config, filter, bd_config_xcode_settings)) {
                for (uint32_t i = 0, i_end = bd_config_get_array_count(config, filter, bd_config_xcode_settings); i < i_end; i++) {
                    const char* item = bd_config_get_array_string(config, filter, bd_config_xcode_settings, i);
                    const char* equals = strchr(item, '=');
                    if (equals == NULL) {
                        bd_fail("bd_config_xcode_settings items must be KEY=VALUE, not %s", item);
                    }
                    struct cr_str* key = cr_str_create_data(mem, item, (uint32_t)(equals - item));
                    bd_xs_set(xs, cr_str_get_data(key), p, c, cr_str_create(mem, equals + 1));
                    cr_str_destroy(key);
                }
            }
        }
    }
}

static void bd_x_collect_platform_settings(struct bd_xs* xs, uint32_t platforms) {
    bool both = platforms == (bd_platform_macos | bd_platform_ios);
    for (uint32_t p = 0; p < bd_x_platform_count; p++) {
        if ((platforms & bd_x_platforms[p]) == 0) continue;
        for (uint32_t c = 0; c < bd_profile_count; c++) {
            bd_xs_set(xs, "SDKROOT", p, c, cr_str_create(xs->bd->mem, both ? "auto" : bd_x_platforms[p] == bd_platform_ios ? "iphoneos" : "macosx"));
            if (platforms & bd_platform_ios) {
                bd_xs_set(xs, "SUPPORTED_PLATFORMS", p, c, cr_str_create(xs->bd->mem, both ? "iphoneos iphonesimulator macosx" : "iphoneos iphonesimulator"));
                bd_xs_set(xs, "TARGETED_DEVICE_FAMILY", p, c, cr_str_create(xs->bd->mem, "1,2"));
            }
        }
    }
}

static struct cr_str* bd_x_project_relative_path(struct bd* bd, struct cr_str* path) {
    const char* data = cr_str_get_data(path);
    const char* cwd = cr_str_get_data(bd->cwd_path);
    uint32_t cwd_count = cr_str_get_count(bd->cwd_path);
    if (strncmp(data, cwd, cwd_count) == 0 && data[cwd_count] == '/') {
        return cr_str_create(bd->mem, data + cwd_count + 1);
    }
    return cr_str_create_copy(bd->mem, path);
}

static struct cr_str* bd_x_default_group(struct cr_mem* mem, struct cr_str* path) {
    const char* data = cr_str_get_data(path);
    while (strncmp(data, "../", 3) == 0) {
        data += 3;
    }
    const char* slash = strrchr(data, '/');
    if (slash == NULL || slash == data) return NULL;
    return cr_str_create_data(mem, data, (uint32_t)(slash - data));
}

static void bd_x_emit_source_extra(struct cr_str* sources, struct cr_str* group, const char* dest) {
    if (group) {
        cr_str_append_format(sources, "              group: %s\n", cr_str_get_data(group));
    }
    if (dest) {
        cr_str_append_format(sources, "              destinationFilters: %s\n", dest);
    }
}

static const char* bd_x_dest_filter(uint32_t filter, uint32_t platforms) {
    if (platforms != (bd_platform_macos | bd_platform_ios)) return NULL;
    uint32_t apple = filter & platforms;
    if (apple == bd_platform_macos) return "[macOS]";
    if (apple == bd_platform_ios) return "[iOS]";
    return NULL;
}

static void bd_x_emit_shader_sources(struct bd* bd, struct cr_str* sources, struct bd_target* embed_target, const char* dest) {
    for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(embed_target->inputs); i < i_end; i++) {
        struct _bd_input_desc* desc = bd_arr_ptr_get(embed_target->inputs, i);
        if (desc->target == NULL || desc->target->type != bd_target_type_shaders) continue;

        for (uint32_t s = 0, s_end = bd_arr_ptr_get_count(desc->target->inputs); s < s_end; s++) {
            struct _bd_input_desc* src = bd_arr_ptr_get(desc->target->inputs, s);
            if (src->path == NULL) continue;

            struct cr_str* path = bd_x_project_relative_path(bd, src->path);
            struct cr_str* group = src->group ? cr_str_create_copy(bd->mem, src->group) : bd_x_default_group(bd->mem, path);
            cr_str_append_format(sources, "            - path: %s\n", cr_str_get_data(path));
            bd_x_emit_source_extra(sources, group, dest);
            cr_str_append_cstr(sources, "              buildPhase: none\n");
            if (group) cr_str_destroy(group);
            cr_str_destroy(path);
        }
    }
}

static void bd_x_set_entitlements(struct bd* bd, struct bd_xs* xs, struct bd_target* target, uint32_t platforms) {
    struct cr_mem* mem = bd->mem;
    for (uint32_t p = 0; p < bd_x_platform_count; p++) {
        uint32_t platform = bd_x_platforms[p];
        uint32_t filter = platform | bd_profile_release;
        if ((platforms & platform) == 0 || !bd_config_has_entitlements(target->config, filter)) continue;

        struct cr_str* plist = bd_generate_entitlements(bd, target->config, filter, false, false);
        struct cr_str* path = cr_str_create_copy(mem, bd->output_path);
        cr_str_append_path_component_cstr(path, "xcodegen");
        cr_util_create_dir(path);
        cr_str_append_path_component_format(path, "%s-%s.entitlements", cr_str_get_data(target->name), bd_platform_label(platform));
        if (!cr_str_write_to_file(plist, path)) {
            bd_fail("cannot write %s", cr_str_get_data(path));
        }
        struct cr_str* relative = bd_x_project_relative_path(bd, path);
        bd_xs_set_for_platforms(xs, "CODE_SIGN_ENTITLEMENTS", platform, cr_str_get_data(relative));

        cr_str_destroy(relative);
        cr_str_destroy(path);
        cr_str_destroy(plist);
    }
}

static void bd_x_emit_target(struct bd* bd, struct cr_str* yml, struct bd_target* target, struct bd_arr_ptr* queue) {
    struct cr_mem* mem = bd->mem;

    const char* type_name;
    uint32_t platforms;

    switch (target->type) {
    case bd_target_type_app: type_name = "application"; platforms = bd_platform_macos | bd_platform_ios; break;
    case bd_target_type_executable: type_name = "tool"; platforms = bd_platform_macos; break;
    case bd_target_type_swift_module: type_name = "library.static"; platforms = bd_platform_macos | bd_platform_ios; break;
    case bd_target_type_library: type_name = "library.static"; platforms = bd_platform_macos | bd_platform_ios; break;
    case bd_target_type_ui_tests: type_name = "bundle.ui-testing"; platforms = bd_platform_ios; break;
    default: return;
    }

    struct bd_xs* xs = bd_xs_create(bd);
    bd_x_collect_platform_settings(xs, platforms);

    if (target->type == bd_target_type_app) {
        for (uint32_t p = 0; p < bd_x_platform_count; p++) {
            for (uint32_t c = 0; c < bd_profile_count; c++) {
                bd_xs_set(xs, "SKIP_INSTALL", p, c, cr_str_create(mem, "NO"));
            }
        }
    }

    struct cr_str* sources = cr_str_create_empty(mem);
    struct cr_str* deps = cr_str_create_empty(mem);
    struct cr_str* scripts = cr_str_create_empty(mem);
    struct bd_arr_ptr* embed_targets = bd_arr_ptr_create(mem);
    struct cr_str* libs[bd_x_platform_count][bd_profile_count] = { 0 };
    struct cr_str* libtool_libs[bd_x_platform_count][bd_profile_count] = { 0 };

    for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(target->inputs); i < i_end; i++) {
        struct _bd_input_desc* desc = bd_arr_ptr_get(target->inputs, i);
        if (desc->is_indirect) continue;
        if (desc->xcode_skip) continue;

        uint32_t apple = desc->filter & platforms;
        if (apple == 0) continue;

        const char* dest = bd_x_dest_filter(desc->filter, platforms);

        if (desc->target) {
            switch (desc->target->type) {
            case bd_target_type_swift_module: {
                cr_str_append_format(deps, "            - target: %s\n", cr_str_get_data(desc->target->name));
                if (dest) {
                    cr_str_append_format(deps, "              destinationFilters: %s\n", dest);
                }
                bd_arr_ptr_append(queue, desc->target);
            } break;
            case bd_target_type_embed: {
                const char* embed_name = cr_str_get_data(desc->target->name);
                for (uint32_t p = 0; p < bd_x_platform_count; p++) {
                    if ((apple & bd_x_platforms[p]) == 0) continue;
                    const char* platform_dest = bd_x_dest_filter(bd_x_platforms[p], platforms);
                    cr_str_append_format(sources, "            - path: build/temp/%s-xcode-%s/%s.c\n", embed_name, bd_platform_label(bd_x_platforms[p]), embed_name);
                    cr_str_append_cstr(sources, "              group: generated\n");
                    cr_str_append_cstr(sources, "              optional: true\n");
                    if (platform_dest) {
                        cr_str_append_format(sources, "              destinationFilters: %s\n", platform_dest);
                    }
                }

                bd_x_emit_shader_sources(bd, sources, desc->target, dest);
                bd_arr_ptr_append(embed_targets, desc->target);
            } break;
            default: break;
            }
            continue;
        }

        enum cr_file_type src_type = cr_str_get_file_type(desc->path);

        if (src_type == cr_file_type_a) {
            struct cr_str* (*lib_list)[bd_profile_count] = target->type == bd_target_type_library ? libtool_libs : libs;
            for (uint32_t p = 0; p < bd_x_platform_count; p++) {
                if ((apple & bd_x_platforms[p]) == 0) continue;
                for (uint32_t c = 0; c < bd_profile_count; c++) {
                    if ((desc->filter & bd_filter_profile_at_index(c)) == 0) continue;
                    if (lib_list[p][c] == NULL) {
                        lib_list[p][c] = cr_str_create_empty(mem);
                    } else {
                        cr_str_append_cstr(lib_list[p][c], " ");
                    }
                    cr_str_append(lib_list[p][c], desc->path);
                }
            }
            continue;
        }

        enum cr_util_file_type file_type;
        if (!cr_util_get_file_type(desc->path, &file_type)) {
            bd_fail("the input %s of %s is missing: %s", cr_str_get_data(desc->path), cr_str_get_data(target->name), strerror(errno));
        }

        struct cr_str* path = bd_x_project_relative_path(bd, desc->path);
        struct cr_str* group = desc->group ? cr_str_create_copy(mem, desc->group) : bd_x_default_group(mem, path);

        if (file_type == cr_util_file_type_directory) {
            cr_str_append_format(sources, "            - path: %s\n", cr_str_get_data(path));
            bd_x_emit_source_extra(sources, group, dest);
        } else {
            cr_str_append_format(sources, "            - path: %s\n", cr_str_get_data(path));
            bd_x_emit_source_extra(sources, group, dest);

            bool compiled = src_type == cr_file_type_c_source || src_type == cr_file_type_cpp_source ||
                src_type == cr_file_type_objc_source || src_type == cr_file_type_metal ||
                src_type == cr_file_type_swift || src_type == cr_file_type_hlsl;
            if (!compiled) {
                cr_str_append_cstr(sources, "              buildPhase: none\n");
            }
            if (desc->overrides.config && (bd_config_is_string(desc->overrides.config, bd_platform_macos | bd_profile_debug, bd_config_c_warnings, "none") || bd_config_is_string(desc->overrides.config, bd_platform_macos | bd_profile_release, bd_config_c_warnings, "none"))) {
                cr_str_append_cstr(sources, "              compilerFlags: \"-w\"\n");
            }
        }

        if (group) cr_str_destroy(group);
        cr_str_destroy(path);
    }

    for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(bd->xcode->dependencies); i < i_end; i++) {
        struct bd_xcode_dependency* dep = bd_arr_ptr_get(bd->xcode->dependencies, i);
        if (dep->target != target) continue;

        uint32_t apple = dep->filter & platforms;
        if (apple == 0) continue;

        const char* dest = bd_x_dest_filter(dep->filter, platforms);
        struct cr_str* ref = bd_x_project_ref_name(mem, dep->project);
        cr_str_append_format(deps, "            - target: %s/%s\n", cr_str_get_data(ref), cr_str_get_data(dep->target_name));
        if (dep->embed) {
            cr_str_append_cstr(deps, "              embed: true\n");
        }
        if (dest) {
            cr_str_append_format(deps, "              destinationFilters: %s\n", dest);
        }
        cr_str_destroy(ref);
    }

    if (target->type == bd_target_type_app || target->type == bd_target_type_executable) {
        bd_x_set_entitlements(bd, xs, target, platforms);
    }

    bd_x_collect_config_settings(xs, target->config, platforms, libs, libtool_libs);

    for (uint32_t p = 0; p < bd_x_platform_count; p++) {
        for (uint32_t c = 0; c < bd_profile_count; c++) {
            if (libs[p][c]) cr_str_destroy(libs[p][c]);
            if (libtool_libs[p][c]) cr_str_destroy(libtool_libs[p][c]);
        }
    }

    cr_str_append_format(yml, "    %s:\n", cr_str_get_data(target->name));
    cr_str_append_format(yml, "        type: %s\n", type_name);
    cr_str_append_format(yml, "        supportedDestinations: %s\n", platforms == (bd_platform_macos | bd_platform_ios) ? "[iOS, macOS]" : platforms == bd_platform_ios ? "[iOS]" : "[macOS]");

    bd_xs_emit(xs, yml, 8, platforms);

    if (target->type == bd_target_type_app) {
        cr_str_append_cstr(yml, "        info:\n");
        cr_str_append_format(yml, "            path: build/xcodegen/%s-Info.plist\n", cr_str_get_data(target->name));
        cr_str_append_cstr(yml, "            properties:\n");
        cr_str_append_cstr(yml, "                UILaunchScreen: {}\n");
        if (bd_config_has_value(target->config, bd_platform_ios | bd_profile_release, bd_config_apple_scene_delegate)) {
            const char* delegate = bd_config_get_string(target->config, bd_platform_ios | bd_profile_release, bd_config_apple_scene_delegate);
            cr_str_append_cstr(yml, "                UIApplicationSceneManifest:\n");
            cr_str_append_cstr(yml, "                    UIApplicationSupportsMultipleScenes: true\n");
            cr_str_append_cstr(yml, "                    UISceneConfigurations:\n");
            cr_str_append_cstr(yml, "                        UIWindowSceneSessionRoleApplication:\n");
            cr_str_append_cstr(yml, "                            - UISceneConfigurationName: Default Configuration\n");
            cr_str_append_format(yml, "                              UISceneDelegateClassName: %s\n", delegate);
        }
        bd_x_append_info_plist_properties(mem, yml, target->config);
    }
    if (bd_arr_ptr_get_count(embed_targets) > 0) {
        const char* separator = " && \\\n                  ";
        cr_str_append_cstr(scripts, "            - script: |\n");
        bd_x_append_command_prefix(bd->xcode, scripts);
        cr_str_append_cstr(scripts, "                  PLATFORM_NAME_BD=$(echo $PLATFORM_DISPLAY_NAME | awk '{print tolower($0)}')\n");
        cr_str_append_cstr(scripts, "                  PROFILE_BD=$(echo $CONFIGURATION | awk '{print tolower($0)}')\n");
        cr_str_append_cstr(scripts, "                  ");
        for (uint32_t e = 0, e_end = bd_arr_ptr_get_count(embed_targets); e < e_end; e++) {
            struct bd_target* embed_target = bd_arr_ptr_get(embed_targets, e);
            cr_str_append_format(scripts, "./build/bd --target=%s --platform=$PLATFORM_NAME_BD --profile=$PROFILE_BD%s", cr_str_get_data(embed_target->name), separator);
        }
        for (uint32_t e = 0, e_end = bd_arr_ptr_get_count(embed_targets); e < e_end; e++) {
            struct bd_target* embed_target = bd_arr_ptr_get(embed_targets, e);
            const char* name = cr_str_get_data(embed_target->name);
            cr_str_append_format(scripts, "mkdir -p build/temp/%s-xcode-$PLATFORM_NAME_BD%s", name, separator);
            cr_str_append_format(scripts, "cp build/temp/%s-$PLATFORM_NAME_BD-$PROFILE_BD/%s.c build/temp/%s-xcode-$PLATFORM_NAME_BD/%s.c", name, name, name, name);
            if (e + 1 < e_end) cr_str_append_cstr(scripts, separator);
        }
        cr_str_append_cstr(scripts, "\n");
        cr_str_append_cstr(scripts, "              name: embed\n");
        cr_str_append_cstr(scripts, "              basedOnDependencyAnalysis: NO\n");
        cr_str_append_cstr(scripts, "              outputFiles:\n");
        for (uint32_t e = 0, e_end = bd_arr_ptr_get_count(embed_targets); e < e_end; e++) {
            struct bd_target* embed_target = bd_arr_ptr_get(embed_targets, e);
            const char* name = cr_str_get_data(embed_target->name);
            for (uint32_t p = 0; p < bd_x_platform_count; p++) {
                if ((platforms & bd_x_platforms[p]) == 0) continue;
                cr_str_append_format(scripts, "                - $(SRCROOT)/build/temp/%s-xcode-%s/%s.c\n", name, bd_platform_label(bd_x_platforms[p]), name);
            }
        }
    }

    if (cr_str_get_count(scripts) > 0) {
        cr_str_append_cstr(yml, "        preBuildScripts:\n");
        cr_str_append(yml, scripts);
    }

    if (cr_str_get_count(sources) > 0) {
        cr_str_append_cstr(yml, "        sources:\n");
        cr_str_append(yml, sources);
    }
    if (cr_str_get_count(deps) > 0) {
        cr_str_append_cstr(yml, "        dependencies:\n");
        cr_str_append(yml, deps);
    }

    cr_str_destroy(sources);
    cr_str_destroy(deps);
    cr_str_destroy(scripts);
    bd_arr_ptr_destroy(embed_targets);
    bd_xs_destroy(xs);
}

static const char* bd_x_project_settings =
    "        ALWAYS_SEARCH_USER_PATHS: NO\n"
    "        ASSETCATALOG_COMPILER_GENERATE_SWIFT_ASSET_SYMBOL_EXTENSIONS: NO\n"
    "        CLANG_ADDRESS_SANITIZER_CONTAINER_OVERFLOW: YES\n"
    "        CLANG_ANALYZER_NONNULL: YES\n"
    "        CLANG_ANALYZER_NUMBER_OBJECT_CONVERSION: YES_AGGRESSIVE\n"
    "        CLANG_CXX_LANGUAGE_STANDARD: \"c++17\"\n"
    "        CLANG_ENABLE_MODULES: YES\n"
    "        CLANG_ENABLE_OBJC_ARC: NO\n"
    "        CLANG_ENABLE_OBJC_WEAK: YES\n"
    "        CLANG_UNDEFINED_BEHAVIOR_SANITIZER_INTEGER: YES\n"
    "        CLANG_UNDEFINED_BEHAVIOR_SANITIZER_NULLABILITY: YES\n"
    "        CLANG_WARN_ASSIGN_ENUM: YES\n"
    "        CLANG_WARN_ATOMIC_IMPLICIT_SEQ_CST: YES\n"
    "        CLANG_WARN_BLOCK_CAPTURE_AUTORELEASING: YES\n"
    "        CLANG_WARN_BOOL_CONVERSION: YES\n"
    "        CLANG_WARN_COMMA: YES\n"
    "        CLANG_WARN_COMPLETION_HANDLER_MISUSE: YES\n"
    "        CLANG_WARN_CONSTANT_CONVERSION: YES\n"
    "        CLANG_WARN_CXX0X_EXTENSIONS: YES\n"
    "        CLANG_WARN_DEPRECATED_OBJC_IMPLEMENTATIONS: YES\n"
    "        CLANG_WARN_DIRECT_OBJC_ISA_USAGE: YES_ERROR\n"
    "        CLANG_WARN_DOCUMENTATION_COMMENTS: NO\n"
    "        CLANG_WARN_EMPTY_BODY: YES\n"
    "        CLANG_WARN_ENUM_CONVERSION: YES\n"
    "        CLANG_WARN_FLOAT_CONVERSION: YES\n"
    "        CLANG_WARN_FRAMEWORK_INCLUDE_PRIVATE_FROM_PUBLIC: YES\n"
    "        CLANG_WARN_IMPLICIT_FALLTHROUGH: YES\n"
    "        CLANG_WARN_IMPLICIT_SIGN_CONVERSION: YES\n"
    "        CLANG_WARN_INFINITE_RECURSION: YES\n"
    "        CLANG_WARN_INT_CONVERSION: YES\n"
    "        CLANG_WARN_NON_LITERAL_NULL_CONVERSION: YES\n"
    "        CLANG_WARN_OBJC_EXPLICIT_OWNERSHIP_TYPE: YES\n"
    "        CLANG_WARN_OBJC_IMPLICIT_ATOMIC_PROPERTIES: YES\n"
    "        CLANG_WARN_OBJC_IMPLICIT_RETAIN_SELF: YES\n"
    "        CLANG_WARN_OBJC_INTERFACE_IVARS: YES\n"
    "        CLANG_WARN_OBJC_LITERAL_CONVERSION: YES\n"
    "        CLANG_WARN_OBJC_MISSING_PROPERTY_SYNTHESIS: YES\n"
    "        CLANG_WARN_OBJC_REPEATED_USE_OF_WEAK: YES\n"
    "        CLANG_WARN_OBJC_ROOT_CLASS: YES_ERROR\n"
    "        CLANG_WARN_QUOTED_INCLUDE_IN_FRAMEWORK_HEADER: YES\n"
    "        CLANG_WARN_RANGE_LOOP_ANALYSIS: YES\n"
    "        CLANG_WARN_SEMICOLON_BEFORE_METHOD_BODY: YES\n"
    "        CLANG_WARN_STRICT_PROTOTYPES: YES\n"
    "        CLANG_WARN_SUSPICIOUS_IMPLICIT_CONVERSION: YES\n"
    "        CLANG_WARN_SUSPICIOUS_MOVE: YES\n"
    "        CLANG_WARN_UNGUARDED_AVAILABILITY: YES_AGGRESSIVE\n"
    "        CLANG_WARN_UNREACHABLE_CODE: NO\n"
    "        CLANG_WARN__DUPLICATE_METHOD_MATCH: YES\n"
    "        CLANG_WARN__EXIT_TIME_DESTRUCTORS: YES\n"
    "        COPY_PHASE_STRIP: NO\n"
    "        DEAD_CODE_STRIPPING: YES\n"
    "        ENABLE_STRICT_OBJC_MSGSEND: YES\n"
    "        ENABLE_USER_SCRIPT_SANDBOXING: NO\n"
    "        GCC_C_LANGUAGE_STANDARD: c11\n"
    "        GCC_DYNAMIC_NO_PIC: NO\n"
    "        GCC_NO_COMMON_BLOCKS: YES\n"
    "        GCC_TREAT_INCOMPATIBLE_POINTER_TYPE_WARNINGS_AS_ERRORS: YES\n"
    "        GCC_WARN_64_TO_32_BIT_CONVERSION: YES\n"
    "        GCC_WARN_ABOUT_MISSING_FIELD_INITIALIZERS: YES\n"
    "        GCC_WARN_ABOUT_MISSING_NEWLINE: YES\n"
    "        GCC_WARN_ABOUT_MISSING_PROTOTYPES: YES\n"
    "        GCC_WARN_ABOUT_RETURN_TYPE: YES_ERROR\n"
    "        GCC_WARN_FOUR_CHARACTER_CONSTANTS: YES\n"
    "        GCC_WARN_HIDDEN_VIRTUAL_FUNCTIONS: YES\n"
    "        GCC_WARN_INITIALIZER_NOT_FULLY_BRACKETED: YES\n"
    "        GCC_WARN_NON_VIRTUAL_DESTRUCTOR: YES\n"
    "        GCC_WARN_PEDANTIC: NO\n"
    "        GCC_WARN_SHADOW: YES\n"
    "        GCC_WARN_SIGN_COMPARE: YES\n"
    "        GCC_WARN_STRICT_SELECTOR_MATCH: YES\n"
    "        GCC_WARN_UNDECLARED_SELECTOR: YES\n"
    "        GCC_WARN_UNINITIALIZED_AUTOS: YES_AGGRESSIVE\n"
    "        GCC_WARN_UNKNOWN_PRAGMAS: YES\n"
    "        GCC_WARN_UNUSED_FUNCTION: NO\n"
    "        GCC_WARN_UNUSED_LABEL: YES\n"
    "        GCC_WARN_UNUSED_PARAMETER: NO\n"
    "        GCC_WARN_UNUSED_VARIABLE: NO\n"
    "        GENERATE_INFOPLIST_FILE: YES\n"
    "        MTL_FAST_MATH: YES\n"
    "        OTHER_LIBTOOLFLAGS: \"-no_warning_for_no_symbols\"\n"
    "        PRODUCT_NAME: \"$(TARGET_NAME)\"\n"
    "        SKIP_INSTALL: YES\n"
    "        STRING_CATALOG_GENERATE_SYMBOLS: YES\n"
    "        SWIFT_VERSION: 5.0\n"
    "    configs:\n"
    "        debug:\n"
    "            DEBUG_INFORMATION_FORMAT: dwarf\n"
    "            ENABLE_TESTABILITY: YES\n"
    "            MTL_ENABLE_DEBUG_INFO: INCLUDE_SOURCE\n"
    "            ONLY_ACTIVE_ARCH: YES\n"
    "            SWIFT_ACTIVE_COMPILATION_CONDITIONS: DEBUG\n"
    "            SWIFT_COMPILATION_MODE: incremental\n"
    "            SWIFT_OPTIMIZATION_LEVEL: \"-Onone\"\n"
    "        release:\n"
    "            DEBUG_INFORMATION_FORMAT: \"dwarf-with-dsym\"\n"
    "            ENABLE_NS_ASSERTIONS: NO\n"
    "            MTL_ENABLE_DEBUG_INFO: NO\n"
    "            SWIFT_COMPILATION_MODE: wholemodule\n"
    "            SWIFT_OPTIMIZATION_LEVEL: \"-O\"\n";

static void bd_x_disable_queue_debugging(struct cr_mem* mem, struct cr_str* project_name, struct bd_target* target) {
    struct cr_str* path = cr_str_create_format(mem, "%s.xcodeproj/xcshareddata/xcschemes/%s.xcscheme",
                                               cr_str_get_data(project_name), cr_str_get_data(target->name));

    struct cr_str* scheme = cr_str_create_with_contents_of_file(mem, path);
    assert(scheme);

    const char* data = cr_str_get_data(scheme);
    const char* launch = strstr(data, "<LaunchAction");
    assert(launch);
    const char* tag_end = strchr(launch, '>');
    assert(tag_end);

    cr_str_insert_cstr(scheme, (uint32_t)(tag_end - data), "\n      queueDebuggingEnabled = \"No\"");

    bool written = cr_str_write_to_file(scheme, path);
    assert(written);

    cr_str_destroy(scheme);
    cr_str_destroy(path);
}

int bd_xcodegen_run(struct bd* bd) {
    struct cr_mem* mem = bd->mem;
    struct bd_xcode* xcode = bd->xcode;

    if (xcode == NULL || bd_arr_ptr_get_count(xcode->targets) == 0) {
        bd_fail("--xcodegen needs targets from bd_xcode_add_target");
    }

    struct cr_str* name = xcode->project_name ? cr_str_create_copy(mem, xcode->project_name) : cr_str_get_last_path_component(mem, bd->cwd_path);

    struct cr_str* yml = cr_str_create_empty(mem);
    cr_str_append_format(yml, "name: %s\n", cr_str_get_data(name));
    cr_str_append_cstr(yml, "options:\n");
    cr_str_append_cstr(yml, "    xcodeVersion: \"99.99\"\n");
    cr_str_append_cstr(yml, "    settingPresets: none\n");
    cr_str_append_cstr(yml, "    usesTabs: NO\n");
    cr_str_append_cstr(yml, "    indentWidth: 4\n");
    cr_str_append_cstr(yml, "    tabWidth: 4\n");
    cr_str_append_cstr(yml, "    groupSortPosition: top\n");

    if (bd_arr_ptr_get_count(xcode->group_order) > 0) {
        cr_str_append_cstr(yml, "    groupOrdering:\n        - order: [");
        for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(xcode->group_order); i < i_end; i++) {
            if (i > 0) cr_str_append_cstr(yml, ", ");
            cr_str_append(yml, bd_arr_ptr_get(xcode->group_order, i));
        }
        cr_str_append_cstr(yml, "]\n");
    }

    struct bd_arr_ptr* projects = bd_arr_ptr_create(mem);
    for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(xcode->dependencies); i < i_end; i++) {
        struct bd_xcode_dependency* dep = bd_arr_ptr_get(xcode->dependencies, i);
        bool known = false;
        for (uint32_t j = 0, j_end = bd_arr_ptr_get_count(projects); j < j_end; j++) {
            if (cr_str_is_equal(bd_arr_ptr_get(projects, j), dep->project)) known = true;
        }
        if (!known) bd_arr_ptr_append(projects, dep->project);
    }
    if (bd_arr_ptr_get_count(projects) > 0) {
        cr_str_append_cstr(yml, "projectReferences:\n");
        for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(projects); i < i_end; i++) {
            struct cr_str* project = bd_arr_ptr_get(projects, i);
            struct cr_str* ref = bd_x_project_ref_name(mem, project);
            cr_str_append_format(yml, "    %s:\n        path: %s\n", cr_str_get_data(ref), cr_str_get_data(project));
            cr_str_destroy(ref);
        }
    }
    bd_arr_ptr_destroy(projects);

    cr_str_append_cstr(yml, "configs:\n    debug: debug\n    release: release\n");
    cr_str_append_cstr(yml, "settings:\n");
    cr_str_append_cstr(yml, "    base:\n");

    struct bd_target* first_target = bd_arr_ptr_get(xcode->targets, 0);
    const char* team = bd_apple_team(bd, first_target->config, bd_platform_ios | bd_profile_debug);
    if (team != NULL) {
        cr_str_append_format(yml, "        DEVELOPMENT_TEAM: %s\n", team);
    }

    cr_str_append_cstr(yml, bd_x_project_settings);

    cr_str_append_cstr(yml, "targets:\n");

    struct bd_arr_ptr* queue = bd_arr_ptr_create(mem);
    struct bd_arr_ptr* done = bd_arr_ptr_create(mem);

    for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(xcode->targets); i < i_end; i++) {
        bd_arr_ptr_append(queue, bd_arr_ptr_get(xcode->targets, i));
    }

    for (uint32_t q = 0; q < bd_arr_ptr_get_count(queue); q++) {
        struct bd_target* target = bd_arr_ptr_get(queue, q);

        bool emitted = false;
        for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(done); i < i_end; i++) {
            if (bd_arr_ptr_get(done, i) == target) emitted = true;
        }
        if (emitted) continue;
        bd_arr_ptr_append(done, target);

        bd_x_emit_target(bd, yml, target, queue);
    }

    cr_str_append_cstr(yml, "schemes:\n");
    for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(xcode->targets); i < i_end; i++) {
        struct bd_target* target = bd_arr_ptr_get(xcode->targets, i);
        const char* target_name = cr_str_get_data(target->name);

        if (target->type == bd_target_type_ui_tests) {
            cr_str_append_format(yml, "    %s:\n", target_name);
            cr_str_append_cstr(yml, "        build:\n            targets:\n");
            cr_str_append_format(yml, "                %s: [test]\n", target_name);
            cr_str_append_cstr(yml, "        test:\n            targets:\n");
            cr_str_append_format(yml, "                - %s\n", target_name);
            cr_str_append_cstr(yml, "            captureScreenshotsAutomatically: false\n");
            continue;
        }
        if (target->type != bd_target_type_app && target->type != bd_target_type_executable) continue;

        cr_str_append_format(yml, "    %s:\n", target_name);
        cr_str_append_cstr(yml, "        build:\n            targets:\n");
        cr_str_append_format(yml, "                %s: all\n", target_name);
        cr_str_append_cstr(yml, "        run:\n            config: debug\n");
        uint32_t run_filter = bd_platform_macos | bd_profile_debug;
        struct cr_str* products_path = bd_products_path(bd, target, run_filter);
        struct bd_arr_ptr* run_args = bd_arr_ptr_create(mem);
        bd_config_args_expand(bd, target->config, run_filter, bd_config_run_args, products_path, run_args);
        if (bd_arr_ptr_get_count(run_args) > 0) {
            struct cr_str* line = cr_str_create_empty(mem);
            for (uint32_t a = 0, a_end = bd_arr_ptr_get_count(run_args); a < a_end; a++) {
                if (a > 0) cr_str_append_cstr(line, " ");
                cr_str_append(line, bd_arr_ptr_get(run_args, a));
            }
            cr_str_append_cstr(yml, "            commandLineArguments:\n                ");
            bd_x_append_yaml_string(yml, line);
            cr_str_append_cstr(yml, ": true\n");
            cr_str_destroy(line);
        }
        bd_args_destroy(run_args);
        cr_str_destroy(products_path);
    }

    bd_arr_ptr_destroy(queue);
    bd_arr_ptr_destroy(done);

    struct cr_str* xcodegen_dir = cr_str_create_copy(mem, bd->output_path);
    cr_str_append_path_component_cstr(xcodegen_dir, "xcodegen");
    cr_util_create_dir(xcodegen_dir);

    struct cr_str* spec_path = cr_str_create_copy(mem, xcodegen_dir);
    cr_str_append_path_component_cstr(spec_path, "project.yml");
    bool written = cr_str_write_to_file(yml, spec_path);
    assert(written);

    printf("[Wrote %s]\n", cr_str_get_data(spec_path));

    struct bd_arr_ptr* args = bd_arr_ptr_create(mem);
    bd_arr_ptr_append(args, cr_str_create(mem, "xcodegen"));
    bd_arr_ptr_append(args, cr_str_create(mem, "generate"));
    bd_arr_ptr_append(args, cr_str_create(mem, "--spec"));
    bd_arr_ptr_append(args, cr_str_create_copy(mem, spec_path));
    bd_arr_ptr_append(args, cr_str_create(mem, "--project"));
    bd_arr_ptr_append(args, cr_str_create(mem, "."));
    bd_arr_ptr_append(args, cr_str_create(mem, "--project-root"));
    bd_arr_ptr_append(args, cr_str_create(mem, "."));

    struct cr_str* output = cr_str_create_empty(mem);
    int exit_code = bd_exec(args, output);
    printf("%s", cr_str_get_data(output));
    cr_str_destroy(output);

    if (exit_code == 0) {
        for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(xcode->targets); i < i_end; i++) {
            struct bd_target* target = bd_arr_ptr_get(xcode->targets, i);
            if (target->type != bd_target_type_app && target->type != bd_target_type_executable) continue;

            bd_x_disable_queue_debugging(mem, name, target);
        }

        struct cr_str* user_data_path = cr_str_create_format(mem, "%s.xcodeproj/project.xcworkspace/xcuserdata/%s.xcuserdatad", cr_str_get_data(name), getenv("USER"));
        cr_util_create_dir(user_data_path);

        struct cr_str* settings_path = cr_str_create_copy(mem, user_data_path);
        cr_str_append_path_component_cstr(settings_path, "WorkspaceSettings.xcsettings");

        struct cr_str* settings = cr_str_create_format(mem,
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
            "<plist version=\"1.0\">\n"
            "<dict>\n"
            "\t<key>DerivedDataCustomLocation</key>\n"
            "\t<string>%s/xcode</string>\n"
            "\t<key>DerivedDataLocationStyle</key>\n"
            "\t<string>WorkspaceRelativePath</string>\n"
            "</dict>\n"
            "</plist>\n",
            cr_str_get_data(bd->output_path));
        written = cr_str_write_to_file(settings, settings_path);
        assert(written);
        cr_str_destroy(settings);
        cr_str_destroy(settings_path);
        cr_str_destroy(user_data_path);

        printf("[Generated %s.xcodeproj]\n", cr_str_get_data(name));
    }

    cr_str_destroy(spec_path);
    cr_str_destroy(xcodegen_dir);
    cr_str_destroy(yml);
    cr_str_destroy(name);

    return exit_code;
}
