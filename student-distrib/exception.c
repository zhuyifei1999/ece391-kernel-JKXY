#include "types.h"
#include "interrupt.h"
#include "lib.h"
#include "abort.h"
#include "initcall.h"

#define DEFINE_EXC_HANDLER(fn, intr) static void init_exc_ ## fn() { \
    intr_setaction(intr, (struct intr_action){ \
        .handler = &fn } ); \
} \
DEFINE_INITCALL(init_exc_ ## fn, early)

// create stub exception handler
#define STUB_EXC_HANDLER(mnemonic, intr, fn_name) \
static void fn_name(struct intr_info *info) {     \
    printf(mnemonic ": 0x%x", info->error_code);  \
    abort();                                      \
}                                                 \
DEFINE_EXC_HANDLER(fn_name, intr)

// initialize stub exception handlers
STUB_EXC_HANDLER("#DE", INTR_EXC_DIVIDE_BY_ZERO_ERROR,          divide_by_zero);
STUB_EXC_HANDLER("#BR", INTR_EXC_BOUND_RANGE_EXCEEDED,          bound_range_exceeded);
STUB_EXC_HANDLER("#UD", INTR_EXC_INVALID_OPCODE,                invalid_opcode);
STUB_EXC_HANDLER("#NM", INTR_EXC_DEVICE_NOT_AVAILABLE,          device_not_available);
STUB_EXC_HANDLER("#TS", INTR_EXC_INVALID_TSS,                   invalid_tss);
STUB_EXC_HANDLER("#NP", INTR_EXC_SEGMENT_NOT_PRESENT,           segment_not_present);
STUB_EXC_HANDLER("#SS", INTR_EXC_STACK_SEGMENT_FAULT,           stack_segment);
STUB_EXC_HANDLER("#GP", INTR_EXC_GENERAL_PROTECTION_FAULT,      general_protection_fault);
STUB_EXC_HANDLER("#PF", INTR_EXC_PAGE_FAULT,                    page_fault);
STUB_EXC_HANDLER("#MF", INTR_EXC_X87_FLOATING_POINT_EXCEPTION,  x87_floating_point_exception);
STUB_EXC_HANDLER("#AC", INTR_EXC_ALIGNMENT_CHECK,               alignment_check);
STUB_EXC_HANDLER("#XF", INTR_EXC_SIMD_FLOATING_POINT_EXCEPTION, simd_floating_point_exception);
STUB_EXC_HANDLER("#VE", INTR_EXC_VIRTUALIZATION_EXCEPTION,      virtualization_exception);
