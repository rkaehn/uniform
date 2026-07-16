#include "obj.h"
#include <stdio.h>
#include <stdlib.h>

enum obj_type {
    obj_type_null,
    obj_type_false,
    obj_type_true,
    obj_type_number,
    obj_type_string,
    obj_type_object,
    obj_type_array,
};

struct obj {
    enum obj_type type;
    union {
        double number;
        struct cr_str* string;
        struct obj_dict* object;
        struct obj_arr* array;
    };
};

#define cr_arr obj_arr
#define cr_arr_item struct obj*
#include "cr_arr.h"

#define cr_dict obj_dict
#define cr_dict_key struct cr_str*
#define cr_dict_value struct obj*
#define cr_dict_is_equal cr_str_is_equal
#define cr_dict_hash cr_str_hash
#include "cr_dict.h"

struct obj* obj_create_null(void) {
    struct obj* obj = cr_malloc(sizeof(*obj), 0, cr_loc);
    obj->type = obj_type_null;
    return obj;
}

struct obj* obj_create_false(void) {
    struct obj* obj = cr_malloc(sizeof(*obj), 0, cr_loc);
    obj->type = obj_type_false;
    return obj;
}

struct obj* obj_create_true(void) {
    struct obj* obj = cr_malloc(sizeof(*obj), 0, cr_loc);
    obj->type = obj_type_true;
    return obj;
}

struct obj* obj_create_number(double number) {
    struct obj* obj = cr_malloc(sizeof(*obj), 0, cr_loc);
    obj->type = obj_type_number;
    obj->number = number;
    return obj;
}

struct obj* obj_create_string(const char* string) {
    struct obj* obj = cr_malloc(sizeof(*obj), 0, cr_loc);
    obj->type = obj_type_string;
    obj->string = cr_str_create(string);
    return obj;
}

struct obj* obj_create_object(void) {
    struct obj* obj = cr_malloc(sizeof(*obj), 0, cr_loc);
    obj->type = obj_type_object;
    obj->object = obj_dict_create();
    return obj;
}

struct obj* obj_create_array(void) {
    struct obj* obj = cr_malloc(sizeof(*obj), 0, cr_loc);
    obj->type = obj_type_array;
    obj->array = obj_arr_create();
    return obj;
}

struct obj* obj_create_copy(struct obj* original) {
    assert(original);

    struct obj* obj = cr_malloc(sizeof(*obj), 0, cr_loc);
    obj->type = original->type;

    if (original->type == obj_type_number) {
        obj->number = original->number;
    } else if (original->type == obj_type_string) {
        obj->string = cr_str_create_copy(original->string);
    } else if (original->type == obj_type_object) {
        obj->object = obj_dict_create();

        struct obj_dict_iter iter = obj_dict_iter(original->object);
        struct obj_dict_pair* pair;

        while (obj_dict_iter_next(&iter, &pair)) {
            struct cr_str* key = cr_str_create_copy(pair->key);
            struct obj* value = obj_create_copy(pair->value);
            obj_dict_set(obj->object, key, value);
        }
    } else if (original->type == obj_type_array) {
        obj->array = obj_arr_create();
        for (uint32_t i = 0, i_end = obj_arr_get_count(original->array); i < i_end; i++) {
            struct obj* value = obj_create_copy(obj_arr_get(original->array, i));
            obj_arr_append(obj->array, value);
        }
    }

    return obj;
}

void obj_destroy(struct obj* obj) {
    assert(obj);

    if (obj->type == obj_type_string) {
        cr_str_destroy(obj->string);
    } else if (obj->type == obj_type_object) {
        struct obj_dict_iter iter = obj_dict_iter(obj->object);
        struct obj_dict_pair* pair;

        while (obj_dict_iter_next(&iter, &pair)) {
            cr_str_destroy(pair->key);
            obj_destroy(pair->value);
        }
        obj_dict_destroy(obj->object);
    } else if (obj->type == obj_type_array) {
        for (uint32_t i = 0, i_end = obj_arr_get_count(obj->array); i < i_end; i++) {
            obj_destroy(obj_arr_get(obj->array, i));
        }
        obj_arr_destroy(obj->array);
    }

    cr_free(obj, cr_loc);
}

