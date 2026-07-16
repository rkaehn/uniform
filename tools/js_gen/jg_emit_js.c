#include "jg.h"

static enum jg_abi jg_abi;
static struct cr_str* jg_out;
static struct cr_str* jg_installs;

__attribute__((format(printf, 1, 2)))
static void jg_emit(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char buf[8192];
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (len < 0 || (size_t)len >= sizeof(buf)) {
        jg_fatal("generated text exceeds the emit buffer");
    }
    cr_str_append_cstr(jg_out, buf);
}

static const char* const jg_kind_names[jg_kind_count] = { "bool", "i8", "u8", "i16", "u16", "i32", "u32", "i64", "u64", "f32", "f64", "isize", "usize" };
static const char* const jg_kind_views[jg_kind_count] = { "js_u8", "js_i8", "js_u8", "js_i16", "js_u16", "js_i32", "js_u32", NULL, NULL, "js_f32", "js_f64", NULL, NULL };

static enum jg_kind jg_kind_of(struct jg_type* t) {
    return jg_type_abi_kind(t, jg_abi);
}

static uint32_t jg_shift(enum jg_kind kind) {
    return jg_kind_size(kind) == 8 ? 3 : jg_kind_size(kind) == 4 ? 2 : jg_kind_size(kind) == 2 ? 1 : 0;
}

static const char* jg_scalar_store(enum jg_kind kind, const char* off, const char* value, char* buf, size_t size) {
    if (kind == jg_kind_i64 || kind == jg_kind_u64) {
        snprintf(buf, size, "js_w64(%s, %s)", off, value);
    } else if (kind == jg_kind_bool) {
        snprintf(buf, size, "js_u8[%s] = %s ? 1 : 0", off, value);
    } else if (jg_shift(kind) == 0) {
        snprintf(buf, size, "%s[%s] = %s", jg_kind_views[kind], off, value);
    } else {
        snprintf(buf, size, "%s[(%s) >> %u] = %s", jg_kind_views[kind], off, jg_shift(kind), value);
    }
    return buf;
}

static const char* jg_scalar_load(enum jg_kind kind, const char* off, char* buf, size_t size) {
    if (kind == jg_kind_i64 || kind == jg_kind_u64) {
        snprintf(buf, size, "js_r64(%s)", off);
    } else if (kind == jg_kind_bool) {
        snprintf(buf, size, "js_u8[%s] !== 0", off);
    } else if (jg_shift(kind) == 0) {
        snprintf(buf, size, "%s[%s]", jg_kind_views[kind], off);
    } else {
        snprintf(buf, size, "%s[(%s) >> %u]", jg_kind_views[kind], off, jg_shift(kind));
    }
    return buf;
}

static uint32_t jg_size(struct jg_type* t) {
    return jg_type_size(t, jg_abi);
}

static uint32_t jg_matrix_count(struct jg_record* matrix) {
    struct jg_type* columns = matrix->fields[0].type;
    return columns->count * columns->inner->count;
}

static uint32_t jg_matrix_offset(struct jg_record* matrix, uint32_t element) {
    struct jg_type* columns = matrix->fields[0].type;
    uint32_t lanes = columns->inner->count;
    return element / lanes * jg_size(columns->inner) + element % lanes * jg_kind_size(columns->inner->kind);
}

static const char* jg_array_writer(struct jg_type* elem, char* buf, size_t size) {
    if (elem->cls == jg_cls_record && elem->record->is_handle) {
        snprintf(buf, size, "js_pa_num(%%s, 'f64')");
    } else if (elem->cls == jg_cls_record && elem->record->is_wide) {
        snprintf(buf, size, "js_pa_num(%%s, 'u64')");
    } else if (elem->cls == jg_cls_record) {
        snprintf(buf, size, "js_pa_%s(%%s)", elem->record->name);
    } else if (elem->cls == jg_cls_scalar) {
        snprintf(buf, size, "js_pa_num(%%s, '%s')", jg_kind_names[jg_kind_of(elem)]);
    } else if (elem->cls == jg_cls_vector) {
        snprintf(buf, size, "js_pa_vec(%%s, '%s', %u)", jg_kind_names[elem->kind], elem->count);
    } else if (elem->cls == jg_cls_enum) {
        snprintf(buf, size, "js_pa_enum(%%s, js_enums.%s)", elem->en->name);
    } else if (jg_type_is_cstr(elem)) {
        snprintf(buf, size, "js_pa_str(%%s)");
    } else if (jg_type_is_bytes(elem)) {
        snprintf(buf, size, "js_pa_bytes(%%s)");
    } else if (elem->cls == jg_cls_ptr) {
        snprintf(buf, size, "js_pa_ptr(%%s)");
    } else {
        jg_fatal("no array writer for an element type");
    }
    return buf;
}

static const char* jg_array_reader(struct jg_type* elem, const char* off, const char* count, char* buf, size_t size) {
    if (elem->cls == jg_cls_record && elem->record->is_handle) {
        snprintf(buf, size, "js_ra_num(%s, %s, 'f64')", off, count);
    } else if (elem->cls == jg_cls_record && elem->record->is_wide) {
        snprintf(buf, size, "js_ra_num(%s, %s, 'u64')", off, count);
    } else if (elem->cls == jg_cls_record) {
        snprintf(buf, size, "js_ra_%s(%s, %s)", elem->record->name, off, count);
    } else if (elem->cls == jg_cls_scalar) {
        snprintf(buf, size, "js_ra_num(%s, %s, '%s')", off, count, jg_kind_names[jg_kind_of(elem)]);
    } else if (elem->cls == jg_cls_vector) {
        snprintf(buf, size, "js_ra_vec(%s, %s, '%s', %u)", off, count, jg_kind_names[elem->kind], elem->count);
    } else if (elem->cls == jg_cls_enum) {
        snprintf(buf, size, "js_ra_enum(%s, %s, js_enum_names.%s)", off, count, elem->en->name);
    } else if (jg_type_is_cstr(elem)) {
        snprintf(buf, size, "js_ra_str(%s, %s)", off, count);
    } else {
        snprintf(buf, size, "js_ra_ptr(%s, %s)", off, count);
    }
    return buf;
}

static const char* jg_vector_load(struct jg_type* vector, const char* base, char* buf, size_t size) {
    size_t len = (size_t)snprintf(buf, size, "[");
    for (uint32_t c = 0; c < vector->count; c++) {
        char off[256];
        char load[256];
        snprintf(off, sizeof(off), "%s + %u", base, c * jg_kind_size(vector->kind));
        len += (size_t)snprintf(buf + len, size - len, "%s%s", c > 0 ? ", " : "", jg_scalar_load(vector->kind, off, load, sizeof(load)));
    }
    snprintf(buf + len, size - len, "]");
    return buf;
}

