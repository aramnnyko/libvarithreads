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

typedef enum { THREAD_READY, THREAD_BLOCKED, THREAD_ZOMBIE } thread_state_t;

typedef struct mock_thread {
    int id;
    int priority;
    long long debt;
    thread_state_t state;
    struct mock_thread* next;
    struct mock_thread* prev;

    struct mock_thread_stat* stat;  // Для статистики
    void* user_data;                // Для генератора используется, контролируется и освобождается только генератором
    long long got_debugging;        // отладочная
} mock_thread_t;

typedef struct mock_group {
    int priority;
    int count;
    long long W;
    long long W_base;
    long long deficit;
    long long fant_calls;
    mock_thread_t* current;

} mock_group_t;

typedef struct {
    int group_count;   // Реальное число групп (<= MAX_GROUPS)
    long long W;       // Текущий общий вес (может временно меняться в середине цикла)
    long long W_eff;   // Сколько осталось. для вычислений
    long long W_base;  // постоянный общий вес (без временных добавок Δ)
    long long R;       // Число квантов в текущем цикле

    int thread_id;  //  для присвоения ID новому потоку

    mock_group_t groups[MAX_GROUPS];

    int cycle;  // Для генератора и статистики.
} sim_state_t;

extern sim_state_t sim;

// --- Публичный API ---

void sim_init(int group_count, const int priorities[]);
mock_thread_t* sim_create_thread(int priority);
void sim_block_thread(mock_thread_t* thread);
void sim_unblock_thread(mock_thread_t* thread);
void sim_remove_thread(mock_thread_t* thread);

void sim_run(int iterations);

void sim_cleanup(void);

// Для генератора
void sim_step(void);
void reset(void);

#endif