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
#include "drivers/terminal.h"
#include "lib/stdio.h"

struct task_struct *init_task;

static int kernel_main(void *args) {
    init_task = current;
    strcpy(init_task->comm, "swapper");
    init_task->ppid = 0;

    // TODO: do root mount
    // init_task->cwd = filp_open('/', O_RDWR, 0);

    // if the schedule queue has anything, it's the boot context which
    // wouldn't work if rescheduled.
    list_pop_front(&schedule_queue);

    // Initialize drivers
    DO_INITCALL(drivers);

    // device (1, 0) is 0th initrd
    struct file *initrd_block = filp_open_anondevice(MKDEV(1, 0), 0, S_IFBLK | 0666);
    do_mount(initrd_block, get_sb_op_by_name("ece391fs"), &root_path);

    init_task->cwd = filp_open("/", 0, 0);

    terminal_open();
    int32_t cnt;
    uint8_t buf[1024];
    terminal_read(buf, 200);
    terminal_write(buf, 200);
    terminal_read(buf, 200);
    terminal_write(buf, 200);

#if RUN_TESTS
    /* Run tests */
    launch_tests();
#endif

    printf("init_task running with PID %d", current->pid);

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
