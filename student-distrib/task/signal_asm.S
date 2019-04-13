#define ASM     1

#include "../asm.h"
#include "../interrupt.h"
#include "../syscall.h"

.data

ENTRY(ECE391_sigret_start):
    movl $NR_ECE391_sigreturn,%eax
    int $INTR_SYSCALL
ENTRY(ECE391_sigret_postint):
    hlt // if fail, segv us
ENTRY(ECE391_sigret_stop):