static const char* jg_matrix_load(struct jg_record* matrix, const char* base, char* buf, size_t size) {
    size_t len = (size_t)snprintf(buf, size, "[");
    for (uint32_t e = 0; e < jg_matrix_count(matrix); e++) {
        char off[256];
        char load[256];
        snprintf(off, sizeof(off), "%s + %u", base, jg_matrix_offset(matrix, e));
        len += (size_t)snprintf(buf + len, size - len, "%s%s", e > 0 ? ", " : "", jg_scalar_load(matrix->fields[0].type->inner->kind, off, load, sizeof(load)));
    }
    snprintf(buf + len, size - len, "]");
    return buf;
}

static void jg_emit_vector_store(struct jg_type* vector, const char* base, const char* value, const char* indent) {
    for (uint32_t c = 0; c < vector->count; c++) {
        char off[256];
        char element[256];
        char buf[512];
        snprintf(off, sizeof(off), "%s + %u", base, c * jg_kind_size(vector->kind));
        snprintf(element, sizeof(element), "%s[%u]", value, c);
        jg_emit("%s%s;\n", indent, jg_scalar_store(vector->kind, off, element, buf, sizeof(buf)));
    }
}

static void jg_emit_matrix_store(struct jg_record* matrix, const char* base, const char* value, const char* indent) {
    for (uint32_t e = 0; e < jg_matrix_count(matrix); e++) {
        char off[256];
        char element[256];
        char buf[512];
        snprintf(off, sizeof(off), "%s + %u", base, jg_matrix_offset(matrix, e));
        snprintf(element, sizeof(element), "%s[%u]", value, e);
        jg_emit("%s%s;\n", indent, jg_scalar_store(matrix->fields[0].type->inner->kind, off, element, buf, sizeof(buf)));
    }
}

static const char* jg_register_expr(struct jg_site* site, const char* holder, char* buf, size_t size) {
    switch (site->scope) {
    case jg_scope_call: snprintf(buf, size, "js_cb_call(%s)", holder); break;
    case jg_scope_tree: snprintf(buf, size, "js_cb_tree(%s)", holder); break;
    case jg_scope_input: snprintf(buf, size, "js_cb_input(%s)", holder); break;
    case jg_scope_persistent: snprintf(buf, size, "js_cb_register(%s)", holder); break;
    default: jg_fatal("%s: a replace scope is only valid on function parameters", site->name);
    }
    return buf;
}

struct jg_wctx {
    struct jg_record* record;
    const char* d;
    const char* o;
    int depth;
    bool has_cb;
};

static void jg_emit_write_fields(struct jg_record* record, struct jg_wctx* ctx, const char* indent);

static void jg_emit_count_store(struct jg_wctx* ctx, struct jg_field* field, const char* value, const char* indent) {
    struct jg_field* count = &ctx->record->fields[field->count_field];
    char off[256];
    char buf[512];
    snprintf(off, sizeof(off), "%s + %u", ctx->o, count->off[jg_abi]);
    jg_emit("%s%s;\n", indent, jg_scalar_store(jg_kind_of(count->type), off, value, buf, sizeof(buf)));
}

static void jg_emit_write_value(struct jg_type* t, const char* off, const char* v, const char* indent, struct jg_wctx* ctx, struct jg_field* field) {
    char buf[1024];
    char inner[64];
    snprintf(inner, sizeof(inner), "%s    ", indent);
    switch (t->cls) {
    case jg_cls_scalar:
        jg_emit("%s%s;\n", indent, jg_scalar_store(jg_kind_of(t), off, v, buf, sizeof(buf)));
        break;
    case jg_cls_enum:
        jg_emit("%sconst e = js_enums.%s[%s];\n%sjs_u32[(%s) >> 2] = e !== undefined ? e : js_enum(js_enums.%s, %s);\n", indent, t->en->name, v, indent, off, t->en->name, v);
        break;
    case jg_cls_vector:
        jg_emit_vector_store(t, off, v, indent);
        break;
    case jg_cls_record: {
        struct jg_record* r = t->record;
        if (r->is_handle) {
            jg_emit("%sjs_f64[(%s) >> 3] = %s;\n", indent, off, v);
        } else if (r->is_wide) {
            jg_emit("%sjs_w64(%s, %s);\n", indent, off, v);
        } else if (r->is_matrix) {
            jg_emit_matrix_store(r, off, v, indent);
        } else if (r->name[0] == 0) {
            struct jg_wctx sub = { .record = r, .d = v, .o = off, .depth = ctx->depth + 1 };
            jg_emit_write_fields(r, &sub, indent);
        } else {
            jg_emit("%sjs_w_%s(%s, %s);\n", indent, r->name, off, v);
        }
    } break;
    case jg_cls_array: {
        uint32_t stride = jg_size(t->inner);
        if (jg_type_is_chars(t)) {
            jg_emit("%sjs_wchars(%s, %s, %u);\n", indent, off, v, t->count);
        } else if (t->inner->cls == jg_cls_array && jg_type_is_chars(t->inner)) {
            jg_emit("%sfor (let i = 0; i < %u && i < %s.length; i++) js_wchars(%s + i * %u, %s[i], %u);\n", indent, t->count, v, off, stride, v, t->inner->count);
        } else {
            char eoff[256];
            char ev[64];
            snprintf(eoff, sizeof(eoff), "%s + i%d * %u", off, ctx->depth, stride);
            snprintf(ev, sizeof(ev), "%s[i%d]", v, ctx->depth);
            jg_emit("%sfor (let i%d = 0; i%d < %u && i%d < %s.length; i%d++) {\n", indent, ctx->depth, ctx->depth, t->count, ctx->depth, v, ctx->depth);
            struct jg_wctx sub = *ctx;
            sub.depth++;
            jg_emit_write_value(t->inner, eoff, ev, inner, &sub, NULL);
            jg_emit("%s}\n", indent);
        }
        if (field != NULL && field->count_field >= 0) {
            snprintf(buf, sizeof(buf), "Math.min(%s.length, %u)", v, t->count);
            jg_emit_count_store(ctx, field, buf, indent);
        }
    } break;
    case jg_cls_ptr: {
        if (field != NULL && field->site >= 0 && jg_model.sites[field->site].scope == jg_scope_owner) {
            jg_emit("%sjs_wp(%s, typeof %s === 'function' ? js_cb_addr(%d) : %s);\n", indent, off, v, field->site, v);
        } else if (field != NULL && field->site >= 0) {
            struct jg_site* site = &jg_model.sites[field->site];
            char reg[256];
            jg_emit("%sif (typeof %s === 'function') {\n%sjs_wp(%s, js_cb_addr(%d));\n%sif (cb === 0) cb = %s;\n%s} else {\n%sjs_wp(%s, %s);\n%s}\n", indent, v, inner, off, field->site, inner, jg_register_expr(site, ctx->d, reg, sizeof(reg)), indent, inner, off, v, indent);
            ctx->has_cb = true;
        } else if (jg_type_is_cstr(t) || jg_type_is_bytes(t) || (field != NULL && field->count_field >= 0)) {
            char pack[512];
            if (jg_type_is_cstr(t) || jg_type_is_bytes(t)) {
                snprintf(pack, sizeof(pack), "%s(%s)", jg_type_is_cstr(t) ? "js_pstr" : "js_pbytes", v);
            } else {
                char writer[256];
                jg_array_writer(t->inner, writer, sizeof(writer));
                snprintf(pack, sizeof(pack), writer, v);
            }
            jg_emit("%sjs_wp(%s, typeof %s === 'number' ? %s : %s);\n", indent, off, v, v, pack);
            if (field != NULL && field->count_field >= 0) {
                snprintf(buf, sizeof(buf), "typeof %s === 'number' ? js_num(%s.%s) : js_last_len", v, ctx->d, ctx->record->fields[field->count_field].js);
                jg_emit_count_store(ctx, field, buf, indent);
            }
        } else {
            jg_emit("%sjs_wp(%s, %s);\n", indent, off, v);
        }
    } break;
    default:
        jg_fatal("%s: unsupported field type", ctx->record->name);
    }
}

