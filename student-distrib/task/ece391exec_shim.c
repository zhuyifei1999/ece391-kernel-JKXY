#include "ece391exec_shim.h"
#include "task.h"
#include "sched.h"
#include "clone.h"
#include "exec.h"
#include "../lib/string.h"
#include "../mm/kmalloc.h"
#include "../syscall.h"
#include "../signal.h"
#include "../err.h"
#include "../errno.h"

static int ece391execute_child(void *args) {
    char *cmd_full = args;
    char *space = strchr(cmd_full, ' ');

    // 3 elements, 0th is the command, 1st is the one for getargs(), 2st is NULL
    char **argv = kcalloc(3, sizeof(*argv));
    if (space) {
        argv[0] = strndup(cmd_full, space - cmd_full);
        argv[1] = strdup(space + 1);
    } else
        argv[0] = strdup(cmd_full);

    kfree(cmd_full);
    return do_execve(strdup(argv[0]), argv, NULL);
}

DEFINE_SYSCALL1(ECE391, execute, char *, command) {
    uint32_t length = safe_arr_null_term(command, sizeof(char), false);
    if (!length)
        return -EFAULT;

    char *command_kern = strndup(command, length);
    if (!command_kern)
        return -ENOMEM;

    struct task_struct *child = kernel_thread(&ece391execute_child, command_kern);

    if (IS_ERR(child)) {
        kfree(command_kern);
        return PTR_ERR(child);
    }

    wake_up_process(child);

    return do_wait(child);
}

DEFINE_SYSCALL2(ECE391, getargs, char *, buf, int32_t, nbytes) {
    uint32_t length = safe_arr_null_term((char *)ECE391_ARGSADDR, sizeof(char), false);
    char *args_kern = strndup((char *)ECE391_ARGSADDR, length);
    if (!args_kern)
        return -ENOMEM;

    uint32_t safe_nbytes = safe_buf(buf, nbytes, true);
    if (!safe_nbytes && nbytes)
        return -EFAULT;

    strncpy(buf, args_kern, safe_nbytes);

    kfree(args_kern);

    return 0;
}
