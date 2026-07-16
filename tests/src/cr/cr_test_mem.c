#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "cr.h"
#include "cr_tests.h"

static int check_mem(void* ptr, uint64_t size, uint8_t pattern) {
    uint8_t* p = (uint8_t*)ptr;
    for (uint64_t i = 0; i < size; i++) {
        if (p[i] != pattern) {
            return 0;
        }
    }
    return 1;
}

static void test_mem_basic(struct cr_mem* mem) {
    void* ptr[3];

    ptr[0] = cr_malloc(mem, 64, 0, cr_loc);
    assert(ptr[0]);
    ptr[1] = cr_malloc(mem, 16, 0, cr_loc);
    assert(ptr[1]);
    ptr[2] = cr_malloc(mem, 32, 0, cr_loc);
    assert(ptr[2]);

    cr_free(mem, ptr[0], cr_loc);
    cr_free(mem, ptr[1], cr_loc);
    cr_free(mem, ptr[2], cr_loc);

    ptr[0] = cr_malloc(mem, 128, 0, cr_loc);
    assert(ptr[0]);
    cr_free(mem, ptr[0], cr_loc);
}

static void test_mem_loaded(struct cr_mem* mem) {
    void* ptr[8];
    uint64_t block_size = 16;

    for (int i = 0; i < 8; i++) {
        ptr[i] = cr_malloc(mem, block_size, 0, cr_loc);
        assert(ptr[i]);
    }

    void* p_fail = cr_try_malloc(mem, 1, 0, cr_loc);
    assert(p_fail == NULL);

    for (int i = 0; i < 8; i++) {
        cr_free(mem, ptr[i], cr_loc);
    }

    void* p_success = cr_malloc(mem, 128, 0, cr_loc);
    assert(p_success);
    cr_free(mem, p_success, cr_loc);
}

static void test_mem_realloc(struct cr_mem* mem, bool is_bounded) {
    void* p1 = cr_realloc(mem, NULL, 32, cr_loc);
    assert(p1);

    memset(p1, 0xaa, 32);
    void* p2 = cr_realloc(mem, p1, 16, cr_loc);
    assert(p2);
    assert(check_mem(p2, 16, 0xaa));

    memset(p2, 0xbb, 16);
    void* p3 = cr_realloc(mem, p2, 64, cr_loc);
    assert(p3);
    assert(check_mem(p3, 16, 0xbb));

    if (is_bounded) {
        void* p4 = cr_malloc(mem, 64, 0, cr_loc);
        assert(p4);
        void* p_fail = cr_try_realloc(mem, p3, 128, cr_loc);
        assert(p_fail == NULL);
        assert(check_mem(p3, 16, 0xbb));
        cr_free(mem, p4, cr_loc);
    }

    cr_free(mem, p3, cr_loc);

    void* p_final = cr_malloc(mem, 128, 0, cr_loc);
    assert(p_final);
    cr_free(mem, p_final, cr_loc);
}

static void test_mem_many(struct cr_mem* mem) {
    void* ptr[8] = { 0 };

    for (int i = 0; i < 8; i++) {
        ptr[i] = cr_malloc(mem, 16, 0, cr_loc);
        assert(ptr[i]);
    }

    cr_free(mem, ptr[0], cr_loc);
    cr_free(mem, ptr[1], cr_loc);
    ptr[0] = NULL;
    ptr[1] = NULL;

    cr_free(mem, ptr[3], cr_loc);
    cr_free(mem, ptr[5], cr_loc);
    ptr[3] = NULL;
    ptr[5] = NULL;

    void* p32 = cr_malloc(mem, 32, 0, cr_loc);
    assert(p32);

    void* p32_fail = cr_try_malloc(mem, 32, 0, cr_loc);
    assert(p32_fail == NULL);

    void* p16_a = cr_malloc(mem, 16, 0, cr_loc);
    assert(p16_a);
    void* p16_b = cr_malloc(mem, 16, 0, cr_loc);
    assert(p16_b);

    cr_free(mem, p32, cr_loc);
    cr_free(mem, p16_a, cr_loc);
    cr_free(mem, p16_b, cr_loc);
    for (int i = 0; i < 8; i++) {
        cr_free(mem, ptr[i], cr_loc);
    }

    void* p_final = cr_malloc(mem, 128, 0, cr_loc);
    assert(p_final);
    cr_free(mem, p_final, cr_loc);
}

