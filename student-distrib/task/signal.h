#ifndef _SIGNAL_H
#define _SIGNAL_H

// source: <uapi/asm/signal.h>

#define SIGHUP		 1
#define SIGINT		 2
#define SIGQUIT		 3
#define SIGILL		 4
#define SIGTRAP		 5
#define SIGABRT		 6
#define SIGIOT		 6
#define SIGBUS		 7
#define SIGFPE		 8
#define SIGKILL		 9
#define SIGUSR1		10
#define SIGSEGV		11
#define SIGUSR2		12
#define SIGPIPE		13
#define SIGALRM		14
#define SIGTERM		15
#define SIGSTKFLT	16
#define SIGCHLD		17
#define SIGCONT		18
#define SIGSTOP		19
#define SIGTSTP		20
#define SIGTTIN		21
#define SIGTTOU		22
#define SIGURG		23
#define SIGXCPU		24
#define SIGXFSZ		25
#define SIGVTALRM	26
#define SIGPROF		27
#define SIGWINCH	28
#define SIGIO		29
#define SIGPOLL		SIGIO
/*
#define SIGLOST		29
*/
#define SIGPWR		30
#define SIGSYS		31
#define	SIGUNUSED	31

#define SIG_ECE391_DIV_ZERO  0
#define SIG_ECE391_SEGFAULT  1
#define SIG_ECE391_INTERRUPT 2
#define SIG_ECE391_ALARM     3
#define SIG_ECE391_USER1     4

// source: <asm/signal.h>
#define _NSIG		64

#define _NSIG_BPW	32

#define _NSIG_WORDS	(_NSIG / _NSIG_BPW)

#ifdef ASM

#define SIG_DFL 0
#define SIG_IGN 1

#else

#define SIG_DFL ((void *)0)
#define SIG_IGN ((void *)1)

#include "../lib/stdint.h"
#include "../lib/stdbool.h"
#include "../atomic.h"

struct task_struct;
struct intr_info;

struct sigaction {
    void *sigaction;
    // unsigned long mask;
    int flags;
    // void (*restorer)(void);
};

struct sigpending {
    bool is_pending;
    uint16_t signum;
    bool forced;
};

struct sigactions {
    atomic_t refcount;
    struct sigaction sigactions[_NSIG];
};

struct siginfo {
    int      si_signo;     /* Signal number */
    int      si_errno;     /* An errno value */
    int      si_code;      /* Signal code */
    int      si_trapno;    /* Trap number that caused
                              hardware-generated signal
                              (unused on most architectures) */
    int      si_pid;       /* Sending process ID */
    unsigned int si_uid;       /* Real user ID of sending process */
    int      si_status;    /* Exit value or signal */
    long long  si_utime;     /* User time consumed */
    long long  si_stime;     /* System time consumed */
    union sigval {
    	int sival_int;
    	void *sival_ptr;
    } si_value;     /* Signal value */
    int      si_int;       /* POSIX.1b signal */
    void    *si_ptr;       /* POSIX.1b signal */
    int      si_overrun;   /* Timer overrun count;
                              POSIX.1b timers */
    int      si_timerid;   /* Timer ID; POSIX.1b timers */
    void    *si_addr;      /* Memory location which caused fault */
    long     si_band;      /* Band event (was int in
                              glibc 2.3.2 and earlier) */
    int      si_fd;        /* File descriptor */
    short    si_addr_lsb;  /* Least significant bit of address
                              (since Linux 2.6.32) */
    void    *si_lower;     /* Lower bound when address violation
                              occurred (since Linux 3.19) */
    void    *si_upper;     /* Upper bound when address violation
                              occurred (since Linux 3.19) */
    int      si_pkey;      /* Protection key on PTE that caused
                              fault (since Linux 4.6) */
    void    *si_call_addr; /* Address of system call instruction
                              (since Linux 3.5) */
    int      si_syscall;   /* Number of attempted system call
                              (since Linux 3.5) */
    unsigned int si_arch;  /* Architecture of attempted system call
                              (since Linux 3.5) */
};

bool signal_pending(struct task_struct *task);
bool fatal_signal_pending(struct task_struct *task);

void send_sig(struct task_struct *task, uint16_t signum);
void force_sig(struct task_struct *task, uint16_t signum);

int32_t send_sig_pg(uint16_t pgid, uint16_t signum);

void kernel_mask_signal(uint16_t signum);
void kernel_unmask_signal(uint16_t signum);

uint16_t kernel_peek_pending_sig();
uint16_t kernel_get_pending_sig();

void deliver_signal(struct intr_info *regs);

#endif

#endif
