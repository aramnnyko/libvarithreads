#ifndef STAT_H
#define STAT_H

#include "drrd.h"

typedef struct mock_thread_stat {
    int id;        // уникальный идентификатор потока
    int priority;  // приоритет (совпадает с g->priority)

    int created_at;  // номер цикла создания; 0 — до старта
    int removed_at;  // номер цикла удаления;

    long long actual_quant;             // всего квантов реально выполнено
    long long actual_cycle;             // всего циклов прожито от первого до последнего (исключением границ, где поток присутствовал не полностью)
    long long actual_full_quant_cycle;  // из них — циклов, где получено ровно priority квантов (из которых 0 за погашение долга)

    long long missed_before;  // квантов пропущено в цикле создания (кредит got)
    long long missed_after;   // квантов пропущено в цикле удаления (delta)

    long long debt_accrued;  // всего квантов ушло в долг (каждая блокировка +1)
    long long debt_repaid;   // всего квантов долга погашено (двойной квант +1)
    long long debt_final;    // долг на момент удаления или конца симуляции
    long long debt_max;      // пиковое значение (accrued − repaid) за всю жизнь

    int first_run_at;  // цикл первого фактического выполнения; -1 — ни разу
    int last_run_at;   // цикл последнего фактического выполнения; -1 — ни разу

    bool debt_in_this_cycle;  // Было ли в этом цикле начисление или погашение долга. Дополнительная для actual_full_quant_cycle

    struct mock_thread_stat* next;
} mock_thread_stat_t;

typedef struct {
    mock_thread_stat_t* first;
    mock_thread_stat_t* back;
    int count;
} stat_list_t;

extern stat_list_t stat;

#ifdef ENABLE_STAT

void stat_init(void);
void stat_reset(void);
void stat_cleanup(void);

mock_thread_stat_t* stat_create_thread(mock_thread_t* t, long long got);
void stat_thread_blocked(mock_thread_t* t);
void stat_thread_ready(mock_thread_t* t);
void stat_thread_repay(mock_thread_t* t);
void stat_thread_remove(mock_thread_t* t, long long delta);

#else /* !ENABLE_STAT */

static inline void stat_init(void) {}
static inline void stat_reset(void) {}
static inline void stat_cleanup(void) {}

static inline mock_thread_stat_t* stat_create_thread(mock_thread_t* t, long long got) {
    (void)t, (void)got;
    return NULL;
}
static inline void stat_thread_blocked(mock_thread_t* t) { (void)t; }
static inline void stat_thread_ready(mock_thread_t* t) { (void)t; }
static inline void stat_thread_repay(mock_thread_t* t) { (void)t; }
static inline void stat_thread_remove(mock_thread_t* t, long long delta) { (void)t, (void)delta; }

#endif /* ENABLE_STAT */

#endif /* STAT_H */