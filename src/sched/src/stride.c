#include "../include/stride.h"

#include <stdlib.h>

#include "../../fiber/include/fiber.h"

#define STRIDE_BIG (1LL << 32)  // константа для шага

sched_state_t sched;

// ============================================================================
//  Круговой двусвязный список потоков внутри группы
// ============================================================================

static void insert_thread(sched_thread_t* t) {
    if (sched.head == NULL) {
        t->next = t;
        t->prev = t;
        sched.head = t;
        return;
    }

    sched_thread_t* curr = sched.head;
    sched_thread_t* next = curr->next;

    // curr <-> t <-> next
    curr->next = t;
    t->prev = curr;
    t->next = next;
    next->prev = t;
}

static void remove_thread(sched_thread_t* t) {
    if (t->next == t) {
        sched.head = NULL;
        return;
    }

    sched_thread_t* prev = t->prev;
    sched_thread_t* next = t->next;

    //    prev <-> next
    prev->next = next;
    next->prev = prev;

    sched.head = t->next;
}

// ============================================================================
//  Работа с группами
// ============================================================================

static sched_thread_t* sched_select_thread(void) {
    sched_thread_t* best = NULL;
    sched_thread_t* t = sched.head;

    do {
        if (t->state == THREAD_ZOMBIE) return t;
        if (t->state == THREAD_READY)
            if (!best || t->pass < best->pass) best = t;

        t = t->next;
    } while (t != sched.head);

    return best;
}

// ============================================================================
//  Обработчики состояний
// ============================================================================

static void handle_thread_blocked(sched_thread_t* /* t */) { return; /* пропустить  */ }

static void handle_thread_ready(sched_thread_t* t) {
    fiber_run_thread(t);  // «эй, я выбрал вот этот — делай что хочешь»
}

static void handle_thread_remove(sched_thread_t* t) {
    sched.thread_live--;

    sched.global_tickets -= t->priority;
    if (sched.global_tickets > 0) sched.global_stride = STRIDE_BIG / sched.global_tickets;

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
        case THREAD_BLOCKED: handle_thread_blocked(t); return STEP_ABORT;
        case THREAD_READY  : handle_thread_ready(t); return t->state == THREAD_READY ? STEP_CONTINUE : STEP_ABORT;
        case THREAD_ZOMBIE : handle_thread_remove(t); return STEP_ABORT;
        default            : return STEP_CONTINUE;
    }
}

// ============================================================================
//  Шаг
// ============================================================================

void sched_step(void) {
    if (!sched.head) return;

    sched_thread_t* t = sched_select_thread();
    if (!t) return;

    sched.global_pass += sched.global_stride;
    if (handle_thread_state(t) == STEP_ABORT) return;

    t->pass += t->stride;
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

void sched_init(void) {
    sched.head = NULL;
    sched.global_pass = 0;
    sched.global_tickets = 0;
    sched.global_stride = 0;
    sched.thread_live = 0;
    sched.thread_blocked = 0;
}

// ============================================================================
//  Создание / удаление
// ============================================================================

sched_thread_t* sched_create_thread(int priority) {
    if (priority <= 0) {
        fprintf(stderr, "sched_create_thread: priority must be > 0, got %d\n", priority);
        return NULL;
    }

    sched_thread_t* t = calloc(1, sizeof(*t));
    if (!t) return NULL;
    fiber_create_thread(t);  // «эй, я создался — положи в user_data что надо»
    if (!t->fiber) {
        free(t);
        return NULL;
    }

    t->priority = priority;
    t->stride = STRIDE_BIG / priority;
    t->remain = t->stride;
    t->pass = sched.global_pass + t->remain;

    sched.global_tickets += priority;
    if (sched.global_tickets > 0) sched.global_stride = STRIDE_BIG / sched.global_tickets;

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
    while (sched.head != NULL) {
        sched_remove_thread(sched.head);
        handle_thread_remove(sched.head);
    }
}

// ============================================================================
//  Блокировка / разблокировка
// ============================================================================

void sched_block_thread(sched_thread_t* thread) {
    if (thread->state == THREAD_READY) {
        thread->state = THREAD_BLOCKED;
        sched.thread_blocked++;

        thread->remain = thread->pass - sched.global_pass;
        sched.global_tickets -= thread->priority;
        if (sched.global_tickets > 0) sched.global_stride = STRIDE_BIG / sched.global_tickets;
    }
}

void sched_unblock_thread(sched_thread_t* thread) {
    if (thread->state == THREAD_BLOCKED) {
        thread->state = THREAD_READY;
        sched.thread_blocked--;

        thread->pass = sched.global_pass + thread->remain;
        sched.global_tickets += thread->priority;
        if (sched.global_tickets > 0) sched.global_stride = STRIDE_BIG / sched.global_tickets;
    }
}