#include "tree.h"
#include "cr.h"
#include <stdio.h>
#include <stdlib.h>

#define cr_tree_level_bits 6
#define cr_tree_index_bits (32 - cr_tree_level_bits)
#define cr_tree_level_count_max (1u << cr_tree_level_bits)

struct cr_tree_node_ref {
    struct cr_tree* tree;
    uint32_t index;
    uint32_t owners;
};

struct cr_tree_node_header {
    struct cr_tree_node_ref* ref;
    struct cr_tree_loc loc;
    uint32_t parent_index;
    uint32_t first_child_index;
    uint32_t next_index;
    uint32_t prev_index;
    uint32_t is_selected;
    uint8_t _[cr_tree_node_header_size - sizeof(struct cr_tree_node_ref*) - 6 * sizeof(uint32_t)];
};
static_assert(sizeof(struct cr_tree_node_header) == cr_tree_node_header_size, "");

struct cr_tree {
    struct cr_mem* mem;
    struct cr_mem* temp_mem;
    size_t node_size;
    uint32_t node_capacity;
    uint32_t node_count;
    void* node_data;
    struct {
        struct cr_tree_loc_2* moves;
        uint32_t move_count;
    } drag;
    void (*on_move)(const struct cr_tree_loc* adds, uint32_t add_count, const struct cr_tree_loc* deletes, uint32_t delete_count, const struct cr_tree_loc_2* moves, uint32_t move_count, const struct cr_tree_node* nodes, void* user);
    void* on_move_user;
};

static void cr_tree_ensure_capacity(struct cr_tree* tree, uint32_t new_capacity);

struct cr_tree_loc cr_tree_loc_make_index(uint32_t index) {
    assert(index < (1u << cr_tree_index_bits));

    return (struct cr_tree_loc) {
        .raw = index,
    };
}

struct cr_tree_loc cr_tree_loc_make_index_level(uint32_t index, uint32_t level) {
    return (struct cr_tree_loc) {
        .raw = (level << cr_tree_index_bits) | index,
    };
}

uint32_t cr_tree_loc_get_index(struct cr_tree_loc loc) {
    return loc.raw & ((1u << cr_tree_index_bits) - 1);
}

uint32_t cr_tree_loc_get_level(struct cr_tree_loc loc) {
    return loc.raw >> cr_tree_index_bits;
}

struct cr_tree_loc_2 cr_tree_loc_2_make(uint32_t src_index, uint32_t src_level, uint32_t dst_index, uint32_t dst_level) {
    return (struct cr_tree_loc_2) {
        .src.raw = (src_level << cr_tree_index_bits) | src_index,
        .dst.raw = (dst_level << cr_tree_index_bits) | dst_index,
    };
}

struct cr_tree_node_ref* cr_tree_node_ref_retain(struct cr_tree* tree, uint32_t index, uint32_t owner) {
    assert(popcount_uint32(owner) == 1);

    struct cr_tree_node_header* header = (struct cr_tree_node_header*)cr_tree_node_at(tree->node_data, index, tree->node_size);

    if (header->ref == NULL) {
        header->ref = cr_malloc(tree->mem, sizeof(struct cr_tree_node_ref), 0, cr_loc);
        header->ref->tree = tree;
        header->ref->index = index;
        header->ref->owners = owner;
    } else {
        assert((header->ref->owners & owner) == 0);
        header->ref->owners |= owner;
    }

    return header->ref;
}

void cr_tree_node_ref_release(struct cr_tree_node_ref* ref, uint32_t owner) {
    assert(popcount_uint32(owner) == 1);

    struct cr_tree_node_header* header = (struct cr_tree_node_header*)cr_tree_node_at(ref->tree->node_data, ref->index, ref->tree->node_size);

    assert(header->ref->owners & owner);

    header->ref->owners &= ~(uint32_t)owner;

    if (header->ref->owners == 0) {
        cr_free(ref->tree->mem, header->ref, cr_loc);
        header->ref = NULL;
    }
}

uint32_t cr_tree_node_ref_get_index(struct cr_tree_node_ref* ref) {
    return ref->index;
}

struct cr_tree_node* cr_tree_node_ref_ptr(struct cr_tree_node_ref* ref) {
    return cr_tree_node_at(ref->tree->node_data, ref->index, ref->tree->node_size);
}

void cr_tree_node_init(struct cr_tree_node* node) {
    struct cr_tree_node_header* header = (struct cr_tree_node_header*)node;

    header->ref = NULL;
    header->loc = cr_tree_loc_make_index_level(uint_max, uint_max);
    header->parent_index = uint_max;
    header->first_child_index = uint_max;
    header->next_index = uint_max;
    header->prev_index = uint_max;
    header->is_selected = false;
}

