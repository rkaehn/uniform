#include "cr.h"
#if cr_ios || cr_macos
#include <CommonCrypto/CommonCrypto.h>
#include <mach-o/dyld.h>
#endif
#if !cr_wasm
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#endif
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <ftw.h>
#include <poll.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

_Thread_local cr_check_handler_t* cr_check_handler = NULL;

#if cr_wasm
cr_js_import(cr_js_check_fail) void cr_js_check_fail(const char* message);
#endif

void cr_check_fail(const char* func, const char* file, int line, const char* fmt, ...) {
    static _Thread_local char message[1024];
    char text[768];
    va_list args;
    va_start(args, fmt);
    vsnprintf(text, sizeof(text), fmt, args);
    va_end(args);
    const char* base = strrchr(file, '/');
    snprintf(message, sizeof(message), "%s (%s, %s:%d)", text, func, base != NULL ? base + 1 : file, line);
    if (cr_check_handler != NULL) {
        cr_check_handler(message);
    }
#if cr_wasm
    cr_js_check_fail(message);
#endif
    fprintf(stderr, "check failed: %s\n", message);
    abort();
}

struct cr_str* cr_util_get_working_dir(struct cr_mem* mem) {
    struct cr_str* result = NULL;
    char* cwd = getcwd(NULL, 0);
    if (cwd) {
        result = cr_str_create(mem, cwd);
        free(cwd);
    }
    return result;
}

struct cr_str* cr_util_get_documents_dir(struct cr_mem* mem) {
    const char* home = getenv("HOME");
    return home ? cr_str_create_format(mem, "%s/Documents", home) : NULL;
}

#if !cr_wasm
struct cr_str* cr_util_get_real_path(struct cr_mem* mem, struct cr_str* path) {
    char* real = realpath(cr_str_get_data(path), NULL);
    struct cr_str* result = real ? cr_str_create(mem, real) : NULL;
    free(real);
    return result;
}

struct cr_str* cr_util_get_executable_path(struct cr_mem* mem) {
    char path[PATH_MAX];
#if cr_ios || cr_macos
    uint32_t size = sizeof(path);
    if (_NSGetExecutablePath(path, &size) != 0) {
        return NULL;
    }
#else
    ssize_t count = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (count == -1) {
        return NULL;
    }
    path[count] = 0;
#endif
    uint8_t raw[cr_str_size];
    return cr_util_get_real_path(mem, cr_str_temp(raw, path));
}
#endif

#if cr_macos
struct cr_str* cr_util_get_resources_dir(struct cr_mem* mem) {
    struct cr_str* dir = cr_util_get_executable_path(mem);
    if (dir == NULL) {
        return NULL;
    }
    cr_str_remove_last_path_component(dir);
    const char* suffix = "/Contents/MacOS";
    uint32_t count = cr_str_get_count(dir);
    uint32_t suffix_count = (uint32_t)strlen(suffix);
    if (count <= suffix_count || strcmp(cr_str_get_data(dir) + count - suffix_count, suffix) != 0) {
        cr_str_destroy(dir);
        return NULL;
    }
    cr_str_remove_last_path_component(dir);
    cr_str_append_path_component_cstr(dir, "Resources");
    return dir;
}
#elif cr_ios
struct cr_str* cr_util_get_resources_dir(struct cr_mem* mem) {
    struct cr_str* dir = cr_util_get_executable_path(mem);
    if (dir != NULL) {
        cr_str_remove_last_path_component(dir);
    }
    return dir;
}
#else
struct cr_str* cr_util_get_resources_dir(struct cr_mem* mem) {
    return NULL;
}
#endif

static bool cr_util_stat(const char* path, enum cr_util_file_type* out_file_type, uint64_t* out_file_size) {
    struct stat st;
    if (stat(path, &st) == -1) {
        return false;
    }
    *out_file_type = S_ISDIR(st.st_mode) ? cr_util_file_type_directory : S_ISREG(st.st_mode) ? cr_util_file_type_regular : cr_util_file_type_other;
    *out_file_size = (uint64_t)st.st_size;
    return true;
}

