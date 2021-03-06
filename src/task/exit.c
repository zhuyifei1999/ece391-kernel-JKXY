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
    if (current->cwd)
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
            array_destroy(&current->files->cloexec);
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

    if (current->fxsave_data)
        kfree(current->fxsave_data);

    uint32_t i;
    array_for_each(&current->sigpending.siginfos, i) {
        struct siginfo *siginfo = array_get(&current->sigpending.siginfos, i);
        if (siginfo)
            kfree(siginfo);
    }
    array_destroy(&current->sigpending.siginfos);

    // if parent process id is 0, this is a child reaper
    if (!current->ppid)
        panic("Killing process tree! exitcode=%d\n", exitcode);

    // Signal parent so it can mourn us
    struct task_struct *parent = get_task_from_pid(current->ppid);
    if (parent->sigactions->sigactions[SIGCHLD].sigaction == SIG_IGN) {
        _do_wait(current);
    } else {
        struct siginfo siginfo = {
            .signo = SIGCHLD,
            .code = CLD_EXITED,
            .sifields.sigchld = {
                .pid = current->pid,
                .status = exitcode,
            },
        };
        send_sig_info(parent, &siginfo);
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
// check the range of status
DEFINE_SYSCALL1(LINUX, exit, int, status) {
    if (status < 0 || status > 255)
        status = 255;
    do_exit(status << 8);
}
// We don't yet support thread groups, silence the warnng
DEFINE_SYSCALL0(LINUX, exit_group) {
    return -ENOSYS;
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
        .sigaction = SIG_DFL,
    };

    // wait for the child process to end
    while (true) {
        current->state = TASK_INTERRUPTIBLE;
        schedule();
        current->state = TASK_RUNNING;

        if (signal_pending(current)) {
            struct siginfo siginfo;
            // return exitcode when SIGCHILD is received
            if (kernel_peek_pending_sig(SIGCHLD, &siginfo)) {
                if (siginfo.code == CLD_EXITED && siginfo.sifields.sigchld.pid == task->pid) {
                    kernel_get_pending_sig(SIGCHLD, &siginfo);
                    current->sigactions->sigactions[SIGCHLD] = oldaction;
                    return _do_wait(task);
                }
            }
            current->sigactions->sigactions[SIGCHLD] = oldaction;
            return -EINTR;
        }
    }

    current->sigactions->sigactions[SIGCHLD] = oldaction;

    return ret;
}

int32_t do_waitpg(uint32_t pgid, uint16_t *pid, bool wait) {
    bool haschild = false;

    struct list_node *node;
    list_for_each(&tasks, node) {
        struct task_struct *task = node->value;
        if (task->ppid == current->pid && (!pgid || task->pgid == pgid)) {
            haschild = true;
            if (task->state == TASK_ZOMBIE) {
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

        if (signal_pending(current)) {
            struct siginfo siginfo;
            if (kernel_peek_pending_sig(SIGCHLD, &siginfo)) {
                if (siginfo.code == CLD_EXITED) {
                    struct task_struct *task = get_task_from_pid(siginfo.sifields.sigchld.pid);
                    if (!pgid || task->pgid == pgid) {
                        kernel_get_pending_sig(SIGCHLD, &siginfo);
                        current->sigactions->sigactions[SIGCHLD] = oldaction;
                        *pid = task->pid;
                        return _do_wait(task);
                    }
                }
            }
            current->sigactions->sigactions[SIGCHLD] = oldaction;
            return -EINTR;
        }
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

int32_t do_sys_waitpid(int32_t pid, int *wstatus, int options) {
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

DEFINE_SYSCALL3(LINUX, waitpid, int32_t, pid, int *, wstatus, int, options) {
    return do_sys_waitpid(pid, wstatus, options);
}

DEFINE_SYSCALL4(LINUX, wait4, int32_t, pid, int *, wstatus, int, options, void *, rusage) {
    return do_sys_waitpid(pid, wstatus, options);
}
