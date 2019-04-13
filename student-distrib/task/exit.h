#ifndef _EXIT_H
#define _EXIT_H

#include "task.h"

noreturn void do_exit(int exitcode);
int32_t do_wait(struct task_struct *task);
int32_t do_waitall(uint16_t *pid);

void do_free_tasks();

#endif
