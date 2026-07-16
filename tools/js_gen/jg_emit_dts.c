#include "jg.h"

static bool jg_ts_in[jg_max_records];
static bool jg_ts_out[jg_max_records];

static bool jg_ts_is_interface(struct jg_record* record) {
    return record->name[0] != 0 && record->is_public && record->is_handle == false && record->is_wide == false && record->is_matrix == false && strcmp(record->name, "ui_key") != 0;
}

static uint32_t jg_ts_index(struct jg_record* record) {
    return (uint32_t)(record - jg_model.records);
}

static void jg_ts_record_name(struct jg_record* record, bool input, char* buf, size_t size) {
    snprintf(buf, size, "%s%s", record->js, input && jg_ts_out[jg_ts_index(record)] ? "Init" : "");
}

static void jg_ts_fields(struct cr_str* out, struct jg_record* record, bool input, const char* indent);

static const char* const jg_ts_namespaces[] = { "app", "gpu", "ui", "hud", "log", "bd" };

static void jg_ts_hand_written(struct cr_str* out) {
    struct cr_str* api = jg_read_file("src/js/api.d.ts");
    cr_str_append(out, api);
    cr_str_destroy(api);
}

void jg_ts_type(struct jg_type* t, bool input, char* buf, size_t size) {
    char inner[1024];
    switch (t->cls) {
    case jg_cls_void:
        snprintf(buf, size, "void");
        break;
    case jg_cls_scalar:
        snprintf(buf, size, "%s", t->kind == jg_kind_bool ? "boolean" : "number");
        break;
    case jg_cls_vector:
        if (input) {
            snprintf(buf, size, "number[]");
        } else {
            snprintf(buf, size, t->count == 2 ? "[number, number]" : t->count == 3 ? "[number, number, number]" : "[number, number, number, number]");
        }
        break;
    case jg_cls_enum:
        snprintf(buf, size, input ? "%s | number" : "%s", t->en->js);
        break;
    case jg_cls_record:
        if (t->record->is_handle || t->record->is_wide) {
            snprintf(buf, size, "number");
        } else if (t->record->is_matrix) {
            snprintf(buf, size, "number[]");
        } else if (strcmp(t->record->name, "ui_key") == 0) {
            snprintf(buf, size, input ? "UiKey" : "number | undefined");
        } else if (t->record->name[0] == 0) {
            struct cr_str* fields = cr_str_create_empty(jg_mem);
            jg_ts_fields(fields, t->record, input, " ");
            snprintf(buf, size, "{%s}", cr_str_get_data(fields));
            cr_str_destroy(fields);
        } else {
            jg_ts_record_name(t->record, input, buf, size);
        }
        break;
    case jg_cls_ptr:
        if (jg_type_is_cstr(t)) {
            snprintf(buf, size, "string");
        } else if (jg_type_is_bytes(t)) {
            snprintf(buf, size, "Uint8Array");
        } else {
            snprintf(buf, size, "number");
        }
        break;
    case jg_cls_array:
        if (jg_type_is_chars(t)) {
            snprintf(buf, size, "string");
        } else {
            jg_ts_type(t->inner, input, inner, sizeof(inner));
            snprintf(buf, size, "%s[]", inner);
        }
        break;
    default:
        snprintf(buf, size, "any");
        break;
    }
}

static bool jg_ts_is_out_struct(struct jg_type* fn, struct jg_type* p) {
    return fn->inner->cls == jg_cls_void && p->cls == jg_cls_ptr && p->inner->is_const == false && p->inner->cls == jg_cls_record && jg_ts_is_interface(p->inner->record);
}

static void jg_ts_mark_fn(struct jg_type* fn);

static void jg_ts_mark_record(struct jg_record* record, bool input);

static void jg_ts_mark_type(struct jg_type* t, bool input) {
    if (jg_type_is_fn_ptr(t)) {
        if (input) {
            jg_ts_mark_fn(t->inner);
        }
    } else if (t->cls == jg_cls_ptr || t->cls == jg_cls_array) {
        jg_ts_mark_type(t->inner, input);
    } else if (t->cls == jg_cls_record && t->record->is_object == false) {
        jg_ts_mark_record(t->record, input);
    }
}

