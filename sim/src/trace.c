#include "../include/trace.h"

static FILE* g_fp = NULL;
static int g_only_prio = -1;

void trace_open(const char* path, int only_priority) {
    if (g_fp) fclose(g_fp);
    g_fp = fopen(path, "w");
    if (!g_fp) return;

    g_only_prio = only_priority;

    fprintf(g_fp,
            "cycle,R,event,id,prio,g_count,g_fant,g_W,g_Wbase,"
            "got,delta,sim_W,sim_Wbase\n");
}

void trace_close(void) {
    if (g_fp) {
        fclose(g_fp);
        g_fp = NULL;
    }
    g_only_prio = -1;
}

void trace_log(int cycle, long long R, trace_event_t ev, int id, int priority, int g_count, long long g_fant, long long g_W, long long g_Wbase, long long got, long long delta, long long sim_W,
               long long sim_Wbase) {
    if (!g_fp) return;
    if (g_only_prio >= 0 && priority != g_only_prio) return;

    static const char* name[] = {
        "CREATE_BEFORE", "CREATE_AFTER", "REMOVE_BEFORE", "REMOVE_AFTER", "BLOCK", "REPAY",
    };

    fprintf(g_fp, "%d,%lld,%s,%d,%d,%d,%lld,%lld,%lld,%lld,%lld,%lld,%lld\n", cycle, R, name[ev], id, priority, g_count, g_fant, g_W, g_Wbase, got, delta, sim_W, sim_Wbase);
}