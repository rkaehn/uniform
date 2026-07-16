#include "jg.h"

#define jg_max_args 256

static struct jg_type jg_types[jg_max_types];
static uint32_t jg_type_count;

static struct {
    char** args;
    uint32_t count;
} jg_args[jg_abi_count];

static CXIndex jg_index;
static enum jg_abi jg_abi;
static uint32_t jg_union_next = 1;

struct jg_layout_ctx {
    struct jg_record* record;
    uint32_t base;
    uint32_t union_id;
    uint32_t index;
};

struct jg_names_ctx {
    const char** names;
    uint32_t count;
};

static struct jg_type* jg_type_new(enum jg_cls cls) {
    assert(jg_type_count < jg_max_types);
    struct jg_type* t = &jg_types[jg_type_count++];
    memset(t, 0, sizeof(*t));
    t->cls = cls;
    return t;
}

static const char* jg_cx(CXString string) {
    const char* text = clang_getCString(string);
    const char* copy = jg_strdup(text != NULL ? text : "");
    clang_disposeString(string);
    return copy;
}

static const char* jg_cursor_file(CXCursor cursor) {
    CXFile file = NULL;
    clang_getExpansionLocation(clang_getCursorLocation(cursor), &file, NULL, NULL, NULL);
    return file != NULL ? jg_base_name(jg_cx(clang_getFileName(file))) : "";
}

static bool jg_scalar_kind(enum CXTypeKind kind, enum jg_kind* out) {
    switch (kind) {
    case CXType_Bool: *out = jg_kind_bool; return true;
    case CXType_Char_S: case CXType_SChar: *out = jg_kind_i8; return true;
    case CXType_Char_U: case CXType_UChar: *out = jg_kind_u8; return true;
    case CXType_Short: *out = jg_kind_i16; return true;
    case CXType_UShort: *out = jg_kind_u16; return true;
    case CXType_Int: *out = jg_kind_i32; return true;
    case CXType_UInt: *out = jg_kind_u32; return true;
    case CXType_Long: *out = jg_kind_isize; return true;
    case CXType_ULong: *out = jg_kind_usize; return true;
    case CXType_LongLong: *out = jg_kind_i64; return true;
    case CXType_ULongLong: *out = jg_kind_u64; return true;
    case CXType_Float: *out = jg_kind_f32; return true;
    case CXType_Double: *out = jg_kind_f64; return true;
    default: return false;
    }
}

static bool jg_cursor_is_anonymous(CXCursor decl, const char* name) {
    return clang_Cursor_isAnonymous(decl) || name[0] == 0 || strstr(name, "(unnamed") != NULL || strstr(name, "(anonymous") != NULL;
}

static struct jg_record* jg_record_get(CXType t);
static struct jg_enum* jg_enum_get(CXType t);
static void jg_record_layout(struct jg_record* record, CXType t);

static struct jg_type* jg_type_from(CXType declared) {
    CXType t = clang_getCanonicalType(declared);
    enum jg_kind kind;
    struct jg_type* type;
    if (t.kind == CXType_Void) {
        type = jg_type_new(jg_cls_void);
    } else if (jg_scalar_kind(t.kind, &kind)) {
        type = jg_type_new(jg_cls_scalar);
        type->kind = kind;
    } else if (t.kind == CXType_ExtVector) {
        type = jg_type_new(jg_cls_vector);
        if (jg_scalar_kind(clang_getCanonicalType(clang_getElementType(t)).kind, &type->kind) == false) {
            jg_fatal("unsupported vector element in %s", jg_cx(clang_getTypeSpelling(declared)));
        }
        type->count = (uint32_t)clang_getNumElements(t);
    } else if (t.kind == CXType_ConstantArray) {
        type = jg_type_new(jg_cls_array);
        type->inner = jg_type_from(clang_getArrayElementType(t));
        type->count = (uint32_t)clang_getNumElements(t);
    } else if (t.kind == CXType_Pointer) {
        type = jg_type_new(jg_cls_ptr);
        type->inner = jg_type_from(clang_getPointeeType(t));
    } else if (t.kind == CXType_FunctionProto || t.kind == CXType_FunctionNoProto) {
        type = jg_type_new(jg_cls_fn);
        type->inner = jg_type_from(clang_getResultType(t));
        int count = clang_getNumArgTypes(t);
        assert(count >= 0 && count <= jg_max_params);
        for (int i = 0; i < count; i++) {
            type->params[type->param_count++] = jg_type_from(clang_getArgType(t, (unsigned)i));
        }
        type->variadic = clang_isFunctionTypeVariadic(t);
    } else if (t.kind == CXType_Record) {
        type = jg_type_new(jg_cls_record);
        type->record = jg_record_get(t);
    } else if (t.kind == CXType_Enum) {
        type = jg_type_new(jg_cls_enum);
        type->en = jg_enum_get(t);
    } else {
        jg_fatal("unsupported type %s", jg_cx(clang_getTypeSpelling(declared)));
    }
    type->is_const = clang_isConstQualifiedType(declared) || clang_isConstQualifiedType(t);
    return type;
}

