#ifndef _INIT_TASK_H
#define _INIT_TASK_H

#include "task/task.h"

extern struct task_struct *swapper_task;

noreturn void exec_swapper_task(void);

#endif
