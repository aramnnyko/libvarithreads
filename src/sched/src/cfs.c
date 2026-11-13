#include "../include/cfs.h"

#include <stdlib.h>

#include "../../fiber/include/fiber.h"

#define CFS_WEIGHT_SCALE (1LL << 32)

sched_state_t sched;

// ============================================================================
//  Компаратор
// ============================================================================

static int cfs_cmp(const struct rb_node* a, const struct rb_node* b) {
    sched_thread_t* ta = rb_node_data(sched_thread_t, a, rb_node);
    sched_thread_t* tb = rb_node_data(sched_thread_t, b, rb_node);

    // В этой библиотеке большие элементы уходят ВЛЕВО.
    // Нам нужно, чтобы влево уходили МЕНЬШИЕ vruntime. Переворачиваем:
    if (ta->vruntime < tb->vruntime) return 1;   // ta "больше" (пойдёт вправо)
    if (ta->vruntime > tb->vruntime) return -1;  // tb "меньше" (пойдёт влево)

    // Tie-breaker: если vruntime равны, сравниваем адреса
    if (ta < tb) return 1;
    if (ta > tb) return -1;
    return 0;
}

// ============================================================================
//  Круговой двусвязный список потоков внутри группы
// ============================================================================

static void insert_thread(sched_thread_t* t) {
    if (!t->in_tree) {
        rb_tree_insert(&sched.tree, &t->rb_node, cfs_cmp);
        t->in_tree = 1;
    }
}

static void remove_thread(sched_thread_t* t) {
    if (t->in_tree) {
        rb_tree_remove(&sched.tree, &t->rb_node);
        t->in_tree = 0;
    }
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

    remove_thread(t);
    fiber_remove_thread(t);  // «эй, я удаляюсь — забери user_data»
    free(t);
}

// ============================================================================
//  Диспетчер
// ============================================================================

typedef enum { STEP_CONTINUE, STEP_ABORT } step_result_t;

// static step_result_t handle_thread_state(sched_thread_t* t) {
//     switch (t->state) {
//         case THREAD_BLOCKED: handle_thread_blocked(t); return STEP_ABORT;
//         case THREAD_READY  : handle_thread_ready(t); return STEP_CONTINUE;
//         case THREAD_ZOMBIE : handle_thread_remove(t); return STEP_ABORT;
//         default            : return STEP_CONTINUE;
//     }
// }

static step_result_t handle_thread_state(sched_thread_t* t) {
    switch (t->state) {
        case THREAD_BLOCKED: handle_thread_blocked(t); return STEP_ABORT;

        case THREAD_READY:

            handle_thread_ready(t);  // запускаем поток

            // Если после выполнения он ВСЁ ЕЩЁ READY — значит был yield.
            if (t->state == THREAD_READY) { return STEP_CONTINUE; }

            // Если он завершился — удаляем.
            if (t->state == THREAD_ZOMBIE) { handle_thread_remove(t); }

            // Если он заблокировался — просто выходим (он уже вынут из дерева).
            return STEP_ABORT;

        case THREAD_ZOMBIE: handle_thread_remove(t); return STEP_ABORT;

        default: return STEP_ABORT;
    }
}

// ============================================================================
//  Шаг
// ============================================================================

void sched_step(void) {
    if (rb_tree_is_empty(&sched.tree)) return;

    struct rb_node* min_node = rb_tree_first(&sched.tree);
    sched_thread_t* t = rb_node_data(sched_thread_t, min_node, rb_node);
    if (!t) return;

    remove_thread(t);

    if (handle_thread_state(t) == STEP_ABORT) return;  // Если уснул/умер — выходим

    // Сюда дошли ТОЛЬКО если поток сделал yield (STEP_CONTINUE)
    t->vruntime += CFS_WEIGHT_SCALE / t->weight;
    if (t->vruntime > sched.min_vruntime) { sched.min_vruntime = t->vruntime; }

    insert_thread(t);
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
    rb_tree_init(&sched.tree);
    sched.min_vruntime = 0;
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
    t->vruntime = sched.min_vruntime;
    t->state = THREAD_READY;

    insert_thread(t);
    sched.thread_live++;

    return t;
}

void sched_remove_thread(sched_thread_t* thread) {
    sched_unblock_thread(thread);
    thread->state = THREAD_ZOMBIE;
}

void sched_cleanup(void) {
    while (!rb_tree_is_empty(&sched.tree)) {
        struct rb_node* min_node = rb_tree_first(&sched.tree);
        sched_thread_t* t = rb_node_data(sched_thread_t, min_node, rb_node);

        sched_remove_thread(t);
        handle_thread_remove(t);
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

    remove_thread(thread);
}

void sched_unblock_thread(sched_thread_t* thread) {
    if (thread->state != THREAD_BLOCKED) return;

    thread->state = THREAD_READY;
    sched.thread_blocked--;

    if (thread->vruntime < sched.min_vruntime) { thread->vruntime = sched.min_vruntime; }

    insert_thread(thread);
}