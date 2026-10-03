// main.c
#include "../include/drrd.h"
#include "../include/print.h"
#include "../include/stat.h"

int main(void) {
    int priorities[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    sim_init(10, priorities);

    // Несколько потоков в обеих группах
    mock_thread_t* a = sim_create_thread(1);
    mock_thread_t* b = sim_create_thread(1);
    mock_thread_t* c = sim_create_thread(2);
    mock_thread_t* d = sim_create_thread(2);

    (void)a;
    (void)b;
    (void)c;
    (void)d;

    sim_run(10);

    sim_create_thread(3);
    sim_create_thread(4);
    sim_create_thread(5);
    sim_create_thread(6);
    sim_create_thread(7);
    sim_create_thread(8);
    sim_create_thread(9);
    sim_create_thread(10);

    // Проверка блокировки и разблокировки
    // (если что-то пойдёт не так — увидим по Diff)
    sim_block_thread(b);
    sim_run(3);
    sim_unblock_thread(b);
    sim_run(3);

    sim_create_thread(3);
    sim_create_thread(4);
    sim_create_thread(5);
    sim_create_thread(6);
    sim_create_thread(7);
    sim_create_thread(8);
    sim_create_thread(9);
    sim_create_thread(10);

    // Удаление одного потока в середине
    sim_remove_thread(a);
    sim_run(5);

    sim_block_thread(b);
    sim_run(300);

    sim_cleanup();

    print_header();
    print_thread_stats();
    print_stats_dept();
    print_group_stats();
    print_diff_histogram();

    stat_cleanup();

    return 0;
}