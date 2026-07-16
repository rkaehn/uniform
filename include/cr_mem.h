#pragma once

struct cr_mem;

struct cr_mem_buddy_params {
    struct cr_mem* mem;
    int32_t backing_po2;
    int32_t l0_po2;
    void* raw_mem;
    void* raw_nodes;
    void* raw_backing;
    bool wants_rec;
};

struct cr_mem_lin_params {
    struct cr_mem* mem;
    void* raw_mem;
    void* raw_backing;
    uint64_t backing_size;
    bool wants_rec;
};

extern struct cr_mem* const cr_mem_libc;

uint32_t cr_rec_get_alloc_count(void);
uint32_t cr_rec_get_alloc_count_filter_mem(struct cr_mem* mem);
uint32_t cr_rec_get_max_alloc_count(void);
void cr_rec_print_allocs(void);

void* cr_malloc(struct cr_mem* mem, uint64_t size, uint64_t align, const char* label);
void* cr_realloc(struct cr_mem* mem, void* ptr, uint64_t size, const char* label);
void* cr_try_malloc(struct cr_mem* mem, uint64_t size, uint64_t align, const char* label);
void* cr_try_realloc(struct cr_mem* mem, void* ptr, uint64_t size, const char* label);
void cr_free(struct cr_mem* mem, void* ptr, const char* label);

void cr_mem_destroy(struct cr_mem* mem);

uint64_t cr_mem_buddy_get_mem_size(void);
uint64_t cr_mem_buddy_get_nodes_size(int32_t backing_po2, int32_t l0_po2);
uint64_t cr_mem_buddy_get_backing_size(int32_t backing_po2);
struct cr_mem* cr_mem_buddy_create(const struct cr_mem_buddy_params* params);

struct cr_mem* cr_mem_lin_create(const struct cr_mem_lin_params* params);
void cr_mem_lin_reset(struct cr_mem* mem);
void* cr_mem_lin_get_backing(struct cr_mem* mem);
