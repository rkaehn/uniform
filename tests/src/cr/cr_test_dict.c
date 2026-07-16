#include <assert.h>
#include <stdio.h>
#include "cr.h"
#include "cr_tests.h"

#define cr_dict dict
#define cr_dict_key char*
#define cr_dict_value uint64_t
#define cr_dict_is_equal(a, b) (strcmp(a, b) == 0)
#define cr_dict_hash(key) cr_hash(key, strlen(key), 0)
#include "cr_dict.h"

#define cr_dict dict2
#define cr_dict_key uint32_t
#define cr_dict_value uint32_t
#define cr_dict_is_equal(a, b) ((a) == (b))
#define cr_dict_hash(key) cr_hash(&key, sizeof(key), 0)
#include "cr_dict.h"

#define cr_dict set
#define cr_dict_key uint32_t
#define cr_dict_is_equal(a, b) ((a) == (b))
#define cr_dict_hash(key) cr_hash(&key, sizeof(key), 0)
#include "cr_dict.h"

static void test_dict_basic(struct cr_mem* mem) {
    {
        struct dict2* dict = dict2_create(mem);

        bool v;
        uint32_t value;

        assert(dict2_get_capacity(dict) == 0);

        v = dict2_set(dict, 4096, 123);
        assert(v == false);
        assert(dict2_get_count(dict) == 1);

        v = dict2_set(dict, 4097, 125);
        assert(v == false);
        assert(dict2_get_count(dict) == 2);

        v = dict2_set(dict, 4097, 124);
        assert(v);
        assert(dict2_get_count(dict) == 2);

        v = dict2_get(dict, 4097, &value);
        assert(v && value == 124);

        v = dict2_remove(dict, 4097);
        assert(v);
        assert(dict2_get_count(dict) == 1);

        v = dict2_get(dict, 4097, NULL);
        assert(v == false);

        v = dict2_get(dict, 4096, &value);
        assert(v && value == 123);

        v = dict2_remove(dict, 4096);
        assert(v);
        assert(dict2_get_count(dict) == 0);

        v = dict2_remove(dict, 4098);
        assert(v == false);

        uint32_t* p;
        v = dict2_set_ptr(dict, 4096, &p);
        assert(v == false);
        *p = 456;

        v = dict2_get_ptr(dict, 4096, &p);
        assert(v);
        assert(*p == 456);

        v = dict2_remove(dict, 4096);
        assert(v);

        v = dict2_get_ptr(dict, 4096, &p);
        assert(v == false);
        assert(p == NULL);

        assert(dict2_get_count(dict) == 0);

        dict2_ensure_capacity(dict, 64);
        assert(dict2_get_capacity(dict) == 64);

        dict2_ensure_capacity(dict, 65);
        assert(dict2_get_capacity(dict) == 128);

        dict2_destroy(dict);
    }

    {
        struct dict* dict = dict_create(mem);

        uint32_t str_count = 1u << 16;
        uint32_t str_size = 8;
        char* str = cr_malloc(mem, str_count * str_size, 0, cr_loc);
        char* str_start = str;
        memset(str, 0, str_count * str_size);

        bool v;
        uint64_t value;

        for (uint32_t i = 0; i < str_count; i++) {
            snprintf(str, str_size, "%u", i);
            v = dict_set(dict, str, i);
            assert(v == false);
            assert(dict_get_count(dict) == i + 1);
            str += str_size;

            if (dict_get_count(dict) == 13) {
                assert(dict_get_capacity(dict) == 16);
            } else if (dict_get_count(dict) == 14) {
                assert(dict_get_capacity(dict) == 32);
            }
        }

        assert(dict_get_capacity(dict) == str_count * 2);

        str = str_start;

        for (uint32_t i = 0; i < str_count; i++) {
            snprintf(str, str_size, "%u", i);
            v = dict_get(dict, str, &value);
            assert(v && value == i);
            v = dict_remove(dict, str);
            assert(v);
            assert(dict_get_count(dict) == str_count - (i + 1));
            str += str_size;

            if (dict_get_count(dict) == 8) {
                assert(dict_get_capacity(dict) == 32);
            } else if (dict_get_count(dict) == 7) {
                assert(dict_get_capacity(dict) == 16);
            }
        }

        assert(dict_get_capacity(dict) == 16);

        cr_free(mem, str_start, cr_loc);

        dict_destroy(dict);
    }

    {
        struct set* set = set_create(mem);

        uint32_t count = 1u << 16;

        struct set_iter iter = set_iter(set);
        struct set_pair* pair;

        for (uint32_t i = 0; i < count; i++) {
            set_set(set, i);
        }

        bool* iter_list = cr_malloc(mem, count, 0, cr_loc);
        memset(iter_list, 0, count);

        while (set_iter_next(&iter, &pair)) {
            iter_list[pair->key] = 1;
        }
        assert(pair == NULL);

        for (uint32_t i = 0; i < count; i++) {
            assert(iter_list[i] == 1);
        }

        cr_free(mem, iter_list, cr_loc);

        bool v = set_iter_next(&iter, &pair);
        assert(v == false);
        assert(pair == NULL);

        set_destroy(set);
    }
}

