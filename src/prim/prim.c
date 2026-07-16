#include "prim.h"
#include <stdio.h>
#include <stdlib.h>

enum prim_type {
    prim_type_null,
    prim_type_false,
    prim_type_true,
    prim_type_number,
    prim_type_string,
    prim_type_object,
    prim_type_array,
};

struct prim {
    struct cr_mem* mem;
    enum prim_type type;
    union {
        double number;
        struct cr_str* string;
        struct prim_dict* object;
        struct prim_arr* array;
    };
};

#define cr_arr prim_arr
#define cr_arr_item struct prim*
#include "cr_arr.h"

#define cr_dict prim_dict
#define cr_dict_key struct cr_str*
#define cr_dict_value struct prim*
#define cr_dict_is_equal cr_str_is_equal
#define cr_dict_hash cr_str_hash
#include "cr_dict.h"

struct prim* prim_create_null(struct cr_mem* mem) {
    struct prim* prim = cr_malloc(mem, sizeof(*prim), 0, cr_loc);
    prim->mem = mem;
    prim->type = prim_type_null;
    return prim;
}

struct prim* prim_create_false(struct cr_mem* mem) {
    struct prim* prim = cr_malloc(mem, sizeof(*prim), 0, cr_loc);
    prim->mem = mem;
    prim->type = prim_type_false;
    return prim;
}

struct prim* prim_create_true(struct cr_mem* mem) {
    struct prim* prim = cr_malloc(mem, sizeof(*prim), 0, cr_loc);
    prim->mem = mem;
    prim->type = prim_type_true;
    return prim;
}

struct prim* prim_create_number(struct cr_mem* mem, double number) {
    struct prim* prim = cr_malloc(mem, sizeof(*prim), 0, cr_loc);
    prim->mem = mem;
    prim->type = prim_type_number;
    prim->number = number;
    return prim;
}

struct prim* prim_create_string(struct cr_mem* mem, const char* string) {
    struct prim* prim = cr_malloc(mem, sizeof(*prim), 0, cr_loc);
    prim->mem = mem;
    prim->type = prim_type_string;
    prim->string = cr_str_create(mem, string);
    return prim;
}

struct prim* prim_create_object(struct cr_mem* mem) {
    struct prim* prim = cr_malloc(mem, sizeof(*prim), 0, cr_loc);
    prim->mem = mem;
    prim->type = prim_type_object;
    prim->object = prim_dict_create(mem);
    return prim;
}

struct prim* prim_create_array(struct cr_mem* mem) {
    struct prim* prim = cr_malloc(mem, sizeof(*prim), 0, cr_loc);
    prim->mem = mem;
    prim->type = prim_type_array;
    prim->array = prim_arr_create(mem);
    return prim;
}

struct prim* prim_create_copy(struct cr_mem* mem, struct prim* original) {
    assert(original);

    struct prim* prim = cr_malloc(mem, sizeof(*prim), 0, cr_loc);
    prim->mem = mem;
    prim->type = original->type;

    if (original->type == prim_type_number) {
        prim->number = original->number;
    } else if (original->type == prim_type_string) {
        prim->string = cr_str_create_copy(mem, original->string);
    } else if (original->type == prim_type_object) {
        prim->object = prim_dict_create(mem);

        struct prim_dict_iter iter = prim_dict_iter(original->object);
        struct prim_dict_pair* pair;

        while (prim_dict_iter_next(&iter, &pair)) {
            struct cr_str* key = cr_str_create_copy(mem, pair->key);
            struct prim* value = prim_create_copy(mem, pair->value);
            prim_dict_set(prim->object, key, value);
        }
    } else if (original->type == prim_type_array) {
        prim->array = prim_arr_create(mem);
        for (uint32_t i = 0, i_end = prim_arr_get_count(original->array); i < i_end; i++) {
            struct prim* value = prim_create_copy(mem, prim_arr_get(original->array, i));
            prim_arr_append(prim->array, value);
        }
    }

    return prim;
}

void prim_destroy(struct prim* prim) {
    assert(prim);

    if (prim->type == prim_type_string) {
        cr_str_destroy(prim->string);
    } else if (prim->type == prim_type_object) {
        struct prim_dict_iter iter = prim_dict_iter(prim->object);
        struct prim_dict_pair* pair;

        while (prim_dict_iter_next(&iter, &pair)) {
            cr_str_destroy(pair->key);
            prim_destroy(pair->value);
        }
        prim_dict_destroy(prim->object);
    } else if (prim->type == prim_type_array) {
        for (uint32_t i = 0, i_end = prim_arr_get_count(prim->array); i < i_end; i++) {
            prim_destroy(prim_arr_get(prim->array, i));
        }
        prim_arr_destroy(prim->array);
    }

    cr_free(prim->mem, prim, cr_loc);
}

bool prim_is_null(struct prim* prim) {
    assert(prim);
    return prim->type == prim_type_null;
}

