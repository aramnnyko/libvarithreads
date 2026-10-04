#include "../include/drrd.h"

#include "../include/stat.h"
#include "../include/trace.h"

// ============================================================================
//  Глобальное состояние симулятора
// ============================================================================

sim_state_t sim;

// ============================================================================
//  share_t —  delta / got результат вычисления доли потока
// ============================================================================

typedef struct {
    long long delta;  // сколько потоку осталось получить» в этом цикле
    long long got;    // сколько поток «уже получил» в этом цикле
} share_t;

// Формулы для delta и got.
// count == 0 — группа пуста, экстраполируем по позиции в цикле.
// count > 0  — берём виртуальное среднее fant_calls / count.
share_t compute_share(mock_group_t* g, const sim_state_t* sim) {
    share_t s;
    if (g->count == 0) {
        if (sim->R == 0) {
            // либо начало симуляции, либо только что после reset
            // поток стартует с нуля — ничего не пропустил
            s.got = 0;
            s.delta = g->priority;
        } else if (sim->R >= sim->W) {
            // цикл уже закрыт — поток в текущем цикле не побегает
            // весь цикл пропущен
            s.got = g->priority;
            s.delta = 0;
        } else {
            // середина цикла — экстраполируем
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

// Вставка нового потока сразу после current.
static void insert_thread_in_group(mock_group_t* g, mock_thread_t* t) {
    if (g->head == NULL) {
        // Первый поток — образуем круговой список из одного узла
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

// Вышивание потока из кругового списка.
static void remove_thread_from_group(mock_group_t* g, mock_thread_t* t) {
    assert(g->head != NULL);
    assert(t != NULL);

    // Единственный поток в группе
    if (g->head == t && t->next == t) {
        g->head = NULL;
        g->current = NULL;
        return;
    }

    // Сдвигаем head, если удаляем голову
    if (g->head == t) g->head = t->next;

    // Вышиваем t из кругового списка
    t->prev->next = t->next;
    t->next->prev = t->prev;

    // Сдвигаем current, если он указывал на удаляемый поток
    if (g->current == t) g->current = t->next;
}

// ============================================================================
//  Проверки инвариантов
// ============================================================================

// Группа: если пуста — всё в нулях; если непуста — W_base = priority×count,
// W ≥ 0, круговой список содержит ровно count узлов с правильным приоритетом.
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

// Глобальные инварианты: суммы по группам сходятся с глобальным состоянием.
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

// Полный пересчёт дефицитов по формуле deficit = W·R − W_total·calls.
// Вызывается после каждой динамической операции (add/remove).
void recalculation_deficit(void) {
    for (int i = 0; i < sim.group_count; i++) {
        mock_group_t* g = &sim.groups[i];
        if (g->head != NULL) g->deficit = g->W * sim.R - sim.W * g->calls;
    }
}

// Инкремент дефицитов на текущий вес группы. Вызывается в конце sim_step.
void update_deficit(void) {
    for (int i = 0; i < sim.group_count; i++) {
        mock_group_t* g = &sim.groups[i];
        if (g->head != NULL) g->deficit += g->W;
    }
}

// Сброс в начале каждого цикла: W и calls обнуляются,
// W_base, count и fant_calls сохраняются — они описывают состав группы.
void reset(void) {
    sim.W = sim.W_base;
    sim.R = 0;
    for (int i = 0; i < sim.group_count; i++) {
        mock_group_t* g = &sim.groups[i];
        g->W = g->W_base;
        g->calls = 0;
        g->fant_calls = 0;
        g->deficit = 0;

        // сброс per-cycle счётчика у всех живых потоков группы
        if (g->head != NULL) {
            mock_thread_t* t = g->head;
            do {
                t->got_thread = 0;
                t = t->next;
            } while (t != g->head);
        }
    }

    stat_reset(sim.cycle);

    check_invariants();
}

// Группа с наибольшим дефицитом. Дефициты могут быть отрицательными,
// поэтому начальное значение — LLONG_MIN, а не -1.
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

static void handle_thread_blocked(mock_thread_t* t) {
    assert(t != NULL);
    assert(t->state == THREAD_BLOCKED);
    t->debt++;
    stat_thread_blocked(t, t->stat);
    assert(t->debt >= 0);

    mock_group_t* g = &sim.groups[find_group(t->priority)];
    trace_log(sim.cycle, sim.R, TR_BLOCK, t->id, t->priority, g->count, g->fant_calls, g->W, g->W_base, 0, 0, t->debt, t->got_thread, sim.W, sim.W_base);
}

static void handle_thread_ready(mock_thread_t* t) {
    assert(t != NULL);
    assert(t->state == THREAD_READY);
    stat_thread_ready(t->stat, sim.cycle);

    mock_group_t* g = &sim.groups[find_group(t->priority)];
    trace_log(sim.cycle, sim.R, TR_RUN, t->id, t->priority, g->count, g->fant_calls, g->W, g->W_base, 0, 0, t->debt, t->got_thread, sim.W, sim.W_base);

    if (t->debt > 0) {
        t->debt--;
        stat_thread_repay(t->stat);

        trace_log(sim.cycle, sim.R, TR_REPAY, t->id, t->priority, g->count, g->fant_calls, g->W, g->W_base, 0, 0, t->debt, t->got_thread, sim.W, sim.W_base);
    }
    assert(t->debt >= 0);
}

static void handle_thread_remove(mock_thread_t* t) {
    assert(t != NULL);
    assert(t->state == THREAD_ZOMBIE);

    mock_group_t* g = &sim.groups[find_group(t->priority)];
    assert(g != NULL);
    assert(g->count > 0);

    share_t s = compute_share(g, &sim);

    if (s.delta < 0) {
        fprintf(stderr,
                "NEG DELTA: cycle=%d R=%lld W=%lld Wbase=%lld "
                "grp=%d prio=%d count=%d W_g=%lld Wbase_g=%lld "
                "delta=%lld got=%lld\n",
                sim.cycle, sim.R, sim.W, sim.W_base, find_group(t->priority), t->priority, g->count, g->W, g->W_base, s.delta, s.got);
        // сюда же можно abort() или assert(0) для остановки
    }

    if (t->got_thread != s.got) {
        fprintf(stderr,
                "MISMATCH: cycle=%d R=%lld id=%d prio=%d "
                "got_thread=%lld s.got=%lld g_count=%d g_fant=%lld "
                "g_W=%lld g_Wbase=%lld sim_W=%lld sim_R=%lld\n",
                sim.cycle, sim.R, t->id, t->priority, t->got_thread, s.got, g->count, g->fant_calls, g->W, g->W_base, sim.W, sim.R);
    }
    assert(t->got_thread >= s.got - 1);
    assert(t->got_thread <= s.got + 1);
    assert(s.delta >= 0);
    assert(s.got >= 0);

    int id = t->id;
    int priority = t->priority;
    long long save_debt = t->debt;
    long long save_got_thread = t->got_thread;
    trace_log(sim.cycle, sim.R, TR_REMOVE_BEFORE, id, priority, g->count, g->fant_calls, g->W, g->W_base, s.got, s.delta, t->debt, t->got_thread, sim.W, sim.W_base);

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

    // Если группа опустела, сбрасываем состояние её текущего цикла
    if (g->head == NULL) {
        sim.W -= g->W;  // убрать резидуал из глобального веса
        g->W = 0;
        g->fant_calls = 0;
        g->deficit = 0;
        g->calls = 0;

        assert(g->W == 0);
        assert(g->W_base == 0);
        assert(g->deficit == 0);
        assert(g->calls == 0);
        assert(g->fant_calls == 0);
        assert(g->count == 0);
        assert(g->head == NULL);
        assert(g->current == NULL);
    }

    // Пересчёт дефицитов всех групп с новыми весами
    recalculation_deficit();

    trace_log(sim.cycle, sim.R, TR_REMOVE_AFTER, id, priority, g->count, g->fant_calls, g->W, g->W_base, s.got, s.delta, save_debt, save_got_thread, sim.W, sim.W_base);

    check_invariants();
}

// ============================================================================
//  Диспетчер по состоянию потока
// ============================================================================

typedef enum {
    STEP_CONTINUE,  // фиксируем решение (шаг 4 sim_step)
    STEP_ABORT,     // выходим из sim_step немедленно
} step_result_t;

// Возвращает STEP_ABORT для ZOMBIE, потому что при удалении поток
// уже исчез — фиксировать решение по нему нельзя.
static step_result_t handle_thread_state(mock_thread_t* t) {
    assert(t != NULL);

    switch (t->state) {
        case THREAD_BLOCKED: handle_thread_blocked(t); return STEP_CONTINUE;
        case THREAD_READY  : handle_thread_ready(t); return STEP_CONTINUE;
        case THREAD_ZOMBIE : handle_thread_remove(t); return STEP_ABORT;

        default: fprintf(stderr, "WARNING: unexpected thread state %d\n", t->state); return STEP_CONTINUE;
    }
}

// ============================================================================
//  Шаг симулятора
// ============================================================================

void sim_step(void) {
    check_invariants();
    if (sim.W == 0) return;

    // 1. Выбор группы с наибольшим дефицитом
    int gid = sim_select_group();
    if (gid < 0) return;  // нет активных групп
    mock_group_t* g = &sim.groups[gid];

    // 2. Выбор потока
    mock_thread_t* thread = sim_select_thread(gid);
    assert(thread != NULL);
    assert(thread == g->current);

    // 3. Обработка состояния
    if (handle_thread_state(thread) == STEP_ABORT) return;

    // 4. Фиксация решения
    g->current = thread->next;
    assert(g->current != NULL);

    g->calls++;
    g->fant_calls++;
    g->deficit -= sim.W;
    sim.R++;

    thread->got_thread++;

    // 5. Обновление дефицитов всех групп на их вес
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
        update_deficit();  // ← инициализация дефицитов на новый цикл
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
        assert(priorities[i] > 0 && "Priority must be > 0");
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
//  Создание / удаление потоков
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

    if (s.delta < 0) {
        fprintf(stderr,
                "NEG DELTA: cycle=%d R=%lld W=%lld Wbase=%lld "
                "grp=%d prio=%d count=%d W_g=%lld Wbase_g=%lld "
                "delta=%lld got=%lld\n",
                sim.cycle, sim.R, sim.W, sim.W_base, group_id, priority, g->count, g->W, g->W_base, s.delta, s.got);
        // сюда же можно abort() или assert(0) для остановки
    }

    assert(s.delta >= 0);
    assert(s.got >= 0);

    if (g->current != NULL) assert(g->current->got_thread == s.got);

    t->id = sim.thread_id++;
    t->priority = priority;
    t->debt = 0;
    t->state = THREAD_READY;
    t->next = t;
    t->prev = t;
    t->stat = stat_create_thread(t, sim.cycle, s.got);
    assert(t->stat != NULL);

    if (g->current != NULL) t->got_thread = g->current->got_thread;
    else t->got_thread = s.got;
    assert(t->got_thread >= 0);

    trace_log(sim.cycle, sim.R, TR_CREATE_BEFORE, t->id, priority, g->count, g->fant_calls, g->W, g->W_base, s.got, s.delta, t->debt, t->got_thread, sim.W, sim.W_base);

    insert_thread_in_group(g, t);

    // delta — временная надбавка к весу этого цикла,
    // got — кредит, добавляемый в fant_calls (сохраняет среднее)
    g->W += s.delta;
    sim.W += s.delta;
    g->W_base += priority;
    sim.W_base += priority;
    g->fant_calls += s.got;

    g->count += 1;

    recalculation_deficit();

    check_invariants();

    trace_log(sim.cycle, sim.R, TR_CREATE_AFTER, t->id, priority, g->count, g->fant_calls, g->W, g->W_base, s.got, s.delta, t->debt, t->got_thread, sim.W, sim.W_base);

    return t;
}

void sim_remove_thread(mock_thread_t* thread) {
    assert(thread != NULL);
    thread->state = THREAD_ZOMBIE;
}

// Принудительно удалить все оставшиеся потоки во всех группах.
// Используется в конце симуляции — потоки удаляются целиком, вся
// информация о них остаётся в статистике.
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

// ============================================================================
//  Блокировка / разблокировка
// ============================================================================

void sim_block_thread(mock_thread_t* thread) {
    assert(thread != NULL);
    if (thread->state == THREAD_READY) {
        thread->state = THREAD_BLOCKED;
        mock_group_t* g = &sim.groups[find_group(thread->priority)];
        trace_log(sim.cycle, sim.R, TR_SET_BLOCKED, thread->id, thread->priority, g->count, g->fant_calls, g->W, g->W_base, 0, 0, thread->debt, thread->got_thread, sim.W, sim.W_base);
    }

    check_invariants();
}

void sim_unblock_thread(mock_thread_t* thread) {
    assert(thread != NULL);
    if (thread->state == THREAD_BLOCKED) {
        thread->state = THREAD_READY;
        mock_group_t* g = &sim.groups[find_group(thread->priority)];
        trace_log(sim.cycle, sim.R, TR_SET_READY, thread->id, thread->priority, g->count, g->fant_calls, g->W, g->W_base, 0, 0, thread->debt, thread->got_thread, sim.W, sim.W_base);
    }
    check_invariants();
}
