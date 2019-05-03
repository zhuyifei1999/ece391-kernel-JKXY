#include "signal.h"
#include "sched.h"
#include "task.h"
#include "exit.h"
#include "userstack.h"
#include "../lib/bsr.h"
#include "../mm/kmalloc.h"
#include "../panic.h"
#include "../syscall.h"
#include "../err.h"

enum default_action {
    SIG_IGNORE,
    SIG_KILL,
    SIG_STOP,
    SIG_CONT,
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
    case SIGSTOP:
    case SIGTSTP:
    case SIGTTIN:
    case SIGTTOU:
        return SIG_STOP;
    case SIGCONT:
        return SIG_CONT;
    default:
        return SIG_IGNORE;
    }
}

uint32_t fatal_signal_pending(struct task_struct *task) {
    // We assume all forced signals are fatal
    return (task->sigpending.pending_mask & (task->sigpending.forced_mask | SIG_UNMASKABLE));
}

uint32_t signal_pending(struct task_struct *task) {
    return fatal_signal_pending(task) | (task->sigpending.pending_mask & ~task->sigpending.blocked_mask);
}

uint16_t signal_pending_one(struct task_struct *task) {
    uint32_t mask;
    // priortize fatals
    mask = fatal_signal_pending(task);
    if (mask)
        return bsr(mask);
    mask = signal_pending(task);
    if (mask)
        return bsr(mask);
    return 0;
}

bool signal_is_fatal(struct task_struct *task, uint16_t signum) {
    return MASKVAL(signum) & (task->sigpending.forced_mask | SIG_UNMASKABLE);
}

void send_sig_info(struct task_struct *task, struct siginfo *siginfo) {
    if (task->sigpending.pending_mask & MASKVAL(siginfo->signo))
        return;
    struct sigaction *sigaction = &task->sigactions->sigactions[siginfo->signo];
    if (sigaction->sigaction == SIG_IGN)
        return;

    // special case SIGCONT
    if (siginfo->signo == SIGCONT) {
        if (task->stopped) {
            task->stopped = false;
            wake_up_process(task);
        }
        return;
    }

    task->sigpending.pending_mask |= MASKVAL(siginfo->signo);

    struct siginfo *siginfo_h = kmalloc(sizeof(*siginfo_h));
    *siginfo_h = *siginfo;

    array_set(&task->sigpending.siginfos, siginfo->signo, siginfo_h);

    if (task->state == TASK_INTERRUPTIBLE)
        wake_up_process(task);
}

void force_sig_info(struct task_struct *task, struct siginfo *siginfo) {
    send_sig_info(task, siginfo);

    task->sigpending.forced_mask |= MASKVAL(siginfo->signo);
}

void send_sig(struct task_struct *task, uint16_t signum) {
    struct siginfo siginfo = {
        .signo = signum,
        .code = SI_KERNEL,
    };
    send_sig_info(task, &siginfo);
}

void force_sig(struct task_struct *task, uint16_t signum) {
    struct siginfo siginfo = {
        .signo = signum,
        .code = SI_KERNEL,
    };
    force_sig_info(task, &siginfo);
}

int32_t send_sig_info_pg(uint16_t pgid, struct siginfo *siginfo) {
    struct task_struct *leader = get_task_from_pid(pgid);
    if (IS_ERR(leader))
        return PTR_ERR(leader);

    if (leader->pgid != pgid) {
        // Not a leader
        send_sig_info(leader, siginfo);
        return 0;
    }

    struct list_node *node;
    list_for_each(&tasks, node) {
        struct task_struct *task = node->value;
        if (task->pgid == pgid)
            send_sig_info(task, siginfo);
    }

    return 0;
}

void kernel_mask_signal(uint16_t signum) {
    current->sigactions->sigactions[signum].sigaction = SIG_IGN;
}

void kernel_unmask_signal(uint16_t signum) {
    current->sigactions->sigactions[signum].sigaction = SIG_DFL;
}

bool kernel_sig_ispending(uint16_t signum) {
    return current->sigpending.pending_mask & MASKVAL(signum);
}

