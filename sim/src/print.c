#include "../include/print.h"

#include "../include/drrd.h"
#include "../include/stat.h"

extern sim_state_t sim;  // глобальный sim из drrd.c

// ============================================================================
//  Шапка
// ============================================================================

void print_header(void) {
    printf("=== DRRD simulation summary ===\n");
    printf("Cycles:        %d\n", sim.cycle);
    printf("Groups:        %d\n", sim.group_count);
    printf("Threads total: %d\n", stat.count);
    printf("\n");
}

// ============================================================================
//  Таблица потоков
// ============================================================================

// Expected = priority × число циклов, которые поток реально прожил.
// created_at == 0 означает «до старта» и не считается отдельным циклом.
static long long cycles_lived(const mock_thread_stat_t* s) {
    long long c = s->removed_at - s->created_at + 1;
    if (s->created_at == 0) c -= 1;
    return c;
}

void print_thread_stats(void) {
    printf("%4s %4s %9s %9s %9s %9s %10s %10s %10s %10s %10s %10s %6s\n", "ID", "Prio", "CreatedAt", "RemovedAt", "AliveFull", "FullQuota", "Expected", "Actual", "FantTotal", "LostTotal", "Debt", "SumCheck",
           "Diff");

    int debt_mismatch = 0;

    for (mock_thread_stat_t* s = stat.first; s != NULL; s = s->next) {
        long long Expected = (long long)s->priority * cycles_lived(s);
        long long Actual = s->actual_quant;
        long long FantTotal = s->missed_before;
        long long LostTotal = s->missed_after;
        long long Debt = s->debt_final;

        if (Debt != s->debt_accrued - s->debt_repaid) debt_mismatch++;

        long long SumCheck = Actual + FantTotal + LostTotal + Debt;
        long long Diff = Expected - SumCheck;

        printf("%4d %4d %9d %9d %9lld %9lld %10lld %10lld %10lld %10lld %10lld %10lld %6lld\n", s->id, s->priority, s->created_at, s->removed_at, s->actual_cycle, s->actual_full_quant_cycle, Expected, Actual,
               FantTotal, LostTotal, Debt, SumCheck, Diff);
    }

    if (debt_mismatch > 0) fprintf(stderr, "WARN: debt_final != accrued - repaid for %d thread(s)\n", debt_mismatch);
}

// ============================================================================
//  Долг: глобальная сводка и чемпионы
// ============================================================================

void print_stats_dept(void) {
    long long total_accrued = 0, total_repaid = 0, total_lost = 0;
    long long max_accrued = 0, max_repaid = 0, max_lost = 0, max_peak = 0;
    int id_accrued = -1, id_repaid = -1, id_lost = -1, id_peak = -1;

    for (mock_thread_stat_t* s = stat.first; s != NULL; s = s->next) {
        total_accrued += s->debt_accrued;
        total_repaid += s->debt_repaid;
        total_lost += s->debt_final;

        if (s->debt_accrued > max_accrued) {
            max_accrued = s->debt_accrued;
            id_accrued = s->id;
        }
        if (s->debt_repaid > max_repaid) {
            max_repaid = s->debt_repaid;
            id_repaid = s->id;
        }
        if (s->debt_final > max_lost) {
            max_lost = s->debt_final;
            id_lost = s->id;
        }
        if (s->debt_max > max_peak) {
            max_peak = s->debt_max;
            id_peak = s->id;
        }
    }

    printf("=== Debt summary ===\n");
    printf("Accrued (total):          %lld\n", total_accrued);
    printf("Repaid  (total):          %lld\n", total_repaid);
    printf("Lost    (total):          %lld\n", total_lost);
    printf("Peak debt (thread):       %lld (id=%d)\n", max_peak, id_peak);
    printf("Max accrued (thread):     %lld (id=%d)\n", max_accrued, id_accrued);
    printf("Max repaid  (thread):     %lld (id=%d)\n", max_repaid, id_repaid);
    printf("Max lost    (thread):     %lld (id=%d)\n", max_lost, id_lost);

    // После sim_cleanup все потоки удалены: долг каждого либо погашен,
    // либо потерян при удалении. Проверка сильная — падение означает баг.
    if (total_accrued != total_repaid + total_lost) fprintf(stderr, "WARN: total accrued != repaid + lost\n");

    printf("\n");
}

// ============================================================================
//  Сводка по группам
// ============================================================================

void print_group_stats(void) {
    printf("%4s %4s %8s %10s %10s %10s %10s %10s %10s %6s\n", "Grp", "Prio", "Threads", "TotalExp", "TotalAct", "Fant", "Lost", "Debt", "SumCheck", "Diff");

    for (int i = 0; i < sim.group_count; i++) {
        int prio = sim.groups[i].priority;

        long long threads = 0;
        long long total_exp = 0, total_act = 0;
        long long fant = 0, lost = 0, debt = 0;

        for (mock_thread_stat_t* s = stat.first; s != NULL; s = s->next) {
            if (s->priority != prio) continue;

            threads++;
            total_exp += (long long)s->priority * cycles_lived(s);
            total_act += s->actual_quant;
            fant += s->missed_before;
            lost += s->missed_after;
            debt += s->debt_final;
        }

        long long sumcheck = total_act + fant + lost + debt;
        long long diff = total_exp - sumcheck;

        printf("%4d %4d %8lld %10lld %10lld %10lld %10lld %10lld %10lld %6lld\n", i, prio, threads, total_exp, total_act, fant, lost, debt, sumcheck, diff);
    }

    printf("\n");
}

// ============================================================================
//  Гистограмма |Diff|
// ============================================================================

// Верхняя граница для массива частот. Если |Diff| превысит — попадёт
// в последнюю ячейку, чтобы не терять данные.
#define HIST_MAX 10

void print_diff_histogram(void) {
    int hist[HIST_MAX + 1] = {0};
    long long total = 0;
    int max_diff = 0;

    for (mock_thread_stat_t* s = stat.first; s != NULL; s = s->next) {
        long long Expected = (long long)s->priority * cycles_lived(s);
        long long SumCheck = s->actual_quant + s->missed_before + s->missed_after + s->debt_final;
        long long d = Expected - SumCheck;
        if (d < 0) d = -d;
        if (d > max_diff) max_diff = (int)d;
        if (d > HIST_MAX) d = HIST_MAX;
        hist[d]++;
        total++;
    }

    if (max_diff > HIST_MAX) printf("Note: |Diff| > %d collapsed into last bucket.\n", HIST_MAX);

    printf("=== |Diff| histogram (%lld threads) ===\n", total);
    for (int i = 0; i <= HIST_MAX; i++) {
        if (hist[i] == 0) continue;
        printf("%2d | ", i);
        for (int j = 0; j < hist[i]; j++) putchar('#');
        printf(" %d\n", hist[i]);
    }
}