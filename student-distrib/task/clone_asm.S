#define ASM     1

#include "../asm.h"

.text

ENTRY(entry_task):
    push    %edx
    push    %ecx
    push    %ebx
    push    %eax
    call    clone_child
