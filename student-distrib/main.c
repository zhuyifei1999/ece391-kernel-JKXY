#include "main.h"
#include "task/sched.h"
#include "task/kthread.h"
#include "task/clone.h"
#include "task/exec.h"
#include "task/session.h"
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

#if RUN_TESTS
static int kselftest(void *args) {
    set_current_comm("kselftest");
    if (!launch_tests())
        tty_switch_foreground(MKDEV(TTY_MAJOR, 1));
    return 0;
}
#endif

static int run_init_process(void *args) {
    char *argv[] = {
        "shell",
        NULL
    };
    char *envp[] = {
        "HOME=/",
        // "TERM=linux"
        NULL
    };

    if (args && !current->session) {
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
    }

    int32_t res = do_execve(argv[0], argv, envp);
    if (res)
        panic("Could not execute init: %d\n", res);

    return res;
}

static int kernel_dummy_init(void *args) {
    // The purpose of this dummy PID 1 is to fork off all the shells on different TTYs,
    // because the ECE391 subsystem is too bad and can't self-govern.

    set_current_comm("kernel_init");

#if !RUN_TESTS
    tty_switch_foreground(MKDEV(TTY_MAJOR, 1));
#endif

    int i;
    // Opening 3 shells on tty 1-3
    for (i = 1; i <= 3; i++) {
        int *kthread_arg = kmalloc(sizeof(*kthread_arg));
        *kthread_arg = i;

        wake_up_process(kthread(&run_init_process, kthread_arg));
    }

    // TODO: Make a centain signal do the subsystem switch
    while (1) {
        current->state = TASK_INTERRUPTIBLE;
        schedule();
    }
}

noreturn void kernel_main(void) {
    swapper_task = current;
    // DO NOT replace this with a single set. Other values must be zero-initialized.
    *swapper_task = (struct task_struct){
        .comm      = "swapper",
    };

    // Initialize drivers
    DO_INITCALL(drivers);

    // device (1, 0) is 0th initrd
    // struct file *initrd_block = filp_open_anondevice(MKDEV(1, 0), 0, S_IFBLK | 0666);
    struct file *initrd_block = filp_open_anondevice(MKDEV(8, 2), 0, S_IFBLK | 0666);
    if (IS_ERR(initrd_block))
        panic("Could not open initrd: %d\n", PTR_ERR(initrd_block));
    int32_t res = do_mount(initrd_block, get_sb_op_by_name("ece391fs"), &root_path);
    if (res < 0)
        panic("Could not mount root: %d\n", res);
    swapper_task->cwd = filp_open("/", 0, 0);
    if (IS_ERR(swapper_task->cwd))
        panic("Could not set working directory to root directory: %d\n", PTR_ERR(swapper_task->cwd));

    init_task = kernel_thread(&kernel_dummy_init, NULL);

    wake_up_process(kernel_thread(&kthreadd, NULL));
    schedule();

    wake_up_process(init_task);

#if RUN_TESTS
    // start the tests in a seperate kthread, and let that start init
    wake_up_process(kthread(&kselftest, NULL));
#endif

    for (;;) {
        while (list_isempty(&schedule_queue))
            asm volatile ("hlt" : : : "memory");
        schedule();
    }
}
