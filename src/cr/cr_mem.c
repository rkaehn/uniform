#include "cr.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#if cr_mem_rec_trace
#include <execinfo.h>
#endif

#ifndef cr_mem_rec
#define cr_mem_rec (DEBUG != 0)
#endif
#ifndef cr_mem_rec_trace
#define cr_mem_rec_trace 0
#endif

#define cr_mem_flag_wants_rec (1lu)
#define cr_mem_flag_thread_safe (1lu << 1)

struct cr_mem_header {
    uint32_t flags;
    pthread_t thread;
    void* (*malloc)(struct cr_mem* mem, uint64_t size, uint64_t align);
    void* (*realloc)(struct cr_mem* mem, void* ptr, uint64_t size);
    void (*free)(struct cr_mem* mem, void* ptr);
    void (*destroy)(struct cr_mem* mem);
};

static void cr_mem_check_thread(struct cr_mem_header* header, const char* func) {
    if (header->flags & cr_mem_flag_thread_safe) {
        return;
    }
    cr_check(pthread_equal(header->thread, pthread_self()), "%s uses a single-threaded allocator from a thread other than the one that created it", func);
}

#if cr_mem_rec
struct cr_rec_alloc {
#if cr_mem_rec_trace
    char func[256];
#else
    char func[8];
#endif
    struct cr_mem* mem;
    const char* label;
    void* ptr;
    uint64_t size;
    uint64_t align;
};

struct cr_rec_alloc_key {
    void* ptr;
    struct cr_mem* mem;
};

#define cr_dict cr_dict_rec_alloc
#define cr_dict_key struct cr_rec_alloc_key
#define cr_dict_value struct cr_rec_alloc
#define cr_dict_is_equal(a, b) ((a).ptr == (b).ptr && (a).mem == (b).mem)
#define cr_dict_hash(key) cr_hash(&key, sizeof(key), 0)
#include "cr_dict.h"

struct cr_rec {
    pthread_mutex_t mutex;
    struct cr_dict_rec_alloc* allocs;
    uint32_t max_alloc_count;
    struct cr_mem* caller_mem;
};
#endif

#define cr_mem_flag_po2_free_mem (1lu << 8)
#define cr_mem_flag_po2_free_nodes (1lu << 9)
#define cr_mem_flag_po2_free_backing (1lu << 10)
#define cr_mem_flag_lin_free_mem (1lu << 8)
#define cr_mem_flag_lin_free_backing (1lu << 9)

#define cr_mem_buddy_l_alloc (-2)
#define cr_mem_buddy_l_full (-1)

struct cr_mem_buddy_node {
    int8_t l;
};

struct cr_mem_buddy {
    struct cr_mem_header header;
    struct cr_mem_buddy_node* nodes;
    void* backing;
    int32_t backing_po2;
    int32_t l0_po2;
    uint64_t cache_index;
    int32_t cache_po2;
};

#define cr_arr cr_arr_ptr
#define cr_arr_item void*
#include "cr_arr.h"

struct cr_mem_lin {
    struct cr_mem_header header;
    void* backing;
    uint64_t backing_size;
    uint64_t offset;
#if cr_mem_rec
    struct cr_arr_ptr* allocs;
#endif
};

static void* cr_mem_libc_malloc(struct cr_mem* mem, uint64_t size, uint64_t align);
static void* cr_mem_libc_realloc(struct cr_mem* mem, void* ptr, uint64_t size);
static void cr_mem_libc_free(struct cr_mem* mem, void* ptr);
static void cr_mem_libc_destroy(struct cr_mem* mem);

static struct cr_mem_header cr_mem_static_no_rec = {
    .flags = cr_mem_flag_thread_safe,
    .malloc = cr_mem_libc_malloc,
    .realloc = cr_mem_libc_realloc,
    .free = cr_mem_libc_free,
    .destroy = cr_mem_libc_destroy,
};

static struct cr_mem_header cr_mem_static = {
#if cr_mem_rec
    .flags = cr_mem_flag_thread_safe | cr_mem_flag_wants_rec,
#else
    .flags = cr_mem_flag_thread_safe,
#endif
    .malloc = cr_mem_libc_malloc,
    .realloc = cr_mem_libc_realloc,
    .free = cr_mem_libc_free,
    .destroy = cr_mem_libc_destroy,
};

