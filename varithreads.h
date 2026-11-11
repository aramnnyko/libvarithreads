#ifndef VARITHREADS_H
#define VARITHREADS_H

#include <stdint.h>

#define VT_MAX_THREADS 4096

typedef struct {
    uint32_t index;
    uint32_t generation;
} thread_t;

typedef void (*thread_fn_t)(void*);

/* жизненный цикл */
/* алгоритмозависимо */
#if defined(VT_SCHED_DRRD)
int vt_init(int group_count, const int priorities[]);
#elif defined(VT_SCHED_RR)
int vt_init(void);
#elif defined(VT_SCHED_WRR)
int vt_init(void);
#elif defined(VT_SCHED_STRIDE)
int vt_init(void);
#else
#error "Define VT_SCHED_DRRD, VT_SCHED_RR, VT_SCHED_WRR or VT_SCHED_STRIDE"
#endif

void vt_run(void);
void vt_cleanup(void);

/* потоки */
/* алгоритмозависимо */
#if defined(VT_SCHED_DRRD)
thread_t thread_create(thread_fn_t fn, void* arg, int priority);
#elif defined(VT_SCHED_RR)
thread_t thread_create(thread_fn_t fn, void* arg);
#elif defined(VT_SCHED_WRR)
thread_t thread_create(thread_fn_t fn, void* arg, int weight);
#elif defined(VT_SCHED_STRIDE)
thread_t thread_create(thread_fn_t fn, void* arg, int priority);
#else
#error "Define VT_SCHED_DRRD, VT_SCHED_RR, VT_SCHED_WRR or VT_SCHED_STRIDE"
#endif

void thread_yield(void);
void thread_block(void);
void thread_unblock(thread_t t);
void thread_join(thread_t t);
void thread_exit(void) __attribute__((noreturn));
thread_t thread_self(void);

#endif
