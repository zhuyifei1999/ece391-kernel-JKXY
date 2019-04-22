#ifndef _SCHEDULE_H
#define _SCHEDULE_H

#include "../structure/list.h"
#include "../interrupt.h"
#include "task.h"

extern struct list schedule_queue;

#define SCHEDULE_TICK 1 // Let's schedule every 1 PIT tick

void schedule(void);
void cond_schedule(void);
void pit_schedule(struct intr_info *info);

void wake_up_process(struct task_struct *task);

#endif