static enum CXChildVisitResult jg_visit_param_name(CXCursor cursor, CXCursor parent, CXClientData data) {
    struct jg_names_ctx* ctx = data;
    if (clang_getCursorKind(cursor) == CXCursor_ParmDecl) {
        assert(ctx->count < jg_max_params);
        ctx->names[ctx->count++] = jg_cx(clang_getCursorSpelling(cursor));
    }
    return CXChildVisit_Continue;
}

static void jg_param_names(CXCursor decl, CXType declared, const char** names) {
    struct jg_names_ctx ctx = { .names = names };
    clang_visitChildren(decl, jg_visit_param_name, &ctx);
    if (ctx.count > 0) {
        return;
    }
    CXType t = declared;
    while (t.kind == CXType_Pointer || t.kind == CXType_Elaborated) {
        t = t.kind == CXType_Pointer ? clang_getPointeeType(t) : clang_Type_getNamedType(t);
    }
    if (t.kind == CXType_Typedef) {
        clang_visitChildren(clang_getTypeDeclaration(t), jg_visit_param_name, &ctx);
    }
}

static enum CXVisitorResult jg_visit_field(CXCursor cursor, CXClientData data) {
    struct jg_layout_ctx* ctx = data;
    struct jg_record* record = ctx->record;
    CXType declared = clang_getCursorType(cursor);
    CXType t = clang_getCanonicalType(declared);
    const char* name = jg_cx(clang_getCursorSpelling(cursor));
    long long bits = clang_Cursor_getOffsetOfField(cursor);
    assert(bits >= 0 && "field offset");
    uint32_t off = ctx->base + (uint32_t)(bits / 8);

    if (t.kind == CXType_Record && clang_Cursor_isAnonymousRecordDecl(clang_getTypeDeclaration(t))) {
        struct jg_layout_ctx sub = *ctx;
        sub.base = off;
        if (clang_getCursorKind(clang_getTypeDeclaration(t)) == CXCursor_UnionDecl) {
            sub.union_id = jg_abi == jg_abi_native ? jg_union_next++ : record->fields[ctx->index].union_id;
        }
        clang_Type_visitFields(t, jg_visit_field, &sub);
        ctx->index = sub.index;
        return CXVisit_Continue;
    }

    struct jg_field* field;
    if (jg_abi == jg_abi_native) {
        assert(record->field_count < jg_max_fields);
        field = &record->fields[record->field_count++];
        memset(field, 0, sizeof(*field));
        snprintf(field->name, sizeof(field->name), "%s", name);
        field->type = jg_type_from(declared);
        field->union_id = ctx->union_id;
        field->count_field = -1;
        field->site = -1;
        if (jg_type_is_fn_ptr(field->type)) {
            jg_param_names(cursor, declared, field->param_names);
        }
    } else {
        field = &record->fields[ctx->index];
        if (ctx->index >= record->field_count || strcmp(field->name, name) != 0) {
            jg_fatal("%s: the wasm and native layouts of %s disagree", record->name, name);
        }
    }
    ctx->index++;
    field->off[jg_abi] = off;

    if (t.kind == CXType_Record && jg_abi == jg_abi_wasm && field->type->record->name[0] == 0) {
        jg_record_layout(field->type->record, t);
    }
    return CXVisit_Continue;
}

