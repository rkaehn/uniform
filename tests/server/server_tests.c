#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <netdb.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "picohttpparser.h"
#include "cr.h"
cr_warning_push
cr_warning_ignore_everything
#include "bearssl.h"
cr_warning_pop
#include "cl.h"
#include "cl_crypto.h"
#include "cl_private.h"
#include "cl_site.h"

#define ts_conn_magic 0x746573746d616763ull
#define ts_read_buf_size (1u << 15)

struct ts_conn {
    uint64_t magic;
};

static struct cl_site ts_site;
static const struct cl_site_file* ts_index_html;
static const struct cl_site_file* ts_app_js;
static const struct cl_site_file* ts_big_bin;
static int ts_port = 9446;

static _Atomic long ts_open_count;
static _Atomic long ts_close_count;

struct ts_trust {
    br_x509_trust_anchor ta;
    unsigned char buf[8192];
    size_t offset;
};

static struct ts_trust ts_trust;

struct ts_client {
    int fd;
    br_ssl_client_context cc;
    br_x509_minimal_context xc;
    br_sslio_context ioc;
    unsigned char iobuf[BR_SSL_BUFSIZE_BIDI];
    unsigned char rbuf[ts_read_buf_size];
    size_t rbuf_offset;
};

struct ts_response {
    int status;
    int has_connection_close;
    int has_content_encoding_br;
    char content_type[64];
    char location[128];
    unsigned char* body;
    size_t body_size;
};

static void ts_on_conn_update(struct cl_server_conn_update* update, void* args) {
    (void)args;
    struct ts_conn* conn = update->data;

    switch (update->type) {
    case cl_server_conn_update_type_open: {
        assert(conn->magic != ts_conn_magic);
        conn->magic = ts_conn_magic;
        atomic_fetch_add(&ts_open_count, 1);
    } break;
    case cl_server_conn_update_type_close: {
        assert(conn->magic == ts_conn_magic);
        conn->magic = 0;
        atomic_fetch_add(&ts_close_count, 1);
    } break;
    case cl_server_conn_update_type_http_request: {
        assert(conn->magic == ts_conn_magic);
        cl_site_respond(&ts_site, update);
    } break;
    }
}

static void ts_files_init(void) {
    static const char index_html[] = "<!DOCTYPE html><html><body>server test fixture</body></html>";
    ts_index_html = cl_site_add_file(&ts_site, "app/index.html", (const unsigned char*)index_html, sizeof(index_html) - 1, cl_server_http_content_type_text_html, cl_server_http_content_encoding_none);

    size_t app_js_size = 100 * 1024 + 13;
    unsigned char* app_js = malloc(app_js_size);
    uint64_t seed = 1;
    for (size_t i = 0; i < app_js_size; i++) {
        app_js[i] = (unsigned char)cr_rand(&seed);
    }
    ts_app_js = cl_site_add_file(&ts_site, "app/app.js", app_js, app_js_size, cl_server_http_content_type_text_javascript, cl_server_http_content_encoding_none);

    size_t big_size = 6 * 1024 * 1024 + 7;
    unsigned char* big = malloc(big_size);
    seed = 2;
    for (size_t i = 0; i < big_size; i++) {
        big[i] = (unsigned char)cr_rand(&seed);
    }
    ts_big_bin = cl_site_add_file(&ts_site, "big.bin", big, big_size, cl_server_http_content_type_application_octet_stream, cl_server_http_content_encoding_br);
}

static void ts_trust_append(void* args, const void* data, size_t size) {
    struct ts_trust* trust = args;
    assert(trust->offset + size <= sizeof(trust->buf));
    memcpy(trust->buf + trust->offset, data, size);
    trust->offset += size;
}

