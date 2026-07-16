#include "bd_private.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct bd_ref* bd_ref_create(struct bd* bd, struct bd_config* config, struct cr_str* path, struct bd_task* task, uint32_t filter) {
    struct bd_ref* ref = cr_malloc(bd->mem, sizeof(*ref), 0, cr_loc);

    memset(ref, 0, sizeof(*ref));
    ref->bd = bd;
    ref->config = config;
    ref->path = cr_str_create_copy(bd->mem, path);
    ref->task = task;
    ref->filter = bd_filter_fix(filter);

    return ref;
}

static void bd_ref_destroy(struct bd_ref* ref) {
    cr_str_destroy(ref->path);
    cr_free(ref->bd->mem, ref, cr_loc);
}

static struct bd_arr_ptr* bd_stage_swift_module(struct bd_target_build* target_build, struct bd_arr_ptr* inputs) {
    struct bd_target* target = target_build->target;
    struct bd_arr_ptr* outputs = bd_arr_ptr_create(target->bd->mem);

    struct bd_task* task = bd_task_create(target_build, bd_task_type_swift_module_compile, target->config);
    task->swift_module_compile.src_paths = bd_arr_ptr_create(target->bd->mem);
    task->swift_module_compile.import_paths = bd_arr_ptr_create(target->bd->mem);
    task->swift_module_compile.module_name = cr_str_create_copy(target->bd->mem, target->name);

    struct cr_str* output_a_path = cr_str_create_copy(target->bd->mem, target_build->temp_path);
    cr_str_append_path_component_format(output_a_path, "lib%s.a", cr_str_get_data(target->name));
    task->swift_module_compile.output_a_path = output_a_path;

    struct cr_str* output_module_path = cr_str_create_copy(target->bd->mem, target_build->temp_path);
    cr_str_append_path_component_format(output_module_path, "%s.swiftmodule", cr_str_get_data(target->name));
    task->swift_module_compile.output_module_path = output_module_path;

    bd_arr_ptr_append(outputs, bd_ref_create(target->bd, target->config, output_a_path, task, bd_all));
    bd_arr_ptr_append(outputs, bd_ref_create(target->bd, target->config, output_module_path, task, bd_all));

    for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(inputs); i < i_end; i++) {
        struct bd_ref* src = bd_arr_ptr_get(inputs, i);
        enum cr_file_type src_file_type = cr_str_get_file_type(src->path);

        switch (src_file_type) {
        case cr_file_type_swift: {
            if (src->task) {
                bd_arr_ptr_append(task->deps, src->task);
            }

            bd_arr_ptr_append(task->swift_module_compile.src_paths, cr_str_create_copy(target->bd->mem, src->path));
        } break;
        case cr_file_type_swiftmodule: {
            if (src->task) {
                bd_arr_ptr_append(task->deps, src->task);
            }

            struct cr_str* dir_path = cr_str_create_copy(target->bd->mem, src->path);
            cr_str_remove_last_path_component(dir_path);

            bd_arr_ptr_append(task->swift_module_compile.import_paths, dir_path);
        } break;
        default: break;
        }
    }

    return outputs;
}

static void bd_stage_compile_metal(struct bd_target_build* target_build, struct bd_config* config, struct cr_str* src_path, struct cr_str* src_name, struct bd_task* dep_task, struct bd_config* src_config, struct bd_arr_ptr* outputs) {
    struct bd_target* target = target_build->target;
    struct bd_task* object_task = bd_task_create(target_build, bd_task_type_metal_src_to_object, config);

    if (dep_task) {
        bd_arr_ptr_append(object_task->deps, dep_task);
    }

    object_task->metal_src_to_object.src_path = cr_str_create_copy(target->bd->mem, src_path);

    struct cr_str* object_path = cr_str_create_copy(target->bd->mem, target_build->temp_path);
    cr_str_append_path_component_format(object_path, "%s.air", cr_str_get_data(src_name));
    object_task->metal_src_to_object.object_path = object_path;

    bd_arr_ptr_append(outputs, bd_ref_create(target->bd, src_config, object_path, object_task, bd_all));
}

static struct bd_arr_ptr* bd_stage_compile(struct bd_target_build* target_build, struct bd_arr_ptr* inputs) {
    struct bd_target* target = target_build->target;
    struct bd_arr_ptr* outputs = bd_arr_ptr_create(target->bd->mem);

