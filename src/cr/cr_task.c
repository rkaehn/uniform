#include "cr.h"
#include <pthread.h>
#include <stdatomic.h>

#define _cr_task_log(...) ((void)0)

#define _CR_TASK_FLAG_W (1llu << 63)
#define _CR_TASK_FLAG_S (1llu << 31)
#define _CR_TASK_FLAG_SYNC (1llu << 30)
#define _CR_EXECUTOR_FLAG_Q (1llu << 31)
#define _CR_POOL_BATCH 8llu
#define _CR_POOL_ITEM 16llu
#define _CR_POOL_IDX_MASK ((1llu << (_CR_POOL_BATCH + _CR_POOL_ITEM)) - 1llu)

struct _cr_sync {
#if cr_wasm
    _Atomic uint32_t is_done;
#else
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int is_done;
#endif
};

struct _cr_task_ref {
    _Atomic uint32_t next_idx_pool;
    _Atomic uint32_t next_idx_list;
    struct cr_task* task;
};

struct _cr_pool {
#if cr_wasm
    _Atomic uint32_t lock;
#else
    pthread_mutex_t mutex;
#endif
    struct cr_mem* mem;
    void* batches[1llu << _CR_POOL_BATCH];
    uint64_t batch_count;
    uint64_t item_size;
    _Atomic uint64_t c;
};

struct cr_task {
    _Atomic uint32_t next_idx_pool;
    _Atomic uint32_t next_idx_list;
    uint64_t idx;
    struct cr_executor* executor;
    struct _cr_sync* sync;
    _Atomic uint64_t wait;
    _Atomic uint64_t signal;
    void (*func)(void*);
    void* args;
#if cr_wasm
    char _[16];
#endif
};
static_assert(sizeof(struct cr_task) == 64, "");

struct cr_executor {
    struct _cr_pool task_pool;
    struct _cr_pool task_ref_pool;
    _Atomic uint64_t c;
#if !cr_wasm
    pthread_mutex_t mutex;
    pthread_cond_t cond;
#endif
    pthread_t* workers;
    int worker_count;
};

void _cr_pool_init(struct _cr_pool* pool, struct cr_mem* mem, uint64_t item_size);
void _cr_pool_deinit(struct _cr_pool* pool);
uint64_t _cr_pool_pop(struct _cr_pool* pool);
void _cr_pool_push(struct _cr_pool* pool, uint64_t idx);

void _cr_task_end_wait(struct cr_task* task);
void _cr_task_begin_signal(struct cr_task* task);

void _cr_executor_schedule_task(struct cr_executor* executor, struct cr_task* task);
void* _cr_executor_worker_func(void* args);

#if cr_wasm

static_assert(_CR_POOL_BATCH + _CR_POOL_ITEM <= 32, "workers futex-wait on the low 32 bits of executor c, so every idx bit must live there and the ABA counter must not");
static_assert(__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__, "the futex address (int*)&executor->c is the low word of c only on little endian");

__attribute__((import_module("env"), import_name("cr_js_thread_free_count"))) uint32_t cr_js_thread_free_count(void);

static void _cr_pool_lock(struct _cr_pool* pool) {
    uint32_t expected = 0;
    while (!atomic_compare_exchange_weak_explicit(&pool->lock, &expected, 1u, memory_order_acquire, memory_order_relaxed)) {
        expected = 0;
    }
}

static void _cr_pool_unlock(struct _cr_pool* pool) {
    atomic_store_explicit(&pool->lock, 0u, memory_order_release);
}

#else

static void _cr_pool_lock(struct _cr_pool* pool) {
    pthread_mutex_lock(&pool->mutex);
}

static void _cr_pool_unlock(struct _cr_pool* pool) {
    pthread_mutex_unlock(&pool->mutex);
}

#endif

void _cr_pool_init(struct _cr_pool* pool, struct cr_mem* mem, uint64_t item_size) {
#if cr_wasm
    atomic_store_explicit(&pool->lock, 0u, memory_order_release);
#else
    pthread_mutex_init(&pool->mutex, NULL);
#endif
    pool->mem = mem;
    memset(pool->batches, 0, sizeof(pool->batches));
    pool->batch_count = 0;
    pool->item_size = item_size;
    atomic_store_explicit(&pool->c, _CR_POOL_IDX_MASK, memory_order_release);
}

void _cr_pool_deinit(struct _cr_pool* pool) {
#if !cr_wasm
    pthread_mutex_destroy(&pool->mutex);
#endif
    for (uint64_t i = 0; i < pool->batch_count; i++) {
        cr_free(pool->mem, pool->batches[i], cr_loc);
    }
}

