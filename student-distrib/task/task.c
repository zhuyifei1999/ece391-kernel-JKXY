#include "task.h"
#include "kthread.h"
#include "../mm/paging.h"
#include "../eflags.h"
#include "../x86_desc.h"
#include "../lib/cli.h"
#include "../panic.h"
#include "../initcall.h"
#include "../err.h"
#include "sched.h"

// struct task_struct *tasks[MAXPID];
struct list tasks[PID_BUCKETS];
static struct list free_tasks;

static uint16_t _next_pid() {
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
    if (pid > MAXPID)
        return ERR_PTR(-ESRCH);
    struct list_node *node;
    list_for_each(&tasks[pid & (PID_BUCKETS - 1)], node) {
        struct task_struct *task = node->value;
        if (task->pid == pid)
            return task;
    }
    return ERR_PTR(-ESRCH);
}

// TODO: When we get userspace, merge with generic clone()
struct task_struct *kernel_thread(int (*fn)(void *args), void *args) {
    do_free_tasks();

    struct task_struct *task = alloc_pages(TASK_STACK_PAGES, TASK_STACK_PAGES_POW, 0);
    if (!task)
        return ERR_PTR(-ENOMEM);

    if (is_boot_context()) {
        // boot context, fill with dummies, cwd will be initialized by initial thread
        *task = (struct task_struct){
            .pid       = next_pid(),
            .ppid      = 0,
            .comm      = "init_task",
            .mm        = NULL,
            .state     = TASK_RUNNING,
            .subsystem = SUBSYSTEM_LINUX,
            .cwd       = NULL,
            .exe       = NULL,
        };
    } else {
        atomic_inc(&current->cwd->refcount);
        *task = (struct task_struct){
            .pid       = next_pid(),
            .ppid      = current->pid,
            .comm      = "kthread",
            .mm        = NULL,
            .state     = TASK_RUNNING,
            .subsystem = SUBSYSTEM_LINUX,
            .cwd       = current->cwd,
            .exe       = NULL,
        };
    }

    // The position of this struct intr_info controls where the stack is going
    // to end up, not any of the contents in the struct, bacause we are using
    // the same code segment.
    struct intr_info *regs = (void *)((uint32_t)task +
        TASK_STACK_PAGES * PAGE_SIZE_SMALL - sizeof(struct intr_info));
    // function prototype don't matter here
    extern void (*entry_task)(void);
    *regs = (struct intr_info){
        .eax    = (uint32_t)fn,
        .ebx    = (uint32_t)args,
        .eflags = EFLAGS_BASE | IF,
        .eip    = (uint32_t)&entry_task,
        .cs     = KERNEL_CS,
    };

    task->return_regs = regs;

    list_insert_back(&tasks[task->pid & (PID_BUCKETS - 1)], task);

    return task;
}

noreturn
void do_exit(int exitcode) {
    current->exitcode = exitcode;
    current->state = TASK_ZOMBIE;

    filp_close(current->cwd);
    if (current->exe)
        filp_close(current->exe);

    uint32_t i;
    array_for_each(&current->files.files, i) {
        struct file *file = array_get(&current->files.files, i);
        if (file)
            filp_close(file);
    }

    struct task_struct *parent = get_task_from_pid(current->ppid);
    // if the parent is kthreadd then just auto reap it.
    // TODO: read POSIX and determine based on signals
    if (parent == kthreadd_task) {
        do_wait(current);
    }
    // TODO; SIGNAL parent so they can "mourn us"
    wake_up_process(parent);
    // TODO: reparent children
    schedule();

    panic("Dead process scheduled.\n");
}

// Reap a child process, return its exitcode
int do_wait(struct task_struct *task) {
    if (current != task) {
        current->state = TASK_INTERRUPTIBLE;
        while (task->state != TASK_ZOMBIE)
            schedule();
        current->state = TASK_RUNNING;
    }

    task->state = TASK_DEAD;

    uint16_t pid = task->pid;
    list_remove(&tasks[pid & (PID_BUCKETS - 1)], task);

    list_insert_back(&free_tasks, task);

    return task->exitcode;
}

void do_free_tasks() {
    while (!list_isempty(&free_tasks)) {
        struct task_struct *task = list_pop_front(&free_tasks);
        free_pages(task, TASK_STACK_PAGES, 0);
    }
}

asmlinkage noreturn
void kthread_execute(int (*fn)(void *data), void *data) {
    do_exit((*fn)(data));
}

static void init_tasks() {
    int i;
    for (i = 0; i < PID_BUCKETS; i++) {
        list_init(&tasks[i]);
    }

    list_init(&free_tasks);
}
DEFINE_INITCALL(init_tasks, early);