    for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(inputs); i < i_end; i++) {
        struct bd_ref* src = bd_arr_ptr_get(inputs, i);
        enum cr_file_type src_file_type = cr_str_get_file_type(src->path);

        struct bd_config* config = src->config ? src->config : target->config;

        switch (src_file_type) {
        case cr_file_type_c_source:
        case cr_file_type_cpp_source:
        case cr_file_type_objc_source: {
            struct cr_str* src_name = cr_str_get_last_path_component(target->bd->mem, src->path);
            cr_str_remove_path_extension(src_name);

            struct bd_task* object_task = bd_task_create(target_build, bd_task_type_c_src_to_object, config);
            if (src->task) {
                bd_arr_ptr_append(object_task->deps, src->task);
            }
            for (uint32_t j = 0, j_end = bd_arr_ptr_get_count(inputs); j < j_end; j++) {
                struct bd_ref* other = bd_arr_ptr_get(inputs, j);
                if (other->task != NULL && other->task->type == bd_task_type_run && other->task != src->task) {
                    bd_arr_ptr_append(object_task->deps, other->task);
                }
            }

            object_task->c_src_to_object.src_path = cr_str_create_copy(target->bd->mem, src->path);

            struct cr_str* object_path = cr_str_create_copy(target->bd->mem, target_build->temp_path);
            cr_str_append_path_component_format(object_path, "%s.o", cr_str_get_data(src_name));
            object_task->c_src_to_object.object_path = object_path;

            bd_arr_ptr_append(outputs, bd_ref_create(target->bd, src->config, object_path, object_task, bd_all));

            cr_str_destroy(src_name);
        } break;
        case cr_file_type_metal: {
            struct cr_str* src_name = cr_str_get_last_path_component(target->bd->mem, src->path);
            cr_str_remove_path_extension(src_name);

            bd_stage_compile_metal(target_build, config, src->path, src_name, src->task, src->config, outputs);

            cr_str_destroy(src_name);
        } break;
        case cr_file_type_hlsl: {
            struct cr_str* src_name = cr_str_get_last_path_component(target->bd->mem, src->path);
            cr_str_remove_path_extension(src_name);

            struct bd_task* slang_task = bd_task_create(target_build, bd_task_type_slang_to_native, config);
            if (src->task) {
                bd_arr_ptr_append(slang_task->deps, src->task);
            }

            slang_task->slang_to_native.slang_path = cr_str_create_copy(target->bd->mem, src->path);

            const char* ext;

            switch (bd_filter_platform(target_build->filter)) {
            case bd_platform_macos:
            case bd_platform_ios: ext = "metal"; break;
            case bd_platform_wasm: ext = "module.wgsl"; break;
            case bd_platform_linux: assert(0);
            default: assert(0);
            }

            struct cr_str* output_path = cr_str_create_copy(target->bd->mem, target_build->temp_path);
            cr_str_append_path_component_format(output_path, "%s.%s", cr_str_get_data(src_name), ext);
            slang_task->slang_to_native.output_path = output_path;

            switch (bd_filter_platform(target_build->filter)) {
            case bd_platform_macos:
            case bd_platform_ios: {
                bd_stage_compile_metal(target_build, config, output_path, src_name, slang_task, src->config, outputs);
            } break;
            case bd_platform_wasm: {
                bd_arr_ptr_append(outputs, bd_ref_create(target->bd, src->config, output_path, slang_task, bd_all));
            } break;
            case bd_platform_linux:
                assert(0);
            default:
                assert(0);
            }

            cr_str_destroy(src_name);
        } break;
        case cr_file_type_c_header: {
        } break;
        default: {
            bd_arr_ptr_append(outputs, bd_ref_create(target->bd, src->config, src->path, src->task, bd_all));
        } break;
        }
    }

    return outputs;
}

static bool bd_inputs_have_objects(struct bd_arr_ptr* inputs) {
    for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(inputs); i < i_end; i++) {
        struct bd_ref* input = bd_arr_ptr_get(inputs, i);
        enum cr_file_type input_file_type = cr_str_get_file_type(input->path);
        if (input_file_type == cr_file_type_o || input_file_type == cr_file_type_a) {
            return true;
        }
    }
    return false;
}

static struct bd_arr_ptr* bd_stage_link_exec(struct bd_target_build* target_build, struct bd_arr_ptr* inputs) {
    struct bd_target* target = target_build->target;
    struct bd_arr_ptr* outputs = bd_arr_ptr_create(target->bd->mem);

    if (bd_inputs_have_objects(inputs) == false) {
        for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(inputs); i < i_end; i++) {
            struct bd_ref* input = bd_arr_ptr_get(inputs, i);
            bd_arr_ptr_append(outputs, bd_ref_create(target->bd, input->config, input->path, input->task, bd_all));
        }
        return outputs;
    }

    struct bd_task* task = bd_task_create(target_build, bd_task_type_c_link_objects, target->config);
    task->c_link_objects.object_paths = bd_arr_ptr_create(target->bd->mem);

    struct cr_str* output_path = cr_str_create_copy(target->bd->mem, target_build->temp_path);
    switch (bd_filter_platform(target_build->filter)) {
    case bd_platform_macos:
    case bd_platform_ios:
    case bd_platform_linux: {
        cr_str_append_path_component_cstr(output_path, bd_executable_name(target, target_build->filter));
    } break;
    case bd_platform_wasm: {
        cr_str_append_path_component_cstr(output_path, "index.wasm");
    } break;
    default: assert(0);
    }
    task->c_link_objects.output_path = output_path;

    bd_arr_ptr_append(outputs, bd_ref_create(target->bd, target->config, output_path, task, bd_all));

    for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(inputs); i < i_end; i++) {
        struct bd_ref* input = bd_arr_ptr_get(inputs, i);

        enum cr_file_type input_file_type = cr_str_get_file_type(input->path);

        if (input_file_type == cr_file_type_o || input_file_type == cr_file_type_a) {
            if (input->task) {
                bd_arr_ptr_append(task->deps, input->task);
            }
            bd_arr_ptr_append(task->c_link_objects.object_paths, cr_str_create_copy(target->bd->mem, input->path));
        } else if (input_file_type != cr_file_type_swiftmodule) {
            bd_arr_ptr_append(outputs, bd_ref_create(target->bd, input->config, input->path, input->task, bd_all));
        }
    }

    return outputs;
}