uint32_t cr_tree_node_get_first_child_index(struct cr_tree_node* node) {
    return ((struct cr_tree_node_header*)node)->first_child_index;
}

uint32_t cr_tree_node_get_parent_index(struct cr_tree_node* node) {
    return ((struct cr_tree_node_header*)node)->parent_index;
}

uint32_t cr_tree_node_get_next_index(struct cr_tree_node* node) {
    return ((struct cr_tree_node_header*)node)->next_index;
}

uint32_t cr_tree_node_get_prev_index(struct cr_tree_node* node) {
    return ((struct cr_tree_node_header*)node)->prev_index;
}

struct cr_tree_loc cr_tree_node_get_loc(struct cr_tree_node* node) {
    return ((struct cr_tree_node_header*)node)->loc;
}

uint32_t cr_tree_node_get_index(struct cr_tree_node* node) {
    struct cr_tree_loc loc = ((struct cr_tree_node_header*)node)->loc;
    return cr_tree_loc_get_index(loc);
}

uint32_t cr_tree_node_get_level(struct cr_tree_node* node) {
    struct cr_tree_loc loc = ((struct cr_tree_node_header*)node)->loc;
    return cr_tree_loc_get_level(loc);
}

bool cr_tree_node_get_is_selected(struct cr_tree_node* node) {
    return ((struct cr_tree_node_header*)node)->is_selected;
}

void* cr_tree_node_get_content(struct cr_tree_node* node) {
    return (char*)node + sizeof(struct cr_tree_node_header);
}

struct cr_tree* cr_tree_create(struct cr_mem* mem, struct cr_mem* temp_mem, size_t content_size) {
    struct cr_tree* tree = cr_malloc(mem, sizeof(*tree), 0, cr_loc);

    memset(tree, 0, sizeof(*tree));
    tree->mem = mem;
    tree->temp_mem = temp_mem;
    tree->node_size = cr_tree_node_size(content_size);

    return tree;
}

void cr_tree_destroy(struct cr_tree* tree) {
    if (tree->drag.moves) {
        cr_free(tree->temp_mem, tree->drag.moves, cr_loc);
    }
    if (tree->node_data) {
        cr_free(tree->mem, tree->node_data, cr_loc);
    }
    cr_free(tree->mem, tree, cr_loc);
}

void cr_tree_set_on_move(struct cr_tree* tree, void (*on_move)(const struct cr_tree_loc* adds, uint32_t add_count, const struct cr_tree_loc* deletes, uint32_t delete_count, const struct cr_tree_loc_2* moves, uint32_t move_count, const struct cr_tree_node* nodes, void* user), void* user) {
    tree->on_move = on_move;
    tree->on_move_user = user;
}

uint32_t cr_tree_get_node_count(struct cr_tree* tree) {
    return tree->node_count;
}

struct cr_tree_node* cr_tree_get_node(struct cr_tree* tree, uint32_t index) {
    assert(index < tree->node_count);

    return cr_tree_node_at(tree->node_data, index, tree->node_size);
}

uint32_t cr_tree_get_next_index(struct cr_tree* tree, uint32_t index) {
    while (1) {
        struct cr_tree_node_header* header = (struct cr_tree_node_header*)cr_tree_node_at(tree->node_data, index, tree->node_size);
        if (header->next_index != uint_max) {
            return header->next_index;
        } else if (header->parent_index != uint_max) {
            index = header->parent_index;
        } else {
            return tree->node_count;
        }
    }
}

static void cr_tree_apply_update(struct cr_tree* tree, struct cr_tree_node_header* header, uint32_t dst_index, uint32_t* last_index, uint32_t last_index_count) {
    uint32_t dst_level = cr_tree_loc_get_level(header->loc);

    uint32_t prev_index = last_index[dst_level];
    header->prev_index = prev_index;
    header->next_index = uint_max;
    if (prev_index != uint_max) {
        struct cr_tree_node* node = cr_tree_node_at(tree->node_data, prev_index, tree->node_size);
        ((struct cr_tree_node_header*)node)->next_index = dst_index;
    }

    uint32_t parent_index = dst_level > 0 ? last_index[dst_level - 1] : uint_max;
    header->parent_index = parent_index;
    header->first_child_index = uint_max;
    if ((parent_index != uint_max) && (prev_index == uint_max)) {
        struct cr_tree_node* node = cr_tree_node_at(tree->node_data, parent_index, tree->node_size);
        ((struct cr_tree_node_header*)node)->first_child_index = dst_index;
    }

    last_index[dst_level] = dst_index;
    for (uint32_t i = dst_level + 1; i < last_index_count; i++) {
        last_index[i] = uint_max;
    }
}

