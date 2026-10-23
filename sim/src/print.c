#include "../include/print.h"

#include "../include/drrd.h"
#include "../include/stat.h"

extern sim_state_t sim;

// ============================================================================
//  Вычисляемые величины
// ============================================================================

static long long cycles_lived(const mock_thread_stat_t* s) {
    long long c = s->removed_at - s->created_at + 1;
    if (s->created_at == 0) c -= 1;
    return c > 0 ? c : 0;
}

// ============================================================================
//  Шапка
// ============================================================================

void print_header(void) {
    printf("=== DRRD simulation summary ===\n");
    printf("Cycles:        %d\n", sim.cycle);
    printf("Groups:        %d\n", sim.group_count);
    printf("Threads total: %d\n\n", stat.count);
}

// ============================================================================
//  Таблица потоков
// ============================================================================

void print_thread_stats(void) {
    printf("%4s %4s %9s %9s %9s %9s %9s %10s %10s %10s %10s %8s %10s %6s\n", "ID", "Prio", "CreatedAt", "RemovedAt", "FrstRunAt", "LastRunAt", "AliveFull", "Expected", "Actual", "FantTotal", "LostTotal",
           "Debt", "SumCheck", "Diff");

    for (mock_thread_stat_t* s = stat.first; s != NULL; s = s->next) {
        long long ID = s->id;
        long long Prio = s->priority;
        long long CreatedAt = s->created_at;
        long long RemovedAt = s->removed_at;
        long long FrstRunAt = s->first_run_at;
        long long LastRunAt = s->last_run_at;

        long long AliveFull = s->actual_cycle;
        long long Actual = s->actual_quant;
        long long FantTotal = s->missed_before;
        long long LostTotal = s->missed_after;
        long long Debt = s->debt_final;

        long long SumCheck = Actual + FantTotal + LostTotal + Debt;
        long long Expected = (long long)s->priority * cycles_lived(s);
        long long Diff = Expected - SumCheck;

        printf("%4lld %4lld %9lld %9lld %9lld %9lld %9lld %10lld %10lld %10lld %10lld %8lld %10lld %6lld\n", ID, Prio, CreatedAt, RemovedAt, FrstRunAt, LastRunAt, AliveFull, Expected, Actual, FantTotal,
               LostTotal, Debt, SumCheck, Diff);
    }
    printf("\n");
}

// ============================================================================
//  Сводка по группам
// ============================================================================

void print_group_stats(void) {
    printf("%4s %4s %8s %10s %10s %10s %10s %8s %10s %6s\n", "Grp", "Prio", "Threads", "TotalExp", "TotalAct", "Fant", "Lost", "Debt", "SumCheck", "Diff");

    for (int i = 0; i < sim.group_count; i++) {
        int prio = sim.groups[i].priority;

        long long threads = 0;
        long long total_exp = 0;
        long long total_act = 0;
        long long fant = 0;
        long long lost = 0;
        long long debt = 0;

        for (mock_thread_stat_t* s = stat.first; s != NULL; s = s->next) {
            if (s->priority != prio) continue;

            long long e = (long long)s->priority * cycles_lived(s);

            threads++;
            total_exp += e;
            total_act += s->actual_quant;
            fant += s->missed_before;
            lost += s->missed_after;
            debt += s->debt_final;
        }

        long long sumcheck = total_act + fant + lost + debt;
        long long diff = total_exp - sumcheck;

        printf("%4d %4d %8lld %10lld %10lld %10lld %10lld %8lld %10lld %6lld\n", i, prio, threads, total_exp, total_act, fant, lost, debt, sumcheck, diff);
    }
    printf("\n");
}

// ============================================================================
//  Сводка по долгам
// ============================================================================

void print_stats_dept(void) {
    long long total_accrued = 0;
    long long total_repaid = 0;
    long long total_lost = 0;

    long long max_accrued = 0;
    long long max_repaid = 0;
    long long max_lost = 0;
    long long max_peak = 0;

    int id_accrued = -1;
    int id_repaid = -1;
    int id_lost = -1;
    int id_peak = -1;

    for (mock_thread_stat_t* s = stat.first; s != NULL; s = s->next) {
        long long accrued = s->debt_accrued;
        long long repaid = s->debt_repaid;
        long long lost = s->debt_final;
        long long peak = s->debt_max;

        total_accrued += accrued;
        total_repaid += repaid;
        total_lost += lost;

        if (accrued > max_accrued) {
            max_accrued = accrued;
            id_accrued = s->id;
        }
        if (repaid > max_repaid) {
            max_repaid = repaid;
            id_repaid = s->id;
        }
        if (lost > max_lost) {
            max_lost = lost;
            id_lost = s->id;
        }
        if (peak > max_peak) {
            max_peak = peak;
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

    if (total_accrued != total_repaid + total_lost) fprintf(stderr, "WARN: total accrued != repaid + lost\n");

    printf("\n");
}

// ============================================================================
//  Гистограмма |Diff|
// ============================================================================

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
        printf("%2d | %d\n", i, hist[i]);
    }
}
