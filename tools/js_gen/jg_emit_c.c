#include "jg.h"

static const char* const jg_lanes[4] = { "x", "y", "z", "w" };

static const char* jg_guard(uint32_t platforms, uint32_t abi_mask) {
    bool wasm = (platforms & jg_platform_wasm) && (abi_mask & (1u << jg_abi_wasm));
    bool macos = (platforms & jg_platform_macos) && (abi_mask & (1u << jg_abi_native));
    bool ios = (platforms & jg_platform_ios) && (abi_mask & (1u << jg_abi_native));
    if ((platforms & jg_platform_tool) && macos) return "uniform_tool";
    if (wasm && macos && ios) return "1";
    if (wasm && !macos && !ios) return "cr_wasm";
    if (!wasm && macos && ios) return "!cr_wasm";
    if (!wasm && macos && !ios) return "cr_macos";
    if (!wasm && !macos && ios) return "cr_ios";
    if (wasm && macos && !ios) return "cr_wasm || cr_macos";
    if (wasm && !macos && ios) return "cr_wasm || cr_ios";
    return "0";
}

static const char* jg_platform_guard(uint32_t platforms) {
    return jg_guard(platforms, (1u << jg_abi_native) | (1u << jg_abi_wasm));
}

static const char* jg_func_guard(struct jg_func* func) {
    return jg_guard(func->platforms, func->abi_mask);
}

static void jg_emit_asserts(struct cr_str* out, enum jg_abi abi) {
    for (uint32_t i = 0; i < jg_model.record_count; i++) {
        struct jg_record* record = &jg_model.records[i];
        if (record->is_public == false || (abi == jg_abi_wasm && (record->platforms & jg_platform_wasm) == 0)) {
            continue;
        }
        char ctype[256];
        struct jg_type t = { .cls = jg_cls_record, .record = record };
        jg_type_c(&t, ctype, sizeof(ctype));
        const char* guard = abi == jg_abi_native && (record->platforms & jg_platform_ios) == 0 ? jg_platform_guard(record->platforms) : NULL;
        if (guard) {
            cr_str_append_format(out, "#if %s\n", guard);
        }
        cr_str_append_format(out, "_Static_assert(sizeof(%s) == %u, \"\");\n", ctype, record->size[abi]);
        for (uint32_t f = 0; f < record->field_count; f++) {
            cr_str_append_format(out, "_Static_assert(offsetof(%s, %s) == %u, \"\");\n", ctype, record->fields[f].name, record->fields[f].off[abi]);
        }
        if (guard) {
            cr_str_append_cstr(out, "#endif\n");
        }
    }
}

static const char* jg_value_from_double(struct jg_type* t, const char* a, char* buf, size_t size) {
    char ctype[256];
    jg_type_c(t, ctype, sizeof(ctype));
    if (t->cls == jg_cls_scalar && t->kind == jg_kind_bool) {
        snprintf(buf, size, "%s != 0.0", a);
    } else if (jg_type_is_wide(t)) {
        snprintf(buf, size, "(%s)js_i64(%s)", ctype, a);
    } else if (t->cls == jg_cls_scalar) {
        snprintf(buf, size, "(%s)%s", ctype, a);
    } else if (t->cls == jg_cls_enum) {
        snprintf(buf, size, "(%s)(int)%s", ctype, a);
    } else if (t->cls == jg_cls_record && t->record->is_handle) {
        snprintf(buf, size, "(%s) { .raw = js_unpack(%s) }", ctype, a);
    } else if (t->cls == jg_cls_record && t->record->is_wide) {
        snprintf(buf, size, "(%s) { .raw = js_u64(%s) }", ctype, a);
    } else if (t->cls == jg_cls_record) {
        snprintf(buf, size, "*(const %s*)js_p(%s)", ctype, a);
    } else if (t->cls == jg_cls_ptr) {
        snprintf(buf, size, "(%s)js_p(%s)", ctype, a);
    } else {
        assert(0 && "value from double");
    }
    return buf;
}

