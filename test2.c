#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "mtk_c.h"

// 外部関数（タスク管理システムコール）のプロトタイプ宣言
extern void set_task(void (*func)());
extern void begin_sch();


void task1() {
    while(true)
    {
        for (int i = 0; i < NUMSEMAPHORE; i++) {
        	printf("task1: semaphore[%d].count = %d\n", i, semaphore[i].count);
        	printf("task1: semaphore[%d].nst = %d\n", i, semaphore[i].nst);
        	printf("task1: semaphore[%d].task_list = %d\n", i, semaphore[i].task_list);
        }
        	printf("task1: ready = %d\n", ready);
        	printf("task1: curr_task = %d\n", curr_task);
        	printf("task1: new_task = %d\n", new_task);
        	printf("task1: next_task = %d\n", next_task);
    }
}


void task2() {
    while(true){
        for (int i = 0; i < NUMSEMAPHORE; i++) {
        	printf("task2: semaphore[%d].count = %d\n", i, semaphore[i].count);
        	printf("task2: semaphore[%d].nst = %d\n", i, semaphore[i].nst);
        	printf("task2: semaphore[%d].task_list = %d\n", i, semaphore[i].task_list);
        }
        	printf("task2: ready = %d\n", ready);
        	printf("task2: curr_task = %d\n", curr_task);
        	printf("task2: new_task = %d\n", new_task);
        	printf("task2: next_task = %d\n", next_task);
    }
}




int main() {
    *(char*)0x00d00039='a';
    init_kernel();
    set_task(task1);
    set_task(task2);
    begin_sch();
    return 0;
}
