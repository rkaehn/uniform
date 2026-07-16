#include "jg.h"

struct jg_record* jg_find_record(const char* name) {
    for (uint32_t i = 0; name[0] != 0 && i < jg_model.record_count; i++) {
        if (strcmp(jg_model.records[i].name, name) == 0) {
            return &jg_model.records[i];
        }
    }
    return NULL;
}

struct jg_enum* jg_find_enum(const char* name) {
    for (uint32_t i = 0; i < jg_model.enum_count; i++) {
        if (strcmp(jg_model.enums[i].name, name) == 0) {
            return &jg_model.enums[i];
        }
    }
    return NULL;
}

struct jg_func* jg_find_func(const char* name) {
    for (uint32_t i = 0; i < jg_model.func_count; i++) {
        if (strcmp(jg_model.funcs[i].name, name) == 0) {
            return &jg_model.funcs[i];
        }
    }
    return NULL;
}

struct jg_constant* jg_find_constant(struct jg_enum* en, const char* name) {
    for (uint32_t i = 0; i < en->constant_count; i++) {
        if (strcmp(en->constants[i].name, name) == 0) {
            return &en->constants[i];
        }
    }
    return NULL;
}

struct jg_constant* jg_union_constant(struct jg_record* record, struct jg_field* field) {
    if (record->type_field < 0) {
        return NULL;
    }
    struct jg_enum* en = record->fields[record->type_field].type->en;
    char name[jg_name * 2];
    snprintf(name, sizeof(name), "%s%s", en->prefix, field->name);
    return jg_find_constant(en, name);
}

bool jg_union_discriminated(struct jg_record* record, uint32_t union_id) {
    if (union_id == 0 || record->type_field < 0) {
        return false;
    }
    for (uint32_t i = 0; i < record->field_count; i++) {
        struct jg_field* field = &record->fields[i];
        if (field->union_id == union_id && jg_union_constant(record, field) == NULL) {
            return false;
        }
    }
    return true;
}

bool jg_type_is_cstr(struct jg_type* t) {
    return t->cls == jg_cls_ptr && t->inner->cls == jg_cls_scalar && t->inner->kind == jg_kind_i8;
}

bool jg_type_is_bytes(struct jg_type* t) {
    return t->cls == jg_cls_ptr && t->inner->is_const && (t->inner->cls == jg_cls_void || (t->inner->cls == jg_cls_scalar && t->inner->kind == jg_kind_u8));
}

bool jg_type_is_fn_ptr(struct jg_type* t) {
    return t->cls == jg_cls_ptr && t->inner->cls == jg_cls_fn;
}

bool jg_type_is_integer(struct jg_type* t) {
    return t->cls == jg_cls_scalar && t->kind != jg_kind_bool && t->kind != jg_kind_f32 && t->kind != jg_kind_f64;
}

bool jg_type_is_wide(struct jg_type* t) {
    return t->cls == jg_cls_scalar && (t->kind == jg_kind_i64 || t->kind == jg_kind_u64 || t->kind == jg_kind_isize || t->kind == jg_kind_usize);
}

bool jg_type_is_chars(struct jg_type* t) {
    return t->cls == jg_cls_array && t->inner->cls == jg_cls_scalar && (t->inner->kind == jg_kind_i8 || t->inner->kind == jg_kind_u8);
}

bool jg_type_is_struct(struct jg_type* t) {
    return t->cls == jg_cls_record && t->record->is_handle == false && t->record->is_wide == false && t->record->is_matrix == false;
}

bool jg_type_is_mem(struct jg_type* t) {
    return t->cls == jg_cls_ptr && t->inner->cls == jg_cls_record && strcmp(t->inner->record->name, "cr_mem") == 0;
}

bool jg_field_is_hidden(struct jg_field* field) {
    return field->name[0] == '_' || jg_type_is_mem(field->type);
}