uint64_t _cr_pool_pop(struct _cr_pool* pool) {
    _cr_task_log("pool pop (item size: %llu)\n", pool->item_size);
    uint64_t c = atomic_load_explicit(&pool->c, memory_order_acquire);
    uint64_t n, idx;
    do {
        idx = c & _CR_POOL_IDX_MASK;
        if (idx == _CR_POOL_IDX_MASK) {
            _cr_pool_lock(pool);
            c = atomic_load_explicit(&pool->c, memory_order_acquire);
            idx = c & _CR_POOL_IDX_MASK;
            if (idx == _CR_POOL_IDX_MASK) {
                _cr_task_log("pool alloc (item size: %llu)\n", pool->item_size);
                uint64_t batch_idx = pool->batch_count++;
                assert(batch_idx < (1llu << _CR_POOL_BATCH) && "too many tasks");
                pool->batches[batch_idx] = cr_malloc(pool->mem, (1llu << _CR_POOL_ITEM) * pool->item_size, 0, cr_loc);
                assert(pool->batches[batch_idx] && "alloc failed");
                idx = batch_idx << _CR_POOL_ITEM;
                for (uint64_t i = 1; i < (1llu << _CR_POOL_ITEM) - 1llu; i++) {
                    void* item = (char*)(pool->batches[batch_idx]) + i * pool->item_size;
                    atomic_store_explicit((_Atomic uint32_t*)item, idx | (i + 1llu), memory_order_relaxed);
                }
                void* item = (char*)(pool->batches[batch_idx]) + ((1llu << _CR_POOL_ITEM) - 1llu) * pool->item_size;
                atomic_store_explicit((_Atomic uint32_t*)item, _CR_POOL_IDX_MASK, memory_order_relaxed);
                atomic_store_explicit(&pool->c, ((c + (1llu << 32)) & ~_CR_POOL_IDX_MASK) | idx | 1llu, memory_order_release);
                _cr_pool_unlock(pool);
                return idx;
            }
            _cr_pool_unlock(pool);
        }
        uint64_t batch_idx = idx >> _CR_POOL_ITEM;
        uint64_t item_idx = idx & ((1llu << _CR_POOL_ITEM) - 1llu);
        void* item = (char*)(pool->batches[batch_idx]) + item_idx * pool->item_size;
        n = ((c + (1llu << 32)) & ~_CR_POOL_IDX_MASK) | atomic_load_explicit((_Atomic uint32_t*)item, memory_order_relaxed);
    } while (!atomic_compare_exchange_weak_explicit(&pool->c, &c, n, memory_order_acq_rel, memory_order_acquire));
    return idx;
}

void _cr_pool_push(struct _cr_pool* pool, uint64_t idx) {
    _cr_task_log("pool push (item size: %llu)\n", pool->item_size);
    uint64_t batch_idx = idx >> _CR_POOL_ITEM;
    uint64_t item_idx = idx & ((1llu << _CR_POOL_ITEM) - 1llu);
    void* item = (char*)(pool->batches[batch_idx]) + item_idx * pool->item_size;
    uint64_t c = atomic_load_explicit(&pool->c, memory_order_acquire);
    uint64_t n;
    do {
        atomic_store_explicit((_Atomic uint32_t*)item, c & _CR_POOL_IDX_MASK, memory_order_relaxed);
        n = ((c + (1llu << 32)) & ~_CR_POOL_IDX_MASK) | idx;
    } while (!atomic_compare_exchange_weak_explicit(&pool->c, &c, n, memory_order_acq_rel, memory_order_acquire));
}

struct cr_task* cr_task_create(struct cr_executor* executor, void (*func)(void*), void* args) {
    uint64_t idx = _cr_pool_pop(&executor->task_pool);
    uint64_t batch_idx = idx >> _CR_POOL_ITEM;
    uint64_t item_idx = idx & ((1llu << _CR_POOL_ITEM) - 1llu);
    struct cr_task* task = (struct cr_task*)executor->task_pool.batches[batch_idx] + item_idx;
    atomic_store_explicit(&task->next_idx_list, _CR_POOL_IDX_MASK, memory_order_relaxed);
    task->idx = idx;
    task->executor = executor;
    task->sync = NULL;
    task->func = func;
    task->args = args;
    atomic_store_explicit(&task->wait, 0, memory_order_release);
    atomic_store_explicit(&task->signal, _CR_POOL_IDX_MASK, memory_order_release);
    return task;
}

