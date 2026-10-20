#include "../include/drrd.h"

#include "../include/stat.h"
#include "../include/trace.h"

sim_state_t sim;

// ============================================================================
//  share_t — сколько потоку осталось получить (delta) и уже зачтено (got)
// ============================================================================

typedef struct {
    long long delta;
    long long got;
} share_t;

static share_t compute_share(mock_group_t* g) {
    share_t s;
    if (g->count == 0) {
        if (sim.R == 0) {
            s.got = 0;
            s.delta = g->priority;
        } else if (sim.R >= sim.W) {
            s.got = g->priority;
            s.delta = 0;
        } else {
            s.delta = (long long)g->priority * (sim.W - sim.R) / sim.W;
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

static void insert_thread_in_group(mock_group_t* g, mock_thread_t* t) {
    if (g->current == NULL) {
        t->next = t;
        t->prev = t;
        g->current = t;
        return;
    }

    mock_thread_t* curr = g->current;
    mock_thread_t* next = curr->next;

    // curr <-> t <-> next
    curr->next = t;
    t->prev = curr;
    t->next = next;
    next->prev = t;
}

static void remove_thread_from_group(mock_group_t* g, mock_thread_t* t) {
    if (t->next == t) {
        g->current = NULL;
        return;
    }

    mock_thread_t* prev = t->prev;
    mock_thread_t* next = t->next;

    //    prev <-> next
    prev->next = next;
    next->prev = prev;

    g->current = t->next;
}

// ============================================================================
//  Работа с группами
// ============================================================================

static int find_group(int priority) {
    for (int i = 0; i < sim.group_count; i++)
        if (sim.groups[i].priority == priority) return i;

    return -1;
}

void update_deficit(void) {
    for (int i = 0; i < sim.group_count; i++)
        if (sim.groups[i].current != NULL) sim.groups[i].deficit += sim.groups[i].W;
}

static void recalculation_deficit(void) {
    sim.W_eff = 0;
    for (int i = 0; i < sim.group_count; i++) {
        mock_group_t* g = &sim.groups[i];
        if (g->current == NULL) continue;

        g->W = (g->priority * g->count) - g->fant_calls;
        sim.W_eff += g->W;
        g->deficit = g->W;
    }
}

void reset(void) {
    sim.W = sim.W_eff = sim.W_base;
    sim.R = 0;

    for (int i = 0; i < sim.group_count; i++) {
        mock_group_t* g = &sim.groups[i];
        if (g->current == NULL) continue;

        g->W = g->W_base;
        g->fant_calls = 0;
        g->deficit = 0;

        // блок только для тестирования
        mock_thread_t* t = g->current;
        mock_thread_t* end = g->current;
        do {
            t->got_debugging = 0;
            t = t->next;
        } while (t != end);
    }

    // блок статистики
    stat_reset();

    update_deficit();
}

static int sim_select_group(void) {
    int selected = -1;
    long long max_deficit = LLONG_MIN;
    for (int i = 0; i < sim.group_count; i++) {
        if (sim.groups[i].current == NULL) continue;
        if (selected == -1 || sim.groups[i].deficit > max_deficit) {
            max_deficit = sim.groups[i].deficit;
            selected = i;
        }
    }
    return selected;
}

mock_thread_t* sim_select_thread(int group_id) { return sim.groups[group_id].current; }

// ============================================================================
//  Обработчики состояний
// ============================================================================

static void handle_thread_blocked(mock_thread_t* t) {
    t->debt++;
    stat_thread_blocked(t);
}

static void handle_thread_ready(mock_thread_t* t) {
    stat_thread_ready(t);

    if (t->debt > 0) { t->debt--, stat_thread_repay(t); }
}

static void handle_thread_remove(mock_thread_t* t) {
    mock_group_t* g = &sim.groups[find_group(t->priority)];
    share_t s = compute_share(g);

    sim.W -= s.delta;
    g->W_base -= t->priority;
    sim.W_base -= t->priority;
    g->fant_calls -= s.got;

    stat_thread_remove(t, s.delta);
    g->count--;

    remove_thread_from_group(g, t);
    free(t);

    recalculation_deficit();
}

// ============================================================================
//  Диспетчер
// ============================================================================

typedef enum { STEP_CONTINUE, STEP_ABORT } step_result_t;

static step_result_t handle_thread_state(mock_thread_t* t) {
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

void sim_step(void) {
    if (sim.W == 0) return;

    mock_group_t* g = &sim.groups[sim_select_group()];

    if (handle_thread_state(g->current) == STEP_ABORT) return;

    // переменная только для тестирования
    g->current->got_debugging++;

    g->fant_calls++;
    g->deficit -= sim.W_eff;
    sim.R++;

    g->current = g->current->next;

    update_deficit();
}

// ============================================================================
//  Основной цикл
// ============================================================================

void sim_run(int iterations) {
    for (int i = 0; i < iterations; i++) {
        sim.cycle++;
        reset();
        while (sim.R < sim.W) sim_step();
    }
}

// ============================================================================
//  Инициализация
// ============================================================================

void sim_init(int group_count, const int priorities[]) {
    sim.group_count = group_count;
    sim.W = 0;
    sim.W_eff = 0;
    sim.W_base = 0;
    sim.R = 0;
    sim.cycle = 0;
    sim.thread_id = 0;

    for (int i = 0; i < group_count; i++) {
        mock_group_t* g = &sim.groups[i];
        g->priority = priorities[i];
        g->count = 0;
        g->W = 0;
        g->W_base = 0;
        g->deficit = 0;
        g->fant_calls = 0;
        g->current = NULL;
    }

    stat_init();
}

// ============================================================================
//  Создание / удаление
// ============================================================================

mock_thread_t* sim_create_thread(int priority) {
    mock_group_t* g = &sim.groups[find_group(priority)];
    mock_thread_t* t = calloc(1, sizeof(*t));
    share_t s = compute_share(g);

    t->id = sim.thread_id++;
    t->priority = priority;
    t->state = THREAD_READY;
    t->debt = 0;
    t->next = t;
    t->prev = t;
    t->stat = stat_create_thread(t, s.got);
    t->got_debugging = (g->current != NULL) ? g->current->got_debugging : s.got;

    sim.W += s.delta;
    g->W_base += priority;
    sim.W_base += priority;
    g->fant_calls += s.got;

    insert_thread_in_group(g, t);
    g->count += 1;

    recalculation_deficit();

    return t;
}

void sim_remove_thread(mock_thread_t* thread) { thread->state = THREAD_ZOMBIE; }

void sim_cleanup(void) {
    for (int i = 0; i < sim.group_count; i++) {
        while (sim.groups[i].current != NULL) {
            sim.groups[i].current->state = THREAD_ZOMBIE;
            handle_thread_remove(sim.groups[i].current);
        }
    }
}

// ============================================================================
//  Блокировка / разблокировка
// ============================================================================

void sim_block_thread(mock_thread_t* thread) {
    if (thread->state == THREAD_READY) { thread->state = THREAD_BLOCKED; }
}

void sim_unblock_thread(mock_thread_t* thread) {
    if (thread->state == THREAD_BLOCKED) { thread->state = THREAD_READY; }
}