static void ts_trust_init(struct ts_trust* trust, const unsigned char* pem, size_t pem_size) {
    memset(trust, 0, sizeof(*trust));

    unsigned char der[4096];
    void* obj_data;
    size_t obj_size;
    size_t obj_count = 1;
    ssize_t decoded = cl_decode_pem(pem, pem_size, der, sizeof(der), &obj_data, &obj_size, &obj_count);
    assert(decoded > 0 && obj_count == 1);

    br_x509_decoder_context dc;
    br_x509_decoder_init(&dc, ts_trust_append, trust);
    br_x509_decoder_push(&dc, obj_data, obj_size);
    assert(br_x509_decoder_last_error(&dc) == 0);

    trust->ta.dn.data = trust->buf;
    trust->ta.dn.len = trust->offset;
    if (br_x509_decoder_isCA(&dc)) {
        trust->ta.flags |= BR_X509_TA_CA;
    }

    br_x509_pkey* pk = br_x509_decoder_get_pkey(&dc);
    assert(pk && pk->key_type == BR_KEYTYPE_RSA);
    trust->ta.pkey.key_type = BR_KEYTYPE_RSA;

    assert(trust->offset + pk->key.rsa.nlen + pk->key.rsa.elen <= sizeof(trust->buf));
    trust->ta.pkey.key.rsa.n = trust->buf + trust->offset;
    trust->ta.pkey.key.rsa.nlen = pk->key.rsa.nlen;
    memcpy(trust->ta.pkey.key.rsa.n, pk->key.rsa.n, pk->key.rsa.nlen);
    trust->offset += pk->key.rsa.nlen;
    trust->ta.pkey.key.rsa.e = trust->buf + trust->offset;
    trust->ta.pkey.key.rsa.elen = pk->key.rsa.elen;
    memcpy(trust->ta.pkey.key.rsa.e, pk->key.rsa.e, pk->key.rsa.elen);
    trust->offset += pk->key.rsa.elen;
}

static int ts_sock_connect(void) {
    struct addrinfo* info = NULL;
    struct addrinfo hints = {
        .ai_socktype = SOCK_STREAM,
    };
    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", ts_port);

    int err = getaddrinfo("127.0.0.1", port_str, &hints, &info);
    assert(err == 0);

    int fd = -1;
    for (struct addrinfo* info_i = info; info_i != NULL; info_i = info_i->ai_next) {
        fd = socket(info_i->ai_family, info_i->ai_socktype, info_i->ai_protocol);
        if (fd == -1) {
            continue;
        }
        err = connect(fd, info_i->ai_addr, info_i->ai_addrlen);
        if (err == -1) {
            close(fd);
            fd = -1;
            continue;
        }
        break;
    }
    freeaddrinfo(info);

    if (fd != -1) {
        struct timeval timeout = { .tv_sec = 20 };
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    }

    return fd;
}

static int ts_sock_read(void* args, unsigned char* buf, size_t buf_size) {
    struct ts_client* client = args;
    while (1) {
        ssize_t count = read(client->fd, buf, buf_size);
        if (count > 0) {
            return (int)count;
        }
        if (count == -1 && errno == EINTR) {
            continue;
        }
        return -1;
    }
}

static int ts_sock_write(void* args, const unsigned char* buf, size_t buf_size) {
    struct ts_client* client = args;
    while (1) {
        ssize_t count = write(client->fd, buf, buf_size);
        if (count > 0) {
            return (int)count;
        }
        if (count == -1 && errno == EINTR) {
            continue;
        }
        return -1;
    }
}

static struct ts_client* ts_client_open(void) {
    struct ts_client* client = malloc(sizeof(*client));
    memset(client, 0, sizeof(*client));

    client->fd = ts_sock_connect();
    assert(client->fd != -1);

    br_ssl_client_init_full(&client->cc, &client->xc, &ts_trust.ta, 1);
    br_ssl_engine_set_buffer(&client->cc.eng, client->iobuf, sizeof(client->iobuf), 1);

    int err = br_ssl_client_reset(&client->cc, "localhost", 0);
    assert(err == 1);

    br_sslio_init(&client->ioc, &client->cc.eng, ts_sock_read, client, ts_sock_write, client);

