#include "../include/drrd.h"

#include "../include/stat.h"

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
        s.delta = (sim->W > 0) ? (long long)g->priority * (sim->W - sim->R) / sim->W : g->priority;
        s.got = g->priority - s.delta;
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
    }

    stat_reset(sim.cycle);
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

// BLOCKED: поток не выполняется, но группа уже «заплатила» за решение.
// Поток запоминает недополученный квант в debt.
static void handle_thread_blocked(mock_thread_t* t) {
    t->debt++;
    stat_thread_blocked(t, t->stat);
}

// READY: штатный квант + при наличии долга — дополнительный квант
// погашения. Погашение не увеличивает calls/R: группа уже учла этот
// квант в своей квоте ранее.
static void handle_thread_ready(mock_thread_t* t) {
    stat_thread_ready(t, t->stat, sim.cycle);
    if (t->debt > 0) {
        t->debt--;
        stat_thread_repay(t, t->stat);
    }
}

// ZOMBIE: физическое удаление. Сначала вычитаем из весов и fant_calls
// то, что было добавлено при создании (или скорректировано ранее),
// потом вышиваем из списка и пересчитываем дефициты.
static void handle_thread_remove(mock_thread_t* t) {
    mock_group_t* g = &sim.groups[find_group(t->priority)];
    share_t s = compute_share(g, &sim);

    g->W -= s.delta;
    sim.W -= s.delta;
    g->W_base -= t->priority;
    sim.W_base -= t->priority;
    g->fant_calls -= s.got;

    // Вышиваем из кругового списка (до статистики, чтобы группа
    // уже была в новом составе при пересчёте дефицитов)
    remove_thread_from_group(g, t);

    // Передаём в статистику
    stat_thread_remove(t, t->stat, sim.cycle, s.delta);

    free(t);

    g->count -= 1;

    // Если группа опустела, сбрасываем состояние её текущего цикла
    if (g->head == NULL) {
        g->deficit = 0;
        g->calls = 0;
    }

    // Пересчёт дефицитов всех групп с новыми весами
    recalculation_deficit();
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
    if (sim.W == 0) return;

    // 1. Выбор группы с наибольшим дефицитом
    int gid = sim_select_group();
    if (gid < 0) return;  // нет активных групп
    mock_group_t* g = &sim.groups[gid];

    // 2. Выбор потока
    mock_thread_t* thread = sim_select_thread(gid);

    // 3. Обработка состояния
    if (handle_thread_state(thread) == STEP_ABORT) return;

    // 4. Фиксация решения
    g->current = thread->next;
    g->calls++;
    g->fant_calls++;
    g->deficit -= sim.W;
    sim.R++;

    // 5. Обновление дефицитов всех групп на их вес
    update_deficit();
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
}

// ============================================================================
//  Создание / удаление потоков
// ============================================================================

mock_thread_t* sim_create_thread(int priority) {
    assert(priority > 0);

    int group_id = find_group(priority);
    assert(group_id != -1 && "group for this priority must exist");

    mock_thread_t* t = calloc(1, sizeof(*t));
    if (!t) return NULL;

    mock_group_t* g = &sim.groups[group_id];
    share_t s = compute_share(g, &sim);

    t->id = sim.thread_id++;
    t->priority = priority;
    t->debt = 0;
    t->state = THREAD_READY;
    t->next = t;
    t->prev = t;
    t->stat = stat_create_thread(t, sim.cycle, s.got);

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

    return t;
}

void sim_remove_thread(mock_thread_t* thread) { thread->state = THREAD_ZOMBIE; }

// Принудительно удалить все оставшиеся потоки во всех группах.
// Используется в конце симуляции — потоки удаляются целиком, вся
// информация о них остаётся в статистике.
void sim_cleanup(void) {
    for (int i = 0; i < sim.group_count; i++) {
        mock_group_t* g = &sim.groups[i];
        while (g->head != NULL) { handle_thread_remove(g->head); }
    }
}

// ============================================================================
//  Блокировка / разблокировка
// ============================================================================

void sim_block_thread(mock_thread_t* thread) {
    if (thread->state == THREAD_READY) thread->state = THREAD_BLOCKED;
}

void sim_unblock_thread(mock_thread_t* thread) {
    if (thread->state == THREAD_BLOCKED) thread->state = THREAD_READY;
}
