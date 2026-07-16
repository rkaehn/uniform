#pragma once

#include "cr.h"
#include "app.h"
#include "app_event.h"
#include "gpu.h"
#include "ui.h"
#include "ui_input.h"
#include "ui_gestures.h"
#include "ui_text_input.h"
#include "ui_list.h"
#include "ui_physics.h"
#include "ui_util.h"
#include "hud.h"
#include "log.h"
#if uniform_tool
#include "prim.h"
#include "bd.h"
#include "bd_xcode.h"
#endif
#include "js.h"
#include <string.h>
#include <math.h>

#if cr_wasm
#define js_export(ret, name, args) __attribute__((export_name(#name))) ret js_x_##name args
#else
#define js_export(ret, name, args) ret js_x_##name args
#endif

extern uint8_t* js_arena;
extern double* js_args;
extern const void* const js_tramp_addrs[];

#if cr_wasm
cr_js_import(js_dispatch)
#endif
double js_dispatch(uint32_t kind, uint32_t id, double a0, double a1, double a2, double a3, double a4, double a5);

void* js_bump(uint32_t size, uint32_t align);
uint32_t js_bump_mark(void);
void js_bump_release(uint32_t mark);

cr_hdef void* js_p(double d) {
    cr_check(d >= 0.0 && d < 9007199254740992.0 && d == floor(d), "the value is not a pointer");
    return (void*)(uintptr_t)(uint64_t)d;
}

cr_hdef double js_ptr(const void* p) {
    uint64_t value = (uint64_t)(uintptr_t)p;
    assert(value < (1ull << 53));
    return (double)value;
}

cr_hdef double js_pack(uint64_t raw) {
    if ((uint32_t)raw == 0) {
        return 0.0;
    }
    assert((raw >> 32) < 0x7ff00000ull);
    double value;
    memcpy(&value, &raw, sizeof(value));
    return value;
}

cr_hdef uint64_t js_unpack(double value) {
    uint64_t raw;
    memcpy(&raw, &value, sizeof(raw));
    return raw;
}

cr_hdef uint64_t js_u64(double d) {
    cr_check(d >= 0.0 && d < 9007199254740992.0 && d == floor(d), "a 64 bit value must be an integer below 2^53");
    return (uint64_t)d;
}

cr_hdef int64_t js_i64(double d) {
    cr_check(d > -9007199254740992.0 && d < 9007199254740992.0 && d == floor(d), "a 64 bit value must be an integer within 53 bits");
    return (int64_t)d;
}

cr_hdef double js_double_u64(uint64_t value) {
    assert(value < (1ull << 53) && "a 64 bit value from the library must fit in 53 bits");
    return (double)value;
}

cr_hdef double js_double_i64(int64_t value) {
    assert(value > -(1ll << 53) && value < (1ll << 53) && "a 64 bit value from the library must fit in 53 bits");
    return (double)value;
}

cr_hdef uint32_t js_cb_id(const void* user) {
    return (uint32_t)(uintptr_t)user;
}

cr_hdef void* js_cb_user(uint32_t id) {
    return (void*)(uintptr_t)id;
}