static void jg_record_layout(struct jg_record* record, CXType t) {
    long long size = clang_Type_getSizeOf(t);
    long long align = clang_Type_getAlignOf(t);
    assert(size >= 0 && align >= 0 && "record layout");
    record->size[jg_abi] = (uint32_t)size;
    record->align[jg_abi] = (uint32_t)align;
    struct jg_layout_ctx ctx = { .record = record };
    clang_Type_visitFields(t, jg_visit_field, &ctx);
    if (jg_abi == jg_abi_wasm && ctx.index != record->field_count) {
        jg_fatal("%s: the wasm and native field lists disagree", record->name);
    }
}

static struct jg_record* jg_record_get(CXType t) {
    CXCursor decl = clang_getTypeDeclaration(t);
    CXCursor definition = clang_getCursorDefinition(decl);
    if (clang_Cursor_isNull(definition) == false) {
        decl = definition;
    }
    const char* usr = jg_cx(clang_getCursorUSR(decl));
    struct jg_record* record = NULL;
    for (uint32_t i = 0; record == NULL && i < jg_model.record_count; i++) {
        if (strcmp(jg_model.records[i].usr, usr) == 0) {
            record = &jg_model.records[i];
        }
    }
    if (record == NULL) {
        const char* name = jg_cx(clang_getCursorSpelling(decl));
        bool anonymous = jg_cursor_is_anonymous(decl, name);
        assert(jg_model.record_count < jg_max_records);
        record = &jg_model.records[jg_model.record_count++];
        memset(record, 0, sizeof(*record));
        snprintf(record->name, sizeof(record->name), "%s", anonymous ? "" : name);
        jg_camel(record->name, record->js, sizeof(record->js), true);
        record->usr = usr;
        record->file = jg_cursor_file(decl);
        record->platforms = jg_header_platforms(record->file);
        record->is_union = clang_getCursorKind(decl) == CXCursor_UnionDecl;
        const char* spelling = jg_cx(clang_getTypeSpelling(t));
        if (jg_starts_with(spelling, "const ")) {
            spelling += strlen("const ");
        }
        record->is_typedef = jg_starts_with(spelling, "struct ") == false && jg_starts_with(spelling, "union ") == false;
        record->type_field = -1;
    }
    if (record->is_defined == false && clang_isCursorDefinition(decl)) {
        record->is_defined = true;
        record->is_public = record->name[0] != 0 && record->platforms != 0;
        jg_record_layout(record, t);
    }
    return record;
}

static enum CXChildVisitResult jg_visit_enum_constant(CXCursor cursor, CXCursor parent, CXClientData data) {
    struct jg_enum* en = data;
    enum CXCursorKind kind = clang_getCursorKind(cursor);
    if (kind == CXCursor_FlagEnum) {
        en->is_flag = true;
    } else if (kind == CXCursor_EnumConstantDecl) {
        assert(en->constant_count < jg_max_constants);
        struct jg_constant* constant = &en->constants[en->constant_count++];
        memset(constant, 0, sizeof(*constant));
        snprintf(constant->name, sizeof(constant->name), "%s", jg_cx(clang_getCursorSpelling(cursor)));
        constant->value = clang_getEnumConstantDeclValue(cursor);
    }
    return CXChildVisit_Continue;
}

static struct jg_enum* jg_enum_get(CXType t) {
    CXCursor decl = clang_getTypeDeclaration(t);
    const char* name = jg_cx(clang_getCursorSpelling(decl));
    struct jg_enum* en = jg_find_enum(name);
    if (en != NULL) {
        return en;
    }
    assert(jg_model.enum_count < jg_max_enums);
    en = &jg_model.enums[jg_model.enum_count++];
    memset(en, 0, sizeof(*en));
    snprintf(en->name, sizeof(en->name), "%s", name);
    jg_camel(name, en->js, sizeof(en->js), true);
    en->platforms = jg_header_platforms(jg_cursor_file(decl));
    en->is_public = en->platforms != 0 && name[0] != 0;
    clang_visitChildren(decl, jg_visit_enum_constant, en);
    if (en->constant_count > 1) {
        struct jg_constant* last = &en->constants[en->constant_count - 1];
        char sentinel[jg_name + 8];
        snprintf(sentinel, sizeof(sentinel), "%s_count", name);
        if (strcmp(last->name, sentinel) == 0 && last->value == (int64_t)(en->constant_count - 1)) {
            en->constant_count--;
        }
    }
    return en;
}

