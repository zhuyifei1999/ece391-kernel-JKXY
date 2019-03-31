#include "task.h"
#include "sched.h"
#include "clone.h"
#include "exec.h"
#include "../lib/string.h"
#include "../syscall.h"
#include "../signal.h"
#include "../err.h"
#include "../errno.h"

static int ece391execute_child(void *args) {
    return do_execve(args, NULL, NULL);
}

DEFINE_SYSCALL1(ECE391, execute, char *, command) {
    uint32_t length = safe_arr_null_term(command, sizeof(char), false);
    if (!length)
        return -EFAULT;

    char *command_kern = strndup(command, length);
    if (!command_kern)
        return -ENOMEM;

    struct task_struct *child = do_clone(SIGCHLD, ece391execute_child, command_kern, NULL, NULL);

    if (IS_ERR(child)) {
        kfree(command_kern);
        return PTR_ERR(child);
    }

    wake_up_process(child);

    return do_wait(child);
}