    return client;
}

static void ts_client_shutdown(struct ts_client* client) {
    close(client->fd);
    free(client);
}

static void ts_client_write(struct ts_client* client, const char* data, size_t data_size) {
    int err = br_sslio_write_all(&client->ioc, data, data_size);
    assert(err == 0);
    err = br_sslio_flush(&client->ioc);
    assert(err == 0);
}

static void ts_client_write_aborted(struct ts_client* client, const char* data, size_t data_size) {
    if (br_sslio_write_all(&client->ioc, data, data_size) == 0) {
        br_sslio_flush(&client->ioc);
    }
}

static void ts_client_request(struct ts_client* client, const char* path) {
    char request[256];
    int size = snprintf(request, sizeof(request), "GET %s HTTP/1.1\r\nHost: localhost\r\n\r\n", path);
    assert(size > 0 && (size_t)size < sizeof(request));
    ts_client_write(client, request, (size_t)size);
}

static void ts_client_read_response(struct ts_client* client, struct ts_response* out, int read_body) {
    memset(out, 0, sizeof(*out));

    struct phr_header headers[32];
    size_t header_count;
    int minor;
    int status;
    const char* msg;
    size_t msg_size;
    int header_size;

    while (1) {
        header_count = cr_count(headers);
        header_size = phr_parse_response((char*)client->rbuf, client->rbuf_offset, &minor, &status, &msg, &msg_size, headers, &header_count, 0);
        if (header_size > 0) {
            break;
        }
        assert(header_size != -1);
        assert(client->rbuf_offset < sizeof(client->rbuf));
        int count = br_sslio_read(&client->ioc, client->rbuf + client->rbuf_offset, sizeof(client->rbuf) - client->rbuf_offset);
        assert(count > 0);
        client->rbuf_offset += (size_t)count;
    }

    out->status = status;

    size_t content_length = 0;
    int has_content_length = 0;

    for (size_t i = 0; i < header_count; i++) {
        if (cl_header_is(&headers[i], "Content-Length")) {
            has_content_length = 1;
            for (size_t c = 0; c < headers[i].value_len; c++) {
                assert(headers[i].value[c] >= '0' && headers[i].value[c] <= '9');
                content_length = content_length * 10 + (size_t)(headers[i].value[c] - '0');
            }
        } else if (cl_header_is(&headers[i], "Connection")) {
            if (headers[i].value_len == 5 && strncasecmp(headers[i].value, "close", 5) == 0) {
                out->has_connection_close = 1;
            }
        } else if (cl_header_is(&headers[i], "Content-Encoding")) {
            if (headers[i].value_len == 2 && strncasecmp(headers[i].value, "br", 2) == 0) {
                out->has_content_encoding_br = 1;
            }
        } else if (cl_header_is(&headers[i], "Content-Type")) {
            size_t copy_size = MIN(headers[i].value_len, sizeof(out->content_type) - 1);
            memcpy(out->content_type, headers[i].value, copy_size);
            out->content_type[copy_size] = 0;
        } else if (cl_header_is(&headers[i], "Location")) {
            size_t copy_size = MIN(headers[i].value_len, sizeof(out->location) - 1);
            memcpy(out->location, headers[i].value, copy_size);
            out->location[copy_size] = 0;
        }
    }

    assert(has_content_length);

    client->rbuf_offset -= (size_t)header_size;
    if (client->rbuf_offset > 0) {
        memmove(client->rbuf, client->rbuf + header_size, client->rbuf_offset);
    }

    out->body_size = content_length;

    if (read_body == 0 || content_length == 0) {
        return;
    }

    out->body = malloc(content_length);

    size_t body_offset = MIN(client->rbuf_offset, content_length);
    memcpy(out->body, client->rbuf, body_offset);
    client->rbuf_offset -= body_offset;
    if (client->rbuf_offset > 0) {
        memmove(client->rbuf, client->rbuf + body_offset, client->rbuf_offset);
    }

    while (body_offset < content_length) {
        int count = br_sslio_read(&client->ioc, out->body + body_offset, content_length - body_offset);
        assert(count > 0);
        body_offset += (size_t)count;
    }
}

