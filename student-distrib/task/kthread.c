#include "kthread.h"
#include "clone.h"
#include "sched.h"
#include "../structure/list.h"
#include "../lib/string.h"
#include "../mm/kmalloc.h"
#include "../initcall.h"
#include "../err.h"
#include "../errno.h"

struct task_struct *kthreadd_task;

static struct list kthread_create_queue;

struct create_entry {
    int (*fn)(void *args);
    void *args;
    struct task_struct *caller;
    struct task_struct *kthread;
};

int kthreadd(void *args) {
    kthreadd_task = current;
    strcpy(kthreadd_task->comm, "kthreadd");

    while (1) {
        current->state = TASK_INTERRUPTIBLE;
        while (list_isempty(&kthread_create_queue))
            schedule();
        current->state = TASK_RUNNING;

        while (!list_isempty(&kthread_create_queue)) {
            struct create_entry *entry = list_pop_front(&kthread_create_queue);
            entry->kthread = kernel_thread(entry->fn, entry->args);
            wake_up_process(entry->caller);
        }
    }
}

struct task_struct *kthread(int (*fn)(void *args), void *args) {
    struct create_entry *entry = kmalloc(sizeof(*entry));
    if (!entry)
        return ERR_PTR(-ENOMEM);
    *entry = (struct create_entry){
        .fn     = fn,
        .args   = args,
        .caller = current,
    };

    struct task_struct *ret;
    int32_t res = list_insert_back(&kthread_create_queue, entry);
    if (res < 0) {
        ret = ERR_PTR(res);
        goto out_free;
    }

    wake_up_process(kthreadd_task);
    current->state = TASK_UNINTERRUPTIBLE;
    while (!entry->kthread)
        schedule();
    current->state = TASK_RUNNING;

    ret = entry->kthread;

out_free:
    kfree(entry);
    return ret;
}

static void init_kthread() {
    list_init(&kthread_create_queue);
}
DEFINE_INITCALL(init_kthread, early);