void cr_tree_insert(struct cr_tree* tree, struct cr_tree_insertion* insertions, uint32_t insertion_count, const struct cr_tree_node* nodes) {
    if (insertion_count == 0) {
        return;
    }

    assert(tree->drag.moves == NULL);

    uint32_t dst_count = tree->node_count + insertion_count;

    cr_tree_ensure_capacity(tree, dst_count);

    memmove(cr_tree_node_at(tree->node_data, insertion_count, tree->node_size), tree->node_data, tree->node_count * tree->node_size);

    struct cr_tree_insertion* insertion = insertions;
    struct cr_tree_insertion* insertion_end = insertion + insertion_count;
    uint32_t index_insertion = 0;
    uint32_t index_existing = insertion_count;

    uint32_t last_index[cr_tree_level_count_max];
    memset(last_index, 0xff, sizeof(last_index));

    for (uint32_t dst_index = 0; dst_index < dst_count; dst_index++) {
        bool is_src_insertion = (insertion != insertion_end) && (dst_index == insertion->index);
        struct cr_tree_node* src;
        if (is_src_insertion) {
            src = cr_tree_node_at(nodes, index_insertion, tree->node_size);
        } else {
            src = cr_tree_node_at(tree->node_data, index_existing, tree->node_size);
        }
        struct cr_tree_node* dst = cr_tree_node_at(tree->node_data, dst_index, tree->node_size);

        struct cr_tree_node_header* src_header = (struct cr_tree_node_header*)src;
        struct cr_tree_node_header* dst_header = (struct cr_tree_node_header*)dst;

        assert((dst == src) == (insertion == insertion_end));

        if (dst != src) {
            memcpy(dst, src, tree->node_size);
        }

        uint32_t dst_level = is_src_insertion ? insertion->level : cr_tree_loc_get_level(src_header->loc);
        assert(dst_level < cr_tree_level_count_max);
        dst_header->loc = cr_tree_loc_make_index_level(dst_index, dst_level);
        if (dst_header->ref) {
            dst_header->ref->index = dst_index;
        }

        cr_tree_apply_update(tree, dst_header, dst_index, last_index, cr_count(last_index));

        if (is_src_insertion) {
            insertion++;
            index_insertion++;
        } else {
            index_existing++;
        }
    }

    assert(insertion == insertion_end);

    tree->node_count = dst_count;
}

void cr_tree_remove(struct cr_tree* tree, struct cr_tree_removal* removals, uint32_t removal_count, bool adjust_capacity) {
    if (removal_count == 0) {
        return;
    }

    assert(tree->drag.moves == NULL);

    uint32_t src_count = tree->node_count;
    assert(removal_count <= src_count);

    struct cr_tree_removal* removal = removals;
    struct cr_tree_removal* removal_end = removal + removal_count;
    uint32_t dst_index = 0;

    uint32_t last_index[cr_tree_level_count_max];
    memset(last_index, 0xff, sizeof(last_index));

    for (uint32_t src_index = 0; src_index < src_count; src_index++) {
        if ((removal != removal_end) && (src_index == removal->index)) {
            removal++;
            continue;
        }

        struct cr_tree_node* src = cr_tree_node_at(tree->node_data, src_index, tree->node_size);
        struct cr_tree_node* dst = cr_tree_node_at(tree->node_data, dst_index, tree->node_size);

        struct cr_tree_node_header* src_header = (struct cr_tree_node_header*)src;
        struct cr_tree_node_header* dst_header = (struct cr_tree_node_header*)dst;

        assert((dst == src) == (removal == removals));

        if (dst != src) {
            memcpy(dst, src, tree->node_size);

            uint32_t src_level = cr_tree_loc_get_level(src_header->loc);
            dst_header->loc = cr_tree_loc_make_index_level(dst_index, src_level);
            if (dst_header->ref) {
                dst_header->ref->index = dst_index;
            }
        }

        cr_tree_apply_update(tree, dst_header, dst_index, last_index, cr_count(last_index));

        dst_index++;
    }

    assert(removal == removal_end);

    tree->node_count = tree->node_count - removal_count;

    if (adjust_capacity) {
        cr_tree_ensure_capacity(tree, tree->node_count);
    }
}

static void cr_tree_ensure_capacity(struct cr_tree* tree, uint32_t new_capacity) {
    new_capacity = max_uint32(16u, next_po2_uint32(new_capacity));

    if (tree->node_capacity != new_capacity) {
        size_t new_size = new_capacity * tree->node_size;

        tree->node_data = cr_realloc(tree->mem, tree->node_data, new_size, cr_loc);
        tree->node_capacity = new_capacity;
    }
}

