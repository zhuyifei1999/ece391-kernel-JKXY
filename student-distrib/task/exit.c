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
    // set task state to TASK_DEAD
    task->state = TASK_DEAD;

    // remove task from task list_node
    list_remove(&tasks, task);
    // insert task to free_tasks list
    list_insert_back(&free_tasks, task);

    // return the exitcode of the process
    return task->exitcode;
}

noreturn
void do_exit(int exitcode) {
    // place exitcode into current
    current->exitcode = exitcode;
    // set the state of the current process to TASK_ZOMBIE
    current->state = TASK_ZOMBIE;

    // HACK: This is a userspace-mapped kernel memory. Attempting to free the
    // page like a normal user page will cause a kernel panic. This must be
    // done before attempting to free the page directory.
    exit_vidmap_cb();

    // close current working directory
    filp_close(current->cwd);
    if (current->exe)
        filp_close(current->exe);
    // close the session
    if (current->session)
        put_session();

    // close opened files and free them
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

    // free memory management information
    if (current->mm) {
        if (!atomic_dec(&current->mm->refcount)) {
            free_directory(current->mm->page_directory);
            kfree(current->mm);
        }
    }

    // free signal handler information
    if (!atomic_dec(&current->sigactions->refcount))
        kfree(current->sigactions);

    // if parent process id is 0, this is a child reaper
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
// check the range of status
DEFINE_SYSCALL1(LINUX, exit, int, status) {
    if (status < 0 || status > 255)
        status = 255;
    do_exit(status);
}

// Reap a child process, return its exitcode
int32_t do_wait(struct task_struct *task) {
    // check whether the parent
    if (task->ppid != current->pid)
        return -ECHILD;

    // if state is TASK_ZOMBIE, call _do_wait
    if (task->state == TASK_ZOMBIE)
        return _do_wait(task);

    int32_t ret;

    struct sigaction oldaction = current->sigactions->sigactions[SIGCHLD];

    current->sigactions->sigactions[SIGCHLD] = (struct sigaction){
        .action = SIG_DFL,
    };

    // wait for the child process to end
    while (true) {
        current->state = TASK_INTERRUPTIBLE;
        schedule();
        current->state = TASK_RUNNING;

        uint16_t signal = kernel_peek_pending_sig();
        if (signal) {
            // return existcode when SIGCHILD is received
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
