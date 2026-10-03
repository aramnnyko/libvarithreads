#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/fuzz.h"
#include "../include/trace.h"

int main(int argc, char** argv) {
    unsigned seed = FUZZ_SEED_DEFAULT;
    int cycles = FUZZ_CYCLES_DEFAULT;
    const char* trace_path = NULL;
    int trace_prio = -1;

    if (argc > 1) seed = (unsigned)strtoul(argv[1], NULL, 10);
    if (argc > 2) cycles = atoi(argv[2]);

    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "--trace") == 0 && i + 1 < argc) {
            trace_path = argv[++i];
        } else if (strcmp(argv[i], "--prio") == 0 && i + 1 < argc) {
            trace_prio = atoi(argv[++i]);
        }
    }

    if (trace_path) trace_open(trace_path, trace_prio);

    fuzz_run(seed, cycles);

    if (trace_path) trace_close();
    return 0;
}