void cr_task_retain(struct cr_task* task) {
    uint64_t c = atomic_fetch_add_explicit(&task->signal, 1llu << 48, memory_order_relaxed);
    assert((!(c & _CR_TASK_FLAG_S) || (c >> 48) != 0) && "task no longer exists");
    assert((c >> 48) < 0xffff && "retain count too high");
}

void cr_task_release(struct cr_task* task) {
    uint64_t c = atomic_fetch_sub_explicit(&task->signal, 1llu << 48, memory_order_acq_rel);
    assert((c >> 48) > 0 && "unbalanced retain/release count");
    if ((c & _CR_TASK_FLAG_S) && (c >> 48) == 1) _cr_pool_push(&task->executor->task_pool, task->idx);
}

void cr_task_request_signal(struct cr_task* task, struct cr_task* task_signaling) {
    uint64_t c = atomic_load_explicit(&task_signaling->signal, memory_order_acquire);
    uint64_t idx = _cr_pool_pop(&task_signaling->executor->task_ref_pool);
    uint64_t batch_idx = idx >> _CR_POOL_ITEM;
    uint64_t item_idx = idx & ((1llu << _CR_POOL_ITEM) - 1llu);
    struct _cr_task_ref* task_ref = (struct _cr_task_ref*)task_signaling->executor->task_ref_pool.batches[batch_idx] + item_idx;
    task_ref->task = task;
    uint64_t n;
    do {
        assert(((c >> 32) & 0xffff) < 0xffff && "too many signal requests");
        if ((c & _CR_TASK_FLAG_S) == 0) {
            atomic_store_explicit(&task_ref->next_idx_list, c & _CR_POOL_IDX_MASK, memory_order_relaxed);
            n = ((c + (1llu << 32)) & ~_CR_POOL_IDX_MASK) | idx;
        } else break;
    } while (!atomic_compare_exchange_weak_explicit(&task_signaling->signal, &c, n, memory_order_acq_rel, memory_order_acquire));
    if (c & _CR_TASK_FLAG_S) {
        _cr_pool_push(&task_signaling->executor->task_ref_pool, idx);
        cr_task_signal(task);
    }
}

void cr_task_wait_request_signal(struct cr_task* task, struct cr_task* task_signaling) {
    cr_task_wait(task);
    cr_task_request_signal(task, task_signaling);
}

void cr_task_wait(struct cr_task* task) {
    uint64_t c = atomic_fetch_add_explicit(&task->wait, 1, memory_order_relaxed);
    assert(c != _CR_TASK_FLAG_W && "unexpected wait request");
}

void cr_task_signal(struct cr_task* task) {
    uint64_t c = atomic_fetch_sub_explicit(&task->wait, 1, memory_order_acq_rel);
    assert((c & ~_CR_TASK_FLAG_W) > 0 && "negative wait count");
    if (c == (_CR_TASK_FLAG_W | 1llu)) _cr_task_end_wait(task);
}

void cr_task_run(struct cr_task* task) {
    uint64_t c = atomic_fetch_or_explicit(&task->wait, _CR_TASK_FLAG_W, memory_order_acq_rel);
    assert((c & _CR_TASK_FLAG_W) == 0 && "tried to run more than once");
    if (c == 0) _cr_task_end_wait(task);
}

void cr_task_sync(struct cr_task* task) {
#if cr_wasm
    struct _cr_sync sync;
    atomic_store_explicit(&sync.is_done, 0u, memory_order_relaxed);
    task->sync = &sync;
    uint64_t c = atomic_fetch_or_explicit(&task->signal, _CR_TASK_FLAG_SYNC, memory_order_acq_rel);
    assert((c & _CR_TASK_FLAG_SYNC) == 0 && "sync flag already set");
    if ((c & _CR_TASK_FLAG_S) == 0) {
        while (atomic_load_explicit(&sync.is_done, memory_order_acquire) == 0) {}
    }
#else
    struct _cr_sync sync;
    pthread_mutex_init(&sync.mutex, NULL);
    pthread_cond_init(&sync.cond, NULL);
    sync.is_done = 0;
    task->sync = &sync;
    uint64_t c = atomic_fetch_or_explicit(&task->signal, _CR_TASK_FLAG_SYNC, memory_order_acq_rel);
    assert((c & _CR_TASK_FLAG_SYNC) == 0 && "sync flag already set");
    if ((c & _CR_TASK_FLAG_S) == 0) {
        pthread_mutex_lock(&sync.mutex);
        while (!sync.is_done) {
            pthread_cond_wait(&sync.cond, &sync.mutex);
        }
        pthread_mutex_unlock(&sync.mutex);
    }
    pthread_cond_destroy(&sync.cond);
    pthread_mutex_destroy(&sync.mutex);
#endif
}

