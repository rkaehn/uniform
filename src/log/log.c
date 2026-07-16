#include "log.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <stdalign.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef log_signal
#define log_signal 0
#endif
#ifndef log_mmap
#define log_mmap 0
#endif
#ifndef log_std
#define log_std 0
#endif
#if log_std && !defined(__wasm__)
#define log_capture 1
#else
#define log_capture 0
#endif

#if log_signal
#include <execinfo.h>
#include <signal.h>
#endif

#if log_mmap
#include <sys/mman.h>
#endif

#define log_run_bits 8
#define log_line_bits 8
#define log_line_buf_size (sizeof(struct log_line) * (1u << log_line_bits))
#define log_line_msg_size (sizeof(((struct log_line*)NULL)->msg))
#define log_line_flag_msg 0x01
#define log_line_flag_msg_first 0x02
#define log_line_flag_msg_last 0x04

#define log_min(a, b) ((a) < (b) ? (a) : (b))
#define log_max(a, b) ((a) > (b) ? (a) : (b))

static_assert(log_run_bits <= 16, "");
static_assert(log_line_bits <= 16, "");

struct log_line {
    struct {
        uint16_t flags;
        uint16_t size;
    } header;
    char msg[252];
};
static_assert(sizeof(struct log_line) == 256, "");

struct log_header {
    alignas(128) atomic_uint_fast64_t counter;
    alignas(128) struct {
        uint16_t prefs[log_pref_count];
    } user;
};
static_assert(sizeof(struct log_header) == 256, "");

#if log_capture
struct log_pipe {
    bool is_init;
    bool use_old;
    int fd_originals[2];
    int fds[2];
    char temp[1u << 10];
    uint32_t first;
    uint32_t count;
};
#endif

struct log {
    int fd;
    uint8_t* raw;
    uint32_t size;
#if log_capture
    struct log_pipe pipe;
#endif
#if log_signal
    struct {
        struct sigaction segv;
        struct sigaction ill;
        struct sigaction abrt;
        struct sigaction fpe;
    } signal;
#endif
    struct {
        alignas(128) atomic_uint_fast8_t barrier_index;
        alignas(128) atomic_uint_fast32_t barrier_0;
        alignas(128) atomic_uint_fast32_t barrier_1;
    } render;
    struct {
        bool is_frozen;
        uint16_t frozen_line;
        alignas(128) uint8_t frozen_log_line_buf[log_line_buf_size];
    } view;
};

static void log_store(const char* data, uint32_t size);
static void log_apply_mode(enum log_mode);
static void log_freeze(void);
void log_dump(void);

static struct log* log_ptr(void) {
    static struct log log;
    return &log;
}

static uint16_t log_counter_run(uint64_t counter) {
    return (uint16_t)counter & ((1u << log_run_bits) - 1u);
}

static uint16_t log_counter_line(uint64_t counter) {
    return (uint16_t)(counter >> log_run_bits) & ((1u << log_line_bits) - 1u);
}

#if log_signal
static void log_handle_signal(int sig, siginfo_t* info, void* user) {
    (void)user;

    log_dump();

    static int latch = 0;
    if (latch) {
        return;
    }
    latch = 1;

    char** strings = NULL;

    char temp[1u << 12];
    uint32_t offset = 0;

    void* callstack[16];
    int n = backtrace(callstack, sizeof(callstack) / sizeof(callstack[0]));
    strings = backtrace_symbols(callstack, n);

    int err = snprintf(temp + offset, sizeof(temp) - offset, "%s, %p\n", strsignal(sig), info->si_addr);
    if (err == -1) {
        goto end;
    }
    offset = log_min(sizeof(temp) - 1, offset + (uint32_t)err);

    for (int i = 0; i < n; i++) {
        err = snprintf(temp + offset, sizeof(temp) - offset, "%s%s", strings[i], i == (n - 1) ? "" : "\n");
        if (err == -1) {
            goto end;
        }
        offset = log_min(sizeof(temp) - 1, offset + (uint32_t)err);
    }

end:
    if (strings) {
        free(strings);
    }

    struct log* log = log_ptr();

    if (offset != 0) {
        log_store(temp, offset);
        log_set_pref(log_pref_freeze_on_next_open, 1);
    }

    if (sig == SIGSEGV) {
        sigaction(sig, &log->signal.segv, NULL);
    } else if (sig == SIGILL) {
        sigaction(sig, &log->signal.ill, NULL);
    } else if (sig == SIGABRT) {
        sigaction(sig, &log->signal.abrt, NULL);
    } else if (sig == SIGFPE) {
        sigaction(sig, &log->signal.fpe, NULL);
    }

    raise(sig);
}
#endif

