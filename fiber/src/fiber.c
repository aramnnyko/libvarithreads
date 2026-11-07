#include "../include/fiber.h"

#include "../../sched/include/drrd.h"

// «эй, я создался — положи в user_data что надо»
void fiber_create_thread(struct sched_thread* sched) {
    fiber_thread_t* t = calloc(1, sizeof(*t));
    t->sched = sched;
    sched->fiber = t;
}

// «эй, я удаляюсь — забери user_data»
void fiber_remove_thread(struct sched_thread* sched) {}

// «эй, я выбрал вот этот — делай что хочешь»
void fiber_run_thread(struct sched_thread* sched) {}