#pragma once

#include "js_internal.h"
#include <JavaScriptCore/JavaScriptCore.h>
#include <setjmp.h>

struct jsc_entry {
    const char* name;
    JSObjectCallAsFunctionCallback callback;
};

extern const struct jsc_entry jsc_table[];

struct jsc_guard {
    struct jsc_guard* prev;
    jmp_buf jmp;
};

extern struct jsc_guard* jsc_guard_top;

cr_hdef void jsc_guard_push(struct jsc_guard* guard) {
    guard->prev = jsc_guard_top;
    jsc_guard_top = guard;
}

cr_hdef void jsc_guard_pop(struct jsc_guard* guard) {
    jsc_guard_top = guard->prev;
}

JSValueRef jsc_guard_throw(JSContextRef ctx, struct jsc_guard* guard, JSValueRef* exception);

#define jsc_guard_enter(guard) (jsc_guard_push(guard), _setjmp((guard)->jmp) != 0)

cr_hdef const double* jsc_args(JSContextRef ctx, size_t argc, const JSValueRef argv[], double* temp, uint32_t count) {
    if (argc == 0) {
        return js_args;
    }
    assert(argc == count && "wrong argument count");
    for (uint32_t i = 0; i < count; i++) {
        temp[i] = JSValueToNumber(ctx, argv[i], NULL);
    }
    return temp;
}
