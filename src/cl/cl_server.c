#include <arpa/inet.h>
#include <netinet/in.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/socket.h>
#include <unistd.h>
#if __APPLE__
#include <sys/event.h>
#else
#include <sys/epoll.h>
#endif
#include "picohttpparser.h"
#include "cr.h"
cr_warning_push
cr_warning_ignore_everything
#include "bearssl.h"
cr_warning_pop
#include "cl_crypto.h"
#include "cl_private.h"

#define cl_batch_bits 6
#define cl_conn_bits 4
#define cl_flags_bits 4
#define cl_generation_bits 16
#define cl_batch_count_max (1 << cl_batch_bits)
#define cl_conn_count_max (1 << cl_conn_bits)
#define cl_certificate_count_max 8
#define cl_event_count_max 64
#define cl_idle_timeout_default 60.0
#define cl_sweep_interval 1.0

#define cl_conn_handle_flag_is_valid 0x01
#define cl_conn_handle_flag_is_open 0x02

struct cl_conn_handle {
    uint32_t batch: cl_batch_bits;
    uint32_t conn: cl_conn_bits;
    uint32_t flags: cl_flags_bits;
    uint32_t generation: cl_generation_bits;
};
_Static_assert(sizeof(struct cl_conn_handle) == 4, "");

struct cl_conn {
    struct cl_conn_handle handle_self;
    struct cl_conn_handle handle_next;
    void* data;
    int can_write;
    int can_read;
    int close_after_send;
    int is_draining;
    int fd;
    double last_active_time;
    br_ssl_server_context sc;
    uint8_t iobuf[BR_SSL_BUFSIZE_BIDI];
    struct {
        uint8_t buf[1u << 14];
        size_t offset;
        int needs_input;
    } recv;
    struct {
        int is_active;
        uint8_t headers[512];
        size_t header_size;
        size_t header_offset;
        const uint8_t* body;
        size_t body_size;
        size_t body_offset;
    } send;
};

struct cl {
    struct cr_mem* mem;
    int fd;
    double idle_timeout;
    struct {
        uint8_t buf[1u << 14];
        size_t offset;
        br_x509_certificate certificates[cl_certificate_count_max];
        br_rsa_private_key private_key;
        size_t certificate_count;
    } secrets;
    struct {
        br_ssl_session_cache_lru lru;
        uint8_t buf[1u << 15];
    } sessions;
    struct {
        void* batches[cl_batch_count_max];
        size_t batch_count;
        struct cl_conn_handle handle_next;
        size_t conn_size;
        size_t conn_data_offset;
    } conns;
    cl_server_on_conn_update* on_conn_update;
    void* args;
};

static struct cl_conn* cl_conn_handle_get_conn(struct cl* cl, struct cl_conn_handle handle) {
    assert(handle.flags & cl_conn_handle_flag_is_valid);
    assert(handle.batch < cl->conns.batch_count);
    struct cl_conn* conn = (struct cl_conn*)((char*)cl->conns.batches[handle.batch] + handle.conn * cl->conns.conn_size);
    assert(memcmp(&conn->handle_self, &handle, sizeof(handle)) == 0);
    return conn;
}

static struct cl_conn* cl_conn_handle_find_open_conn(struct cl* cl, struct cl_conn_handle handle) {
    if ((handle.flags & cl_conn_handle_flag_is_open) == 0) {
        return NULL;
    }
    if (handle.batch >= cl->conns.batch_count) {
        return NULL;
    }
    struct cl_conn* conn = (struct cl_conn*)((char*)cl->conns.batches[handle.batch] + handle.conn * cl->conns.conn_size);
    if (memcmp(&conn->handle_self, &handle, sizeof(handle)) != 0) {
        return NULL;
    }
    return conn;
}

