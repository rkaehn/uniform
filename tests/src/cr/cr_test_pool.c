#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "cr.h"
#include "cr_tests.h"

#define cr_handle thing
#include "cr_handle.h"

#define thing_capacity 64

struct thing_backing {
    struct thing handle;
    uint64_t value;
};

#define cr_pool thing_pool
#define cr_pool_item struct thing_backing
#define cr_pool_capacity thing_capacity
#define cr_pool_handle thing
#include "cr_pool.h"

#define cr_handle big
#include "cr_handle.h"

struct big_backing {
    struct big handle;
    uint8_t bytes[200];
};

#define cr_pool big_pool
#define cr_pool_item struct big_backing
#define cr_pool_capacity 4096
#define cr_pool_handle big
#include "cr_pool.h"

static void test_pool_handle(void) {
    struct thing null = { 0 };
    assert(thing_is_null(null));
    assert(thing_get_gen(null) == 0);
    assert(thing_get_idx(null) == uint32_max);
    assert(thing_is_equal(null, thing_make(0, uint32_max)));

    struct thing handle = thing_make(7, 3);
    assert(thing_get_gen(handle) == 7);
    assert(thing_get_idx(handle) == 3);
    assert(thing_is_null(handle) == false);
    assert(thing_is_equal(handle, handle));
    assert(thing_is_equal(handle, thing_make(7, 4)) == false);
    assert(thing_is_equal(handle, thing_make(8, 3)) == false);

    struct thing gen0 = thing_make(0, 3);
    assert(thing_is_null(gen0) == false);
    assert(thing_is_equal(gen0, null) == false);
}

static void test_pool_basic(struct cr_mem* mem) {
    struct thing_pool* pool = cr_malloc(mem, sizeof(*pool), 0, cr_loc);
    thing_pool_init(pool);

    assert(thing_pool_get_count(pool) == 0);

    struct thing_backing* a = thing_pool_alloc(pool);
    assert(thing_pool_get_count(pool) == 1);
    assert(a->value == 0);
    assert(thing_is_null(a->handle) == false);
    assert(thing_get_gen(a->handle) == 1);
    assert(thing_pool_item(pool, a->handle) == a);

    struct thing_backing* b = thing_pool_alloc(pool);
    assert(thing_pool_get_count(pool) == 2);
    assert(b != a);
    assert(thing_is_equal(a->handle, b->handle) == false);
    assert(thing_get_idx(a->handle) != thing_get_idx(b->handle));
    assert(thing_pool_item(pool, b->handle) == b);

    struct thing a_stale = a->handle;
    a->value = 123;
    thing_pool_dealloc(pool, a);
    assert(thing_pool_get_count(pool) == 1);

    struct thing_backing* c = thing_pool_alloc(pool);
    assert(c == a);
    assert(c->value == 0);
    assert(thing_get_idx(c->handle) == thing_get_idx(a_stale));
    assert(thing_get_gen(c->handle) == thing_get_gen(a_stale) + 1);
    assert(thing_is_equal(c->handle, a_stale) == false);

    thing_pool_dealloc(pool, c);
    thing_pool_dealloc(pool, b);
    assert(thing_pool_get_count(pool) == 0);

    cr_free(mem, pool, cr_loc);
}

static void test_pool_full(struct cr_mem* mem) {
    struct thing_pool* pool = cr_malloc(mem, sizeof(*pool), 0, cr_loc);
    thing_pool_init(pool);

    struct thing handles[thing_capacity];

    for (uint32_t i = 0; i < thing_capacity; i++) {
        struct thing_backing* item = thing_pool_alloc(pool);
        item->value = i;
        handles[i] = item->handle;
    }
    assert(thing_pool_get_count(pool) == thing_capacity);

    for (uint32_t i = 0; i < thing_capacity; i++) {
        for (uint32_t j = i + 1; j < thing_capacity; j++) {
            assert(thing_get_idx(handles[i]) != thing_get_idx(handles[j]));
        }
        assert(thing_pool_item(pool, handles[i])->value == i);
    }

    for (uint32_t i = 0; i < thing_capacity; i++) {
        thing_pool_dealloc(pool, thing_pool_item(pool, handles[i]));
    }
    assert(thing_pool_get_count(pool) == 0);

    for (uint32_t i = 0; i < thing_capacity; i++) {
        struct thing_backing* item = thing_pool_alloc(pool);
        assert(item->value == 0);
        assert(thing_get_gen(item->handle) == 2);
    }
    assert(thing_pool_get_count(pool) == thing_capacity);

    for (uint32_t i = 0; i < thing_capacity; i++) {
        thing_pool_dealloc(pool, &pool->items[i]);
    }

    cr_free(mem, pool, cr_loc);
}

