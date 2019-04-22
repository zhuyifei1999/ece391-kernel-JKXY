#include "sched.h"
#include "exit.h"
#include "../x86_desc.h"
#include "../mm/paging.h"
#include "../interrupt.h"
#include "../initcall.h"
#include "../panic.h"

// queue for schedule
struct list schedule_queue;
LIST_STATIC_INIT(schedule_queue);

// counter for pit
static uint32_t schedule_pit_counter;

// Actually, this won't return, but jump directly to ISR return
static void _switch_to(struct task_struct *task, struct intr_info *info) {
    schedule_pit_counter = 0;
    // store into into current's return registers
    current->return_regs = info;

    if (task->mm) // this task has userspace, update page directory
        switch_directory(task->mm->page_directory);
    // set ss0
    tss.ss0 = KERNEL_DS;
    // set esp0
    tss.esp0 = (uint32_t)task + TASK_STACK_PAGES * PAGE_SIZE_SMALL;
    // pop all registers
    set_all_regs(task->return_regs);
}

// schedule's interrupt handler
static void schedule_handler(struct intr_info *info) {
    struct task_struct *task = (struct task_struct *)info->eax;
    _switch_to(task, info);
}

static void switch_to(struct task_struct *task) {
    // #include "../printk.h"
    // printk("Switching to task %p\n", task);
    // printk(" Comm: %s\n", task->comm);
    // return if current task is task
    if (task == current)
        return;
    // call interrupt
    asm volatile (
        "int %1;"
        :
        : "a"(task), "i"(INTR_SCHED)
        : "memory"
    );
}

void schedule(void) {
    extern struct task_struct *swapper_task;

    // place current at the end of the schedule queue if it is not swapper_task
    if (current != swapper_task && current->state == TASK_RUNNING)
        list_insert_back(&schedule_queue, current);

    // if the schedule queue is empty, switch to the swapper_task
    if (list_isempty(&schedule_queue)) {
        switch_to(swapper_task);
    } else
        // switch to the first process in the schedule queue
        switch_to(list_pop_front(&schedule_queue));

    // we are safe to clean up whatever task that needs clean up here
    do_free_tasks();
}

void cond_schedule(void) {
    // schedule when counter reaches threshold
    if (schedule_pit_counter < SCHEDULE_TICK)
        return;
    schedule();
}

// increment pit counter
void pit_schedule(struct intr_info *info) {
    schedule_pit_counter++;
}

void wake_up_process(struct task_struct *task) {
    if (task == current)
        return; // can't wake up self

    unsigned long flags;
    cli_and_save(flags);
    // place task at the end of the schedule queue if it is not in the queue
    if (!list_contains(&schedule_queue, task))
        list_insert_back(&schedule_queue, task);
    restore_flags(flags);
}

// initialize scheduler
static void init_sched() {
    intr_setaction(INTR_SCHED, (struct intr_action){
        .handler = &schedule_handler } );
}
DEFINE_INITCALL(init_sched, early);
