#pragma once

#include "cr.h"
#include <clang-c/Index.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

#define jg_max_records 256
#define jg_max_fields 48
#define jg_max_enums 96
#define jg_max_constants 160
#define jg_max_funcs 512
#define jg_max_params 12
#define jg_max_vars 16
#define jg_max_sites 64
#define jg_max_macros 256
#define jg_max_types 8192
#define jg_name 96

#define jg_platform_macos 0x1u
#define jg_platform_ios 0x2u
#define jg_platform_wasm 0x4u
#define jg_platform_all 0x7u
#define jg_platform_tool 0x8u

enum jg_abi {
    jg_abi_native,
    jg_abi_wasm,
    jg_abi_count,
};

enum jg_cls {
    jg_cls_void,
    jg_cls_scalar,
    jg_cls_vector,
    jg_cls_record,
    jg_cls_enum,
    jg_cls_ptr,
    jg_cls_fn,
    jg_cls_array,
};

enum jg_kind {
    jg_kind_bool,
    jg_kind_i8,
    jg_kind_u8,
    jg_kind_i16,
    jg_kind_u16,
    jg_kind_i32,
    jg_kind_u32,
    jg_kind_i64,
    jg_kind_u64,
    jg_kind_f32,
    jg_kind_f64,
    jg_kind_isize,
    jg_kind_usize,
    jg_kind_count,
};

struct jg_record;
struct jg_enum;

struct jg_type {
    enum jg_cls cls;
    enum jg_kind kind;
    uint32_t count;
    bool is_const;
    bool variadic;
    struct jg_record* record;
    struct jg_enum* en;
    struct jg_type* inner;
    struct jg_type* params[jg_max_params];
    uint32_t param_count;
};

struct jg_field {
    char name[jg_name];
    char js[jg_name];
    struct jg_type* type;
    const char* param_names[jg_max_params];
    uint32_t off[jg_abi_count];
    uint32_t union_id;
    int32_t count_field;
    int32_t site;
    bool is_count;
    bool is_user;
};

struct jg_record {
    char name[jg_name];
    char js[jg_name];
    const char* usr;
    const char* file;
    uint32_t platforms;
    bool is_union;
    bool is_typedef;
    bool is_public;
    bool is_handle;
    bool is_wide;
    bool is_matrix;
    bool is_object;
    bool is_input;
    bool is_defined;
    struct jg_field fields[jg_max_fields];
    uint32_t field_count;
    uint32_t size[jg_abi_count];
    uint32_t align[jg_abi_count];
    int32_t type_field;
};

struct jg_constant {
    char name[jg_name];
    char js[jg_name];
    int64_t value;
};

struct jg_enum {
    char name[jg_name];
    char js[jg_name];
    char ns[16];
    char prefix[jg_name];
    uint32_t platforms;
    bool is_flag;
    bool is_public;
    struct jg_constant constants[jg_max_constants];
    uint32_t constant_count;
};

enum jg_role {
    jg_role_none,
    jg_role_mem,
    jg_role_value,
    jg_role_string,
    jg_role_bytes,
    jg_role_ptr,
    jg_role_struct_in,
    jg_role_array_in,
    jg_role_count,
    jg_role_fn,
    jg_role_user,
    jg_role_out,
    jg_role_out_array,
    jg_role_capacity,
    jg_role_out_ptr_array,
    jg_role_out_count,
    jg_role_out_string,
    jg_role_out_size,
    jg_role_inout,
};

struct jg_param {
    char name[jg_name];
    char js[jg_name];
    struct jg_type* type;
    const char* param_names[jg_max_params];
    enum jg_role role;
    int32_t partner;
    int32_t site;
};

struct jg_func {
    char name[jg_name];
    char ns[16];
    char js[jg_name];
    const char* spelling;
    struct jg_type* ret;
    struct jg_param params[jg_max_params];
    uint32_t param_count;
    bool variadic;
    bool is_inline;
    bool bound;
    bool is_helper;
    uint32_t abi_mask;
    uint32_t platforms;
    bool adopts;
    bool root_from_result;
    int32_t root_param;
};

struct jg_var {
    char name[jg_name];
    char ns[16];
    char js[jg_name];
    struct jg_type* type;
    uint32_t platforms;
};

enum jg_scope {
    jg_scope_call,
    jg_scope_tree,
    jg_scope_input,
    jg_scope_persistent,
    jg_scope_replace,
    jg_scope_owner,
};

