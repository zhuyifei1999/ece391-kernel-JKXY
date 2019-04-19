#include "signal.h"
#include "sched.h"
#include "task.h"
#include "exit.h"
#include "userstack.h"
#include "../syscall.h"
#include "../err.h"

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
    if (sigaction->sigaction == SIG_IGN)
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

int32_t send_sig_pg(uint16_t pgid, uint16_t signum) {
    struct task_struct *leader = get_task_from_pid(pgid);
    if (IS_ERR(leader))
        return PTR_ERR(leader);

    if (leader->pgid != pgid) {
        // Not a leader
        send_sig(leader, signum);
        return 0;
    }

    struct list_node *node;
    list_for_each(&tasks, node) {
        struct task_struct *task = node->value;
        if (task->pgid == pgid)
            send_sig(task, signum);
    }

    return 0;
}

void kernel_mask_signal(uint16_t signum) {
    current->sigactions->sigactions[signum].sigaction = SIG_IGN;
}

void kernel_unmask_signal(uint16_t signum) {
    current->sigactions->sigactions[signum].sigaction = SIG_DFL;
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

struct ece391_user_context {
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
    uint32_t esi;
    uint32_t edi;
    uint32_t ebp;
    uint32_t eax;
    uint16_t ds __attribute__((aligned(4)));
    uint16_t es __attribute__((aligned(4)));
    uint16_t fs __attribute__((aligned(4)));
    uint32_t intr_num;
    uint32_t error_code;
    uint32_t eip;
    uint16_t cs __attribute__((aligned(4)));
    uint32_t eflags;
    uint32_t esp;
    uint16_t ss __attribute__((aligned(4)));
};

void deliver_signal(struct intr_info *regs) {
    if (!signal_pending(current))
        return;

    struct sigpending sigpending = current->sigpending;
    current->sigpending.is_pending = false;

    uint16_t signum = sigpending.signum;
    struct sigaction *sigaction = &current->sigactions->sigactions[signum];
    if (sigaction->sigaction == SIG_IGN)
        return;
    if (sigpending.forced || sigaction->sigaction == SIG_DFL) {
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

    switch (current->subsystem) {
    case SUBSYSTEM_LINUX:
        goto force_segv;  // TODO:
    case SUBSYSTEM_ECE391: {
        // function prototype don't matter here
        extern uint8_t ECE391_sigret_start;
        extern uint8_t ECE391_sigret_stop;

        uint32_t saved_esp = regs->esp;

        if (push_userstack(regs, &ECE391_sigret_start, &ECE391_sigret_stop - &ECE391_sigret_start) < 0)
            goto force_segv;
        uint32_t returnaddr = regs->esp;

        struct ece391_user_context context = {
            .ebx    = regs->ebx,
            .ecx    = regs->ecx,
            .edx    = regs->edx,
            .esi    = regs->esi,
            .edi    = regs->edi,
            .ebp    = regs->ebp,
            .eax    = regs->eax,
            .ds     = regs->ds,
            .es     = regs->es,
            .fs     = regs->fs,
            .eip    = regs->eip,
            .cs     = regs->cs, // DO NOT restore this
            .eflags = regs->eflags,
            .esp    = saved_esp,
            .ss     = regs->ss,
        };

        if (push_userstack(regs, &context, sizeof(context)) < 0)
            goto force_segv;

        uint32_t ece391_signum;
        switch (signum) {
        case SIGFPE:
            ece391_signum = SIG_ECE391_DIV_ZERO;
            break;
        case SIGSEGV:
            ece391_signum = SIG_ECE391_SEGFAULT;
            break;
        case SIGINT:
            ece391_signum = SIG_ECE391_INTERRUPT;
            break;
        case SIGALRM:
            ece391_signum = SIG_ECE391_ALARM;
            break;
        case SIGUSR1:
            ece391_signum = SIG_ECE391_USER1;
            break;
        default:
            goto force_segv; // how is this possible?
        }

        if (push_userstack(regs, &ece391_signum, sizeof(ece391_signum)) < 0)
            goto force_segv;

        if (push_userstack(regs, &returnaddr, sizeof(returnaddr)) < 0)
            goto force_segv;


        regs->eip = (uint32_t)sigaction->sigaction;
        return;
    }
    }

force_segv:
    force_sig(current, SIGSEGV);
    deliver_signal(regs);
}

struct linux_sigaction {
    void     (*sa_sigaction)(int, struct siginfo *, void *);
    unsigned long   sa_mask;
    int        sa_flags;
    void     (*sa_restorer)(void);
};

DEFINE_SYSCALL2(ECE391, set_handler, int32_t, signum, void *, handler_address) {
    switch (signum) {
    case SIG_ECE391_DIV_ZERO:
        signum = SIGFPE;
        break;
    case SIG_ECE391_SEGFAULT:
        signum = SIGSEGV;
        break;
    case SIG_ECE391_INTERRUPT:
        signum = SIGINT;
        break;
    case SIG_ECE391_ALARM:
        signum = SIGALRM;
        break;
    case SIG_ECE391_USER1:
        signum = SIGUSR1;
        break;
    default:
        return -EINVAL;
    }

    current->sigactions->sigactions[signum].sigaction = handler_address;
    return 0;
}

DEFINE_SYSCALL3(LINUX, rt_sigaction, int, signum, const struct linux_sigaction *, act, struct linux_sigaction *, oldact) {
    if (signum <= 0 || signum >= _NSIG)
        return -EINVAL;
    if (signum == SIGKILL || signum == SIGSTOP)
        return -EINVAL;

    if (safe_buf(act, sizeof(*act), false) != sizeof(*act))
        return -EFAULT;

    if (safe_buf(oldact, sizeof(*oldact), false) == sizeof(*oldact)) {
        *oldact = (struct linux_sigaction){
            .sa_sigaction = current->sigactions->sigactions[signum].sigaction,
            .sa_flags = current->sigactions->sigactions[signum].flags,
        };
    }

    current->sigactions->sigactions[signum].sigaction = act->sa_sigaction;
    current->sigactions->sigactions[signum].flags = act->sa_flags;

    return 0;
}

DEFINE_SYSCALL_COMPLEX(ECE391, sigreturn, regs) {
    extern uint8_t ECE391_sigret_start;
    extern uint8_t ECE391_sigret_postint;

    struct ece391_user_context *context = (void *)(regs->eip - (&ECE391_sigret_postint - &ECE391_sigret_start) - sizeof(struct ece391_user_context));
    if (safe_buf(context, sizeof(*context), false) != sizeof(*context)) {
        force_sig(current, SIGSEGV);
        return;
    }

    regs->ebx    = context->ebx;
    regs->ecx    = context->ecx;
    regs->edx    = context->edx;
    regs->esi    = context->esi;
    regs->edi    = context->edi;
    regs->ebp    = context->ebp;
    regs->eax    = context->eax;
    regs->ds     = context->ds;
    regs->es     = context->es;
    regs->fs     = context->fs;
    regs->eip    = context->eip;
    regs->eflags = context->eflags;
    regs->esp    = context->esp;
    regs->ss     = context->ss;

    // FIXME: Bad regs here can cause a panic

    return;
}