static struct cl_conn_handle cl_alloc_conn(struct cl* cl) {
    if ((cl->conns.handle_next.flags & cl_conn_handle_flag_is_valid) == 0 && cl->conns.batch_count < cl_batch_count_max) {
        size_t batch_size = cl_conn_count_max * cl->conns.conn_size;
        void* batch = malloc(batch_size);
        if (!batch) {
            goto fail;
        }

        size_t batch_index = cl->conns.batch_count++;
        cl->conns.batches[batch_index] = batch;

        struct cl_conn_handle handle_next = (struct cl_conn_handle) { 0 };

        for (size_t i = 0; i < cl_conn_count_max; i++) {
            struct cl_conn* conn = (struct cl_conn*)((char*)batch + i * cl->conns.conn_size);
            conn->handle_next = handle_next;
            conn->handle_self = (struct cl_conn_handle) {
                .flags = cl_conn_handle_flag_is_valid,
                .conn = (uint32_t)i,
                .batch = (uint32_t)batch_index,
            };
            handle_next = conn->handle_self;
        }

        cl->conns.handle_next = handle_next;
    }

    if (cl->conns.handle_next.flags & cl_conn_handle_flag_is_valid) {
        struct cl_conn* conn = cl_conn_handle_get_conn(cl, cl->conns.handle_next);

#if DEBUG
        size_t no_debug_size = offsetof(struct cl_conn, data);
        memset((char*)conn + no_debug_size, 0xff, cl->conns.conn_size - no_debug_size);
#endif

        if (conn->handle_next.flags & cl_conn_handle_flag_is_valid) {
            struct cl_conn* conn_next = cl_conn_handle_get_conn(cl, conn->handle_next);
            conn_next->handle_self.generation++;
            cl->conns.handle_next = conn_next->handle_self;
        } else {
            cl->conns.handle_next = (struct cl_conn_handle) { 0 };
        }

        conn->handle_self.generation++;
        conn->handle_self.flags |= cl_conn_handle_flag_is_open;
        conn->handle_next = (struct cl_conn_handle) { 0 };

        conn->data = (char*)conn + cl->conns.conn_data_offset;
        conn->can_write = 0;
        conn->can_read = 0;
        conn->close_after_send = 0;
        conn->is_draining = 0;
        conn->fd = -1;
        conn->last_active_time = cr_get_current_time();

        br_ssl_server_init_full_rsa(&conn->sc, cl->secrets.certificates, cl->secrets.certificate_count, &cl->secrets.private_key);
        cl_crypto_init_engine(&conn->sc.eng);
        br_ssl_server_set_cache(&conn->sc, &cl->sessions.lru.vtable);
        br_ssl_engine_set_buffer(&conn->sc.eng, conn->iobuf, sizeof(conn->iobuf), 1);

        conn->recv.offset = 0;
        conn->recv.needs_input = 0;
        conn->send.is_active = 0;

        cl->on_conn_update(&(struct cl_server_conn_update) {
            .type = cl_server_conn_update_type_open,
            .data = conn->data,
        }, cl->args);

        return conn->handle_self;
    }

fail:
    return (struct cl_conn_handle) { 0 };
}

static void cl_dealloc_conn(struct cl* cl, struct cl_conn* conn) {
    assert(conn->handle_self.flags & cl_conn_handle_flag_is_open);

    cl->on_conn_update(&(struct cl_server_conn_update) {
        .type = cl_server_conn_update_type_close,
        .data = conn->data,
    }, cl->args);

    conn->handle_self.flags &= ~(uint32_t)cl_conn_handle_flag_is_open;

    if (cl->conns.handle_next.flags & cl_conn_handle_flag_is_valid) {
        struct cl_conn* conn_next = cl_conn_handle_get_conn(cl, cl->conns.handle_next);
        conn_next->handle_self.generation++;
        conn->handle_next = conn_next->handle_self;
    } else {
        conn->handle_next = (struct cl_conn_handle) { 0 };
    }
    conn->handle_self.generation++;
    cl->conns.handle_next = conn->handle_self;
}

static void cl_close_conn(struct cl* cl, struct cl_conn* conn) {
    close(conn->fd);
    cl_dealloc_conn(cl, conn);
}

static int cl_read_certificates(struct cl* cl, const char* pem, size_t pem_size) {
    void* obj_data[cl_certificate_count_max];
    size_t obj_size[cl_certificate_count_max];
    size_t obj_count = cl_certificate_count_max;

    ssize_t offset = cl_decode_pem((const uint8_t*)pem, pem_size, cl->secrets.buf + cl->secrets.offset, sizeof(cl->secrets.buf) - cl->secrets.offset, obj_data, obj_size, &obj_count);
    if (offset == -1) {
        return -1;
    }
    cl->secrets.offset += (size_t)offset;

    if (obj_count == 0) {
        cl_log("no certificates in pem\n");
        return -1;
    }

    for (size_t i = 0; i < obj_count; i++) {
        cl->secrets.certificates[i].data = obj_data[i];
        cl->secrets.certificates[i].data_len = obj_size[i];
    }
    cl->secrets.certificate_count = obj_count;

    return 0;
}

