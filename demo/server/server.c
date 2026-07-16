#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include "cr.h"
#include "cl.h"
#include "cl_site.h"

extern const char* const demo_site_names[];
extern const unsigned char* const demo_site_data[];
extern const unsigned long long demo_site_sizes[];
extern const unsigned long long demo_site_count;

static struct cl_site server_site;

static void server_on_conn_update(struct cl_server_conn_update* update, void* args) {
    (void)args;

    if (update->type != cl_server_conn_update_type_http_request) {
        return;
    }

    printf("%.*s\n", (int)update->http_request.path_size, update->http_request.path);

    cl_site_respond(&server_site, update);
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
        case 'b':
            bind_address = optarg;
            break;
        default:
            printf("usage: %s --certificate <path> --private-key <path> --port <d> --bind <address>\n", argv[0]);
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

    cl_site_add_embed(&server_site, "", demo_site_names, demo_site_data, demo_site_sizes, demo_site_count);

    struct cl* server = cl_server_create(&(struct cl_server_params) {
        .mem = mem,
        .certificate_pem = cr_str_get_data(certificate),
        .certificate_pem_size = cr_str_get_count(certificate),
        .private_key_pem = cr_str_get_data(private_key),
        .private_key_pem_size = cr_str_get_count(private_key),
        .port = port,
        .bind_address = bind_address,
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
