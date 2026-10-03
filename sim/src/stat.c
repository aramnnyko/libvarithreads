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

        // Отзываем кредит за прошедший цикл — при условии, что кредит вообще был выдан.
        if (s->actual_full_quant_cycle > 0 && s->debt_in_this_cycle) s->actual_full_quant_cycle -= 1;

        // Выдаём кредит за новый цикл.
        s->actual_cycle++;
        s->actual_full_quant_cycle++;

        s->debt_in_this_cycle = false;
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
    s->actual_full_quant_cycle = (got == 0) ? 1 : 0;

    s->missed_before = got;
    s->missed_after = 0;

    s->debt_accrued = 0;
    s->debt_repaid = 0;
    s->debt_final = 0;
    s->debt_max = 0;

    s->first_run_at = -1;
    s->last_run_at = -1;

    s->debt_in_this_cycle = false;
    s->next = NULL;

    stat_list_add(s);

    return s;
}

void stat_thread_remove(mock_thread_t* t, mock_thread_stat_t* s, int cycle, long long delta) {
    s->removed_at = cycle;

    if (delta != 0)
        if (s->actual_cycle > 0) s->actual_cycle -= 1;

    if (delta != 0 || s->debt_in_this_cycle)
        if (s->actual_full_quant_cycle > 0) s->actual_full_quant_cycle -= 1;

    s->missed_after = delta;
    s->debt_final = t->debt;
}

void stat_thread_blocked(mock_thread_t* t, mock_thread_stat_t* s) {
    s->debt_accrued++;
    if (t->debt > s->debt_max) s->debt_max = t->debt;

    s->debt_in_this_cycle = true;
}

void stat_thread_ready(mock_thread_stat_t* s, int cycle) {
    s->actual_quant++;
    if (s->first_run_at == -1) s->first_run_at = cycle;
    s->last_run_at = cycle;
}

void stat_thread_repay(mock_thread_stat_t* s) {
    s->actual_quant++;
    s->debt_repaid++;

    s->debt_in_this_cycle = true;
}
