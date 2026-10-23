#ifndef FUZZ_H
#define FUZZ_H

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

// ---------- Настройка набора групп ----------

#define FUZZ_NUM_GROUPS 10
// priorities для инициализации заданы в fuzz.c

// ---------- Вероятности ----------

#define FUZZ_EVENT_PROB   5   // % — шанс события на каждой проверке
#define FUZZ_PROB_CREATE  10  // вероятность создания
#define FUZZ_PROB_REMOVE  10  // вероятность удаления
#define FUZZ_PROB_BLOCK   40  // вероятность блокировки
#define FUZZ_PROB_UNBLOCK 40  // вероятность разблокировки

// ---------- Границы ----------

#define FUZZ_MAX_ALIVE 50
#define FUZZ_MIN_ALIVE 10

#define FUZZ_MIN_RUNNING_TO_BLOCK   10  // блокировать, только если running > этого
#define FUZZ_MIN_BLOCKED_TO_UNBLOCK 1   // разблокировать, только если blocked >= этого

#define FUZZ_INITIAL_THREADS FUZZ_MIN_ALIVE

// ---------- Дефолты ----------

#define FUZZ_SEED_DEFAULT   1u
#define FUZZ_CYCLES_DEFAULT 1000

// ---------- API ----------

void fuzz_run(unsigned seed, int cycles);

#endif