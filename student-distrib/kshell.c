#include "lib/stdio.h"
#include "task/clone.h"
#include "task/exit.h"
#include "task/kthread.h"
#include "task/session.h"
#include "task/signal.h"
#include "char/tty.h"
#include "tests.h"

#if RUN_TESTS
static int kselftest(void *args) {
    set_current_comm("kselftest");
    launch_tests();
    return 0;
}
#endif

#define BUFSIZE 1024

static int kshell(void *args) {
    set_current_comm("kshell");

    struct file *tty = filp_open_anondevice(TTY_CONSOLE, O_RDWR, S_IFCHR | 0666);

    while (1) {
        fprintf(tty, "kshell> ");

        char buf[BUFSIZE];
        int32_t len;

        len = filp_read(tty, buf, BUFSIZE - 1);
        if (len < 0) {
            fprintf(tty, "TTY read failure: %d\n", len);
            continue;
        }
        if (!len) {
            fprintf(tty, "\\n isn't part of tty read?!\n");
            continue;
        }

        buf[len-1] = '\0';
        if (!strcmp(buf, ""))
            continue;
        else if (!strcmp(buf, "exit"))
            break;
        else if (!strcmp(buf, "ps")) {
            fprintf(tty, "PID     PPID    TTY     STAT    COMM\n");

            struct list_node *node;
            list_for_each(&tasks, node) {
                struct task_struct *task = node->value;

#define FIELD_WIDTH 8
                char tty_field[FIELD_WIDTH+1];
                if (task->session && task->session->tty)
                    snprintf(tty_field, FIELD_WIDTH, "tty%d", MINOR(task->session->tty->device_num));
                else
                    strcpy(tty_field, "?");
                tty_field[FIELD_WIDTH] = '\0';

                char stat_field[FIELD_WIDTH+1];
                snprintf(stat_field, FIELD_WIDTH + 1, "%s%s%s",
                    task->state == TASK_RUNNING ? "R" :
                    task->state == TASK_INTERRUPTIBLE ? "S" :
                    task->state == TASK_UNINTERRUPTIBLE ? "D" :
                    task->state == TASK_UNINTERRUPTIBLE ? "Z" : "?",
                    (task->session && task->session->sid == task->pid) ? "s" : "",
                    (task->session && task->session->foreground_pgid == task->pgid) ? "+" : ""
                );

                fprintf(tty, "%-8d%-8d%-8s%-8s", task->pid, task->ppid, tty_field, stat_field);
                if (task->mm)
                    fprintf(tty, "%s\n", task->comm);
                else
                    fprintf(tty, "[%s]\n", task->comm);
            }
        } else if (!strcmp(buf, "kselftest")) {
#if RUN_TESTS
            struct task_struct *kselftest_task = kernel_thread(&kselftest, NULL);
            wake_up_process(kselftest_task);
            do_wait(kselftest_task);
#else
            fprintf(tty, "kselftest unavailable\n");
#endif
        } else if (!strcmp(buf, "startlinux")) {
            extern struct task_struct *init_task;
            send_sig(init_task, SIGTERM);
        } else {
            fprintf(tty, "Unknown command \"%s\"\n", buf);
        }
    }

    filp_close(tty);
    return 0;
}
DEFINE_INIT_KTHREAD(kshell);
