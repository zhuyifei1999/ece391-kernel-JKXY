#include "signal.h"
#include "sched.h"
#include "task.h"
#include "exit.h"

enum default_action {
    SIG_IGNORE,
    SIG_KILL,
};

static enum default_action default_sig_action(uint16_t signum) {
    // We just implement these
    switch (signum) {
    case SIGHUP:
    case SIGINT:
    case SIGILL:
    case SIGABRT:
    case SIGBUS:
    case SIGKILL:
    case SIGSEGV:
    case SIGPIPE:
    case SIGTERM:
        return SIG_KILL;
    default:
        return SIG_IGNORE;
    }
}

static inline bool is_fatal_signal(uint16_t signum) {
    return signum == SIGKILL;
}

bool signal_pending(struct task_struct *task) {
    return task->sigpending.is_pending;
}

bool fatal_signal_pending(struct task_struct *task) {
    // We assume all forced signals are fatal
    return task->sigpending.is_pending && (task->sigpending.forced || is_fatal_signal(task->sigpending.signum));
}

void send_sig(struct task_struct *task, uint16_t signum) {
    if (signal_pending(task))
        return;
    struct sigaction *sigaction = &task->sigactions->sigactions[signum];
    if (sigaction->action == SIG_IGN)
        return;

    task->sigpending = (struct sigpending){
        .is_pending = true,
        .signum     = signum,
        .forced     = false,
    };

    if (task->state == TASK_INTERRUPTIBLE)
        wake_up_process(task);
}

void force_sig(struct task_struct *task, uint16_t signum) {
    if (fatal_signal_pending(task))
        return;

    task->sigpending = (struct sigpending){
        .is_pending = true,
        .signum     = signum,
        .forced     = true,
    };

    if (task->state == TASK_INTERRUPTIBLE)
        wake_up_process(task);
}

void kernel_mask_signal(uint16_t signum) {
    current->sigactions->sigactions[signum].action = SIG_IGN;
}

void kernel_unmask_signal(uint16_t signum) {
    current->sigactions->sigactions[signum].action = SIG_DFL;
}

uint16_t kernel_peek_pending_sig() {
    if (!current->sigpending.is_pending)
        return 0;
    return current->sigpending.signum;
}

uint16_t kernel_get_pending_sig() {
    uint16_t ret = kernel_peek_pending_sig();
    current->sigpending.is_pending = false;
    return ret;
}

void deliver_signal(struct intr_info *intr_info) {
    if (!signal_pending(current))
        return;

    uint16_t signum = current->sigpending.signum;
    struct sigaction *sigaction = &current->sigactions->sigactions[signum];
    if (sigaction->action == SIG_IGN)
        return;
    if (current->sigpending.forced || sigaction->action == SIG_DFL) {
        switch (default_sig_action(signum)) {
        case SIG_IGNORE:
            return;
        case SIG_KILL:
            switch (current->subsystem) {
            case SUBSYSTEM_LINUX:
                do_exit(127 + signum);
            case SUBSYSTEM_ECE391:
                do_exit(256);
            }
        }
    }

    // TODO:
}
