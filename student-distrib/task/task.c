#include "task.h"
#include "../err.h"
#include "../mm/paging.h"
#include "../eflags.h"
#include "../x86_desc.h"
#include "../lib.h"
#include "../panic.h"
#include "schedule.h"

struct task_struct *tasks[MAXPID];

static uint16_t _next_pid() {
    // TODO: change to atomic variables
    static uint16_t pid = 0;
    unsigned long flags;

    cli_and_save(flags);
    uint16_t ret = pid;
    pid++;
    if (pid > MAXPID)
        pid = LOOPPID;
    restore_flags(flags);

    return ret;
}

static uint16_t next_pid() {
    uint16_t pid;
    do {
        pid = _next_pid();
    } while (PTR_ERR(get_task_from_pid(pid)) != -ESRCH);
    return pid;
}

struct task_struct *get_task_from_pid(uint16_t pid) {
    if (pid > MAXPID || !tasks[pid])
        return ERR_PTR(-ESRCH);
    return tasks[pid];
}

void wake_up_process(struct task_struct *task) {
    task->state = TASK_RUNNING;
}

// TODO: When we get userspace, merge with generic clone()
struct task_struct *kernel_thread(int (*fn)(void *data), void *data) {
    void *stack = alloc_pages((1<<TASK_STACK_PAGES), TASK_STACK_PAGES, 0);
    if (!stack)
        return ERR_PTR(-ENOMEM);

    struct task_struct *task = task_from_stack(stack);
    *task = (struct task_struct) {
        .pid       = next_pid(),
        .ppid      = current->pid,
        .comm      = "kthread",
        .mm        = NULL,
        .state     = TASK_SLEEP,
        .subsystem = SUBSYSTEM_ECE391,
    };

    /* function prototype don't matter here */
    extern void (*entry_task)(void);
    struct intr_info *regs = (void *)((uint32_t)task - sizeof(struct intr_info));
    *regs = (struct intr_info){
        .eax    = (uint32_t)fn,
        .ebx    = (uint32_t)data,
        .eflags = EFLAGS_BASE | IF,
        .eip    = (uint32_t)&entry_task,
        .cs     = KERNEL_CS,
    };

    task->return_regs = regs;

    tasks[task->pid] = task;

    return task;
}

noreturn
void do_exit(int exitcode) {
    current->exitcode = exitcode;
    current->state = TASK_ZOMBIE;

    // TODO; SIGNAL parent so they can "mourn us"
    // TODO: reparent children
    schedule();

    panic("Dead process scheduled.\n");
}

asmlinkage noreturn
void kthread_execute(int (*fn)(void *data), void *data) {
    do_exit((*fn)(data));
}
