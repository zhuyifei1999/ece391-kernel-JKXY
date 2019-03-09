#include "lib.h"
#include "interrupt.h"
#include "x86_desc.h"

// TODO: prototype
asmlinkage
void common_interrupt_handler(struct intr_info *info) {
    printf("Interrupted by number %d, code = %d\n", info->intr_num, info->error_code);
}

#define _init_IDT_entry(intr, _type, _dpl, suffix) do { \
    /* we are just resolving address of the symbol here, prototype doesn't matter */ \
    extern void (*ISR_ ## intr ## _ ## suffix)(void); \
    uint32_t addr = (uint32_t)&ISR_ ## intr ## _ ## suffix; \
    idt_desc_t *entry = &idt[intr]; \
    entry->offset_15_00 = addr; \
    entry->offset_31_16 = addr >> 16; \
    entry->seg_selector = KERNEL_CS; \
    entry->size = 1; /* 32 bit handler */ \
    entry->dpl = _dpl; \
    entry->present = 1; \
    entry->type = _type; \
} while (0);
#define init_IDT_entry(intr, _type, _dpl, suffix) _init_IDT_entry(intr, _type, _dpl, suffix)

void init_IDT() {
    init_IDT_entry(INTR_EXC_DIVIDE_BY_ZERO_ERROR, IDT_TYPE_INTERRUPT, KERNEL_DPL, nocode);
    init_IDT_entry(INTR_EXC_DEBUG, IDT_TYPE_INTERRUPT, KERNEL_DPL, nocode);
    init_IDT_entry(INTR_EXC_NON_MASKABLE_INTERRUPT, IDT_TYPE_INTERRUPT, KERNEL_DPL, nocode);
    init_IDT_entry(INTR_EXC_BREAKPOINT, IDT_TYPE_TRAP, KERNEL_DPL, nocode);
    init_IDT_entry(INTR_EXC_OVERFLOW, IDT_TYPE_TRAP, KERNEL_DPL, nocode);
    init_IDT_entry(INTR_EXC_BOUND_RANGE_EXCEEDED, IDT_TYPE_INTERRUPT, KERNEL_DPL, nocode);
    init_IDT_entry(INTR_EXC_INVALID_OPCODE, IDT_TYPE_INTERRUPT, KERNEL_DPL, nocode);
    init_IDT_entry(INTR_EXC_DEVICE_NOT_AVAILABLE, IDT_TYPE_INTERRUPT, KERNEL_DPL, nocode);
    init_IDT_entry(INTR_EXC_DOUBLE_FAULT, IDT_TYPE_INTERRUPT, KERNEL_DPL, hascode);
    init_IDT_entry(INTR_EXC_COPROCESSOR_SEGMENT_OVERRUN, IDT_TYPE_INTERRUPT, KERNEL_DPL, nocode);
    init_IDT_entry(INTR_EXC_INVALID_TSS, IDT_TYPE_INTERRUPT, KERNEL_DPL, hascode);
    init_IDT_entry(INTR_EXC_SEGMENT_NOT_PRESENT, IDT_TYPE_INTERRUPT, KERNEL_DPL, hascode);
    init_IDT_entry(INTR_EXC_STACK_SEGMENT_FAULT, IDT_TYPE_INTERRUPT, KERNEL_DPL, hascode);
    init_IDT_entry(INTR_EXC_GENERAL_PROTECTION_FAULT, IDT_TYPE_INTERRUPT, KERNEL_DPL, hascode);
    init_IDT_entry(INTR_EXC_PAGE_FAULT, IDT_TYPE_INTERRUPT, KERNEL_DPL, hascode);
    init_IDT_entry(INTR_EXC_X87_FLOATING_POINT_EXCEPTION, IDT_TYPE_INTERRUPT, KERNEL_DPL, nocode);
    init_IDT_entry(INTR_EXC_ALIGNMENT_CHECK, IDT_TYPE_INTERRUPT, KERNEL_DPL, hascode);
    init_IDT_entry(INTR_EXC_MACHINE_CHECK, IDT_TYPE_INTERRUPT, KERNEL_DPL, nocode);
    init_IDT_entry(INTR_EXC_SIMD_FLOATING_POINT_EXCEPTION, IDT_TYPE_INTERRUPT, KERNEL_DPL, nocode);
    init_IDT_entry(INTR_EXC_VIRTUALIZATION_EXCEPTION, IDT_TYPE_INTERRUPT, KERNEL_DPL, nocode);
    init_IDT_entry(INTR_EXC_SECURITY_EXCEPTION, IDT_TYPE_INTERRUPT, KERNEL_DPL, hascode);

    init_IDT_entry(0x80, IDT_TYPE_TRAP, USER_DPL, nocode);

    barrier();

    lidt(idt_desc_ptr);
}