int log_init(const char* path) {
    int result = -1;

    struct log* log = log_ptr();

    if (log->raw != NULL) {
        log_deinit();
    }

    memset(log, 0, sizeof(*log));

    log->size += log_line_buf_size;
    log->size += sizeof(struct log_header);

    bool needs_init = false;

    if (path && log_mmap) {
#if log_mmap
        log->fd = open(path, O_RDWR);
        if (log->fd == -1) {
            if (errno != ENOENT) {
                fprintf(stderr, "could not open log file at %s: %s\n", path, strerror(errno));
                goto end;
            }

            needs_init = true;
            log->fd = open(path, O_RDWR | O_CREAT, 0600);
            if (log->fd == -1) {
                fprintf(stderr, "could not create log file at %s: %s\n", path, strerror(errno));
                goto end;
            }

            int err = ftruncate(log->fd, (off_t)log->size);
            if (err == -1) {
                fprintf(stderr, "could not ftruncate log file: %s\n", strerror(errno));
                goto end;
            }
        }
        log->raw = mmap(NULL, log->size, PROT_READ | PROT_WRITE, MAP_SHARED, log->fd, 0);
#endif
    } else {
        log->fd = -1;
        log->raw = malloc(log->size);
        memset(log->raw, 0, log->size);
    }

    if (log->raw == NULL) {
        fprintf(stderr, "could not allocate memory for log file: %s\n", strerror(errno));
        goto end;
    }

    struct log_header* log_header = (void*)(log->raw + log_line_buf_size);
    uint64_t new_counter = 0;

    if (needs_init == false) {
        uint64_t counter = atomic_load_explicit(&log_header->counter, memory_order_relaxed);
        new_counter |= ((uint64_t)log_counter_run(counter) + 1) & ((1llu << log_run_bits) - 1llu);
        new_counter |= (uint64_t)log_counter_line(counter) << log_run_bits;
    }

    atomic_store_explicit(&log_header->counter, new_counter, memory_order_relaxed);

    log_apply_mode((enum log_mode)log_get_pref(log_pref_mode));

    if (log_get_pref(log_pref_freeze_on_next_open)) {
        log_set_pref(log_pref_freeze_on_next_open, 0);
        log_freeze();
    }

#if log_signal
    struct sigaction sa;
    memset(&sa, 0, sizeof(struct sigaction));
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = log_handle_signal;
    sigaction(SIGSEGV, &sa, &log->signal.segv);
    sigaction(SIGILL, &sa, &log->signal.ill);
    sigaction(SIGABRT, &sa, &log->signal.abrt);
    sigaction(SIGFPE, &sa, &log->signal.fpe);
#endif

    result = 0;

end:
    return result;
}

void log_deinit(void) {
    struct log* log = log_ptr();
    if (log->raw == NULL) return;

    log_apply_mode(log_mode_tool);

#if log_signal
    sigaction(SIGSEGV, &log->signal.segv, NULL);
    sigaction(SIGILL, &log->signal.ill, NULL);
    sigaction(SIGABRT, &log->signal.abrt, NULL);
    sigaction(SIGFPE, &log->signal.fpe, NULL);
#endif

    if (log->fd != -1) {
#if log_mmap
        munmap(log->raw, log->size);
        close(log->fd);
#endif
    } else {
        free(log->raw);
    }

    log->raw = NULL;
}

bool log_is_active(void) {
    return log_ptr()->raw != NULL;
}