static void jg_emit_write_field(struct jg_wctx* ctx, struct jg_field* field, const char* indent) {
    char off[256];
    char v[256];
    char inner[64];
    snprintf(off, sizeof(off), "%s + %u", ctx->o, field->off[jg_abi]);
    snprintf(v, sizeof(v), "%s.%s", ctx->d, field->js);
    snprintf(inner, sizeof(inner), "%s    ", indent);
    struct jg_type* t = field->type;
    bool nested = t->cls == jg_cls_array || (t->cls == jg_cls_record && t->record->name[0] == 0);
    if (nested) {
        char nv[32];
        snprintf(nv, sizeof(nv), "n%d", ctx->depth);
        jg_emit("%s{\n%sconst %s = %s;\n%sif (%s !== undefined) {\n", indent, inner, nv, v, inner, nv);
        char inner2[64];
        snprintf(inner2, sizeof(inner2), "%s    ", inner);
        struct jg_wctx sub = *ctx;
        sub.depth++;
        jg_emit_write_value(t, off, nv, inner2, &sub, field);
        ctx->has_cb = ctx->has_cb || sub.has_cb;
        jg_emit("%s}\n%s}\n", inner, indent);
        return;
    }
    jg_emit("%sv = %s;\n%sif (v !== undefined) {\n", indent, v, indent);
    jg_emit_write_value(t, off, "v", inner, ctx, field);
    jg_emit("%s}\n", indent);
}

static void jg_emit_write_fields(struct jg_record* record, struct jg_wctx* ctx, const char* indent) {
    uint32_t done_union = 0;
    for (uint32_t i = 0; i < record->field_count; i++) {
        struct jg_field* field = &record->fields[i];
        if (jg_type_is_mem(field->type)) {
            jg_emit("%sjs_wp(%s + %u, js_mem_libc);\n", indent, ctx->o, field->off[jg_abi]);
            continue;
        }
        if (field->is_count || field->is_user || jg_field_is_hidden(field)) {
            continue;
        }
        if (field->union_id != 0 && jg_union_discriminated(record, field->union_id)) {
            if (field->union_id == done_union) {
                continue;
            }
            done_union = field->union_id;
            char inner[64];
            snprintf(inner, sizeof(inner), "%s    ", indent);
            jg_emit("%sswitch (t) {\n", indent);
            for (uint32_t m = i; m < record->field_count; m++) {
                struct jg_field* member = &record->fields[m];
                if (member->union_id != field->union_id) {
                    continue;
                }
                jg_emit("%scase %lld:\n", indent, (long long)jg_union_constant(record, member)->value);
                jg_emit_write_field(ctx, member, inner);
                jg_emit("%sbreak;\n", inner);
            }
            jg_emit("%sdefault:\n%sbreak;\n%s}\n", indent, inner, indent);
            continue;
        }
        jg_emit_write_field(ctx, field, indent);
    }
}

static int32_t jg_user_field(struct jg_record* record) {
    for (uint32_t i = 0; i < record->field_count; i++) {
        if (record->fields[i].is_user) {
            return (int32_t)i;
        }
    }
    return -1;
}

static const char* jg_writer_suffix(struct jg_record* record) {
    char hook[jg_name + 32];
    snprintf(hook, sizeof(hook), "\nfunction js_w_%s(", record->name);
    return strstr(jg_hand_written_text(), hook) != NULL ? "_fields" : "";
}

static void jg_emit_writer(struct jg_record* record) {
    if (record->is_matrix) {
        jg_emit("function js_w_%s(o, v) {\n", record->name);
        jg_emit_matrix_store(record, "o", "v", "    ");
        jg_emit("}\n\n");
    } else {
        struct cr_str* out = jg_out;
        jg_out = cr_str_create_empty(jg_mem);
        struct jg_wctx ctx = { .record = record, .d = "d", .o = "o" };
        jg_emit_write_fields(record, &ctx, "    ");
        struct cr_str* fields = jg_out;
        jg_out = out;
        jg_emit("function js_w_%s%s(o, d) {\n    if (d == null) return;\n    let v;\n", record->name, jg_writer_suffix(record));
        if (ctx.has_cb) {
            jg_emit("    let cb = 0;\n");
        }
        if (record->type_field >= 0) {
            struct jg_field* type = &record->fields[record->type_field];
            jg_emit("    const t = js_enum(js_enums.%s, d.%s);\n", type->type->en->name, type->js);
        }
        cr_str_append(jg_out, fields);
        cr_str_destroy(fields);
        int32_t user = jg_user_field(record);
        if (user >= 0) {
            jg_emit(ctx.has_cb ? "    js_wp(o + %u, cb !== 0 ? cb : js_num(d.user));\n" : "    js_wp(o + %u, js_num(d.user));\n", record->fields[user].off[jg_abi]);
        }
        jg_emit("}\n\n");
    }
}

static bool jg_helper_used(const char* prefix, struct jg_record* record) {
    char name[jg_name + 32];
    snprintf(name, sizeof(name), "%s%s", prefix, record->name);
    return jg_has_identifier(cr_str_get_data(jg_out), name) || jg_has_identifier(jg_hand_written_text(), name);
}

static void jg_emit_helpers(struct jg_record* record) {
    uint32_t size = record->size[jg_abi];
    if (jg_helper_used("js_pone_", record)) {
        jg_emit("function js_pone_%s(d) {\n    const p = js_scratch(%u);\n    js_u8.fill(0, p, p + %u);\n    js_w_%s(p, d);\n    return js_base + p;\n}\n\n", record->name, size, size, record->name);
    }
    if (jg_helper_used("js_pa_", record)) {
        jg_emit("function js_pa_%s(a) {\n    if (a == null || a.length === 0) {\n        js_last_len = 0;\n        return 0;\n    }\n    const n = a.length;\n    const p = js_scratch(%u * n);\n    js_u8.fill(0, p, p + %u * n);\n    for (let i = 0; i < n; i++) js_w_%s(p + i * %u, a[i]);\n    js_last_len = n;\n    return js_base + p;\n}\n\n", record->name, size, size, record->name, size);
    }
    if (jg_helper_used("js_ra_", record)) {
        jg_emit("function js_ra_%s(o, n) {\n    const a = [];\n    for (let i = 0; i < n; i++) a.push(js_r_%s(o + i * %u));\n    return a;\n}\n\n", record->name, record->name, size);
    }
}

