#include "../include/drrd.h"

sched_state_t sched;

// ============================================================================
//  share_t — сколько потоку осталось получить (delta) и уже зачтено (got)
// ============================================================================

typedef struct {
    long long delta;
    long long got;
} share_t;

static share_t compute_share(sched_group_t* g) {
    share_t s;
    if (g->count == 0) {
        if (sched.R == 0) {
            s.got = 0;
            s.delta = g->priority;
        } else if (sched.R >= sched.W) {
            s.got = g->priority;
            s.delta = 0;
        } else {
            s.delta = (long long)g->priority * (sched.W - sched.R) / sched.W;
            s.got = g->priority - s.delta;
        }
    } else {
        s.got = g->fant_calls / g->count;
        s.delta = (g->priority > s.got) ? (g->priority - s.got) : 0;
    }
    return s;
}

// ============================================================================
//  Круговой двусвязный список потоков внутри группы
// ============================================================================

static void insert_thread_in_group(sched_group_t* g, sched_thread_t* t) {
    if (g->current == NULL) {
        t->next = t;
        t->prev = t;
        g->current = t;
        return;
    }

    sched_thread_t* curr = g->current;
    sched_thread_t* next = curr->next;

    // curr <-> t <-> next
    curr->next = t;
    t->prev = curr;
    t->next = next;
    next->prev = t;
}

static void remove_thread_from_group(sched_group_t* g, sched_thread_t* t) {
    if (t->next == t) {
        g->current = NULL;
        return;
    }

    sched_thread_t* prev = t->prev;
    sched_thread_t* next = t->next;

    //    prev <-> next
    prev->next = next;
    next->prev = prev;

    g->current = t->next;
}

// ============================================================================
//  Работа с группами
// ============================================================================

static int find_group(int priority) {
    for (int i = 0; i < sched.group_count; i++)
        if (sched.groups[i].priority == priority) return i;

    return -1;
}

void update_deficit(void) {
    for (int i = 0; i < sched.group_count; i++)
        if (sched.groups[i].current != NULL) sched.groups[i].deficit += sched.groups[i].W;
}

static void recalculation_deficit(void) {
    sched.W_eff = 0;
    for (int i = 0; i < sched.group_count; i++) {
        sched_group_t* g = &sched.groups[i];
        if (g->current == NULL) continue;

        g->W = (g->priority * g->count) - g->fant_calls;
        sched.W_eff += g->W;
        g->deficit = g->W;
    }
}

void reset(void) {
    sched.W = sched.W_eff = sched.W_base;
    sched.R = 0;

    for (int i = 0; i < sched.group_count; i++) {
        sched_group_t* g = &sched.groups[i];
        if (g->current == NULL) continue;

        g->W = g->W_base;
        g->fant_calls = 0;
        g->deficit = 0;
    }

    update_deficit();
}

static int sched_select_group(void) {
    int selected = -1;
    long long max_deficit = LLONG_MIN;
    for (int i = 0; i < sched.group_count; i++) {
        if (sched.groups[i].current == NULL) continue;
        if (selected == -1 || sched.groups[i].deficit > max_deficit) {
            max_deficit = sched.groups[i].deficit;
            selected = i;
        }
    }
    return selected;
}

sched_thread_t* sched_select_thread(int group_id) { return sched.groups[group_id].current; }

// ============================================================================
//  Обработчики состояний
// ============================================================================

static void handle_thread_blocked(sched_thread_t* t) { t->debt++; }

static void handle_thread_ready(sched_thread_t* t) {
    fiber_run_thread(t);  // «эй, я выбрал вот этот — делай что хочешь»

    if (t->debt > 0) {
        fiber_run_thread(t);  // «эй, я выбрал вот этот — делай что хочешь»
        t->debt--;
    }
}

static void handle_thread_remove(sched_thread_t* t) {
    sched_group_t* g = &sched.groups[find_group(t->priority)];
    share_t s = compute_share(g);

    sched.W -= s.delta;
    g->W_base -= t->priority;
    sched.W_base -= t->priority;
    g->fant_calls -= s.got;

    g->count--;

    if (t->state == THREAD_BLOCKED) sched.thread_blocked--;
    sched.thread_live--;

    remove_thread_from_group(g, t);
    fiber_remove_thread(t);  // «эй, я удаляюсь — забери user_data»
    free(t);

    recalculation_deficit();
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
    if (sched.W == 0) return;

    sched_group_t* g = &sched.groups[sched_select_group()];

    if (handle_thread_state(g->current) == STEP_ABORT) return;

    g->fant_calls++;
    g->deficit -= sched.W_eff;
    sched.R++;

    g->current = g->current->next;

    update_deficit();
}

// ============================================================================
//  Основной цикл
// ============================================================================

void sched_run(void) {
    while (sched.thread_live > 0 && sched.thread_live > sched.thread_blocked) {
        reset();
        while (sched.R < sched.W) sched_step();
    }

    if (sched.thread_live > 0) {  // Дедлок
        sched_cleanup();
        fprintf(stderr, "sched_run: deadlock (%d live, %d blocked)\n", sched.thread_live, sched.thread_blocked);
    }
}

// ============================================================================
//  Инициализация
// ============================================================================

void sched_init(int group_count, const int priorities[]) {
    sched.group_count = group_count;
    sched.W = 0;
    sched.W_eff = 0;
    sched.W_base = 0;
    sched.R = 0;
    sched.thread_id = 0;
    sched.thread_live = 0;
    sched.thread_blocked = 0;

    for (int i = 0; i < group_count; i++) {
        sched_group_t* g = &sched.groups[i];
        g->priority = priorities[i];
        g->count = 0;
        g->W = 0;
        g->W_base = 0;
        g->deficit = 0;
        g->fant_calls = 0;
        g->current = NULL;
    }
}

// ============================================================================
//  Создание / удаление
// ============================================================================

sched_thread_t* sched_create_thread(int priority) {
    sched_group_t* g = &sched.groups[find_group(priority)];
    sched_thread_t* t = calloc(1, sizeof(*t));
    fiber_create_thread(t);  // «эй, я создался — положи в user_data что надо»

    share_t s = compute_share(g);

    t->id = sched.thread_id++;
    t->priority = priority;
    t->state = THREAD_READY;
    t->debt = 0;
    t->next = t;
    t->prev = t;

    sched.W += s.delta;
    g->W_base += priority;
    sched.W_base += priority;
    g->fant_calls += s.got;

    insert_thread_in_group(g, t);
    g->count += 1;
    sched.thread_live++;

    recalculation_deficit();

    return t;
}

void sched_remove_thread(sched_thread_t* thread) { thread->state = THREAD_ZOMBIE; }

void sched_cleanup(void) {
    for (int i = 0; i < sched.group_count; i++) {
        while (sched.groups[i].current != NULL) {
            sched.groups[i].current->state = THREAD_ZOMBIE;
            handle_thread_remove(sched.groups[i].current);
        }
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