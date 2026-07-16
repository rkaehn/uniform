#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "cr.h"
#include "tree.h"
#include "tree_tests.h"

#define test_node_size cr_tree_node_size(sizeof(struct node_content))

struct node_content {
    uint64_t id_;
};

struct it_params {
    struct cr_tree* tree;
    uint32_t max_node_count;
    void* nodes;
    struct cr_tree_insertion* insertions;
    struct cr_tree_removal* removals;
    uint64_t* seed;
};

struct gen_subtree_params {
    void* nodes;
    struct cr_tree_insertion* insertions;
    uint32_t max_count;
    uint32_t start_index;
    uint32_t start_level;
    uint64_t* seed;
};

static void test_tree_validate(struct cr_tree* tree) {
    uint32_t prev_level = 0;

    struct cr_tree_iter iter = cr_tree_iter(tree);
    struct cr_tree_node* node;
    uint32_t i;

    uint32_t iter_count = 0;

    while (cr_tree_iter_next(&iter, &node, &i)) {
        iter_count++;

        uint32_t index = cr_tree_node_get_index(node);
        assert(index == i);

        uint32_t level = cr_tree_node_get_level(node);
        if (i != 0) {
            assert(level <= prev_level + 1);
        } else {
            assert(level == 0);
        }

        uint32_t next_index = cr_tree_node_get_next_index(node);
        if (next_index != uint_max) {
            struct cr_tree_node* next_node = cr_tree_get_node(tree, next_index);
            uint32_t prev_index = cr_tree_node_get_prev_index(next_node);
            assert(prev_index == i);
        }

        uint32_t first_child_index = cr_tree_node_get_first_child_index(node);
        if (first_child_index != uint_max) {
            struct cr_tree_node* first_child_node = cr_tree_get_node(tree, first_child_index);
            uint32_t parent_index = cr_tree_node_get_parent_index(first_child_node);
            assert(parent_index == i);
        }

        prev_level = level;
    }

    assert(iter_count == cr_tree_get_node_count(tree));
}

static void test_tree_gen_subtree(const struct gen_subtree_params* params) {
    struct cr_tree_insertion* insertion = params->insertions;
    struct cr_tree_insertion* insertion_end = insertion + params->max_count;
    uint32_t current_index = params->start_index;
    uint32_t current_level = params->start_level;
    uint32_t target_level = current_level;

    for (; insertion != insertion_end; insertion++, current_index++) {
        if (current_level == target_level) {
            target_level = params->start_level + (cr_rand(params->seed) % (16 - params->start_level));
        }

        if (target_level < current_level) {
            current_level = target_level;
        } else if (target_level > current_level && current_index != 0) {
            current_level++;
        }

        struct cr_tree_node* node = cr_tree_node_at(params->nodes, (uint64_t)(insertion - params->insertions), test_node_size);
        cr_tree_node_init(node);

        struct node_content* cn = cr_tree_node_get_content(node);
        cn->id_ = cr_rand(params->seed);

        *insertion = (struct cr_tree_insertion) {
            .index = current_index,
            .level = current_level,
        };
    }
}

static void test_tree_it(const struct it_params* params) {
    uint32_t node_count = cr_tree_get_node_count(params->tree);

    bool is_insertion = node_count < (params->max_node_count / 2);

    if (is_insertion) {
        uint32_t insertion_count_max = params->max_node_count - node_count;
        uint32_t insertion_count = (cr_rand(params->seed) % insertion_count_max) + 1;

        uint32_t start_index = cr_rand(params->seed) % (node_count + 1);
        uint32_t start_level = 0;
        if (start_index != 0) {
            struct cr_tree_node* node = cr_tree_get_node(params->tree, start_index - 1);
            start_level = cr_tree_node_get_level(node);
        }

        memset(params->nodes, 0, insertion_count * test_node_size);

        test_tree_gen_subtree(&(struct gen_subtree_params) {
            .nodes = params->nodes,
            .insertions = params->insertions,
            .max_count = insertion_count,
            .start_index = start_index,
            .start_level = start_level,
            .seed = params->seed,
        });

        cr_tree_insert(params->tree, params->insertions, insertion_count, params->nodes);

        for (uint32_t i = 0; i < insertion_count; i++) {
            struct cr_tree_node* node = cr_tree_get_node(params->tree, params->insertions[i].index);
            struct node_content* cn = cr_tree_node_get_content(node);

            struct cr_tree_node* insert_node = cr_tree_node_at(params->nodes, i, test_node_size);
            struct node_content* insert_cn = cr_tree_node_get_content(insert_node);

            assert(cn->id_ == insert_cn->id_);
        }
    } else {
        uint32_t start_index = cr_rand(params->seed) % node_count;
        uint32_t end_index = cr_tree_get_next_index(params->tree, start_index);
        uint32_t removal_count = end_index - start_index;

        struct cr_tree_removal* removal = params->removals;
        struct cr_tree_removal* removal_end = removal + removal_count;

        for (; removal != removal_end; removal++) {
            *removal = (struct cr_tree_removal) {
                .index = start_index + (uint32_t)(removal - params->removals),
            };
        }

        cr_tree_remove(params->tree, params->removals, removal_count, true);
    }

    test_tree_validate(params->tree);
}

