#include "../include/fiber.h"

#define FIBER_STACK_SIZE (256 * 1024)

static ucontext_t sched_ctx;
static fiber_thread_t* current = NULL;
static fiber_thread_t* pending_dead = NULL;

static struct slot {
    uint32_t generation;
    fiber_thread_t* fiber;
} slots[VT_MAX_THREADS];

static uint32_t free_stack[VT_MAX_THREADS];
static uint32_t free_top = 0;
static uint32_t global_generation = 0;

/* ── слоты ─────────────────────────────────── */

static inline int pop_slot(void) { return free_top ? (int)free_stack[--free_top] : -1; }

static inline void push_slot(uint32_t i) { free_stack[free_top++] = i; }

static fiber_thread_t* get(thread_t h) {
    if (h.index >= VT_MAX_THREADS) return NULL;
    if (slots[h.index].generation != h.generation) return NULL;
    return slots[h.index].fiber;
}

static inline thread_t make_handle(uint32_t idx) { return (thread_t){.index = idx, .generation = slots[idx].generation}; }

static inline thread_t invalid_handle(void) { return (thread_t){.index = UINT32_MAX, .generation = 0}; }

/* ── трамплин ──────────────────────────────── */

static inline void trampoline(void) {
    current->fn(current->arg);
    fiber_exit();
}

/* ── отложенное освобождение ───────────────── */

static inline void free_pending_dead(void) {
    if (pending_dead) {
        slots[pending_dead->slot_index].fiber = NULL;
        push_slot(pending_dead->slot_index);
        free(pending_dead->ctx.uc_stack.ss_sp);
        free(pending_dead);
        pending_dead = NULL;
    }
}

/* ── init / cleanup ────────────────────────── */

void fiber_init(void) {
    current = NULL;
    pending_dead = NULL;
    global_generation = 0;
    free_top = 0;

    for (uint32_t i = 0; i < VT_MAX_THREADS; i++) {
        slots[i].fiber = NULL;
        slots[i].generation = 0;
        free_stack[free_top++] = i;
    }
}

void fiber_cleanup(void) {
    free_pending_dead();
    current = NULL;
}

/* ── хуки для планировщика ─────────────────── */

void fiber_create_thread(struct sched_thread* sched) {
    if (!sched) return;

    int i = pop_slot();
    if (i < 0) {
        sched->fiber = NULL;
        return;
    }

    fiber_thread_t* t = calloc(1, sizeof(*t));
    if (!t) {
        push_slot((uint32_t)i);
        sched->fiber = NULL;
        return;
    }

    t->slot_index = (uint32_t)i;
    t->sched = sched;

    slots[i].generation = ++global_generation;
    slots[i].fiber = t;

    sched->fiber = t;
}

void fiber_remove_thread(struct sched_thread* sched) {
    if (!sched) return;
    fiber_thread_t* t = sched->fiber;
    if (!t) {
        sched->fiber = NULL;
        return;
    }

    t->finished = 1;

    fiber_thread_t* w = t->join_head;
    while (w) {
        fiber_thread_t* next = w->join_next;
        w->join_next = NULL;
        sched_unblock_thread(w->sched);
        w = next;
    }
    t->join_head = NULL;

    sched->fiber = NULL;

    free_pending_dead();
    pending_dead = t;
}

void fiber_run_thread(struct sched_thread* sched) {
    if (!sched || !sched->fiber) return;

    current = sched->fiber;
    swapcontext(&sched_ctx, &current->ctx);
    current = NULL;
}

/* ── API ───────────────────────────────────── */

thread_t fiber_create(thread_fn_t fn, void* arg, struct sched_thread* sched) {
    if (!sched) return invalid_handle();
    if (!sched->fiber) {
        sched_remove_thread(sched);
        return invalid_handle();
    }

    fiber_thread_t* t = sched->fiber;

    t->fn = fn;
    t->arg = arg;

    void* stack = malloc(FIBER_STACK_SIZE);
    if (!stack) {
        sched_remove_thread(sched);
        return invalid_handle();
    }

    getcontext(&t->ctx);
    t->ctx.uc_stack.ss_sp = stack;
    t->ctx.uc_stack.ss_size = FIBER_STACK_SIZE;
    t->ctx.uc_link = NULL;
    makecontext(&t->ctx, trampoline, 0);

    return make_handle(t->slot_index);
}

void fiber_yield(void) {
    if (!current) return;
    swapcontext(&current->ctx, &sched_ctx);
}

void fiber_block(void) {
    if (!current) return;
    sched_block_thread(current->sched);
    swapcontext(&current->ctx, &sched_ctx);
}

void fiber_unblock(thread_t h) {
    fiber_thread_t* t = get(h);
    if (!t || t->finished) return;
    sched_unblock_thread(t->sched);
}

void fiber_join(thread_t target_h) {
    if (!current) return;
    fiber_thread_t* target = get(target_h);
    if (!target || target == current || target->finished) return;

    current->join_next = target->join_head;
    target->join_head = current;

    fiber_block();
}

void fiber_exit(void) {
    if (!current) abort();

    sched_remove_thread(current->sched);
    setcontext(&sched_ctx);
    __builtin_unreachable();
}

thread_t fiber_self(void) {
    if (!current) return invalid_handle();
    return make_handle(current->slot_index);
}