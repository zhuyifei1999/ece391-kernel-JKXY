#include "exit.h"
#include "sched.h"
#include "session.h"
#include "signal.h"
#include "../char/tty.h"
#include "../syscall.h"
#include "../panic.h"
#include "../mm/paging.h"
#include "../mm/kmalloc.h"
#include "../errno.h"

static struct list free_tasks;
LIST_STATIC_INIT(free_tasks);

static int _do_wait(struct task_struct *task) {
    task->state = TASK_DEAD;

    list_remove(&tasks, task);
    list_insert_back(&free_tasks, task);

    return task->exitcode;
}

noreturn
void do_exit(int exitcode) {
    current->exitcode = exitcode;
    current->state = TASK_ZOMBIE;

    // HACK
    exit_vidmap_cb();

    if (current->cwd)
        filp_close(current->cwd);
    if (current->exe)
        filp_close(current->exe);
    if (current->session)
        put_session();

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

    if (!atomic_dec(&current->sigactions->refcount))
        kfree(current->sigactions);

    if (current->fxsave_data)
        kfree(current->fxsave_data);

    if (!current->ppid)
        panic("Killing process tree!\n");

    // Signal parent so it can mourn us
    struct task_struct *parent = get_task_from_pid(current->ppid);
    if (parent->sigactions->sigactions[SIGCHLD].action == SIG_IGN) {
        _do_wait(current);
    } else {
        send_sig(parent, SIGCHLD);
    }

    // Reparent children to init
    struct list_node *node;
    list_for_each(&tasks, node) {
        struct task_struct *task = node->value;
        if (task->ppid == current->pid)
            task->ppid = 1;
    }

    schedule();

    BUG();
}

DEFINE_SYSCALL1(ECE391, halt, uint8_t, status) {
    do_exit(status);
}
DEFINE_SYSCALL1(LINUX, exit, int, status) {
    if (status < 0 || status > 255)
        status = 255;
    do_exit(status);
}

// Reap a child process, return its exitcode
int32_t do_wait(struct task_struct *task) {
    if (task->ppid != current->pid)
        return -ECHILD;

    if (task->state == TASK_ZOMBIE)
        return _do_wait(task);

    int32_t ret;

    struct sigaction oldaction = current->sigactions->sigactions[SIGCHLD];

    current->sigactions->sigactions[SIGCHLD] = (struct sigaction){
        .action = SIG_DFL,
    };

    while (true) {
        current->state = TASK_INTERRUPTIBLE;
        schedule();
        current->state = TASK_RUNNING;

        uint16_t signal = kernel_peek_pending_sig();
        if (signal) {
            if (signal == SIGCHLD && task->state == TASK_ZOMBIE) {
                kernel_get_pending_sig();
                ret = _do_wait(task);
            } else {
                ret = -EINTR;
            }
            break;
        }
    }

    current->sigactions->sigactions[SIGCHLD] = oldaction;

    return ret;
}

int32_t do_waitall(uint16_t *pid) {
    while (true) {
        bool haschild = false;

        struct list_node *node;
        list_for_each(&tasks, node) {
            struct task_struct *task = node->value;
            if (task->ppid == current->pid) {
                haschild = true;
                if (task->state == TASK_ZOMBIE) {
                    if (pid)
                        *pid = task->pid;
                    return _do_wait(task);
                }
            }
        }

        if (!haschild)
            return -ECHILD;

        struct sigaction oldaction = current->sigactions->sigactions[SIGCHLD];

        current->sigactions->sigactions[SIGCHLD] = (struct sigaction){
            .action = SIG_DFL,
        };

        while (true) {
            current->state = TASK_INTERRUPTIBLE;
            schedule();
            current->state = TASK_RUNNING;

            uint16_t signal = kernel_peek_pending_sig();
            if (signal) {
                if (signal == SIGCHLD) {
                    kernel_get_pending_sig();
                    break;
                } else {
                    current->sigactions->sigactions[SIGCHLD] = oldaction;
                    return -EINTR;
                }
            }
        }

        current->sigactions->sigactions[SIGCHLD] = oldaction;
    }
}

void do_free_tasks() {
    while (!list_isempty(&free_tasks)) {
        struct task_struct *task = list_pop_front(&free_tasks);
        free_pages(task, TASK_STACK_PAGES, 0);
    }
}
