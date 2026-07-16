#pragma once

#if cr_wasm
#define cr_str_size 16
#else
#define cr_str_size 24
#endif

enum cr_file_type {
    cr_file_type_none,
    cr_file_type_ds_store,
    cr_file_type_c_source,
    cr_file_type_c_header,
    cr_file_type_cpp_source,
    cr_file_type_cpp_header,
    cr_file_type_objc_source,
    cr_file_type_o,
    cr_file_type_a,
    cr_file_type_metal,
    cr_file_type_air,
    cr_file_type_metallib,
    cr_file_type_hlsl,
    cr_file_type_wgsl,
    cr_file_type_plist,
    cr_file_type_txt,
    cr_file_type_html,
    cr_file_type_js,
    cr_file_type_wasm,
    cr_file_type_br,
    cr_file_type_swift,
    cr_file_type_swiftmodule,
    cr_file_type_other,
};

struct cr_str;

struct cr_str* cr_str_temp(uint8_t* raw, char* cstr);

struct cr_str* cr_str_create(struct cr_mem* mem, const char* cstr);
__attribute__((format(printf, 2, 3)))
struct cr_str* cr_str_create_format(struct cr_mem* mem, const char* fmt, ...);
struct cr_str* cr_str_create_empty(struct cr_mem* mem);
struct cr_str* cr_str_create_copy(struct cr_mem* mem, struct cr_str* original);
struct cr_str* cr_str_create_data(struct cr_mem* mem, const char* data, uint32_t data_count);
struct cr_str* cr_str_create_with_contents_of_file(struct cr_mem* mem, struct cr_str* path);
void cr_str_destroy(struct cr_str* str);

uint32_t cr_str_get_count(struct cr_str* str);
const char* cr_str_get_data(struct cr_str* str);
char* cr_str_get_raw(struct cr_str* str);

bool cr_str_write_to_file(struct cr_str* str, struct cr_str* path);

void cr_str_insert_data(struct cr_str* str, uint32_t index, const char* data, uint32_t data_count);
void cr_str_insert_cstr(struct cr_str* str, uint32_t index, const char* cstr);
void cr_str_insert(struct cr_str* str, uint32_t index, struct cr_str* str_2);

void cr_str_append(struct cr_str* str, struct cr_str* str_2);
void cr_str_append_data(struct cr_str* str, const char* data, uint32_t data_count);
void cr_str_append_cstr(struct cr_str* str, const char* cstr);
__attribute__((format(printf, 2, 3)))
void cr_str_append_format(struct cr_str* str, const char* fmt, ...);

void cr_str_remove_all(struct cr_str* str);
void cr_str_remove_last(struct cr_str* str, uint32_t n);

void cr_str_append_path_component(struct cr_str* str, struct cr_str* component);
void cr_str_append_path_component_cstr(struct cr_str* str, const char* cstr);
__attribute__((format(printf, 2, 3)))
void cr_str_append_path_component_format(struct cr_str* str, const char* fmt, ...);

void cr_str_remove_last_path_component(struct cr_str* str);

struct cr_str* cr_str_get_last_path_component(struct cr_mem* mem, struct cr_str* str);

bool cr_str_append_path_extension(struct cr_str* str, struct cr_str* extension);
bool cr_str_append_path_extension_cstr(struct cr_str* str, const char* cstr);
__attribute__((format(printf, 2, 3)))
bool cr_str_append_path_extension_format(struct cr_str* str, const char* fmt, ...);

void cr_str_remove_path_extension(struct cr_str* str);

struct cr_str* cr_str_get_path_extension(struct cr_mem* mem, struct cr_str* str);

enum cr_file_type cr_str_get_file_type(struct cr_str* str);

bool cr_str_is_absolute_path(struct cr_str* str);

bool cr_str_is_equal(struct cr_str* str, struct cr_str* str_2);
uint64_t cr_str_hash(struct cr_str* str);
