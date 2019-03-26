#include "main.h"
#include "task/sched.h"
#include "task/kthread.h"
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

struct task_struct *swapper_task;

#if RUN_TESTS
static int kselftest(void *args) {
    strcpy(current->comm, "kselftest");
    launch_tests();
    return 0;
}
#endif

static int kernel_main(void *args) {
    swapper_task = current;
    strcpy(swapper_task->comm, "swapper");

    // Initialize drivers
    DO_INITCALL(drivers);

    // device (1, 0) is 0th initrd
    struct file *initrd_block = filp_open_anondevice(MKDEV(1, 0), 0, S_IFBLK | 0666);
    if (IS_ERR(initrd_block))
        panic("Could not open initrd: %d\n", PTR_ERR(initrd_block));
    int32_t res = do_mount(initrd_block, get_sb_op_by_name("ece391fs"), &root_path);
    if (res < 0)
        panic("Could not mount root: %d\n", res);
    swapper_task->cwd = filp_open("/", 0, 0);
    if (IS_ERR(swapper_task->cwd))
        panic("Could not set working directory to root directory: %d\n", PTR_ERR(swapper_task->cwd));

    struct task_struct *kthreadd_task = kernel_thread(&kthreadd, NULL);
    wake_up_process(kthreadd_task);
    schedule();

#if RUN_TESTS
    // start the tests in a seperate kthread
    wake_up_process(kthread(&kselftest, NULL));
#endif

    for (;;) {
        while (list_isempty(&schedule_queue))
            asm volatile ("hlt" : : : "memory");
        schedule();
    }
}

noreturn
void exec_swapper_task(void) {
    struct task_struct *task = kernel_thread(&kernel_main, NULL);
    wake_up_process(task);
    schedule();
    // The scheduling will corrupt this stack. Boot context is unschedulable
    panic("Boot context reschduled.\n");
}
