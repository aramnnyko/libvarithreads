#include "../include/lottery.h"

#include <stdlib.h>

#include "../../fiber/include/fiber.h"

sched_state_t sched;

static uint64_t rng_state = 0x9E3779B97F4A7C15ULL;
static inline uint64_t xorshift64(void) {
    uint64_t x = rng_state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    return rng_state = x;
}

// ============================================================================
//  Круговой двусвязный список потоков внутри группы
// ============================================================================

static void insert_thread(sched_thread_t* t) { bit_add(&sched.tree, t->slot + 1, t->ticket); }

static void remove_thread(sched_thread_t* t) { bit_add(&sched.tree, t->slot + 1, -t->ticket); }

// ============================================================================
//  Обработчики состояний
// ============================================================================

static void handle_thread_blocked(sched_thread_t* /* t */) { return; /* пропустить  */ }

static void handle_thread_ready(sched_thread_t* t) {
    fiber_run_thread(t);  // «эй, я выбрал вот этот — делай что хочешь»
}

static void handle_thread_remove(sched_thread_t* t) {
    sched.thread_live--;

    remove_thread(t);
    sched.total_tickets -= t->ticket;
    sched.threads[t->slot] = NULL;
    fiber_remove_thread(t);  // «эй, я удаляюсь — забери user_data»
    free(t);
}

// ============================================================================
//  Диспетчер
// ============================================================================

static void handle_thread_state(sched_thread_t* t) {
    switch (t->state) {
        case THREAD_BLOCKED: handle_thread_blocked(t); return;
        case THREAD_READY  : handle_thread_ready(t); return;
        case THREAD_ZOMBIE : handle_thread_remove(t); return;
        default            : return;
    }
}

// ============================================================================
//  Шаг
// ============================================================================

void sched_step(void) {
    if (sched.total_tickets <= 0) return;

    uint64_t r = (uint64_t)(((__uint128_t)xorshift64() * sched.total_tickets) >> 64);
    int slot = bit_find(&sched.tree, (int)r);
    sched_thread_t* t = sched.threads[slot - 1];
    if (!t) return;

    handle_thread_state(t);
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
    memset(sched.threads, 0, sizeof(sched.threads));
    bit_init(&sched.tree, sched.bit_array, LOTTERY_MAX_THREADS);

    sched.total_tickets = 0;
    sched.thread_live = 0;
    sched.thread_blocked = 0;
}

// ============================================================================
//  Создание / удаление
// ============================================================================

sched_thread_t* sched_create_thread(int ticket) {
    if (ticket <= 0) {
        fprintf(stderr, "sched_create_thread: ticket must be > 0, got %d\n", ticket);
        return NULL;
    }

    int slot = -1;
    for (int i = 0; i < LOTTERY_MAX_THREADS; i++) {
        if (sched.threads[i] == NULL) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        fprintf(stderr, "sched_create_thread: no free slot\n");
        return NULL;
    }

    sched_thread_t* t = calloc(1, sizeof(*t));
    if (!t) return NULL;

    fiber_create_thread(t);  // «эй, я создался — положи в user_data что надо»
    if (!t->fiber) {
        free(t);
        return NULL;
    }

    t->ticket = ticket;
    t->slot = slot;
    t->state = THREAD_READY;

    sched.threads[slot] = t;
    insert_thread(t);
    sched.total_tickets += ticket;

    sched.thread_live++;

    return t;
}

void sched_remove_thread(sched_thread_t* thread) {
    sched_unblock_thread(thread);
    thread->state = THREAD_ZOMBIE;
}

void sched_cleanup(void) {
    for (int i = 0; i < LOTTERY_MAX_THREADS; i++) {
        if (!sched.threads[i]) continue;
        sched_remove_thread(sched.threads[i]);
        handle_thread_remove(sched.threads[i]);
    }
}

// ============================================================================
//  Блокировка / разблокировка
// ============================================================================

void sched_block_thread(sched_thread_t* thread) {
    if (thread->state == THREAD_READY) {
        thread->state = THREAD_BLOCKED;
        remove_thread(thread);
        sched.total_tickets -= thread->ticket;
        sched.thread_blocked++;
    }
}

void sched_unblock_thread(sched_thread_t* thread) {
    if (thread->state == THREAD_BLOCKED) {
        thread->state = THREAD_READY;
        insert_thread(thread);
        sched.total_tickets += thread->ticket;
        sched.thread_blocked--;
    }
}