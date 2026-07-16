#include "cr.h"

#ifndef cr_dict
#error cr_dict not defined
#endif
#ifndef cr_dict_key
#error cr_dict_key not defined
#endif
#ifndef cr_dict_is_equal
#error cr_dict_is_equal not defined
#endif
#ifndef cr_dict_hash
#error cr_dict_hash not defined
#endif
#define cr_dict_iter cr_concat(cr_dict, _iter)
#define cr_dict_pair cr_concat(cr_dict, _pair)
#define cr_dict_priv cr_concat(_, cr_dict)
#define cr_dict_fn(fn) cr_concat3(cr_dict, _, fn)
#define cr_dict_priv_fn(fn) cr_concat4(_, cr_dict, _, fn)

#define cr_dict_flag_is_special ((uint8_t)0x80)
#define cr_dict_special_tmb ((uint8_t)0x80)
#define cr_dict_special_empty ((uint8_t)0xff)
#define cr_dict_index_flag_is_empty ((uint32_t)1 << 31)
#define cr_dict_flags cr_dict_index_flag_is_empty
#define cr_dict_block_capacity_none (-1)

struct cr_dict;

struct cr_dict_iter {
    struct cr_dict* dict;
    uint32_t index;
};

struct cr_dict_pair {
    cr_dict_key key;
#ifdef cr_dict_value
    cr_dict_value value;
#endif
};

struct cr_dict_priv {
    struct cr_mem* mem;
    uchar16* control;
    struct cr_dict_pair* pairs;
    uint32_t count;
    int32_t block_capacity_po2;
};

cr_hdef struct cr_dict* cr_dict_fn(create)(struct cr_mem* mem) {
    struct cr_dict_priv* dict = cr_malloc(mem, sizeof(*dict), 0, cr_loc);

    dict->mem = mem;
    dict->control = NULL;
    dict->pairs = NULL;
    dict->count = 0;
    dict->block_capacity_po2 = cr_dict_block_capacity_none;

    return (struct cr_dict*)dict;
}

cr_hdef void cr_dict_fn(destroy)(struct cr_dict* dict_) {
    struct cr_dict_priv* dict = (struct cr_dict_priv*)dict_;

    if (dict->control) {
        cr_free(dict->mem, dict->control, cr_loc);
        cr_free(dict->mem, dict->pairs, cr_loc);
    }
    cr_free(dict->mem, dict, cr_loc);
}

cr_hdef uint32_t cr_dict_fn(get_count)(struct cr_dict* dict_) {
    struct cr_dict_priv* dict = (struct cr_dict_priv*)dict_;

    return dict->count;
}

cr_hdef uint32_t cr_dict_fn(get_capacity)(struct cr_dict* dict_) {
    struct cr_dict_priv* dict = (struct cr_dict_priv*)dict_;

    return dict->block_capacity_po2 == cr_dict_block_capacity_none ? 0 : (1u << dict->block_capacity_po2) * 16u;
}

