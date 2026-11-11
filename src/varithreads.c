#include "../varithreads.h"

#include "../fiber/include/fiber.h"
#include "../mode/include/mode.h"

/* алгоритмозависимо */
#if defined(VT_SCHED_DRRD)
#include "../sched/include/drrd.h"
#elif defined(VT_SCHED_RR)
#include "../sched/include/rr.h"
#elif defined(VT_SCHED_WRR)
#include "../sched/include/wrr.h"
#elif defined(VT_SCHED_STRIDE)
#include "../sched/include/stride.h"
#else
#error "Define VT_SCHED_DRRD, VT_SCHED_RR, VT_SCHED_WRR or VT_SCHED_STRIDE"
#endif

/* жизненный цикл */
/* алгоритмозависимо */
#if defined(VT_SCHED_DRRD)
int vt_init(int group_count, const int priorities[]) {
    fiber_init();
    sched_init(group_count, priorities);
    mode_init();
    return 0;
}
#elif defined(VT_SCHED_RR)
int vt_init(void) {
    fiber_init();
    sched_init();
    mode_init();
    return 0;
}
#elif defined(VT_SCHED_WRR)
int vt_init(void) {
    fiber_init();
    sched_init();
    mode_init();
    return 0;
}
#elif defined(VT_SCHED_STRIDE)
int vt_init(void) {
    fiber_init();
    sched_init();
    mode_init();
    return 0;
}
#else
#error "Define VT_SCHED_DRRD, VT_SCHED_RR, VT_SCHED_WRR or VT_SCHED_STRIDE"
#endif

void vt_run(void) { mode_run(); }

void vt_cleanup(void) {
    mode_shutdown();
    sched_cleanup();  // сначала планировщик — он зовёт fiber_remove_thread
    fiber_cleanup();  // потом fiber — освобождает pending_dead
}

/* потоки */
/* алгоритмозависимо */
#if defined(VT_SCHED_DRRD)
thread_t thread_create(thread_fn_t fn, void* arg, int priority) { return fiber_create(fn, arg, sched_create_thread(priority)); }
#elif defined(VT_SCHED_RR)
thread_t thread_create(thread_fn_t fn, void* arg) { return fiber_create(fn, arg, sched_create_thread()); }
#elif defined(VT_SCHED_WRR)
thread_t thread_create(thread_fn_t fn, void* arg, int weight) { return fiber_create(fn, arg, sched_create_thread(weight)); }
#elif defined(VT_SCHED_STRIDE)
thread_t thread_create(thread_fn_t fn, void* arg, int priority) { return fiber_create(fn, arg, sched_create_thread(priority)); }
#else
#error "Define VT_SCHED_DRRD, VT_SCHED_RR, VT_SCHED_WRR or VT_SCHED_STRIDE"
#endif

void thread_yield(void) { fiber_yield(); }

void thread_block(void) { fiber_block(); }
void thread_unblock(thread_t t) { fiber_unblock(t); }

void thread_join(thread_t t) { fiber_join(t); }
void thread_exit(void) { fiber_exit(); }
thread_t thread_self(void) { return fiber_self(); }