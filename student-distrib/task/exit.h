#ifndef _EXIT_H
#define _EXIT_H

#include "task.h"

noreturn void do_exit(int exitcode);
int do_wait(struct task_struct *task);

void do_free_tasks();

#endif