static struct cr_mem* const cr_mem_libc_no_rec = (struct cr_mem*)&cr_mem_static_no_rec;
struct cr_mem* const cr_mem_libc = (struct cr_mem*)&cr_mem_static;
static _Thread_local struct cr_mem* cr_mem_current = (struct cr_mem*)&cr_mem_static;

void cr_mem_set(struct cr_mem* mem) {
    cr_check(mem != NULL, "cr_mem_set needs an allocator");
    cr_mem_check_thread((struct cr_mem_header*)mem, "cr_mem_set");
    cr_mem_current = mem;
}

struct cr_mem* cr_mem_get(void) {
    return cr_mem_current;
}

#if cr_mem_rec
static struct cr_rec rec;

static void cr_rec_lock(void) {
    pthread_mutex_lock(&rec.mutex);
    rec.caller_mem = cr_mem_current;
    cr_mem_current = cr_mem_libc_no_rec;
}

static void cr_rec_unlock(void) {
    cr_mem_current = rec.caller_mem;
    pthread_mutex_unlock(&rec.mutex);
}

__attribute__((constructor(0))) static void cr_rec_init(void) {
    memset(&rec, 0, sizeof(rec));
    pthread_mutex_init(&rec.mutex, NULL);
    cr_rec_lock();
    rec.allocs = cr_dict_rec_alloc_create();
    cr_rec_unlock();
}

uint32_t cr_rec_get_alloc_count(void) {
    pthread_mutex_lock(&rec.mutex);

    uint32_t count = cr_dict_rec_alloc_get_count(rec.allocs);

    pthread_mutex_unlock(&rec.mutex);

    return count;
}

uint32_t cr_rec_get_alloc_count_filter_mem(struct cr_mem* mem) {
    pthread_mutex_lock(&rec.mutex);

    uint32_t result = 0;

    struct cr_dict_rec_alloc_iter iter = cr_dict_rec_alloc_iter(rec.allocs);
    struct cr_dict_rec_alloc_pair* pair;

    while (cr_dict_rec_alloc_iter_next(&iter, &pair)) {
        if (pair->key.mem == mem) {
            result++;
        }
    }

    pthread_mutex_unlock(&rec.mutex);

    return result;
}

uint32_t cr_rec_get_max_alloc_count(void) {
    return rec.max_alloc_count;
}

void cr_rec_print_allocs(void) {
    pthread_mutex_lock(&rec.mutex);

    printf("alloc count: %u\n", cr_dict_rec_alloc_get_count(rec.allocs));

    struct cr_dict_rec_alloc_iter iter = cr_dict_rec_alloc_iter(rec.allocs);
    struct cr_dict_rec_alloc_pair* pair;

    while (cr_dict_rec_alloc_iter_next(&iter, &pair)) {
        struct cr_rec_alloc alloc = pair->value;
        printf("(struct cr_rec_alloc) { .func = %s, .mem = %p, .label = %s, .ptr = %p, .size = %ld, .align = %ld }\n", alloc.func, (void*)alloc.mem, alloc.label, alloc.ptr, (long)alloc.size, (long)alloc.align);
    }

    pthread_mutex_unlock(&rec.mutex);
}

#if cr_mem_rec_trace
static void cr_rec_get_func(char* out_buf, uint32_t out_buf_size) {
    void* buf[16];
    int n = backtrace(buf, cr_count(buf));
    char** strings = backtrace_symbols(buf, n);

    assert(strings);

    out_buf[0] = '\0';

    for (int i = 0; i < n; i++) {
        bool is_spaces = false;
        int col_count = 0;

        char* name = NULL;
        uint32_t name_count = 0;

        for (char* ptr = strings[i];; ptr++) {
            assert(*ptr);

            if (*ptr == ' ' && !is_spaces) {
                is_spaces = true;
                if (col_count == 3) {
                    name_count = (uint32_t)(ptr - name);
                    break;
                }
            } else if (*ptr != ' ' && is_spaces) {
                is_spaces = false;
                col_count++;
                if (col_count == 3) {
                    name = ptr;
                }
            }
        }

        bool is_cr = false;
        is_cr |= name[0] == 'c' && name[1] == 'r' && name[2] == '_';
        is_cr |= name[0] == '_' && name[1] == 'c' && name[2] == 'r' && name[3] == '_';

        if (!is_cr) {
            uint32_t size = min_uint(out_buf_size - 1, name_count);
            memcpy(out_buf, name, size);
            out_buf[size] = '\0';
            break;
        }
    }

    free(strings);
}
#endif