static bool jg_type_is_value(struct jg_type* t) {
    return t->cls == jg_cls_scalar || t->cls == jg_cls_enum || t->cls == jg_cls_vector || t->cls == jg_cls_record;
}

enum jg_kind jg_type_abi_kind(struct jg_type* t, enum jg_abi abi) {
    if (t->kind == jg_kind_isize) {
        return abi == jg_abi_wasm ? jg_kind_i32 : jg_kind_i64;
    }
    if (t->kind == jg_kind_usize) {
        return abi == jg_abi_wasm ? jg_kind_u32 : jg_kind_u64;
    }
    return t->kind;
}

uint32_t jg_kind_size(enum jg_kind kind) {
    switch (kind) {
    case jg_kind_bool: case jg_kind_i8: case jg_kind_u8: return 1;
    case jg_kind_i16: case jg_kind_u16: return 2;
    case jg_kind_i32: case jg_kind_u32: case jg_kind_f32: return 4;
    case jg_kind_i64: case jg_kind_u64: case jg_kind_f64: return 8;
    default: assert(0 && "platform sized kind"); return 0;
    }
}

uint32_t jg_type_size(struct jg_type* t, enum jg_abi abi) {
    switch (t->cls) {
    case jg_cls_scalar: return jg_kind_size(jg_type_abi_kind(t, abi));
    case jg_cls_vector: return jg_kind_size(t->kind) * (t->count == 3 ? 4 : t->count);
    case jg_cls_enum: return 4;
    case jg_cls_record: return t->record->size[abi];
    case jg_cls_ptr: return abi == jg_abi_wasm ? 4 : 8;
    case jg_cls_array: return t->count * jg_type_size(t->inner, abi);
    default: assert(0 && "sized type"); return 0;
    }
}

const char* jg_type_c(struct jg_type* t, char* buf, size_t size) {
    static const char* const scalars[jg_kind_count] = { "bool", "char", "uint8_t", "int16_t", "uint16_t", "int32_t", "uint32_t", "int64_t", "uint64_t", "float", "double", "intptr_t", "size_t" };
    static const char* const vectors[jg_kind_count] = { NULL, "char", "uchar", "short", "ushort", "int", "uint", "long", "ulong", "float", "double", NULL, NULL };
    const char* c = t->is_const ? "const " : "";
    char inner[256];
    switch (t->cls) {
    case jg_cls_void: snprintf(buf, size, "%svoid", c); break;
    case jg_cls_scalar: snprintf(buf, size, "%s%s", c, scalars[t->kind]); break;
    case jg_cls_vector: assert(vectors[t->kind] != NULL); snprintf(buf, size, "%s%s%u", c, vectors[t->kind], t->count); break;
    case jg_cls_enum: snprintf(buf, size, "%senum %s", c, t->en->name); break;
    case jg_cls_record:
        if (t->record->is_typedef) {
            snprintf(buf, size, "%s%s", c, t->record->name);
        } else {
            snprintf(buf, size, "%s%s %s", c, t->record->is_union ? "union" : "struct", t->record->name);
        }
        break;
    case jg_cls_ptr: assert(t->inner->cls != jg_cls_fn); snprintf(buf, size, "%s*", jg_type_c(t->inner, inner, sizeof(inner))); break;
    default: assert(0 && "c type"); break;
    }
    return buf;
}

uint32_t jg_param_doubles(struct jg_param* param) {
    if (param->role == jg_role_mem || param->role == jg_role_user) {
        return 0;
    }
    return param->role == jg_role_value && param->type->cls == jg_cls_vector ? param->type->count : 1;
}

bool jg_func_returns_out(struct jg_func* func) {
    return func->ret->cls == jg_cls_vector || (func->ret->cls == jg_cls_record && func->ret->record->is_handle == false && func->ret->record->is_wide == false);
}

bool jg_func_available(struct jg_func* func, enum jg_abi abi) {
    return func->bound && (func->platforms & jg_abi_platforms(abi)) != 0 && (func->abi_mask & (1u << abi)) != 0;
}