void cr_tree_select(struct cr_tree* tree, const struct cr_tree_loc* locs, uint32_t loc_count, enum cr_tree_selection_mode mode) {
    const struct cr_tree_loc* loc = locs;
    const struct cr_tree_loc* loc_end = loc ? loc + loc_count : NULL;

    if (loc_count > 0) {
        uint32_t prev_index = cr_tree_loc_get_index(locs[0]);
        for (uint32_t i = 1; i < loc_count; i++) {
            uint32_t index = cr_tree_loc_get_index(locs[i]);
            assert(index > prev_index);
            prev_index = index;
        }
    }

    switch (mode) {
    case cr_tree_selection_mode_add: {
        for (; loc != loc_end; loc++) {
            uint32_t index = cr_tree_loc_get_index(*loc);
            struct cr_tree_node_header* header = (struct cr_tree_node_header*)cr_tree_node_at(tree->node_data, index, tree->node_size);
            header->is_selected = true;
        }
    } break;
    case cr_tree_selection_mode_subtract: {
        for (; loc != loc_end; loc++) {
            uint32_t index = cr_tree_loc_get_index(*loc);
            struct cr_tree_node_header* header = (struct cr_tree_node_header*)cr_tree_node_at(tree->node_data, index, tree->node_size);
            header->is_selected = false;
        }
    } break;
    case cr_tree_selection_mode_replace: {
        uint32_t node_count = tree->node_count;
        for (uint32_t i = 0; i < node_count; i++) {
            struct cr_tree_node_header* header = (struct cr_tree_node_header*)cr_tree_node_at(tree->node_data, i, tree->node_size);
            if ((loc != loc_end) && (cr_tree_loc_get_index(*loc) == i)) {
                header->is_selected = true;
                loc++;
            } else {
                header->is_selected = false;
            }
        }
    } break;
    }
}

static int cr_tree_move_cmp_removals(const void* _a, const void* _b) {
    const struct cr_tree_removal* a = _a;
    const struct cr_tree_removal* b = _b;
    if (a->index < b->index) return -1;
    if (a->index > b->index) return 1;
    return 0;
}

void cr_tree_move(struct cr_tree* tree, const struct cr_tree_loc* adds, uint32_t add_count, const struct cr_tree_loc* deletes, uint32_t delete_count, const struct cr_tree_loc_2* moves, uint32_t move_count, const struct cr_tree_node* nodes) {
    assert(add_count > 0 || delete_count > 0 || move_count > 0);

    size_t nodes_size = (add_count + move_count) * tree->node_size;
    size_t removal_size = (delete_count + move_count) * sizeof(struct cr_tree_removal);
    size_t insertion_size = (add_count + move_count) * sizeof(struct cr_tree_insertion);

    struct cr_tree_node* temp_nodes = cr_malloc(tree->temp_mem, nodes_size, 0, cr_loc);
    void* temp_operations = cr_malloc(tree->temp_mem, max_uint64(removal_size, insertion_size), 0, cr_loc);
    struct cr_tree_removal* temp_removals = temp_operations;
    struct cr_tree_insertion* temp_insertions = temp_operations;

    uint32_t removal_count = 0;
    uint32_t node_count = 0;
    uint32_t add_index = 0;
    const struct cr_tree_loc* add = adds;
    const struct cr_tree_loc* add_end = add ? add + add_count : NULL;
    const struct cr_tree_loc_2* move = moves;
    const struct cr_tree_loc_2* move_end = move ? move + move_count : NULL;

    for (uint32_t i = 0; i < add_count + move_count; i++) {
        struct cr_tree_node* dst = cr_tree_node_at(temp_nodes, node_count++, tree->node_size);
        const struct cr_tree_node* src;
        if ((add == add_end) || ((move != move_end) && (cr_tree_loc_get_index(move->dst) < cr_tree_loc_get_index(*add)))) {
            assert(move != move_end);
            struct cr_tree_removal* removal = temp_removals + removal_count++;
            removal->index = cr_tree_loc_get_index(move->src);
            src = cr_tree_node_at(tree->node_data, cr_tree_loc_get_index(move->src), tree->node_size);
            move++;
        } else {
            assert(add != add_end);
            src = cr_tree_node_at(nodes, add_index, tree->node_size);
            add_index++;
            add++;
        }
        memcpy(dst, src, tree->node_size);
    }

    assert(add == add_end);
    assert(move == move_end);

    const struct cr_tree_loc* delete_end = deletes ? deletes + delete_count : NULL;

    for (const struct cr_tree_loc* delete = deletes; delete != delete_end; delete++) {
        struct cr_tree_removal* removal = temp_removals + removal_count++;
        removal->index = cr_tree_loc_get_index(*delete);

        struct cr_tree_node_header* header = (struct cr_tree_node_header*)cr_tree_node_at(tree->node_data, removal->index, tree->node_size);
        assert(header->ref == NULL);
    }

    qsort(temp_removals, removal_count, sizeof(struct cr_tree_removal), cr_tree_move_cmp_removals);

    cr_tree_remove(tree, temp_removals, removal_count, false);

    uint32_t insertion_count = 0;
    add = adds;
    move = moves;

    for (uint32_t i = 0; i < add_count + move_count; i++) {
        struct cr_tree_insertion* insertion = temp_insertions + insertion_count++;
        if ((add == add_end) || ((move != move_end) && (cr_tree_loc_get_index(move->dst) < cr_tree_loc_get_index(*add)))) {
            assert(move != move_end);
            insertion->index = cr_tree_loc_get_index(move->dst);
            insertion->level = cr_tree_loc_get_level(move->dst);
            move++;
        } else {
            assert(add != add_end);
            insertion->index = cr_tree_loc_get_index(*add);
            insertion->level = cr_tree_loc_get_level(*add);
            add++;
        }
    }

    assert(add == add_end);
    assert(move == move_end);

    cr_tree_insert(tree, temp_insertions, insertion_count, temp_nodes);

    cr_free(tree->temp_mem, temp_nodes, cr_loc);
    cr_free(tree->temp_mem, temp_operations, cr_loc);

    if (tree->on_move) {
        tree->on_move(adds, add_count, deletes, delete_count, moves, move_count, nodes, tree->on_move_user);
    }
}

