#ifndef _TASK_H
#define _TASK_H

#include "../lib/stdint.h"
#include "../mm/paging.h"
#include "../compiler.h"
#include "../interrupt.h"
#include "../char/tty.h"
#include "../structure/list.h"
#include "../structure/array.h"
#include "../vfs/file.h"
#include "../panic.h"
#include "../atomic.h"

#define MAXPID 32767  // See paging.h for explanation
#define LOOPPID 16    // When MAXPID is reached, loop from here

#define PID_BUCKETS 16 // number of buckets for PID

struct mm_struct {
    atomic_t refcount;
    page_directory_t *page_directory;
};

struct files_struct {
    atomic_t refcount;
    struct array files;
};

enum task_state {
    TASK_RUNNING,
    TASK_INTERRUPTIBLE,
    TASK_UNINTERRUPTIBLE,
    TASK_ZOMBIE,
    TASK_DEAD,
};

enum subsystem {
    SUBSYSTEM_LINUX,
    SUBSYSTEM_ECE391,
};
#define NUM_SUBSYSTEMS 2

struct task_struct {
    uint16_t pid;
    uint16_t ppid;
    char comm[16];
    struct mm_struct *mm;
    struct files_struct *files;
    struct file *cwd;
    struct file *exe;
    struct tty *tty;
    struct intr_info *return_regs;
    enum task_state state;
    enum subsystem subsystem;
    int exitcode;
};

#define TASK_STACK_PAGES_POW 2  // each task has 4 (1<<2) pages for kernel stack
#define TASK_STACK_PAGES (1<<TASK_STACK_PAGES_POW)
#define ALIGN_SP 0xf        // C likes stuffs to be aligned

// the task_struct is always at the top of the pages of kernel stack
static inline __always_inline __attribute__((pure, const))
struct task_struct *task_from_stack(void *stack) {
    uint32_t addr = (uint32_t)stack;
    return (void *)(addr & ~(TASK_STACK_PAGES * PAGE_SIZE_SMALL - 1));
}
static inline __always_inline __attribute__((pure, const))
struct task_struct *get_current(void) {
    int temp;
    return task_from_stack(&temp);
}

// to GDB this, ((struct task_struct *)((uint32_t)$esp & ~(4*(1<<12)-1)))
#define current (get_current())

static inline __always_inline noreturn
void set_all_regs(struct intr_info *regs) {
    asm volatile (
        "movl %%ebp,%0;"
        "movl %1,%%esp;"
        "jmp ISR_return;"
        :
        : "m"(regs->intr_ebp), "irm"(regs)
    );
    BUG();
}

extern struct list tasks[PID_BUCKETS];

struct task_struct *get_task_from_pid(uint16_t pid);

noreturn void do_exit(int exitcode);
int do_wait(struct task_struct *task);

void do_free_tasks();

#endif
