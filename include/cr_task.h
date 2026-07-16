#pragma once

struct cr_mem;
struct cr_task;
struct cr_executor;

struct cr_task* cr_task_create(struct cr_executor* executor, void (*func)(void*), void* args);
void cr_task_retain(struct cr_task* task);
void cr_task_release(struct cr_task* task);
void cr_task_request_signal(struct cr_task* task, struct cr_task* task_signaling);
void cr_task_wait_request_signal(struct cr_task* task, struct cr_task* task_signaling);
void cr_task_wait(struct cr_task* task);
void cr_task_signal(struct cr_task* task);
void cr_task_run(struct cr_task* task);
void cr_task_sync(struct cr_task* task);
void cr_task_run_sync(struct cr_task* task);

struct cr_executor* cr_executor_create(struct cr_mem* mem, int worker_count);
void cr_executor_destroy(struct cr_executor* executor);