cr_hdef uint32_t cr_dict_fn(find_index)(struct cr_dict* dict_, cr_dict_key key, char op) {
    struct cr_dict_priv* dict = (struct cr_dict_priv*)dict_;

    if (dict->block_capacity_po2 == cr_dict_block_capacity_none) {
        return uint32_max;
    }

    uint64_t hash = cr_dict_hash(key);
    uint8_t h2 = (uint8_t)extract_bits_uint64(hash, 57, 7);
    uint64_t h1 = extract_bits_uint64(hash, 0, 57);

    uint32_t first_tmb = uint32_max;
    uint32_t result;

    uint32_t block_mask = (1u << dict->block_capacity_po2) - 1u;
    uint32_t index_mask = (1u << (dict->block_capacity_po2 + 4)) - 1u;
    uint32_t index_start = (h1 & block_mask) * 16;
    uint32_t index = index_start;

    while (1) {
        uchar16 block = dict->control[index >> 4];
        uint32_t index_in_block = index % 16;

        if (block[index_in_block] == h2) {
            struct cr_dict_pair* pair = dict->pairs + index;
            if (cr_dict_is_equal(pair->key, key)) {
                result = index;
                goto handle_result;
            }
        } else if (block[index_in_block] == cr_dict_special_empty) {
            result = (first_tmb == uint32_max ? index : first_tmb) | cr_dict_index_flag_is_empty;
            goto handle_result;
        } else if (block[index_in_block] == cr_dict_special_tmb && first_tmb == uint32_max) {
            first_tmb = index;
        }

        index = ((index + 1u) & index_mask);
        if (index == index_start) {
            break;
        }
    }
    
    result = first_tmb | cr_dict_index_flag_is_empty;

handle_result: ;
    if (op == 'c' && result & cr_dict_index_flag_is_empty) {
        assert(result != uint32_max);
        uint32_t block_index = (result & ~cr_dict_flags) >> 4;
        dict->control[block_index][result % 16] = h2;
        dict->count++;
    } else if (op == 'd' && (result & cr_dict_index_flag_is_empty) == 0) {
        index = (result + 1u) & index_mask;
        index_start = result;
        uint32_t block_start = result >> 4;
        bool needs_tmb = false;

        while (1) {
            uchar16 block = dict->control[index >> 4];
            uint32_t index_in_block = index % 16;

            if ((block[index_in_block] & cr_dict_flag_is_special) == 0) {
                struct cr_dict_pair* p_pair = dict->pairs + index;
                uint64_t p_hash = cr_dict_hash(p_pair->key);
                uint64_t p_h1 = extract_bits_uint64(p_hash, 0, 57);
                uint32_t p_block_home = p_h1 & block_mask;
                uint32_t d1 = ((index >> 4) - p_block_home) & block_mask;
                uint32_t d2 = ((index >> 4) - block_start) & block_mask;
                if (d1 >= d2) {
                    needs_tmb = true;
                    break;
                }
            } else if (block[index_in_block] == cr_dict_special_empty) {
                break;
            }

            index = ((index + 1) & index_mask);
            if (index == index_start) {
                break;
            }
        }

        dict->control[result >> 4][result % 16] = needs_tmb ? cr_dict_special_tmb : cr_dict_special_empty;
        dict->count--;
    }

    return result;
}

cr_hdef void cr_dict_priv_fn(resize)(struct cr_dict* dict_, int32_t new_block_capacity_po2) {
    struct cr_dict_priv* dict = (struct cr_dict_priv*)dict_;

    assert(new_block_capacity_po2 != dict->block_capacity_po2);
    assert(new_block_capacity_po2 >= 0);
    assert(new_block_capacity_po2 <= 14);
    assert(dict->count <= (1u << new_block_capacity_po2) * 16);

    uchar16* old_control = dict->control;
    struct cr_dict_pair* old_pairs = dict->pairs;
    int32_t old_block_capacity_po2 = dict->block_capacity_po2;

    uint32_t pairs_size = (1u << new_block_capacity_po2) * 16 * sizeof(struct cr_dict_pair);
    uint32_t control_size = (1u << new_block_capacity_po2) * sizeof(uchar16);

    dict->pairs = cr_malloc(dict->mem, pairs_size, 16, cr_loc);
    dict->control = cr_malloc(dict->mem, control_size, 16, cr_loc);
    dict->block_capacity_po2 = new_block_capacity_po2;
    dict->count = 0;

    memset(dict->control, cr_dict_special_empty, control_size);

    if (old_control) {
        uint32_t old_capacity = (1u << old_block_capacity_po2) * 16;

        for (uint32_t old_index = 0; old_index < old_capacity; old_index++) {
            uchar16 old_block = old_control[old_index >> 4];
            if ((old_block[old_index % 16] & cr_dict_flag_is_special) == 0) {
                struct cr_dict_pair* old_pair = old_pairs + old_index;

                uint32_t raw_index = cr_dict_fn(find_index)(dict_, old_pair->key, 'c');
                assert(raw_index & cr_dict_index_flag_is_empty);

                uint32_t index = raw_index & ~cr_dict_flags;
                struct cr_dict_pair* pair = dict->pairs + index;

                pair->key = old_pair->key;
#ifdef cr_dict_value
                pair->value = old_pair->value;
#endif
            }
        }

        cr_free(dict->mem, old_control, cr_loc);
        cr_free(dict->mem, old_pairs, cr_loc);
    }
}