static struct bd_arr_ptr* bd_stage_archive(struct bd_target_build* target_build, struct bd_arr_ptr* inputs) {
    struct bd_target* target = target_build->target;
    struct bd_arr_ptr* outputs = bd_arr_ptr_create(target->bd->mem);

    struct bd_task* task = bd_task_create(target_build, bd_task_type_c_archive, target->config);
    task->c_archive.object_paths = bd_arr_ptr_create(target->bd->mem);
    task->c_archive.archive_paths = bd_arr_ptr_create(target->bd->mem);

    struct cr_str* output_path = cr_str_create_copy(target->bd->mem, target_build->temp_path);
    cr_str_append_path_component_format(output_path, "lib%s.a", cr_str_get_data(target->name));
    task->c_archive.output_path = output_path;

    struct cr_str* script_path = cr_str_create_copy(target->bd->mem, target_build->temp_path);
    cr_str_append_path_component_format(script_path, "lib%s.mri", cr_str_get_data(target->name));
    task->c_archive.script_path = script_path;

    bd_arr_ptr_append(outputs, bd_ref_create(target->bd, target->config, output_path, task, bd_all));

    for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(inputs); i < i_end; i++) {
        struct bd_ref* input = bd_arr_ptr_get(inputs, i);

        enum cr_file_type input_file_type = cr_str_get_file_type(input->path);

        if (input_file_type == cr_file_type_o || input_file_type == cr_file_type_a) {
            if (input->task) {
                bd_arr_ptr_append(task->deps, input->task);
            }
            struct bd_arr_ptr* paths = input_file_type == cr_file_type_o ? task->c_archive.object_paths : task->c_archive.archive_paths;
            bd_arr_ptr_append(paths, cr_str_create_copy(target->bd->mem, input->path));
        } else {
            bd_arr_ptr_append(outputs, bd_ref_create(target->bd, input->config, input->path, input->task, bd_all));
        }
    }

    return outputs;
}

static struct bd_arr_ptr* bd_stage_link_metallib(struct bd_target_build* target_build, struct bd_arr_ptr* inputs) {
    struct bd_target* target = target_build->target;
    struct bd_arr_ptr* outputs = bd_arr_ptr_create(target->bd->mem);

    struct bd_task* task = bd_task_create(target_build, bd_task_type_metal_link_objects, target->config);
    task->metal_link_objects.object_paths = bd_arr_ptr_create(target->bd->mem);

    struct cr_str* output_path = cr_str_create_copy(target->bd->mem, target_build->temp_path);
    cr_str_append_path_component_format(output_path, "%s.metallib", cr_str_get_data(target->name));
    task->metal_link_objects.output_path = output_path;

    bd_arr_ptr_append(outputs, bd_ref_create(target->bd, target->config, output_path, task, bd_all));

    for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(inputs); i < i_end; i++) {
        struct bd_ref* input = bd_arr_ptr_get(inputs, i);

        enum cr_file_type input_file_type = cr_str_get_file_type(input->path);

        if (input_file_type == cr_file_type_air) {
            if (input->task) {
                bd_arr_ptr_append(task->deps, input->task);
            }
            bd_arr_ptr_append(task->metal_link_objects.object_paths, cr_str_create_copy(target->bd->mem, input->path));
        } else {
            bd_arr_ptr_append(outputs, bd_ref_create(target->bd, input->config, input->path, input->task, bd_all));
        }
    }

    return outputs;
}

static struct bd_arr_ptr* bd_stage_bundle_wgsl(struct bd_target_build* target_build, struct bd_arr_ptr* inputs) {
    struct bd_target* target = target_build->target;
    struct bd_arr_ptr* outputs = bd_arr_ptr_create(target->bd->mem);

    struct bd_task* task = bd_task_create(target_build, bd_task_type_wgsl_bundle, target->config);
    task->wgsl_bundle.input_paths = bd_arr_ptr_create(target->bd->mem);

    struct cr_str* output_path = cr_str_create_copy(target->bd->mem, target_build->temp_path);
    cr_str_append_path_component_format(output_path, "%s.wgsl", cr_str_get_data(target->name));
    task->wgsl_bundle.output_path = output_path;

    bd_arr_ptr_append(outputs, bd_ref_create(target->bd, target->config, output_path, task, bd_all));

    for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(inputs); i < i_end; i++) {
        struct bd_ref* input = bd_arr_ptr_get(inputs, i);

        if (cr_str_get_file_type(input->path) == cr_file_type_wgsl) {
            if (input->task) {
                bd_arr_ptr_append(task->deps, input->task);
            }
            bd_arr_ptr_append(task->wgsl_bundle.input_paths, cr_str_create_copy(target->bd->mem, input->path));
        } else {
            bd_arr_ptr_append(outputs, bd_ref_create(target->bd, input->config, input->path, input->task, bd_all));
        }
    }

    return outputs;
}

static struct bd_task* bd_task_copy_create(struct bd_target_build* target_build, struct cr_str* dir_path, bool clear) {
    struct cr_mem* mem = target_build->bd->mem;

    struct bd_task* task = bd_task_create(target_build, bd_task_type_package_copy, target_build->target->config);
    task->package_copy.dir_path = cr_str_create_copy(mem, dir_path);
    task->package_copy.clear = clear;
    task->package_copy.input_paths = bd_arr_ptr_create(mem);
    task->package_copy.output_paths = bd_arr_ptr_create(mem);

