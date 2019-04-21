#include "exit.h"
#include "sched.h"
#include "session.h"
#include "signal.h"
#include "../char/tty.h"
#include "../syscall.h"
#include "../panic.h"
#include "../mm/paging.h"
#include "../mm/kmalloc.h"
#include "../syscall.h"
#include "../err.h"
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
            array_destroy(&current->files->cloexec);
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
    if (parent->sigactions->sigactions[SIGCHLD].sigaction == SIG_IGN) {
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
    do_exit(status & 0377);
}
DEFINE_SYSCALL1(LINUX, exit, int, status) {
    if (status < 0 || status > 255)
        status = 255;
    do_exit(status);
}
// We don't yet support thread groups, silence the warnng
DEFINE_SYSCALL0(LINUX, exit_group) {
    return -ENOSYS;
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
        .sigaction = SIG_DFL,
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

int32_t do_waitpg(uint32_t pgid, uint16_t *pid, bool wait) {
    while (true) {
        bool haschild = false;

        struct list_node *node;
        list_for_each(&tasks, node) {
            struct task_struct *task = node->value;
            if (task->ppid == current->pid && (!pgid || task->pgid == pgid)) {
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

        if (!wait) {
            *pid = 0;
            return 0;
        }

        struct sigaction oldaction = current->sigactions->sigactions[SIGCHLD];

        current->sigactions->sigactions[SIGCHLD] = (struct sigaction){
            .sigaction = SIG_DFL,
        };

        while (true) {
            current->state = TASK_INTERRUPTIBLE;
            schedule();
            current->state = TASK_RUNNING;

            uint16_t signal = kernel_peek_pending_sig();
            if (signal) {
                if (signal == SIGCHLD) {
                    // TODO: If the signal source is not the corresponding PG this should -EINTR
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

// source: <uapi/linux/wait.h>
#define WNOHANG   0x00000001
#define WUNTRACED 0x00000002
#define WSTOPPED  WUNTRACED

// FIXME: wstatus is not exitcode
DEFINE_SYSCALL3(LINUX, waitpid, int32_t, pid, int *, wstatus, int, options) {
    int32_t exitcode;
    if (pid < 1) {
        uint16_t pgid;
        if (pid < -1)
            pgid = -pid;
        else if (pid == -1)
            pgid = 0;
        else // pid == 0
            pgid = current->pgid;
        uint16_t pid_k;
        exitcode = do_waitpg(pgid, &pid_k, !(options & WNOHANG));
        pid = pid_k;
    } else {
        struct task_struct *task = get_task_from_pid(pid);
        if (IS_ERR(task))
            return PTR_ERR(task);

        if (task->state != TASK_ZOMBIE && (options & WNOHANG))
            return 0;
        exitcode = do_wait(task);
    }

    if (exitcode < 0)
        return exitcode;

    if (safe_buf(wstatus, sizeof(*wstatus), true) == sizeof(*wstatus))
        *wstatus = exitcode;

    return pid;
}