bool jg_role_is_hidden(enum jg_role role) {
    switch (role) {
    case jg_role_mem:
    case jg_role_count:
    case jg_role_user:
    case jg_role_out:
    case jg_role_out_array:
    case jg_role_out_ptr_array:
    case jg_role_out_count:
    case jg_role_out_string:
    case jg_role_out_size:
        return true;
    default:
        return false;
    }
}

static const char* jg_out_base(const char* name) {
    return jg_starts_with(name, "out_") ? name + 4 : jg_starts_with(name, "inout_") ? name + 6 : name;
}

static bool jg_is_count_name(const char* owner, struct jg_type* owner_type, const char* candidate, bool follows) {
    bool bytes = jg_type_is_cstr(owner_type) || jg_type_is_bytes(owner_type);
    if (strcmp(candidate, "size") == 0) {
        return bytes && follows;
    }
    if (strcmp(candidate, "count") == 0 || strcmp(candidate, "len") == 0) {
        return bytes == false && follows;
    }
    if (strcmp(owner, "argv") == 0 && strcmp(candidate, "argc") == 0) {
        return true;
    }
    size_t len = strlen(owner);
    const char* last = strrchr(owner, '_');
    const char* bases[5] = { owner, owner, owner, last != NULL ? last + 1 : NULL, owner };
    size_t base_lens[5] = {
        len,
        len > 1 && owner[len - 1] == 's' ? len - 1 : 0,
        len > 2 && owner[len - 2] == 'e' && owner[len - 1] == 's' ? len - 2 : 0,
        last != NULL ? strlen(last + 1) : 0,
        last != NULL ? (size_t)(last - owner) : 0,
    };
    static const char* const suffixes[] = { "_count", "_size", "_len" };
    for (uint32_t b = 0; b < cr_count(bases); b++) {
        for (uint32_t s = 0; base_lens[b] > 0 && s < cr_count(suffixes); s++) {
            if (strncmp(candidate, bases[b], base_lens[b]) == 0 && strcmp(candidate + base_lens[b], suffixes[s]) == 0) {
                return true;
            }
        }
    }
    return false;
}

static void jg_split_name(const char* name, char* ns, size_t ns_size, char* js, size_t js_size) {
    const char* underscore = strchr(name, '_');
    if (underscore == NULL || (size_t)(underscore - name) >= ns_size) {
        jg_fatal("%s has no namespace prefix", name);
    }
    snprintf(ns, ns_size, "%.*s", (int)(underscore - name), name);
    jg_camel(underscore + 1, js, js_size, false);
}

static const char* jg_name_override(const char* name) {
    for (uint32_t i = 0; i < jg_name_override_count; i++) {
        if (strcmp(jg_name_overrides[i][0], name) == 0) {
            return jg_name_overrides[i][1];
        }
    }
    return NULL;
}

static const struct jg_scope_rule* jg_scope_rule(const char* holder) {
    for (uint32_t i = 0; i < jg_scope_rule_count; i++) {
        if (strcmp(jg_scope_rules[i].holder, holder) == 0) {
            return &jg_scope_rules[i];
        }
    }
    return NULL;
}

static int32_t jg_add_site(const char* holder, const char* member, struct jg_type* fn_ptr, const char* const* param_names, bool is_param, uint32_t platforms) {
    const struct jg_scope_rule* rule = jg_scope_rule(holder);
    if (rule == NULL) {
        jg_error("%s.%s is a callback without a scope rule; add one to jg_scope_rules", holder, member);
        return -1;
    }
    assert(jg_model.site_count < jg_max_sites);
    struct jg_site* site = &jg_model.sites[jg_model.site_count];
    memset(site, 0, sizeof(*site));
    snprintf(site->name, sizeof(site->name), "%s_%s", holder, member);
    jg_camel(member, site->js, sizeof(site->js), false);
    site->fn = fn_ptr->inner;
    memcpy(site->param_names, param_names, sizeof(site->param_names));
    struct jg_type* last = site->fn->param_count > 0 ? site->fn->params[site->fn->param_count - 1] : NULL;
    site->generic = last != NULL && last->cls == jg_cls_ptr && last->inner->cls == jg_cls_void && last->inner->is_const == false;
    site->is_param = is_param;
    site->scope = rule->scope;
    site->user_expr = rule->user_expr;
    site->platforms = platforms;
    if (site->generic == false && site->user_expr == NULL && site->scope != jg_scope_owner) {
        jg_error("%s.%s has no user parameter and no user expression in its scope rule", holder, member);
    }
    return (int32_t)jg_model.site_count++;
}

