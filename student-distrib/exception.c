#include "types.h"
#include "interrupt.h"
#include "lib.h"
#include "abort.h"
#include "initcall.h"

static void general_protection_fault(struct intr_info *info) {
    printf("#GPF: 0x%x", info->error_code);
    abort();
}

static void init_exc_handlers() {
    intr_setaction(INTR_EXC_GENERAL_PROTECTION_FAULT, (struct intr_action){
        .handler = &general_protection_fault } );
}
DEFINE_INITCALL(init_exc_handlers, early);