bool kernel_peek_pending_sig(uint16_t signum, struct siginfo *siginfo) {
    if (!kernel_sig_ispending(signum))
        return false;

    if (siginfo)
        *siginfo = *(struct siginfo *)array_get(&current->sigpending.siginfos, signum);
    return true;
}

bool kernel_get_pending_sig(uint16_t signum, struct siginfo *siginfo) {
    bool ret = kernel_peek_pending_sig(signum, siginfo);
    current->sigpending.pending_mask &= ~MASKVAL(signum);
    current->sigpending.forced_mask &= ~MASKVAL(signum);
    return ret;
}

static void set_sigmask(uint32_t newmask) {
    current->sigpending.blocked_mask = newmask & ~SIG_UNMASKABLE;
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

// source: <uapi/asm/sigcontext.h>
struct _fpx_sw_bytes {
    /*
     * If set to FP_XSTATE_MAGIC1 then this is an xstate context.
     * 0 if a legacy frame.
     */
    uint32_t magic1;

    /*
     * Total size of the fpstate area:
     *
     *  - if magic1 == 0 then it's sizeof(struct _fpstate)
     *  - if magic1 == FP_XSTATE_MAGIC1 then it's sizeof(struct _xstate)
     *    plus extensions (if any)
     */
    uint32_t extended_size;

    /*
     * Feature bit mask (including FP/SSE/extended state) that is present
     * in the memory layout:
     */
    uint64_t xfeatures;

    /*
     * Actual XSAVE state size, based on the xfeatures saved in the layout.
     * 'extended_size' is greater than 'xstate_size':
     */
    uint32_t xstate_size;

    /* For future use: */
    uint32_t padding[7];
};

/*
 * As documented in the iBCS2 standard:
 *
 * The first part of "struct _fpstate" is just the normal i387 hardware setup,
 * the extra "status" word is used to save the coprocessor status word before
 * entering the handler.
 *
 * The FPU state data structure has had to grow to accommodate the extended FPU
 * state required by the Streaming SIMD Extensions.  There is no documented
 * standard to accomplish this at the moment.
 */

/* 10-byte legacy floating point register: */
struct _fpreg {
    uint16_t significand[4];
    uint16_t exponent;
};

/* 16-byte floating point register: */
struct _fpxreg {
    uint16_t significand[4];
    uint16_t exponent;
    uint16_t padding[3];
};

/* 16-byte XMM register: */
struct _xmmreg {
    uint32_t element[4];
};

#define X86_FXSR_MAGIC            0x0000

/*
 * The 32-bit FPU frame:
 */
struct _fpstate {
    /* Legacy FPU environment: */
    uint32_t cw;
    uint32_t sw;
    uint32_t tag;
    uint32_t ipoff;
    uint32_t cssel;
    uint32_t dataoff;
    uint32_t datasel;
    struct _fpreg _st[8];
    uint16_t status;
    uint16_t magic;  /* 0xffff: regular FPU data only */
                     /* 0x0000: FXSR FPU data */

    /* FXSR FPU environment */
    uint32_t _fxsr_env[6];    /* FXSR FPU env is ignored */
    uint32_t mxcsr;
    uint32_t reserved;
    struct _fpxreg _fxsr_st[8];    /* FXSR FPU reg data is ignored */
    struct _xmmreg _xmm[8];    /* First 8 XMM registers */
    union {
        uint32_t padding1[44];    /* Second 8 XMM registers plus padding */
        uint32_t padding[44];    /* Alias name for old user-space */
    };

    union {
        uint32_t padding2[12];
        struct _fpx_sw_bytes sw_reserved;    /* Potential extended state is encoded here */
    };
};

struct sigcontext {
    uint16_t gs, __gsh;
    uint16_t fs, __fsh;
    uint16_t es, __esh;
    uint16_t ds, __dsh;
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t esp;
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;
    uint32_t trapno;
    uint32_t err;
    uint32_t eip;
    uint16_t cs, __csh;
    uint32_t eflags;
    uint32_t esp_at_signal;
    uint16_t ss, __ssh;
    struct _fpstate *fpstate;
    uint32_t oldmask;
    uint32_t cr2;
};

struct _header {
    uint64_t xfeatures;
    uint64_t reserved1[2];
    uint64_t reserved2[5];
};

struct _ymmh_state {
    /* 16x YMM registers, 16 bytes each: */
    uint32_t ymmh_space[64];
};

/*
 * Extended state pointed to by sigcontext::fpstate.
 *
 * In addition to the fpstate, information encoded in _xstate::xstate_hdr
 * indicates the presence of other extended state information supported
 * by the CPU and kernel:
 */
struct _xstate {
    struct _fpstate fpstate;
    struct _header xstate_hdr;
    struct _ymmh_state ymmh;
    /* New processor state extensions go here: */
};

// source: "arch/x86/um/signal.c"
struct sigframe {
    void *pretcode;
    int sig;
    struct sigcontext sc;
    struct _xstate fpstate;
    unsigned long extramask[1];
    // char retcode[8];
    uint64_t retcode;
};

// source: "arch/x86/kernel/signal.c"
static const struct {
    uint16_t poplmovl;
    uint32_t val;
    uint16_t int80;
} __attribute__((packed)) retcode = {
    0xb858,        /* popl %eax; movl $..., %eax */
    NR_LINUX_sigreturn,
    0x80cd,        /* int $0x80 */
};

struct sigframe test;

void deliver_signal(struct intr_info *regs) {
    uint16_t signum = signal_pending_one(current);
    if (!signum)
        return;

    bool is_fatal = signal_is_fatal(current, signum);

    current->sigpending.pending_mask &= ~MASKVAL(signum);
    current->sigpending.forced_mask &= ~MASKVAL(signum);

    struct sigaction *sigaction = &current->sigactions->sigactions[signum];

    if (is_fatal || sigaction->sigaction == SIG_DFL) {
        switch (default_sig_action(signum)) {
        case SIG_IGNORE:
            return;
        case SIG_KILL:
            switch (current->subsystem) {
            case SUBSYSTEM_LINUX:
                do_exit(128 + signum);
            case SUBSYSTEM_ECE391:
                do_exit(256);
            }
        case SIG_STOP:
            current->stopped = true;
            while (current->stopped)
                schedule();
        case SIG_CONT:
            BUG(); // should be treated specially
        }
    }

    if (sigaction->sigaction == SIG_IGN)
        return;

    switch (current->subsystem) {
    case SUBSYSTEM_LINUX: {
        uint32_t saved_esp = regs->esp;
        if (push_userstack(regs, NULL, sizeof(struct sigframe)) < 0)
            goto force_segv;

        struct sigframe *sigframe = (void *)regs->esp;
        *sigframe = (struct sigframe){
            .pretcode = &sigframe->retcode,
            .sig = signum,
            .sc = {
                .gs  = regs->gs,
                .fs  = regs->fs,
                .es  = regs->es,
                .ds  = regs->ds,
                .edi = regs->edi,
                .esi = regs->esi,
                .ebp = regs->ebp,
                .esp = regs->intr_esp,
                .ebx = regs->ebx,
                .edx = regs->edx,
                .ecx = regs->ecx,
                .eax = regs->eax,
                .trapno = regs->intr_num,
                .err    = regs->error_code,
                .eip    = regs->eip,
                .cs     = regs->cs,
                .eflags = regs->eflags,
                .esp_at_signal = saved_esp,
                .ss  = regs->ss,
                // .struct _fpstate *fpstate = regs->struct,
                .oldmask = current->sigpending.blocked_mask,
                // .cr2 = regs->cr2,
            },
            .retcode = *(uint64_t *)&retcode,
        };
        if (sigaction->flags & SA_RESTORER)
            sigframe->pretcode = sigaction->restorer;

        set_sigmask(sigaction->mask);
        regs->eip = (uint32_t)sigaction->sigaction;
        regs->eax = signum;
        return;
    }
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
            .intr_num   = regs->intr_num,
            .error_code = regs->error_code,
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

DEFINE_SYSCALL3(LINUX, rt_sigaction, int, signum, const struct sigaction *, act, struct sigaction *, oldact) {
    if (signum <= 0 || signum >= NSIG)
        return -EINVAL;
    if (signum == SIGKILL || signum == SIGSTOP)
        return -EINVAL;

    if (safe_buf(oldact, sizeof(*oldact), false) == sizeof(*oldact))
        *oldact = current->sigactions->sigactions[signum];

    if (safe_buf(act, sizeof(*act), false) == sizeof(*act))
        current->sigactions->sigactions[signum] = *act;

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

DEFINE_SYSCALL_COMPLEX(LINUX, sigreturn, regs) {
    struct sigframe *context = (void *)(regs->eip - sizeof(struct sigframe));
    if (safe_buf(context, sizeof(*context), false) != sizeof(*context)) {
        force_sig(current, SIGSEGV);
        return;
    }

    regs->ebx    = context->sc.ebx;
    regs->ecx    = context->sc.ecx;
    regs->edx    = context->sc.edx;
    regs->esi    = context->sc.esi;
    regs->edi    = context->sc.edi;
    regs->ebp    = context->sc.ebp;
    regs->eax    = context->sc.eax;
    regs->ds     = context->sc.ds;
    regs->es     = context->sc.es;
    regs->fs     = context->sc.fs;
    regs->eip    = context->sc.eip;
    regs->eflags = context->sc.eflags;
    regs->esp    = context->sc.esp_at_signal;
    regs->ss     = context->sc.ss;
    regs->ds     = context->sc.ds;
    regs->es     = context->sc.es;
    regs->fs     = context->sc.fs;
    regs->gs     = context->sc.gs;

    set_sigmask(context->sc.oldmask);

    // FIXME: Bad regs here can cause a panic

    return;
}

DEFINE_SYSCALL4(LINUX, rt_sigprocmask, int, how, const uint32_t *, set, uint32_t *, oldset, uint32_t, sigsetsize) {
    if (safe_buf(oldset, sizeof(*oldset), false) == sizeof(*oldset))
        *oldset = current->sigpending.blocked_mask;

    if (safe_buf(set, sizeof(*set), false) == sizeof(*set)) {
        uint32_t curset = current->sigpending.blocked_mask;

        switch (how) {
        case SIG_BLOCK:
            curset |= *set;
            break;
        case SIG_UNBLOCK:
            curset &= ~*set;
            break;
        case SIG_SETMASK:
            curset = *set;
            break;
        default:
            return -EINVAL;
        }

        set_sigmask(curset);
    }

    return 0;
}

DEFINE_SYSCALL2(LINUX, kill, int32_t, pid, uint32_t, signum) {
    if (signum > NSIG)
        return -EINVAL;

    struct siginfo siginfo = {
        .signo = signum,
        .code = SI_USER,
        .sifields.kill.pid = current->pid,
    };

    if (pid < 1) {
        bool hashit = false;

        uint16_t pgid;
        if (pid < -1)
            pgid = -pid;
        else if (pid == -1)
            pgid = 0;
        else // pid == 0
            pgid = current->pgid;

        struct list_node *node;
        list_for_each(&tasks, node) {
            struct task_struct *task = node->value;
            if ((!pgid && task->pid > 1) || task->pgid == pgid) {
                hashit = true;
                if (signum)
                    send_sig_info(task, &siginfo);
            }
        }

        if (!hashit)
            return -ESRCH;
    } else {
        struct task_struct *task = get_task_from_pid(pid);
        if (IS_ERR(task))
            return PTR_ERR(task);

        if (signum)
            send_sig_info(task, &siginfo);
    }

    return 0;
}

DEFINE_SYSCALL3(LINUX, tgkill, int32_t, tgid, int32_t, tid, uint32_t, signum) {
    if (signum > NSIG)
        return -EINVAL;

    if (tid != tgid)
        return -ESRCH;

    struct siginfo siginfo = {
        .signo = signum,
        .code = SI_USER,
        .sifields.kill.pid = current->pid,
    };

    struct task_struct *task = get_task_from_pid(tid);
    if (IS_ERR(task))
        return PTR_ERR(task);

    if (signum)
        send_sig_info(task, &siginfo);

    return 0;
}