    return task;
}

static struct bd_ref* bd_task_copy_add(struct bd_task* task, struct bd_ref* input) {
    struct cr_mem* mem = task->bd->mem;

    if (input->task) {
        bd_arr_ptr_append(task->deps, input->task);
    }

    struct cr_str* output_path = cr_str_create_copy(mem, task->package_copy.dir_path);
    struct cr_str* comp = cr_str_get_last_path_component(mem, input->path);
    cr_str_append_path_component(output_path, comp);
    cr_str_destroy(comp);

    bd_arr_ptr_append(task->package_copy.input_paths, cr_str_create_copy(mem, input->path));
    bd_arr_ptr_append(task->package_copy.output_paths, output_path);

    return bd_ref_create(task->bd, task->config, output_path, task, bd_all);
}

static struct bd_arr_ptr* bd_stage_package_copy(struct bd_target_build* target_build, struct bd_arr_ptr* inputs) {
    struct bd_task* task = bd_task_copy_create(target_build, target_build->products_path, false);
    struct bd_arr_ptr* outputs = bd_arr_ptr_create(target_build->bd->mem);

    for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(inputs); i < i_end; i++) {
        bd_arr_ptr_append(outputs, bd_task_copy_add(task, bd_arr_ptr_get(inputs, i)));
    }

    return outputs;
}

static struct bd_arr_ptr* bd_stage_install(struct bd_target_build* target_build, struct bd_arr_ptr* inputs) {
    struct bd_target* target = target_build->target;
    struct bd* bd = target->bd;
    struct cr_mem* mem = bd->mem;
    uint32_t filter = target_build->filter;

    struct cr_str* install_dir = cr_str_create(mem, bd_config_get_string(target->config, filter, bd_config_install_path));
    struct cr_str* install_path = bd_path_resolve(bd, install_dir);
    cr_str_append_path_component_format(install_path, "%s-%s", bd_platform_label(bd_filter_platform(filter)), bd_profile_label(bd_filter_profile(filter)));
    cr_str_destroy(install_dir);

    struct bd_task* task = bd_task_copy_create(target_build, install_path, true);
    struct bd_arr_ptr* outputs = bd_arr_ptr_create(mem);

    for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(inputs); i < i_end; i++) {
        bd_arr_ptr_append(outputs, bd_task_copy_add(task, bd_arr_ptr_get(inputs, i)));
    }

    cr_str_destroy(install_path);

    return outputs;
}

static struct bd_arr_ptr* bd_stage_embed(struct bd_target_build* target_build, struct bd_arr_ptr* inputs) {
    struct bd_target* target = target_build->target;
    struct bd_arr_ptr* outputs = bd_arr_ptr_create(target->bd->mem);

    struct bd_task* task = bd_task_create(target_build, bd_task_type_embed, target->config);
    task->embed.input_paths = bd_arr_ptr_create(target->bd->mem);
    task->embed.symbol_prefix = cr_str_create_copy(target->bd->mem, target->name);
    char* prefix_raw = cr_str_get_raw(task->embed.symbol_prefix);
    for (uint32_t i = 0, i_end = cr_str_get_count(task->embed.symbol_prefix); i < i_end; i++) {
        char c = prefix_raw[i];
        int is_ident = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9' && i > 0);
        if (!is_ident) {
            prefix_raw[i] = '_';
        }
    }

    struct cr_str* output_path = cr_str_create_copy(target->bd->mem, target_build->temp_path);
    cr_str_append_path_component_format(output_path, "%s.c", cr_str_get_data(target->name));
    task->embed.output_path = output_path;

    for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(inputs); i < i_end; i++) {
        struct bd_ref* input = bd_arr_ptr_get(inputs, i);
        if (input->task) {
            bd_arr_ptr_append(task->deps, input->task);
        }

        bd_arr_ptr_append(task->embed.input_paths, cr_str_create_copy(target->bd->mem, input->path));
    }

    bd_arr_ptr_append(outputs, bd_ref_create(target->bd, target->config, output_path, task, bd_all));

    return outputs;
}

static struct bd_arr_ptr* bd_stage_package_web(struct bd_target_build* target_build, struct bd_arr_ptr* inputs) {
    struct bd_target* target = target_build->target;
    struct bd_arr_ptr* outputs = bd_arr_ptr_create(target->bd->mem);

    struct bd_task* task = bd_task_create(target_build, bd_task_type_package_web, target->config);
    task->package_web.input_paths = bd_arr_ptr_create(target->bd->mem);

    task->package_web.temp_path = cr_str_create_copy(target->bd->mem, target_build->temp_path);

    struct cr_str* public_path = cr_str_create_copy(target->bd->mem, target_build->products_path);
    cr_str_append_path_component_cstr(public_path, "public");
    task->package_web.public_path = public_path;

    bd_arr_ptr_append(outputs, bd_ref_create(target->bd, target->config, public_path, task, bd_all));

    bool has_html = false;
    for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(inputs); i < i_end; i++) {
        struct bd_ref* input = bd_arr_ptr_get(inputs, i);
        if (input->task) {
            bd_arr_ptr_append(task->deps, input->task);
        }
        if (cr_str_get_file_type(input->path) == cr_file_type_html) {
            has_html = true;
        }

        bd_arr_ptr_append(task->package_web.input_paths, cr_str_create_copy(target->bd->mem, input->path));
    }

    if (!has_html) {
        struct cr_str* index_path = cr_str_create_copy(target->bd->mem, task->package_web.temp_path);
        cr_str_append_path_component_cstr(index_path, "index.html");
        task->package_web.index_path = index_path;
        bd_arr_ptr_append(task->package_web.input_paths, cr_str_create_copy(target->bd->mem, index_path));
    }

    return outputs;
}