bool obj_is_null(struct obj* obj) {
    assert(obj);
    return obj->type == obj_type_null;
}

bool obj_is_false(struct obj* obj) {
    assert(obj);
    return obj->type == obj_type_false;
}

bool obj_is_true(struct obj* obj) {
    assert(obj);
    return obj->type == obj_type_true;
}

bool obj_is_string(struct obj* obj) {
    assert(obj);
    return obj->type == obj_type_string;
}

bool obj_is_object(struct obj* obj) {
    assert(obj);
    return obj->type == obj_type_object;
}

double obj_number_get(struct obj* obj) {
    assert(obj);
    assert(obj->type == obj_type_number);
    return obj->number;
}

const char* obj_string_get(struct obj* obj) {
    assert(obj);
    assert(obj->type == obj_type_string);
    return cr_str_get_data(obj->string);
}

struct obj* obj_array_get(struct obj* obj, uint32_t index) {
    assert(obj);
    assert(obj->type == obj_type_array);

    return obj_arr_get(obj->array, index);
}

uint32_t obj_array_get_count(struct obj* obj) {
    assert(obj);
    assert(obj->type == obj_type_array);

    return obj_arr_get_count(obj->array);
}

void obj_array_append(struct obj* obj, struct obj* item) {
    assert(obj);
    assert(obj->type == obj_type_array);
    assert(item != NULL);

    obj_arr_append(obj->array, item);
}

struct obj* obj_object_get(struct obj* obj, const char* key) {
    assert(obj);
    assert(obj->type == obj_type_object);

    uint8_t temp[cr_str_size];
    struct cr_str* temp_key_str = cr_str_temp(temp, (char*)key);

    struct obj* result = NULL;
    obj_dict_get(obj->object, temp_key_str, &result);

    return result;
}

void obj_object_set(struct obj* obj, const char* key, struct obj* value) {
    assert(obj);
    assert(obj->type == obj_type_object);
    assert(value != NULL);

    uint8_t temp[cr_str_size];
    struct cr_str* temp_key_str = cr_str_temp(temp, (char*)key);

    struct obj_dict_pair* pair;
    bool did_exist = obj_dict_set_pair(obj->object, temp_key_str, &pair);

    if (did_exist) {
        obj_destroy(pair->value);
    } else {
        pair->key = cr_str_create(key);
    }
    pair->value = value;
}

struct kv_pair {
    struct cr_str* key;
    struct obj* value;
};

static int compare_kv_pairs(const void* a, const void* b) {
    const struct kv_pair* pa = (const struct kv_pair*)a;
    const struct kv_pair* pb = (const struct kv_pair*)b;
    return strcmp(cr_str_get_data(pa->key), cr_str_get_data(pb->key));
}

static void append_indent(struct cr_str* str, int level) {
    for (int i = 0; i < level * 4; i++) {
        cr_str_append_cstr(str, " ");
    }
}

static void append_json_string(struct cr_str* str, struct cr_str* value) {
    cr_str_append_cstr(str, "\"");

    const char* data = cr_str_get_data(value);
    for (uint32_t i = 0, i_end = cr_str_get_count(value); i < i_end; i++) {
        uint8_t c = (uint8_t)data[i];
        if (c == '"') {
            cr_str_append_cstr(str, "\\\"");
        } else if (c == '\\') {
            cr_str_append_cstr(str, "\\\\");
        } else if (c == '\b') {
            cr_str_append_cstr(str, "\\b");
        } else if (c == '\f') {
            cr_str_append_cstr(str, "\\f");
        } else if (c == '\n') {
            cr_str_append_cstr(str, "\\n");
        } else if (c == '\r') {
            cr_str_append_cstr(str, "\\r");
        } else if (c == '\t') {
            cr_str_append_cstr(str, "\\t");
        } else if (c < 0x20) {
            cr_str_append_format(str, "\\u%04x", c);
        } else {
            cr_str_append_data(str, &data[i], 1);
        }
    }

    cr_str_append_cstr(str, "\"");
}

