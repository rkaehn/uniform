#include "cr.h"
#include <string.h>

#ifndef cr_arr
#error cr_arr not defined
#endif
#ifndef cr_arr_item
#error cr_arr_item not defined
#endif
#define cr_arr_priv cr_concat(_, cr_arr)
#define cr_arr_fn(fn) cr_concat3(cr_arr, _, fn)
#define cr_arr_priv_fn(fn) cr_concat4(_, cr_arr, _, fn)

struct cr_arr;

struct cr_arr_priv {
    struct cr_mem* mem;
    uint32_t capacity;
    uint32_t count;
    cr_arr_item* data;
};

cr_hdef struct cr_arr* cr_arr_fn(create)(struct cr_mem* mem) {
    struct cr_arr_priv* arr = cr_malloc(mem, sizeof(*arr), 0, cr_loc);

    arr->mem = mem;
    arr->capacity = 0;
    arr->count = 0;
    arr->data = NULL;

    return (struct cr_arr*)arr;
}

cr_hdef void cr_arr_fn(destroy)(struct cr_arr* arr_) {
    struct cr_arr_priv* arr = (struct cr_arr_priv*)arr_;

    if (arr->data) {
        cr_free(arr->mem, arr->data, cr_loc);
    }
    cr_free(arr->mem, arr, cr_loc);
}

cr_hdef uint32_t cr_arr_fn(get_count)(struct cr_arr* arr_) {
    struct cr_arr_priv* arr = (struct cr_arr_priv*)arr_;
    return arr->count;
}

cr_hdef cr_arr_item* cr_arr_fn(get_data)(struct cr_arr* arr_) {
    struct cr_arr_priv* arr = (struct cr_arr_priv*)arr_;
    return arr->data;
}

cr_hdef void cr_arr_priv_fn(ensure_capacity)(struct cr_arr* arr_, uint32_t new_capacity) {
    struct cr_arr_priv* arr = (struct cr_arr_priv*)arr_;

    new_capacity = max_uint32(16u, next_po2_uint32(new_capacity));

    if (arr->capacity < new_capacity) {
        uint64_t new_size = (uint64_t)new_capacity * sizeof(cr_arr_item);

        arr->data = cr_realloc(arr->mem, arr->data, new_size, cr_loc);
        arr->capacity = new_capacity;
    }
}

cr_hdef void cr_arr_priv_fn(resize)(struct cr_arr* arr_, uint32_t new_count) {
    struct cr_arr_priv* arr = (struct cr_arr_priv*)arr_;

    cr_arr_priv_fn(ensure_capacity)(arr_, new_count);

    arr->count = new_count;
}

cr_hdef cr_arr_item* cr_arr_fn(append_ptr)(struct cr_arr* arr_, uint32_t item_count) {
    struct cr_arr_priv* arr = (struct cr_arr_priv*)arr_;
    assert(item_count > 0);

    uint32_t count = arr->count;

    cr_arr_priv_fn(resize)(arr_, count + item_count);

    return &arr->data[count];
}

cr_hdef void cr_arr_fn(append)(struct cr_arr* arr_, cr_arr_item item) {
    cr_arr_item* data = cr_arr_fn(append_ptr)(arr_, 1);

    *data = item;
}

cr_hdef void cr_arr_fn(append_items)(struct cr_arr* arr_, cr_arr_item* items, uint32_t item_count) {
    cr_arr_item* data = cr_arr_fn(append_ptr)(arr_, item_count);

    memcpy(data, items, item_count * sizeof(cr_arr_item));
}

cr_hdef void cr_arr_fn(remove)(struct cr_arr* arr_, uint32_t index) {
    struct cr_arr_priv* arr = (struct cr_arr_priv*)arr_;

    assert(index < arr->count);

    if (index != arr->count - 1) {
        uint32_t move_count = arr->count - index - 1;

        memmove(arr->data + index, arr->data + index + 1, move_count * sizeof(cr_arr_item));
    }

    cr_arr_priv_fn(resize)(arr_, arr->count - 1);
}

cr_hdef void cr_arr_fn(remove_all)(struct cr_arr* arr_) {
    struct cr_arr_priv* arr = (struct cr_arr_priv*)arr_;

    if (arr->count > 0) {
        cr_arr_priv_fn(resize)(arr_, 0);
    }
}

cr_hdef cr_arr_item* cr_arr_fn(get_ptr)(const struct cr_arr* arr_, uint32_t index) {
    const struct cr_arr_priv* arr = (const struct cr_arr_priv*)arr_;

    assert(index < arr->count);

    return &arr->data[index];
}

cr_hdef cr_arr_item cr_arr_fn(get)(const struct cr_arr* arr_, uint32_t index) {
    return *cr_arr_fn(get_ptr)(arr_, index);
}

cr_hdef void cr_arr_fn(set)(struct cr_arr* arr_, uint32_t index, cr_arr_item item) {
    struct cr_arr_priv* arr = (struct cr_arr_priv*)arr_;

    assert(index < arr->count);

    arr->data[index] = item;
}

cr_hdef struct cr_arr* cr_arr_fn(create_copy)(struct cr_mem* mem, struct cr_arr* src_) {
    struct cr_arr_priv* src = (struct cr_arr_priv*)src_;

    struct cr_arr* arr_ = cr_arr_fn(create)(mem);

    if (src->count > 0) {
        cr_arr_fn(append_items)(arr_, src->data, src->count);
    }

    return arr_;
}

#undef cr_arr_item
#undef cr_arr
#undef cr_arr_priv
#undef cr_arr_fn
#undef cr_arr_priv_fn
