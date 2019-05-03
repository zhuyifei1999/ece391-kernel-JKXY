#include "poll.h"
#include "../mm/kmalloc.h"
#include "../task/task.h"
#include "../task/sched.h"
#include "../time/sleep.h"
#include "../atomic.h"
#include "../syscall.h"
#include "../err.h"
#include "../errno.h"

struct pollfd {
    int   fd;         /* file descriptor */
    short events;     /* requested events */
    short revents;    /* returned events */
};

DEFINE_SYSCALL3(LINUX, poll, struct pollfd *, fds, int32_t, nfds, int32_t, timeout) {
    if (safe_buf(fds, nfds * sizeof(*fds), true) != nfds * sizeof(*fds))
        return -EFAULT;

    struct poll_entry *poll_table = kcalloc(nfds, sizeof(*poll_table));
    if (!poll_table)
        return -ENOMEM;

    uint32_t i;
    for (i = 0; i < nfds; i++) {
        poll_table[i].task = current;
        poll_table[i].events = fds[i].events;
        list_init(&poll_table[i].cleanup_cb);

        struct file *file = array_get(&current->files->files, fds[i].fd);
        if (file) {
            atomic_inc(&file->refcount);
            poll_table[i].file = file;
        } else {
            poll_table[i].revents |= POLLNVAL;
        }
    }

    struct sleep_spec *sleep_spec = NULL;

    if (timeout > 0) {
        struct timespec timespec = {
            .sec = timeout / 1000,
            .nsec = timeout % 1000 * 1000
        };
        sleep_spec = sleep_add(&timespec);

        if (IS_ERR(sleep_spec))
            return PTR_ERR(sleep_spec);
    }

    int32_t res;
    while (true) {
        if (signal_pending(current)) {
            res = -EINTR;
            break;
        }

        for (i = 0; i < nfds; i++) {
            struct file *file = poll_table[i].file;
            if (!file)
                continue;
            if (file->op->poll) {
                (*file->op->poll)(file, &poll_table[i]);
            } else {
                if (file->op->read && (poll_table[i].events & POLLIN))
                    poll_table[i].revents |= POLLIN;
                if (file->op->write && (poll_table[i].events & POLLOUT))
                    poll_table[i].revents |= POLLOUT;
            }
        }

        res = 0;
        for (i = 0; i < nfds; i++) {
            if (poll_table[i].revents)
                res++;
        }

        if (res || !timeout)
            break;

        if (sleep_spec && sleep_hashit(sleep_spec))
            break;

        current->state = TASK_INTERRUPTIBLE;
        schedule();
        current->state = TASK_RUNNING;
    }

    for (i = 0; i < nfds; i++) {
        struct list_node *node;
        list_for_each(&poll_table[i].cleanup_cb, node) {
            poll_cleanup_t *cleanup_cb = node->value;
            (*cleanup_cb)(&poll_table[i]);
        }
        list_destroy(&poll_table[i].cleanup_cb);

        if (poll_table[i].file)
            filp_close(poll_table[i].file);

        fds[i].revents = poll_table[i].revents;
    }

    if (sleep_spec)
        sleep_finalize(sleep_spec);

    return res;
}