static void jg_emit_read_fields(struct jg_record* record, const char* o, const char* indent, bool* first);

static void jg_load_expr(struct jg_type* t, const char* off, struct jg_record* record, struct jg_field* field, const char* o, char* buf, size_t size) {
    char count[256];
    switch (t->cls) {
    case jg_cls_scalar:
        jg_scalar_load(jg_kind_of(t), off, buf, size);
        break;
    case jg_cls_enum:
        snprintf(buf, size, "js_enum_name(js_enum_names.%s, js_u32[(%s) >> 2])", t->en->name, off);
        break;
    case jg_cls_vector:
        jg_vector_load(t, off, buf, size);
        break;
    case jg_cls_record:
        if (t->record->is_handle) {
            snprintf(buf, size, "js_f64[(%s) >> 3]", off);
        } else if (t->record->is_wide) {
            snprintf(buf, size, "js_r64(%s)", off);
        } else if (t->record->is_matrix) {
            jg_matrix_load(t->record, off, buf, size);
        } else {
            assert(t->record->name[0] != 0);
            snprintf(buf, size, "js_r_%s(%s)", t->record->name, off);
        }
        break;
    case jg_cls_array: {
        uint32_t stride = jg_size(t->inner);
        if (jg_type_is_chars(t)) {
            snprintf(buf, size, "js_rchars(%s, %u)", off, t->count);
        } else if (t->inner->cls == jg_cls_array && jg_type_is_chars(t->inner)) {
            snprintf(buf, size, "js_ra_chars(%s, %u, %u, %u)", off, t->count, stride, t->inner->count);
        } else {
            snprintf(count, sizeof(count), "%u", t->count);
            jg_array_reader(t->inner, off, count, buf, size);
        }
    } break;
    case jg_cls_ptr: {
        struct jg_field* counter = field != NULL && field->count_field >= 0 ? &record->fields[field->count_field] : NULL;
        if (counter != NULL) {
            char coff[256];
            snprintf(coff, sizeof(coff), "%s + %u", o, counter->off[jg_abi]);
            jg_scalar_load(jg_kind_of(counter->type), coff, count, sizeof(count));
        }
        if (jg_type_is_cstr(t)) {
            if (counter != NULL) {
                snprintf(buf, size, "js_host_str_n(js_rp(%s), %s)", off, count);
            } else {
                snprintf(buf, size, "js_host_str(js_rp(%s))", off);
            }
        } else if (jg_type_is_bytes(t) && counter != NULL) {
            snprintf(buf, size, "js_host_read(js_rp(%s), %s)", off, count);
        } else if (counter != NULL && t->inner->cls != jg_cls_fn && t->inner->cls != jg_cls_void) {
            char copy[512];
            snprintf(copy, sizeof(copy), "js_host_copy(js_rp(%s), (%s) * %u)", off, count, jg_size(t->inner));
            jg_array_reader(t->inner, copy, count, buf, size);
        } else {
            snprintf(buf, size, "js_rp(%s)", off);
        }
    } break;
    default:
        jg_fatal("%s: unsupported field type in a reader", record->name);
    }
}

static void jg_emit_read_field(struct jg_record* record, struct jg_field* field, const char* o, const char* indent) {
    char off[256];
    char buf[1024];
    snprintf(off, sizeof(off), "%s + %u", o, field->off[jg_abi]);
    struct jg_type* t = field->type;
    if (t->cls == jg_cls_record && t->record->name[0] == 0) {
        jg_emit("%s%s: {\n", indent, field->js);
        char inner[64];
        snprintf(inner, sizeof(inner), "%s    ", indent);
        bool inner_first = true;
        jg_emit_read_fields(t->record, off, inner, &inner_first);
        jg_emit("\n%s}", indent);
    } else {
        jg_load_expr(t, off, record, field, o, buf, sizeof(buf));
        jg_emit("%s%s: %s", indent, field->js, buf);
    }
}

static void jg_emit_read_fields(struct jg_record* record, const char* o, const char* indent, bool* first) {
    for (uint32_t i = 0; i < record->field_count; i++) {
        struct jg_field* field = &record->fields[i];
        if (jg_field_is_hidden(field) || (field->union_id != 0 && jg_union_discriminated(record, field->union_id))) {
            continue;
        }
        jg_emit("%s\n", *first ? "" : ",");
        *first = false;
        jg_emit_read_field(record, field, o, indent);
    }
}

static void jg_emit_reader(struct jg_record* record) {
    uint32_t size = record->size[jg_abi];
    if (record->is_matrix) {
        char buf[1024];
        jg_emit("function js_r_%s(o) {\n    return %s;\n}\n\n", record->name, jg_matrix_load(record, "o", buf, sizeof(buf)));
    } else {
        bool first = true;
        bool variants = false;
        for (uint32_t i = 0; i < record->field_count; i++) {
            variants = variants || (record->fields[i].union_id != 0 && jg_union_discriminated(record, record->fields[i].union_id));
        }
        jg_emit("function js_r_%s(o) {\n    %s{", record->name, variants ? "const r = " : "return ");
        jg_emit_read_fields(record, "o", "        ", &first);
        jg_emit("%s    };\n", first ? " " : "\n");
        if (variants) {
            struct jg_field* type = &record->fields[record->type_field];
            jg_emit("    switch (js_u32[(o + %u) >> 2]) {\n", type->off[jg_abi]);
            for (uint32_t i = 0; i < record->field_count; i++) {
                struct jg_field* field = &record->fields[i];
                if (field->union_id == 0 || jg_union_discriminated(record, field->union_id) == false) {
                    continue;
                }
                jg_emit("    case %lld:\n        r.%s = ", (long long)jg_union_constant(record, field)->value, field->js);
                char off[256];
                snprintf(off, sizeof(off), "o + %u", field->off[jg_abi]);
                if (field->type->cls == jg_cls_record && field->type->record->name[0] == 0) {
                    bool inner_first = true;
                    jg_emit("{");
                    jg_emit_read_fields(field->type->record, off, "            ", &inner_first);
                    jg_emit("%s        };\n", inner_first ? " " : "\n");
                } else {
                    char buf[1024];
                    jg_load_expr(field->type, off, record, field, "o", buf, sizeof(buf));
                    jg_emit("%s;\n", buf);
                }
                jg_emit("        break;\n");
            }
            jg_emit("    default:\n        break;\n    }\n    return r;\n");
        }
        jg_emit("}\n\n");
    }
}

static bool jg_record_available(struct jg_record* record) {
    return record->is_public && (record->platforms & jg_abi_platforms(jg_abi)) != 0 && record->is_handle == false && record->is_wide == false;
}

