#ifndef _ABORT_H
#define _ABORT_H

// this will cause a hang. if that fails, triple fault.

#ifdef ASM

.macro  abort
    cli
1:
    hlt
    jmp     1b

    lidt    0
    int3
.endm

#else

#include "compiler.h"

static noreturn inline __attribute__((always_inline))
void abort(void) {
    asm volatile ("cli");
    for (;;) asm volatile ("hlt");
    asm volatile ("lidt 0; int3;");
}

#endif

#endif