static void cr_rec_track(struct cr_mem* mem, void* ptr, uint64_t size, uint64_t align, const char* label) {
    struct cr_rec_alloc* alloc;
    bool did_exist = cr_dict_rec_alloc_set_ptr(rec.allocs, (struct cr_rec_alloc_key) { .mem = mem, .ptr = ptr }, &alloc);
    assert(did_exist == false);

    memset(alloc, 0, sizeof(*alloc));
#if cr_mem_rec_trace
    cr_rec_get_func(alloc->func, sizeof(alloc->func));
#endif
    alloc->mem = mem;
    alloc->label = label;
    alloc->ptr = ptr;
    alloc->size = size;
    alloc->align = align;

    rec.max_alloc_count = max_uint32(rec.max_alloc_count, cr_dict_rec_alloc_get_count(rec.allocs));
}
#else
uint32_t cr_rec_get_alloc_count(void) {
    return 0;
}

uint32_t cr_rec_get_alloc_count_filter_mem(struct cr_mem* mem) {
    return 0;
}

uint32_t cr_rec_get_max_alloc_count(void) {
    return 0;
}

void cr_rec_print_allocs(void) {
}
#endif

void* cr_try_malloc(uint64_t size, uint64_t align, const char* label) {
    struct cr_mem* mem = cr_mem_current;
    struct cr_mem_header* header = (struct cr_mem_header*)mem;

    align = align > 0 ? align : sizeof(void*);
    assert((popcount_uint64(align) == 1) && (align >= sizeof(void*)) && (align <= 256));

#if cr_mem_rec
    if (header->flags & cr_mem_flag_wants_rec) {
        cr_rec_lock();
    }
#endif

    void* result = header->malloc(mem, size, align);

#if cr_mem_rec
    if (header->flags & cr_mem_flag_wants_rec) {
        if (result != NULL) {
            cr_rec_track(mem, result, size, align, label);
        }
        cr_rec_unlock();
    }
#endif

    assert(((uint64_t)result % align) == 0);

    return result;
}

void* cr_malloc(uint64_t size, uint64_t align, const char* label) {
    void* result = cr_try_malloc(size, align, label);
    assert(size == 0 || result != NULL);
    return result;
}

void* cr_try_realloc(void* ptr, uint64_t size, const char* label) {
    struct cr_mem* mem = cr_mem_current;
    struct cr_mem_header* header = (struct cr_mem_header*)mem;

#if cr_mem_rec
    if (header->flags & cr_mem_flag_wants_rec) {
        cr_rec_lock();
        if (ptr != NULL) {
            struct cr_rec_alloc* alloc;
            bool did_exist = cr_dict_rec_alloc_get_ptr(rec.allocs, (struct cr_rec_alloc_key) { .mem = mem, .ptr = ptr }, &alloc);
            if (did_exist == false) {
                cr_rec_unlock();
            }
            cr_check(did_exist, "%s reallocates %p through an allocator that did not allocate it", label, ptr);
        }
    }
#endif

    void* result = header->realloc(mem, ptr, size);

#if cr_mem_rec
    if (header->flags & cr_mem_flag_wants_rec) {
        if (ptr != NULL || result != NULL) {
            bool did_fail = size > 0 && result == NULL;

            if (ptr && !did_fail) {
                cr_dict_rec_alloc_remove(rec.allocs, (struct cr_rec_alloc_key) { .mem = mem, .ptr = ptr });
            }
            if (result) {
                cr_rec_track(mem, result, size, sizeof(void*), label);
            }
        }
        cr_rec_unlock();
    }
#endif

    return result;
}

void* cr_realloc(void* ptr, uint64_t size, const char* label) {
    void* result = cr_try_realloc(ptr, size, label);
    assert(size == 0 || result != NULL);
    return result;
}

void cr_free(void* ptr, const char* label) {
    if (ptr == NULL) {
        return;
    }

    struct cr_mem* mem = cr_mem_current;
    struct cr_mem_header* header = (struct cr_mem_header*)mem;
    (void)label;

#if cr_mem_rec
    if (header->flags & cr_mem_flag_wants_rec) {
        cr_rec_lock();
        bool did_exist = cr_dict_rec_alloc_remove(rec.allocs, (struct cr_rec_alloc_key) { .mem = mem, .ptr = ptr });
        if (did_exist == false) {
            cr_rec_unlock();
        }
        cr_check(did_exist, "%s frees %p through an allocator that did not allocate it", label, ptr);
    }
#endif

    header->free(mem, ptr);

#if cr_mem_rec
    if (header->flags & cr_mem_flag_wants_rec) {
        cr_rec_unlock();
    }
#endif
}

