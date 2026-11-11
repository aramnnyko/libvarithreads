#include "../include/cooperative.h"

#include "../../sched/include/drrd.h"

void mode_init(void) {}
void mode_run(void) { sched_run(); }
void mode_shutdown(void) {}