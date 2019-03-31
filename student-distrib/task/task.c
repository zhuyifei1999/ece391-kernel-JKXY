#include "task.h"
#include "kthread.h"
#include "../mm/paging.h"
#include "../mm/kmalloc.h"
#include "../lib/cli.h"
#include "../panic.h"
#include "../initcall.h"
#include "../err.h"
#include "sched.h"

// struct task_struct *tasks[MAXPID];
struct list tasks[PID_BUCKETS];
static struct list free_tasks;

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

noreturn
void do_exit(int exitcode) {
    current->exitcode = exitcode;
    current->state = TASK_ZOMBIE;

    filp_close(current->cwd);
    if (current->exe)
        filp_close(current->exe);

    if (current->files) {
        if (!atomic_dec(&current->files->refcount)) {
            uint32_t i;
            array_for_each(&current->files->files, i) {
                struct file *file = array_get(&current->files->files, i);
                if (file)
                    filp_close(file);
            }
            array_destroy(&current->files->files);
            kfree(current->files);
        }
    }

    if (current->mm) {
        if (!atomic_dec(&current->mm->refcount)) {
            free_directory(current->mm->page_directory);
            kfree(current->mm);
        }
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

static void init_tasks() {
    int i;
    for (i = 0; i < PID_BUCKETS; i++) {
        list_init(&tasks[i]);
    }

    list_init(&free_tasks);
}
DEFINE_INITCALL(init_tasks, early);