struct jg_call {
    struct jg_func* func;
    char args[32][512];
    uint32_t arg_count;
    char out_names[jg_max_params][jg_name];
    char out_exprs[jg_max_params][512];
    uint32_t out_count;
    struct cr_str* post;
    bool capacity;
    bool scratch;
};

__attribute__((format(printf, 2, 3)))
static void jg_call_arg(struct jg_call* call, const char* fmt, ...) {
    assert(call->arg_count < cr_count(call->args));
    va_list args;
    va_start(args, fmt);
    vsnprintf(call->args[call->arg_count++], sizeof(call->args[0]), fmt, args);
    va_end(args);
}

__attribute__((format(printf, 3, 4)))
static void jg_call_out(struct jg_call* call, struct jg_param* param, const char* fmt, ...) {
    const char* base = jg_starts_with(param->name, "out_") ? param->name + 4 : param->name;
    jg_camel(base, call->out_names[call->out_count], sizeof(call->out_names[0]), false);
    va_list args;
    va_start(args, fmt);
    vsnprintf(call->out_exprs[call->out_count++], sizeof(call->out_exprs[0]), fmt, args);
    va_end(args);
}

static void jg_value_load(struct jg_type* t, const char* base, char* buf, size_t size) {
    if (t->cls == jg_cls_vector) {
        jg_vector_load(t, base, buf, size);
    } else if (t->cls == jg_cls_enum) {
        snprintf(buf, size, "js_enum_name(js_enum_names.%s, js_u32[%s >> 2])", t->en->name, base);
    } else if (t->cls == jg_cls_scalar) {
        jg_scalar_load(jg_kind_of(t), base, buf, size);
    } else if (t->cls == jg_cls_record && t->record->is_handle) {
        snprintf(buf, size, "js_f64[%s >> 3]", base);
    } else if (t->cls == jg_cls_record && t->record->is_wide) {
        snprintf(buf, size, "js_r64(%s)", base);
    } else if (t->cls == jg_cls_record) {
        snprintf(buf, size, "js_r_%s(%s)", t->record->name, base);
    } else {
        assert(0 && "value load");
    }
}

static void jg_emit_wrapper_param(struct jg_call* call, uint32_t p) {
    struct jg_func* func = call->func;
    struct jg_param* param = &func->params[p];
    struct jg_type* t = param->type;
    const char* js = param->js;
    char buf[1024];
    switch (param->role) {
    case jg_role_mem:
    case jg_role_user:
        break;
    case jg_role_ptr:
    case jg_role_capacity:
        jg_call_arg(call, "(%s == null ? 0 : %s)", js, js);
        call->capacity = call->capacity || param->role == jg_role_capacity;
        break;
    case jg_role_value:
        if (t->cls == jg_cls_scalar && t->kind == jg_kind_bool) {
            jg_call_arg(call, "%s ? 1 : 0", js);
        } else if (t->cls == jg_cls_enum) {
            jg_call_arg(call, "js_enum(js_enums.%s, %s)", t->en->name, js);
        } else if (t->cls == jg_cls_vector) {
            for (uint32_t c = 0; c < t->count; c++) {
                jg_call_arg(call, "%s[%u]", js, c);
            }
        } else if (jg_type_is_struct(t) || (t->cls == jg_cls_record && t->record->is_matrix)) {
            jg_emit("    const a%u = js_pone_%s(%s);\n", p, t->record->name, js);
            jg_call_arg(call, "a%u", p);
            call->scratch = true;
        } else {
            jg_call_arg(call, "(%s == null ? 0 : %s)", js, js);
        }
        break;
    case jg_role_string:
    case jg_role_bytes: {
        const char* pack = param->role == jg_role_string ? "js_pstr" : "js_pbytes";
        if (param->partner >= 0) {
            jg_emit("    const a%u = %s(%s);\n    const n%u = js_last_len;\n", p, pack, js, p);
        } else {
            jg_emit("    const a%u = typeof %s === 'number' ? %s : %s(%s);\n", p, js, js, pack, js);
        }
        jg_call_arg(call, "a%u", p);
        call->scratch = true;
    } break;
    case jg_role_count: {
        int32_t owner = -1;
        for (uint32_t q = 0; owner < 0 && q < func->param_count; q++) {
            if (func->params[q].partner == (int32_t)p) {
                owner = (int32_t)q;
            }
        }
        assert(owner >= 0);
        jg_call_arg(call, "n%d", owner);
    } break;
    case jg_role_struct_in:
        jg_emit("    const a%u = typeof %s === 'number' ? %s : js_pone_%s(%s);\n", p, js, js, t->inner->record->name, js);
        jg_call_arg(call, "a%u", p);
        call->scratch = true;
        break;
    case jg_role_array_in: {
        char writer[256];
        jg_array_writer(t->inner, writer, sizeof(writer));
        snprintf(buf, sizeof(buf), writer, js);
        jg_emit("    const a%u = %s;\n    const n%u = js_last_len;\n", p, buf, p);
        jg_call_arg(call, "a%u", p);
        call->scratch = true;
    } break;
    case jg_role_fn:
        if (jg_model.sites[param->site].scope == jg_scope_replace) {
            jg_call_arg(call, "js_cb_replace(%s, %s)", func->params[0].js, js);
        } else {
            jg_call_arg(call, "js_cb_call(%s)", js);
        }
        break;
    case jg_role_out: {
        uint32_t size = jg_size(t->inner);
        jg_emit("    const o%u = js_scratch(%u);\n", p, size);
        if (t->inner->cls == jg_cls_record || t->inner->cls == jg_cls_vector) {
            jg_emit("    js_u8.fill(0, o%u, o%u + %u);\n", p, p, size);
        }
        jg_call_arg(call, "js_base + o%u", p);
        snprintf(buf, sizeof(buf), "o%u", p);
        char load[1024];
        jg_value_load(t->inner, buf, load, sizeof(load));
        jg_call_out(call, param, "%s", load);
        call->scratch = true;
    } break;
    case jg_role_out_array: {
        const char* capacity = func->params[param->partner].js;
        jg_emit("    const o%u = js_scratch(%u * (%s == null ? 0 : %s));\n", p, jg_size(t->inner), capacity, capacity);
        jg_call_arg(call, "js_base + o%u", p);
        snprintf(buf, sizeof(buf), "o%u", p);
        char reader[512];
        char count[256];
        snprintf(count, sizeof(count), "Math.min(r, %s == null ? 0 : %s)", capacity, capacity);
        jg_call_out(call, param, "%s", jg_array_reader(t->inner, buf, count, reader, sizeof(reader)));
        call->scratch = true;
    } break;
    case jg_role_out_ptr_array:
    case jg_role_out_string: {
        struct jg_param* counter = &func->params[param->partner];
        jg_emit("    const o%u = js_scratch(%u);\n    const o%u = js_scratch(%u);\n", p, jg_abi == jg_abi_wasm ? 4 : 8, param->partner, jg_size(counter->type->inner));
        jg_call_arg(call, "js_base + o%u", p);
        char count[256];
        snprintf(buf, sizeof(buf), "o%u", param->partner);
        jg_scalar_load(jg_kind_of(counter->type->inner), buf, count, sizeof(count));
        if (param->role == jg_role_out_string) {
            jg_call_out(call, param, "js_host_str_n(js_rp(o%u), %s)", p, count);
        } else {
            char copy[512];
            char reader[512];
            snprintf(copy, sizeof(copy), "js_host_copy(js_rp(o%u), (%s) * %u)", p, count, jg_size(t->inner->inner));
            jg_call_out(call, param, "%s", jg_array_reader(t->inner->inner, copy, count, reader, sizeof(reader)));
        }
        call->scratch = true;
    } break;
    case jg_role_out_count:
    case jg_role_out_size:
        jg_call_arg(call, "js_base + o%u", p);
        break;
    case jg_role_inout:
        jg_emit("    const a%u = js_pone_%s(%s);\n", p, t->inner->record->name, js);
        jg_call_arg(call, "a%u", p);
        cr_str_append_format(call->post, "    Object.assign(%s, js_r_%s(a%u - js_base));\n", js, t->inner->record->name, p);
        call->scratch = true;
        break;
    default:
        jg_fatal("%s: unhandled role for %s", func->name, param->name);
    }
}

