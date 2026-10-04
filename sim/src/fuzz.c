#include "../include/fuzz.h"

#include <stdint.h>

#include "../include/drrd.h"
#include "../include/print.h"
#include "../include/stat.h"

// Группы, с которыми работает симуляция
// fuzz.c

static const int g_priorities[FUZZ_NUM_GROUPS] = {1, 2, 3, 4, 5, 9, 100, 110, 120, 130};
// static const int g_priorities[FUZZ_NUM_GROUPS] = {1, 2, 3, 4, 5, 9, 10, 11, 12, 13};
// static const int g_priorities[FUZZ_NUM_GROUPS] = {1, 2, 3, 4, 5};

// Глобальный sim из drrd.c
extern sim_state_t sim;

// ============================================================================
//  PRNG — xorshift64
// ============================================================================

static uint64_t rng_state;

static uint64_t rng_next(void) {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 7;
    rng_state ^= rng_state << 17;
    return rng_state;
}

static uint32_t rng_range(uint32_t n) {
    if (n == 0) return 0;
    return (uint32_t)(((__uint128_t)rng_next() * n) >> 64);
}

// ============================================================================
//  Пулы живых потоков
// ============================================================================

#define POOL_RUNNING 0
#define POOL_BLOCKED 1
#define POOL_NONE    (-1)

typedef struct {
    int pool;
    int pool_idx;
} fuzz_meta_t;

static mock_thread_t* running[FUZZ_MAX_ALIVE];
static mock_thread_t* blocked[FUZZ_MAX_ALIVE];
static int n_running = 0;
static int n_blocked = 0;

static fuzz_meta_t* meta_of(mock_thread_t* t) { return (fuzz_meta_t*)t->user_data; }

static void pool_add(int pool, mock_thread_t* t) {
    mock_thread_t** arr = (pool == POOL_RUNNING) ? running : blocked;
    int* n = (pool == POOL_RUNNING) ? &n_running : &n_blocked;
    fuzz_meta_t* m = meta_of(t);

    arr[*n] = t;
    m->pool = pool;
    m->pool_idx = *n;
    (*n)++;
}

static void pool_remove(mock_thread_t* t) {
    fuzz_meta_t* m = meta_of(t);
    if (m->pool == POOL_NONE) return;

    mock_thread_t** arr = (m->pool == POOL_RUNNING) ? running : blocked;
    int* n = (m->pool == POOL_RUNNING) ? &n_running : &n_blocked;

    int i = m->pool_idx;
    int last = *n - 1;
    if (i != last) {
        arr[i] = arr[last];
        meta_of(arr[i])->pool_idx = i;
    }
    (*n)--;

    m->pool = POOL_NONE;
    m->pool_idx = -1;
}

// ============================================================================
//  Типы событий
// ============================================================================

enum { EV_CREATE, EV_REMOVE, EV_BLOCK, EV_UNBLOCK, EV_NONE };

// ============================================================================
//  Счётчики для дебага
// ============================================================================

static int ev_created = 0;
static int ev_removed = 0;
static int ev_blocked = 0;
static int ev_unblocked = 0;
static int ev_skipped = 0;
static int max_alive_seen = 0;

// ============================================================================
//  Проверки «можно ли выполнить событие»
// ============================================================================

static int can_create(void) { return (n_running + n_blocked) < FUZZ_MAX_ALIVE; }
static int can_remove(void) { return (n_running + n_blocked) > FUZZ_MIN_ALIVE; }

// ============================================================================
//  Действия
// ============================================================================

static void do_create(void) {
    int gi = rng_range(FUZZ_NUM_GROUPS);
    mock_thread_t* t = sim_create_thread(g_priorities[gi]);
    if (!t) return;

    fuzz_meta_t* m = malloc(sizeof(*m));
    m->pool = POOL_NONE;
    m->pool_idx = -1;
    t->user_data = m;

    pool_add(POOL_RUNNING, t);
    ev_created++;

    int alive = n_running + n_blocked;
    if (alive > max_alive_seen) max_alive_seen = alive;
}

static void do_remove(void) {
    int total = n_running + n_blocked;
    if (total == 0) return;

    int pick = rng_range(total);
    mock_thread_t* t;
    if (pick < n_running) t = running[pick];
    else t = blocked[pick - n_running];

    pool_remove(t);        // откуда бы он ни был — по t->pool
    sim_remove_thread(t);  // state = ZOMBIE, дальше разберётся sim_step
    ev_removed++;
}

// ============================================================================
//  Диспетчер с каскадом одной замены
// ============================================================================

static int try_event(int ev) {
    switch (ev) {
        case EV_CREATE:
            if (!can_create()) return 0;
            do_create();
            return 1;
        case EV_REMOVE:
            if (!can_remove()) return 0;
            do_remove();
            return 1;
    }
    return 0;
}

static int pair_of(int ev) {
    switch (ev) {
        case EV_CREATE: return EV_REMOVE;
        case EV_REMOVE: return EV_CREATE;
    }
    return EV_NONE;
}

static int pick_event_type(void) {
    uint32_t r = rng_range(100);
    if (r < FUZZ_PROB_CREATE) return EV_CREATE;
    r -= FUZZ_PROB_CREATE;
    if (r < FUZZ_PROB_REMOVE) return EV_REMOVE;
    r -= FUZZ_PROB_REMOVE;
    return EV_UNBLOCK;
}

static void maybe_event(void) {
    if (rng_range(100) >= FUZZ_EVENT_PROB) return;

    int ev = pick_event_type();

    // Каскад: primary → pair → create → skip
    if (try_event(ev)) return;
    if (try_event(pair_of(ev))) return;
    if (try_event(EV_CREATE)) return;

    ev_skipped++;
}

// ============================================================================
//  Драйвер
// ============================================================================

void fuzz_run(unsigned seed, int cycles) {
    rng_state = seed ? seed : 1;

    printf("seed:      %u\n", seed);
    printf("cycles:    %d\n", cycles);
    printf("max_alive: %d\n", FUZZ_MAX_ALIVE);
    printf("event_p:   %d%%\n\n", FUZZ_EVENT_PROB);

    n_running = 0;
    n_blocked = 0;
    ev_created = ev_removed = ev_blocked = ev_unblocked = ev_skipped = 0;
    max_alive_seen = 0;

    sim_init(FUZZ_NUM_GROUPS, g_priorities);

    // Стартовый набор потоков
    for (int i = 0; i < FUZZ_INITIAL_THREADS; i++) do_create();

    for (int c = 0; c < cycles; c++) {
        sim.cycle++;
        reset();
        update_deficit();

        maybe_event();  // до первого sim_step в цикле

        while (sim.R < sim.W) {
            sim_step();
            maybe_event();  // после каждого sim_step
        }

        maybe_event();  // после закрытия цикла, до следующего reset
    }

    sim_cleanup();

    print_header();
    print_thread_stats();
    print_group_stats();
    print_diff_histogram();

    printf("\n=== Fuzz counters ===\n");
    printf("created:   %d\n", ev_created);
    printf("removed:   %d\n", ev_removed);
    printf("blocked:   %d\n", ev_blocked);
    printf("unblocked: %d\n", ev_unblocked);
    printf("skipped:   %d\n", ev_skipped);
    printf("max alive: %d\n", max_alive_seen);

    stat_cleanup();
}