static void jg_ts_mark_record(struct jg_record* record, bool input) {
    bool* marked = input ? &jg_ts_in[jg_ts_index(record)] : &jg_ts_out[jg_ts_index(record)];
    if (*marked || record->is_public == false) {
        return;
    }
    *marked = true;
    for (uint32_t i = 0; i < record->field_count; i++) {
        jg_ts_mark_type(record->fields[i].type, input);
    }
}

static void jg_ts_mark_fn(struct jg_type* fn) {
    for (uint32_t i = 0; i < fn->param_count; i++) {
        jg_ts_mark_type(fn->params[i], false);
        if (jg_ts_is_out_struct(fn, fn->params[i])) {
            jg_ts_mark_type(fn->params[i], true);
        }
    }
    jg_ts_mark_type(fn->inner, true);
}

static void jg_ts_mark_all(void) {
    for (uint32_t i = 0; i < jg_model.func_count; i++) {
        struct jg_func* func = &jg_model.funcs[i];
        if (func->is_helper) {
            for (uint32_t p = 0; p < func->param_count; p++) {
                jg_ts_mark_type(func->params[p].type, true);
            }
            jg_ts_mark_type(func->ret, false);
        }
        if (func->bound == false) {
            continue;
        }
        for (uint32_t p = 0; p < func->param_count; p++) {
            struct jg_param* param = &func->params[p];
            switch (param->role) {
            case jg_role_struct_in:
            case jg_role_value:
            case jg_role_array_in:
                jg_ts_mark_type(param->type, true);
                break;
            case jg_role_inout:
                jg_ts_mark_type(param->type, true);
                jg_ts_mark_type(param->type, false);
                break;
            case jg_role_fn:
                jg_ts_mark_fn(param->type->inner);
                break;
            case jg_role_out:
            case jg_role_out_array:
            case jg_role_out_ptr_array:
                jg_ts_mark_type(param->type, false);
                break;
            default:
                break;
            }
        }
        jg_ts_mark_type(func->ret, false);
    }
}

static void jg_ts_fn_type(struct jg_type* fn, const char* const* names, char* buf, size_t size) {
    char params[1024] = "";
    char ret[256];
    jg_ts_type(fn->inner, true, ret, sizeof(ret));
    uint32_t count = fn->param_count;
    if (count > 0 && fn->params[count - 1]->cls == jg_cls_ptr && fn->params[count - 1]->inner->cls == jg_cls_void) {
        count--;
    }
    for (uint32_t i = 0; i < count; i++) {
        struct jg_type* p = fn->params[i];
        struct jg_record* pointee = p->cls == jg_cls_ptr && p->inner->cls == jg_cls_record ? p->inner->record : NULL;
        bool is_struct = pointee != NULL && jg_ts_is_interface(pointee) && pointee->is_object == false;
        bool is_array = is_struct && i + 1 < count && jg_type_is_integer(fn->params[i + 1]);
        char t[512];
        jg_ts_type(is_struct ? p->inner : p, false, t, sizeof(t));
        if (is_array) {
            strncat(t, "[]", sizeof(t) - strlen(t) - 1);
        } else if (jg_ts_is_out_struct(fn, p)) {
            char init[256];
            jg_ts_type(p->inner, true, init, sizeof(init));
            snprintf(ret, sizeof(ret), "%s | void", init);
        }
        char name[jg_name];
        const char* raw = names[i] != NULL && names[i][0] != 0 ? names[i] : "arg";
        jg_camel(jg_starts_with(raw, "out_") ? raw + 4 : raw, name, sizeof(name), false);
        size_t len = strlen(params);
        snprintf(params + len, sizeof(params) - len, "%s%s: %s", i > 0 ? ", " : "", name, t);
    }
    snprintf(buf, size, "((%s) => %s) | number", params, ret);
}