void cr_mem_destroy(struct cr_mem* mem) {
    struct cr_mem_header* header = (struct cr_mem_header*)mem;

    cr_mem_check_thread(header, "cr_mem_destroy");
    header->destroy(mem);
}

static void* cr_mem_libc_malloc(struct cr_mem* mem, uint64_t size, uint64_t align) {
    (void)mem;

    if (size == 0) {
        return NULL;
    }

    if (align <= sizeof(void*)) {
        return malloc((size_t)size);
    }

    size = (size + align - 1llu) & ~(align - 1llu);

    return aligned_alloc((size_t)align, (size_t)size);
}

static void* cr_mem_libc_realloc(struct cr_mem* mem, void* ptr, uint64_t size) {
    (void)mem;

    if (size == 0) {
        free(ptr);
        return NULL;
    }

    return realloc(ptr, (size_t)size);
}

static void cr_mem_libc_free(struct cr_mem* mem, void* ptr) {
    (void)mem;

    free(ptr);
}

static void cr_mem_libc_destroy(struct cr_mem* mem) {
    (void)mem;
}

static int32_t cr_mem_buddy_get_po2(struct cr_mem_buddy* mem, uint64_t size, uint64_t align) {
    int32_t po2 = 64 - clz_uint64(size - 1llu);
    po2 = max_int32(po2, ctz_uint64(align));
    po2 = max_int32(po2, mem->l0_po2);

    return po2;
}

static uint64_t cr_mem_buddy_get_leaf_index(int32_t backing_po2, int32_t l0_po2, uint64_t offset) {
    return (1llu << (backing_po2 - l0_po2)) - 1llu + (offset >> l0_po2);
}

static void cr_mem_buddy_update_up(struct cr_mem_buddy* mem, int32_t node_po2, uint64_t node_index, int32_t node_l, bool can_merge) {
    struct cr_mem_buddy_node* nodes = mem->nodes;
    int32_t backing_po2 = mem->backing_po2;
    uint64_t top_index = node_index;
    int32_t top_po2 = node_po2;

    for (; node_po2 < backing_po2; node_po2++) {
        uint64_t sibling_index = ((node_index - 1llu) ^ 1llu) + 1llu;
        int32_t sibling_l = nodes[sibling_index].l;
        node_index = (node_index - 1llu) >> 1;
        int32_t l;
        if (can_merge && node_l == node_po2 && sibling_l == node_po2) {
            l = node_po2 + 1;
        } else {
            l = max_int32(max_int32(node_l, sibling_l), cr_mem_buddy_l_full);
        }
        if (nodes[node_index].l == l) {
            break;
        }
        nodes[node_index].l = (int8_t)l;
        if (can_merge && l == node_po2 + 1) {
            top_index = node_index;
            top_po2 = node_po2 + 1;
        }
        node_l = l;
    }

    if (can_merge) {
        mem->cache_index = top_index;
        mem->cache_po2 = top_po2;
    }
}

