#include "sched.h"
#include "exit.h"
#include "fp.h"
#include "../lib/cli.h"
#include "../mm/paging.h"
#include "../x86_desc.h"
#include "../interrupt.h"
#include "../initcall.h"
#include "../panic.h"

struct list schedule_queue;
LIST_STATIC_INIT(schedule_queue);

static uint32_t schedule_pit_counter;

// Actually, this won't return, but jump directly to ISR return
static void _switch_to(struct task_struct *task, struct intr_info *info) {
    schedule_pit_counter = 0;
    current->return_regs = info;
    sched_fxsave();

    if (task->mm) // this task has userspace, update page directory
        switch_directory(task->mm->page_directory);

    tss.ss0 = KERNEL_DS;
    tss.esp0 = (uint32_t)task + TASK_STACK_PAGES * PAGE_SIZE_SMALL;
    set_all_regs(task->return_regs);
}

static void schedule_handler(struct intr_info *info) {
    struct task_struct *task = (struct task_struct *)info->eax;
    _switch_to(task, info);
}

static void switch_to(struct task_struct *task) {
    // #include "../printk.h"
    // printk("Switching to task %p\n", task);
    // printk(" Comm: %s\n", task->comm);
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
    unsigned long flags;
    cli_and_save(flags);

    extern struct task_struct *swapper_task;
    if (
        current != swapper_task && !current->stopped && (
            current->state == TASK_RUNNING ||
            current->wakeup_current
        )
    ) {
        list_insert_back(&schedule_queue, current);
        current->wakeup_current = false;
    }

    if (list_isempty(&schedule_queue)) {
        switch_to(swapper_task);
    } else
        switch_to(list_pop_front(&schedule_queue));

    // we are safe to clean up whatever task that needs clean up here
    do_free_tasks();

    restore_flags(flags);
}

void cond_schedule(void) {
    if (schedule_pit_counter < SCHEDULE_TICK)
        return;
    schedule();
}

void pit_schedule(struct intr_info *info) {
    schedule_pit_counter++;
}

void wake_up_process(struct task_struct *task) {
    if (task == current) {
        // One case where this happens is interrupts. The task is added to
        // a structure accessible by an interrupt handler, who wakes the task
        // up before the task goes to sleep. We workaround this by letting
        // its next schedule() call 'wake up' itself as if it's running.
        current->wakeup_current = true;
        return;
    }

    unsigned long flags;
    cli_and_save(flags);
    if (!list_contains(&schedule_queue, task))
        list_insert_back(&schedule_queue, task);
    restore_flags(flags);
}

static void init_sched() {
    intr_setaction(INTR_SCHED, (struct intr_action){
        .handler = &schedule_handler } );
}
DEFINE_INITCALL(init_sched, early);
