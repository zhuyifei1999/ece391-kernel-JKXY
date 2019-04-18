#include "session.h"
#include "task.h"
#include "../mm/kmalloc.h"
#include "../err.h"
#include "../errno.h"


int32_t do_setsid(void) {
    struct list_node *node;
    list_for_each(&tasks, node) {
        struct task_struct *task = node->value;
        if (task->pgid == current->pid)
            return -EPERM;
    }

    struct session *session = kmalloc(sizeof(*session));
    if (!session)
        return -ENOMEM;
    *session = (struct session){
        .sid = current->pid,
        .foreground_pgid = current->pid,
        .refcount = ATOMIC_INITIALIZER(1),
    };

    current->session = session;
    current->pgid = current->pid;

    return current->pgid;
}

void put_session() {
    struct session *session = current->session;
    current->session = NULL;
    current->pgid = 0;

    if (!session)
        return;

    if (atomic_dec(&session->refcount))
        return;

    if (session->tty) {
        if (session->tty->session == session)
            session->tty->session = NULL;
        tty_put(session->tty);
    }
    kfree(session);
}

int32_t do_setpgid(int32_t pid, int32_t pgid) {
    if (!pid)
        pid = current->pid;
    if (!pgid)
        pgid = current->pid;

    struct task_struct *task = get_task_from_pid(pid);
    if (IS_ERR(task))
        return PTR_ERR(task);

    struct task_struct *leader = get_task_from_pid(pgid);
    if (IS_ERR(leader))
        return PTR_ERR(leader);

    if (!task->session || !leader->session)
        return -EINVAL;

    if (task->session->sid == task->pid)
        return -EPERM;

    if (task->session != leader->session)
        return -EPERM;

    task->pgid = pgid;
    return 0;
}
