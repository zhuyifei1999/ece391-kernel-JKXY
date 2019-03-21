#include "main.h"
#include "task/sched.h"
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

struct task_struct *init_task;

#if RUN_TESTS
static int kselftest(void *args) {
    printf("kselftest running with PID %d\n", current->pid);
    launch_tests();
    return 0;
}
#endif

static int kernel_main(void *args) {
    init_task = current;
    strcpy(init_task->comm, "swapper");

    printf("init_task running with PID %d\n", current->pid);

    // Initialize drivers
    DO_INITCALL(drivers);

    // device (1, 0) is 0th initrd
    struct file *initrd_block = filp_open_anondevice(MKDEV(1, 0), 0, S_IFBLK | 0666);
    do_mount(initrd_block, get_sb_op_by_name("ece391fs"), &root_path);
    init_task->cwd = filp_open("/", 0, 0);

#if RUN_TESTS
    // start the tests in a seperate kernel thread
    struct task_struct *kselftest_thread = kernel_thread(&kselftest, NULL);
    wake_up_process(kselftest_thread);
    schedule();
    do_wait(kselftest_thread);
#endif

    printf("init_task idling with PID %d\n", current->pid);

    for (;;) {
        while (list_isempty(&schedule_queue))
            asm volatile ("hlt" : : : "memory");
        schedule();
    }
}

noreturn
void exec_init_task(void) {
    struct task_struct *task = kernel_thread(&kernel_main, NULL);
    wake_up_process(task);
    schedule();
    // The scheduling will corrupt this stack. Boot context is unschedulable
    panic("Boot context reschduled.\n");
}