static struct bd_arr_ptr* bd_stage_package_app(struct bd_target_build* target_build, struct bd_arr_ptr* inputs) {
    struct bd_target* target = target_build->target;
    struct bd_arr_ptr* outputs = bd_arr_ptr_create(target->bd->mem);

    struct bd_task* task = bd_task_create(target_build, bd_task_type_package_app, target->config);
    task->package_app.resource_paths = bd_arr_ptr_create(target->bd->mem);

    struct cr_str* app_path = bd_app_path(target->bd, target, target_build->filter);
    task->package_app.app_path = app_path;

    bd_arr_ptr_append(outputs, bd_ref_create(target->bd, target->config, app_path, task, bd_all));

    for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(inputs); i < i_end; i++) {
        struct bd_ref* input = bd_arr_ptr_get(inputs, i);
        if (input->task) {
            bd_arr_ptr_append(task->deps, input->task);
        }

        enum cr_file_type input_file_type = cr_str_get_file_type(input->path);

        if (input_file_type == cr_file_type_none) {
            if (task->package_app.exec_path != NULL) {
                bd_fail("the app %s has more than one executable input: %s and %s", cr_str_get_data(target->name), cr_str_get_data(task->package_app.exec_path), cr_str_get_data(input->path));
            }
            task->package_app.exec_path = cr_str_create_copy(target->bd->mem, input->path);
        } else {
            bd_arr_ptr_append(task->package_app.resource_paths, cr_str_create_copy(target->bd->mem, input->path));
        }
    }

    bool export = target_build->mode == bd_mode_export;
    bool provisioned = bd_filter_platform(target_build->filter) == bd_platform_ios ||
        (export && bd_export_is_app_store(target->config, target_build->filter));
    if (provisioned || bd_config_has_entitlements(target->config, target_build->filter)) {
        struct cr_str* entitlements_path = cr_str_create_copy(target->bd->mem, target_build->temp_path);
        cr_str_append_path_component_format(entitlements_path, "%s.entitlements", cr_str_get_data(target->name));
        task->package_app.entitlements_path = entitlements_path;
    }

    return outputs;
}

static struct bd_arr_ptr* bd_stage_sign(struct bd_target_build* target_build, struct bd_arr_ptr* inputs) {
    struct bd_target* target = target_build->target;
    struct cr_mem* mem = target->bd->mem;
    struct bd_arr_ptr* outputs = bd_arr_ptr_create(mem);

    struct bd_ref* exec = NULL;
    for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(inputs); i < i_end; i++) {
        struct bd_ref* input = bd_arr_ptr_get(inputs, i);
        enum cr_file_type input_file_type = cr_str_get_file_type(input->path);
        if (input_file_type == cr_file_type_none && exec == NULL) {
            exec = input;
        }
    }

    bool export = target_build->mode == bd_mode_export;
    bool entitled = bd_config_has_entitlements(target->config, target_build->filter);
    struct bd_task* task = NULL;
    if (exec != NULL && (entitled || export)) {
        task = bd_task_create(target_build, bd_task_type_sign, target->config);
        task->sign.exec_path = cr_str_create_copy(mem, exec->path);
        if (exec->task) {
            bd_arr_ptr_append(task->deps, exec->task);
        }
        if (entitled) {
            struct cr_str* entitlements_path = cr_str_create_copy(mem, target_build->temp_path);
            cr_str_append_path_component_format(entitlements_path, "%s.entitlements", cr_str_get_data(target->name));
            task->sign.entitlements_path = entitlements_path;
        }
    }

    for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(inputs); i < i_end; i++) {
        struct bd_ref* input = bd_arr_ptr_get(inputs, i);
        bd_arr_ptr_append(outputs, bd_ref_create(target->bd, input->config, input->path, input == exec && task != NULL ? task : input->task, bd_all));
    }

    return outputs;
}

static struct bd_arr_ptr* bd_stage_run(struct bd_target_build* target_build, struct bd_arr_ptr* inputs) {
    struct bd_target* target = target_build->target;
    struct cr_mem* mem = target->bd->mem;
    struct bd_arr_ptr* outputs = bd_arr_ptr_create(mem);

    struct bd_task* task = bd_task_create(target_build, bd_task_type_run, target->config);
    task->run.args = bd_arr_ptr_create(mem);

    struct bd_ref* exec = NULL;
    for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(inputs); i < i_end; i++) {
        struct bd_ref* input = bd_arr_ptr_get(inputs, i);
        if (input->task) {
            bd_arr_ptr_append(task->deps, input->task);
        }
        if (exec == NULL && input->task != NULL && cr_str_get_file_type(input->path) == cr_file_type_none) {
            exec = input;
        }
    }
    if (exec == NULL) {
        bd_fail("the run target %s needs an executable target as input", cr_str_get_data(target->name));
    }
    bd_arr_ptr_append(task->run.args, cr_str_create_copy(mem, exec->path));

    bd_config_args_expand(target->bd, target->config, target_build->filter, bd_config_run_args, target_build->products_path, task->run.args);

    bd_arr_ptr_append(outputs, bd_ref_create(target->bd, target->config, target_build->products_path, task, bd_all));

    return outputs;
}

