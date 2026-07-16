#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "cr.h"
#include "cr_tests.h"

#define M 256
#define N 128

typedef uint8_t m_t;
static_assert(M <= (1llu << (sizeof(m_t) * 8llu)), "not enough bits");
static_assert(M <= (1llu << 10) && N <= (1llu << 8), "args packing overflows 32-bit pointers");

static struct cr_executor* executor;
static struct cr_task* m[N];
static struct cr_task* end_task;
static m_t c[M * N];
static m_t s[M * N];

static void down(void* args) {
    uint64_t n = (uintptr_t)args & 0xff;
    uint64_t dist = ((uintptr_t)args >> 8) & 0x3ff;
    uint64_t i = (uintptr_t)args >> 18;
    c[i * N + n] += c[(i - (dist >> 1)) * N + n];
}

static void up(void* args) {
    uint64_t n = (uintptr_t)args & 0xff;
    uint64_t dist = ((uintptr_t)args >> 8) & 0x3ff;
    uint64_t i = (uintptr_t)args >> 18;
    m_t temp = i == dist - 1 ? 0 : c[i * N + n];
    c[i * N + n] = temp + c[(i - (dist >> 1)) * N + n];
    c[(i - (dist >> 1)) * N + n] = temp;
}

static void setup(void* args) {
    uint64_t n = (uintptr_t)args & 0xff;
    uint64_t dist = ((uintptr_t)args >> 8) & 0x3ff;
    uint64_t stage = ((uintptr_t)args >> 18) & 0x3ff;
    uint64_t dist_next;
    uint64_t stage_next = stage;
    if (stage == 0) {
        if (dist == 2) {
            m_t sr = 0;
            uint64_t seed = ((uintptr_t)args >> 28) * N + n + 1;
            uint64_t r = 0;
            for (uint64_t i = 0; i < M; i++) {
                if (i % 32 == 0) r = cr_rand(&seed);
                m_t v = r & 1;
                c[i * N + n] = v;
                s[i * N + n] = sr;
                sr += v;
                r >>= 1;
            }
        }
        dist_next = dist << 1;
        if (dist_next == M) {
            stage_next = 1;
        }
    } else {
        if (dist == 1) {
            for (uint64_t i = 0; i < M; i++) {
                assert(c[i * N + n] == s[i * N + n]);
            }
            cr_task_request_signal(end_task, m[n]);
            return;
        }
        dist_next = dist >> 1;
    }
    struct cr_task* next = cr_task_create(executor, setup, (void*)(uintptr_t)((stage_next << 18) | (dist_next << 8) | n));
    for (uint64_t i = dist - 1; i < M; i += dist) {
        struct cr_task* t = cr_task_create(executor, stage ? up : down, (void*)(uintptr_t)((i << 18) | (dist << 8) | n));
        cr_task_wait_request_signal(t, m[n]);
        cr_task_wait_request_signal(next, t);
        cr_task_run(t);
    }
    cr_task_run(next);
    m[n] = next;
}

static void bump(void* args) {
    (*(uint64_t*)args)++;
}

void cr_test_task(void) {
    uint32_t init_allocs = cr_rec_get_alloc_count();

    executor = cr_executor_create(cr_mem_libc, (int)cr_get_hardware_concurrency());
    for (uint64_t iter = 0; iter < 4; iter++) {
        struct cr_task* start = cr_task_create(executor, NULL, NULL);
        end_task = cr_task_create(executor, NULL, NULL);
        for (uint64_t i = 0; i < N; i++) {
            cr_task_wait(end_task);
            struct cr_task* t = cr_task_create(executor, setup, (void*)(uintptr_t)((iter << 28) | (2llu << 8) | i));
            cr_task_wait_request_signal(t, start);
            cr_task_run(t);
            m[i] = t;
        }
        cr_task_run(start);
        cr_task_run_sync(end_task);
    }
    cr_executor_destroy(executor);
    executor = NULL;
    assert(cr_rec_get_alloc_count() == init_allocs);

    struct cr_executor* idle = cr_executor_create(cr_mem_libc, 4);
    cr_executor_destroy(idle);
    assert(cr_rec_get_alloc_count() == init_allocs);

    uint64_t bumps = 0;
    for (uint64_t i = 0; i < 4; i++) {
        struct cr_executor* e = cr_executor_create(cr_mem_libc, 2);
        struct cr_task* t = cr_task_create(e, bump, &bumps);
        cr_task_run_sync(t);
        cr_executor_destroy(e);
        assert(bumps == i + 1);
    }
    assert(cr_rec_get_alloc_count() == init_allocs);

    printf("cr_test_task: ok\n");
}