void log_set_pref(uint32_t slot, uint16_t value) {
    struct log* log = log_ptr();
    if (log->raw == NULL) return;

    assert(slot < log_pref_count);

    struct log_header* header = (void*)(log->raw + log_line_buf_size);

    header->user.prefs[slot] = value;
}

uint16_t log_get_pref(uint32_t slot) {
    struct log* log = log_ptr();
    if (log->raw == NULL) return 0;

    assert(slot < log_pref_count);

    struct log_header* header = (void*)(log->raw + log_line_buf_size);

    return header->user.prefs[slot];
}

void log_clear(void) {
    struct log* log = log_ptr();
    if (log->raw == NULL) return;

    uint8_t barrier_index = atomic_load_explicit(&log->render.barrier_index, memory_order_relaxed);
    atomic_fetch_add_explicit(barrier_index ? &log->render.barrier_1 : &log->render.barrier_0, 1u, memory_order_relaxed);

    struct log_header* header = (void*)(log->raw + log_line_buf_size);
    atomic_store_explicit(&header->counter, 0, memory_order_relaxed);

    ((struct log_line*)log->raw + ((1u << log_line_bits) - 1u))->header.flags = 0;
    ((struct log_line*)log->raw)->header.flags = 0;

    atomic_fetch_sub_explicit(barrier_index ? &log->render.barrier_1 : &log->render.barrier_0, 1u, memory_order_release);
}

static void log_store(const char* data, uint32_t size) {
    struct log* log = log_ptr();

    uint32_t line_count = (size + log_line_msg_size - 1) / log_line_msg_size;
    assert(line_count < (1u << log_line_bits) - 1u);

    uint8_t barrier_index = atomic_load_explicit(&log->render.barrier_index, memory_order_relaxed);
    atomic_fetch_add_explicit(barrier_index ? &log->render.barrier_1 : &log->render.barrier_0, 1u, memory_order_relaxed);

    struct log_header* header = (void*)(log->raw + log_line_buf_size);
    uint64_t counter = atomic_fetch_add_explicit(&header->counter, (uint64_t)line_count << log_run_bits, memory_order_relaxed);

    uint16_t line = log_counter_line(counter);

    const char* src = data;
    const char* src_end = data + size;

    for (uint16_t line_index = 0; line_index < line_count; line_index++) {
        struct log_line* log_line = (struct log_line*)log->raw + line;

        log_line->header.flags = log_line_flag_msg;
        if (line_index == 0) {
            log_line->header.flags |= log_line_flag_msg_first;
        }
        if (line_index == line_count - 1) {
            log_line->header.flags |= log_line_flag_msg_last;
        }
        log_line->header.size = (uint16_t)log_min((unsigned long)(src_end - src), log_line_msg_size);
        memcpy(log_line->msg, src, log_line->header.size);

        line = (line + 1u) & ((1u << log_line_bits) - 1u);
        src += log_line_msg_size;
    }

    ((struct log_line*)log->raw + line)->header.flags = 0;

    atomic_fetch_sub_explicit(barrier_index ? &log->render.barrier_1 : &log->render.barrier_0, 1u, memory_order_release);
}

void log_s(const char* fmt, ...) {
    struct log* log = log_ptr();
    if (log->raw == NULL) return;

    char temp[1u << 12];

    va_list list;
    va_start(list, fmt);
    int req = vsnprintf(temp, sizeof(temp), fmt, list);
    va_end(list);

    if (req <= 0) {
        return;
    }

    log_store(temp, (uint32_t)req);
}

void log_str(const char* str, uint64_t size) {
    struct log* log = log_ptr();
    if (log->raw == NULL) return;

    assert(size <= UINT32_MAX);

    if (size == 0) {
        return;
    }

    log_store(str, (uint32_t)size);
}