static void ts_client_read_eof(struct ts_client* client) {
    assert(client->rbuf_offset == 0);
    unsigned char byte;
    int count = br_sslio_read(&client->ioc, &byte, 1);
    assert(count <= 0);
}

static const char* ts_content_type_str(enum cl_server_http_content_type content_type) {
    switch (content_type) {
    case cl_server_http_content_type_text_html: return "text/html";
    case cl_server_http_content_type_text_javascript: return "text/javascript";
    case cl_server_http_content_type_application_wasm: return "application/wasm";
    case cl_server_http_content_type_application_octet_stream: return "application/octet-stream";
    default: assert(0); return NULL;
    }
}

static void ts_expect_file(struct ts_client* client, const char* request_path, const struct cl_site_file* file) {
    ts_client_request(client, request_path);

    struct ts_response response;
    ts_client_read_response(client, &response, 1);
    assert(response.status == 200);
    assert(response.body_size == file->size);
    assert(memcmp(response.body, file->data, file->size) == 0);
    assert(response.has_content_encoding_br == (file->content_encoding == cl_server_http_content_encoding_br));
    assert(strcmp(response.content_type, ts_content_type_str(file->content_type)) == 0);
    assert(response.has_connection_close == 0);
    free(response.body);
}

static void ts_expect_not_found(struct ts_client* client, const char* request_path) {
    ts_client_request(client, request_path);

    struct ts_response response;
    ts_client_read_response(client, &response, 1);
    assert(response.status == 404);
    assert(response.body_size == 0);
    free(response.body);
}

static void ts_expect_redirect(struct ts_client* client, const char* request_path, const char* location) {
    ts_client_request(client, request_path);

    struct ts_response response;
    ts_client_read_response(client, &response, 1);
    assert(response.status == 301);
    assert(strcmp(response.location, location) == 0);
    assert(response.body_size == 0);
    free(response.body);
}

static void ts_expect_raw(const char* request, int status, const struct cl_site_file* file, int expect_close) {
    struct ts_client* client = ts_client_open();

    ts_client_write(client, request, strlen(request));

    struct ts_response response;
    ts_client_read_response(client, &response, 1);
    assert(response.status == status);
    assert(response.has_connection_close == expect_close);
    if (file) {
        assert(response.body_size == file->size);
        assert(memcmp(response.body, file->data, file->size) == 0);
    } else {
        assert(response.body_size == 0);
    }
    free(response.body);

    if (expect_close) {
        ts_client_read_eof(client);
    }

    ts_client_shutdown(client);
}

static uint64_t ts_crypto_rng_state = 0x123456789abcdef0ull;

static uint64_t ts_crypto_rng(void) {
    uint64_t x = ts_crypto_rng_state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    ts_crypto_rng_state = x;
    return x;
}

static void ts_crypto_fill(uint8_t* buf, size_t size) {
    for (size_t i = 0; i < size; i++) {
        buf[i] = (uint8_t)ts_crypto_rng();
    }
}

