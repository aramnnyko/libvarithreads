#include "../varithreads.h"

#include "fiber/include/fiber.h"
#include "mode/include/mode.h"
#include "sched_select.h"

/* жизненный цикл */

int vt_init(VT_INIT_ARGS) {
    fiber_init();
    VT_INIT_CALL;
    mode_init();
    return 0;
}

void vt_run(void) { mode_run(); }

void vt_cleanup(void) {
    mode_shutdown();
    sched_cleanup();  // сначала планировщик — он зовёт fiber_remove_thread
    fiber_cleanup();  // потом fiber — освобождает pending_dead
}

/* потоки */
/* алгоритмозависимо */
thread_t thread_create(VT_CREATE_ARGS) { return fiber_create(fn, arg, VT_CREATE_CALL); }

void thread_yield(void) { fiber_yield(); }

void thread_block(void) { fiber_block(); }
void thread_unblock(thread_t t) { fiber_unblock(t); }

void thread_join(thread_t t) { fiber_join(t); }
void thread_exit(void) { fiber_exit(); }
thread_t thread_self(void) { return fiber_self(); }