#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "cr.h"
#include "prim.h"
#include "prim_tests.h"

void prim_test(void) {
    struct cr_mem* mem = cr_mem_libc;

    struct prim* null_prim = prim_create_null(mem);
    struct prim* false_prim = prim_create_false(mem);
    struct prim* true_prim = prim_create_true(mem);
    assert(prim_is_null(null_prim));
    assert(prim_is_false(false_prim));
    assert(prim_is_true(true_prim));
    prim_destroy(null_prim);
    prim_destroy(false_prim);
    prim_destroy(true_prim);

    struct prim* object = prim_create_object(mem);
    prim_object_set(object, "count", prim_create_number(mem, 42.0));
    prim_object_set(object, "name", prim_create_string(mem, "uniform"));

    struct prim* array = prim_create_array(mem);
    prim_array_append(array, prim_create_number(mem, 1.0));
    prim_array_append(array, prim_create_number(mem, 2.0));
    prim_object_set(object, "items", array);

    assert(prim_number_get(prim_object_get(object, "count")) == 42.0);
    assert(strcmp(prim_string_get(prim_object_get(object, "name")), "uniform") == 0);
    assert(prim_object_get(object, "missing") == NULL);
    assert(prim_array_get_count(prim_object_get(object, "items")) == 2);
    assert(prim_number_get(prim_array_get(prim_object_get(object, "items"), 1)) == 2.0);

    struct prim* copy = prim_create_copy(mem, object);
    assert(prim_number_get(prim_object_get(copy, "count")) == 42.0);
    assert(prim_array_get_count(prim_object_get(copy, "items")) == 2);

    struct cr_str* compact = prim_as_str(mem, object, false);
    struct cr_str* compact_copy = prim_as_str(mem, copy, false);
    assert(cr_str_is_equal(compact, compact_copy));
    assert(strstr(cr_str_get_data(compact), "\"count\":42") != NULL);
    assert(strstr(cr_str_get_data(compact), "\"name\":\"uniform\"") != NULL);
    cr_str_destroy(compact);
    cr_str_destroy(compact_copy);

    prim_destroy(copy);
    prim_destroy(object);

    printf("prim_test: ok\n");
}