#if log_capture
static void log_pipe_drain(struct log_pipe* p) {
    while (1) {
        assert(p->first + p->count < sizeof(p->temp));
        ssize_t read_count = read(p->fds[0], p->temp + p->first + p->count, sizeof(p->temp) - (p->first + p->count));
        if (read_count <= 0) {
            if (read_count == -1 && errno == EINTR) {
                continue;
            }
            break;
        }

        if (p->use_old) {
            char* ptr = p->temp + p->first + p->count;
            ssize_t size = read_count;

            while (1) {
                ssize_t write_count = write(p->fd_originals[0], ptr, (size_t)size);
                if (write_count <= 0) {
                    if (write_count == -1 && errno == EINTR) {
                        continue;
                    }
                    break;
                }
                ptr += write_count;
                size -= write_count;
                if (size == 0) {
                    break;
                }
            }
        }

        uint32_t new_start = p->first + p->count;
        uint32_t new_end = new_start + (uint32_t)read_count;

        for (uint32_t i = new_start; i != new_end; i++) {
            if (p->temp[i] == '\n') {
                log_store(p->temp + p->first, i - p->first);
                p->first = i + 1;
                p->count = 0;
            } else {
                p->count++;
            }
        }

        if (p->first + p->count == sizeof(p->temp)) {
            if (p->first == 0) {
                log_store(p->temp, sizeof(p->temp));
                p->count = 0;
            } else {
                memmove(p->temp, p->temp + p->first, p->count);
                p->first = 0;
            }
        }
    }
}

static void log_pipe_init(struct log_pipe* p) {
    memset(p, 0, sizeof(*p));
    p->is_init = true;
    p->fd_originals[0] = -1;
    p->fd_originals[1] = -1;
    p->fds[0] = -1;
    p->fds[1] = -1;

    p->fd_originals[0] = dup(STDOUT_FILENO);
    assert(p->fd_originals[0] != -1);

    p->fd_originals[1] = dup(STDERR_FILENO);
    assert(p->fd_originals[1] != -1);

    int err = pipe(p->fds);
    assert(err != -1);

    int flags = fcntl(p->fds[0], F_GETFL, 0);
    assert(flags != -1);
    err = fcntl(p->fds[0], F_SETFL, flags | O_NONBLOCK);
    assert(err != -1);

    flags = fcntl(p->fds[1], F_GETFL, 0);
    assert(flags != -1);
    err = fcntl(p->fds[1], F_SETFL, flags | O_NONBLOCK);
    assert(err != -1);

    err = dup2(p->fds[1], STDOUT_FILENO);
    assert(err != -1);

    err = dup2(p->fds[1], STDERR_FILENO);
    assert(err != -1);

    close(p->fds[1]);
}

static void log_pipe_deinit(struct log_pipe* p) {
    int err = dup2(p->fd_originals[0], STDOUT_FILENO);
    assert(err != -1);

    err = dup2(p->fd_originals[1], STDERR_FILENO);
    assert(err != -1);

    close(p->fd_originals[0]);
    close(p->fd_originals[1]);
    close(p->fds[0]);

    memset(p, 0, sizeof(*p));
}
#endif

void log_apply_mode(enum log_mode mode) {
    struct log* log = log_ptr();
    if (log->raw == NULL) return;

#if log_capture
    if (mode == log_mode_tool) {
        if (log->pipe.is_init) {
            fflush(stdout);
            log_pipe_drain(&log->pipe);
            log_pipe_deinit(&log->pipe);
        }
    } else if (mode == log_mode_term || mode == log_mode_both) {
        if (log->pipe.is_init == false) {
            fflush(stdout);
            log_pipe_init(&log->pipe);
        }
        log->pipe.use_old = mode == log_mode_both;
    }
#endif
}

void log_dump(void) {
    struct log* log = log_ptr();
    if (log->raw == NULL) return;

#if log_capture
    if (log->pipe.is_init) {
        fflush(stdout);
        log_pipe_drain(&log->pipe);
    }
#endif
}

