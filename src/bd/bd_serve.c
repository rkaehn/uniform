#include "bd_private.h"
#include "cl.h"
#include "cl_site.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct bd_serve_state {
    struct cr_mem* mem;
    struct cl_site* site;
    struct cr_str* dir;
};

struct bd_serve_conn {
    struct cr_str* body;
};

static struct cr_str* bd_serve_read(struct cr_mem* mem, struct cr_str* dir, const char* name, size_t name_size) {
    if (memmem(name, name_size, "..", 2) != NULL) {
        return NULL;
    }
    struct cr_str* path = cr_str_create_copy(mem, dir);
    struct cr_str* component = cr_str_create_data(mem, name, (uint32_t)name_size);
    cr_str_append_path_component(path, component);
    struct cr_str* contents = cr_str_create_with_contents_of_file(mem, path);
    cr_str_destroy(component);
    cr_str_destroy(path);
    return contents;
}

static void bd_serve_on_conn_update(struct cl_server_conn_update* update, void* args) {
    struct bd_serve_state* serve = args;
    struct bd_serve_conn* conn = update->data;
    if (update->type == cl_server_conn_update_type_open) {
        conn->body = NULL;
        return;
    }
    if (conn->body != NULL) {
        cr_str_destroy(conn->body);
        conn->body = NULL;
    }
    if (update->type == cl_server_conn_update_type_close || cl_site_respond_dev(update)) {
        return;
    }

    const char* path = update->http_request.path;
    size_t path_size = update->http_request.path_size;
    const char* query = memchr(path, '?', path_size);
    if (query != NULL) {
        path_size = (size_t)(query - path);
    }
    if (serve->dir != NULL && path[path_size - 1] != '/') {
        conn->body = bd_serve_read(serve->mem, serve->dir, path + 1, path_size - 1);
    }
    if (conn->body != NULL) {
        update->http_request.out_desc = (struct cl_server_http_response_desc) {
            .type = cl_server_http_response_type_ok,
            .content_type = cl_site_content_type(path, path_size),
            .static_body = (const unsigned char*)cr_str_get_data(conn->body),
            .static_body_size = cr_str_get_count(conn->body),
        };
    } else {
        cl_site_respond(serve->site, update);
    }
    if (update->http_request.out_desc.type == cl_server_http_response_type_ok) {
        update->http_request.out_desc.extra_headers = "Cache-Control: no-store\r\n";
    }
}

static void bd_serve_append_url_arg(struct cr_str* url, const char* arg) {
    for (const unsigned char* c = (const unsigned char*)arg; *c; c++) {
        bool plain = (*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') || (*c >= '0' && *c <= '9') || *c == '-' || *c == '.' || *c == '_' || *c == '~';
        if (plain) {
            cr_str_append_data(url, (const char*)c, 1);
        } else {
            cr_str_append_format(url, "%%%02X", *c);
        }
    }
}

struct cr_str* bd_serve_path(struct cr_mem* mem, struct bd_arr_ptr* args) {
    struct cr_str* path = cr_str_create(mem, "/");
    for (uint32_t i = 0, i_end = bd_arr_ptr_get_count(args); i < i_end; i++) {
        cr_str_append_cstr(path, i == 0 ? "?" : "&");
        bd_serve_append_url_arg(path, cr_str_get_data(bd_arr_ptr_get(args, i)));
    }
    return path;
}

int bd_serve_site(struct cr_mem* mem, const struct bd_serve_desc* desc) {
    struct bd_cert* cert = bd_cert_load(mem);
    const char* path = desc->path != NULL ? desc->path : "/";
    struct cr_str* device = desc->device != NULL ? bd_device_resolve(mem, desc->device) : NULL;

    struct bd_serve_state state = { .mem = mem, .site = desc->site, .dir = desc->dir };
    struct cl* cl = cl_server_create(&(struct cl_server_params) {
        .mem = mem,
        .certificate_pem = cr_str_get_data(cert->certificate),
        .certificate_pem_size = cr_str_get_count(cert->certificate),
        .private_key_pem = cr_str_get_data(cert->private_key),
        .private_key_pem_size = cr_str_get_count(cert->private_key),
        .port = desc->port,
        .bind_address = desc->bind_address,
        .conn_data_size = sizeof(struct bd_serve_conn),
        .on_conn_update = bd_serve_on_conn_update,
        .args = &state,
    });
    if (cl == NULL) {
        bd_fail("cannot listen on port %d, maybe another server uses it", desc->port);
    }

    printf("https://%s:%d%s\n", desc->bind_address != NULL ? desc->bind_address : "localhost", desc->port, path);
    if (cert->host != NULL) {
        printf("https://%s:%d%s\n", cr_str_get_data(cert->host), desc->port, path);
    }

    int result = 0;
    if (device != NULL) {
        struct cr_str* url = cr_str_create_format(mem, "https://%s:%d%s", cert->host != NULL ? cr_str_get_data(cert->host) : "localhost", desc->port, path);
        result = bd_exec_inherit(bd_args_create(mem, "xcrun", "devicectl", "device", "process", "launch", "--terminate-existing", "--device", cr_str_get_data(device), "--payload-url", cr_str_get_data(url), "com.apple.mobilesafari", NULL));
        cr_str_destroy(url);
    }
    if (result == 0) {
        result = cl_server_run(cl) == 0 ? 0 : 1;
    }

    if (device != NULL) {
        cr_str_destroy(device);
    }
    bd_cert_destroy(cert);
    return result;
}

int bd_serve(struct bd* bd, struct bd_target* target, uint32_t filter, const char* path) {
    struct cr_mem* mem = bd->mem;

    struct cl_site site = { 0 };
    struct bd_arr_ptr* files = bd_arr_ptr_create(mem);
    struct cr_str* public_path = bd_products_path(bd, target, filter);
    cr_str_append_path_component_cstr(public_path, "public");
    struct cr_util_dir_iter iter = cr_util_dir_iter(mem, public_path);
    while (cr_util_dir_iter_next(&iter)) {
        if (iter.file_type != cr_util_file_type_regular || cr_str_get_file_type(iter.full_path) == cr_file_type_ds_store) continue;
        if (site.file_count == cl_site_file_count_max) {
            bd_fail("cannot serve %s, it has more than %d files", cr_str_get_data(public_path), cl_site_file_count_max);
        }
        struct cr_str* contents = cr_str_create_with_contents_of_file(mem, iter.full_path);
        if (contents == NULL) {
            bd_fail("cannot read the file %s", cr_str_get_data(iter.full_path));
        }
        bd_arr_ptr_append(files, contents);

        const char* name = cr_str_get_data(iter.relative_path);
        const unsigned char* data = (const unsigned char*)cr_str_get_data(contents);
        unsigned long long size = cr_str_get_count(contents);
        cl_site_add_embed(&site, "", &name, &data, &size, 1);
    }
    cr_str_destroy(public_path);

    const char* device = NULL;
    if (bd->device_set) {
        device = bd->device != NULL ? cr_str_get_data(bd->device) : bd_config_has_value(target->config, filter, bd_config_run_device) ? bd_config_get_string(target->config, filter, bd_config_run_device) : "";
    }

    int result = bd_serve_site(mem, &(struct bd_serve_desc) {
        .site = &site,
        .port = bd_config_get_int(target->config, filter, bd_config_run_port),
        .path = path,
        .device = device,
    });

    bd_args_destroy(files);
    return result;
}