static void cr_build_ranges(struct cr_tree* tree, const uint32_t* indices, uint32_t index_count, struct cr_range_uint* out_ranges, uint32_t* out_range_count) {
    uint32_t range_count = 0;

    uint32_t last_index = uint_max;

    for (const uint32_t* index = indices; index != indices + index_count; index++) {
        uint32_t index_i = *index;
        if ((index_i > last_index) || (last_index == uint_max)) {
            last_index = cr_tree_get_next_index(tree, index_i) - 1;
            out_ranges[range_count++] = (struct cr_range_uint) {
                .start = index_i,
                .end = last_index
            };
        }
    }

    *out_range_count = range_count;
}

void cr_tree_duplicate(struct cr_tree* tree, const uint32_t* indices, uint32_t index_count) {
    assert(index_count > 0);

    struct cr_range_uint* ranges = cr_malloc(tree->temp_mem, index_count * sizeof(*ranges), 0, cr_loc);
    struct cr_tree_loc* adds = cr_malloc(tree->temp_mem, tree->node_count * sizeof(*adds), 0, cr_loc);
    struct cr_tree_node* nodes = cr_malloc(tree->temp_mem, tree->node_count * tree->node_size, 0, cr_loc);

    uint32_t range_count = 0;
    cr_build_ranges(tree, indices, index_count, ranges, &range_count);

    struct cr_tree_node* last_range_start_node = cr_tree_node_at(tree->node_data, ranges[range_count - 1].start, tree->node_size);
    uint32_t first_index = ranges[range_count - 1].end + 1;
    uint32_t first_level = cr_tree_node_get_level(last_range_start_node);

    uint32_t add_count = 0;
    uint32_t node_count = 0;

    for (uint32_t i = 0; i < range_count; i++) {
        uint32_t range_start_level = cr_tree_node_get_level(cr_tree_node_at(tree->node_data, ranges[i].start, tree->node_size));
        int level_delta = (int)first_level - (int)range_start_level;

        for (uint32_t j = ranges[i].start; j <= ranges[i].end; j++) {
            struct cr_tree_node* src_node = cr_tree_node_at(tree->node_data, j, tree->node_size);

            struct cr_tree_node* add_node = cr_tree_node_at(nodes, node_count++, tree->node_size);
            cr_tree_node_init(add_node);
            memcpy(cr_tree_node_get_content(add_node), cr_tree_node_get_content(src_node), tree->node_size - sizeof(struct cr_tree_node_header));

            uint32_t src_level = cr_tree_node_get_level(src_node);
            uint32_t dst_index = first_index + add_count;
            uint32_t dst_level = (uint32_t)((int)src_level + level_delta);

            adds[add_count++] = cr_tree_loc_make_index_level(dst_index, dst_level);
        }
    }

    cr_tree_move(tree, adds, add_count, NULL, 0, NULL, 0, nodes);

    struct cr_tree_loc* select_locs = adds;
    uint32_t select_loc_count = 0;
    uint32_t total_count = 0;

    for (uint32_t i = 0; i < range_count; i++) {
        uint32_t select_index = first_index + total_count;

        select_locs[select_loc_count++] = cr_tree_loc_make_index_level(select_index, 0);

        total_count += ranges[i].end - ranges[i].start + 1;
    }

    cr_tree_select(tree, select_locs, select_loc_count, cr_tree_selection_mode_replace);

    cr_free(tree->temp_mem, ranges, cr_loc);
    cr_free(tree->temp_mem, adds, cr_loc);
    cr_free(tree->temp_mem, nodes, cr_loc);
}

