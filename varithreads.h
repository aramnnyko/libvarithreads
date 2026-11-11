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
int vt_init(int group_count, const int priorities[]);
void vt_run(void);
void vt_cleanup(void);

/* потоки */
thread_t thread_create(thread_fn_t fn, void* arg, int priority);
void thread_yield(void);
void thread_block(void);
void thread_unblock(thread_t t);
void thread_join(thread_t t);
void thread_exit(void) __attribute__((noreturn));
thread_t thread_self(void);
int thread_finished(thread_t t);

#endif