#ifndef FIBER_H
#define FIBER_H

#include <stdint.h>
#include <stdlib.h>
#include <ucontext.h>

#include "../../varithreads.h"  // thread_t, thread_fn_t, VT_MAX_THREADS

struct sched_thread;

typedef struct fiber_thread {
    ucontext_t ctx;  // снимок регистров

    thread_fn_t fn;  // что запускать в первый раз
    void* arg;       // аргумент

    int finished;                    // «умер?» (для join)
    struct fiber_thread* join_head;  // кто ждёт меня
    struct fiber_thread* join_next;  // я в чьём-то join_head

    struct sched_thread* sched;
    uint32_t slot_index;  // где я лежу в slots[]
} fiber_thread_t;

/* ── Контракт с планировщиком ──────────────────── */
void fiber_create_thread(struct sched_thread* sched);
void fiber_remove_thread(struct sched_thread* sched);
void fiber_run_thread(struct sched_thread* sched) __attribute__((hot));

/* API для facade */
void fiber_init(void);
void fiber_cleanup(void);
thread_t fiber_create(thread_fn_t fn, void* arg, struct sched_thread* sched);
void fiber_yield(void) __attribute__((hot));

void fiber_block(void) __attribute__((hot));
void fiber_unblock(thread_t h);
void fiber_join(thread_t h);
void fiber_exit(void) __attribute__((noreturn));
thread_t fiber_self(void);
int fiber_finished(thread_t h);

#endif