#ifndef _INIT_TASK_H
#define _INIT_TASK_H

#include "task/task.h"

extern struct task_struct *init_task;

noreturn void switch_to_init_task(void);

#endif
