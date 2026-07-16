#include "cr.h"
#include <string.h>

#ifndef cr_pool
#error cr_pool not defined
#endif
#ifndef cr_pool_item
#error cr_pool_item not defined
#endif
#ifndef cr_pool_capacity
#error cr_pool_capacity not defined
#endif
#ifndef cr_pool_handle
#error cr_pool_handle not defined
#endif
#define cr_pool_fn(fn) cr_concat3(cr_pool, _, fn)
#define cr_pool_handle_fn(fn) cr_concat3(cr_pool_handle, _, fn)

struct cr_pool {
    uint32_t stack_idx;
    uint32_t stack[cr_pool_capacity];
    cr_pool_item items[cr_pool_capacity];
};

cr_hdef void cr_pool_fn(init)(struct cr_pool* pool) {
    memset(pool, 0, sizeof(*pool));

    uint32_t* stack = pool->stack;

    for (uint32_t i = 0; i < cr_pool_capacity; i++, stack++) {
        *stack = i;
    }
}

cr_hdef uint32_t cr_pool_fn(get_count)(struct cr_pool* pool) {
    return pool->stack_idx;
}

cr_hdef cr_pool_item* cr_pool_fn(alloc)(struct cr_pool* pool) {
    assert(pool->stack_idx < cr_pool_capacity);
    uint32_t idx = pool->stack[pool->stack_idx++];
    cr_pool_item* item = &pool->items[idx];

    uint32_t gen = cr_pool_handle_fn(get_gen)(item->handle) + 1;

    memset(item, 0, sizeof(*item));
    item->handle = cr_pool_handle_fn(make)(gen, idx);

    return item;
}

cr_hdef void cr_pool_fn(dealloc)(struct cr_pool* pool, cr_pool_item* item) {
    assert(pool->stack_idx != 0);
    uint32_t idx = cr_pool_handle_fn(get_idx)(item->handle);

    item->handle = cr_pool_handle_fn(make)(cr_pool_handle_fn(get_gen)(item->handle), uint32_max);

    pool->stack[--pool->stack_idx] = idx;
}

cr_hdef __attribute__((always_inline))
cr_pool_item* cr_pool_fn(item)(struct cr_pool* pool, struct cr_pool_handle handle) {
    uint32_t idx = cr_pool_handle_fn(get_idx)(handle);
    assert(idx != uint32_max);
    assert(pool->items[idx].handle.raw == handle.raw);
    return &pool->items[idx];
}

#undef cr_pool
#undef cr_pool_item
#undef cr_pool_capacity
#undef cr_pool_handle
#undef cr_pool_fn
#undef cr_pool_handle_fn