static struct bd_arr_ptr* bd_stage_mode(struct bd_target_build* target_build, struct bd_arr_ptr* inputs) {
    struct bd_target* target = target_build->target;
    struct cr_mem* mem = target->bd->mem;

    struct bd_arr_ptr* commands = bd_arr_ptr_create(mem);
    switch (target_build->mode) {
    case bd_mode_install:
    case bd_mode_run: {
        bd_install_commands(target->bd, target, target_build->filter, commands);
    } break;
    case bd_mode_export: {
        bd_export_commands(target->bd, target, target_build->filter, commands);
    } break;
    default: break;
    }

    struct bd_task* last = NULL;
    for (uint32_t c = 0, c_end = bd_arr_ptr_get_count(commands); c < c_end; c++) {
        struct bd_task* task = bd_task_create(target_build, bd_task_type_run, target->config);
        task->run.args = bd_arr_ptr_get(commands, c);
        for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(inputs); i < i_end; i++) {
            struct bd_ref* input = bd_arr_ptr_get(inputs, i);
            if (input->task) {
                bd_arr_ptr_append(task->deps, input->task);
            }
        }
        if (last) {
            bd_arr_ptr_append(task->deps, last);
        }
        last = task;
    }
    bd_arr_ptr_destroy(commands);

    struct bd_arr_ptr* outputs = bd_arr_ptr_create(mem);
    for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(inputs); i < i_end; i++) {
        struct bd_ref* input = bd_arr_ptr_get(inputs, i);
        bd_arr_ptr_append(outputs, bd_ref_create(target->bd, input->config, input->path, last ? last : input->task, bd_all));
    }
    return outputs;
}

struct bd_build* bd_build_create(struct bd* bd) {
    struct bd_build* build = cr_malloc(bd->mem, sizeof(*build), 0, cr_loc);

    memset(build, 0, sizeof(*build));
    build->bd = bd;
    build->targets = bd_arr_ptr_create(bd->mem);

    return build;
}

static void bd_target_build_destroy(struct bd_target_build* target_build) {
    cr_str_destroy(target_build->temp_path);
    cr_str_destroy(target_build->products_path);
    for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(target_build->tasks); i < i_end; i++) {
        bd_task_destroy(bd_arr_ptr_get(target_build->tasks, i));
    }
    bd_arr_ptr_destroy(target_build->tasks);
    for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(target_build->outputs); i < i_end; i++) {
        bd_ref_destroy(bd_arr_ptr_get(target_build->outputs, i));
    }
    bd_arr_ptr_destroy(target_build->outputs);
    cr_free(target_build->bd->mem, target_build, cr_loc);
}

void bd_build_destroy(struct bd_build* build) {
    for (uint32_t t = 0, t_end = bd_arr_ptr_get_count(build->targets); t < t_end; t++) {
        bd_target_build_destroy(bd_arr_ptr_get(build->targets, t));
    }
    bd_arr_ptr_destroy(build->targets);
    cr_free(build->bd->mem, build, cr_loc);
}

struct bd_target_build* bd_build_target(struct bd_build* build, struct bd_target* target, uint32_t platform, uint32_t profile, enum bd_mode mode) {
    struct bd* bd = build->bd;
    uint32_t filter = platform | profile;

    for (uint32_t t = 0, t_end = bd_arr_ptr_get_count(build->targets); t < t_end; t++) {
        struct bd_target_build* target_build = bd_arr_ptr_get(build->targets, t);
        if (target_build->target == target && target_build->filter == filter) {
            return target_build;
        }
    }

    struct bd_target_build* target_build = cr_malloc(bd->mem, sizeof(*target_build), 0, cr_loc);

    memset(target_build, 0, sizeof(*target_build));
    target_build->bd = bd;
    target_build->target = target;
    target_build->filter = filter;
    target_build->mode = mode;
    target_build->temp_path = bd_temp_path(bd, target, filter);
    target_build->products_path = bd_products_path(bd, target, filter);
    target_build->tasks = bd_arr_ptr_create(bd->mem);

    struct bd_task* clear = bd_task_create(target_build, bd_task_type_clear, target->config);
    clear->clear.temp_path = cr_str_create_copy(bd->mem, target_build->temp_path);
    clear->clear.products_path = cr_str_create_copy(bd->mem, target_build->products_path);
    target_build->clear = clear;

    struct bd_arr_ptr* refs = bd_arr_ptr_create(bd->mem);

