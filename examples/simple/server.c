#include "cl.h"
#include "cl_site.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct cr_str* server_read(const char* dir, const char* name) {
    struct cr_str* path = cr_str_create_format("%s/%s", dir, name);
    struct cr_str* contents = cr_str_create_with_contents_of_file(path);
    cr_check(contents != NULL, "cannot read %s", cr_str_get_data(path));
    return contents;
}

static void server_add(struct cl_site* site, const char* name) {
    struct cr_str* contents = server_read("build/wasm", name);
    cl_site_add_file(site, name, (const unsigned char*)cr_str_get_data(contents), cr_str_get_count(contents), cl_site_content_type(name, strlen(name)), cl_server_http_content_encoding_none);
}

static void server_on_conn_update(struct cl_server_conn_update* update, void* args) {
    if (update->type == cl_server_conn_update_type_http_request) {
        cl_site_respond(args, update);
    }
}

int main(void) {
    setbuf(stdout, NULL);

    struct cl_site site = { 0 };
    server_add(&site, "index.html");
    server_add(&site, "lib.js");
    server_add(&site, "index.wasm");

    struct cr_str* config = cr_str_create_format("%s/.config/uniform", getenv("HOME"));
    struct cr_str* certificate = server_read(cr_str_get_data(config), "certificate.pem");
    struct cr_str* private_key = server_read(cr_str_get_data(config), "private_key.pem");

    struct cl* server = cl_server_create(&(struct cl_server_params) {
        .certificate_pem = cr_str_get_data(certificate),
        .certificate_pem_size = cr_str_get_count(certificate),
        .private_key_pem = cr_str_get_data(private_key),
        .private_key_pem_size = cr_str_get_count(private_key),
        .port = 9443,
        .on_conn_update = server_on_conn_update,
        .args = &site,
    });
    printf("https://localhost:9443/\n");
    return cl_server_run(server);
}