cr_hdef inline __attribute__((always_inline)) bool cr_dict_priv_fn(set)(struct cr_dict* dict_, cr_dict_key key, struct cr_dict_pair** out_pair)  {
    struct cr_dict_priv* dict = (struct cr_dict_priv*)dict_;

#ifndef cr_dict_fix
    if (dict->block_capacity_po2 == cr_dict_block_capacity_none || dict->count >= (1u << dict->block_capacity_po2) * 13) {
        cr_dict_priv_fn(resize)(dict_, dict->block_capacity_po2 + 1);
    }
#else
    if (dict->block_capacity_po2 == cr_dict_block_capacity_none || dict->count == (1u << dict->block_capacity_po2) * 16) {
        assert(0 && "cannot resize to fit");
    }
#endif

    uint32_t raw_index = cr_dict_fn(find_index)(dict_, key, 'c');

    bool did_exist = (raw_index & cr_dict_index_flag_is_empty) == 0;

    uint32_t index = raw_index & ~cr_dict_flags;
    struct cr_dict_pair* pair = dict->pairs + index;

    if (did_exist == false) {
        pair->key = key;
    }

    *out_pair = pair;

    return did_exist;
}

cr_hdef inline __attribute__((always_inline)) bool cr_dict_priv_fn(get)(struct cr_dict* dict_, cr_dict_key key, struct cr_dict_pair** out_pair) {
    struct cr_dict_priv* dict = (struct cr_dict_priv*)dict_;

    uint32_t raw_index = cr_dict_fn(find_index)(dict_, key, 'r');

    bool did_exist = (raw_index & cr_dict_index_flag_is_empty) == 0;

    if (did_exist) {
        uint32_t index = raw_index & ~cr_dict_flags;
        *out_pair = dict->pairs + index;
    }

    return did_exist;
}

#ifdef cr_dict_value
cr_hdef bool cr_dict_fn(set_pair)(struct cr_dict* dict_, cr_dict_key key, struct cr_dict_pair** out_pair) {
    struct cr_dict_pair* pair;
    bool did_exist = cr_dict_priv_fn(set)(dict_, key, &pair);

    if (out_pair) {
        *out_pair = pair;
    }

    return did_exist;
}

cr_hdef bool cr_dict_fn(set_ptr)(struct cr_dict* dict_, cr_dict_key key, cr_dict_value** out_value_ptr) {
    struct cr_dict_pair* pair;
    bool did_exist = cr_dict_priv_fn(set)(dict_, key, &pair);

    if (out_value_ptr) {
        *out_value_ptr = &pair->value;
    }

    return did_exist;
}

cr_hdef bool cr_dict_fn(set)(struct cr_dict* dict_, cr_dict_key key, cr_dict_value value) {
    struct cr_dict_pair* pair;
    bool did_exist = cr_dict_priv_fn(set)(dict_, key, &pair);

    pair->value = value;

    return did_exist;
}

cr_hdef bool cr_dict_fn(get_ptr)(struct cr_dict* dict_, cr_dict_key key, cr_dict_value** out_value_ptr) {
    struct cr_dict_pair* pair;
    bool did_exist = cr_dict_priv_fn(get)(dict_, key, &pair);

    if (out_value_ptr) {
        *out_value_ptr = did_exist ? &pair->value : NULL;
    }

    return did_exist;
}

cr_hdef bool cr_dict_fn(get)(struct cr_dict* dict_, cr_dict_key key, cr_dict_value* out_value) {
    struct cr_dict_pair* pair;
    bool did_exist = cr_dict_priv_fn(get)(dict_, key, &pair);

    if (did_exist && out_value) {
        *out_value = pair->value;
    }

    return did_exist;
}
#else
cr_hdef bool cr_dict_fn(set)(struct cr_dict* dict_, cr_dict_key key) {
    struct cr_dict_pair* pair;
    bool did_exist = cr_dict_priv_fn(set)(dict_, key, &pair);

    return did_exist;
}

