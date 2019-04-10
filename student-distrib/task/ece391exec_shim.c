#include "ece391exec_shim.h"
#include "task.h"
#include "sched.h"
#include "clone.h"
#include "exec.h"
#include "exit.h"
#include "../lib/string.h"
#include "../mm/kmalloc.h"
#include "../syscall.h"
#include "../signal.h"
#include "../err.h"
#include "../errno.h"

/*
 * ece391execute_child
 *   DESCRIPTION: call system call handlers
 *   INPUTS: struct intr_info *info
 */
static int ece391execute_child(void *args) {
    char *cmd_full = args;
    // find the place of space
    char *space = strchr(cmd_full, ' ');

    // 3 elements, 0th is the command, 1st is the one for getargs(), 2st is NULL
    char **argv = kcalloc(3, sizeof(*argv));
    if (space) {
        argv[0] = strndup(cmd_full, space - cmd_full);
        for (; *space == ' '; space++);
        argv[1] = strdup(space);
    } else
        argv[0] = strdup(cmd_full);

    kfree(cmd_full);

    int32_t res = do_execve(strdup(argv[0]), argv, NULL);

    kfree(argv[0]);
    if (argv[1])
        kfree(argv[1]);
    kfree(argv);

    return res;
}

DEFINE_SYSCALL1(ECE391, execute, char *, command) {
    // acquire the length of the command
    uint32_t length = safe_arr_null_term(command, sizeof(char), false);
    // sanity check
    if (!length)
        return -EFAULT;

    // copy command to comand_kern
    char *command_kern = strndup(command, length);
    // sanity check
    if (!command_kern)
        return -ENOMEM;

    // create child task struct
    struct task_struct *child = kernel_thread(&ece391execute_child, command_kern);

    // check if there is error
    if (IS_ERR(child)) {
        kfree(command_kern);
        return PTR_ERR(child);
    }

    // put child process into queue
    wake_up_process(child);

    return do_wait(child);
}

DEFINE_SYSCALL2(ECE391, getargs, char *, buf, int32_t, nbytes) {
    // acquire the length of arguments
    uint32_t length = safe_arr_null_term((char *)ECE391_ARGSADDR, sizeof(char), false);
    // copy arguments to args_kern
    char *args_kern = strndup((char *)ECE391_ARGSADDR, length);
    // sanity check
    if (!args_kern)
        return -ENOMEM;

    // aquire length of buffer
    uint32_t safe_nbytes = safe_buf(buf, nbytes, true);
    // sanity check
    if (!safe_nbytes && nbytes)
        return -EFAULT;

    // copy args_kern to buf
    strncpy(buf, args_kern, safe_nbytes);

    // free args_kern
    kfree(args_kern);

    return 0;
}
