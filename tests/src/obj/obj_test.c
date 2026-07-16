#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "cr.h"
#include "obj.h"
#include "obj_tests.h"

void obj_test(void) {

    struct obj* null_obj = obj_create_null();
    struct obj* false_obj = obj_create_false();
    struct obj* true_obj = obj_create_true();
    assert(obj_is_null(null_obj));
    assert(obj_is_false(false_obj));
    assert(obj_is_true(true_obj));
    obj_destroy(null_obj);
    obj_destroy(false_obj);
    obj_destroy(true_obj);

    struct obj* object = obj_create_object();
    obj_object_set(object, "count", obj_create_number(42.0));
    obj_object_set(object, "name", obj_create_string("uniform"));

    struct obj* array = obj_create_array();
    obj_array_append(array, obj_create_number(1.0));
    obj_array_append(array, obj_create_number(2.0));
    obj_object_set(object, "items", array);

    assert(obj_number_get(obj_object_get(object, "count")) == 42.0);
    assert(strcmp(obj_string_get(obj_object_get(object, "name")), "uniform") == 0);
    assert(obj_object_get(object, "missing") == NULL);
    assert(obj_array_get_count(obj_object_get(object, "items")) == 2);
    assert(obj_number_get(obj_array_get(obj_object_get(object, "items"), 1)) == 2.0);

    struct obj* copy = obj_create_copy(object);
    assert(obj_number_get(obj_object_get(copy, "count")) == 42.0);
    assert(obj_array_get_count(obj_object_get(copy, "items")) == 2);

    struct cr_str* compact = obj_as_str(object, false);
    struct cr_str* compact_copy = obj_as_str(copy, false);
    assert(cr_str_is_equal(compact, compact_copy));
    assert(strstr(cr_str_get_data(compact), "\"count\":42") != NULL);
    assert(strstr(cr_str_get_data(compact), "\"name\":\"uniform\"") != NULL);
    cr_str_destroy(compact);
    cr_str_destroy(compact_copy);

    obj_destroy(copy);
    obj_destroy(object);

    printf("obj_test: ok\n");
}