static void jg_wrapper_result(struct jg_call* call, char* buf, size_t size) {
    struct jg_func* func = call->func;
    struct jg_type* t = func->ret;
    char outs[2048] = "";
    if (call->out_count == 1) {
        snprintf(outs, sizeof(outs), "%s", call->out_exprs[0]);
    } else if (call->out_count > 1) {
        size_t len = (size_t)snprintf(outs, sizeof(outs), "{ ");
        for (uint32_t i = 0; i < call->out_count; i++) {
            len += (size_t)snprintf(outs + len, sizeof(outs) - len, "%s%s: %s", i > 0 ? ", " : "", call->out_names[i], call->out_exprs[i]);
        }
        snprintf(outs + len, sizeof(outs) - len, " }");
    }
    if (t->cls == jg_cls_void) {
        snprintf(buf, size, "%s", call->out_count > 0 ? outs : "undefined");
    } else if (t->cls == jg_cls_scalar && t->kind == jg_kind_bool) {
        snprintf(buf, size, call->out_count > 0 ? "r !== 0 ? %s : null" : "r !== 0", outs);
    } else if (t->cls == jg_cls_scalar && call->capacity && call->out_count == 1) {
        snprintf(buf, size, "%s", outs);
    } else if (t->cls == jg_cls_scalar && call->out_count > 0) {
        snprintf(buf, size, "{ result: r, %s", outs + 2);
    } else if (t->cls == jg_cls_enum) {
        snprintf(buf, size, "js_enum_name(js_enum_names.%s, r)", t->en->name);
    } else if (jg_type_is_cstr(t)) {
        snprintf(buf, size, "js_host_str(r)");
    } else if (jg_func_returns_out(func)) {
        jg_value_load(t, "or", buf, size);
    } else {
        snprintf(buf, size, "r");
    }
}

static void jg_emit_wrapper(struct jg_func* func) {
    bool ret_out = jg_func_returns_out(func);
    bool call_sites = false;
    const char* owner = NULL;
    struct cr_str* params = cr_str_create_empty(jg_mem);
    for (uint32_t p = 0; p < func->param_count; p++) {
        struct jg_param* param = &func->params[p];
        if (param->role == jg_role_struct_in && jg_record_has_scope(param->type->inner->record, jg_scope_owner)) {
            if (func->ret->cls != jg_cls_ptr) {
                jg_fatal("%s: owner scoped callbacks need the function to return the owner", func->name);
            }
            owner = param->js;
        }
        if (jg_role_is_hidden(param->role) == false) {
            cr_str_append_format(params, "%s%s", cr_str_get_count(params) > 0 ? ", " : "", param->js);
        }
        if (param->role == jg_role_fn) {
            call_sites = call_sites || jg_model.sites[param->site].scope == jg_scope_call;
        }
        if (param->role == jg_role_struct_in || param->role == jg_role_value || param->role == jg_role_array_in || param->role == jg_role_inout) {
            struct jg_type* t = param->type->cls == jg_cls_ptr ? param->type->inner : param->type;
            call_sites = call_sites || (t->cls == jg_cls_record && jg_record_has_scope(t->record, jg_scope_call));
        }
    }
    jg_emit("function js_f_%s(%s) {\n", func->name, cr_str_get_data(params));
    struct cr_str* body = jg_out;
    jg_out = cr_str_create_empty(jg_mem);
    struct jg_call call = { .func = func, .post = cr_str_create_empty(jg_mem) };
    for (uint32_t p = 0; p < func->param_count; p++) {
        jg_emit_wrapper_param(&call, p);
    }
    if (ret_out) {
        jg_emit("    const or = js_scratch(%u);\n", jg_size(func->ret));
        jg_call_arg(&call, "js_base + or");
    }
    struct cr_str* prep = jg_out;
    jg_out = body;
    if (call.scratch) {
        jg_emit("    const m = js_scratch_cursor;\n");
    }
    if (call_sites) {
        jg_emit("    const cm = js_cb_calls.length;\n");
    }
    if (func->adopts) {
        jg_emit("    const pm = js_cb_pending.length;\n");
    }
    cr_str_append(jg_out, prep);
    cr_str_destroy(prep);
    const char* assign = func->ret->cls == jg_cls_void || ret_out ? "" : "const r = ";
    if (owner != NULL) {
        jg_emit("    const po = js_cb_owner_pending;\n    js_cb_owner_pending = %s;\n", owner);
    }
    if (jg_abi == jg_abi_native) {
        for (uint32_t i = 0; i < call.arg_count; i++) {
            jg_emit("    js_f64[%u] = %s;\n", i, call.args[i]);
        }
        jg_emit("    %sjs_x.%s();\n", assign, func->name);
    } else {
        struct cr_str* args = cr_str_create_empty(jg_mem);
        for (uint32_t i = 0; i < call.arg_count; i++) {
            cr_str_append_format(args, "%s%s", i > 0 ? ", " : "", call.args[i]);
        }
        jg_emit("    %sjs_x.%s(%s);\n", assign, func->name, cr_str_get_data(args));
        cr_str_destroy(args);
    }
    if (call_sites) {
        jg_emit("    js_cb_call_release(cm);\n");
    }
    if (func->adopts) {
        jg_emit("    js_cb_adopt(pm, js_f64[js_root_slot]);\n");
    }
    if (owner != NULL) {
        jg_emit("    js_cb_owner_pending = po;\n    js_cb_own(r, %s);\n", owner);
    }
    cr_str_append(jg_out, call.post);
    char result[2048];
    jg_wrapper_result(&call, result, sizeof(result));
    if (call.scratch || ret_out) {
        jg_emit("    const result = %s;\n    js_scratch_cursor = %s;\n    return result;\n}\n", result, call.scratch ? "m" : "or");
    } else {
        jg_emit("    return %s;\n}\n", result);
    }
    if ((func->platforms & jg_abi_platforms(jg_abi)) == jg_abi_platforms(jg_abi)) {
        jg_emit("%s.%s = js_f_%s;\n\n", func->ns, func->js, func->name);
    } else {
        cr_str_append_format(jg_installs, "    if (js_x.%s !== undefined) %s.%s = js_f_%s;\n", func->name, func->ns, func->js, func->name);
        jg_emit("\n");
    }
    cr_str_destroy(params);
    cr_str_destroy(call.post);
}