static void jg_ts_field_type(struct jg_field* field, bool input, char* buf, size_t size) {
    struct jg_type* t = field->type;
    char elem[512];
    if (jg_type_is_fn_ptr(t) && field->site >= 0 && input) {
        jg_ts_fn_type(t->inner, field->param_names, buf, size);
    } else if (t->cls == jg_cls_ptr && field->count_field >= 0 && jg_type_is_cstr(t) == false && jg_type_is_bytes(t) == false && t->inner->cls != jg_cls_fn && t->inner->cls != jg_cls_void) {
        jg_ts_type(t->inner, input, elem, sizeof(elem));
        snprintf(buf, size, input ? "%s[] | number" : "%s[]", elem);
    } else if (t->cls == jg_cls_ptr && jg_type_is_bytes(t) && (field->count_field < 0 || input)) {
        snprintf(buf, size, input ? "Uint8Array | number" : "number");
    } else if (t->cls == jg_cls_ptr && jg_type_is_cstr(t) && input) {
        snprintf(buf, size, "string | number");
    } else {
        jg_ts_type(t, input, buf, size);
    }
}

static void jg_ts_fields(struct cr_str* out, struct jg_record* record, bool input, const char* indent) {
    bool inline_mode = indent[0] == ' ' && indent[1] == 0;
    for (uint32_t i = 0; i < record->field_count; i++) {
        struct jg_field* field = &record->fields[i];
        if (jg_field_is_hidden(field) || (input && (field->is_count || field->is_user))) {
            continue;
        }
        if (input == false && field->union_id != 0 && jg_union_discriminated(record, field->union_id)) {
            continue;
        }
        char type[2048];
        jg_ts_field_type(field, input, type, sizeof(type));
        cr_str_append_format(out, "%s%s%s: %s;%s", indent, field->js, input ? "?" : "", type, inline_mode ? "" : "\n");
    }
}

static void jg_ts_output(struct cr_str* out, struct jg_record* record) {
    char name[jg_name];
    jg_ts_record_name(record, false, name, sizeof(name));
    uint32_t union_id = 0;
    for (uint32_t i = 0; union_id == 0 && i < record->field_count; i++) {
        if (record->fields[i].union_id != 0 && jg_union_discriminated(record, record->fields[i].union_id)) {
            union_id = record->fields[i].union_id;
        }
    }
    if (union_id == 0) {
        cr_str_append_format(out, "interface %s {\n", name);
        jg_ts_fields(out, record, false, "    ");
        cr_str_append_cstr(out, "}\n\n");
        return;
    }
    struct cr_str* base = cr_str_create_empty(jg_mem);
    for (uint32_t i = 0; i < record->field_count; i++) {
        struct jg_field* field = &record->fields[i];
        if ((int32_t)i == record->type_field || jg_field_is_hidden(field) || field->union_id == union_id) {
            continue;
        }
        char type[2048];
        jg_ts_field_type(field, false, type, sizeof(type));
        cr_str_append_format(base, "%s: %s; ", field->js, type);
    }
    bool has_base = cr_str_get_count(base) > 0;
    cr_str_append_format(out, has_base ? "type %s = { %s} & (" : "type %s =%s", name, has_base ? cr_str_get_data(base) : "");
    cr_str_destroy(base);
    struct jg_field* type_field = &record->fields[record->type_field];
    struct jg_enum* en = type_field->type->en;
    for (uint32_t c = 0; c < en->constant_count; c++) {
        cr_str_append_format(out, "\n    | { %s: '%s'", type_field->js, en->constants[c].js);
        for (uint32_t m = 0; m < record->field_count; m++) {
            struct jg_field* member = &record->fields[m];
            if (member->union_id == union_id && jg_union_constant(record, member) == &en->constants[c]) {
                char type[2048];
                jg_ts_field_type(member, false, type, sizeof(type));
                cr_str_append_format(out, "; %s: %s", member->js, type);
            }
        }
        cr_str_append_cstr(out, " }");
    }
    cr_str_append_cstr(out, has_base ? "\n);\n\n" : ";\n\n");
}

static bool jg_ts_param_shown(enum jg_role role) {
    return jg_role_is_hidden(role) == false;
}

