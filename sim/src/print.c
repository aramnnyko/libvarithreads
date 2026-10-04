#include "../include/print.h"

#include "../include/drrd.h"
#include "../include/stat.h"

extern sim_state_t sim;

static long long cycles_lived(const mock_thread_stat_t* s) {
    long long c = s->removed_at - s->created_at + 1;
    if (s->created_at == 0) c -= 1;
    return c > 0 ? c : 0;
}

void print_header(void) {
    printf("=== DRRD simulation summary ===\n");
    printf("Cycles:        %d\n", sim.cycle);
    printf("Groups:        %d\n", sim.group_count);
    printf("Threads total: %d\n\n", stat.count);
}

void print_thread_stats(void) {
    printf("%4s %4s %9s %9s %9s %10s %10s %10s %10s %10s %6s\n", "ID", "Prio", "CreatedAt", "RemovedAt", "AliveFull", "Expected", "Actual", "FantTotal", "LostTotal", "SumCheck", "Diff");

    for (mock_thread_stat_t* s = stat.first; s != NULL; s = s->next) {
        long long Expected = (long long)s->priority * cycles_lived(s);
        long long Actual = s->actual_quant;
        long long FantTotal = s->missed_before;
        long long LostTotal = s->missed_after;
        long long SumCheck = Actual + FantTotal + LostTotal;
        long long Diff = Expected - SumCheck;

        printf("%4d %4d %9d %9d %9lld %10lld %10lld %10lld %10lld %10lld %6lld\n", s->id, s->priority, s->created_at, s->removed_at, s->actual_cycle, Expected, Actual, FantTotal, LostTotal, SumCheck, Diff);
    }
    printf("\n");
}

void print_group_stats(void) {
    printf("%4s %4s %8s %10s %10s %10s %10s %10s %6s\n", "Grp", "Prio", "Threads", "TotalExp", "TotalAct", "Fant", "Lost", "SumCheck", "Diff");

    for (int i = 0; i < sim.group_count; i++) {
        int prio = sim.groups[i].priority;
        long long threads = 0;
        long long total_exp = 0, total_act = 0, fant = 0, lost = 0;

        for (mock_thread_stat_t* s = stat.first; s != NULL; s = s->next) {
            if (s->priority != prio) continue;
            threads++;
            total_exp += (long long)s->priority * cycles_lived(s);
            total_act += s->actual_quant;
            fant += s->missed_before;
            lost += s->missed_after;
        }

        long long sumcheck = total_act + fant + lost;
        long long diff = total_exp - sumcheck;

        printf("%4d %4d %8lld %10lld %10lld %10lld %10lld %10lld %6lld\n", i, prio, threads, total_exp, total_act, fant, lost, sumcheck, diff);
    }
    printf("\n");
}

#define HIST_MAX 10

void print_diff_histogram(void) {
    int hist[HIST_MAX + 1] = {0};
    long long total = 0;
    int max_diff = 0;

    for (mock_thread_stat_t* s = stat.first; s != NULL; s = s->next) {
        long long Expected = (long long)s->priority * cycles_lived(s);
        long long SumCheck = s->actual_quant + s->missed_before + s->missed_after;
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