void cr_tree_group(struct cr_tree* tree, const uint32_t* indices, uint32_t index_count, uint32_t* out_group_index, bool should_select) {
    assert(index_count > 0);

    struct cr_range_uint* ranges = cr_malloc(tree->temp_mem, index_count * sizeof(*ranges), 0, cr_loc);
    struct cr_tree_loc_2* moves = cr_malloc(tree->temp_mem, tree->node_count * sizeof(*moves), 0, cr_loc);
    struct cr_tree_node* group_node = cr_malloc(tree->temp_mem, tree->node_size, 0, cr_loc);

    uint32_t range_count = 0;
    cr_build_ranges(tree, indices, index_count, ranges, &range_count);

    struct cr_tree_node* first_range_start_node = cr_tree_node_at(tree->node_data, ranges[0].start, tree->node_size);
    uint32_t first_index = cr_tree_node_get_index(first_range_start_node);
    uint32_t first_level = cr_tree_node_get_level(first_range_start_node);
    uint32_t group_index = uint_max;

    uint32_t move_count = 0;

    for (uint32_t i = 0; i < range_count; i++) {
        uint32_t range_start_level = cr_tree_node_get_level(cr_tree_node_at(tree->node_data, ranges[i].start, tree->node_size));
        int level_delta = (int)(first_level + 1) - (int)range_start_level;

        for (uint32_t j = ranges[i].start; j <= ranges[i].end; j++) {
            struct cr_tree_loc src_loc = cr_tree_node_get_loc(cr_tree_node_at(tree->node_data, j, tree->node_size));
            uint32_t src_index = cr_tree_loc_get_index(src_loc);
            uint32_t src_level = cr_tree_loc_get_level(src_loc);
            uint32_t dst_index = first_index + move_count + 1;
            uint32_t dst_level = (uint32_t)((int)src_level + level_delta);

            if (dst_level >= cr_tree_level_count_max) {
                goto end;
            }

            moves[move_count++] = cr_tree_loc_2_make(src_index, src_level, dst_index, dst_level);
        }
    }

    {
        cr_tree_node_init(group_node);
        memset(cr_tree_node_get_content(group_node), 0, tree->node_size - cr_tree_node_header_size);
        struct cr_tree_loc group_loc = cr_tree_loc_make_index_level(first_index, first_level);

        cr_tree_move(tree, &group_loc, 1, NULL, 0, moves, move_count, group_node);

        if (should_select) {
            cr_tree_select(tree, &group_loc, 1, cr_tree_selection_mode_replace);
        }

        group_index = cr_tree_loc_get_index(group_loc);
    }

end:
    cr_free(tree->temp_mem, ranges, cr_loc);
    cr_free(tree->temp_mem, moves, cr_loc);
    cr_free(tree->temp_mem, group_node, cr_loc);

    if (out_group_index) {
        *out_group_index = group_index;
    }
}

void cr_tree_delete(struct cr_tree* tree, const uint32_t* indices, uint32_t index_count) {
    assert(index_count > 0);

    struct cr_range_uint* ranges = cr_malloc(tree->temp_mem, index_count * sizeof(*ranges), 0, cr_loc);
    struct cr_tree_loc* deletes = cr_malloc(tree->temp_mem, tree->node_count * sizeof(*deletes), 0, cr_loc);

    uint32_t range_count = 0;
    cr_build_ranges(tree, indices, index_count, ranges, &range_count);

    uint32_t delete_count = 0;

    for (uint32_t i = 0; i < range_count; i++) {
        for (uint32_t j = ranges[i].start; j <= ranges[i].end; j++) {
            struct cr_tree_loc src_loc = cr_tree_node_get_loc(cr_tree_node_at(tree->node_data, j, tree->node_size));
            uint32_t src_index = cr_tree_loc_get_index(src_loc);
            uint32_t src_level = cr_tree_loc_get_level(src_loc);

            deletes[delete_count++] = cr_tree_loc_make_index_level(src_index, src_level);
        }
    }

    cr_tree_move(tree, NULL, 0, deletes, delete_count, NULL, 0, NULL);

    cr_free(tree->temp_mem, ranges, cr_loc);
    cr_free(tree->temp_mem, deletes, cr_loc);
}

