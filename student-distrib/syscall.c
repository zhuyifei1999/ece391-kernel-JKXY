#include "syscall.h"
#include "task/sched.h"
#include "printk.h"
#include "errno.h"

// create array of system call handlers
intr_handler_t *syscall_handlers[NUM_SUBSYSTEMS][MAX_SYSCALL];

/*
 * syscall_handler
 *   DESCRIPTION: call system call handlers
 *   INPUTS: struct intr_info *info
 */
static void syscall_handler(struct intr_info *info) {
    intr_handler_t *handler = NULL;
    // printk("Syscall: %u %x %x %x\n", info->eax, info->ebx, info->ecx, info->edx);
    // perform sanity check on the value of eax
    if (info->eax < MAX_SYSCALL) // load proper handler into handler
        handler = syscall_handlers[current->subsystem][info->eax];
    // print error message when handler is not defined
    if (!handler) {
        printk("Unknown syscall: %u\n", info->eax);
        info->eax = -ENOSYS;
    } else {
        // call handler when defined
        (*handler)(info);
        // printk("Sysret: %x\n", info->eax);
    }

    // Evil ece391 subsystem shim
    if (current->subsystem == SUBSYSTEM_ECE391 && (int32_t)info->eax < 0)
        info->eax = -1;
}

/*
 * init_syscall_handler
 *   DESCRIPTION: initiate system call handlers
 */
static void init_syscall_handler() {
    intr_setaction(INTR_SYSCALL, (struct intr_action){
        .handler = &syscall_handler });
}
DEFINE_INITCALL(init_syscall_handler, drivers);