static void test_crypto(void) {
    const br_block_ctr_class* aes = cl_aes_ctr_get_vtable();
    br_ghash gh = cl_ghash_get();
    if (aes == NULL || gh == NULL) {
        printf("test_crypto: skipped\n");
        return;
    }

    for (int iter = 0; iter < 300; iter++) {
        uint8_t key[32];
        uint8_t iv[12];
        uint8_t a[1024];
        uint8_t b[1024];
        size_t key_len = (const size_t[]) { 16, 24, 32 }[iter % 3];
        size_t len = ts_crypto_rng() % (sizeof(a) + 1);
        uint32_t cc = iter % 5 == 0 ? 0xffffffffu - (uint32_t)(ts_crypto_rng() % 4) : (uint32_t)ts_crypto_rng();
        ts_crypto_fill(key, sizeof(key));
        ts_crypto_fill(iv, sizeof(iv));
        ts_crypto_fill(a, len);
        memcpy(b, a, len);

        br_aes_gen_ctr_keys fast;
        br_aes_ct64_ctr_keys ref;
        aes->init(&fast.vtable, key, key_len);
        br_aes_ct64_ctr_init(&ref, key, key_len);
        uint32_t cc_fast = fast.vtable->run(&fast.vtable, iv, cc, a, len);
        uint32_t cc_ref = br_aes_ct64_ctr_run(&ref, iv, cc, b, len);
        assert(cc_fast == cc_ref);
        assert(memcmp(a, b, len) == 0);
    }

    for (int iter = 0; iter < 300; iter++) {
        uint8_t y_fast[16];
        uint8_t y_ref[16];
        uint8_t h[16];
        uint8_t data[1024];
        size_t len = ts_crypto_rng() % (sizeof(data) + 1);
        ts_crypto_fill(y_fast, sizeof(y_fast));
        memcpy(y_ref, y_fast, sizeof(y_fast));
        ts_crypto_fill(h, sizeof(h));
        ts_crypto_fill(data, len);
        gh(y_fast, h, data, len);
        br_ghash_ctmul64(y_ref, h, data, len);
        assert(memcmp(y_fast, y_ref, sizeof(y_fast)) == 0);
    }

    printf("test_crypto: ok\n");
}

static void test_get_app(void) {
    struct ts_client* client = ts_client_open();

    ts_expect_redirect(client, "/app", "/app/");
    ts_expect_file(client, "/app/", ts_index_html);
    ts_expect_file(client, "/app/index.html", ts_index_html);
    ts_expect_file(client, "/app/index.html?v=123", ts_index_html);
    ts_expect_redirect(client, "/app?v=123", "/app/");
    ts_expect_not_found(client, "/");

    ts_client_shutdown(client);
    printf("test_get_app: ok\n");
}

static void test_get_big(void) {
    struct ts_client* client = ts_client_open();
    ts_expect_file(client, "/big.bin", ts_big_bin);
    ts_client_shutdown(client);
    printf("test_get_big: ok\n");
}

static void test_not_found(void) {
    struct ts_client* client = ts_client_open();

    ts_expect_not_found(client, "/missing");
    ts_expect_file(client, "/app/index.html", ts_index_html);
    ts_expect_not_found(client, "//");
    ts_expect_not_found(client, "/app/index.html/");
    ts_expect_not_found(client, "/APP/INDEX.HTML");
    ts_expect_not_found(client, "/app/missing.js");
    ts_expect_not_found(client, "/big.bin/");

    ts_client_shutdown(client);
    printf("test_not_found: ok\n");
}

static void test_keep_alive_sequence(void) {
    struct ts_client* client = ts_client_open();

    for (int i = 0; i < 32; i++) {
        switch (i % 3) {
        case 0: ts_expect_file(client, "/app/index.html", ts_index_html); break;
        case 1: ts_expect_file(client, "/app/app.js", ts_app_js); break;
        case 2: ts_expect_not_found(client, "/nope"); break;
        }
    }

    ts_client_shutdown(client);
    printf("test_keep_alive_sequence: ok\n");
}

static void test_back_to_back(void) {
    struct ts_client* client = ts_client_open();

    const char* requests =
        "GET /app/index.html HTTP/1.1\r\nHost: localhost\r\n\r\n"
        "GET /app/app.js HTTP/1.1\r\nHost: localhost\r\n\r\n";
    ts_client_write(client, requests, strlen(requests));

    struct ts_response response;
    ts_client_read_response(client, &response, 1);
    assert(response.status == 200);
    assert(response.body_size == ts_index_html->size);
    assert(memcmp(response.body, ts_index_html->data, ts_index_html->size) == 0);
    free(response.body);

    ts_client_read_response(client, &response, 1);
    assert(response.status == 200);
    assert(response.body_size == ts_app_js->size);
    assert(memcmp(response.body, ts_app_js->data, ts_app_js->size) == 0);
    free(response.body);

    ts_client_shutdown(client);
    printf("test_back_to_back: ok\n");
}