static void jg_analyze_record_shape(struct jg_record* record) {
    if (record->field_count == 1 && strcmp(record->fields[0].name, "raw") == 0 && record->fields[0].type->cls == jg_cls_scalar && record->fields[0].type->kind == jg_kind_u64 && strcmp(record->name, "ui_key") != 0) {
        bool packed = strcmp(record->file, "cr_handle.h") == 0;
        record->is_handle = packed;
        record->is_wide = packed == false;
    }
    if (record->field_count == 1 && strcmp(record->fields[0].name, "columns") == 0 && record->fields[0].type->cls == jg_cls_array && record->fields[0].type->inner->cls == jg_cls_vector) {
        record->is_matrix = true;
    }
    for (uint32_t i = 0; i < record->field_count; i++) {
        struct jg_field* field = &record->fields[i];
        jg_camel(field->name, field->js, sizeof(field->js), false);
        if (strcmp(field->name, "type") == 0 && field->type->cls == jg_cls_enum) {
            record->type_field = (int32_t)i;
        }
    }
}

static void jg_analyze_record_counts(struct jg_record* record) {
    for (uint32_t i = 0; i < record->field_count; i++) {
        struct jg_field* field = &record->fields[i];
        struct jg_type* t = field->type;
        bool counted = (t->cls == jg_cls_ptr && t->inner->cls != jg_cls_fn && t->inner->cls != jg_cls_void) || jg_type_is_bytes(t) || (t->cls == jg_cls_array && jg_type_is_chars(t) == false);
        if (t->cls == jg_cls_ptr && t->inner->cls == jg_cls_record && t->inner->record->is_public == false) {
            counted = false;
        }
        if (counted == false) {
            continue;
        }
        for (uint32_t j = 0; j < record->field_count; j++) {
            struct jg_field* other = &record->fields[j];
            if (j != i && jg_type_is_integer(other->type) && jg_is_count_name(field->name, t, other->name, j > i)) {
                field->count_field = (int32_t)j;
                other->is_count = true;
                break;
            }
        }
    }
}

static void jg_mark_input(struct jg_record* record);

static void jg_mark_input_type(struct jg_type* t) {
    if (t->cls == jg_cls_record) {
        jg_mark_input(t->record);
    } else if (t->cls == jg_cls_array || (t->cls == jg_cls_ptr && t->inner->cls == jg_cls_record && t->inner->is_const)) {
        jg_mark_input_type(t->inner);
    }
}

static void jg_mark_input(struct jg_record* record) {
    if (record->is_input || record->is_public == false) {
        return;
    }
    record->is_input = true;
    for (uint32_t i = 0; i < record->field_count; i++) {
        jg_mark_input_type(record->fields[i].type);
    }
}

