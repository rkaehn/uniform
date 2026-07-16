#include <assert.h>
#include <stdio.h>
#include "cr.h"
#include "cr_tests.h"

#define cr_arr arr
#define cr_arr_item int
#include "cr_arr.h"

static void test_arr_basic(struct cr_mem* mem) {
    struct arr* arr = arr_create(mem);
    assert(arr);
    assert(arr_get_count(arr) == 0);

    arr_append(arr, 123);
    assert(arr_get_count(arr) == 1);
    assert(arr_get(arr, 0) == 123);

    arr_set(arr, 0, 456);
    assert(arr_get_count(arr) == 1);
    assert(arr_get(arr, 0) == 456);

    arr_destroy(arr);
}

static void test_arr_append(struct cr_mem* mem) {
    struct arr* arr = arr_create(mem);

    for (int i = 0; i < 1000; i++) {
        arr_append(arr, i * 10);
    }
    assert(arr_get_count(arr) == 1000);
    assert(arr_get(arr, 0) == 0);
    assert(arr_get(arr, 500) == 5000);
    assert(arr_get(arr, 999) == 9990);

    int items[] = { 100, 200, 300 };
    arr_append_items(arr, items, 3);
    assert(arr_get_count(arr) == 1003);
    assert(arr_get(arr, 1000) == 100);
    assert(arr_get(arr, 1001) == 200);
    assert(arr_get(arr, 1002) == 300);

    arr_destroy(arr);
}

static void test_arr_remove(struct cr_mem* mem) {
    struct arr* arr = arr_create(mem);
    arr_append(arr, 10);
    arr_append(arr, 20);
    arr_append(arr, 30);
    arr_append(arr, 40);
    arr_append(arr, 50);
    assert(arr_get_count(arr) == 5);

    arr_remove(arr, 2);
    assert(arr_get_count(arr) == 4);
    assert(arr_get(arr, 0) == 10);
    assert(arr_get(arr, 1) == 20);
    assert(arr_get(arr, 2) == 40);
    assert(arr_get(arr, 3) == 50);

    arr_remove(arr, 0);
    assert(arr_get_count(arr) == 3);
    assert(arr_get(arr, 0) == 20);
    assert(arr_get(arr, 1) == 40);
    assert(arr_get(arr, 2) == 50);

    arr_remove(arr, 2);
    assert(arr_get_count(arr) == 2);
    assert(arr_get(arr, 0) == 20);
    assert(arr_get(arr, 1) == 40);

    arr_destroy(arr);

    arr = arr_create(mem);
    for (int i = 0; i < 100; i++) {
        arr_append(arr, i);
    }
    assert(arr_get_count(arr) == 100);

    arr_remove_all(arr);
    assert(arr_get_count(arr) == 0);

    arr_append(arr, 999);
    assert(arr_get_count(arr) == 1);
    assert(arr_get(arr, 0) == 999);

    arr_destroy(arr);
}

static void test_arr_copy(struct cr_mem* mem) {
    struct arr* arr1 = arr_create(mem);
    arr_append(arr1, 10);
    arr_append(arr1, 20);
    arr_append(arr1, 30);

    struct arr* arr2 = arr_create_copy(mem, arr1);
    assert(arr_get_count(arr1) == 3);
    assert(arr_get_count(arr2) == 3);
    assert(arr_get_data(arr1) != arr_get_data(arr2));
    assert(arr_get(arr2, 0) == 10);
    assert(arr_get(arr2, 1) == 20);
    assert(arr_get(arr2, 2) == 30);

    arr_set(arr1, 0, 15);
    assert(arr_get(arr1, 0) == 15);
    assert(arr_get(arr2, 0) == 10);

    arr_destroy(arr1);
    arr_destroy(arr2);

    struct arr* empty_arr = arr_create(mem);
    struct arr* empty_arr_copy = arr_create_copy(mem, empty_arr);
    assert(arr_get_count(empty_arr) == 0);
    assert(arr_get_count(empty_arr_copy) == 0);
    arr_destroy(empty_arr);
    arr_destroy(empty_arr_copy);
}

void cr_test_arr(void) {
    struct cr_mem* mem = cr_mem_libc;

    test_arr_basic(mem);
    test_arr_append(mem);
    test_arr_remove(mem);
    test_arr_copy(mem);

    printf("cr_test_arr: ok\n");
}