static bool jg_ts_param_optional(struct jg_func* func, uint32_t p) {
    bool desc = func->params[p].role == jg_role_struct_in || (func->params[p].role == jg_role_value && func->params[p].type->cls == jg_cls_record);
    for (uint32_t q = p + 1; desc && q < func->param_count; q++) {
        enum jg_role role = func->params[q].role;
        desc = jg_ts_param_shown(role) == false || role == jg_role_struct_in || (role == jg_role_value && func->params[q].type->cls == jg_cls_record);
    }
    return desc;
}

static void jg_ts_params(struct cr_str* out, struct jg_func* func) {
    uint32_t n = 0;
    for (uint32_t p = 0; p < func->param_count; p++) {
        struct jg_param* param = &func->params[p];
        if (jg_ts_param_shown(param->role) == false) {
            continue;
        }
        char t[1024];
        char elem[512];
        switch (param->role) {
        case jg_role_fn:
            jg_ts_fn_type(param->type->inner, param->param_names, t, sizeof(t));
            break;
        case jg_role_array_in:
            jg_ts_type(param->type->inner, true, elem, sizeof(elem));
            snprintf(t, sizeof(t), "%s[]", elem);
            break;
        case jg_role_struct_in:
            jg_ts_type(param->type->inner, true, elem, sizeof(elem));
            snprintf(t, sizeof(t), "%s | number", elem);
            break;
        case jg_role_inout:
            jg_ts_type(param->type->inner, true, t, sizeof(t));
            break;
        case jg_role_string:
        case jg_role_bytes:
            jg_ts_type(param->type, true, elem, sizeof(elem));
            snprintf(t, sizeof(t), param->partner >= 0 ? "%s" : "%s | number", elem);
            break;
        default:
            jg_ts_type(param->type, true, t, sizeof(t));
            break;
        }
        cr_str_append_format(out, "%s%s%s: %s", n > 0 ? ", " : "", param->js, jg_ts_param_optional(func, p) ? "?" : "", t);
        n++;
    }
}

static void jg_ts_return(struct jg_func* func, char* buf, size_t size) {
    char single[512] = "";
    char fields[2048] = "";
    uint32_t outs = 0;
    bool capacity = false;
    for (uint32_t p = 0; p < func->param_count; p++) {
        struct jg_param* param = &func->params[p];
        char t[512];
        char elem[256];
        switch (param->role) {
        case jg_role_out:
            jg_ts_type(param->type->inner, false, t, sizeof(t));
            break;
        case jg_role_out_ptr_array:
            jg_ts_type(param->type->inner->inner, false, elem, sizeof(elem));
            snprintf(t, sizeof(t), "%s[]", elem);
            break;
        case jg_role_out_string:
            snprintf(t, sizeof(t), "string");
            break;
        case jg_role_out_array:
            jg_ts_type(param->type->inner, false, elem, sizeof(elem));
            snprintf(t, sizeof(t), "%s[]", elem);
            capacity = true;
            break;
        default:
            continue;
        }
        char name[jg_name];
        jg_camel(jg_starts_with(param->name, "out_") ? param->name + 4 : param->name, name, sizeof(name), false);
        size_t len = strlen(fields);
        snprintf(fields + len, sizeof(fields) - len, "%s%s: %s", outs > 0 ? "; " : "", name, t);
        if (outs++ == 0) {
            snprintf(single, sizeof(single), "%s", t);
        }
    }
    char combined[2200];
    snprintf(combined, sizeof(combined), outs > 1 ? "{ %s }" : "%s", outs > 1 ? fields : single);
    struct jg_type* t = func->ret;
    if (t->cls == jg_cls_void) {
        snprintf(buf, size, "%s", outs > 0 ? combined : "void");
    } else if (t->cls == jg_cls_scalar && t->kind == jg_kind_bool) {
        snprintf(buf, size, outs > 0 ? "%s | null" : "boolean", combined);
    } else if (t->cls == jg_cls_scalar && capacity && outs == 1) {
        snprintf(buf, size, "%s", combined);
    } else if (t->cls == jg_cls_scalar && outs > 0) {
        snprintf(buf, size, "{ result: number; %s }", fields);
    } else if (jg_type_is_cstr(t)) {
        snprintf(buf, size, "string");
    } else {
        jg_ts_type(t, false, buf, size);
    }
}