static struct cr_tree* test_tree_ops_make(struct cr_mem* mem, const uint32_t* levels, const uint64_t* ids, uint32_t count) {
    struct cr_tree* tree = cr_tree_create(mem, mem, sizeof(struct node_content));

    void* nodes = cr_malloc(mem, count * test_node_size, 0, cr_loc);
    struct cr_tree_insertion* insertions = cr_malloc(mem, count * sizeof(*insertions), 0, cr_loc);

    for (uint32_t i = 0; i < count; i++) {
        struct cr_tree_node* node = cr_tree_node_at(nodes, i, test_node_size);
        cr_tree_node_init(node);
        struct node_content* cn = cr_tree_node_get_content(node);
        cn->id_ = ids[i];
        insertions[i] = (struct cr_tree_insertion) { .index = i, .level = levels[i] };
    }

    cr_tree_insert(tree, insertions, count, nodes);

    cr_free(mem, nodes, cr_loc);
    cr_free(mem, insertions, cr_loc);

    return tree;
}

static uint64_t test_tree_id_at(struct cr_tree* tree, uint32_t index) {
    struct node_content* cn = cr_tree_node_get_content(cr_tree_get_node(tree, index));
    return cn->id_;
}

static uint32_t test_tree_count_id(struct cr_tree* tree, uint64_t id_) {
    uint32_t count = 0;
    struct cr_tree_iter iter = cr_tree_iter(tree);
    struct cr_tree_node* node;
    uint32_t i;
    while (cr_tree_iter_next(&iter, &node, &i)) {
        struct node_content* cn = cr_tree_node_get_content(node);
        count += cn->id_ == id_ ? 1 : 0;
    }
    return count;
}

static void test_tree_select_ops(struct cr_mem* mem) {
    uint32_t levels[] = { 0, 1, 1, 0 };
    uint64_t ids[] = { 10, 11, 12, 13 };
    struct cr_tree* tree = test_tree_ops_make(mem, levels, ids, 4);

    struct cr_tree_loc replace_locs[] = { cr_tree_loc_make_index(1), cr_tree_loc_make_index(2) };
    cr_tree_select(tree, replace_locs, 2, cr_tree_selection_mode_replace);
    assert(cr_tree_node_get_is_selected(cr_tree_get_node(tree, 0)) == false);
    assert(cr_tree_node_get_is_selected(cr_tree_get_node(tree, 1)) == true);
    assert(cr_tree_node_get_is_selected(cr_tree_get_node(tree, 2)) == true);
    assert(cr_tree_node_get_is_selected(cr_tree_get_node(tree, 3)) == false);

    struct cr_tree_loc add_locs[] = { cr_tree_loc_make_index(3) };
    cr_tree_select(tree, add_locs, 1, cr_tree_selection_mode_add);
    assert(cr_tree_node_get_is_selected(cr_tree_get_node(tree, 3)) == true);
    assert(cr_tree_node_get_is_selected(cr_tree_get_node(tree, 1)) == true);

    struct cr_tree_loc subtract_locs[] = { cr_tree_loc_make_index(1) };
    cr_tree_select(tree, subtract_locs, 1, cr_tree_selection_mode_subtract);
    assert(cr_tree_node_get_is_selected(cr_tree_get_node(tree, 1)) == false);
    assert(cr_tree_node_get_is_selected(cr_tree_get_node(tree, 2)) == true);

    cr_tree_destroy(tree);
}

static void test_tree_duplicate_ops(struct cr_mem* mem) {
    uint32_t levels[] = { 0, 1, 1, 0 };
    uint64_t ids[] = { 10, 11, 12, 13 };
    struct cr_tree* tree = test_tree_ops_make(mem, levels, ids, 4);

    uint32_t indices[] = { 1 };
    cr_tree_duplicate(tree, indices, 1);

    assert(cr_tree_get_node_count(tree) == 5);
    assert(test_tree_count_id(tree, 11) == 2);
    test_tree_validate(tree);

    uint32_t subtree[] = { 0 };
    cr_tree_duplicate(tree, subtree, 1);

    assert(cr_tree_get_node_count(tree) == 9);
    assert(test_tree_count_id(tree, 10) == 2);
    assert(test_tree_count_id(tree, 11) == 4);
    assert(test_tree_count_id(tree, 12) == 2);
    test_tree_validate(tree);

    cr_tree_destroy(tree);
}

