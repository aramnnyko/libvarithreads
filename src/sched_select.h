#ifndef SCHED_SELECT_H
#define SCHED_SELECT_H

// #define VT_SCHED_DRRD
// #define VT_SCHED_RR
// #define VT_SCHED_WRR
// #define VT_SCHED_STRIDE
// #define VT_SCHED_CFS

/* алгоритмозависимо */
#if defined(VT_SCHED_DRRD)

#include "sched/include/drrd.h"
#define VT_INIT_ARGS   int gc, const int p[]
#define VT_INIT_CALL   sched_init(gc, p)
#define VT_CREATE_ARGS thread_fn_t fn, void *arg, int priority
#define VT_CREATE_CALL sched_create_thread(priority)

#elif defined(VT_SCHED_RR)
#include "sched/include/rr.h"
#define VT_INIT_ARGS   void
#define VT_INIT_CALL   sched_init()
#define VT_CREATE_ARGS thread_fn_t fn, void *arg
#define VT_CREATE_CALL sched_create_thread()

#elif defined(VT_SCHED_WRR)
#include "sched/include/wrr.h"
#define VT_INIT_ARGS   void
#define VT_INIT_CALL   sched_init()
#define VT_CREATE_ARGS thread_fn_t fn, void *arg, int weight
#define VT_CREATE_CALL sched_create_thread(weight)

#elif defined(VT_SCHED_STRIDE)
#include "sched/include/stride.h"
#define VT_INIT_ARGS   void
#define VT_INIT_CALL   sched_init()
#define VT_CREATE_ARGS thread_fn_t fn, void *arg, int priority
#define VT_CREATE_CALL sched_create_thread(priority)

#elif defined(VT_SCHED_CFS)
#include "sched/include/cfs.h"
#define VT_INIT_ARGS   void
#define VT_INIT_CALL   sched_init()
#define VT_CREATE_ARGS thread_fn_t fn, void *arg, int weight
#define VT_CREATE_CALL sched_create_thread(weight)

#elif defined(__INTELLISENSE__) || defined(__clangd__)
/* Browse-only: no scheduler, opaque type, stub macros.
   Ничего не определяем глобально — .c файлы это не увидят. */
struct sched_thread;
typedef struct sched_thread sched_thread_t;
#define VT_INIT_ARGS   void
#define VT_INIT_CALL   ((void)0)
#define VT_CREATE_ARGS thread_fn_t fn, void *arg
#define VT_CREATE_CALL ((void)0)

#else
#error "Define VT_SCHED_DRRD, VT_SCHED_RR, VT_SCHED_WRR or VT_SCHED_STRIDE or VT_SCHED_CFS"
#endif

#endif