bool prim_is_false(struct prim* prim) {
    assert(prim);
    return prim->type == prim_type_false;
}

bool prim_is_true(struct prim* prim) {
    assert(prim);
    return prim->type == prim_type_true;
}

bool prim_is_string(struct prim* prim) {
    assert(prim);
    return prim->type == prim_type_string;
}

bool prim_is_object(struct prim* prim) {
    assert(prim);
    return prim->type == prim_type_object;
}

double prim_number_get(struct prim* prim) {
    assert(prim);
    assert(prim->type == prim_type_number);
    return prim->number;
}

const char* prim_string_get(struct prim* prim) {
    assert(prim);
    assert(prim->type == prim_type_string);
    return cr_str_get_data(prim->string);
}

struct prim* prim_array_get(struct prim* prim, uint32_t index) {
    assert(prim);
    assert(prim->type == prim_type_array);

    return prim_arr_get(prim->array, index);
}

uint32_t prim_array_get_count(struct prim* prim) {
    assert(prim);
    assert(prim->type == prim_type_array);

    return prim_arr_get_count(prim->array);
}

void prim_array_append(struct prim* prim, struct prim* item) {
    assert(prim);
    assert(prim->type == prim_type_array);
    assert(item != NULL);

    prim_arr_append(prim->array, item);
}

struct prim* prim_object_get(struct prim* prim, const char* key) {
    assert(prim);
    assert(prim->type == prim_type_object);

    uint8_t temp[cr_str_size];
    struct cr_str* temp_key_str = cr_str_temp(temp, (char*)key);

    struct prim* result = NULL;
    prim_dict_get(prim->object, temp_key_str, &result);

    return result;
}

void prim_object_set(struct prim* prim, const char* key, struct prim* value) {
    assert(prim);
    assert(prim->type == prim_type_object);
    assert(value != NULL);

    uint8_t temp[cr_str_size];
    struct cr_str* temp_key_str = cr_str_temp(temp, (char*)key);

    struct prim_dict_pair* pair;
    bool did_exist = prim_dict_set_pair(prim->object, temp_key_str, &pair);

    if (did_exist) {
        prim_destroy(pair->value);
    } else {
        pair->key = cr_str_create(prim->mem, key);
    }
    pair->value = value;
}

struct kv_pair {
    struct cr_str* key;
    struct prim* value;
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

static struct cr_str* prim_as_str_internal(struct cr_mem* mem, struct prim* prim, bool pretty, int indent_level) {
    assert(prim);

    switch (prim->type) {
        case prim_type_null: {
            return cr_str_create(mem, "null");
        } break;

        case prim_type_false: {
            return cr_str_create(mem, "false");
        } break;

        case prim_type_true: {
            return cr_str_create(mem, "true");
        } break;

        case prim_type_number: {
            return cr_str_create_format(mem, "%g", prim->number);
        } break;

        case prim_type_string: {
            struct cr_str* result = cr_str_create_empty(mem);
            append_json_string(result, prim->string);
            return result;
        } break;

        case prim_type_array: {
            uint32_t count = prim_arr_get_count(prim->array);
            struct cr_str* result = cr_str_create(mem, "[");

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

                struct prim* item = prim_arr_get(prim->array, i);
                struct cr_str* item_str = prim_as_str_internal(mem, item, pretty, indent_level + 1);

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

        case prim_type_object: {
            uint32_t count = prim_dict_get_count(prim->object);
            struct cr_str* result = cr_str_create(mem, "{");

            if (count == 0) {
                cr_str_append_cstr(result, "}");
                return result;
            }

            if (pretty) {
                cr_str_append_cstr(result, "\n");
            }

            struct kv_pair* sorted_pairs = cr_malloc(mem, count * sizeof(struct kv_pair), 0, cr_loc);
            struct prim_dict_iter iter = prim_dict_iter(prim->object);
            struct prim_dict_pair* pair;
            uint32_t n = 0;

            while (prim_dict_iter_next(&iter, &pair)) {
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

                struct cr_str* val_str = prim_as_str_internal(mem, sorted_pairs[i].value, pretty, indent_level + 1);
                cr_str_append(result, val_str);
                cr_str_destroy(val_str);

                if (i < count - 1) {
                    cr_str_append_cstr(result, ",");
                }

                if (pretty) {
                    cr_str_append_cstr(result, "\n");
                }
            }

            cr_free(mem, sorted_pairs, cr_loc);

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

struct cr_str* prim_as_str(struct cr_mem* mem, struct prim* prim, bool pretty) {
    return prim_as_str_internal(mem, prim, pretty, 0);
}

void prim_print(struct prim* prim, bool pretty) {
    struct cr_str* str = prim_as_str(prim->mem, prim, pretty);
    printf("%s\n", cr_str_get_data(str));
    cr_str_destroy(str);
}