static const char* jg_value_to_double(struct jg_type* t, const char* v, char* buf, size_t size) {
    if (t->cls == jg_cls_scalar && t->kind == jg_kind_bool) {
        snprintf(buf, size, "%s ? 1.0 : 0.0", v);
    } else if (jg_type_is_wide(t)) {
        snprintf(buf, size, "js_double_i64((int64_t)%s)", v);
    } else if (t->cls == jg_cls_scalar) {
        snprintf(buf, size, "(double)%s", v);
    } else if (t->cls == jg_cls_enum) {
        snprintf(buf, size, "(double)(int)%s", v);
    } else if (t->cls == jg_cls_record && t->record->is_handle) {
        snprintf(buf, size, "js_pack(%s.raw)", v);
    } else if (t->cls == jg_cls_record && t->record->is_wide) {
        snprintf(buf, size, "js_double_u64(%s.raw)", v);
    } else if (t->cls == jg_cls_ptr) {
        snprintf(buf, size, "js_ptr(%s)", v);
    } else {
        assert(0 && "value to double");
    }
    return buf;
}

static const char* jg_export_arg(struct jg_param* param, uint32_t n, char* buf, size_t size) {
    char ctype[256];
    char a[32];
    struct jg_type* t = param->type;
    snprintf(a, sizeof(a), "a%u", n);
    switch (param->role) {
    case jg_role_mem:
        snprintf(buf, size, "cr_mem_libc");
        break;
    case jg_role_user:
        snprintf(buf, size, "js_cb_user((uint32_t)a%u)", n - 1);
        break;
    case jg_role_fn:
        snprintf(buf, size, "a%u != 0.0 ? js_tramp_%s : NULL", n, jg_model.sites[param->site].name);
        break;
    case jg_role_value:
        if (t->cls == jg_cls_vector) {
            char vtype[64];
            struct jg_type elem = { .cls = jg_cls_scalar, .kind = t->kind };
            jg_type_c(&elem, ctype, sizeof(ctype));
            int len = snprintf(buf, size, "(%s) { ", jg_type_c(t, vtype, sizeof(vtype)));
            for (uint32_t c = 0; c < t->count; c++) {
                len += snprintf(buf + len, size - (size_t)len, "%s(%s)a%u", c > 0 ? ", " : "", ctype, n + c);
            }
            snprintf(buf + len, size - (size_t)len, " }");
        } else {
            jg_value_from_double(t, a, buf, size);
        }
        break;
    case jg_role_count:
    case jg_role_capacity:
        snprintf(buf, size, "(%s)a%u", jg_type_c(t, ctype, sizeof(ctype)), n);
        break;
    default:
        snprintf(buf, size, "(%s)js_p(a%u)", jg_type_c(t, ctype, sizeof(ctype)), n);
        break;
    }
    return buf;
}