static int cl_read_private_key(struct cl* cl, const char* pem, size_t pem_size) {
    void* obj_data;
    size_t obj_size;
    size_t obj_count = 1;

    ssize_t offset = cl_decode_pem((const uint8_t*)pem, pem_size, cl->secrets.buf + cl->secrets.offset, sizeof(cl->secrets.buf) - cl->secrets.offset, &obj_data, &obj_size, &obj_count);
    if (offset == -1) {
        return -1;
    }
    cl->secrets.offset += (size_t)offset;

    if (obj_count == 0) {
        cl_log("no private key in pem\n");
        return -1;
    }

    br_skey_decoder_context dc;
    br_skey_decoder_init(&dc);
    br_skey_decoder_push(&dc, obj_data, obj_size);

    int err = br_skey_decoder_last_error(&dc);
    if (err) {
        cl_log("error while decoding private key, error code: %d\n", err);
        return -1;
    }

    if (br_skey_decoder_key_type(&dc) != BR_KEYTYPE_RSA) {
        cl_log("error while decoding private key (not rsa)\n");
        return -1;
    }

    const br_rsa_private_key* src = br_skey_decoder_get_rsa(&dc);

    if (cl->secrets.offset + src->plen + src->qlen + src->dplen + src->dqlen + src->iqlen > sizeof(cl->secrets.buf)) {
        cl_log("error while decoding private key (buf)\n");
        return -1;
    }

    br_rsa_private_key* dst = &cl->secrets.private_key;
    memcpy(dst, src, sizeof(*dst));

#define _copy(field) \
    dst->field = (uint8_t*)(cl->secrets.buf + cl->secrets.offset); \
    memcpy(dst->field, src->field, dst->field##len); \
    cl->secrets.offset += dst->field##len;

    _copy(p)
    _copy(q)
    _copy(dp)
    _copy(dq)
    _copy(iq)

#undef _copy

    return 0;
}

static int cl_make_non_blocking(int fd) {
    int a = fcntl(fd, F_GETFL, 0);
    if (a == -1) {
        cl_log("fcntl(F_GETFL): %s\n", cl_errno);
        return -1;
    }

    a = fcntl(fd, F_SETFL, a | O_NONBLOCK);
    if (a == -1) {
        cl_log("fcntl(F_SETFL): %s\n", cl_errno);
        return -1;
    }

    return 0;
}

static ssize_t cl_non_blocking_write(int fd, int* can_write, const uint8_t* buf, ssize_t buf_size) {
    assert(*can_write);
    ssize_t offset = 0;
    while (offset < buf_size) {
        ssize_t err = write(fd, buf + offset, (size_t)(buf_size - offset));
        if (err <= 0) {
            if (err == -1 && errno == EAGAIN) {
                *can_write = 0;
                break;
            }
            if (err == -1 && errno == EINTR) {
                continue;
            }
            return -1;
        }
        offset += err;
    }
    return offset;
}

static ssize_t cl_non_blocking_read(int fd, int* can_read, uint8_t* buf, ssize_t buf_size) {
    assert(*can_read);
    ssize_t offset = 0;
    while (offset < buf_size) {
        ssize_t err = read(fd, buf + offset, (size_t)(buf_size - offset));
        if (err <= 0) {
            if (err == -1 && errno == EAGAIN) {
                *can_read = 0;
                break;
            }
            if (err == -1 && errno == EINTR) {
                continue;
            }
            return -1;
        }
        offset += err;
    }
    return offset;
}

static void cl_conn_start_send(struct cl_conn* conn, int status_code, const struct cl_server_http_response_desc* desc, const char* location, int is_head) {
    const char* status_text;
    switch (status_code) {
    case 200: status_text = "OK"; break;
    case 301: status_text = "Moved Permanently"; break;
    case 400: status_text = "Bad Request"; break;
    case 404: status_text = "Not Found"; break;
    case 405: status_text = "Method Not Allowed"; break;
    default: assert(0);
    }

    char* buf = (char*)conn->send.headers;
    size_t buf_size = sizeof(conn->send.headers);
    size_t offset = 0;
    int err;

#define _append(...) \
    err = snprintf(buf + offset, buf_size - offset, __VA_ARGS__); \
    assert(err >= 0 && (size_t)err < buf_size - offset); \
    offset += (size_t)err;

    _append("HTTP/1.1 %d %s\r\n", status_code, status_text)
    _append("Content-Length: %zu\r\n", desc->static_body_size)

    if (desc->content_type != cl_server_http_content_type_none) {
        const char* content_type = NULL;
        switch (desc->content_type) {
        case cl_server_http_content_type_none: assert(0); break;
        case cl_server_http_content_type_text_html: content_type = "text/html"; break;
        case cl_server_http_content_type_text_javascript: content_type = "text/javascript"; break;
        case cl_server_http_content_type_application_wasm: content_type = "application/wasm"; break;
        case cl_server_http_content_type_application_octet_stream: content_type = "application/octet-stream"; break;
        }
        _append("Content-Type: %s\r\n", content_type)
    }

    if (desc->content_encoding != cl_server_http_content_encoding_none) {
        const char* content_encoding = NULL;
        switch (desc->content_encoding) {
        case cl_server_http_content_encoding_none: assert(0); break;
        case cl_server_http_content_encoding_br: content_encoding = "br"; break;
        }
        _append("Content-Encoding: %s\r\n", content_encoding)
    }

    if (location) {
        _append("Location: %s\r\n", location)
    }

    if (desc->extra_headers) {
        size_t size = strlen(desc->extra_headers);
        assert(size >= 2 && memcmp(desc->extra_headers + size - 2, "\r\n", 2) == 0);
        _append("%s", desc->extra_headers)
    }

    _append("Cross-Origin-Opener-Policy: same-origin\r\n")
    _append("Cross-Origin-Embedder-Policy: require-corp\r\n")

    if (conn->close_after_send) {
        _append("Connection: close\r\n")
    }

    _append("\r\n")

#undef _append

    conn->send.is_active = 1;
    conn->send.header_size = offset;
    conn->send.header_offset = 0;
    conn->send.body = is_head ? NULL : desc->static_body;
    conn->send.body_size = is_head ? 0 : desc->static_body_size;
    conn->send.body_offset = 0;
}

static int cl_header_value_has_token(const struct phr_header* header, const char* token) {
    size_t token_size = strlen(token);
    size_t i = 0;
    while (i < header->value_len) {
        while (i < header->value_len && (header->value[i] == ' ' || header->value[i] == '\t' || header->value[i] == ',')) {
            i++;
        }
        size_t start = i;
        while (i < header->value_len && header->value[i] != ',') {
            i++;
        }
        size_t end = i;
        while (end > start && (header->value[end - 1] == ' ' || header->value[end - 1] == '\t')) {
            end--;
        }
        if (end - start == token_size && strncasecmp(header->value + start, token, token_size) == 0) {
            return 1;
        }
    }
    return 0;
}

static int cl_conn_parse(struct cl* cl, struct cl_conn* conn) {
    struct phr_header headers[64];
    size_t header_count = cr_count(headers);
    const char* method = NULL;
    size_t method_size = 0;
    const char* path = NULL;
    size_t path_size = 0;
    int minor = 0;

    int request_size = phr_parse_request((const char*)conn->recv.buf, conn->recv.offset, &method, &method_size, &path, &path_size, &minor, headers, &header_count, 0);

    if (request_size == -2) {
        if (conn->recv.offset == sizeof(conn->recv.buf)) {
            return -1;
        }
        conn->recv.needs_input = 1;
        return 0;
    }

    if (request_size <= 0) {
        return -1;
    }

    conn->last_active_time = cr_get_current_time();

    if (minor != 1) {
        conn->close_after_send = 1;
    }

    int is_bad = path_size == 0 || path[0] != '/';

    for (size_t i = 0; i < header_count; i++) {
        if (cl_header_is(&headers[i], "Content-Length")) {
            int is_zero = headers[i].value_len > 0;
            for (size_t c = 0; c < headers[i].value_len; c++) {
                if (headers[i].value[c] != '0') {
                    is_zero = 0;
                    break;
                }
            }
            if (!is_zero) {
                is_bad = 1;
            }
        } else if (cl_header_is(&headers[i], "Transfer-Encoding")) {
            is_bad = 1;
        } else if (cl_header_is(&headers[i], "Connection")) {
            if (cl_header_value_has_token(&headers[i], "close")) {
                conn->close_after_send = 1;
            }
        }
    }

    int is_get = method_size == 3 && memcmp(method, "GET", 3) == 0;
    int is_head = method_size == 4 && memcmp(method, "HEAD", 4) == 0;

    if (is_bad) {
        conn->close_after_send = 1;
        cl_conn_start_send(conn, 400, &(struct cl_server_http_response_desc) { 0 }, NULL, 0);
    } else if (!is_get && !is_head) {
        conn->close_after_send = 1;
        cl_conn_start_send(conn, 405, &(struct cl_server_http_response_desc) { 0 }, NULL, 0);
    } else {
        struct cl_server_conn_update update = {
            .type = cl_server_conn_update_type_http_request,
            .http_request = {
                .path = path,
                .path_size = path_size,
            },
            .data = conn->data,
        };
        cl->on_conn_update(&update, cl->args);

        struct cl_server_http_response_desc* desc = &update.http_request.out_desc;
        assert(desc->static_body_size == 0 || desc->static_body != NULL);

        switch (desc->type) {
        case cl_server_http_response_type_ok:
            cl_conn_start_send(conn, 200, desc, NULL, is_head);
            break;
        case cl_server_http_response_type_moved_permanently:
            assert(update.http_request.out_location[0] != 0);
            cl_conn_start_send(conn, 301, desc, update.http_request.out_location, is_head);
            break;
        case cl_server_http_response_type_not_found:
            cl_conn_start_send(conn, 404, desc, NULL, is_head);
            break;
        default:
            assert(0);
        }
    }

    conn->recv.offset -= (size_t)request_size;
    if (conn->recv.offset > 0) {
        memmove(conn->recv.buf, conn->recv.buf + request_size, conn->recv.offset);
    }

    return 0;
}

static ssize_t cl_conn_handle_sendapp(struct cl_conn* conn, uint8_t* buf, size_t buf_size) {
    size_t offset = 0;

    if (conn->send.header_offset < conn->send.header_size) {
        size_t copy_size = MIN(buf_size, conn->send.header_size - conn->send.header_offset);
        memcpy(buf, conn->send.headers + conn->send.header_offset, copy_size);
        conn->send.header_offset += copy_size;
        offset += copy_size;
    }

    if (offset < buf_size && conn->send.body_offset < conn->send.body_size) {
        size_t copy_size = MIN(buf_size - offset, conn->send.body_size - conn->send.body_offset);
        memcpy(buf + offset, conn->send.body + conn->send.body_offset, copy_size);
        conn->send.body_offset += copy_size;
        offset += copy_size;
    }

    if (conn->send.header_offset == conn->send.header_size && conn->send.body_offset == conn->send.body_size) {
        conn->send.is_active = 0;
    }

    conn->last_active_time = cr_get_current_time();

    return (ssize_t)offset;
}

static ssize_t cl_conn_handle_recvapp(struct cl_conn* conn, const uint8_t* buf, size_t buf_size) {
    size_t copy_size = MIN(buf_size, sizeof(conn->recv.buf) - conn->recv.offset);
    assert(copy_size > 0);

    memcpy(conn->recv.buf + conn->recv.offset, buf, copy_size);
    conn->recv.offset += copy_size;
    conn->recv.needs_input = 0;

    return (ssize_t)copy_size;
}

static void cl_conn_process(struct cl* cl, struct cl_conn* conn) {
    while (1) {
        uint32_t status = br_ssl_engine_current_state(&conn->sc.eng);

        if (status & BR_SSL_CLOSED) {
            goto close;
        }

        int can_sendrec = (status & BR_SSL_SENDREC) && conn->can_write;
        int can_recvrec = (status & BR_SSL_RECVREC) && conn->can_read;
        int can_parse = !conn->send.is_active && !conn->close_after_send && !conn->recv.needs_input && conn->recv.offset > 0;
        int can_sendapp = (status & BR_SSL_SENDAPP) && conn->send.is_active;
        int can_discard = (status & BR_SSL_RECVAPP) && conn->close_after_send;
        int can_recvapp = (status & BR_SSL_RECVAPP) && !conn->close_after_send && conn->recv.offset < sizeof(conn->recv.buf);

        if (can_sendrec) {
            size_t buf_size;
            uint8_t* buf = br_ssl_engine_sendrec_buf(&conn->sc.eng, &buf_size);
            ssize_t count = cl_non_blocking_write(conn->fd, &conn->can_write, buf, (ssize_t)buf_size);
            if (count == -1) goto close;
            if (count > 0) {
                assert(count <= (ssize_t)buf_size);
                br_ssl_engine_sendrec_ack(&conn->sc.eng, (size_t)count);
            }
        } else if (can_recvrec) {
            size_t buf_size;
            uint8_t* buf = br_ssl_engine_recvrec_buf(&conn->sc.eng, &buf_size);
            ssize_t count = cl_non_blocking_read(conn->fd, &conn->can_read, buf, (ssize_t)buf_size);
            if (count == -1) goto close;
            if (count > 0) {
                assert(count <= (ssize_t)buf_size);
                br_ssl_engine_recvrec_ack(&conn->sc.eng, (size_t)count);
            }
        } else if (can_parse) {
            int err = cl_conn_parse(cl, conn);
            if (err == -1) goto close;
        } else if (can_sendapp) {
            size_t buf_size;
            uint8_t* buf = br_ssl_engine_sendapp_buf(&conn->sc.eng, &buf_size);
            ssize_t count = cl_conn_handle_sendapp(conn, buf, buf_size);
            assert(count > 0 && count <= (ssize_t)buf_size);
            br_ssl_engine_sendapp_ack(&conn->sc.eng, (size_t)count);
            br_ssl_engine_flush(&conn->sc.eng, 0);
        } else if (can_discard) {
            size_t buf_size;
            br_ssl_engine_recvapp_buf(&conn->sc.eng, &buf_size);
            assert(buf_size > 0);
            br_ssl_engine_recvapp_ack(&conn->sc.eng, buf_size);
        } else if (can_recvapp) {
            size_t buf_size;
            uint8_t* buf = br_ssl_engine_recvapp_buf(&conn->sc.eng, &buf_size);
            ssize_t count = cl_conn_handle_recvapp(conn, buf, buf_size);
            assert(count > 0 && count <= (ssize_t)buf_size);
            br_ssl_engine_recvapp_ack(&conn->sc.eng, (size_t)count);
        } else {
            if (conn->close_after_send && !conn->send.is_active && (status & BR_SSL_SENDREC) == 0 && !conn->is_draining) {
                conn->is_draining = 1;
                shutdown(conn->fd, SHUT_WR);
            }
            return;
        }
    }

close:
    cl_close_conn(cl, conn);
}

struct cl* cl_server_create(const struct cl_server_params* params) {
    int result = -1;
    struct cl* cl = NULL;

    signal(SIGPIPE, SIG_IGN);
    cl_inc_fd_limit();

    cl = cr_malloc(params->mem, sizeof(*cl), 0, cr_loc);
    memset(cl, 0, sizeof(*cl));
    cl->mem = params->mem;
    cl->fd = -1;
    cl->idle_timeout = params->idle_timeout > 0 ? params->idle_timeout : cl_idle_timeout_default;

    size_t conn_size = 0;
    conn_size += align_up_uint64(sizeof(struct cl_conn), 16);
    cl->conns.conn_data_offset = conn_size;
    conn_size += align_up_uint64(params->conn_data_size, 16);
    cl->conns.conn_size = conn_size;

    cl->on_conn_update = params->on_conn_update;
    cl->args = params->args;

    if (params->port < 1 || params->port > 65535) {
        cl_log("invalid port: %d\n", params->port);
        goto end;
    }

    int cert_err = cl_read_certificates(cl, params->certificate_pem, params->certificate_pem_size);
    if (cert_err == -1) {
        goto end;
    }

    cert_err = cl_read_private_key(cl, params->private_key_pem, params->private_key_pem_size);
    if (cert_err == -1) {
        goto end;
    }

    br_ssl_session_cache_lru_init(&cl->sessions.lru, cl->sessions.buf, sizeof(cl->sessions.buf));

    struct sockaddr_in addr4;
    struct sockaddr_in6 addr6;
    memset(&addr4, 0, sizeof(addr4));
    memset(&addr6, 0, sizeof(addr6));
    addr4.sin_family = AF_INET;
    addr4.sin_port = htons((uint16_t)params->port);
    addr6.sin6_family = AF_INET6;
    addr6.sin6_port = htons((uint16_t)params->port);
    struct sockaddr* addr = (struct sockaddr*)&addr6;
    socklen_t addr_size = sizeof(addr6);
    if (params->bind_address == NULL) {
        addr6.sin6_addr = in6addr_any;
    } else if (inet_pton(AF_INET, params->bind_address, &addr4.sin_addr) == 1) {
        addr = (struct sockaddr*)&addr4;
        addr_size = sizeof(addr4);
    } else if (inet_pton(AF_INET6, params->bind_address, &addr6.sin6_addr) != 1) {
        cl_log("invalid bind address: %s\n", params->bind_address);
        goto end;
    }

    cl->fd = socket(addr->sa_family, SOCK_STREAM, 0);
    if (cl->fd == -1 && params->bind_address == NULL) {
        addr4.sin_addr.s_addr = INADDR_ANY;
        addr = (struct sockaddr*)&addr4;
        addr_size = sizeof(addr4);
        cl->fd = socket(addr->sa_family, SOCK_STREAM, 0);
    }
    if (cl->fd == -1) {
        cl_log("could not create socket\n%s\n", cl_errno);
        goto end;
    }

    int opt_flag = 1;
    int err = setsockopt(cl->fd, SOL_SOCKET, SO_REUSEADDR, &opt_flag, (socklen_t)sizeof(opt_flag));
    if (err == -1) {
        cl_log("could not set SO_REUSEADDR\n%s\n", cl_errno);
        goto end;
    }

    if (addr->sa_family == AF_INET6) {
        int v6_only = 0;
        err = setsockopt(cl->fd, IPPROTO_IPV6, IPV6_V6ONLY, &v6_only, (socklen_t)sizeof(v6_only));
        if (err == -1) {
            cl_log("could not set IPV6_V6ONLY\n%s\n", cl_errno);
            goto end;
        }
    }

    err = bind(cl->fd, addr, addr_size);
    if (err == -1) {
        cl_log("could not bind to socket\n%s\n", cl_errno);
        goto end;
    }

    err = cl_make_non_blocking(cl->fd);
    if (err == -1) {
        goto end;
    }

    err = listen(cl->fd, INT_MAX);
    if (err == -1) {
        cl_log("could not listen to socket\n%s\n", cl_errno);
        goto end;
    }

    result = 0;

end:
    if (result == -1 && cl) {
        if (cl->fd != -1) {
            close(cl->fd);
        }
        cr_free(params->mem, cl, cr_loc);
        cl = NULL;
    }
    return cl;
}

static void cl_sweep_idle_conns(struct cl* cl, double now) {
    for (size_t batch = 0; batch < cl->conns.batch_count; batch++) {
        for (size_t i = 0; i < cl_conn_count_max; i++) {
            struct cl_conn* conn = (struct cl_conn*)((char*)cl->conns.batches[batch] + i * cl->conns.conn_size);
            if ((conn->handle_self.flags & cl_conn_handle_flag_is_open) && now - conn->last_active_time > cl->idle_timeout) {
                cl_close_conn(cl, conn);
            }
        }
    }
}

static int cl_accept_conns(struct cl* cl, int efd) {
    while (1) {
        int fd = accept(cl->fd, NULL, NULL);
        if (fd == -1) {
            if (errno == EAGAIN) {
                return 0;
            }
            if (errno == EINTR || errno == ECONNABORTED) {
                continue;
            }
            cl_log("accept(): %s\n", cl_errno);
            return 0;
        }

        int err = cl_make_non_blocking(fd);
        if (err == -1) {
            close(fd);
            continue;
        }

        struct cl_conn_handle handle = cl_alloc_conn(cl);
        if ((handle.flags & cl_conn_handle_flag_is_valid) == 0) {
            cl_log("conn alloc failed\n");
            close(fd);
            continue;
        }

        struct cl_conn* conn = cl_conn_handle_get_conn(cl, handle);
        conn->fd = fd;

        err = br_ssl_server_reset(&conn->sc);
        if (err == 0) {
            cl_log("conn ssl reset error\n");
            cl_close_conn(cl, conn);
            continue;
        }

#if __APPLE__
        struct kevent evs[2];
        memset(evs, 0, sizeof(evs));
        for (int e = 0; e < 2; e++) {
            evs[e].ident = (uintptr_t)conn->fd;
            evs[e].filter = e == 0 ? EVFILT_READ : EVFILT_WRITE;
            evs[e].flags = EV_ADD | EV_ENABLE | EV_CLEAR;
            memcpy(&evs[e].udata, &handle, sizeof(handle));
        }
        err = kevent(efd, evs, 2, NULL, 0, NULL);
#else
        struct epoll_event ev;
        memset(&ev, 0, sizeof(ev));
        ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
        memcpy(&ev.data, &handle, sizeof(handle));
        err = epoll_ctl(efd, EPOLL_CTL_ADD, conn->fd, &ev);
#endif
        if (err == -1) {
            cl_log("conn event add failed\n%s\n", cl_errno);
            cl_close_conn(cl, conn);
            continue;
        }
    }
}

int cl_server_run(struct cl* cl) {
    int result = -1;

#if __APPLE__
    int efd = kqueue();
#else
    int efd = epoll_create1(0);
#endif
    if (efd == -1) {
        cl_log("event init failed\n%s\n", cl_errno);
        goto end;
    }

#if __APPLE__
    struct kevent server_ev;
    memset(&server_ev, 0, sizeof(server_ev));
    server_ev.ident = (uintptr_t)cl->fd;
    server_ev.filter = EVFILT_READ;
    server_ev.flags = EV_ADD | EV_ENABLE | EV_CLEAR;
    int err = kevent(efd, &server_ev, 1, NULL, 0, NULL);
#else
    struct epoll_event server_ev;
    memset(&server_ev, 0, sizeof(server_ev));
    server_ev.events = EPOLLIN | EPOLLET;
    int err = epoll_ctl(efd, EPOLL_CTL_ADD, cl->fd, &server_ev);
#endif
    if (err == -1) {
        cl_log("event add failed\n%s\n", cl_errno);
        goto end;
    }

    cl_log("started...\n");

    double sweep_time = cr_get_current_time();

    while (1) {
#if __APPLE__
        struct kevent events[cl_event_count_max];
        struct timespec timeout = { .tv_sec = 1 };
        int event_count = kevent(efd, NULL, 0, events, cl_event_count_max, &timeout);
#else
        struct epoll_event events[cl_event_count_max];
        int event_count = epoll_wait(efd, events, cl_event_count_max, 1000);
#endif
        if (event_count == -1) {
            if (errno == EINTR) {
                continue;
            }
            cl_log("event wait failed\n%s\n", cl_errno);
            goto end;
        }

        for (int i = 0; i < event_count; i++) {
            struct cl_conn_handle event_handle;
#if __APPLE__
            memcpy(&event_handle, &events[i].udata, sizeof(event_handle));
            int event_can_read = events[i].filter == EVFILT_READ;
            int event_can_write = events[i].filter == EVFILT_WRITE;
#else
            memcpy(&event_handle, &events[i].data, sizeof(event_handle));
            int event_can_read = (events[i].events & (EPOLLIN | EPOLLERR | EPOLLHUP)) != 0;
            int event_can_write = (events[i].events & (EPOLLOUT | EPOLLERR | EPOLLHUP)) != 0;
#endif
            if ((event_handle.flags & cl_conn_handle_flag_is_valid) == 0) {
                err = cl_accept_conns(cl, efd);
                if (err == -1) {
                    goto end;
                }
            } else {
                struct cl_conn* conn = cl_conn_handle_find_open_conn(cl, event_handle);
                if (conn == NULL) {
                    continue;
                }

                if (event_can_write) {
                    conn->can_write = 1;
                }
                if (event_can_read) {
                    conn->can_read = 1;
                }

                cl_conn_process(cl, conn);
            }
        }

        double now = cr_get_current_time();
        if (now - sweep_time >= cl_sweep_interval) {
            sweep_time = now;
            cl_sweep_idle_conns(cl, now);
            cl_accept_conns(cl, efd);
        }
    }

end:
    if (efd != -1) {
        close(efd);
    }
    return result;
}