static void test_tree_group_delete_ops(struct cr_mem* mem) {
    uint32_t levels[] = { 0, 1, 1, 0 };
    uint64_t ids[] = { 10, 11, 12, 13 };
    struct cr_tree* tree = test_tree_ops_make(mem, levels, ids, 4);

    uint32_t indices[] = { 1, 2 };
    uint32_t group_index = uint_max;
    cr_tree_group(tree, indices, 2, &group_index, true);

    assert(cr_tree_get_node_count(tree) == 5);
    assert(group_index == 1);
    assert(cr_tree_node_get_level(cr_tree_get_node(tree, group_index)) == 1);
    assert(test_tree_id_at(tree, group_index) == 0);
    assert(test_tree_id_at(tree, 2) == 11);
    assert(test_tree_id_at(tree, 3) == 12);
    assert(cr_tree_node_get_level(cr_tree_get_node(tree, 2)) == 2);
    assert(cr_tree_node_get_level(cr_tree_get_node(tree, 3)) == 2);
    assert(cr_tree_node_get_is_selected(cr_tree_get_node(tree, group_index)) == true);
    test_tree_validate(tree);

    uint32_t delete_indices[] = { 0 };
    cr_tree_delete(tree, delete_indices, 1);

    assert(cr_tree_get_node_count(tree) == 1);
    assert(test_tree_id_at(tree, 0) == 13);
    test_tree_validate(tree);

    cr_tree_destroy(tree);
}

static void test_tree_add_ref_ops(struct cr_mem* mem) {
    uint32_t levels[] = { 0, 0 };
    uint64_t ids[] = { 10, 13 };
    struct cr_tree* tree = test_tree_ops_make(mem, levels, ids, 2);

    struct cr_tree_node_ref* ref = cr_tree_node_ref_retain(tree, 1, 1u);
    assert(cr_tree_node_ref_get_index(ref) == 1);

    void* nodes = cr_malloc(mem, 2 * test_node_size, 0, cr_loc);
    for (uint32_t i = 0; i < 2; i++) {
        struct cr_tree_node* node = cr_tree_node_at(nodes, i, test_node_size);
        cr_tree_node_init(node);
        struct node_content* cn = cr_tree_node_get_content(node);
        cn->id_ = 20 + i;
    }

    cr_tree_add(tree, 1, nodes, 2, true);
    cr_free(mem, nodes, cr_loc);

    assert(cr_tree_get_node_count(tree) == 4);
    assert(test_tree_id_at(tree, 2) == 20);
    assert(test_tree_id_at(tree, 3) == 21);
    test_tree_validate(tree);

    struct node_content* ref_cn = cr_tree_node_get_content(cr_tree_node_ref_ptr(ref));
    assert(ref_cn->id_ == 13);

    cr_tree_node_ref_release(ref, 1u);

    cr_tree_destroy(tree);
}

static void test_tree_drag_ops(struct cr_mem* mem) {
    uint32_t levels[] = { 0, 1, 1, 0 };
    uint64_t ids[] = { 10, 11, 12, 13 };
    struct cr_tree* tree = test_tree_ops_make(mem, levels, ids, 4);

    uint32_t indices[] = { 3 };
    struct cr_tree_cursor_info cursor_info = { 0 };

    bool active = cr_tree_update_drag(tree, indices, 1, true, 2, 1, &cursor_info, NULL, NULL);
    assert(active);
    assert(cursor_info.is_active);
    assert(cursor_info.level <= 63);

    cr_tree_end_drag(tree, false);
    assert(cr_tree_get_node_count(tree) == 4);
    assert(test_tree_id_at(tree, 3) == 13);
    test_tree_validate(tree);

    active = cr_tree_update_drag(tree, indices, 1, true, 2, 1, &cursor_info, NULL, NULL);
    assert(active);
    cr_tree_end_drag(tree, true);
    assert(cr_tree_get_node_count(tree) == 4);
    assert(test_tree_count_id(tree, 13) == 1);
    test_tree_validate(tree);

    active = cr_tree_update_drag(tree, indices, 1, false, 0, 0, &cursor_info, NULL, NULL);
    assert(active);

    cr_tree_destroy(tree);
}

void tree_test(void) {
    struct cr_mem* mem = cr_mem_libc;

    struct cr_tree* tree = cr_tree_create(mem, mem, sizeof(struct node_content));
    assert(cr_tree_get_node_count(tree) == 0);

    uint32_t max_node_count = 512;
    void* nodes = cr_malloc(mem, max_node_count * test_node_size, 0, cr_loc);
    void* insertions = cr_malloc(mem, max_node_count * sizeof(struct cr_tree_insertion), 0, cr_loc);
    void* removals = cr_malloc(mem, max_node_count * sizeof(struct cr_tree_removal), 0, cr_loc);

    uint64_t seed = 0;

    for (int i = 0; i < 16384; i++) {
        test_tree_it(&(struct it_params) {
            .tree = tree,
            .max_node_count = max_node_count,
            .nodes = nodes,
            .insertions = insertions,
            .removals = removals,
            .seed = &seed,
        });
    }

    cr_tree_destroy(tree);
    cr_free(mem, nodes, cr_loc);
    cr_free(mem, insertions, cr_loc);
    cr_free(mem, removals, cr_loc);

    test_tree_select_ops(mem);
    test_tree_duplicate_ops(mem);
    test_tree_group_delete_ops(mem);
    test_tree_add_ref_ops(mem);
    test_tree_drag_ops(mem);

    printf("tree_test: ok\n");
}