static void log_wait_at_barrier(uint16_t* out_line) {
    struct log* log = log_ptr();

    struct log_header* header = (void*)(log->raw + log_line_buf_size);
    uint64_t counter = atomic_load_explicit(&header->counter, memory_order_relaxed);

    uint8_t barrier_index = atomic_fetch_xor_explicit(&log->render.barrier_index, 1u, memory_order_release);
    atomic_uint_fast32_t* barrier = barrier_index ? &log->render.barrier_1 : &log->render.barrier_0;

    while (atomic_load_explicit(barrier, memory_order_acquire) != 0) {}

    *out_line = log_counter_line(counter);
}

static void log_freeze(void) {
    struct log* log = log_ptr();

    if (log->view.is_frozen == false) {
        log->view.is_frozen = true;
        log_wait_at_barrier(&log->view.frozen_line);
        memcpy(log->view.frozen_log_line_buf, log->raw, log_line_buf_size);
    }
}

void log_resume(void) {
    struct log* log = log_ptr();
    if (log->raw == NULL) return;

    log->view.is_frozen = false;
}

bool log_is_paused(void) {
    struct log* log = log_ptr();

    return log->raw != NULL && log->view.is_frozen;
}

static uint32_t log_iterate_letters(const struct log_line* log_lines, uint32_t char_view_width, uint16_t line_first, uint16_t line_last, bool (*func)(uint16_t line, uint32_t u, uint32_t v, uint32_t cp, void* args), void* args) {
    char str[4] = { 0 };
    uint8_t str_offset = 0;
    bool emitted = false;

    uint16_t line = line_first;
    uint32_t u = 0;
    uint32_t v = 0;

    while (1) {
        const struct log_line* log_line = log_lines + line;

        if (log_line->header.flags == 0) {
            goto end;
        }

        bool wants_new_row = (log_line->header.flags & log_line_flag_msg_first) && line != line_first;

        if (log_line->header.flags & log_line_flag_msg_first) {
            str_offset = 0;
        }

        const char* msg = log_line->msg;
        const char* msg_end = msg + log_line->header.size;

        for (; msg != msg_end; msg++) {
            if (str_offset == sizeof(str)) {
                str_offset = 0;
            }

            str[str_offset++] = *msg;

            uint32_t cp = UINT32_MAX;
            uint32_t head = (uint32_t)str[0];

            if (head < 0x80) {
                cp = head;
            } else if ((head & 0xe0) == 0xc0) {
                if (str_offset == 2) {
                    cp = (head & 0x1f) << 6;
                    cp |= (uint32_t)str[1] & 0x3f;
                }
            } else if ((head & 0xf0) == 0xe0) {
                if (str_offset == 3) {
                    cp = (head & 0x0f) << 12;
                    cp |= ((uint32_t)str[1] & 0x3f) << 6;
                    cp |= (uint32_t)str[2] & 0x3f;
                }
            } else if ((head & 0xf8) == 0xf0) {
                if (str_offset == 4) {
                    cp = (head & 0x07) << 18;
                    cp |= ((uint32_t)str[1] & 0x3f) << 12;
                    cp |= ((uint32_t)str[2] & 0x3f) << 6;
                    cp |= (uint32_t)str[3] & 0x3f;
                }
            }

            if (cp != UINT32_MAX) {
                str_offset = 0;

                bool is_overflow = wants_new_row == false && u == char_view_width;
                uint32_t next_u = u;
                uint32_t next_v = v;

                if ((wants_new_row && emitted) || is_overflow) {
                    next_u = 0;
                    next_v++;
                }
                wants_new_row = false;
                emitted = true;

                if (func && func(line, next_u, next_v, cp, args) == false) {
                    goto end;
                }

                u = next_u;
                v = next_v;

                if (cp == '\n') {
                    if (is_overflow == false) {
                        u = 0;
                        v++;
                    }
                } else {
                    u++;
                }
            }
        }

        if (line == line_last) {
            goto end;
        } else {
            line = (line + 1u) & ((1u << log_line_bits) - 1u);
        }
    }

end:
    return emitted ? v + 1 : 0;
}

static const struct log_line* log_view_lines(uint16_t* out_ref_line) {
    struct log* log = log_ptr();

    if (log->view.is_frozen) {
        *out_ref_line = log->view.frozen_line;
        return (const struct log_line*)log->view.frozen_log_line_buf;
    }

    log_wait_at_barrier(out_ref_line);
    return (const struct log_line*)log->raw;
}