static void test_head(void) {
    struct ts_client* client = ts_client_open();

    const char* request = "HEAD /app/index.html HTTP/1.1\r\nHost: localhost\r\n\r\n";
    ts_client_write(client, request, strlen(request));

    struct ts_response response;
    ts_client_read_response(client, &response, 0);
    assert(response.status == 200);
    assert(response.body_size == ts_index_html->size);
    assert(response.has_connection_close == 0);
    free(response.body);

    ts_expect_file(client, "/app/index.html", ts_index_html);

    ts_client_shutdown(client);
    printf("test_head: ok\n");
}

static void test_method_not_allowed(void) {
    ts_expect_raw("PUT /app/index.html HTTP/1.1\r\nHost: localhost\r\n\r\n", 405, NULL, 1);
    printf("test_method_not_allowed: ok\n");
}

static void test_request_body_rejected(void) {
    ts_expect_raw("POST /app/index.html HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\n\r\nhello", 400, NULL, 1);

    size_t body_size = 100 * 1024;
    char* request = malloc(body_size + 256);
    int prefix_size = snprintf(request, 256, "POST /app/index.html HTTP/1.1\r\nHost: localhost\r\nContent-Length: %zu\r\n\r\n", body_size);
    memset(request + prefix_size, 'b', body_size);

    struct ts_client* client = ts_client_open();
    ts_client_write_aborted(client, request, (size_t)prefix_size + body_size);
    free(request);

    struct ts_response response;
    ts_client_read_response(client, &response, 1);
    assert(response.status == 400);
    assert(response.has_connection_close);
    free(response.body);
    ts_client_shutdown(client);

    printf("test_request_body_rejected: ok\n");
}

static void test_content_length_zero(void) {
    ts_expect_raw("GET /app/index.html HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\n\r\n", 200, ts_index_html, 0);
    printf("test_content_length_zero: ok\n");
}

static void test_transfer_encoding_rejected(void) {
    ts_expect_raw("GET /app/index.html HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n", 400, NULL, 1);
    printf("test_transfer_encoding_rejected: ok\n");
}

static void test_http_1_0(void) {
    ts_expect_raw("GET /app/index.html HTTP/1.0\r\n\r\n", 200, ts_index_html, 1);
    printf("test_http_1_0: ok\n");
}

static void test_connection_close(void) {
    ts_expect_raw("GET /app/app.js HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n", 200, ts_app_js, 1);
    ts_expect_raw("GET /app/app.js HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive, close\r\n\r\n", 200, ts_app_js, 1);

    struct ts_client* client = ts_client_open();
    ts_expect_file(client, "/app/index.html", ts_index_html);
    const char* not_close = "GET /app/app.js HTTP/1.1\r\nHost: localhost\r\nConnection: closely\r\n\r\n";
    ts_client_write(client, not_close, strlen(not_close));
    struct ts_response response;
    ts_client_read_response(client, &response, 1);
    assert(response.status == 200);
    assert(response.has_connection_close == 0);
    free(response.body);
    ts_expect_file(client, "/app/index.html", ts_index_html);
    ts_client_shutdown(client);

    printf("test_connection_close: ok\n");
}

static void test_malformed_request(void) {
    struct ts_client* client = ts_client_open();

    const char* request = "garbage request line\r\n\r\n";
    ts_client_write(client, request, strlen(request));
    ts_client_read_eof(client);

    ts_client_shutdown(client);
    printf("test_malformed_request: ok\n");
}

