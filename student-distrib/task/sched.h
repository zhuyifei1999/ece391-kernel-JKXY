#ifndef _SCHEDULE_H
#define _SCHEDULE_H

#include "../structure/list.h"
#include "../interrupt.h"
#include "task.h"

extern struct list schedule_queue;

#define SCHEDULE_TICK 8 // Let's schedule every 8 ticks

void schedule(void);
void cond_schedule(void);
void rtc_schedule(struct intr_info *info);

void wake_up_process(struct task_struct *task);

#endif