static void test_mem_align(struct cr_mem* mem, bool is_lin) {
    uint64_t sizes[] = { 1, 7, 15, 16, 33, 64, 127 };

    for (uint64_t i = 0; i < cr_count(sizes); i++) {
        for (uint64_t align = sizeof(void*); align <= 256; align <<= 1) {
            void* p = cr_malloc(mem, sizes[i], align, cr_loc);
            assert(p != NULL);
            assert(((uint64_t)p % align) == 0);
            memset(p, 0xfe, (size_t)sizes[i]);
            if (is_lin) {
                cr_mem_lin_reset(mem);
            } else {
                cr_free(mem, p, cr_loc);
            }
        }
    }
}

static void test_mem_realloc_zero(struct cr_mem* mem) {
    void* p = cr_malloc(mem, 32, 0, cr_loc);
    assert(p);
    void* p_null = cr_realloc(mem, p, 0, cr_loc);
    assert(p_null == NULL);
}

static void test_mem_buddy_oversized(struct cr_mem* mem) {
    void* p1 = cr_try_malloc(mem, 129, 0, cr_loc);
    assert(p1 == NULL);
    void* p2 = cr_try_malloc(mem, UINT64_MAX, 0, cr_loc);
    assert(p2 == NULL);
}

static void test_mem_buddy_realloc_inplace(struct cr_mem* mem) {
    void* a = cr_malloc(mem, 64, 0, cr_loc);
    assert(a);
    memset(a, 0xcc, 64);
    void* b = cr_malloc(mem, 64, 0, cr_loc);
    assert(b);

    void* a2 = cr_realloc(mem, a, 33, cr_loc);
    assert(a2 == a);
    assert(check_mem(a2, 33, 0xcc));

    void* a3 = cr_realloc(mem, a2, 16, cr_loc);
    assert(a3 == a);
    assert(check_mem(a3, 16, 0xcc));

    void* c = cr_malloc(mem, 32, 0, cr_loc);
    assert(c);
    void* d = cr_malloc(mem, 16, 0, cr_loc);
    assert(d);
    void* e = cr_try_malloc(mem, 16, 0, cr_loc);
    assert(e == NULL);

    cr_free(mem, d, cr_loc);
    cr_free(mem, c, cr_loc);
    cr_free(mem, a3, cr_loc);
    cr_free(mem, b, cr_loc);

    void* p_full = cr_malloc(mem, 128, 0, cr_loc);
    assert(p_full);
    cr_free(mem, p_full, cr_loc);
}

static void test_mem_buddy_stress(struct cr_mem* arg_mem) {
    struct cr_mem* mem = cr_mem_buddy_create(&(struct cr_mem_buddy_params) {
        .mem = arg_mem,
        .backing_po2 = 12,
        .l0_po2 = 4,
        .wants_rec = true,
    });

    struct {
        void* ptr;
        uint64_t size;
        uint8_t pattern;
    } slots[64] = { 0 };

    uint64_t seed = 1;
    uint8_t next_pattern = 1;

    for (uint32_t i = 0; i < 100000; i++) {
        uint32_t slot = (uint32_t)(cr_rand(&seed) % cr_count(slots));
        uint32_t op = (uint32_t)(cr_rand(&seed) % 4);

        if (slots[slot].ptr == NULL) {
            uint64_t size = 1 + cr_rand(&seed) % 256;
            uint64_t align = 8llu << (cr_rand(&seed) % 4);
            void* p = cr_try_malloc(mem, size, align, cr_loc);
            if (p) {
                assert(((uint64_t)p % align) == 0);
                memset(p, next_pattern, (size_t)size);
                slots[slot].ptr = p;
                slots[slot].size = size;
                slots[slot].pattern = next_pattern;
                next_pattern = next_pattern == 255 ? 1 : next_pattern + 1;
            }
        } else if (op == 0) {
            assert(check_mem(slots[slot].ptr, slots[slot].size, slots[slot].pattern));
            cr_free(mem, slots[slot].ptr, cr_loc);
            slots[slot].ptr = NULL;
        } else if (op == 1) {
            uint64_t new_size = 1 + cr_rand(&seed) % 256;
            void* p = cr_try_realloc(mem, slots[slot].ptr, new_size, cr_loc);
            if (p) {
                uint64_t kept = new_size < slots[slot].size ? new_size : slots[slot].size;
                assert(check_mem(p, kept, slots[slot].pattern));
                memset(p, next_pattern, (size_t)new_size);
                slots[slot].ptr = p;
                slots[slot].size = new_size;
                slots[slot].pattern = next_pattern;
                next_pattern = next_pattern == 255 ? 1 : next_pattern + 1;
            } else {
                assert(check_mem(slots[slot].ptr, slots[slot].size, slots[slot].pattern));
            }
        } else {
            assert(check_mem(slots[slot].ptr, slots[slot].size, slots[slot].pattern));
        }
    }

    for (uint64_t i = 0; i < cr_count(slots); i++) {
        if (slots[i].ptr) {
            assert(check_mem(slots[i].ptr, slots[i].size, slots[i].pattern));
            cr_free(mem, slots[i].ptr, cr_loc);
        }
    }

    assert(cr_rec_get_alloc_count_filter_mem(mem) == 0);

    void* p_full = cr_malloc(mem, 1llu << 12, 0, cr_loc);
    assert(p_full);
    cr_free(mem, p_full, cr_loc);

    cr_mem_destroy(mem);
}

