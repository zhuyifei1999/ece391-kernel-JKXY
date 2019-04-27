#include "time.h"
#include "clock.h"
#include "uptime.h"
#include "../drivers/rtc.h"
#include "../mm/paging.h"
#include "../mm/kmalloc.h"
#include "../structure/list.h"
#include "../task/sched.h"
#include "../syscall.h"
#include "../initcall.h"
#include "../err.h"
#include "../errno.h"

static struct list sleep_queue;
LIST_STATIC_INIT(sleep_queue);

struct sleep_spec {
    struct task_struct *task;
    struct timespec endtime;
};

// void timespec_now(struct timespec *spec) {
//     get_uptime(&spec);
//     spec->sec = time_now();
// }

int timespec_cmp(const struct timespec *x, const struct timespec *y) {
    if (x->sec == y->sec)
        return (int32_t)x->nsec - y->nsec;
    return (int32_t)x->sec - y->sec;
}

void timespec_add(struct timespec *x, const struct timespec *y) {
    x->sec += y->sec;
    x->nsec += y->nsec;

    if (x->nsec >= NSEC) {
        x->sec++;
        x->nsec -= NSEC;
    }
}

void timespec_sub(struct timespec *x, const struct timespec *y) {
    if (x->nsec < y->nsec) {
        x->sec--;
        x->nsec += NSEC;
    }

    x->sec -= y->sec;
    x->nsec -= y->nsec;
}

static void rtc_handler() {
    struct timespec now;
    get_uptime(&now);

    list_remove_on_cond_extra(&sleep_queue, struct sleep_spec *, spec, timespec_cmp(&now, &spec->endtime) >= 0, ({
        wake_up_process(spec->task);
    }));
}

struct sleep_spec *sleep_add(const struct timespec *time) {
    struct sleep_spec *spec = kmalloc(sizeof(*spec));
    if (!spec)
        return ERR_PTR(-ENOMEM);

    spec->task = current;
    get_uptime(&spec->endtime);
    timespec_add(&spec->endtime, time);

    list_insert_ordered(&sleep_queue, spec, (void *)&timespec_cmp);
    return spec;
}

bool sleep_hashit(const struct sleep_spec *spec) {
    struct timespec now;
    get_uptime(&now);

    return timespec_cmp(&now, &spec->endtime) >= 0;
}

void sleep_finalize(struct sleep_spec *spec) {
    list_remove(&sleep_queue, spec);
    kfree(spec);
}

DEFINE_SYSCALL2(LINUX, nanosleep, const struct timespec *, req, struct timespec *, rem) {
    if (safe_buf(req, sizeof(*req), false) != sizeof(*req))
        return -EFAULT;

    if (req->nsec >= NSEC)
        return -EINVAL;

    struct sleep_spec *spec = sleep_add(req);
    if (IS_ERR(spec))
        return PTR_ERR(spec);

    if (safe_buf(rem, sizeof(*rem), true) == sizeof(*rem))
        *rem = (struct timespec){0};

    int32_t res = 0;

    while (true) {
        if (sleep_hashit(spec))
            break;

        if (signal_pending(current)) {
            if (safe_buf(rem, sizeof(*rem), true) == sizeof(*rem)) {
                struct timespec now;
                get_uptime(&now);

                *rem = spec->endtime;
                timespec_sub(rem, &now);
            }

            res = -EINTR;
            break;
        }

        current->state = TASK_INTERRUPTIBLE;
        schedule();
        current->state = TASK_RUNNING;
    }

    sleep_finalize(spec);

    return res;
}

static void init_sleep() {
    register_rtc_handler(&rtc_handler);
}
DEFINE_INITCALL(init_sleep, drivers);
