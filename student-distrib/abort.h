#ifndef _ABORT_H
#define _ABORT_H

// this will cause a hang.

#ifdef ASM

.macro  abort
    cli
1:
    hlt
    jmp     1b
.endm

#else

#include "compiler.h"

static noreturn inline __always_inline
void abort(void) {
    asm volatile ("cli");
    for (;;) asm volatile ("hlt");
}

#endif

#endif