static void test_dict_set(struct cr_mem* mem) {
    struct set* set = set_create(mem);

    bool v;

    v = set_set(set, 4096);
    assert(v == false);
    assert(set_get_count(set) == 1);
    v = set_get(set, 4096);
    assert(v);
    v = set_get(set, 4097);
    assert(v == false);
    v = set_remove(set, 4096);
    assert(v);
    assert(set_get_count(set) == 0);
    v = set_remove(set, 4097);
    assert(v == false);

    set_destroy(set);
}

static void test_dict_stress(struct cr_mem* mem) {
    enum test_op {
        test_op_create,
        test_op_read,
        test_op_update,
        test_op_delete,
    };

    struct test_item {
        char* key;
        uint64_t value;
    };

    uint64_t cycle_count_target = 16;
    uint64_t count_lower = 8;
    uint64_t count_upper = 896;

    struct dict* dict = dict_create(mem);

    bool is_dec = false;

    uint64_t seed = 0;
    uint64_t cycle_count = 0;

    uint64_t used_count = 0;
    uint64_t unused_count = 1024;
    struct test_item* used = cr_malloc(mem, unused_count * sizeof(*used), 0, cr_loc);
    struct test_item* unused = cr_malloc(mem, unused_count * sizeof(*unused), 0, cr_loc);

    uint64_t str_size = 8;
    char* str = cr_malloc(mem, unused_count * str_size, 0, cr_loc);

    for (uint64_t i = 0; i < unused_count; i++) {
        unused[i] = (struct test_item) {
            .key = str + i * str_size,
        };
        snprintf(unused[i].key, (size_t)str_size, "%llu", i);
    }

    while (1) {
        uint64_t r_1 = cr_rand(&seed);
        double u_1 = cr_2u01(r_1);

        enum test_op op;
        if (u_1 < (is_dec ? 0.1 : 0.7)) {
            op = test_op_create;
        } else if (u_1 < (is_dec ? 0.2 : 0.8)) {
            op = test_op_read;
        } else if (u_1 < (is_dec ? 0.3 : 0.9)) {
            op = test_op_update;
        } else {
            op = test_op_delete;
        }

        if (op == test_op_create && unused_count == 0) {
            op = test_op_delete;
        } else if ((op == test_op_read || op == test_op_update || op == test_op_delete) && used_count == 0) {
            op = test_op_create;
        }

        uint64_t r_2 = cr_rand(&seed);

        if (op == test_op_create) {
            uint64_t index = r_2 % unused_count;

            used[used_count].key = unused[index].key;
            used[used_count].value = r_2;
            unused[index] = unused[unused_count - 1];

            bool did_exist = dict_set(dict, used[used_count].key, used[used_count].value);
            assert(did_exist == false);

            used_count++;
            unused_count--;
        } else if (op == test_op_read) {
            uint64_t index = r_2 % used_count;

            uint64_t value;
            bool did_exist = dict_get(dict, used[index].key, &value);
            assert(did_exist);

            assert(value == used[index].value);
        } else if (op == test_op_update) {
            uint64_t index = r_2 % used_count;

            used[index].value = r_2;

            bool did_exist = dict_set(dict, used[index].key, used[index].value);
            assert(did_exist);
        } else {
            uint64_t index = r_2 % used_count;

            unused[unused_count].key = used[index].key;
            used[index] = used[used_count - 1];

            bool did_exist = dict_remove(dict, unused[unused_count].key);
            assert(did_exist);

            used_count--;
            unused_count++;
        }

        uint64_t count = dict_get_count(dict);
        assert(count == used_count);

        for (uint64_t i = 0; i < used_count; i++) {
            uint64_t value;
            bool did_exist = dict_get(dict, used[i].key, &value);
            assert(did_exist);
            assert(value == used[i].value);
        }
        for (uint64_t i = 0; i < unused_count; i++) {
            bool did_exist = dict_get(dict, unused[i].key, NULL);
            assert(did_exist == false);
        }

        bool is_low = is_dec && count < count_lower;
        bool is_high = !is_dec && count > count_upper;

        if (is_low || is_high) {
            is_dec = !is_dec;
            cycle_count++;
            if (cycle_count == cycle_count_target) {
                break;
            }
        }
    }

    cr_free(mem, used, cr_loc);
    cr_free(mem, unused, cr_loc);
    cr_free(mem, str, cr_loc);

    dict_destroy(dict);
}

void cr_test_dict(void) {
    struct cr_mem* mem = cr_mem_libc;

    test_dict_basic(mem);
    test_dict_set(mem);
    test_dict_stress(mem);

    {
        struct dict2* empty = dict2_create(mem);
        struct dict2_iter iter = dict2_iter(empty);
        struct dict2_pair* pair;
        assert(dict2_iter_next(&iter, &pair) == false);
        dict2_destroy(empty);
    }

    printf("cr_test_dict: ok\n");
}