static void jg_visit_func(CXCursor cursor, uint32_t platforms) {
    const char* name = jg_cx(clang_getCursorSpelling(cursor));
    const char* spelling = jg_cx(clang_getTypeSpelling(clang_getCursorType(cursor)));
    struct jg_func* func = jg_find_func(name);
    if (func != NULL) {
        if (strcmp(func->spelling, spelling) == 0) {
            func->abi_mask |= 1u << jg_abi;
        }
        return;
    }
    assert(jg_model.func_count < jg_max_funcs);
    func = &jg_model.funcs[jg_model.func_count++];
    memset(func, 0, sizeof(*func));
    snprintf(func->name, sizeof(func->name), "%s", name);
    func->spelling = spelling;
    func->ret = jg_type_from(clang_getCursorResultType(cursor));
    int count = clang_Cursor_getNumArguments(cursor);
    assert(count >= 0 && count <= jg_max_params);
    for (int i = 0; i < count; i++) {
        CXCursor arg = clang_Cursor_getArgument(cursor, (unsigned)i);
        struct jg_param* param = &func->params[func->param_count++];
        memset(param, 0, sizeof(*param));
        const char* arg_name = jg_cx(clang_getCursorSpelling(arg));
        snprintf(param->name, sizeof(param->name), "%s", arg_name[0] != 0 ? arg_name : "arg");
        param->type = jg_type_from(clang_getCursorType(arg));
        param->partner = -1;
        param->site = -1;
        if (jg_type_is_fn_ptr(param->type)) {
            jg_param_names(arg, clang_getCursorType(arg), param->param_names);
        }
    }
    func->variadic = clang_Cursor_isVariadic(cursor);
    func->is_inline = clang_getCursorLinkage(cursor) == CXLinkage_Internal;
    func->abi_mask = 1u << jg_abi;
    func->platforms = platforms;
}

static void jg_visit_var(CXCursor cursor, uint32_t platforms) {
    if (clang_Cursor_getStorageClass(cursor) != CX_SC_Extern) {
        return;
    }
    const char* name = jg_cx(clang_getCursorSpelling(cursor));
    for (uint32_t i = 0; i < jg_model.var_count; i++) {
        if (strcmp(jg_model.vars[i].name, name) == 0) {
            return;
        }
    }
    assert(jg_model.var_count < jg_max_vars);
    struct jg_var* var = &jg_model.vars[jg_model.var_count++];
    memset(var, 0, sizeof(*var));
    snprintf(var->name, sizeof(var->name), "%s", name);
    var->type = jg_type_from(clang_getCursorType(cursor));
    var->platforms = platforms;
}

static void jg_visit_macro(CXCursor cursor, const char* file, uint32_t platforms) {
    const char* name = jg_cx(clang_getCursorSpelling(cursor));
    if (jg_starts_with(name, "cr_") || jg_starts_with(name, "js_") || name[0] == '_') {
        return;
    }
    for (uint32_t i = 0; i < jg_model.macro_count; i++) {
        if (strcmp(jg_model.macros[i].name, name) == 0) {
            return;
        }
    }
    assert(jg_model.macro_count < jg_max_macros);
    struct jg_macro* macro = &jg_model.macros[jg_model.macro_count++];
    memset(macro, 0, sizeof(*macro));
    snprintf(macro->name, sizeof(macro->name), "%s", name);
    macro->file = file;
    macro->platforms = platforms;
    macro->is_function = clang_Cursor_isMacroFunctionLike(cursor);
}