static void jg_emit_export(struct cr_str* out, struct cr_str* header, struct jg_func* func) {
    bool ret_out = jg_func_returns_out(func);
    const char* ret = func->ret->cls == jg_cls_void || ret_out ? "void" : "double";
    struct cr_str* sig = cr_str_create_empty(jg_mem);
    struct cr_str* call = cr_str_create_format(jg_mem, "%s(", func->name);
    uint32_t n = 0;
    uint32_t slots[jg_max_params];
    for (uint32_t p = 0; p < func->param_count; p++) {
        struct jg_param* param = &func->params[p];
        char arg[512];
        slots[p] = n;
        cr_str_append_format(call, "%s%s", p > 0 ? ", " : "", jg_export_arg(param, n, arg, sizeof(arg)));
        for (uint32_t d = 0; d < jg_param_doubles(param); d++) {
            cr_str_append_format(sig, "%sdouble a%u", n > 0 ? ", " : "", n);
            n++;
        }
    }
    char desc[512] = "";
    if (func->adopts && func->root_param >= 0) {
        struct jg_param* param = &func->params[func->root_param];
        struct jg_type base = *(param->type->cls == jg_cls_ptr ? param->type->inner : param->type);
        base.is_const = false;
        char ctype[256];
        snprintf(desc, sizeof(desc), "((const %s*)js_p(a%u))", jg_type_c(&base, ctype, sizeof(ctype)), slots[func->root_param]);
    }
    cr_str_append_cstr(call, ")");
    int64_t root_slot = jg_find_constant(jg_find_enum("js_layout"), "js_layout_root_slot")->value;
    if (n + (ret_out ? 1 : 0) > root_slot) {
        jg_fatal("%s: %u argument slots do not fit below the root slot", func->name, n + (ret_out ? 1 : 0));
    }
    if (ret_out) {
        cr_str_append_format(sig, "%sdouble out", n > 0 ? ", " : "");
    } else if (n == 0) {
        cr_str_append_cstr(sig, "void");
    }
    cr_str_append_format(header, "#if %s\n%s js_x_%s(%s);\n#endif\n", jg_func_guard(func), ret, func->name, cr_str_get_data(sig));
    cr_str_append_format(out, "#if %s\njs_export(%s, %s, (%s)) {\n", jg_func_guard(func), ret, func->name, cr_str_get_data(sig));
    char ctype[256];
    char value[1024];
    if (func->adopts && func->root_from_result == false) {
        cr_str_append_format(out, "    js_args[js_layout_root_slot] = js_pack(ui_get_root(%s->node).raw);\n", desc);
    }
    if (func->adopts && func->root_from_result) {
        cr_str_append_format(out, "    struct ui_node r = %s;\n    js_args[js_layout_root_slot] = js_pack(ui_get_root(r).raw);\n    return js_pack(r.raw);\n", cr_str_get_data(call));
    } else if (func->ret->cls == jg_cls_void) {
        cr_str_append_format(out, "    %s;\n", cr_str_get_data(call));
    } else if (ret_out) {
        cr_str_append_format(out, "    *(%s*)js_p(out) = %s;\n", jg_type_c(func->ret, ctype, sizeof(ctype)), cr_str_get_data(call));
    } else {
        cr_str_append_format(out, "    return %s;\n", jg_value_to_double(func->ret, cr_str_get_data(call), value, sizeof(value)));
    }
    cr_str_append_cstr(out, "}\n#endif\n\n");
    cr_str_destroy(call);
    cr_str_destroy(sig);
}

static void jg_emit_tramp_arg(struct cr_str* out, struct cr_str* after, struct jg_type* fn, uint32_t i, uint32_t user_index) {
    char ctype[256];
    char value[512];
    char p[16];
    struct jg_type* t = fn->params[i];
    snprintf(p, sizeof(p), "p%u", i);
    struct jg_record* pointee = t->cls == jg_cls_ptr && t->inner->cls == jg_cls_record ? t->inner->record : NULL;
    if (t->cls == jg_cls_vector) {
        for (uint32_t c = 0; c < t->count; c++) {
            cr_str_append_format(out, "    a[n++] = (double)p%u.%s;\n", i, jg_lanes[c]);
        }
    } else if (jg_type_is_struct(t)) {
        jg_type_c(t, ctype, sizeof(ctype));
        cr_str_append_format(out, "    %s* c%u = js_bump(sizeof(*c%u), _Alignof(%s));\n    *c%u = p%u;\n    a[n++] = js_ptr(c%u);\n", ctype, i, i, ctype, i, i, i);
    } else if (pointee != NULL && pointee->is_public && pointee->is_object == false) {
        struct jg_type elem = *t->inner;
        elem.is_const = false;
        jg_type_c(&elem, ctype, sizeof(ctype));
        bool writable = t->inner->is_const == false;
        if (i + 1 < user_index && jg_type_is_integer(fn->params[i + 1])) {
            cr_str_append_format(out, "    %s* c%u = js_bump((uint32_t)(sizeof(*c%u) * (p%u > 0 ? (uint32_t)p%u : 1u)), _Alignof(%s));\n", ctype, i, i, i + 1, i + 1, ctype);
            cr_str_append_format(out, "    if (p%u > 0) memcpy(c%u, p%u, sizeof(*c%u) * (uint32_t)p%u);\n    a[n++] = js_ptr(c%u);\n", i + 1, i, i, i, i + 1, i);
            if (writable) {
                cr_str_append_format(after, "    if (p%u > 0) memcpy(p%u, c%u, sizeof(*c%u) * (uint32_t)p%u);\n", i + 1, i, i, i, i + 1);
            }
        } else {
            cr_str_append_format(out, "    %s* c%u = js_bump(sizeof(*c%u), _Alignof(%s));\n    if (p%u) *c%u = *p%u; else memset(c%u, 0, sizeof(*c%u));\n    a[n++] = p%u ? js_ptr(c%u) : 0.0;\n", ctype, i, i, ctype, i, i, i, i, i, i, i);
            if (writable) {
                cr_str_append_format(after, "    if (p%u) *p%u = *c%u;\n", i, i, i);
            }
        }
    } else {
        cr_str_append_format(out, "    a[n++] = %s;\n", jg_value_to_double(t, p, value, sizeof(value)));
    }
}

