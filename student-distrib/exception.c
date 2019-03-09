#include "exception.h"
#include "types.h"
#include "interrupt.h"
#include "lib.h"
#include "abort.h"

static void general_protection_fault(struct intr_info *info) {
    printf("#GPF: %x,", info->error_code);
    abort();
}

void init_exc_handlers() {
    intr_setaction(INTR_EXC_GENERAL_PROTECTION_FAULT, (struct intr_action){ .handler = &general_protection_fault, .stackaction = INTR_STACK_KEEP } );
}
