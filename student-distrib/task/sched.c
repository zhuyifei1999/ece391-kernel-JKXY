#include "sched.h"
#include "../interrupt.h"
#include "../initcall.h"
#include "../panic.h"
#include "../lib.h"

struct list schedule_queue;

// Actually, this won't return, but jump directly to ISR return
static void schedule_handler(struct intr_info *info) {
    current->return_regs = info;
    struct task_struct *task = (struct task_struct *)info->eax;
    // TODO: For userspace, update TSS, page directory, flush TLB
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
        : "a"(task), "i"(INTR_SCHED)
        : "memory"
    );
}

void schedule(void) {
    // TODO: change to atomic variables
    if (current->state == TASK_RUNNING)
        list_insert_back(&schedule_queue, current);

    switch_to(list_pop_front(&schedule_queue));
}

void wake_up_process(struct task_struct *task) {
    if (task->state != TASK_RUNNING)
        panic("Can't schedule task %s at 0x%#x, task is not running\n",
              task->comm,
              (uint32_t)&task);

    list_insert_back(&schedule_queue, task);
}

static void init_sched() {
    list_init(&schedule_queue);

    intr_setaction(INTR_SCHED, (struct intr_action){
        .handler = &schedule_handler } );

    // TODO: Do periodic scheduling
}
DEFINE_INITCALL(init_sched, early);
