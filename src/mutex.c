#include "mutex.h"
#include "task/task.h"
#include "task/sched.h"
#include "task/signal.h"
#include "panic.h"
#include "errno.h"

void mutex_init(struct mutex *mutex) {
    list_init(&mutex->queue);
}

int32_t mutex_lock_interruptable(struct mutex *mutex) {
    if (list_contains(&mutex->queue, current))
        BUG();

    int32_t res = list_insert_back(&mutex->queue, current);
    if (res < 0)
        return res;

    current->state = TASK_INTERRUPTIBLE;
    while (list_peek_front(&mutex->queue) != current && !signal_pending(current)) {
        schedule();
    }
    current->state = TASK_RUNNING;

    if (!signal_pending(current))
        return 0;

    list_remove(&mutex->queue, current);
    return -EINTR;
}

int32_t mutex_lock_uninterruptable(struct mutex *mutex) {
    if (list_contains(&mutex->queue, current))
        BUG();

    int32_t res = list_insert_back(&mutex->queue, current);
    if (res < 0)
        return res;

    current->state = TASK_UNINTERRUPTIBLE;
    while (list_peek_front(&mutex->queue) != current) {
        schedule();
    }
    current->state = TASK_RUNNING;

    return 0;
}

void mutex_unlock(struct mutex *mutex) {
    cli();
    if (list_pop_front(&mutex->queue) != current)
        BUG();

    // Wake up the next in queue
    if (!list_isempty(&mutex->queue))
        wake_up_process(list_peek_front(&mutex->queue));
    sti();
}