static void jg_analyze_record_sites(struct jg_record* record) {
    int32_t user = -1;
    for (uint32_t i = 0; i < record->field_count; i++) {
        struct jg_field* field = &record->fields[i];
        if (strcmp(field->name, "user") == 0 && field->type->cls == jg_cls_ptr && field->type->inner->cls == jg_cls_void) {
            user = (int32_t)i;
        }
    }
    for (uint32_t i = 0; i < record->field_count; i++) {
        struct jg_field* field = &record->fields[i];
        if (jg_type_is_fn_ptr(field->type) == false) {
            continue;
        }
        const struct jg_scope_rule* rule = jg_scope_rule(record->name);
        if (user < 0 && (rule == NULL || rule->scope != jg_scope_owner)) {
            jg_error("%s.%s is a callback in a record without a user field", record->name, field->name);
            continue;
        }
        if (user >= 0) {
            record->fields[user].is_user = true;
        }
        field->site = jg_add_site(record->name, field->name, field->type, field->param_names, false, record->platforms);
    }
}

static int32_t jg_find_partner(struct jg_func* func, uint32_t i, bool (*matches)(struct jg_param* owner, struct jg_param* other, bool follows)) {
    for (uint32_t j = 0; j < func->param_count; j++) {
        struct jg_param* other = &func->params[j];
        bool free = other->role == jg_role_none || other->role == jg_role_value || other->role == jg_role_count;
        if (j != i && free && matches(&func->params[i], other, j > i)) {
            return (int32_t)j;
        }
    }
    return -1;
}

static bool jg_matches_count(struct jg_param* owner, struct jg_param* other, bool follows) {
    return jg_type_is_integer(other->type) && jg_is_count_name(jg_out_base(owner->name), owner->type, other->name, follows);
}

static bool jg_matches_out_count(struct jg_param* owner, struct jg_param* other, bool follows) {
    return other->type->cls == jg_cls_ptr && jg_type_is_integer(other->type->inner) && jg_starts_with(other->name, "out_") && jg_is_count_name(jg_out_base(owner->name), owner->type->inner, other->name + 4, follows);
}

static bool jg_matches_capacity(struct jg_param* owner, struct jg_param* other, bool follows) {
    char full[jg_name + 16];
    snprintf(full, sizeof(full), "%s_capacity", jg_out_base(owner->name));
    return jg_type_is_integer(other->type) && (strcmp(other->name, full) == 0 || strcmp(other->name, "capacity") == 0);
}

static void jg_pair(struct jg_func* func, uint32_t i, enum jg_role role, int32_t partner, enum jg_role partner_role) {
    func->params[i].role = role;
    func->params[i].partner = partner;
    func->params[partner].role = partner_role;
}

