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

share_t compute_share(mock_group_t* g, const sim_state_t* sim) {
    share_t s;
    if (g->count == 0) {
        if (sim->R == 0) {
            s.got = 0;
            s.delta = g->priority;
        } else if (sim->R >= sim->W) {
            s.got = g->priority;
            s.delta = 0;
        } else {
            s.delta = (long long)g->priority * (sim->W - sim->R) / sim->W;
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
    if (g->head == NULL) {
        g->head = t;
        t->next = t;
        t->prev = t;
        g->current = t;
        return;
    }

    mock_thread_t* curr = g->current;
    mock_thread_t* next = curr->next;

    curr->next = t;
    t->prev = curr;
    t->next = next;
    next->prev = t;
}

static void remove_thread_from_group(mock_group_t* g, mock_thread_t* t) {
    assert(g->head != NULL);
    assert(t != NULL);

    if (g->head == t && t->next == t) {
        g->head = NULL;
        g->current = NULL;
        return;
    }

    if (g->head == t) g->head = t->next;

    t->prev->next = t->next;
    t->next->prev = t->prev;

    if (g->current == t) g->current = t->next;
}

// ============================================================================
//  Проверки инвариантов
// ============================================================================

static void check_group(const mock_group_t* g) {
    if (g->count == 0) {
        assert(g->head == NULL);
        assert(g->current == NULL);
        assert(g->W == 0);
        assert(g->W_base == 0);
        assert(g->deficit == 0);
        assert(g->calls == 0);
        assert(g->fant_calls == 0);
        return;
    }

    assert(g->count > 0);
    assert(g->priority > 0);
    assert(g->head != NULL);
    assert(g->current != NULL);
    assert(g->W >= 0);
    assert(g->W_base >= 0);
    assert(g->calls >= 0);
    assert(g->fant_calls >= 0);
    assert(g->W_base == (long long)g->priority * g->count);

    int n = 0;
    const mock_thread_t* t = g->head;
    do {
        assert(t != NULL);
        assert(t->priority == g->priority);
        n++;
        assert(n <= 1000000 && "circular list broken / too long");
        t = t->next;
    } while (t != g->head);

    assert(n == g->count);
}

static void check_global(void) {
    long long w_sum = 0;
    long long wb_sum = 0;
    long long c_sum = 0;

    for (int i = 0; i < sim.group_count; i++) {
        const mock_group_t* g = &sim.groups[i];
        check_group(g);
        w_sum += g->W;
        wb_sum += g->W_base;
        c_sum += g->calls;
    }

    assert(sim.W == w_sum);
    assert(sim.W_base == wb_sum);
    assert(sim.R >= c_sum);
    assert(sim.W >= 0);
    assert(sim.W_base >= 0);
    assert(sim.R >= 0);
    assert(sim.thread_id >= 0);
}

static void check_invariants(void) { check_global(); }

// ============================================================================
//  Работа с группами
// ============================================================================

int find_group(int priority) {
    for (int i = 0; i < sim.group_count; i++)
        if (sim.groups[i].priority == priority) return i;
    return -1;
}

void recalculation_deficit(void) {
    for (int i = 0; i < sim.group_count; i++) {
        mock_group_t* g = &sim.groups[i];
        if (g->head != NULL) g->deficit = g->W * sim.R - sim.W * g->calls;
    }
}

void update_deficit(void) {
    for (int i = 0; i < sim.group_count; i++) {
        mock_group_t* g = &sim.groups[i];
        if (g->head != NULL) g->deficit += g->W;
    }
}

void reset(void) {
    sim.W = sim.W_base;
    sim.R = 0;
    for (int i = 0; i < sim.group_count; i++) {
        mock_group_t* g = &sim.groups[i];
        g->W = g->W_base;
        g->calls = 0;
        g->fant_calls = 0;
        g->deficit = 0;
    }

    stat_reset(sim.cycle);
    check_invariants();
}

int sim_select_group(void) {
    int selected = -1;
    long long max_deficit = LLONG_MIN;
    for (int i = 0; i < sim.group_count; i++) {
        if (sim.groups[i].head == NULL) continue;
        if (selected == -1 || sim.groups[i].deficit > max_deficit) {
            max_deficit = sim.groups[i].deficit;
            selected = i;
        }
    }
    return selected;
}

mock_thread_t* sim_select_thread(int group_id) {
    mock_group_t* g = &sim.groups[group_id];
    if (!g->head) return NULL;
    return g->current;
}

// ============================================================================
//  Обработчики состояний
// ============================================================================

static void handle_thread_ready(mock_thread_t* t) {
    assert(t != NULL);
    assert(t->state == THREAD_READY);
    stat_thread_ready(t->stat, sim.cycle);

    mock_group_t* g = &sim.groups[find_group(t->priority)];
    trace_log(sim.cycle, sim.R, TR_RUN, t->id, t->priority, g->count, g->fant_calls, g->W, g->W_base, 0, 0, sim.W, sim.W_base);
}

static void handle_thread_remove(mock_thread_t* t) {
    assert(t != NULL);
    assert(t->state == THREAD_ZOMBIE);

    mock_group_t* g = &sim.groups[find_group(t->priority)];
    assert(g != NULL);
    assert(g->count > 0);

    share_t s = compute_share(g, &sim);
    assert(s.delta >= 0);
    assert(s.got >= 0);

    int id = t->id;
    int priority = t->priority;

    trace_log(sim.cycle, sim.R, TR_REMOVE_BEFORE, id, priority, g->count, g->fant_calls, g->W, g->W_base, s.got, s.delta, sim.W, sim.W_base);

    g->W -= s.delta;
    sim.W -= s.delta;
    g->W_base -= t->priority;
    sim.W_base -= t->priority;
    g->fant_calls -= s.got;

    remove_thread_from_group(g, t);
    stat_thread_remove(t, t->stat, sim.cycle, s.delta);

    free(t);

    g->count -= 1;
    assert(g->count >= 0);

    if (g->head == NULL) {
        sim.W -= g->W;
        g->W = 0;
        g->W_base = 0;
        g->deficit = 0;
        g->calls = 0;
        g->fant_calls = 0;

        assert(g->W == 0);
        assert(g->W_base == 0);
        assert(g->deficit == 0);
        assert(g->calls == 0);
        assert(g->fant_calls == 0);
        assert(g->count == 0);
        assert(g->head == NULL);
        assert(g->current == NULL);
    }

    recalculation_deficit();

    trace_log(sim.cycle, sim.R, TR_REMOVE_AFTER, id, priority, g->count, g->fant_calls, g->W, g->W_base, s.got, s.delta, sim.W, sim.W_base);

    check_invariants();
}

// ============================================================================
//  Диспетчер
// ============================================================================

typedef enum { STEP_CONTINUE, STEP_ABORT } step_result_t;

static step_result_t handle_thread_state(mock_thread_t* t) {
    assert(t != NULL);
    switch (t->state) {
        case THREAD_READY : handle_thread_ready(t); return STEP_CONTINUE;
        case THREAD_ZOMBIE: handle_thread_remove(t); return STEP_ABORT;
        default           : fprintf(stderr, "WARNING: unexpected thread state %d\n", t->state); return STEP_CONTINUE;
    }
}

// ============================================================================
//  Шаг
// ============================================================================

void sim_step(void) {
    check_invariants();
    if (sim.W == 0) return;

    int gid = sim_select_group();
    if (gid < 0) return;
    mock_group_t* g = &sim.groups[gid];

    mock_thread_t* thread = sim_select_thread(gid);
    assert(thread != NULL);
    assert(thread == g->current);

    if (handle_thread_state(thread) == STEP_ABORT) return;

    g->current = thread->next;
    assert(g->current != NULL);

    g->calls++;
    g->fant_calls++;
    g->deficit -= sim.W;
    sim.R++;

    update_deficit();
    check_invariants();
}

// ============================================================================
//  Основной цикл
// ============================================================================

void sim_run(int iterations) {
    for (int i = 0; i < iterations; i++) {
        sim.cycle++;
        reset();
        update_deficit();
        while (sim.R < sim.W) sim_step();
    }
}

// ============================================================================
//  Инициализация
// ============================================================================

void sim_init(int group_count, const int priorities[]) {
    assert(group_count > 0 && group_count <= MAX_GROUPS);

    sim.group_count = group_count;
    sim.W = 0;
    sim.W_base = 0;
    sim.R = 0;
    sim.cycle = 0;
    sim.thread_id = 0;

    for (int i = 0; i < group_count; i++) {
        assert(priorities[i] > 0);
        mock_group_t* g = &sim.groups[i];
        g->priority = priorities[i];
        g->count = 0;
        g->W = 0;
        g->W_base = 0;
        g->deficit = 0;
        g->calls = 0;
        g->fant_calls = 0;
        g->head = NULL;
        g->current = NULL;
    }

    stat_init();
    check_invariants();
}

// ============================================================================
//  Создание / удаление
// ============================================================================

mock_thread_t* sim_create_thread(int priority) {
    assert(priority > 0);

    int group_id = find_group(priority);
    assert(group_id != -1 && "group for this priority must exist");
    mock_group_t* g = &sim.groups[group_id];

    mock_thread_t* t = calloc(1, sizeof(*t));
    assert(t != NULL);
    if (!t) return NULL;

    share_t s = compute_share(g, &sim);
    assert(s.delta >= 0);
    assert(s.got >= 0);

    t->id = sim.thread_id++;
    t->priority = priority;
    t->state = THREAD_READY;
    t->next = t;
    t->prev = t;
    t->stat = stat_create_thread(t, sim.cycle, s.got);
    assert(t->stat != NULL);

    trace_log(sim.cycle, sim.R, TR_CREATE_BEFORE, t->id, priority, g->count, g->fant_calls, g->W, g->W_base, s.got, s.delta, sim.W, sim.W_base);

    insert_thread_in_group(g, t);

    g->W += s.delta;
    sim.W += s.delta;
    g->W_base += priority;
    sim.W_base += priority;
    g->fant_calls += s.got;
    g->count += 1;

    recalculation_deficit();
    check_invariants();

    trace_log(sim.cycle, sim.R, TR_CREATE_AFTER, t->id, priority, g->count, g->fant_calls, g->W, g->W_base, s.got, s.delta, sim.W, sim.W_base);

    return t;
}

void sim_remove_thread(mock_thread_t* thread) {
    assert(thread != NULL);
    thread->state = THREAD_ZOMBIE;
}

void sim_cleanup(void) {
    for (int i = 0; i < sim.group_count; i++) {
        mock_group_t* g = &sim.groups[i];
        while (g->head != NULL) {
            mock_thread_t* t = g->current;
            assert(t != NULL);
            t->state = THREAD_ZOMBIE;
            handle_thread_remove(t);
        }
    }
    check_invariants();
}