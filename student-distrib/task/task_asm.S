#define ASM     1

#include "../asm.h"

.text

ENTRY(entry_task):
    test    %eax,%eax
    je      1f
    // kthread
    push    %ebx
    push    %eax
    call    kthread_execute
    // won't return

    // userspace
1:
    // TODO