static uint16_t log_view_first_line(const struct log_line* lines, uint16_t ref_line) {
    uint16_t first = ref_line;

    while (1) {
        uint16_t prev = (first - 1u) & ((1u << log_line_bits) - 1u);
        if (prev == ref_line || lines[prev].header.flags == 0) {
            break;
        }
        first = prev;
    }

    return first;
}

uint32_t log_view_row_count(uint32_t view_width_chars) {
    struct log* log = log_ptr();
    if (log->raw == NULL) return 0;

    assert(view_width_chars > 0);

    uint16_t ref_line;
    const struct log_line* lines = log_view_lines(&ref_line);
    uint16_t first = log_view_first_line(lines, ref_line);
    uint16_t last = (ref_line - 1u) & ((1u << log_line_bits) - 1u);

    return log_iterate_letters(lines, view_width_chars, first, last, NULL, NULL);
}

struct log_view_glyphs_ctx {
    int32_t base_row;
    int32_t row_start;
    int32_t row_end;
    void (*func)(int32_t row, uint32_t col, uint32_t cp, void* user);
    void* user;
};

static bool log_view_glyphs_func(uint16_t line, uint32_t u, uint32_t v, uint32_t cp, void* args) {
    (void)line;
    struct log_view_glyphs_ctx* ctx = args;

    int32_t row = ctx->base_row + (int32_t)v;

    if (row >= ctx->row_end) {
        return false;
    }

    if (row >= ctx->row_start) {
        ctx->func(row, u, cp, ctx->user);
    }

    return true;
}

static uint16_t log_view_walk_back(const struct log_line* lines, uint32_t view_width_chars, uint16_t ref_line, uint32_t rows_needed, uint32_t* out_rows) {
    uint16_t line = ref_line;
    uint32_t rows = 0;

    while (rows < rows_needed) {
        uint16_t line_last = (line - 1u) & ((1u << log_line_bits) - 1u);

        if (lines[line_last].header.flags == 0) {
            break;
        }

        uint16_t line_first = line_last;

        while ((lines[line_first].header.flags & log_line_flag_msg_first) == 0) {
            uint16_t prev = (line_first - 1u) & ((1u << log_line_bits) - 1u);
            if (prev == ref_line || lines[prev].header.flags == 0) {
                break;
            }
            line_first = prev;
        }

        rows += log_iterate_letters(lines, view_width_chars, line_first, line_last, NULL, NULL);
        line = line_first;
    }

    *out_rows = rows;
    return line;
}

void log_view_glyphs(uint32_t view_width_chars, int32_t row_start, int32_t row_end, void (*func)(int32_t row, uint32_t col, uint32_t cp, void* user), void* user) {
    struct log* log = log_ptr();
    if (log->raw == NULL) return;

    assert(view_width_chars > 0);
    assert(func != NULL);

    if (row_start >= 0 || row_start >= row_end) {
        return;
    }

    uint16_t ref_line;
    const struct log_line* lines = log_view_lines(&ref_line);

    uint32_t rows_back;
    uint16_t first = log_view_walk_back(lines, view_width_chars, ref_line, (uint32_t)-row_start, &rows_back);

    if (rows_back == 0) {
        return;
    }

    struct log_view_glyphs_ctx ctx = {
        .base_row = -(int32_t)rows_back,
        .row_start = row_start,
        .row_end = row_end,
        .func = func,
        .user = user,
    };

    uint16_t last = (ref_line - 1u) & ((1u << log_line_bits) - 1u);

    log_iterate_letters(lines, view_width_chars, first, last, log_view_glyphs_func, &ctx);
}

void log_pause(void) {
    struct log* log = log_ptr();
    if (log->raw == NULL) return;

    if (log->view.is_frozen == false) {
        log_dump();
        log_freeze();
    }
}

void log_set_mode(enum log_mode mode) {
    log_apply_mode(mode);
    log_set_pref(log_pref_mode, (uint16_t)mode);
}