struct cr_util_dir_iter cr_util_dir_iter(struct cr_mem* mem, struct cr_str* path) {
    struct cr_util_dir_iter iter = {
        .mem = mem,
        .base_path = path,
        .stack[0].relative_path = cr_str_create_empty(mem),
    };
    return iter;
}

bool cr_util_dir_iter_next(struct cr_util_dir_iter* inout_iter) {
    if (inout_iter->relative_path) {
        if (inout_iter->file_type == cr_util_file_type_directory && inout_iter->skip == false) {
            inout_iter->depth++;
            assert(inout_iter->depth < cr_count(inout_iter->stack));
            inout_iter->stack[inout_iter->depth].dir = NULL;
            inout_iter->stack[inout_iter->depth].relative_path = inout_iter->relative_path;
        } else {
            cr_str_destroy(inout_iter->relative_path);
        }
        cr_str_destroy(inout_iter->full_path);
        inout_iter->relative_path = NULL;
        inout_iter->full_path = NULL;
        inout_iter->skip = false;
    }

    while (1) {
        __typeof(inout_iter->stack[0])* ptr = &inout_iter->stack[inout_iter->depth];

        if (ptr->dir == NULL) {
            struct cr_str* path = cr_str_create_copy(inout_iter->mem, inout_iter->base_path);
            cr_str_append_path_component(path, ptr->relative_path);
            ptr->dir = opendir(cr_str_get_data(path));
            cr_str_destroy(path);
        }

        struct dirent* entry = ptr->dir ? readdir(ptr->dir) : NULL;

        if (entry == NULL) {
            if (ptr->dir) {
                closedir(ptr->dir);
            }
            cr_str_destroy(ptr->relative_path);

            if (inout_iter->depth == 0) {
                return false;
            }
            inout_iter->depth--;
            continue;
        }

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        struct cr_str* relative_path = cr_str_create_copy(inout_iter->mem, ptr->relative_path);
        cr_str_append_path_component_cstr(relative_path, entry->d_name);

        struct cr_str* full_path = cr_str_create_copy(inout_iter->mem, inout_iter->base_path);
        cr_str_append_path_component(full_path, relative_path);

        if (!cr_util_stat(cr_str_get_data(full_path), &inout_iter->file_type, &inout_iter->file_size)) {
            inout_iter->file_type = cr_util_file_type_other;
            inout_iter->file_size = 0;
        }
        inout_iter->relative_path = relative_path;
        inout_iter->full_path = full_path;
        return true;
    }
}

void cr_util_dir_iter_skip(struct cr_util_dir_iter* inout_iter) {
    inout_iter->skip = true;
}

bool cr_util_create_dir(struct cr_str* path) {
    bool success = false;

    char* str_start = cr_str_get_raw(path);
    char* str = str_start + 1;

    while (1) {
        char current = *str;

        if (current == cr_path_div || current == 0) {
            *str = 0;
            int err = mkdir(str_start, 0777);
            *str = current;

            if (err != 0 && errno != EEXIST) {
                goto end;
            }

            if (current == 0) {
                break;
            }
        }
       
        str++;
    }

    success = true;

end:
    if (!success) {
        fprintf(stderr, "cr_util_create_dir(%s)\n", cr_str_get_data(path));
    }
    return success;
}

static bool cr_util_copy_dir(struct cr_str* src_path, struct cr_str* dst_path) {
    bool success = false;

    struct stat src_stat;
    int err = stat(cr_str_get_data(src_path), &src_stat);
    if (err == -1) {
        goto end;
    }

    if (!S_ISDIR(src_stat.st_mode)) {
        goto end;
    }

    err = mkdir(cr_str_get_data(dst_path), 0600);
    if (err == -1 && errno != EEXIST) {
        fprintf(stderr, "mkdir(%s): %s\n", cr_str_get_data(dst_path), strerror(errno));
        goto end;
    }

    err = chmod(cr_str_get_data(dst_path), src_stat.st_mode);
    if (err == -1) {
        fprintf(stderr, "chmod(%s): %s\n", cr_str_get_data(dst_path), strerror(errno));
        goto end;
    }

    success = true;

end:
    return success;
}