static void test_oversized_request(void) {
    struct ts_client* client = ts_client_open();

    size_t filler_size = 20 * 1024;
    char* request = malloc(filler_size + 256);
    int prefix_size = snprintf(request, 256, "GET /app/index.html HTTP/1.1\r\nHost: localhost\r\nX-Filler: ");
    memset(request + prefix_size, 'a', filler_size);
    memcpy(request + prefix_size + filler_size, "\r\n\r\n", 4);

    ts_client_write_aborted(client, request, (size_t)prefix_size + filler_size + 4);
    free(request);

    ts_client_read_eof(client);
    ts_client_shutdown(client);

    client = ts_client_open();
    ts_expect_file(client, "/app/index.html", ts_index_html);
    ts_client_shutdown(client);

    printf("test_oversized_request: ok\n");
}

static void test_slow_client(void) {
    struct ts_client* client = ts_client_open();

    const char* request = "GET /app/index.html HTTP/1.1\r\nHost: localhost\r\n\r\n";
    for (size_t i = 0; i < strlen(request); i++) {
        ts_client_write(client, request + i, 1);
        usleep(2000);
    }

    struct ts_response response;
    ts_client_read_response(client, &response, 1);
    assert(response.status == 200);
    assert(response.body_size == ts_index_html->size);
    free(response.body);

    ts_client_shutdown(client);
    printf("test_slow_client: ok\n");
}

static void test_partial_request_close(void) {
    struct ts_client* client = ts_client_open();

    const char* partial = "GET /ind";
    ts_client_write(client, partial, strlen(partial));

    ts_client_shutdown(client);

    client = ts_client_open();
    ts_expect_file(client, "/app/index.html", ts_index_html);
    ts_client_shutdown(client);

    printf("test_partial_request_close: ok\n");
}

static void test_tls_garbage(void) {
    int fd = ts_sock_connect();
    assert(fd != -1);

    const char* garbage = "this is not a tls client hello at all........";
    ssize_t written = write(fd, garbage, strlen(garbage));
    assert(written > 0);

    char buf[256];
    while (1) {
        ssize_t count = read(fd, buf, sizeof(buf));
        if (count <= 0) {
            break;
        }
    }
    close(fd);

    struct ts_client* client = ts_client_open();
    ts_expect_file(client, "/app/index.html", ts_index_html);
    ts_client_shutdown(client);

    printf("test_tls_garbage: ok\n");
}

static void test_connection_churn(void) {
    for (int i = 0; i < 300; i++) {
        struct ts_client* client = ts_client_open();
        if (i % 7 == 0) {
            ts_expect_file(client, "/app/app.js", ts_app_js);
        } else {
            ts_expect_file(client, "/app/index.html", ts_index_html);
        }
        ts_client_shutdown(client);
    }
    printf("test_connection_churn: ok\n");
}

struct ts_worker_args {
    int index;
    int request_count;
};

static void* ts_concurrent_worker(void* _args) {
    struct ts_worker_args* args = _args;
    uint64_t seed = (uint64_t)args->index + 1000;

    struct ts_client* client = ts_client_open();

    for (int i = 0; i < args->request_count; i++) {
        uint64_t r = cr_rand(&seed);
        switch (r % 4) {
        case 0: ts_expect_file(client, "/app/index.html", ts_index_html); break;
        case 1: ts_expect_file(client, "/app/app.js", ts_app_js); break;
        case 2: ts_expect_file(client, "/big.bin", ts_big_bin); break;
        case 3: ts_expect_not_found(client, "/missing"); break;
        }
    }

    ts_client_shutdown(client);
    return NULL;
}