static void test_mem_lin_limits(struct cr_mem* mem, uint64_t backing_size) {
    void* p1 = cr_try_malloc(mem, backing_size + 1, 0, cr_loc);
    assert(p1 == NULL);
    void* p2 = cr_try_malloc(mem, UINT64_MAX, 0, cr_loc);
    assert(p2 == NULL);
    void* p3 = cr_try_malloc(mem, UINT64_MAX - 8, 0, cr_loc);
    assert(p3 == NULL);

    void* p4 = cr_realloc(mem, NULL, 16, cr_loc);
    assert(p4);
    void* p5 = cr_try_malloc(mem, UINT64_MAX - 16, 0, cr_loc);
    assert(p5 == NULL);

    cr_mem_lin_reset(mem);
}

static void test_mem_lin_basic(struct cr_mem* mem) {
    void* p1 = cr_malloc(mem, 10, 0, cr_loc);
    assert(p1);
    void* p2 = cr_malloc(mem, 20, 0, cr_loc);
    assert(p2);
    void* p3 = cr_malloc(mem, 30, 0, cr_loc);
    assert(p3);

    cr_mem_lin_reset(mem);
}

void cr_test_mem(void) {
    struct cr_mem* arg_mem = cr_mem_libc;

    {
        struct cr_mem* mem = cr_mem_buddy_create(&(struct cr_mem_buddy_params) {
            .mem = arg_mem,
            .backing_po2 = 7,
            .l0_po2 = 4,
            .wants_rec = true,
        });
        test_mem_basic(mem);
        test_mem_loaded(mem);
        assert(cr_rec_get_alloc_count_filter_mem(mem) == 0);
        test_mem_realloc(mem, true);
        test_mem_many(mem);
        cr_free(mem, NULL, cr_loc);
        test_mem_realloc_zero(mem);
        test_mem_buddy_oversized(mem);
        test_mem_buddy_realloc_inplace(mem);
        assert(cr_rec_get_alloc_count_filter_mem(mem) == 0);
        cr_mem_destroy(mem);
    }

    {
        struct cr_mem* mem = cr_mem_buddy_create(&(struct cr_mem_buddy_params) {
            .mem = arg_mem,
            .backing_po2 = 8,
            .l0_po2 = 4,
            .wants_rec = true,
        });
        test_mem_align(mem, false);
        assert(cr_rec_get_alloc_count_filter_mem(mem) == 0);
        cr_mem_destroy(mem);
    }

    test_mem_buddy_stress(arg_mem);

    {
        uint64_t backing_size = 4096;
        void* backing = cr_malloc(arg_mem, backing_size + 256, 256, cr_loc);
        for (uint64_t offset = 0; offset <= 256; offset++) {
            void* offset_backing = (char*)backing + offset;
            struct cr_mem* mem = cr_mem_lin_create(&(struct cr_mem_lin_params) {
                .mem = arg_mem,
                .raw_backing = offset_backing,
                .backing_size = backing_size,
                .wants_rec = true,
            });
            test_mem_lin_basic(mem);
            test_mem_align(mem, true);
            cr_free(mem, NULL, cr_loc);
            test_mem_lin_limits(mem, backing_size);
            assert(cr_rec_get_alloc_count_filter_mem(mem) == 0);
            cr_mem_destroy(mem);
        }
        cr_free(arg_mem, backing, cr_loc);
    }

    {
        struct cr_mem* mem = arg_mem;
        test_mem_basic(mem);
        test_mem_realloc(mem, false);
        test_mem_align(mem, false);
        cr_free(mem, NULL, cr_loc);
        test_mem_realloc_zero(mem);
    }

    printf("cr_test_mem: ok\n");
}
