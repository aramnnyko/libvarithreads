#ifndef LOTTERY_H
#define LOTTERY_H

#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "../common/external/byronlai_bit/bit.h"

#define LOTTERY_MAX_THREADS 4096

typedef enum { THREAD_READY, THREAD_BLOCKED, THREAD_ZOMBIE } thread_state_t;

struct fiber_thread;  // ← forward

typedef struct sched_thread {
    int ticket;
    int slot;

    thread_state_t state;
    struct fiber_thread* fiber;
} sched_thread_t;

typedef struct {
    sched_thread_t* threads[LOTTERY_MAX_THREADS];  // массив всех потоков
    int bit_array[LOTTERY_MAX_THREADS + 1];        // массив для Fenwick tree
    struct bit tree;                               // сам объект bit
    long long total_tickets;                       // общее число билетов

    int thread_live;     //  для окончания цикла
    int thread_blocked;  //  для поимки дедлока
} sched_state_t;

extern sched_state_t sched;

// --- Публичный API ---

void sched_init(void);
sched_thread_t* sched_create_thread(int ticket);
void sched_block_thread(sched_thread_t* thread);
void sched_unblock_thread(sched_thread_t* thread);
void sched_remove_thread(sched_thread_t* thread);

void sched_run(void);

void sched_cleanup(void);

#endif