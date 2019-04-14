#include "main.h"
#include "task/sched.h"
#include "task/kthread.h"
#include "task/clone.h"
#include "task/exec.h"
#include "task/exit.h"
#include "task/session.h"
#include "task/signal.h"
#include "char/tty.h"
#include "mm/kmalloc.h"
#include "initcall.h"
#include "panic.h"
#include "structure/list.h"
#include "tests.h"
#include "vfs/file.h"
#include "vfs/device.h"
#include "vfs/superblock.h"
#include "vfs/path.h"
#include "vfs/mount.h"
#include "lib/string.h"
#include "lib/stdio.h"
#include "lib/stdlib.h"
#include "atomic.h"
#include "err.h"

struct task_struct *swapper_task;
struct task_struct *init_task;

static int run_init_process(void *args) {
    set_current_comm("init");

    char *argv[] = {
        "shell",
        NULL
    };
    char *envp[] = {
        "HOME=/",
        // "TERM=linux"
        NULL
    };

    if (!current->session) {
        do_setsid();
    }

    int i;
    for (i = 0; i < _NSIG; i++)
        kernel_unmask_signal(i);

    int32_t res = do_execve(argv[0], argv, envp);
    if (res)
        panic("Could not execute init: %d\n", res);

    return res;
}

static int kernel_init_shepherd(void *args) {
    set_current_comm("shepherd");

    do_setsid();

    // The args specify the TTY number
    int *tty_num = args;
    struct file *file = filp_open_anondevice(MKDEV(TTY_MAJOR, *tty_num), O_RDWR, S_IFCHR | 0666);
    kfree(args);

    if (!IS_ERR(file)) {
        if (current->session && current->session->tty) {
            fprintf(file, "TTY%d\n", MINOR(current->session->tty->device_num));
        }
        filp_close(file);
    }

    kernel_unmask_signal(SIGTERM);
    kernel_unmask_signal(SIGCHLD);

    struct task_struct *userspace_init = kernel_thread(&run_init_process, NULL);
    wake_up_process(userspace_init);

    while (true) {
        uint16_t signal = kernel_get_pending_sig();
        switch (signal) {
        case SIGTERM: // TODO: Do subsystem switch
            send_sig(userspace_init, SIGTERM);
            return 0;
        case SIGCHLD:
            do_wait(userspace_init);
            userspace_init = kernel_thread(&run_init_process, NULL);
            wake_up_process(userspace_init);
            break;
        default:
            current->state = TASK_INTERRUPTIBLE;
            schedule();
            current->state = TASK_RUNNING;
        }
    }
}

static int kernel_dummy_init(void *args) {
    // The purpose of this dummy PID 1 is to fork off all the shells on different TTYs,
    // because the ECE391 subsystem is too bad and can't self-govern.

    set_current_comm("kernel_init");

    tty_switch_foreground(MKDEV(TTY_MAJOR, 1));

    int i;

#define NUM_TERMS 3
    struct task_struct *shepards[NUM_TERMS];

    // Opening 3 shells on tty 1-3
    for (i = 0; i < NUM_TERMS; i++) {
        int *kthread_arg = kmalloc(sizeof(*kthread_arg));
        *kthread_arg = i + 1;

        shepards[i] = kernel_thread(&kernel_init_shepherd, kthread_arg);
        wake_up_process(shepards[i]);
    }

    kernel_unmask_signal(SIGTERM);

    // TODO: Make a centain signal do the subsystem switch
    while (true) {
        uint16_t signal = kernel_get_pending_sig();
        switch (signal) {
        case SIGTERM: // TODO: Do subsystem switch
            for (i = 0; i < NUM_TERMS; i++) {
                send_sig(shepards[i], SIGTERM);
            }
            goto do_switch;
        default:
            current->state = TASK_INTERRUPTIBLE;
            schedule();
            current->state = TASK_RUNNING;
        }
    }

do_switch: // TODO
    panic("TODO");
}

noreturn void kernel_main(void) {
    swapper_task = current;
    // DO NOT replace this with a single set. Other values must be zero-initialized.
    *swapper_task = (struct task_struct){
        .comm      = "swapper",
    };

    swapper_task->sigactions = kcalloc(1, sizeof(*swapper_task->sigactions));
    atomic_set(&swapper_task->sigactions->refcount, 1);
    int i;
    for (i = 0; i < _NSIG; i++)
        kernel_mask_signal(i);

    // Initialize drivers
    DO_INITCALL(drivers);

    // device (1, 0) is 0th initrd
    struct file *root_block = filp_open_anondevice(MKDEV(1, 0), 0, S_IFBLK | 0666);
    // device (8, 2) is secondary ATA master
    // struct file *root_block = filp_open_anondevice(MKDEV(8, 2), 0, S_IFBLK | 0666);
    if (IS_ERR(root_block))
        panic("Could not open initrd: %d\n", PTR_ERR(root_block));
    int32_t res = do_mount(root_block, get_sb_op_by_name("ece391fs"), &root_path);
    if (res < 0)
        panic("Could not mount root: %d\n", res);
    swapper_task->cwd = filp_open("/", 0, 0);
    if (IS_ERR(swapper_task->cwd))
        panic("Could not set working directory to root directory: %d\n", PTR_ERR(swapper_task->cwd));

    init_task = kernel_thread(&kernel_dummy_init, NULL);

    wake_up_process(kernel_thread(&kthreadd, NULL));
    schedule();

    DO_INITCALL(init_kthread);

    wake_up_process(init_task);

    for (;;) {
        while (list_isempty(&schedule_queue))
            asm volatile ("hlt" : : : "memory");
        schedule();
    }
}