static void* cr_mem_buddy_malloc(struct cr_mem* _mem, uint64_t size, uint64_t align) {
    struct cr_mem_buddy* mem = (struct cr_mem_buddy*)_mem;

    if (size == 0) {
        return NULL;
    }

    int32_t po2 = cr_mem_buddy_get_po2(mem, size, align);

    struct cr_mem_buddy_node* nodes = mem->nodes;
    int32_t backing_po2 = mem->backing_po2;
    int32_t l0_po2 = mem->l0_po2;

    if (nodes[0].l < po2) {
        return NULL;
    }

    uint64_t node_index = mem->cache_index;
    int32_t node_po2 = mem->cache_po2;

    if (po2 > node_po2) {
        node_index = ((node_index + 1llu) >> (po2 - node_po2)) - 1llu;
        node_po2 = po2;
    }

    int32_t node_l = nodes[node_index].l;

    while (node_l < po2) {
        node_index = (node_index - 1llu) >> 1;
        node_po2++;
        node_l = nodes[node_index].l;
    }

    for (; node_po2 > po2; node_po2--) {
        node_index = (node_index << 1) + 1llu;
        if (node_po2 == node_l) {
            node_l = node_po2 - 1;
            nodes[node_index].l = (int8_t)node_l;
            nodes[node_index + 1llu].l = (int8_t)node_l;
        } else {
            int32_t node_l_1 = nodes[node_index].l;
            if (node_l_1 < po2) {
                node_index++;
            } else {
                node_l = node_l_1;
            }
        }
    }

    assert(node_l == po2);
    nodes[node_index].l = cr_mem_buddy_l_alloc;

    mem->cache_index = node_index;
    mem->cache_po2 = node_po2;

    uint64_t first_in_l = (1llu << (backing_po2 - node_po2)) - 1llu;
    uint64_t offset = (node_index - first_in_l) << node_po2;

    if (node_po2 > l0_po2) {
        nodes[cr_mem_buddy_get_leaf_index(backing_po2, l0_po2, offset)].l = (int8_t)node_po2;
    }

    cr_mem_buddy_update_up(mem, node_po2, node_index, cr_mem_buddy_l_alloc, false);

    return (char*)mem->backing + offset;
}

static void cr_mem_buddy_find_node(struct cr_mem_buddy* mem, void* ptr, int32_t* out_node_po2, uint64_t* out_node_index) {
    assert((ptr >= mem->backing) && "user error");

    struct cr_mem_buddy_node* nodes = mem->nodes;
    int32_t backing_po2 = mem->backing_po2;
    int32_t l0_po2 = mem->l0_po2;

    uint64_t offset = (uint64_t)((char*)ptr - (char*)mem->backing);
    assert((offset < (1llu << backing_po2)) && "user error");
    assert(((offset & ((1llu << l0_po2) - 1llu)) == 0) && "user error");

    int32_t leaf_l = nodes[cr_mem_buddy_get_leaf_index(backing_po2, l0_po2, offset)].l;
    int32_t node_po2 = leaf_l == cr_mem_buddy_l_alloc ? l0_po2 : leaf_l;
    assert((node_po2 >= l0_po2 && node_po2 <= backing_po2) && "user error");
    assert(((offset & ((1llu << node_po2) - 1llu)) == 0) && "user error");

    uint64_t node_index = (1llu << (backing_po2 - node_po2)) - 1llu + (offset >> node_po2);
    assert((nodes[node_index].l == cr_mem_buddy_l_alloc) && "user error");

    *out_node_po2 = node_po2;
    *out_node_index = node_index;
}

static void cr_mem_buddy_free_ex(struct cr_mem_buddy* mem, int32_t node_po2, uint64_t node_index) {
    struct cr_mem_buddy_node* node = &mem->nodes[node_index];
    assert(node->l == cr_mem_buddy_l_alloc);
    node->l = (int8_t)node_po2;

    cr_mem_buddy_update_up(mem, node_po2, node_index, node_po2, true);
}

static void* cr_mem_buddy_realloc(struct cr_mem* _mem, void* ptr, uint64_t size) {
    struct cr_mem_buddy* mem = (struct cr_mem_buddy*)_mem;

    if (ptr == NULL) {
        return cr_mem_buddy_malloc(_mem, size, sizeof(void*));
    }

    int32_t node_po2 = 0;
    uint64_t node_index = 0;
    cr_mem_buddy_find_node(mem, ptr, &node_po2, &node_index);

    if (size == 0) {
        cr_mem_buddy_free_ex(mem, node_po2, node_index);
        return NULL;
    }

    int32_t po2 = cr_mem_buddy_get_po2(mem, size, sizeof(void*));

    if (po2 == node_po2) {
        return ptr;
    }

    if (po2 < node_po2) {
        struct cr_mem_buddy_node* nodes = mem->nodes;
        uint64_t top_index = node_index;
        for (int32_t l = node_po2; l > po2; l--) {
            nodes[node_index].l = (int8_t)(l - 1);
            node_index = (node_index << 1) + 1llu;
            nodes[node_index + 1llu].l = (int8_t)(l - 1);
        }
        nodes[node_index].l = cr_mem_buddy_l_alloc;
        if (po2 > mem->l0_po2) {
            uint64_t offset = (uint64_t)((char*)ptr - (char*)mem->backing);
            nodes[cr_mem_buddy_get_leaf_index(mem->backing_po2, mem->l0_po2, offset)].l = (int8_t)po2;
        }
        cr_mem_buddy_update_up(mem, node_po2, top_index, node_po2 - 1, false);
        return ptr;
    }

    void* new_ptr = cr_mem_buddy_malloc(_mem, size, sizeof(void*));
    if (new_ptr) {
        memcpy(new_ptr, ptr, (size_t)min_uint64(1llu << node_po2, size));
        cr_mem_buddy_free_ex(mem, node_po2, node_index);
    }

    return new_ptr;
}

