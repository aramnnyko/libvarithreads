#ifndef WRR_H
#define WRR_H

#include <limits.h>
#include <stdio.h>

typedef enum { THREAD_READY, THREAD_BLOCKED, THREAD_ZOMBIE } thread_state_t;

struct fiber_thread;  // ← forward

typedef struct sched_thread {
    int weight;
    int quantum_left;

    thread_state_t state;

    struct sched_thread* next;
    struct sched_thread* prev;

    struct fiber_thread* fiber;
} sched_thread_t;

typedef struct {
    sched_thread_t* current;
    int thread_live;     //  для окончания цикла
    int thread_blocked;  //  для поимки дедлока
} sched_state_t;

extern sched_state_t sched;

// --- Публичный API ---

void sched_init();
sched_thread_t* sched_create_thread(int weight);
void sched_block_thread(sched_thread_t* thread);
void sched_unblock_thread(sched_thread_t* thread);
void sched_remove_thread(sched_thread_t* thread);

void sched_run(void);

void sched_cleanup(void);

#endif