static void jg_emit_tramp(struct cr_str* out, struct cr_str* header, uint32_t kind, struct jg_site* site) {
    char ctype[256];
    char value[512];
    struct jg_type* fn = site->fn;
    const char* guard = jg_platform_guard(site->platforms);
    uint32_t user_index = site->generic ? fn->param_count - 1 : fn->param_count;
    char ret[256];
    jg_type_c(fn->inner, ret, sizeof(ret));
    struct cr_str* params = cr_str_create_empty(jg_mem);
    for (uint32_t i = 0; i < fn->param_count; i++) {
        cr_str_append_format(params, "%s%s p%u", i > 0 ? ", " : "", jg_type_c(fn->params[i], ctype, sizeof(ctype)), i);
    }
    const char* param_text = fn->param_count > 0 ? cr_str_get_data(params) : "void";
    cr_str_append_format(header, "#if %s\n%s js_tramp_%s(%s);\n#endif\n", guard, ret, site->name, param_text);
    cr_str_append_format(out, "#if %s\n%s js_tramp_%s(%s) {\n", guard, ret, site->name, param_text);
    if (site->scope == jg_scope_owner) {
        cr_str_append_cstr(out, "    uint32_t id = 0;\n");
    } else if (site->generic) {
        cr_str_append_format(out, "    uint32_t id = js_cb_id(p%u);\n", user_index);
    } else {
        cr_str_append_format(out, "    uint32_t id = js_cb_id(%s);\n", site->user_expr);
    }
    cr_str_append_cstr(out, "    uint32_t mark = js_bump_mark();\n    double a[6] = { 0 };\n    uint32_t n = 0;\n");
    struct cr_str* after = cr_str_create_empty(jg_mem);
    for (uint32_t i = 0; i < user_index; i++) {
        jg_emit_tramp_arg(out, after, fn, i, user_index);
    }
    bool ret_slot = fn->inner->cls == jg_cls_vector || jg_type_is_struct(fn->inner);
    if (ret_slot) {
        cr_str_append_format(out, "    %s* r = js_bump(sizeof(*r), _Alignof(%s));\n    memset(r, 0, sizeof(*r));\n    a[n++] = js_ptr(r);\n", ret, ret);
    }
    uint32_t doubles = ret_slot ? 1 : 0;
    for (uint32_t i = 0; i < user_index; i++) {
        doubles += fn->params[i]->cls == jg_cls_vector ? fn->params[i]->count : 1;
    }
    if (doubles > 6) {
        jg_fatal("%s: a callback with %u arguments does not fit the dispatch slots", site->name, doubles);
    }
    bool uses_result = ret_slot == false && fn->inner->cls != jg_cls_void;
    cr_str_append_format(out, "    %sjs_dispatch(%uu, id, a[0], a[1], a[2], a[3], a[4], a[5]);\n", uses_result ? "double d = " : "", kind);
    cr_str_append(out, after);
    if (ret_slot) {
        cr_str_append_format(out, "    %s result = *r;\n    js_bump_release(mark);\n    return result;\n", ret);
    } else if (fn->inner->cls == jg_cls_void) {
        cr_str_append_cstr(out, "    js_bump_release(mark);\n");
    } else {
        cr_str_append_format(out, "    js_bump_release(mark);\n    return %s;\n", jg_value_from_double(fn->inner, "d", value, sizeof(value)));
    }
    cr_str_append_cstr(out, "}\n#endif\n\n");
    cr_str_destroy(params);
    cr_str_destroy(after);
}