void cr_task_run_sync(struct cr_task* task) {
    cr_task_retain(task);
    cr_task_run(task);
    cr_task_sync(task);
    cr_task_release(task);
}

void _cr_task_end_wait(struct cr_task* task) {
    if (task->func) {
        _cr_executor_schedule_task(task->executor, task);
    } else {
        _cr_task_begin_signal(task);
    }
}

void _cr_task_begin_signal(struct cr_task* task) {
    uint64_t c = atomic_fetch_or_explicit(&task->signal, _CR_TASK_FLAG_S, memory_order_acq_rel);
    assert((c & _CR_TASK_FLAG_S) == 0 && "tried to finalize more than once");
    if (c & _CR_TASK_FLAG_SYNC) {
#if cr_wasm
        atomic_store_explicit(&task->sync->is_done, 1u, memory_order_release);
#else
        pthread_mutex_lock(&task->sync->mutex);
        task->sync->is_done = 1;
        pthread_cond_signal(&task->sync->cond);
        pthread_mutex_unlock(&task->sync->mutex);
#endif
    }
    uint64_t waiting_count = (c >> 32) & 0xffff;
    uint64_t idx = c & _CR_POOL_IDX_MASK;
    for (uint64_t i = 0; i < waiting_count; i++) {
        assert(idx != _CR_POOL_IDX_MASK && "inconsistent state");
        uint64_t batch_idx = idx >> _CR_POOL_ITEM;
        uint64_t item_idx = idx & ((1llu << _CR_POOL_ITEM) - 1llu);
        struct _cr_task_ref* task_ref = (struct _cr_task_ref*)task->executor->task_ref_pool.batches[batch_idx] + item_idx;
        struct cr_task* next_task = task_ref->task;
        uint64_t next_idx = atomic_load_explicit(&task_ref->next_idx_list, memory_order_relaxed);
        _cr_pool_push(&task->executor->task_ref_pool, idx);
        cr_task_signal(next_task);
        idx = next_idx;
    }
    if ((c >> 48) == 0) _cr_pool_push(&task->executor->task_pool, task->idx);
}

struct cr_executor* cr_executor_create(struct cr_mem* mem, int worker_count) {
    size_t aligned_size_executor = (sizeof(struct cr_executor) + 255) & ~(size_t)0xff;
    struct cr_executor* executor = cr_malloc(mem, aligned_size_executor + (size_t)worker_count * sizeof(pthread_t), 0, cr_loc);
    assert(executor && "alloc failed");
    _cr_pool_init(&executor->task_pool, mem, sizeof(struct cr_task));
    _cr_pool_init(&executor->task_ref_pool, mem, sizeof(struct _cr_task_ref));
    atomic_store_explicit(&executor->c, _CR_POOL_IDX_MASK, memory_order_release);
#if !cr_wasm
    pthread_mutex_init(&executor->mutex, NULL);
    pthread_cond_init(&executor->cond, NULL);
#endif
    executor->workers = (pthread_t*)((char*)executor + aligned_size_executor);
    executor->worker_count = worker_count;
    for (int i = 0; i < worker_count; i++) {
        pthread_create(&executor->workers[i], NULL, _cr_executor_worker_func, executor);
#if cr_wasm
        pthread_detach(executor->workers[i]);
#endif
    }
    return executor;
}

void cr_executor_destroy(struct cr_executor* executor) {
#if cr_wasm
    uint32_t free_count = cr_js_thread_free_count();
    atomic_fetch_or_explicit(&executor->c, _CR_EXECUTOR_FLAG_Q, memory_order_acq_rel);
    __builtin_wasm_memory_atomic_notify((int*)(void*)&executor->c, (unsigned int)executor->worker_count);
    while (cr_js_thread_free_count() < free_count + (uint32_t)executor->worker_count) {}
#else
    pthread_mutex_lock(&executor->mutex);
    atomic_fetch_or_explicit(&executor->c, _CR_EXECUTOR_FLAG_Q, memory_order_acq_rel);
    pthread_cond_broadcast(&executor->cond);
    pthread_mutex_unlock(&executor->mutex);
    for (int i = 0; i < executor->worker_count; i++) {
        pthread_join(executor->workers[i], NULL);
    }
    pthread_cond_destroy(&executor->cond);
    pthread_mutex_destroy(&executor->mutex);
#endif
    _cr_pool_deinit(&executor->task_pool);
    _cr_pool_deinit(&executor->task_ref_pool);
    cr_free(executor->task_pool.mem, executor, cr_loc);
}

