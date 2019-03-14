#define ASM     1

#include "../asm.h"

.text

ENTRY(entry_task):
    test    %eax,%eax
    je      userspace
    // kthread
    push    %ebx
    push    %eax
    call    kthread_execute
    // won't return

userspace:
    // TODO