static enum CXChildVisitResult jg_visit_top(CXCursor cursor, CXCursor parent, CXClientData data) {
    enum CXCursorKind kind = clang_getCursorKind(cursor);
    if (kind != CXCursor_StructDecl && kind != CXCursor_UnionDecl && kind != CXCursor_EnumDecl && kind != CXCursor_FunctionDecl && kind != CXCursor_VarDecl && kind != CXCursor_MacroDefinition) {
        return CXChildVisit_Continue;
    }
    const char* file = jg_cursor_file(cursor);
    uint32_t platforms = jg_header_platforms(file);
    bool support = false;
    for (uint32_t i = 0; i < jg_support_header_count; i++) {
        support = support || strcmp(file, jg_support_headers[i]) == 0;
    }
    if (platforms == 0 || (support && kind != CXCursor_StructDecl && kind != CXCursor_UnionDecl)) {
        return CXChildVisit_Continue;
    }
    switch (kind) {
    case CXCursor_StructDecl:
    case CXCursor_UnionDecl: {
        if (clang_isCursorDefinition(cursor) == false) {
            break;
        }
        CXType t = clang_getCursorType(cursor);
        if (jg_abi == jg_abi_native) {
            jg_record_get(t);
        } else {
            struct jg_record* record = jg_find_record(jg_cx(clang_getCursorSpelling(cursor)));
            if (record != NULL) {
                jg_record_layout(record, t);
            }
        }
    } break;
    case CXCursor_EnumDecl: {
        if (jg_abi == jg_abi_native && clang_isCursorDefinition(cursor)) {
            jg_enum_get(clang_getCursorType(cursor));
        }
    } break;
    case CXCursor_FunctionDecl: {
        jg_visit_func(cursor, platforms);
    } break;
    case CXCursor_VarDecl: {
        if (jg_abi == jg_abi_native) {
            jg_visit_var(cursor, platforms);
        }
    } break;
    case CXCursor_MacroDefinition: {
        if (jg_abi == jg_abi_native) {
            jg_visit_macro(cursor, file, platforms);
        }
    } break;
    default:
        break;
    }
    return CXChildVisit_Continue;
}

static const char* jg_resource_dir(void) {
    static const char* dir;
    if (dir == NULL) {
        struct cr_str* args[2] = { cr_str_create(jg_mem, jg_args[jg_abi_wasm].args[0]), cr_str_create(jg_mem, "-print-resource-dir") };
        struct cr_str* output = cr_str_create_empty(jg_mem);
        struct cr_str* error = cr_str_create_empty(jg_mem);
        if (cr_exec(args, 2, output, error) != 0 || cr_str_get_count(output) == 0) {
            jg_fatal("%s -print-resource-dir failed", jg_args[jg_abi_wasm].args[0]);
        }
        while (cr_str_get_count(output) > 0 && cr_str_get_data(output)[cr_str_get_count(output) - 1] == '\n') {
            cr_str_remove_last(output, 1);
        }
        dir = jg_strdup(cr_str_get_data(output));
    }
    return dir;
}

static CXTranslationUnit jg_parse_file(enum jg_abi abi, const char* path, bool strict) {
    const char* args[jg_max_args + 2];
    uint32_t count = 0;
    char resource[1024];
    snprintf(resource, sizeof(resource), "-resource-dir=%s", jg_resource_dir());
    args[count++] = resource;
    for (uint32_t i = 1; i < jg_args[abi].count; i++) {
        args[count++] = jg_args[abi].args[i];
    }
    args[count++] = "-w";
    CXTranslationUnit tu = NULL;
    unsigned options = CXTranslationUnit_DetailedPreprocessingRecord | CXTranslationUnit_SkipFunctionBodies;
    enum CXErrorCode code = clang_parseTranslationUnit2(jg_index, path, args, (int)count, NULL, 0, options, &tu);
    if (code != CXError_Success) {
        jg_fatal("%s: clang could not parse %s (error %d)", jg_abi_labels[abi], path, (int)code);
    }
    bool failed = false;
    for (unsigned i = 0, n = clang_getNumDiagnostics(tu); i < n; i++) {
        CXDiagnostic diagnostic = clang_getDiagnostic(tu, i);
        if (clang_getDiagnosticSeverity(diagnostic) >= CXDiagnostic_Error) {
            failed = true;
            if (strict) {
                fprintf(stderr, "%s\n", jg_cx(clang_formatDiagnostic(diagnostic, clang_defaultDiagnosticDisplayOptions())));
            }
        }
        clang_disposeDiagnostic(diagnostic);
    }
    if (failed && strict) {
        jg_fatal("%s: %s has errors", jg_abi_labels[abi], path);
    }
    return tu;
}

