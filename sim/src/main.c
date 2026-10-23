#define _POSIX_C_SOURCE 199309L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../include/drrd.h"
#include "../include/fuzz.h"
#include "../include/print.h"
#include "../include/stat.h"
#include "../include/trace.h"

int main(int argc, char** argv) {
    unsigned seed = FUZZ_SEED_DEFAULT;
    int cycles = FUZZ_CYCLES_DEFAULT;

    if (argc > 1) seed = (unsigned)strtoul(argv[1], NULL, 10);
    if (argc > 2) cycles = atoi(argv[2]);

    struct timespec t_start, t_end;
    clock_gettime(CLOCK_MONOTONIC, &t_start);

    fuzz_run(seed, cycles);

    clock_gettime(CLOCK_MONOTONIC, &t_end);

    extern unsigned long long total_sim_steps;
    double elapsed = (t_end.tv_sec - t_start.tv_sec) + (t_end.tv_nsec - t_start.tv_nsec) / 1e9;
    printf("Elapsed time: %.6f s\n", elapsed);
    printf("Total steps: %llu\n", total_sim_steps);

    return 0;
}