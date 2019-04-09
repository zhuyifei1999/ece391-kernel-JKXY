#include "sched.h"
#include "exit.h"
#include "../x86_desc.h"
#include "../mm/paging.h"
#include "../interrupt.h"
#include "../initcall.h"
#include "../panic.h"

struct list schedule_queue;

static uint32_t schedule_rtc_counter;

// Actually, this won't return, but jump directly to ISR return
static void _switch_to(struct task_struct *task, struct intr_info *info) {
    schedule_rtc_counter = 0;
    current->return_regs = info;

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
    extern struct task_struct *swapper_task;

    if (current != swapper_task && current->state == TASK_RUNNING)
        list_insert_back(&schedule_queue, current);

    if (list_isempty(&schedule_queue)) {
        switch_to(swapper_task);
    } else
        switch_to(list_pop_front(&schedule_queue));

    // we are safe to clean up whatever task that needs clean up here
    do_free_tasks();
}

void cond_schedule(void) {
    if (schedule_rtc_counter < SCHEDULE_TICK)
        return;
    schedule();
}

void rtc_schedule(struct intr_info *info) {
    schedule_rtc_counter++;
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
}
DEFINE_INITCALL(init_sched, early);
