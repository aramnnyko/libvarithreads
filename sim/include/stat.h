#ifndef STAT_H
#define STAT_H

typedef struct mock_thread_stat {
    int id;        // уникальный идентификатор потока
    int priority;  // приоритет (совпадает с g->priority)

    int created_at;  // номер цикла создания; 1 — до старта
    int removed_at;  // номер цикла удаления; -1 — жив до конца

    long long actual_quant;             // всего квантов реально выполнено
    long long actual_cycle;             // всего циклов прожито от первого до последнего (исключением границ, где поток присутствовал не полностью)
    long long actual_full_quant_cycle;  // из них — циклов, где получено ровно priority квантов

    long long missed_before;  // квантов пропущено в цикле создания (кредит got)
    long long missed_after;   // квантов пропущено в цикле удаления (delta)

    long long debt_accrued;  // всего квантов ушло в долг (каждая блокировка +1)
    long long debt_repaid;   // всего квантов долга погашено (двойной квант +1)
    long long debt_final;    // долг на момент удаления или конца симуляции
    long long debt_max;      // пиковое значение (accrued − repaid) за всю жизнь

    int first_run_at;  // цикл первого фактического выполнения; -1 — ни разу
    int last_run_at;   // цикл последнего фактического выполнения; -1 — ни разу

    struct mock_thread_stat* next;
} mock_thread_stat_t;

typedef struct {
    mock_thread_stat_t* first;
    mock_thread_stat_t* back;
    int count;
} stat_list_t;

#endif