static const char* jg_classify_param(struct jg_func* func, uint32_t i) {
    struct jg_param* param = &func->params[i];
    struct jg_type* t = param->type;
    if (param->role != jg_role_none) {
        return NULL;
    }
    if (jg_type_is_mem(t)) {
        param->role = jg_role_mem;
        return NULL;
    }
    if (jg_type_is_fn_ptr(t)) {
        param->role = jg_role_fn;
        if (i + 1 < func->param_count) {
            struct jg_param* next = &func->params[i + 1];
            if (next->type->cls == jg_cls_ptr && next->type->inner->cls == jg_cls_void && next->type->inner->is_const == false && (strcmp(next->name, "user") == 0 || strcmp(next->name, "args") == 0)) {
                jg_pair(func, i, jg_role_fn, (int32_t)(i + 1), jg_role_user);
            }
        }
        param->site = jg_add_site(func->name, param->name, t, param->param_names, true, func->platforms);
        return param->site >= 0 ? NULL : "callback without a scope rule";
    }
    if (jg_type_is_value(t)) {
        param->role = jg_role_value;
        return NULL;
    }
    if (t->cls != jg_cls_ptr) {
        return "unsupported parameter type";
    }
    struct jg_type* p = t->inner;
    int32_t count = jg_find_partner(func, i, jg_matches_count);
    bool is_out = jg_starts_with(param->name, "out_") || strcmp(param->name, "out") == 0;
    if (jg_type_is_cstr(t) || jg_type_is_bytes(t)) {
        param->role = jg_type_is_cstr(t) ? jg_role_string : jg_role_bytes;
        if (count >= 0) {
            jg_pair(func, i, param->role, count, jg_role_count);
        }
        return NULL;
    }
    if (p->cls == jg_cls_void) {
        param->role = jg_role_ptr;
        return NULL;
    }
    if (p->cls == jg_cls_ptr) {
        if (jg_type_is_cstr(p) && is_out) {
            int32_t size = jg_find_partner(func, i, jg_matches_out_count);
            if (size < 0) {
                return "out string without a size partner";
            }
            jg_pair(func, i, jg_role_out_string, size, jg_role_out_size);
            return NULL;
        }
        if (jg_type_is_cstr(p) && count >= 0) {
            jg_pair(func, i, jg_role_array_in, count, jg_role_count);
            return NULL;
        }
        if (is_out && p->inner->is_const) {
            int32_t out_count = jg_find_partner(func, i, jg_matches_out_count);
            if (out_count < 0) {
                return "out pointer array without a count partner";
            }
            jg_pair(func, i, jg_role_out_ptr_array, out_count, jg_role_out_count);
            return NULL;
        }
        return "pointer to pointer parameter";
    }
    if (p->cls == jg_cls_record && (p->record->is_public == false || p->record->is_object)) {
        param->role = jg_role_ptr;
        return NULL;
    }
    if (p->cls != jg_cls_record && p->cls != jg_cls_scalar && p->cls != jg_cls_vector && p->cls != jg_cls_enum) {
        return "unsupported pointer parameter";
    }
    if (p->is_const && count >= 0) {
        jg_pair(func, i, jg_role_array_in, count, jg_role_count);
    } else if (p->is_const && p->cls == jg_cls_record) {
        param->role = jg_role_struct_in;
    } else if (p->is_const) {
        return "pointer to a constant value without a count";
    } else if (is_out) {
        int32_t capacity = jg_find_partner(func, i, jg_matches_capacity);
        if (capacity >= 0) {
            jg_pair(func, i, jg_role_out_array, capacity, jg_role_capacity);
        } else {
            param->role = jg_role_out;
        }
    } else if (jg_starts_with(param->name, "inout_") && p->cls == jg_cls_record) {
        param->role = jg_role_inout;
    } else {
        param->role = jg_role_ptr;
    }
    return NULL;
}

static const char* jg_classify_ret(struct jg_func* func) {
    struct jg_type* t = func->ret;
    if (t->cls == jg_cls_void || jg_type_is_value(t) || jg_type_is_cstr(t) || (t->cls == jg_cls_ptr && t->inner->cls != jg_cls_fn)) {
        return NULL;
    }
    return "unsupported return type";
}

static void jg_func_names(struct jg_func* func) {
    jg_split_name(func->name, func->ns, sizeof(func->ns), func->js, sizeof(func->js));
    for (uint32_t i = 0; i < func->param_count; i++) {
        jg_camel(func->params[i].name, func->params[i].js, sizeof(func->params[i].js), false);
    }
}

static bool jg_func_is_value_only(struct jg_func* func) {
    if (jg_type_is_value(func->ret) == false && jg_type_is_cstr(func->ret) == false) {
        return false;
    }
    for (uint32_t i = 0; i < func->param_count; i++) {
        if (jg_type_is_value(func->params[i].type) == false && jg_type_is_cstr(func->params[i].type) == false) {
            return false;
        }
    }
    return true;
}

