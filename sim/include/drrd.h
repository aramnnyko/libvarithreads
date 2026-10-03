#ifndef SIM_H
#define SIM_H

#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct mock_thread_stat;

#define MAX_GROUPS 10

typedef enum { THREAD_READY, THREAD_BLOCKED, THREAD_DONE, THREAD_ZOMBIE } thread_state_t;

typedef struct mock_thread {
    int id;

    int priority;
    thread_state_t state;
    long long debt;
    struct mock_thread* next;
    struct mock_thread* prev;

    struct mock_thread_stat* stat;
} mock_thread_t;

typedef struct mock_group {
    int priority;
    int count;
    long long W;
    long long W_base;
    long long deficit;
    long long calls;
    long long fant_calls;
    mock_thread_t* head;
    mock_thread_t* current;

} mock_group_t;

typedef struct {
    int group_count;   // Реальное число групп (<= MAX_GROUPS)
    long long W;       // Текущий общий вес (может временно меняться в середине цикла)
    long long W_base;  // постоянный общий вес (без временных добавок Δ)
    long long R;       // Число квантов в текущем цикле

    int cycle;
    int thread_id;  //  для присвоения ID новому потоку

    mock_group_t groups[MAX_GROUPS];

} sim_state_t;

// --- Публичный API ---

void sim_init(int group_count, const int priorities[]);
mock_thread_t* sim_create_thread(int priority);

void sim_block_thread(mock_thread_t* thread);
void sim_unblock_thread(mock_thread_t* thread);
void sim_remove_thread(mock_thread_t* thread);

void sim_run(int iterations);

void sim_cleanup(void);
#endif