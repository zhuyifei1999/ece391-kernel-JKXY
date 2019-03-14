#include "init_task.h"
#include "task/schedule.h"
#include "initcall.h"
#include "panic.h"
#include "tests.h"
#include "lib.h"

struct task_struct *init_task;

static int main(void *args) {
    init_task = current;
    strcpy(init_task->comm, "swapper");
    init_task->ppid = 0;

    // Initialize drivers
    DO_INITCALL(drivers);

#if RUN_TESTS
    /* Run tests */
    launch_tests();
#endif

    printf("init_task running with PID %d", current->pid);

    for (;;) {
        schedule();
        asm volatile ("hlt" : : : "memory");
    }
}

noreturn
void exec_init_task(void) {
    struct task_struct *task = kernel_thread(&init_task_fn, NULL);
    wake_up_process(task);
    schedule();
    // The scheduling will corrupt this stack. Boot context is unschedulable
    panic("Boot context reschduled.\n");
}
