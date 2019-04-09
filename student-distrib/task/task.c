#include "task.h"
#include "sched.h"
#include "../initcall.h"
#include "../err.h"
#include "../errno.h"

// struct task_struct *tasks[MAXPID];
struct list tasks[PID_BUCKETS];

struct task_struct *get_task_from_pid(uint16_t pid) {
    if (pid > MAXPID)
        return ERR_PTR(-ESRCH);

    struct task_struct *task;
    FOR_EACH_TASK(task, ({
        if (task->pid == pid)
            return task;
    }));

    return ERR_PTR(-ESRCH);
}

asmlinkage
void return_to_userspace(struct intr_info *info) {
    cond_schedule();
}

static void init_tasks() {
    int i;
    for (i = 0; i < PID_BUCKETS; i++) {
        list_init(&tasks[i]);
    }
}
DEFINE_INITCALL(init_tasks, early);