static void jg_emit_dispatch_arg(struct jg_type* fn, uint32_t i, uint32_t user_index, uint32_t* n, struct cr_str* args, struct cr_str* post) {
    struct jg_type* t = fn->params[i];
    struct jg_record* pointee = t->cls == jg_cls_ptr && t->inner->cls == jg_cls_record ? t->inner->record : NULL;
    uint32_t a = *n;
    char expr[512];
    char buf[1024];
    if (t->cls == jg_cls_scalar) {
        snprintf(expr, sizeof(expr), t->kind == jg_kind_bool ? "a%u !== 0" : "a%u", a);
    } else if (t->cls == jg_cls_enum) {
        snprintf(expr, sizeof(expr), "js_enum_name(js_enum_names.%s, a%u)", t->en->name, a);
    } else if (t->cls == jg_cls_vector) {
        size_t len = (size_t)snprintf(expr, sizeof(expr), "[");
        for (uint32_t c = 0; c < t->count; c++) {
            len += (size_t)snprintf(expr + len, sizeof(expr) - len, "%sa%u", c > 0 ? ", " : "", a + c);
        }
        snprintf(expr + len, sizeof(expr) - len, "]");
    } else if (t->cls == jg_cls_record && (t->record->is_handle || t->record->is_wide)) {
        snprintf(expr, sizeof(expr), "a%u", a);
    } else if (t->cls == jg_cls_record) {
        snprintf(expr, sizeof(expr), "js_r_%s(a%u - js_base)", t->record->name, a);
    } else if (pointee != NULL && pointee->is_public && pointee->is_object == false && i + 1 < user_index && jg_type_is_integer(fn->params[i + 1])) {
        char off[64];
        char count[64];
        snprintf(off, sizeof(off), "a%u - js_base", a);
        snprintf(count, sizeof(count), "a%u", a + 1);
        jg_emit("        const w%u = a%u === 0 ? [] : %s;\n", i, a, jg_array_reader(t->inner, off, count, buf, sizeof(buf)));
        snprintf(expr, sizeof(expr), "w%u", i);
        if (t->inner->is_const == false) {
            cr_str_append_format(post, "        for (let k = 0; k < w%u.length; k++) js_w_%s(a%u - js_base + k * %u, w%u[k]);\n", i, pointee->name, a, jg_size(t->inner), i);
        }
    } else if (pointee != NULL && pointee->is_public && pointee->is_object == false) {
        jg_emit("        const w%u = a%u === 0 ? null : js_r_%s(a%u - js_base);\n", i, a, pointee->name, a);
        snprintf(expr, sizeof(expr), "w%u", i);
        if (t->inner->is_const == false) {
            cr_str_append_format(post, "        if (r !== undefined && r !== null) js_w_%s(a%u - js_base, r); else if (w%u !== null) js_w_%s(a%u - js_base, w%u);\n", pointee->name, a, i, pointee->name, a, i);
        }
    } else if (t->cls == jg_cls_ptr) {
        snprintf(expr, sizeof(expr), "a%u", a);
    } else {
        jg_fatal("unsupported callback parameter");
    }
    *n += t->cls == jg_cls_vector ? t->count : 1;
    cr_str_append_format(args, "%s%s", i > 0 ? ", " : "", expr);
}

static void jg_emit_dispatch_body(struct jg_site* site) {
    struct jg_type* fn = site->fn;
    if (site->is_param) {
        jg_emit("        const f = js_cb_get(id);\n        if (f === null) return 0;\n");
    } else if (site->scope == jg_scope_owner) {
        jg_emit("        const h = js_cb_owner(a0);\n        const f = h === null ? undefined : h.%s;\n        if (typeof f !== 'function') return 0;\n", site->js);
    } else {
        jg_emit("        const h = js_cb_get(id);\n        const f = h === null ? undefined : h.%s;\n        if (typeof f !== 'function') return 0;\n", site->js);
    }
    uint32_t user_index = site->generic ? fn->param_count - 1 : fn->param_count;
    uint32_t n = 0;
    struct cr_str* args = cr_str_create_empty(jg_mem);
    struct cr_str* post = cr_str_create_empty(jg_mem);
    for (uint32_t i = 0; i < user_index; i++) {
        jg_emit_dispatch_arg(fn, i, user_index, &n, args, post);
    }
    jg_emit("        const r = f.call(%s%s%s);\n", site->is_param ? "undefined" : "h", cr_str_get_count(args) > 0 ? ", " : "", cr_str_get_data(args));
    cr_str_append(jg_out, post);
    struct jg_type* ret = fn->inner;
    if (ret->cls == jg_cls_vector) {
        char base[64];
        snprintf(base, sizeof(base), "a%u - js_base", n);
        jg_emit("        if (r != null) {\n");
        jg_emit_vector_store(ret, base, "r", "            ");
        jg_emit("        }\n        return 0;\n");
    } else if (jg_type_is_struct(ret)) {
        jg_emit("        if (r != null) js_w_%s(a%u - js_base, r);\n        return 0;\n", ret->record->name, n);
    } else if (ret->cls == jg_cls_void) {
        jg_emit("        js_observe(r);\n        return 0;\n");
    } else if (ret->cls == jg_cls_scalar && ret->kind == jg_kind_bool) {
        jg_emit("        return r ? 1 : 0;\n");
    } else if (ret->cls == jg_cls_enum) {
        jg_emit("        return js_enum(js_enums.%s, r);\n", ret->en->name);
    } else {
        jg_emit("        return js_num(r);\n");
    }
    cr_str_destroy(args);
    cr_str_destroy(post);
}