static void jg_analyze_funcs(void) {
    for (uint32_t i = 0; i < jg_model.func_count; i++) {
        struct jg_func* func = &jg_model.funcs[i];
        jg_func_names(func);
        if (jg_is_excluded(func->name)) {
            continue;
        }
        if (func->is_inline && jg_func_is_value_only(func)) {
            func->is_helper = true;
        } else if (func->variadic) {
            jg_error("%s is variadic; exclude it and write it by hand", func->name);
        } else {
            func->bound = true;
        }
    }
    for (uint32_t i = 0; i < jg_model.func_count; i++) {
        struct jg_func* func = &jg_model.funcs[i];
        struct jg_type* ret = func->ret;
        if (func->bound && ret->cls == jg_cls_ptr && ret->inner->cls == jg_cls_record && ret->inner->is_const == false && ret->inner->record->is_public) {
            ret->inner->record->is_object = true;
        }
    }
    for (uint32_t i = 0; i < jg_model.func_count; i++) {
        struct jg_func* func = &jg_model.funcs[i];
        if (func->bound == false) {
            continue;
        }
        for (uint32_t p = 0; p < func->param_count; p++) {
            const char* reason = jg_classify_param(func, p);
            if (reason != NULL) {
                jg_error("%s: parameter %s: %s", func->name, func->params[p].name, reason);
                func->bound = false;
            }
        }
        const char* reason = jg_classify_ret(func);
        if (reason != NULL) {
            jg_error("%s: %s", func->name, reason);
            func->bound = false;
        }
    }
}

static void jg_mark_inputs(void) {
    for (uint32_t i = 0; i < jg_model.func_count; i++) {
        struct jg_func* func = &jg_model.funcs[i];
        if (func->bound == false) {
            continue;
        }
        for (uint32_t p = 0; p < func->param_count; p++) {
            struct jg_param* param = &func->params[p];
            if (param->role == jg_role_value || param->role == jg_role_struct_in || param->role == jg_role_array_in || param->role == jg_role_inout) {
                jg_mark_input_type(param->type);
            }
        }
    }
    bool changed = true;
    while (changed) {
        changed = false;
        for (uint32_t r = 0; r < jg_model.record_count; r++) {
            struct jg_record* record = &jg_model.records[r];
            for (uint32_t i = 0; record->is_input && i < record->field_count; i++) {
                struct jg_type* t = record->fields[i].type;
                if (jg_type_is_fn_ptr(t) == false) {
                    continue;
                }
                for (uint32_t p = 0; p < t->inner->param_count; p++) {
                    struct jg_type* param = t->inner->params[p];
                    if (param->cls == jg_cls_ptr && param->inner->cls == jg_cls_record && param->inner->is_const == false && param->inner->record->is_input == false && param->inner->record->is_public) {
                        jg_mark_input(param->inner->record);
                        changed = true;
                    }
                }
            }
        }
    }
}

static void jg_analyze_enum(struct jg_enum* en) {
    snprintf(en->prefix, sizeof(en->prefix), "%s_", en->name);
    bool shared = en->constant_count > 0;
    for (uint32_t c = 0; c < en->constant_count; c++) {
        shared = shared && jg_starts_with(en->constants[c].name, en->prefix);
    }
    if (shared == false && en->constant_count > 0) {
        size_t common = strlen(en->constants[0].name);
        for (uint32_t c = 1; c < en->constant_count; c++) {
            size_t k = 0;
            while (k < common && en->constants[c].name[k] == en->constants[0].name[k]) {
                k++;
            }
            common = k;
        }
        while (common > 0 && en->constants[0].name[common - 1] != '_') {
            common--;
        }
        if (common == 0) {
            jg_error("enum %s has no shared constant prefix", en->name);
            return;
        }
        snprintf(en->prefix, sizeof(en->prefix), "%.*s", (int)common, en->constants[0].name);
    }
    for (uint32_t c = 0; c < en->constant_count; c++) {
        struct jg_constant* constant = &en->constants[c];
        const char* override = jg_name_override(constant->name);
        if (en->is_flag) {
            jg_split_name(constant->name, en->ns, sizeof(en->ns), constant->js, sizeof(constant->js));
        } else if (override != NULL) {
            snprintf(constant->js, sizeof(constant->js), "%s", override);
        } else {
            jg_camel(constant->name + strlen(en->prefix), constant->js, sizeof(constant->js), false);
        }
        if (constant->js[0] >= '0' && constant->js[0] <= '9') {
            jg_error("enumerator %s maps to '%s', which starts with a digit; add a name override", constant->name, constant->js);
        }
        for (uint32_t d = 0; d < c; d++) {
            if (strcmp(en->constants[d].js, constant->js) == 0) {
                jg_error("enumerators %s and %s both map to %s", en->constants[d].name, constant->name, constant->js);
            }
        }
    }
}