struct jg_site {
    char name[jg_name];
    char js[jg_name];
    struct jg_type* fn;
    const char* param_names[jg_max_params];
    const char* user_expr;
    bool generic;
    bool is_param;
    enum jg_scope scope;
    uint32_t platforms;
};

struct jg_macro {
    char name[jg_name];
    char ns[16];
    char js[jg_name];
    const char* file;
    uint32_t platforms;
    bool is_function;
    bool is_number;
    double value;
};

struct jg_model {
    struct jg_record records[jg_max_records];
    uint32_t record_count;
    struct jg_enum enums[jg_max_enums];
    uint32_t enum_count;
    struct jg_func funcs[jg_max_funcs];
    uint32_t func_count;
    struct jg_var vars[jg_max_vars];
    uint32_t var_count;
    struct jg_site sites[jg_max_sites];
    uint32_t site_count;
    struct jg_macro macros[jg_max_macros];
    uint32_t macro_count;
};

struct jg_header {
    const char* path;
    uint32_t platforms;
};

struct jg_scope_rule {
    const char* holder;
    enum jg_scope scope;
    const char* user_expr;
};

extern struct cr_mem* jg_mem;
extern struct jg_model jg_model;
extern const char* jg_out_dir;
extern const struct jg_header jg_headers[];
extern const uint32_t jg_header_count;
extern const char* const jg_support_headers[];
extern const uint32_t jg_support_header_count;
extern const char* const jg_excludes[];
extern const uint32_t jg_exclude_count;
extern const struct jg_scope_rule jg_scope_rules[];
extern const uint32_t jg_scope_rule_count;
extern const char* const jg_name_overrides[][2];
extern const uint32_t jg_name_override_count;
extern const char* const jg_abi_labels[jg_abi_count];
extern uint32_t jg_error_count;

__attribute__((format(printf, 1, 2)))
void jg_error(const char* fmt, ...);
__attribute__((format(printf, 1, 2), noreturn))
void jg_fatal(const char* fmt, ...);
const char* jg_strdup(const char* str);
bool jg_starts_with(const char* str, const char* prefix);
bool jg_ends_with(const char* str, const char* suffix);
void jg_camel(const char* snake, char* out, size_t size, bool pascal);
const char* jg_base_name(const char* path);
struct cr_str* jg_read_file(const char* path);
void jg_write_output(const char* name, struct cr_str* text);
bool jg_is_excluded(const char* name);
const char* jg_hand_written_text(void);
bool jg_has_identifier(const char* text, const char* name);
uint32_t jg_header_platforms(const char* file);
uint32_t jg_abi_platforms(enum jg_abi abi);

void jg_parse_set_args(enum jg_abi abi, char** args, uint32_t count);
void jg_parse(void);

struct jg_record* jg_find_record(const char* name);
struct jg_enum* jg_find_enum(const char* name);
struct jg_func* jg_find_func(const char* name);
struct jg_constant* jg_find_constant(struct jg_enum* en, const char* name);
struct jg_constant* jg_union_constant(struct jg_record* record, struct jg_field* field);
bool jg_union_discriminated(struct jg_record* record, uint32_t union_id);
bool jg_type_is_cstr(struct jg_type* t);
bool jg_type_is_bytes(struct jg_type* t);
bool jg_type_is_fn_ptr(struct jg_type* t);
bool jg_type_is_integer(struct jg_type* t);
bool jg_type_is_wide(struct jg_type* t);
bool jg_type_is_chars(struct jg_type* t);
bool jg_type_is_struct(struct jg_type* t);
bool jg_type_is_mem(struct jg_type* t);
bool jg_field_is_hidden(struct jg_field* field);
bool jg_record_has_scope(struct jg_record* record, enum jg_scope scope);
enum jg_kind jg_type_abi_kind(struct jg_type* t, enum jg_abi abi);
uint32_t jg_kind_size(enum jg_kind kind);
uint32_t jg_type_size(struct jg_type* t, enum jg_abi abi);
const char* jg_type_c(struct jg_type* t, char* buf, size_t size);
uint32_t jg_param_doubles(struct jg_param* param);
bool jg_func_returns_out(struct jg_func* func);
bool jg_func_available(struct jg_func* func, enum jg_abi abi);
bool jg_role_is_hidden(enum jg_role role);
void jg_analyze(void);

void jg_emit_c(void);
void jg_emit_jsc(void);
void jg_emit_js(enum jg_abi abi, const char* path);
void jg_emit_dts(void);
void jg_ts_type(struct jg_type* t, bool input, char* buf, size_t size);