static bool cr_util_copy_file(struct cr_str* src_path, struct cr_str* dst_path, void* buf, uint32_t buf_size) {
    bool success = false;
    int src_fd = -1;
    int dst_fd = -1;

    src_fd = open(cr_str_get_data(src_path), O_RDONLY);
    if (src_fd == -1) {
        fprintf(stderr, "open(%s): %s\n", cr_str_get_data(src_path), strerror(errno));
        goto end;
    }

    struct stat src_stat;
    int err = fstat(src_fd, &src_stat);
    if (err == -1) {
        goto end;
    }

    if (!S_ISREG(src_stat.st_mode)) {
        goto end;
    }

    dst_fd = open(cr_str_get_data(dst_path), O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (dst_fd == -1) {
        fprintf(stderr, "open(%s): %s\n", cr_str_get_data(dst_path), strerror(errno));
        goto end;
    }

    while (1) {
        ssize_t read_count = read(src_fd, buf, buf_size);
        if (read_count == -1) {
            if (errno == EINTR) {
                continue;
            }
            goto end;
        }
        if (read_count == 0) {
            break;
        }
        char* ptr = buf;
        ssize_t size = read_count;
        while (1) {
            ssize_t write_count = write(dst_fd, ptr, (size_t)size);
            if (write_count <= 0) {
                if (write_count == -1 && errno == EINTR) {
                    continue;
                }
                goto end;
            }
            ptr += write_count;
            size -= write_count;
            if (size == 0) {
                break;
            }
        }
    }

    err = fchmod(dst_fd, src_stat.st_mode);
    if (err == -1) {
        goto end;
    }

    success = true;

end:
    if (src_fd != -1) {
        close(src_fd);
    }
    if (dst_fd != -1) {
        close(dst_fd);
    }
    return success;
}

bool cr_util_copy(struct cr_mem* mem, struct cr_str* src_path, struct cr_str* dst_path) {
    bool success = false;
    uint32_t buf_size = 1lu << 14;
    void* buf = cr_malloc(mem, buf_size, 0, cr_loc);
    struct cr_str* full_dst_path = NULL;

    enum cr_util_file_type src_file_type;
    if (!cr_util_get_file_type(src_path, &src_file_type)) {
        goto end;
    }

    if (src_file_type == cr_util_file_type_directory) {
        if (!cr_util_copy_dir(src_path, dst_path)) {
            goto end;
        }

        struct cr_util_dir_iter iter = cr_util_dir_iter(mem, src_path);

        while (cr_util_dir_iter_next(&iter)) {
            full_dst_path = cr_str_create_copy(mem, dst_path);
            cr_str_append_path_component(full_dst_path, iter.relative_path);

            switch (iter.file_type) {
            case cr_util_file_type_directory: {
                if (!cr_util_copy_dir(iter.full_path, full_dst_path)) {
                    goto end;
                }
            } break;
            case cr_util_file_type_regular: {
                if (!cr_util_copy_file(iter.full_path, full_dst_path, buf, buf_size)) {
                    goto end;
                }
            } break;
            default:
                goto end;
            }

            cr_str_destroy(full_dst_path);
            full_dst_path = NULL;
        }
    } else if (src_file_type == cr_util_file_type_regular) {
        full_dst_path = cr_str_create_copy(mem, dst_path);

        enum cr_util_file_type dst_file_type;
        if (cr_util_get_file_type(dst_path, &dst_file_type) && dst_file_type == cr_util_file_type_directory) {
            struct cr_str* comp = cr_str_get_last_path_component(mem, src_path);
            cr_str_append_path_component(full_dst_path, comp);
            cr_str_destroy(comp);
        }

        if (!cr_util_copy_file(src_path, full_dst_path, buf, buf_size)) {
            goto end;
        }
    } else {
        goto end;
    }

    success = true;

end:
    cr_free(mem, buf, cr_loc);
    if (full_dst_path) {
        cr_str_destroy(full_dst_path);
    }
    if (!success) {
        fprintf(stderr, "cr_util_copy(%s, %s)\n", cr_str_get_data(src_path), cr_str_get_data(dst_path));
    }
    return success;
}

static int cr_util_remove_func(const char* path, const struct stat* stat, int flag, struct FTW* ftw) {
    (void)stat;
    (void)flag;
    (void)ftw;
    return remove(path);
}

bool cr_util_remove(struct cr_str* path) {
    int err = remove(cr_str_get_data(path));
    if (err == -1 && (errno == ENOTEMPTY || errno == EEXIST)) {
        err = nftw(cr_str_get_data(path), cr_util_remove_func, 64, FTW_DEPTH | FTW_PHYS);
    }
    if (err == -1 && errno != ENOENT) {
        fprintf(stderr, "cr_util_remove(%s): %s\n", cr_str_get_data(path), strerror(errno));
        return false;
    }
    return true;
}

bool cr_util_set_executable(struct cr_str* path) {
    return chmod(cr_str_get_data(path), 0755) == 0;
}

bool cr_util_get_file_type(struct cr_str* path, enum cr_util_file_type* out_file_type) {
    uint64_t file_size;
    return cr_util_stat(cr_str_get_data(path), out_file_type, &file_size);
}

bool cr_util_does_file_exist(struct cr_str* path) {
    struct stat src_stat;
    int err = stat(cr_str_get_data(path), &src_stat);
    return err == 0;
}

int cr_atoi(const char* str_data, uint32_t str_size, long* out_number) {
    char temp[32];
    if (str_size >= sizeof(temp)) {
        return -1;
    }
    memcpy(temp, str_data, str_size);
    temp[str_size] = 0;
    *out_number = atoi(temp);
    return 0;
}

#if cr_wasm
cr_js_import(cr_js_now) double cr_js_now(void);
cr_js_import(cr_js_thread_count) uint32_t cr_js_thread_count(void);
#endif

double cr_get_current_time(void) {
#if cr_wasm
    return cr_js_now() / 1000.0;
#else
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC_RAW, &t);
    return (double)t.tv_sec + (double)t.tv_nsec * 1e-9;
#endif
}

