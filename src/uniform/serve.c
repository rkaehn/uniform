#include "uniform.h"

#if uniform_tool

#include "bd_private.h"
#include "cl.h"
#include "cl_site.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool uniform_serve_add_runtime(struct cl_site* site) {
    struct cr_str* dir = uniform_root(cr_mem_libc);
    cr_str_append_path_component_cstr(dir, "bin/wasm-release");
    uint32_t count = 0;
    struct cr_util_dir_iter iter = cr_util_dir_iter(cr_mem_libc, dir);
    while (cr_util_dir_iter_next(&iter)) {
        if (iter.file_type != cr_util_file_type_regular || cr_str_get_file_type(iter.full_path) == cr_file_type_ds_store) continue;
        struct cr_str* contents = cr_str_create_with_contents_of_file(cr_mem_libc, iter.full_path);
        if (contents == NULL) continue;
        const char* name = cr_str_get_data(iter.relative_path);
        cl_site_add_file(site, name, (const unsigned char*)cr_str_get_data(contents), cr_str_get_count(contents), cl_site_content_type(name, strlen(name)), cl_server_http_content_encoding_none);
        count++;
    }
    if (count == 0) {
        fprintf(stderr, "uniform: the web runtime in %s is missing\n", cr_str_get_data(dir));
    }
    cr_str_destroy(dir);
    return count > 0;
}

int uniform_serve(const char* script_path, int port, const char* bind_address, const char* device, int argc, char** argv, int script_args) {
    struct cr_mem* mem = cr_mem_libc;
    struct bd_arr_ptr* args = bd_arr_ptr_create(mem);
    for (int i = script_args; i < argc; i++) {
        bd_arr_ptr_append(args, cr_str_create(mem, argv[i]));
    }
    struct cr_str* path = bd_serve_path(mem, args);
    bd_args_destroy(args);
    struct cr_str* script = cr_str_create(mem, script_path);
    struct cr_str* name = cr_str_get_last_path_component(mem, script);
    struct cr_str* dir = cr_str_create_copy(mem, script);
    cr_str_remove_last_path_component(dir);
    if (cr_str_get_count(dir) == 0) {
        cr_str_append_cstr(dir, ".");
    }
    struct cr_str* index_html = bd_uniform_index_html(mem, cr_str_get_data(name), cr_str_get_data(name));

    struct cl_site site = { 0 };
    cl_site_add_file(&site, "index.html", (const unsigned char*)cr_str_get_data(index_html), cr_str_get_count(index_html), cl_server_http_content_type_text_html, cl_server_http_content_encoding_none);
    if (!uniform_serve_add_runtime(&site)) {
        return 1;
    }

    int result = bd_serve_site(mem, &(struct bd_serve_desc) {
        .site = &site,
        .dir = dir,
        .port = port,
        .bind_address = bind_address,
        .path = cr_str_get_data(path),
        .device = device,
    });

    cr_str_destroy(index_html);
    cr_str_destroy(path);
    cr_str_destroy(dir);
    cr_str_destroy(name);
    cr_str_destroy(script);
    return result;
}

#endif
