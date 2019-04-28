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

#define HAS_ECE391 1

static void mount_root_device(uint32_t device_num, char *fsname) {
    struct file *root_block = filp_open_anondevice(device_num, 0, S_IFBLK | 0666);
    if (IS_ERR(root_block))
        panic("Could not open root device: %d\n", PTR_ERR(root_block));
    int32_t res = do_mount(root_block, get_sb_op_by_name(fsname), &root_path);
    if (res < 0)
        panic("Could not mount root: %d\n", res);
    filp_close(root_block);
}

static int run_init_process(void *args) {
    set_current_comm("init");

    char *argv[] = {
        args,
        NULL
    };
    char *envp[] = {
        "HOME=/",
        "TERM=linux-16color",
        "PATH=/bin:/",
        NULL
    };

    if (!current->session) {
        do_setsid();
    }

    int32_t res = do_execve(argv[0], argv, envp);
    if (res)
        panic("Could not execute init: %d\n", res);

    return res;
}

#if HAS_ECE391

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

    struct task_struct *userspace_init = kernel_thread(&run_init_process, "/shell");
    wake_up_process(userspace_init);

    while (true) {
        uint16_t signal = signal_pending_one(current);
        if (signal)
            kernel_get_pending_sig(signal, NULL);
        switch (signal) {
        case SIGTERM: // TODO: Do subsystem switch
            send_sig(userspace_init, SIGTERM);
            return 0;
        case SIGCHLD:
            do_wait(userspace_init);
            userspace_init = kernel_thread(&run_init_process, "/shell");
            wake_up_process(userspace_init);
            break;
        default:
            current->state = TASK_INTERRUPTIBLE;
            schedule();
            current->state = TASK_RUNNING;
        }
    }
}

#endif

static int kernel_dummy_init(void *args) {
    // The purpose of this dummy PID 1 is to fork off all the shells on different TTYs,
    // because the ECE391 subsystem is too bad and can't self-govern.

    set_current_comm("kernel_init");

    tty_switch_foreground(MKDEV(TTY_MAJOR, 1));

#if HAS_ECE391

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
        uint16_t signal = signal_pending_one(current);
        if (signal)
            kernel_get_pending_sig(signal, NULL);
        switch (signal) {
        case SIGTERM:
            goto do_switch;
        default:
            current->state = TASK_INTERRUPTIBLE;
            schedule();
            current->state = TASK_RUNNING;
        }
    }

do_switch:
    tty_switch_foreground(MKDEV(TTY_MAJOR, 1));
    printk("Starting Linux\n");

    for (i = 0; i < NUM_TERMS; i++) {
        send_sig(shepards[i], SIGTERM);
    }

do_switch_loop:;
    // Terminate all userspace
    struct list_node *node;
    list_for_each(&tasks, node) {
        struct task_struct *task = node->value;
        if (task->mm && task->subsystem == SUBSYSTEM_ECE391 && task != current)
            send_sig(task, SIGTERM);
    }

    // Wait for them to exit. Because we are child reaper, SIGCHLD messes up our
    // terminal reading if we don't wait first.
    schedule();
    list_for_each(&tasks, node) {
        struct task_struct *task = node->value;
        if (task->mm && task->subsystem == SUBSYSTEM_ECE391 && task != current)
            goto do_switch_loop;
    }
#endif

    int32_t res = do_umount(&root_path);
    if (res < 0)
        panic("Could not umount root: %d\n", res);

    // device (8, 2) is secondary ATA master
    mount_root_device(MKDEV(8, 2), "ustar");

    // attach to TTY 1
    do_setsid();
    struct file *file = filp_open_anondevice(MKDEV(TTY_MAJOR, 1), O_RDWR, S_IFCHR | 0666);
    if (!IS_ERR(file))
        filp_close(file);

    return run_init_process("/bin/sh");
}

noreturn void kernel_main(void) {
    swapper_task = current;
    // DO NOT replace this with a single set. Other values must be zero-initialized.
    *swapper_task = (struct task_struct){
        .comm      = "swapper",
    };

    swapper_task->sigactions = kmalloc(sizeof(*swapper_task->sigactions));
    *swapper_task->sigactions = (struct sigactions){
        .refcount = ATOMIC_INITIALIZER(1),
    };

    int i;
    for (i = 0; i < NSIG; i++)
        kernel_mask_signal(i);

    // Initialize drivers
    DO_INITCALL(drivers);

    // device (1, 0) is 0th initrd
    mount_root_device(MKDEV(1, 0), "ece391fs");

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