static void cr_mem_buddy_free(struct cr_mem* _mem, void* ptr) {
    struct cr_mem_buddy* mem = (struct cr_mem_buddy*)_mem;

    int32_t node_po2 = 0;
    uint64_t node_index = 0;
    cr_mem_buddy_find_node(mem, ptr, &node_po2, &node_index);

    cr_mem_buddy_free_ex(mem, node_po2, node_index);
}

static void cr_mem_buddy_destroy(struct cr_mem* _mem) {
    struct cr_mem_buddy* mem = (struct cr_mem_buddy*)_mem;

    if (mem->header.flags & cr_mem_flag_po2_free_backing) {
        cr_free(mem->backing, cr_loc);
    }
    if (mem->header.flags & cr_mem_flag_po2_free_nodes) {
        cr_free(mem->nodes, cr_loc);
    }
    if (mem->header.flags & cr_mem_flag_po2_free_mem) {
        cr_free(mem, cr_loc);
    }
}

struct cr_mem* cr_mem_buddy_create(const struct cr_mem_buddy_params* params) {
    assert(params->backing_po2 >= 0 && params->backing_po2 < 32);
    assert(params->l0_po2 >= 0 && params->l0_po2 < 32);
    assert(params->backing_po2 > params->l0_po2);

    struct cr_mem_buddy* mem;
    if (params->raw_mem) {
        mem = params->raw_mem;
    } else {
        mem = cr_malloc(sizeof(*mem), 0, cr_loc" (cr_mem_buddy)");
        assert(mem);
    }

    memset(mem, 0, sizeof(*mem));

    if (params->raw_nodes) {
        mem->nodes = params->raw_nodes;
    } else {
        uint64_t nodes_size = cr_mem_buddy_get_nodes_size(params->backing_po2, params->l0_po2);
        mem->nodes = cr_malloc(nodes_size, 0, cr_loc" (cr_mem_buddy nodes)");
        assert(mem->nodes);
    }
    if (params->raw_backing) {
        mem->backing = params->raw_backing;
    } else {
        uint64_t backing_size = cr_mem_buddy_get_backing_size(params->backing_po2);
        uint64_t backing_align = 1llu << clamp_int32(params->backing_po2, 3, 8);
        mem->backing = cr_malloc(backing_size, backing_align, cr_loc" (cr_mem_buddy backing)");
        assert(mem->backing);
    }

#if cr_mem_rec
    if (params->wants_rec) {
        mem->header.flags |= cr_mem_flag_wants_rec;
    }
#endif
    mem->header.thread = pthread_self();
    if (params->raw_mem == NULL) {
        mem->header.flags |= cr_mem_flag_po2_free_mem;
    }
    if (params->raw_nodes == NULL) {
        mem->header.flags |= cr_mem_flag_po2_free_nodes;
    }
    if (params->raw_backing == NULL) {
        mem->header.flags |= cr_mem_flag_po2_free_backing;
    }
    mem->header.malloc = cr_mem_buddy_malloc;
    mem->header.realloc = cr_mem_buddy_realloc;
    mem->header.free = cr_mem_buddy_free;
    mem->header.destroy = cr_mem_buddy_destroy;
    mem->backing_po2 = params->backing_po2;
    mem->l0_po2 = params->l0_po2;

    mem->nodes[0] = (struct cr_mem_buddy_node) {
        .l = (int8_t)params->backing_po2,
    };
    mem->cache_index = 0;
    mem->cache_po2 = params->backing_po2;

    return (struct cr_mem*)mem;
}

uint64_t cr_mem_buddy_get_mem_size(void) {
    return sizeof(struct cr_mem_buddy);
}

uint64_t cr_mem_buddy_get_nodes_size(int32_t backing_po2, int32_t l0_po2) {
    uint64_t l0_count = 1llu << (backing_po2 - l0_po2);
    return (l0_count * 2) * sizeof(struct cr_mem_buddy_node);
}

