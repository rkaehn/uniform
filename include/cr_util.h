#pragma once

enum cr_util_file_type {
    cr_util_file_type_regular,
    cr_util_file_type_directory,
    cr_util_file_type_other,
};

struct cr_sha256 {
    _Alignas(16) char _[128];
};

struct cr_util_dir_iter {
    struct cr_mem* mem;
    struct cr_str* base_path;
    struct {
        void* dir;
        struct cr_str* relative_path;
    } stack[16];
    uint32_t depth;
    struct cr_str* relative_path;
    struct cr_str* full_path;
    enum cr_util_file_type file_type;
    uint64_t file_size;
    bool skip;
};

struct cr_str* cr_util_get_working_dir(struct cr_mem* mem);
struct cr_str* cr_util_get_documents_dir(struct cr_mem* mem);
struct cr_str* cr_util_get_executable_path(struct cr_mem* mem);
struct cr_str* cr_util_get_resources_dir(struct cr_mem* mem);
struct cr_str* cr_util_get_real_path(struct cr_mem* mem, struct cr_str* path);

struct cr_util_dir_iter cr_util_dir_iter(struct cr_mem* mem, struct cr_str* path);
bool cr_util_dir_iter_next(struct cr_util_dir_iter* inout_iter);
void cr_util_dir_iter_skip(struct cr_util_dir_iter* inout_iter);
bool cr_util_create_dir(struct cr_str* path);
bool cr_util_copy(struct cr_mem* mem, struct cr_str* src_path, struct cr_str* dst_path);
bool cr_util_remove(struct cr_str* path);
bool cr_util_set_executable(struct cr_str* path);
bool cr_util_get_file_type(struct cr_str* path, enum cr_util_file_type* out_file_type);
bool cr_util_does_file_exist(struct cr_str* path);

int cr_atoi(const char* str_data, uint32_t str_size, long* out_number);
double cr_get_current_time(void);
uint32_t cr_get_hardware_concurrency(void);

#if cr_wasm
cr_js_import(cr_js_fetch) void cr_fetch(const char* url, void (*func)(void* user, void* data, uint32_t size), void* user);
#endif

void cr_sha256_init(struct cr_sha256* sha256);
void cr_sha256_update(struct cr_sha256* sha256, const void* data, uint64_t size);
void cr_sha256_final(struct cr_sha256* sha256, void* out_result);

int cr_exec(struct cr_str** arguments, uint32_t argument_count, struct cr_str* standard_output, struct cr_str* standard_error);