static void jg_ts_namespace(struct cr_str* out, const char* ns) {
    char name[64];
    jg_camel(ns, name, sizeof(name), true);
    cr_str_append_format(out, "interface %sApi {\n", name);
    for (uint32_t i = 0; i < jg_model.func_count; i++) {
        struct jg_func* func = &jg_model.funcs[i];
        if (strcmp(func->ns, ns) != 0 || (func->bound == false && func->is_helper == false)) {
            continue;
        }
        cr_str_append_format(out, "    %s(", func->js);
        char ret[2300];
        if (func->is_helper) {
            for (uint32_t p = 0; p < func->param_count; p++) {
                char t[512];
                struct jg_type* pt = func->params[p].type;
                if (pt->cls == jg_cls_vector) {
                    snprintf(t, sizeof(t), "number | number[]");
                } else {
                    jg_ts_type(pt, true, t, sizeof(t));
                }
                cr_str_append_format(out, "%s%s: %s", p > 0 ? ", " : "", func->params[p].js, t);
            }
            jg_ts_type(func->ret, false, ret, sizeof(ret));
        } else {
            jg_ts_params(out, func);
            jg_ts_return(func, ret, sizeof(ret));
        }
        cr_str_append_format(out, "): %s;\n", ret);
    }
    for (uint32_t i = 0; i < jg_model.var_count; i++) {
        if (strcmp(jg_model.vars[i].ns, ns) == 0) {
            cr_str_append_format(out, "    %s: number;\n", jg_model.vars[i].js);
        }
    }
    for (uint32_t i = 0; i < jg_model.enum_count; i++) {
        struct jg_enum* en = &jg_model.enums[i];
        for (uint32_t c = 0; en->is_flag && en->is_public && strcmp(en->ns, ns) == 0 && c < en->constant_count; c++) {
            cr_str_append_format(out, "    %s: %s;\n", en->constants[c].js, en->js);
        }
    }
    for (uint32_t i = 0; i < jg_model.macro_count; i++) {
        struct jg_macro* macro = &jg_model.macros[i];
        if (macro->is_number && strcmp(macro->ns, ns) == 0) {
            cr_str_append_format(out, "    %s: number;\n", macro->js);
        }
    }
    cr_str_append_format(out, "}\n\ndeclare const %s: %sApi;\n\n", ns, name);
}

void jg_emit_dts(void) {
    jg_ts_mark_all();
    struct cr_str* out = cr_str_create(jg_mem, "type UiKey = string | number | undefined;\n\n");
    for (uint32_t i = 0; i < jg_model.enum_count; i++) {
        struct jg_enum* en = &jg_model.enums[i];
        if (en->is_public == false || jg_starts_with(en->name, "js_")) {
            continue;
        }
        cr_str_append_format(out, "type %s =%s", en->js, en->is_flag ? " number" : "");
        for (uint32_t c = 0; en->is_flag == false && c < en->constant_count; c++) {
            cr_str_append_format(out, "%s '%s'", c ? " |" : "", en->constants[c].js);
        }
        cr_str_append_cstr(out, ";\n");
    }
    cr_str_append_cstr(out, "\n");
    for (uint32_t i = 0; i < jg_model.record_count; i++) {
        struct jg_record* record = &jg_model.records[i];
        if (jg_ts_is_interface(record) == false) {
            continue;
        }
        if (jg_ts_in[i] || jg_ts_out[i] == false) {
            char name[jg_name];
            jg_ts_record_name(record, true, name, sizeof(name));
            cr_str_append_format(out, "interface %s {\n", name);
            jg_ts_fields(out, record, true, "    ");
            cr_str_append_cstr(out, "}\n\n");
        }
        if (jg_ts_out[i]) {
            jg_ts_output(out, record);
        }
    }
    for (uint32_t i = 0; i < cr_count(jg_ts_namespaces); i++) {
        jg_ts_namespace(out, jg_ts_namespaces[i]);
    }
    jg_ts_hand_written(out);
    jg_write_output("uniform.d.ts", out);
    cr_str_destroy(out);
}