cr_hdef bool cr_dict_fn(get)(struct cr_dict* dict_, cr_dict_key key) {
    struct cr_dict_pair* pair;
    bool did_exist = cr_dict_priv_fn(get)(dict_, key, &pair);

    return did_exist;
}
#endif

cr_hdef bool cr_dict_fn(remove)(struct cr_dict* dict_, cr_dict_key key) {
#ifndef cr_dict_fix
    struct cr_dict_priv* dict = (struct cr_dict_priv*)dict_;

    if (dict->block_capacity_po2 > 0 && dict->count <= (1u << dict->block_capacity_po2) * 4) {
        cr_dict_priv_fn(resize)(dict_, dict->block_capacity_po2 - 1);
    }
#endif

    uint32_t raw_index = cr_dict_fn(find_index)(dict_, key, 'd');

    bool did_exist = (raw_index & cr_dict_index_flag_is_empty) == 0;

    return did_exist;
}

cr_hdef void cr_dict_fn(ensure_capacity)(struct cr_dict* dict_, uint32_t capacity) {
    struct cr_dict_priv* dict = (struct cr_dict_priv*)dict_;

    int32_t block_capacity_po2 = ctz_uint32(max_uint32(next_po2_uint32(capacity), 16) >> 4);

    if (block_capacity_po2 > dict->block_capacity_po2) {
        cr_dict_priv_fn(resize)(dict_, block_capacity_po2);
    }
}

cr_hdef struct cr_dict_iter cr_dict_fn(iter)(struct cr_dict* dict_) {
    struct cr_dict_iter iter = {
        .dict = dict_,
        .index = 0,
    };

    return iter;
}

cr_hdef bool cr_dict_fn(iter_next)(struct cr_dict_iter* inout_iter, struct cr_dict_pair** out_pair_ptr) {
    struct cr_dict_priv* dict = (struct cr_dict_priv*)inout_iter->dict;

    if (dict->count == 0) {
        return false;
    }

    uint32_t capacity = (1u << dict->block_capacity_po2) * 16;
    uint32_t index = inout_iter->index;

    if (index >= capacity) {
        return false;
    }

    for (; index < capacity; index++) {
        uchar16 block = dict->control[index >> 4];
        if ((block[index % 16] & cr_dict_flag_is_special) == 0) {
            *out_pair_ptr = dict->pairs + index;
            inout_iter->index = index + 1;
            return true;
        }
    }

    *out_pair_ptr = NULL;
    inout_iter->index = capacity;
    return false;
}

cr_hdef void cr_dict_fn(remove_all)(struct cr_dict* dict_) {
    struct cr_dict_priv* dict = (struct cr_dict_priv*)dict_;

#ifndef cr_dict_fix
    if (dict->control) {
        cr_free(dict->mem, dict->control, cr_loc);
        cr_free(dict->mem, dict->pairs, cr_loc);
        dict->control = NULL;
        dict->pairs = NULL;
    }
    dict->block_capacity_po2 = cr_dict_block_capacity_none;
#else
    if (dict->block_capacity_po2 != cr_dict_block_capacity_none) {
        uint32_t control_size = (1u << dict->block_capacity_po2) * sizeof(uchar16);
        memset(dict->control, cr_dict_special_empty, control_size);
    }
#endif

    dict->count = 0;
}

#undef cr_dict
#undef cr_dict_key
#undef cr_dict_value
#undef cr_dict_is_equal
#undef cr_dict_hash
#undef cr_dict_fix
#undef cr_dict_iter
#undef cr_dict_pair
#undef cr_dict_priv
#undef cr_dict_fn
#undef cr_dict_priv_fn

#undef cr_dict_flag_is_special
#undef cr_dict_special_tmb
#undef cr_dict_special_empty
#undef cr_dict_index_flag_is_empty
#undef cr_dict_block_capacity_none
