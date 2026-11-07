#ifndef FIBER_H
#define FIBER_H

#include <stdlib.h>
#include <ucontext.h>

struct sched_thread;  // ← forward

typedef struct fiber_thread {
    struct sched_thread* sched;
    ucontext_t ctx;
    void* stack;
    // thread_fn_t fn;
    void* arg;
    int ctx_ready;

    struct fiber_thread_t* join_head;  // кто меня ждёт
    struct fiber_thread_t* join_next;  // моя ссылка, пока я жду

    struct fiber_thread_t* next_all;   // все потоки
    struct fiber_thread_t* next_dead;  // мёртвые, чей стек можно освободить
} fiber_thread_t;

/* ── Контракт с планировщиком ──────────────────── */

void fiber_create_thread(struct sched_thread* sched_t);  // «эй, создался — положи в user_data что надо»
void fiber_remove_thread(struct sched_thread* sched_t);  // «эй, удаляюсь — забери user_data»
void fiber_run_thread(struct sched_thread* sched_t);     // «эй, я выбрал вот этот — делай что хочешь»

#endif