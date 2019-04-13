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
    void     (*action)(int);
    // void     (*sigaction)(int, void *, void *);
    // sigset_t   mask;
    int        flags;
    // void     (*restorer)(void);
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

bool signal_pending(struct task_struct *task);
bool fatal_signal_pending(struct task_struct *task);

void send_sig(struct task_struct *task, uint16_t signum);
void force_sig(struct task_struct *task, uint16_t signum);

int32_t send_sig_pg(uint16_t pgid, uint16_t signum);

void kernel_mask_signal(uint16_t signum);
void kernel_unmask_signal(uint16_t signum);

uint16_t kernel_peek_pending_sig();
uint16_t kernel_get_pending_sig();

void deliver_signal(struct intr_info *intr_info);

#endif

#endif
