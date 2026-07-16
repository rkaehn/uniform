#include "cr.h"

#ifndef cr_handle
#error cr_handle not defined
#endif
#define cr_handle_fn(fn) cr_concat3(cr_handle, _, fn)

struct cr_handle {
    uint64_t raw;
};

cr_hdef __attribute__((always_inline))
struct cr_handle cr_handle_fn(make)(uint32_t gen, uint32_t idx) {
    return (struct cr_handle) { .raw = ((uint64_t)gen << 32) | (uint64_t)~idx, };
}

cr_hdef __attribute__((always_inline))
uint32_t cr_handle_fn(get_gen)(struct cr_handle handle) {
    return (uint32_t)(handle.raw >> 32);
}

cr_hdef __attribute__((always_inline))
uint32_t cr_handle_fn(get_idx)(struct cr_handle handle) {
    return ~(uint32_t)handle.raw;
}

cr_hdef __attribute__((always_inline))
bool cr_handle_fn(is_null)(struct cr_handle handle) {
    return cr_handle_fn(get_idx)(handle) == uint32_max;
}

cr_hdef __attribute__((always_inline))
bool cr_handle_fn(is_equal)(struct cr_handle a, struct cr_handle b) {
    return a.raw == b.raw;
}

#undef cr_handle
#undef cr_handle_fn
