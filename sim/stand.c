/* sim/stand.c — DRR с пересчётом из остатка
 *
 * Сборка: gcc -O2 -o stand sim/stand.c
 * Запуск: ./stand [w0 w1 w2 w3]
 *
 * Enter — шаг
 * N+ / N- — вес группы N ±1
 * N=V     — вес группы N
 * q       — выход
 *
 * Правила:
 * - вес нельзя опустить ниже уже выбранного (picked)
 * - новый W_total не может стать меньше R
 */
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define N_GROUPS 4

typedef struct {
    long long weight;        /* сконфигурированный вес */
    long long picked;        /* всего выбрано в этом цикле */
    long long anchor_picked; /* picked на момент последнего ре-анкора */
    long long deficit;
} group_t;

static group_t G[N_GROUPS];
static long long W_total = 0;
static long long R = 0;
static long long R_anchor = 0;
static long long W_eff_total = 0;
static long long cycle = 0;

/* эффективный вес: сколько ещё «осталось получить» */
static long long W_eff(const group_t* g) {
    long long we = g->weight - g->anchor_picked;
    return we > 0 ? we : 0;
}

/* полный пересчёт состояния как будто цикл начинается сейчас */
static void re_anchor(void) {
    R_anchor = R;
    W_eff_total = 0;
    for (int i = 0; i < N_GROUPS; i++) {
        G[i].anchor_picked = G[i].picked;
        G[i].deficit = 0;
        W_eff_total += W_eff(&G[i]);
    }
}

static void cycle_end(void) {
    cycle++;
    R = 0;
    R_anchor = 0;
    W_eff_total = 0;
    W_total = 0;
    for (int i = 0; i < N_GROUPS; i++) {
        G[i].picked = 0;
        G[i].anchor_picked = 0;
        G[i].deficit = 0;
        W_total += G[i].weight;
        W_eff_total += G[i].weight > 0 ? G[i].weight : 0;
    }
}

static void print_state(void) {
    long long R_eff = R - R_anchor;
    printf("\n[cycle=%lld  R=%lld/%lld  eff=%lld/%lld]\n", cycle, R, W_total, R_eff, W_eff_total);
    printf("  grp   weight  picked  rem  W_eff   deficit\n");
    for (int i = 0; i < N_GROUPS; i++) {
        group_t* g = &G[i];
        printf("  G%d    %6lld  %6lld  %4lld  %5lld  %8lld\n", i, g->weight, g->picked, g->weight - g->picked, W_eff(g), g->deficit);
    }
}

static int pick_group(void) {
    int sel = -1;
    long long mx = LLONG_MIN;
    for (int i = 0; i < N_GROUPS; i++) {
        if (W_eff(&G[i]) == 0) continue;
        if (sel == -1 || G[i].deficit > mx) {
            mx = G[i].deficit;
            sel = i;
        }
    }
    return sel;
}

static void do_step(void) {
    if (R >= W_total || W_eff_total == 0) {
        printf("\n*** END OF CYCLE %lld ***\n", cycle);
        cycle_end();
        print_state();
        return;
    }

    for (int i = 0; i < N_GROUPS; i++) {
        long long we = W_eff(&G[i]);
        if (we > 0) G[i].deficit += we;
    }

    int g = pick_group();
    if (g < 0) return;

    G[g].deficit -= W_eff_total;
    G[g].picked++;
    R++;

    printf("\nPICK G%d\n", g);
    print_state();

    if (R >= W_total) {
        printf("\n*** END OF CYCLE %lld ***\n", cycle);
        cycle_end();
        print_state();
    }
}

static void set_weight(int g, long long w) {
    if (g < 0 || g >= N_GROUPS) return;
    if (w < 0) w = 0;
    if (w < G[g].picked) {
        printf("refused: weight < picked (%lld < %lld)\n", w, G[g].picked);
        return;
    }
    long long new_total = W_total - G[g].weight + w;
    if (new_total < R) {
        printf("refused: new W_total %lld < R %lld\n", new_total, R);
        return;
    }
    G[g].weight = w;
    W_total = new_total;

    /* пересчёт из остатка */
    re_anchor();

    printf("G%d weight -> %lld, W_total -> %lld\n", g, w, W_total);
    print_state();
}

int main(int argc, char** argv) {
    long long w[N_GROUPS] = {10, 10, 10, 10};
    for (int i = 0; i < N_GROUPS && i + 1 < argc; i++) w[i] = atoll(argv[i + 1]);
    for (int i = 0; i < N_GROUPS; i++) {
        G[i].weight = w[i];
        W_total += w[i];
        W_eff_total += w[i];
    }

    print_state();
    printf("\nEnter=step  N+/N-=weight  N=V=weight  q=quit\n");

    char line[128];
    while (1) {
        printf("> ");
        fflush(stdout);
        if (!fgets(line, sizeof(line), stdin)) break;
        line[strcspn(line, "\n")] = 0;

        if (!line[0]) {
            do_step();
            continue;
        }
        if (!strcmp(line, "q")) break;

        if (line[0] < '0' || line[0] > '9') {
            printf("unknown\n");
            continue;
        }
        int g = line[0] - '0';
        if (g >= N_GROUPS) {
            printf("out of range\n");
            continue;
        }

        char op = line[1];
        if (op == '+') {
            set_weight(g, G[g].weight + 1);
            continue;
        }
        if (op == '-') {
            set_weight(g, G[g].weight - 1);
            continue;
        }
        if (op == '=') {
            set_weight(g, atoll(&line[2]));
            continue;
        }
        printf("unknown\n");
    }
    return 0;
}