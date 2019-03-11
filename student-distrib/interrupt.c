#include "lib.h"
#include "interrupt.h"
#include "x86_desc.h"
#include "initcall.h"
#include "tests.h"

asmlinkage
void do_interrupt(struct intr_info *info) {
    struct intr_action action = intr_actions[info->intr_num];
    if (action.handler) {
        (*action.handler)(info);
    } else {
        printf("[Unhandled interrupt] number = 0x%x, code = 0x%x\n",
               info->intr_num, info->error_code);
    }
}

void intr_setaction(uint8_t intr_num, struct intr_action action) {
    intr_actions[intr_num] = action;
}

struct intr_action intr_getaction(uint8_t intr_num) {
    return intr_actions[intr_num];
}

#define _init_IDT_entry(intr, _type, _dpl, suffix) do { \
    /* we are just resolving address of the symbol here, prototype doesn't matter */ \
    extern void (*ISR_ ## intr ## _ ## suffix)(void); \
    uint32_t addr = (uint32_t)&ISR_ ## intr ## _ ## suffix; \
    struct idt_desc *entry = &idt[intr]; \
    entry->offset_15_00 = addr; \
    entry->offset_31_16 = addr >> 16; \
    entry->seg_selector = KERNEL_CS; \
    entry->size = 1; /* 32 bit handler */ \
    entry->dpl = _dpl; \
    entry->present = 1; \
    entry->type = _type; \
} while (0);
#define init_IDT_entry(intr, _type, _dpl, suffix) _init_IDT_entry(intr, _type, _dpl, suffix)

// this initializer initializes IDT, with the macro defined above
static void init_IDT() {
    init_IDT_entry(INTR_EXC_DIVIDE_BY_ZERO_ERROR, IDT_TYPE_INTERRUPT, KERNEL_DPL, nocode);
    init_IDT_entry(INTR_EXC_DEBUG, IDT_TYPE_INTERRUPT, KERNEL_DPL, nocode);
    init_IDT_entry(INTR_EXC_NON_MASKABLE_INTERRUPT, IDT_TYPE_INTERRUPT, KERNEL_DPL, nocode);
    init_IDT_entry(INTR_EXC_BREAKPOINT, IDT_TYPE_TRAP, KERNEL_DPL, nocode);
    init_IDT_entry(INTR_EXC_OVERFLOW, IDT_TYPE_TRAP, KERNEL_DPL, nocode);
    init_IDT_entry(INTR_EXC_BOUND_RANGE_EXCEEDED, IDT_TYPE_INTERRUPT, KERNEL_DPL, nocode);
    init_IDT_entry(INTR_EXC_INVALID_OPCODE, IDT_TYPE_INTERRUPT, KERNEL_DPL, nocode);
    init_IDT_entry(INTR_EXC_DEVICE_NOT_AVAILABLE, IDT_TYPE_INTERRUPT, KERNEL_DPL, nocode);
    // TODO: make double fault use a seperate TSS with task gate
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

    init_IDT_entry(INTR_IRQ0, IDT_TYPE_INTERRUPT, KERNEL_DPL, nocode);
    init_IDT_entry(INTR_IRQ1, IDT_TYPE_INTERRUPT, KERNEL_DPL, nocode);
    init_IDT_entry(INTR_IRQ2, IDT_TYPE_INTERRUPT, KERNEL_DPL, nocode);
    init_IDT_entry(INTR_IRQ3, IDT_TYPE_INTERRUPT, KERNEL_DPL, nocode);
    init_IDT_entry(INTR_IRQ4, IDT_TYPE_INTERRUPT, KERNEL_DPL, nocode);
    init_IDT_entry(INTR_IRQ5, IDT_TYPE_INTERRUPT, KERNEL_DPL, nocode);
    init_IDT_entry(INTR_IRQ6, IDT_TYPE_INTERRUPT, KERNEL_DPL, nocode);
    init_IDT_entry(INTR_IRQ7, IDT_TYPE_INTERRUPT, KERNEL_DPL, nocode);
    init_IDT_entry(INTR_IRQ8, IDT_TYPE_INTERRUPT, KERNEL_DPL, nocode);
    init_IDT_entry(INTR_IRQ9, IDT_TYPE_INTERRUPT, KERNEL_DPL, nocode);
    init_IDT_entry(INTR_IRQ10, IDT_TYPE_INTERRUPT, KERNEL_DPL, nocode);
    init_IDT_entry(INTR_IRQ11, IDT_TYPE_INTERRUPT, KERNEL_DPL, nocode);
    init_IDT_entry(INTR_IRQ12, IDT_TYPE_INTERRUPT, KERNEL_DPL, nocode);
    init_IDT_entry(INTR_IRQ13, IDT_TYPE_INTERRUPT, KERNEL_DPL, nocode);
    init_IDT_entry(INTR_IRQ14, IDT_TYPE_INTERRUPT, KERNEL_DPL, nocode);
    init_IDT_entry(INTR_IRQ15, IDT_TYPE_INTERRUPT, KERNEL_DPL, nocode);

#if RUN_TESTS
    init_IDT_entry(INTR_TEST, IDT_TYPE_INTERRUPT, KERNEL_DPL, nocode);
#endif

    init_IDT_entry(INTR_SYSCALL, IDT_TYPE_TRAP, USER_DPL, nocode);

    lidt(idt_desc_ptr);
}
DEFINE_INITCALL(init_IDT, early);


#if RUN_TESTS
/* IDT Entry Test
 *
 * Asserts that first 10 IDT entries are not NULL
 * Coverage: IDT definition
 */
static void idt_entry_test() {
    int i;
    for (i = 0; i < 10; ++i){
        TEST_ASSERT(idt[i].offset_15_00 || idt[i].offset_31_16);
    }
}
DEFINE_TEST(idt_entry_test);
#endif
