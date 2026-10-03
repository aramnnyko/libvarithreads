#ifndef FUZZ_H
#define FUZZ_H

// ---------- Настройка набора групп ----------

#define FUZZ_NUM_GROUPS 10
// priorities для инициализации заданы в fuzz.c

// ---------- Вероятности ----------

#define FUZZ_EVENT_PROB   20  // % — шанс события на каждой проверке
#define FUZZ_PROB_CREATE  30  // внутри события
#define FUZZ_PROB_REMOVE  30
#define FUZZ_PROB_BLOCK   30
#define FUZZ_PROB_UNBLOCK 10

// ---------- Границы ----------

#define FUZZ_MAX_ALIVE 5000
#define FUZZ_MIN_ALIVE 10

#define FUZZ_MIN_RUNNING_TO_BLOCK   10  // блокировать, только если running > этого
#define FUZZ_MIN_BLOCKED_TO_UNBLOCK 1   // разблокировать, только если blocked >= этого

#define FUZZ_INITIAL_THREADS 8

// ---------- Дефолты ----------

#define FUZZ_SEED_DEFAULT   1u
#define FUZZ_CYCLES_DEFAULT 1000

// ---------- API ----------

void fuzz_run(unsigned seed, int cycles);

#endif