bool jg_record_has_scope(struct jg_record* record, enum jg_scope scope) {
    for (uint32_t i = 0; i < record->field_count; i++) {
        struct jg_field* field = &record->fields[i];
        if (field->site >= 0 && jg_model.sites[field->site].scope == scope) {
            return true;
        }
        struct jg_type* t = field->type;
        while (t->cls == jg_cls_array || (t->cls == jg_cls_ptr && t->inner->is_const)) {
            t = t->inner;
        }
        if (t->cls == jg_cls_record && t->record != record && jg_record_has_scope(t->record, scope)) {
            return true;
        }
    }
    return false;
}

static bool jg_record_has_node(struct jg_record* record) {
    for (uint32_t i = 0; i < record->field_count; i++) {
        struct jg_field* field = &record->fields[i];
        if (strcmp(field->name, "node") == 0 && field->type->cls == jg_cls_record && strcmp(field->type->record->name, "ui_node") == 0) {
            return true;
        }
    }
    return false;
}

static void jg_analyze_roots(void) {
    for (uint32_t i = 0; i < jg_model.func_count; i++) {
        struct jg_func* func = &jg_model.funcs[i];
        func->root_param = -1;
        if (func->bound == false) {
            continue;
        }
        for (uint32_t p = 0; p < func->param_count; p++) {
            struct jg_param* param = &func->params[p];
            if (param->role != jg_role_struct_in && param->role != jg_role_value && param->role != jg_role_array_in && param->role != jg_role_inout) {
                continue;
            }
            struct jg_type* t = param->type->cls == jg_cls_ptr ? param->type->inner : param->type;
            if (t->cls != jg_cls_record || (jg_record_has_scope(t->record, jg_scope_tree) == false && jg_record_has_scope(t->record, jg_scope_input) == false)) {
                continue;
            }
            func->adopts = true;
            if (func->root_param < 0 && jg_record_has_node(t->record)) {
                func->root_param = (int32_t)p;
            }
        }
        if (func->adopts == false) {
            continue;
        }
        func->root_from_result = func->ret->cls == jg_cls_record && strcmp(func->ret->record->name, "ui_node") == 0;
        if (func->root_from_result == false && func->root_param < 0) {
            jg_error("%s declares tree or input callbacks but neither returns a node nor takes a node", func->name);
        }
    }
}

void jg_analyze(void) {
    for (uint32_t i = 0; i < jg_model.record_count; i++) {
        jg_analyze_record_shape(&jg_model.records[i]);
    }
    for (uint32_t i = 0; i < jg_model.record_count; i++) {
        if (jg_model.records[i].is_public) {
            jg_analyze_record_counts(&jg_model.records[i]);
        }
    }
    jg_analyze_funcs();
    jg_mark_inputs();
    for (uint32_t i = 0; i < jg_model.record_count; i++) {
        if (jg_model.records[i].is_input) {
            jg_analyze_record_sites(&jg_model.records[i]);
        }
    }
    jg_analyze_roots();
    for (uint32_t i = 0; i < jg_model.enum_count; i++) {
        if (jg_model.enums[i].is_public) {
            jg_analyze_enum(&jg_model.enums[i]);
        }
    }
    for (uint32_t i = 0; i < jg_model.var_count; i++) {
        struct jg_var* var = &jg_model.vars[i];
        jg_split_name(var->name, var->ns, sizeof(var->ns), var->js, sizeof(var->js));
    }
    for (uint32_t i = 0; i < jg_model.macro_count; i++) {
        struct jg_macro* macro = &jg_model.macros[i];
        jg_split_name(macro->name, macro->ns, sizeof(macro->ns), macro->js, sizeof(macro->js));
    }
}
