#ifndef MODE_H
#define MODE_H

#include "../../varithreads.h"

/* алгоритмозависимо */
#if defined(VT_SCHED_DRRD)
#include "../../sched/include/drrd.h"
#elif defined(VT_SCHED_RR)
#include "../../sched/include/rr.h"
#elif defined(VT_SCHED_WRR)
#include "../../sched/include/wrr.h"
#elif defined(VT_SCHED_STRIDE)
#include "../../sched/include/stride.h"
#else
#error "Define VT_SCHED_DRRD, VT_SCHED_RR, VT_SCHED_WRR or VT_SCHED_STRIDE"
#endif

void mode_init(void);
void mode_run(void);
void mode_shutdown(void);

#endif