void cr_tree_add(struct cr_tree* tree, uint32_t index, const struct cr_tree_node* nodes, uint32_t node_count, bool should_select) {
    assert(node_count > 0);

    struct cr_tree_loc* adds = cr_malloc(tree->temp_mem, node_count * sizeof(*adds), 0, cr_loc);

    uint32_t first_index = tree->node_count;
    uint32_t first_level = 0;

    if (index != uint_max && index < tree->node_count) {
        first_index = cr_tree_get_next_index(tree, index);
        first_level = cr_tree_node_get_level(cr_tree_node_at(tree->node_data, index, tree->node_size));
    }

    uint32_t add_count = 0;

    while (add_count != node_count) {
        struct cr_tree_node* src_node = cr_tree_node_at(nodes, add_count, tree->node_size);
        uint32_t src_level = cr_tree_node_get_level(src_node);
        if (((struct cr_tree_node_header*)src_node)->loc.raw == cr_tree_loc_make_index_level(uint_max, uint_max).raw) {
            src_level = 0;
        }
        uint32_t dst_index = first_index + add_count;
        uint32_t dst_level = first_level + src_level;

        adds[add_count++] = cr_tree_loc_make_index_level(dst_index, dst_level);
    }

    cr_tree_move(tree, adds, add_count, NULL, 0, NULL, 0, nodes);

    if (should_select) {
        cr_tree_select(tree, adds, add_count, cr_tree_selection_mode_replace);
    }

    cr_free(tree->temp_mem, adds, cr_loc);
}

bool cr_tree_update_drag(struct cr_tree* tree, const uint32_t* indices, uint32_t index_count, bool is_target_bottom, uint32_t target_index, uint32_t target_level, struct cr_tree_cursor_info* out_cursor_info, bool (*_is_group)(struct cr_tree_node* node, void* user), void* is_group_user) {
    assert(index_count > 0);

    bool success = false;

    struct cr_range_uint* ranges = cr_malloc(tree->temp_mem, index_count * sizeof(*ranges), 0, cr_loc);

    if (tree->drag.moves == NULL) {
        tree->drag.moves = cr_malloc(tree->temp_mem, tree->node_count * sizeof(*tree->drag.moves), 0, cr_loc);
    }
    tree->drag.move_count = 0;

    struct cr_tree_loc_2* moves = tree->drag.moves;

    uint32_t range_count = 0;
    cr_build_ranges(tree, indices, index_count, ranges, &range_count);

    uint32_t first_index = is_target_bottom ? target_index + 1 : target_index;

    uint32_t prev_index = first_index > 0 ? first_index - 1 : uint_max;
    uint32_t next_index = first_index < tree->node_count ? first_index : uint_max;

    uint32_t start = ranges[0].start;

    for (uint32_t i = 0; i < range_count; i++) {
        if ((i == range_count - 1) || (ranges[i].end + 1 != ranges[i + 1].start)) {
            bool contains_prev = (first_index > 0) && ((first_index - 1) >= start) && ((first_index - 1) <= ranges[i].end);
            bool contains_next = (first_index >= start) && (first_index <= ranges[i].end);

            if (contains_prev && contains_next) {
                goto end;
            } else if (contains_prev) {
                prev_index = start > 0 ? start - 1 : uint_max;
                break;
            } else if (contains_next) {
                next_index = ranges[i].end + 1 < tree->node_count ? ranges[i].end + 1 : uint_max;
                break;
            }

            if (i != range_count - 1) {
                start = ranges[i + 1].start;
            }
        }
    }

    struct cr_tree_node* prev_node = prev_index != uint_max ? cr_tree_node_at(tree->node_data, prev_index, tree->node_size) : NULL;
    struct cr_tree_node* next_node = next_index != uint_max ? cr_tree_node_at(tree->node_data, next_index, tree->node_size) : NULL;

    uint64_t allowed_levels = 0;

    for (uint32_t i = 0; i < cr_tree_level_count_max; i++) {
        bool is_allowed_prev;
        if (prev_node) {
            uint32_t prev_level = cr_tree_node_get_level(prev_node);
            bool is_group = _is_group ? _is_group(prev_node, is_group_user) : false;
            is_allowed_prev = i <= (is_group ? prev_level + 1 : prev_level);
        } else {
            is_allowed_prev = i == 0;
        }
        bool is_allowed_next;
        if (next_node) {
            uint32_t next_level = cr_tree_node_get_level(next_node);
            is_allowed_next = i >= next_level;
        } else {
            is_allowed_next = true;
        }
        if (is_allowed_prev && is_allowed_next) {
            allowed_levels |= 1llu << i;
        }
    }

    if (allowed_levels == 0) {
        goto end;
    }

    uint32_t min_dist = uint_max;
    uint32_t first_level = 0;

    for (uint32_t i = 0; i < cr_tree_level_count_max; i++) {
        if ((1llu << i) & allowed_levels) {
            uint32_t dist = (uint32_t)abs((int)target_level - (int)i);
            if (dist < min_dist) {
                min_dist = dist;
                first_level = i;
            }
        }
    }

    out_cursor_info->level = first_level;
    if (first_index == tree->node_count) {
        out_cursor_info->is_bottom = true;
        out_cursor_info->row = first_index - 1;
    } else {
        out_cursor_info->is_bottom = false;
        out_cursor_info->row = first_index;
    }

    uint32_t before_count = 0;

    for (uint32_t i = 0; i < range_count; i++) {
        if (ranges[i].start >= first_index) break;
        before_count += min_uint32(ranges[i].end + 1, first_index) - ranges[i].start;
    }

    first_index -= before_count;

    uint32_t move_count = 0;
    uint32_t dst_offset = 0;

    for (uint32_t i = 0; i < range_count; i++) {
        uint32_t range_start_level = cr_tree_node_get_level(cr_tree_node_at(tree->node_data, ranges[i].start, tree->node_size));
        int level_delta = (int)first_level - (int)range_start_level;

        for (uint32_t j = ranges[i].start; j <= ranges[i].end; j++) {
            struct cr_tree_loc src_loc = cr_tree_node_get_loc(cr_tree_node_at(tree->node_data, j, tree->node_size));
            uint32_t src_index = cr_tree_loc_get_index(src_loc);
            uint32_t src_level = cr_tree_loc_get_level(src_loc);

            uint32_t dst_index = first_index + dst_offset++;
            uint32_t dst_level = (uint32_t)((int)src_level + level_delta);

            if (dst_level >= cr_tree_level_count_max) {
                goto end;
            }

            if ((src_index != dst_index) || (src_level != dst_level)) {
                moves[move_count++] = cr_tree_loc_2_make(src_index, src_level, dst_index, dst_level);
            }
        }
    }

    tree->drag.move_count = move_count;
    out_cursor_info->is_active = move_count > 0;
    success = true;

end:
    cr_free(tree->temp_mem, ranges, cr_loc);

    return success;
}

