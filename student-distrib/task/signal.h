#ifndef _SIGNAL_H
#define _SIGNAL_H

// source: <uapi/asm/signal.h>

#define SIGHUP    1
#define SIGINT    2
#define SIGQUIT   3
#define SIGILL    4
#define SIGTRAP   5
#define SIGABRT   6
#define SIGIOT    6
#define SIGBUS    7
#define SIGFPE    8
#define SIGKILL   9
#define SIGUSR1   10
#define SIGSEGV   11
#define SIGUSR2   12
#define SIGPIPE   13
#define SIGALRM   14
#define SIGTERM   15
#define SIGSTKFLT 16
#define SIGCHLD   17
#define SIGCONT   18
#define SIGSTOP   19
#define SIGTSTP   20
#define SIGTTIN   21
#define SIGTTOU   22
#define SIGURG    23
#define SIGXCPU   24
#define SIGXFSZ   25
#define SIGVTALRM 26
#define SIGPROF   27
#define SIGWINCH  28
#define SIGIO     29
#define SIGPOLL   SIGIO
/*
#define SIGLOST   29
*/
#define SIGPWR    30
#define SIGSYS    31
#define SIGUNUSED 31

#define SA_NOCLDSTOP 0x00000001u
#define SA_NOCLDWAIT 0x00000002u
#define SA_SIGINFO   0x00000004u
#define SA_ONSTACK   0x08000000u
#define SA_RESTART   0x10000000u
#define SA_NODEFER   0x40000000u
#define SA_RESETHAND 0x80000000u

#define SA_NOMASK  SA_NODEFER
#define SA_ONESHOT SA_RESETHAND

#define SA_RESTORER 0x04000000

#define SI_USER   0    /* sent by kill, sigsend, raise */
#define SI_KERNEL 0x80 /* sent by the kernel from somewhere */

#define CLD_EXITED  1   /* child has exited */
#define CLD_KILLED  2   /* child was killed */
#define CLD_DUMPED  3   /* child terminated abnormally */
#define CLD_TRAPPED 4   /* traced child has trapped */
#define CLD_STOPPED 5   /* child has stopped */
#define CLD_CONTINUED 6 /* stopped child has continued */

#define SIG_ECE391_DIV_ZERO  0
#define SIG_ECE391_SEGFAULT  1
#define SIG_ECE391_INTERRUPT 2
#define SIG_ECE391_ALARM     3
#define SIG_ECE391_USER1     4

#define MASKVAL(signum) (1 << (signum))

// source: <asm/signal.h>

#define SIG_UNMASKABLE (MASKVAL(SIGKILL) | MASKVAL(SIGSTOP) | MASKVAL(SIGCONT))

#define NSIG 32

#ifdef ASM

#define SIG_DFL 0
#define SIG_IGN 1

#else

#define SIG_DFL ((void *)0)
#define SIG_IGN ((void *)1)


#include "../lib/stdint.h"
#include "../lib/stdbool.h"
#include "../structure/array.h"
#include "../atomic.h"

// source : <asm-generic/siginfo.h>

typedef union sigval {
    int sival_int;
    void /* __user */ *sival_ptr;
} sigval_t;

struct siginfo {
    int signo;
    int errno;
    int code;

    union {
        int pad[((128 - (4 * sizeof(int))) / sizeof(int))];

        /* kill() */
        struct {
            uint32_t pid;    /* sender's pid */
            uint32_t uid;    /* sender's uid */
        } kill;

        /* POSIX.1b timers */
        struct {
            uint32_t tid;    /* timer id */
            int overrun;        /* overrun count */
            char pad[sizeof(uint32_t) - sizeof(int)];
            sigval_t sigval;    /* same as below */
            int sys_private;       /* not to be passed to user */
        } timer;

        /* POSIX.1b signals */
        struct {
            uint32_t pid;    /* sender's pid */
            uint32_t uid;    /* sender's uid */
            sigval_t sigval;
        } rt;

        /* SIGCHLD */
        struct {
            uint32_t pid;    /* which child */
            uint32_t uid;    /* sender's uid */
            int status;        /* exit code */
            uint32_t utime;
            uint32_t stime;
        } sigchld;

        /* SIGILL, SIGFPE, SIGSEGV, SIGBUS */
        struct {
            void *addr; /* faulting insn/memory ref. */
#ifdef _ARCH_SI_TRAPNO
            int trapno;    /* TRAP # which caused the signal */
#endif
            short addr_lsb; /* LSB of the reported address */
        } sigfault;

        /* SIGPOLL */
        struct {
            uint32_t band;    /* POLL_IN, POLL_OUT, POLL_MSG */
            int fd;
        } sigpoll;
    } sifields;
};

struct task_struct;
struct intr_info;

struct sigaction {
    // union {
    //     void (*handler)(int);
    //     void (*sigaction)(int, struct siginfo *, void *);
    // } _u9;
    void *sigaction;
    uint32_t mask;
    uint32_t flags;
    void (*restorer)(void);
};

struct sigpending {
    uint32_t blocked_mask;
    uint32_t pending_mask;
    uint32_t forced_mask;
    struct array siginfos;
};

struct sigactions {
    atomic_t refcount;
    struct sigaction sigactions[NSIG];
};

uint32_t fatal_signal_pending(struct task_struct *task);
uint32_t signal_pending(struct task_struct *task);
uint16_t signal_pending_one(struct task_struct *task);
bool signal_is_fatal(struct task_struct *task, uint16_t signum);

void send_sig_info(struct task_struct *task, struct siginfo *siginfo);
void force_sig_info(struct task_struct *task, struct siginfo *siginfo);

void send_sig(struct task_struct *task, uint16_t signum);
void force_sig(struct task_struct *task, uint16_t signum);

int32_t send_sig_info_pg(uint16_t pgid, struct siginfo *siginfo);

void kernel_mask_signal(uint16_t signum);
void kernel_unmask_signal(uint16_t signum);

bool kernel_sig_ispending(uint16_t signum);
bool kernel_peek_pending_sig(uint16_t signum, struct siginfo *siginfo);
bool kernel_get_pending_sig(uint16_t signum, struct siginfo *siginfo);

void deliver_signal(struct intr_info *regs);

#endif

#endif