static struct cr_str* obj_as_str_internal(struct obj* obj, bool pretty, int indent_level) {
    assert(obj);

    switch (obj->type) {
        case obj_type_null: {
            return cr_str_create("null");
        } break;

        case obj_type_false: {
            return cr_str_create("false");
        } break;

        case obj_type_true: {
            return cr_str_create("true");
        } break;

        case obj_type_number: {
            return cr_str_create_format("%g", obj->number);
        } break;

        case obj_type_string: {
            struct cr_str* result = cr_str_create_empty();
            append_json_string(result, obj->string);
            return result;
        } break;

        case obj_type_array: {
            uint32_t count = obj_arr_get_count(obj->array);
            struct cr_str* result = cr_str_create("[");

            if (count == 0) {
                cr_str_append_cstr(result, "]");
                return result;
            }

            if (pretty) {
                cr_str_append_cstr(result, "\n");
            }

            for (uint32_t i = 0; i < count; i++) {
                if (pretty) {
                    append_indent(result, indent_level + 1);
                }

                struct obj* item = obj_arr_get(obj->array, i);
                struct cr_str* item_str = obj_as_str_internal(item, pretty, indent_level + 1);

                cr_str_append(result, item_str);
                cr_str_destroy(item_str);

                if (i < count - 1) {
                    cr_str_append_cstr(result, ",");
                }

                if (pretty) {
                    cr_str_append_cstr(result, "\n");
                }
            }

            if (pretty) {
                append_indent(result, indent_level);
            }

            cr_str_append_cstr(result, "]");

            return result;
        }

        case obj_type_object: {
            uint32_t count = obj_dict_get_count(obj->object);
            struct cr_str* result = cr_str_create("{");

            if (count == 0) {
                cr_str_append_cstr(result, "}");
                return result;
            }

            if (pretty) {
                cr_str_append_cstr(result, "\n");
            }

            struct kv_pair* sorted_pairs = cr_malloc(count * sizeof(struct kv_pair), 0, cr_loc);
            struct obj_dict_iter iter = obj_dict_iter(obj->object);
            struct obj_dict_pair* pair;
            uint32_t n = 0;

            while (obj_dict_iter_next(&iter, &pair)) {
                sorted_pairs[n].key = pair->key;
                sorted_pairs[n].value = pair->value;
                n++;
            }

            qsort(sorted_pairs, count, sizeof(struct kv_pair), compare_kv_pairs);

            for (uint32_t i = 0; i < count; i++) {
                if (pretty) {
                    append_indent(result, indent_level + 1);
                }

                append_json_string(result, sorted_pairs[i].key);
                cr_str_append_cstr(result, ":");

                if (pretty) {
                    cr_str_append_cstr(result, " ");
                }

                struct cr_str* val_str = obj_as_str_internal(sorted_pairs[i].value, pretty, indent_level + 1);
                cr_str_append(result, val_str);
                cr_str_destroy(val_str);

                if (i < count - 1) {
                    cr_str_append_cstr(result, ",");
                }

                if (pretty) {
                    cr_str_append_cstr(result, "\n");
                }
            }

            cr_free(sorted_pairs, cr_loc);

            if (pretty) {
                append_indent(result, indent_level);
            }

            cr_str_append_cstr(result, "}");

            return result;
        }

        default:
            assert(0);
    }
}

struct cr_str* obj_as_str(struct obj* obj, bool pretty) {
    return obj_as_str_internal(obj, pretty, 0);
}

void obj_print(struct obj* obj, bool pretty) {
    struct cr_str* str = obj_as_str(obj, pretty);
    printf("%s\n", cr_str_get_data(str));
    cr_str_destroy(str);
}
