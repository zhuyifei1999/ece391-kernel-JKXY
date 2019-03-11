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
#define STUB_EXC_HANDLER(fn_name, intr, mnemonic) \
static void fn_name(struct intr_info *info) { \
    printf(mnemonic ": 0x%x", info->error_code); \
    abort(); \
} \
DEFINE_EXC_HANDLER(fn_name, intr)

// initialize stub exception handlers
STUB_EXC_HANDLER(divide_by_zero, INTR_EXC_DIVIDE_BY_ZERO_ERROR, "#DE");
STUB_EXC_HANDLER(bound_range_exceeded, INTR_EXC_BOUND_RANGE_EXCEEDED, "#BR");
STUB_EXC_HANDLER(invalid_opcode, INTR_EXC_INVALID_OPCODE, "#UD");
STUB_EXC_HANDLER(device_not_available, INTR_EXC_DEVICE_NOT_AVAILABLE, "#NM");
STUB_EXC_HANDLER(invalid_tss, INTR_EXC_INVALID_TSS, "#TS");
STUB_EXC_HANDLER(segment_not_present, INTR_EXC_SEGMENT_NOT_PRESENT, "#NP");
STUB_EXC_HANDLER(stack_segment, INTR_EXC_STACK_SEGMENT_FAULT, "#SS");
STUB_EXC_HANDLER(general_protection_fault, INTR_EXC_GENERAL_PROTECTION_FAULT, "#GP");
STUB_EXC_HANDLER(page_fault, INTR_EXC_PAGE_FAULT, "#PF");
STUB_EXC_HANDLER(x87_floating_point_exception, INTR_EXC_X87_FLOATING_POINT_EXCEPTION, "#MF");
STUB_EXC_HANDLER(alignment_check, INTR_EXC_ALIGNMENT_CHECK, "#AC");
STUB_EXC_HANDLER(simd_floating_point_exception, INTR_EXC_SIMD_FLOATING_POINT_EXCEPTION, "#XF");
STUB_EXC_HANDLER(virtualization_exception, INTR_EXC_VIRTUALIZATION_EXCEPTION, "#VE");
