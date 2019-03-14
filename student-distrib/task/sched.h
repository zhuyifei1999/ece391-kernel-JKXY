#ifndef _SCHEDULE_H
#define _SCHEDULE_H

#include "../structure/list.h"
#include "task.h"

#define INTR_SCHED 0x81

extern struct linked_list schedule_queue;

void schedule(void);

void wake_up_process(struct task_struct *task);

#endif
