#include "types.h"
#include "interrupt.h"
#include "lib.h"
#include "abort.h"
#include "initcall.h"

static void general_protection_fault(struct intr_info *info) {
    printf("#GP: 0x%x", info->error_code);
    abort();
}

static void page_fault(struct intr_info *info) {
    printf("#PF: 0x%x", info->error_code);
    abort();
}

static void init_exc_handlers() {
    intr_setaction(INTR_EXC_GENERAL_PROTECTION_FAULT, (struct intr_action){
        .handler = &general_protection_fault } );
    intr_setaction(INTR_EXC_PAGE_FAULT, (struct intr_action){
        .handler = &page_fault } );
}
DEFINE_INITCALL(init_exc_handlers, early);