uint32_t cr_get_hardware_concurrency(void) {
#if cr_wasm
    return cr_js_thread_count();
#else
    long count = sysconf(_SC_NPROCESSORS_ONLN);
    return count > 0 ? (uint32_t)count : 1u;
#endif
}

#if cr_ios || cr_macos
static_assert(sizeof(CC_SHA256_CTX) <= sizeof(((struct cr_sha256*)NULL)->_), "");

void cr_sha256_init(struct cr_sha256* sha256) {
    CC_SHA256_CTX* ctx = (CC_SHA256_CTX*)sha256->_;
    CC_SHA256_Init(ctx);
}

void cr_sha256_update(struct cr_sha256* sha256, const void* data, uint64_t size) {
    CC_SHA256_CTX* ctx = (CC_SHA256_CTX*)sha256->_;
    CC_SHA256_Update(ctx, data, (CC_LONG)size);
}

void cr_sha256_final(struct cr_sha256* sha256, void* out_result) {
    CC_SHA256_CTX* ctx = (CC_SHA256_CTX*)sha256->_;
    CC_SHA256_Final(out_result, ctx);
}
#else
struct cr_sha256_ctx {
    uint8_t data[64];
    uint64_t datalen;
    uint64_t bitlen;
    uint32_t state[8];
};
static_assert(sizeof(struct cr_sha256_ctx) <= sizeof(((struct cr_sha256*)NULL)->_), "");

#define ROTR(a,b) (((a) >> (b)) | ((a) << (32-(b))))
#define CH(x,y,z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x,y,z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0(x) (ROTR(x,2) ^ ROTR(x,13) ^ ROTR(x,22))
#define EP1(x) (ROTR(x,6) ^ ROTR(x,11) ^ ROTR(x,25))
#define SIG0(x) (ROTR(x,7) ^ ROTR(x,18) ^ ((x) >> 3))
#define SIG1(x) (ROTR(x,17) ^ ROTR(x,19) ^ ((x) >> 10))