static void test_concurrent(void) {
    pthread_t threads[16];
    struct ts_worker_args args[16];

    for (int i = 0; i < 16; i++) {
        args[i] = (struct ts_worker_args) { .index = i, .request_count = 20 };
        int err = pthread_create(&threads[i], NULL, ts_concurrent_worker, &args[i]);
        assert(err == 0);
    }
    for (int i = 0; i < 16; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("test_concurrent: ok\n");
}

static void test_idle_timeout(void) {
    struct ts_client* client = ts_client_open();

    ts_expect_file(client, "/app/index.html", ts_index_html);

    double start = cr_get_current_time();
    ts_client_read_eof(client);
    double elapsed = cr_get_current_time() - start;
    assert(elapsed > 4.0 && elapsed < 10.0);

    ts_client_shutdown(client);

    int fd = ts_sock_connect();
    assert(fd != -1);
    char byte;
    ssize_t count = read(fd, &byte, 1);
    assert(count <= 0);
    close(fd);

    printf("test_idle_timeout: ok\n");
}

static void test_conn_counts_settle(void) {
    double start = cr_get_current_time();
    while (atomic_load(&ts_open_count) != atomic_load(&ts_close_count)) {
        assert(cr_get_current_time() - start < 10.0);
        usleep(100000);
    }
    printf("test_conn_counts_settle: ok\n");
}

static void* ts_server_thread(void* args) {
    struct cl* server = args;
    cl_server_run(server);
    assert(0);
    return NULL;
}

static void ts_wait_ready(void) {
    double start = cr_get_current_time();
    while (1) {
        int fd = ts_sock_connect();
        if (fd != -1) {
            close(fd);
            usleep(100000);
            return;
        }
        assert(cr_get_current_time() - start < 5.0);
        usleep(50000);
    }
}

int main(int argc, char** argv) {
    setbuf(stdout, NULL);

    const char* certificate_path = "certificate.pem";
    const char* private_key_path = "private_key.pem";

    struct option options[] = {
        { "certificate", required_argument, NULL, 'c' },
        { "private-key", required_argument, NULL, 'k' },
        { "port", required_argument, NULL, 'p' },
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
            ts_port = atoi(optarg);
            break;
        default:
            printf("usage: %s --certificate <path> --private-key <path> --port <d>\n", argv[0]);
            return 1;
        }
    }

    struct cr_str* certificate_path_str = cr_str_create(cr_mem_libc, certificate_path);
    struct cr_str* certificate = cr_str_create_with_contents_of_file(cr_mem_libc, certificate_path_str);
    if (certificate == NULL) {
        printf("could not read certificate: %s\n", certificate_path);
        return 1;
    }

    struct cr_str* private_key_path_str = cr_str_create(cr_mem_libc, private_key_path);
    struct cr_str* private_key = cr_str_create_with_contents_of_file(cr_mem_libc, private_key_path_str);
    if (private_key == NULL) {
        printf("could not read private key: %s\n", private_key_path);
        return 1;
    }

    ts_files_init();
    ts_trust_init(&ts_trust, (const unsigned char*)cr_str_get_data(certificate), cr_str_get_count(certificate));

    struct cl* server = cl_server_create(&(struct cl_server_params) {
        .mem = cr_mem_libc,
        .certificate_pem = cr_str_get_data(certificate),
        .certificate_pem_size = cr_str_get_count(certificate),
        .private_key_pem = cr_str_get_data(private_key),
        .private_key_pem_size = cr_str_get_count(private_key),
        .port = ts_port,
        .idle_timeout = 5.0,
        .conn_data_size = sizeof(struct ts_conn),
        .on_conn_update = ts_on_conn_update,
    });
    assert(server);

    pthread_t server_thread;
    int err = pthread_create(&server_thread, NULL, ts_server_thread, server);
    assert(err == 0);

    ts_wait_ready();

    test_crypto();
    test_get_app();
    test_get_big();
    test_not_found();
    test_keep_alive_sequence();
    test_back_to_back();
    test_head();
    test_method_not_allowed();
    test_request_body_rejected();
    test_content_length_zero();
    test_transfer_encoding_rejected();
    test_http_1_0();
    test_connection_close();
    test_malformed_request();
    test_oversized_request();
    test_slow_client();
    test_partial_request_close();
    test_tls_garbage();
    test_connection_churn();
    test_concurrent();
    test_idle_timeout();
    test_conn_counts_settle();

    printf("server_tests: ok\n");

    return 0;
}
