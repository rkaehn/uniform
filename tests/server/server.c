#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include "cr.h"
#include "cl.h"
#include "cl_site.h"

extern const char* const tests_site_names[];
extern const unsigned char* const tests_site_data[];
extern const unsigned long long tests_site_sizes[];
extern const unsigned long long tests_site_count;

static struct cl_site server_site;
static const char* server_refs_root;

struct server_conn {
    struct cr_str* body;
};

static void server_refs_respond(struct cl_server_conn_update* update, struct server_conn* conn) {
    const char* name = update->http_request.path + 6;
    size_t name_size = update->http_request.path_size - 6;
    const char* file = memchr(name, '/', name_size);
    size_t file_size = file != NULL ? (size_t)(name + name_size - file - 1) : 0;

    update->http_request.out_desc = (struct cl_server_http_response_desc) {
        .type = cl_server_http_response_type_not_found,
    };
    if (name_size >= 256 || name[0] == '.' || file_size == 0 || file[1] == '.' || memchr(file + 1, '/', file_size) != NULL) {
        return;
    }

    struct cr_str* path = cr_str_create_format(cr_mem_libc, "%s/%.*s", server_refs_root, (int)name_size, name);
    struct cr_str* body = cr_str_create_with_contents_of_file(cr_mem_libc, path);
    cr_str_destroy(path);
    if (body == NULL) {
        return;
    }

    conn->body = body;
    update->http_request.out_desc = (struct cl_server_http_response_desc) {
        .type = cl_server_http_response_type_ok,
        .content_type = cl_server_http_content_type_application_octet_stream,
        .extra_headers = "Cache-Control: no-store\r\n",
        .static_body = (const unsigned char*)cr_str_get_data(body),
        .static_body_size = cr_str_get_count(body),
    };
}

static void server_on_conn_update(struct cl_server_conn_update* update, void* args) {
    (void)args;

    struct server_conn* conn = update->data;
    if (update->type == cl_server_conn_update_type_open) {
        conn->body = NULL;
        return;
    }
    if (conn->body != NULL) {
        cr_str_destroy(conn->body);
        conn->body = NULL;
    }
    if (update->type != cl_server_conn_update_type_http_request || cl_site_respond_dev(update)) {
        return;
    }

    printf("%.*s\n", (int)update->http_request.path_size, update->http_request.path);

    if (server_refs_root != NULL && update->http_request.path_size > 6 && memcmp(update->http_request.path, "/refs/", 6) == 0) {
        server_refs_respond(update, conn);
        return;
    }

    cl_site_respond(&server_site, update);

    if (update->http_request.out_desc.type == cl_server_http_response_type_ok) {
        update->http_request.out_desc.extra_headers = "Cache-Control: no-store\r\n";
    }
}

int main(int argc, char** argv) {
    setbuf(stdout, NULL);

    const char* certificate_path = "certificate.pem";
    const char* private_key_path = "private_key.pem";
    const char* bind_address = NULL;
    int port = 443;

    struct option options[] = {
        { "certificate", required_argument, NULL, 'c' },
        { "private-key", required_argument, NULL, 'k' },
        { "port", required_argument, NULL, 'p' },
        { "refs", required_argument, NULL, 'r' },
        { "bind", required_argument, NULL, 'b' },
        { 0 },
    };
    while (1) {
        int c = getopt_long(argc, argv, "", options, NULL);
        if (c == -1) break;
        switch (c) {
        case 'c':
            certificate_path = optarg;
            break;
        case 'k':
            private_key_path = optarg;
            break;
        case 'p':
            port = atoi(optarg);
            break;
        case 'r':
            server_refs_root = optarg;
            break;
        case 'b':
            bind_address = optarg;
            break;
        default:
            printf("usage: %s --certificate <path> --private-key <path> --port <d> --refs <dir> --bind <address>\n", argv[0]);
            return 1;
        }
    }

    struct cr_mem* mem = cr_mem_libc;

    struct cr_str* certificate_path_str = cr_str_create(mem, certificate_path);
    struct cr_str* certificate = cr_str_create_with_contents_of_file(mem, certificate_path_str);
    if (certificate == NULL) {
        printf("could not read certificate: %s\n", certificate_path);
        return 1;
    }

    struct cr_str* private_key_path_str = cr_str_create(mem, private_key_path);
    struct cr_str* private_key = cr_str_create_with_contents_of_file(mem, private_key_path_str);
    if (private_key == NULL) {
        printf("could not read private key: %s\n", private_key_path);
        return 1;
    }

    cl_site_add_embed(&server_site, "", tests_site_names, tests_site_data, tests_site_sizes, tests_site_count);

    struct cl* server = cl_server_create(&(struct cl_server_params) {
        .mem = mem,
        .certificate_pem = cr_str_get_data(certificate),
        .certificate_pem_size = cr_str_get_count(certificate),
        .private_key_pem = cr_str_get_data(private_key),
        .private_key_pem_size = cr_str_get_count(private_key),
        .port = port,
        .bind_address = bind_address,
        .conn_data_size = sizeof(struct server_conn),
        .on_conn_update = server_on_conn_update,
    });
    if (server == NULL) {
        return 1;
    }

    if (port == 443) {
        printf("https://localhost/\n");
    } else {
        printf("https://localhost:%d/\n", port);
    }

    cl_server_run(server);

    return 1;
}