void _cr_executor_schedule_task(struct cr_executor* executor, struct cr_task* task) {
#if cr_wasm
    uint64_t c = atomic_load_explicit(&executor->c, memory_order_acquire);
    uint64_t n;
    do {
        atomic_store_explicit(&task->next_idx_list, (uint32_t)(c & _CR_POOL_IDX_MASK), memory_order_relaxed);
        n = ((c + (1llu << 32)) & ~_CR_POOL_IDX_MASK) | task->idx;
    } while (!atomic_compare_exchange_weak_explicit(&executor->c, &c, n, memory_order_acq_rel, memory_order_acquire));
    __builtin_wasm_memory_atomic_notify((int*)(void*)&executor->c, 1);
#else
    uint64_t c = atomic_load_explicit(&executor->c, memory_order_acquire);
    uint64_t n;
    do {
        uint64_t idx = c & _CR_POOL_IDX_MASK;
        if (idx == _CR_POOL_IDX_MASK) {
            pthread_mutex_lock(&executor->mutex);
            c = atomic_load_explicit(&executor->c, memory_order_acquire);
            idx = c & _CR_POOL_IDX_MASK;
            if (idx == _CR_POOL_IDX_MASK) {
                atomic_store_explicit(&task->next_idx_list, _CR_POOL_IDX_MASK, memory_order_relaxed);
                atomic_store_explicit(&executor->c, ((c + (1llu << 32)) & ~_CR_POOL_IDX_MASK) | task->idx, memory_order_release);
                pthread_cond_signal(&executor->cond);
                pthread_mutex_unlock(&executor->mutex);
                return;
            }
            pthread_mutex_unlock(&executor->mutex);
        }
        atomic_store_explicit(&task->next_idx_list, (uint32_t)idx, memory_order_relaxed);
        n = ((c + (1llu << 32)) & ~_CR_POOL_IDX_MASK) | task->idx;
    } while (!atomic_compare_exchange_weak_explicit(&executor->c, &c, n, memory_order_acq_rel, memory_order_acquire));
    pthread_cond_signal(&executor->cond);
#endif
}

void* _cr_executor_worker_func(void* args) {
    struct cr_executor* executor = args;
    while (1) {
        struct cr_task* task;
        uint64_t c = atomic_load_explicit(&executor->c, memory_order_acquire);
        uint64_t n;
        do {
            uint64_t idx = c & _CR_POOL_IDX_MASK;
            if (idx == _CR_POOL_IDX_MASK) {
#if cr_wasm
                if (c & _CR_EXECUTOR_FLAG_Q) return NULL;
                __builtin_wasm_memory_atomic_wait32((int*)(void*)&executor->c, (int)(uint32_t)c, -1);
                goto next_task;
#else
                pthread_mutex_lock(&executor->mutex);
                c = atomic_load_explicit(&executor->c, memory_order_acquire);
                idx = c & _CR_POOL_IDX_MASK;
                if (idx == _CR_POOL_IDX_MASK) {
                    if (c & _CR_EXECUTOR_FLAG_Q) {
                        pthread_mutex_unlock(&executor->mutex);
                        return NULL;
                    }
                    pthread_cond_wait(&executor->cond, &executor->mutex);
                    pthread_mutex_unlock(&executor->mutex);
                    goto next_task;
                }
                pthread_mutex_unlock(&executor->mutex);
#endif
            }
            uint64_t batch_idx = idx >> _CR_POOL_ITEM;
            uint64_t item_idx = idx & ((1llu << _CR_POOL_ITEM) - 1llu);
            task = (struct cr_task*)executor->task_pool.batches[batch_idx] + item_idx;
            n = ((c + (1llu << 32)) & ~_CR_POOL_IDX_MASK) | atomic_load_explicit(&task->next_idx_list, memory_order_relaxed);
        } while (!atomic_compare_exchange_weak_explicit(&executor->c, &c, n, memory_order_acq_rel, memory_order_acquire));
        task->func(task->args);
        _cr_task_begin_signal(task);
    next_task: ;
    }
    return NULL;
}
