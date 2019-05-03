#ifndef _KTHREAD_H
#define _KTHREAD_H

#include "task.h"
#include "sched.h"

extern struct task_struct *kthreadd_task;

int kthreadd(void *args);

struct task_struct *kthread(int (*fn)(void *args), void *args);

#define DEFINE_INIT_KTHREAD(fn) static void __run_ ## fn(void) { \
    wake_up_process(kthread(&fn, NULL));                         \
}                                                                \
DEFINE_INITCALL(__run_ ## fn, init_kthread)

#endif