static const uint32_t k[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

static void cr_sha256_transform(struct cr_sha256_ctx* ctx, const uint8_t data[]) {
    uint32_t a, b, c, d, e, f, g, h, i, j, t1, t2, m[64];

    for (i = 0, j = 0; i < 16; i++, j += 4) {
        m[i] = ((uint32_t)data[j] << 24) | ((uint32_t)data[j + 1] << 16) | ((uint32_t)data[j + 2] << 8) | ((uint32_t)data[j + 3]);
    }
    for (; i < 64; i++) {
        m[i] = SIG1(m[i - 2]) + m[i - 7] + SIG0(m[i - 15]) + m[i - 16];
    }

    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];
    e = ctx->state[4];
    f = ctx->state[5];
    g = ctx->state[6];
    h = ctx->state[7];

    for (i = 0; i < 64; i++) {
        t1 = h + EP1(e) + CH(e,f,g) + k[i] + m[i];
        t2 = EP0(a) + MAJ(a,b,c);
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }

    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
}

void cr_sha256_init(struct cr_sha256* sha256) {
    struct cr_sha256_ctx* ctx = (struct cr_sha256_ctx*)&sha256->_;
    ctx->datalen = 0;
    ctx->bitlen = 0;
    ctx->state[0] = 0x6a09e667;
    ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372;
    ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f;
    ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab;
    ctx->state[7] = 0x5be0cd19;
}

void cr_sha256_update(struct cr_sha256* sha256, const void* data, uint64_t size) {
    struct cr_sha256_ctx* ctx = (struct cr_sha256_ctx*)&sha256->_;
    const uint8_t* bytes = data;

    for (uint64_t i = 0; i < size; i++) {
        ctx->data[ctx->datalen] = bytes[i];
        ctx->datalen++;
        if (ctx->datalen == 64) {
            cr_sha256_transform(ctx, ctx->data);
            ctx->bitlen += 512;
            ctx->datalen = 0;
        }
    }
}

void cr_sha256_final(struct cr_sha256* sha256, void* out_result) {
    struct cr_sha256_ctx* ctx = (struct cr_sha256_ctx*)&sha256->_;
    uint8_t* hash = out_result;
    uint64_t i = ctx->datalen;

    if (ctx->datalen < 56) {
        ctx->data[i++] = 0x80;
        while (i < 56) {
            ctx->data[i++] = 0x00;
        }
    } else {
        ctx->data[i++] = 0x80;
        while (i < 64) {
            ctx->data[i++] = 0x00;
        }
        cr_sha256_transform(ctx, ctx->data);
        memset(ctx->data, 0, 56);
    }

    ctx->bitlen += ctx->datalen * 8;
    ctx->data[63] = (uint8_t)(ctx->bitlen);
    ctx->data[62] = (uint8_t)(ctx->bitlen >> 8);
    ctx->data[61] = (uint8_t)(ctx->bitlen >> 16);
    ctx->data[60] = (uint8_t)(ctx->bitlen >> 24);
    ctx->data[59] = (uint8_t)(ctx->bitlen >> 32);
    ctx->data[58] = (uint8_t)(ctx->bitlen >> 40);
    ctx->data[57] = (uint8_t)(ctx->bitlen >> 48);
    ctx->data[56] = (uint8_t)(ctx->bitlen >> 56);
    cr_sha256_transform(ctx, ctx->data);

    for (i = 0; i < 4; i++) {
        hash[i] = (ctx->state[0] >> (24 - i * 8)) & 0xff;
        hash[i + 4] = (ctx->state[1] >> (24 - i * 8)) & 0xff;
        hash[i + 8] = (ctx->state[2] >> (24 - i * 8)) & 0xff;
        hash[i + 12] = (ctx->state[3] >> (24 - i * 8)) & 0xff;
        hash[i + 16] = (ctx->state[4] >> (24 - i * 8)) & 0xff;
        hash[i + 20] = (ctx->state[5] >> (24 - i * 8)) & 0xff;
        hash[i + 24] = (ctx->state[6] >> (24 - i * 8)) & 0xff;
        hash[i + 28] = (ctx->state[7] >> (24 - i * 8)) & 0xff;
    }
}
#endif