uint64_t cr_mem_buddy_get_backing_size(int32_t backing_po2) {
    return 1llu << backing_po2;
}

static void* cr_mem_lin_malloc(struct cr_mem* _mem, uint64_t size, uint64_t align) {
    struct cr_mem_lin* mem = (struct cr_mem_lin*)_mem;

    if (size == 0) {
        return NULL;
    }

    uint64_t current_offset = mem->offset;
    uint64_t current_addr = (uint64_t)((char*)mem->backing + current_offset);
    uint64_t padding = (~current_addr + 1llu) & (align - 1llu);
    uint64_t available = mem->backing_size - current_offset;

    if (padding > available || size > available - padding) {
        return NULL;
    }

    mem->offset = current_offset + padding + size;

    void* result = (char*)mem->backing + current_offset + padding;

#if cr_mem_rec
    if (mem->header.flags & cr_mem_flag_wants_rec) {
        cr_arr_ptr_append(mem->allocs, result);
    }
#endif

    return result;
}

static void* cr_mem_lin_realloc(struct cr_mem* _mem, void* ptr, uint64_t size) {
    if (ptr == NULL) {
        return cr_mem_lin_malloc(_mem, size, sizeof(void*));
    }

    assert(0);
}

static void cr_mem_lin_free(struct cr_mem* _mem, void* ptr) {
    (void)_mem;
    (void)ptr;
    assert(0);
}

static void cr_mem_lin_destroy(struct cr_mem* _mem) {
    struct cr_mem_lin* mem = (struct cr_mem_lin*)_mem;

#if cr_mem_rec
    if (mem->header.flags & cr_mem_flag_wants_rec) {
        cr_rec_lock();
        cr_arr_ptr_destroy(mem->allocs);
        cr_rec_unlock();
    }
#endif
    if (mem->header.flags & cr_mem_flag_lin_free_backing) {
        cr_free(mem->backing, cr_loc);
    }
    if (mem->header.flags & cr_mem_flag_lin_free_mem) {
        cr_free(mem, cr_loc);
    }
}

struct cr_mem* cr_mem_lin_create(const struct cr_mem_lin_params* params) {
    struct cr_mem_lin* mem;
    if (params->raw_mem) {
        mem = params->raw_mem;
    } else {
        mem = cr_malloc(sizeof(*mem), 0, cr_loc" (cr_mem_lin)");
    }

    memset(mem, 0, sizeof(*mem));

    if (params->raw_backing) {
        mem->backing = params->raw_backing;
    } else {
        mem->backing = cr_malloc(params->backing_size, 0, cr_loc" (cr_mem_lin backing)");
        assert(mem->backing);
    }

#if cr_mem_rec
    if (params->wants_rec) {
        mem->header.flags |= cr_mem_flag_wants_rec;
        cr_rec_lock();
        mem->allocs = cr_arr_ptr_create();
        cr_rec_unlock();
    }
#endif
    mem->header.thread = pthread_self();
    if (params->raw_mem == NULL) {
        mem->header.flags |= cr_mem_flag_lin_free_mem;
    }
    if (params->raw_backing == NULL) {
        mem->header.flags |= cr_mem_flag_lin_free_backing;
    }
    mem->header.malloc = cr_mem_lin_malloc;
    mem->header.realloc = cr_mem_lin_realloc;
    mem->header.free = cr_mem_lin_free;
    mem->header.destroy = cr_mem_lin_destroy;
    mem->backing_size = params->backing_size;

    return (struct cr_mem*)mem;
}

void cr_mem_lin_reset(struct cr_mem* _mem) {
    struct cr_mem_lin* mem = (struct cr_mem_lin*)_mem;

    cr_mem_check_thread(&mem->header, "cr_mem_lin_reset");
    mem->offset = 0;

#if cr_mem_rec
    if (mem->header.flags & cr_mem_flag_wants_rec) {
        cr_rec_lock();

        for (uint32_t i = 0, i_end = cr_arr_ptr_get_count(mem->allocs); i < i_end; i++) {
            void* ptr = cr_arr_ptr_get(mem->allocs, i);
            cr_dict_rec_alloc_remove(rec.allocs, (struct cr_rec_alloc_key) { .mem = _mem, .ptr = ptr });
        }
        cr_arr_ptr_remove_all(mem->allocs);

        cr_rec_unlock();
    }
#endif
}