static void jg_emit_var(struct cr_str* out, struct cr_str* header, struct jg_var* var) {
    char ctype[256];
    const char* guard = jg_platform_guard(var->platforms);
    if (var->type->cls == jg_cls_ptr) {
        cr_str_append_format(header, "#if %s\ndouble js_x_%s_get(void);\nvoid js_x_%s_set(double value);\n#endif\n", guard, var->name, var->name);
        cr_str_append_format(out, "#if %s\njs_export(double, %s_get, (void)) {\n    return js_ptr(%s);\n}\n\njs_export(void, %s_set, (double value)) {\n    %s = (%s)js_p(value);\n}\n#endif\n\n", guard, var->name, var->name, var->name, var->name, jg_type_c(var->type, ctype, sizeof(ctype)));
    } else {
        cr_str_append_format(header, "#if %s\ndouble js_x_%s_get(void);\n#endif\n", guard, var->name);
        cr_str_append_format(out, "#if %s\njs_export(double, %s_get, (void)) {\n    return js_ptr(&%s);\n}\n#endif\n\n", guard, var->name, var->name);
    }
}

void jg_emit_c(void) {
    struct cr_str* header = cr_str_create(jg_mem, "#pragma once\n\n#include \"js_internal.h\"\n\n");
    struct cr_str* out = cr_str_create(jg_mem, "#include \"js_gen.h\"\n#include <stddef.h>\n\n#if cr_wasm\n");
    jg_emit_asserts(out, jg_abi_wasm);
    cr_str_append_cstr(out, "#else\n");
    jg_emit_asserts(out, jg_abi_native);
    cr_str_append_cstr(out, "#endif\n\n");
    cr_str_append_format(header, "#define js_site_count %uu\n\n", jg_model.site_count);
    for (uint32_t s = 0; s < jg_model.site_count; s++) {
        jg_emit_tramp(out, header, s, &jg_model.sites[s]);
    }
    for (uint32_t i = 0; i < jg_model.func_count; i++) {
        if (jg_model.funcs[i].bound) {
            jg_emit_export(out, header, &jg_model.funcs[i]);
        }
    }
    for (uint32_t i = 0; i < jg_model.var_count; i++) {
        jg_emit_var(out, header, &jg_model.vars[i]);
    }
    cr_str_append_cstr(out, "const void* const js_tramp_addrs[js_site_count] = {\n");
    for (uint32_t s = 0; s < jg_model.site_count; s++) {
        struct jg_site* site = &jg_model.sites[s];
        cr_str_append_format(out, "#if %s\n    (const void*)js_tramp_%s,\n#else\n    NULL,\n#endif\n", jg_platform_guard(site->platforms), site->name);
    }
    cr_str_append_cstr(out, "};\n");
    jg_write_output("js_gen.h", header);
    jg_write_output("js_gen.c", out);
    cr_str_destroy(header);
    cr_str_destroy(out);
}

static const char* const jg_jsc_signature = "(JSContextRef ctx, JSObjectRef function, JSObjectRef this_object, size_t argc, const JSValueRef argv[], JSValueRef* exception)";
static const char* const jg_jsc_guard = "    struct jsc_guard guard;\n    if (jsc_guard_enter(&guard)) {\n        return jsc_guard_throw(ctx, &guard, exception);\n    }\n";