static void jg_write_tu(enum jg_abi abi, const char* name, struct cr_str* extra) {
    struct cr_str* tu = cr_str_create_empty(jg_mem);
    for (uint32_t i = 0; i < jg_header_count; i++) {
        if (jg_headers[i].platforms & jg_abi_platforms(abi)) {
            cr_str_append_format(tu, "#include \"%s\"\n", jg_base_name(jg_headers[i].path));
        }
    }
    if (extra != NULL) {
        cr_str_append(tu, extra);
    }
    jg_write_output(name, tu);
    cr_str_destroy(tu);
}

static enum CXChildVisitResult jg_visit_probe(CXCursor cursor, CXCursor parent, CXClientData data) {
    if (clang_getCursorKind(cursor) != CXCursor_VarDecl) {
        return CXChildVisit_Continue;
    }
    const char* name = jg_cx(clang_getCursorSpelling(cursor));
    if (jg_starts_with(name, "jg_c_") == false) {
        return CXChildVisit_Continue;
    }
    CXEvalResult result = clang_Cursor_Evaluate(cursor);
    if (result == NULL) {
        return CXChildVisit_Continue;
    }
    for (uint32_t i = 0; i < jg_model.macro_count; i++) {
        struct jg_macro* macro = &jg_model.macros[i];
        if (strcmp(macro->name, name + 5) != 0) {
            continue;
        }
        CXEvalResultKind kind = clang_EvalResult_getKind(result);
        if (kind == CXEval_Float) {
            macro->value = clang_EvalResult_getAsDouble(result);
            macro->is_number = true;
        } else if (kind == CXEval_Int) {
            macro->value = (double)clang_EvalResult_getAsLongLong(result);
            macro->is_number = true;
        }
    }
    clang_EvalResult_dispose(result);
    return CXChildVisit_Continue;
}

static void jg_probe_macros(void) {
    struct cr_str* extra = cr_str_create_empty(jg_mem);
    for (uint32_t i = 0; i < jg_model.macro_count; i++) {
        struct jg_macro* macro = &jg_model.macros[i];
        if (macro->is_function == false) {
            cr_str_append_format(extra, "static const double jg_c_%s = (double)(%s);\n", macro->name, macro->name);
        }
    }
    jg_write_tu(jg_abi_native, "probe.c", extra);
    cr_str_destroy(extra);
    char path[1024];
    snprintf(path, sizeof(path), "%s/probe.c", jg_out_dir);
    CXTranslationUnit tu = jg_parse_file(jg_abi_native, path, false);
    clang_visitChildren(clang_getTranslationUnitCursor(tu), jg_visit_probe, NULL);
    clang_disposeTranslationUnit(tu);
}

void jg_parse_set_args(enum jg_abi abi, char** args, uint32_t count) {
    assert(count >= 1 && count <= jg_max_args);
    jg_args[abi].args = args;
    jg_args[abi].count = count;
}

void jg_parse(void) {
    for (uint32_t abi = 0; abi < jg_abi_count; abi++) {
        if (jg_args[abi].count == 0) {
            jg_fatal("the %s compiler command is missing; pass --%s <cc> <flags...>", jg_abi_labels[abi], jg_abi_labels[abi]);
        }
    }
    jg_index = clang_createIndex(0, 0);
    for (uint32_t abi = 0; abi < jg_abi_count; abi++) {
        char name[64];
        char path[1024];
        snprintf(name, sizeof(name), "tu_%s.c", jg_abi_labels[abi]);
        snprintf(path, sizeof(path), "%s/%s", jg_out_dir, name);
        jg_write_tu((enum jg_abi)abi, name, NULL);
        jg_abi = (enum jg_abi)abi;
        CXTranslationUnit tu = jg_parse_file(jg_abi, path, true);
        clang_visitChildren(clang_getTranslationUnitCursor(tu), jg_visit_top, NULL);
        clang_disposeTranslationUnit(tu);
    }
    jg_abi = jg_abi_native;
    jg_probe_macros();
    if (jg_model.record_count == 0 || jg_model.func_count == 0) {
        jg_fatal("the headers produced no records or functions");
    }
}
