#include "syscall.h"
#include "lib/stdio.h"
#include "errno.h"

intr_handler_t *syscall_handlers[NUM_SUBSYSTEMS][MAX_SYSCALL];

static void syscall_handler(struct intr_info *info) {
    intr_handler_t *handler = NULL;
    if (info->eax < MAX_SYSCALL)
        handler = syscall_handlers[current->subsystem][info->eax];
    if (!handler) {
        printf("Unknown syscall: %d\n", info->eax);
        info->eax = -ENOSYS;
    } else {
        (*handler)(info);
    }

    // Evil ece391 subsystem shim
    if ((int32_t)info->eax < 0)
        info->eax = -1;
}

static void init_syscall_handler() {
    intr_setaction(INTR_SYSCALL, (struct intr_action){
        .handler = &syscall_handler });
}
DEFINE_INITCALL(init_syscall_handler, drivers);
