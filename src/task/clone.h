#ifndef _CLONE_H
#define _CLONE_H

#include "task.h"

struct user_desc;

// copied from <uapi/linux/sched.h>

#define CSIGNAL              0x000000ff /* signal mask to be sent at exit */ // TODO
#define CLONE_VM             0x00000100 /* set if VM shared between processes */
// #define CLONE_FS             0x00000200 /* set if fs info shared between processes */
#define CLONE_FILES          0x00000400 /* set if open files shared between processes */
#define CLONE_SIGHAND        0x00000800 /* set if signal handlers and blocked signals shared */
// #define CLONE_PTRACE         0x00002000 /* set if we want to let tracing continue on the child too */
#define CLONE_VFORK          0x00004000 /* set if the parent wants the child to wake it up on mm_release */
#define CLONE_PARENT         0x00008000 /* set if we want to have the same parent as the cloner */
#define CLONE_THREAD         0x00010000 /* Same thread group? */ // TODO
// #define CLONE_NEWNS          0x00020000 /* New mount namespace group */
// #define CLONE_SYSVSEM        0x00040000 /* share system V SEM_UNDO semantics */
#define CLONE_SETTLS         0x00080000 /* create a new TLS for the child */
#define CLONE_PARENT_SETTID  0x00100000 /* set the TID in the parent */
// #define CLONE_CHILD_CLEARTID 0x00200000 /* clear the TID in the child */ // TODO
// #define CLONE_DETACHED       0x00400000 /* Unused, ignored */
// #define CLONE_UNTRACED       0x00800000 /* set if the tracing process can't force CLONE_PTRACE on this clone */
#define CLONE_CHILD_SETTID   0x01000000 /* set the TID in the child */

struct task_struct *do_clone(uint32_t flags, int (*fn)(void *args), void *args, int *ptid, int *ctid, struct user_desc *newtls);

struct task_struct *kernel_thread(int (*fn)(void *args), void *args);

#endif