static void test_pool_gen_wrap(struct cr_mem* mem) {
    struct thing_pool* pool = cr_malloc(mem, sizeof(*pool), 0, cr_loc);
    thing_pool_init(pool);

    struct thing_backing* item = thing_pool_alloc(pool);
    uint32_t idx = thing_get_idx(item->handle);
    thing_pool_dealloc(pool, item);

    pool->items[idx].handle = thing_make(uint32_max, uint32_max);

    item = thing_pool_alloc(pool);
    assert(thing_get_gen(item->handle) == 0);
    assert(thing_get_idx(item->handle) == idx);
    assert(thing_is_null(item->handle) == false);
    assert(thing_pool_item(pool, item->handle) == item);

    thing_pool_dealloc(pool, item);
    cr_free(mem, pool, cr_loc);
}

static void test_pool_two_pools(struct cr_mem* mem) {
    struct thing_pool* things = cr_malloc(mem, sizeof(*things), 0, cr_loc);
    struct big_pool* bigs = cr_malloc(mem, sizeof(*bigs), 0, cr_loc);
    thing_pool_init(things);
    big_pool_init(bigs);

    struct thing_backing* thing_item = thing_pool_alloc(things);
    struct big_backing* big_item = big_pool_alloc(bigs);

    for (uint32_t i = 0; i < cr_count(big_item->bytes); i++) {
        assert(big_item->bytes[i] == 0);
    }
    memset(big_item->bytes, 0xab, sizeof(big_item->bytes));

    assert(thing_get_idx(thing_item->handle) == big_get_idx(big_item->handle));
    assert(thing_pool_item(things, thing_item->handle) == thing_item);
    assert(big_pool_item(bigs, big_item->handle) == big_item);

    big_pool_dealloc(bigs, big_item);
    assert(thing_pool_get_count(things) == 1);

    big_item = big_pool_alloc(bigs);
    for (uint32_t i = 0; i < cr_count(big_item->bytes); i++) {
        assert(big_item->bytes[i] == 0);
    }

    big_pool_dealloc(bigs, big_item);
    thing_pool_dealloc(things, thing_item);

    cr_free(mem, things, cr_loc);
    cr_free(mem, bigs, cr_loc);
}

static void test_pool_stress(struct cr_mem* mem) {
    uint32_t cycle_count_target = 16;
    uint32_t count_upper = thing_capacity - 4;
    uint32_t count_lower = 4;

    struct test_slot {
        struct thing handle;
        uint64_t value;
    };

    struct thing_pool* pool = cr_malloc(mem, sizeof(*pool), 0, cr_loc);
    thing_pool_init(pool);

    struct test_slot used[thing_capacity];
    uint32_t used_count = 0;

    uint32_t last_gen[thing_capacity] = { 0 };

    bool is_dec = false;
    uint64_t seed = 0;
    uint32_t cycle_count = 0;

    while (1) {
        uint64_t r_1 = cr_rand(&seed);
        double u_1 = cr_2u01(r_1);
        uint64_t r_2 = cr_rand(&seed);

        bool is_alloc = u_1 < (is_dec ? 0.25 : 0.75);
        if (used_count == 0) {
            is_alloc = true;
        } else if (used_count == thing_capacity) {
            is_alloc = false;
        }

        if (is_alloc) {
            struct thing_backing* item = thing_pool_alloc(pool);
            assert(item->value == 0);

            uint32_t idx = thing_get_idx(item->handle);
            uint32_t gen = thing_get_gen(item->handle);
            assert(idx < thing_capacity);
            assert(gen > last_gen[idx]);
            last_gen[idx] = gen;

            item->value = r_2;
            used[used_count++] = (struct test_slot) {
                .handle = item->handle,
                .value = r_2,
            };
        } else {
            uint32_t index = (uint32_t)(r_2 % used_count);
            struct thing_backing* item = thing_pool_item(pool, used[index].handle);
            assert(item->value == used[index].value);
            thing_pool_dealloc(pool, item);
            used[index] = used[--used_count];
        }

        assert(thing_pool_get_count(pool) == used_count);

        for (uint32_t i = 0; i < used_count; i++) {
            struct thing_backing* item = thing_pool_item(pool, used[i].handle);
            assert(item->value == used[i].value);
            assert(thing_is_equal(item->handle, used[i].handle));
        }

        bool is_low = is_dec && used_count < count_lower;
        bool is_high = !is_dec && used_count > count_upper;

        if (is_low || is_high) {
            is_dec = !is_dec;
            cycle_count++;
            if (cycle_count == cycle_count_target) {
                break;
            }
        }
    }

    while (used_count != 0) {
        thing_pool_dealloc(pool, thing_pool_item(pool, used[--used_count].handle));
    }
    assert(thing_pool_get_count(pool) == 0);

    cr_free(mem, pool, cr_loc);
}

void cr_test_pool(void) {
    struct cr_mem* mem = cr_mem_libc;

    test_pool_handle();
    test_pool_basic(mem);
    test_pool_full(mem);
    test_pool_gen_wrap(mem);
    test_pool_two_pools(mem);
    test_pool_stress(mem);

    printf("cr_test_pool: ok\n");
}
