#ifndef DRRD_H
#define DRRD_H

#include <limits.h>
#include <stdio.h>

#define MAX_GROUPS 10

typedef enum { THREAD_READY, THREAD_BLOCKED, THREAD_ZOMBIE } thread_state_t;

struct fiber_thread;  // ← forward

typedef struct sched_thread {
    int id;
    int priority;
    long long debt;
    thread_state_t state;
    struct sched_thread* next;
    struct sched_thread* prev;

    struct fiber_thread* fiber;
} sched_thread_t;

typedef struct sched_group {
    int priority;
    int count;
    long long W;
    long long W_base;
    long long deficit;
    long long fant_calls;
    sched_thread_t* current;

} sched_group_t;

typedef struct {
    int group_count;   // Реальное число групп (<= MAX_GROUPS)
    long long W;       // Текущий общий вес (может временно меняться в середине цикла)
    long long W_eff;   // Сколько осталось. для вычислений
    long long W_base;  // постоянный общий вес (без временных добавок Δ)
    long long R;       // Число квантов в текущем цикле

    int thread_id;       //  для присвоения ID новому потоку
    int thread_live;     //  для окончания цикла
    int thread_blocked;  //  для поимки дедлока

    sched_group_t groups[MAX_GROUPS];

} sched_state_t;

extern sched_state_t sched;

// --- Публичный API ---

void sched_init(int group_count, const int priorities[]);
sched_thread_t* sched_create_thread(int priority);
void sched_block_thread(sched_thread_t* thread);
void sched_unblock_thread(sched_thread_t* thread);
void sched_remove_thread(sched_thread_t* thread);

void sched_run(void);

#endif