void cr_tree_end_drag(struct cr_tree* tree, bool apply) {
    struct cr_tree_loc_2* moves = tree->drag.moves;
    uint32_t move_count = tree->drag.move_count;

    memset(&tree->drag, 0, sizeof(tree->drag));

    if (apply && moves && move_count > 0) {
        cr_tree_move(tree, NULL, 0, NULL, 0, moves, move_count, NULL);
    }

    if (moves) {
        cr_free(tree->temp_mem, moves, cr_loc);
    }
}

void cr_tree_print(struct cr_tree* tree) {
    for (uint32_t i = 0; i < tree->node_count; i++) {
        printf("%.2u: ", i);
        struct cr_tree_node* node = cr_tree_get_node(tree, i);
        struct cr_tree_loc loc = cr_tree_node_get_loc(node);
        for (uint32_t j = 0; j < cr_tree_loc_get_level(loc); j++) {
            printf("  ");
        }
        uint32_t parent_index = cr_tree_node_get_parent_index(node);
        uint32_t first_child_index = cr_tree_node_get_first_child_index(node);
        uint32_t next_index = cr_tree_node_get_next_index(node);
        uint32_t prev_index = cr_tree_node_get_prev_index(node);

        printf("(pa: %u, fc: %u, nx: %u, pv: %u) ", parent_index, first_child_index, next_index, prev_index);
        printf("\n");
    }
}

struct cr_tree_iter cr_tree_iter(struct cr_tree* tree) {
    struct cr_tree_iter iter = {
        .tree = tree,
        .index = 0,
    };

    return iter;
}

bool cr_tree_iter_next(struct cr_tree_iter* inout_iter, struct cr_tree_node** out_node_ptr, uint32_t* out_index) {
    if (inout_iter->index >= inout_iter->tree->node_count) {
        return false;
    }

    *out_node_ptr = cr_tree_node_at(inout_iter->tree->node_data, inout_iter->index, inout_iter->tree->node_size);
    *out_index = inout_iter->index;

    inout_iter->index++;

    return true;
}