void jg_emit_jsc(void) {
    struct cr_str* out = cr_str_create(jg_mem, "#include \"js_gen.h\"\n#include \"jsc_gen.h\"\n\n");
    for (uint32_t i = 0; i < jg_model.func_count; i++) {
        struct jg_func* func = &jg_model.funcs[i];
        if (jg_func_available(func, jg_abi_native) == false) {
            continue;
        }
        uint32_t n = jg_func_returns_out(func) ? 1 : 0;
        for (uint32_t p = 0; p < func->param_count; p++) {
            n += jg_param_doubles(&func->params[p]);
        }
        struct cr_str* args = cr_str_create_empty(jg_mem);
        for (uint32_t a = 0; a < n; a++) {
            cr_str_append_format(args, "%sa[%u]", a > 0 ? ", " : "", a);
        }
        if (n > 16) {
            jg_fatal("%s: %u call arguments do not fit the argument slots", func->name, n);
        }
        cr_str_append_format(out, "#if %s\nstatic JSValueRef jsc_%s%s {\n    double temp[%u];\n    const double* a = jsc_args(ctx, argc, argv, temp, %u);\n%s", jg_func_guard(func), func->name, jg_jsc_signature, n > 0 ? n : 1, n, jg_jsc_guard);
        if (func->ret->cls == jg_cls_void || jg_func_returns_out(func)) {
            cr_str_append_format(out, "    js_x_%s(%s);\n    jsc_guard_pop(&guard);\n    return JSValueMakeUndefined(ctx);\n}\n#endif\n\n", func->name, cr_str_get_data(args));
        } else {
            cr_str_append_format(out, "    double r = js_x_%s(%s);\n    jsc_guard_pop(&guard);\n    return JSValueMakeNumber(ctx, r);\n}\n#endif\n\n", func->name, cr_str_get_data(args));
        }
        cr_str_destroy(args);
    }
    for (uint32_t i = 0; i < jg_model.var_count; i++) {
        struct jg_var* var = &jg_model.vars[i];
        cr_str_append_format(out, "#if %s\nstatic JSValueRef jsc_%s_get%s {\n    return JSValueMakeNumber(ctx, js_x_%s_get());\n}\n\n", jg_platform_guard(var->platforms), var->name, jg_jsc_signature, var->name);
        if (var->type->cls == jg_cls_ptr) {
            cr_str_append_format(out, "static JSValueRef jsc_%s_set%s {\n    double temp[1];\n    const double* a = jsc_args(ctx, argc, argv, temp, 1);\n%s    js_x_%s_set(a[0]);\n    jsc_guard_pop(&guard);\n    return JSValueMakeUndefined(ctx);\n}\n", var->name, jg_jsc_signature, jg_jsc_guard, var->name);
        }
        cr_str_append_cstr(out, "#endif\n\n");
    }
    cr_str_append_cstr(out, "const struct jsc_entry jsc_table[] = {\n");
    for (uint32_t i = 0; i < jg_model.func_count; i++) {
        struct jg_func* func = &jg_model.funcs[i];
        if (jg_func_available(func, jg_abi_native)) {
            cr_str_append_format(out, "#if %s\n    { \"%s\", jsc_%s },\n#endif\n", jg_func_guard(func), func->name, func->name);
        }
    }
    for (uint32_t i = 0; i < jg_model.var_count; i++) {
        struct jg_var* var = &jg_model.vars[i];
        cr_str_append_format(out, "#if %s\n    { \"%s_get\", jsc_%s_get },\n", jg_platform_guard(var->platforms), var->name, var->name);
        if (var->type->cls == jg_cls_ptr) {
            cr_str_append_format(out, "    { \"%s_set\", jsc_%s_set },\n", var->name, var->name);
        }
        cr_str_append_cstr(out, "#endif\n");
    }
    cr_str_append_cstr(out, "    { NULL, NULL },\n};\n");
    jg_write_output("jsc_gen.c", out);
    cr_str_destroy(out);
}
