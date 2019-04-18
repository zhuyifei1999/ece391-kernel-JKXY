#include "task.h"
#include "sched.h"
#include "signal.h"
#include "tls.h"
#include "../err.h"
#include "../errno.h"

struct list tasks;
LIST_STATIC_INIT(tasks);

struct task_struct *get_task_from_pid(uint16_t pid) {
    if (pid > MAXPID)
        return ERR_PTR(-ESRCH);

    struct list_node *node;
    list_for_each(&tasks, node) {
        struct task_struct *task = node->value;
        if (task->pid == pid)
            return task;
    }

    return ERR_PTR(-ESRCH);
}

asmlinkage
void return_to_userspace(struct intr_info *info) {
    deliver_signal(info);
    cond_schedule();
    load_tls();
}
