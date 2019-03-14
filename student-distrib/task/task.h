#ifndef _TASK_H
#define _TASK_H

#include "../types.h"
#include "../mm/paging.h"
#include "../compiler.h"
#include "../interrupt.h"
#include "../structure/list.h"

#define MAXPID 32767  // See paging.h for explanation
#define LOOPPID 16    // When MAXPID is reached, loop from here

#define PID_BUCKETS 16 // number of buckets for PID

struct mm_struct {
    page_directory_t *page_directory;
};

enum task_state {
    TASK_RUNNING,
    TASK_INTERRUPTABLE,
    TASK_UNINTERRUPTABLE,
    TASK_ZOMBIE,
};

enum subsystem {
    SUBSYSTEM_ECE391,
    SUBSYSTEM_LINUX,
};

struct task_struct {
    uint16_t pid;
    uint16_t ppid;
    char comm[16];
    struct mm_struct *mm;
    struct intr_info *return_regs;
    enum task_state state;
    enum subsystem subsystem;
    int exitcode;
};

#define TASK_STACK_PAGES 2  // each task has 4 (1<<2) pages for kernel stack
#define ALIGN_SP 0xf        // C likes stuffs to be aligned

// the task_struct is always at the bottom of the kernel stack
static inline __always_inline
struct task_struct *task_from_stack(void *stack) {
    uint32_t addr;
    addr = (uint32_t)stack / TASK_STACK_PAGES / PAGE_SIZE_SMALL;
    addr = (addr + 1) * TASK_STACK_PAGES * PAGE_SIZE_SMALL;
    addr -= sizeof(struct task_struct);
    addr &= -ALIGN_SP;
    return (struct task_struct *)addr;
}
static inline __always_inline
struct task_struct *get_current(void) {
    int temp;
    return task_from_stack(&temp);
}

#define current (get_current())

extern struct linked_list tasks[PID_BUCKETS];

struct task_struct *kernel_thread(int (*fn)(void *data), void *data);
struct task_struct *get_task_from_pid(uint16_t pid);

noreturn void do_exit(int exitcode);

#endif
