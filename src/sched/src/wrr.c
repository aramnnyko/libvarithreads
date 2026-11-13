#include "../include/wrr.h"

#include <stdlib.h>

#include "../../fiber/include/fiber.h"

sched_state_t sched;

// ============================================================================
//  Круговой двусвязный список потоков внутри группы
// ============================================================================

static void insert_thread(sched_thread_t* t) {
    if (sched.current == NULL) {
        t->next = t;
        t->prev = t;
        sched.current = t;
        return;
    }

    sched_thread_t* curr = sched.current;
    sched_thread_t* next = curr->next;

    // curr <-> t <-> next
    curr->next = t;
    t->prev = curr;
    t->next = next;
    next->prev = t;
}

static void remove_thread(sched_thread_t* t) {
    if (t->next == t) {
        sched.current = NULL;
        return;
    }

    sched_thread_t* prev = t->prev;
    sched_thread_t* next = t->next;

    //    prev <-> next
    prev->next = next;
    next->prev = prev;

    sched.current = t->next;
}

// ============================================================================
//  Обработчики состояний
// ============================================================================

static void handle_thread_blocked(sched_thread_t* /* t */) { return; /* пропустить  */ }

static void handle_thread_ready(sched_thread_t* t) {
    while (t->quantum_left > 0) {
        fiber_run_thread(t);  // «эй, я выбрал вот этот — делай что хочешь»
        t->quantum_left--;

        if (t->state != THREAD_READY) break;
    }

    t->quantum_left = t->weight;
}

static void handle_thread_remove(sched_thread_t* t) {
    sched.thread_live--;

    remove_thread(t);
    fiber_remove_thread(t);  // «эй, я удаляюсь — забери user_data»
    free(t);
}

// ============================================================================
//  Диспетчер
// ============================================================================

typedef enum { STEP_CONTINUE, STEP_ABORT } step_result_t;

static step_result_t handle_thread_state(sched_thread_t* t) {
    switch (t->state) {
        case THREAD_BLOCKED: handle_thread_blocked(t); return STEP_CONTINUE;
        case THREAD_READY  : handle_thread_ready(t); return STEP_CONTINUE;
        case THREAD_ZOMBIE : handle_thread_remove(t); return STEP_ABORT;
        default            : return STEP_CONTINUE;
    }
}

// ============================================================================
//  Шаг
// ============================================================================

void sched_step(void) {
    if (!sched.current) return;

    if (handle_thread_state(sched.current) == STEP_ABORT) return;

    sched.current = sched.current->next;
}

// ============================================================================
//  Основной цикл
// ============================================================================

void sched_run(void) {
    while (sched.thread_live > 0 && sched.thread_live > sched.thread_blocked) { sched_step(); }

    if (sched.thread_live > 0) {  // Дедлок
        fprintf(stderr, "sched_run: deadlock (%d live, %d blocked)\n", sched.thread_live, sched.thread_blocked);
    }
}

// ============================================================================
//  Инициализация
// ============================================================================

void sched_init() {
    sched.current = NULL;
    sched.thread_live = 0;
    sched.thread_blocked = 0;
}

// ============================================================================
//  Создание / удаление
// ============================================================================

sched_thread_t* sched_create_thread(int weight) {
    if (weight <= 0) {
        fprintf(stderr, "sched_create_thread: weight must be > 0, got %d\n", weight);
        return NULL;
    }

    sched_thread_t* t = calloc(1, sizeof(*t));
    if (!t) return NULL;
    fiber_create_thread(t);  // «эй, я создался — положи в user_data что надо»
    if (!t->fiber) {
        free(t);
        return NULL;
    }

    t->weight = weight;
    t->quantum_left = weight;
    t->state = THREAD_READY;
    t->next = t;
    t->prev = t;

    insert_thread(t);
    sched.thread_live++;

    return t;
}

void sched_remove_thread(sched_thread_t* thread) {
    sched_unblock_thread(thread);
    thread->state = THREAD_ZOMBIE;
}

void sched_cleanup(void) {
    while (sched.current != NULL) {
        sched_remove_thread(sched.current);
        handle_thread_remove(sched.current);
    }
}

// ============================================================================
//  Блокировка / разблокировка
// ============================================================================

void sched_block_thread(sched_thread_t* thread) {
    if (thread->state == THREAD_READY) {
        thread->state = THREAD_BLOCKED;
        sched.thread_blocked++;
    }
}

void sched_unblock_thread(sched_thread_t* thread) {
    if (thread->state == THREAD_BLOCKED) {
        thread->state = THREAD_READY;
        sched.thread_blocked--;
    }
}