    for (uint32_t r = 0, r_end = bd_arr_ptr_get_count(target->inputs); r < r_end; r++) {
        struct _bd_input_desc* desc = bd_arr_ptr_get(target->inputs, r);
        if (!bd_filter_matches(desc->filter, filter)) continue;

        struct bd_config* final_config = desc->overrides.config ? desc->overrides.config : target->config;

        if (desc->target) {
            assert(desc->path == NULL);

            uint32_t final_platform = desc->overrides.platform ? desc->overrides.platform : platform;
            uint32_t final_profile = desc->overrides.profile ? desc->overrides.profile : profile;

            struct bd_target_build* input_build = bd_build_target(build, desc->target, final_platform, final_profile, bd_mode_build);

            for (uint32_t o = 0, o_end = bd_arr_ptr_get_count(input_build->outputs); o < o_end; o++) {
                struct bd_ref* ref = bd_arr_ptr_get(input_build->outputs, o);
                if (desc->target->type == bd_target_type_run) {
                    if (desc->is_indirect) {
                        bd_arr_ptr_append(clear->deps, ref->task);
                    } else {
                        if (desc->name == NULL) {
                            bd_fail("the input of %s from the run target %s needs a name, the file it uses", cr_str_get_data(target->name), cr_str_get_data(desc->target->name));
                        }
                        struct cr_str* path = cr_str_create_copy(bd->mem, ref->path);
                        cr_str_append_path_component(path, desc->name);
                        bd_arr_ptr_append(refs, bd_ref_create(bd, final_config, path, ref->task, bd_all));
                        cr_str_destroy(path);
                    }
                    continue;
                }
                if (desc->name) {
                    struct cr_str* last = cr_str_get_last_path_component(bd->mem, ref->path);
                    bool matches = cr_str_is_equal(last, desc->name);
                    cr_str_destroy(last);
                    if (matches == false) continue;
                }
                if (desc->is_indirect) {
                    if (ref->task) {
                        bd_arr_ptr_append(clear->deps, ref->task);
                    }
                } else {
                    bd_arr_ptr_append(refs, bd_ref_create(bd, final_config, ref->path, ref->task, bd_all));
                }
            }
        } else {
            struct cr_str* path = bd_path_resolve(bd, desc->path);

            enum cr_util_file_type file_type;
            if (!cr_util_get_file_type(path, &file_type)) {
                bd_fail("the input %s of %s is missing: %s", cr_str_get_data(path), cr_str_get_data(target->name), strerror(errno));
            }

            if (file_type == cr_util_file_type_directory) {
                struct cr_util_dir_iter iter = cr_util_dir_iter(bd->mem, path);

                while (cr_util_dir_iter_next(&iter)) {
                    if (iter.file_type == cr_util_file_type_regular && cr_str_get_file_type(iter.full_path) != cr_file_type_ds_store) {
                        bd_arr_ptr_append(refs, bd_ref_create(bd, final_config, iter.full_path, NULL, bd_all));
                    }
                }
            } else if (file_type == cr_util_file_type_regular) {
                bd_arr_ptr_append(refs, bd_ref_create(bd, final_config, path, NULL, bd_all));
            } else {
                bd_fail("the input %s of %s is neither a file nor a directory", cr_str_get_data(path), cr_str_get_data(target->name));
            }

            cr_str_destroy(path);
        }
    }

    typedef struct bd_arr_ptr* bd_stage_func(struct bd_target_build*, struct bd_arr_ptr*);

    bd_stage_func* stages[16] = { 0 };
    uint32_t stage_count = 0;

    if (target->type != bd_target_type_aggregate && target->type != bd_target_type_embed && target->type != bd_target_type_run) {
        stages[stage_count++] = bd_stage_compile;
    }

    switch (target->type) {
    case bd_target_type_swift_module: {
        stages[stage_count++] = bd_stage_swift_module;
    } break;
    case bd_target_type_library: {
        stages[stage_count++] = bd_stage_archive;
        stages[stage_count++] = bd_stage_package_copy;
    } break;
    case bd_target_type_executable: {
        stages[stage_count++] = bd_stage_link_exec;
        stages[stage_count++] = bd_stage_package_copy;
        if (platform == bd_platform_macos) {
            stages[stage_count++] = bd_stage_sign;
        }
    } break;
    case bd_target_type_app: {
        stages[stage_count++] = bd_stage_link_exec;
        switch (platform) {
        case bd_platform_macos:
        case bd_platform_ios: {
            stages[stage_count++] = bd_stage_package_app;
        } break;
        case bd_platform_linux: {
            stages[stage_count++] = bd_stage_package_copy;
        } break;
        case bd_platform_wasm: {
            stages[stage_count++] = bd_stage_package_web;
        } break;
        default:
            assert(0);
        }
    } break;
    case bd_target_type_ui_tests: {
    } break;
    case bd_target_type_shaders: {
        switch (platform) {
        case bd_platform_macos:
        case bd_platform_ios: {
            stages[stage_count++] = bd_stage_link_metallib;
        } break;
        case bd_platform_wasm: {
            stages[stage_count++] = bd_stage_bundle_wgsl;
        } break;
        case bd_platform_linux: {
        } break;
        default: assert(0);
        }
        stages[stage_count++] = bd_stage_package_copy;
    } break;
    case bd_target_type_aggregate: {
        stages[stage_count++] = bd_stage_package_copy;
    } break;
    case bd_target_type_embed: {
        stages[stage_count++] = bd_stage_embed;
    } break;
    case bd_target_type_run: {
        stages[stage_count++] = bd_stage_run;
    } break;
    }

    if (bd_config_has_value(target->config, filter, bd_config_install_path)) {
        stages[stage_count++] = bd_stage_install;
    }

    if (mode != bd_mode_build) {
        stages[stage_count++] = bd_stage_mode;
    }

    for (uint32_t stage_index = 0; stage_index < stage_count; stage_index++) {
        struct bd_arr_ptr* refs_next = stages[stage_index](target_build, refs);
        for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(refs); i < i_end; i++) {
            bd_ref_destroy(bd_arr_ptr_get(refs, i));
        }
        bd_arr_ptr_destroy(refs);
        refs = refs_next;
    }

    target_build->outputs = refs;

    bd_arr_ptr_append(build->targets, target_build);

    return target_build;
}

