#ifndef TRACE_H
#define TRACE_H

#include <stdio.h>

typedef enum {
    TR_CREATE_BEFORE,
    TR_CREATE_AFTER,
    TR_REMOVE_BEFORE,
    TR_REMOVE_AFTER,
    TR_BLOCK,        // current был BLOCKED, debt++
    TR_REPAY,        // current READY с долгом, debt--, +2 кванта
    TR_RUN,          // current READY без долга, +1 квант
    TR_SET_BLOCKED,  // внешний sim_block_thread
    TR_SET_READY,    // внешний sim_unblock_thread
} trace_event_t;

void trace_open(const char* path, int only_priority);  // -1 = все
void trace_close(void);

void trace_log(int cycle, long long R, trace_event_t ev, int id, int priority, int g_count, long long g_fant, long long g_W, long long g_Wbase, long long got, long long delta, long long debt,
               long long got_thread, long long sim_W, long long sim_Wbase);

#endif