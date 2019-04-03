#include "sched.h"
#include "../x86_desc.h"
#include "../mm/paging.h"
#include "../interrupt.h"
#include "../initcall.h"
#include "../panic.h"

struct list schedule_queue;

// Actually, this won't return, but jump directly to ISR return
static void schedule_handler(struct intr_info *info) {
    current->return_regs = info;
    struct task_struct *task = (struct task_struct *)info->eax;
    if (task->mm) // this task has userspace, update page directory
        switch_directory(task->mm->page_directory);

    tss.ss0 = KERNEL_DS;
    tss.esp0 = (uint32_t)task + TASK_STACK_PAGES * PAGE_SIZE_SMALL;
    set_all_regs(task->return_regs);
}

static void switch_to(struct task_struct *task) {
    // printf("Switching to task %#x", (uint32_t)task);
    // printf(" Comm: %s\n", task->comm);
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
    if (current->state == TASK_RUNNING)
        list_insert_back(&schedule_queue, current);

    switch_to(list_pop_front(&schedule_queue));

    // we are safe to clean up whatever task that needs clean up here
    do_free_tasks();
}

void wake_up_process(struct task_struct *task) {
    if (task == current)
        return; // can't wake up self

    unsigned long flags;
    cli_and_save(flags);
    if (!list_contains(&schedule_queue, task))
        list_insert_back(&schedule_queue, task);
    restore_flags(flags);
}

static void init_sched() {
    list_init(&schedule_queue);

    intr_setaction(INTR_SCHED, (struct intr_action){
        .handler = &schedule_handler } );

    // TODO: Do periodic scheduling
}
DEFINE_INITCALL(init_sched, early);