#if !cr_wasm
int cr_exec(struct cr_str** arguments, uint32_t argument_count, struct cr_str* standard_output, struct cr_str* standard_error) {
    assert(argument_count > 0);

    bool has_output = standard_output != NULL;
    bool has_error = standard_error != NULL;

    int output_fd[2] = { -1, -1 };
    int error_fd[2] = { -1, -1 };
    int err;

    if (has_output) {
#if cr_ios || cr_macos
        err = pipe(output_fd);
#else
        err = pipe2(output_fd, O_CLOEXEC);
#endif
        assert(err != -1);
    }
    if (has_error) {
#if cr_ios || cr_macos
        err = pipe(error_fd);
#else
        err = pipe2(error_fd, O_CLOEXEC);
#endif
        assert(err != -1);
    }

    char** cmd_args = malloc((argument_count + 1) * sizeof(char*));
    for (size_t i = 0; i < argument_count; i++) {
        cmd_args[i] = (char*)cr_str_get_data(arguments[i]);
    }
    cmd_args[argument_count] = NULL;

    posix_spawnattr_t attr;
    posix_spawnattr_init(&attr);
    sigset_t default_signals;
    sigemptyset(&default_signals);
    sigaddset(&default_signals, SIGINT);
    sigaddset(&default_signals, SIGQUIT);
    posix_spawnattr_setsigdefault(&attr, &default_signals);
#if cr_ios || cr_macos
    posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETSIGDEF | POSIX_SPAWN_CLOEXEC_DEFAULT);
#else
    posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETSIGDEF);
#endif

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);

#if cr_ios || cr_macos
    for (int fd = STDIN_FILENO; fd <= STDERR_FILENO; fd++) {
        if (fcntl(fd, F_GETFD) != -1) {
            posix_spawn_file_actions_addinherit_np(&actions, fd);
        }
    }
#endif

    if (has_output) {
        posix_spawn_file_actions_adddup2(&actions, output_fd[1], STDOUT_FILENO);
        posix_spawn_file_actions_addclose(&actions, output_fd[0]);
        posix_spawn_file_actions_addclose(&actions, output_fd[1]);
    }
    if (has_error) {
        posix_spawn_file_actions_adddup2(&actions, error_fd[1], STDERR_FILENO);
        posix_spawn_file_actions_addclose(&actions, error_fd[0]);
        posix_spawn_file_actions_addclose(&actions, error_fd[1]);
    }

    extern char** environ;

    pid_t pid;
    err = posix_spawnp(&pid, cmd_args[0], &actions, &attr, cmd_args, environ);

    posix_spawn_file_actions_destroy(&actions);
    posix_spawnattr_destroy(&attr);
    free(cmd_args);

    if (err != 0) {
        if (has_output) {
            close(output_fd[0]);
            close(output_fd[1]);
        }
        if (has_error) {
            close(error_fd[0]);
            close(error_fd[1]);
        }
        return -1;
    }

    if (has_output) close(output_fd[1]);
    if (has_error) close(error_fd[1]);

    struct pollfd fds[2];
    fds[0].fd = has_output ? output_fd[0] : -1;
    fds[0].events = POLLIN;
    fds[1].fd = has_error ? error_fd[0] : -1;
    fds[1].events = POLLIN;

    char buf[4096];

    while ((fds[0].fd != -1) || (fds[1].fd != -1)) {
        err = poll(fds, 2, -1);
        if (err == -1 && errno == EINTR) continue;
        assert(err != -1);

        if (fds[0].fd != -1 && (fds[0].revents & (POLLIN | POLLHUP))) {
            ssize_t count = read(fds[0].fd, buf, sizeof(buf));
            if (count > 0) {
                cr_str_append_data(standard_output, buf, (uint32_t)count);
            } else {
                close(fds[0].fd);
                fds[0].fd = -1;
            }
        }

        if (fds[1].fd != -1 && (fds[1].revents & (POLLIN | POLLHUP))) {
            ssize_t count = read(fds[1].fd, buf, sizeof(buf));
            if (count > 0) {
                cr_str_append_data(standard_error, buf, (uint32_t)count);
            } else {
                close(fds[1].fd);
                fds[1].fd = -1;
            }
        }
    }

    int status;
    while (waitpid(pid, &status, 0) == -1 && errno == EINTR);

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return -1;
}
#endif
