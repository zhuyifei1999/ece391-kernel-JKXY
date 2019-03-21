#ifndef _KTHREAD_H
#define _KTHREAD_H

#include "task.h"

extern struct task_struct *kthreadd_task;

int kthreadd(void *args);

struct task_struct *kthread(int (*fn)(void *args), void *args);

#endif