static void jg_emit_dispatch(void) {
    struct cr_str* bodies[jg_max_sites];
    uint32_t body_count = 0;
    uint32_t sites[jg_max_sites];
    uint32_t groups[jg_max_sites];
    uint32_t site_count = 0;
    struct cr_str* out = jg_out;
    for (uint32_t s = 0; s < jg_model.site_count; s++) {
        struct jg_site* site = &jg_model.sites[s];
        if ((site->platforms & jg_abi_platforms(jg_abi)) == 0) {
            continue;
        }
        jg_out = cr_str_create_empty(jg_mem);
        jg_emit_dispatch_body(site);
        uint32_t group = body_count;
        for (uint32_t b = 0; b < body_count; b++) {
            if (strcmp(cr_str_get_data(bodies[b]), cr_str_get_data(jg_out)) == 0) {
                group = b;
            }
        }
        if (group == body_count) {
            bodies[body_count++] = jg_out;
        } else {
            cr_str_destroy(jg_out);
        }
        sites[site_count] = s;
        groups[site_count] = group;
        site_count++;
    }
    jg_out = out;
    jg_emit("function js_dispatch_gen(kind, id, a0, a1, a2, a3, a4, a5) {\n    switch (kind) {\n");
    for (uint32_t b = 0; b < body_count; b++) {
        for (uint32_t i = 0; i < site_count; i++) {
            if (groups[i] == b) {
                jg_emit("    case %u:\n", sites[i]);
            }
        }
        jg_emit("    {\n");
        cr_str_append(jg_out, bodies[b]);
        jg_emit("    }\n");
        cr_str_destroy(bodies[b]);
    }
    jg_emit("    default:\n        js_assert(false, 'unknown callback kind ' + kind);\n    }\n}\n\n");
}

static bool jg_enum_available(struct jg_enum* en) {
    return en->is_public && (en->platforms & jg_abi_platforms(jg_abi)) != 0;
}

void jg_emit_js(enum jg_abi abi, const char* path) {
    jg_abi = abi;
    jg_out = cr_str_create(jg_mem, "'use strict';\n\n");
    jg_installs = cr_str_create_empty(jg_mem);
    jg_emit("const js_ptr_size = %u;\n\n", abi == jg_abi_wasm ? 4 : 8);
    if (abi == jg_abi_wasm) {
        jg_emit("function js_wp(o, p) {\n    js_u32[o >> 2] = p == null ? 0 : p;\n}\n\nfunction js_rp(o) {\n    return js_u32[o >> 2];\n}\n\n");
    } else {
        jg_emit("function js_wp(o, p) {\n    const v = p == null ? 0 : p;\n    js_u32[o >> 2] = v >>> 0;\n    js_u32[(o >> 2) + 1] = (v / 4294967296) >>> 0;\n}\n\nfunction js_rp(o) {\n    return js_u32[o >> 2] + js_u32[(o >> 2) + 1] * 4294967296;\n}\n\n");
    }

    for (uint32_t i = 0; i < jg_model.enum_count; i++) {
        struct jg_enum* en = &jg_model.enums[i];
        if (jg_starts_with(en->name, "js_") == false || en->is_public == false) {
            continue;
        }
        jg_emit("const %s = {", en->name);
        for (uint32_t c = 0; c < en->constant_count; c++) {
            jg_emit("%s %s: %lld", c ? "," : "", en->constants[c].js, (long long)en->constants[c].value);
        }
        jg_emit(" };\n\n");
    }

    jg_emit("const js_enums = {\n");
    for (uint32_t i = 0; i < jg_model.enum_count; i++) {
        struct jg_enum* en = &jg_model.enums[i];
        if (en->is_flag || jg_starts_with(en->name, "js_") || jg_enum_available(en) == false) {
            continue;
        }
        jg_emit("    %s: {", en->name);
        for (uint32_t c = 0; c < en->constant_count; c++) {
            jg_emit("%s %s: %lld", c ? "," : "", en->constants[c].js, (long long)en->constants[c].value);
        }
        jg_emit(" },\n");
    }
    jg_emit("};\n\nconst js_enum_names = {\n");
    for (uint32_t i = 0; i < jg_model.enum_count; i++) {
        struct jg_enum* en = &jg_model.enums[i];
        if (en->is_flag || jg_starts_with(en->name, "js_") || jg_enum_available(en) == false) {
            continue;
        }
        jg_emit("    %s: {", en->name);
        for (uint32_t c = 0; c < en->constant_count; c++) {
            jg_emit("%s %lld: '%s'", c ? "," : "", (long long)en->constants[c].value, en->constants[c].js);
        }
        jg_emit(" },\n");
    }
    jg_emit("};\n\n");

    for (uint32_t i = 0; i < jg_model.record_count; i++) {
        struct jg_record* record = &jg_model.records[i];
        if (jg_record_available(record) && strcmp(record->name, "ui_key") != 0) {
            jg_emit_writer(record);
            jg_emit_reader(record);
        }
    }

    for (uint32_t i = 0; i < jg_model.func_count; i++) {
        struct jg_func* func = &jg_model.funcs[i];
        if (jg_func_available(func, abi) && strcmp(func->ns, "js") != 0) {
            jg_emit_wrapper(func);
        }
    }

    for (uint32_t i = 0; i < jg_model.var_count; i++) {
        struct jg_var* var = &jg_model.vars[i];
        if ((var->platforms & jg_abi_platforms(abi)) == 0) {
            continue;
        }
        if (var->type->cls == jg_cls_ptr) {
            jg_emit("Object.defineProperty(%s, '%s', { enumerable: true, configurable: true, get() { return js_x.%s_get(); }, set(v) { js_x.%s_set(js_num(v)); } });\n\n", var->ns, var->js, var->name, var->name);
        } else {
            jg_emit("Object.defineProperty(%s, '%s', { enumerable: true, get() { return js_x.%s_get(); } });\n\n", var->ns, var->js, var->name);
        }
    }

    for (uint32_t i = 0; i < jg_model.enum_count; i++) {
        struct jg_enum* en = &jg_model.enums[i];
        if (en->is_flag == false || jg_enum_available(en) == false) {
            continue;
        }
        for (uint32_t c = 0; c < en->constant_count; c++) {
            jg_emit("%s.%s = %lld;\n", en->ns, en->constants[c].js, (long long)en->constants[c].value);
        }
        jg_emit("\n");
    }

    for (uint32_t i = 0; i < jg_model.macro_count; i++) {
        struct jg_macro* macro = &jg_model.macros[i];
        if (macro->is_number && (macro->platforms & jg_abi_platforms(abi)) != 0) {
            jg_emit("%s.%s = %.17g;\n", macro->ns, macro->js, macro->value);
        }
    }

    jg_emit("\nconst js_structs = {\n");
    for (uint32_t i = 0; i < jg_model.record_count; i++) {
        struct jg_record* record = &jg_model.records[i];
        if (jg_record_available(record)) {
            jg_emit("    %s: [js_w_%s, js_r_%s, %u],\n", record->name, record->name, record->name, record->size[abi]);
        }
    }
    jg_emit("};\n\nconst js_site_count = %u;\n\n", jg_model.site_count);
    jg_emit_dispatch();
    jg_emit("function js_install_platform() {\n");
    cr_str_append(jg_out, jg_installs);
    jg_emit("}\n\n");

    for (uint32_t i = 0; i < jg_model.record_count; i++) {
        struct jg_record* record = &jg_model.records[i];
        if (jg_record_available(record) && strcmp(record->name, "ui_key") != 0) {
            jg_emit_helpers(record);
        }
    }

    jg_write_output(path, jg_out);
    cr_str_destroy(jg_installs);
    cr_str_destroy(jg_out);
    jg_out = NULL;
}
