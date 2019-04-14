#ifndef _MTUEX_H
#define _MTUEX_H

#include "structure/list.h"
#include "initcall.h"

struct mutex {
    struct list queue;
};

void mutex_init(struct mutex *mutex);

int32_t mutex_lock_interruptable(struct mutex *mutex);
int32_t mutex_lock_uninterruptable(struct mutex *mutex);
void mutex_unlock(struct mutex *mutex);

#define MUTEX_STATIC_INIT(mutex) static void __init_mutex_ ## mutex() { \
    mutex_init(&mutex);                                               \
}                                                                   \
DEFINE_INITCALL(__init_mutex_ ## mutex, early)

#endif
