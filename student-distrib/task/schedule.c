#include "schedule.h"
#include "../interrupt.h"
#include "../initcall.h"

// Actually, this won't return, but jump directly to ISR return
static void schedule_handler(struct intr_info *info) {
    current->return_regs = info;
    struct task_struct *task = (struct task_struct *)info->eax;
    asm volatile (
        "mov %0,%%esp;"
        "jmp ISR_return;"
        :
        : "rm"(task->return_regs)
    );
}

static void switch_to(struct task_struct *task) {
    if (task == current)
        return;
    asm volatile (
        "int %1;"
        :
        : "a"(task), "i"(INTR_SCHEDULER)
        : "memory"
    );
}

void schedule(void) {
    static int pid = 0;

    while (1) {
        struct task_struct *task = tasks[pid++];
        if (pid > MAXPID)
            pid = 0;

        if (task && task->state == TASK_RUNNING) {
            switch_to(task);
            break;
        }
    }
}

static void init_scheduler() {
    intr_setaction(INTR_SCHEDULER, (struct intr_action){
        .handler = &schedule_handler } );

    // TODO: Do periodic scheduling
}
DEFINE_INITCALL(init_scheduler, early);