bool bd_build_run(struct bd_build* build) {
    struct bd* bd = build->bd;
    struct cr_task* sync = cr_task_create(bd->exec, NULL, NULL);

    for (uint32_t t = 0, t_end = bd_arr_ptr_get_count(build->targets); t < t_end; t++) {
        struct bd_target_build* target_build = bd_arr_ptr_get(build->targets, t);

        for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(target_build->tasks); i < i_end; i++) {
            struct bd_task* task = bd_arr_ptr_get(target_build->tasks, i);
            task->cr_task = cr_task_create(bd->exec, &bd_func, task);
            cr_task_retain(task->cr_task);
            for (uint32_t j = 0, j_end = bd_arr_ptr_get_count(task->deps); j < j_end; j++) {
                struct bd_task* dep_task = bd_arr_ptr_get(task->deps, j);
                assert(dep_task->cr_task);
                cr_task_wait_request_signal(task->cr_task, dep_task->cr_task);
            }
            cr_task_wait_request_signal(sync, task->cr_task);
            cr_task_run(task->cr_task);
        }
    }

    cr_task_run_sync(sync);

    bool success = true;

    for (uint32_t t = 0, t_end = bd_arr_ptr_get_count(build->targets); t < t_end; t++) {
        struct bd_target_build* target_build = bd_arr_ptr_get(build->targets, t);

        for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(target_build->tasks); i < i_end; i++) {
            struct bd_task* task = bd_arr_ptr_get(target_build->tasks, i);
            success = success && task->success;
        }
    }

    return success;
}

static void bd_json_append_string(struct cr_str* text, const char* string) {
    cr_str_append_cstr(text, "\"");
    for (const char* c = string; *c; c++) {
        if (*c == '"' || *c == '\\') {
            cr_str_append_cstr(text, "\\");
        }
        cr_str_append_data(text, c, 1);
    }
    cr_str_append_cstr(text, "\"");
}

static struct cr_str* bd_build_compile_commands(struct bd_build* build) {
    struct bd* bd = build->bd;
    struct bd_arr_ptr* files = bd_arr_ptr_create(bd->mem);
    struct cr_str* text = cr_str_create(bd->mem, "[\n");

    for (uint32_t t = 0, t_end = bd_arr_ptr_get_count(build->targets); t < t_end; t++) {
        struct bd_target_build* target_build = bd_arr_ptr_get(build->targets, t);

        for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(target_build->tasks); i < i_end; i++) {
            struct bd_task* task = bd_arr_ptr_get(target_build->tasks, i);
            if (task->type != bd_task_type_c_src_to_object) continue;

            bool listed = false;
            for (uint32_t f = 0, f_end = bd_arr_ptr_get_count(files); f < f_end; f++) {
                listed = listed || cr_str_is_equal(bd_arr_ptr_get(files, f), task->c_src_to_object.src_path);
            }
            if (listed) continue;
            bd_arr_ptr_append(files, cr_str_create_copy(bd->mem, task->c_src_to_object.src_path));

            struct bd_arr_ptr* args = bd_c_src_to_object_args(task);

            cr_str_append_cstr(text, "{ \"directory\": ");
            bd_json_append_string(text, cr_str_get_data(bd->cwd_path));
            cr_str_append_cstr(text, ", \"file\": ");
            bd_json_append_string(text, cr_str_get_data(task->c_src_to_object.src_path));
            cr_str_append_cstr(text, ", \"output\": ");
            bd_json_append_string(text, cr_str_get_data(task->c_src_to_object.object_path));
            cr_str_append_cstr(text, ", \"arguments\": [");
            for (uint32_t a = 0, a_end = bd_arr_ptr_get_count(args); a < a_end; a++) {
                if (a > 0) cr_str_append_cstr(text, ", ");
                bd_json_append_string(text, cr_str_get_data(bd_arr_ptr_get(args, a)));
                cr_str_destroy(bd_arr_ptr_get(args, a));
            }
            bd_arr_ptr_destroy(args);
            cr_str_append_cstr(text, "] },\n");
        }
    }

    if (bd_arr_ptr_get_count(files) > 0) {
        cr_str_remove_last(text, 2);
        cr_str_append_cstr(text, "\n");
    }
    cr_str_append_cstr(text, "]\n");

    for (uint32_t f = 0, f_end = bd_arr_ptr_get_count(files); f < f_end; f++) {
        cr_str_destroy(bd_arr_ptr_get(files, f));
    }
    bd_arr_ptr_destroy(files);

    return text;
}

void bd_compile_commands_write(struct bd* bd) {
    struct bd_build* build = bd_build_create(bd);
    struct bd_arr_ptr* targets = bd_xcode_targets(bd);

    for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(targets); i < i_end; i++) {
        bd_build_target(build, bd_arr_ptr_get(targets, i), bd_platform_macos, bd_profile_debug, bd_mode_build);
    }

    struct cr_str* text = bd_build_compile_commands(build);
    struct cr_str* path = cr_str_create_copy(bd->mem, bd->output_path);
    cr_str_append_path_component_cstr(path, "compile_commands.json");
    bool written = cr_str_write_to_file(text, path);
    assert(written);

    cr_str_destroy(path);
    cr_str_destroy(text);
    bd_build_destroy(build);
}
