#define ASM     1

#include "../asm.h"
#include "../interrupt.h"
#include "../abort.h"

.text

ENTRY(entry_task):
    int $INTR_ENTRY
    abort
