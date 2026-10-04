#include "../include/stat.h"

#include "../include/drrd.h"

// ============================================================================
//  Глобальное состояние
// ============================================================================

stat_list_t stat;

static void stat_list_add(mock_thread_stat_t* s) {
    if (stat.first == NULL) {
        stat.first = s;
        stat.back = s;
    } else {
        stat.back->next = s;
        stat.back = s;
    }
    stat.count++;
}

void stat_init(void) {
    stat.first = NULL;
    stat.back = NULL;
    stat.count = 0;
}

void stat_reset(int cycle) {
    if (cycle < 2) return;  // нет предыдущего цикла

    mock_thread_stat_t* s = stat.first;

    while (s != NULL) {
        if (s->removed_at != -1) {
            s = s->next;
            continue;
        }

        // Выдаём кредит за новый цикл.
        s->actual_cycle++;

        s = s->next;
    }
}

void stat_cleanup(void) {
    mock_thread_stat_t* s = stat.first;
    while (s != NULL) {
        mock_thread_stat_t* next = s->next;
        free(s);
        s = next;
    }
    stat.first = NULL;
    stat.back = NULL;
    stat.count = 0;
}

// ============================================================================
//  Создание / удаление
// ============================================================================

mock_thread_stat_t* stat_create_thread(mock_thread_t* t, int cycle, long long got) {
    mock_thread_stat_t* s = calloc(1, sizeof(*s));
    if (!s) return NULL;

    s->id = t->id;
    s->priority = t->priority;

    s->created_at = cycle;
    s->removed_at = -1;
    s->actual_quant = 0;

    s->actual_cycle = (got == 0) ? 1 : 0;

    s->missed_before = got;
    s->missed_after = 0;

    s->first_run_at = -1;
    s->last_run_at = -1;

    s->next = NULL;

    stat_list_add(s);

    return s;
}

void stat_thread_remove(mock_thread_t* t, mock_thread_stat_t* s, int cycle, long long delta) {
    s->removed_at = cycle;
    (void)t;

    if (delta != 0)
        if (s->actual_cycle > 0) s->actual_cycle -= 1;

    s->missed_after = delta;
}

void stat_thread_ready(mock_thread_stat_t* s, int cycle) {
    s->actual_quant++;
    if (s->first_run_at == -1) s->first_run_at = cycle;
    s->last_run_at = cycle;
}
