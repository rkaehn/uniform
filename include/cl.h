#pragma once

#include "cr.h"

struct cl;
struct cl_server_conn_update;

typedef void cl_server_on_conn_update(struct cl_server_conn_update* update, void* args);

enum cl_server_http_response_type {
    cl_server_http_response_type_none,
    cl_server_http_response_type_ok,
    cl_server_http_response_type_moved_permanently,
    cl_server_http_response_type_not_found,
};

enum cl_server_http_content_type {
    cl_server_http_content_type_none,
    cl_server_http_content_type_text_html,
    cl_server_http_content_type_text_javascript,
    cl_server_http_content_type_application_wasm,
    cl_server_http_content_type_application_octet_stream,
};

enum cl_server_http_content_encoding {
    cl_server_http_content_encoding_none,
    cl_server_http_content_encoding_br,
};

enum cl_server_conn_update_type {
    cl_server_conn_update_type_open,
    cl_server_conn_update_type_close,
    cl_server_conn_update_type_http_request,
};

struct cl_server_params {
    struct cr_mem* mem;
    const char* certificate_pem;
    size_t certificate_pem_size;
    const char* private_key_pem;
    size_t private_key_pem_size;
    int port;
    const char* bind_address;
    double idle_timeout;
    size_t conn_data_size;
    cl_server_on_conn_update* on_conn_update;
    void* args;
};

struct cl_server_http_response_desc {
    enum cl_server_http_response_type type;
    enum cl_server_http_content_type content_type;
    enum cl_server_http_content_encoding content_encoding;
    const char* extra_headers;
    const unsigned char* static_body;
    size_t static_body_size;
};

struct cl_server_conn_update {
    enum cl_server_conn_update_type type;
    struct {
        const char* path;
        size_t path_size;
        struct cl_server_http_response_desc out_desc;
        char out_location[256];
    } http_request;
    void* data;
};

struct cl* cl_server_create(const struct cl_server_params* params);
int cl_server_run(struct cl* server);
