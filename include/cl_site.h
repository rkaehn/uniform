#pragma once

#include <stdio.h>
#include <stdlib.h>
#include "cr.h"
#include "cl.h"

#define cl_site_file_count_max 64
#define cl_site_file_path_size_max 120

struct cl_site_file {
    char path[cl_site_file_path_size_max];
    size_t path_size;
    const unsigned char* data;
    size_t size;
    enum cl_server_http_content_type content_type;
    enum cl_server_http_content_encoding content_encoding;
};

struct cl_site {
    struct cl_site_file files[cl_site_file_count_max];
    size_t file_count;
};

cr_hdef int cl_site_path_has_suffix(const char* path, size_t path_size, const char* suffix) {
    size_t suffix_size = strlen(suffix);
    return path_size >= suffix_size && memcmp(path + path_size - suffix_size, suffix, suffix_size) == 0;
}

cr_hdef enum cl_server_http_content_type cl_site_content_type(const char* path, size_t path_size) {
    if (cl_site_path_has_suffix(path, path_size, ".html")) {
        return cl_server_http_content_type_text_html;
    }
    if (cl_site_path_has_suffix(path, path_size, ".js")) {
        return cl_server_http_content_type_text_javascript;
    }
    if (cl_site_path_has_suffix(path, path_size, ".wasm")) {
        return cl_server_http_content_type_application_wasm;
    }
    return cl_server_http_content_type_application_octet_stream;
}

cr_hdef int cl_site_hex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

cr_hdef bool cl_site_respond_dev(struct cl_server_conn_update* update) {
    const char* path = update->http_request.path;
    size_t path_size = update->http_request.path_size;
    const char* query = memchr(path, '?', path_size);
    size_t query_size = 0;
    if (query != NULL) {
        path_size = (size_t)(query - path);
        query++;
        query_size = (size_t)(path + update->http_request.path_size - query);
    }

    if (query != NULL && path_size == 4 && memcmp(path, "/log", 4) == 0) {
        for (size_t i = 0; i < query_size; i++) {
            int hi;
            int lo;
            if (query[i] == '%' && i + 2 < query_size && (hi = cl_site_hex(query[i + 1])) >= 0 && (lo = cl_site_hex(query[i + 2])) >= 0) {
                putchar(hi << 4 | lo);
                i += 2;
            } else {
                putchar(query[i]);
            }
        }
        putchar('\n');
        update->http_request.out_desc = (struct cl_server_http_response_desc) { .type = cl_server_http_response_type_ok };
        return true;
    }

    if (path_size == 5 && memcmp(path, "/exit", 5) == 0) {
        int code = 0;
        for (size_t i = 0; i < query_size && query[i] >= '0' && query[i] <= '9'; i++) {
            code = code * 10 + (query[i] - '0');
        }
        fflush(stdout);
        exit(code);
    }

    return false;
}

cr_hdef struct cl_site_file* cl_site_add_file(struct cl_site* site, const char* path, const unsigned char* data, size_t size, enum cl_server_http_content_type content_type, enum cl_server_http_content_encoding content_encoding) {
    assert(site->file_count < cl_site_file_count_max);
    struct cl_site_file* file = &site->files[site->file_count++];

    file->path_size = strlen(path);
    assert(file->path_size < cl_site_file_path_size_max);
    memcpy(file->path, path, file->path_size + 1);

    file->data = data;
    file->size = size;
    file->content_type = content_type;
    file->content_encoding = content_encoding;

    printf("serving /%s (%zu bytes)\n", file->path, file->size);

    return file;
}

cr_hdef void cl_site_add_embed(struct cl_site* site, const char* prefix, const char* const names[], const unsigned char* const data[], const unsigned long long sizes[], unsigned long long count) {
    for (size_t i = 0; i < count; i++) {
        char path[cl_site_file_path_size_max];
        int path_size = snprintf(path, sizeof(path), "%s%s", prefix, names[i]);
        assert(path_size > 0 && (size_t)path_size < sizeof(path));

        enum cl_server_http_content_encoding content_encoding = cl_server_http_content_encoding_none;
        if (cl_site_path_has_suffix(path, (size_t)path_size, ".br")) {
            path_size -= 3;
            path[path_size] = 0;
            content_encoding = cl_server_http_content_encoding_br;
        }

        cl_site_add_file(site, path, data[i], sizes[i], cl_site_content_type(path, (size_t)path_size), content_encoding);
    }
}

cr_hdef struct cl_site_file* cl_site_find(struct cl_site* site, const char* a, size_t a_size, const char* b) {
    size_t b_size = strlen(b);
    for (size_t i = 0; i < site->file_count; i++) {
        struct cl_site_file* file = &site->files[i];
        if (file->path_size == a_size + b_size && memcmp(file->path, a, a_size) == 0 && memcmp(file->path + a_size, b, b_size) == 0) {
            return file;
        }
    }
    return NULL;
}

cr_hdef void cl_site_respond(struct cl_site* site, struct cl_server_conn_update* update) {
    const char* request_path = update->http_request.path;
    size_t request_path_size = update->http_request.path_size;

    const char* query = memchr(request_path, '?', request_path_size);
    if (query) {
        request_path_size = (size_t)(query - request_path);
    }

    const char* path = request_path + 1;
    size_t path_size = request_path_size - 1;

    int is_dir = path_size == 0 || path[path_size - 1] == '/';

    struct cl_site_file* file = cl_site_find(site, path, path_size, is_dir ? "index.html" : "");

    if (file) {
        update->http_request.out_desc = (struct cl_server_http_response_desc) {
            .type = cl_server_http_response_type_ok,
            .content_type = file->content_type,
            .content_encoding = file->content_encoding,
            .static_body = file->data,
            .static_body_size = file->size,
        };
        return;
    }

    if (!is_dir && request_path_size + 2 <= sizeof(update->http_request.out_location) && cl_site_find(site, path, path_size, "/index.html")) {
        memcpy(update->http_request.out_location, request_path, request_path_size);
        update->http_request.out_location[request_path_size] = '/';
        update->http_request.out_location[request_path_size + 1] = 0;
        update->http_request.out_desc = (struct cl_server_http_response_desc) {
            .type = cl_server_http_response_type_moved_permanently,
        };
        return;
    }

    update->http_request.out_desc = (struct cl_server_http_response_desc) {
        .type = cl_server_http_response_type_not_found,
    };
}
